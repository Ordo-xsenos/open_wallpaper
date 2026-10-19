# M3 Manager, IPC and persistence acceptance

M3 began with explicit user authorization on 2026-10-18. The user confirmed the
specification and direct code implementation. M1/M2 Windows acceptance remains
pending; M3 does not convert those results into PASS.

## Build and deploy on Windows

Install shared Qt 6.5+ for MSVC x64, including Qt Declarative/Quick Controls and
Qt Quick Dialogs. Prefer a Qt kit matching the installed MSVC toolset. Qt is
provided independently of the static vcpkg Core dependencies; Core has no Qt links.
From x64 Developer PowerShell, substitute your actual Qt installation:

```powershell
$qt = 'C:/Qt/<version>/msvc2022_64'
.\tools\Bootstrap-Windows.ps1
cmake --preset win-manager-debug -DCMAKE_PREFIX_PATH="$qt"
cmake --build --preset win-manager-debug
ctest --preset win-manager-debug --output-on-failure
.\tools\Deploy-Manager.ps1 -Preset win-msvc-debug -QtRoot $qt
.\out\deploy\win-msvc-debug\AetherWall.Manager.exe --core-window
```

With Core stopped, `--core-window` makes Manager automatically launch diagnostic windows.
Verify those before testing experimental desktop integration. For Release repeat
with `win-manager-release`, then deploy with `-Preset win-msvc-release`.
Normal Manager first probes the existing Core, then starts it automatically once
if unavailable. `--no-auto-core` disables automatic startup for development. Core
starts as `--service`: unlimited desktop run, persisted settings/assignments,
background console detachment and native tray. For isolated data, launch Core
manually with `--service --mode window --all-monitors --data-dir PATH`.
Traditional finite CLI diagnostics continue without `--service`.

## Required Windows cases (all NOT TESTED here)

| Case | Steps | Required evidence |
| --- | --- | --- |
| Dynamic deployment | Launch staged Manager on a Windows test machine without a Qt developer PATH | DLLs/QML plugins found, no static Qt embedded; retain dependency notices |
| Process separation | Apply an example; close Manager; reopen from Core tray | Scene continues; Core has no Qt DLLs; reopening shows current state |
| Complete exit | Choose Exit completely in Manager or tray | Both applications close, owned surfaces/GPU/pipe/tray released |
| Library | Search title/author, favorites filter, title/favorite sort, details/open folder | Correct actual metadata and static previews; keyboard focus visible |
| Import | Import a valid directory, ZIP and a path with spaces/Cyrillic; corrupt versions/paths/images/ZIP bombs | Valid managed copy; originals unchanged; bounded rejection and actionable response |
| Duplicate/delete | Import same UUID twice; delete inactive copy; attempt delete active/disconnected assignment | No overwrite; active/assigned deletion rejected; no external directory deletion |
| Per-monitor apply | Apply different scenes, then All monitors; force staging rejection on a test device | Successful confirmation only after commit; failed staging preserves old assignments/scenes |
| Persistent state | Favorite, assign, change FPS/GPU/pause/language; restart Core/Manager | SQLite state and JSON settings restored; scenes remain independent |
| Settings recovery | With Core stopped, truncate settings.json after creating a backup; restart | Valid backup/defaults loaded with warning; damaged file retained, backup not overwritten by damaged data |
| Autostart | Default off; enable and inspect HKCU Run; disable and sign in again | Current-user entry only after explicit action; no admin requirement; disabled entry removed |
| Pause/tray | Per-output and all-output pause/resume, add/remove a display, restart Explorer | Manual blocker retained, automatic blockers respected, tray restored |
| Reconnection | Restart/terminate Core while Manager is open; restart Core | UI shows disconnected/read-only state and reconnects without replaying mutations |
| ACL/session | Connect as another user/logon, from remote host, and to a spoofed server in another session | DACL/remote rejection and Manager peer checks; no command reaches runtime |
| Framing | Split header/payload, malformed JSON/version/UTF-8, duplicate keys, oversized lengths, stalled peers | Bounded reject/timeout; later valid connection works, Core stays responsive |
| Accessibility/DPI | Tab/Shift+Tab, Enter/Space, screen reader, 100/150/200% scale, 900×620 window | Named controls, focus, readable text, scrollable long settings, no overlapping controls |

Record OS build/driver/GPU/displays, source and binary hashes, exact Qt version,
logs, screenshots and PASS/FAIL/NOT TESTED for each row. Re-run M0–M2 test plans.
Profile import pauses and verify observed pipe timeouts rather than infer timing
from source. Autostart and deployment commands were not run in this Linux session.

## Local Linux checks

```sh
cmake --preset manager-dev -DAETHERWALL_JSON_INCLUDE_DIR=/usr/include/cudnn_frontend/thirdparty
cmake --build --preset manager-dev
ctest --preset manager-dev --output-on-failure
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  out/build/manager-dev/apps/manager/AetherWall.Manager --capture /tmp/manager.png
```

The include override is specific to this host; normal installations use the distro
nlohmann-json package. Qt Manager is compilable on Linux for UI verification; Core
is Windows-only. BUILD_TESTING Linux builds include `--test-library` for an explicitly
labelled headless test transport using real bundled imports and storage; it does
not render wallpapers or supply synthetic performance measurements.

The Qt test verifies Manager commands against ControlService and SQLite through
an injected transport, and has a separate local-socket case. This sandbox rejects
Unix socket creation (`Operation not permitted`), so that case is explicitly
SKIPPED here; Windows Named Pipe remains NOT TESTED. Captured UI screenshots are
headless Linux/test-transport evidence, not Windows integration screenshots.

## Scope still pending

This is a working M3 vertical slice, not release acceptance. Data-root/cache
migration controls, library pagination beyond the 128-entry preview cap, background
import jobs and Windows production validation remain follow-up tasks. Discover
and online downloads belong to M4. Installer/uninstaller, full redistribution
license audit and hardware release matrix belong to M5.
