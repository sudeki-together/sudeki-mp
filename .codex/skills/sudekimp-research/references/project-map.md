# SudekiMP project map

Use this map to route research. The repository documents remain the authoritative detailed record; do not duplicate their full contents into the skill.

| Path | Load when working on |
| --- | --- |
| `RECOVER.txt` | Current recovery checkpoint, safety state, resume commands, and next experiments |
| `README.md` | Public project status, supported build, launch modes, and repository policy |
| `docs/milestones.md` | Phase/milestone acceptance state and outstanding validations |
| `docs/research-log.md` | Chronological evidence, failed passes, live observations, and latest experimental details |
| `docs/executable.md` | PE architecture, image base, executable hash, and import/static facts |
| `docs/installer.md` | Offline installer identity, extraction/install baseline, and hashes |
| `docs/mod-loader.md` | Launcher/DLL architecture, configuration, exact-build gates, logging, and hook lifecycle |
| `docs/engine-functions.md` | Named/provisional engine functions, RVAs, signatures, ABIs, callers, and confidence |
| `docs/structures.md` | Reverse-engineered object layouts, fields, ownership, and confidence |
| `docs/combat.md` | Quick Menu time scaling, Plasmatica, direct skills, Spirit Strikes, protection, and combat contexts |
| `docs/cleanroom.md` | Native testroom boot, party/dummy controls, resources, split-screen toggle, and cleanroom validation |
| `docs/recording.md` | Windowed Wine and OBSVkCapture/Game Capture workflow |
| `research/signatures/` | Compact exact-build signatures for proven hook locations |
| `tools/ghidra/` | Reproducible exact-image static-analysis reports |
| `src/hooks/` | Runtime control, skill, camera, compositor, HUD, presentation, and timing hooks |
| `src/cleanroom/` | Cleanroom overlay and native test-fixture integration |
| `src/engine/` | Isolated engine-facing helpers and correctable reverse-engineered abstractions |
| `tests/` | ABI, parser, state-machine, exact-image install/restore, and regression checks |
| `tools/continue-research.sh` | Focused launch modes and temporary configuration ownership |
| `tools/stop-sudeki.sh` | Emergency process stop for a stuck live test |

## Stable architectural facts

- Supported executable: uppercase `SUDEKI.exe`, GOG build `50303954381148403`, PE32 x86, image base `0x00400000`.
- Supported SHA256: `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
- The launcher and hooks must reject unknown builds safely; repository defaults keep experiments disabled.
- Milestones 1, 2, 4, and 5 are proven: normal-speed Quick Menu, independently timed Plasmatica, independently disabled/restored AI, and two simultaneous human input paths.
- Clean split-screen, distinct assigned cameras, Player 2 controller movement, maximum separation, shared full-width Quit UI, and viewport-owned HUD data/portrait art have live proofs.
- Sudeki often contains multiplayer-shaped seams—per-character arbiters, native AI overrides, independently renderable party actors, targeters, and camera targets—but that is not proof every global system is safe for concurrency.

## Current frontier

The latest source is testing viewport-owned ranged presentation in the cleanroom. For Tal Player 1 and Ailish Player 2, Ailish's own combat viewport should use first-person arms/weapon while Tal's observing viewport sees her complete animated world body; leaving combat should restore her preserved third-person orbit. This build passed compilation and exact-image checks but still needs the user's live visual confirmation.

Known adjacent open defects include intermittent HUD ownership after role changes, world-action animation translation for a ranged Player 1 observed from another viewport, shared Spirit Strike temporal/history effects, global actor freeze during Spirit presentations, and remaining real-time concurrent Skill Strike acceptance tests.

Always re-read the tail of `docs/research-log.md` before trusting this frontier paragraph; it is a convenience summary, not a replacement for the living log.
