#!/usr/bin/env python3
"""Supervised TCP Xvfb automation; no real desktop acceptance."""
import argparse,datetime,json,os,socket,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--build',default='.build/linux-gui');p.add_argument('--logs',default='.build/evidence/xcb');a=p.parse_args()
root=Path(__file__).resolve().parents[2];build=(root/a.build).resolve();logs=(root/a.logs).resolve();logs.mkdir(parents=True,exist_ok=True);runtime=logs/'runtime';runtime.mkdir(exist_ok=True);runtime.chmod(0o700)
display=None
for number in range(77,100):
 if Path('/tmp/.X'+str(number)+'-lock').exists():continue
 try:
  s=socket.socket();s.bind(('127.0.0.1',6000+number));s.close();display=number;break
 except OSError:continue
if display is None:raise SystemExit('No free TCP display')
command=['Xvfb',':'+str(display),'-screen','0','1280x800x24','-nolisten','unix','-nolisten','local','-listen','tcp','-ac']
(logs/'server-command.json').write_text(json.dumps(command)+'\n');server_log=(logs/'xvfb-server.log').open('w');server=subprocess.Popen(command,stdout=server_log,stderr=subprocess.STDOUT)
try:
 ready=False
 for _ in range(100):
  if server.poll() is not None:break
  try:
   with socket.create_connection(('127.0.0.1',6000+display),.1):ready=True;break
  except OSError:time.sleep(.1)
 if not ready:raise SystemExit('Xvfb TCP unavailable; inspect server log')
 env=dict(os.environ,DISPLAY='127.0.0.1:'+str(display),QT_QPA_PLATFORM='xcb',XDG_RUNTIME_DIR=str(runtime),JDE_TEST_SCREENSHOTS=str(logs))
 cases=[('xcb-smoke.log',[str(build/'qt/json-dictionary-editor'),'--smoke-test'],env),('xcb-interactions.log',[str(build/'qt/jsondict_interaction_tests'),'-v2','-o',str(logs/'interactions.xml')+',xml'],env),('xcb-save-state.log',[str(build/'qt/jsondict_save_state_tests'),'-v2','-o',str(logs/'save-state.xml')+',xml'],dict(env,LD_PRELOAD=str(build/'qt/libjsondict_file_faults.so')))]
 for name,args,environment in cases:
  started=datetime.datetime.now(datetime.timezone.utc).isoformat()
  with (logs/name).open('w') as f:r=subprocess.run(args,env=environment,stdout=f,stderr=subprocess.STDOUT,timeout=120)
  with (logs/'commands.jsonl').open('a') as f:f.write(json.dumps({'command':args,'exit_code':r.returncode,'backend':'xcb/Xvfb TCP','display':env['DISPLAY'],'started':started,'log':name})+'\n')
  if r.returncode:raise SystemExit(name+' failed; raw log retained')
 print('XVFB_TCP_OK: smoke, interactions and save-state; AUTOMATION_ONLY')
finally:
 server.terminate()
 try:server.wait(timeout=5)
 except subprocess.TimeoutExpired:server.kill();server.wait()
 server_log.close()
