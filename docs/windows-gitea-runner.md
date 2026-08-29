# Windows Gitea Actions runner

This installs a **repository-scoped Windows runner** for SudekiMP at
`https://git.unfilteredrealm.com`. It is intended to validate the native
Windows build and eventually run a Windows build workflow.

## Security boundary

The default `windows:host` label runs workflow commands directly on the
Windows machine as the signed-in runner user. Treat it like granting that user
remote code-execution authority to every workflow allowed to target this
runner.

Use a **repository-level** registration token for SudekiMP whenever possible.
Do not use an instance-level token on a personal desktop unless every
repository on that Forge is trusted. Never put the token in a Git commit,
workflow, command history, or screenshot.

## Install

On the 64-bit Windows machine, clone this repository or copy just the
installer script. Open PowerShell as the Windows user that should own the
runner and run:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\tools\install-windows-gitea-runner.ps1
```

The script downloads the pinned Gitea runner `0.6.1`, verifies its published
SHA-256, writes its files below
`%LOCALAPPDATA%\SudekiMP\gitea-runner`, and starts a limited current-user
scheduled task at logon. It presents the token prompt through `act_runner`
itself; the script does not accept, print, or persist the registration token.

The runner uses the label `windows:host`. Its default name is based on the
computer name. Override non-secret settings when needed:

```powershell
.\tools\install-windows-gitea-runner.ps1 `
  -RunnerName 'sudeki-windows-builder' `
  -InstallDirectory 'D:\SudekiMP-Runner'
```

The script only accepts the project Forge URL. Do not change it to an
untrusted server without reviewing the script and its security model.

## Confirm it works

1. In the Forge repository settings, enable **Actions** if it is not already
   enabled.
2. Open **Actions** and manually run **Windows runner smoke**.
3. Confirm that it is assigned to the Windows runner and reports the expected
   computer name, PowerShell version, architecture, and Git version.

The smoke workflow is manual-only and does not check out the repository or
build Sudeki. It proves registration and host-label routing without spending
time on a full native build.

## Prepare it for the real build

Before adding a Windows build workflow, install the documented MSYS2
`MINGW32` toolchain on that Windows host. See
[windows-build.md](windows-build.md). The runner should then receive a
workflow that builds the project and retains the PE32 DLL/launcher artifacts;
do not automatically publish or inject them from CI.

## Stop or remove

To remove the scheduled task, registration file, configuration, and runner
binary from that Windows account:

```powershell
.\tools\install-windows-gitea-runner.ps1 -Uninstall
```

Also remove or revoke the runner from the Forge UI when it is no longer
trusted. The script does not revoke server-side tokens for you.
