# Implementation status

## Current milestone

**M5 preview packaging and automatic release workflow implemented; Windows release acceptance pending.**
On 2026-10-18 the user authorized M3 and confirmed following the specification
with a direct code implementation. M1/M2 Windows acceptance and the full M0
regression matrix remain pending. This authorization is not a fabricated PASS.
On 2026-10-18 the user authorized M4, then M5, and automatic preview publication.
No external deployment or hosting spend is implied.

On 2026-10-17 the user reported successful M0 execution on a friend's Windows
computer and explicitly authorized M1. Their screenshot shows the diagnostic
D3D11 window and reports an fxc failure caused by the reserved identifier `point`.
The friend renamed it to `p`; that fix is now in this repository. OS/GPU details,
behind-icon input tests and Explorer restart logs were not supplied. These M0
matrix entries remain NOT TESTED independently; user-reported success is not
recorded as a fabricated full matrix PASS.

## M5 preview packaging

- Version 0.5.0-preview.1, generated application/PE version metadata, per-session
  running markers for Core/Manager and a stable per-user Inno Setup installer.
- Fixed Programs directory, standard-user installation, optional shortcuts/launch,
  no forced process termination or default autostart. Uninstall preserves user data
  and removes only an autostart command pointing to its installed Core.
- Fresh staging, Release/Qt version gates, official app-local CRT, SChannel/QML
  deployment, x64 PE checks, required dependency/notices checks and per-file hashes.
- Exact-tag Qt source notice collection, companion source archives, portable
  binary ZIP and installer SHA-256 sidecars; automatic GitHub pre-releases from main.
- Unified CI with Linux checks, Core Debug and Manager Release packaging.
  Packaging policy tests use synthetic PE fixtures; they do not run executables.
- Linux Manager builds and 70 CTest entries pass; 32 release policy/notice/publication tests and 12
  server tests pass. The user subsequently reported successful Windows CI jobs. Installed runtime,
  real hardware and exact redistribution audit remain unverified.

This is the source/build slice of M5. Completion requires the M5_TEST_PLAN.md
Windows matrix and durable source/license materials before public distribution.

## Automatic preview publication and project documentation

- Successful main push/manual builds publish unique GitHub pre-releases with
  installer, portable ZIP, SHA-256, application source and matching Qt sources.
- PR/non-main builds cannot publish; only the publish job has repository write
  permissions. Missing/corrupt assets fail before draft creation; failed uploads
  never promote a draft. No stable-release or hardware acceptance is implied.
- README introduces the product, downloads, first run and limitations in English,
  with linked Russian and Uzbek Latin versions.
  Detailed diagnostic/build commands moved to DEVELOPMENT.md.
- Publication orchestration is tested with a fake gh process, not a real upload.

## CI/runtime and community follow-up

- Merged the overlapping Windows workflows into AetherWall CI; main pushes, PRs
  targeting main and manual runs use one workflow with no repeated Release build.
  Publication requires Linux, Debug and Release to succeed and remains main-only.
- The reported Windows `0xC0000135` catalog test startup failure is addressed by
  CTest prepending the linked Qt DLL directory to PATH. No failed test is skipped.
- Staged runtime checks launch Core --help, SceneCheck examples and software-rendered
  Manager/QML with a clean PATH before packaging. Timeout/exit failures abort the
  release; logs, startup capture and CTest dependency diagnostics are retained.
- CONTRIBUTING, CODE_OF_CONDUCT, SECURITY, bug/feature issue forms and a PR template
  are added in English; all README variants link to them and are in source packages.
- Linux Manager build, 70 CTest entries and 44 Python tests pass after the change.
  YAML/publication gates, local documentation links and issue schemas validate.
- Windows execution of these new runtime checks remains NOT TESTED. Description
  and private reporting API writes failed from this environment; follow
  COMMUNITY_SETUP.md and verify the repository's Community Standards after merge.

## M4 catalog slice

- Manager Discover page: search, categories, pagination, sorting, RU/EN, explicit
  bundled examples and configurable HTTPS API/CDN origins. Offline installed
  wallpapers remain available; no fake production endpoint or popularity counts.
- Six deterministic original CC0 packages with preview hashes and rights evidence,
  an embedded mock transport and matching FastAPI mock API.
- Bounded streaming downloads, progress/cancel/retry, idle/total timeouts, manual
  redirect allowlist, size/SHA-256 checks and off-GUI Core scene validation.
