#!/bin/sh
# Copyright (c) 2026 muyuzy123-pixel
# SPDX-License-Identifier: MIT
# Optional macOS maintainer tool; ordinary Windows builds consume the checked-in ICO.
set -eu
TOOL_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ "$#" -gt 2 ]; then
    echo 'usage: generate_icon.sh [OUTPUT.ico [PREVIEW_DIRECTORY]]' >&2
    exit 2
fi
OUTPUT=${1:-"$TOOL_DIR/../resources/app.ico"}
SDK=$(xcrun --sdk macosx --show-sdk-path)
COMPILER=$(xcrun --find swiftc)
/usr/bin/arch -x86_64 /usr/bin/true
SCRATCH=$(mktemp -d "${TMPDIR:-/tmp}/jsondict-icon.XXXXXX")
trap 'rm -rf "$SCRATCH"' EXIT HUP INT TERM
"$COMPILER" -swift-version 5 -parse-as-library -O \
    -target x86_64-apple-macosx13.0 -sdk "$SDK" \
    -module-cache-path "$SCRATCH/module-cache" -Xlinker -no_adhoc_codesign \
    "$TOOL_DIR/GenerateIcon.swift" -o "$SCRATCH/generate-icon"
if [ "$#" -eq 2 ]; then
    /usr/bin/arch -x86_64 "$SCRATCH/generate-icon" "$OUTPUT" "$2"
else
    /usr/bin/arch -x86_64 "$SCRATCH/generate-icon" "$OUTPUT"
fi
