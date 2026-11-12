"""Build, flash transaction, private artifacts, and report ownership."""

import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import socket
import subprocess
import sys
import time

from .boards import BOARDS, esptool_path, load_config, redact
from .ingress import LocalIngress, make_certificates
from .monitor import Monitor, summarize

ROOT = Path(__file__).resolve().parents[2]
FIRMWARE = Path(__file__).parent / "firmware"


def prepare_project(output, config, board):
    definitions = {
        "TEST_BOARD_HEADER": board.header,
        "TEST_WIFI_SSID": config["wifi_ssid"],
        "TEST_WIFI_PASSWORD": config["wifi_password"],
        "TEST_PROVISION_TOKEN": config["provision_token"],
        "TEST_LINK_URL": f'wss://{config["lan_address"]}:{config["tls_port"]}/api/device-link',
        "FLOVA_TLS_ROOT_CERTS": (output / "ca.crt").read_text(),
    }
    header = "#pragma once\n" + "".join(
        f"#define {key} {json.dumps(value)}\n" for key, value in definitions.items())
    (output / "private_config.h").write_text(header + f"#define TEST_EPOCH {int(time.time())}\n")
    includes = [output, *[ROOT / "packages" / name / "include" for name in (
        "flova-embedded-sdk", "flova-arduino", "flova-esp32", "flova-esp8266")]]
    flags = "\n".join(f'  -I"{path}"' for path in includes)
    (output / "platformio.ini").write_text(f"""[platformio]
extra_configs = {ROOT / 'platformio.ini'}
src_dir = {FIRMWARE}

[env:board-e2e]
extends = env:{board.environment}
lib_extra_dirs = {ROOT / 'packages'}
  {ROOT / 'third_party'}
  {ROOT / 'protocol/generated'}
build_src_filter = +<main.cpp>
build_flags =
  ${{env:{board.environment}.build_flags}}
{flags}
  -DFLOVA_DEVELOPMENT_E2E=1
  -DFLOVA_LOGGING_ENABLED=0
""")


