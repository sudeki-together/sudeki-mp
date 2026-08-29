#requires -Version 5.1
<#
Installs a repository-scoped Gitea Actions runner for this project on a
64-bit Windows machine. The registration token is deliberately entered into
act_runner's interactive prompt and is never written to this script, a log,
or the repository.

Run from an elevated PowerShell only if your local Task Scheduler policy
requires it. The default task runs as the current user at logon with limited
privileges, not as SYSTEM.
#>
[CmdletBinding()]
param(
    [ValidatePattern('^https://git\.unfilteredrealm\.com/?$')]
    [string]$InstanceUrl = 'https://git.unfilteredrealm.com',

    [string]$RunnerName = "$env:COMPUTERNAME-sudekimp-windows",

    [string]$Labels = 'windows:host',

    [string]$InstallDirectory = (Join-Path $env:LOCALAPPDATA 'SudekiMP\gitea-runner'),

    [switch]$NoScheduledTask,

    [switch]$Uninstall
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RunnerVersion = '0.6.1'
$RunnerFileName = "act_runner-$RunnerVersion-windows-amd64.exe"
$RunnerUrl = "https://dl.gitea.com/gitea-runner/$RunnerVersion/$RunnerFileName"
$ExpectedSha256 = 'f9eee934cfd03d14caabde65603881b9e59f732d89c934aa3e4333ba644a8270'
$TaskName = 'SudekiMP-GiteaRunner'
$InstallDirectory = [System.IO.Path]::GetFullPath($InstallDirectory)
$RunnerPath = Join-Path $InstallDirectory 'act_runner.exe'
$ConfigPath = Join-Path $InstallDirectory 'config.yaml'
$RegistrationPath = Join-Path $InstallDirectory '.runner'
$WrapperPath = Join-Path $InstallDirectory 'run-runner.cmd'

function Assert-WindowsAmd64 {
    if (-not [Environment]::Is64BitOperatingSystem) {
        throw 'The Gitea runner installer requires 64-bit Windows.'
    }
    if ([string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
        throw 'LOCALAPPDATA is unavailable; run this as a normal Windows user.'
    }
}

function Get-RunnerVersion {
    param([string]$Path)

    $versionOutput = & $Path --version 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "act_runner could not start: $versionOutput"
    }
    return ($versionOutput | Out-String).Trim()
}

function Install-RunnerBinary {
    $temporaryPath = Join-Path $InstallDirectory "$RunnerFileName.download"

    if (Test-Path -LiteralPath $RunnerPath -PathType Leaf) {
        Write-Host "Using existing runner: $(Get-RunnerVersion -Path $RunnerPath)"
        return
    }

    Write-Host "Downloading Gitea runner $RunnerVersion for Windows x64..."
    Invoke-WebRequest -Uri $RunnerUrl -OutFile $temporaryPath -UseBasicParsing
    $actualSha256 = (Get-FileHash -LiteralPath $temporaryPath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actualSha256 -ne $ExpectedSha256) {
        Remove-Item -LiteralPath $temporaryPath -Force -ErrorAction SilentlyContinue
        throw "Runner SHA-256 mismatch. Expected $ExpectedSha256, got $actualSha256."
    }
    Move-Item -LiteralPath $temporaryPath -Destination $RunnerPath -Force
    Write-Host "Installed runner: $(Get-RunnerVersion -Path $RunnerPath)"
}

function Initialize-RunnerFiles {
    if (-not (Test-Path -LiteralPath $ConfigPath -PathType Leaf)) {
        Write-Host 'Generating the runner configuration file...'
        & $RunnerPath generate-config | Set-Content -LiteralPath $ConfigPath -Encoding utf8
        if ($LASTEXITCODE -ne 0) {
            throw 'act_runner generate-config failed.'
        }
    }

    # act_runner generates Linux Docker labels by default.  This runner is
    # intentionally a Windows host runner, so write the requested labels into
    # config.yaml before the daemon starts; otherwise it tries to find Docker.
    $configText = Get-Content -LiteralPath $ConfigPath -Raw
    $labelLines = @()
    foreach ($label in ($Labels -split ',')) {
        $trimmedLabel = $label.Trim()
        if (-not [string]::IsNullOrWhiteSpace($trimmedLabel)) {
            $labelLines += "    - `"$trimmedLabel`""
        }
    }
    if ($labelLines.Count -eq 0) {
        throw 'At least one runner label is required.'
    }

    $newline = [Environment]::NewLine
    $replacement = '  labels:' + $newline + ($labelLines -join $newline) + $newline
    $updatedConfigText = [regex]::Replace(
        $configText,
        '(?ms)^  labels:\r?\n(?:    - .*\r?\n)+',
        $replacement)
    if ($updatedConfigText -eq $configText) {
        throw 'act_runner generated an unrecognized labels section.'
    }
    Set-Content -LiteralPath $ConfigPath -Value $updatedConfigText -Encoding utf8

@'
@echo off
setlocal
if exist "C:\msys64\mingw64\bin\node.exe" set "PATH=C:\msys64\mingw64\bin;%PATH%"
if exist "C:\msys64\usr\bin\git.exe" set "PATH=C:\msys64\usr\bin;%PATH%"
cd /d "%~dp0"
"%~dp0act_runner.exe" daemon --config "%~dp0config.yaml"
'@ | Set-Content -LiteralPath $WrapperPath -Encoding ascii
}

function Register-Runner {
    Push-Location $InstallDirectory
    try {
        if (Test-Path -LiteralPath $RegistrationPath -PathType Leaf) {
            Write-Host "Existing runner registration found at $RegistrationPath."
            return
        }

        Write-Host ''
        Write-Host 'Paste the repository or organization registration token only when act_runner asks for it.' -ForegroundColor Yellow
        Write-Host 'The token is not saved in this script, PowerShell history, or a runner log.' -ForegroundColor Yellow
        & $RunnerPath --config $ConfigPath register `
            --instance $InstanceUrl `
            --name $RunnerName `
            --labels $Labels
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $RegistrationPath -PathType Leaf)) {
            throw 'Runner registration did not complete. No scheduled task was created.'
        }
    }
    finally {
        Pop-Location
    }
}

