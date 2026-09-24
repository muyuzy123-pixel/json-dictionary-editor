#!/bin/sh
# Portable core test only; this does not exercise the Win32 UI or file-save code.
set -eu
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
BUILD_DIR=${BUILD_DIR:-"$PROJECT_DIR/build-core"}
if [ "$(uname -s)" = Darwin ]; then
    CXX=${CXX:-$(xcrun --find clang++)}
else
    CXX=${CXX:-clang++}
fi
command -v "$CXX" >/dev/null 2>&1 || { echo "C++ compiler unavailable: $CXX" >&2; exit 2; }
command -v shasum >/dev/null 2>&1 || { echo "shasum is required." >&2; exit 2; }
mkdir -p "$BUILD_DIR"
BUILD_DIR=$(CDPATH= cd -- "$BUILD_DIR" && pwd)
set -- -std=c++17 -Wall -Wextra -Wpedantic -Werror
if [ "$(uname -s)" = Darwin ]; then
    # Avoid the arm64 linker's implicit ad-hoc signature during candidate review.
    # Apple Silicon requires Rosetta to execute this unsigned x86_64 test binary.
    SDK_PATH=${SDKROOT:-$(xcrun --sdk macosx --show-sdk-path)}
    set -- "$@" -target x86_64-apple-macosx13.0 -isysroot "$SDK_PATH" -Wl,-no_adhoc_codesign
fi
"$CXX" --version > "$BUILD_DIR/compiler-version.txt"
sed -n '1,3p' "$BUILD_DIR/compiler-version.txt" > "$BUILD_DIR/build-info.txt"
compiler_path=$(command -v "$CXX")
digest=$(shasum -a 256 "$compiler_path")
printf 'compiler_sha256=%s\n' "${digest%% *}" >> "$BUILD_DIR/build-info.txt"
if [ "$(uname -s)" = Darwin ]; then
    printf 'sdk=%s\n' "$(basename -- "$SDK_PATH")" >> "$BUILD_DIR/build-info.txt"
    printf 'test_target=x86_64-apple-macosx13.0\nimplicit_adhoc_signing=disabled\n' >> "$BUILD_DIR/build-info.txt"
else
    printf 'test_target=compiler default\n' >> "$BUILD_DIR/build-info.txt"
fi
"$CXX" "$@" -O2 "$PROJECT_DIR/src/json_core.cpp" "$PROJECT_DIR/src/core_self_test.cpp" \
    -o "$BUILD_DIR/core-self-test"
"$BUILD_DIR/core-self-test"
digest=$(shasum -a 256 "$BUILD_DIR/core-self-test")
printf 'core_sha256=%s\n' "${digest%% *}" >> "$BUILD_DIR/build-info.txt"
if [ "${RUN_SANITIZERS:-0}" = 1 ]; then
    "$CXX" "$@" -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all \
        "$PROJECT_DIR/src/json_core.cpp" "$PROJECT_DIR/src/core_self_test.cpp" \
        -o "$BUILD_DIR/core-self-test-sanitized"
    "$BUILD_DIR/core-self-test-sanitized"
    digest=$(shasum -a 256 "$BUILD_DIR/core-self-test-sanitized")
    printf 'sanitized_core_sha256=%s\n' "${digest%% *}" >> "$BUILD_DIR/build-info.txt"
fi
