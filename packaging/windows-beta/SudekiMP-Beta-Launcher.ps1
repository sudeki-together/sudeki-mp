# SudekiMP Windows Beta Launcher
#
# This is the update helper used by SudekiMP.BetaLauncher.exe. It keeps the game's files
# untouched: it remembers a game folder, validates/starts the shipped loader,
# and can optionally fetch a newer SudekiMP beta package after an explicit
# confirmation.  It never runs automatic updates.

param(
    [switch]$UpdateOnly,
    [uint32]$WaitForPid = 0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$ScriptVersion = '1'
$PackageApi = 'https://git.unfilteredrealm.com/api/v1/packages/sudeki-together?type=generic&name=sudekimp-windows-beta'
$PackageDownloadRoot = 'https://git.unfilteredrealm.com/api/packages/sudeki-together/generic/sudekimp-windows-beta'
$PackageArchiveName = 'SudekiMP-Windows-Beta.zip'
$PackageDirectory = $PSScriptRoot
$SettingsDirectory = Join-Path $env:LOCALAPPDATA 'SudekiMP'
$SettingsPath = Join-Path $SettingsDirectory 'windows-beta-launcher.json'
$UpdateLogPath = Join-Path $SettingsDirectory 'windows-beta-updater.log'
$script:GameDirectory = $null
$script:statusLabel = $null

function Show-Problem([string]$Message) {
    [void][System.Windows.Forms.MessageBox]::Show(
        $Message,
        'SudekiMP Windows Beta',
        [System.Windows.Forms.MessageBoxButtons]::OK,
        [System.Windows.Forms.MessageBoxIcon]::Error)
}

function Write-UpdateLog([string]$Message) {
    try {
        New-Item -ItemType Directory -Path $SettingsDirectory -Force | Out-Null
        Add-Content -LiteralPath $UpdateLogPath -Encoding UTF8 -Value (
            ('[{0:O}] {1}' -f (Get-Date), $Message))
    }
    catch {
        # Logging must never turn an update failure into an unrecoverable state.
    }
}

function Get-SettingsGameDirectory {
    if (-not (Test-Path -LiteralPath $SettingsPath -PathType Leaf)) {
        return $null
    }
    try {
        $settings = Get-Content -LiteralPath $SettingsPath -Raw |
            ConvertFrom-Json
        if ($null -ne $settings.GameDirectory -and
            -not [string]::IsNullOrWhiteSpace([string]$settings.GameDirectory)) {
            return [string]$settings.GameDirectory
        }
    }
    catch {
        # A bad local preference must never prevent manual folder selection.
    }
    return $null
}

function Save-SettingsGameDirectory([string]$Directory) {
    New-Item -ItemType Directory -Path $SettingsDirectory -Force | Out-Null
    [pscustomobject]@{
        Version = $ScriptVersion
        GameDirectory = $Directory
    } | ConvertTo-Json | Set-Content -LiteralPath $SettingsPath -Encoding UTF8
}

function Resolve-InitialGameDirectory {
    $saved = Get-SettingsGameDirectory
    if ($null -ne $saved -and
        (Test-Path -LiteralPath (Join-Path $saved 'SUDEKI.exe') -PathType Leaf)) {
        return $saved
    }

    # Support both recommended <game>\SudekiMP\ and flat developer installs.
    foreach ($candidate in @($PackageDirectory, (Split-Path -Parent $PackageDirectory))) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and
            (Test-Path -LiteralPath (Join-Path $candidate 'SUDEKI.exe') -PathType Leaf)) {
            return $candidate
        }
    }
    return $null
}

function Get-InstallPaths {
    if ([string]::IsNullOrWhiteSpace($script:GameDirectory)) {
        return $null
    }
    return [pscustomobject]@{
        Game = Join-Path $script:GameDirectory 'SUDEKI.exe'
        Loader = Join-Path $PackageDirectory 'SudekiMP.Launcher.exe'
        Dll = Join-Path $PackageDirectory 'SudekiMP.dll'
    }
}

