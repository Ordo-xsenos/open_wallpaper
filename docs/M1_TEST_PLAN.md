# M1 acceptance tests

M0 ran on the user's friend's Windows computer after fixing the reserved HLSL
identifier `point`. The screenshot shows diagnostic window output; no complete
desktop/input/Explorer-restart matrix was supplied. The user explicitly authorized
M1 on 2026-10-17. Retain the missing M0 checks as NOT TESTED, rather than inferring
their results. Re-run the [M0 scenarios](TEST_PLAN.md) after this renderer change.

## Build and automated tests

Run bootstrap again (it upgrades the clean project-local vcpkg checkout), then
configure/build/CTest for Debug and Release as described in README. There are
portable tests for schema and metadata rejection, Unicode/duplicate keys, budgets,
path traversal/case collisions, compressed packages, bad image dimensions, scene
ordering/animation classification, geometry, and demand-driven rendering.

Validate every bundled directory with `AetherWall.SceneCheck`. Its success covers
package/header validation; the real Core must still decode pixels and draw them.

## Windows manual cases (all NOT TESTED locally)

| Case | Steps | Required result |
| --- | --- | --- |
| Scene examples | Run each of stars/rain/gradient/mountains/minimal_particles in window mode | Correct image layers and animated particles/gradient; no compiler or D3D validation errors |
| Static scene / AC-05 | Run `still`, leave untouched for 20 s | After initial redraws, `totalDrawPresents` stops increasing and `intervalPresented=0`; no normal Present calls in ETW |
| Static invalidation | Resize/restore window, change DPI, then wait again | One redraw as needed, followed by stable Present count |
| ZIP parity | Package stars as documented, run directory then `.awscene` | Same scene and animation; no extracted files |
| Transactional reload | Copy mountains to `out/test-scene`, start with `--scene out/test-scene`; corrupt manifest, press F5; then restore, F5 | Invalid version keeps prior scene; restored scene commits without restarting Core |
| Pixel decoding failure | After a successful image scene, truncate an image after its PNG header, F5 | Decoder rejects it, old GPU textures remain active, error is logged |
| JPEG and WebP | Replace an image source in a copied scene with a valid JPEG/static WebP; F5; then test damaged copies | Supported images decode; damaged/animated/oversized data is rejected |
| Color/alpha | Use transparent front mountain layer and tint/opacity 0.5 | Transparent regions reveal lower layers; no opaque black rectangle |
| Transform/fit | Edit position/rotation/scale and cover/contain/stretch; F5 | Correct bounds, crop/letterbox, rotation and ordering |
| Scene recovery | Run images/particles in desktop mode; restart Explorer once | Same committed scene restored, or safe bounded refusal; rerun desktop input checks |
| Pause | Lock/unlock or minimize/restore animated window | No active rendering while paused; animation time does not jump by the pause duration |

Use `tools/Test-M0.ps1 -Mode window -Scene scenes/bundled/still -Duration 30`
for an evidence folder, and repeat in desktop mode after window validation.
Record exact OS build, GPU/driver, monitor resolution/DPI, actual shader/swap path,
source hashes/revision, logs, results and limitations. A fallback diagnostic
gradient after a scene error does not count as a successful scene load.

M1 is not accepted until the Windows build, pixel decoding/rendering, replacement
failure behavior, static Present behavior and M0 regression cases have been run.
M2 sources were subsequently started with explicit user authorization; run
M2_TEST_PLAN.md in addition to these pending M1 cases.
