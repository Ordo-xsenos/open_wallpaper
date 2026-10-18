# Repository Guidelines

## Project Structure & Module Organization

`WINDOWS11_WALLPAPER_ENGINE_SPEC.md` defines AetherWall, a Windows 11 x64 wallpaper engine. Development currently targets M0: desktop integration with a GPU gradient. Do not begin M1 until M0 passes on a real Windows 11 24H2+ desktop.

`apps/core/` contains the executable entry point. `src/desktop/` isolates experimental Shell integration; `src/graphics/` contains Direct3D 11 rendering; `src/system/` contains portable options, timing, and recovery policies; `src/diagnostics/` contains logging. Trusted HLSL lives in `shaders/`, GoogleTest tests in `tests/unit/`, and compatibility reports in `docs/`. `tools/` contains Windows verification scripts. Qt Manager and the catalog are future milestones.

## Build, Test, and Development Commands

Use x64 Developer PowerShell with MSVC, Windows SDK, CMake 3.28+, Ninja, and the vcpkg setup documented in `README.md`.

- `cmake --preset win-msvc-debug`: configure the Windows Debug build.
- `cmake --build --preset win-msvc-debug`: compile Core, embedded shaders, and tests.
- `ctest --preset win-msvc-debug --output-on-failure`: run policy tests.
- `cmake --preset portable-tests`: configure Linux policy tests; this does not build Core.
- `cmake --build --preset portable-tests` and `ctest --preset portable-tests`: verify portable logic.

Start Core with `--mode window` before testing `--mode desktop`. Both default to a finite diagnostic run. See `docs/TEST_PLAN.md` for manual checks.

## Coding Style & Naming Conventions

Use C++20, four-space indentation, `PascalCase` types, `camelCase` functions and variables, and matching `.h`/`.cpp` names. Keep Windows APIs outside portable policy code. Use RAII and `Microsoft::WRL::ComPtr`; check HRESULTs and Win32 failures. Formatting is configured in `.clang-format`.

## Testing Guidelines

Use GoogleTest suites named after the behavior under test. Exercise invalid arguments, recovery exhaustion, and timing boundaries. No percentage coverage threshold is defined. Record actual Windows results as `PASS`, `FAIL`, or `NOT TESTED`, including OS, GPU, driver, logs, and source revision. Linux tests cannot establish desktop compatibility.

## Commit & Pull Request Guidelines

No Git history is available yet. Use short imperative subjects, such as `Add bounded desktop recovery`. Describe behavior, milestone, tests performed, and remaining limitations. Include screenshots or diagnostic logs for rendering and Shell changes; link relevant acceptance criteria or issues.