function Test-LocalInstall([switch]$ShowError) {
    $paths = Get-InstallPaths
    if ($null -eq $paths) {
        if ($ShowError) { Show-Problem 'Choose the folder that contains SUDEKI.exe first.' }
        return $false
    }
    foreach ($path in @($paths.Game, $paths.Loader, $paths.Dll)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            if ($ShowError) { Show-Problem "Required file is missing:`r`n$path" }
            return $false
        }
    }
    return $true
}

function Get-CheckResult {
    if (-not (Test-LocalInstall)) {
        return [pscustomobject]@{ Success = $false; Text = 'Select a valid Sudeki folder.' }
    }
    $paths = Get-InstallPaths
    try {
        $output = (& $paths.Loader --check $paths.Game $paths.Dll 2>&1 | Out-String).Trim()
        if ($LASTEXITCODE -eq 0) {
            return [pscustomobject]@{ Success = $true; Text = $output }
        }
        return [pscustomobject]@{ Success = $false; Text = $output }
    }
    catch {
        return [pscustomobject]@{ Success = $false; Text = $_.Exception.Message }
    }
}

function Get-LatestPackage {
    $packages = @(Invoke-RestMethod -Uri $PackageApi -Method Get -UseBasicParsing)
    $latest = $packages |
        Where-Object { $_.type -eq 'generic' -and $_.name -eq 'sudekimp-windows-beta' } |
        Sort-Object { [datetime]$_.created_at } -Descending |
        Select-Object -First 1
    if ($null -eq $latest -or [string]::IsNullOrWhiteSpace([string]$latest.version)) {
        throw 'The project server did not return a Windows beta package.'
    }
    return $latest
}

