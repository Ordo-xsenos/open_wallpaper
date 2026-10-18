# M0 verification plan

## Setup and evidence

Use a real Windows 11 24H2+ x64 interactive desktop, ordinary user rights, and
a built Debug/Release executable. Record OS build including UBR, GPU/driver,
physical vs virtual/remote session, monitor resolution/scaling, FPS/VSync/swap
settings, source commit SHA (or the supplied source hashes when unavailable),
duration, steps, logs, and screenshots. Keep all unperformed checks `NOT TESTED`.

`tools/Test-M0.ps1` captures much of this automatically; complete `RESULTS.md`
manually. Exit code 0 is not a visual test. Hosted Windows CI can check MSVC/fxc
compilation and unit tests, but cannot certify an interactive user's Shell topology.

## Automated checks

Run configure, build, and CTest for both Windows presets from README. Policy
tests cover strict CLI validation, explicit unlimited runs, WARP restrictions,
deadline rounding, dropping late frames, and permanent recovery exhaustion.
Linux policy tests check the same logic but do not exercise Windows code.

Check CLI behavior as well:

```powershell
$exe = '.\out\build\win-msvc-debug\AetherWall.Core.exe'
& $exe --help                  # exit 0, no HWND created
& $exe --fps 999               # exit 2, no HWND created
& $exe --mode desktop --warp   # exit 2, no HWND created
```

## Manual cases

| Case / related AC | Steps | Required observation |
| --- | --- | --- |
| Diagnostic renderer / AC-06 | Run window mode 60 s; resize and minimize/restore | Animated gradient, correct new client bounds, no error or endless redraw while minimized; closes at deadline |
| Software isolation | Run `Test-M0.ps1 -Mode window -Warp` if hardware rendering fails | Useful diagnostic only; WARP success never counts as desktop/GPU acceptance |
| Ordinary privileges / AC-01 | Run desktop mode from a non-elevated shell | No UAC prompt; no request to weaken security |
| Behind icons / AC-02 | After passing window mode, run desktop 300 s; select/drag icons, marquee-select, open desktop menu, use taskbar | Gradient behind icons; all input and taskbar remain usable |
| Surface behavior / AC-03 | Inspect Alt+Tab and attempt dragging/resizing wallpaper | No wallpaper entry, frame, drag/resize or focus capture; M0 console is separately expected |
| Explorer lifecycle / AC-04 | During a 300 s run, restart Windows Explorer once using Task Manager | Surface stops on host loss; gradient reattaches behind icons or exits safely with error; no stale overlay |
| Lock and sleep | Lock/unlock once; separately test sleep/wake | Rendering pauses; resumes or exits safely after bounded recovery; inspect logs |
| Display / DPI | Change primary-monitor scale and resolution; disconnect/reconnect a secondary display | No overlay, stale-sized surface, stuck process or unbounded recovery; primary-monitor-only output is expected in M0 |
| Virtual desktops | Open Task View, switch desktops, return | Icons/input work; any unsupported behavior is recorded; safe refusal is allowed, invisible output is not a functional PASS |
| Swap-chain comparison | Separate runs with `-SwapChain flip`, then `blt`, same resolution/FPS/VSync | Record actual selected swap effect, visible result, logs and limitations for each |
| Frame cap | Separate 15/30/60 FPS runs, VSync on/off where useful | Bounded present rate without busy-spin; use PresentMon/ETW for actual presentation timing, not only Task Manager |
| Shutdown / AC-15 subset | Let duration expire; separately Ctrl+C; close window in diagnostic mode | Owned window disappears, process exits, normal desktop remains |
| Unsupported host | If available, run where no supported empty WorkerW is found | Exit 1 with topology/error log; no visible fallback window or stuck overlay |
| Multiple instances | While one finite run is active, start a second | Second exits with an instance error; first continues |

Do not force unsupported topology by changing system security or injecting code
into Explorer. Do not kill or reset the GPU driver as part of routine M0 tests;
real device-loss behavior remains `NOT TESTED` until exercised in a controlled
Windows setup. Unit recovery-budget tests alone do not validate GPU recovery.

## M0 gate

M0 passes only after the hardware diagnostic renderer, behind-icon placement,
normal icon/menu/taskbar interaction, surface restrictions, clean shutdown,
and Explorer restart/recovery or safe refusal are actually observed on Windows.
If initial desktop attachment is refused, the safe-failure case may pass but
the functional desktop gate remains unpassed. Add the evidence to
`DESKTOP_INTEGRATION.md` and update `IMPLEMENTATION_STATUS.md` before M1.

The full AC-01–AC-16 matrix is still the release contract. AC-05 (static scenes) now has an M1 implementation pending Windows acceptance;
see M1_TEST_PLAN.md. AC-08 battery/fullscreen policies, AC-09 independent monitors, AC-10–AC-13 Manager/
catalog/packages, and installer aspects of AC-15 are not implemented by M0.