function Install-RunnerTask {
    $identity = "$env:USERDOMAIN\$env:USERNAME"
    $actionArgument = '/d /c ""{0}""' -f $WrapperPath
    $action = New-ScheduledTaskAction -Execute $env:ComSpec -Argument $actionArgument
    $trigger = New-ScheduledTaskTrigger -AtLogOn -User $identity
    $principal = New-ScheduledTaskPrincipal -UserId $identity -LogonType Interactive -RunLevel Limited
    $settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -StartWhenAvailable

    Register-ScheduledTask -TaskName $TaskName -Action $action -Trigger $trigger `
        -Principal $principal -Settings $settings -Description `
        'SudekiMP repository-scoped Gitea Actions runner.' -Force | Out-Null
    Start-ScheduledTask -TaskName $TaskName
    Write-Host "Installed and started scheduled task: $TaskName"
}

function Remove-Runner {
    $task = Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue
    if ($null -ne $task) {
        Stop-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue
        Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false
        Write-Host "Removed scheduled task: $TaskName"
    }
    if (Test-Path -LiteralPath $InstallDirectory) {
        Remove-Item -LiteralPath $InstallDirectory -Recurse -Force
        Write-Host "Removed runner files: $InstallDirectory"
    }
}

Assert-WindowsAmd64
if ($Uninstall) {
    Remove-Runner
    exit 0
}

New-Item -ItemType Directory -Path $InstallDirectory -Force | Out-Null
Install-RunnerBinary
Initialize-RunnerFiles
Register-Runner
if (-not $NoScheduledTask) {
    Install-RunnerTask
}

Write-Host ''
Write-Host 'Gitea runner setup is complete.' -ForegroundColor Green
Write-Host "Runner: $RunnerName"
Write-Host "Labels: $Labels"
Write-Host "Directory: $InstallDirectory"
Write-Host 'Enable Actions for SudekiMP, then manually dispatch the Windows runner smoke workflow.'
