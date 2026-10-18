**English** · [Русский](README.ru.md) · [O‘zbek](README.uz.md)

# AetherWall

**Live wallpapers for Windows 11, with GPU scenes, multiple monitors, and control over animation.**

AetherWall renders scenes with Direct3D 11 and HLSL behind your desktop icons.
Choose a wallpaper, assign different scenes to your monitors, and decide when
animations should pause. Closing Manager leaves the wallpapers running;
the independent Core stays in the system tray.

[![CI](https://github.com/Ordo-xsenos/open_wallpaper/actions/workflows/release-preview.yml/badge.svg)](https://github.com/Ordo-xsenos/open_wallpaper/actions/workflows/release-preview.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
![Windows 11 x64](https://img.shields.io/badge/Windows-11%20x64-0078D4)
![Preview](https://img.shields.io/badge/Status-Preview-orange)

[Download for Windows](https://github.com/Ordo-xsenos/open_wallpaper/releases) ·
[CI builds](https://github.com/Ordo-xsenos/open_wallpaper/actions/workflows/release-preview.yml) ·
[Report a problem](https://github.com/Ordo-xsenos/open_wallpaper/issues/new/choose)

![AetherWall scene catalog](docs/screenshots/m5-discover-linux.png)

*Actual Manager screenshot from Linux development. The window labels its test
connection; this image does not demonstrate Windows desktop rendering.*

## Features

- **GPU scenes.** Image layers, gradients, motion, and particles: stars, rain,
  and snow. Six original examples ship with the app.
- **Multiple monitors.** One scene everywhere or different scenes on each display,
  accounting for layout and DPI.
- **Animation controls.** 15/30/60 FPS limits, GPU selection, and pause rules for
  fullscreen applications, battery use, and locked sessions.
- **Local library.** Import scene folders and `.awscene` packages, search,
  save favorites, and restore monitor assignments between sessions.
- **Catalog.** Search, categories, cancellable downloads, and SHA-256 validation.
  Bundled examples and installed wallpapers work offline.
- **Russian and English UI.** Dark Qt Manager, tray controls, and optional
  autostart when you sign in to Windows.

## Download and install

Requires **Windows 11 24H2 or later (build 26100+), x64**, and a Direct3D 11 GPU.
Prebuilt downloads do not require Visual Studio, Qt, or Python.

Open [Releases](https://github.com/Ordo-xsenos/open_wallpaper/releases), choose
an available preview, and expand **Assets**:

| Download | Purpose |
| --- | --- |
| `AetherWall-…-win-x64-setup.exe` | Installer — the usual way to install |
| `AetherWall-…-win-x64.zip` | Run without installing; extract the entire ZIP |
| `SHA256SUMS.txt` and `*.sha256` | Checksums for the downloads |
| `AetherWall-M5.zip` | Project source code, not the application |
| `qt…-source.zip` | Companion Qt source materials |

The installer needs no administrator rights and uses
`%LOCALAPPDATA%\Programs\AetherWall`. Enable autostart separately in Settings.
For the ZIP version, run `AetherWall.Manager.exe` and keep all DLLs and folders
beside it. Both versions store settings and the library in your user profile.

## First run

1. Open **AetherWall** and choose **Start engine**.
2. Select a scene in **Library**, choose a monitor, and apply it.
3. Adjust FPS and pause rules in the performance settings.

Closing Manager keeps wallpapers running. To stop everything, use the complete
exit action in Manager or the tray menu. Exit completely before updating or
uninstalling. Uninstalling preserves the library, settings, and imported source files.

To test rendering in an ordinary window, launch
`AetherWall.Manager.exe --core-window`, then start the engine. This diagnostic
mode helps distinguish GPU problems from desktop integration problems.

## Your scenes and catalog

A scene contains `manifest.json`, images, and built-in effects. Import a folder
or `.awscene` through Library; see the [scene format](docs/SCENE_FORMAT.md)
and [bundled examples](scenes/README.md).

No public catalog server is configured by default. Open **Discover → Examples**
for bundled scenes. To use your own catalog, enter its HTTPS address in Settings.
The separate [FastAPI server](server/README.md) supports search, moderation, and
local or S3-compatible storage. Published content requires author rights.

## Project status

AetherWall is a **preview**. CI checks compilation and automated tests; the full
Windows, GPU, and monitor compatibility matrix still needs verification. Shell
integration is experimental, performance figures have not been established, and
current builds are unsigned.

Successful `main` builds create individual GitHub **Pre-releases** with the installer,
ZIP, sources, and checksums. Failed builds and pull requests do not publish.
Each release points to its own commit and preserves previous downloads.

For problems, include your build version, Windows version, GPU/driver, reproduction
steps, and logs from `%LOCALAPPDATA%\AetherWall\logs` with personal information removed.
See the [acceptance matrix](docs/M5_TEST_PLAN.md) and
[desktop compatibility notes](docs/DESKTOP_INTEGRATION.md).

## Development

C++20, Direct3D 11/HLSL, Qt Quick, and SQLite. Core owns rendering, persistent
state, and the tray. Manager uses Named Pipe IPC and handles network requests.
The Python catalog server runs separately.

- [Build and diagnostic commands](docs/DEVELOPMENT.md)
- [Installer builds and releases](docs/RELEASE.md)
- [Scene format](docs/SCENE_FORMAT.md) and [IPC protocol](docs/IPC.md)
- [Implementation status](docs/IMPLEMENTATION_STATUS.md) and [specification](WINDOWS11_WALLPAPER_ENGINE_SPEC.md)

## Contributing

Read [CONTRIBUTING](CONTRIBUTING.md) and the [Code of Conduct](CODE_OF_CONDUCT.md).
Use the [issue forms](https://github.com/Ordo-xsenos/open_wallpaper/issues/new/choose)
for bugs and ideas. Report vulnerabilities privately following [SECURITY](SECURITY.md).

## License

Project code is [MIT](LICENSE). Original bundled scenes use CC0-1.0.
Dependencies have their own terms: [third-party notices](THIRD_PARTY_NOTICES.md).
