# SudekiMP

SudekiMP is a research and modding project intended to add local co-op, and eventually online co-op, to the GOG Windows release of *Sudeki*. It does not contain or redistribute the game, its installer, or its assets. A legitimate user-supplied copy is required.

## Current milestone

Phases 0 and 1 are complete for GOG build `50303954381148403`. The three-part offline installer is hashed, the installed game has a complete 394-file SHA256 manifest, separate read-only `vanilla` and writable `working` trees verify byte-for-byte, and the user-confirmed vanilla gameplay baseline passes under Wine.

The exact executable is `SUDEKI.exe` (uppercase), SHA256 `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`. Ghidra 12.1.2 has imported and analyzed it. Milestones 1, 2, 4, and the local two-player movement proof (Milestone 5) are complete. Phase 5 fully traces Elco's Plasmatica task from `CSkill::Use` through animation, projectile, damage, and completion. See [docs/combat.md](docs/combat.md), [docs/engine-functions.md](docs/engine-functions.md), [docs/milestones.md](docs/milestones.md), and [RECOVER.txt](RECOVER.txt).

The Phase 4 foothold builds as PE32 `SudekiMP.dll` plus `SudekiMP.Launcher.exe`. The launcher safely rejects unknown builds. The DLL's disabled-by-default Quick Menu option has now been tested under Wine: live memory and menu-state capture confirmed normal world speed while the menu remained active, with no executable-file modification. See [docs/mod-loader.md](docs/mod-loader.md).

An additional disabled-by-default Plasmatica diagnostic/control hook has passed synthetic and inert-image Wine preflight tests. Live captures established the complete task lifetime, including targeting teardown, actor-forward missile launch, native collision damage, task-lifetime invulnerability, and ordered recovery. Milestone 2 is confirmed: independent 2.0x caster and camera rates shorten the cast while preserving all four authored camera angles at normal world simulation. Phase 6 found Sudeki's native `ac_QuickSkill0..5` real-time activation route, and the user confirmed Elco's `5` through `9` keys through a guarded native state transition. Native consumable slots `1` through `4` remain functional without modification. Spirit Strike has no native direct input action, but a disabled-by-default prototype now resolves the front character's authored pair and launches the selected variant through Sudeki's native validation, UI/control transition, activation, and cleanup path. Its default `G` key is configurable through `[Bindings] SpiritStrike`; a live Ailish test on an `H` override completed normally. Phase 8 and Milestone 4 are also confirmed: vanilla transfers a nested AI-active mode with the controller target, while Sudeki's exported refcounted control APIs can independently disable and restore non-front Buki AI without moving that target. Milestone 5 proved simultaneous independent movement for Player 1 and AI-overridden Buki in one process. The next live battle confirmed independent Buki weak attacks through her own arbiter while another character remained Player 1, and a passive field trace confirmed native targeting state remained active with Buki's AI disabled. Following tests confirmed Buki's separate movement rotates with Player 1's shared camera and that an outward-only 10-unit boundary prevents further separation while preserving inward and sideways movement. The disabled midpoint prototype is now live-confirmed: Sudeki's camera followed the space between Ailish and independently moved Buki, then immediately restored native Ailish focus and Buki AI. Zoom remains native, and a second viewport/render camera has not yet been investigated. Free-roam camera usability remains unresolved.

## Build on Linux

```bash
./tools/build-linux.sh
./tools/run-wine.sh --check
./tools/run-wine.sh --windowed
```

Build output is ignored under `build/mingw32/bin/`. The scripts use the per-user MinGW-w64 Flatpak SDK and the dedicated Sudeki Wine prefix by default.

To resume research without accidentally using the three-save offline prefix:

```bash
./tools/continue-research.sh --safe
./tools/continue-research.sh --trace
./tools/continue-research.sh --second-player-attack-test
./tools/continue-research.sh --second-player-camera-movement-test
./tools/continue-research.sh --second-player-separation-test
./tools/continue-research.sh --shared-group-camera-test
./tools/continue-research.sh --speed-test 2.0
./tools/continue-research.sh --spirit-strike-test
./tools/continue-research.sh --spirit-strike-test H
```

The resume helper requires the 11-slot research save directory, rebuilds and verifies the exact supported executable, prints the current checkpoint and next targets, and restores the generated configuration to disabled defaults when the run ends. `--speed-test` is an explicit reproduction mode, not the default balance configuration.

Research launches now use Sudeki's native windowed option so the game remains a normal desktop window after focus changes. They also run Wine through this host's existing `obs-gamecapture`/OBSVkCapture integration—the same prefix command used by its Lutris games—so the Flatpak OBS **Game Capture** source can receive Sudeki's OpenGL frames. **Window Capture (PipeWire)** remains a fallback. See [docs/recording.md](docs/recording.md).

## Repository policy

Only original source, documentation, signatures, configuration, hooks, and tooling belong here. User-supplied installers, installed game files, archive contents, memory dumps containing game data, and other copyrighted assets must remain outside version control.
