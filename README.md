# SudekiMP

SudekiMP is a reverse-engineering and modding project that is extending the
GOG Windows release of *Sudeki* into local co-op and, eventually,
host-authoritative online co-op.

The repository contains only original source code, documentation, signatures,
configuration, hooks, and development tooling. It does **not** contain the
game, installer, saves, extracted resources, or copyrighted game assets. A
legitimate user-supplied copy is required.

> [!IMPORTANT]
> SudekiMP is an active research prototype, not a general-purpose release.
> Individual systems have strong live proofs, but they are not yet integrated
> into a complete co-op playthrough.

## Latest Windows beta package

The latest successful Windows build is available as the
[`SudekiMP Windows beta package](https://git.unfilteredrealm.com/sudeki-together/-/packages/generic/sudekimp-windows-beta/ci-163-0431a5f),
published from [Windows CI run #163](https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/actions/runs/163).
It contains a ready-to-copy `SudekiMP` folder with a standalone PE32 beta
launcher, raw loader, DLL, safe default configuration, XInput diagnostic and
focused Windows local-co-op toggle,
co-op save fixtures, launch script, and Windows guide—not Sudeki or any game
assets. The beta launcher supports pasted game paths, optional in-app project
music, recoverable co-op-save isolation, and a browser link to the public
manual-download page. See
[Windows build instructions](docs/windows-build.md) for
the supported game build and installation steps.

## Supported game build

SudekiMP currently supports one exact executable:

| Property | Value |
| --- | --- |
| Store/build | GOG offline build `50303954381148403` |
| Executable | `SUDEKI.exe` (PE32/x86) |
| SHA256 | `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94` |
| Static analysis | Ghidra 12.1.2 |
| Primary runtime | Wine on Linux |

The launcher refuses unknown executable builds. Runtime experiments are
disabled by default, signature-gated, reversible, and do not patch the game on
disk. The original vanilla installation remains separate from the writable
research copy; the complete 394-file baseline is hashed and reproducible.

## Current status

### Confirmed foundations

- The GOG installer, installed files, executable, and vanilla Wine baseline
  are documented and reproducible.
- `SudekiMP.dll` and `SudekiMP.Launcher.exe` build as PE32 binaries on Linux
  and safely reject an unsupported executable.
- Sudeki's Quick Menu slowdown is understood. A live-only prototype keeps the
  shared world simulation at `1.0x` while the menu remains logically active.
- Elco's Plasmatica path is traced from skill activation through targeting,
  animation, projectile launch, collision damage, invulnerability, and
  cleanup. Per-skill actor and cinematic-camera rates can be changed without
  changing global world time.
- Native real-time Skill Strike activation, consumable slots, and direct
  Spirit Strike activation have all been demonstrated.
- Sudeki's native AI override/default-control exports can release and restore
  an individual party member without removing that character's movement,
  targeting, or attack systems.

### Local co-op proofs

- Keyboard/mouse Player 1 and a Linux-controller Player 2 can move and attack
  independently in one Sudeki process.
- Player 2 input is transported from Linux joydev through a small loopback UDP
  bridge because the test controller is not exposed to Wine as XInput or
  DirectInput.
- A maximum-separation guard, shared midpoint camera, distinct render-only
  cameras, and alternating dual-camera split-screen compositor all have live
  proofs.
- A disabled-by-default native Player 2 obstruction prototype now binds the
  named camera to Player 2 and lets Sudeki's Exploration solver keep it out of
  outdoor world geometry. This first cut is collision parity, not input parity:
  while native Exploration is ready, Player 1 camera broadcasts are filtered
  and Player 2 right-stick orbit is disabled; the right stick still operates in
  the existing manual fallback/combat/unsupported phases.
- The compositor preserves native world rendering, culling, shadows, doors,
  and simulation by caching one complete native frame per engine frame rather
  than replaying the game renderer.
- Viewport-owned character names, HP, SP, companion ordering, and native
  portrait art are confirmed for the two-character prototype.
- The map pointer and heading now resolve the character assigned to each
  cached viewport instead of hard-coding party slot zero for both halves.
- The native Quit interface renders once at full width over the preserved
  split-screen background.
- Skills and Spirit Strikes can leave the other player's simulation and
  movement active at normal world speed. Camera containment and observer
  animation coverage remain incomplete for several moves.

### Sudeki Together front end

The New Game flow can now open a dedicated Sudeki Together roster page:

1. Choose Single Player or Co-op.
2. Assign distinct Player 1 and Player 2 characters.
3. Select Ailish, Tal, Buki, or Elco.
4. Persist the selected role contract in a sidecar profile.
5. Wait for a selected character when the story has not added them yet.

The page reuses Sudeki's native title fade, font submission, and four resident
character-portrait textures. No portrait file is extracted or committed. The
four-card presentation, Back navigation, duplicate-role rejection, cleanup,
and return to native New Game flow work.

The same page now includes `SudekiMP Settings`. Its first bounded settings
panel tunes the real Talos encounter for a locked co-op profile: explicit
enable/disable, `1x`–`4x` maximum HP, stagger limits, and stagger-session
duration. Settings persist in the existing sidecar. Vanilla values remain the
default and Single Player never receives the co-op override.

The Tal-to-Ailish roster handoff now activates the selected split roles without
duplicating Ailish's HUD portrait. Participation is separate from selection:
F10 drops Player 2 back to native AI and full-screen play without erasing the
locked character, and F10 again requests that same character. With the Linux
controller bridge, Start requests drop-in and holding Back+Start for one second
drops out. This is the first two-player implementation of a seat model intended
to scale to additional local or network players later.

An opt-in party-atomic transition prototype addresses Sudeki's single active
world. On an authored temporary-room entry or exit, it quiesces the second
viewport/control lease, lets Sudeki place the native lead, then calls the
engine's own formation-pop operation so every declared party member follows
with collision/navigation-aware offsets. Co-op is rebuilt only after the new
zone is stable. Failure leaves the game in usable native single-player mode,
retains the roster contract, and permits a later manual drop-in.

The travel-vote state machine and overlay remain available for isolated
research, but the adapter is disabled in the integrated lifecycle profile. A
live Player 2 veto proved that the current `EnterTemporaryZone` hook runs after
Sudeki has already started the native door approach/script; returning from that
void call without loading cannot roll the native task back and can leave the
world paused. The feature must remain off until a target-specific pre-OnAction
hook can defer the approach itself and replay that exact action only after
consent.

### Cleanroom and research tools

- Sudeki's shipped `testroom` can start with Ailish and expose an F8 research
  menu for party members, Training Dummy, combat mode, camera mode, infinite
  resources, and split-screen Player 2.
- The F7 traversal prototype distinguishes persistent worlds from temporary
  interiors and uses Sudeki's native `SetZoneNOW` and
  `EnterTemporaryZone` paths. It records actor-specific arrival anchors and
  fails closed for unknown destinations instead of inventing coordinates.
- The orange story-intro crash was traced to repeated accelerator-resource
  loading. An exact-build cache now lets the complete intro play normally.
- An opt-in F6 Story Test Boost is available while the roster prototype is
  installed. It starts off, runs engine/world time at the configured multiplier
  while the game is focused, and gives each current party member one lease from
  Sudeki's native refcounted invulnerability system. Normal speed is restored
  whenever title/loading/focus-loss state is observed; party protection is
  reconciled across rebuilt and newly joined party members.
- Cafu was confirmed as an unfinished Elco-derived developer variant, not a
  fifth complete party archetype. His native fire crash is contained, and an
  Elco pistol visual attaches correctly, but the missing projectile visual and
  malformed Cafu pistol remain asset-completion/SudekiForge work.

### Latest confirmed encounter: Talos

The natural final-battle transition normally collapses the party to Tal. The
local co-op lifecycle profile restores Ailish, Buki, and Elco through Sudeki's
native party lifecycle, preserving human-controlled co-op seats and returning
any remaining companions to native AI control.

The companions originally attacked Talos's clones but ignored the real boss.
Static and live analysis found the exact cause: the shared AI candidate
validator rejects AI-unit type `3` when an authored request bit is set. Real
Talos is type `3`; his clones are type `1`.

The accepted prototype clears that one request bit only in a temporary stack
copy, only for a restored retail companion evaluating the exact live real
Talos candidate. It does not change Talos, clone behavior, factions, target
pointers, damage, or persistent AI state. Live acceptance confirmed Ailish,
Buki, and Elco attack both the real Talos and his clones.

## Integration frontier

The project has proven the engine rails needed for local co-op, but the next
work is integration rather than adding more isolated features:

1. Validate roster lock-in through the real Tal-only-to-Ailish party arrival.
2. Confirm HUD, movement, camera, input, and AI ownership survive every role
   bind and party/level reconstruction.
3. Finish visible Player 2 locomotion during Pure Land and complete one
   uninterrupted two-player combat encounter.
4. Finish skill-camera startup, cinematic, and restoration containment.
5. Resolve ranged observer presentation: full-body locomotion, firing, aim,
   weapon attachment, and vanilla-compatible first-person-only reloads.
6. Build direct combat loadouts before attempting full per-player Quick Menu
   UI/state virtualization. Sudeki's Quick Menu is currently a global payload.
7. Add an optional shared-focus camera alongside split-screen.
8. Generalize doors, party transitions, cutscenes, quests, and scripted world
   progression.
9. Expand to three or four local players only after two-player play is stable.
10. Begin host-authoritative networking only after the local simulation and
    ownership model is reliable.

Known global or shared systems include Skill Targeting, Spirit Strike power,
Quick Menu state, some cinematic cameras and post-processing histories, and
parts of HUD/input presentation. They must be virtualized deliberately; the
project will not synchronize arbitrary process memory between machines.

## Build and verify on Linux

For the shortest supported two-player setup, start with the
[Linux Local Co-op Beta Quick Start](docs/linux-coop-beta.md), including the
desktop launcher at `./tools/sudekimp-beta-launcher.sh`. Install its Linux
desktop entry with `./tools/install-linux-launcher.sh`. The commands below
are the underlying developer and research workflow.

```bash
./tools/build-linux.sh
./tools/run-wine.sh --check
```

The build uses the configured MinGW-w64 environment and produces ignored
artifacts under `build/mingw32/bin/`.

For the dedicated working game and research saves:

```bash
./tools/continue-research.sh --check
./tools/continue-research.sh --safe
```

Useful focused modes include:

```bash
./tools/continue-research.sh --cleanroom
./tools/continue-research.sh --controller-bridge-test
./tools/continue-research.sh --realtime-skill-coop-test
./tools/continue-research.sh --zone-traversal-test
./tools/continue-research.sh --talos-party-test
```

Run `./tools/continue-research.sh --help` for the complete research-mode list.
Focused modes temporarily generate their required configuration and restore
the checked-in disabled defaults when the run ends.

## Build and run on Windows

Windows does not require Wine. The supported source-build path uses MSYS2's
32-bit `MINGW32` GCC environment because Sudeki and the injected DLL are PE32
and the low-level hooks use GNU calling-convention and inline-assembly
extensions.

The repository now provides a checked build helper and a PowerShell installer:

```text
tools/build-windows.sh
tools/install-windows.ps1
```

For a dedicated native-Windows build machine, see
[windows-gitea-runner.md](docs/windows-gitea-runner.md). It installs a
repository-scoped Actions runner and includes a manual smoke workflow; it does
not publish builds or run game injection from CI.

The installer verifies the exact GOG executable hash and copies only the
launcher, DLL, and configuration into a removable `SudekiMP` subdirectory. It
does not modify or redistribute the game. See
[docs/windows-build.md](docs/windows-build.md) for the complete setup,
preflight, launch, limitations, and eventual binary-release path.

Use the emergency stop helper if a live experiment stalls:

```bash
./tools/stop-sudeki.sh
```

## OBS recording

Research launches use Sudeki's native windowed option and, by default, start
Wine through the host's `obs-gamecapture`/OBSVkCapture hook. Because Sudeki is
PE32, both the 32-bit capture hook and the OBSVkCapture source are required.

1. Start OBS before Sudeki.
2. Add the OBSVkCapture **Game Capture** source.
3. Launch through `continue-research.sh`.
4. Confirm the source receives the Sudeki texture before recording.

Set `SUDEKIMP_DISABLE_OBS_GAMECAPTURE=true` only when capture injection must be
disabled for a diagnostic run. See [docs/recording.md](docs/recording.md).

## Documentation map

- [RECOVER.txt](RECOVER.txt) — operational checkpoint and safe resume notes
- [docs/project-status.md](docs/project-status.md) — planning status and open
  acceptance work
- [docs/milestones.md](docs/milestones.md) — milestone evidence and status
- [docs/research-log.md](docs/research-log.md) — chronological experiments,
  failures, and confirmations
- [docs/combat.md](docs/combat.md) — combat, skills, timing, and protection
- [docs/cleanroom.md](docs/cleanroom.md) — native testroom harness
- [docs/engine-functions.md](docs/engine-functions.md) — RVAs, ABIs, signatures,
  and confidence
- [docs/structures.md](docs/structures.md) — reverse-engineered structures
- [docs/player-statehood-design.md](docs/player-statehood-design.md) — player
  leases, interaction authority, shared inventory, and shop/blacksmith roadmap
- [docs/mod-loader.md](docs/mod-loader.md) — launcher, DLL, and hook lifecycle
- [docs/windows-build.md](docs/windows-build.md) — native Windows build,
  validation, installation, and launch
- [docs/windows-agent-handoff.md](docs/windows-agent-handoff.md) — bounded
  native-Windows agent scope and acceptance evidence
- [docs/recording.md](docs/recording.md) — windowed Wine and OBS capture
- [docs/sync.md](docs/sync.md) — private development mirror

## Repository policy

Do not commit or redistribute Sudeki installers, executables, archives,
extracted assets, saves, memory dumps containing game data, or Wine prefixes.
Keep the user-supplied vanilla tree read-only and run experiments against the
dedicated working copy. Reverse-engineered names and structures must remain
isolated and correctable as the engine model improves.
