#!/usr/bin/env python3
"""Fail if a translated Swift UI/parser key is missing from either language."""

from pathlib import Path
import re

source = Path(__file__).resolve().parents[1] / "Sources" / "JSONDictionaryEditor"
catalogue = source / "Resources"
entry = re.compile(r'^"([^"\n]+)"\s*=\s*"(?:\\.|[^"\\])*";', re.MULTILINE)
keys = {
    language: set(entry.findall((catalogue / f"{language}.lproj" / "Localizable.strings").read_text()))
    for language in ("en", "zh-Hans")
}
if keys["en"] != keys["zh-Hans"]:
    raise SystemExit(f"catalogue mismatch: English only={keys['en'] - keys['zh-Hans']}; "
                     f"Chinese only={keys['zh-Hans'] - keys['en']}")

required = set()
for file in source.glob("*.swift"):
    code = file.read_text()
    required.update(re.findall(r'\btr\("((?:\\.|[^"\\])*)"\)', code))
    if file.name == "OrderedJSONCodec.swift":
        required.update(re.findall(r'\berror\("((?:\\.|[^"\\])*)"\)', code))
        required.update(re.findall(r'\bmessage:\s*"((?:\\.|[^"\\])*)"', code))
required.update(("个键", "个键复数", "个元素", "个元素复数", "个节点", "个节点复数",
                 "个顶层键", "个顶层键复数"))
missing = required - keys["en"]
if missing:
    raise SystemExit(f"missing translations: {sorted(missing)}")
print(f"LOCALE_OK: {len(required)} required keys, matched en/zh-Hans catalogues")
