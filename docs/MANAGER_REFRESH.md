# Manager interface and startup refresh

## Behavior

Manager opens Installed and probes Core before starting it once automatically.
Signing in to Windows still starts Core only if the separate autostart setting
is enabled. Installed and Discover lead with static previews; selection never
applies a wallpaper by itself. Apply remains pinned below scrolling metadata.
Settings contains performance/general sections; Displays opens a modal overview.
Default size is 1440×900 logical pixels, limited to the available screen. Minimum
1000×650 also shrinks to fit smaller available desktops at high DPI.

Launch failures retain process-creation details, early exit code or readiness
timeout. Logs and copied diagnostics work before connection. Copy includes OS/
application version, current launch/render error, log location and at most 16 KiB
from the newest Core log. Review personal paths before sharing it.

A failed render in service mode leaves IPC, library and settings available;
Apply is disabled until a renderer/display set exists. Retry startup reuses the
existing reload command when connected. Recovery remains bounded and does not
create an automatic restart loop. Diagnostic CLI failures still return nonzero.
Closing Manager sends no stop and only releases its observation handle.

## Reproduce local checks

```sh
cmake --build --preset manager-dev
ctest --preset manager-dev --output-on-failure
cmake --build out/build/manager-dev --target AetherWall.Manager_qmllint
python3 -m pytest tests/release server/tests -q
```

Linux captures use `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software` with a
writable `XDG_CACHE_HOME`, `--capture PATH --test-library --capture-size 1440x900`.
Use `--test-english`, `--capture-page 1` (Discover), `2` (Displays), `3`
(Performance), `4` (Settings), and `--test-render-failure` for the labelled fixture.
Set `QT_SCALE_FACTOR=1.5` or `2` for scaled captures. Test transport and test renderer
failure flags are compiled only into the Linux development harness. They do not
prove Windows desktop compatibility. Local reports: `out/verification/manager-refresh/`.

## Windows acceptance — NOT TESTED locally

1. Install/extract fresh payload without developer PATH or elevation. Manager
   starts Core and loads six examples. Reopening Manager reuses the same Core.
2. In isolated copies, remove Core EXE or deny its execution. Verify readable
   error, accessible Logs/Copy diagnostics, and no repeated automatic launches.
3. Run new `Manager.WindowsProcessLifetime` and Manager control suites. Probe
   tests cover Unicode/spaced paths, distinct start/exit errors, duplicate-launch
   prevention and a child surviving destruction of its observer.
4. Reproduce the user's rendering failure. Verify the actual Core log and status,
   library access, disabled Apply, and successful explicit retry where supported.
   Never report antivirus blocking without evidence. Do not disable protection.
5. Verify RU/EN, keyboard tab/activation, import and delete confirmations, resize
   the inspector, scroll metadata/settings, and test Windows scaling 100/150/200%.
6. Apply a scene; close Manager with Settings open. Core and wallpaper continue.
   Complete exit closes both. Reinstall/update preserves settings and favorites.
7. Release staging runs a finite WARP window render in addition to help, scene
   validation and QML capture. Inspect `core-window-warp.log` and Core logs.
   Hosted software rendering does not establish WorkerW/input/GPU compatibility.
