import io
import json
from pathlib import Path
import subprocess
import tempfile
import time
import unittest
from unittest.mock import MagicMock, patch

from board_e2e.boards import load_config, redact
from board_e2e.monitor import Monitor, parse_event, summarize, SAMPLE_FIELDS
from board_e2e import runner


def configuration():
    return {"development": True, "wifi_ssid": "test-network", "wifi_password": "test-password",
            "provision_token": "test-token", "lan_address": "192.168.1.109"}


def sample(**changes):
    fields = dict.fromkeys(SAMPLE_FIELDS, 0)
    fields.update(ready=1, connected=1, heap=150000, min_heap=140000, largest=77000,
                  stack=4700, run_count=100, run_total_us=1000, run_max_us=100)
    fields.update(changes)
    return {"event": "sample", **fields}


class ConfigurationTests(unittest.TestCase):
    def test_development_and_private_lan_are_required(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "config.json"
            for updates in ({"development": False}, {"lan_address": "8.8.8.8"},
                            {"lan_address": "127.0.0.1"}, {"provision_token": "x" * 65},
                            {"engine_port": True}, {"tls_port": 4000}):
                path.write_text(json.dumps({**configuration(), **updates}))
                with self.subTest(updates=updates), self.assertRaises(ValueError):
                    load_config(path)
            path.write_text(json.dumps(configuration()))
            self.assertEqual(load_config(path)["engine_port"], 4000)

    def test_secrets_and_cpp_escaped_secrets_are_redacted(self):
        config = {**configuration(), "wifi_password": 'pass"word'}
        text = 'pass"word pass\\"word test-token test-network'
        self.assertEqual(redact(text, config), "[redacted] [redacted] [redacted] [redacted]")


class MonitorTests(unittest.TestCase):
    def test_only_typed_events_are_retained(self):
        self.assertIsNone(parse_event(b"arbitrary SDK output"))
        self.assertEqual(parse_event(b'FLOVA_E2E {"event":"boot","secret":"ignored"}'),
                         {"event": "boot"})
        for value in ({"event": "unknown"}, {"event": "ready"},
                      {**sample(), "heap": "not numeric"}):
            with self.assertRaises(ValueError):
                parse_event(b"FLOVA_E2E " + json.dumps(value).encode())

    def test_partial_lines_are_reassembled_and_panics_fail(self):
        serial = MagicMock()
        serial.read_until.side_effect = [b"FLOVA_", b'E2E {"event":"boot"}\n']
        monitor = Monitor(serial, io.StringIO())
        self.assertEqual(monitor.next(time.monotonic() + 1, allow_boot=True)["event"], "boot")
        with self.assertRaises(RuntimeError):
            parse_event(b"Guru Meditation Error: test panic")

    def test_unexpected_reboot_oversize_and_storage_failures_fail(self):
        for line in (b'FLOVA_E2E {"event":"boot"}\n', b"x" * 2049,
                     b"FLOVA_E2E " + json.dumps(sample(storage_failures=1)).encode() + b"\n",
                     b"FLOVA_E2E " + json.dumps(sample(min_heap=4095)).encode() + b"\n"):
            serial = MagicMock()
            serial.read_until.return_value = line
            with self.subTest(line=line[:40]), self.assertRaises(RuntimeError):
                Monitor(serial, io.StringIO()).next(time.monotonic() + 1)

    def test_persistence_restore_mismatch_fails(self):
        monitor = Monitor(MagicMock(), io.StringIO())
        monitor.benchmark = MagicMock()
        monitor.command = MagicMock(return_value={"ok": 1, "value_milli": 24900})
        monitor.wait = MagicMock(side_effect=[
            {"temp_present": 0, "humidity_present": 0}, {"event": "boot"},
            {"temp_present": 1, "humidity_present": 1, "temp_milli": 24900},
        ])
        with self.assertRaisesRegex(RuntimeError, "restoration mismatch"):
            monitor.exercise(5, 0)

    def test_window_percentile_is_not_labeled_loop_percentile(self):
        result = summarize([sample(), sample(run_max_us=500, run_total_us=3000)])
        self.assertEqual(result["mean_run_us"], 20)
        self.assertEqual(result["p95_window_maximum_us"], 500)
        self.assertEqual(result["minimum_heap_bytes"], 140000)


class RunnerSafetyTests(unittest.TestCase):
    def test_flash_requires_explicit_port(self):
        with patch("sys.stderr", io.StringIO()), self.assertRaises(SystemExit):
            runner.arguments(["--config", "private.json", "--flash"])

    def test_build_only_never_opens_hardware_or_ingress(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config = root / "config.json"
            config.write_text(json.dumps(configuration()))
            output = root / "run"
            with patch.object(runner, "ROOT", root), \
                 patch.object(runner, "make_certificates"), \
                 patch.object(runner, "prepare_project"), \
                 patch.object(runner, "LocalIngress") as ingress, \
                 patch.object(runner, "esptool_path") as tool, \
                 patch.object(runner.shutil, "which", return_value="tool"), \
                 patch.object(runner.socket, "create_connection") as network, \
                 patch.object(runner.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, "", "")), \
                 patch.object(runner.subprocess, "check_output", return_value="test-revision"), \
                 patch("sys.stdout", io.StringIO()):
                result = runner.main(["--config", str(config), "--output", str(output)])
                ingress.assert_not_called()
                tool.assert_not_called()
                network.assert_not_called()
            self.assertEqual(result, 0)
            self.assertEqual(json.loads((output / "report.json").read_text())["status"],
                             "built-not-hardware-tested")

    def test_existing_output_is_never_overwritten(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config = root / "config.json"
            config.write_text(json.dumps(configuration()))
            output = root / "run"
            output.mkdir()
            report = output / "report.json"
            report.write_text("existing user artifact")
            with patch.object(runner.shutil, "which", return_value="tool"), \
                 patch("sys.stderr", io.StringIO()):
                result = runner.main(["--config", str(config), "--output", str(output)])
            self.assertEqual(result, 1)
            self.assertEqual(report.read_text(), "existing user artifact")

    def test_failed_upload_still_restores_full_flash(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / ".local").mkdir()
            config = root / "config.json"
            config.write_text(json.dumps(configuration()))
            output = root / "run"
            commands = []

            def run(command, **_):
                commands.append(command)
                text, code = "", 0
                if "flash_id" in command:
                    text = "Detected flash size: 4MB"
                elif "read_flash" in command:
                    Path(command[-1]).write_bytes(b"x" * (4 * 1024 * 1024))
                elif "--upload-port" in command:
                    text, code = "simulated test upload failure", 1
                return subprocess.CompletedProcess(command, code, text, "")

            with patch.object(runner, "ROOT", root), \
                 patch.object(runner, "make_certificates"), \
                 patch.object(runner, "prepare_project"), \
                 patch.object(runner, "LocalIngress"), \
                 patch.object(runner, "esptool_path", return_value=Path("esptool.py")), \
                 patch.object(runner.shutil, "which", return_value="tool"), \
                 patch.object(runner.socket, "create_connection"), \
                 patch.object(runner.subprocess, "run", side_effect=run), \
                 patch.object(runner.subprocess, "check_output", return_value="test-revision"), \
                 patch.dict("sys.modules", {"serial": MagicMock()}), \
                 patch("sys.stdout", io.StringIO()), patch("sys.stderr", io.StringIO()):
                result = runner.main(["--config", str(config), "--output", str(output),
                                      "--flash", "--port", "/dev/test-board"])
            report = json.loads((output / "report.json").read_text())
            self.assertEqual(result, 1)
            self.assertEqual(report["restoration"], "verified")
            self.assertTrue(any("write_flash" in c for c in commands))
            self.assertFalse(any("erase_flash" in c for c in commands))


if __name__ == "__main__":
    unittest.main()
