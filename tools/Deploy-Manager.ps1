[CmdletBinding()]
param(
    [ValidateSet('win-msvc-debug','win-msvc-release')][string]$Preset = 'win-msvc-release',
    [Parameter(Mandatory=$true)][string]$QtRoot,
    [string]$StageDirectory
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root "out/build/$Preset"
$stage = if ($StageDirectory) { [IO.Path]::GetFullPath($StageDirectory) } else { Join-Path $root "out/deploy/$Preset" }
$manager = Join-Path $build 'apps/manager/AetherWall.Manager.exe'
$core = Join-Path $build 'AetherWall.Core.exe'
$deploy = Join-Path $QtRoot 'bin/windeployqt.exe'
foreach ($file in @($manager,$core,$deploy)) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing file: $file" }
}
if ((Test-Path -LiteralPath $stage) -and @(Get-ChildItem -LiteralPath $stage -Force).Count) {
    throw 'Deployment directory must be empty; stale DLLs are not merged into a release.'
}
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item -LiteralPath $manager,$core -Destination $stage -Force
$checker = Join-Path $build 'AetherWall.SceneCheck.exe'
Copy-Item -LiteralPath $checker -Destination $stage -Force
foreach ($directory in @('scenes','licenses')) {
    $source = Join-Path $build $directory
    if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination $stage -Recurse -Force }
}
$configuration = if ($Preset -eq 'win-msvc-debug') { '--debug' } else { '--release' }
& $deploy $configuration --qmldir (Join-Path $root 'apps/manager/qml') --no-translations --no-compiler-runtime (Join-Path $stage 'AetherWall.Manager.exe')
if ($LASTEXITCODE -ne 0) { throw 'windeployqt failed.' }
# Retain Qt's license texts; M5 must audit the complete redistribution bundle.
$qtLicenses = Join-Path $QtRoot 'Licenses'
if (Test-Path -LiteralPath $qtLicenses) {
    Copy-Item -LiteralPath $qtLicenses -Destination (Join-Path $stage 'Qt-Licenses') -Recurse -Force
}
Copy-Item -LiteralPath (Join-Path $root 'LICENSE'),(Join-Path $root 'THIRD_PARTY_NOTICES.md') -Destination $stage -Force
Write-Host "Staged Manager + Core: $stage"
Write-Host 'This is a development deployment, not an installer or a completed license audit.'
