# Windows agent handoff: first native build and launch

## Objective

Prove that the current SudekiMP source can be built, installed, injected, and
launched on native Windows against the one supported user-supplied GOG build.

This is a platform-validation pass, not a multiplayer implementation pass.
Stop after the title/roster interface is reached and the startup evidence has
been collected.

## Supported inputs

- Repository: `https://git.unfilteredrealm.com/sudeki-together/sudeki-mp.git`
- GOG package: Sudeki `1.0`, package `13212`, build
  `50303954381148403`
- Required `SUDEKI.exe` SHA256:
  `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`

The user will transfer their own legitimate offline installer set separately.
Do not add the installer, game, saves, extracted assets, logs containing game
data, or build products to Git.

## Required procedure

1. Install Sudeki from the three GOG offline-installer files into a normal
   Windows directory.
2. Verify `SUDEKI.exe` with PowerShell `Get-FileHash` before modifying or
   launching anything.
3. Clone or update the repository. Preserve unrelated local changes and report
   the checked-out commit.
4. Follow [windows-build.md](windows-build.md) exactly:
   - use the MSYS2 `MINGW32` environment;
   - verify `gcc -dumpmachine` begins with `i686-w64-mingw32`;
   - run `./tools/build-windows.sh`;
   - install with `tools/install-windows.ps1`.
5. Run the launcher's `--check` preflight before launching the game.
6. Launch only through the generated `Launch SudekiMP.cmd`.
7. Allow the launcher to skip the three startup-logo movies through the
   checked-in default configuration. Confirm the ordinary title screen and the
   Sudeki Together New Game page can be reached.
8. Exit normally where possible. If the game stalls, end `SUDEKI.exe` through
   Task Manager and retain the matching log/configuration.

## Acceptance evidence

Record all of the following:

- Windows version and architecture.
- MSYS2 environment, `gcc --version`, `gcc -dumpmachine`, `cmake --version`,
  and `ninja --version`.
- Repository commit hash.
- `SUDEKI.exe` SHA256.
- Whether `tools/build-windows.sh` completed without warnings promoted to
  errors.
- `file`/PowerShell evidence that `SudekiMP.dll` and
  `SudekiMP.Launcher.exe` are 32-bit PE files, if an inspection tool is
  available.
- Complete launcher `--check` output.
- The beginning and end of `SudekiMP.log`, including the exact-build line and
  `status=ready`.
- A screenshot of the title screen and one of the Sudeki Together page.
- Normal exit code, crash dialog, or Task Manager termination outcome.

## Safety and scope boundaries

- Never patch or replace `SUDEKI.exe`.
- Never bypass the launcher's executable-hash or signature gates.
- Do not enable arbitrary experimental INI combinations.
- Do not copy the Linux Wine prefix or Linux controller bridge to Windows.
- Do not diagnose multiplayer input, split-screen combat, cameras, Talos, or
  story progression until native build/injection/title startup passes.
- Do not publish or redistribute any transferred GOG installer or installed
  game file.
- If a step fails, preserve the first failure and logs before changing the
  toolchain, configuration, or code.

## Deliverable

Return a concise pass/fail report with commands, versions, hashes, logs, and
screenshots. If code changes were necessary, explain the Windows-specific root
cause, keep exact-build safety intact, rerun build and preflight, and place the
change in a separate commit for review.
