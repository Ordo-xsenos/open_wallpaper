[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$PayloadDirectory,
    [Parameter(Mandatory=$true)][string]$ReportDirectory
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$payload = [IO.Path]::GetFullPath($PayloadDirectory)
$reports = [IO.Path]::GetFullPath($ReportDirectory)
New-Item -ItemType Directory -Force -Path $reports | Out-Null

function Invoke-PayloadCheck {
    param([string]$Executable, [string[]]$Arguments, [string]$Name)
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = Join-Path $payload $Executable
    $start.WorkingDirectory = $payload
    $start.UseShellExecute = $false
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    foreach ($argument in $Arguments) { $start.ArgumentList.Add($argument) }
    # Exercise the actual staged DLLs/plugins, without a developer Qt/VS PATH.
    $start.Environment['PATH'] = "$env:SystemRoot\System32;$env:SystemRoot"
    foreach ($environmentName in @('QT_PLUGIN_PATH','QT_QPA_PLATFORM_PLUGIN_PATH','QML_IMPORT_PATH','QML2_IMPORT_PATH','QT_QPA_PLATFORM')) {
        $start.Environment.Remove($environmentName) | Out-Null
    }
    # This is a QML/runtime smoke check, not a Direct3D/desktop compatibility test.
    $start.Environment['QT_QUICK_BACKEND'] = 'software'
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $start
    try {
        if (-not $process.Start()) { throw "Cannot start $Executable" }
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $timedOut = -not $process.WaitForExit(30000)
        if ($timedOut) {
            # Stop only the child created by this check; never another user's Core.
            $process.Kill($true)
            $process.WaitForExit()
        }
        $output = $stdout.GetAwaiter().GetResult() + $stderr.GetAwaiter().GetResult()
        $output | Set-Content -LiteralPath (Join-Path $reports "$Name.log") -Encoding utf8
        if ($timedOut) { throw "$Executable runtime smoke check timed out; see $reports/$Name.log" }
        if ($process.ExitCode -ne 0) { throw "$Executable exited with code $($process.ExitCode); see $reports/$Name.log" }
    } finally {
        $process.Dispose()
    }
}

Invoke-PayloadCheck -Executable 'AetherWall.Core.exe' -Arguments @('--help') -Name 'core-help'
# Exercise real graphics initialization and shutdown using software D3D11 in a
# bounded window. Desktop/Shell integration still requires a real Windows desktop.
Invoke-PayloadCheck -Executable 'AetherWall.Core.exe' -Arguments @('--debug','--mode','window','--warp','--duration','2','--log-dir',(Join-Path $reports 'core-window'),'--data-dir',(Join-Path $reports 'core-window-data')) -Name 'core-window-warp'
foreach ($scene in Get-ChildItem -LiteralPath (Join-Path $payload 'scenes/bundled') -Directory) {
    Invoke-PayloadCheck -Executable 'AetherWall.SceneCheck.exe' -Arguments @($scene.FullName) -Name "scene-$($scene.Name)"
}
$capture = Join-Path $reports 'manager-startup.png'
if (Test-Path -LiteralPath $capture) { Remove-Item -LiteralPath $capture }
Invoke-PayloadCheck -Executable 'AetherWall.Manager.exe' -Arguments @('--debug', '--debug-dir', (Join-Path $reports 'debug-session'), '--capture', $capture) -Name 'manager-startup'
if (-not (Test-Path -LiteralPath $capture -PathType Leaf) -or (Get-Item -LiteralPath $capture).Length -eq 0) {
    throw 'Manager did not produce its QML startup capture.'
}
Write-Host "Staged runtime checks passed with a clean PATH. Evidence: $reports"

foreach ($scriptName in @('Start-AetherWallDebug.ps1','Collect-AetherWallDiagnostics.ps1')) {
    # Parse the interactive launcher without starting another UI or waiting for input.
    [void][scriptblock]::Create((Get-Content -LiteralPath (Join-Path $payload $scriptName) -Raw))
}
$session = Join-Path $reports 'debug-session'
if (@(Get-ChildItem -LiteralPath $session -Filter 'manager_*.log' -File).Count -eq 0) {
    throw 'Debug Manager did not create a diagnostic log.'
}
$archive = & (Join-Path $payload 'Collect-AetherWallDiagnostics.ps1') -SessionDirectory $session
if (-not (Test-Path -LiteralPath $archive -PathType Leaf)) { throw 'Diagnostic archive was not created.' }

$unpackedDiagnostics = Join-Path $reports ('diagnostics-check-' + [Guid]::NewGuid().ToString('N'))
try {
    Expand-Archive -LiteralPath $archive -DestinationPath $unpackedDiagnostics
    if (-not (Test-Path -LiteralPath (Join-Path $unpackedDiagnostics 'system.json')) -or
        @(Get-ChildItem -LiteralPath $unpackedDiagnostics -Filter 'manager_*.log' -File).Count -eq 0) {
        throw 'Diagnostic ZIP is missing system information or Manager logs.'
    }
} finally {
    if (Test-Path -LiteralPath $unpackedDiagnostics) { Remove-Item -LiteralPath $unpackedDiagnostics -Recurse -Force }
}
