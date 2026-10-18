# Dependency materials for the preview

Exact application version and Qt input are in `packaging/release.json`; the payload
manifest records source revision and every shipped file's size/SHA-256. The vcpkg
registry baseline and static-MD dependencies are pinned in `vcpkg.json`. Preserve
the collected `licenses/` tree when sharing binaries.

Qt is dynamically linked and replaceable. The internal release workflow collects
license/copyright/attribution files from clean `v6.8.3` Qt source checkouts and
creates companion source ZIPs. `licenses/Qt/SOURCES.json` records module commits.
This collection includes module-wide terms and third-party source attributions;
the exact deployed DLL/plugin inventory still needs a redistribution review.
Ensure notices for any additional Qt plugin module are supplied before publication.

Canonical corresponding sources:

- [Qt 6.8.3 source submodules](https://download.qt.io/archive/qt/6.8/6.8.3/submodules/).
- [Qt licensing and obligations](https://doc.qt.io/qt-6/licensing.html).
- Application source: companion `AetherWall-M5.zip`, with `SOURCE_SHA256SUMS`.
- Other native dependencies: upstream sources/notices identified in
  THIRD_PARTY_NOTICES.md and copied vcpkg `licenses/<port>/copyright` files.

The builder must retain and, when required, supply exact corresponding source and
build/replacement instructions alongside the binary distribution. CI artifacts
expire after 14 days; they are not a permanent public source offer. Before a public
release, host the companion application and Qt sources durably with that release,
check all deployed third-party notices and satisfy the selected Qt license terms.
No public publication or completed legal review is claimed by this pipeline.

MSVC runtime is copied only from `%VCToolsRedistDir%\x64\Microsoft.VC143.CRT`,
the VS 2022 redistributable directory. No developer/debug CRT or System32 DLLs are
copied. The builder must hold applicable Visual Studio redistribution rights and
retain the installed Microsoft terms. This document identifies the runtime source;
it does not replace those terms. App-local runtime servicing requires repackaging
when Microsoft publishes runtime fixes. Microsoft documents both
[redistribution rules](https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files)
and [app-local deployment](https://learn.microsoft.com/en-us/cpp/windows/walkthrough-deploying-a-visual-cpp-application-to-an-application-local-folder).

SChannel provides Windows TLS using the system certificate store; no OpenSSL or
Python/server runtime is included by the desktop installer. Standard Qt deployment
may include additional Qt DLLs/plugins: inspect the final payload, not this summary,
to establish the complete dependency/license inventory.
