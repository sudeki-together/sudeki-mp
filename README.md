# SudekiMP

SudekiMP is a research and modding project intended to add local co-op, and eventually online co-op, to the GOG Windows release of *Sudeki*. It does not contain or redistribute the game, its installer, or its assets. A legitimate user-supplied copy is required.

## Current milestone

Phases 0 and 1 are complete for GOG build `50303954381148403`. The three-part offline installer is hashed, the installed game has a complete 394-file SHA256 manifest, separate read-only `vanilla` and writable `working` trees verify byte-for-byte, and the user-confirmed vanilla gameplay baseline passes under Wine.

The exact executable is `SUDEKI.exe` (uppercase), SHA256 `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`. Ghidra 12.1.2 has imported and analyzed it. Milestones 1, 2, 4, and the local two-player movement proof (Milestone 5) are complete. Phase 5 fully traces Elco's Plasmatica task from `CSkill::Use` through animation, projectile, damage, and completion. See [docs/combat.md](docs/combat.md), [docs/engine-functions.md](docs/engine-functions.md), [docs/milestones.md](docs/milestones.md), and [RECOVER.txt](RECOVER.txt).

The Phase 4 foothold builds as PE32 `SudekiMP.dll` plus `SudekiMP.Launcher.exe`. The launcher safely rejects unknown builds. The DLL's disabled-by-default Quick Menu option has now been tested under Wine: live memory and menu-state capture confirmed normal world speed while the menu remained active, with no executable-file modification. See [docs/mod-loader.md](docs/mod-loader.md).

An additional disabled-by-default Plasmatica diagnostic/control hook has passed synthetic and inert-image Wine preflight tests. Live captures established the complete task lifetime, including targeting teardown, actor-forward missile launch, native collision damage, task-lifetime invulnerability, and ordered recovery. Milestone 2 is confirmed: independent 2.0x caster and camera rates shorten the cast while preserving all four authored camera angles at normal world simulation. Phase 6 found Sudeki's native `ac_QuickSkill0..5` real-time activation route, and the user confirmed Elco's `5` through `9` keys through a guarded native state transition. Native consumable slots `1` through `4` remain functional without modification. Spirit Strike has no native direct input action, but a disabled-by-default prototype now resolves the front character's authored pair and launches the selected variant through Sudeki's native validation, UI/control transition, activation, and cleanup path. Its default `G` key is configurable through `[Bindings] SpiritStrike`; a live Ailish test on an `H` override completed normally. Phase 8 and Milestone 4 are also confirmed: vanilla transfers a nested AI-active mode with the controller target, while Sudeki's exported refcounted control APIs can independently disable and restore non-front Buki AI without moving that target. Milestone 5 then proved simultaneous independent movement for Player 1 and AI-overridden Buki in one process. Free-roam camera usability remains unresolved after several input-routing experiments; the next camera target is the engine's actual desired-distance/profile update path, not more controller-field remapping.

## Build on Linux

```bash
./tools/build-linux.sh
./tools/run-wine.sh --check
./tools/run-wine.sh
```

Build output is ignored under `build/mingw32/bin/`. The scripts use the per-user MinGW-w64 Flatpak SDK and the dedicated Sudeki Wine prefix by default.

To resume research without accidentally using the three-save offline prefix:

```bash
./tools/continue-research.sh --safe
./tools/continue-research.sh --trace
./tools/continue-research.sh --speed-test 2.0
./tools/continue-research.sh --spirit-strike-test
./tools/continue-research.sh --spirit-strike-test H
```

The resume helper requires the 11-slot research save directory, rebuilds and verifies the exact supported executable, prints the current checkpoint and next targets, and restores the generated configuration to disabled defaults when the run ends. `--speed-test` is an explicit reproduction mode, not the default balance configuration.

## Repository policy

Only original source, documentation, signatures, configuration, hooks, and tooling belong here. User-supplied installers, installed game files, archive contents, memory dumps containing game data, and other copyrighted assets must remain outside version control.
