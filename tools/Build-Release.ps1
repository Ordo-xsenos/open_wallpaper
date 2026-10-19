[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$QtRoot,
    [Parameter(Mandatory=$true)][string]$QtNoticesRoot,
    [string]$Iscc = "${env:ProgramFiles(x86)}/Inno Setup 6/ISCC.exe",
    [string]$Revision = 'unversioned-source'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$QtRoot = [IO.Path]::GetFullPath($QtRoot)
$QtNoticesRoot = [IO.Path]::GetFullPath($QtNoticesRoot)
$Iscc = [IO.Path]::GetFullPath($Iscc)
$root = Split-Path -Parent $PSScriptRoot
$metadata = Get-Content -Raw (Join-Path $root 'packaging/release.json') | ConvertFrom-Json
$build = Join-Path $root 'out/build/win-msvc-release'
$release = Join-Path $root 'out/release'
$stage = Join-Path $root ('out/release-stage/' + [guid]::NewGuid().ToString('N'))

foreach ($path in @($Iscc, (Join-Path $QtRoot 'bin/qmake.exe'), (Join-Path $QtNoticesRoot 'SOURCES.json'))) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing release input: $path" }
}
$qtVersion = & (Join-Path $QtRoot 'bin/qmake.exe') -query QT_VERSION
if ($LASTEXITCODE -ne 0 -or $qtVersion.Trim() -ne $metadata.qtVersion) { throw "Release requires Qt $($metadata.qtVersion)." }
$cache = Get-Content -Raw (Join-Path $build 'CMakeCache.txt')
if ($cache -notmatch '(?m)^CMAKE_BUILD_TYPE:STRING=Release\r?$' -or $cache -notmatch '(?m)^AETHERWALL_BUILD_MANAGER:BOOL=ON\r?$') {
    throw 'Configure win-manager-release first; Debug binaries must never be packaged.'
}
$qtCache = [regex]::Match($cache, '(?m)^Qt6_DIR:PATH=([^\r\n]+)')
if (-not $qtCache.Success) { throw 'Cannot identify the Qt kit used by CMake.' }
$builtQt = [IO.Path]::GetFullPath($qtCache.Groups[1].Value)
$expectedQt = [IO.Path]::GetFullPath((Join-Path $QtRoot 'lib/cmake/Qt6'))
if (-not [String]::Equals($builtQt, $expectedQt, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'CMake was configured against another Qt kit; reconfigure with the release QtRoot.'
}
$crt = Join-Path $env:VCToolsRedistDir 'x64/Microsoft.VC143.CRT'
if (-not (Test-Path -LiteralPath $crt -PathType Container)) { throw 'VS 2022 x64 redistributable CRT directory is required (VCToolsRedistDir).' }
Push-Location $root
try {
    & cmake --build --preset win-manager-release
    if ($LASTEXITCODE -ne 0) { throw 'Release compilation failed.' }
    $qtTestDirectory = Join-Path $build 'apps/manager'
    Get-ChildItem -LiteralPath $qtTestDirectory -Filter '*-results.txt' -File | Remove-Item
    & ctest --preset win-manager-release --output-on-failure --no-tests=error
    if ($LASTEXITCODE -ne 0) {
        # Keep Qt assertions visible even when Windows does not capture stdout.
        Get-ChildItem -LiteralPath $qtTestDirectory -Filter '*-results.txt' -File | ForEach-Object {
            Get-Content -LiteralPath $_.FullName | ForEach-Object { Write-Host $_ }
        }
        $report = Join-Path $root 'out/reports'
        New-Item -ItemType Directory -Force -Path $report | Out-Null
        $testExe = Join-Path $build 'apps/manager/aetherwall_catalog_tests.exe'
        if ((Get-Command dumpbin -ErrorAction SilentlyContinue) -and (Test-Path -LiteralPath $testExe)) {
            & dumpbin /DEPENDENTS $testExe | Out-File (Join-Path $report 'release-dependencies.txt') -Encoding utf8
        }
        throw 'Release CTest failed. Inspect catalog-results.txt, CTest logs and out/reports/release-dependencies.txt.'
    }
    & (Join-Path $PSScriptRoot 'Deploy-Manager.ps1') -Preset win-msvc-release -QtRoot $QtRoot -StageDirectory $stage
    Set-Content -LiteralPath (Join-Path $stage 'qt.conf') -Encoding ascii -Value "[Paths]`nPrefix=.`nPlugins=.`nQmlImports=qml"
    Copy-Item -LiteralPath $QtNoticesRoot -Destination (Join-Path $stage 'licenses/Qt') -Recurse
    # Use only the officially redistributable CRT, never DLLs from System32 or bin/Host*.
    Get-ChildItem -LiteralPath $crt -Filter '*.dll' -File | Copy-Item -Destination $stage
    New-Item -ItemType Directory -Force -Path (Join-Path $stage 'tls'),(Join-Path $stage 'licenses/Microsoft') | Out-Null
    Copy-Item -LiteralPath (Join-Path $QtRoot 'plugins/tls/qschannelbackend.dll') -Destination (Join-Path $stage 'tls') -Force
    Copy-Item -LiteralPath (Join-Path $root 'docs/DEPENDENCY_SOURCES.md') -Destination (Join-Path $stage 'licenses/Microsoft/Redistribution.md')
    foreach ($name in @('PREVIEW_NOTES.md','DEPENDENCY_SOURCES.md')) {
        Copy-Item -LiteralPath (Join-Path $root "docs/$name") -Destination $stage
    }
    foreach ($name in @('AetherWall-Debug.cmd','Start-AetherWallDebug.ps1','Collect-AetherWallDiagnostics.ps1')) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $stage
    }
    Copy-Item -LiteralPath (Join-Path $root 'docs/DEBUGGING.md') -Destination $stage
    & (Join-Path $PSScriptRoot 'Test-ReleasePayload.ps1') -PayloadDirectory $stage -ReportDirectory (Join-Path $root 'out/reports/release-smoke')
    & python (Join-Path $PSScriptRoot 'Release-Bundle.py') seal --payload $stage --revision $Revision
    if ($LASTEXITCODE -ne 0) { throw 'Release payload validation failed.' }
    New-Item -ItemType Directory -Force $release | Out-Null
    $portable = Join-Path $release "AetherWall-$($metadata.version)-win-x64.zip"
    & python (Join-Path $PSScriptRoot 'Release-Bundle.py') zip --payload $stage --output $portable
    if ($LASTEXITCODE -ne 0) { throw 'Portable ZIP failed.' }
    & python (Join-Path $PSScriptRoot 'Release-Bundle.py') verify --payload $stage
    if ($LASTEXITCODE -ne 0) { throw 'Staging changed before installer compilation.' }
    & $Iscc "/DPayloadDir=$stage" "/DReleaseVersion=$($metadata.version)" "/DReleaseOutput=$release" (Join-Path $root 'packaging/windows/AetherWall.iss')
    if ($LASTEXITCODE -ne 0) { throw 'Inno Setup compilation failed.' }
    $installer = Join-Path $release "AetherWall-$($metadata.version)-win-x64-setup.exe"
    $hash = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash.ToLowerInvariant()
    Set-Content -LiteralPath "$installer.sha256" -Encoding ascii -Value "$hash  $([IO.Path]::GetFileName($installer))"
    Write-Host "Internal preview artifacts: $release"
    Write-Host "Validated staging retained for inspection: $stage"
    Write-Host 'Windows hardware/install/uninstall acceptance and public release approval are still required.'
} finally {
    Pop-Location
}
