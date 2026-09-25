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
python3 "$SOURCE_ROOT/scripts/verify_localizations.py"
plutil -lint "$SOURCE_ROOT/Resources/Info.plist" \
    "$SOURCE_ROOT/Resources/LocalizationBundleInfo.plist" \
    "$SOURCE_ROOT/Resources/en.lproj/InfoPlist.strings" \
    "$SOURCE_ROOT/Resources/zh-Hans.lproj/InfoPlist.strings" \
    "$SOURCE_ROOT/Sources/JSONDictionaryEditor/Resources/en.lproj/Localizable.strings" \
    "$SOURCE_ROOT/Sources/JSONDictionaryEditor/Resources/zh-Hans.lproj/Localizable.strings"
mkdir -p "$BUILD_DIR" "$APP_PATH/Contents/MacOS" "$APP_PATH/Contents/Resources"

{
    print "Build date (UTC): $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    print "Deployment target: macOS 13.0; architectures: arm64 x86_64"
    print "Build method: direct swiftc; Swift language mode 5; DIRECT_SWIFTC_BUILD resources; no SwiftPM execution"
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
        Resources/LocalizationBundleInfo.plist Resources/en.lproj/InfoPlist.strings \
        Resources/zh-Hans.lproj/InfoPlist.strings \
        Sources/JSONDictionaryEditor/Resources/en.lproj/Localizable.strings \
        Sources/JSONDictionaryEditor/Resources/zh-Hans.lproj/Localizable.strings \
        Sources/JSONDictionaryEditor/*.swift scripts/*.sh scripts/verify_localizations.py
) > "$BUILD_DIR/INPUTS.sha256"

for architecture in arm64 x86_64; do
    mkdir -p "$BUILD_DIR/module-cache-$architecture" "$BUILD_DIR/$architecture"
    "$SWIFT_COMPILER" \
        -swift-version 5 -O -whole-module-optimization -parse-as-library \
        -D DIRECT_SWIFTC_BUILD \
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

# Match SwiftPM's resource-bundle layout and exact lookup spelling without
# running SwiftPM (which synthesizes Bundle.module only for its own builds).
for destination in "$APP_PATH/Contents/Resources" "$BUILD_DIR/arm64" "$BUILD_DIR/x86_64"; do
    bundle="$destination/JSONDictionaryEditor_JSONDictionaryEditor.bundle"
    mkdir -p "$bundle/en.lproj" "$bundle/zh-hans.lproj"
    install -m 644 "$SOURCE_ROOT/Resources/LocalizationBundleInfo.plist" "$bundle/Info.plist"
    install -m 644 "$SOURCE_ROOT/Sources/JSONDictionaryEditor/Resources/en.lproj/Localizable.strings" \
        "$bundle/en.lproj/Localizable.strings"
    install -m 644 "$SOURCE_ROOT/Sources/JSONDictionaryEditor/Resources/zh-Hans.lproj/Localizable.strings" \
        "$bundle/zh-hans.lproj/Localizable.strings"
done
for language in en zh-Hans; do
    mkdir -p "$APP_PATH/Contents/Resources/$language.lproj"
    install -m 644 "$SOURCE_ROOT/Resources/$language.lproj/InfoPlist.strings" \
        "$APP_PATH/Contents/Resources/$language.lproj/InfoPlist.strings"
done

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
        "JSON字典编辑器.app/Contents/Resources/AppIcon.icns" \
        "JSON字典编辑器.app/Contents/Resources/en.lproj/InfoPlist.strings" \
        "JSON字典编辑器.app/Contents/Resources/zh-Hans.lproj/InfoPlist.strings" \
        "JSON字典编辑器.app/Contents/Resources/JSONDictionaryEditor_JSONDictionaryEditor.bundle/Info.plist" \
        "JSON字典编辑器.app/Contents/Resources/JSONDictionaryEditor_JSONDictionaryEditor.bundle/en.lproj/Localizable.strings" \
        "JSON字典编辑器.app/Contents/Resources/JSONDictionaryEditor_JSONDictionaryEditor.bundle/zh-hans.lproj/Localizable.strings" \
        "arm64/JSONDictionaryEditor_JSONDictionaryEditor.bundle/Info.plist" \
        "arm64/JSONDictionaryEditor_JSONDictionaryEditor.bundle/en.lproj/Localizable.strings" \
        "arm64/JSONDictionaryEditor_JSONDictionaryEditor.bundle/zh-hans.lproj/Localizable.strings" \
        "x86_64/JSONDictionaryEditor_JSONDictionaryEditor.bundle/Info.plist" \
        "x86_64/JSONDictionaryEditor_JSONDictionaryEditor.bundle/en.lproj/Localizable.strings" \
        "x86_64/JSONDictionaryEditor_JSONDictionaryEditor.bundle/zh-hans.lproj/Localizable.strings"
) > "$BUILD_DIR/SHA256SUMS"
print "UNSIGNED_BUILD_OK: $APP_PATH"
print "此构建不签名、不公证、不打发布包；Apple 芯片上的原生 GUI 运行未验证。"
