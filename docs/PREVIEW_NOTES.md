# AetherWall 0.5.0-preview.1

Preview for Windows 11 24H2+ x64. Full hardware/install acceptance is pending.
No performance, compatibility or power-efficiency guarantee is made.

- Original GPU scenes, local library, per-monitor assignments and pause policies.
- Russian/English Qt Manager, independent Core and tray, verified catalog imports.
- Six original examples work without an online catalog. No public server is built in.
- Installer uses the current user's Programs folder and requests no administrator rights.

Start Manager, choose a scene and monitor, then Apply. Closing Manager leaves
Core running; use the tray or Manager's complete-exit action before updating or
uninstalling. The installer refuses while the application running markers exist.
The installer does not enable autostart or launch wallpaper rendering by default.

Uninstall preserves `%LOCALAPPDATA%\AetherWall`, imported source files, settings,
library and logs. It removes its installed program files/shortcuts and only the
autostart value pointing to this installed Core. Reinstalling can restore an
autostart preference that you previously enabled in the preserved settings.

Experimental Windows Shell integration can fail safely on unsupported desktops.
Test `--core-window` diagnostic mode before desktop mode. Windows installation,
upgrade/uninstall, Explorer recovery, mixed-DPI outputs and integrated/discrete GPU
acceptance remain NOT TESTED in the development Linux environment.

Artifacts produced by CI are unsigned preliminary builds. A successful build is
not a hardware/desktop acceptance result. See RELEASE.md and M5_TEST_PLAN.md in
the source distribution for remaining Windows acceptance checks.
