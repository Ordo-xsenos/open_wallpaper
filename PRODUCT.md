# AetherWall product

<!-- impeccable:product-schema 1 -->

## Platform

Windows 11 native desktop (Qt Quick Manager and independent Win32 Core).

## Stack

C++20, Qt 6 Quick/QML, Direct3D 11, SQLite, versioned JSON over secured Named Pipe.
The specification and user confirmation on 2026-10-18 define this stack.

## Users and purpose

Windows users choose original local wallpaper scenes, assign them to displays,
and control resource use. Closing Manager leaves wallpapers and Core's tray running.

## Capabilities and constraints

Follow WINDOWS11_WALLPAPER_ENGINE_SPEC.md. The current preview implementation includes
M3-style library/import, favorites, monitor assignment, settings, diagnostics and
process controls, plus M4-style catalog browsing with bundled original examples
and optional server configuration. These capabilities do not establish milestone
acceptance. Installer and release validation are M5. No Electron. Qt must be dynamically
linked. Russian default and English switching; dark appearance and DPI adaptation.
No fabricated compatibility or performance results. Core alone owns graphics,
persistent application state and tray; Manager communicates only through IPC.

## Evidence

Six original bundled scenes; M0 user-reported diagnostic execution. M1/M2 Windows
acceptance remains pending. User approved following the specification and building
Manager directly in code on 2026-10-18. The approved Manager refresh follows the
classic Wallpaper Engine desktop experience: top Installed/Discover navigation,
compact original-preview grid, resizable inspector with explicit pinned Apply,
separate Settings/Performance window and modal Displays. Opening Manager probes
for Core and automatically starts it when needed; Windows-login autostart remains
a separate opt-in setting.

The refresh has a SHIP disposition for its reviewed UI scope after resolving
explicit-Apply, truthful display-state and localized-dialog findings (see
`out/verification/manager-refresh/finish-verdict.md`). Captures are Linux offscreen
Qt renders with test transport labelled as having no GPU. Windows runtime and
rendering acceptance remain NOT TESTED; no milestone advancement is implied.

## Product principles

Keep engine operation independent of the interface. Preserve the current scene on
rejected replacements. Keep imported content local and bounded. Expose connection
and failure states honestly. Default Windows-login autostart to off.
