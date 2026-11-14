#!/usr/bin/env python3
"""Bounded parallel MQTT regression checks; no accounts, services or boards changed."""

from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess
import sys
import time

SDK = Path(__file__).resolve().parents[1]


def check(name, directory, commands):
    started = time.monotonic()
    for command in commands:
        try:
            result = subprocess.run(command, cwd=directory, capture_output=True, text=True,
                                    timeout=max(1, 120 - (time.monotonic() - started)))
        except (OSError, subprocess.TimeoutExpired) as error:
            return name, False, time.monotonic() - started, str(error)
        if result.returncode:
            return name, False, time.monotonic() - started, result.stdout + result.stderr
    return name, True, time.monotonic() - started, ""


def main():
    jobs = [
        ("SDK", SDK, [
            ["cmake", "-S", ".", "-B", "build/host"],
            ["cmake", "--build", "build/host", "--parallel", "2"],
            ["ctest", "--test-dir", "build/host", "--output-on-failure"],
            [sys.executable, "-m", "unittest", "discover", "-s", "scripts/board_e2e", "-t", "scripts", "-q"],
        ]),
        ("Engine (no DB)", SDK.parent / "flova-engine", [[
            "mix", "test", "test/flova_engine/mqtt", "test/flova_engine/device_link",
            "test/flova_engine_web/controllers/mqtt_native_webhook_controller_test.exs",
            "test/flova_engine_web/controllers/mqtt_webhook_controller_test.exs",
            "test/flova_engine_web/controllers/device_link_controller_test.exs",
        ]]),
        ("Console types", SDK.parent / "flova-console", [["npx", "--no-install", "tsc", "--noEmit"]]),
        ("PWA", SDK.parent / "flova-studio", [
            ["npm", "run", "typecheck", "-w", "@flova/app"],
            ["npx", "--no-install", "vitest", "run",
             "apps/app/src/provisioning/mqttEndpoint.test.ts",
             "apps/app/src/provisioning/deviceProvisioningClient.test.ts"],
        ]),
    ]
    started = time.monotonic()
    with ThreadPoolExecutor(max_workers=len(jobs)) as pool:
        results = list(pool.map(lambda job: check(*job), jobs))
    for name, passed, seconds, detail in results:
        print(f'{"PASS" if passed else "FAIL"} {name} ({seconds:.1f}s)')
        if detail:
            print(detail)
    print(f"Total: {time.monotonic() - started:.1f}s; DB, physical boards and browser acceptance not tested.")
    return 0 if all(result[1] for result in results) else 1


if __name__ == "__main__":
    sys.exit(main())
