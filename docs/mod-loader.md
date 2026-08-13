# Minimal mod foothold

## Scope

Phase 4 currently produces two original 32-bit Windows PE artifacts:

- `SudekiMP.dll`: validates the loaded game, scans for confirmed signatures, reads configuration, optionally changes the Quick Menu mode request, and can install an opt-in Plasmatica diagnostic logger.
- `SudekiMP.Launcher.exe`: validates `SUDEKI.exe`, starts it suspended, loads the DLL without altering game files, calls the exported initializer, resumes only after initialization succeeds, and remains attached until the game exits.

`EnableQuickMenuNormalSpeed` defaults to `false`, and the DLL logs both the requested and applied state. When explicitly enabled, it changes only the immediate mode value in the confirmed Quick Menu activation instruction from `1` to `0`. It does not modify `SUDEKI.exe` on disk.

`EnablePlasmaticaTrace` also defaults to `false`. It is diagnostic instrumentation, not a gameplay patch. When enabled, it logs the activation lifetime and script-facing animation, camera, scripted-missile, direct-damage, and animation-speed calls made while the selected `SkillData` name is exactly `Plasmatica`. Relevant records include elapsed milliseconds from activation so normal-speed and adjusted-speed casts can be compared. All wrapper calls forward the original object, arguments, and return value unchanged.

`EnablePlasmaticaAnimationSpeed` defaults to `false`. The experimental `PlasmaticaAnimationSpeed` value is accepted only in the range `0.25` through `4.0`; invalid enabled configuration fails safely. The hook arms only when the confirmed Plasmatica task pushes `ANIMID_SKILL_02`, captures the native object at the interpreter's exact binding-dispatch call, and rejects it unless its vtable is the supported build's concrete `CNewMissileAimingGameModelAnimation` vtable. A successful application restores the model multiplier that existed before the cast. It does not alter the global game-speed state.

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
- `SudekiMP.SkillTraceImageTest.exe` opened the exact user-supplied `SUDEKI.exe` read-only, mapped its PE sections into inert memory, installed three call hooks, fourteen export-slot hooks, and the script opcode `0x27` and object-method opcode `0x28` pointer hooks, verified them, uninstalled them, and verified every original target was restored.

The generated x86 assembly was also checked for ABI compatibility. The fastcall bridge receives Sudeki's `this` pointer in `ECX`, treats `EDX` as an ignored bridge register, preserves the original stack-argument positions, and uses the matching callee cleanup sizes.

Two live Plasmatica casts completed normally with this logger and no debugger attached. Both produced `begin`, successful `use_return`, and `end` records, but no export-slot wrapper events. This does not show that the underlying engine functions were unused; it shows that rewriting those export slots after loader initialization does not intercept this task's native calls. Further instrumentation must target the internal script/native binding layer.

The next disabled-by-default logger revision targets that layer at opcode `0x27`'s exact jump-table slot. It logs only the call hash, script thread, and instruction offset while the existing Plasmatica lifetime gate is active, then forwards to the original handler. Its inert-image install/restore test passes; it has not yet had a live cast.
