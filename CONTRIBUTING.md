# Contributing to AetherWall

Thank you for helping improve the project. Read our [Code of Conduct](CODE_OF_CONDUCT.md)
and check existing issues before proposing work. Documentation is primarily English;
Russian and Uzbek README translations are welcome.

## Bugs, ideas, and security

Use the [issue forms](https://github.com/Ordo-xsenos/open_wallpaper/issues/new/choose).
For bugs include the build/commit, Windows build, GPU and driver, monitor/DPI layout,
reproduction steps, expected behavior, and relevant logs with personal data removed.
Separate diagnostic window behavior from desktop integration behavior.
Follow [SECURITY](SECURITY.md) for vulnerabilities; do not disclose them in public issues.

## Development setup

Core requires Windows x64, VS 2022 MSVC, Windows SDK/fxc, CMake 3.28+, Ninja, and
the pinned vcpkg setup. Manager also requires shared Qt. See
[development instructions](docs/DEVELOPMENT.md) and [release builds](docs/RELEASE.md).
Linux can build portable logic and Manager for development, but cannot establish
Windows desktop compatibility.

From x64 Developer PowerShell with a shared Qt kit:

```powershell
.\tools\Bootstrap-Windows.ps1
cmake --preset win-manager-debug -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
cmake --build --preset win-manager-debug
ctest --preset win-manager-debug --output-on-failure
```

On Linux with the dependencies in the development guide:

```sh
cmake --preset portable-tests
cmake --build --preset portable-tests
ctest --preset portable-tests --output-on-failure
python -m pip install './server[test]'
python -m pytest tests/release server/tests -q
```

## Code and tests

Use C++20, four-space indentation, `PascalCase` types, `camelCase` functions and
variables, RAII, and the repository's `.clang-format`. Keep Windows APIs out of
portable policy code. Check HRESULT/Win32 errors and preserve the active scene
when a replacement is rejected. Core owns the database and rendering; Manager
uses IPC. Keep Qt runtime dependencies configured for CTest, not dependent on a
developer's global PATH.

Run checks relevant to your change. Scene/parser/storage changes need invalid-input
and rollback cases; runtime packaging needs clean-PATH checks. For Windows Shell,
GPU, recovery, and performance changes, follow the relevant test plan in `docs/`.
Record actual PASS/FAIL/NOT TESTED with OS/GPU/driver/source identity. A Linux test
or successful build does not prove Windows compatibility.

## Pull requests

- Keep changes focused; use a short imperative commit subject.
- Describe the problem, resulting behavior, related issue, tests and limitations.
- Add screenshots for UI changes and logs for rendering/Shell changes.
- Keep all three README versions consistent when changing public-facing behavior.
- Do not commit credentials, user libraries, downloaded runtimes, or build outputs.

New scenes/artwork must be original or explicitly authorized. Include authorship,
license/source and rights evidence. Do not copy Steam Workshop, Wallpaper Engine,
or other creators' work without permission. Changes to dependencies must retain
their license notices and corresponding source obligations.

The unified CI checks Linux policies/server/packaging and Windows Debug/Release.
Pull requests never publish releases; publication follows successful `main` builds.
