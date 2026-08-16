# SudekiMP

SudekiMP is a research and modding project intended to add local co-op, and eventually online co-op, to the GOG Windows release of *Sudeki*. It does not contain or redistribute the game, its installer, or its assets. A legitimate user-supplied copy is required.

## Current milestone

Phases 0 and 1 are complete for GOG build `50303954381148403`. The three-part offline installer is hashed, the installed game has a complete 394-file SHA256 manifest, separate read-only `vanilla` and writable `working` trees verify byte-for-byte, and the user-confirmed vanilla gameplay baseline passes under Wine.

The exact executable is `SUDEKI.exe` (uppercase), SHA256 `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`. Ghidra 12.1.2 has imported and analyzed it. Milestones 1, 2, 4, and the local two-player movement proof (Milestone 5) are complete. Phase 5 fully traces Elco's Plasmatica task from `CSkill::Use` through animation, projectile, damage, and completion. See [docs/combat.md](docs/combat.md), [docs/engine-functions.md](docs/engine-functions.md), [docs/milestones.md](docs/milestones.md), and [RECOVER.txt](RECOVER.txt).

The Phase 4 foothold builds as PE32 `SudekiMP.dll` plus `SudekiMP.Launcher.exe`. The launcher safely rejects unknown builds. The DLL's disabled-by-default Quick Menu option has now been tested under Wine: live memory and menu-state capture confirmed normal world speed while the menu remained active, with no executable-file modification. See [docs/mod-loader.md](docs/mod-loader.md).

An additional disabled-by-default Plasmatica diagnostic/control hook has passed synthetic and inert-image Wine preflight tests. Live captures established the complete task lifetime, including targeting teardown, actor-forward missile launch, native collision damage, task-lifetime invulnerability, and ordered recovery. Milestone 2 is confirmed: independent 2.0x caster and camera rates shorten the cast while preserving all four authored camera angles at normal world simulation. Phase 6 found Sudeki's native `ac_QuickSkill0..5` real-time activation route, and the user confirmed Elco's `5` through `9` keys through a guarded native state transition. Native consumable slots `1` through `4` remain functional without modification. Spirit Strike has no native direct input action, but a disabled-by-default prototype now resolves the front character's authored pair and launches the selected variant through Sudeki's native validation, UI/control transition, activation, and cleanup path. Its default `G` key is configurable through `[Bindings] SpiritStrike`; a live Ailish test on an `H` override completed normally. Phase 8 and Milestone 4 are also confirmed: vanilla transfers a nested AI-active mode with the controller target, while Sudeki's exported refcounted control APIs can independently disable and restore non-front Buki AI without moving that target. Milestone 5 proved simultaneous independent movement for Player 1 and AI-overridden Buki in one process. The next live battle confirmed independent Buki weak attacks through her own arbiter while another character remained Player 1, and a passive field trace confirmed native targeting state remained active with Buki's AI disabled. Following tests confirmed Buki's separate movement rotates with Player 1's shared camera and that an outward-only 10-unit boundary prevents further separation while preserving inward and sideways movement. The disabled midpoint prototype is now live-confirmed: Sudeki's camera followed the space between Ailish and independently moved Buki, then immediately restored native Ailish focus and Buki AI.

The first dual-viewport proof is also live-confirmed: Sudeki rendered two side-by-side gameplay views in one process. Early render-replay versions exposed black shadow/visibility corruption, missing door geometry, a split title/menu, and frozen actors; those paths are rejected. The replacement compositor copies complete native frames after `EndScene`, alternates isolated Ailish/Buki render states, and presents clean character-centered views without replaying simulation, culling, shadows, doors, or callbacks. The integrated test combines those cameras with independently movable Buki and the separation guard. The Quit menu now renders once at full width over the frozen camera pair. Viewport-owned HUD names, HP, SP, companion order, and portrait art are also live-confirmed. Portraits use Sudeki's lower-level synchronous cycle-icon resource selector rather than its broader HUD refresh. Sudeki's internal Ailish/Buki party-order rotation is treated as presentation state, so it no longer destroys Camera 2, invalidates both caches, briefly unsplits the view, or pulses the minimap. The final log recorded one camera acquisition, active per-viewport HUD/portrait ownership, one tolerated order rotation, and no release/reacquire loop. Per-player camera input, aspect/projection tuning, additional shared menus/cinematics, and broader party combinations remain later work. Free-roam camera usability remains unresolved.

