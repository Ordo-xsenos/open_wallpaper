# AetherWall

Experimental Windows 11 x64 live wallpaper engine, implemented from
`WINDOWS11_WALLPAPER_ENGINE_SPEC.md`. M0–M2 implement desktop integration,
GPU scenes and multi-monitor scheduling. **M4 adds catalog discovery and verified
package downloads** to the Qt Manager, plus a FastAPI catalog with development
SQLite/local storage and PostgreSQL/S3 adapters. Core remains independent of Qt
and networking. M0 execution was user-reported; M1–M4 Windows build/runtime
acceptance remains pending. Linux verification cannot establish desktop
compatibility. See [implementation status](docs/IMPLEMENTATION_STATUS.md).
**M5 preview packaging is now implemented as source**: per-user Inno Setup,
portable ZIP, payload hashes/dependency checks and internal Windows CI. Windows
installer compilation, installation and integrated/discrete GPU acceptance remain
NOT TESTED locally. See [release build](docs/RELEASE.md),
[preview notes](docs/PREVIEW_NOTES.md) and [M5 acceptance](docs/M5_TEST_PLAN.md).
The current source snapshot is 0.5.0-preview.1; it is not an accepted public release.

![Manager catalog — Linux offscreen capture, injected test transport without GPU](docs/screenshots/m5-discover-linux.png)

This is an actual development Manager capture, not Windows compatibility evidence.

## M4 catalog

Open **Catalog → Examples** for six original embedded scenes, or launch Manager
with `--mock-catalog`. Search, category filters, sorting and pagination use the
same client as the configured API. Choose a scene and download it; Manager verifies
size/SHA-256/manifest before Core repeats verification and atomically imports it.
Installation preserves active wallpaper; apply the scene from Library. Existing
bundled scenes show **Installed**; delete an unassigned example from Library to
exercise downloading it again. Cancellation, retries and offline errors are visible.

No public server URL is built in. Settings accept an HTTPS catalog origin and
optional explicitly trusted HTTPS asset origins. Local HTTP requires an explicit
loopback-only development option. See [server setup](server/README.md) for FastAPI,
PostgreSQL/S3, moderation and rights proof; [M4 checks](docs/M4_TEST_PLAN.md) covers
verification and deployment limitations. Hosting and spending are unconfigured.

## M3 Manager: build and run

Core builds still do not need Qt. To include Manager, install a **shared Qt 6.5+
MSVC x64 kit**, then use x64 Developer PowerShell and your actual Qt root:

```powershell
$qt = 'C:/Qt/<version>/msvc2022_64'
.\tools\Bootstrap-Windows.ps1
cmake --preset win-manager-debug -DCMAKE_PREFIX_PATH="$qt"
cmake --build --preset win-manager-debug
ctest --preset win-manager-debug --output-on-failure
.\tools\Deploy-Manager.ps1 -Preset win-msvc-debug -QtRoot $qt
.\out\deploy\win-msvc-debug\AetherWall.Manager.exe --core-window
```

Click **Start engine** to open diagnostic surfaces first. The `--core-window`
Manager flag controls that launch; a normal Manager start requests experimental
desktop mode. Manager does not automatically replace a running Core. Closing
Manager leaves Core and tray active; **Exit completely** stops both.

Manager contains Library, Catalog, Monitors, Performance and Settings, static previews,
search/favorite filters, sorting, directory/.awscene import, per-output or all-output
apply, pause, FPS/GPU policies, Russian/English and opt-in current-user autostart.
Local data defaults to `%LOCALAPPDATA%\AetherWall`: `library.sqlite3`, managed
`library/<scene UUID>/`, `settings.json` and `settings.json.bak`; logs are separate.
Source scene folders are copied, never moved or deleted. Invalid GPU staging
preserves the active scenes. The initial local library cap is 128 entries and
2048×2048 preview images. Catalog cache is bounded to 256 MiB.

`Core --service` restores persisted settings/assignments and runs indefinitely;
desktop service mode detaches the diagnostic console. Traditional finite diagnostic
commands below remain available. `--data-dir PATH` isolates library/settings for
testing. Stored settings govern service mode; omit `--service` for CLI FPS/policy
experiments. GUI data-root/cache migration controls remain follow-up work.

Read [M3 Windows tests](docs/M3_TEST_PLAN.md), [IPC contract](docs/IPC.md), and
[Qt redistribution notes](THIRD_PARTY_NOTICES.md). Windows deployment scripts and
Core's new pipe/tray path have not been run in this Linux environment.

## Windows prerequisites

- Windows 11 24H2+ x64 (build 26100+) with an interactive desktop for desktop mode.
- Visual Studio 2022 Build Tools: **Desktop development with C++**, x64 MSVC and
  Windows SDK, including `fxc.exe` (HLSL Shader Model 5.0).
