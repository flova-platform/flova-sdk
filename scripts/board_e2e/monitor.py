"""Bounded serial protocol, reusable acceptance scenarios, and measured summaries."""

import json
import math
import time
from collections import deque

PREFIX = b"FLOVA_E2E "
SAMPLE_FIELDS = (
    "ms", "ready", "wifi", "connected", "heap", "min_heap", "largest", "stack",
    "run_count", "run_total_us", "run_max_us", "reports", "rejected",
    "storage_failures", "dirty", "sdk_work_total_us", "sdk_work_max_us",
)
MEMORY_FIELDS = ("fragmentation", "tls_stack")
EVENT_FIELDS = {
    "sample": SAMPLE_FIELDS,
    "ready": ("temp_present", "temp_milli", "humidity_present"),
    "flush": ("ok", "value_milli"),
    **{name: () for name in ("boot", "started", "paused", "disconnected", "connecting", "failed")},
}


def parse_event(line):
    if not line.startswith(PREFIX):
        if any(marker in line for marker in (b"Guru Meditation", b"Stack smashing", b"panic'ed",
                                             b"Exception (", b"Soft WDT reset", b"wdt reset",
                                             b">>>stack>>>")):
            raise RuntimeError("firmware panic detected")
        return None  # Never retain arbitrary SDK/boot output or payloads.
    value = json.loads(line[len(PREFIX):])
    if not isinstance(value, dict) or value.get("event") not in EVENT_FIELDS:
        raise ValueError("invalid board test event")
    fields = EVENT_FIELDS[value["event"]]
    if value["event"] == "sample":
        fields += tuple(key for key in MEMORY_FIELDS if key in value)
    if any(type(value.get(key)) is not int or value[key] < 0 for key in fields):
        raise ValueError("invalid numeric board test field")
    return {"event": value["event"], **{key: value[key] for key in fields}}


