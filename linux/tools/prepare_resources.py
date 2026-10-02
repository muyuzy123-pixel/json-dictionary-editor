#!/usr/bin/env python3
"""Copy the original icon's largest embedded PNG; never redraw it."""
import hashlib,struct
from pathlib import Path
root=Path(__file__).resolve().parents[2]
data=(root/'windows/resources/app.ico').read_bytes();pictures=[]
for i in range(struct.unpack_from('<H',data,4)[0]):
 w,h,_,_,_,bits,size,offset=struct.unpack_from('<BBBBHHII',data,6+i*16)
 image=data[offset:offset+size]
 if image.startswith(b'\x89PNG\r\n\x1a\n'):pictures.append((w or 256,h or 256,image))
w,h,png=max(pictures,key=lambda v:v[0]*v[1])
expected='49de8a9dcf56d854156763a0d079fa314a437d459d719f3b0b5e5650863468ff'
if (w,h)!=(256,256) or hashlib.sha256(png).hexdigest()!=expected:raise SystemExit('Unexpected original icon snapshot')
target=root/'linux/resources/json-dictionary-editor.png';target.parent.mkdir(parents=True,exist_ok=True)
if not target.exists() or target.read_bytes()!=png:target.write_bytes(png)
print('ICON_OK: original embedded 256x256 PNG; SHA-256 '+expected)
