#!/usr/bin/env python3
"""Synchronize canonical example source and SDK headers into a Console checkout."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

sdk = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('console', type=Path)
parser.add_argument('--revision', required=True, help='Expected SDK checkout commit SHA')
parser.add_argument('--check', action='store_true')
args = parser.parse_args()
revision = subprocess.check_output(['git', '-C', str(sdk), 'rev-parse', 'HEAD'], text=True).strip()
if args.revision != revision:
    parser.error('SDK checkout does not match --revision')
path = args.console / 'lib/ide/catalog.json'
catalog = json.loads(path.read_text())
catalog['examples'] = json.loads((sdk / 'examples/ide-examples.json').read_text())
for example in catalog['examples']:
    example['files'] = {'src/main.cpp': (sdk / example['source']).read_text()}
# Canonical include closure: independent of the previous Console catalog.
headers = {}
include_roots = [
    'packages/flova-embedded-sdk/include', 'packages/flova-arduino/include',
    'packages/flova-esp32/include', 'packages/flova-esp8266/include',
    'packages/flova-sdk-release/src', 'protocol/generated/firmware',
    'third_party/zcbor/include', 'third_party/coremqtt/source/include',
    'third_party/coremqtt/source/interface', 'third_party/coremqtt/config',
]
for directory in include_roots:
    for header in sorted((sdk / directory).rglob('*.h')):
        headers[str(header.relative_to(sdk))] = header.read_text()
catalog['headers'] = dict(sorted(headers.items()))
compiled_sources = {}
for directory in ['packages', 'third_party', 'protocol/generated']:
    for source in sorted((sdk / directory).rglob('*')):
        if source.is_file() and source.suffix in ('.h', '.c', '.cpp', '.inc'):
            compiled_sources[str(source.relative_to(sdk))] = source.read_text()
compiled_sources['exporter'] = (sdk / 'scripts/export_sdk_release.sh').read_text()
catalog['sdkSourceDigest'] = hashlib.sha256(json.dumps(compiled_sources, sort_keys=True).encode()).hexdigest()
catalog['sdkGitRevision'] = revision
# Content identity prevents edited SDK sources sharing a compiler cache identity.
catalog['sdkRevision'] = hashlib.sha256(json.dumps(
    {'headers': catalog['headers'], 'examples': catalog['examples'], 'git': revision, 'sourceDigest': catalog['sdkSourceDigest']},
    sort_keys=True, separators=(',', ':')).encode()).hexdigest()[:40]
catalog['sdkVersion'] = json.loads((sdk / 'packages/flova-sdk-release/library.json').read_text())['version']
output = json.dumps(catalog, indent=2, ensure_ascii=False) + '\n'
if args.check:
    if output != path.read_text():
        raise SystemExit('Console catalog differs from canonical SDK sources; run sync_console_catalog.py')
else:
    path.write_text(output)

# Bundle the same source for the offline compiler; registry publication is separate.
import gzip
import io
import tarfile
import tempfile
temporary_root = sdk.parent / 'tmp'
temporary_root.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix='flova-console-sdk-', dir=temporary_root) as temporary:
    exported = Path(temporary) / 'FlovaSDK'
    subprocess.run(['sh', str(sdk / 'scripts/export_sdk_release.sh'), str(exported)], check=True)
    archive_buffer = io.BytesIO()
    with tarfile.open(fileobj=archive_buffer, mode='w', format=tarfile.GNU_FORMAT) as archive:
        for file in sorted(exported.rglob('*')):
            relative = file.relative_to(exported)
            if not file.is_file() or (relative.parts[0] not in ('src', 'extras') and str(relative) not in ('LICENSE', 'library.json')):
                continue
            if relative.parts[0] == 'extras' and 'LICENSE' not in file.name and 'UPSTREAM' not in file.name:
                continue
            data = file.read_bytes()
            info = tarfile.TarInfo(str(relative))
            info.size = len(data)
            info.mode = 0o644
            archive.addfile(info, io.BytesIO(data))
    compressed = io.BytesIO()
    with gzip.GzipFile(fileobj=compressed, mode='wb', filename='', mtime=0) as stream:
        stream.write(archive_buffer.getvalue())
    bundle = compressed.getvalue()
bundle_path = args.console / 'lib/ide/sdk/FlovaSDK.tar.gz'
dependencies_path = args.console / 'lib/ide/sdk-dependencies.json'
dependencies = json.loads(dependencies_path.read_text())
# Keep historical pins verifiable for existing projects; only the bundled SDK is default.
dependencies = [entry for entry in dependencies if not entry.get('bundled')]
for entry in dependencies:
    entry['default'] = entry['spec'].startswith('bblanchon/ArduinoJson@')
dependencies.insert(0, {'spec': 'flova-platform/FlovaSDK@' + catalog['sdkVersion'],
                       'sha256': hashlib.sha256(bundle).hexdigest(),
                       'bundled': 'FlovaSDK.tar.gz', 'default': True})
dependency_text = json.dumps(dependencies, indent=2) + '\n'
if args.check:
    if not bundle_path.exists() or bundle_path.read_bytes() != bundle or dependencies_path.read_text() != dependency_text:
        raise SystemExit('Console SDK bundle or dependency pins are out of sync')
else:
    bundle_path.parent.mkdir(parents=True, exist_ok=True)
    bundle_path.write_bytes(bundle)
    dependencies_path.write_text(dependency_text)
