# M5 preview acceptance

## Actual local results (2026-10-18)

Linux, GCC 16.2.1, shared Qt 6.12. No MSVC/Windows SDK, PowerShell or ISCC here.

| Check | Result | Evidence |
| --- | --- | --- |
| Manager/control build | PASS | `cmake --build --preset manager-dev` |
| CTest | PASS | 70 entries; Unix socket subcase skips under sandbox restrictions |
| Packaging policy | PASS | 24 tests: seal/ZIP/hash, tampering/extra files, missing notices/plugins/runtime, debug/private files, i386/link rejection, wrong Qt and false acceptance metadata; Qt attribution controls/BOM, malformed metadata diagnostics and license path bounds |
| Server regression | PASS | 12 real in-process ASGI/SQLite tests |
| Screenshots | PASS | Actual offscreen Linux Manager; explicitly labelled test transport, no GPU |
| PowerShell, Windows PE resources, ISCC | NOT TESTED | Prepared source; needs Windows CI/local build |
| Installer/upgrade/uninstaller | NOT TESTED | Run the matrix below on Windows 11 |
| Integrated/discrete GPU acceptance | NOT TESTED | No Windows GPU session here |
| Exact redistributed license/source audit | NOT TESTED | Collected materials are inputs, not a completed audit |

Raw local logs: `out/verification/m5-*`. `pytest tests/release` uses explicitly
synthetic PE headers to exercise packaging policy, not executable compatibility.
Run `python -m pytest tests/release server/tests -q` and the normal CMake/CTest
presets to repeat checks. Windows Release CI additionally includes the catalog Qt
suite without the Unix socket transport harness. Hosted CI has not been executed
from this workspace.

## Record identity before testing

Record OS edition/build, standard-user account, GPU(s), driver(s), display/DPI/
refresh layout, source commit or source-ZIP SHA, artifact SHA, Qt/MSVC versions,
installed path, commands and attached logs/screenshots. Use a clean Windows 11
24H2+ x64 VM for installation tests, then physical integrated and discrete GPU PCs
for desktop/recovery/performance. Keep a backup of test data before schema upgrades.

## Installation matrix

1. Install without elevation on a machine with no Qt/VS or VC runtime on PATH.
   Check per-user Programs path, optional desktop shortcut, RU/EN installer, start
   menu, independent app-local CRT/Qt/QML and SChannel HTTPS. No HKLM/service/task
   or default Run entry may be created. Custom `/DIR` must be rejected.
2. Run Core window mode before desktop. Exit completely, install the same candidate
   again and upgrade an older preview. Favorites/settings/assignments/imported
   assets must survive. Exit older M4 processes manually: those lack running markers.
3. While current Core or Manager runs, try install and uninstall. AppMutex must
   require complete exit; no process should be killed. Closing Manager alone leaves
   Core running and must still block. Check processes from another session/user
   cannot overwrite this user's installation or control its Core.
4. Enable autostart, exit completely, uninstall. The matching quoted installed Core
   Run value must disappear. Repeat with the value pointing to another executable:
   it must remain untouched. Verify registry errors are recorded in uninstall logs.
5. Hash the library/settings/imported source files before uninstall. They must be
   unchanged afterward; no purge prompt or recursive data deletion is permitted.
   Reinstall and verify restore, including the previously chosen autostart setting.
6. Repeat silent install/uninstall using `/VERYSILENT /SUPPRESSMSGBOXES /NORESTART
   /LOG=...`; verify exit codes, files-in-use failure behavior and data preservation.
   Test Unicode/spaces in profile paths and filesystem permissions/disk exhaustion.
7. Extract portable ZIP into a fresh directory and run without SDK paths. Portable
   programs still use the standard per-user data folder; this is not a portable
   user-data mode. Verify file hashes against `release-manifest.json`.

## Full specification acceptance

| Criteria | Reproducible test | Actual Windows result |
| --- | --- | --- |
| AC-01 | Standard-user install/first run; steps above | NOT TESTED |
| AC-02–04 | Desktop icons/input, Alt+Tab and Explorer recovery in TEST_PLAN.md | NOT TESTED |
| AC-05–07 | Static Present count, shader scenes and FPS in M1_TEST_PLAN.md/PERFORMANCE.md | NOT TESTED |
| AC-08–09 | Fullscreen/lock, mixed DPI/negative coordinates and recovery in M2_TEST_PLAN.md | NOT TESTED |
| AC-10 | Close Manager and confirm Core continues, M3_TEST_PLAN.md | NOT TESTED |
| AC-11–13 | Offline, corrupt scene, hash/cancel/retry, M4_TEST_PLAN.md | NOT TESTED |
| AC-14 | Device removal/recovery, M2_TEST_PLAN.md | NOT TESTED |
| AC-15 | Reinstall/data retention and autostart ownership, steps 2–6 | NOT TESTED |
| AC-16 | Clean MSVC Release + CTest + ISCC + dependency-free first run | NOT TESTED |

Replace results only with observed evidence. Public preview requires all applicable
criteria, real integrated/discrete GPU performance reports, completed redistribution
inventory and permanent companion sources. No public release was published.

## Windows CI follow-up

The first user-run Windows preview reached Qt notice collection and failed with
`JSONDecodeError: Invalid control character`. The collector now accepts literal
control characters inside strings of the checked Qt attribution sources and UTF-8
BOM, preserves original metadata/license bytes, and names malformed files in errors.
The 24 packaging tests and 12 server tests pass locally after the fix. The Windows
workflow must be rerun on the updated commit; full build/install acceptance remains
pending until that run and the manual matrix complete.
