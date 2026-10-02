#!/usr/bin/env python3
"""Core-first exact-commit verification. External preservation is a separate preflight."""
import argparse,datetime,hashlib,json,os,platform,shutil,subprocess,time,xml.etree.ElementTree as ET
from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--fresh',action='store_true');p.add_argument('--xvfb',action='store_true');p.add_argument('--logs',default='.build/evidence/verified');a=p.parse_args()
logs=(root/a.logs).resolve();logs.mkdir(parents=True,exist_ok=True);runtime=logs/'runtime';runtime.mkdir(exist_ok=True);runtime.chmod(0o700)
env=dict(os.environ,QT_QPA_PLATFORM='offscreen',XDG_RUNTIME_DIR=str(runtime),JDE_TEST_SCREENSHOTS=str(logs))
fixed='7ee1c5bb722f9041cd2ba513de044ea1a01cf0c5'
def run(name,command,environment=None,backend='not-applicable',check=True):
 start=datetime.datetime.now(datetime.timezone.utc).isoformat();begin=time.monotonic();print('BEGIN '+name,flush=True)
 with (logs/name).open('w') as f:
  f.write('$ '+repr(command)+'\n');f.flush();r=subprocess.run(command,cwd=root,env=environment,stdout=f,stderr=subprocess.STDOUT)
 record={'command':command,'cwd':str(root),'started':start,'duration_seconds':time.monotonic()-begin,'exit_code':r.returncode,'backend':backend,'log':name}
 with (logs/'commands.jsonl').open('a') as f:f.write(json.dumps(record)+'\n')
 print('END '+name+' exit='+str(r.returncode),flush=True)
 if check and r.returncode:raise SystemExit(name+' failed; full log retained')
 return r.returncode
def protected():
 paths=subprocess.check_output(['git','ls-files','-z','--','macos','windows','tests','LICENSE'],cwd=root).split(b'\0')
 return {x.decode():hashlib.sha256((root/x.decode()).read_bytes()).hexdigest() for x in paths if x}
def inventory(build,junit,out,backend):
 data=json.loads(subprocess.check_output(['ctest','--test-dir',str(build),'--show-only=json-v1'],cwd=root))
 results={t.attrib['name']:('failed' if t.find('failure') is not None else 'skipped' if t.find('skipped') is not None else 'passed') for t in ET.parse(junit).findall('.//testcase')}
 rows=[]
 for t in data['tests']:
  result=results.get(t['name'],'unknown')
  rows.append({'name':t['name'],'command':t.get('command'),'properties':t.get('properties',[]),'backend':backend if t['name'].startswith('linux_qt_') else 'not-applicable','result':result,'exit_code':0 if result=='passed' else None})
 (logs/out).write_text(json.dumps({'tests':rows,'count':len(rows),'raw_inventory':data},indent=2)+'\n')
probes=[['uname','-a'],['uname','-m'],['cat','/etc/os-release'],['id'],['cat','/proc/self/uid_map'],['cat','/proc/self/gid_map'],['g++','--version'],['cmake','--version'],['qmake6','--version'],['qtpaths6','--plugin-dir'],['ls','-l','/usr/lib/x86_64-linux-gnu/qt6/plugins/platforms'],['findmnt','-T',str(root),'-o','TARGET,FSTYPE,OPTIONS'],['findmnt','-T','/tmp','-o','TARGET,FSTYPE,OPTIONS'],['findmnt','-T','/dev/shm','-o','TARGET,FSTYPE,OPTIONS'],['ls','-l','/dev/dri','/dev/fuse'],['dpkg-query','-W','fonts-noto-cjk','fonts-noto-color-emoji','qt6-wayland']]
for i,command in enumerate(probes):run('environment-'+str(i).zfill(2)+'.log',command,check=False)
environment={'recorded_at':datetime.datetime.now(datetime.timezone.utc).isoformat(),'uid':os.getuid(),'gid':os.getgid(),'architecture':platform.machine(),'graphics':{k:os.environ.get(k) for k in ['DISPLAY','WAYLAND_DISPLAY','XDG_RUNTIME_DIR','XDG_SESSION_TYPE','QT_QPA_PLATFORM']},'programs':{p:shutil.which(p) for p in ['sudo','docker','apt-get','g++','cmake','qmake6','qtpaths6','Xvfb']},'writable':{p:os.access(p,os.W_OK) for p in ['/usr','/var/lib/dpkg']}}
(logs/'environment.json').write_text(json.dumps(environment,indent=2)+'\n')
run('source-ancestor.log',['git','merge-base','--is-ancestor',fixed,'HEAD']);run('protected-diff.log',['git','diff','--exit-code',fixed,'--','macos','windows','tests','LICENSE'])
head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip();status=subprocess.check_output(['git','status','--porcelain'],cwd=root,text=True)
if status:raise SystemExit('Exact-commit verification requires a clean checkout: '+status)
before=protected();(logs/'protected-before.json').write_text(json.dumps(before,indent=2)+'\n')
release=dict(line.strip().split('=',1) for line in Path('/etc/os-release').read_text().splitlines() if '=' in line)
target=release.get('ID','').strip('"')=='ubuntu' and release.get('VERSION_ID','').strip('"')=='24.04' and platform.machine()=='x86_64'
(logs/'scope.json').write_text(json.dumps({'commit':head,'fixed_base':fixed,'target_ubuntu24_amd64':target,'automatic_only':True,'real_desktop_accepted':False,'power_loss_durability_accepted':False},indent=2)+'\n')
if a.fresh:
 for name in ['linux-core','linux-gui']:
  path=root/'.build'/name
  if path.is_symlink():raise SystemExit('Refusing to remove a symlink build directory')
  if path.exists():shutil.rmtree(path)
