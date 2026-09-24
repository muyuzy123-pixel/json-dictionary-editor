#!/bin/sh
# Package already-built preview executables. This never builds, signs or uploads.
set -eu
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
REPO_ROOT=$(CDPATH= cd -- "$PROJECT_DIR/.." && pwd)
: "${SOURCE_COMMIT:?Set SOURCE_COMMIT to the frozen full Git commit}"
: "${RELEASE_TAG:?Set RELEASE_TAG, for example preview-1}"
: "${DIST_DIR:?Set DIST_DIR to the directory containing the built executables}"
BUILD_DIR=${BUILD_DIR:-"$PROJECT_DIR/build-cross"}
export REPO_ROOT SOURCE_COMMIT RELEASE_TAG DIST_DIR BUILD_DIR

python3 - <<'PY'
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import zipfile

repo = Path(os.environ['REPO_ROOT']).resolve()
build = Path(os.environ['BUILD_DIR']).resolve()
dist = Path(os.environ['DIST_DIR']).resolve()
commit = os.environ['SOURCE_COMMIT']
tag = os.environ['RELEASE_TAG']

def require(condition, message):
    if not condition:
        raise SystemExit(message)

def digest(data):
    return hashlib.sha256(data).hexdigest()

require(re.fullmatch(r'[0-9a-f]{40}|[0-9a-f]{64}', commit), 'SOURCE_COMMIT must be a full lowercase Git commit ID.')
require(re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]*', tag), 'RELEASE_TAG has unsupported characters.')
require(build.is_dir() and dist.is_dir(), 'BUILD_DIR and DIST_DIR must already exist.')
require(not dist.is_relative_to(repo), 'Preview packages must be written outside the source repository.')
resolved = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', commit + '^{commit}'], text=True).strip()
require(resolved == commit, 'SOURCE_COMMIT does not resolve to the requested commit.')
metadata = (build / 'build-info.txt').read_text(encoding='utf-8')

def field(name):
    values = re.findall(r'^' + re.escape(name) + r'=(.*)$', metadata, flags=re.MULTILINE)
    require(len(values) == 1, 'Missing or ambiguous build metadata: ' + name)
    return values[0]

require(field('source_commit') == commit, 'Build SOURCE_COMMIT does not match packaging SOURCE_COMMIT.')
require(field('toolchain_archive_sha256') == '2cab02a2e964bd4aae981150a45985d07c657cfa8d244959eb9e2dcc5eedd7b1',
        'This preview notice set is for the recorded LLVM-MinGW 20260616 archive only.')
require('clang version 22.1.8 ' in metadata, 'Unexpected compiler version for this preview notice set.')
inputs_data = (build / 'build-inputs.sha256').read_bytes()
require(digest(inputs_data) == field('inputs_sha256'), 'Build input manifest hash mismatch.')
for line in inputs_data.decode('utf-8').splitlines():
    expected, relative = line.split('  ', 1)
    source = repo / relative
    require(source.is_file() and not source.is_symlink() and source.resolve().is_relative_to(repo),
            'Unsafe or missing build input: ' + relative)
    require(digest(source.read_bytes()) == expected, 'Build input changed after compilation: ' + relative)
    committed = subprocess.check_output(['git', '-C', str(repo), 'show', commit + ':' + relative])
    require(digest(committed) == expected, 'Build input does not match SOURCE_COMMIT: ' + relative)

version_match = re.search(r'VALUE\s+"ProductVersion",\s+"([^"\\]+)\\0"',
                          (repo / 'windows/resources/app.rc').read_text(encoding='utf-8'))
require(version_match is not None, 'Cannot read the Windows resource version.')
version = version_match.group(1)
require(version == field('project_version'), 'Windows resource and build versions differ.')

