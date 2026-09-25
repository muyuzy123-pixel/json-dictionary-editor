#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-$ROOT/.build/shared-tests}"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"

# x86_64 permits unsigned macOS test executables; Apple Silicon uses Rosetta.
# No codesign step and no implicit linker ad-hoc signing.
xcrun swiftc -target x86_64-apple-macosx13.0 -Xlinker -no_adhoc_codesign \
  -D DIRECT_SWIFTC_BUILD \
  -module-cache-path "$OUT/swift-module-cache" \
  "$ROOT/macos/Sources/JSONDictionaryEditor/JSONModel.swift" \
  "$ROOT/macos/Sources/JSONDictionaryEditor/OrderedJSONCodec.swift" \
  "$ROOT/macos/Sources/JSONDictionaryEditor/Localization.swift" \
  "$ROOT/macos/Sources/JSONDictionaryEditor/DirectSwiftCResources.swift" \
  "$ROOT/tests/adapters/swift_adapter.swift" -o "$OUT/swift-adapter"
xcrun clang++ -std=c++17 -O2 -target x86_64-apple-macosx13.0 -Wl,-no_adhoc_codesign \
  -I "$ROOT/windows/src" "$ROOT/windows/src/json_core.cpp" \
  "$ROOT/tests/adapters/cpp_adapter.cpp" -o "$OUT/cpp-adapter"
printf 'Adapters ready: %s\n' "$OUT"
