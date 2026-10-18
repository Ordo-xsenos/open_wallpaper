[CmdletBinding()]
param(
    [ValidateSet('win-msvc-debug', 'win-msvc-release')]
    [string]$Preset = 'win-msvc-debug',
    [ValidateSet('window', 'desktop')]
    [string]$Mode = 'window',
    [ValidateSet(15, 30, 60)]
    [int]$Fps = 30,
    [ValidateRange(1, 86400)]
    [int]$Duration = 60,
    [ValidateSet('auto', 'flip', 'blt')]
    [string]$SwapChain = 'auto',
    [switch]$NoVsync,
    [switch]$Warp,
    [string]$Scene = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$Mode = $Mode.ToLowerInvariant()
$SwapChain = $SwapChain.ToLowerInvariant()
$Preset = $Preset.ToLowerInvariant()
if ($Warp -and $Mode -ne 'window') { throw 'WARP is only available in window mode.' }
$projectRoot = Split-Path -Parent $PSScriptRoot
$executable = Join-Path $projectRoot "out/build/$Preset/AetherWall.Core.exe"
if (-not (Test-Path $executable)) { throw "Build $Preset before running this script." }
$stamp = (Get-Date).ToUniversalTime().ToString('yyyyMMdd_HHmmss_fff')
$report = Join-Path $projectRoot "out/reports/${stamp}_${Mode}_${SwapChain}"
$logPath = Join-Path $report 'logs'
New-Item -ItemType Directory -Force -Path $logPath | Out-Null

$os = Get-CimInstance Win32_OperatingSystem |
    Select-Object Caption, Version, BuildNumber, OSArchitecture
$gpu = @(Get-CimInstance Win32_VideoController |
    Select-Object Name, DriverVersion, DriverDate, PNPDeviceID, CurrentHorizontalResolution,
        CurrentVerticalResolution, CurrentRefreshRate)
$windowsVersion = Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion'
[ordered]@{
    capturedAtUtc = (Get-Date).ToUniversalTime().ToString('o')
    operatingSystem = $os
    displayVersion = $windowsVersion.DisplayVersion
    updateBuildRevision = $windowsVersion.UBR
    videoControllers = $gpu
    preset = $Preset
    mode = $Mode
    fps = $Fps
    durationSeconds = $Duration
    swapChain = $SwapChain
    vsync = -not $NoVsync.IsPresent
    warp = $Warp.IsPresent
    scene = $Scene
} | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 (Join-Path $report 'environment.json')

# Source hashes work even when this directory was transferred without .git metadata.
$sourceFiles = @()
foreach ($directory in @('apps', 'src', 'shaders', 'tests', 'tools', 'scenes')) {
    $sourceFiles += Get-ChildItem -Path (Join-Path $projectRoot $directory) -Recurse -File
}
foreach ($filename in @('CMakeLists.txt', 'CMakePresets.json', 'vcpkg.json')) {
    $sourceFiles += Get-Item (Join-Path $projectRoot $filename)
}
$sourceFiles | Sort-Object FullName | ForEach-Object {
    [pscustomobject]@{
        path = $_.FullName.Substring($projectRoot.Length + 1)
        sha256 = (Get-FileHash -Algorithm SHA256 $_.FullName).Hash
    }
} | Export-Csv -NoTypeInformation -Encoding UTF8 (Join-Path $report 'source-hashes.csv')
Get-FileHash -Algorithm SHA256 $executable |
    Format-List | Out-File -Encoding UTF8 (Join-Path $report 'binary-hash.txt')

Push-Location $projectRoot
try {
    & ctest --preset $Preset --output-on-failure --no-tests=error --output-log (Join-Path $report 'ctest.log')
    if ($LASTEXITCODE -ne 0) { throw "CTest failed. Report: $report" }
    $arguments = @('--mode', $Mode, '--fps', "$Fps", '--duration', "$Duration",
        '--swap-chain', $SwapChain, '--log-dir', ('"' + $logPath + '"'))
    if ($NoVsync) { $arguments += '--no-vsync' }
    if ($Warp) { $arguments += '--warp' }
    if ($Scene) {
        $scenePath = (Resolve-Path $Scene).Path
        $arguments += @('--scene', ('"' + $scenePath + '"'))
    }
    Write-Host "Running $Mode mode for $Duration seconds. Report: $report"
    $process = Start-Process -FilePath $executable -ArgumentList $arguments -NoNewWindow -Wait -PassThru `
        -RedirectStandardOutput (Join-Path $report 'stdout.txt') `
        -RedirectStandardError (Join-Path $report 'stderr.txt')
    $exitCode = $process.ExitCode
    @"
# AetherWall manual result

Mode: $Mode; swap chain requested: $SwapChain; Core exit code: $exitCode.
Scene: $Scene (empty means the built-in gradient).
Core exit code 0 does not establish visual correctness.
Source identity: source-hashes.csv; executable identity: binary-hash.txt.
Record the commit SHA as well when available: NOT RECORDED.

| Check | Status | Evidence / limitations |
| --- | --- | --- |
| CTest policy tests | PASS | ctest.log |
| Animated GPU gradient visible | NOT TESTED | Fill after visual inspection |
| M1 scene matches manifest, images/alpha/motion/particles work | NOT TESTED | When using -Scene |
| Static totalDrawPresents stops growing after initial redraws | NOT TESTED | Use bundled/still |
| Icons, selection, context menu, taskbar usable | NOT TESTED | Desktop mode only |
| No wallpaper entry in Alt+Tab / no drag or resize | NOT TESTED | Console remains a diagnostic window |
| Explorer restart and reattachment | NOT TESTED | Follow docs/TEST_PLAN.md |
| Lock/unlock, sleep/resume, DPI/display changes | NOT TESTED | Follow docs/TEST_PLAN.md |
| Clean exit / safe refusal without overlay | NOT TESTED | Inspect desktop and logs |

Overall M0 gate: NOT TESTED.
Fill PASS/FAIL/NOT TESTED, exact steps, screen resolution/DPI, physical/virtual/remote session,
and attach logs/screenshots before changing the gate status.
"@ | Set-Content -Encoding UTF8 (Join-Path $report 'RESULTS.md')
    Write-Host "Exit code: $exitCode. Complete $report/RESULTS.md manually."
    if ($exitCode -ne 0) { throw "Core failed or safely refused this topology. See $report/logs." }
} finally {
    Pop-Location
}
