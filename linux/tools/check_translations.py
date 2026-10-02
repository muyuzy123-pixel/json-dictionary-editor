#!/usr/bin/env python3
import json,re,xml.etree.ElementTree as ET
from pathlib import Path
root=Path(__file__).resolve().parents[1]
mapping=json.loads((root/'translations/zh.json').read_text())
sources={'String','Number','Boolean','Object','Array','Key / Index','Type','Value'}
patterns=[r'ui\("((?:[^"\\]|\\.)*)"\)',r'key = "((?:[^"\\]|\\.)*)"',r'\{\s*"[^"\n]+"\s*,\s*"((?:[^"\\]|\\.)*)"\}',r'throw std::runtime_error\("((?:[^"\\]|\\.)*)"\)']
for directory in ('qt','src'):
 for p in (root/directory).glob('*.cpp'):
  for pattern in patterns:sources.update(json.loads('"'+s+'"') for s in re.findall(pattern,p.read_text()))
for lang in ('en','zh_CN'):
 tree=ET.parse(root/'translations'/f'editor_{lang}.ts')
 catalog={m.findtext('source'):m.findtext('translation') for m in tree.findall('.//message')}
 missing=sources-catalog.keys()
 if missing:raise SystemExit('Missing messages: '+repr(sorted(missing)))
 for source in sources:
  expected=mapping[source] if lang=='zh_CN' else source
  if catalog[source]!=expected:raise SystemExit('Incorrect translation: '+source)
print(f'TRANSLATIONS_OK: {len(sources)} runtime messages covered')
