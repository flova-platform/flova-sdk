"""Build, flash transaction, private artifacts, and report ownership."""

import argparse
import errno
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


def open_serial(serial, port_name):
    # CH340 may briefly reject control requests while recovering after upload.
    for attempt in range(5):
        port = serial.Serial(port=None, baudrate=115200, timeout=.2)
        port.dtr, port.rts = False, False
        port.port = port_name
        try:
            port.open()
            return port
        except (OSError, serial.SerialException) as error:
            port.close()
            if getattr(error, "errno", None) != errno.EPROTO or attempt == 4:
                raise
            time.sleep(1)


def read_unity(serial, port_name):
    # PlatformIO's monitor toggles both modem lines on open, which can fail
    # with CH340 EPROTO. Use the same explicit line state as the E2E monitor.
    time.sleep(1)
    port = open_serial(serial, port_name)
    with port:
        port.reset_input_buffer()
        port.rts = True
        time.sleep(.1)
        port.rts = False
        deadline = time.monotonic() + 45
        while time.monotonic() < deadline:
            line = port.read_until(b"\n", 2048).decode("utf-8", errors="ignore")
            if ":FAIL" in line:
                raise RuntimeError("on-board Unity assertion failed")
            result = re.search(r"(\d+) Tests (\d+) Failures (\d+) Ignored", line)
            if result:
                tests, failures, ignored = map(int, result.groups())
                if not tests or failures or ignored:
                    raise RuntimeError("on-board Unity tests did not all pass")
                return tests
        raise TimeoutError("timed out waiting for on-board Unity result")


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
    (output / "private_config.h").write_text(header + f"#define TEST_EPOCH {int(time.time())}\n#define TEST_REPORT_INTERVAL_MS {200 if config.get('benchmark') else 60000}\n")
    includes = [output, *[ROOT / "packages" / name / "include" for name in (
        "flova-embedded-sdk", "flova-arduino", "flova-esp32", "flova-esp8266")]]
    flags = "\n".join(f'  -I"{path}"' for path in includes)
    # espressif8266 4.2.1's esptool 3.0 repeats DTR transitions that fail on
    # this CH340/Linux adapter. Pin the same reviewed uploader for both suites.
    uploader = "platform_packages = platformio/tool-esptoolpy@2.41100.0" if board.chip == "esp8266" else ""
    (output / "platformio.ini").write_text(f"""[platformio]
extra_configs = {ROOT / 'platformio.ini'}
src_dir = {FIRMWARE}
test_dir = {ROOT / 'test'}

[env:board-unit]
extends = env:{board.unit_environment}
{uploader}
upload_speed = 460800
lib_extra_dirs = {ROOT / 'packages'}
  {ROOT / 'third_party'}
  {ROOT / 'protocol/generated'}
build_flags =
  ${{env:{board.unit_environment}.build_flags}}
{flags}

[env:board-e2e]
extends = env:{board.environment}
{uploader}
upload_speed = 460800
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
    if board.chip == "esp8266":
        with (output / "platformio.ini").open("a") as project:
            project.write(f"""
[env:board-e2e-stock]
extends = env:board-e2e
build_flags =
  ${{env.build_flags}}
  ${{esp8266_profile.build_flags}}
{flags}
  -DFLOVA_DEVELOPMENT_E2E=1
  -DFLOVA_LOGGING_ENABLED=0
  -fstack-usage
""")


def arguments(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", required=True, type=Path, help="private development JSON configuration")
    parser.add_argument("--board", choices=BOARDS, default="esp32")
    parser.add_argument("--output", type=Path, help="new private artifact directory; must not exist")
    parser.add_argument("--flash", action="store_true",
                        help="authorize backup, test uploads, full erase, and original-flash restoration")
    parser.add_argument("--port", help="explicit serial port; required with --flash")
    parser.add_argument("--benchmark", action="store_true", help="explicitly enable 200 ms stress telemetry during acceptance")
    parser.add_argument("--keep-running", action="store_true",
                        help="after acceptance keep telemetry/TLS ingress alive until interrupted, then restore flash")
    parser.add_argument("--seconds", type=int, default=20, help="steady telemetry benchmark duration (5–1800)")
    parser.add_argument("--reconnects", type=int, default=3, help="Wi-Fi loss/recovery cycles (0–100)")
    parser.add_argument("--minimum-heap", type=int, default=8192, help="minimum free-heap safety floor in bytes")
    args = parser.parse_args(argv)
    if args.flash and not args.port:
        parser.error("--flash requires an explicit --port")
    if args.keep_running and not args.flash:
        parser.error("--keep-running requires --flash")
    if not 5 <= args.seconds <= 1800 or not 0 <= args.reconnects <= 100 or args.minimum_heap < (8192 if args.board == "esp8266" else 4096):
        parser.error("invalid duration/reconnect count or heap floor (ESP8266 minimum 8192, ESP32 minimum 4096)")
    return args


def main(argv=None):
    args = arguments(argv)
    os.umask(0o077)  # Headers, firmware binaries and flash backups contain credentials.
    config = {}
    output = None
    report = {"board": args.board, "mode": "hardware" if args.flash else "build-only",
              "status": "failed", "stages": [], "restoration": "not-needed",
              "workload": {"steady_seconds": args.seconds, "reconnects": args.reconnects,
                           "report_interval_ms": 200 if args.benchmark else 60000, "deferred_flush_ms": 5000,
                           "minimum_heap_bytes": args.minimum_heap}}
    modified = False
    lock = None
    tool = None
    previous_term = None

    def run(name, command, cwd=ROOT, timeout=300):
        print(f"[{name}]", flush=True)
        start = time.monotonic()
        for attempt in range(5):
            result = subprocess.run([str(part) for part in command], cwd=cwd,
                                    capture_output=True, text=True, timeout=timeout)
            text = result.stdout + result.stderr
            if not result.returncode or "[Errno 71] Protocol error" not in text or attempt == 4:
                break
            time.sleep(1)
        (output / f"{name}.log").write_text(redact(text, config))
        report["stages"].append({"name": name, "seconds": round(time.monotonic() - start, 3),
                                 "exit_code": result.returncode})
        if result.returncode:
            raise RuntimeError(f"{name} failed; see {output / (name + '.log')}")
        return text

    try:
        config = load_config(args.config)
        config["benchmark"] = args.benchmark
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
        report["heap_minimum_method"] = ("sampled DRAM at loop/run/report boundaries" if args.board == "esp8266"
                                         else "ESP32 lifetime heap low-water counter")
        report["certificate_validity"] = run("certificate-validity", ["openssl", "x509", "-in",
                                                   output / "server.crt", "-noout", "-dates"]).strip()
        run("compile-unit-tests", ["pio", "test", "-d", output, "-e", "board-unit",
                                    "--without-uploading", "--without-testing"])
        build = run("compile-harness", ["pio", "run", "-d", output, "-e", "board-e2e"])
        report["build_resources"] = [line.strip() for line in build.splitlines()
                                     if any(label in line for label in ("RAM:", "Flash:", "IRAM:"))]
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
                run("upload-unit-tests", ["pio", "test", "-d", output, "-e", "board-unit",
                                          "--upload-port", args.port, "--without-testing"])
                report["on_board_tests_passed"] = read_unity(serial, args.port)
                run("erase-test-state", tool + ["erase_flash"])
                run("upload-harness", ["pio", "run", "-d", output, "-e", "board-e2e",
                                         "-t", "upload", "--upload-port", args.port])
                time.sleep(1)
                port = open_serial(serial, args.port)
                with port, (output / "events.jsonl").open("w") as log:
                    # Restart only after the host owns the serial stream, so
                    # ready/restore assertions cannot miss fast startup events.
                    port.reset_input_buffer()
                    port.rts = True
                    time.sleep(.1)
                    port.rts = False
                    monitor = Monitor(port, log, args.minimum_heap, 1024 if args.board == "esp8266" else 0, 200 if args.benchmark else 60000)
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
                    if args.keep_running:
                        monitor.command("l", "started")
                        report.update(status="passed", restoration="pending-live-session", live=True,
                                      pid=os.getpid(), serial_port=args.port)
                        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
                        print(f"Acceptance passed; live telemetry active: {output / 'report.json'}", flush=True)
                        try:
                            while True:
                                event = monitor.next(time.monotonic() + 60)
                                if event["event"] == "sample":
                                    (output / "live.json").write_text(json.dumps(event) + "\n")
                        except KeyboardInterrupt:
                            report["live"] = False
            report["status"] = "passed"
        else:
            report["status"] = "built-not-hardware-tested"
    except (Exception, KeyboardInterrupt) as error:
        report.update(status="failed", live=False)
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
