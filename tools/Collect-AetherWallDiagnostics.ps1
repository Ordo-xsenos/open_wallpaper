[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$SessionDirectory)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$session = [IO.Path]::GetFullPath($SessionDirectory)
if (-not (Test-Path -LiteralPath $session -PathType Container)) { throw 'Diagnostic session directory does not exist.' }
$destination = Join-Path ([IO.Path]::GetTempPath()) ('AetherWall-diagnostics-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $destination | Out-Null
$warnings = [Collections.Generic.List[string]]::new()
try {
    # Read only our session logs, never settings, catalog credentials, SQLite or user assets.
    # FileShare permits collection while Core is still writing its flushed log.
    $total = 0L
    $files = @(Get-ChildItem -LiteralPath $session -File | Where-Object { $_.Name -match '^(manager_.*\.log|m0_.*\.log|session\.json)$' } | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 12)
    foreach ($file in $files) {
        $inputStream = $null
        $outputStream = $null
        try {
            $inputStream = [IO.File]::Open($file.FullName, [IO.FileMode]::Open, [IO.FileAccess]::Read, ([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
            $remaining = [Math]::Min(16781312L, [Math]::Min($inputStream.Length, 67108864L - $total))
            $outputStream = [IO.File]::Create((Join-Path $destination $file.Name))
            $buffer = New-Object byte[] 65536
            while ($remaining -gt 0) {
                $read = $inputStream.Read($buffer, 0, [int][Math]::Min($buffer.Length, $remaining))
                if ($read -eq 0) { break }
                $outputStream.Write($buffer, 0, $read)
                $remaining -= $read
                $total += $read
            }
        } catch { $warnings.Add("Could not collect $($file.Name): $($_.Exception.Message)") }
        finally {
            if ($outputStream) { $outputStream.Dispose() }
            if ($inputStream) { $inputStream.Dispose() }
        }
    }
    $system = [ordered]@{ collectedUtc = [DateTime]::UtcNow.ToString('o'); windows = $null; gpu = @(); warnings = @() }
    try {
        $system.windows = Get-CimInstance Win32_OperatingSystem | Select-Object Caption, Version, BuildNumber, OSArchitecture
        $system.gpu = @(Get-CimInstance Win32_VideoController | Select-Object Name, DriverVersion, DriverDate, VideoProcessor)
    } catch { $warnings.Add("System information unavailable: $($_.Exception.Message)") }
    # Restricted to AetherWall application crashes; absence does not prove antivirus interference.
    try {
        $events = @(Get-WinEvent -FilterHashtable @{ LogName='Application'; Id=1000,1001; StartTime=(Get-Date).AddHours(-2) } -MaxEvents 100 -ErrorAction SilentlyContinue | Where-Object { $_.Message -match 'AetherWall\.(Manager|Core)\.exe' } | Select-Object -First 10 TimeCreated, Id, ProviderName, Message)
        $events | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $destination 'application-events.json') -Encoding UTF8
    } catch { $warnings.Add("Application events unavailable: $($_.Exception.Message)") }
    $system.warnings = @($warnings.ToArray())
    $system | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $destination 'system.json') -Encoding UTF8
    $zip = $session + '.zip'
    # Keep the archive beside its session; repeated collections use a unique suffix.
    if (Test-Path -LiteralPath $zip) { $zip = $session + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8) + '.zip' }
    Compress-Archive -LiteralPath @(Get-ChildItem -LiteralPath $destination -File | ForEach-Object { $_.FullName }) -DestinationPath $zip
    Write-Host "Diagnostics archive: $zip"
    Write-Output $zip
} finally {
    Remove-Item -LiteralPath $destination -Recurse -Force
}
