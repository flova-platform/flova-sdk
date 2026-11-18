import io
import json
from pathlib import Path
import subprocess
import tempfile
import time
import unittest
from unittest.mock import MagicMock, patch

from board_e2e.boards import BOARDS, load_config, redact
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
    def test_esp8266_uses_its_own_board_and_full_flash_profile(self):
        board = BOARDS["esp8266"]
        self.assertEqual((board.environment, board.unit_environment, board.chip, board.header),
                         ("datastream-api-esp8266", "test-esp8266", "esp8266", "platform_esp8266.h"))
        self.assertEqual(board.flash_bytes, 4 * 1024 * 1024)

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
    def test_smoke_requires_new_report_and_clean_delivery_ack(self):
        monitor = Monitor(MagicMock(), io.StringIO())
        monitor.command = MagicMock()
        monitor.wait = MagicMock(side_effect=[
            {"temp_present": 0, "humidity_present": 0}, sample(reports=4), sample(reports=5),
        ])
        result = monitor.smoke()
        predicate = monitor.wait.call_args_list[2].args[1]
        self.assertFalse(predicate(sample(reports=4)))
        self.assertFalse(predicate(sample(reports=5, dirty=1)))
        self.assertFalse(predicate(sample(reports=5, connected=0)))
        self.assertTrue(predicate(sample(reports=5)))
        self.assertTrue(result["telemetry_and_link_acks"])
        self.assertFalse(result["persistent_reboot_restore"])
        self.assertEqual(monitor.command.call_args_list, [(("s", "started"),), (("p", "paused"),)])

    def test_smoke_timeout_is_not_success(self):
        monitor = Monitor(MagicMock(), io.StringIO())
        monitor.command = MagicMock()
        monitor.wait = MagicMock(side_effect=[
            {"temp_present": 0, "humidity_present": 0}, sample(), TimeoutError("missing ACK"),
        ])
        with self.assertRaisesRegex(TimeoutError, "telemetry delivery ACK"):
            monitor.smoke()

    def test_short_benchmark_counts_only_the_observed_sample_interval(self):
        for reports, passes in [([4, 9, 13], True), ([4, 4, 4], False)]:
            monitor = Monitor(MagicMock(), io.StringIO(), report_interval_ms=200)
            monitor.command = MagicMock()
            monitor.wait = MagicMock()
            monitor.next = MagicMock(side_effect=[
                sample(ms=1000 * (i + 1), reports=count)
                for i, count in enumerate(reports)
            ])
            with self.subTest(reports=reports), patch("board_e2e.monitor.time.monotonic", side_effect=[0, 0, 1, 2, 3]):
                if passes:
                    monitor.benchmark(3)
                    monitor.wait.assert_called_once()
                else:
                    with self.assertRaisesRegex(RuntimeError, "insufficient successful telemetry"):
                        monitor.benchmark(3)
                    monitor.wait.assert_not_called()

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
        for line in (b"Exception (28):", b"Soft WDT reset", b">>>stack>>>"):
            with self.subTest(line=line), self.assertRaises(RuntimeError):
                parse_event(line)

    def test_esp8266_memory_fields_are_typed_and_summarized(self):
        reading = sample(fragmentation=12, tls_stack=5000)
        parsed = parse_event(b"FLOVA_E2E " + json.dumps(reading).encode())
        result = summarize([parsed])
        self.assertEqual(result["maximum_dram_fragmentation_percent"], 12)
        self.assertEqual(result["maximum_tls_stack_used_bytes"], 5000)
        reading["fragmentation"] = "unknown"
        with self.assertRaises(ValueError):
            parse_event(b"FLOVA_E2E " + json.dumps(reading).encode())

    def test_expected_boot_resynchronizes_after_rom_noise_but_preserves_frame_bound(self):
        serial = MagicMock()
        serial.read_until.side_effect = [b"\x02" * 2048, b"\x04" * 2048,
                                        b'romFLOVA_E2E {"event":"boot"}\n']
        monitor = Monitor(serial, io.StringIO())
        self.assertEqual(monitor.next(time.monotonic() + 1, allow_boot=True)["event"], "boot")
        for line in (b'FLOVA_E2E ' + b'x' * 2049, b'Exception (28):'):
            serial.read_until.side_effect = None
            serial.read_until.return_value = line
            with self.assertRaises(RuntimeError):
                Monitor(serial, io.StringIO()).next(time.monotonic() + 1, allow_boot=True)

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
        result = summarize([sample(sdk_work_total_us=2000, sdk_work_max_us=200),
                            sample(run_max_us=500, run_total_us=3000,
                                   sdk_work_total_us=6000, sdk_work_max_us=800)])
        self.assertEqual(result["mean_run_us"], 20)
        self.assertEqual(result["p95_window_maximum_us"], 500)
        self.assertEqual(result["minimum_heap_bytes"], 140000)
        self.assertEqual(result["mean_sdk_work_us"], 40)
        self.assertEqual(result["maximum_sdk_work_us"], 800)
        self.assertEqual(result["p95_sdk_work_window_maximum_us"], 800)