- Core rechecks a frozen local snapshot before transactional import/download
  history; installed/active scenes remain intact on failure. M3 settings/database
  migrate to schema v2 while retaining preferences and favorites.
- FastAPI read routes, indexed bound-parameter metadata queries, SQLite development
  repository, PostgreSQL psycopg adapter, local/S3 object storage, administrator CLI
  validation/rights proof/draft moderation and environment deployment examples.
- Local ASGI, portable control and Qt tests pass; the evidence and remaining manual
  cases are in M4_TEST_PLAN.md. Real PostgreSQL/S3/HTTPS deployment and Windows
  acceptance are NOT TESTED. Trending/popular have stable fallback ordering until
  measured counts exist. Core import remains synchronous and bounded.

## Implemented

- M0 desktop integration, diagnostic window, bounded recovery, logs and frame cap retained.
- Version 1 JSON scene definitions, IScene/IRenderer boundaries, strict metadata,
  types, fields, reference, dimension, particle and memory limits.
- Directory and .awscene ZIP loader; no extraction, executable content, links,
  unsafe/colliding paths, encryption or unlimited decompression.
- PNG/JPEG WIC and static WebP decoding, immutable shared GPU textures, SDR/sRGB
  sampling/blending, layers, transforms, tint/opacity, aspect-fit and motion.
- Trusted gradient/pulse effects and instanced stars/rain/snow (one draw per layer).
- Demand-driven static rendering with invalidation and bounded Shell/stop watchdog;
  totalDrawPresents logging for real Windows verification.
- --scene startup loading and F5 diagnostic reload; rejected packages/decodes
  preserve the previous scene, startup failure leaves the built-in gradient.
- Six original bundled scenes, deterministic artwork generator, public JSON Schema,
  SceneCheck command and Windows acceptance instructions.
- Updated vcpkg baseline/dependencies, clean-checkout bootstrap upgrades, Windows CI,
  and copying installed dependency copyright notices beside Core.

## M2 implemented

- Up to sixteen logical monitors, physical coordinates including negative origins,
  stable DisplayConfig device paths with logged GDI fallback, refresh rates and DPI.
- One owned output/window/swap chain and timeline per monitor, independent scene
  assignments, same-on-all fallback, per-window F5 and manual Space pause.
- Per-adapter device/context and per-snapshot texture sharing; bounded staged GPU
  texture uploads. Auto / power-saving / high-performance adapter preference.
- Eco/Balanced/Smooth frame caps; nonblocking Present with separate queue-busy and
  occluded handling; static demand rendering retained.
- Per-monitor foreground fullscreen/maximized rules, battery, lock, suspend,
  minimized, manual and console display-off blockers; WinEvent hooks and watchdog.
- Bounded group recovery after display/Shell/device changes, preserving committed
  CPU snapshots, animation time and manual pause for remembered monitor IDs.
- Per-output submission avg/P95, asynchronous GPU timestamps when available,
  successful Present rates, process/render-thread CPU time and memory counters.
- Windows M2 capture script, manual test matrix and honest performance report.

Individual physical-panel DPMS state is not reliably identified: global console
power and observed DXGI occlusion are handled. Settings are process-local until M3;
mirrored physical panels share a logical output. WorkerW coverage/topology remains
conservative and can be refused. Windows multi-output timing and hybrid-GPU power
behavior are unverified; see M2_TEST_PLAN.md and PERFORMANCE.md.

## M3 vertical slice

- Dynamically linked Qt Quick Manager with Library, Monitors, Performance and
  Settings, Russian/English, static previews, search/favorites/sort/details,
  local import/delete/open folder, per-output/all-output apply and pause.
- Core-owned SQLite metadata/favorites/import history/assignments and versioned
  JSON settings; atomic replacement/backup and recovery warning. Imports create
  validated managed snapshots and leave external sources intact.
- Versioned bounded JSON protocol, strict framing/fields, explicit responses,
  one overlapped pipe worker and main-thread runtime dispatch. Windows pipe has
  a logon-SID DACL, first-instance and remote-client rejection; Manager verifies
  server session/logon and uses identification QoS.
- All-output apply stages every upload before committing assignments and prepared
  renderer states. Rejected scenes and active/assigned deletion preserve old state.
- Core tray, Manager open/pause/reload/complete-exit actions and Explorer re-add;
  Manager close sends no stop command. Opt-in HKCU autostart; service restores state.
