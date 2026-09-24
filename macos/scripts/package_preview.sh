#!/bin/zsh
# This explicit preview-packaging entry point signs only an independent staging copy.
# build.sh and verify.sh retain their unsigned-build contract.
set -euo pipefail

SCRIPT_DIR="${0:A:h}"
SOURCE_ROOT="${SCRIPT_DIR:h}"
REPO_ROOT="${SOURCE_ROOT:h}"
: "${SOURCE_COMMIT:?Set SOURCE_COMMIT to the frozen 40-character source commit.}"
: "${BUILD_DIR:?Set BUILD_DIR to a completed unsigned build.}"
: "${DIST_DIR:?Set DIST_DIR to the preview archive output directory.}"
RELEASE_TAG="${RELEASE_TAG:-preview-1}"
BUILD_DIR="${BUILD_DIR:A}"
DIST_DIR="${DIST_DIR:A}"
PACKAGE_README="${PACKAGE_README:-$SOURCE_ROOT/README.md}"
PACKAGE_README="${PACKAGE_README:A}"
PACKAGE_WORK_DIR="${PACKAGE_WORK_DIR:-$BUILD_DIR/preview-package}"
PACKAGE_WORK_DIR="${PACKAGE_WORK_DIR:A}"
AUDIT_DIR="${AUDIT_DIR:-$PACKAGE_WORK_DIR/evidence}"
AUDIT_DIR="${AUDIT_DIR:A}"
APP_NAME="JSON字典编辑器.app"
UNSIGNED_APP="$BUILD_DIR/$APP_NAME"
ZIP_NAME="JSONDictionaryEditor-macOS-Universal.zip"
ZIP_OUTPUT="$DIST_DIR/$ZIP_NAME"
EXPECTED_ICON_SHA256="e0a3b35e53ee2fe28bd95b988e6d9555a4902361c3338051c3667e92245ea53d"

fail() { print -u2 -- "$1"; exit 1; }
[[ "$SOURCE_COMMIT" =~ '^[0-9a-f]{40}$' ]] || fail "SOURCE_COMMIT must be a full lowercase Git commit hash."
[[ "$RELEASE_TAG" =~ '^[A-Za-z0-9][A-Za-z0-9._-]*$' ]] || fail "Invalid RELEASE_TAG."
[[ -f "$PACKAGE_README" && -f "$REPO_ROOT/LICENSE" ]] || fail "README or repository LICENSE is missing."
[[ -f "$UNSIGNED_APP/Contents/MacOS/JSONDictionaryEditor" ]] || fail "Unsigned App is missing."
[[ ! -e "$ZIP_OUTPUT" ]] || fail "Preview archive already exists; choose a new DIST_DIR."
[[ ! -e "$PACKAGE_WORK_DIR" ]] || fail "PACKAGE_WORK_DIR already exists; choose a fresh staging directory."
[[ "$PACKAGE_WORK_DIR" != "$UNSIGNED_APP/"* ]] || fail "Staging must be outside the unsigned App."

git_root="$(git -C "$REPO_ROOT" rev-parse --show-toplevel)"
[[ "${git_root:A}" == "$REPO_ROOT" ]] || fail "The candidate directory must have its own Git repository."
[[ "$(git -C "$REPO_ROOT" rev-parse HEAD)" == "$SOURCE_COMMIT" ]] || fail "SOURCE_COMMIT does not match this checkout."
git -C "$REPO_ROOT" diff --quiet || fail "Tracked source files have uncommitted changes."
git -C "$REPO_ROOT" diff --cached --quiet || fail "The source index has uncommitted changes."
[[ -z "$(git -C "$REPO_ROOT" ls-files --others --exclude-standard)" ]] || fail "Untracked source files remain; freeze the source before packaging."
if ! /usr/bin/arch -arm64 /usr/bin/true || ! /usr/bin/arch -x86_64 /usr/bin/true; then
    fail "Final archive validation requires an Apple Silicon Mac with existing Rosetta."
fi

