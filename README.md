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
- The compositor preserves native world rendering, culling, shadows, doors,
  and simulation by caching one complete native frame per engine frame rather
  than replaying the game renderer.
- Viewport-owned character names, HP, SP, companion ordering, and native
  portrait art are confirmed for the two-character prototype.
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

The critical remaining validation is the handoff from a saved roster contract
to gameplay ownership. A Tal-only opening must remain native until Ailish joins
the party, then atomically bind controller input, AI ownership, HUD, and
cameras to the locked roles. The implementation scaffolding exists, but that
Tal-to-Ailish story transition still requires end-to-end live acceptance.

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
- Cafu was confirmed as an unfinished Elco-derived developer variant, not a
  fifth complete party archetype. His native fire crash is contained, and an
  Elco pistol visual attaches correctly, but the missing projectile visual and
  malformed Cafu pistol remain asset-completion/SudekiForge work.

### Latest confirmed encounter: Talos

The natural final-battle transition normally collapses the party to Tal. The
current research mode restores Ailish, Buki, and Elco through Sudeki's native
party lifecycle and returns them to native AI control.

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
- [docs/mod-loader.md](docs/mod-loader.md) — launcher, DLL, and hook lifecycle
- [docs/recording.md](docs/recording.md) — windowed Wine and OBS capture
- [docs/sync.md](docs/sync.md) — private development mirror

## Repository policy

Do not commit or redistribute Sudeki installers, executables, archives,
extracted assets, saves, memory dumps containing game data, or Wine prefixes.
Keep the user-supplied vanilla tree read-only and run experiments against the
dedicated working copy. Reverse-engineered names and structures must remain
isolated and correctable as the engine model improves.
