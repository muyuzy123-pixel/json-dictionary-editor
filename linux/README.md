# Linux cloud build: Ubuntu 24.04 x86_64

Full fixed base: 7ee1c5bb722f9041cd2ba513de044ea1a01cf0c5.
Parent: c60b61622d5e3767cc2f972eec67a29c57f3a60e.
Development branch: codex/linux-cloud-20261002.
Earlier disconnected-build observations do not certify recovered-source bytes.
The current exact commit, test names/commands/exits/backends, failures/skips,
environment and hashes are provided in the independent-readback delivery report.

The original C++ Document/Node, json_core.*, json_search_aliases.hpp, 1.1.1
self-test, macos/, windows/, shared tests/sample, original icon and MIT license
are unchanged. No replacement parser or Qt JSON numeric model.

## Preserve first, then build

Before long development, independently read back a unique externally saved
file and a full-history git bundle, verify SHA-256, git bundle verify and clone.
An upload receipt alone is insufficient. Do not copy local credentials.
GitHub write access is optional when this persistent bundle route succeeds.

Record actual OS/architecture, UID and namespace/installation permissions,
compiler/CMake/Qt, display variables/platform plugins and mounts. No sudo,
Docker, local Mac/QEMU or host-OS replacement prerequisite.
Ubuntu dependencies: g++, CMake, Python3, qt6-base-dev/base-dev-tools,
qt6-tools-dev, qt6-l10n-tools, qt6-qpa-plugins, qt6-wayland.
Optional: Xvfb, desktop-file-utils, dpkg-dev. Recommend Noto CJK/emoji fonts.
Qt 6.4 system APIs and dynamic libraries are used.

From a clean delivered-commit checkout:

    cmake -S linux -B .build/linux-core -DJDE_BUILD_GUI=OFF -DCMAKE_BUILD_TYPE=Release
    cmake --build .build/linux-core -j2
    ctest --test-dir .build/linux-core --output-on-failure
    python3 linux/tools/verify_cloud.py --fresh --xvfb
    .build/linux-gui/qt/json-dictionary-editor

Core OFF never finds or links Qt. verify_cloud.py performs core first, then GUI,
all CTest groups, offscreen and optional supervised TCP Xvfb xcb checks. It saves
complete logs, command exits, per-test inventories, JUnit/QtTest XML, protected
hashes, clean HEAD and binary hash in .build/evidence/verified.
The script does not replace the external preservation preflight.
Fixture exit 77 is a skip, not a pass. Original icon PNG is extracted/hash checked;
the shared English sample is bundled directly.

## Editing and failed saves

Single-document tree/inspector/ordered mutations/bilingual search/Raw JSON.
System/Simplified Chinese/English and preferences in main and Raw windows.
Language switching does not change document, node IDs, selection/expansion/search,
pending drafts, scroll/cursor, or automatically apply/save/reset the tree.
Unsafe inspector controls/CR/oversized text route to Raw.

Save stages Apply/Discard/Cancel on a candidate. Live document/draft changes
only after success. Failed/cancelled Save, including Save during New/Open/Close,
preserves live content, selection and draft. Committed failures mark a previously
clean document dirty before displaying the error, identify the attempted target
and retained recovery file, and block retries/probing of that uncertain target.
Explicitly inspect/reopen or save elsewhere. Failed Save As retains original path.

Preserve: exact number spelling/high precision/exponents; object/array order;
strict UTF-8/Unicode/decoded duplicate-key rejection; object root; depth512;
250000 core parse nodes; GUI50000; final UTF-8 16MiB including BOM/newline;
Raw4MiB; inspector1048576/key65535 UTF-16 units; original default C++ model.

## Atomic saving, metadata and boundaries

Only user-owned single-link regular local files with supported plain metadata.
Same-directory exclusive temp, full writes/EINTR/short writes, file/directory
fsync, identity/SHA-256 baseline, renameat2 EXCHANGE/NOREPLACE, installed and
actual replaced-version verification. No direct overwrite fallback.
Mode/group retained; symlink ancestors, hard links, special modes, nonowned
files and every xattr/ACL explicitly refused.

Metadata presence is inside stable fstat/ctime reads. Prepared temp, installed
target, actual replaced old and final snapshots all enforce the policy.
Presence is sufficient because no attributes are supported; no copying/ACL
support is claimed. Real additions/changes/default ACLs, postcommit edits and
synchronized separate-process metadata writers exercise actual syscalls.
Affected metadata is retained in old recovery or new target on committed errors.
These schedules do not prove all races, including changes after last observation.

Every successful replacement retains .jsondict-backup-* too. Do not unlink the
only old copy before a final fallible fsync. No automatic pruning or assumed
rollback. Inspect/copy both versions deliberately; recovery may be unverified.

Managed-cloud overlay fsync=volatile tests do not prove power-loss durability.
UID/GID maps may contain only 0; ACL fixtures use a mapped UID. Root policy
rejection is not non-root kernel-permission acceptance.
Unaccepted: real GNOME X11/Wayland/native dialogs/portal/IME/clipboard/focus/DPI/
accessibility/desktop glyphs; ext4/xfs/btrfs/removable/network/FUSE; power loss,
all crashes/process schedules/parent-directory moves; non-root EACCES/EROFS.
Offscreen/Xvfb screenshots and widget clicks are automation only.

## Delivery and packaging

First preserve/read back exact source commit+full bundle, complete snapshot,
runnable binary, complete logs/environment/hashes/report. Then check real desktop
availability and produce Ubuntu24.04 amd64 .deb with separately recorded install/
uninstall/smoke. Qt stays dynamic, original icon/MIT retained.
No main/tag/release/other-platform writes. AppImage deferred; its unexecuted
pins are not packaging or bundled-library license/source compliance evidence.
