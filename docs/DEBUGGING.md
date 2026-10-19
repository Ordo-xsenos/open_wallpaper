# Collecting AetherWall diagnostics

Debug logging works in the Release build. It does not require administrator access.

## One-click Windows session

1. Install/extract a build containing `AetherWall-Debug.cmd`.
2. Exit AetherWall completely through its tray menu. Closing Manager alone leaves Core running. The debug launcher refuses to reuse an existing process.
3. Open **AetherWall Debug** from the Start menu, or double-click `AetherWall-Debug.cmd` beside `AetherWall.Manager.exe`.
4. Reproduce the problem, then close the Manager window. Keep the launcher console open until it prints **Diagnostics archive**.
5. Send that `.zip` file for analysis. Archives are saved under `%LOCALAPPDATA%\AetherWall\diagnostics`, beside their session directory. Core may remain running in the tray; exit it normally when finished.

The launcher uses Windows PowerShell with a process-local execution policy. It does not change Windows security settings. If organizational policy prevents it from running, use the direct executable commands below and send the log folder.

## What is recorded

- Manager startup, Qt plugin/platform/QML messages, IPC command names and request IDs, connection errors, Core launch failures and early exit codes.
- Core startup phases, Windows environment, display/GPU selection, renderer/Shell errors, recovery and periodic metrics; debug mode adds IPC sizes and initialization checkpoints.
- Windows build, GPU driver information, Manager exit code, and recent Application Error/Windows Error Reporting entries mentioning AetherWall when accessible. DLL failures before application startup may appear only in the exit code or Windows events.

Each process log is flushed immediately and capped at 16 MiB. Collection copies at most 12 session files and 64 MiB of log data, including logs still being written. This is targeted application diagnostics; it does not enable the Direct3D debug layer or collect all Windows events.

The archive excludes settings databases, catalog credentials, wallpaper files and memory dumps. Paths and Windows error messages may contain your Windows username or other local information; review the ZIP before sharing. Logs do not by themselves establish antivirus interference.

## Direct commands

```powershell
.\AetherWall.Manager.exe --debug --debug-dir "$env:LOCALAPPDATA\AetherWall\diagnostics\manual"
# Independent finite renderer check; exit the normal Core first:
.\AetherWall.Core.exe --debug --mode window --duration 15 --log-dir "$env:LOCALAPPDATA\AetherWall\diagnostics\manual"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Collect-AetherWallDiagnostics.ps1 -SessionDirectory "$env:LOCALAPPDATA\AetherWall\diagnostics\manual"
```

Without `--debug-dir`, Manager creates a timestamped session directory. A Core started by that Manager inherits debug logging and the same directory. Restart both processes for a fresh diagnostic session; attaching Manager to an already-running Core cannot enable its debug logging retroactively.

Logs remain on disk until you remove them; delete old diagnostic sessions after investigation.
