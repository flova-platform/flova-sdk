#!/usr/bin/env python3
"""Fail when critical constrained-target stack frames regress."""

import argparse
import re
from pathlib import Path


ESP8266_LIMITS = (
    ("setup", re.compile(r"\bvoid setup\(\)$"), 128),
    ("acceptProvisioning", re.compile(r"acceptProvisioning\("), 384),
    ("completeBootstrap", re.compile(r"completeBootstrap\("), 384),
    ("drainConfiguration", re.compile(r"drainConfiguration\("), 384),
    ("startConfigurationRestore", re.compile(r"startConfigurationRestore\("), 256),
    ("stepConfigurationWork", re.compile(r"stepConfigurationWork\(\)"), 256),
    ("stepConfigurationCandidate", re.compile(r"stepConfigurationCandidate\(\)"), 256),
    ("stepConfigurationDigest", re.compile(r"stepConfigurationDigest\(\)"), 256),
    ("stepConfigurationSemantic", re.compile(r"stepConfigurationSemantic\(\)"), 256),
    ("stepConfigurationReferences", re.compile(r"stepConfigurationReferences\(\)"), 256),
    ("stepConfigurationApply", re.compile(r"stepConfigurationApply\(\)"), 256),
    ("validateConfigurationUnit", re.compile(r"validateConfigurationUnit\("), 256),
    ("finishConfigurationApply", re.compile(r"finishConfigurationApply\(\)"), 256),
    ("finishConfigurationVerification", re.compile(r"finishConfigurationVerification\(\)"), 256),
    ("processOta", re.compile(r"FlovaClient::processOta\(\)"), 512),
    ("FlovaEsp8266Platform::installOta", re.compile(r"FlovaEsp8266Platform::installOta\("), 768),
    ("sendDatastreamBinding", re.compile(r"sendDatastreamBinding\(\)"), 768),
    ("handleDatastreamBound", re.compile(r"handleDatastreamBound\("), 768),
    ("dispatchPendingFrame", re.compile(r"dispatchPendingFrame\(\)"), 256),
    ("Device::receive", re.compile(r"Device::receive\(const flova::Message&\)"), 128),
    ("ArduinoFlovaLink::accept", re.compile(r"ArduinoFlovaLink::accept\("), 64),
    ("Device::reserveCommand", re.compile(r"Device::reserveCommand\("), 384),
    ("Device::replyStorageUnavailable", re.compile(r"Device::replyStorageUnavailable\("), 384),
    ("Device::storeResult", re.compile(r"Device::storeResult\("), 128),
    ("Device::publishBatch", re.compile(r"Device::publishBatch\("), 128),
    ("ArduinoFlovaLink::sendStateBatch", re.compile(r"ArduinoFlovaLink::sendStateBatch\("), 512),
    ("ArduinoDeviceLink::publishState", re.compile(r"ArduinoDeviceLink::publishState\("), 384),
)

OPTIONAL_LIMITS = {
    "sendDatastreamBinding",
    # With schedule restoration removed, these are inlined into the checked
    # stepConfigurationWork frame (confirmed by the linked symbol table).
    "stepConfigurationSemantic",
    "finishConfigurationApply",
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "profiles", nargs="*", default=["universal-esp8266"],
        choices=["universal-esp8266", "universal-esp8266-guardrails"],
    )
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    failed = False
    for target in args.profiles:
        directory = root / ".pio" / "build" / target
        limits = ESP8266_LIMITS
        if target == "universal-esp8266-guardrails":
            limits += (
                ("stepGuardrailReferences", re.compile(r"stepGuardrailReferences\("), 256),
                ("validateOfflineGuardrailStream", re.compile(r"validateOfflineGuardrailStream\("), 256),
                ("applyGuardrailSafe", re.compile(r"applyGuardrailSafe\("), 128),
            )
        usages = {name: -1 for name, _, _ in limits}
        for path in directory.rglob("*.su"):
            for line in path.read_text(errors="replace").splitlines():
                parts = line.split("\t")
                if len(parts) < 3 or not parts[1].isdigit():
                    continue
                signature, size = parts[0], int(parts[1])
                for name, pattern, _ in limits:
                    if pattern.search(signature):
                        usages[name] = max(usages[name], size)

        for name, _, limit in limits:
            actual = usages[name]
            if actual < 0:
                if name in OPTIONAL_LIMITS:
                    print(f"{target} {name}: inlined (caller frame checked)")
                else:
                    print(f"missing {target} stack-usage record: {name}")
                    failed = True
            elif actual > limit:
                print(f"{target} stack frame too large: {name} uses {actual} bytes (limit {limit})")
                failed = True
            else:
                print(f"{target} {name}: {actual} / {limit} bytes")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