# Validate the exact build inputs and unsigned output before copying anything.
BUILD_DIR="$BUILD_DIR" "$SCRIPT_DIR/verify.sh"
icon_digest="$(shasum -a 256 "$UNSIGNED_APP/Contents/Resources/AppIcon.icns")"
[[ "${icon_digest%% *}" == "$EXPECTED_ICON_SHA256" ]] || fail "Historical macOS icon SHA-256 mismatch; stop before signing."

mkdir -p "${PACKAGE_WORK_DIR:h}"
mkdir "$PACKAGE_WORK_DIR"
mkdir -p "$DIST_DIR" "$AUDIT_DIR"
RUN_AUDIT="$AUDIT_DIR/package-$(date -u +%Y%m%dT%H%M%SZ)-$$"
mkdir "$RUN_AUDIT"
STAGE="$PACKAGE_WORK_DIR/stage"
EXTRACTED="$PACKAGE_WORK_DIR/extracted"
PENDING_ZIP="$PACKAGE_WORK_DIR/$ZIP_NAME"
mkdir "$STAGE" "$EXTRACTED"

ditto --norsrc --noextattr --noqtn "$UNSIGNED_APP" "$STAGE/$APP_NAME"
install -m 644 "$REPO_ROOT/LICENSE" "$STAGE/LICENSE"
install -m 644 "$PACKAGE_README" "$STAGE/README.md"
install -m 644 "$SOURCE_ROOT/Resources/SampleDictionary.json" "$STAGE/SampleDictionary.json"
install -m 644 "$REPO_ROOT/LICENSE" "$STAGE/$APP_NAME/Contents/Resources/LICENSE"

PLIST="$STAGE/$APP_NAME/Contents/Info.plist"
version="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$PLIST")"
build_number="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleVersion' "$PLIST")"
bundle_id="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$PLIST")"
copyright="$(/usr/libexec/PlistBuddy -c 'Print :NSHumanReadableCopyright' "$PLIST")"
[[ "$version" == 1.0.0 && "$build_number" == 1 ]] || fail "Unexpected macOS application version."
[[ "$bundle_id" == com.codex.JSONDictionaryEditor ]] || fail "Unexpected macOS bundle identifier."
[[ "$copyright" == 'Copyright (c) 2026 muyuzy123-pixel' ]] || fail "Unexpected copyright statement."

source_plist="$PACKAGE_WORK_DIR/SOURCE.plist"
plutil -create xml1 "$source_plist"
plutil -insert source_commit -string "$SOURCE_COMMIT" "$source_plist"
plutil -insert tag -string "$RELEASE_TAG" "$source_plist"
plutil -insert app_version -string "$version" "$source_plist"
plutil -insert architecture -string universal "$source_plist"
plutil -convert json -o "$STAGE/SOURCE.json" "$source_plist"
install -m 644 "$STAGE/SOURCE.json" "$STAGE/$APP_NAME/Contents/Resources/SOURCE.json"

# Metadata cleanup and ad-hoc signing are restricted to this fresh staging copy.
xattr -cr "$STAGE/$APP_NAME"
codesign --force --sign - --timestamp=none "$STAGE/$APP_NAME" \
    2>&1 | tee "$RUN_AUDIT/sign.log"
codesign --verify --deep --strict --verbose=4 "$STAGE/$APP_NAME" \
    2>&1 | tee "$RUN_AUDIT/staged-signature.log"
COPYFILE_DISABLE=1 ditto -c -k --norsrc --noextattr --noqtn "$STAGE" "$PENDING_ZIP"
unzip -tq "$PENDING_ZIP" 2>&1 | tee "$RUN_AUDIT/zip-integrity.log"
unzip -Z1 "$PENDING_ZIP" > "$RUN_AUDIT/zip-entries.txt"
ditto -x -k "$PENDING_ZIP" "$EXTRACTED"

# All acceptance commands below use the App extracted from the final ZIP bytes.
FINAL_APP="$EXTRACTED/$APP_NAME"
FINAL_BINARY="$FINAL_APP/Contents/MacOS/JSONDictionaryEditor"
codesign --verify --deep --strict --verbose=4 "$FINAL_APP" \
    2>&1 | tee "$RUN_AUDIT/extracted-signature.log"
