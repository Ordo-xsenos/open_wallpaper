# Windows 11 desktop integration

## Scope and support status

M0 targets Windows 11 x64 build 26100+ in an interactive desktop session.
The user reported successful M0 execution on 2026-10-17; the supplied screenshot
shows diagnostic window output. Desktop/input/restart details remain unverified
independently, and M1/M2 changes require a new Windows regression run. A version check
is only an eligibility check, not a compatibility guarantee. New Windows builds
must be tested rather than inferred compatible.

## Host selection

1. Locate Progman and the current Shell window; require the same owning process.
2. Send experimental `0x052C` once per discovery with `SendMessageTimeoutW` and a
   1000 ms timeout (`SMTO_ABORTIFHUNG | SMTO_BLOCK | SMTO_ERRORONEXIT`).
3. Log Progman/WorkerW HWNDs, parent, PID, bounds, visibility and DPI awareness,
   and any direct `SHELLDLL_DefView` child.
4. Require exactly one visible icon-bearing top-level Progman/WorkerW. Choose an
   empty, visible, same-process WorkerW behind it in top-level z-order. Refuse
   missing, ambiguous, nested, or otherwise unsupported layouts.
5. Create an invisible surface under a thread DPI context matching that host.
   Set child styles, verify matching awareness, call `SetParent`, map each physical monitor
   rectangle through the parent DPI transform into client coordinates, and initialize
   D3D11. Refuse hosts that do not cover the requested monitor.
6. Show the entire output set without activation only after attachment and validation. No border,
   caption, layered-window dependency, fullscreen-exclusive mode, or Shell input hook.

M0 uses a native child HWND, not an injected Explorer component. It never
destroys Shell windows, replaces the Shell, changes system security or static
wallpaper settings, or automatically launches Explorer. Only owned HWNDs are destroyed.

## Presentation and recovery

`auto` tries a two-buffer flip-discard swap chain; invalid/unsupported creation
can fall back to blt-discard. Other errors are not treated as compatibility
fallbacks. `flip` and `blt` force independent experiments so successful creation
with invisible output can be diagnosed. An API success does not prove that DWM
shows the surface at the intended depth.

A hidden top-level control HWND receives TaskbarCreated, session, power, and
display broadcasts. Before rendering, the host's current HWND, parent relationship,
Shell PID, visibility, class and relative z-order are checked. Invalid state stops
rendering, hides the owned surface and releases graphics before rediscovery.
Initial recovery waits one second; subsequent failures wait two seconds. There
are at most three consecutive failed attempts and six attempts per thirty seconds,
to prevent restart storms while allowing ordinary long-lived reconnects. Failure to initialize the first surface exits
directly. Device removed/reset uses the same bounded recovery path.

Lock/suspend, battery and console display-off stop active rendering; manual pause,
foreground fullscreen/maximized and minimized diagnostic windows are per-output. Occluded
swap chains are probed at 250 ms intervals without drawing. Normal Ctrl+C or a
finite run exits through cleanup. Closing/killing the console process is not a
graceful-shutdown test; Windows owns final process resource cleanup in that case.

## Known risks and intentional limits

- Progman/WorkerW/message `0x052C` is an undocumented Shell arrangement.
- `SetParent` is documented, but using Explorer as a cross-process parent and
  relying on its topology remains experimental. Parent/child races cannot be
  eliminated by HWND checks alone; visual and failure-path testing is essential.
- Modern nested WorkerW layouts may be refused by this first implementation.
  Do not broaden discovery heuristics without an actual topology report and input tests.
- Matching parent DPI context reduces cross-DPI reparenting risk; it does not
  prove sharpness or geometry correctness at every scaling setting.
- M2 adds independent logical outputs, GPU selection and pause policies. M3 adds Core tray and
  persistent library/settings, pending Windows acceptance. See M2_TEST_PLAN.md. Independent panel
  DPMS detection is not implemented; console power notifications are global.
- Frame-rate logs count successful Present calls, not scan-out. GPU timestamps measure
  draw commands only; see PERFORMANCE.md for all metric definitions.
- The console remains visible in finite diagnostic mode; service desktop mode
  detaches it and uses the M3 tray. In diagnostic mode, the
  wallpaper surface must not appear there. Removing the console is later work.
- Rapid repeated changes can exhaust the restart-storm budget. Scene/time/manual
  pause snapshots survive in-process rebuilds for up to sixteen remembered IDs;
  device-path/alias changes can identify the display as a new output.

## Compatibility evidence

| OS build | GPU / driver | Session / DPI / resolution | Window mode | Desktop / swap effect | Explorer restart | Evidence |
| --- | --- | --- | --- | --- | --- | --- |
| NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED | M0 user-reported; diagnostic screenshot supplied, full matrix absent |

Add only observed results, including source revision/hashes, actual swap effect,
PASS/FAIL/NOT TESTED, logs, limitations, and steps from `TEST_PLAN.md`.

## API references

- [SetParent and DPI caveats](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setparent)
- [SendMessageTimeoutW return and timeout behavior](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendmessagetimeoutw)
- [CreateSwapChainForHwnd and deferred destruction](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgifactory2-createswapchainforhwnd)
- [DXGI flip model](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-flip-model)
