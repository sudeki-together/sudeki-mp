# SudekiMP

SudekiMP adds multiplayer to the GOG Windows release of *Sudeki* (2005): play
your saved story with a friend over a LAN or VPN, or share one PC in split
screen. It is an injected mod — the game on disk is never patched — and it
keeps Sudeki's own systems (combat, cameras, animation, AI) running wherever
they are understood, instead of re-implementing them.

The repository contains only original source code, documentation, signatures,
configuration, hooks and tooling. It does **not** contain the game, installer,
saves, extracted resources or copyrighted game assets. You need your own copy
of Sudeki.

> [!NOTE]
> SudekiMP is playable with limits and still under active development. The
> table below says exactly what works today and what does not. Everything in it
> was accepted in live play by the project owner on one machine; nothing has
> been through a public beta yet.

## Download

Get the newest launcher from the
[SudekiMP releases page](https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/releases)
(`sudekimp-windows-launcher-<version>.zip`). It unpacks to a ready-to-copy
`SudekiMP` folder with the launcher, raw loader, DLL, safe default
configuration, XInput diagnostic, co-op save fixtures, launch script and the
Windows guide — not Sudeki or any game assets. The launcher's *Check for
updates* (Tools tab) asks that page for a newer release and offers to open it;
it never downloads or installs anything by itself. Install steps are in
[docs/windows-build.md](docs/windows-build.md); Linux/Wine players start with
[docs/linux-coop-beta.md](docs/linux-coop-beta.md).

## What you can play today

| Mode | Status | What works | What does not (yet) |
| --- | --- | --- | --- |
| **Story co-op over LAN** (launcher profile *Multiplayer (title menu lobby)*) | Playable with limits | One player hosts a saved game, friends join from the game's main menu. Everyone moves, fights (melee, ranged, skills, Spirit Strikes), sees the same enemies, deaths, drops, XP popups, doors and chests. The joining player has the game's own camera, HUD, audio and controls for their character. | Only the host can open doors and chests, talk, shop or save; joiners see the result. One shared party inventory, owned by the host. Both players need an identical copy of the save. No leader change or spectating when a character falls. Bosses, the full campaign and saves transfer are untested or unbuilt. |
| **Two games on one PC** (profile *Local host + client (two windows)*) | Playable with limits | Starts a host and a client window on the same machine, each with its own saves folder, auto-joined into one room. Same rules as above. | Both games must run windowed; this is how the mode has been tested so far. |
| **Local co-op split screen** (profile *Local co-op (2 players)*) | Experimental | Keyboard/mouse Player 1 and a controller Player 2 in one Sudeki process, each with their own camera half; drop in/out with Start / F10. | Accepted on Linux/Wine with a Linux controller only; native Windows controller delivery is a follow-up. Many menus are shared and shown full width. Player 2 cannot interact with the world yet. |
| **Mods** (launcher Mods tab) | Experimental | Texture packages, TexMod `.tpf` conversion, archive/sound/movie files, a clean 4× font example, Main Menu armour choice, flight mode in Safe launch. | See [docs/mod-packages.md](docs/mod-packages.md) for format limits. |
| **Cleanroom (test room)** and **Safe launch** | Tools | The shipped `testroom` with an F8 research menu; a plain launch with every hook off. | Research use. |

"Playable with limits" means: the owner has played the route from the castle
road to New Brightwater and back, repeatedly, as Ailish hosting with Tal
joining, on one Linux PC running two game windows under Wine. It has **not**
been run across two machines or on native Windows yet, and other character
combinations are less proven. Every row above is tracked with its evidence in
[docs/status-matrix.md](docs/status-matrix.md).

## Supported game build

SudekiMP works with exactly one executable and refuses every other:

| Property | Value |
| --- | --- |
| Store/build | GOG offline build `50303954381148403` |
| Executable | `SUDEKI.exe` (PE32/x86) |
| SHA256 | `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94` |
| Runtimes | Windows; Wine on Linux (development platform) |

Every hook checks the exact bytes it expects before installing and backs out
cleanly if anything differs. Nothing is written to the game's files.

## Playing story co-op

1. Both players install the same SudekiMP version beside their own copy of
   Sudeki, and both have the **same save** (copy the host's `SAVESLOTnnnn`
   folder to the joiner — the lobby checks the save fingerprint and refuses a
   mismatch).
2. Each player starts the launcher and picks **Multiplayer (title menu lobby)**,
   then **Play**. The game's main menu gains a **Multiplayer** entry.
