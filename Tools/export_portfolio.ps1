[CmdletBinding()]
param(
    [string]$Destination = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression

$OutpostRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$ExportRoot = [IO.Path]::GetFullPath($Destination)

if (-not (Test-Path -LiteralPath $OutpostRoot -PathType Container)) {
    throw "Outpost project root was not found: $OutpostRoot"
}
New-Item -ItemType Directory -Path $ExportRoot -Force | Out-Null

$SourceExcludedNames = @(
    'Binaries', 'Intermediate', 'Saved', 'DerivedDataCache', 'Release', '.vs',
    '.git', 'Build', 'obj', 'bin', '__pycache__'
)

function Get-RelativeArchivePath([string]$Root, [string]$Path) {
    return [IO.Path]::GetRelativePath($Root, $Path).Replace('\', '/')
}

function Test-ExcludedSourcePath([string]$RelativePath) {
    foreach ($Name in $SourceExcludedNames) {
        if ($RelativePath -match "(^|/)$([regex]::Escape($Name))(/|$)") { return $true }
    }
    return $RelativePath -match '(^|/)\.(sln|suo|VC\.db)$' -or $RelativePath -match '\.pdb$'
}

function Get-FilesForArchive([string]$Root, [string[]]$Folders, [string[]]$Files, [bool]$ExcludeSourceBuildFolders) {
    $Result = [System.Collections.Generic.List[object]]::new()
    foreach ($Folder in $Folders) {
        $FolderPath = Join-Path $Root $Folder
        if (-not (Test-Path -LiteralPath $FolderPath -PathType Container)) {
            throw "Required archive folder was not found: $FolderPath"
        }
        foreach ($Item in Get-ChildItem -LiteralPath $FolderPath -File -Recurse) {
            $Relative = Get-RelativeArchivePath $Root $Item.FullName
            if ($ExcludeSourceBuildFolders -and (Test-ExcludedSourcePath $Relative)) { continue }
            $Result.Add([pscustomobject]@{ File = $Item; Relative = $Relative })
        }
    }
    foreach ($File in $Files) {
        $FilePath = Join-Path $Root $File
        if (-not (Test-Path -LiteralPath $FilePath -PathType Leaf)) {
            throw "Required archive file was not found: $FilePath"
        }
        $Item = Get-Item -LiteralPath $FilePath
        $Result.Add([pscustomobject]@{ File = $Item; Relative = Get-RelativeArchivePath $Root $Item.FullName })
    }
    return $Result
}

function Write-Zip([string]$ZipPath, [object[]]$Entries) {
    $ZipFullPath = [IO.Path]::GetFullPath($ZipPath)
    $ZipStream = [IO.File]::Open($ZipFullPath, [IO.FileMode]::Create, [IO.FileAccess]::Write, [IO.FileShare]::None)
    $Archive = [IO.Compression.ZipArchive]::new($ZipStream, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($Entry in $Entries) {
            $ZipEntry = $Archive.CreateEntry($Entry.Relative, [IO.Compression.CompressionLevel]::Optimal)
            $Input = $Entry.File.OpenRead()
            $Output = $ZipEntry.Open()
            try { $Input.CopyTo($Output) }
            finally { $Output.Dispose(); $Input.Dispose() }
        }
    }
    finally {
        $Archive.Dispose()
        $ZipStream.Dispose()
    }
    $Info = Get-Item -LiteralPath $ZipFullPath
    Write-Host ("Created {0} ({1:N0} bytes, {2} files)" -f $Info.FullName, $Info.Length, $Entries.Count)
}

$SourceFolders = @('Source', 'Config', 'Content', 'SourceArt', 'Tools', 'Docs')
$SourceFiles = @('Outpost2D.uproject', 'README.md', 'Play.cmd', '.gitignore')
$SourceEntries = Get-FilesForArchive $OutpostRoot $SourceFolders $SourceFiles $true
$SourceZip = Join-Path $ExportRoot 'Outpost2D-Source.zip'
Write-Zip $SourceZip $SourceEntries

$ReleaseRoot = Join-Path $OutpostRoot 'Release/Windows'
if (-not (Test-Path -LiteralPath $ReleaseRoot -PathType Container)) {
    throw "Windows package was not found: $ReleaseRoot"
}
$ReleaseEntries = Get-FilesForArchive $ReleaseRoot @('Engine', 'Outpost2D') @(
    'Outpost2D.exe', 'NOTICES.txt', 'PLAY.cmd', 'README.txt',
    'Manifest_DebugFiles_Win64.txt', 'Manifest_NonUFSFiles_Win64.txt', 'Manifest_UFSFiles_Win64.txt'
) $false | Where-Object {
    $_.Relative -notmatch '(^|/)(Saved|Intermediate|DerivedDataCache|\.vs|SourceArt|Tools|Docs)(/|$)' -and $_.Relative -notmatch '\.pdb$'
}
$ReleaseZip = Join-Path $ExportRoot 'Outpost2D-Windows.zip'
Write-Zip $ReleaseZip $ReleaseEntries

Write-Host "Portfolio exports are in the verified destination: $ExportRoot"
