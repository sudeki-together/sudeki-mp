# Building and running SudekiMP on Windows

SudekiMP can be built and launched directly on Windows. Wine is a development
convenience used by the primary research machine; it is not a requirement for
the DLL or launcher.

This is currently a **Windows beta loader**, not a finished co-op release.
It provides the same exact-build-gated DLL, configuration, and launcher used by
the Linux research environment. Several local-co-op systems still require
focused configuration and live acceptance before ordinary players should use
them for a complete playthrough.

The Windows CI beta archive contains a ready-to-copy `SudekiMP` folder with
`Launch SudekiMP.cmd` and `README-Windows.txt`. Copy that folder beside the
supported `SUDEKI.exe`, then run the `.cmd` file. Do not open
`SudekiMP.Launcher.exe` directly: it is a console loader which needs explicit
game/DLL paths.

An automated coding agent performing the first native-machine validation should
also follow [windows-agent-handoff.md](windows-agent-handoff.md), which limits
the pass to build, injection, title startup, and evidence collection.

## Requirements

- 64-bit Windows 10 or Windows 11.
- A legitimate installation of GOG offline build `50303954381148403`.
- `SUDEKI.exe` with SHA256
  `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
- Git for Windows or the Git package supplied by MSYS2.
- MSYS2 with its `MINGW32` environment and 32-bit MinGW-w64 GCC, CMake, and
  Ninja packages.

The game and injected DLL are PE32/x86. The hook source also uses GCC calling
convention attributes and GNU inline assembly, so the supported Windows build
path is 32-bit MinGW GCC. A normal 64-bit compiler or an MSVC build is not a
compatible substitute.

> [!NOTE]
> MSYS2 classifies `MINGW32` as a legacy environment, but its current package
> repository still supplies the required i686 GCC, CMake, and Ninja packages.
> SudekiMP's build script checks the environment and compiler target before it
> configures the project.

## 1. Verify the game

In PowerShell, replace the example path with the real GOG installation:

```powershell
Get-FileHash 'C:\GOG Games\Sudeki\SUDEKI.exe' -Algorithm SHA256
```

Do not continue if the hash differs. The launcher also checks the hash, PE
machine, timestamp, and image size and refuses to inject into an unknown build.
It never patches `SUDEKI.exe` on disk.

Keep a clean vanilla installation or backup. Development should use a separate
working copy when practical.

## 2. Install the 32-bit build tools

Install [MSYS2](https://www.msys2.org/), open its ordinary terminal, and update
it according to the MSYS2 update prompt:

```bash
pacman -Syu
```

If MSYS2 asks for the terminal to be closed, reopen it and run the update again.
Then install Git and the required i686 packages:

```bash
pacman -S --needed git mingw-w64-i686-gcc mingw-w64-i686-cmake mingw-w64-i686-ninja
```

Close that terminal and open **MSYS2 MINGW32**. Its prompt should report
`MINGW32`, and this command must begin with `i686-w64-mingw32`:

```bash
gcc -dumpmachine
```

## 3. Clone and build

From the MINGW32 terminal:

```bash
git clone https://git.unfilteredrealm.com/sudeki-together/sudeki-mp.git
cd sudeki-mp
./tools/build-windows.sh
```

Build products are written to:

```text
build/windows-mingw32/bin/SudekiMP.Launcher.exe
build/windows-mingw32/bin/SudekiMP.dll
build/windows-mingw32/bin/SudekiMP.ini
```

The DLL is linked with the GCC runtime statically, so those three files are the
only SudekiMP runtime files the current launcher requires.

## 4. Install beside a working game

Open PowerShell in the repository and run:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\install-windows.ps1 `
  -GameDirectory 'C:\GOG Games\Sudeki'
```

The installer performs the executable hash check first. It then creates:

```text
C:\GOG Games\Sudeki\SudekiMP\
  SudekiMP.Launcher.exe
  SudekiMP.dll
  SudekiMP.ini
  Launch SudekiMP.cmd
```

It does not overwrite the game executable, archives, saves, or other game
assets. Re-running it updates only those four mod-side files. Removing that
`SudekiMP` directory removes the developer installation.

## 5. Preflight and launch

Before the first launch, open Command Prompt in the installed `SudekiMP`
directory and validate the exact game/DLL pair:

```bat
SudekiMP.Launcher.exe --check ..\SUDEKI.exe SudekiMP.dll
```

The expected result includes:

```text
Build supported; launcher will permit injection.
```

Then double-click `Launch SudekiMP.cmd`. The launcher:

1. validates `SUDEKI.exe`;
2. starts the game suspended;
3. loads and initializes `SudekiMP.dll`;
4. resumes the game only if every enabled hook passes its exact signature and
   dependency gates; and
5. waits until the game exits.

`SudekiMP.log` is written beside `SUDEKI.exe`. On a normal initialization it
should contain `status=ready`.

## Configuration and safety

Edit the installed `SudekiMP\SudekiMP.ini`, not the copy under `config/`, when
testing a local installation. Most reverse-engineering experiments are disabled
by default. Enable only a documented, dependency-complete profile; arbitrary
mixtures of experimental switches are not supported.

The Windows build does not include the Linux joydev-to-UDP controller sender.
That helper exists only because the research controller was not exposed to
Wine. Native Windows controller routing still needs its own integration and
acceptance pass; building successfully should not be mistaken for a complete
Windows two-player input release.

If initialization is rejected, the launcher terminates the still-suspended
child rather than running a partially hooked game. If the game stalls after a
successful launch, end `SUDEKI.exe` from Task Manager and preserve
`SudekiMP.log` with the matching configuration for diagnosis.

## Future binary releases

Once the current prototype reaches a tester-ready milestone, Windows users
should not need MSYS2 or source code. A release archive can contain the three
generated runtime files plus the same installer script. Users will still
supply their own supported GOG installation; no Sudeki executable, archive,
save, or extracted asset will be distributed.