The next integrated combat prototype is implemented and awaiting live confirmation. It keeps the shared simulation at native `1.0x`, tracks a combat context for each local player, exposes four character-owned native Skill Strike activations for Player 2 on `F1..F4`, and leaves Player 1 on Sudeki's native `5..8` route. The exact build has one proven global Skill Targeting node, so activations are rejected only while that target-selection window is occupied; executions may overlap after confirmation. Plasmatica's `SetRenderCamera` request is routed to the caster's cached viewport while the other viewport retains its normal player camera. Native skill availability, validation, SP cost, targeting, animation, projectiles, damage, protection, and cleanup remain unchanged. The new code passes PE32 build, ABI/state-machine tests, inline-hook restoration tests, and exact-image installation/restoration preflight; no live gameplay result is claimed yet.

The first physical Player 2 controller route is now live-confirmed on Linux. Because the connected Razer Raiju Mobile is visible as `/dev/input/js0` but not as XInput or DirectInput inside Wine, `sudekimp-input-bridge` reads Linux joydev events and sends a versioned 32-byte state packet to the DLL over UDP loopback. The receiver accepts only `127.0.0.1`, neutralizes stale input after 250 ms, and never modifies a device or game file. In the focused test, keyboard/mouse retained Player 1 while the Raiju left stick independently moved AI-overridden Buki with analog speed. The log recorded stable character/arbiter ownership, clean stops, and two A-button weak-attack submissions. This castle save disallowed visible combat, so the A-to-attack animation still needs an arena check. Both sticks, triggers, D-pad, and standard buttons are transported. Player 2 selection is now party-generic: F10 targets the first non-front active party member, matching Camera 2, so Tal/Ailish and other two-character saves can use the same bridge; that broader pairing is awaiting live confirmation.

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
./tools/continue-research.sh --cleanroom
./tools/continue-research.sh --test-arena
./tools/continue-research.sh --trace
./tools/continue-research.sh --second-player-attack-test
./tools/continue-research.sh --second-player-camera-movement-test
./tools/continue-research.sh --second-player-separation-test
./tools/continue-research.sh --shared-group-camera-test
./tools/continue-research.sh --split-screen-render-test
./tools/continue-research.sh --viewport-hud-test
./tools/continue-research.sh --controller-bridge-test
./tools/continue-research.sh --realtime-skill-coop-test
./tools/continue-research.sh --speed-test 2.0
./tools/continue-research.sh --spirit-strike-test
./tools/continue-research.sh --spirit-strike-test H
```

The resume helper requires the 11-slot research save directory, rebuilds and verifies the exact supported executable, prints the current checkpoint and next targets, and restores the generated configuration to disabled defaults when the run ends. `--cleanroom` (also available through the older `--test-arena` alias) passes Sudeki's shipped `-Level testroom -DT 1 -Ailish 1` startup options. Ailish is the lead; F8 opens the cleanroom-only party/dummy, combat/camera, infinite-resource, and optional split-screen Player 2 controls. The cleanroom's right-side badge reports `P2 READY` only when a second character is independently controlled and the Razer/Linux input bridge is live. Once the native managers are ready, the cleanroom also fills Sudeki's developer inventory, unlocks all party Spirit Strikes, repairs missing maximum HP/SP values, and equips a character's starter weapon only when its native weapon slot is empty. This does not extract, repack, or alter the game's archive. The compositor test temporarily changes native anti-aliasing to `0` and restores its original numeric value when the game exits. `--controller-bridge-test` starts and stops only the repository-built Linux helper; override `/dev/input/js0` with `SUDEKIMP_INPUT_DEVICE=/dev/input/jsN` if needed. `--speed-test` is an explicit reproduction mode, not the default balance configuration.

Research launches now use Sudeki's native windowed option so the game remains a normal desktop window after focus changes. They also run Wine through this host's existing `obs-gamecapture`/OBSVkCapture integration—the same prefix command used by its Lutris games—so the Flatpak OBS **Game Capture** source can receive Sudeki's OpenGL frames. **Window Capture (PipeWire)** remains a fallback. See [docs/recording.md](docs/recording.md).

## Repository policy

Only original source, documentation, signatures, configuration, hooks, and tooling belong here. User-supplied installers, installed game files, archive contents, memory dumps containing game data, and other copyrighted assets must remain outside version control.
