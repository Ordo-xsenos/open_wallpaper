# Build an internal Windows preview

M5 packaging is implemented as source; Windows build/install/hardware acceptance
has not been run in this Linux workspace. This is not a ready-made installer.

## Inputs

Use VS 2022 x64 Developer PowerShell, Windows SDK/fxc, CMake 3.28+, Ninja,
Python 3.11+, Inno Setup 6.3+ and a shared Qt **6.8.3 msvc2022_64** kit.
The development Manager may use newer Qt; the release payload is deliberately
restricted to one Qt version and matching notices. Keep enough space for Qt sources.
The workflow `M5 internal preview` prepares these inputs on Windows Server 2022;
its results do not establish Windows 11 desktop compatibility.

## Build

From the repository root, prepare pristine source checkouts and notices:

```powershell
foreach ($module in @('qtbase','qtdeclarative','qtsvg','qtshadertools','qttranslations')) {
    git clone --depth 1 --branch v6.8.3 "https://code.qt.io/qt/$module.git" "out/qt-sources/$module"
    if ($LASTEXITCODE -ne 0) { throw 'Qt source checkout failed' }
}
python tools/Collect-QtNotices.py --sources out/qt-sources --output out/qt-notices
$qt = 'C:/Qt/6.8.3/msvc2022_64'
.\tools\Bootstrap-Windows.ps1
cmake --preset win-manager-release -DCMAKE_PREFIX_PATH="$qt"
.\tools\Build-Release.ps1 -QtRoot $qt -QtNoticesRoot "$PWD/out/qt-notices"
python tools/Package-Source.py
```

For Git checkouts pass `-Revision (git rev-parse HEAD)`; source ZIPs use the explicit
`unversioned-source` marker. Use a fresh notice output directory. Deployment also
requires an empty staging directory; release builds use a unique stage automatically.
The release script compiles Release, runs CTest, stages Qt/QML/SChannel and official
app-local CRT, checks all bundled scenes, seals the payload with hashes, verifies
again and invokes ISCC. Set `-Iscc PATH` for a nonstandard compiler installation.

Expected outputs in `out/release/`:

- `AetherWall-0.5.0-preview.1-win-x64-setup.exe` and SHA-256 sidecar.
- `AetherWall-0.5.0-preview.1-win-x64.zip` and SHA-256 sidecar.
- `out/share/AetherWall-M5.zip`: application source companion.

The stage remains under `out/release-stage/<id>/` for inspection. A failed build
does not convert old artifacts into a successful release; inspect command exit
status and hashes before using any output. Installer compilation and PowerShell
scripts remain NOT TESTED locally until this procedure runs on Windows.

## Installation and data

Fixed per-user target: `%LOCALAPPDATA%\Programs\AetherWall`. The installer rejects
custom `/DIR` overrides to keep executables separate from managed data. Start menu
shortcut is installed; desktop shortcut and first launch are optional. Autostart
remains an explicit application preference. No HKLM, service, scheduled task,
driver or antivirus changes are made. x64 native Windows build 26100+ is required.

Exit completely before install/upgrade/uninstall. New binaries maintain per-session
running markers checked by Inno's AppMutex; an older M4 build has no marker and
must be closed manually. Files-in-use errors must be resolved before continuing.
No process is force-killed. Stable AppId supports in-place upgrades in this preview
channel. Downgrading settings/database schemas is unsupported: back up data first.

Uninstall retains library/settings/logs and arbitrary user-added files. Program
files installed by Inno are removed; this installation's quoted Core autostart
command is removed, while another executable's value is left intact. There is no
purge-data option. For a fresh profile, back up/rename the data directory manually.

## Release gates

Run M5_TEST_PLAN.md on a standard-user Windows 11 machine with no Qt/VS on PATH.
Complete AC-01–AC-16 and integrated/discrete GPU evidence. Audit deployed notices,
ship permanent companion source materials, and decide code signing before a public
release. CI only uploads internal artifacts and cannot publish a GitHub Release.
No certificate, signing account, hosting or public release is configured.

## Development screenshots

These are actual Linux offscreen Qt captures with an explicitly labelled injected
test transport and no GPU. They show the current UI, not Windows runtime results:
[Discover](screenshots/m5-discover-linux.png) and
[Settings/version](screenshots/m5-settings-linux.png).