class RunnerSafetyTests(unittest.TestCase):
    def test_hardware_smoke_requires_identity_but_build_only_does_not(self):
        args = ["--config", "private.json", "--smoke"]
        self.assertIsNone(runner.arguments(args).identity)
        with patch("sys.stderr", io.StringIO()), self.assertRaises(SystemExit):
            runner.arguments(args + ["--flash", "--port", "/dev/test"])
        parsed = runner.arguments(args + ["--flash", "--port", "/dev/test", "--identity", "identity.json"])
        self.assertEqual(parsed.identity, Path("identity.json"))

    def test_smoke_rejects_long_running_modes(self):
        for flag in ("--benchmark", "--keep-running"):
            with self.subTest(flag=flag), patch("sys.stderr", io.StringIO()), self.assertRaises(SystemExit):
                runner.arguments(["--config", "private.json", "--smoke", "--flash", "--port", "/dev/test", flag])

    def test_generated_endpoint_is_mqtt_not_websocket(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            (output / "ca.crt").write_text("test CA")
            runner.prepare_project(output, {**configuration(), "tls_port": 9443}, BOARDS["esp8266"])
            header = (output / "private_config.h").read_text()
            self.assertIn('"mqtts://192.168.1.109:9443"', header)
            self.assertNotIn("wss://", header)
            self.assertIn("#define TEST_COMMAND_CHECK_ENABLED 0", header)
            runner.prepare_project(output, {**configuration(), "tls_port": 9443, "command_check": True}, BOARDS["esp8266"])
            self.assertIn("#define TEST_COMMAND_CHECK_ENABLED 1", (output / "private_config.h").read_text())

    def test_keep_running_requires_hardware(self):
        with patch("sys.stderr", io.StringIO()), self.assertRaises(SystemExit):
            runner.arguments(["--config", "private.json", "--keep-running"])

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

    def test_failed_upload_restoration_matches_fixture_mode(self):
        for disposable in (False, True):
            with self.subTest(disposable=disposable):
                self.check_failed_upload_restoration(disposable)

    def check_failed_upload_restoration(self, disposable):
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
                args = ["--config", str(config), "--output", str(output),
                        "--flash", "--keep-running", "--port", "/dev/test-board"]
                if disposable:
                    args.append("--disposable")
                result = runner.main(args)
            report = json.loads((output / "report.json").read_text())
            self.assertEqual(result, 1)
            self.assertEqual(report["restoration"], "skipped-disposable" if disposable else "verified")
            self.assertEqual(any("read_flash" in c for c in commands), not disposable)
            self.assertEqual(any("write_flash" in c for c in commands), not disposable)
            self.assertFalse(any("erase_flash" in c for c in commands))

    def test_live_session_writes_acceptance_before_wait_and_restores_on_stop(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / ".local").mkdir()
            config = root / "config.json"
            config.write_text(json.dumps(configuration()))
            output = root / "run"
            commands = []

            def run(command, **_):
                commands.append(command)
                text = ""
                if "flash_id" in command:
                    text = "Detected flash size: 4MB"
                elif "read_flash" in command:
                    Path(command[-1]).write_bytes(b"x" * (4 * 1024 * 1024))
                elif "--test-port" in command:
                    text = "6 test cases: 6 succeeded"
                return subprocess.CompletedProcess(command, 0, text, "")

            def stop(*_):
                report = json.loads((output / "report.json").read_text())
                self.assertEqual(report["status"], "passed")
                self.assertEqual(report["restoration"], "pending-live-session")
                raise KeyboardInterrupt()

            with patch.object(runner, "ROOT", root), \
                 patch.object(runner, "make_certificates"), \
                 patch.object(runner, "prepare_project"), \
                 patch.object(runner, "LocalIngress"), \
                 patch.object(runner, "esptool_path", return_value=Path("esptool.py")), \
                 patch.object(runner.shutil, "which", return_value="tool"), \
                 patch.object(runner.socket, "create_connection"), \
                 patch.object(runner.subprocess, "run", side_effect=run), \
                 patch.object(runner.subprocess, "check_output", return_value="test-revision"), \
                 patch.object(runner, "read_unity", return_value=6), \
                 patch.object(runner, "Monitor") as monitor, \
                 patch.object(runner, "summarize", return_value={}), \
                 patch.dict("sys.modules", {"serial": MagicMock()}), \
                 patch("sys.stdout", io.StringIO()):
                monitor.return_value.exercise.return_value = {"telemetry_and_link_acks": True}
                monitor.return_value.next.side_effect = stop
                result = runner.main(["--config", str(config), "--output", str(output),
                                      "--board", "esp8266", "--flash", "--keep-running",
                                      "--port", "/dev/test-board"])
            report = json.loads((output / "report.json").read_text())
            self.assertEqual(result, 0)
            self.assertEqual(report["restoration"], "verified")
            self.assertFalse(report["live"])
            self.assertTrue(any("write_flash" in c for c in commands))


class UnitySerialTests(unittest.TestCase):
    def test_serial_noise_never_counts_as_a_test_result(self):
        serial = MagicMock()
        port = serial.Serial.return_value
        port.read_until.side_effect = [b"rom noise", b"6 Tests 0 Failures 0 Ignored\n"]
        self.assertEqual(runner.read_unity(serial, "/dev/test-board"), 6)
        port.read_until.side_effect = [b"test_main.cpp:14:test_case:FAIL\n"]
        with self.assertRaises(RuntimeError):
            runner.read_unity(serial, "/dev/test-board")


if __name__ == "__main__":
    unittest.main()
