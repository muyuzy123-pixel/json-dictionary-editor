#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
REPO_ROOT=$(CDPATH= cd -- "$PROJECT_DIR/.." && pwd)
BUILD_DIR=${BUILD_DIR:-"$PROJECT_DIR/build-cross"}
DIST_DIR=${DIST_DIR:-"$PROJECT_DIR/dist"}
ARCHS=${ARCHS:-"x64 arm64"}

if [ -z "${LLVM_MINGW_ROOT:-}" ]; then
    echo "Set LLVM_MINGW_ROOT to an extracted LLVM-MinGW UCRT toolchain." >&2
    exit 2
fi
TOOLCHAIN=$LLVM_MINGW_ROOT
if [ -z "$ARCHS" ]; then
    echo "ARCHS must contain x64, arm64, or both." >&2
    exit 2
fi
for label in $ARCHS; do
    case "$label" in
        x64) target=x86_64 ;;
        arm64) target=aarch64 ;;
        *) echo "Unsupported architecture: $label" >&2; exit 2 ;;
    esac
    for name in "$target-w64-mingw32-clang++" "$target-w64-mingw32-windres" llvm-strip; do
        if [ ! -x "$TOOLCHAIN/bin/$name" ]; then
            echo "Missing tool: $name" >&2
            exit 2
        fi
    done
done
command -v shasum >/dev/null 2>&1 || { echo "shasum is required." >&2; exit 2; }
if [ -n "${LLVM_MINGW_ARCHIVE:-}" ] && [ ! -f "$LLVM_MINGW_ARCHIVE" ]; then
    echo "LLVM_MINGW_ARCHIVE does not name a file." >&2
    exit 2
fi
mkdir -p "$BUILD_DIR" "$DIST_DIR"
BUILD_DIR=$(CDPATH= cd -- "$BUILD_DIR" && pwd)
DIST_DIR=$(CDPATH= cd -- "$DIST_DIR" && pwd)
METADATA="$BUILD_DIR/build-info.txt"
INPUTS="$BUILD_DIR/build-inputs.sha256"
# These files are the build and preview-package inputs, relative to the repository.
# Public verification reports can be added later without changing these inputs.
(
    cd "$REPO_ROOT"
    shasum -a 256 windows/src/*.cpp windows/src/*.hpp \
        windows/resources/app.rc windows/resources/app.manifest windows/resources/app.ico \
        windows/resources/SampleDictionary.json windows/README.md \
        windows/scripts/build_cross_macos.sh windows/scripts/package_preview.sh \
        windows/scripts/verify_on_windows.ps1 LICENSE THIRD_PARTY_NOTICES.txt \
        licenses/LLVM-LICENSE.TXT licenses/COPYING.MinGW-w64-runtime.txt
) > "$INPUTS"
input_digest=$(shasum -a 256 "$INPUTS")
printf 'project_version=1.0.1\narchitectures=%s\n' "$ARCHS" > "$METADATA"
printf 'source_commit=%s\ninputs_sha256=%s\n' "${SOURCE_COMMIT:-not supplied}" "${input_digest%% *}" >> "$METADATA"
if [ -n "${LLVM_MINGW_ARCHIVE:-}" ]; then
    archive_digest=$(shasum -a 256 "$LLVM_MINGW_ARCHIVE")
    archive_hash=${archive_digest%% *}
    printf 'toolchain_archive_sha256=%s\n' "$archive_hash" >> "$METADATA"
else
    printf 'toolchain_archive_sha256=not supplied\n' >> "$METADATA"
fi
# Target-prefixed executables are wrappers in LLVM-MinGW. Record both them
# and the implementation binaries; the archive digest covers the full toolchain.
for tool in clang ld.lld llvm-rc; do
    if [ -f "$TOOLCHAIN/bin/$tool" ]; then
        digest=$(shasum -a 256 "$TOOLCHAIN/bin/$tool")
        printf 'tool_binary_sha256=%s %s\n' "${digest%% *}" "$tool" >> "$METADATA"
    fi
done

build_target() {
    target=$1
    label=$2
    compiler="$TOOLCHAIN/bin/$target-w64-mingw32-clang++"
    windres="$TOOLCHAIN/bin/$target-w64-mingw32-windres"
    resource="$BUILD_DIR/app-$label.res"
    output="$DIST_DIR/JSONDictionaryEditor-Windows-$label.exe"
    temporary="$BUILD_DIR/JSONDictionaryEditor-Windows-$label.tmp.exe"
    link_map="$BUILD_DIR/link-$label.map"

    # Capture commands before filtering so metadata failures cannot be hidden by a pipe.
    "$compiler" --version > "$BUILD_DIR/compiler-$label.txt"
    sed -n '1,3p' "$BUILD_DIR/compiler-$label.txt" >> "$METADATA"
    for tool in "$target-w64-mingw32-clang++" "$target-w64-mingw32-windres" llvm-strip; do
        digest=$(shasum -a 256 "$TOOLCHAIN/bin/$tool")
        printf 'tool_sha256=%s %s\n' "${digest%% *}" "$tool" >> "$METADATA"
    done
    (cd "$PROJECT_DIR/resources" && "$windres" --codepage=65001 app.rc -O coff -o "$resource")
    "$compiler" \
        -std=c++17 -O2 -DNDEBUG \
        -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_WIN32_WINNT=0x0A00 \
        -Wall -Wextra -Wpedantic -Werror -ffunction-sections -fdata-sections \
        "$PROJECT_DIR/src/json_core.cpp" "$PROJECT_DIR/src/windows_app.cpp" "$resource" \
        -o "$temporary" -mwindows -municode -static -static-libgcc -static-libstdc++ \
        "-Wl,-Map,$link_map" -Wl,--gc-sections -lbcrypt -lcomctl32 -lcomdlg32 -lshell32 -lole32 \
        -luxtheme -lgdi32 -luser32
    "$TOOLCHAIN/bin/llvm-strip" "$temporary"
    mv -f "$temporary" "$output"
    map_digest=$(shasum -a 256 "$link_map")
    printf 'link_map_sha256=%s link-%s.map\n' "${map_digest%% *}" "$label" >> "$METADATA"
    digest=$(shasum -a 256 "$output")
    printf 'output_sha256=%s %s\n' "${digest%% *}" "$(basename -- "$output")" >> "$METADATA"
    printf 'Built %s\n' "$output"
}
for label in $ARCHS; do
    case "$label" in
        x64) build_target x86_64 x64 ;;
        arm64) build_target aarch64 arm64 ;;
    esac
done
printf 'Cross-build completed; metadata: %s\n' "$METADATA"
