# Linux port — initial working snapshot

Target: Ubuntu 24.04 x86_64, C++17, Qt 6 Widgets, CMake. Based on the unchanged
macOS/Windows 1.1.1 source. This Linux directory is an incomplete implementation
snapshot for continuation in a cloud task, not a release or target-environment
acceptance record.

## Core-first build

```sh
cmake -S linux -B .build/linux-core -DJDE_BUILD_GUI=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build .build/linux-core -j2
ctest --test-dir .build/linux-core --output-on-failure
```

`jsondict_core` directly compiles `../windows/src/json_core.cpp`; the existing
`json_core.hpp`, `json_search_aliases.hpp` and `core_self_test.cpp` remain the
source of truth. `jsondict_session` adds transactional inspector/raw changes and
the existing GUI limits. No Qt JSON model or replacement parser is used.

The initial portable check on AppleClang/macOS passed the existing core test,
Linux session regression, and shared C++ fixtures. These are host checks only;
no Linux/Ubuntu run has yet been recorded for this snapshot.

## Current implementation status

- Complete initial core/session targets and session regression executable.
- Draft Linux POSIX file store and file regression cases. Not compiled on Linux
  yet. It requires same-directory temporary files, SHA-256 baselines, fsync,
  `renameat2(RENAME_EXCHANGE/RENAME_NOREPLACE)` and verification of the actual
  replaced version. Atomic-save failures must not degrade to direct overwrite.
- Draft Qt language service with `.qm` resource references. Translation source
  resources and the application/interaction-test targets are not implemented yet.
- Main tree/inspector, unified draft decisions, raw dialog, packaging and desktop
  acceptance still need implementation.

## Required cloud sequence

Record actual architecture, distro, compiler, CMake, Qt, installation permissions
and graphical backends. Run the independent core targets before enabling Qt.
Do not label non-Ubuntu-24.04/x86_64 checks as target acceptance. Continue with
file tests and the Qt GUI, then a first runnable build and regression report;
only after that produce `.deb` and AppImage. Keep automatic checks, actual
desktop operations and unaccepted scenarios distinct. Preserve the existing
macOS/Windows source and language-switching behavior, and do not rebuild their
packages as part of this port.

Use the existing English sample at `../windows/resources/SampleDictionary.json`.
Preserve ordered objects/arrays, original number text, strict Unicode/UTF-8,
object roots, 16 MiB output, 50,000 GUI nodes, 512 nesting levels, 4 MiB raw edits
and the 1.1.1 empty-container search fix.
