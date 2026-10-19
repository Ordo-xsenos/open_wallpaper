[CmdletBinding()]
param([string]$ApplicationDirectory = $PSScriptRoot)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
try {
    $application = [IO.Path]::GetFullPath($ApplicationDirectory)
    $manager = Join-Path $application 'AetherWall.Manager.exe'
    if (-not (Test-Path -LiteralPath $manager -PathType Leaf)) { throw "Manager not found: $manager" }
    $running = @(Get-Process -Name 'AetherWall.Manager','AetherWall.Core' -ErrorAction SilentlyContinue)
    if ($running.Count -gt 0) {
        throw 'Exit AetherWall completely using Exit in its tray menu, then run AetherWall-Debug.cmd again. An existing Core cannot enable debug logging retroactively.'
    }
    $session = Join-Path $env:LOCALAPPDATA ('AetherWall/diagnostics/' + [DateTime]::UtcNow.ToString('yyyyMMdd_HHmmss_fff') + '_' + [Guid]::NewGuid().ToString('N').Substring(0,8))
    New-Item -ItemType Directory -Path $session -Force | Out-Null
    $report = [ordered]@{ startedUtc = [DateTime]::UtcNow.ToString('o'); applicationDirectory = $application; managerExitCode = $null; managerExitCodeHex = $null; launchError = $null }
    Write-Host 'Debug logging enabled. Reproduce the problem, then close the Manager window.'
    Write-Host "Logs: $session"
    try {
        # Start-Process on Windows PowerShell 5.1 joins argument strings: quote the path explicitly.
        $child = Start-Process -FilePath $manager -WorkingDirectory $application -ArgumentList @('--debug','--debug-dir',('"' + $session + '"')) -PassThru
        # Retain the native handle even if Manager exits immediately. Wait only
        # for Manager; Start-Process -Wait can also wait for its long-lived Core.
        $null = $child.Handle
        $child.WaitForExit()
        $report.managerExitCode = $child.ExitCode
        $report.managerExitCodeHex = '0x' + $child.ExitCode.ToString('X8')
        $child.Dispose()
    } catch {
        $report.launchError = $_.Exception.Message
    }
    $report | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $session 'session.json') -Encoding UTF8
    & (Join-Path $PSScriptRoot 'Collect-AetherWallDiagnostics.ps1') -SessionDirectory $session
    if ($report.launchError) { Write-Warning $report.launchError }
    Write-Host 'Send the diagnostics ZIP for analysis. Core may still be running in the tray.'
} catch {
    Write-Host $_.Exception.Message -ForegroundColor Red
} finally {
    Read-Host 'Press Enter to close' | Out-Null
}
