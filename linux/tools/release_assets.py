#!/usr/bin/env python3
"""Prepare existing-test-backed Linux assets and verify a cloud download."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import re
import shutil
import struct
import subprocess
import tarfile
import tempfile
import xml.etree.ElementTree as ET

from qt_test_results import verify_xml

ROOT = Path(__file__).resolve().parents[2]
BASE = 'e8c49e6ada0383d20b37a1152f13f36cb0b4d676'
CHANGES = {'.github/workflows/linux-release-build.yml', 'linux/README.md',
           'linux/RELEASE_BUILD.md', 'linux/tools/release_assets.py'}
NOT_RUN = ['real_desktop', 'installation_and_uninstallation', 'input_method',
           'Wayland', 'high_DPI', 'other_distributions_and_architectures',
           'AppImage', 'other_filesystems', 'crash_and_power_loss_durability']
READY = 'Linux 发布构建就绪，真实桌面未验收'
RUNTIME_PACKAGES = ('libqt6widgets6t64', 'qt6-qpa-plugins')
RUNTIME_FILES = {
    'bin/json-dictionary-editor',
    'share/icons/hicolor/256x256/apps/json-dictionary-editor.png',
    'share/applications/json-dictionary-editor.desktop',
    'share/doc/json-dictionary-editor/copyright',
    'share/doc/json-dictionary-editor/README.md',
    'share/doc/json-dictionary-editor/RELEASE_BUILD.md',
    'share/doc/json-dictionary-editor/SampleDictionary.json',
    'BUILD_INFO.json', 'RUNNING.txt',
}

def require(value, message):
    if not value:
        raise RuntimeError(message)

def sha(data):
    return hashlib.sha256(data).hexdigest()

def command(args, **kw):
    return subprocess.check_output(args, cwd=ROOT, **kw)

def git(*args):
    return command(['git', *args])

def dump(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

def source_guard():
    head = git('rev-parse', 'HEAD').decode().strip()
    require(re.fullmatch('[0-9a-f]{40}', head), 'Invalid source commit')
    git('merge-base', '--is-ancestor', BASE, head)
    changed = {p.decode() for p in git('diff', '--name-only', '-z', BASE, head).split(b'\0') if p}
    require(changed <= CHANGES, 'Changes outside Linux release preparation: ' + repr(sorted(changed - CHANGES)))
    require(not git('status', '--porcelain').strip(), 'Source checkout must be clean')
    return head

def preflight():
    release = dict(line.split('=', 1) for line in Path('/etc/os-release').read_text().splitlines() if '=' in line)
    require(release.get('ID', '').strip('"') == 'ubuntu' and
            release.get('VERSION_ID', '').strip('"') == '24.04' and
            platform.machine() == 'x86_64', 'Requires actual Ubuntu 24.04 x86_64')
    head = source_guard()
    require(os.environ.get('GITHUB_SHA', head) == head, 'Checkout differs from workflow source commit')
    print('SOURCE_AND_RUNNER_OK ' + head)
    return head

def check_junit(xml_path, inventory):
    expected = {t['name'] for t in inventory['raw_inventory']['tests']}
    cases = ET.parse(xml_path).findall('.//testcase')
    names = [c.attrib['name'] for c in cases]
    require(expected and len(names) == len(set(names)) and set(names) == expected,
            'CTest result inventory incomplete: ' + str(xml_path))
    require(all(c.find('failure') is None and c.find('error') is None and
                c.find('skipped') is None and c.attrib.get('status') not in ('notrun', 'disabled')
                for c in cases), 'Failed, skipped or incomplete CTest result: ' + str(xml_path))
    require(all(t['result'] == 'passed' for t in inventory['tests']), 'Non-passing inventory')
    return names

def automatic_checks(head):
    logs = ROOT / '.build/evidence/verified'
    final = json.loads((logs / 'source-final.json').read_text())
    scope = json.loads((logs / 'scope.json').read_text())
    require(final['commit'] == head and final['status'] == '' and
            scope['commit'] == head and scope['target_ubuntu24_amd64'] is True,
            'Checks must belong to this clean target commit')
    result = {'source_commit': head, 'result': 'PASS', 'backend': 'offscreen automation',
              'not_run': NOT_RUN, 'ctest_groups': {}, 'qt_test_groups': {}}
    for kind in ('core', 'gui'):
        inv = json.loads((logs / (kind + '-inventory.json')).read_text())
        result['ctest_groups'][kind] = check_junit(logs / (kind + '-junit.xml'), inv)
    for kind in ('interactions', 'save-state'):
        record = verify_xml(logs / (kind + '.xml'), logs / (kind + '-inventory.json'), 'offscreen')
        result['qt_test_groups'][kind] = record
    records = [json.loads(line) for line in (logs / 'commands.jsonl').read_text().splitlines()]
    required = {'core-configure.log', 'core-build.log', 'core-ctest.log', 'translations.log',
                'gui-configure.log', 'gui-build.log', 'gui-ctest.log', 'core-ldd.log',
                'offscreen-smoke.log', 'offscreen-interactions.log', 'offscreen-save-state.log',
                'source-ancestor.log', 'protected-diff.log', 'final-protected-diff.log'}
    relevant = [r for r in records if r['log'] in required]
    require(len(relevant) == len(required) and {r['log'] for r in relevant} == required and
            all(r['exit_code'] == 0 for r in relevant), 'Missing or failed required command')
    require('libQt' not in (logs / 'core-ldd.log').read_text(), 'Core-only binary links Qt')
    require(json.loads((logs / 'protected-before.json').read_text()) ==
            json.loads((logs / 'protected-after.json').read_text()), 'Protected source changed')
    result['commands'] = records  # Preserve failed optional probes without calling them passes.
    result['repeated_core_tests_in_gui_group'] = True
    return result, final

def archive_directory(directory, destination, prefix, epoch):
    with tarfile.open(destination, 'w:gz') as tar:
        for path in sorted(directory.rglob('*')):
            require(not path.is_symlink(), 'Unexpected symlink in generated assets')
            if not path.is_file():
                continue
            info = tar.gettarinfo(str(path), prefix + '/' + path.relative_to(directory).as_posix())
            info.uid = info.gid = 0
            info.uname = info.gname = ''
            info.mtime = epoch
            with path.open('rb') as stream:
                tar.addfile(info, stream)

def prepare(directory):
    head = preflight()
    checks, final = automatic_checks(head)
    print('AUTOMATIC_CHECKS_OK ' + json.dumps({
        'ctest_groups': {k: len(v) for k, v in checks['ctest_groups'].items()},
        'qt_test_incidents': {k: v['count'] for k, v in checks['qt_test_groups'].items()},
        'repeated_core_tests_in_gui_group': True}))
    directory.mkdir(parents=True, exist_ok=True)
    require(not list(directory.iterdir()), 'Output directory must be empty')
    short = head[:12]
    source_name = 'json-dictionary-editor-source-' + short + '.tar.gz'
    runtime_name = 'json-dictionary-editor-ubuntu24.04-x86_64-' + short + '.tar.gz'
    logs_name = 'linux-automatic-checks-' + short + '.tar.gz'
    source_prefix = source_name[:-7]
    runtime_prefix = runtime_name[:-7]
    logs_prefix = logs_name[:-7]
    git('archive', '--format=tar.gz', '--prefix=' + source_prefix + '/',
        '--output=' + str(directory / source_name), head)
    binary = ROOT / '.build/linux-gui/qt/json-dictionary-editor'
    binary_sha = sha(binary.read_bytes())
    require(binary_sha == final['binary_sha256'], 'Binary changed after automatic checks')
    ldd = command(['ldd', str(binary)]).decode()
    require('not found' not in ldd, 'Unresolved runtime dependency')
    package_rows = command(['dpkg-query', '-W',
                            '-f=${binary:Package}\t${db:Status-Status}\t${Version}\n',
                            *RUNTIME_PACKAGES]).decode().splitlines()
    runtime_packages = {}
    for row in package_rows:
        package, status, version = row.split('\t')
        package = package.split(':', 1)[0]
        require(status == 'installed' and version and package not in runtime_packages,
                'Runtime dependency not installed: ' + package)
        runtime_packages[package] = version
    require(set(runtime_packages) == set(RUNTIME_PACKAGES), 'Runtime package inventory mismatch')
    dump(ROOT / '.build/evidence/runtime-packages.json', runtime_packages)
    info = {'source_commit': head, 'fixed_candidate_base': BASE, 'target': 'Ubuntu 24.04 x86_64',
            'app_version': '1.1.1-linux-preview', 'binary_sha256': binary_sha,
            'source_archive': source_name, 'source_archive_sha256': sha((directory / source_name).read_bytes()),
            'binary_archive': runtime_name, 'binary_prefix': runtime_prefix,
            'source_prefix': source_prefix, 'checks_archive': logs_name, 'checks_prefix': logs_prefix,
            'dynamic_system_qt': True, 'third_party_libraries_bundled': False,
            'not_run': NOT_RUN, 'automatic_checks': 'PASS',
            'gcc': command(['g++', '--version']).decode(), 'cmake': command(['cmake', '--version']).decode(),
            'qt': command(['qmake6', '--version']).decode(), 'runtime_ldd': ldd,
            'runtime_packages': runtime_packages,
            'os_release': Path('/etc/os-release').read_text(),
            'runner_image': {k: os.environ.get(k) for k in ('ImageOS', 'ImageVersion', 'RUNNER_OS', 'RUNNER_ARCH')},
            'workflow_run': {k: os.environ.get(k) for k in ('GITHUB_REPOSITORY', 'GITHUB_RUN_ID', 'GITHUB_RUN_ATTEMPT')},
            'expected_completion_state': READY}
    epoch = int(git('show', '-s', '--format=%ct', head).decode().strip())
    with tempfile.TemporaryDirectory(prefix='linux-release-stage-', dir=ROOT / '.build') as temp:
        stage = Path(temp)
        subprocess.run(['cmake', '--install', str(ROOT / '.build/linux-gui'), '--prefix', str(stage)],
                       cwd=ROOT, check=True)
        shutil.copyfile(ROOT / 'linux/RELEASE_BUILD.md',
                        stage / 'share/doc/json-dictionary-editor/RELEASE_BUILD.md')
        dump(stage / 'BUILD_INFO.json', info)
        (stage / 'RUNNING.txt').write_text(
            'Ubuntu 24.04 x86_64 build. Dynamic system Qt 6; no Qt libraries bundled.\n'
            'Runtime packages: ' + ' and '.join(RUNTIME_PACKAGES)
            + ', resolved with their dependencies by the system package manager.\n'
            'From this directory: ./bin/json-dictionary-editor [dictionary.json]\n'
            'Real desktop, installation, IME, Wayland and high DPI: NOT RUN in this build round.\n', encoding='utf-8')
        actual = {p.relative_to(stage).as_posix() for p in stage.rglob('*') if p.is_file()}
        require(actual == RUNTIME_FILES, 'Unexpected runtime payload: ' + repr(actual ^ RUNTIME_FILES))
        require(sha((stage / 'bin/json-dictionary-editor').read_bytes()) == binary_sha,
                'Staging changed the tested binary')
        archive_directory(stage, directory / runtime_name, runtime_prefix, epoch)
    dump(directory / 'BUILD_INFO.json', info)
    dump(directory / 'AUTOMATIC_CHECKS.json', checks)
    (directory / 'RELEASE_NOTES.md').write_text(
        '# Linux 开源发布构建候选\n\n' + READY + '\n\n'
        '源码提交：`' + head + '`；起点：`' + BASE + '`。\n\n'
        '目标 Ubuntu 24.04 x86_64，C++17 与 Qt 6 Widgets，动态使用系统 Qt。'
        '保留数字原文、对象顺序、Unicode 校验、对象根限制、容量限制和既有原子保存策略。'
        '复用共享核心、空容器搜索修复及英文示例。\n\n'
        '自动检查结果和完整日志见 AUTOMATIC_CHECKS.json 与 linux-automatic-checks 归档。'
        '核心检查也在 GUI CTest 组重复运行；两组数量不能相加当作不同测试数量。\n\n'
        '本轮未执行：真实桌面、安装卸载、输入法、Wayland、高 DPI、其他发行版与架构、'
        '其他文件系统、崩溃和掉电验收。offscreen 属于自动检查。\n\n'
        '使用者需具备 Ubuntu 的 Qt 6 Widgets 与 QPA 运行依赖；运行方法见二进制归档 RUNNING.txt。'
        '运行包为九项文件，包含 README 相对链接引用的 RELEASE_BUILD.md。'
        '实际运行依赖包及版本见 BUILD_INFO.json 的 runtime_packages。\n\n'
        '保存层仍拒绝不支持的属性、ACL、链接或原子操作，没有直接覆盖降级。\n\n'
        '这些是待发布资产；此工作流未创建版本标签或公开 Release。'
        '原 1.1.1-0linux1 Ubuntu 固定候选及其验收记录保留。'
        'Actions 资产保留期为 90 天，最终公开发布仍待后续决定。\n', encoding='utf-8')
    archive_directory(ROOT / '.build/evidence', directory / logs_name, logs_prefix, epoch)
    names = sorted(p.name for p in directory.iterdir())
    (directory / 'SHA256SUMS').write_text(''.join(sha((directory / name).read_bytes()) + '  ' + name + '\n'
                                                for name in names), encoding='utf-8')
    source_guard()
    digest = sha((directory / 'SHA256SUMS').read_bytes())
    if os.environ.get('GITHUB_OUTPUT'):
        with open(os.environ['GITHUB_OUTPUT'], 'a', encoding='utf-8') as output:
            output.write('manifest_sha256=' + digest + '\n')
    print('ASSETS_PREPARED commit=' + head + ' manifest_sha256=' + digest)

def archive_files(path, prefix, allow_symlinks=False):
    result = {}
    total = 0
    with tarfile.open(path, 'r:gz') as tar:
        for item in tar:
            name = PurePosixPath(item.name)
            require(not name.is_absolute() and '..' not in name.parts and '\\' not in item.name,
                    'Unsafe archive path')
            require(item.name == prefix or item.name.startswith(prefix + '/'), 'Unexpected archive prefix')
            if item.isdir():
                continue
            relative = item.name[len(prefix) + 1:]
            require(relative and relative not in result, 'Duplicate archive file')
            total += item.size
            require(total < 256 * 1024 * 1024, 'Archive exceeds expected bounds')
            if item.issym() and allow_symlinks:
                data, kind = item.linkname.encode(), '120000'
            else:
                require(item.isfile(), 'Unexpected archive member type')
                data, kind = tar.extractfile(item).read(), '100755' if item.mode & 0o111 else '100644'
            result[relative] = (data, kind)
    return result

def verify(directory, report_path):
    report = {'result': 'FAIL', 'scope': 'cloud artifact download and static correspondence', 'not_run': NOT_RUN}
    try:
        head = preflight()
        expected_manifest = os.environ.get('EXPECTED_MANIFEST_SHA256', '')
        require(re.fullmatch('[0-9a-f]{64}', expected_manifest), 'Missing producer manifest digest')
        manifest_bytes = (directory / 'SHA256SUMS').read_bytes()
        require(sha(manifest_bytes) == expected_manifest, 'Downloaded manifest differs from producer digest')
        assets = {}
        for line in manifest_bytes.decode().splitlines():
            digest, name = line.split('  ', 1)
            require(re.fullmatch('[0-9a-f]{64}', digest) and re.fullmatch('[A-Za-z0-9_.-]+', name)
                    and name not in assets, 'Malformed manifest')
            data = (directory / name).read_bytes()
            require(sha(data) == digest, 'Asset digest mismatch: ' + name)
            assets[name] = {'sha256': digest, 'bytes': len(data)}
        require(set(p.name for p in directory.iterdir()) == set(assets) | {'SHA256SUMS'}, 'Missing or extra download')
        info = json.loads((directory / 'BUILD_INFO.json').read_text())
        checks = json.loads((directory / 'AUTOMATIC_CHECKS.json').read_text())
        require(info['source_commit'] == checks['source_commit'] == head and checks['result'] == 'PASS',
                'Source or automatic result mismatch')
        source = archive_files(directory / info['source_archive'], info['source_prefix'], True)
        require(sha((directory / info['source_archive']).read_bytes()) == info['source_archive_sha256'], 'Source archive identity mismatch')
        expected = {}
        for row in git('ls-tree', '-rz', head).split(b'\0'):
            if not row:
                continue
            meta, path = row.split(b'\t', 1)
            mode, kind, object_id = meta.split()
            require(kind == b'blob', 'Source archive contains unsupported git object')
            expected[path.decode()] = (git('cat-file', 'blob', object_id.decode()), mode.decode())
        require(source == expected, 'Full source archive differs in content or file mode from commit')
        runtime = archive_files(directory / info['binary_archive'], info['binary_prefix'])
        require(set(runtime) == RUNTIME_FILES, 'Unexpected runtime payload/test binary/library')
        binary, mode = runtime['bin/json-dictionary-editor']
        require(mode == '100755' and binary[:6] == b'\x7fELF\x02\x01' and
                struct.unpack_from('<H', binary, 18)[0] == 62 and sha(binary) == info['binary_sha256'],
                'Binary architecture, mode or digest mismatch')
        require(json.loads(runtime['BUILD_INFO.json'][0]) == info, 'Embedded build information mismatch')
        for deployed, original in {
                'share/doc/json-dictionary-editor/copyright': 'LICENSE',
                'share/doc/json-dictionary-editor/README.md': 'linux/README.md',
                'share/doc/json-dictionary-editor/RELEASE_BUILD.md': 'linux/RELEASE_BUILD.md',
                'share/doc/json-dictionary-editor/SampleDictionary.json': 'windows/resources/SampleDictionary.json',
                'share/icons/hicolor/256x256/apps/json-dictionary-editor.png': 'linux/resources/json-dictionary-editor.png',
                'share/applications/json-dictionary-editor.desktop': 'linux/packaging/json-dictionary-editor.desktop'}.items():
            require(runtime[deployed][0] == expected[original][0], 'Runtime resource differs: ' + deployed)
        readme = runtime['share/doc/json-dictionary-editor/README.md'][0].decode('utf-8')
        require('[RELEASE_BUILD.md](RELEASE_BUILD.md)' in readme,
                'Expected packaged README documentation link missing')
        running = runtime['RUNNING.txt'][0].decode('utf-8')
        require('Runtime packages: ' + ' and '.join(RUNTIME_PACKAGES) + ',' in running and
                set(info['runtime_packages']) == set(RUNTIME_PACKAGES) and
                all(info['runtime_packages'].values()), 'Runtime dependency documentation mismatch')
        evidence = archive_files(directory / info['checks_archive'], info['checks_prefix'])
        require(json.loads(evidence['runtime-packages.json'][0]) == info['runtime_packages'],
                'Runtime package documentation differs from runner package registration')
        require(json.loads(evidence['verified/source-final.json'][0])['binary_sha256'] == info['binary_sha256'],
                'Evidence belongs to a different tested binary')
        for kind in ('core', 'gui'):
            inv = json.loads(evidence['verified/' + kind + '-inventory.json'][0])
            cases = ET.fromstring(evidence['verified/' + kind + '-junit.xml'][0]).findall('.//testcase')
            names = [c.attrib['name'] for c in cases]
            require(len(names) == len(set(names)) and set(names) == set(checks['ctest_groups'][kind]) and
                    set(names) == {t['name'] for t in inv['raw_inventory']['tests']} and
                    all(c.find('failure') is None and c.find('error') is None and c.find('skipped') is None for c in cases),
                    'Downloaded CTest result incomplete or failed')
        report.update(result='PASS', completion_state=READY, source_commit=head,
                      expected_manifest_sha256=expected_manifest, assets=assets,
                      source_files=len(source), runtime_files=sorted(runtime),
                      runtime_packages=info['runtime_packages'],
                      binary_sha256=info['binary_sha256'], ctest_groups=checks['ctest_groups'])
    except Exception as error:
        report['error'] = str(error)
        raise
    finally:
        report_path.parent.mkdir(parents=True, exist_ok=True)
        dump(report_path, report)
    print(READY)
    print('CLOUD_RECEPTION_RECEIPT ' + json.dumps(report, ensure_ascii=False))

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=['preflight', 'prepare', 'verify'])
    parser.add_argument('--directory', type=Path, default=ROOT / '.build/release-assets')
    parser.add_argument('--report', type=Path, default=ROOT / '.build/reception/RECEPTION.json')
    args = parser.parse_args()
    if args.operation == 'preflight':
        preflight()
    elif args.operation == 'prepare':
        prepare(args.directory.resolve())
    else:
        verify(args.directory.resolve(), args.report.resolve())
