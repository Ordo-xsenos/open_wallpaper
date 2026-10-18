# Build and publish a Windows preview

Windows preview artifacts are built in GitHub Actions. A user-reported successful
Windows CI run produced the installer; installation/hardware acceptance is separate
from compilation. See the repository Releases page for automatically published builds.

## Inputs

Use VS 2022 x64 Developer PowerShell, Windows SDK/fxc, CMake 3.28+, Ninja,
Python 3.11+, Inno Setup 6.3+ and a shared Qt **6.8.3 msvc2022_64** kit.
The development Manager may use newer Qt; the release payload is deliberately
restricted to one Qt version and matching notices. Keep enough space for Qt sources.
The unified workflow `AetherWall CI` prepares these inputs on Windows Server 2022;
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
The release script compiles Release once, runs CTest, stages Qt/QML/SChannel and
official app-local CRT, then exercises staged Core --help, every SceneCheck example,
and Manager/QML startup with a clean PATH. It seals the payload with hashes,
verifies again and invokes ISCC. Set `-Iscc PATH` for a nonstandard compiler installation.

Expected outputs in `out/release/`:

- `AetherWall-0.5.0-preview.1-win-x64-setup.exe` and SHA-256 sidecar.
- `AetherWall-0.5.0-preview.1-win-x64.zip` and SHA-256 sidecar.
- `out/share/AetherWall-M5.zip`: application source companion.

The stage remains under `out/release-stage/<id>/` for inspection. A failed build
does not convert old artifacts into a successful release; inspect command exit
status and hashes before using any output. PowerShell/installer compilation runs on the Windows CI host; Linux checks do
not replace installation and hardware tests on Windows 11.

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
retain companion source materials and decide code signing before a stable release.
Preview publication is automatic as described below. No signing certificate or
catalog hosting is configured.

## Development screenshots

These are actual Linux offscreen Qt captures with an explicitly labelled injected
test transport and no GPU. They show the current UI, not Windows runtime results:
[Discover](screenshots/m5-discover-linux.png) and
[Settings/version](screenshots/m5-settings-linux.png).

## Automatic GitHub Releases

A push to `main` or a manual `release-preview.yml` run on `main` builds/tests the
preview and publishes a **Pre-release** after Linux tests, Windows Core Debug and Windows Manager Release succeed.
Pull requests are built/tested but never published. Only the publication job has
`contents: write`; it uses the repository's built-in `GITHUB_TOKEN`, not a personal
access token. Repository/organization policy must permit this permission.

Each run uses a new tag `v<VERSION>-build.<RUN_ID>.<ATTEMPT>` targeting the exact
built commit. Existing releases/assets are never overwritten and previews are not
marked as the stable Latest release. Use the repository's `/releases` page rather
than `/releases/latest` to find previews. Before publishing, the script checks the
complete installer/portable/source/Qt asset set, ZIP integrity and binary/source
sidecar hashes, then generates `SHA256SUMS.txt` for every attached file.

Creation/uploads occur as a draft. Only after upload succeeds does the job publish
it. A failed upload may leave a draft visible to maintainers; it does not expose
an incomplete public release. Rerunning the workflow uses a distinct run attempt.
The release URL is written into the publish job's Summary. Build logs remain in
Actions; download assets and matching Qt/application source companions are stored
in Releases without the 14-day Actions artifact retention period.

To publish after pushing the updated workflow, either wait for its automatic run
or explicitly run `gh workflow run release-preview.yml --ref main`. Older successful
runs do not gain the new publication job retrospectively. The binary files retain
`packaging/release.json`'s application version; the tag identifies the particular
build of that version.

## Unified CI and Qt test dependencies

Only `release-preview.yml` is triggered for main pushes, main-targeting PRs and
manual runs. It owns Linux policy/Python checks, Core Debug, Manager Release,
packaging and publication; the former `windows-build.yml` is removed. This avoids
building Core Release twice for each push. Publication depends on every required
job and still runs only for main push/manual events. No PR token gets write access.

CTest's Windows catalog test prepends the linked Qt Core DLL directory to PATH
using `ENVIRONMENT_MODIFICATION`; this applies before deployment, including local
CTest runs. Failure diagnostics include dumpbin dependencies at
`out/reports/release-dependencies.txt`. Staged runtime checks use only Windows
system directories on PATH; their logs and software-rendered QML startup capture
are retained at `out/reports/release-smoke/`. They do not prove desktop/D3D behavior.

Repository description and private reporting setup are documented in
[COMMUNITY_SETUP.md](COMMUNITY_SETUP.md). If branch protection required checks from
the old workflow, replace those requirements with the actual Linux, Debug and
Release job checks from AetherWall CI after the first run; do not disable protection.
