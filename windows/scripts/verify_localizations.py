#!/usr/bin/env python3
"""Check required Win32 language-catalogue literals before release builds."""

from pathlib import Path
import re

source = Path(__file__).resolve().parents[1] / "src"
app = (source / "windows_app.cpp").read_text()
catalogue = (source / "localization.hpp").read_text()
entries = set(re.findall(r'\{L"((?:\\.|[^"\\])*)",\s*L"', catalogue))
required = set(re.findall(r'app_l10n::text\(L"((?:\\.|[^"\\])*)"\)', app))
required.update(re.findall(r'set_label\([^,]+,\s*L"((?:\\.|[^"\\])*)"\)', app))
required.update(re.findall(r'AppendLocalizedMenu\([^;]*?,\s*L"((?:\\.|[^"\\])*)"\)', app))
required.update(re.findall(
    r'(?:CreateChild|CreateWindowExW)\s*\([^;]*?L"(?:STATIC|BUTTON|EDIT)"\s*,\s*L"((?:\\.|[^"\\])*)"',
    app, re.DOTALL))
required.update(re.findall(r'SetStatusKey\(L"((?:\\.|[^"\\])*)"\)', app))
required.update(re.findall(r'L"((?:\\.|[^"\\])*)"\s*,\s*MB_ICON', app))
required = {key for key in required if re.search(r'[\u3400-\u9fff]', key)}
missing = required - entries
if missing:
    raise SystemExit(f"missing English translations: {sorted(missing)}")
reasons = (source / "json_core.hpp").read_text().split("enum class ErrorReason", 1)[1].split("};", 1)[0]
labels = set(re.findall(r'\b([A-Z][A-Za-z]+)\b', reasons)) - {"Unknown"}
translated = set(re.findall(r'case jsondict::ErrorReason::([A-Za-z]+):', app))
if labels - translated:
    raise SystemExit(f"missing structured error translations: {sorted(labels - translated)}")
print(f"LOCALE_OK: {len(required)} UI keys, {len(labels)} structured error reasons")
