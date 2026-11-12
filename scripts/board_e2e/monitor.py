"""Bounded serial protocol, reusable acceptance scenarios, and measured summaries."""

import json
import math
import time

PREFIX = b"FLOVA_E2E "
SAMPLE_FIELDS = (
    "ms", "ready", "wifi", "connected", "heap", "min_heap", "largest", "stack",
    "run_count", "run_total_us", "run_max_us", "reports", "rejected",
    "storage_failures", "dirty",
)
EVENT_FIELDS = {
    "sample": SAMPLE_FIELDS,
    "ready": ("temp_present", "temp_milli", "humidity_present"),
    "flush": ("ok", "value_milli"),
    **{name: () for name in ("boot", "started", "paused", "disconnected", "connecting", "failed")},
}


def parse_event(line):
    if not line.startswith(PREFIX):
        if any(marker in line for marker in (b"Guru Meditation", b"Stack smashing", b"panic'ed")):
            raise RuntimeError("firmware panic detected")
        return None  # Never retain arbitrary SDK/boot output or payloads.
    value = json.loads(line[len(PREFIX):])
    if not isinstance(value, dict) or value.get("event") not in EVENT_FIELDS:
        raise ValueError("invalid board test event")
    fields = EVENT_FIELDS[value["event"]]
    if any(type(value.get(key)) is not int or value[key] < 0 for key in fields):
        raise ValueError("invalid numeric board test field")
    return {"event": value["event"], **{key: value[key] for key in fields}}


class Monitor:
    def __init__(self, serial, log, minimum_heap=4096):
        self.serial, self.log = serial, log
        self.minimum_heap = minimum_heap
        self.pending = b""
        self.samples = []

    def next(self, deadline, allow_boot=False):
        while time.monotonic() < deadline:
            self.pending += self.serial.read_until(b"\n", 2048)
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
                readings.append(event["reports"])
        if len(readings) < 2 or readings[-1] - readings[0] < seconds * 2:
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


def summarize(samples):
    connected = [s for s in samples if s["ready"] and s["connected"] and s["run_count"]]
    if not connected:
        raise ValueError("no connected benchmark samples")
    peaks = sorted(s["run_max_us"] for s in connected)
    return {
        "connected_windows": len(connected),
        "mean_run_us": sum(s["run_total_us"] for s in connected) /
                       sum(s["run_count"] for s in connected),
        "maximum_run_us": max(peaks),
        "p95_window_maximum_us": peaks[math.ceil(len(peaks) * .95) - 1],
        "minimum_heap_bytes": min(s["min_heap"] for s in samples),
        "minimum_connected_largest_block_bytes": min(s["largest"] for s in connected),
        "minimum_stack_headroom_bytes": min(s["stack"] for s in samples),
    }
