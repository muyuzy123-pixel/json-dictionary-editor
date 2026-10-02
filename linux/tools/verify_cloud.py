#!/usr/bin/env python3
"""Core-first cloud verification. Packaging has a separate target-environment gate."""
import argparse,datetime,hashlib,json,os,platform,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--xvfb',action='store_true');args=p.parse_args()
logs=root/'.build/evidence';logs.mkdir(parents=True,exist_ok=True)
runtime=logs/'runtime';runtime.mkdir(exist_ok=True);runtime.chmod(0o700)
env=dict(os.environ,QT_QPA_PLATFORM='offscreen',XDG_RUNTIME_DIR=str(runtime),JDE_TEST_SCREENSHOTS=str(logs))
def run(name,command,check=True,environment=None):
 with (logs/name).open('w') as f:
  f.write('$ '+repr(command)+'\n');f.flush()
  result=subprocess.run(command,cwd=root,env=environment,stdout=f,stderr=subprocess.STDOUT)
 if check and result.returncode:raise SystemExit(f'{name} failed: exit {result.returncode}')
 return result.returncode
probes=['uname -a','uname -m','cat /etc/os-release','id','command -v sudo docker apt-get cmake g++ qmake6 qtpaths6 Xvfb xvfb-run || true','test -w /usr && echo /usr:writable; test -w /var/lib/dpkg && echo dpkg:writable','g++ --version','cmake --version','qmake6 --version','qtpaths6 --plugin-dir','ls -l /usr/lib/x86_64-linux-gnu/qt6/plugins/platforms','findmnt -T . -o TARGET,FSTYPE,OPTIONS','findmnt -T /dev/shm -o TARGET,FSTYPE,OPTIONS','ls -l /dev/dri /dev/fuse']
with (logs/'environment.log').open('w') as out:
 out.write(datetime.datetime.now(datetime.timezone.utc).isoformat()+'\n')
 for key in ['DISPLAY','WAYLAND_DISPLAY','XDG_RUNTIME_DIR','XDG_SESSION_TYPE']:out.write(key+'='+os.environ.get(key,'<unset>')+'\n')
 for command in probes:
  result=subprocess.run(['bash','-c',command],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);out.write('$ '+command+'\n'+result.stdout+'exit='+str(result.returncode)+'\n')
fixed='7ee1c5bb722f9041cd2ba513de044ea1a01cf0c5'
run('source-ancestor.log',['git','merge-base','--is-ancestor',fixed,'HEAD'])
run('protected-source.log',['git','diff','--exit-code',fixed,'--','macos','windows','tests'])
head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip()
status=subprocess.check_output(['git','status','--porcelain'],cwd=root,text=True)
source={str(path.relative_to(root)):hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted((root/'linux').rglob('*')) if path.is_file()}
(logs/'source.json').write_text(json.dumps({'fixed_base':fixed,'HEAD':head,'status':status,'linux_sha256':source},indent=2))
release=dict(line.strip().split('=',1) for line in Path('/etc/os-release').read_text().splitlines() if '=' in line)
target=release.get('ID','').strip('"')=='ubuntu' and release.get('VERSION_ID','').strip('"')=='24.04' and platform.machine()=='x86_64'
(logs/'scope.json').write_text(json.dumps({'target_ubuntu24_amd64':target,'automatic_checks_only':True,'desktop_acceptance':False},indent=2))
run('core-configure.log',['cmake','-S','linux','-B','.build/linux-core','-DJDE_BUILD_GUI=OFF','-DCMAKE_BUILD_TYPE=Release'])
run('core-build.log',['cmake','--build','.build/linux-core','-j2'])
run('core-ctest.log',['ctest','--test-dir','.build/linux-core','--output-on-failure','-V'])
if 'Qt6_DIR:' in (root/'.build/linux-core/CMakeCache.txt').read_text():raise SystemExit('Core-only build found Qt unexpectedly')
run('translations.log',['python3','linux/tools/check_translations.py'])
run('gui-configure.log',['cmake','-S','linux','-B','.build/linux-gui','-DJDE_BUILD_GUI=ON','-DCMAKE_BUILD_TYPE=Release'])
run('gui-build.log',['cmake','--build','.build/linux-gui','-j2'])
run('gui-ctest.log',['ctest','--test-dir','.build/linux-gui','--output-on-failure','-V'],environment=env)
run('offscreen-smoke.log',['.build/linux-gui/qt/json-dictionary-editor','--smoke-test'],environment=env)
run('offscreen-interactions.log',['.build/linux-gui/qt/jsondict_interaction_tests','-v2'],environment=env)
if args.xvfb:run('xvfb-supervisor.log',['python3','linux/tools/verify_xvfb_tcp.py'])
# Record hashes again after original-icon generation and every verification step.
source={str(path.relative_to(root)):hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted((root/'linux').rglob('*')) if path.is_file()}
(logs/'source-final.json').write_text(json.dumps({'fixed_base':fixed,'HEAD':head,'status':subprocess.check_output(['git','status','--porcelain'],cwd=root,text=True),'linux_sha256':source},indent=2))
print('CLOUD_CHECKS_OK; target_environment='+str(target)+'; automatic only, desktop not accepted')