common = {
    'SampleDictionary.json': repo / 'windows/resources/SampleDictionary.json',
    '使用说明.md': repo / 'windows/README.md',
    'LICENSE': repo / 'LICENSE',
    'THIRD_PARTY_NOTICES.txt': repo / 'THIRD_PARTY_NOTICES.txt',
    'verify_on_windows.ps1': repo / 'windows/scripts/verify_on_windows.ps1',
    'licenses/LLVM-LICENSE.TXT': repo / 'licenses/LLVM-LICENSE.TXT',
    'licenses/COPYING.MinGW-w64-runtime.txt': repo / 'licenses/COPYING.MinGW-w64-runtime.txt',
}
packages = []
# Validate both inputs before creating either final archive.
for arch in ('x64', 'arm64'):
    exe_name = 'JSONDictionaryEditor-Windows-' + arch + '.exe'
    exe = dist / exe_name
    map_name = 'link-' + arch + '.map'
    link_map = build / map_name
    trace_name = 'link-' + arch + '.log'
    link_trace = build / trace_name
    require(exe.is_file() and link_map.is_file() and link_trace.is_file(),
            'Missing executable, link map or verbose linker record for ' + arch)
    exe_data = exe.read_bytes()
    map_data = link_map.read_bytes()
    trace_data = link_trace.read_bytes()
    require('output_sha256=' + digest(exe_data) + ' ' + exe_name in metadata.splitlines(),
            'Executable hash differs from completed build: ' + arch)
    require('link_map_sha256=' + digest(map_data) + ' ' + map_name in metadata.splitlines(),
            'Link map hash differs from completed build: ' + arch)
    require('link_trace_sha256=' + digest(trace_data) + ' ' + trace_name in metadata.splitlines(),
            'Verbose linker record hash differs from completed build: ' + arch)
    # MinGW LLD maps flatten member names; --verbose retains actual archive provenance.
    libraries = sorted(set(re.findall(r'^ld\.lld: Loaded (?:.*[/\\])?([A-Za-z0-9_+.-]+\.a)\(',
                                      trace_data.decode('utf-8'), flags=re.MULTILINE)))
    require('libc++.a' in libraries and 'libmingw32.a' in libraries,
            'Verbose linker record does not identify expected runtime libraries: ' + arch)
    require(not set(libraries).intersection({'libpthread.a', 'libwinpthread.a', 'libwinstorecompat.a'}),
            'Additional runtime notices need review before packaging: ' + arch)
    target = dist / ('JSONDictionaryEditor-Windows-' + arch + '.zip')
    require(not target.exists(), 'Refusing to overwrite an existing package: ' + target.name)
    payload = {name: path.read_bytes() for name, path in common.items()}
    payload['JSONDictionaryEditor.exe'] = exe_data
    payload['SOURCE.json'] = (json.dumps({
        'source_commit': commit, 'tag': tag, 'app_version': version, 'architecture': arch,
    }, ensure_ascii=False, indent=2) + '\n').encode('utf-8')
    payload['SHA256SUMS'] = ''.join(digest(data) + '  ' + name + '\n'
                                  for name, data in sorted(payload.items())).encode('utf-8')
    packages.append((target, payload, libraries))

results = []
for target, payload, libraries in packages:
    temporary = target.with_suffix('.zip.tmp')
    require(not temporary.exists(), 'Refusing to overwrite a previous temporary archive.')
    try:
        with zipfile.ZipFile(temporary, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for name, data in sorted(payload.items()):
                info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
                info.create_system = 3
                info.external_attr = (0o100644 << 16)
                info.compress_type = zipfile.ZIP_DEFLATED
                archive.writestr(info, data)
        with zipfile.ZipFile(temporary) as archive:
            require(archive.testzip() is None, 'ZIP CRC verification failed.')
            require(set(archive.namelist()) == set(payload), 'Unexpected ZIP content.')
            for info in archive.infolist():
                require(archive.read(info) == payload[info.filename], 'Archived bytes changed.')
                require(not any(ord(c) > 127 for c in info.filename) or (info.flag_bits & 0x800),
                        'Non-ASCII ZIP name lacks UTF-8 flag.')
        temporary.rename(target)
    finally:
        if temporary.exists():
            temporary.unlink()
    results.append({'file': target.name, 'sha256': digest(target.read_bytes()),
                    'entries': sorted(payload), 'linked_archives': libraries,
                    'source_commit': commit, 'tag': tag})
    print('PACKAGED: ' + target.name)
report = dist / 'windows-preview-packages.json'
report.write_text(json.dumps(results, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print('Package and linked-runtime report: ' + report.name)
PY
