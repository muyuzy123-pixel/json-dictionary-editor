#!/usr/bin/env python3
"""Ubuntu 24.04 amd64 .deb, after the preserved build and passing checks."""
import argparse,hashlib,json,os,platform,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--build',default='.build/linux-gui');p.add_argument('--output',default='.build/dist');args=p.parse_args()
release=dict(line.strip().split('=',1) for line in Path('/etc/os-release').read_text().splitlines() if '=' in line)
if release.get('ID','').strip('"')!='ubuntu' or release.get('VERSION_ID','').strip('"')!='24.04' or platform.machine()!='x86_64':
 raise SystemExit('Packaging requires actual Ubuntu 24.04 x86_64; host checks cannot replace this gate')
build=(root/args.build).resolve();out=(root/args.output).resolve();out.mkdir(parents=True,exist_ok=True)
if subprocess.check_output(['git','status','--porcelain'],cwd=root,text=True):raise SystemExit('Packaging requires an exact clean source commit')
head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip()
subprocess.run(['cmake','--build',str(build),'-j2'],check=True)
env=dict(os.environ,QT_QPA_PLATFORM='offscreen')
subprocess.run(['ctest','--test-dir',str(build),'--output-on-failure'],env=env,check=True)
subprocess.run([str(build/'qt/json-dictionary-editor'),'--smoke-test'],env=env,check=True)
with tempfile.TemporaryDirectory(prefix='jde-deb-',dir=out) as temp:
 work=Path(temp);stage=work/'debian/json-dictionary-editor';stage.mkdir(parents=True)
 subprocess.run(['cmake','--install',str(build),'--prefix','/usr'],env=dict(os.environ,DESTDIR=str(stage)),check=True)
 binary=stage/'usr/bin/json-dictionary-editor'
 documentation=stage/'usr/share/doc/json-dictionary-editor'
 (documentation/'build-info.json').write_text(json.dumps({'source_commit':head,'target':'Ubuntu 24.04 amd64','binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'qt':'dynamic system Qt 6.4+','third_party_libraries_bundled':False},indent=2)+'\n')
 (work/'debian/control').write_text('Source: json-dictionary-editor\nSection: editors\nPriority: optional\nMaintainer: JSON Dictionary Editor contributors\n\nPackage: json-dictionary-editor\nArchitecture: amd64\nDescription: Ordered JSON dictionary editor\n')
 result=subprocess.check_output(['dpkg-shlibdeps','-O','-e'+str(stage/'usr/bin/json-dictionary-editor')],cwd=work,text=True)
 depends=next(line.split('=',1)[1] for line in result.splitlines() if line.startswith('shlibs:Depends='))
 control=stage/'DEBIAN';control.mkdir()
 installed_size=(sum(path.stat().st_size for path in (stage/'usr').rglob('*') if path.is_file())+1023)//1024
 (control/'control').write_text('Package: json-dictionary-editor\nVersion: 1.1.1-0linux1\nArchitecture: amd64\nMaintainer: JSON Dictionary Editor contributors\nSection: editors\nPriority: optional\nInstalled-Size: '+str(installed_size)+'\nDepends: '+depends+', qt6-qpa-plugins (>= 6.4), qt6-wayland (>= 6.4)\nRecommends: fonts-noto-cjk, fonts-noto-color-emoji\nDescription: JSON dictionary editor for Ubuntu 24.04\n Preserves object order and original number text; dynamically linked Qt 6 Widgets.\n')
 (control/'md5sums').write_text(''.join(hashlib.md5(path.read_bytes()).hexdigest()+'  '+str(path.relative_to(stage))+'\n' for path in sorted((stage/'usr').rglob('*')) if path.is_file()))
 subprocess.run(['desktop-file-validate',str(stage/'usr/share/applications/json-dictionary-editor.desktop')],check=True)
 target=out/'json-dictionary-editor_1.1.1-0linux1_amd64.deb'
 subprocess.run(['dpkg-deb','--root-owner-group','--build',str(stage),str(target)],check=True)
print(target)
print('Generated package still requires install/uninstall and actual desktop acceptance')
