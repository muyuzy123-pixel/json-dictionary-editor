# Linux recovery snapshot

Continue from fixed full snapshot 7ee1c5bb722f9041cd2ba513de044ea1a01cf0c5
(parent c60b61622d5e3767cc2f972eec67a29c57f3a60e), branch
codex/linux-cloud-20261002. This recovery text was reconstructed after the
cloud executor went offline. It must be rebuilt and tested before use.
It is not a final accepted release. No GitHub release/main changes were made.

The reusable data model still comes directly from windows/src/json_core.*
and json_search_aliases.hpp and the existing 1.1.1 self-test. No Qt JSON model
or replacement parser is introduced. macos/, windows/, tests/ are unchanged.

## Build

Record actual environment before installing dependencies. Do not assume sudo
or Docker. On the observed managed Ubuntu cloud, root had to use
APT::Sandbox::User=root and a writable custom archives directory because uid
switching and the default archives/partial directory were unavailable.

Ubuntu 24.04 amd64 build dependencies: g++, cmake, qt6-base-dev,
qt6-base-dev-tools, qt6-tools-dev, qt6-l10n-tools, qt6-qpa-plugins, qt6-wayland.
Optional automation/packaging dependencies: xvfb, xauth, desktop-file-utils,
patchelf, squashfs-tools, dpkg-dev. Recommend fonts-noto-cjk and
fonts-noto-color-emoji for correct Chinese/emoji rendering.

From the repository root:

    cmake -S linux -B .build/linux-core -DJDE_BUILD_GUI=OFF -DCMAKE_BUILD_TYPE=Release
    cmake --build .build/linux-core -j2
    ctest --test-dir .build/linux-core --output-on-failure
    python3 linux/tools/check_translations.py
    cmake -S linux -B .build/linux-gui -DJDE_BUILD_GUI=ON -DCMAKE_BUILD_TYPE=Release
    cmake --build .build/linux-gui -j2
    QT_QPA_PLATFORM=offscreen ctest --test-dir .build/linux-gui --output-on-failure
    .build/linux-gui/qt/json-dictionary-editor

Core-only builds never find/link Qt. Qt APIs target system Qt 6.4.2.
prepare_resources.py extracts the original icon PNG (no redrawing) at GUI
configure time. Existing English sample JSON is bundled directly.
python3 linux/tools/verify_cloud.py --xvfb records full logs in .build/evidence.
Xvfb TCP is an automation fallback for cloud Unix-socket restrictions.

## Editing

The single-document tree/inspector supports six kinds, ordered add/duplicate/
delete/move/sort, full-value search using existing bilingual type aliases,
explicit inspector application, and transactional Raw JSON check/format/apply.
Draft resolution before navigation/save/structure/new/open/close is uniformly
Apply / Discard / Cancel. Language is available in main and Raw windows, with
System / Chinese / English and atomic QSettings preference persistence.
Language text updates never rebuild Document, replace draft text, commit or
save data, or reset the tree model. Controls with CR/control characters or
oversized text use Raw JSON to avoid silent text normalization.

Limits remain: original number spelling; ordered members/elements; strict
UTF-8/Unicode and decoded duplicate-key rejection; object root; 512 nested
containers; 250,000 core parse nodes; 50,000 GUI nodes; 16 MiB final UTF-8
including BOM/newline; 4 MiB Raw text; inspector 1,048,576 UTF-16 units and
keys 65,535 units. The original C++ default keys/formatting behavior remains.

## Saving and recovery boundary

Only user-owned local regular files with one link and plain supported metadata
are eligible. Every save uses a same-directory exclusive temporary file,
complete writes/EINTR handling, file fsync, SHA-256+identity baselines,
renameat2 EXCHANGE or NOREPLACE, directory fsync, and verification of both the
installed file identity/content and the actual version replaced.
No direct overwrite fallback exists. Symbolic links (including ancestors),
hard links, special modes, extended attributes/ACLs are explicitly rejected
rather than silently dropping metadata. Basic mode and group are preserved.

Every replacement retains .jsondict-backup-* in the containing directory even
after success. This closes the counterexample where unlinking the last old
version before a final failing fsync loses recovery. A failed post-commit Save
keeps the document unsaved; the target may already contain new bytes. Inspect
both versions, copy the retained old file to a separate recovery name, and
choose deliberately before retrying. Backups are never automatically pruned.

Observed tests used an overlay mounted with fsync=volatile. These test API
behavior, not power-loss durability. Recovery text has not been retested after
the executor went offline. ext4/xfs/btrfs, tmpfs, removable media, remote/NFS/
SMB filesystems, FUSE, portals, arbitrary multiprocess races, rename of parent
directories, crash/power failure, non-root kernel permission enforcement,
and desktop integration remain separately unaccepted. One synchronized
cross-process commit-gap race is covered; it does not establish every
concurrent schedule. Metadata refusal is a policy, not metadata preservation.

## Packaging

Generate .deb/AppImage only after a fresh runnable build and all regressions
pass on Ubuntu 24.04 x86_64. pack_deb.py is a recovery draft and unexecuted.
Qt is dynamic; .deb uses system dependencies. AppImage bundling remains
unfinished: collect exact bundled-library copyright/license/source manifests,
Qt LGPL obligations, and the pinned type-2 runtime's static dependencies before
distribution. Do not substitute an AppDir/shell launcher for a genuine
AppImage. No AppImage binary or full distribution license review exists yet.

## Acceptance remaining

Real GNOME X11/Wayland operations; native open/save dialogs and portal routes;
DPI/scaling; CJK/emoji rendering on desktop; keyboard/focus/clipboard/IME;
language state under real menu operation; crash/restart and recoveries;
.deb installation/uninstallation; AppImage extraction/FUSE launch and Qt
dependency/third-party source/license checks. offscreen/Xvfb QtTest is automatic,
not physical desktop acceptance. Rebuild recovery text before binding hashes
or creating final deliverables.
