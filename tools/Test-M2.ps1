[CmdletBinding()]
param(
    [ValidateSet('win-msvc-debug', 'win-msvc-release')]
    [string]$BuildPreset = 'win-msvc-release',
    [ValidateSet('window', 'desktop')]
    [string]$Mode = 'window',
    [ValidateSet('eco', 'balanced', 'smooth')]
    [string]$PerformancePreset = 'balanced',
    [ValidateSet('auto', 'power-saving', 'high-performance')]
    [string]$Gpu = 'auto',
    [ValidateRange(10, 86400)]
    [int]$Duration = 120,
    [string]$Scene = 'scenes/bundled/stars',
    [hashtable]$MonitorScenes = @{},
    [ValidateSet('on', 'off')][string]$PauseFullscreen = 'on',
    [ValidateSet('on', 'off')][string]$PauseMaximized = 'off',
    [ValidateSet('on', 'off')][string]$PauseBattery = 'on'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$projectRoot = Split-Path -Parent $PSScriptRoot
$BuildPreset = $BuildPreset.ToLowerInvariant()
$Mode = $Mode.ToLowerInvariant()
$PerformancePreset = $PerformancePreset.ToLowerInvariant()
$Gpu = $Gpu.ToLowerInvariant()
$executable = Join-Path $projectRoot "out/build/$BuildPreset/AetherWall.Core.exe"
if (-not (Test-Path -LiteralPath $executable)) { throw "Build $BuildPreset first." }
$stamp = (Get-Date).ToUniversalTime().ToString('yyyyMMdd_HHmmss_fff')
$report = Join-Path $projectRoot "out/reports/${stamp}_m2_${Mode}_${PerformancePreset}"
New-Item -ItemType Directory -Force -Path (Join-Path $report 'logs') | Out-Null

# Start-Process joins its argument array into one command line on Windows.
# Quote embedded quotes and trailing backslashes for native argv parsing.
function Quote-NativeArgument([string]$Value) {
    return '"' + [regex]::Replace([regex]::Replace($Value, '(\\*)"', '$1$1\"'), '(\\+)$', '$1$1') + '"'
}
Push-Location $projectRoot
try {
    & ctest --preset $BuildPreset --output-on-failure --no-tests=error --output-log (Join-Path $report 'ctest.log')
    if ($LASTEXITCODE -ne 0) { throw "CTest failed. Report: $report" }
    & $executable --list-monitors --log-dir (Join-Path $report 'logs') |
        Set-Content -Encoding UTF8 (Join-Path $report 'monitors.txt')
    if ($LASTEXITCODE -ne 0) { throw 'Monitor enumeration failed.' }
    $arguments = @('--mode', $Mode, '--all-monitors', '--preset', $PerformancePreset,
        '--gpu', $Gpu, '--duration', "$Duration", '--scene', (Resolve-Path -LiteralPath $Scene).Path,
        '--pause-fullscreen', $PauseFullscreen.ToLowerInvariant(),
        '--pause-maximized', $PauseMaximized.ToLowerInvariant(),
        '--pause-battery', $PauseBattery.ToLowerInvariant(), '--log-dir', (Join-Path $report 'logs'))
    foreach ($selector in ($MonitorScenes.Keys | Sort-Object)) {
        $arguments += @('--monitor-scene', [string]$selector, (Resolve-Path -LiteralPath $MonitorScenes[$selector]).Path)
    }
    [ordered]@{
        capturedAtUtc = (Get-Date).ToUniversalTime().ToString('o')
        os = Get-CimInstance Win32_OperatingSystem | Select-Object Caption, Version, BuildNumber, OSArchitecture
        gpu = @(Get-CimInstance Win32_VideoController | Select-Object Name, DriverVersion, PNPDeviceID)
        arguments = $arguments
        buildPreset = $BuildPreset
        performanceResults = 'NOT TESTED: complete RESULTS.md after inspection and measurement'
    } | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 (Join-Path $report 'environment.json')
    $sourceFiles = @()
    foreach ($directory in @('apps', 'src', 'shaders', 'tests', 'tools', 'scenes', 'docs')) {
        $sourceFiles += Get-ChildItem -LiteralPath (Join-Path $projectRoot $directory) -Recurse -File
    }
    foreach ($filename in @('CMakeLists.txt', 'CMakePresets.json', 'vcpkg.json')) {
        $sourceFiles += Get-Item -LiteralPath (Join-Path $projectRoot $filename)
    }
    $sourceFiles | Sort-Object FullName | ForEach-Object {
        [pscustomobject]@{
            path = $_.FullName.Substring($projectRoot.Length + 1)
            sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash
        }
    } | Export-Csv -NoTypeInformation -Encoding UTF8 (Join-Path $report 'source-hashes.csv')
    Get-FileHash -Algorithm SHA256 -LiteralPath $executable |
        Format-List | Out-File -Encoding UTF8 (Join-Path $report 'binary-hash.txt')
    Write-Host "Running $Mode on all outputs for $Duration seconds. Report: $report"
    $nativeArguments = @($arguments | ForEach-Object { Quote-NativeArgument $_ })
    $process = Start-Process -FilePath $executable -ArgumentList $nativeArguments -NoNewWindow -Wait -PassThru `
        -RedirectStandardOutput (Join-Path $report 'stdout.txt') `
        -RedirectStandardError (Join-Path $report 'stderr.txt')
    @"
# M2 Windows verification

Core exit code: $($process.ExitCode). Build: $BuildPreset; mode: $Mode.
Preset: $PerformancePreset; GPU preference: $Gpu. Duration: $Duration seconds.
Identity and exact arguments: environment.json, source-hashes.csv, binary-hash.txt.
Display paths: monitors.txt; bounds, refresh and adapter selection: logs/.
CTest: PASS (ctest.log). Exit 0 alone does not prove visual or performance correctness.

| Check | Status | Evidence |
| --- | --- | --- |
| Independent scenes / negative coordinates / mixed DPI | NOT TESTED | Record each display resolution, scale, refresh and screenshot |
| Shared adapter/textures; hybrid GPU preference | NOT TESTED | Record actual adapter LUID and ETW/VRAM evidence |
| Per-output fullscreen / maximized / Space pause-resume | NOT TESTED | Follow docs/M2_TEST_PLAN.md |
| Battery / lock / suspend / console display-off | NOT TESTED | Include pauseReasons and stable Present counters |
| Hot-plug / rotation / refresh / Explorer / device reset | NOT TESTED | Same scene restored or safe refusal; no overlay |
| Static output Present count remains unchanged | NOT TESTED | Five-second logs, without expose/resize events |
| 15/30/60 FPS, avg/P95, CPU, memory, available GPU timing | NOT TESTED | Complete docs/PERFORMANCE.md with ETW/PresentMon traces |
| M0/M1 regression and clean shutdown | NOT TESTED | Include WIC/WebP, F5 rejection and desktop input |

Overall M2 acceptance: NOT TESTED.
Individual panel sleep is not identified by the console-wide power event;
record real per-output occlusion behavior and this limitation.
"@ | Set-Content -Encoding UTF8 (Join-Path $report 'RESULTS.md')
    Write-Host "Exit code: $($process.ExitCode). Complete $report/RESULTS.md manually."
    if ($process.ExitCode -ne 0) { throw "Core failed or safely refused this topology. See $report/logs." }
} finally {
    Pop-Location
}
