# M2 performance report and measurement protocol

## Current evidence

Windows measurements: **NOT TESTED**. No interactive Windows machine, D3D driver,
MSVC or Microsoft fxc is available in this development environment. Portable tests
establish policy behavior, not FPS, power use, Shell compatibility or GPU efficiency.
No performance advantage over another wallpaper engine is claimed.

| Test machine | OS / driver / GPU | Display geometry / DPI / Hz | Scenario | FPS / avg / P95 | CPU / memory / GPU draw time | Status |
| --- | --- | --- | --- | --- | --- | --- |
| Integrated-GPU Windows 11 notebook | NOT RECORDED | NOT RECORDED | Static, animated, paused; 15/30/60 | NOT MEASURED | NOT MEASURED | NOT TESTED |
| Discrete/hybrid Windows 11 notebook | NOT RECORDED | NOT RECORDED | Two independent outputs; each GPU preference | NOT MEASURED | NOT MEASURED | NOT TESTED |

## Measurement procedure

1. Build Release, run CTest, record binary/source hashes, OS build/UBR, driver,
   actual adapter LUID, display topology, resolution, scale and refresh. Record
   AC/battery, power mode, thermals and background applications. Preserve raw traces.
2. Warm up 30 seconds; then record at least 120 seconds for `still`, `gradient`,
   `stars`, `rain` and `mountains`, at each preset. Repeat on one output, two outputs
   with the same scene, and independent scenes. Retain identical content/settings
   for comparisons. Repeat runs and report variation rather than one best sample.
3. Capture PresentMon/ETW and inspect with Windows Performance Analyzer. Use actual
   displayed-frame cadence for delivered FPS and frame-interval avg/P95. Inspect CPU
   scheduling, wakeups, GPU engines, copies and allocations. Task Manager is only an
   additional reference. Record tool versions and capture commands from those tools.
4. Repeat fullscreen on one monitor, battery, lock, console display-off, pause and
   static scenes. Compare steady periods without expose/resize/recovery events;
   paused/static outputs should have no continuing draw/normal Present submissions.
5. Repeat hot-plug, Explorer restart and controlled device loss. Track peak as well
   as steady memory across recovery/reload. Record safe refusal separately from
   successful rendering. Validate that shared devices/textures reduce duplication
   on the same adapter without making claims about different adapters.
6. Compare Lively Wallpaper and Wallpaper Engine only on the same machine with
   matched scene complexity, resolution, FPS and power settings. If content cannot
   be matched, label the comparison uncontrolled; do not infer superiority.

Example evidence capture (run from the repository root):

```powershell
.\tools\Test-M2.ps1 -Mode desktop -PerformancePreset balanced -Duration 180 `
    -Gpu auto -MonitorScenes @{
        DISPLAY1 = 'scenes/bundled/stars'; DISPLAY2 = 'scenes/bundled/rain'
    }
```

## Core log definitions and limits

Five-second logs provide complementary diagnostics:

- `wallTimePresentRate`: successful normal Present calls / elapsed wall seconds,
  per output. It is not a count of frames actually scanned out by the display.
- `submissionAvgMs/P95Ms`: wall duration of render commands plus nonblocking
  Present, from the current logging interval (latest 600 samples at most).
  These are submission costs, not refresh intervals or pure CPU execution time.
- `gpuDrawAvgMs/P95Ms`: D3D11 disjoint/timestamp queries around that output's draw
  commands, rolling over the latest 600 valid measurements. Read asynchronously
  with DONOTFLUSH; skip busy slots/disjoint samples. `gpuSamples=0` means unavailable
  or pending, not zero GPU cost. Queuing/Present/composition/cross-GPU copies are
  outside the draw measurement. Query failure is logged; there is no GPU load %.
- `processCpuOneCorePercent`: kernel + user process time / wall time, with 100%
  meaning one logical core; includes decoding/logging and driver work in process.
- `renderThreadCpuOneCorePercent`: the message/render thread's kernel + user time
  on the same scale, including window handling, policy work, upload and logging.
  Neither value is normalized to Task Manager's whole-machine CPU percentage.
- `workingSetBytes`: total resident process working set, including shared pages.
  `privateCommitBytes`: committed private bytes, not private resident memory or VRAM.
- `totalDrawPresents`/`totalBusyPresents`: attempts / queue-busy results during this
  output incarnation. Rebuilds reset output counters. Busy/occluded results retain
  pending redraw. Shared GPU contexts are called serially on one thread;
  DXGI_PRESENT_DO_NOT_WAIT avoids waiting for a full presentation queue.
- `pauseReasons`: bit mask: manual 1, lock 2, suspend 4, battery 8, console display-off
  16, fullscreen 32, maximized 64, minimized 128. Multiple blockers can coexist.

Output timelines, scene snapshots and manual pause survive bounded in-process
recovery, but telemetry windows reset. Console display state is global; individual
panel sleep cannot be concluded from it. DXGI occlusion behavior and VSync delivery
must be tested per topology, swap effect and driver.

## Implementation references

- [GPU preference enumeration](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_6/nf-dxgi1_6-idxgifactory6-enumadapterbygpupreference)
- [DXGI Present options](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-present)
- [Asynchronous D3D11 query data](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-getdata)
- [Power-setting notifications](https://learn.microsoft.com/en-us/windows/win32/power/power-setting-guids)