signature="$(codesign --display --verbose=4 "$FINAL_APP" 2>&1)"
print -r -- "$signature" > "$RUN_AUDIT/signature-metadata.txt"
[[ "$signature" == *'Signature=adhoc'* && "$signature" == *'TeamIdentifier=not set'* ]] \
    || fail "The archive is not exclusively ad-hoc signed."
[[ "$signature" != *'Authority='* ]] || fail "Unexpected signing authority."
architectures="$(xcrun lipo -archs "$FINAL_BINARY")"
[[ "$architectures" == *arm64* && "$architectures" == *x86_64* ]] || fail "Universal architectures are missing."
print -r -- "$architectures" > "$RUN_AUDIT/architectures.txt"
plutil -lint "$FINAL_APP/Contents/Info.plist" | tee "$RUN_AUDIT/plist-check.log"
cmp "$SOURCE_ROOT/Resources/Info.plist" "$FINAL_APP/Contents/Info.plist"
cmp "$REPO_ROOT/LICENSE" "$EXTRACTED/LICENSE"
cmp "$REPO_ROOT/LICENSE" "$FINAL_APP/Contents/Resources/LICENSE"
cmp "$PACKAGE_README" "$EXTRACTED/README.md"
cmp "$SOURCE_ROOT/Resources/SampleDictionary.json" "$EXTRACTED/SampleDictionary.json"
cmp "$EXTRACTED/SOURCE.json" "$FINAL_APP/Contents/Resources/SOURCE.json"
[[ "$(plutil -extract source_commit raw -o - "$EXTRACTED/SOURCE.json")" == "$SOURCE_COMMIT" ]]
[[ "$(plutil -extract tag raw -o - "$EXTRACTED/SOURCE.json")" == "$RELEASE_TAG" ]]
[[ "$(plutil -extract app_version raw -o - "$EXTRACTED/SOURCE.json")" == "$version" ]]
[[ "$(plutil -extract architecture raw -o - "$EXTRACTED/SOURCE.json")" == universal ]]

icon_digest="$(shasum -a 256 "$FINAL_APP/Contents/Resources/AppIcon.icns")"
[[ "${icon_digest%% *}" == "$EXPECTED_ICON_SHA256" ]] || fail "Extracted historical icon SHA-256 mismatch."
print -r -- "$icon_digest" > "$RUN_AUDIT/icon-sha256.txt"
iconutil -c iconset "$FINAL_APP/Contents/Resources/AppIcon.icns" -o "$PACKAGE_WORK_DIR/AppIcon.iconset"
[[ -f "$PACKAGE_WORK_DIR/AppIcon.iconset/icon_512x512@2x.png" ]] || fail "The expected icon representation is absent."
for architecture in arm64 x86_64; do
    actual_version="$(/usr/bin/arch -"$architecture" "$FINAL_BINARY" --version)"
    print -r -- "$actual_version" > "$RUN_AUDIT/version-$architecture.txt"
    [[ "$actual_version" == "JSON 字典编辑器 $version ($build_number)" ]] || fail "CLI version mismatch."
    /usr/bin/arch -"$architecture" "$FINAL_BINARY" --self-test \
        2>&1 | tee "$RUN_AUDIT/self-test-$architecture.log"
    /usr/bin/arch -"$architecture" "$FINAL_BINARY" --validate-json "$EXTRACTED/SampleDictionary.json" \
        2>&1 | tee "$RUN_AUDIT/sample-$architecture.log"
done

# Signing must not have touched any of the original unsigned build files.
(
    cd "$BUILD_DIR"
    shasum -a 256 -c SHA256SUMS
) | tee "$RUN_AUDIT/unsigned-input-preserved.log"
mv "$PENDING_ZIP" "$ZIP_OUTPUT"
(
    cd "$DIST_DIR"
    shasum -a 256 "$ZIP_NAME"
) > "$RUN_AUDIT/SHA256SUMS"
install -m 644 "$EXTRACTED/SOURCE.json" "$RUN_AUDIT/SOURCE.json"
print "PREVIEW_PACKAGE_OK: $ZIP_OUTPUT"
print "Evidence: $RUN_AUDIT"
print "Extracted App: $FINAL_APP"
print "Ad-hoc signature only; no Developer ID, notarization, GUI acceptance, or remote publication."