def arguments(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", required=True, type=Path, help="private development JSON configuration")
    parser.add_argument("--board", choices=BOARDS, default="esp32")
    parser.add_argument("--output", type=Path, help="new private artifact directory; must not exist")
    parser.add_argument("--flash", action="store_true",
                        help="authorize backup, test uploads, full erase, and original-flash restoration")
    parser.add_argument("--port", help="explicit serial port; required with --flash")
    parser.add_argument("--seconds", type=int, default=20, help="steady telemetry benchmark duration (5–1800)")
    parser.add_argument("--reconnects", type=int, default=3, help="Wi-Fi loss/recovery cycles (0–100)")
    parser.add_argument("--minimum-heap", type=int, default=4096, help="minimum free-heap safety floor in bytes")
    args = parser.parse_args(argv)
    if args.flash and not args.port:
        parser.error("--flash requires an explicit --port")
    if not 5 <= args.seconds <= 1800 or not 0 <= args.reconnects <= 100 or args.minimum_heap < 4096:
        parser.error("invalid benchmark duration, reconnect count, or heap floor (minimum 4096)")
    return args


def main(argv=None):
    args = arguments(argv)
    os.umask(0o077)  # Headers, firmware binaries and flash backups contain credentials.
    config = {}
    output = None
    report = {"board": args.board, "mode": "hardware" if args.flash else "build-only",
              "status": "failed", "stages": [], "restoration": "not-needed",
              "workload": {"steady_seconds": args.seconds, "reconnects": args.reconnects,
                           "report_interval_ms": 200, "deferred_flush_ms": 5000,
                           "minimum_heap_bytes": args.minimum_heap}}
    modified = False
    lock = None
    tool = None
    previous_term = None

    def run(name, command, cwd=ROOT, timeout=300):
        print(f"[{name}]", flush=True)
        start = time.monotonic()
        result = subprocess.run([str(part) for part in command], cwd=cwd,
                                capture_output=True, text=True, timeout=timeout)
        text = result.stdout + result.stderr
        (output / f"{name}.log").write_text(redact(text, config))
        report["stages"].append({"name": name, "seconds": round(time.monotonic() - start, 3),
                                 "exit_code": result.returncode})
        if result.returncode:
            raise RuntimeError(f"{name} failed; see {output / (name + '.log')}")
        return text

    try:
        config = load_config(args.config)
        for executable in ("pio", "openssl"):
            if not shutil.which(executable):
                raise RuntimeError(f"required executable not found: {executable}")
        if args.flash:
            import serial  # PlatformIO's Python environment already includes pyserial.
            with socket.create_connection(("127.0.0.1", config["engine_port"]), timeout=5):
                pass
            def interrupted(*_):
                raise KeyboardInterrupt()
            previous_term = signal.signal(signal.SIGTERM, interrupted)
        board = BOARDS[args.board]
        candidate = (args.output or ROOT / ".local/board-e2e" / time.strftime("%Y%m%d-%H%M%S")).resolve()
        candidate.mkdir(mode=0o700, parents=True, exist_ok=False)
        output = candidate
        make_certificates(output, config["lan_address"], run)
        prepare_project(output, config, board)
        report["revision"] = subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
        report["working_tree_dirty"] = bool(subprocess.check_output(
            ["git", "status", "--porcelain"], cwd=ROOT, text=True).strip())
        run("compile-unit-tests", ["pio", "test", "-e", board.unit_environment,
                                    "--without-uploading", "--without-testing"])
        run("compile-harness", ["pio", "run", "-d", output, "-e", "board-e2e"])
        image = output / ".pio/build/board-e2e/firmware.bin"
        if image.is_file():
            report["firmware_sha256"] = hashlib.sha256(image.read_bytes()).hexdigest()
            report["firmware_image_bytes"] = image.stat().st_size
        if args.flash:
            lock = (ROOT / ".local/board-e2e.lock").open("w")
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            tool = [sys.executable, esptool_path(), "--chip", board.chip,
                    "--port", args.port, "--baud", "460800"]
            identity = run("identify-board", tool + ["flash_id"])
            detected = re.search(r"Detected flash size: (\d+)MB", identity)
            if not detected or int(detected[1]) * 1024 * 1024 != board.flash_bytes:
                raise RuntimeError("board flash size does not match the reviewed board profile")
            backup = output / "original-flash.bin"
            run("backup-flash", tool + ["read_flash", "0", str(board.flash_bytes), backup])
            if backup.stat().st_size != board.flash_bytes:
                raise RuntimeError("incomplete full-flash backup; refusing to upload")
            report["original_flash_sha256"] = hashlib.sha256(backup.read_bytes()).hexdigest()
            with LocalIngress(output, config):
                modified = True  # Restore even when an upload/erase is interrupted.
                unit_output = run("hardware-unit-tests", ["pio", "test", "-e", board.unit_environment,
                                                          "--upload-port", args.port, "--test-port", args.port])
                cases = re.search(r"(\d+) test cases: (\d+) succeeded", unit_output)
                if not cases or int(cases[1]) == 0 or cases[1] != cases[2]:
                    raise RuntimeError("hardware Unity tests did not execute successfully")
                run("erase-test-state", tool + ["erase_flash"])
                run("upload-harness", ["pio", "run", "-d", output, "-e", "board-e2e",
                                         "-t", "upload", "--upload-port", args.port])
                port = serial.Serial(port=None, baudrate=115200, timeout=.2)
                port.dtr, port.rts = False, False
                port.port = args.port
                with port, (output / "events.jsonl").open("w") as log:
                    # Restart only after the host owns the serial stream, so
                    # ready/restore assertions cannot miss fast startup events.
                    port.rts = True
                    time.sleep(.1)
                    port.rts = False
                    monitor = Monitor(port, log, args.minimum_heap)
                    start = time.monotonic()
                    monitor_ok = False
                    try:
                        report["checks"] = monitor.exercise(args.seconds, args.reconnects)
                        report["benchmark"] = summarize(monitor.samples)
                        monitor_ok = True
                    finally:
                        report["sample_count"] = len(monitor.samples)
                        report["stages"].append({"name": "monitor-and-benchmark",
                                                 "seconds": round(time.monotonic() - start, 3),
                                                 "exit_code": 0 if monitor_ok else 1})
            report["status"] = "passed"
        else:
            report["status"] = "built-not-hardware-tested"
    except (Exception, KeyboardInterrupt) as error:
        report["error"] = redact(str(error) or "interrupted", config)
        print(report["error"], file=sys.stderr)
    finally:
        if modified:
            # Do not interrupt the recovery transaction with a second Ctrl+C.
            old_signals = {s: signal.signal(s, signal.SIG_IGN) for s in (signal.SIGINT, signal.SIGTERM)}
            try:
                run("restore-original-flash", tool + ["write_flash", "0", output / "original-flash.bin"])
                report["restoration"] = "verified"
            except Exception:
                report["restoration"] = "failed"
                report["status"] = "failed"
                print(f"RESTORATION FAILED. Keep {output / 'original-flash.bin'}; see README recovery command.",
                      file=sys.stderr)
            finally:
                for number, handler in old_signals.items():
                    signal.signal(number, handler)
        if lock:
            lock.close()
        if previous_term is not None:
            signal.signal(signal.SIGTERM, previous_term)
        if output:
            (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
            print(f"{report['status']}: {output / 'report.json'}")
    return 0 if report["status"] in ("passed", "built-not-hardware-tested") else 1
