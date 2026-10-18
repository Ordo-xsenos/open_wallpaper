# Third-party notices

Core links Windows SDK libraries and the M1 dependencies listed below, and embeds original HLSL.
It contains no Wallpaper Engine or Lively Wallpaper code or assets.

## GoogleTest (tests only)

GoogleTest is obtained separately through vcpkg on Windows or a system package
for Linux policy tests. It is BSD-3-Clause licensed, copyright Google Inc.
Its full upstream license must accompany any distributed test binary:
https://github.com/google/googletest/blob/main/LICENSE

The pinned Windows vcpkg registry baseline is
`9e593bb18ea69cc5095e012465dcd675a822ed0d` (release 2026.07.29).
The installed license is available in the build tree under
`vcpkg_installed/x64-windows-static-md/share/gtest/copyright`.
Core does not link GoogleTest.

## M1 runtime dependencies

- nlohmann/json: MIT; copyright Niels Lohmann and contributors.
  https://github.com/nlohmann/json/blob/develop/LICENSE.MIT
- libarchive: BSD licenses; preserve its full bundled notices, not just this summary.
  https://github.com/libarchive/libarchive/blob/master/COPYING
- libwebp: BSD-3-Clause with the accompanying patent grant.
  https://github.com/webmproject/libwebp/blob/main/COPYING and
  https://github.com/webmproject/libwebp/blob/main/PATENTS
- zlib (ZIP compression): zlib license.
  https://github.com/madler/zlib/blob/develop/LICENSE

Windows builds copy installed vcpkg copyright files to `licenses/` beside Core.
Redistribute that directory with binaries, including all transitive dependency
notices. Source bundles do not contain these external library sources/binaries.
Bundled original artwork/manifests use CC0-1.0; see `scenes/README.md`.

## Microsoft SDK and build tools

MSVC, Windows SDK, and the HLSL compiler are external build prerequisites under
their respective Microsoft licenses; their sources are not included here.

## M4 external dependencies

- FastAPI: MIT, [upstream license](https://github.com/fastapi/fastapi/blob/master/LICENSE).
- Uvicorn: BSD-3-Clause, [upstream license](https://github.com/Kludex/uvicorn/blob/main/LICENSE.md).
- psycopg (optional PostgreSQL adapter): LGPL-3.0;
  [full terms](https://github.com/psycopg/psycopg/blob/master/LICENSE.txt).
- boto3 (optional S3 adapter): Apache-2.0,
  [upstream license](https://github.com/boto/boto3/blob/develop/LICENSE).
- Linux development SHA-256 uses the externally installed OpenSSL Crypto library;
  Windows uses the system BCrypt provider. Preserve the installed OpenSSL notices
  if distributing Linux development binaries.

Python dependencies and transitive packages are installed externally, not vendored
in the source ZIP. Before distributing a server/container or desktop release,
collect notices from the exact installed versions, including Starlette, Pydantic,
botocore, libpq/binary wheels, Qt TLS plugins and test-only dependencies as applicable.
The current dependency ranges are development constraints, not a frozen production
release audit. The MIT project license does not relicense these components.

## M3 storage and Manager

- SQLite: upstream places its code in the public domain; preserve the installed
  vcpkg notice with distributed binaries. [Upstream copyright statement](https://www.sqlite.org/copyright.html).
- Qt 6: external shared installation, linked only by Manager. Qt offers commercial
  and open-source licensing; individual modules and bundled third-party libraries
  have their own terms. See [Qt licensing](https://doc.qt.io/qt-6/licensing.html)
  and the exact installation's license/source materials. This project uses Qt
  Core, Gui, Network, Concurrent, Qml, Quick, Quick Controls and Quick Dialogs; Qt Test is used
  only for development. Static Qt is rejected by the Manager CMake target.
- Qt deployment must retain applicable license texts, notices and the materials
  required by the selected Qt license, including replacement/relinking and source
  availability obligations where applicable. Deploy-Manager.ps1 stages runtime
  dependencies and available license texts; it is not a completed release audit.

The original AetherWall application/tray icon is authored SVG geometry in
apps/manager/assets/aetherwall.svg; the PNG and ICO are derived from it. Both use the
project's MIT license. No third-party logo/icon artwork is embedded.

## M5 preview distribution

Release staging preserves vcpkg notices and collects Qt module/third-party license
files from the exact source tags recorded in `licenses/Qt/SOURCES.json`. Dynamic
Qt, SChannel TLS and official VS redistributable CRT are staged app-locally. See
[dependency source materials](docs/DEPENDENCY_SOURCES.md) for exact-source companion
requirements and Microsoft redistribution references. This source change is not a
completed audit of an executable installer; that audit remains a public-release gate.
