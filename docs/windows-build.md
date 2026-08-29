# Building and running SudekiMP on Windows

SudekiMP can be built and launched directly on Windows. Wine is a development
convenience used by the primary research machine; it is not a requirement for
the DLL or launcher.

This is currently a **Windows beta loader with a focused local-co-op input
acceptance path**, not a finished co-op release. It provides the same
exact-build-gated DLL, configuration, and launcher used by the Linux research
environment. The launcher can reserve one detected XInput slot for Player 2;
several gameplay and lifecycle systems still require live acceptance before
ordinary players should use it for a complete playthrough.

The Windows CI beta archive contains a ready-to-copy `SudekiMP` folder with
`Launch SudekiMP.cmd`, `SudekiMP.BetaLauncher.exe`, `SudekiMP.XInputProbe.exe`,
co-op save fixtures, and `README-Windows.txt`. Copy that folder beside the supported
`SUDEKI.exe`, then run the `.cmd` file. The standalone launcher accepts either
a pasted game-folder path or Browse selection, remembers it under
`%LOCALAPPDATA%\SudekiMP`, verifies the exact build, and launches the raw
loader with the correct paths. Do not open `SudekiMP.Launcher.exe` directly:
it is a console loader which needs explicit game/DLL paths.

The beta launcher uses the project crest as its application icon, includes a
developer link to [wander](https://git.unfilteredrealm.com/wander), and can
play the public project music inside the launcher. Music is fetched only after
the user presses Play, then cached locally; it is not embedded in the package.

The beta launcher's **Get latest beta** button opens the public package page in
a browser. It does not download, run PowerShell, or replace local files. Manual
download and extraction remain the supported update process.

The launcher also exposes **Windows local co-op beta**. With a controller
confirmed by its XInput probe as slot `0`, ticking that option writes only the
package-local `SudekiMP.ini` profile: it enables the proven roster/control/split
stack, routes slot 0 through the mod’s Player 2 protocol, and masks that one
slot from the game’s native Player 1 reads. Player 1 remains keyboard/mouse.
For a loaded save, the injected DLL waits for the world and native party to
settle, then uses its existing atomic roster path to make **Tal** the host and
**Ailish** Player 2. The launcher itself never modifies a save or live party
pointer. If either hero is not in the loaded party, it stays single-player and
records the deferred roster state in `SudekiMP.log` instead of guessing.
The profile deliberately leaves the experimental native P2 collision camera off
for this first Windows input pass so the existing right-stick orbit remains
available. It also leaves the separate Talos full-party restoration experiment
off: Talos's retail Void handoff collapses the party and takes global cinematic
camera ownership, which is not yet safe with the two-viewport runtime. It does
not modify `SUDEKI.exe`, game data, or saves.

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
build/windows-mingw32/bin/SudekiMP.BetaLauncher.exe
build/windows-mingw32/bin/SudekiMP.dll
build/windows-mingw32/bin/SudekiMP.ini
```

The DLL is linked with the GCC runtime statically. The installed beta contains
the native front end, raw loader, DLL, configuration, XInput diagnostic, and
optional co-op save fixtures; it
does not require an extra MSYS2 runtime beside the game.

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
  SudekiMP.BetaLauncher.exe
  SudekiMP.XInputProbe.exe
  SudekiMP.dll
  SudekiMP.ini
  CoopSaveFixtures\
  Launch SudekiMP.cmd
```

It does not overwrite the game executable, archives, saves, or other game
assets. Re-running it updates only those mod-side files. Removing that
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

Then double-click `Launch SudekiMP.cmd`. The standalone launcher lets you
paste or browse for the Sudeki game folder, then it:

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

### Co-op fixture save isolation

The standalone beta launcher has an **Install co-op save fixtures…** action.
It is deliberately separate from game launch. After an explicit warning and
confirmation, it moves the live `%APPDATA%\Sudeki\Save` directory to a
timestamped `%APPDATA%\Sudeki\SudekiMP-Backups\Save-*` archive, creates a new
empty `Save` directory, and copies the packaged beta fixtures there. If fixture
copying fails, it removes the incomplete new directory and attempts to restore
the moved archive. It never deletes an existing save archive or writes game
installation files.

The **Test XInput controller…** control starts only `SudekiMP.XInputProbe.exe`.
It reports whether Windows exposes an XInput controller and its slot; it makes
no game/input mutation. Use its output as the first prerequisite for the
separate native Windows Player 2 input acceptance pass.

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
