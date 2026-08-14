# Minimal mod foothold

## Scope

Phase 4 currently produces two original 32-bit Windows PE artifacts:

- `SudekiMP.dll`: validates the loaded game, scans for confirmed signatures, reads configuration, optionally changes the Quick Menu mode request, and can install an opt-in Plasmatica diagnostic logger.
- `SudekiMP.Launcher.exe`: validates `SUDEKI.exe`, starts it suspended, loads the DLL without altering game files, calls the exported initializer, resumes only after initialization succeeds, and remains attached until the game exits.

`EnableQuickMenuNormalSpeed` defaults to `false`, and the DLL logs both the requested and applied state. When explicitly enabled, it changes only the immediate mode value in the confirmed Quick Menu activation instruction from `1` to `0`. It does not modify `SUDEKI.exe` on disk.

`EnablePlasmaticaTrace` also defaults to `false`. It is diagnostic instrumentation, not a gameplay patch. When enabled, it logs the activation lifetime and script-facing animation, camera, scripted-missile, direct-damage, and animation-speed calls made while the selected `SkillData` name is exactly `Plasmatica`. Relevant records include elapsed milliseconds from activation so normal-speed and adjusted-speed casts can be compared. All wrapper calls forward the original object, arguments, and return value unchanged.

`EnablePlasmaticaAnimationSpeed` defaults to `false`. The experimental `PlasmaticaAnimationSpeed` value is accepted only in the range `0.25` through `4.0`; invalid enabled configuration fails safely. The hook arms only when the confirmed Plasmatica task pushes `ANIMID_SKILL_02`, captures the native object at the interpreter's exact binding-dispatch call, and rejects it unless its vtable is the supported build's concrete `CNewMissileAimingGameModelAnimation` vtable. A successful application restores the model multiplier that existed before the cast. It does not alter the global game-speed state.

`EnableQuickSkillInputTrace` and `EnableRangedQuickSkillPrototype` default to `false`. The first observes Sudeki's native `ac_QuickSkill0..5` route. The second uses the game's own UI/control transition only when a ranged QuickSkill fails in the exact `Armed | Strafing` state, then retries through the unchanged validator after 75 ms. It does not write character-state flags or SP.

`EnableDirectSpiritStrikePrototype` defaults to `false`. When explicitly enabled, `[Bindings] SpiritStrike` (default `G`) submits a resolved definition through the native validator, a 75 ms native UI/control transition, a second validation, and the native activation implementation. The first parser accepts a single letter, digit, `F1..F24`, named navigation/numpad/punctuation key, or `Mouse1..Mouse5`; invalid enabled configuration fails initialization safely. Chords and controllers are intentionally deferred. `SpiritStrikeId=-1` derives the pair from the front character and uses `SpiritStrikeVariant=1|2`; a fixed ID `0..15` remains available for diagnosis. A live Ailish automatic-resolution test selected type `0x01`/ID `2` and completed normally. Native validation also rejected repeated activation attempts while a strike was active.

`EnableControlSeparationPrototype` defaults to `false`. When enabled, `[Bindings] ToggleBukiAi` (default `J`) targets Buki only when she is not the front/controller character. It calls Sudeki's exported refcounted `AiOverrideControl` and `AiDefaultControl` functions, verifies both the override count and nested AI mode, and leaves the global controller target unchanged. Two live disable/restore cycles passed: Buki stopped with refcount/mode `1/0`, then resumed normal AI at `0/1`.

`EnableSecondPlayerMovementPrototype` also defaults to `false` and requires the control-separation prototype. While Buki's verified native override is active, it submits normalized fixed-world-axis `I/J/K/L` directions to Buki's own arbiter at RVA `0x000DAE80`; Player 1's normal controller and `W/A/S/D` route remain untouched. The dedicated test launcher temporarily moves `ToggleBukiAi` to `F10` to avoid overlap with `J`. Live testing confirmed independent two-character movement and a clean native AI restore. The prototype refuses to act when Buki is the controller target or no longer belongs to the active group.

`EnableSecondPlayerCameraRelativeMovementPrototype` defaults to `false` and requires the movement prototype. It passes the local `I/J/K/L` vector through Sudeki's own callee-cleaned movement-camera transform at RVA `0x000291A0`, clears vertical motion, normalizes the horizontal result, and then uses the already-proven Buki arbiter path. Its exact entry signature and inert-image installation pass; live directional behavior remains pending.

`EnableSecondPlayerSeparationGuardPrototype` defaults to `false` and also requires movement. `SecondPlayerMaximumSeparation` defaults to an experimental `10.0` units. The guard reads both characters' native `CPosition` objects, blocks only movement whose dot product points farther outward once the horizontal limit is reached, and always permits movement back toward Player 1. It does not teleport, accelerate, or take control of either character. The value is a test starting point rather than a balance decision; live confirmation remains pending.

