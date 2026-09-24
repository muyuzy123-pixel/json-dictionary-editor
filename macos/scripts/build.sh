#!/bin/zsh
set -euo pipefail

SCRIPT_DIR="${0:A:h}"
SOURCE_ROOT="${SCRIPT_DIR:h}"
BUILD_DIR="${BUILD_DIR:-${SOURCE_ROOT:h}/build/macos}"
BUILD_DIR="${BUILD_DIR:A}"
SDK_PATH="${MACOS_SDK_PATH:-$(xcrun --sdk macosx --show-sdk-path)}"
SWIFT_COMPILER="$(xcrun --find swiftc)"
APP_PATH="$BUILD_DIR/JSON字典编辑器.app"

if [[ ! -d "$SDK_PATH" ]]; then
    print -u2 "SDK 不存在：$SDK_PATH；可设置 MACOS_SDK_PATH 或 DEVELOPER_DIR。"
    exit 2
fi
if [[ -e "$BUILD_DIR/SHA256SUMS" ]]; then
    print -u2 "该 BUILD_DIR 已有完整构建；请指定新目录，以保留旧的摘要记录。"
    exit 2
fi
mkdir -p "$BUILD_DIR" "$APP_PATH/Contents/MacOS" "$APP_PATH/Contents/Resources"

{
    print "Build date (UTC): $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    print "Deployment target: macOS 13.0; architectures: arm64 x86_64"
    print "Build method: direct swiftc; Swift language mode 5; no SwiftPM execution"
    print "Signing: disabled, including linker ad-hoc signing (-no_adhoc_codesign)"
    print "Host: $(sw_vers -productVersion) ($(sw_vers -buildVersion)), $(uname -m)"
    print "SDK version: $(xcrun --sdk "$SDK_PATH" --show-sdk-version)"
    print "SDK path: $SDK_PATH"
    "$SWIFT_COMPILER" --version
    xcrun clang --version
    print "Compiler executable SHA-256 (not the whole toolchain):"
    shasum -a 256 "$SWIFT_COMPILER"
    print "SDKSettings.plist SHA-256 (not the whole SDK):"
    shasum -a 256 "$SDK_PATH/SDKSettings.plist"
} > "$BUILD_DIR/BUILD_INFO.txt"
(
    cd "$SOURCE_ROOT"
    shasum -a 256 Package.swift Resources/Info.plist Resources/AppIcon.icns Resources/SampleDictionary.json \
        Sources/JSONDictionaryEditor/*.swift scripts/*.sh
) > "$BUILD_DIR/INPUTS.sha256"

for architecture in arm64 x86_64; do
    mkdir -p "$BUILD_DIR/module-cache-$architecture" "$BUILD_DIR/$architecture"
    "$SWIFT_COMPILER" \
        -swift-version 5 -O -whole-module-optimization -parse-as-library \
        -module-name JSONDictionaryEditor \
        -sdk "$SDK_PATH" -target "$architecture-apple-macosx13.0" \
        -module-cache-path "$BUILD_DIR/module-cache-$architecture" \
        -Xlinker -no_adhoc_codesign \
        "$SOURCE_ROOT"/Sources/JSONDictionaryEditor/*.swift \
        -o "$BUILD_DIR/$architecture/JSONDictionaryEditor" \
        2>&1 | tee "$BUILD_DIR/build-$architecture.log"
done

xcrun lipo -create \
    "$BUILD_DIR/arm64/JSONDictionaryEditor" \
    "$BUILD_DIR/x86_64/JSONDictionaryEditor" \
    -output "$APP_PATH/Contents/MacOS/JSONDictionaryEditor"
install -m 644 "$SOURCE_ROOT/Resources/Info.plist" "$APP_PATH/Contents/Info.plist"
install -m 644 "$SOURCE_ROOT/Resources/AppIcon.icns" "$APP_PATH/Contents/Resources/AppIcon.icns"

for binary in "$BUILD_DIR/arm64/JSONDictionaryEditor" \
              "$BUILD_DIR/x86_64/JSONDictionaryEditor" \
              "$APP_PATH/Contents/MacOS/JSONDictionaryEditor"; do
    load_commands="$(xcrun otool -l "$binary")"
    if [[ "$load_commands" == *LC_CODE_SIGNATURE* ]]; then
        print -u2 "构建产物意外含代码签名，停止：$binary"
        exit 1
    fi
done
(
    cd "$BUILD_DIR"
    shasum -a 256 BUILD_INFO.txt INPUTS.sha256 \
        arm64/JSONDictionaryEditor x86_64/JSONDictionaryEditor \
        "JSON字典编辑器.app/Contents/MacOS/JSONDictionaryEditor" \
        "JSON字典编辑器.app/Contents/Info.plist" \
        "JSON字典编辑器.app/Contents/Resources/AppIcon.icns"
) > "$BUILD_DIR/SHA256SUMS"
print "UNSIGNED_BUILD_OK: $APP_PATH"
print "此构建不签名、不公证、不打发布包；Apple 芯片上的原生 GUI 运行未验证。"
