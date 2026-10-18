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
foreach ($scene in Get-ChildItem -LiteralPath (Join-Path $payload 'scenes/bundled') -Directory) {
    Invoke-PayloadCheck -Executable 'AetherWall.SceneCheck.exe' -Arguments @($scene.FullName) -Name "scene-$($scene.Name)"
}
$capture = Join-Path $reports 'manager-startup.png'
if (Test-Path -LiteralPath $capture) { Remove-Item -LiteralPath $capture }
Invoke-PayloadCheck -Executable 'AetherWall.Manager.exe' -Arguments @('--capture', $capture) -Name 'manager-startup'
if (-not (Test-Path -LiteralPath $capture -PathType Leaf) -or (Get-Item -LiteralPath $capture).Length -eq 0) {
    throw 'Manager did not produce its QML startup capture.'
}
Write-Host "Staged runtime checks passed with a clean PATH. Evidence: $reports"