run('core-configure.log',['cmake','-S','linux','-B','.build/linux-core','-DJDE_BUILD_GUI=OFF','-DCMAKE_BUILD_TYPE=Release'])
run('core-build.log',['cmake','--build','.build/linux-core','-j2'])
run('core-ctest.log',['ctest','--test-dir','.build/linux-core','--output-on-failure','-V','--output-junit',str(logs/'core-junit.xml')])
shutil.copy(root/'.build/linux-core/Testing/Temporary/LastTest.log',logs/'core-LastTest.log')
if any(x in (root/'.build/linux-core/CMakeCache.txt').read_text() for x in ['Qt6_DIR:','Qt6Core_DIR:','Qt6Widgets_DIR:']):raise SystemExit('Core-only build unexpectedly found Qt')
run('core-ldd.log',['ldd','.build/linux-core/jsondict_core_tests'])
if 'libQt' in (logs/'core-ldd.log').read_text():raise SystemExit('Core-only executable linked Qt')
inventory(root/'.build/linux-core',logs/'core-junit.xml','core-inventory.json','not-applicable')
run('translations.log',['python3','linux/tools/check_translations.py'])
run('gui-configure.log',['cmake','-S','linux','-B','.build/linux-gui','-DJDE_BUILD_GUI=ON','-DCMAKE_BUILD_TYPE=Release'])
run('gui-build.log',['cmake','--build','.build/linux-gui','-j2'])
run('gui-ctest.log',['ctest','--test-dir','.build/linux-gui','--output-on-failure','-V','--output-junit',str(logs/'gui-junit.xml')],env,'offscreen')
shutil.copy(root/'.build/linux-gui/Testing/Temporary/LastTest.log',logs/'gui-LastTest.log')
inventory(root/'.build/linux-gui',logs/'gui-junit.xml','gui-inventory.json','offscreen')
run('offscreen-smoke.log',['.build/linux-gui/qt/json-dictionary-editor','--smoke-test'],env,'offscreen')
run('offscreen-interactions.log',['.build/linux-gui/qt/jsondict_interaction_tests','-v2','-o',str(logs/'interactions.xml')+',xml'],env,'offscreen')
run('offscreen-save-state.log',['.build/linux-gui/qt/jsondict_save_state_tests','-v2','-o',str(logs/'save-state.xml')+',xml'],dict(env,LD_PRELOAD=str(root/'.build/linux-gui/qt/libjsondict_file_faults.so')),'offscreen')
if a.xvfb:run('xvfb-supervisor.log',['python3','linux/tools/verify_xvfb_tcp.py','--logs',str(logs/'xcb')],backend='xcb/Xvfb')
after=protected();(logs/'protected-after.json').write_text(json.dumps(after,indent=2)+'\n')
if before!=after:raise SystemExit('Protected source changed')
run('final-protected-diff.log',['git','diff','--exit-code',fixed,'--','macos','windows','tests','LICENSE'])
status=subprocess.check_output(['git','status','--porcelain'],cwd=root,text=True)
if status:raise SystemExit('Source changed during verification: '+status)
source={str(path.relative_to(root)):hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted((root/'linux').rglob('*')) if path.is_file()};binary=root/'.build/linux-gui/qt/json-dictionary-editor'
(logs/'source-final.json').write_text(json.dumps({'commit':head,'linux_sha256':source,'protected_sha256':after,'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'status':status},indent=2)+'\n')
print('CLOUD_CHECKS_OK commit='+head+' target='+str(target)+' automatic only; desktop and durability not accepted')
