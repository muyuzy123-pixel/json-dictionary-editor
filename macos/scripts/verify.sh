#!/bin/zsh
set -euo pipefail

SCRIPT_DIR="${0:A:h}"
SOURCE_ROOT="${SCRIPT_DIR:h}"
BUILD_DIR="${BUILD_DIR:-${SOURCE_ROOT:h}/build/macos}"
BUILD_DIR="${BUILD_DIR:A}"
APP_PATH="$BUILD_DIR/JSON字典编辑器.app"
BINARY_PATH="$APP_PATH/Contents/MacOS/JSONDictionaryEditor"
python3 "$SOURCE_ROOT/scripts/verify_localizations.py"

(
    cd "$SOURCE_ROOT"
    shasum -a 256 -c "$BUILD_DIR/INPUTS.sha256"
)
(
    cd "$BUILD_DIR"
    shasum -a 256 -c SHA256SUMS
)
plutil -lint "$APP_PATH/Contents/Info.plist"
[[ "$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$APP_PATH/Contents/Info.plist")" == "com.codex.JSONDictionaryEditor" ]]
version="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$APP_PATH/Contents/Info.plist")"
build_number="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleVersion' "$APP_PATH/Contents/Info.plist")"
[[ "$version" == 1.1.1 && "$build_number" == 3 ]]
[[ "$(/usr/bin/arch -x86_64 "$BINARY_PATH" --version)" == "JSON Dictionary Editor $version ($build_number)" ]]
architectures="$(xcrun lipo -archs "$BINARY_PATH")"
[[ "$architectures" == *arm64* && "$architectures" == *x86_64* ]]
load_commands="$(xcrun otool -l "$BINARY_PATH")"
[[ "$load_commands" != *LC_CODE_SIGNATURE* ]]

/usr/bin/arch -x86_64 "$BINARY_PATH" --self-test
/usr/bin/arch -x86_64 "$BINARY_PATH" --validate-json "$SOURCE_ROOT/Resources/SampleDictionary.json"

CHECK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/json-dictionary-verify.XXXXXX")"
trap '/bin/rm -rf "$CHECK_DIR"' EXIT
print -r -- '{"duplicate": 1, "duplicate": 2}' > "$CHECK_DIR/invalid.json"
set +e
/usr/bin/arch -x86_64 "$BINARY_PATH" --validate-json "$CHECK_DIR/invalid.json"
invalid_result=$?
/usr/bin/arch -x86_64 "$BINARY_PATH" --validate-json
usage_result=$?
set -e
[[ "$invalid_result" == 1 && "$usage_result" == 2 ]]
iconutil -c iconset "$APP_PATH/Contents/Resources/AppIcon.icns" -o "$CHECK_DIR/AppIcon.iconset"
[[ -f "$CHECK_DIR/AppIcon.iconset/icon_512x512@2x.png" ]]
cmp "$SOURCE_ROOT/Resources/AppIcon.icns" "$APP_PATH/Contents/Resources/AppIcon.icns"
bundle="$APP_PATH/Contents/Resources/JSONDictionaryEditor_JSONDictionaryEditor.bundle"
cmp "$SOURCE_ROOT/Resources/LocalizationBundleInfo.plist" "$bundle/Info.plist"
cmp "$SOURCE_ROOT/Sources/JSONDictionaryEditor/Resources/en.lproj/Localizable.strings" \
    "$bundle/en.lproj/Localizable.strings"
cmp "$SOURCE_ROOT/Sources/JSONDictionaryEditor/Resources/zh-Hans.lproj/Localizable.strings" \
    "$bundle/zh-hans.lproj/Localizable.strings"
for language in en zh-Hans; do
    cmp "$SOURCE_ROOT/Resources/$language.lproj/InfoPlist.strings" \
        "$APP_PATH/Contents/Resources/$language.lproj/InfoPlist.strings"
done
print "VERIFY_OK: unsigned Universal structure, input/output hashes, x86_64 core tests, JSON validation and failure exit codes"
print "未执行 arm64 程序或 GUI 验收；x86_64 在 Apple 芯片主机上通过 Rosetta 运行。"
