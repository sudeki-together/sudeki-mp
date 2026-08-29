# Linux Local Co-op Beta Quick Start

This guide is for the current **two-player local co-op beta**. It runs one
authoritative Sudeki process through Wine: Player 1 uses keyboard and mouse;
Player 2 uses a Linux controller. This is not online multiplayer.

## What you need

- A Linux desktop with Wine and a controller exposed as `/dev/input/js0`.
- The supported GOG release of `SUDEKI.exe` (GOG build `50303954381148403`).
  The launcher validates SHA-256
  `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94` and
  refuses unknown executables.
- A local clone of this repository and the project's 32-bit MinGW build
  dependencies. See [mod-loader.md](mod-loader.md#linux-build) for the
  toolchain details.

Do not copy, commit, or redistribute Sudeki's game files or saves.

## Build once

From the repository root:

```bash
./tools/build-linux.sh
```

The command produces the PE32 launcher and `SudekiMP.dll` in
`build/mingw32/bin/`, plus the Linux controller-to-loopback bridge in
`build/linux/bin/`.

## Choose your game and Wine prefix

The scripts default to the maintainer's local paths. Set these variables for
your own installation instead:

```bash
export SUDEKIMP_GAME='/absolute/path/to/SUDEKI.exe'
export SUDEKIMP_WINEPREFIX="$HOME/Games/sudeki-coop-prefix"
```

Validate the exact game/DLL pair before launching:

```bash
./tools/continue-research.sh --check
```

The expected result says the build is supported and the launcher will permit
injection. A hash mismatch is a stop condition, not something to bypass.

## Play local co-op

Connect the second controller and start the graphical launcher:

```bash
./tools/sudekimp-beta-launcher.sh
```

It presents the supported launch options, explains each one, and lets you
choose the game executable, Wine prefix, and controller device through
**Settings**. **Paste paths…** accepts all three values directly from the
keyboard or clipboard. Choose **Play local co-op beta** to use the tested
profile. The launcher writes its own output to `build/linux/beta-launcher.log`.
It uses the Sudeki Together crest, opens the developer page for **wander**, and
can stream **Map Inversion** inside the launcher through `ffplay` after it
checks the public project music catalog. Music is optional; the launcher still
works when `curl` or FFmpeg is absent.
If Zenity or a graphical desktop session is unavailable, it automatically uses
an accessible terminal menu instead.

To add it to your Linux desktop/app launcher, run this once from the repository
root:

```bash
./tools/install-linux-launcher.sh
```

This creates a per-user `.desktop` entry pointing at this checkout; it does not
copy game files or install system-wide software.

The equivalent terminal command is:

```bash
SUDEKIMP_INPUT_DEVICE=/dev/input/js0 \
  ./tools/continue-research.sh --party-lifecycle-trace
```

Use another `/dev/input/jsN` value if your controller is assigned a different
device. The launch output identifies the controller bridge; if it cannot open
the device, Player 2 remains unavailable rather than receiving guessed input.

Load a normal save with Tal and Ailish available. Player 1 is the host/front
character and uses keyboard/mouse. Player 2 uses the controller and receives
the right-side camera.

## Current controls and behavior

- Player 2 left stick moves their character.
- `A` interacts outside combat and performs the character's normal weak action
  during combat.
- `X` is contextual: Tal/Buki use Strong; Ailish/Elco use the experimental
  camera-only perspective toggle when that camera mode is eligible.
- Start requests Player 2 participation; hold Back + Start for one second to
  leave.
- Campaign rooms and doors are host-led. Player 1 starts the native transition
  and SudekiMP reacquires Player 2 after the destination has settled.
- Inventory, money, shops, save books, and native modal menus remain shared
  game systems in this beta. Do not expect independent merchant checkout yet.

## Known beta limitations

- This profile has only been accepted on Linux/Wine with one keyboard/mouse
  host and one Linux controller. Native Windows controller delivery is a
  separate follow-up.
- Player 2's native collision camera deliberately does not expose independent
  right-stick orbit while its authored Exploration camera is active. The
  fallback camera can orbit but has weaker obstruction behavior.
- Some menus are intentionally shown full-width because Sudeki owns them as
  one global native UI.
- P2 world interaction and per-player merchant/forge checkout are still under
  investigation; do not treat an orange interaction indicator as proof that a
  native action was dispatched.

## If something goes wrong

Close the game and attach `SudekiMP.log` from the working game directory when
reporting a problem. Include the launch command, whether both camera halves
appeared, controller model/device path, and the last action before failure.
Do not share copyrighted game data or personal saves publicly.