class Monitor:
    def __init__(self, serial, log, minimum_heap=4096, minimum_stack=0, report_interval_ms=200):
        self.serial, self.log = serial, log
        self.minimum_heap = minimum_heap
        self.minimum_stack = minimum_stack
        self.report_interval_ms = report_interval_ms
        self.pending = b""
        self.samples = deque(maxlen=4096)

    def next(self, deadline, allow_boot=False):
        while time.monotonic() < deadline:
            self.pending += self.serial.read_until(b"\n", 2048)
            if allow_boot and not self.pending.startswith(PREFIX):
                # ESP8266 ROM uses 74880 baud, unlike the 115200-baud sketch.
                # Synchronize only during an expected boot, without retaining
                # arbitrary boot bytes or relaxing the typed-frame size bound.
                parse_event(self.pending)  # Still reject known panic signatures.
                offset = self.pending.find(PREFIX)
                if offset < 0:
                    self.pending = self.pending[-(len(PREFIX) - 1):]
                    continue
                self.pending = self.pending[offset:]
            if len(self.pending) > 2048:
                raise RuntimeError("serial line exceeds test protocol bound")
            if not self.pending.endswith(b"\n"):
                continue
            line, self.pending = self.pending, b""
            event = parse_event(line.strip())
            if event is None:
                continue
            self.log.write(json.dumps(event) + "\n")
            self.log.flush()
            if event["event"] == "failed":
                raise RuntimeError("firmware startup/provisioning failed")
            if event["event"] == "boot" and not allow_boot:
                raise RuntimeError("unexpected board reboot")
            if event["event"] == "sample":
                self.samples.append(event)
                if event["rejected"] or event["storage_failures"]:
                    raise RuntimeError("SDK reported rejected telemetry or storage failures")
                if event["min_heap"] < self.minimum_heap:
                    raise RuntimeError("board minimum heap below configured safety floor")
                if event["stack"] < self.minimum_stack:
                    raise RuntimeError("board continuation stack below configured safety floor")
            return event
        raise TimeoutError("timed out waiting for board test event")

    def wait(self, name, predicate=lambda _: True, timeout=60, allow_boot=False):
        deadline = time.monotonic() + timeout
        while True:
            event = self.next(deadline, allow_boot)
            if event["event"] == name and predicate(event):
                return event

    def command(self, value, event):
        self.serial.write(value.encode("ascii"))
        self.serial.flush()
        return self.wait(event)

    def benchmark(self, seconds):
        self.command("s", "started")
        deadline = time.monotonic() + seconds
        readings = []
        while time.monotonic() < deadline:
            event = self.next(deadline + 2)
            if event["event"] == "sample":
                if not event["ready"] or not event["connected"]:
                    raise RuntimeError("connection lost during steady-state benchmark")
                readings.append((event["ms"], event["reports"]))
        # Compare counters over their sampled interval, not unsampled window edges.
        # Leave 20% for real loop/network latency; ACK and readiness gates remain strict.
        if len(readings) < 2 or readings[-1][1] - readings[0][1] < max(0, int((readings[-1][0] - readings[0][0]) / self.report_interval_ms * .8) - 1):
            raise RuntimeError("insufficient successful telemetry for benchmark")
        self.command("p", "paused")
        # Dirty state clears only after matching Link delivery ACKs. Merely
        # joining Wi-Fi or opening a socket is not an ingestion/delivery check.
        self.wait("sample", lambda e: e["ready"] and not e["dirty"], timeout=20)

    def exercise(self, seconds, reconnects):
        initial = self.wait("ready", timeout=120, allow_boot=True)
        if initial["temp_present"] or initial["humidity_present"]:
            raise RuntimeError("test must start with erased SDK state")
        self.benchmark(seconds)
        for _ in range(reconnects):
            self.command("d", "disconnected")
            self.wait("sample", lambda e: not e["wifi"] and not e["connected"])
            self.command("c", "connecting")
            self.wait("sample", lambda e: e["ready"] and e["connected"])
            self.benchmark(3)
        flushed = self.command("f", "flush")
        if flushed["ok"] != 1:
            raise RuntimeError("explicit persistence flush failed")
        self.serial.write(b"r")
        self.serial.flush()
        self.wait("boot", timeout=15, allow_boot=True)
        restored = self.wait("ready", timeout=120)
        if (restored["temp_present"] != 1 or restored["humidity_present"] != 0 or
                restored["temp_milli"] != flushed["value_milli"]):
            raise RuntimeError("persistent/nonpersistent reboot restoration mismatch")
        return {"telemetry_and_link_acks": True, "reconnect_cycles": reconnects,
                "explicit_flush": True, "persistent_reboot_restore": True,
                "nonpersistent_not_restored": True}

    def smoke(self):
        """One provision/report/ACK, without stress, reconnect or persistence tests."""
        initial = self.wait("ready", timeout=45, allow_boot=True)
        if initial["temp_present"] or initial["humidity_present"]:
            raise RuntimeError("test must start with erased SDK state")
        baseline = self.wait("sample", lambda e: e["ready"] and e["connected"], timeout=5)
        self.command("s", "started")
        try:
            self.wait("sample", lambda e: e["ready"] and e["connected"] and
                      e["reports"] > baseline["reports"] and not e["dirty"], timeout=15)
        except TimeoutError as error:
            raise TimeoutError("fresh telemetry delivery ACK not received within 15 seconds") from error
        self.command("p", "paused")
        return {"telemetry_and_link_acks": True, "reconnect_cycles": 0,
                "explicit_flush": False, "persistent_reboot_restore": False,
                "nonpersistent_not_restored": False}


def summarize(samples):
    connected = [s for s in samples if s["ready"] and s["connected"] and s["run_count"]]
    if not connected:
        raise ValueError("no connected benchmark samples")
    peaks = sorted(s["run_max_us"] for s in connected)
    work_peaks = sorted(s["sdk_work_max_us"] for s in connected)
    result = {
        "connected_windows": len(connected),
        "mean_run_us": sum(s["run_total_us"] for s in connected) /
                       sum(s["run_count"] for s in connected),
        "maximum_run_us": max(peaks),
        "p95_window_maximum_us": peaks[math.ceil(len(peaks) * .95) - 1],
        "mean_sdk_work_us": sum(s["sdk_work_total_us"] for s in connected) /
                            sum(s["run_count"] for s in connected),
        "maximum_sdk_work_us": max(work_peaks),
        "p95_sdk_work_window_maximum_us": work_peaks[math.ceil(len(work_peaks) * .95) - 1],
        "minimum_heap_bytes": min(s["min_heap"] for s in samples),
        "minimum_connected_largest_block_bytes": min(s["largest"] for s in connected),
        "minimum_stack_headroom_bytes": min(s["stack"] for s in samples),
        "first_connected_heap_bytes": connected[0]["heap"],
        "last_connected_heap_bytes": connected[-1]["heap"],
    }
    memory = [s for s in connected if all(key in s for key in MEMORY_FIELDS)]
    if memory:
        result.update(maximum_dram_fragmentation_percent=max(s["fragmentation"] for s in memory),
                      maximum_tls_stack_used_bytes=max(s["tls_stack"] for s in memory))
    return result