`EnableSecondPlayerWeakAttackPrototype` defaults to `false` and requires the same control-separation prototype. While Buki's verified native override is active, rising edges from `[Bindings] SecondPlayerWeakAttack` (default `U`) submit only the weak-attack state to Buki's own arbiter through RVA `0x000DB0E0`; all other combat states are zero. The ABI adapter and exact-image install/restore tests pass under Wine. In the live proof the user completed a battle while one stable Buki arbiter repeatedly entered the native `IsAttacking` state, and nearest-target lock-on remained visibly active. Target selection and all attack/state validation remain native.

`EnableSecondPlayerTargetTrace` defaults to `false`. While Buki's verified override is active and Sudeki owns the foreground, it samples her unchanged `CTargeter` at no more than 10 Hz and logs only changes to the ordinary target node and auto-target flag together with Buki's forward vector. The trace never calls targeting logic and never assigns, clears, scores, or replaces a target. Build, inert-image preflight, and the first live capture pass.

`EnablePlayerMovementTrace` defaults to `false`. It wraps only the two exact calls from the global controller's normal movement consumer to the per-character arbiter movement routine, samples the unchanged world direction and movement parameters, and forwards immediately. The live trace confirmed the expected normalized horizontal direction, speed, turn rate, movement mode, and controlled character. It does not synthesize or redirect movement.

`EnableFreeRoamCameraModifierPrototype` defaults to `false` and remains an unsuccessful diagnostic prototype. It gates native mouse-Y `CameraU/CameraD` events outside combat behind configurable `[Bindings] FreeRoamCameraModifier` (`LeftCtrl` by default), while leaving combat input unchanged. The modifier edge was observed live, but the user did not obtain a useful camera result. Earlier wheel remap, held-pulse, and late controller-field injection attempts also failed visibly. This option must not be treated as a completed camera feature; further work belongs at the native desired-distance/profile update path.

`tools/continue-research.sh --spirit-strike-test H` temporarily selects `H` in the generated test configuration, then restores the repository default after Sudeki exits. This provides a direct live check that the configured key—not a compiled-in `G` constant—drives activation.

`tools/continue-research.sh --control-separation-test` temporarily enables only the guarded Buki toggle and restores the generated configuration after Sudeki exits.

`tools/continue-research.sh --second-player-movement-test` temporarily enables the guarded Buki toggle and second movement source, uses `F10` for the toggle, and restores the generated configuration after Sudeki exits.

`tools/continue-research.sh --second-player-attack-test` adds the disabled weak-attack prototype to that setup and binds it to `U`. Control anyone except Buki, use `F10` to acquire/release the native AI override, move Buki with `I/J/K/L`, and tap `U` once to request her weak attack.

`tools/continue-research.sh --second-player-camera-movement-test` selects only the shared-camera movement follow-up. Its live run confirmed that Buki's independent input rotates with Player 1's shared camera while Player 1 retains camera focus. `--second-player-separation-test` additionally enables the experimental 10-unit outward-only guard; that guard remains pending live confirmation.

`tools/continue-research.sh --second-player-target-trace` enables only Buki's native AI override plus the passive target trace. It is intended to compare target changes before attacks, during Player 1 combat, and around enemy death without synthesizing Player 2 input. The first live run retained one stable non-null node and the enabled auto-target flag throughout the override, then restored Buki's AI cleanly.

That live check passed: initialization logged virtual key `0x48`, and pressing `H` completed the same validated Ailish Spirit Strike with activation result `1`.

## Linux build

The current host uses these per-user Flatpak components:

```text
org.ghidra_sre.Ghidra 12.1.2
org.freedesktop.Sdk.Extension.mingw-w64 14.0.0 (25.08)
```

Build with:

```bash
./tools/build-linux.sh
```

Outputs are placed under ignored directory `build/mingw32/bin/`.

The build is configured for PE32/i386 only. CMake fails configuration if the selected Windows target is not 32-bit.

## Preflight and launch

Validate without launching:

```bash
./tools/run-wine.sh --check
```

Launch the default working copy:

```bash
./tools/run-wine.sh
```

Or provide another exact executable path:

```bash
./tools/run-wine.sh /path/to/working/SUDEKI.exe
```

Environment overrides:

```text
SUDEKIMP_GAME
SUDEKIMP_WINEPREFIX
```

The Linux wrapper converts absolute Unix paths to Wine `Z:\...` paths without requiring a separate `winepath` command.

## Safety gates

Before process creation, the launcher requires all of:

```text
SHA256 = 8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94
machine = IMAGE_FILE_MACHINE_I386
COFF timestamp = 0x534D1533
SizeOfImage = 0x0045F000
SudekiMP.dll exists
```

The DLL independently repeats the disk hash and PE checks, validates the loaded image, finds the relocation-tolerant Quick Menu signature exactly once in the loaded `.text` section, and remains inert on any mismatch.

