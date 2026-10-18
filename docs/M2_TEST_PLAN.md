# M2 Windows acceptance plan

Status: **NOT TESTED on Windows**. The user authorized starting M2 on
2026-10-17; M1 Windows acceptance and M0 desktop regressions remain pending.
Use x64 Developer PowerShell, bootstrap, build and CTest both Debug and Release
as described in README.md. Keep source hashes and the actual tested binary.

## First run: independent diagnostic windows

Run from the repository root. Discover real aliases before assigning scenes:

```powershell
$exe = '.\out\build\win-msvc-debug\AetherWall.Core.exe'
& $exe --list-monitors
& $exe --mode window --all-monitors --preset balanced --duration 120 `
    --monitor-scene DISPLAY1 scenes/bundled/stars `
    --monitor-scene DISPLAY2 scenes/bundled/rain
```

Substitute connected aliases. Without `--all-monitors`, diagnostic mode opens
only on the primary. Desktop mode always creates one output per detected monitor.
`--scene` is the fallback for outputs without assignments. Omit it for the built-in
gradient. Each diagnostic window has F5 reload and Space pause/resume; closing any
window or Ctrl+C stops the whole Core. Desktop windows do not capture these keys.

Repeat with the persistent device-path IDs printed by `--list-monitors`, quoted
as single PowerShell strings. DISPLAY aliases may change after reconnecting;
physical device paths are preferred but can also change after driver/port changes.
Assignments survive recovery within this process; persistence across process restarts
belongs to M3. Duplicate connected ID/alias assignments are rejected. Unknown or
disconnected selectors are logged and retained for subsequent discovery.

## Desktop, geometry and resource sharing

1. Repeat the run using `--mode desktop` only after diagnostic rendering works.
   Verify different scenes behind icons on both displays, selection, context menu,
   taskbar and no wallpaper Alt+Tab entry. Preserve logs and screenshots.
2. Test a display left of and another above the primary; test portrait rotation,
   100%/150%/200% mixed scaling, resolution and refresh changes, primary switching.
   Expect correct full-monitor coverage, no seams or foreground overlay.
3. Test duplicate/mirror mode. Windows exposes mirrored displays as one logical
   output; independent scenes require extended mode. No physical-panel independence
   is claimed for mirrored targets.
4. Assign the same image scene to both outputs on one GPU. Inspect selected LUIDs,
   device creation and GPU allocations using PIX/ETW; expect one device/context and
   one texture set per scene snapshot on that adapter. Different adapters retain
   their own resources. Live/staged uploaded textures are capped at 256 MiB/device;
   this is not a limit on total VRAM including swap chains and driver allocations.
5. Repeat `--gpu auto`, `power-saving`, `high-performance` on a hybrid notebook.
   Auto favors the minimum-power adapter among those driving that monitor, then
   other capable hardware. Explicit preference can incur cross-adapter copying.
   Log actual adapter/driver, measure power and frame delivery; no vendor assumptions.

An unsupported WorkerW topology or parent that does not cover the selected monitor
must produce a logged safe refusal, destroy partial outputs, and leave the normal
desktop usable. Do not expand Shell heuristics without evidence.

## Pause and scheduling

- Fullscreen policy defaults on: cover only one monitor with a foreground game or
  borderless application. That output's Present counter must stop; the other runs.
  Return to desktop and expect animation to resume without advancing paused time.
- Maximize a window: default remains active unless its real bounds also meet the
  fullscreen test. Repeat with `--pause-maximized on`; only intersected outputs pause.
  Repeat with `--pause-fullscreen off`. Own windows and Shell desktop are excluded.
- Toggle Space twice in one diagnostic window. Other outputs continue. Combine
  manual pause with lock or battery: clearing one blocker must not clear another.
- Unplug AC on a notebook: default pauses all outputs; `--pause-battery off` keeps
  them running. Lock/unlock, suspend/resume, minimize/restore diagnostic windows.
- Test display timeout and wake. Console-wide display-off notifications pause all
  outputs. DXGI occlusion pauses/probes an individual output, but its behavior must
  be observed on the tested driver. Independent panel DPMS sleep is not identified
  reliably by this implementation; record the limitation rather than a false PASS.
- Assign `still` to one output and `stars` to another. After expose/resize settles,
  only the animated output's `totalDrawPresents` should grow. Confirm 15/30/60 caps
  for Eco/Balanced/Smooth, including mixed refresh rates. Present busy results must
  retain pending static redraws without spinning or blocking the other output.

## Recovery and regression

While different scenes are active, unplug/reconnect a display, change topology,
restart Explorer manually, lock/unlock and sleep/resume. Verify the last committed
scenes, paused timelines and manual-pause state survive on stable IDs. Failed F5
reload must preserve the previous image and still survive subsequent recovery.

Perform a controlled driver-reset/device-loss test only on a suitable test machine;
retain failure reason, attempt logs and cleanup evidence. Recovery hides all owned
surfaces, releases devices and rebuilds after one second (two seconds after failure).
Three consecutive failures or six attempts within thirty seconds stop safely.
Ordinary successful reconnects over longer sessions do not exhaust a lifetime budget.

Run M0 TEST_PLAN.md and M1_TEST_PLAN.md regressions, both swap effects, clean exit,
and repeated startup/shutdown. Use [Test-M2.ps1](../tools/Test-M2.ps1) to capture
CTest, arguments, machine details, source/binary hashes, monitor IDs and a result form:

```powershell
.\tools\Test-M2.ps1 -Mode window -MonitorScenes @{
    DISPLAY1 = 'scenes/bundled/stars'; DISPLAY2 = 'scenes/bundled/rain'
}
```

Complete every result as PASS/FAIL/NOT TESTED and record display scales/refresh rates,
physical/remote session, exact steps and limitations. Performance measurements use
[PERFORMANCE.md](PERFORMANCE.md); script completion never marks visual checks passed.