- Qt/shared-build guards, development deployment script, original application icon,
  portable persistence/protocol tests and Manager-to-ControlService tests.

This is the requested minimal compilable vertical slice. Data-root/cache migration
controls, asynchronous import jobs, pagination above 128 local scenes and Windows
acceptance remain follow-up tasks. The fixed pipe endpoint cannot host parallel
Core servers from different interactive sessions. Import can briefly delay rendering.

## Local verification (Linux)

| Check | Status | Evidence / limitations |
| --- | --- | --- |
| Portable C++ build | PASS | CMake portable-tests; GCC 16.2.1 |
| Unit tests | PASS | 68 tests: CLI, clocks, recovery, package/parser, paths/budgets, geometry, redraw, monitor assignment, pause, metrics, recovery storms, IPC validation, SQLite/import, settings recovery |
| Address/undefined-behavior sanitizer run | PASS | Same 68 tests; leak detection disabled because LeakSanitizer cannot run under this traced environment |
| Bundled manifests/packages | PASS | All six exercised by C++ tests |
| Public JSON Schema and SceneCheck CLI | PASS | All six directories and separately generated .awscene archives validated |
| HLSL syntax/translation | PASS | All four shaders compiled with glslang to SPIR-V; this is not Microsoft fxc/SM5 acceptance |
| Qt Manager build | PASS | Qt 6.12.0 shared on Linux; C++/QML cache compilation |
| Manager control/persistence tests | PASS | 70 CTest entries pass including Manager and catalog Qt suites; within that suite the actual socket case is SKIPPED because sandbox forbids Unix sockets |
| Headless Manager captures | PASS | Four screens, RU/EN, 1180×780 and 900×620; explicitly labelled test transport, no GPU simulation |
| Catalog ASGI/publication tests | PASS | 12 tests; SQLite/local objects and real SceneCheck; no PostgreSQL/S3/TLS deployment claim |
| Windows pipe/tray/autostart/deployment | NOT TESTED | Needs real Windows execution and ACL/session tests |
| MSVC/fxc M1–M4 Core build and WIC/GPU rendering | NOT TESTED | No Windows SDK or interactive Windows session here |
| Actual static Present count / transactional GPU reload | NOT TESTED | Follow M1_TEST_PLAN.md on Windows |
| M0 desktop/input/recovery regressions | NOT TESTED | Needs actual Windows logs and results |
| Windows build workflow | USER-REPORTED PASS | User reported successful Windows jobs after the notice/min-macro fixes; no local Windows execution |
| Automatic release upload | NOT TESTED | New publish job and orchestration tests prepared; requires updated workflow on GitHub |

Local libraries: nlohmann/json 3.11.3 (existing system header), libarchive 3.8.10,
libwebp 1.6.0, zlib, GoogleTest 1.18.0. On this host nlohmann/json is in a nonstandard
system location; configure with
`cmake --preset portable-tests -DAETHERWALL_JSON_INCLUDE_DIR=/usr/include/cudnn_frontend/thirdparty`.
Other Linux hosts can install the normal development package. Windows uses the
pinned vcpkg registry rather than these host-specific versions.

## Next acceptance step

Run Bootstrap-Windows.ps1 again, configure/build/CTest Debug and Release, then
follow M1_TEST_PLAN.md, M2_TEST_PLAN.md and M3_TEST_PLAN.md: all six scenes, ZIP parity, corrupt-image F5 reload,
static Present count, transforms, image formats and desktop regressions.
Record actual OS/GPU/driver/build/source identity and compatibility results.
Complete the multi-monitor/pause/recovery matrix and PERFORMANCE.md on integrated
and discrete/hybrid Windows hardware. Do not mark M2 accepted from Linux tests.

## Remaining roadmap

- M2 remaining acceptance: Windows build, mixed-DPI dual-monitor rendering/recovery,
  hybrid GPU and real performance measurements; independent panel-sleep detection
  remains a documented limitation.
- M3 follow-up: Windows build/pipe/tray acceptance, data-root/cache migration UI,
  background imports and local-library paging.
- M4 acceptance: Windows download/import, real PostgreSQL/S3/HTTPS deployment,
  provider/hosting decisions and measured popularity/trending.
- M5 remaining acceptance: Windows CI/ISCC execution, install/upgrade/uninstall,
  actual runtime dependency/license audit, source distribution, signing decision,
  hardware validation and complete release AC matrix.
