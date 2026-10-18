[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$projectRoot = Split-Path -Parent $PSScriptRoot
$manifest = Get-Content -Raw (Join-Path $projectRoot 'vcpkg.json') | ConvertFrom-Json
$baseline = $manifest.'builtin-baseline'
if ($baseline -notmatch '^[0-9a-f]{40}$') { throw 'Invalid vcpkg manifest baseline.' }
$vcpkgPath = Join-Path $projectRoot 'out/tools/vcpkg'

foreach ($tool in @('git', 'cmake', 'ninja', 'cl')) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw "Missing $tool. Run in x64 Developer PowerShell with CMake and Ninja on PATH."
    }
}
if ($env:VSCMD_ARG_TGT_ARCH -ne 'x64') {
    throw 'Use an x64 MSVC developer environment (VSCMD_ARG_TGT_ARCH=x64).'
}

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $vcpkgPath) | Out-Null
if (-not (Test-Path $vcpkgPath)) {
    & git clone --filter=blob:none --no-checkout https://github.com/microsoft/vcpkg.git $vcpkgPath
    if ($LASTEXITCODE -ne 0) { throw 'vcpkg clone failed.' }
    & git -C $vcpkgPath checkout --detach $baseline
    if ($LASTEXITCODE -ne 0) { throw 'vcpkg checkout failed.' }
} else {
    if (-not (Test-Path (Join-Path $vcpkgPath '.git'))) {
        throw "$vcpkgPath already exists and is not a vcpkg checkout. Use a clean output directory."
    }
    $actual = & git -C $vcpkgPath rev-parse HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Cannot identify existing vcpkg checkout.' }
    if ($actual.Trim() -ne $baseline) {
        $remote = & git -C $vcpkgPath remote get-url origin
        if ($LASTEXITCODE -ne 0 -or $remote.Trim() -ne 'https://github.com/microsoft/vcpkg.git') {
            throw 'Unexpected vcpkg origin. Existing checkout was left unchanged.'
        }
        $dirty = & git -C $vcpkgPath status --porcelain --untracked-files=no
        if ($LASTEXITCODE -ne 0 -or $dirty) { throw 'vcpkg contains local changes; left unchanged.' }
        & git -C $vcpkgPath fetch --filter=blob:none origin $baseline
        if ($LASTEXITCODE -ne 0) { throw 'Cannot fetch pinned vcpkg baseline.' }
        & git -C $vcpkgPath checkout --detach $baseline
        if ($LASTEXITCODE -ne 0) { throw 'Cannot update vcpkg baseline; no reset was performed.' }
    }
}

& (Join-Path $vcpkgPath 'bootstrap-vcpkg.bat') -disableMetrics
if ($LASTEXITCODE -ne 0) { throw 'vcpkg bootstrap failed.' }
$env:VCPKG_ROOT = $vcpkgPath
$env:VCPKG_DISABLE_METRICS = '1'
Write-Host "VCPKG_ROOT=$env:VCPKG_ROOT"
Write-Host 'Ready: cmake --preset win-msvc-debug'
