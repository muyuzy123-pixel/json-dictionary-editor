# JSON Dictionary Editor for Windows

This native Win32 editor preserves JSON object member order and the original text of numbers. It provides a tree on the left, a typed inspector on the right, and a transactional raw JSON editor.

Version 1.1.1 fixes false “empty object/array” search matches for nonempty containers, includes localization checks in the build, and ships an English-language sample JSON file. Language switching retains its existing behavior.

## Language

Version 1.1.0 includes Simplified Chinese and English in one executable. In either the main window or the raw JSON editor, open **语言 / Language** and choose **跟随系统 / System**, **简体中文**, or **English**. First launch follows the Windows UI language: Chinese systems use Simplified Chinese; others use English. Manual selection is stored per user in `%LOCALAPPDATA%\JSONDictionaryEditor\settings.ini`. Switching languages immediately updates application text without saving the file, applying a draft, changing tree selection, or replacing the raw JSON text. Native file dialogs continue to follow Windows.

## Editor and data rules

- Use the tree, inspector, search, and toolbar to add, duplicate, delete, move, rename, sort, and edit JSON nodes.
- Six types are supported: string, number, Boolean, null, object, and array. A dictionary must have an object root.
- Key and value drafts are applied explicitly. Before navigation, saving, or a structural action, you can Apply, Discard, or Cancel.
- The raw JSON editor checks, formats, and transactionally applies a node or the whole document.
- Objects keep member order; arrays keep element order; numbers like `1.2300e+04` keep their original spelling.
- Files must be valid UTF-8. UTF-8 BOM can be retained; UTF-16 BOM is rejected. Duplicate decoded keys, invalid Unicode, comments, trailing commas, and non-JSON numbers are rejected.
- GUI limits: 16 MiB UTF-8 output, 50,000 nodes, 512 nesting levels, and 4 MiB per raw edit.

Saving uses a same-directory temporary file and checks file identity and SHA-256 to detect external changes. The program does not upload data. File names and JSON values are never translated when switching the UI language.

## Distribution and build

Use the x64 executable for standard Intel/AMD Windows 10 or 11 computers and the ARM64 executable for Windows on ARM. Each portable ZIP includes one EXE, sample JSON, Chinese and English guides, a Windows verification script, and a SHA-256 manifest. The EXEs require no installer or administrator rights and are unsigned, so Windows may show an unknown-publisher warning. Check the supplied SHA-256 digest before running.

From the repository root, use `powershell -NoProfile -File .\windows\scripts\build_on_windows.ps1 -Architecture x64` with Visual Studio 2022, CMake and the Windows SDK. The output is under `windows\build-windows-x64\Release\`. The script runs the localization check before building and CTest afterward. Run `windows\scripts\verify_on_windows.ps1` separately for diagnostic validation and a short window-liveness check. For macOS cross-builds, set `LLVM_MINGW_ROOT` explicitly and run `sh windows/scripts/build_cross_macos.sh`; release packaging and provenance checks are documented in `docs/RELEASING.md`.

The cross-build verifies PE format, resources, dependencies, and tests, but cannot confirm on-screen behavior. On Windows, check the language menu and its persistence, drafts and raw text during switching, file dialogs, search results, DPI layouts, keyboard use, drag and drop, and external-file conflict prompts.

This is a local 1.1.1 release candidate. The existing `preview-1` release is Windows 1.0.1; final 1.1.1 status and limitations are recorded in `docs/releases/1.1.1.md` at the repository root.
