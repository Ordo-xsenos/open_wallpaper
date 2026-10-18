# M4 catalog verification

## Direction contract

Extend the incumbent native dark Qt Manager. Library remains the application
surface; Discover finds and installs scenes without changing the current wallpaper.
Use the existing 208 px navigation, 24 px margins, blue focus/selection and Fusion
controls. No animations run in preview grids. The signature interaction is a
selected static preview beside metadata and a download action; successful validation
and Core import precede the Installed state. RU default and EN switching apply.
At 900×620 the grid reduces columns and details scroll to keep actions reachable.
Examples and injected test transport must be labelled; no invented live catalog.

## Local evidence (2026-10-18)

| Check | Result | Scope |
| --- | --- | --- |
| Manager build and QML lint | PASS | GCC 16.2.1, Qt 6.12 shared, Linux; no MSVC assertion |
| CTest | PASS | 70 entries: 68 portable tests plus Manager and catalog Qt suites |
| ASan/UBSan | PASS | 68 portable tests; leak detection disabled for traced sandbox |
| Server tests | PASS | 12 tests: actual in-process ASGI handlers, SQLite, local streaming, pagination/search/filter over 10,000 synthetic metadata records, bounds, moderation, rate limiting, publication rights proof and real Core validation |
| Python wheel | PASS | Local wheel built without dependency downloads |
| Catalog validation | PASS | Six original deterministic .awscene packages validated with SceneCheck; hashes/preview sizes checked |
| Qt failure cases | PASS | Corruption/truncation/oversize, unsafe URLs/redirect, wrong UUID, cancellation/retry, import/history without applying |
| Settings/DB migrations | PASS | v1→v2 retains FPS, language and favorite; receipt rollback and known SHA-256 vector tested |
| Visual captures | PASS | RU 1180×780; EN selected details and RU disconnected/API-unavailable 900×620, offscreen software rendering |
| Windows / PostgreSQL / S3 / deployed TLS | NOT TESTED | Requires target environments |

The existing real Unix socket test inside the Manager suite is SKIPPED because
the sandbox forbids sockets. Injected IPC tests exercise the actual ControlService
and SQLite instead. Python tests retain the real ASGI/thread-pool path; a test-only
timer handles blocked cross-thread event-loop wakeups in this runner. No socket,
GPU, production storage or desktop compatibility result is inferred.
Raw logs/screenshots are under `out/verification/m4-*` (not shipped as source).

## Repeat automated checks

Build/test Manager using the presets in README. Run `cmake --build
out/build/manager-dev --target AetherWall.Manager_qmllint`, then
`python -m pytest server/tests -q`. Build portable-tests first so server publication
tests find `out/build/portable-tests/AetherWall.SceneCheck`; Windows can pass the
Windows validator to the publication CLI. Missing Linux validator tests skip
explicitly. Rebuild fixture packages with `tools/Build-Catalog.py --validator PATH`.

For headless captures use `QT_QPA_PLATFORM=offscreen`,
`QT_QUICK_BACKEND=software` and a writable `XDG_CACHE_HOME`; launch Manager with
`--test-library --mock-catalog --capture-page 1 --capture PATH.png`.
Add `--capture-size 900x620 --capture-select-first --test-english` for compact EN.
Capture support must be enabled by the development preset. Page indices after M4:
Library 0, Discover 1, Monitors 2, Performance 3, Settings 4.

## Windows and deployment acceptance

1. Build Debug and Release with shared Qt/MSVC, deploy dependencies, start Core
   in diagnostic window mode, then exercise desktop mode with previous matrices.
2. Open Examples, search `parallax`, filter categories, change sorting and language;
   test keyboard focus/selection and scrolling at 100%, 150%, 200% DPI.
3. Delete an unassigned bundled scene from Library, download it again, verify
   Installed appears only after Core success. The active wallpaper must stay intact.
4. Run the local mock API; opt into loopback HTTP. Repeat with trusted HTTPS and
   PostgreSQL/local objects, then a private S3 bucket and optionally a CDN origin.
   Verify streamed bytes/hashes and schema migration using fresh and existing DBs.
5. Disconnect networking, stop API, cancel mid-transfer, retry, corrupt metadata,
   send wrong/oversized/incomplete packages, expire TLS and redirect outside the
   allowlist. Nothing invalid may enter Library or download history. Check cache
   stays ≤256 MiB and interrupted temporary files do not accumulate during a run.
6. Publish without approval: list/detail/assets must hide the draft. Approve, fetch
   and compare SHA-256, withdraw and verify access disappears. Audit rights evidence.
7. Record OS/GPU/driver, source/archive SHA, DB/object backend, commands, logs and
   actual PASS/FAIL/NOT TESTED. No production endpoint, credentials or hosting budget
   is configured by this change. M5 installer/release remains after acceptance.

## UI review and preserved context

A fresh review scored three fixes resolved: pinned compact-window action,
localized RU catalog errors and accessible checked/installed card state. Windows
screen-reader behavior remains unverified. Palette, typography and existing
Fusion controls were preserved. The incumbent DESIGN.md/sidecar schema and freeform
Platform context predate M4; their format was not migrated as part of this work.