function Update-LocalPackage([switch]$SkipConfirmation) {
    if (Get-Process -Name 'SUDEKI' -ErrorAction SilentlyContinue) {
        Show-Problem 'Close Sudeki before updating SudekiMP.'
        Write-UpdateLog 'Update rejected because SUDEKI.exe is still running.'
        return $false
    }
    if (-not $SkipConfirmation) {
        $confirmation = [System.Windows.Forms.MessageBox]::Show(
            "Manual download is recommended. This option contacts the Sudeki Together project server over HTTPS, downloads an unsigned beta ZIP, and replaces only SudekiMP files. It preserves your SudekiMP.ini and never changes game files or saves.`r`n`r`nContinue?",
            'Optional beta update',
            [System.Windows.Forms.MessageBoxButtons]::YesNo,
            [System.Windows.Forms.MessageBoxIcon]::Warning,
            [System.Windows.Forms.MessageBoxDefaultButton]::Button2)
        if ($confirmation -ne [System.Windows.Forms.DialogResult]::Yes) {
            Set-Status 'Update cancelled. No files changed.' $false
            Write-UpdateLog 'Update cancelled by the user.'
            return $false
        }
    }

    $tempRoot = Join-Path -Path ([System.IO.Path]::GetTempPath()) `
        -ChildPath ('SudekiMP-update-' + [guid]::NewGuid().ToString('N'))
    try {
        New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null
        Write-UpdateLog "Starting update from $PackageDirectory."
        Set-Status 'Checking for the latest Windows beta…' $false
        $latest = Get-LatestPackage
        $archive = Join-Path $tempRoot $PackageArchiveName
        $downloadUri = "$PackageDownloadRoot/$($latest.version)/$PackageArchiveName"
        Invoke-WebRequest -Uri $downloadUri -OutFile $archive -UseBasicParsing
        Expand-Archive -LiteralPath $archive -DestinationPath $tempRoot -Force
        $source = Join-Path $tempRoot 'SudekiMP'
        $required = @(
            'SudekiMP.Launcher.exe',
            'SudekiMP.BetaLauncher.exe',
            'SudekiMP.dll',
            'SudekiMP.ini',
            'SudekiMP-Beta-Launcher.ps1',
            'Launch SudekiMP.cmd',
            'README-Windows.txt'
        )
        foreach ($name in $required) {
            if (-not (Test-Path -LiteralPath (Join-Path $source $name) -PathType Leaf)) {
                throw "Downloaded package is incomplete: $name"
            }
        }

        $backup = Join-Path -Path $PackageDirectory `
            -ChildPath ('SudekiMP-Backups\\' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
        New-Item -ItemType Directory -Path $backup -Force | Out-Null
        foreach ($name in $required) {
            $destination = Join-Path $PackageDirectory $name
            if (Test-Path -LiteralPath $destination -PathType Leaf) {
                Copy-Item -LiteralPath $destination -Destination $backup -Force
            }
        }
        foreach ($name in $required) {
            # The INI is player configuration, not an updater-owned file.
            if ($name -eq 'SudekiMP.ini' -and
                (Test-Path -LiteralPath (Join-Path $PackageDirectory $name) -PathType Leaf)) {
                continue
            }
            Copy-Item -LiteralPath (Join-Path $source $name) -Destination (Join-Path $PackageDirectory $name) -Force
        }
        Set-Status "Updated to $($latest.version). Close and reopen this launcher before playing." $true
        Write-UpdateLog "Update succeeded: $($latest.version)."
        return $true
    }
    catch {
        $failure = $_.Exception.Message
        Write-UpdateLog "Update failed: $failure"
        Show-Problem "Update failed. Existing game and save files were not changed.`r`n`r`n$failure`r`n`r`nDetails: $UpdateLogPath"
        Set-Status 'Update failed; see the message above.' $false
        return $false
    }
    finally {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}

function Set-Status([string]$Text, [bool]$Success) {
    if ($null -eq $script:statusLabel) {
        Write-Host $Text
        return
    }
    if ($Success) {
        $script:statusLabel.ForeColor = [System.Drawing.Color]::FromArgb(0, 105, 55)
    }
    else {
        $script:statusLabel.ForeColor = [System.Drawing.Color]::FromArgb(105, 60, 0)
    }
    $script:statusLabel.Text = $Text
}

if ($UpdateOnly) {
    if ($WaitForPid -ne 0) {
        try { Wait-Process -Id $WaitForPid -ErrorAction Stop } catch { }
    }
    if (Update-LocalPackage -SkipConfirmation) {
        $nativeLauncher = Join-Path $PackageDirectory 'SudekiMP.BetaLauncher.exe'
        if (Test-Path -LiteralPath $nativeLauncher -PathType Leaf) {
            Start-Process -FilePath $nativeLauncher -WorkingDirectory $PackageDirectory
        }
    }
    exit
}

$form = New-Object System.Windows.Forms.Form
$form.Text = 'SudekiMP Windows Beta Launcher'
$form.Size = New-Object System.Drawing.Size(650, 355)
$form.MinimumSize = New-Object System.Drawing.Size(650, 355)
$form.StartPosition = 'CenterScreen'
$form.FormBorderStyle = 'FixedDialog'
$form.MaximizeBox = $false
$form.BackColor = [System.Drawing.Color]::FromArgb(24, 29, 36)
$form.ForeColor = [System.Drawing.Color]::White

$title = New-Object System.Windows.Forms.Label
$title.Text = 'SUDEKI MP  /  WINDOWS BETA'
$title.Font = New-Object System.Drawing.Font('Segoe UI', 15, [System.Drawing.FontStyle]::Bold)
$title.Location = New-Object System.Drawing.Point(24, 20)
$title.AutoSize = $true
$form.Controls.Add($title)

$subtitle = New-Object System.Windows.Forms.Label
$subtitle.Text = 'Choose a supported Sudeki folder, verify it, then launch the mod loader.'
$subtitle.Font = New-Object System.Drawing.Font('Segoe UI', 9)
$subtitle.Location = New-Object System.Drawing.Point(26, 55)
$subtitle.AutoSize = $true
$form.Controls.Add($subtitle)

$folderLabel = New-Object System.Windows.Forms.Label
$folderLabel.Text = 'Sudeki folder'
$folderLabel.Location = New-Object System.Drawing.Point(26, 95)
$folderLabel.AutoSize = $true
$form.Controls.Add($folderLabel)

$folderBox = New-Object System.Windows.Forms.TextBox
$folderBox.Location = New-Object System.Drawing.Point(26, 118)
$folderBox.Size = New-Object System.Drawing.Size(485, 25)
$form.Controls.Add($folderBox)

$browseButton = New-Object System.Windows.Forms.Button
$browseButton.Text = 'Browse…'
$browseButton.Location = New-Object System.Drawing.Point(520, 116)
$browseButton.Size = New-Object System.Drawing.Size(95, 28)
$form.Controls.Add($browseButton)

$script:statusLabel = New-Object System.Windows.Forms.Label
$script:statusLabel.Location = New-Object System.Drawing.Point(26, 160)
$script:statusLabel.Size = New-Object System.Drawing.Size(585, 53)
$script:statusLabel.Font = New-Object System.Drawing.Font('Segoe UI', 9)
$form.Controls.Add($script:statusLabel)

$launchButton = New-Object System.Windows.Forms.Button
$launchButton.Text = 'Launch SudekiMP'
$launchButton.Location = New-Object System.Drawing.Point(26, 232)
$launchButton.Size = New-Object System.Drawing.Size(178, 38)
$form.Controls.Add($launchButton)

$verifyButton = New-Object System.Windows.Forms.Button
$verifyButton.Text = 'Verify build'
$verifyButton.Location = New-Object System.Drawing.Point(215, 232)
$verifyButton.Size = New-Object System.Drawing.Size(130, 38)
$form.Controls.Add($verifyButton)

$updateButton = New-Object System.Windows.Forms.Button
$updateButton.Text = 'Optional update…'
$updateButton.Location = New-Object System.Drawing.Point(356, 232)
$updateButton.Size = New-Object System.Drawing.Size(140, 38)
$form.Controls.Add($updateButton)

$closeButton = New-Object System.Windows.Forms.Button
$closeButton.Text = 'Close'
$closeButton.Location = New-Object System.Drawing.Point(507, 232)
$closeButton.Size = New-Object System.Drawing.Size(108, 38)
$form.Controls.Add($closeButton)

$note = New-Object System.Windows.Forms.Label
$note.Text = 'Updates are opt-in and preserve SudekiMP.ini. The launcher never modifies Sudeki.exe, game data, or saves.'
$note.Location = New-Object System.Drawing.Point(26, 292)
$note.Size = New-Object System.Drawing.Size(585, 38)
$note.ForeColor = [System.Drawing.Color]::FromArgb(185, 205, 225)
$form.Controls.Add($note)

$browseButton.Add_Click({
    $dialog = New-Object System.Windows.Forms.FolderBrowserDialog
    $dialog.Description = 'Select the folder containing SUDEKI.exe'
    if ($null -ne $script:GameDirectory) { $dialog.SelectedPath = $script:GameDirectory }
    if ($dialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
        $candidate = $dialog.SelectedPath
        if (-not (Test-Path -LiteralPath (Join-Path $candidate 'SUDEKI.exe') -PathType Leaf)) {
            Show-Problem 'That folder does not contain SUDEKI.exe.'
            return
        }
        $script:GameDirectory = $candidate
        $folderBox.Text = $candidate
        Save-SettingsGameDirectory $candidate
        Set-Status 'Sudeki folder saved. Verify the supported build before launch.' $false
    }
})

$verifyButton.Add_Click({
    $result = Get-CheckResult
    Set-Status $(if ($result.Success) { 'Verification passed: ' + $result.Text } else { 'Verification failed: ' + $result.Text }) $result.Success
})

$launchButton.Add_Click({
    if (-not (Test-LocalInstall -ShowError)) { return }
    $result = Get-CheckResult
    if (-not $result.Success) {
        Show-Problem "SudekiMP will not launch this game build.`r`n`r`n$($result.Text)"
        Set-Status 'Verification failed. Launch blocked.' $false
        return
    }
    $paths = Get-InstallPaths
    Start-Process -FilePath $paths.Loader -ArgumentList @($paths.Game, $paths.Dll) -WorkingDirectory $script:GameDirectory
    Set-Status 'SudekiMP launcher started. The game will open after injection succeeds.' $true
})

$updateButton.Add_Click({ Update-LocalPackage })
$closeButton.Add_Click({ $form.Close() })

$script:GameDirectory = Resolve-InitialGameDirectory
if ($null -ne $script:GameDirectory) {
    $folderBox.Text = $script:GameDirectory
    Set-Status 'Sudeki folder detected. Verify the supported build before launch.' $false
}
else {
    Set-Status 'Choose the folder containing the supported GOG SUDEKI.exe.' $false
}

[void]$form.ShowDialog()
