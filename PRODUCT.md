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

Follow WINDOWS11_WALLPAPER_ENGINE_SPEC.md. M3 adds local library/import, favorites,
monitor assignment, settings, diagnostics and process controls. Online discovery is
M4; installer and release validation are M5. No Electron. Qt must be dynamically
linked. Russian default and English switching; dark appearance and DPI adaptation.
No fabricated compatibility or performance results. Core alone owns graphics,
persistent application state and tray; Manager communicates only through IPC.

## Evidence

Six original bundled scenes; M0 user-reported diagnostic execution. M1/M2 Windows
acceptance remains pending. User approved following the specification and building
Manager directly in code on 2026-10-18.

## Product principles

Keep engine operation independent of the interface. Preserve the current scene on
rejected replacements. Keep imported content local and bounded. Expose connection
and failure states honestly. Default autostart to off.
