[CmdletBinding()]
param(
    [ValidateSet('hard', 'practice')][string]$Mode = 'hard',
    [ValidateSet(1, 2)][int]$Rank = 2
)

$ErrorActionPreference = 'Stop'
$OutpostRoot = Split-Path $PSScriptRoot -Parent
$PackageRoot = Join-Path $OutpostRoot 'Release\Windows\Outpost2D'
$Executable = Join-Path $PackageRoot 'Binaries\Win64\Outpost2D.exe'
$ReportPath = Join-Path $PackageRoot "Saved\autoplay-$Mode-rank$Rank.json"
$ScreenshotRoot = Join-Path $PackageRoot 'Saved\Screenshots'

if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) {
    throw "Packaged executable was not found: $Executable"
}

# Report names contain no timestamp, so freshness is established by the
# package-local file write time. Keep the same second precision as the game
# capture/report path and never remove an older report.
$StartRoundSecond = [DateTime]::ParseExact(([DateTime]::UtcNow).ToString('yyyy.MM.dd-HH.mm.ss'),
    'yyyy.MM.dd-HH.mm.ss', [Globalization.CultureInfo]::InvariantCulture,
    [Globalization.DateTimeStyles]::AssumeUniversal -bor [Globalization.DateTimeStyles]::AdjustToUniversal)

$ArgumentList = @(
    '-RenderOffScreen', '-ForceRes', '-windowed', '-ResX=1440', '-ResY=960',
    '-unattended', '-nosplash', '-nosound', '-OutpostAutoTest', '-OutpostCapture',
    "-OutpostRank=$Rank"
)
if ($Mode -eq 'practice') { $ArgumentList += '-OutpostPractice' }

$OwnedProcess = Start-Process -FilePath $Executable -ArgumentList $ArgumentList `
    -WorkingDirectory $PackageRoot -WindowStyle Hidden -PassThru
$OwnedProcessId = $OwnedProcess.Id

try {
    if (-not $OwnedProcess.WaitForExit(240000)) {
        if (-not $OwnedProcess.HasExited) {
            Stop-Process -Id $OwnedProcessId -Force
        }
        throw "Packaged smoke run timed out after 240 seconds (owned PID $OwnedProcessId)."
    }
    $ExitCode = $OwnedProcess.ExitCode
    if ($ExitCode -ne 0) { throw "Packaged smoke run failed with exit code $ExitCode." }
}
finally {
    $OwnedProcess.Dispose()
}

if (-not (Test-Path -LiteralPath $ReportPath -PathType Leaf)) {
    throw "Packaged automation report was not generated: $ReportPath"
}
$ReportFile = Get-Item -LiteralPath $ReportPath
if ($ReportFile.LastWriteTimeUtc -le $StartRoundSecond) {
    throw "Packaged automation report is stale. Expected newer than $StartRoundSecond UTC, found $($ReportFile.LastWriteTimeUtc) UTC."
}
try { $Report = Get-Content -LiteralPath $ReportPath -Raw | ConvertFrom-Json }
catch { throw "Packaged automation report is not valid JSON: $ReportPath" }

foreach ($Property in @('won', 'mode', 'rank', 'kills', 'oreMined', 'wallMoves', 'inputFailures')) {
    if ($null -eq $Report.$Property) { throw "Packaged automation report is missing '$Property': $ReportPath" }
}

$MinimumOre = if ($Rank -eq 2) { 24 } else { 10 }
$Checks = @(
    [pscustomobject]@{ Pass = [bool]$Report.won; Name = 'won=true' },
    [pscustomobject]@{ Pass = ([string]$Report.mode -eq $Mode); Name = "mode=$Mode" },
    [pscustomobject]@{ Pass = ([int]$Report.inputFailures -eq 0); Name = 'inputFailures=0' },
    [pscustomobject]@{ Pass = ([int]$Report.kills -eq 30); Name = 'kills=30' },
    [pscustomobject]@{ Pass = ([int]$Report.rank -eq $Rank); Name = "rank=$Rank" },
    [pscustomobject]@{ Pass = ([int]$Report.oreMined -ge $MinimumOre); Name = "oreMined>=$MinimumOre" },
    [pscustomobject]@{ Pass = ([int]$Report.wallMoves -ge $(if ($Rank -eq 1) { 1 } else { 0 })); Name = "wallMoves>=$(if ($Rank -eq 1) { 1 } else { 0 })" }
)
$FailedChecks = @($Checks | Where-Object { -not $_.Pass })
if ($FailedChecks.Count) {
    throw ("Packaged smoke checks failed: " + (($FailedChecks | ForEach-Object Name) -join ', '))
}

Write-Host ("Package smoke PASS: mode={0}, rank={1}, hp={2}, time={3}s, dashes={4}, score={5}" -f `
    $Report.mode, $Report.rank, $Report.hp, $Report.time, $Report.dashes, $Report.score)
Write-Host "Report: $ReportPath"
Write-Host "Screenshots: $ScreenshotRoot"
exit 0