The runtime change additionally verifies the complete seven-byte instruction
`C7 40 24 01 00 00 00` immediately before writing. A protection, write,
verification, or protection-restoration failure prevents the suspended game
from being resumed.

The Plasmatica trace has separate gates:

```text
CSkill::Use caller       RVA 0x000998A1 -> expected RVA 0x000B4810
CSkill::StopRumble call  RVA 0x000B4F23 -> expected RVA 0x000B50D0
14 export-table slots    each must contain its exact expected function RVA
```

Installation occurs while the primary Sudeki thread is still suspended. A wrong opcode, call target, or export RVA aborts initialization. Partial installation is rolled back, and the launcher does not resume the child after any initialization error.

The export-table hooks are used because Sudeki exposes the relevant native methods for script binding; Ghidra found no ordinary internal call references to those method entries beyond the export table. The logger does not overwrite the method bodies.

A deliberately wrong PE was tested: the launcher printed its different SHA256, returned exit code `2`, and did not create a process.

## Wine suspended-process bootstrap

Wine returned `ERROR_PARTIAL_COPY` when the launcher attempted to enumerate modules in a newly suspended child, and `kernel32!LoadLibraryW` was not yet mapped at a validated shared address. The launcher therefore has a Wine-compatible fallback:

1. Confirm that `ntdll!LdrLoadDll` exists at the same executable address in launcher and target.
2. Compare the first 16 instruction bytes across both processes.
3. Allocate remote data and a 30-byte x86 call thunk.
4. Change the thunk page from writable to execute/read and flush its instruction cache.
5. Run it, read the returned module handle, and free both temporary allocations.
6. Invoke `SudekiMP_Initialize`, then resume the game's primary thread only on success.

This is runtime injection, not an executable-file patch. A failure while the child remains suspended causes only that newly created child to be terminated.

## Confirmed initialization log

The first successful Wine load produced:

```text
SudekiMP 0.1.0
event=process_attach
module_base=0x7ac20000
game_sha256=8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94
quick_menu_signature_matches=1
quick_menu_signature_rva=0x00098ee5
quick_menu_patch_requested=false
quick_menu_patch_applied=false
status=ready
```

The log is written beside the game executable. Runtime module bases may differ between launches and must never be hard-coded.

## Confirmed enabled experiment

With the generated test configuration temporarily enabled, initialization logged:

```text
quick_menu_patch_requested=true
quick_menu_patch_applied=true
status=ready
```

While the user held the Quick Menu visibly open, a read-only debugger capture confirmed:

```text
activation instruction = C7 40 24 00 00 00 00
Quick Menu +0x29      = 1
Quick Menu +0xFC      = 1
CGameSpeed current    = 0
CGameSpeed requested  = 0
CGameSpeed pause      = 0
```

The user played with the option enabled and observed normal-speed combat while the menu was open. The resulting difficulty was expected and is why this option is an engine proof rather than the final combat design. The generated and repository configurations were returned to `false` after capture.

## Plasmatica trace preflight

Before the first live trace, two standalone Wine tests passed:

- `SudekiMP.CallHookTest.exe` used synthetic executable memory to verify install, rejection, and restoration behavior for relative-call and export-slot hooks.
- `SudekiMP.SkillTraceImageTest.exe` opened the exact user-supplied `SUDEKI.exe` read-only, mapped its PE sections into inert memory, installed four call hooks, fourteen export-slot hooks, and the script opcode `0x27` and object-method opcode `0x28` pointer hooks, verified them, uninstalled them, and verified every original target was restored.

The generated x86 assembly was also checked for ABI compatibility. The fastcall bridge receives Sudeki's `this` pointer in `ECX`, treats `EDX` as an ignored bridge register, preserves the original stack-argument positions, and uses the matching callee cleanup sizes.

The disabled-by-default `EnablePlasmaticaCameraSpeed` experiment is narrower than a native code patch. During the exact accepted Plasmatica camera setup, the script-method hook validates the primary thread, `StartCam` bytecode operand and hash, complete stack shape, animation name, mode, and original `1.0` rate before replacing only that invocation's float. Unknown builds are rejected before hook installation, and a mismatch leaves the call untouched.

Two live Plasmatica casts completed normally with this logger and no debugger attached. Both produced `begin`, successful `use_return`, and `end` records, but no export-slot wrapper events. This does not show that the underlying engine functions were unused; it shows that rewriting those export slots after loader initialization does not intercept this task's native calls. Further instrumentation must target the internal script/native binding layer.

The next disabled-by-default logger revision targets that layer at opcode `0x27`'s exact jump-table slot. It logs only the call hash, script thread, and instruction offset while the existing Plasmatica lifetime gate is active, then forwards to the original handler. Its inert-image install/restore test passes; it has not yet had a live cast.
