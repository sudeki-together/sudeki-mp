# SudekiMP

SudekiMP is a research and modding project intended to add local co-op, and eventually online co-op, to the GOG Windows release of *Sudeki*. It does not contain or redistribute the game, its installer, or its assets. A legitimate user-supplied copy is required.

## Current milestone

Phases 0 and 1 are complete for GOG build `50303954381148403`. The three-part offline installer is hashed, the installed game has a complete 394-file SHA256 manifest, separate read-only `vanilla` and writable `working` trees verify byte-for-byte, and the user-confirmed vanilla gameplay baseline passes under Wine.

The exact executable is `SUDEKI.exe` (uppercase), SHA256 `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`. Ghidra 12.1.2 has imported and analyzed it. Milestone 1 and the Phase 4 DLL foothold are complete. Phase 5 is tracing Elco's Plasmatica task from `CSkill::Use` through animation, projectile, damage, and completion. See [docs/combat.md](docs/combat.md), [docs/engine-functions.md](docs/engine-functions.md), and [docs/executable.md](docs/executable.md).

The Phase 4 foothold builds as PE32 `SudekiMP.dll` plus `SudekiMP.Launcher.exe`. The launcher safely rejects unknown builds. The DLL's disabled-by-default Quick Menu option has now been tested under Wine: live memory and menu-state capture confirmed normal world speed while the menu remained active, with no executable-file modification. See [docs/mod-loader.md](docs/mod-loader.md).

An additional disabled-by-default Plasmatica diagnostic logger has passed synthetic and inert-image Wine preflight tests. Live captures established the task lifetime and identified Plasmatica as compiled script `PC_Elco1__Skill|P`, including its targeting, camera, animation, sound, and completion-wait call path. The wait wrapper is confirmed to call `CNewGameModelAnimation::TsaIsPlaying` on Elco. Projectile and damage timing still require narrower tracing.

## Build on Linux

```bash
./tools/build-linux.sh
./tools/run-wine.sh --check
./tools/run-wine.sh
```

Build output is ignored under `build/mingw32/bin/`. The scripts use the per-user MinGW-w64 Flatpak SDK and the dedicated Sudeki Wine prefix by default.

## Repository policy

Only original source, documentation, signatures, configuration, hooks, and tooling belong here. User-supplied installers, installed game files, archive contents, memory dumps containing game data, and other copyrighted assets must remain outside version control.
