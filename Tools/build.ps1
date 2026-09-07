[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateSet('Editor', 'Test', 'Package')][string]$Action = 'Editor'
)

$ErrorActionPreference = 'Stop'
$OutpostRoot = Split-Path $PSScriptRoot -Parent
$OutpostProject = Join-Path $OutpostRoot 'Outpost2D.uproject'
$BuildTool = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$EditorCmd = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$UatTool = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'

if (-not (Test-Path -LiteralPath $OutpostProject -PathType Leaf)) { throw 'Outpost2D.uproject is missing.' }

function Invoke-Checked([string]$Tool, [string[]]$Arguments, [string]$Label) {
    if (-not (Test-Path -LiteralPath $Tool -PathType Leaf)) { throw "$Label was not found: $Tool" }
    & $Tool @Arguments
    $ExitCode = $LASTEXITCODE
    if ($ExitCode -ne 0) { throw "$Label failed with exit code $ExitCode." }
}

function Convert-ReportTimestamp([string]$Value) {
    $Parsed = [DateTime]::MinValue
    $Format = [Globalization.CultureInfo]::InvariantCulture
    if (-not [DateTime]::TryParseExact($Value, 'yyyy.MM.dd-HH.mm.ss', $Format,
            [Globalization.DateTimeStyles]::AssumeUniversal -bor [Globalization.DateTimeStyles]::AdjustToUniversal,
            [ref]$Parsed)) {
        throw "Automation report has an invalid reportCreatedOn timestamp: $Value"
    }
    return $Parsed
}

function Assert-AutomationReport([DateTime]$TestRunStart, [DateTime]$PreviousReportWriteTime) {
    $ReportPath = Join-Path $OutpostRoot 'Saved\Automation\index.json'
    if (-not (Test-Path -LiteralPath $ReportPath -PathType Leaf)) {
        throw "Automation report was not generated: $ReportPath"
    }
    $ReportFile = Get-Item -LiteralPath $ReportPath
    if ($PreviousReportWriteTime -ne [DateTime]::MinValue -and $ReportFile.LastWriteTimeUtc -le $PreviousReportWriteTime) {
        throw "Automation report was not regenerated. Previous write time: $PreviousReportWriteTime UTC; current: $($ReportFile.LastWriteTimeUtc) UTC."
    }
    if ($ReportFile.LastWriteTimeUtc -lt $TestRunStart) {
        throw "Automation report is stale. Expected a report generated after $TestRunStart UTC, found $($ReportFile.LastWriteTimeUtc) UTC."
    }
    try { $Report = Get-Content -LiteralPath $ReportPath -Raw | ConvertFrom-Json }
    catch { throw "Automation report is not valid JSON: $ReportPath" }
    foreach ($Property in @('reportCreatedOn', 'succeeded', 'failed', 'notRun', 'inProcess')) {
        if ($null -eq $Report.$Property) { throw "Automation report is missing '$Property': $ReportPath" }
    }
    $ReportTimestamp = Convert-ReportTimestamp ([string]$Report.reportCreatedOn)
    if ($ReportTimestamp -lt $TestRunStart) {
        throw "Automation report timestamp is stale. Expected >= $TestRunStart UTC, found $ReportTimestamp UTC."
    }
    $Succeeded = [int]$Report.succeeded
    $Failed = [int]$Report.failed
    $NotRun = [int]$Report.notRun
    $InProcess = [int]$Report.inProcess
    Write-Host ("Automation report: succeeded={0}, failed={1}, notRun={2}, inProcess={3}" -f $Succeeded, $Failed, $NotRun, $InProcess)
    if ($Failed -ne 0 -or $Succeeded -le 0 -or $NotRun -ne 0 -or $InProcess -ne 0) {
        throw 'Automation report did not satisfy the required test result conditions.'
    }
}

if ($Action -eq 'Package') {
    Invoke-Checked $UatTool @(
        "BuildCookRun", "-project=$OutpostProject", '-noP4', '-platform=Win64',
        '-clientconfig=Development', '-build', '-cook', '-stage', '-pak', '-archive',
        "-archivedirectory=$OutpostRoot\Release", '-unattended', '-NoXGE', '-nodebuginfo', '-utf8output'
    ) 'RunUAT BuildCookRun'
    exit 0
}

Invoke-Checked $BuildTool @(
    'Outpost2DEditor', 'Win64', 'Development', "-Project=$OutpostProject",
    '-WaitMutex', '-NoHotReloadFromIDE', '-NoXGE', '-MaxParallelActions=2'
) 'Build.bat'

if ($Action -eq 'Test') {
    # Unreal serializes reportCreatedOn to UTC and second precision, so use the
    # same UTC precision for the freshness boundary and file write check.
    $TestRunStart = [DateTime]::ParseExact(([DateTime]::UtcNow).ToString('yyyy.MM.dd-HH.mm.ss'),
        'yyyy.MM.dd-HH.mm.ss', [Globalization.CultureInfo]::InvariantCulture,
        [Globalization.DateTimeStyles]::AssumeUniversal -bor [Globalization.DateTimeStyles]::AdjustToUniversal)
    $PreviousReportWriteTime = [DateTime]::MinValue
    $ReportPath = Join-Path $OutpostRoot 'Saved\Automation\index.json'
    if (Test-Path -LiteralPath $ReportPath -PathType Leaf) {
        $PreviousReportWriteTime = (Get-Item -LiteralPath $ReportPath).LastWriteTimeUtc
    }
    Invoke-Checked $EditorCmd @(
        $OutpostProject, '-unattended', '-nullrhi', '-nosplash', '-nosound',
        '-ExecCmds=Automation RunTests Outpost.Simulation',
        '-TestExit=Automation Test Queue Empty',
        "-ReportExportPath=$OutpostRoot\Saved\Automation",
        "-abslog=$OutpostRoot\Saved\tests.log"
    ) 'UnrealEditor-Cmd automation'
    Assert-AutomationReport $TestRunStart $PreviousReportWriteTime
}

exit 0