- CMake 3.28+, Ninja, Git, and internet for initial dependency setup.
- Use **Developer PowerShell for VS 2022 configured for x64**. From a regular
  Developer PowerShell, enter the x64 environment if needed:

  ```powershell
  Enter-VsDevShell -VsInstallPath $env:VSINSTALLDIR -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
  ```

Core uses nlohmann/json, libarchive/zlib and libwebp, installed by vcpkg.
PNG/JPEG decoding uses Windows WIC. GoogleTest is a test dependency. The bootstrap
script checks out vcpkg at the manifest baseline, upgrading an existing clean
project-local checkout when needed. No Qt installation is needed for Core-only builds. Scripts run with ordinary
user rights; they do not change system execution policy or security settings.

## Build and test

Run from the repository root. The bootstrap sets `VCPKG_ROOT` for the current
PowerShell process; run it again when opening a new terminal.

```powershell
.\tools\Bootstrap-Windows.ps1
cmake --preset win-msvc-debug
cmake --build --preset win-msvc-debug
ctest --preset win-msvc-debug --output-on-failure

cmake --preset win-msvc-release
cmake --build --preset win-msvc-release
ctest --preset win-msvc-release --output-on-failure
```

The Windows presets enforce x64 MSVC. The build compiles trusted HLSL into
embedded bytecode headers; there is no runtime shader compiler or downloaded code.
If `fxc.exe` is missing, install the Windows SDK or set `FXC_EXECUTABLE` at configure time.
An absent D3D debug layer is logged and retried without the layer; hardware device
creation failures are not silently switched to software rendering.

## Run M1 scenes

From the repository root, after building:

```powershell
$exe = '.\out\build\win-msvc-debug\AetherWall.Core.exe'
& $exe --mode window --scene scenes/bundled/stars --duration 120
& $exe --mode window --scene scenes/bundled/mountains --duration 120
& $exe --mode window --scene scenes/bundled/still --duration 30
.\out\build\win-msvc-debug\AetherWall.SceneCheck.exe scenes/bundled/stars
```

Other examples: `rain`, `gradient`, and `minimal_particles`. Press **F5** in the
diagnostic window to reload `--scene`; a broken package/image keeps the last
working scene. At startup, a rejected scene retains the built-in gradient and
logs an error. Check the `Scene committed` entry before counting a scene as loaded.
Use `--mode desktop --scene scenes/bundled/stars` after validating window mode.
Desktop mode does not capture keyboard input.

For a static scene, `totalDrawPresents` should stop increasing after initial
redraws. A Shell/stop watchdog continues at 250 ms without normal Present calls.
See [scene format and ZIP packaging](docs/SCENE_FORMAT.md),
[examples](scenes/README.md), and [M1 Windows tests](docs/M1_TEST_PLAN.md).

## M2: multiple monitors and performance policies

```powershell
& $exe --list-monitors
& $exe --mode window --all-monitors --preset balanced --duration 120 `
    --monitor-scene DISPLAY1 scenes/bundled/stars `
    --monitor-scene DISPLAY2 scenes/bundled/rain
```

Use aliases actually printed on your machine. For assignments across reconnection,
prefer the quoted persistent device paths shown beside those aliases. Desktop mode
uses all detected monitors; diagnostic mode uses only the primary unless
`--all-monitors` is given. `--scene` supplies the fallback for unassigned outputs.
Mirrored displays are one logical output. Assignments persist during this run's
recovery; settings persistence and GUI/tray controls belong to M3.

Press **Space** in a diagnostic window to pause/resume that output; F5 reloads its
assigned scene. Defaults pause on fullscreen, battery, lock, suspend and console
display-off; maximized-window pause is optional. Fullscreen checks use foreground
bounds per monitor and exclude the Shell and Core's own windows. Independent
panel sleep is not reliably detected; console power state and DXGI occlusion are
handled, with this limit recorded in the acceptance plan.

`--gpu auto` favors a power-saving adapter that drives the monitor, avoiding an
unnecessary cross-GPU path when possible. Explicit GPU preference may increase
copying costs. Compatible outputs share a device/context and immutable textures;
scene uploads are staged with a 256 MiB/device texture budget including live scenes.
Per-output frame caps account for refresh rate and retain VSync by default.

See [M2 Windows tests](docs/M2_TEST_PLAN.md) and the
[performance report/protocol](docs/PERFORMANCE.md). No Windows performance numbers
or compatibility PASS results are claimed yet.

## Run the original diagnostic gradient

First verify the same renderer in an ordinary diagnostic window:

```powershell
.\out\build\win-msvc-debug\AetherWall.Core.exe --mode window --duration 60
```

After the gradient is visible and the window closes cleanly, test desktop mode:

```powershell
.\out\build\win-msvc-debug\AetherWall.Core.exe --mode desktop --duration 60
```

The default is **window mode, 30 FPS, VSync, 60 seconds**. Close the diagnostic
window or press Ctrl+C in its console to stop. Desktop mode deliberately retains
the console for diagnostics; its wallpaper surface has no caption or normal
Alt+Tab entry. Duration `0` explicitly requests an unlimited run. Only one Core
instance may run per Windows session.

