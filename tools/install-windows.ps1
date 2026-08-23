[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$GameDirectory,

    [string]$BuildDirectory = (Join-Path $PSScriptRoot '..\build\windows-mingw32'),

    [switch]$Launch
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$SupportedSha256 = '8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94'
$GameDirectory = [System.IO.Path]::GetFullPath($GameDirectory)
$BuildDirectory = [System.IO.Path]::GetFullPath($BuildDirectory)
$GameExecutable = Join-Path $GameDirectory 'SUDEKI.exe'
$BinaryDirectory = Join-Path $BuildDirectory 'bin'
$InstallDirectory = Join-Path $GameDirectory 'SudekiMP'

if (-not (Test-Path -LiteralPath $GameExecutable -PathType Leaf)) {
    throw "SUDEKI.exe was not found in '$GameDirectory'."
}

$ActualSha256 = (Get-FileHash -LiteralPath $GameExecutable -Algorithm SHA256).Hash.ToLowerInvariant()
if ($ActualSha256 -ne $SupportedSha256) {
    throw @"
Unsupported SUDEKI.exe. No files were installed.
Expected: $SupportedSha256
Actual:   $ActualSha256
SudekiMP currently supports only GOG offline build 50303954381148403.
"@
}

$Artifacts = @(
    'SudekiMP.Launcher.exe',
    'SudekiMP.dll',
    'SudekiMP.ini'
)
foreach ($Artifact in $Artifacts) {
    $Source = Join-Path $BinaryDirectory $Artifact
    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) {
        throw "Missing build artifact '$Source'. Run tools/build-windows.sh first."
    }
}

New-Item -ItemType Directory -Path $InstallDirectory -Force | Out-Null
foreach ($Artifact in $Artifacts) {
    Copy-Item -LiteralPath (Join-Path $BinaryDirectory $Artifact) `
        -Destination (Join-Path $InstallDirectory $Artifact) -Force
}

$LaunchScript = Join-Path $InstallDirectory 'Launch SudekiMP.cmd'
$LaunchContents = @'
@echo off
"%~dp0SudekiMP.Launcher.exe" "%~dp0..\SUDEKI.exe" "%~dp0SudekiMP.dll"
if errorlevel 1 pause
'@
Set-Content -LiteralPath $LaunchScript -Value $LaunchContents -Encoding Ascii

Write-Host "Installed SudekiMP development build to: $InstallDirectory"
Write-Host 'SUDEKI.exe and the game data were not modified.'
Write-Host "Launch with: $LaunchScript"

if ($Launch) {
    & (Join-Path $InstallDirectory 'SudekiMP.Launcher.exe') `
        $GameExecutable (Join-Path $InstallDirectory 'SudekiMP.dll')
}