3. The host creates a room (UDP port 26770, shown on the LAN browser). Joiners
   either pick the room from the browser or enter the host's address directly
   (works over a VPN).
4. The host chooses the saved game. The host plays that save's party leader;
   each joiner picks one of the other party members and locks it. Characters
   nobody picks stay with the game's AI.
5. Everyone presses **Ready**; the host presses **Start**. Both games load the
   save and the joiner takes control of their character.

In play, the host decides where the party goes: zone changes, houses, doors and
chests are triggered by the host, and the joiner is brought along. Controls, camera, HUD and sound for the
joiner are the game's own. If something goes wrong, `SudekiMP.log` is written
beside `SUDEKI.exe` on each machine; attach both when reporting.

To try it alone, choose **Local host + client (two windows)** instead of step 2:
the launcher starts both games windowed on one PC and joins them automatically.

## Building from source

Linux (MinGW-w64 cross build; tests run under Wine):

```bash
./tools/build-linux.sh
```

```bash
./tools/run-wine.sh --check
```

Windows (MSYS2 `MINGW32`, no Wine needed): `tools/build-windows.sh`, then
`tools/install-windows.ps1`. Details, preflight and the exact-build check are in
[docs/windows-build.md](docs/windows-build.md); the Linux developer workflow,
research launch modes and the graphical Linux launcher are in
[docs/linux-coop-beta.md](docs/linux-coop-beta.md) and
`./tools/continue-research.sh --help`. `./tools/stop-sudeki.sh` stops a stalled
live experiment.

## For developers and researchers

SudekiMP is also a reverse-engineering project: the exact-build research behind
every hook is documented and reproducible.

- [docs/status-matrix.md](docs/status-matrix.md) — canonical feature status with
  evidence levels; start here before believing any other document
- [docs/architecture-current.md](docs/architecture-current.md) — what exists now
- [docs/runtime-lifecycle.md](docs/runtime-lifecycle.md) — hook, object-lifetime
  and teardown rules
- [docs/testing.md](docs/testing.md) — what each kind of test proves
- [docs/evidence-index.md](docs/evidence-index.md) — claim vocabulary
- [docs/coop-story-plan-54.md](docs/coop-story-plan-54.md) — the story co-op
  plan and its dated progress log
- [docs/lan-arena.md](docs/lan-arena.md) and
  [docs/lan-arena-combat-graph.md](docs/lan-arena-combat-graph.md) — network
  protocol, replicas and combat graph
- [docs/executable.md](docs/executable.md),
  [docs/engine-functions.md](docs/engine-functions.md),
  [docs/structures.md](docs/structures.md), [docs/combat.md](docs/combat.md) —
  the engine as understood so far (RVAs, ABIs, structures)
- [docs/cleanroom.md](docs/cleanroom.md) — the test-room harness
- [docs/mod-packages.md](docs/mod-packages.md),
  [docs/launcher-mods.md](docs/launcher-mods.md),
  [docs/armour-choice.md](docs/armour-choice.md) — mod packages and the Mods tab
- [docs/mod-loader.md](docs/mod-loader.md) — launcher, DLL and hook lifecycle
- [docs/research-log.md](docs/research-log.md) — the chronological evidence
  ledger; [docs/milestones.md](docs/milestones.md) and
  [docs/project-status.md](docs/project-status.md) are history, not current
  requirements
- [docs/recording.md](docs/recording.md) — windowed Wine and OBS capture
- [AGENTS.md](AGENTS.md) — the rules coding agents follow in this repository

Issues and discussion live on the
[project tracker](https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/issues).

## License

SudekiMP is licensed under the [GNU Affero General Public License, version 3
or later](LICENSE) (`AGPL-3.0-or-later`). Network users of modified versions
must be offered the corresponding source as required by AGPLv3 section 13.

Redistributions must also preserve the reasonable author attribution in
[NOTICE](NOTICE):

> SudekiMP was originally created by wander.

The license covers SudekiMP's original code and documentation. It does not
license or redistribute Sudeki itself, its assets, or any user save data.

## Repository policy

Do not commit or redistribute Sudeki installers, executables, archives,
extracted assets, saves, memory dumps containing game data, or Wine prefixes.
Keep the user-supplied vanilla tree read-only and run experiments against a
dedicated working copy. Reverse-engineered names and structures must remain
isolated and correctable as the engine model improves.