```text
--mode window|desktop       Explicit diagnostic/experimental desktop mode
--fps 15|30|60              CPU-side frame cap; VSync can lower the observed rate
--duration 0..86400         Seconds; default 60, 0 unlimited
--swap-chain auto|flip|blt  Auto tries flip, then blt on unsupported/invalid creation
--no-vsync                 Compare presentation without VSync (not an uncapped loop)
--warp                     Software diagnostic rendering; window mode only
--log-dir PATH             Override the default diagnostic directory
--scene PATH               Scene directory or .awscene ZIP; omitted = diagnostic gradient
--all-monitors             One diagnostic window per logical monitor
--list-monitors            Print aliases/device paths and exit without creating windows
--monitor-scene ID PATH    Repeatable per-output scene assignment
--preset eco|balanced|smooth  15/30/60 FPS; choose this or --fps
--gpu auto|power-saving|high-performance  Adapter selection preference
--pause-fullscreen on|off  Default on, per monitor
--pause-maximized on|off   Default off, per intersected monitor
--pause-battery on|off     Default on
--help                     Print usage without creating a window or log
```

Exit codes: `0` normal stop, `1` initialization/rendering failure or safe desktop
refusal, `2` invalid arguments. A successful exit does **not** prove behind-icon
placement. There is no automatic fallback to a visible standalone wallpaper window
or modification of the user's static wallpaper.

Logs are UTF-8 in `%LOCALAPPDATA%\AetherWall\logs\m0_*.log` by default. Each log is
capped at approximately 4 MiB; up to ten M0 logs are retained. They include OS build,
adapter/driver, Shell topology, HWND/DPI diagnostics, chosen swap effect, recovery
attempts, per-output present rates, submission avg/P95, asynchronous GPU draw timing
when available, CPU time and process memory. Metric definitions and limitations are
in [PERFORMANCE.md](docs/PERFORMANCE.md); GPU utilization is not inferred. Logs can be shared for compatibility testing.

## Record a Windows verification run

```powershell
.\tools\Test-M0.ps1 -Mode window
.\tools\Test-M0.ps1 -Mode desktop -Duration 300
.\tools\Test-M0.ps1 -Mode desktop -SwapChain flip
.\tools\Test-M0.ps1 -Mode desktop -SwapChain blt
.\tools\Test-M0.ps1 -Mode window -Scene scenes/bundled/still -Duration 30
.\tools\Test-M2.ps1 -Mode window -PerformancePreset balanced -Duration 120
```

The script runs CTest, records machine information, binary/source hashes and logs
under `out/reports/`, and creates a manual result form. It never restarts Explorer
automatically or marks visual checks as passed. Complete the form using
[the test plan](docs/TEST_PLAN.md) and retain screenshots where useful.

## Linux policy tests

Linux can test scene packages, metadata/paths, geometry, redraw demand, options,
frame deadlines, multi-monitor assignment/geometry, pause and recovery policies. It
cannot compile this MSVC Core or validate Direct3D/Explorer integration. Install
GoogleTest, nlohmann-json, libarchive, zlib and libwebp development packages, then run:

```sh
cmake --preset portable-tests
cmake --build --preset portable-tests
ctest --preset portable-tests --output-on-failure
```


To build the Qt Manager on Linux for UI/control verification, install shared Qt
6 Core/Gui/Network/Quick/QuickControls/Test development packages and use
`cmake --preset manager-dev`, `cmake --build --preset manager-dev`, then
`ctest --preset manager-dev`. The actual engine stays Windows-only. See the
M3 test plan for headless capture and explicitly labelled test-transport options.

## Compatibility and architecture

The host only accepts an existing empty top-level WorkerW behind a visible icon
host belonging to the same Shell process. Unknown/nested topologies are refused.
M2 creates one surface per logical monitor. Recovery stops after three consecutive
failures or six attempts within thirty seconds; successful long-lived sessions can
recover repeatedly. Recovery restores the last committed CPU scene snapshots.
See [desktop integration](docs/DESKTOP_INTEGRATION.md) for known risks and evidence.

Core is separate from the Qt Quick/QML Manager. Core owns persistent
state and IPC; the FastAPI catalog can run locally. Development
may proceed on M2 sources with user authorization; M1/M2 Windows acceptance and
M0 regressions must still be recorded before claiming compatibility.

## Share sources

Run `python tools/Package-Source.py` to rebuild `out/share/AetherWall-M5.zip`
and its SHA-256 file. The explicit allowlist includes sources, bundled artwork,
docs and source hashes; it excludes build outputs, local data and repository
credentials. The recipient installs the Windows/Qt prerequisites and follows the
Manager/catalog/release build instructions. This ZIP contains source code, not a ready-made installer.

## License

Project code is MIT licensed. See [third-party notices](THIRD_PARTY_NOTICES.md).
