# JSON Dictionary Editor for macOS

A native macOS editor for JSON dictionaries. It preserves object member order and the exact text of numbers while supporting nested objects, arrays, and all six JSON value types. Files stay on your device.

Version 1.1.1 limits “empty object/array” search matches to empty containers, checks that the Chinese localization resource actually loads, and provides an English-language sample JSON file.

## Language

Version 1.1.0 includes Simplified Chinese and English. In the always-visible **语言 / Language** menu, choose **跟随系统 / System**, **简体中文**, or **English**. On first launch the app follows the system UI language: Chinese systems use Simplified Chinese; others use English. Your choice is saved in the app's own preferences and takes effect immediately in every document window and the raw JSON editor. Switching languages does not save a file or apply an editing draft. Finder and native file dialogs continue to use the macOS language.

## Requirements and use

- macOS 13 or later, on Apple silicon or Intel.
- Open a `.json` file or press `⌘N` for a new empty object. The root must be a JSON object.
- Use the left tree to select a node, then edit its key, type, or value in the right inspector. Apply key and number drafts explicitly.
- Add, duplicate, delete, move, expand, search, and sort nodes with the toolbar and context menu.
- The raw JSON editor checks and formats a selected node or the whole document before applying it. A parse error leaves the document unchanged.
- Choose two or four spaces, tabs, or compact output, and optionally keep a final newline. Press `⌘S` to save.

The parser rejects duplicate decoded keys, invalid UTF-8, malformed Unicode escapes, non-JSON numbers, comments, and trailing commas. Numbers such as `1.2300e+04` retain their original spelling. Changing a nonempty container to another type and deleting a nonempty container require confirmation.

## Diagnostic commands

Run the executable inside the app bundle with `--self-test`, `--validate-json <path>`, `--version`, or `--help`. Diagnostic status prefixes and exit codes remain stable across languages. Human-readable help and errors use the selected language.

## Build and verify

From the repository root, run `BUILD_DIR=/tmp/json-editor-111-build ./macos/scripts/build.sh` followed by `BUILD_DIR=/tmp/json-editor-111-build ./macos/scripts/verify.sh`. This direct-swiftc path validates translations and packages both language directories into an unsigned Universal app. Separate preview packaging signs only a staging copy; see `docs/RELEASING.md` from the repository root.

This is a local 1.1.1 release candidate; the existing `preview-1` download contains 1.0.0 (1). The final candidate build and UI results are recorded in the repository’s `docs/releases/1.1.1.md`. The historical AX-direct-setValue save hang remains a separate observation; failure to reproduce it is not proof of a fix. Normal edit/save/reopen must pass before promotion to a final release.
