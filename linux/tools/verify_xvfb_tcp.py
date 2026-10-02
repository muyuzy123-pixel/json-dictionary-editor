#!/usr/bin/env python3
"""Automation via Xvfb TCP when a managed cloud disallows Unix sockets."""
import argparse,os,socket,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--build',default='.build/linux-gui');p.add_argument('--logs',default='.build/evidence');args=p.parse_args()
root=Path(__file__).resolve().parents[2];build=(root/args.build).resolve();logs=(root/args.logs).resolve();logs.mkdir(parents=True,exist_ok=True)
runtime=logs/'runtime';runtime.mkdir(exist_ok=True);runtime.chmod(0o700)
server_log=(logs/'xvfb-server.log').open('w')
server=subprocess.Popen(['Xvfb',':77','-screen','0','1280x800x24','-nolisten','unix','-nolisten','local','-listen','tcp','-ac'],stdout=server_log,stderr=subprocess.STDOUT)
try:
 ready=False
 for _ in range(100):
  if server.poll() is not None:break
  try:
   with socket.create_connection(('127.0.0.1',6077),.1):ready=True;break
  except OSError:time.sleep(.1)
 if not ready:raise SystemExit('Xvfb TCP unavailable; inspect xvfb-server.log (not desktop acceptance)')
 env=dict(os.environ,DISPLAY='127.0.0.1:77',QT_QPA_PLATFORM='xcb',XDG_RUNTIME_DIR=str(runtime),JDE_TEST_SCREENSHOTS=str(logs))
 for name,command in [('xcb-smoke.log',[str(build/'qt/json-dictionary-editor'),'--smoke-test']),('xcb-interactions.log',[str(build/'qt/jsondict_interaction_tests'),'-v2'])]:
  with (logs/name).open('w') as out:subprocess.run(command,env=env,stdout=out,stderr=subprocess.STDOUT,timeout=120,check=True)
 print('XVFB_TCP_OK: automated xcb checks only; no physical desktop operation')
finally:
 server.terminate()
 try:server.wait(timeout=5)
 except subprocess.TimeoutExpired:server.kill();server.wait()
 server_log.close()
