# Research log

## 2026-08-12 — Phase 0 installer inspection

### Confirmed

- The supplied 515,416-byte file is a 32-bit, UPX-compressed GOG Galaxy web bootstrapper for product ID `1207664353`, not an offline Sudeki installer.
- SHA256: `e3b9d985acb267beb6afa2148d3b2c288d7fb5c4d57f0ee647cdb0388557c03d`.
- `7z` can inspect only its PE resources; no game payload is extractable.
- A dedicated Wine prefix was used. The user's default Wine prefix was not used.
- The current Galaxy installer can be downloaded and installed, but Galaxy `2.1.8.30` does not reach authentication under the tested direct GE-Proton/Wine configuration.
- At that point, `Sudeki.exe` and `SOLData.baf` were unavailable; this was resolved by the later offline-installer baseline.
- No game file was patched or modified.

### Hypotheses

- An offline GOG backup installer should be the simplest reproducible Linux path because GOG offline packages commonly avoid Galaxy authentication at install time. This remains untested for the exact Sudeki package.

### Next experiment

Superseded by the completed offline-installer baseline below.

## 2026-08-12 — Offline GOG build established

### Confirmed

- The complete offline package consists of `setup_sudeki_1.0_(13212).exe` and two external `.bin` parts. All three SHA256 hashes are recorded in `research/hashes/sudeki-gog-offline-installer-1.0-13212.sha256`.
- Native silent installation under the isolated 32-bit prefix completed successfully.
- GOG game ID: `1207664353`; build ID: `50303954381148403`; version: `1.0`; language: English.
- The executable is case-sensitively named `SUDEKI.exe` and has SHA256 `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
- `SOLData.baf` has SHA256 `9e604ec2b4736a99e16a4c76dee5842665cd3d138db44a8a694f105fa7ed360c`.
- The install contains 394 files. Read-only vanilla and writable working trees each verify completely against one shared SHA256 manifest.
- After correcting a missing GE-Proton prefix runtime dependency, the unmodified vanilla build reached the main menu at 1920×1080 under Wine.
- A post-launch hash verification passed for all 394 vanilla files.
- Closing the automated fullscreen Wine launch temporarily disrupted the Wayland display and returned the user to a TTY. No process remained after recovery; classify this as a Wine/compositor baseline issue.
- No game file was modified or patched.

### Not yet tested

- New/load game, movement, combat, Quick Menu, character switching, Skill Strikes, Spirit Strikes, saving/loading, cutscenes, and controller input.

### Next experiment

Create a Ghidra project for the exact executable hash and begin read-only static reconnaissance focused on timing imports, menu strings/state, and callers. Do not patch anything until the Quick Menu slowdown mechanism is supported by evidence.

## 2026-08-12 — Manual vanilla baseline accepted

### Confirmed

- The user completed the requested vanilla checks and reported that the baseline is good.
- This closes the manual Phase 1 gate for movement, combat, menus, switching, skills, saves, cutscenes, input, and exit at the level required to begin static reverse engineering.
- The previously observed automated fullscreen teardown/TTY disruption remains classified separately as a Wine/compositor automation issue.

### Next experiment

Begin read-only static reconnaissance of the exact hashed executable. No game-file patch is authorized at this stage.

## 2026-08-12 — Quick Menu slowdown static trace

### Confirmed

- The executable exposes named `CGameSpeed`, Quick Menu, and UI exports, providing anchors for the timing path.
- Quick Menu activation method RVA `0x00098EC0` writes requested speed mode `1` to `CGameSpeed+0x24` at RVA `0x00098EF0`.
- Quick Menu deactivation method RVA `0x00099180` writes requested speed mode `0` to the same field at RVA `0x000991B6`.
- The main frame loop near RVA `0x0028CD05` transitions requested/current modes and selects a frame scale: mode `0` uses `1.0f`, mode `4` uses a variable per-instance value, and other nonzero modes use `0.07f`.
- The selected scale is multiplied into the simulation delta at RVA `0x0028CD86`.
- Full pause and master game speed are separate mechanisms.
- Exact activation and deactivation byte contexts each occur once in this executable. Versioned patterns are recorded in `research/signatures/quick-menu-slowdown.md`.
- Full manifest verification passed for both trees (`394/394` each), including the expected executable SHA256. No game file was modified and the game was not launched during this trace.

### Strong conclusion

Quick Menu bullet time is implemented by requesting global simulation speed mode `1`, whose current scale is `0.07x`. This conclusion has high static confidence but is not yet marked runtime-confirmed.

### Tooling note

The initial pass used PE named exports, GNU `objdump`, section dumps, and exact byte searches. Ghidra 12.1.2 was then installed per-user through Flathub and full auto-analysis succeeded against the same hashed vanilla executable. The external project is `/home/wander/Games/SudekiMP/analysis/SudekiGOG_1_0_50303954381148403`; no generated project data was placed in the repository.

The original, read-only script `tools/ghidra/QuickMenuReport.java` checks the executable SHA256 before running. Ghidra reproduced both Quick Menu mode writes, the frame-loop branches, the pause and master-speed separation, and these initialized floats:

```text
fixed alternate scale: 0.0700000003f (0x3D8F5C29)
normal speed:          1.0f          (0x3F800000)
master speed:          1.0f          (0x3F800000)
```

### Next experiment

Use a non-persistent runtime watch/trace to observe `CGameSpeed+0x20` and `+0x24` while manually opening and closing the Quick Menu. Do not patch yet. Because automated fullscreen teardown previously disrupted the compositor, coordinate the launch and let the user close the game normally.

## 2026-08-12 — Milestone 1 runtime confirmation

### Confirmed

- Wine loaded `d3dx9_30.dll` at Sudeki's preferred `0x00400000` base and relocated `SUDEKI.exe` to `0x79CC0000` (delta `0x798C0000`). Initial preferred-base breakpoints therefore targeted D3DX9 and were discarded.
- Relocated instructions were verified before use: activation RVA `0x00098EF0` was runtime VA `0x79D58EF0`; deactivation RVA `0x000991B6` was runtime VA `0x79D591B6`.
- Immediately before the vanilla close write, `CGameSpeed` current mode was `1`, requested mode was `1`, and full-pause state was `0`.
- For the controlled experiment, only the live activation instruction's immediate operand was changed from `1` to `0`. Current and requested modes were reset to `0`.
- While the Quick Menu was visibly open, its object bytes at `+0x29` and `+0xFC` were both `1`. At the same time, `CGameSpeed` current mode, requested mode, and pause state were all `0`.
- The user observed that the world continued moving at normal rather than slow speed and closed the Quick Menu normally.
- The original activation bytes were restored in process memory after the test.
- Working and vanilla `SUDEKI.exe` files both retained SHA256 `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.

### Conclusion

Milestone 1 is complete: Sudeki can display and operate the Quick Menu while simulation remains at normal speed. The minimal responsible behavior is the Quick Menu activation request for `CGameSpeed` mode `1`.

### Next step

Begin Phase 4 with a minimal, version-gated mod foothold. First output should load into Sudeki, write a startup log, and exit cleanly. Keep the Quick Menu change behind configuration and signature validation rather than modifying the executable on disk.

## 2026-08-12 — Phase 4 DLL foothold

### Confirmed

- A reproducible Linux cross-build produces PE32/i386 `SudekiMP.dll` and console `SudekiMP.Launcher.exe` using MinGW-w64 GCC 15.2.
- `--check` accepts the exact Sudeki SHA256 and raw PE identity without launching it.
- A deliberately wrong PE is rejected with exit code `2` before process creation.
- Wine cannot enumerate `kernel32` in this newly suspended process (`ERROR_PARTIAL_COPY`). The launcher fails closed rather than using an unchecked address.
- A guarded fallback validates shared `ntdll!LdrLoadDll` executable memory and instruction bytes, loads the DLL through a temporary x86 thunk, frees the temporary allocations, calls `SudekiMP_Initialize`, and resumes Sudeki only after success.
- The DLL independently confirmed the exact game hash, loaded PE identity, and exactly one relocation-tolerant Quick Menu signature at RVA `0x00098EE5`.
- `SudekiMP.log` recorded `status=ready` with both `quick_menu_patch_requested=false` and `quick_menu_patch_applied=false`.
- Working and vanilla executable hashes remained unchanged.

### Pending

- The user confirmed that they closed the first successfully injected Sudeki process manually after being told it was safe to close. The process exited without a reported crash, completing the minimal load/log/clean-exit foothold.

### Next experiment

Implement the Quick Menu behavior as a disabled-by-default, signature-gated runtime option. Keep the change in process memory only and verify both the inert default and enabled behavior before moving on.

## 2026-08-12 — Version-gated Quick Menu runtime option

### Confirmed

- `SudekiMP.dll` now exposes the known Quick Menu behavior through `EnableQuickMenuNormalSpeed`, which defaults to `false`.
- The inert/default test logged `quick_menu_patch_requested=false`, `quick_menu_patch_applied=false`, and `status=ready`.
- Enabling the option requires the exact executable hash/PE identity, exactly one activation signature match, and an exact seven-byte instruction match at the write site.
- The enabled test logged `quick_menu_patch_requested=true`, `quick_menu_patch_applied=true`, and `status=ready`.
- With the Quick Menu visibly open, a read-only debugger observed patched bytes `C7 40 24 00 00 00 00`, both menu active bytes set to `1`, and `CGameSpeed` current/requested/pause values all set to `0`.
- The user played during the enabled test and observed that combat remained at normal speed while the menu was open. The user also noted that this was predictably very punishing.
- The patch was applied only to process memory. The repository and generated configurations were restored to disabled after the capture.

### Conclusion

The maintainable Phase 4 foothold can safely reproduce Milestone 1 without editing the game executable. This validates the loader, exact-build gate, pattern scanner, configuration path, and smallest understood runtime modification. The difficulty observation reinforces the planned direction: direct real-time skill/item controls must replace reliance on menu navigation before normal-speed combat becomes a usable design.

## 2026-08-12 — Phase 5 Plasmatica activation trace

### Confirmed

- Elco's Plasmatica enters exported `CSkill::Use(int)` at VA `0x004B4810` / RVA `0x000B4810`.
- The Quick Menu call instruction is VA `0x004998A1` / RVA `0x000998A1`. A preliminary running note incorrectly stated RVA `0x000A98A1`; `0x000998A1` is the corrected value confirmed by Ghidra and disassembly.
- The observed equipped-slot argument was `1`. The selected `SkillData` object contained the UTF-16 name `Plasmatica`, independently confirming the selection.
- `CSkill::Use` validates the request, deducts the cost stored at `SkillData+0x94`, emits `OnSkillStarted`, sets `CSkill+0x6C` to active, stores the equipped slot at `CSkill+0x70`, and stores a returned script/task handle at `CSkill+0x74`.
- A per-frame `CSkill` update waits for the task handle to finish. Completion releases actor-state flags and emits `OnSkillEnded`.
- The first cast did not enter generic `CEntityAttacks::StartAttack`, `StartAttackOnTarget`, or `CSimpleGameModelInterface::SetAnimationSpeedMultiplier`. Concurrent `StartAttack` calls were identified as unrelated `PERSONALSWARM` enemy activity.
- Archive strings identify Elco skill script functions `PC_Elco0__Skill` through `PC_Elco5__Skill`. Runtime table lookup ultimately maps Plasmatica to `PC_Elco1__Skill|P`; earlier `PC_Elco3` and `PC_Elco2` expectations were incorrect.

### Debugger incident

- A later multi-breakpoint GDB trace stopped at `CSkill::Use` and captured the Plasmatica `SkillData`, then the process faulted at address `0x00000001` before any animation, missile, damage, or speed-setter event was logged.
- Wine reported an unhandled page fault and Sudeki exited with Windows status `0xC0000005` (`3221225477`). This run is invalid as behavioral evidence beyond the activation record captured before the fault.
- The unsafe multi-breakpoint script was removed. Do not repeat this tracing method. Prefer in-process, version-gated logging through `SudekiMP.dll`, or a substantially narrower independently validated trace.
- No save, executable, archive, or other installed game file was modified by the debugger.

### Current conclusion

Plasmatica is launched as a script/task whose lifetime is owned by `CSkill`; the activation layer is now understood well enough to move the next instrumentation into the mod DLL. Animation timing, projectile creation, damage timing, and protection behavior remain unconfirmed.

## 2026-08-12 — Safe Plasmatica logger prepared

### Confirmed

- The retired GDB workflow was replaced by opt-in in-process logging behind `EnablePlasmaticaTrace=false`.
- The exact Quick Menu `CSkill::Use` call and completion-path `CSkill::StopRumble` call are validated before their relative targets are redirected.
- Fourteen exact export-table slots cover script-facing animation state/direct calls, missile-animation variants, scripted missile firing, direct damage, and the public animation-speed setter.
- The trace begins only when the selected `SkillData` inline UTF-16 name equals `Plasmatica`; other skill activations pass through without enabling event logging.
- Original functions receive the unchanged object and arguments, and wrapper return values are passed back to Sudeki.
- Any installation mismatch fails closed before the suspended primary game thread resumes. Partial hook installation is restored in reverse order.
- A standalone synthetic-memory hook test passed under Wine.
- A second standalone test mapped the exact `SUDEKI.exe` into inert memory, successfully installed all two call hooks and fourteen export hooks, then restored every original call target and export RVA. It opened the executable read-only and did not create a Sudeki process.
- Generated wrapper assembly has the expected x86 register and stack cleanup behavior.
- Both repository and generated configurations remain disabled after preflight.

### Live-test status

Completed by the first safe logger run below. Normal loading and two complete `begin` -> `use_return` -> `end` lifetimes were confirmed without a debugger. The absence of native wrapper events redirected the investigation to the internal script/native binding layer.

## 2026-08-12 — First safe Plasmatica logger run

### Confirmed

- Sudeki initialized with `EnablePlasmaticaTrace=true`; exact call-target and export-slot validation passed and the game resumed normally.
- Two complete Plasmatica activations were logged without a debugger attachment or crash.
- Both activations selected equipped slot `1`, returned success, set the `CSkill` active byte, produced a non-null task handle, and reached the normal completion path on the same `CSkill` object.
- The two task handles were distinct, as expected for separate activations.
- Neither activation passed through any of the fourteen redirected export-table slots while the trace was active; both ended with `events=0`.
- The generated configuration was returned to `EnablePlasmaticaTrace=false` after capture. The already-running process retains its startup instrumentation until normal exit; the configuration change affects only later launches.

### Interpretation

The zero-event result does not establish that Plasmatica avoids the underlying animation, missile, damage, or speed functions. It establishes only that changing the executable's export-table slots after process loader initialization did not intercept the calls made by this task. The script/native binding may cache function pointers before `SudekiMP_Initialize`, use a separate registration table, or invoke different native entry points.

### Next static target

Trace compiled script function `PC_Elco3__Skill` and the task/native dispatcher reached from `CSkill::Use`. Identify where native function pointers are stored or invoked before adding any further live hook.

## 2026-08-12 — Script-call dispatcher narrowed

### Confirmed statically

- The script interpreter is entered at VA `0x005C41D0` / RVA `0x001C41D0`. Its instruction offset is stored at script-thread `+0x0C`, and the bytecode base is reached through the runtime global at VA `0x007C310C` / RVA `0x003C310C`, then runtime `+0x14`.
- The interpreter dispatches opcodes through an absolute-pointer table beginning at VA `0x00723F04`.
- Opcode `0x27` uses handler VA `0x005C4970` / RVA `0x001C4970`; its jump-table slot is VA `0x00723FA0` / RVA `0x00323FA0`.
- Opcode `0x27` consumes a 32-bit call hash. It first checks the compiled-script function table and, if that misses, consults the internal binding registry at runtime-manager `+0x70`. This is the relevant path for script/global-native calls.
- Opcode `0x28` uses the separate object-method handler at VA `0x005C4B10` / RVA `0x001C4B10`. It eventually invokes a method through the script object's virtual table. It is not the only native-call mechanism.
- `SOLWORLDM.gex` contains symbols and matching table identifiers for all six Elco skill scripts. Static ordering alone did not identify which one was Plasmatica; the live instruction offsets were required.

### Narrow logger prepared

- The opt-in Plasmatica logger now redirects only opcode `0x27`'s exact jump-table pointer in addition to the existing hooks.
- While the Plasmatica lifetime flag is active, the wrapper records the script thread, instruction offset, and 32-bit call hash, then tail-calls the original opcode handler unchanged. Outside that lifetime it immediately forwards the call.
- The exact mapped executable passed installation, redirection, uninstall, and restoration checks. Generated x86 preserves the script thread in `ECX`, clears the unused `EDX` bridge register, and tail-jumps to the original handler.
- Both checked-in and generated configurations remain disabled. A new live capture requires a fresh Sudeki launch because the currently running process contains the previous DLL image.

## 2026-08-12 — Plasmatica opcode-dispatch capture

### Confirmed

- One Plasmatica cast completed normally with the exact opcode `0x27` jump-table hook active. `CSkill::Use` returned success with task handle `0x0814F060`, and the normal end path was reached without a debugger or crash.
- The primary script thread was `0x00B50D8C`. Other thread addresses in the 417-event lifetime were parallel/nested combat scripts and are not automatically attributed to Plasmatica.
- This capture initially associated primary-thread offsets `0x000AACBA` through `0x000AAF60` with `PC_Elco2__Skill|P` by cumulatively pairing record lengths with the wrong table row. A later live table lookup corrected the association to `PC_Elco1__Skill|P`, hash `0x2F41B420`, range `0x000AAC9F–0x000AAF66`.
- The equipped menu slot is `1`, consistent with the corrected intrinsic script index.
- The synchronous call path includes preload, sound, scripted-animation-controller, cone targeting, rail targeting effect, camera collision/playback, current-animation queries, render-camera changes, and light-effect cleanup.
- `IsPlaying|P` (`0x890F6EB1`) was called 233 times from the same Plasmatica operand offset `0x000AAEA4`. `GetCurrentTsaAnimation|P` was called sixteen times in the nested camera/animation setup path.
- The generated configuration was restored to `EnablePlasmaticaTrace=false` after capture. The running process retains the logger only until normal exit.

### Interpretation

Plasmatica's long-lived task demonstrably waits by polling an object's `IsPlaying` state. The next narrow trace should identify the receiver associated with hash `0x890F6EB1` and separately locate projectile/damage events.

## 2026-08-12 — Plasmatica `IsPlaying` receiver identified

### Confirmed

- One normal cast was sampled at polls `1`, `2`, `3`, `50`, `100`, `150`, and `200`. Every sample used script argument `0x07F5B390`.
- The argument object's vtable was module-relative RVA `0x002C0098`. MSVC RTTI resolves its type descriptor to `.?AVGELGroupPtr@@`, or `GELGroupPtr`.
- The wrapper's pointer field at `+0x0C` was `0x07CC15B0`. The pointee's vtable RTTI resolves to `.?AVElcoEntity@@`, or `ElcoEntity`.
- The wrapper and task handle were allocated close together in this run: receiver `0x07F5B390`, task `0x07F5B438`, a difference of `0xA8`. This is an observation, not a documented structure relationship.
- The executable's two exported `CNewGameModelAnimation::TsaIsPlaying` variants share RVA `0x0000F2D0`. That function returns whether byte `this+0x131` equals state `3`. No live call from this particular script wrapper to that RVA has yet been demonstrated.
- `IsPlaying|P`, hash `0x890F6EB1`, is archive table entry `184`, length `0x31`, with reconstructed runtime range `0x00004081–0x000040B1`. Its record is not being treated as decoded executable script until the record format is better understood.

### Correction

The first receiver logger labeled the post-opcode stack word as `result`. It was unchanged from the argument address because opcode `0x27` did not synchronously replace that slot with the eventual Boolean at the observation point. It is not evidence that `IsPlaying` returned an address. The logger source now calls this field `stack_after_dispatch`; the true/false result remains unobserved.

### Strong hypothesis

The direct argument is Elco, and nearby archive symbols include `p_gameModelAnimation`, `GetCurrentTsaAnimation|P`, `IsLocked|P`, and `IsPlaying|P`. This hypothesis was confirmed by the wrapper decoding below.

### Next target

Decode the compiled wrapper reached by this exact `IsPlaying|P` call, then locate the projectile/effect spawn and damage events independently; do not bypass the wait loop.

## 2026-08-12 — Plasmatica animation wait confirmed

### Confirmed

- The opcode `0x27` resolver found `IsPlaying|P` in the compiled-function table, so the native registry correctly returned no direct binding for hash `0x890F6EB1`.
- The live compiled-function entry is `00170003, 890F6EB1, 00003E22, ...`; bytecode execution begins at offset `0x00003E22`. The next compiled entry begins at `0x00003E44`, so the wrapper is `0x22` bytes long.
- Its first opcode `0x28` method operand is at `0x00003E38` with hash `0xB2F8076C`, which the archive symbol dictionary identifies as `GetComponent|N`. The component type hash `0xC5D2509B` resolves to `CNewGameModelAnimation`.
- Its second opcode `0x28` method operand is at `0x00003E3D` with hash `0xDC187AFB`, resolving to `TsaIsPlaying`.
- Both exported `CNewGameModelAnimation::TsaIsPlaying` variants point to RVA `0x0000F2D0`. The implementation returns whether byte `this+0x131` equals animation state `3`.
- Plasmatica therefore waits for Elco's game-model animation state. This is no longer only a hypothesis.

### GEX mapping correction

The live compiled-function hash table also exposed the correct start offsets for the Elco skill entries. Hash `0x2F41B420` (`PC_Elco1__Skill|P`) starts at `0x000AAC9F`; the next entry, hash `0xCCD0E44B` (`PC_Elco2__Skill|P`), starts at `0x000AAF67`. Plasmatica's observed primary-thread instructions lie in the first range, so Plasmatica is `PC_Elco1__Skill|P`. The earlier cumulative-length association was shifted by one table row.

### Next target

Capture opcode `0x28` object-method calls on only the Plasmatica primary script thread. Correlate those calls with projectile/effect creation and damage while sampling the known animation-wait methods to avoid log flooding.

## 2026-08-12 — Plasmatica missile launch gated by animation events

### Confirmed

- A complete cast ended normally with `731` logged events and no debugger attachment or crash.
- Plasmatica enabled skill targeting, polled `ShouldSkillTargettingModeBeActive` 181 times, and disabled targeting before entering its execution sequence.
- The script created and attached an event watch, then used two distinct `HasArbEventOccured|NN` loops. The first loop at operand `0x000AAE2C` made 216 calls before true; the second at `0x000AAE60` made 248 calls before true.
- Immediately after the second event, bytecode pushes number literal `10.0`, resolves `CMissileManager`, and invokes `FireMissileScripted|N` at operand `0x000AAE89`. The script-facing call returned true.
- The script cleared the watched event list and only then began the 233-call `IsPlaying|P` animation-completion wait.
- The final cleanup disabled auto targeting, removed the light effect, destroyed the event watch, resolved `CTargeter`, stopped rumble, and restored the control filter.
- No primary-thread method hash matched `DoDirectDamage|N` (`0xD13C5FE6`), `ModifyHitPoints|NB` (`0x68687A89`), `HitEntity|PB` (`0x2D759B0D`), or `CausePoison|PNNN` (`0x1E18F52A`).

### Interpretation

Projectile launch is tied to the second watched animation/arbiter event rather than to the final animation-state transition or an obvious fixed script delay. Damage is not dispatched directly by the primary Plasmatica script through any of the four known method hashes. It may be handled by the spawned missile's collision path, by a different script thread, or wholly in native code.

### Next target

During one fresh cast, retain the primary-thread trace but monitor only the four known damage-method hashes on every script thread while the Plasmatica lifetime flag is active. This is observation-only and should distinguish a secondary script damage path from a native missile-impact path.

## 2026-08-13 — Plasmatica damage ownership narrowed to native missile path

### Confirmed

- A fresh Plasmatica cast completed normally with `100` sampled events. The event waits became true on calls `216` and `248`, `FireMissileScripted(10)` returned true, `TsaIsPlaying` became false at poll `233`, and the skill task reached its normal end event.
- Across every script thread active during that skill lifetime, no call matched `DoDirectDamage|N` (`0xD13C5FE6`), `ModifyHitPoints|NB` (`0x68687A89`), `HitEntity|PB` (`0x2D759B0D`), or `CausePoison|PNNN` (`0x1E18F52A`).
- Static decompilation follows `CMissileManager::FireMissileScripted(int)` at RVA `0x000C89C0` through missile selection/preparation at `0x000C6DE0`, launch at `0x000C7160`, and native `CMissile` initialization at `0x00186E10`.
- The active missile update at RVA `0x001867D0` handles movement/range and ground/wall impact behavior. Its call through RVA `0x000DCD00` was initially provisional and is corrected by the later static pass below. RVA `0x00186610` terminates the missile.
- The `CMissile` vtable is at VA `0x006D915C`. No direct code reference reaches exported `DoDirectDamage` or `ModifyHitPoints`; their known references are binding/export data.
- Read-only archive inspection found Plasmatica's serialized missile entry: one projectile, velocity `17.0`, range `100.0`, wall and ground collision enabled, penetration disabled, bouncing disabled, and model reference `SFXES005_PLASMATICA_PROJECTILE.HOM:41`.
- Its primary attack record is `Area`, `Magic`, `asSkill`, and has serialized base damage `500`. It links to `PlasmExplosion`, whose serialized base damage is `300`, but the primary record's `Secondary Starts` value is `Never`.

### Interpretation

For this observed cast, damage is not initiated through any of the four known script method bindings on any script thread. Together with the native collision/update path, this makes native missile/collision ownership a strong conclusion. The exact native damage functions were unresolved at this checkpoint and are identified in the later static pass below.

The script's `FireMissileScripted(10)` literal is not the same visible index as Plasmatica's element `2` position in `MissileCombos`; the mapping remains undocumented. The two serialized damage values must not be added together until secondary-attack activation is observed.

### Next target

Resolve the animation state pushed at bytecode operand `0x00004195` and trace the independent per-character animation-speed setter. Then test whether changing only Elco's animation multiplier advances the two watched launch events without changing world speed.

## 2026-08-13 — Independent Plasmatica animation-speed prototype prepared

### Confirmed

- The archive's exact resource/hash entry resolves `0xB0242A96` to `ANIMID_SKILL_02`. This is the value passed to `TsaPushAnimationState` at bytecode operand `0x00004195` during the observed Plasmatica cast.
- MSVC RTTI describes `CNewGameModelAnimation` as deriving through `CSkinnedGameModelInterface` from `CSimpleGameModelInterface`, with all three subobjects at offset zero.
- Native `SetAnimationSpeedMultiplier(float)` at RVA `0x000E0460` writes the model-local input multiplier at `+0x48`, computes `+0x44 * +0x48 * +0x4C`, stores the effective value at `+0x50`, and notifies the concrete model through a virtual call.
- Native reset at RVA `0x000E04B0` restores the input to `1.0`; the getter at RVA `0x000E04F0` returns the effective `+0x50` value.
- The opt-in prototype matches the exact Plasmatica script thread, operand, method hash, and `ANIMID_SKILL_02` hash. It records the existing input multiplier, applies the configured multiplier only to that animation receiver, and restores the recorded value at normal skill cleanup.
- The project cross-compiled without warnings, and both the relative-call-hook test and exact-image skill-hook installation test passed under Wine.

### First runtime experiment — rejected

- The normal-world-speed patch remained effective, but the user did not observe a faster Plasmatica animation at `1.5x`.
- The first hook logged object `0x0838F818` and prior multiplier bits `0x00145DB0`. Those bits are not a plausible animation multiplier; event timing also remained essentially vanilla (`201` and `249` polls for the two arbiter events, and `233` polls until `TsaIsPlaying` became false).
- Static reconstruction of opcode `0x28` confirmed that the logged stack object was only the script-side wrapper. The handler obtains the underlying native object through wrapper virtual slot `+0x14` or `+0x30`, then passes that native pointer as the second stack argument to the binding dispatcher call at RVA `0x001C4C2F`.
- The bad experiment restored the exact bits it had saved, so it did not leave a persistent field change. It nevertheless does not count as independent animation-speed control.

### Initial binding-dispatch prototype

- A private-convention bridge now intercepts only the exact binding-dispatch call, preserving `EAX`, `ECX`, all three stack arguments, the return value, and the dispatcher's `ret 0x0C` cleanup.
- The speed operation is armed only for Plasmatica's primary thread at operand `0x00004195`, method `TsaPushAnimationState`, and resource `ANIMID_SKILL_02`.
- This initial revision required the resolved object vtable to equal supported-build base `CNewGameModelAnimation` RVA `0x002C8504` and the prior multiplier to be finite and positive. A mismatch was logged and left untouched; the concrete-type correction is recorded below.
- The DLL builds without warnings. The synthetic call-hook test, updated exact-image install/restore test, and launcher build check all pass under Wine. Live behavior is ready for a new comparison run but is not yet confirmed.

### Native type-gate capture

- The first corrected-dispatch run reached native object `0x07C5825C` and safely rejected vtable `0x7AEA5464` because the gate expected the base `CNewGameModelAnimation` vtable. No animation field was written and the cast completed normally.
- Relocating that vtable against module base `0x7ABD0000` gives RVA `0x002D5464`. MSVC RTTI identifies it as `CNewMissileAimingGameModelAnimation`.
- Its class hierarchy includes `CNewGameModelAnimation`, `CSkinnedGameModelInterface`, and `CSimpleGameModelInterface`, each with member displacement zero. The live untouched multiplier at native-object offset `+0x48` was `1.0` (`0x3F800000`).
- The exact-type safety gate now accepts only the supported build's `CNewMissileAimingGameModelAnimation` vtable for this Elco experiment. This correction is statically justified and awaits one new runtime cast.

## 2026-08-13 — Milestone 2 confirmed: independent Plasmatica animation speed

### Confirmed

- Two Plasmatica casts completed normally with normal world simulation and `PlasmaticaAnimationSpeed=2.0`.
- Each cast resolved the same native object, validated vtable RVA `0x002D5464`, read the prior per-model multiplier as `1.0` (`0x3F800000`), applied `2.0` (`0x40000000`), and restored exactly to `1.0` during normal skill cleanup.
- The first animation-gated arbiter event changed from normal-speed poll `201` to `101`; the second changed from `249` to `125`; `TsaIsPlaying` changed from false at poll `233` to false at poll `117`. The second 2.0x cast reproduced the same `101`, `125`, and `117` values.
- The near-exact halving demonstrates that the selected caster animation and its animation events were accelerated independently. The hook does not write the global game-speed state, and the Quick Menu normal-world-speed option remained active.
- The user reported correct skill targeting. They also reported that the usual eye-view camera angle appeared to be skipped, likely because its presentation window is coupled to the now-shorter animation/event sequence.
- Repository and generated configurations were returned to disabled defaults after capture. The already-running process retains its startup hooks until normal exit.

### Interpretation

Milestone 2 is achieved for one skill: world simulation can remain at `1.0x` while Plasmatica's caster animation is independently configurable. Camera timing, preferred multiplier, combat protection, and polish remain future work; `2.0x` is evidence, not a committed balance value.

## 2026-08-13 — Plasmatica eye-camera collision gate and timing

### Confirmed

- The timing logger uses `GetTickCount()` only while the exact Plasmatica lifetime gate is active. It also records the narrow camera bindings and collision/render-camera object methods without changing their arguments or results.
- One normal-speed cast returned `1` from `TestCameraCollision|PRN` at bytecode operand `0x0002FC0A`. That cast skipped `TsaPlayCamera` and immediately selected normal-camera resource hash `0x933B5BDE`.
- A second normal-speed cast returned `0`. It called `TsaPlayCamera|PRSNNBS` at `0x00038CEF`, polled `GetCurrentTsaAnimation|P` at `0x00038D1F`, and selected cinematic camera resource hash `0xEE1B1485` before later restoring `0x933B5BDE`.
- Both `2.0x` comparison casts returned `0` from the collision test and executed the same cinematic-selection and normal-restore calls. The user visually observed the cinematic camera in the available open space.
- At `1.0x`, the cinematic camera was selected at `3.298 s`, the projectile was requested at `10.746 s`, the caster animation completed at `14.628 s`, the normal camera was restored at `14.883 s`, and the task ended at `15.003 s`.
- The two `2.0x` casts reproduced selection at `3.279/3.281 s`, projectile request at `7.014/7.014 s`, animation completion at `8.949/8.948 s`, camera restore at `9.203/9.202 s`, and task end at `9.323/9.322 s`.
- The selected cinematic-camera interval therefore changed from `11.585 s` to `5.924/5.921 s`. The preceding collision-to-selection setup remained about `0.25 s` at both speeds.
- The user's reported three or four visible view changes did not correspond to repeated `SetRenderCamera` calls. Successful casts contained exactly one cinematic selection and one normal-camera restore, so the intermediate visible motion belongs inside the authored camera presentation.

### Interpretation

The cinematic is conditionally rejected when its collision test reports insufficient space. When accepted, the same camera path runs at both tested caster speeds, but it remains selected only until the accelerated caster-animation sequence completes. This explains why the shortened cast could show only three authored angles. The follow-up playback-rate investigation is recorded below.

## 2026-08-13 — Independent Plasmatica camera speed confirmed

### Confirmed

- Runtime compiled-function lookup resolved `TsaPlayCamera|PRSNNBS` (`0xF69C244A`) to bytecode offset `0x00004532`, length `0xC4`.
- The wrapper calls `CSpiritCam::StartCam|PPRNNS` (hash `0xEBDE4799`) at bytecode operand `0x000045EF`. Its observed reverse stack begins with `ANIMID_SKILL_02`, integer mode `-1`, and float rate `1.0`.
- The exported native implementation is RVA `0x00012910`. It stores the float at `CSpiritCam + 0x1D4`. Its camera initialization path multiplies that field by another local factor and the constant `24.0` before a virtual playback configuration call.
- Method hash `0x0D719F5D`, observed at Plasmatica operands `0x000AAE2C` and `0x000AAE60`, resolves to `HasArbEventOccured|NN`. Those frame-by-frame calls gate Elco's animation events and do not own camera playback speed.
- Changing a higher-level `TsaPlayCamera` stack value from `1.0` to `2.0` had no visible effect. Hooking `Camera::PlaybackSequenceState` update at RVA `0x001A6590` produced zero calls during accepted casts. Both disproved patches were removed.
- The corrected prototype changes only the `StartCam` float under exact Plasmatica, script-thread, instruction, method-hash, argument-count, animation-name, mode, and original-rate gates. It is disabled by default and fails inertly if any gate differs.
- Three accepted casts logged `previous_bits=0x3F800000`, configured multiplier `0x40000000`, and effective rate `0x40000000`. Elco's separate model multiplier was also applied at `2.0x` and restored to `1.0` after every cast.
- The cinematic render camera was selected at approximately `3.29 s`, the normal camera was restored at approximately `9.22 s`, and the task ended at approximately `9.23 s`, matching the established accelerated-caster timing.
- The user visually confirmed four cinematic camera angles before the return to Elco, compared with three when only the caster was accelerated.
- The exact-image hook test, synthetic call-hook test, launcher build check, and cross-compile all passed.

### Interpretation

Plasmatica's authored cinematic camera has a playback-rate control independent of both world simulation and the caster's model-animation multiplier. Matching the camera and caster multipliers preserves the full four-angle presentation within the shortened skill. The next Phase 5 targets remain exact native impact damage resolution and caster protection/stagger behavior.

## 2026-08-13 — Plasmatica native impact-to-HP chain resolved

### Confirmed

- The earlier provisional `RVA 0x000DCD00 -> RVA 0x00018B90` damage label was incorrect. RVA `0x00018B90` constructs an `SfxSetupPosition`, and RVA `0x000DCD00` submits environment-impact SFX from the missile update. Neither function applies character damage.
- Missile initialization at RVA `0x00186E10` registers the active missile collision body through RVA `0x000EC200`. That registration feeds the engine's collision/contact system rather than the environment-impact SFX call.
- Collision geometry processing at RVA `0x00032A80` directly calls RVA `0x00138870` for a qualifying `CCollisionDamage` contact.
- RVA `0x00138870` checks its multiple-hit timer, rejects ineligible target states, constructs a `DamageStructure`, initializes its configured damage and knockback, copies its damage effect and status-effect array, and calls RVA `0x000DAB50`.
- RVA `0x000DAB50` performs target/state filtering and calls the target combat component at RVA `0x000D21D0` when damage is accepted.
- RVA `0x000D21D0` applies mitigation through the `DamageStructure`, clamps the resulting HP to the target's valid range, and writes current HP at offset `+0x2C` of the combat-data object. The remainder of the function handles reactions, death, status effects, and related combat follow-up.
- Exact-build byte contexts for RVAs `0x00138870`, `0x000DAB50`, and `0x000D21D0` each occur once in the executable and are recorded in `research/signatures/plasmatica-damage.md`.
- Exported `HitEntity` at RVA `0x000A2900` is a shared no-op/export thunk used by many unrelated names. Its appearance in vtable data is not the Plasmatica damage implementation.

### Interpretation

The observed Plasmatica chain is now established from scripted missile launch through native collision handling to the target HP write. The static chain identifies ownership and timing architecture; it does not yet prove which serialized Plasmatica attack record supplies every runtime field or whether the linked `PlasmExplosion` record activates.

### Next target

Determine what protection Elco receives during `ANIMID_SKILL_02`: trace stagger suppression, damage reduction or invulnerability, target retention, and recovery/end-state restoration without changing behavior first.

## 2026-08-13 — Plasmatica inherits refcounted Skill Strike invulnerability

### Confirmed

- Successful `CSkill::Use` calls RVA `0x000DC200` on the actor's `CCharacterArbiter`. That helper increments signed byte `+0x54`, sets arbiter state flag `0x800`, and sets `IsUsingSkill` flag `0x10`.
- Exported `CCharacterArbiter::IsInvulnerable()` at RVA `0x00008980` returns true exactly when the same signed byte at `+0x54` is greater than zero.
- Exported `CCharacterArbiter::GELSetInvulnerable(bool)` at RVA `0x000DCA10` independently uses the same representation: it increments or decrements `+0x54` and sets or clears flag `0x800` according to the resulting refcount.
- When the script task finishes, the `CSkill` update at RVA `0x000B47A0` decrements `+0x54`, preserves or clears `0x800` according to any remaining nested protection, clears `IsUsingSkill` bit `0x10`, and performs the normal end cleanup.
- The GEX binding dictionary maps `GELSetInvulnerable|B` to `0xE2A2F1B8` and `CCharacterArbiter::GELSetInvulnerable|B` to `0x95ABCCAF`. Neither hash appears in the `PC_Elco1__Skill|P` bytecode range or the captured primary-thread call trace.
- Common script functions `StartSkill|PR` and `EndSkill|P` only manage animation caching/unloading in their short compiled bodies; they do not own the native invulnerability state.

### Interpretation

Plasmatica is protected by generic, refcounted Skill Strike invulnerability for the lifetime that `CSkill` considers active. There is no evidence of a Plasmatica-specific damage-reduction scalar or an additional script-controlled protection window. Accelerating the caster animation shortens the task and therefore shortens this invulnerability window along with it; the current hook does not leave protection active after cleanup.

### Next target

Resolve how the line-targeted skill retains its chosen target through `FireMissileScripted(10)`, then document the exact input/turn-rate/animation locks restored at `PC_Elco1__Skill|P` recovery.

## 2026-08-13 — Plasmatica target pointers are cleared before launch

### Confirmed

- Static analysis identifies the global skill-target node at VA `0x007C3B44` and its active byte at VA `0x007C2FCD`. `CTargeter::EndSkillTargetting` clears both as targeting ends.
- `CTargeter::EnableAutoTargetting(false)` clears enabled bit `0x02` at `CTargeter+0x84` and releases the ordinary current-target node at `CTargeter+0x54`.
- The logger sampled those four values immediately before the exact primary-thread `FireMissileScripted` dispatch at bytecode operand `0x000AAE89`. It made no writes.
- Five casts agreed: the skill-target node was null, skill targeting was inactive, the ordinary target node was null, and auto targeting was disabled. Two controlled samples differed in whether the cinematic camera was accepted; their targeting state was identical. Three further casts reproduced the result with the same actor targeter object.
- The raw script argument at each launch was `0x41200000`, the IEEE-754 representation of `10.0`.
- Recovery calls `TsaUnlock`, waits for `TsaIsActive` to become false, restores the normal render camera, calls `EnableAutoTargetting(true)`, resets remaining animation/effect state, destroys the event watch, stops rumble, and reaches the control-filter restoration wrapper before native `CSkill` completion releases invulnerability.

### Interpretation

Plasmatica does not carry its chosen enemy to missile launch through either of Sudeki's visible target pointers. The aiming line is committed earlier, then both targeting mechanisms are deliberately disabled. Camera acceptance is presentation-only with respect to these target states. The remaining narrow question is which actor/aim orientation field the native missile constructor consumes; this is an orientation handoff question, not an unresolved live target-lock question.

### Next target

Resolve the native launch-direction/orientation handoff used after both target pointers are cleared. Then Phase 5 can be closed with a single end-to-end Plasmatica function map before moving to the Phase 6 direct-activation prototype.

## 2026-08-13 — Plasmatica launches along committed actor facing

### Confirmed

- Native resolver RVA `0x000C7AA0` first tries an eligible ordinary target and normalizes `target position - launch origin`.
- Without that target, it uses an active aiming-camera ray only when the owner component at `+0x90` has flag `0x00400000` set and the global aiming camera exists.
- Its final fallback copies the owner transform's direction vector from offsets `+0x50`, `+0x54`, and `+0x58`.
- One final read-only launch capture again found both target pointers null. Owner aim-mode flags were `0x00080812`, which does not contain `0x00400000`, even though the global aiming-camera pointer was non-null.
- The selected branch was therefore actor forward. The captured vector bits were `3F55D7CE,00000000,3F0CBCEB`, consistent with a normalized horizontal facing vector.
- The first launch attempt for this capture session failed earlier in Wine with `alloc_user_handle: Assertion 'index < MAX_USER_HANDLES' failed`. Sudeki's isolated GE-Proton server was stopped and restarted; the clean relaunch and cast then completed normally. This remains classified as a Wine/compositor issue, not a mod failure.

### Phase 5 conclusion

The selected line turns Elco before execution; skill and ordinary targeting are then disabled and cleared; the second animation event launches missile script entry `10`; standard Plasmatica uses Elco's committed forward vector; native collision applies damage; the animation finishes; script cleanup restores camera, auto targeting, effects, rumble, and controls; native `CSkill` completion releases task-lifetime invulnerability. The first Skill Strike is now traced end to end with independent caster and camera speed controls proven at world simulation `1.0x`.

### Next phase

Begin Phase 6 with an observation-first direct-activation design: identify the narrowest safe engine entry that supplies the same equipped-slot/actor context as `CSkill::Use(1)`, then prototype one disabled-by-default Plasmatica hotkey without invoking the Quick Menu.

## 2026-08-13 — Native QuickSkill route and ranged readiness gate

### Confirmed

- The executable action table maps `ac_QuickSkill0..5` to IDs `0x7A..0x7F`. Both the shipped defaults and active Wine-prefix options bind them to DirectInput scan codes `6..11`, physically top-row `5..0`.
- Input handler RVA `0x000277B0` calls the native direct-activation helper at RVA `0x00027BF0` for a key-down event in that action range.
- The helper resolves the active character from the owner at VA `0x00808D94`, obtains `CSkill` at component offset `+0xD8`, walks six ordered entries through `CSkill+0x54`, validates the selected slot with RVA `0x000B4BC0`, and calls `CSkill::Use` at RVA `0x00027CB1` only on validator result `0`.
- Observation-only logging recorded top-row `5` as action `0x7A`. The user saw it activate Iron Will on a melee character with no Quick Menu.
- A controlled Elco test recorded action `0x7A`, selected native slot `0`, and validator result `2`; no `CSkill::Use` followed.
- Validator result `1` is insufficient SP, result `2` maps to `Not ready to use skill.`, and result `3` maps to `Cannot use skill if not in combat.` Static control flow shows result `2` comes from the actor-state readiness branch.
- The user reports direct number-key activation works for Tal and Buki but not Elco and Ailish. Only Elco's rejection has been instrumented to the validator so far.
- A launch during this investigation aborted inside Wine with `alloc_user_handle: Assertion 'index < MAX_USER_HANDLES' failed` before any input/skill trace event. This matches the previously observed GE-Proton/Wine compositor failure and is kept separate from mod behavior.
- `tools/stop-sudeki.sh` now provides an emergency stop scoped to the dedicated Sudeki research prefix.

### Interpretation

Sudeki already contains most of the desired Phase 6 real-time skill entry point. Ranged characters are not missing the input action; their normal combat/aim state fails the shared skill-readiness predicate. The next target is the state transition performed around Quick Menu use for Elco/Ailish. It should be reproduced narrowly before direct activation rather than suppressing validator result `2`.

## 2026-08-13 — Elco's direct-skill blocker isolated to ranged strafing

### Confirmed

- Three direct Elco `5` attempts reached the same skill and failed readiness with `CCharacterArbiter+0x50 = 0x00400002`, the exported `Armed | Strafing` bits.
- Two menu Plasmatica casts passed the menu validator with `+0x50 = 0x00000003` (`Idle | Armed`) and `UsingUI` set at `+0x60`.
- A new hook at the validation inside `CSkill::Use` recorded `UsingUI` cleared while `Idle | Armed` remained; that second validation also returned zero and both casts completed.
- Quick Menu activation calls RVA `0x0000AFD0(true)`. Its skill-selection path calls the menu's full RVA `0x00099180` deactivation routine before `CSkill::Use`; that routine calls RVA `0x0000AFD0(false)`.
- The transition uses Sudeki's actor-control and UI-listener machinery. No evidence supports directly clearing the `Strafing` bit or suppressing validator result `2`.

### Next experiment

A dedicated `--ranged-skill-test` mode guards QuickSkills by exact failure code and arbiter state. The first immediate open/close attempt changed only `UsingUI` and safely aborted because Elco remained strafing. The corrected version held the native transition for 75 ms through the game-thread message loop. Elco naturally changed to `Idle | Armed`; the prototype cleared `UsingUI`, retried the unchanged helper, and both validator calls returned zero. The user confirmed top-row `5` executed its skill normally without displaying the Quick Menu. The next test expands the same guard across `5..0` and directly invokes Plasmatica through top-row `7` (ordinal `2`, native slot `1`).

### Direct Plasmatica confirmation

- The expanded prototype recorded top-row `7` as action `0x7C`, ordinal `2`, resolving native slot `1`.
- The initial call returned readiness result `2` in `Armed | Strafing`; the delayed transition produced `Idle | Armed` and cleared `UsingUI` before retry.
- The helper validation and the internal `CSkill::Use` validation both returned zero. `CSkill::Use` returned success with an active task.
- The Plasmatica trace completed in `14.992 s` with the normal task end. Internal render-camera calls occurred; the user saw no camera sequence at the constrained test position, consistent with the known space/collision-dependent presentation behavior.
- This is the first confirmed real-time, no-menu ranged Skill Strike activation. Remaining Phase 6 work includes the other equipped slots, Ailish, consumables, configurable bindings/loadouts, and encounter-level playability.

## 2026-08-13 — Spirit Strike direct-input entry

### Confirmed statically

- The PC input table and active `PlayerOptions.xml` contain no dedicated Spirit Strike action. Vanilla's own tutorial directs the player through the Quick Menu.
- Quick Menu selection handler RVA `0x00099320`, category `1`, calls Spirit Strike validator RVA `0x00010940` with the manager and selected integer ID. It closes the menu and calls activation RVA `0x0000FBA0` only when validation returns zero.
- The activation implementation validates again and refuses to begin when the manager is already active. It uses the selected definition, gathers eligible party members, starts native state, and sends `OnSpiritStrikeStarted`.
- The definition lookup accepts IDs `0..15` as eight two-entry character pairs. The first four pairs resolve the four main party-character resource types.
- Observation-only hooks now log the Quick Menu's selected Spirit Strike ID, validation result, and activation return. Their inert-image install/restore test passes.

### Live experiments

The current save did not give Elco a Spirit Strike, so the user activated Ailish's first entry instead. The trace recorded ID `2`, validator result `0`, and activation result `1`; the user reported normal execution.

The first automatic-ID prototype incorrectly treated the party/group owner at VA `0x00808D94` as the active character and attempted a virtual type lookup. Pressing `G` crashed before reaching the direct-validator log. That resolver was removed completely; VA `0x00808D94` must not be documented as a confirmed character pointer.

A fixed-ID `2` prototype then passed validation and returned activation result `1` without a UI transition, but did not complete. The game entered deeper slow time with frozen allies and locked character switching. Repeated presses only moved the camera because the manager remained active. This showed that a successful activation return alone was insufficient and that the menu path supplied required transition context.

The next revision added the native UI/control transition but also added an invalid diagnostic state gate. It again interpreted pointer-like party/group-owner fields as arbiter flags and aborted every activation as `not_idle_armed`. That diagnostic gate was removed; no actor-state conclusion is drawn from those values.

The final disabled-by-default `G` prototype polls on the native main-thread frame call at RVA `0x0028DDBA` and submits the explicit configured ID `2`. It calls the unchanged validator, enters the native UI/control transition, waits 75 ms on a game-thread timer, exits the transition, validates again, and calls the native activation implementation only on result `0`. The exact-image install/restore test passes.

The final live trace recorded initial validation `0`, post-transition validation `0`, and activation result `1`. The user confirmed that Ailish performed the complete stage-clearing Spirit Strike and that it stopped normally. Four later `G` presses during the active move returned native validation result `4`; none started another activation. This completes the direct Spirit Strike proof while preserving the native readiness, SSP, party, animation, damage, and cleanup machinery.

ID `2` remains temporary, but the hardcoded key has now been replaced by the first mod input binding. `[Bindings] SpiritStrike` defaults to `G`, accepts one named keyboard or mouse button, and fails safely when an invalid value is enabled. The polling path also requires the foreground window to belong to the Sudeki process. A dedicated Wine parser test passes across valid names, aliases, ranges, and invalid input. The separate PE32 build completed without warnings, and the exact-executable inert-image install/restore test passes with the new hook signature. Chords, controllers, an in-game controls screen, and character/equipment-aware Spirit Strike selection remain future work.

The user also confirmed that native consumable slots `1..4` are a standard working PC feature. SudekiMP does not intercept or alter those actions, so no separate consumable bypass is required.

### Configurable binding live confirmation

The generated test configuration changed `[Bindings] SpiritStrike` from `G` to `H`. Initialization logged virtual key `0x48`. Pressing `H` logged a rising edge for `0x48`, initial and post-transition validator results `0`, and activation result `1`; the user confirmed the Spirit Strike executed. This closes the first configurable mod-action binding. The next functional gap is choosing the correct equipped/controlled-character Spirit Strike definition instead of always submitting captured Ailish ID `2`.

### Corrected front-character resolver candidate

Disassembly of native QuickSkill RVA `0x00027BF0` shows that VA `0x00808D94` is the group object, not a character. The helper passes `group+0x90` to the intrusive-pointer acquisition routine at RVA `0x000015B0`; the acquired first entry is the character whose `CSkill` component is used. Spirit Strike's native party loop independently calls virtual slot `+0x10` on the embedded component at `character+0x2C` to obtain the resource type. This explains the first resolver crash without contradicting the later group-owner findings.

The corrected disabled prototype uses that front character only when `SpiritStrikeId=-1`, validates that the virtual target lies inside the exact supported executable image, maps primary resource types to ID pair starts, and adds configured `SpiritStrikeVariant=1|2`. Null groups, null characters, missing vtables, unexpected function pointers, and unsupported resource types abort with a log rather than activating. Fixed IDs `0..15` remain available for diagnosis. The isolated PE32 build and exact-image hook install/restore test pass.

The live automatic test controlled Ailish and logged group `0x0596E954`, front character `0x07CAAAB0`, resource type `0x01`, variant `1`, and resolved ID `2`. Initial and post-transition validators returned `0`, activation returned `1`, and the user confirmed the move fired normally. The corrected resolver is therefore confirmed for Ailish. Tal, Buki, and Elco pair mappings remain strong static findings until an available live Spirit Strike can exercise each one.

## 2026-08-13 — Encounter validation deferred

The user elected to defer the full no-Quick-Menu encounter playtest. Direct consumables, melee skills, ranged-skill readiness cycling, configurable Spirit Strike input, native activation, and automatic Ailish definition selection have already been confirmed individually. The encounter test remains an integration check and is not recorded as complete. Research now advances to observation-only character human/AI control switching; the deferred encounter must still pass before Milestone 3 is formally closed.

## 2026-08-13 — Multiplayer resource and camera constraints recorded

The user recalls that Spirit Strike power behaves as a party-shared meter: using it as one character appears to deplete it for the others. This is a reported gameplay observation pending a controlled check, not a confirmed engine fact or storage layout. Local multiplayer and eventual networking should conservatively treat the meter as shared and arbitrate simultaneous activation requests until its behavior, owner, write path, and replication model are traced.

The camera plan now explicitly includes scoped ranged-character presentation. Shared-camera local co-op remains the first target, with bounded party separation. Later independent/split views must define per-viewport zoom, camera ownership, and local-character model visibility so Elco's gun scope and Ailish's ranged view do not expose only arms/weapons, hide the wrong model, or globally take over another player's view. Skill cinematics and Spirit Strikes likewise require a multiplayer camera policy rather than inheriting vanilla's global camera seizure unchanged.

## 2026-08-13 — Native character-switch ownership trace

### Confirmed live

- The executable action table assigns `ac_PrevChar` ID `0x32` and `ac_NextChar` ID `0x33`. The input event handler at RVA `0x000277B0` records these at controller action states `+0xFC` and `+0xF4`, respectively.
- The handler returned without changing the party or controller target. The ownership change appeared approximately 64 ms after each key-down, proving that a later frame consumer performs the switch.
- Before Previous, group `0x0596E954` contained characters `0x07CD1B00`, `0x07CD36A8`, `0x07CDA030`, and `0x07CDBBB8` in slots 0 through 3; controller `0x00A7DFA8` targeted slot 0.
- Previous rotated the ordered entries right to `BB B8, 1B 00, 36 A8, A0 30` (full pointers above) and changed `controller+0x248` to `0x07CDBBB8`.
- Next rotated the entries left back to the original order and restored the target to `0x07CD1B00`. Both results remained stable through the one-second snapshots.
- `CGroupPlayers+0xD6` remained `1`; controller modes `+0x80/+0x84` remained `1`; every character byte `+0x2A` remained `1`; actor snapshots and all four `character+0x94` pointers remained unchanged. Those fields cannot individually identify the human-controlled character.

### Confirmed statically

- Controller frame update RVA `0x00027CF0` checks Previous `+0xFC` and Next `+0xF4`, then calls dedicated consumers at RVAs `0x00023F60` and `0x00024060`.
- The consumers validate party count, switch state, front-character state, and world conditions before calling rotations at RVAs `0x00023CE0` and `0x00023B50`.
- Both then call shared reassignment RVA `0x000237B0` with the group, old front, and new front. This routine assigns the new character to global controller target `+0x248`, resets relevant old/new arbiter and character-component state, transitions their `character+0x94` components through RVA `0x000EF700`, and notifies downstream camera/controller listeners.
- RVA `0x000EF700` sets the old component's nested `[component+0x3C]+0x0B` byte to `1` when eligible and invokes the same internal mode setter on the new component with `0`. This is the strongest human/AI enable-state candidate, but its exact meaning remains a hypothesis until the focused live trace observes it.

### Next experiment

The first focused build wrapped the old/new transition call and recorded a startup assignment with no old character: the new character's nested mode changed `1→0`. Wine then exited during the known-problematic startup/intro period with process status `0` and no exception recorded in `SudekiMP.log`. Although this does not prove the wrapper caused the exit, it was unnecessary active instrumentation and was removed.

The corrected character-switch trace only passively reads the nested mode byte in the already-proven before/immediate/delayed snapshots.

## 2026-08-14 — AI-control mode confirmed and separation prototype prepared

### Confirmed live

The passive Previous/Next run resolved the ownership field without intercepting any transition code:

- Before Previous, group slot 0 and `controller+0x248` both referenced character `0x07CD33B0`. Its `character+0x94` component was `0x07CD3CE4`, and nested `[component+0x3C]+0x0B` was `0`. The other three party characters all held mode `1`.
- Approximately 65 ms after Previous, group slot 0/controller target became `0x07CA1A98`; its nested mode changed `1→0`, while old front `0x07CD33B0` changed `0→1`.
- Next restored `0x07CD33B0` to slot 0/controller target and reversed the same two mode values.
- Exactly one party member had mode `0` in each stable vanilla snapshot. All observed `component+0x16A` override refcounts remained zero.

This confirms that nested mode `0` is AI inactive for the currently human-controlled/front character and mode `1` is normal AI-active behavior in the vanilla party path. Confidence is high for this build.

### Confirmed statically

The PE export table names three relevant functions (including the game's original spelling):

- `AiIsOverriden(TPtr<Entity>*)`, RVA `0x000F60A0`
- `AiOverrideControl(TPtr<Entity>*)`, RVA `0x000F60D0`
- `AiDefaultControl(TPtr<Entity>*)`, RVA `0x000F6100`

All resolve the character from the supplied intrusive `TPtr<Entity>`, then its component at `character+0x94`. `AiOverrideControl` invokes RVA `0x000EC350` with true; the first acquire increments `component+0x16A` to one, clears AI work, and sets nested mode `0`. `AiDefaultControl` invokes the same routine with false; the final release returns the count to zero and restores mode `1` when that character is not the current controller target. This is safer and more reversible than direct byte modification or calling the private mode setter.

### Prepared experiment

A disabled-by-default control-separation module now hooks the controller update vtable pointer at RVA `0x002C9F60`, whose exact expected target is RVA `0x00027CF0`, solely to poll a rising key edge on the game thread. `ToggleBukiAi=J` finds resource type `0x05` in the four active group slots, refuses to run if that character is slot 0/controller target, and calls the native override/default exports through the slot's real `TPtr<Entity>` address. It verifies `+0x16A` and the nested mode after both acquire and release and attempts a native rollback if acquire verification fails. It does not assign input, move the controller target, write an executable file, or remain enabled in the normal configuration.

The PE32 build completed without warnings. The exact-image test mapped the supported `SUDEKI.exe` inertly, verified the controller-update vtable pointer was redirected, uninstalled the hook, and verified the original target was restored. Live gameplay behavior remains pending.

### Live result — Milestone 4 complete

The user loaded gameplay with another character under normal human control and pressed `J`. Buki immediately stopped acting while the controlled character remained usable. The log recorded:

```text
control_separation event=override result=success slot=1 character=0x08289660 component=0x08289f94 control_ref_16a=1 ai_enabled_3c_0b=0
```

On the next `J` press, Buki resumed normal behavior. The release logged:

```text
control_separation event=restore result=success slot=1 character=0x08289660 component=0x08289f94 control_ref_16a=0 ai_enabled_3c_0b=1
```

The user performed a second complete cycle in the same run; both acquire and release again passed the exact post-call checks. This proves Sudeki can retain its normal human-controlled front character while a second party character has AI disabled, and that the change is reversible through native APIs. Milestone 4 is complete. It does not yet feed input to Buki; that is the next engine boundary to trace.

## 2026-08-14 — Per-character movement boundary

### Static result

Controller frame update RVA `0x00027CF0` calls a dedicated movement-vector consumer at RVA `0x00028B00`. In normal movement mode `0`, that function:

1. Reads local movement axes from controller `+0x1A0/+0x1A4` (with an optional one-shot value at `+0x1BC`).
2. Preprocesses the axes at RVA `0x000289D0`.
3. Uses the stored camera transform at controller `+0x1F0` through RVA `0x000291A0`.
4. Normalizes the horizontal direction and resolves the character at `controller+0x248` and its arbiter at `character+0x90`.
5. Calls RVA `0x000DAE80` from RVAs `0x00028E3F` or `0x00028E5E` with `(arbiter, direction, speed, 1.0, 0)`.

RVA `0x000DAE80` is a callee-cleaned five-argument function. It preserves native movement-state gates, writes accepted speed/turn rate/mode to the character's movement controller at `character+0x80`, and writes the direction vector to the component at `character+0xAC`. AI update RVA `0x000F4BB0` calls this same function for AI-controlled character movement, making it a genuine per-character boundary rather than a Player-1-only implementation.

### Passive live result

A disabled-by-default trace wrapped only the two Player 1 call instructions, sampled at 10 Hz, and forwarded every argument unchanged. The exact-image test verified both calls redirected and restored. In normal third-person gameplay, the user moved with `W/A/S/D`; every captured record used character `0x07DC86B0`, arbiter `0x07DC8F80`, a normalized horizontal world vector with `Y=0`, turn rate `1.0`, and movement mode `0`. Speed bit patterns decoded to `1.0`, approximately `1.500`, and approximately `1.803`. Directions changed smoothly with the camera and movement input.

This confirms the input-to-character movement seam needed for the first local two-player proof. The next experiment will keep Buki's native AI override active and submit a conservative normalized world-axis direction to her arbiter from otherwise-unbound `I/J/K/L` input while leaving Player 1's controller and `W/A/S/D` path untouched. The dedicated launcher temporarily moves the AI toggle from `J` to `F10`. Arrow keys were rejected because the user's vanilla control file already binds them as Player 1 movement alternatives; numpad keys were rejected because the user's keyboard lacks Num Lock.

### Live result — Milestone 5 complete

With a non-Buki character still controlled through the unchanged Player 1 route, the user enabled Buki's native AI override with `F10` and moved Buki through a separate `I/J/K/L` source. The log recorded repeated cardinal and normalized diagonal submissions to one stable Buki character/arbiter pair, including:

```text
control_separation event=second_player_movement phase=submit character=0x0828bea8 arbiter=0x0828c778 input_x=-1 input_z=-1 speed_bits=0x3f800000 turn_rate_bits=0x3f800000 movement_mode=0
control_separation event=second_player_movement phase=submit character=0x0828bea8 arbiter=0x0828c778 input_x=1 input_z=0 speed_bits=0x3f800000 turn_rate_bits=0x3f800000 movement_mode=0
```

Releasing the movement keys submitted the native stop operation. The final `F10` restored Buki through `AiDefaultControl`; verification passed with override refcount `0` and AI mode `1`. The user completed the independent/simultaneous movement test successfully. This is the first true single-process local multiplayer proof: two party characters accepted separate human input at the same time. The Buki directions are deliberately fixed to world axes and no second attack, targeting, camera, or UI ownership exists yet.

### Doorway/party recovery observation

During the same run, crossing a doorway that begins a party move repositioned Buki into the scripted walk even though ordinary Buki AI was overridden. When she failed to complete the movement normally, the game later accelerated or otherwise forced her through. This is confirmed as visible behavior, but the responsible function and exact mechanism are not yet identified. It suggests doorway/zone recovery has authority separate from ordinary combat AI.

Do not blindly remove this recovery. The multiplayer design should distinguish ordinary same-map separation from committed transitions: require all human players at a transition boundary (or an explicit host/group decision), then let the transition own and place the party before restoring each player's control. Cutscenes require a separate identity test to determine whether their visible cast uses the live party entities, temporary scene actors, or a mixture; only then can absent-player and local-body visibility rules be chosen.

## 2026-08-14 — Free-roam camera input experiments deferred

### Confirmed static/input facts

- `PlayerOptions.xml` binds `ac_CameraU`/`ac_CameraD` to mouse Y (`Type=2`, `Key=1`) and weapon next/previous to mouse wheel directions (`Type=2`, `Key=2/3`).
- The character input handler at RVA `0x000277B0` receives weapon actions `0x2F/0x30` and camera actions `0x69/0x6A`. It writes the camera float payloads to controller offsets `+0x184/+0x188`.
- `CGroupPlayers::InCombat()` is exported at RVA `0x00004FA0` and returns group byte `+0xD4`.
- Camera exports anchor `GetCameraManager` at RVA `0x00038C40`, `CCameraManager::LoadConfig` at `0x000375F0`, `CCameraManager::SetCameraConfig` at `0x00037CD0`, and `CCamera::GetConfigFloat` at `0x000E8D50`.
- The `DEFAULT` data profile contains exploration default/min/max/absolute-max distances `3.5/3.5/6.2/8.5`, user-distance scale `8.0`, and combat values `6.0/6.0/10.0/10.0`.

### Live results

Three increasingly late wheel routes produced no visible free-roam zoom:

1. Direct action remapping preserved the wheel's observed `+/-12.0` float payload, but each relative-axis event was followed by an immediate zero event.
2. Holding the remapped native action for 50 ms prevented that immediate reset but still produced no visible camera change.
3. Queueing a notch and writing `+/-1.0` to controller `+0x184/+0x188` after the native controller update logged one injection for every notch, with zero replaced values, but again did not change the camera.

A fourth prototype suppressed free-roam mouse-Y camera actions unless configurable `LeftCtrl` was held. Seven Ctrl down/up cycles were detected, and the hook followed its non-combat branch, but the user still observed no useful result. The run was classified only by `CGroupPlayers::InCombat()==false`; the active camera/profile and other gameplay state were not independently captured. The modifier result is therefore unsuccessful with a small remaining state ambiguity.

### Conclusion

Controller `+0x184/+0x188` is confirmed input staging, not a proven durable camera control point. Repeating remaps there is not justified. The next camera investigation must begin with a vanilla state where mouse Y visibly changes distance and trace the actual desired/current-distance writer, active camera/profile, config reads, and clamp function. A broader multiplayer camera replacement may ultimately be preferable, but it should be based on that state path rather than blind camera-object writes. The prototype remains disabled by default.

## 2026-08-14 — Per-character normal-attack boundary

### Confirmed static result

The exact-build, read-only Ghidra report `tools/ghidra/AttackInputReport.java` traced the normal combat actions from the character input handler at RVA `0x000277B0` into the controller combat consumer at RVA `0x000286C0`. The action IDs are Weak `0x2C`, Strong `0x2D`, Sweep `0x2E`, Weapon Next `0x2F`, Weapon Previous `0x30`, and Block `0x31`.

At RVA `0x0002891F`, the controller passes its chosen character arbiter to RVA `0x000DB0E0`. That callee is a genuine per-`CCharacterArbiter` combat-input boundary rather than a Player-1-only helper: `ECX` carries the supplied arbiter, `EAX` carries Block, and five stack arguments carry Weak, Strong, Sweep, Weapon Next, and Weapon Previous. The function returns with `ret 0x14`, cleaning those five stack arguments. Its exact entry bytes are:

```text
55 8B 6C 24 08 56 57 8B F8 8B F1
```

The function validates the supplied arbiter's owner and combat components, state/capability flags at `+0x50/+0x58/+0x60`, attack transitions, and weapon requirements. For the normal melee branch it maps state value `1` to weak kind `1`, strong kind `2`, and sweep kind `3` before calling provisional dispatch RVA `0x000DAC00`. Targeting is not replaced by this seam. A second native caller at RVA `0x000DA816` further supports that this is shared arbiter behavior.

### Prepared experiment

A disabled-by-default `EnableSecondPlayerWeakAttackPrototype` now uses an isolated i386 assembly adapter to reproduce that ABI. With Buki's existing native AI override active and verified, a rising `U` edge submits Weak `1` and zero for Strong, Sweep, Block, Weapon Next, and Weapon Previous to Buki's own arbiter. The path refuses to act if Buki is front/controller-owned, outside the active party, lacks the required components, or no longer has the verified override state. The supported entry signature must match before installation.

The PE32 ABI test captured the expected `ECX`, `EAX`, five stack arguments, and native stack cleanup under Wine. The exact-image test then installed and removed the combined control-separation/movement/attack hook against an inert mapping of the supported `SUDEKI.exe`; all preflight tests passed. The focused launcher mode is `tools/continue-research.sh --second-player-attack-test`: `F10` toggles Buki's native AI override, `I/J/K/L` remain the fixed-axis movement proof, and `U` requests only Buki's weak attack.

### Live result — independent Buki combat input confirmed

With a different character remaining under Player 1 control, the user disabled Buki's AI, fought through a complete battle, and won. The log kept one stable Buki character/arbiter pair (`0x07C861B0` / `0x07C86A80`) for both movement and every weak-attack submission. The arbiter's `+0x50` flags moved through native idle/attack values including `0x00000003`, `0x00001002`, and `0x00002002` while `+0x58` remained `0x3C000012` and `+0x60` remained `0x72`. This confirms independently submitted Buki attack input reaches and advances her native combat state while Player 1 remains separate.

The user also observed that AI-overridden characters still appeared to lock onto the nearest target. This is strong live evidence that target selection/facing persists outside the disabled high-level AI decision path, but the exact target pointer and writer were not captured in this run. Treat retained native targeting as a useful confirmed behavior at the visible level and its mechanism as a hypothesis pending a passive target trace.

## 2026-08-14 — Camera-relative movement and separation preflight

### Shared movement-camera transform

The native Player 1 movement consumer calls RVA `0x000291A0` at RVA `0x00028C60` with `(controller, output_vector, local_input_vector)`. Disassembly confirms three stack arguments and `ret 0x0C`. The function refreshes or reuses the controller matrix at `+0x1F0`, clears translation/projective terms in a local copy, and calls `D3DXVec3TransformCoord`. Supported-build entry bytes are:

```text
55 8B EC 83 E4 F0 8B 55 08 D9 EE
```

A new `EnableSecondPlayerCameraRelativeMovementPrototype` option, disabled by default, passes Buki's local `I/J/K/L` vector through that exact helper on the game thread, clears vertical output, horizontally normalizes it, and then uses the already-confirmed arbiter movement submission. It has an independent signature gate. The PE32 build and inert exact-image install/restore test pass.

### Live result — shared-camera-relative Buki movement confirmed

With Ailish remaining Player 1 and the visible camera focus, the user disabled Buki's AI and confirmed that Buki's independent movement rotated as the shared camera rotated. The log retained one Buki character/arbiter pair (`0x08283F18` / `0x082847E8`), marked every submission `camera_relative=true`, and recorded substantially different normalized X/Z world vectors for repeated identical local inputs as the camera changed. Buki's override was then released with refcount `0` and AI mode `1`. This establishes the intended X-Men Legends-style first camera model: Player 1 owns one shared camera, while each local player's input is transformed through that same current camera basis.

### Non-teleporting maximum-separation boundary

Exported `SetPlayerPosition(float,float,float)` at RVA `0x00104ED0` resolves active group slot 0, reads `character+0x44`, and invokes RVA `0x00003050`. That setter compares and writes the three coordinates at `CPosition+0x18/+0x1C/+0x20`, establishing a read-only world-position boundary for every party character sharing the layout.

A separately disabled `EnableSecondPlayerSeparationGuardPrototype` compares AI-overridden Buki's X/Z position with the current controller target. At or beyond configurable `SecondPlayerMaximumSeparation`, it rejects only a movement direction whose horizontal dot product points farther outward. Inward and tangential movement remain available. Missing/invalid position state fails closed, and the prototype never writes a position, teleports a player, accelerates catch-up, or changes doorway transitions. The initial `10.0` value is explicitly a test value, not a balance choice. Build, inert-image preflight, and the live proof pass.

### Live result — outward-only separation confirmed

With camera-relative Buki movement active, the user observed that Buki could no longer travel beyond the configured range while all inward and sideways movement continued to work. The current log held one stable Buki character/arbiter pair (`0x083A5DA8` / `0x083A6678`) and repeatedly alternated `separation_guard phase=block` with `phase=release reason=inward_or_within_limit`. Block records clustered near distance-squared `100` for the 10-unit limit; input submission resumed without a position write as soon as the direction was no longer outward. This confirms the non-teleporting policy and leaves `10.0` as a test value rather than a final camera/balance choice.

The final run was closed through the explicit emergency-stop helper at the user's request. Its generated INI was restored to the repository's disabled defaults. An earlier restore in the same session cleanly released Buki's override; the final in-memory acquisition ended with process termination rather than another native restore call.

### Windowed and OBS Game Capture launch path

The dedicated research profile stored native `FullScreen=True` in its UTF-16 `PlayerOptions.xml`. `tools/configure-windowed.sh` now preserves the first untouched options file beside it, changes only that Boolean, validates a UTF-16 round trip, and provides explicit windowed/fullscreen/check modes. Research launches select native windowed mode by default; this does not patch Sudeki or change its focus-pause policy.

Existing Lutris configuration on this host uses `prefix_command: obs-gamecapture`. The host provides i686 and x86_64 capture hooks, while Flatpak OBS includes the OBSVkCapture source plugin. `tools/run-wine.sh --obs-gamecapture` now applies the same wrapper to Wine. A live PE32 launch logged `Init GLX 1.5.1 (32bit)` before Sudeki rendering initialized, confirming injection of the correct-architecture OpenGL capture hook. User confirmation of the OBS preview itself remains pending.

### Passive retained-target trace and live result

Existing Plasmatica analysis already anchors the ordinary target system: `character+0xAC` is the native `CTargeter`, its intrusive current-target node is at `+0x54`, auto-target enabled is bit `0x02` at `+0x84`, and `CTargeter::GetGelCurrentTarget()` is exported at RVA `0x000B9DC0`. Disassembly confirms the getter copies/resolves the native pointer and leaves the target unchanged.

`EnableSecondPlayerTargetTrace`, disabled by default, samples those fields at up to 10 Hz only while Buki's verified AI override is active and the game owns the foreground. The live run acquired Buki at character `0x07C56948`, component `0x07C5727C`, and targeter `0x07C57558`. Across 1,181 samples, the ordinary target node remained one stable non-null value (`0x081E2290`) and auto targeting remained enabled. The final restore changed the verified override refcount to `0` and AI mode to `1`, confirming clean ownership release.

The first diagnostic also called `GetGelCurrentTarget` and logged its returned address. That address changed almost every sample despite the unchanged node, so it is an ephemeral wrapper/scratch address rather than a valid target-identity key. The call and field were removed. The corrected trace performs only passive reads and compares the durable node and flag. This result confirms that native targeting state survives removal of Buki's high-level AI; it does not yet identify the target entity, scoring logic, or writer.

## 2026-08-14 — Native camera-target seam and midpoint prototype

### Confirmed statically

Three exact-build, read-only Ghidra reports now cover the gameplay target handoff and target class hierarchy: `CameraTargetReport.java`, `CameraTargetHierarchyReport.java`, and `CameraTargetSemanticsReport.java`.

- `CCameraManager::SetCameraTarget` at RVA `0x00037170` does not store a raw position. It resolves a supplied entity into reference-counted `Camera::GameObjectTarget` and derived target objects, then installs targets into `CCamera+0xB4/+0xB8`.
- Shared character reassignment calls RVA `0x0002A370`. That function acquires the new front character's cached `GameObjectTarget`, installs the same target into both camera slots through RVA `0x000E84C0`, and runs the native transition policy.
- Normal live exploration instead held `Camera::OffsetTarget` (vtable RVA `0x002D436C`) in slot 0 and the front character's `GameObjectTarget` (vtable RVA `0x002D42CC`) in slot 1. `OffsetTarget` composes the wrapped target transform with its native framing matrix and exposes the result through the shared target interface.
- The current `CGameCameraMode` singleton is held at RVA `0x00408DA8`. Its `+0x0C` member points to `CCamera+0x2C`, allowing the active `CCamera` base to be recovered without searching arbitrary heap memory.
- RVA `0x00134FB0` allocates a native 0x80-byte `Camera::MatrixTarget`, copies a supplied 4×4 matrix to target `+0x20`, points `+0x60` at that matrix, updates its cached position, links it into the camera target manager list, and returns one owned reference.
- `MatrixTarget` virtual `+0x10` returns the matrix translation row (`matrix+0x30`), while virtual `+0x20` returns the whole matrix. `GameObjectTarget` exposes the controlled entity position and world matrix through the same virtual slots. This makes a synthetic focus compatible with the existing camera consumer rather than a separate raw-coordinate patch.
- RVA `0x00135340` is the native zero-reference unlink/destruction path for the manager's target lists. The prototype therefore uses Sudeki's own create/install/release lifecycle.

Supported entry signatures used by the prototype are:

```text
Install target RVA 0x000E84C0:
53 8B 5C 24 0C 8B 94 9E B4 00 00 00

Create MatrixTarget RVA 0x00134FB0:
53 55 8B 6C 24 0C 68 80 00 00 00

Release target RVA 0x00135340:
53 56 8B 77 04 33 DB 32 C0
```

### Live result — midpoint focus and native restoration confirmed

`EnableSharedGroupCameraPrototype=false` remains disabled by default. While the verified Buki AI override and second-player movement are active, it:

1. Accepts only the live-confirmed `OffsetTarget`/`GameObjectTarget` gameplay pair or the native same-`GameObjectTarget` pair; other target types remain untouched.
2. Retains both original targets and preserves the slot-0 composed framing matrix.
3. Creates one engine-owned `MatrixTarget` and translates that framing matrix by the Player 1-to-P1/Buki-midpoint delta.
4. Installs that target into both native camera slots and updates its matrix each controller frame.
5. Restores only slots still owned by the prototype, then releases every retained/native reference if AI is restored, the camera changes, the engine replaces a target, or required state becomes invalid.

The first visual run rejected the earlier same-target-only assumption safely. Its live pair was `OffsetTarget` in slot 0 and `GameObjectTarget` in slot 1. After accepting that confirmed pair, the user observed that the camera was "certainly shared": it followed the space between stationary Ailish and independently moved Buki rather than remaining locked to Ailish. The acquire log recorded the two originals, one native `MatrixTarget`, and policy `two_player_centroid_preserve_native_offset_no_zoom`.

The first attempt to restore Buki AI produced Microsoft runtime error R6025, "pure virtual function call." Step logging isolated the failure inside the slot-0 reinstall. Before restoration the two native originals had reference counts `1` and `3`, but the `MatrixTarget` had only `1` despite occupying both slots. Disassembly and the live counts together establish an important ABI rule: RVA `0x000E84C0` does not create the persistent slot reference itself; its caller must retain the supplied target once per install. The first restore therefore decremented the synthetic target's only reference to zero while slot 1 still pointed at it.

The corrected path retains the synthetic target before each of its two installs and retains each original before reinstalling it. On the next run the synthetic target entered restoration with reference count `3`. Both native targets reinstalled, the held originals released, the creator reference released the synthetic target, Buki AI resumed, and the camera returned to Ailish immediately without a crash. The final log reached `shared_group_camera phase=restore` cleanly.

This proof changes one camera's focus only. Zoom, distance limits, collision, skill-camera ownership, ranged scope behavior, and render view count are unchanged. Adaptive distance is the next shared-camera task. Split-screen remains a separate rendering investigation requiring a second render camera, viewport/scissor control, aspect policy, and another scene submission.

## 2026-08-14 — First dual-viewport rendering proof

### Exact-build static seam

`tools/ghidra/SplitScreenRenderReport.java` traced the supported executable's D3D9 frame path without changing the analyzed image. The active device pointer is held at RVA `0x003C31DC`; native frame begin and end are RVA `0x001DD200` and `0x001DD540`; the native viewport wrapper is RVA `0x001DCE30`; and the main frame/render loop is RVA `0x0028D3F0`.

The replayable graphics helper at RVA `0x001D4750` receives its renderer in `EAX`, its world context in `EDI`, and no stack arguments. It is called once by the global frame layer at RVA `0x0028D473` and three times inside gameplay-world render owner RVA `0x0000A5B0`, at call-site RVAs `0x0000A62D`, `0x0000A689`, and `0x0000A738`. The isolated `render_phase_abi_test` verified the custom i386 adapter, and the inert exact-image test verified installation and restoration of the supported relative-call sites.

### First live result

A disabled-by-default prototype read the full D3D9 viewport, divided it into left/right halves, and replayed the same native graphics phase in each half before restoring the full viewport. The live log recorded a `1368x768` full viewport divided into two `684`-pixel halves. The user confirmed a genuine side-by-side split; a later 1920x1080 capture clearly showed both halves rendering gameplay in the same Sudeki process.

This is a render-twice proof, not independent-camera completion. Both halves selected the same native camera. Sudeki's current minimap, item bar, and character dial were duplicated or laid out as global HUD content rather than viewport-owned Player 1/Player 2 interfaces.

The capture also established four defects in the broad first pass:

1. The title/main menu was split because the global frame-layer call was duplicated.
2. Large black shadow or visibility regions appeared, especially in the left viewport.
3. Some door/scene geometry visible in one viewport was missing in the other.
4. The bottom-right status dial and other HUD elements had no player-to-viewport ownership.

The shadow and geometry symptoms are confirmed observations. Their cause is still a strong hypothesis: rendering the global layer and all world subpasses twice may re-run a shadow/visibility producer or consume a transient scene queue before the second view. Do not label those defects fixed without another live comparison.

### Second live result — subpasses isolated

The second version removed the primary call at RVA `0x0028D473` and replayed only the three calls owned by the world renderer. Live behavior disproved the assumed presentation boundary: the title and main menu remained split, while loaded gameplay did not split. No black shadow regions were present and the previously missing door rendered correctly. The Ailish/Buki plates still reflected Ailish because per-viewport player/HUD ownership remains unimplemented.

This establishes that the primary call is necessary for the visible gameplay split, while replay of the three conditional subpasses is unnecessary and correlated with the first run's shadow/door corruption.

### Third live result — gameplay gate confirmed, draw replay still corrupts

The current version hooks only the proven primary call at RVA `0x0028D473` and leaves all three world subpasses single-run. Because the same call is reached by menu presentation, replay is permitted only when three engine ownership facts agree: the active group exists, its party slot 0 equals the character controller target at `+0x248`, and the current game-camera mode exposes a readable camera pointer. Title, menu, loading, and teardown states fail closed to one full-width native call.

The third live run confirmed the state boundary. Title and main menu remained one full-width view; after loading, gameplay became side-by-side. The log recorded `gameplay_gate state=inactive reason=front_character_not_controller_owned`, followed by `state=active reason=active_party_controller_camera`. The large black shadow figures/regions returned and the door again vanished from the right view. This proves menu gating is solved while final-draw-only replay remains invalid. The Ailish/Buki plates still reflected Ailish as expected because HUD ownership is unchanged.

### Fourth live result — generation replay rejected

`SplitScreenPrimaryPassReport.java` traced the exact three-call primary sequence. RVA `0x001D48C0` and RVA `0x001D4820` prepare renderer/object work before the final RVA `0x001D4750` draw. The preparation helpers compare per-object 16-bit generation fields with render-generation global RVA `0x003C3150`; native frame begin increments that generation once. The final helper invokes RVA `0x00226F30`, which flushes shared callback queues through RVA `0x00226E90`. The same queue machinery explicitly carries `cShadowRenderCallback`. Replaying only the final helper therefore consumes callbacks on the left and enters the right draw without equivalent per-view work.

The fourth disabled experiment hooked the first call site at RVA `0x0028D45B` and the final call at `0x0028D473`. During verified gameplay it set the left viewport before the native first/middle/final sequence. After the left draw, it selected the right viewport, advanced the render generation, reran first and middle preparation with zero float/delta bits, and submitted the right final draw.

The user observed that neither player characters nor NPCs moved in that build. The black shadow figures remained, and the door still failed to render in one view even though NPCs behind it were visible. This is a live rejection, not a partial fix. Zero delta did not make the generation update render-only, and rebuilding those callbacks did not give either viewport independent visibility state. The code must not return to this path without materially new engine evidence.

### Fifth experiment prepared — finished-frame compositor

The replacement hooks only main-loop frame-end call site RVA `0x0028D58C`, calls native frame end RVA `0x001DD540` first, and leaves every native update and render submission unchanged. Disassembly confirms frame end calls D3D9 `EndScene` at device-vtable byte offset `0xA8`; the main loop then calls D3D9 `Present` at offset `0x44`, so this seam lies between the two. After the already-confirmed active-party/controller/camera gate succeeds, it obtains D3D9 render target 0, copies the completed full-size native frame into a separate same-size render target, and scales that copy into left and right halves. Both halves intentionally show the same camera and HUD. This isolates whether a split presentation can be added without replaying simulation, preparation, culling, shadow, visibility, or callback queues.

The compositor fails safe: unsupported multisampling or any surface/copy failure leaves or restores the native full-width frame and writes a single diagnostic. The focused launcher therefore captures the existing numeric `AntiAliasing` value, temporarily sets it to `0`, and restores the original value when the process exits. The exact-image test confirms only the frame-end call is redirected and restored while the primary and all three world render calls remain native. Build, ABI, and exact-image tests pass. The pending live comparison is deliberately strict: title/menu full-width; gameplay two pixel-equivalent copies; normal player/NPC motion; identical shadows; and the door present in both halves. Only after that passes should two independent full-size camera targets be introduced. Viewport-relative HUD ownership remains later work: each dial must derive portrait/name/HP/SP/loadout from the assigned character, while the recalled party-wide Spirit Strike resource remains shared unless controlled testing disproves it.

### Fifth live result — clean compositor confirmed

The strict comparison passed. The user confirmed that the title and main menu remained one full-width image until the loaded gameplay appeared through the normal fade. Gameplay then became two identical halves. Player and NPC motion remained normal, the black shadow artifacts were absent, and the previously missing door rendered in both halves. The log transitioned from `gameplay_gate state=inactive reason=front_character_not_controller_owned` to `state=active reason=active_party_controller_camera`, then recorded `compositor_active source=1368x768 format=21 multisample=0 layout=left_right camera_policy=duplicate_finished_native_frame`. No `compositor_failure` was recorded. This closes the safe presentation seam and confirms that the earlier corruption came from replaying Sudeki rendering, not from the final image composition.

### Sixth experiment prepared — separate named Player 2 camera

`DualCameraReport.java` traced Sudeki's native named-camera lifecycle. `CCameraManager::AddCamera` at RVA `0x00036C10` manages a ten-camera table, allocates and initializes a distinct `0x108`-byte `CCamera`, copies a name of up to 20 characters to `CCamera+0x4C`, and applies a named configuration. `GELGetCamera` at RVA `0x00036ED0` returns the independent object; `SetRenderCamera` at RVA `0x00036FB0` notifies listeners and installs it at manager `+0x20`; `RemoveCamera` at RVA `0x00036DE0` performs native destruction after another camera is selected. The game itself uses the same path for `default` and `SpeechCamera`.

The next disabled proof creates `SudekiMP_P2` from `default` only after the gameplay gate succeeds. It retains the exact original Player 1 camera and name, resolves the first non-front active party slot as Player 2, and installs a native `MatrixTarget` that preserves Player 1 framing while translating its focus to Player 2. With the current two-character save this should resolve Ailish as Player 1 and Buki as Player 2. Configurable `F9` toggles both compositor halves between the original and new camera so creation, following, independent camera state, and clean restoration can be verified without yet combining two frames. If the party assignment or native render camera changes, the prototype restores/removes only its own camera and fails safe. The PE32 build and exact-image signature/hook test pass; live behavior is pending.

The user also established a presentation requirement for the simultaneous-camera pass: opening Sudeki's in-game pause/exit menu must suspend the split presentation and draw one full-width shared interface over both views. The current active-party/controller/camera gate has not yet been proven to distinguish that menu from gameplay, so a confirmed menu-state signal is required before this can be called complete.

### Sixth live result — native ownership/restoration confirmed; gameplay framing rejected

The live log recorded `second_player_camera phase=acquire` with distinct Player 1 and Player 2 camera objects and party-slot-1 character ownership. Pressing `F9` then recorded `phase=switch active_player=2`; pressing it again recorded `phase=switch active_player=1` with the exact original Ailish camera and character. The user confirmed that restoration was immediate and ordinary doorway animation/control resumed. Native named-camera creation, selection, and restoration therefore work.

The visual and transition behavior rejects the current Camera 2 construction. While selected, `SudekiMP_P2` showed what appeared to be a skybox or otherwise invalid camera angle rather than a normal Buki gameplay view. Both diagnostic halves showed it because the finished-frame compositor intentionally duplicates the one currently selected native frame; this is not evidence that both the Player 1 and Player 2 camera objects were invalid. The player model could still move and NPC simulation continued. Attempting to enter the castle then held Player 1 in the doorway transition until `F9` restored the original Ailish camera, after which the animation behaved normally.

Two conclusions are confirmed. First, applying the `default` named configuration and translating Player 1's target matrix is insufficient to reproduce the active gameplay camera's complete framing/profile state. Second, Sudeki gameplay and scripted transitions observe the global render-camera selection; Camera 2 must not remain installed there across simulation. The next investigation must trace the active gameplay camera's complete initialization and find a render-only selection/restore seam before any simultaneous-camera compositor is attempted.

### Seventh camera experiment prepared — render-only translated state

The exact-build static report `tools/ghidra/CameraRenderIsolationReport.java` separated the two responsibilities previously hidden inside `CCameraManager::SetRenderCamera` at RVA `0x00036FB0`. Global gameplay ownership is stored at `CCameraManager+0x20`. Independently, the selected camera's `CCamera+0x34` render-state pointer is written to `scene_manager->+0x40->+0x7C`, and the world renderer at RVA `0x0000A5B0` consumes that scene-renderer slot. The camera matrix handoff at RVA `0x000E8320` copies 16 floats into render-state `+0x90`, increments its generation at `+0x2C`, and exposes camera position in matrix elements 12 through 14 (`+0xC0/+0xC4/+0xC8`).

The replacement prototype therefore never calls `SetRenderCamera` for Player 2. At the main render-start call site RVA `0x0028D443`, it copies Player 1's complete current render matrix and related `+0xD0..+0xD8` values into `SudekiMP_P2`'s render state, translates only camera position by the Player 2-minus-Player 1 world-position delta, increments the Player 2 render generation, and temporarily changes only the scene renderer's `+0x7C` pointer. Immediately before the native frame-end/`EndScene` call site RVA `0x0028D58C`, it restores Player 1's render state. Manager `+0x20` remains on the original Ailish camera throughout simulation, scripts, and doorway transitions.

The focused PE32 build and exact-image call-hook installation/restoration test pass. Live behavior is pending. Because the finished-frame compositor still duplicates one selected image, both diagnostic halves are expected to change together when `F9` requests the Buki-centered render. This experiment tests framing and isolation only; it does not yet provide simultaneous Ailish/Buki frames or independent Player 2 rotation/zoom.

### Seventh live result — render ownership isolated successfully

The live comparison passed. With `F9` requesting Player 2, both diagnostic halves showed a sensible view centered on Buki. The user observed no skybox, frozen actors, missing geometry, or doorway failure. Ailish movement and the castle transition completed normally while the Player 2 render state was exposed, and toggling back restored ordinary Ailish framing.

The log confirmed the intended ownership split. `phase=acquire` recorded distinct Player 1/Player 2 cameras and render states. Both `phase=switch` records retained the same `global_render_camera=0x00B5FED8`; only `scene_render_state` changed between `0x04372F70` and `0x04381018`. No camera rejection or compositor failure followed. This is direct evidence that the scene renderer can consume a Player 2-centered state without transferring the gameplay camera observed by scripts and doorway transitions.

One expected framing limitation remains. After the doorway transition, the Buki view sat temporarily too close to the doorway. The current proof inherits Ailish's complete orientation/distance and translates only the camera-position row, while Buki remains behind the leader. Per-player follow offset and smoothing should be tuned after simultaneous Ailish-left/Buki-right presentation exists; this does not invalidate the render-isolation result.

### Eighth camera experiment prepared — alternating clean-frame pair

The next disabled prototype combines the two already-proven safe seams without returning to render replay. Sudeki executes exactly one complete native render per engine frame. The render-start hook alternates between the unchanged Ailish state and the isolated Buki-translated state. After native `EndScene`, the compositor copies that full-size finished frame into the matching Player 1 or Player 2 render-target cache. Once both caches are valid, it scales the latest Player 1 image into the left half and the latest Player 2 image into the right half.

This produces simultaneous Ailish/Buki presentation at the cost of temporal staggering: each camera refreshes every other engine frame and the older half is at most one engine frame behind. It never replays update, render preparation, culling, shadows, visibility, door submission, world draw, or callback queues, so the earlier corruption mechanism remains excluded. On gameplay-gate loss, camera ownership changes, party reassignment, surface/device changes, or module teardown, both cache-valid flags are cleared and the native full-width frame remains the fallback until a fresh pair exists.

The new `EnableDualCameraFrameCachePrototype=false` option and `--dual-camera-frame-cache-test` launcher mode are exact-build gated and disabled by default. The PE32 build and exact-image hook installation/restoration test pass. Live testing must verify Ailish-left/Buki-right ownership, cadence/latency, normal actor motion, clean shadows/doors, doorway transitions, and absence of stale or swapped frames. Duplicated Ailish-owned HUD is expected, and the pause/exit full-screen takeover is deliberately outside this pass.

### Eighth live result — simultaneous Ailish/Buki views confirmed

The alternating cache produced the intended distinct presentation: Ailish remained on the left and Buki on the right, with each view centered on its assigned character. Native mouse camera movement and zoom affected both views. The user could not identify meaningful half-rate jitter and reported that everything otherwise appeared normal.

The log recorded `dual_camera_cache_active source=1368x768 format=21 multisample=0 layout=player_one_left_player_two_right cadence=alternate_engine_frames maximum_cache_age_frames=1 render_passes_per_engine_frame=1`. Camera acquisition used `policy=alternating_render_state_frame_cache`. No compositor failure or camera rejection followed. This is the first confirmed simultaneous two-character camera presentation inside one Sudeki process, although camera input and HUD ownership are still shared.

The user also captured a confirmed presentation defect: opening Sudeki's Quit menu while split presentation is active produces one complete half-width Quit interface inside each cached camera frame. The required behavior is one shared full-width pause/quit layer that overtakes both cameras. This is now a live-confirmed state-gating/UI-composition task, not merely a design note. The likely safe route is to identify an explicit pause/quit-menu state, suspend camera-frame composition for that state, and present the native full-width UI once; do not attempt to crop or visually merge the two duplicated menus.

### Ninth experiment prepared — integrated dual-camera local control

The focused `--dual-camera-local-coop-test` mode now combines only previously proven components: the clean alternating camera cache, F10 native Buki AI override/restore, camera-relative I/J/K/L Buki movement, and the 10-unit outward-only separation guard. Player 1 retains normal W/A/S/D. The intended live result is simultaneous independent Ailish/Buki movement while the left and right halves stay assigned to their respective characters. Mouse rotation/zoom remains shared, Buki has no independent camera input yet, and the duplicated Ailish-owned HUD remains expected.

### Ninth live result — integrated local co-op proof confirmed

The user confirmed the combined test succeeded. F10 disabled Buki's AI while Ailish retained Player 1 control; I/J/K/L moved Buki independently and the left/right camera presentation remained assigned to Ailish/Buki. The log simultaneously recorded `control_separation_install=success`, `dual_camera_cache_active`, a successful Buki override at slot 1, sustained camera-relative movement submissions through one stable Buki arbiter, and repeated separation-guard block/release pairs. No compositor failure or camera rejection appeared.

This is the first integrated local co-op proof rather than a collection of isolated subsystems: two independently movable party characters and two distinct character-centered views operate together inside one Sudeki process. It is not yet a finished multiplayer mode. Camera rotation/zoom remains one shared input, the HUD still reflects Ailish on both sides, the Quit menu is duplicated, Player 2 combat has not been enabled in this combined mode, and controller ownership remains future work.

### Tenth experiment — native full-width Quit-menu takeover

`QuitMenuReport.java` resolved a precise presentation signal on the supported executable. Exported `CPCQuitScreenShow(bool)` is RVA `0x0001DBE0` and resolves the singleton pointer at RVA `0x00408D68`; its internal show/hide call at RVA `0x0001D700` writes the visible byte at `CPCQuitScreen+0x1C2`. The object's per-frame render/update at RVA `0x0001D690` checks that same byte before drawing and is called from the main render loop at VA `0x0068D572`. `CPCQuitScreenEnable(bool)` instead changes a gate/refcount at `+0x1CC`, so it is not used as the presentation test. The on-disk show entry begins `56 8B 35 68 8D 80 00 85 F6 74 0A 8B`. Its four-byte absolute singleton operand is loader-relocated, so the runtime gate verifies the stable opcode bytes plus `loaded_module_base + 0x00408D68` rather than comparing the preferred-base operand blindly.

The first compositor change read only the confirmed `+0x1C2` flag. While it was nonzero, the render-start hook left Player 1's native state selected, the frame-end hook skipped split composition, both Ailish/Buki caches were invalidated, and the native full-width frame reached `Present` unchanged. This policy deliberately did not use generic `PauseEverything` state because the required behavior belongs specifically to the Quit interface.

The first live pass fixed the duplicated interface: repeated open/Back cycles produced one full-width native menu and restored Ailish-left/Buki-right gameplay. It also exposed a presentation mismatch. Because the full native frame remained visible under the menu, the paused background temporarily switched to Player 1/Ailish only.

The refinement preserves the last two valid camera frames as render-target textures rather than invalidating them. A third exact call hook at RVA `0x0028D572` runs immediately before `CPCQuitScreen` draws. It saves the complete D3D state with `D3DSBT_ALL`, disables depth/blend/lighting state for a pre-transformed textured-quad pass, draws Player 1 on the left and Player 2 on the right, restores the state block, preserves the original `EAX` receiver, and invokes the unchanged native Quit renderer. No world, culling, shadow, visibility, or simulation callback is replayed.

The PE32 build and expanded exact-image installation/restoration test pass. The user then confirmed the final presentation while sightseeing: one full-width Quit interface appeared over the frozen last-known Ailish-left/Buki-right view, and Back returned to the live two-character/two-camera presentation. The log recorded `quit_backdrop_active ... layer=native_quit_ui_over_cached_gameplay`, repeated active/inactive shared-menu transitions, and no compositor or backdrop failure. This closes the duplicated Quit-menu task for the current prototype.
## 2026-08-14 — Viewport HUD ownership seam prepared

Read-only Ghidra reports `HudOwnershipReport.java` and `HudOwnershipDetailReport.java` identified `UIPortraitGroup` and `UIPortraitGizmo` as the native bottom-right HUD owners. The gizmo stores its party index at `+0x32C`. Its HP/SP ratios, displayed character name, and status bits all resolve `CGroupPlayers+0x90+index*0x0C` through the intrusive smart-pointer copy at RVA `0x000015B0`; the group-level numeric HP/SP draw separately copies slot 0.

The exact callsites are RVA `0x00181517` for group numeric values, `0x000A9D5B` for gizmo HP/SP ratios, `0x000A9E15` for the character name, and `0x000AACAB` for status effects. The matching unlink helper is RVA `0x000015E0`. This makes source-address substitution safer than rotating `CGroupPlayers`: the native helper still owns all link/unlink bookkeeping, and no gameplay consumer sees a changed party or controller target.

A disabled exact-build prototype now swaps the slot-0 and assigned Player 2 smart-pointer source addresses only while the alternating compositor is rendering Player 2. All Player 1 frames and non-HUD calls remain native. MinGW build and the Wine exact-image install/restore test pass. Live confirmation is pending through `--viewport-hud-test`; the portrait texture may be construction-bound and is explicitly not claimed solved yet.

### Live result — data ownership confirmed, portraits isolated

The user capture confirmed the intended data split. The left dial showed Ailish with `1800 HP` and `240 SP`; the right dial showed Buki with `2400 HP` and `150 SP`. Companion labels also exchanged positions. The large and small portrait artwork did not exchange, remaining Ailish-large/Buki-small in both frames. This is a partial success and cleanly proves that portrait art is not sourced by the four data copies.

`HudPortraitBindingReport.java` traced the separate native route. `UIPortraitGizmo+0x2C` is a `UIElementCycleIcon` bound to `HUD_%d_Portrait`; native refresh RVA `0x000AAB00` resolves the character from the indexed party slot and assigns the matching `SUI_PORTRAIT_TAL/AILISH/BUKI/ELCO.SQX` resource. Its smart-pointer copy is callsite RVA `0x000AAB3A`. Gameplay HUD singleton RVA `0x003C2F9C` contains the four bound gizmo pointers at `+0x138`. A follow-up prototype now refreshes all four portrait nodes through this native path on each camera render, using the same render-local slot exchange. Build succeeds; live portrait validation is next.

### Portrait refresh result — ownership changed, asynchronous art stayed blank

The follow-up capture showed the correct Ailish/Buki label, HP, SP, and companion ordering in both viewports, but both large and small head-profile shapes were empty. The runtime log confirmed all four native gizmos were refreshed on the Player 2 pass. This rejects the ownership/address hypothesis and identifies a resource-timing problem: native assignment RVA `0x0015C0E0` receives a completion flag, and ordinary refresh RVA `0x000AAB00` passes zero. Alternating the shared HUD resource every engine frame tears down the prior assignment before its asynchronous work completes.

The next exact-build prototype hooks only the refresh-owned callsite RVA `0x000AAC08`. While SudekiMP is performing its per-camera portrait refresh, the wrapper changes the existing completion argument from `0` to `1`; all natural game calls retain their original argument. The native assignment routine then drains its own pending list before returning. This preserves Sudeki's resource and reference-count behavior while ensuring each portrait is ready before that camera frame is captured.

## 2026-08-15 — Viewport portraits confirmed; full-width pulse fixed

The synchronous experiment restored the correct Ailish and Buki head profiles, proving that the missing art was a resource-completion problem. It also exposed that broad refresh RVA `0x000AAB00` is not a safe per-render API. Even after isolating its resource assignment, runtime logs repeatedly alternated the two character pointers, released Camera 2 with `reason=party_assignment_changed`, reacquired it with reversed ownership, invalidated both frame caches, and logged `dual_camera_cache_active` again. The visible symptom was a brief native full-width frame: the split collapsed for one frame and the minimap pulsed to its full-screen size.

The final implementation removes both portrait-refresh call hooks. It obtains the chosen character's resource type from the embedded virtual object at character `+0x2C`, maps it through native RVA `0x0003F430` and table RVA `0x002C2A94`, then invokes only the narrow resource selector at RVA `0x0015C070`. A small exact-build adapter supplies its confirmed internal convention—resource index in `ECX`, synchronous flag `1` in `EAX`, and `UIPortraitGizmo+0x2C` as the stack receiver. Relocation-aware byte checks cover the selector, mapping lookup, resource-initialization flag, and first table entries. Natural game portrait callsites remain unchanged.

HUD data and portrait selection now follow the stable Ailish/Buki pointers held by the multiplayer camera layer. The four data hooks resolve those identities to the live party slots only while the viewport HUD binding window is active. The camera poll accepts the exact same two pointers in reversed group order as an internal presentation rotation; a missing or genuinely different party member still tears the camera down safely. This is also the desired multiplayer direction: player/camera ownership should not silently change because Sudeki rearranges display order.

The final live test passed. The user confirmed correct Ailish/Buki profiles on their left/right cameras and no brief unsplit or minimap-size pulse. The final log recorded one Camera 2 acquisition, `player_two_hud_portrait ... policy=direct_synchronous_cycle_icon_resource_assignment`, `player_two_hud_ownership ... policy=stable_character_identity_per_viewport`, one `party_order_rotation phase=tolerated`, and no later `party_assignment_changed` release/reacquire loop. The Quit backdrop also remained functional. The PE32 build and exact-image installation/restoration regression test pass.

## 2026-08-15 — Real-time multiplayer Skill Strike prototype prepared

Static reconstruction of native QuickSkill RVA `0x00027BF0` confirmed the path can be parameterized by character rather than being intrinsically tied to the global controller. `character+0xD4` supplies the private availability context, `character+0xD8` is that character's `CSkill`, `CSkill+0x10` is its owner, `+0x3C` contains six SkillData pointers, and `+0x54` contains their ordered indices. The availability predicate at RVA `0x000DA2A0` receives SkillData in `EAX` and the character context in `ESI`; validation remains RVA `0x000B4BC0`, and execution remains `CSkill::Use` RVA `0x000B4810`. The new adapter reproduces only this selection/call logic and leaves authored availability, enabled state, slot, SP, targeting, animation, damage, and cleanup native.

One proven unsafe shared resource remains: Skill Targeting active byte RVA `0x003C2FCD` and node RVA `0x003C3B44`. The first prototype therefore does not pretend simultaneous targeting is solved. Per-player contexts observe idle/targeting/executing phases and reject a second mod activation while the global target-selection window is occupied or while world speed is not the required shared mode `0/0`, pause `0`. A different player may start once targeting ends even while the first native skill task continues, allowing the overlap experiment without pointer corruption.

Plasmatica's authored camera calls are bracketed at its confirmed script `SetRenderCamera` method. An exact six-byte whole-instruction detour at RVA `0x00036FB0` resolves the named camera but suppresses the global owner switch for an assigned multiplayer caster. Its render state is held only for that caster's alternating cached frame; the other viewport retains its normal render state. Both the authored normal-camera request and native skill cleanup clear the override. Default caster and camera rates remain `1.0`.

`--realtime-skill-coop-test` enables the required normal-speed, native ranged transition, F10 Buki AI override, I/J/K/L movement, U attack, dual-camera cache, Player 2 F1-F4 skills, and caster-camera routing while leaving all repository defaults off. The PE32 build passed. `PlayerCombatContextTest`, `SkillActivationAbiTest`, and the expanded `CallHookTest` passed under Wine. `SkillTraceImageTest` mapped the exact user-supplied executable read-only, installed every hook including the SetRenderCamera detour, then restored the original six bytes and all existing call/pointer hooks. Live gameplay validation is the next step and is not yet recorded as success.

### First live result — incomplete; three global-state conflicts confirmed

The prototype initialized and completed a Plasmatica cast without crashing, but the acceptance test did not pass. Native mouse movement drove both viewport cameras, so the user could not operate Elco and Buki as independent camera/aim owners. No physical game controller was present in Linux's input-device list during this run; connecting one and confirming Wine enumeration is required before implementing the Player 2 controller route.

Buki's viewport showed only Elco's first-person arm and gun while observing his ranged firing mode. This confirms that the caster camera override is isolated but the ranged first-person actor/model visibility switch is still global. It must be virtualized or overridden per render viewport so non-caster views retain Elco's complete world model.

The diagnostic context also recorded `world_time state=reject_mod_skill_input current_mode=2 requested_mode=2 paused=0` immediately after Plasmatica began. It returned to realtime `0/0` after the skill ended. Thus the initial guard prevents a second mod activation while native skill time scaling is active, but it does not yet meet the design requirement that the authoritative simulation remain at normal speed. This run is evidence of the remaining native skill-time transition, not a successful real-time test.

### Controller enumeration checkpoint — Raiju Mobile requires translation

Linux enumerated the connected Razer Raiju Mobile Wired as USB `1532:0705` with joystick/event handlers `js0` and `event21`. Two new standalone PE32 probes reproduce the Windows-side result without launching Sudeki: `SudekiMP.XInputProbe` dynamically loads the same `XINPUT1_2.dll` imported by the game, while `SudekiMP.DirectInputProbe` enumerates attached `DI8DEVCLASS_GAMECTRL` devices. Under the dedicated Wine 11.0 Staging prefix, XInput returned `ERROR_DEVICE_NOT_CONNECTED` for slots 0 through 3 and DirectInput enumerated zero game controllers.

The exact game executable imports `XInputGetCapabilities`, `XInputGetState`, and `XInputSetState` from `XINPUT1_2.dll` in addition to `DirectInput8Create`. Razer's own Raiju Mobile documentation says the device uses XInput on PC only after installing its model-specific Windows driver. That kernel-driver route is not applicable to Wine. A reversible prefix-only trial of Wine's `DisableHidraw=1` and `Enable SDL=1` bus options still produced zero XInput and DirectInput devices; both previously absent registry values were removed afterward, restoring the prefix exactly.

No controller input was routed into SudekiMP and no live controller test was claimed. The next practical input device must either expose standard XInput to Wine directly or be translated to a virtual Xbox controller by a host facility such as Steam Input. Once a probe reports a connected slot, SudekiMP can reserve that slot from native Player 1 handling and route its left stick, right stick, buttons, and triggers to Player 2's combat context.

The initial sandbox-local process check could not see the desktop session and was therefore inconclusive about Steam. A host-level check subsequently confirmed that Steam and its runtime helpers were running. Steam was shut down cleanly at the user's request and host-level process enumeration confirmed all matching processes had exited. Repeating both probes afterward produced the same result: all XInput slots returned `ERROR_DEVICE_NOT_CONNECTED`, and DirectInput enumerated zero controllers while logging the Raiju's unsupported vendor HID usages. Steam ownership was therefore disproved as the cause for this device/prefix combination.

## 2026-08-15 — Native Linux Player 2 input bridge prepared

Rather than installing an unavailable proprietary Windows kernel driver or creating a system-wide virtual Xbox device, the first controller prototype now bridges only SudekiMP. A native Linux helper reads the user-owned joydev node and sends a fixed, versioned state packet over UDP loopback. The Wine DLL binds only `127.0.0.1`, rejects the wrong magic/version/size or a non-loopback sender, receives without blocking Sudeki's update thread, and returns neutral input when no valid packet arrives for 250 ms. Repository defaults remain disabled.

The version-1 packet is 32 bytes and encodes sequence, sender monotonic timestamp, signed 16-bit left/right axes, unsigned 16-bit triggers, and standardized button bits. The helper queries `JSIOCGAXMAP` and `JSIOCGBTNMAP` rather than assuming numeric indices. Against the connected `Razer Raiju Mobile Wired` at `/dev/input/js0`, it found 8 axes and 15 buttons with left stick `0/1`, right stick `2/3`, triggers `5/4`, and D-pad `6/7`. This records transport discovery only; trigger semantics are not yet used by gameplay.

At this checkpoint, enabling the bridge made Buki's existing control-separation hook consume the left stick with a configurable radial deadzone, preserve analog magnitude as native movement speed, transform direction through RVA `0x000291A0`, and submit it to Buki's already-proven arbiter RVA `0x000DAE80`. Controller A supplied the same rising weak-attack input previously proven on keyboard U. The receiver exposed a distinct external-bridge combat-context identity. Right-stick values were change-logged as `captured_not_applied_independent_camera_pending`; they did not move Player 1's global camera or the translated Player 2 view.

`sudekimp-input-bridge --self-test`, the PE32 protocol round-trip test, Wine localhost receiver/timeout test, player combat-context and skill-activation regressions, and the exact supported-executable inert-image hook install/restore test all pass. A separate external-sender run then opened the physical Raiju in the Linux helper and delivered sequence `67` through loopback to the PE32 receiver under Wine, completing the non-game end-to-end path. `--controller-bridge-test` packages the native helper, F10 Buki override, dual cameras, camera-relative analog movement, separation guard, and A weak attack while restoring all generated settings on exit. Live in-game ownership is deliberately not claimed until the user confirms movement and attack.

### First live result — independent physical Player 2 movement confirmed

The castle save loaded with Ailish as Player 1 and Buki in slot 1. The receiver connected at sender sequence `1125`; F10 acquired Buki's native AI override successfully. Keyboard/mouse remained Player 1 while the user independently moved Buki with the Raiju left stick and confirmed that two people could control the two characters simultaneously. Every bridge movement record used Buki character `0x082B1D50` and arbiter `0x082B2620`. The input covered the signed analog range, passed through changing camera-relative world vectors, produced sub-1.0 movement-speed bit patterns for partial deflection, reached 1.0 at full deflection, and emitted native stop events inside the deadzone.

Right-stick changes were logged repeatedly with policy `captured_not_applied_independent_camera_pending`, and the user did not report either camera reacting to them. Two A-button rising edges reached `second_player_weak_attack phase=submit` for the same Buki arbiter. The castle conversation save used for this historical run did not permit visible weak attacks, so the capture proved controller button mapping and native submission—not completed combat animation. That final visible action remained a small arena test. The core bridge milestone was nevertheless complete: Linux controller input and Sudeki's normal keyboard/mouse input controlled different party characters in the same process.

## 2026-08-15 — Party-generic Player 2 control prepared

The control-separation selector no longer searches for Buki's resource type. It now uses the same ownership rule as the split-screen camera layer: the controller-owned front character is Player 1, and the first non-null, non-front active party member is Player 2. F10 applies Sudeki's unchanged refcounted AI override to that exact character pointer. Restoration searches by the retained pointer rather than by party position, so HUD/display reordering cannot silently transfer ownership.

All existing movement, separation, weak-attack, Skill Strike, controller-bridge, and Camera 2 paths already consume the retained overridden-character pointer and therefore become party-generic with this selector change. The focused controller launcher now accepts any two-character party, including Tal/Ailish. Repository defaults remain disabled. The PE32 build, controller protocol/receiver tests, and exact supported-image hook installation/restoration test pass; Tal/Ailish live confirmation is pending.

## 2026-08-15 — Shipped native test arena discovered

Read-only inspection of the user-owned `SOLData.baf` found an internal level-selection document containing `Testroom.zone` and `Testroom_Press.zone`. The same archive identifies level ID `30` as `testroom`, and its resource dictionary contains `SMAP_TestRoom.xml`, `SMAP_TestRoom_Jumps.xml`, `SMAP_TestRoom_Press.xml`, `testroom__OnMain`, `Training_Dummy`, and training-objective text. These names establish that test-room and dummy resources ship with this build; they do not yet establish the room's live contents or the dummy's damage/AI behavior.

The exact executable exports `LoadZone(char const *)` at RVA `0x00007B80`, but decompilation shows that function only locates and requests one already-known zone resource. The safer complete startup path is exported `DoOneLevelTest()` at RVA `0x001051F0`. It reads Sudeki's native `-Level` command-line option, lowercases the selected level, selects the corresponding world record, fires `StartLevelFromStartup`, and performs the remaining world-start initialization. `tools/ghidra/TestArenaReport.java` records this exact-build evidence without changing the Ghidra project or game files.

The first launcher pass correctly applied Windows quoting to the forwarded arguments, but Sudeki remained at its normal front end. `InputArgsReport.java` then confirmed that the game's native tokenizer at RVA `0x00022260` splits only on literal spaces and does not interpret quotation marks. Quoted `"-Level"` therefore could not match the later case-insensitive lookup for `-Level`. The launcher now accepts only non-empty game-argument tokens without spaces or quotes and forwards them unquoted. `tools/continue-research.sh --test-arena` supplies the compatible tokens `-Level testroom` with all optional SudekiMP gameplay hooks disabled. Live test-room entry remains pending after this correction. No game archive, executable, save, or installed asset was modified or copied into the repository.

### First live result — native room entry confirmed; no default player

The corrected launch entered a sparse native room containing a floor and wall rather than the normal front end. No player characters, HUD inventory, or healing items existed, and pressing top-row number keys eventually ended the run. This confirms that the shipped level and basic geometry load without a save, but `-Level testroom` alone does not construct a playable party. Missing inventory is irrelevant to the initial movement/render harness and remains deferred.

`TestArenaPlayerArgsReport.java` traced the character creation path at RVA `0x00006AE0`. A non-empty `-DT` argument gates the entire test-player initializer. Inside that gate, case-insensitive value `1` for `-Tal`, `-Buki`, `-Elco`, or `-Ailish` selects the matching native `PC_*` resource; the same path also exposes several non-party test actors. The next `--test-arena` pass therefore adds `-DT 1 -Tal 1 -Ailish 1`. Live player creation is not claimed until the relaunch is observed.

### Second live result — native Tal/Ailish party confirmed

The user confirmed that `-Level testroom -DT 1 -Tal 1 -Ailish 1` created both
Tal and Ailish in the shipped room. This proves the developer player-selection
flags reach the normal playable-character construction path; inventory and
healing-item setup remain intentionally out of scope for the movement/combat
harness.

### First dynamic-spawn result — invalid ResourceName assumption isolated

The Ailish-first room and F8 overlay loaded, but selecting another playable
character faulted inside the supported executable at RVA `0x000B1370` while
`InternalSpawnPC` dereferenced `ResourceName + 8`. The bad address was
`0x00006C61`, the trailing bytes `"al"` from the fabricated `PC_Tal` inline
buffer. This is direct evidence that the earlier 32-byte inline-string model
was wrong; it is not an `InternalSpawnPC` timing failure.

Disassembly confirms the native object is 12 bytes: encoded kind, identifier,
and a pointer to a reference-backed text record. The engine constructor at RVA
`0x001B9440` consumes `ESI=destination`, `EAX=text`, and `ECX=0x7f`; the
public spawn path then canonicalizes that textual name. The matching native
reference cleanup is RVA `0x001B9760`. The mod now uses those two exact-build
entries, validates their prologues before enabling the cleanroom menu, and
releases the temporary reference after the spawn/remove API returns. The PE32
build and exact-image regression pass; the corrected dynamic call still needs
one live confirmation.

## 2026-08-15 — Ailish-first cleanroom menu prepared

The focused launcher now supplies only `-Ailish 1`, making Ailish the initial
lead. `--cleanroom` is the descriptive launcher name and `--test-arena` remains
an alias. A disabled-by-default `EnableCleanroomMenu` option installs only when
the command line also contains the exact testroom, developer-test, and Ailish
tokens.

Read-only disassembly and the export table established the high-level native
routes used by the menu: `InternalSpawnPC(ResourceName, xyz)` RVA `0x000B1B00`,
`RemovePC(ResourceName)` RVA `0x000B23A0`, `SpawnEntity(name, xyz)` RVA
`0x000B20D0`, `DespawnEntity` RVA `0x000B2300`, `GetPC(text)` RVA `0x00104480`,
and `GetGenericEntity(text)` RVA `0x00104400`. The playable resource names are
`PC_Tal`, `PC_Buki`, `PC_Elco`, and `PC_Ailish`; the shipped generic resource is
`TrainingDummy.sol`. Each resolved export must match its exact supported-build
RVA before the feature installs.

The F8 overlay uses the existing controller-update and pre-`EndScene` seams but
refuses to coexist with the prototypes that currently own those same hooks.
Up/Down selects, Enter toggles, and Escape/F8 closes. Ailish is permanently
lead-locked. Spawn/remove requests remain pending until native lookup confirms
their result; a timeout displays failure instead of repeating an unknown
operation. The dummy is requested six units in front of Ailish's initial room
position. Playable actors use Sudeki's party-aware path so normal formation is
expected, but live following remains to be checked.

Confirmed despawns trigger a 260 ms original two-tone descending triangle-wave
cue generated in memory. Its design is documented in
`assets/audio/cleanroom/`; no audio or other asset from Sudeki is stored in the
repository. The PE32 build, cleanroom ResourceName ABI test, and expanded exact
supported-image signature regression all pass. No live menu/dummy/audio result
is claimed yet.

### Dynamic party and dummy follow-up

The native 12-byte `ResourceName` correction passed live testing. Tal, Buki,
and Elco each spawned through `InternalSpawnPC`, joined Sudeki's ordinary party
formation, despawned through `RemovePC`, and spawned again without a fault. The
training group name `TrainingDummy` did not create the expected entity;
read-only archive inspection distinguished the group wrapper from the actual
monster definition. Passing `MON_TrainingDummy` to `SpawnEntity` (which appends
`.SOL`) created the shipped dummy successfully.

The first placement intentionally added six units to the captured Ailish Z
coordinate. Live comparison showed that this was not the visual center of the
room. The next build instead passes Ailish's exact initial cleanroom anchor as
the dummy position.

### Native cleanroom combat and camera controls prepared

Read-only disassembly established that `CGroupPlayers::InCombat` at RVA
`0x00004FA0` reads the state byte at group offset `+0xD4`. The `CGroupPlayers`
constructor installs a combat-event subobject vtable at group offset `+0x44`;
its full transition at RVA `0x00024480` performs native party arm/unarm, combat
camera/UI changes, cleanup, and finally updates the same state byte. The
cleanroom control calls that transition only after both its exact entry bytes
and the live embedded vtable identity match. It does not patch `+0xD4`.

The separate Camera Mode control resolves exported
`SetFirstPersonCameraMode(bool)` at RVA `0x0002A880` and reads the corresponding
gamepad-controller mode bit at `+0xA0`. The F8 menu now exposes both states and
permits entering or leaving combat and first-/third-person presentation during
cleanroom tests. The corrected RVA was caught by the exact-image regression
before live launch. The PE32 build, unit test, DLL dependency check, and exact
supported-image test pass; focused live behavior is pending.

### Cleanroom infinite resources prepared

The supported executable already contains explicit developer resource
bypasses. Exported `NoSpNeeded()` at RVA `0x000B5320` sets global flag RVA
`0x003C2FCC`; `CSkill::Use` and its validator both check that flag before SP
comparison or subtraction. Exported `NoSspNeeded()` at RVA `0x0000F5B0` sets
global flag RVA `0x003C2F23`; Spirit Strike validation and activation check it
before SSP comparison or deduction. These are engine-authored mechanisms, not
guessed actor-stat writes.

The cleanroom enables both flags by default and exposes reversible F8 menu
toggles. Because the SSP bypass prevents spending but does not itself fill an
already-low display, the enabled state also compares exported `GetSsp()` at RVA
`0x0000F5E0` and refills through exported `SetSsp(float)` at RVA `0x0000F5C0`
only below the confirmed native maximum of `200.0`. Initial flag values are
captured and restored on mod unload. All four exports, their exact RVAs, and
their entry bytes are covered by the supported-image regression. The PE32
build, cleanroom unit test, and exact-image test pass; live resource behavior
is pending.

### Cleanroom inventory, Spirit Strike, stat, and weapon initialization

The cleanroom HUD exposed an initialization distinction: Elco and Buki showed
full numeric HP with empty bars, and Ailish showed full numeric SP with an empty
bar. Read-only disassembly of the HUD confirms that the printed numbers use
character-stat offsets `+0x2C` (current HP) and `+0x34` (current SP), while the
bars divide those values by `+0x30` (maximum HP) and `+0x38` (maximum SP).
Therefore the display can show a plausible current number while an invalid
maximum produces an empty bar.

The guarded repair uses exported `GetCharacterNumberStat()` at RVA
`0x000C1270` and `SetCharacterNumberStat()` at RVA `0x000C1350` with the exact
authored names `HitPoints`, `Maximum HitPoints`, `SkillPoints`, and
`Maximum SkillPoints`. It changes only a non-finite or non-positive maximum
whose current value is finite and positive. Every observed value and repair is
logged as raw float bits so the first live pass can confirm the diagnosis.

Exported `FillInventory()` at RVA `0x000204D0` is Sudeki's own developer
inventory routine. Its implementation walks item IDs through `999`, grants or
maximizes the corresponding native inventory entries, adds currency, and
enables inventory categories. The cleanroom calls it once only after the
inventory global at RVA `0x00408D84` and item database global at RVA
`0x00408D80` are live. This supplies all weapons but also deliberately supplies
the rest of the developer inventory.

Static `SOLData.baf` inspection identifies the first weapon record in each
character family as Tal item `0`, Ailish/Alice item `12` (Royal Sceptre), Elco
item `24` (Mk1 Pistol), and Buki item `36` (Zesiro). Weapon definitions expose
character-specific `Item Type` values but no `Level Requirement`; those fields
occur in ability-advancement records instead. The current evidence is therefore
that weapons are character-family restricted, not character-level restricted.

Elco's cleanroom animation-without-projectile behavior is consistent with a
missing native equipped item: the ranged attack route can reach its animation
fallback while the weapon/missile readiness path refuses to fire.
`CCharacterWeapon::SetWeapon(int)` at RVA `0x000D8790` resolves the requested
item through the native inventory and installs it through the full engine path.
The cleanroom invokes it with the starter ID only when character offset `+0xC0`
contains a live weapon component whose current-item field at component offset
`+0x268` is null. Existing equipment is preserved, and each new actor instance
is initialized only once after both its stats and weapon report ready.

Exported `SpiritStrikeEnable(int)` at RVA `0x000113A0` sets one bit for a
non-negative ID and writes `0xFF` to the manager unlock byte at `+0xAC` for a
negative ID. Cleanroom mode uses `-1` once the manager global at RVA
`0x00408D30` is live, captures the prior byte, and restores it on hook unload.
The PE32 build, cleanroom pre-initialization unit test, exact supported-image
entry regression, and diff check pass. Live confirmation of the bars, Elco's
projectile, all weapon tables, and all Spirit Strike tables is pending.

### Toggleable cleanroom split-screen and Razer readiness prepared

The original cleanroom overlay and the multiplayer prototypes could not be
enabled together safely: the cleanroom owned the character-controller update
vtable slot and frame-end callsite, while control separation and split-screen
needed those same two seams. The loader therefore rejected the combination.
The integration now retains one hook owner for each seam. Control separation
invokes a registered cleanroom update observer after native/controller/P2
processing, and split-screen invokes a registered overlay renderer after its
final camera composition. Standalone cleanroom mode retains its original hooks
for configurations that do not load the multiplayer modules.

The F8 menu adds `Split Screen P2`, initially disabled. Enabling it records a
Player 2 request, asks the native AI-override path to claim the first non-front
active party member, and opens the runtime gate on the existing alternating
dual-camera cache. Disabling it closes the render gate, releases Camera 2, and
requests native AI restoration for the retained character pointer. The request
reconciler runs on the controller update and retries at a bounded 250 ms cadence
when a second actor has not finished spawning.

Control separation now exposes three distinct states rather than conflating
them: requested, independently controlled, and external-input ready. The last
state is true only while the UDP receiver has accepted a current bridge packet
inside its existing 250 ms timeout. A cleanroom-only badge on the right side
reports `P2 JOINING`, `P2 RAZER`, or `P2 READY` from those states. It is drawn
after the two-camera composite, so it belongs to the Player 2 presentation
rather than being duplicated into both cached camera frames.

`--cleanroom` now prepares the proven controller-bridge stack—camera-relative
movement, separation guard, native weak attack, dual per-character cameras,
viewport HUD ownership, and Player 2 right-stick camera—but keeps its runtime
split gate disabled until the F8 entry is selected. The Linux helper starts for
the configured Razer joystick when its device node is readable. Missing input
does not prevent cleanroom entry; it leaves the badge in its waiting state.
Anti-aliasing is temporarily set to zero for the existing compositor and is
restored on exit.

The PE32 build completes without warnings. Bash syntax, DLL dependency, diff,
cleanroom-engine, input-protocol, input-receiver, and exact supported-image hook
installation/restoration checks pass. Live validation of toggle-on, readiness,
movement/camera assignment, toggle-off AI restoration, badge placement, and
spawn/despawn interactions is pending.

### Cleanroom camera-facing and global-presentation findings

Live Ailish/Tal cleanroom testing confirmed that Player 2 movement and Camera 2
are independently controllable. Tal rotated toward Camera 2 only after movement
because native movement committed the new facing direction; orbiting the camera
alone did not. Read-only tracing identified `FUN_005114D0` (RVA `0x001114D0`)
as the native `CPosition` forward-vector/orientation commit used by the actor
direction path. The split-screen module now exposes that exact routine to the
controller layer. When Player 2 is armed (`arbiter+0x50 & 0x2`) and the right
stick is outside its deadzone, it commits Camera 2's horizontal forward vector.
This reuses Sudeki's existing world animation and orientation systems; no new
walking or firing animation is being authored.

A rejected render-window experiment called the native ranged first-/third-
person presentation refresh at RVA `0x001888F0` around Player 2 frames. It made
the ranged body visible but left the model animation static and prevented
Ailish's wand from firing. The experiment demonstrably changes gameplay-owned
presentation state and is not a valid render-only boundary. It remains
disabled by default and is no longer enabled by `--cleanroom` or
`--realtime-skill-coop-test`.

Pure Land then exposed a different global-camera fault. Runtime logs recorded
repeated `second_player_camera phase=release
reason=native_render_camera_changed` events followed by acquisitions based on
temporary native skill cameras. This matches the user's report that Player 1
and Player 2 perspectives briefly mirrored or appeared faded over one another.
The render-only swap now derives the native camera state on every frame, saves
that exact state, substitutes the stable Player 2 state only for a requested
Player 2 render, and restores the saved native state afterward. Camera 2 is no
longer destroyed merely because Player 1 enters an authored cinematic camera.

The exported `QuickMenuIsActive()` at RVA `0x0009C330` is now exact-build
checked and used as a presentation gate. While active, the alternating renderer
forces a Player 1 frame, refreshes the Player 1 cache containing the menu, and
preserves the last clean Player 2 world cache instead of drawing the same menu
twice. This is a viewport-presentation correction only. Native global time
scaling and the observation that Pure Land slows the other viewport remain
open real-time combat tasks.

The PE32 build, cleanroom engine, orbit camera, input bridge protocol/receiver,
and exact supported-image hook installation/restoration tests pass. Live
confirmation is pending for restored Ailish wand fire, stationary armed Tal
facing, Player 1-only Quick Menu presentation, and stable Camera 2 during Pure
Land.

### Pure Land temporal-history contamination confirmed

The next live trace disproved a remaining camera-ownership hypothesis. For
Pure Land manager state `10`, the global native camera stayed at `0x0432FEE8`
with render state `0x04353DA0`. Player 1 frames used that state, while Player 2
frames used the stable independent state `0x05A5B278`; Camera 2 was never
released or reacquired. Controller right-stick input continued to update the
Player 2 camera path.

The corrected ranged component snapshot also showed first-person wrapper
`0x05B7F340` with render-object flags `0x0081E014` (hidden bit `4` set), while
the saved world wrapper `0x05B1A148` was the attached model and its render
object flags `0x0081F011` left it visible. Thus the floating/faded duplicate
presentation is not the first-person arms model and not Tal controlling
Ailish's camera. Pure Land uses shared screen-space or temporal render history,
and the alternating one-camera-per-engine-frame prototype feeds Camera 2's
image into the history consumed by the next Player 1 frame.

A diagnostic policy that would retain Player 2's last clean image while the
Spirit manager at global RVA `0x00408D30` is active was rejected before live
use. Although it would contain cross-camera history bleed, a frozen Player 2
viewport is not valid co-op behavior. Both cameras and both input paths remain
live in the current source. The concrete next requirement is distinct
per-viewport render targets and any temporal/history resources consumed by the
effect, followed by composition of both live results; another camera-pointer
patch cannot solve this class of presentation bug.

### First live-history isolation experiment prepared

Read-only exact-build analysis in `PureLandRenderHistoryReport.java` identified
the first concrete shared temporal resource. `CSceneManager::SetMotionBlur` at
RVA `0x0001BE60` lazily constructs one global
`cD3DMotionBlurPostEffect` through RVA `0x001DDFB0` and stores it at scene
manager `+0x70`. That object owns one render-target/history resource at `+0x10`
and one `cScreenshotPostRenderCallback` at `+0x14`. Its post-render callback at
RVA `0x001DE0B0` blends the saved full-screen image; the screenshot callback at
RVA `0x001DE7B0` then captures the current frame back into the same history.
Their vtable slots are RVAs `0x002DD930` and `0x002DD910` respectively.

This establishes a direct contamination path for the alternating-camera
prototype: a Player 2 frame can be captured into the one global history and
then blended into Player 1's next Pure Land frame. A disabled-by-default,
exact-pointer-gated experiment now intercepts only those two callbacks. While
the Spirit manager is active and the current native render is Player 2, both
the blur composite and screenshot capture are skipped. Player 1 retains the
native effect and history; Player 2 still performs its normal live world render
and continues to accept movement, camera, and combat input. No cached-frame
freeze, simulation pause, camera replacement, or executable-file patch is
used.

This is intentionally a Player-1-caster containment proof, not the final
per-player effect architecture. If live testing succeeds, Player 2 casting and
overlapping cinematic effects will still require effect instances/history
owned by each combat/viewport context.

The first live attempt crashed immediately after the isolation hook logged its
first Player 2 suppression. Wine reported a privileged-instruction fault after
control returned through a corrupted stack. Raw disassembly then corrected an
ABI detail Ghidra had inferred incompletely: native motion-blur callback RVA
`0x001DE0B0` ends with `ret 4`, just like the screenshot callback, despite not
using the stack argument in its body. The initial wrapper declared only the
ECX/`this` argument and emitted plain `ret`, leaking four stack bytes per
suppressed call. The wrapper now accepts and forwards the callback flag and
emits `ret 4`; disassembly confirms the matching epilogue. The PE32 rebuild and
exact-image installation/restoration regression pass.

The corrected live run no longer crashed, but Pure Land stalled permanently on
the first isolated Player 2 frame. The runtime log ends immediately after
`spirit_effect_viewport_isolation`; the Spirit manager remains in state `10`.
Static code explains the semantic deadlock: native screenshot callback RVA
`0x001DE7B0` ends by writing `1` to `callback+0x08`. Motion blur checks that
same byte before consuming the captured target. Suppressing the screenshot
callback therefore suppresses a required completion signal, not merely an
image copy.

This suppression policy is rejected and is no longer enabled by either focused
launcher. The next safe experiment must let the native screenshot callback run
to completion on Player 1's render, then prevent only the blur composite from
being drawn into Player 2—or provide a second callback/target pair. It must not
fake completion until the callback scheduler and target lifetime are fully
understood. The option remains disabled by default as a recorded failed
experiment.

### Completion-preserving Pure Land isolation prepared

The replacement keeps the corrected `ret 4` callback ABI but no longer
suppresses the screenshot callback under any condition. The exact callback
object is resolved through scene manager `+0x70`, motion-blur effect `+0x14`,
and its expected exact-build vtable. If `callback+0x08` is still zero when the
alternating compositor requests Player 2 during an active Spirit presentation,
that single render is assigned to Player 1. The native screenshot callback then
performs both its image operation and required completion write. Normal camera
alternation resumes on the next frame.

The motion-blur composite callback alone is omitted on active Player 2 frames.
The screenshot wrapper forwards unconditionally and logs which viewport owned
the first observed completion, allowing the live run to confirm the scheduling
assumption. This is enabled only by the focused cleanroom and real-time skill
launchers; the source default remains off. The acceptance conditions for the
next run are no crash or Spirit-state stall, a completed Pure Land sequence,
and live Player 2 movement/camera without the caster's temporal composite.

The live test met the safety conditions: Pure Land completed repeatedly without
a crash or state-`10` stall, the first screenshot callback reported
`completion_before=0`, `completion_after=1` on Player 1, and Tal retained direct
movement and Camera 2 input. Two presentation defects remained. Some later Pure
Land shots copied Camera 2 into the Player 1 viewport, showing that the same
screenshot callback continues updating the global history after its one-time
completion signal. Gaze of Wind also replaced both cached views because the
cleanroom launcher reported `skill_camera_routing=disabled`.

The refined pass therefore distinguishes completion from recurring capture.
It always runs the callback while `callback+0x08` is zero, forcing that pending
operation through Player 1 if necessary. On later Player 2 frames, where the
byte is already nonzero, it skips the screenshot image update as well as the
motion-blur composite; no required state transition is omitted. The first
attempt to supply caster identity by enabling the Plasmatica script trace during
cleanroom startup was rejected immediately: its scope is Plasmatica, not Spirit
Strikes, and that launch reached the room without its native Ailish startup
actor. The launcher no longer enables that trace. Instead, while the Spirit
manager is active, `SetRenderCamera` requests are attributed directly to the
known Player 1 character and routed through the existing viewport-owned camera
state. The inactive manager transition clears any retained Spirit camera. The
next live run must confirm Ailish spawns normally, Pure Land no longer imports
Camera 2, and Gaze of Wind no longer takes over both viewports.

The same run also clarified that the apparent global freeze is not the known
world-time mode: every attempted mode `1` write was rejected and the log stayed
at mode `0`, paused `0`. Tal's injected movement continued while ordinary world
actors stopped. Spirit Strike therefore owns an additional cinematic/actor
freeze mechanism that must be traced separately after viewport presentation is
stable.

### Render-only ranged full-body borrow prepared

The Ailish-Player-1/Tal-Player-2 asymmetry is now isolated from camera
ownership. In ranged first-person combat, Ailish's component keeps the active
first-person model wrapper at `+0x160` and a retained world-model wrapper at
`+0x164`; the character position's `+0xB4` slot selects which wrapper is
attached for rendering. Native switch RVA `0x00188A90` calls attachment helper
RVA `0x00111B30`, changes reference counts, copies state, and performs other
gameplay presentation work. Repeating that transition at render cadence was
therefore the cause of the frozen body and blocked wand shots in the rejected
prototype.

The replacement never calls either native transition. It activates only for a
Player 2 render, only while Player 1's arbiter reports ranged first-person
state, and only when the position attachment is exactly the first-person
wrapper and both retained wrappers and render objects are valid writable heap
objects. It temporarily points the attachment slot at the already-retained
world wrapper, sets render-object hidden bit `4` on the first-person wrapper,
clears it on the world wrapper, and restores every exact pointer and flag before
the native frame-end routine. Restoration is ownership-aware: a value changed
by the engine during the render window is not overwritten blindly. The option
remains disabled in repository defaults and is enabled only by the focused
cleanroom launcher. Live acceptance requires Ailish to fire normally for
Player 1 while Tal's viewport sees her complete moving/firing world model.

The first live use of that replacement exposed Ailish's complete body without
breaking the render pass, but the saved world wrapper remained in a T-pose.
This separates visibility from animation ownership: `component+0x164` is a
valid retained renderable model, but ranged first-person mode no longer applies
the current skeletal pose to it.

Read-only decompilation of the exact supported executable first established
that ranged update owner RVA `0x001884C0` calls base model update RVA
`0x000E1930`. At callsite RVA `0x000E1A62`, that base update calls RVA
`0x000E3780`, which advances half-float clock fields at `component+0xFC/+0xFE`
and applies channel-2 blend through virtual method `+0x144` on only the attached
wrapper. Mirroring that blend did execute in the next live test, but Ailish
remained T-posed; the staff alone continued following aim. The blend-only
hypothesis is therefore rejected.

Sudeki's native debug-animation status path supplies the missing boundary. It
reads an attached model's animation selection through virtual `+0x100`, rate
through `+0x108`, time through `+0x110`, channel state through `+0x118`, and
blend through `+0x148`. The corresponding setters are `+0xFC`, `+0x104`,
`+0x10C`, `+0x114`, and `+0x144`; native helper RVAs
`0x001E84F0/0x001E8530/0x001E8580/0x001E85D0` apply those setters to every
submodel. The replacement now copies that complete low-level state from the
active first-person interface's canonical submodel to every submodel of the
saved world interface immediately before Player 2 rendering. It temporarily
clears hidden bit `4` during the copy and restores the exact flag. It does not
advance a clock, rerun the component update, perform a native model transition,
or execute projectile, cost, damage, completion, or animation-event paths.
PE32 compilation and exact-image installation/restoration checks pass; live
confirmation remains required.

### Native ranged-presentation comparison and cleanroom ownership findings

The comparison build logged animation IDs and low-level selectors only when
they changed, before the Player 2 render-only wrapper borrow. With Ailish as
the ranged Player 1, the component remained attached to its first-person
wrapper and used the expected strafe/presentation states: idle strafe `0x05`,
missile-strafe-combo state `0x8E`, and character-specific states `0xC2/0xC3`.
Copying those selectors into the retained world wrapper animated the complete
body, but selected semantically unrelated world clips.

After changing ownership to Tal Player 1/Ailish Player 2, the same Ailish
component remained attached to its native world wrapper. A controller weak
attack then placed `ANIMID_MISSILE_COMBO3` (`0x87`) on channel `4` and selected
world clip `55`. This confirms that Camera 2 is capable of drawing and
animating Ailish correctly. The asymmetry is upstream: Sudeki globally selects
a ranged Player 1's first-person presentation controller, while an observed
or AI ranged character receives the authored third-person controller states.
The remaining fix must translate action semantics into the native world-body
states or preserve a second presentation controller; additional camera or
visibility changes cannot solve it.

The same cleanroom run invalidated the starter-weapon argument assumption.
Static archive IDs `0/12/24/36` identify the first Tal/Ailish/Elco/Buki weapon
records, but exported `CCharacterWeapon::SetWeapon(int)` calls native inventory
lookup RVA `0x00021CE0` with category `5` and treats its argument as the index
within that category. Passing global ID `0` equipped Ailish's weapon on Tal,
and passing global ID `24` equipped Tal's weapon on Elco. `FillInventory()`'s
weapon-category order is Ailish, Elco, Tal, Buki, so the corrected starter
slots are Tal `24`, Buki `36`, Elco `12`, and Ailish `0`. The PE32 build,
cleanroom engine test, and exact supported-image regression pass.

One ownership regression remains open: after two-player role changes, Player 1
status could appear in both bottom-right HUD dials. The accepted HUD layer had
previously worked for a stable Ailish/Buki pair. A new observation-only trace
records, for each viewport and primary/companion source, the desired stable
character and actual resolved party slot. The next reproduction will show
whether the fault is source resolution, party reordering, or cached-frame/HUD
timing before another patch is attempted.

### Viewport-owned ranged first-person presentation prepared

The Tal-Player-1/Ailish-Player-2 comparison sharpened the requirement. Ailish
must not be switched globally between first- and third-person. Her owning
viewport should show the native-style first-person presentation while combat
is active, Tal's observing viewport should continue to see her complete world
body, and both viewports should use the world body outside combat.

The next exact-build prototype therefore reads exported
`CGroupPlayers::InCombat()` at RVA `0x00004FA0` and changes only Camera 2's
render state. On entry it preserves Camera 2's complete third-person matrix,
moves only that render matrix to a provisional `1.55`-unit eye offset over the
Player 2 actor, and applies right-stick yaw/pitch to the in-place first-person
basis. The preserved orbit receives the same input and actor translation, so
leaving combat restores the current third-person view instead of rebuilding it
from Player 1. The eye height is a live-test value, not a confirmed authored
character locator.

Model presentation follows the same ownership boundary. On the Player 2 render
window, a ranged Player 2 temporarily attaches and exposes its retained
first-person wrapper and hides its world wrapper. Any ranged Player 1 is
simultaneously exposed through its world wrapper for the observing viewport.
The previous single-swap state is now a two-entry restoration stack so an
Ailish/Elco pairing can perform both operations in one frame. For Player 2's
arms, the existing low-level animation bridge runs in reverse: it resolves the
active high-level animation resource against the first-person model and copies
selector, state, time, rate, and blend without advancing a clock or replaying
events. All attachment pointers and render flags are restored before native
frame end. It never calls global `SetFirstPersonCameraMode()` or the native
gameplay presentation transition.

The PE32 build, orbit-camera unit test, exact supported-image hook installation
and restoration regression, and whitespace check pass. Live acceptance is
pending: Tal must remain fully rendered in his own view, Ailish must see arms
and weapon from Camera 2 in combat, Tal's view must see her complete animated
body, and leaving combat must return Camera 2 to its preserved third-person
orbit.

### Player 2 first-person borrow rejected; world-animation bank corrected

The first Tal-Player-1/Ailish-Player-2 combat activation crashed the exact
supported executable with access violation `0xC0000005` at VA `0x0061BF17`
(RVA `0x0021BF17`). The faulting instruction was `mov eax,[edx+0x1c]` with
`EDX=0`. Its enclosing model-interface method indexes an internal pointer table
with sub-index `12`; the selected entry was null. The last mod events before
the fault were the `world_to_first_person` animation mirror and raw attachment
of Ailish's two-submodel first-person wrapper in place of the three-submodel
world wrapper.

This confirms that a raw `CPosition+0xB4` pointer/visibility substitution is
not a safe way to make a world-attached actor render through the first-person
wrapper. Sudeki's native switch at RVA `0x00188A90` instead calls attachment
helper RVA `0x00111B30`, updates reference counts, saves/restores attachment
state, and refreshes the model. Replaying that gameplay transition per cached
viewport remains out of scope. The P2 first-person camera/model borrow is now
rejected before render and falls back to the stable third-person Camera 2 and
world wrapper. The separate observer-side Player-1 first-person-to-world borrow
remains enabled for focused cleanroom testing.

The same run reconfirmed the observer body was visible but idle-posed. Static
decompilation shows the high-level animation resource stores two authored
handles at `+0x14` and `+0x20`, and component flag `+0x133 & 2` chooses the
currently active bank. The bridge was incorrectly trying that active
first-person handle first against the opposite world model; some numeric
handles resolved there to valid but semantically unrelated clips. It now
prefers the opposite bank handle and uses the active-bank handle only as a
fallback. This is a narrow animation-presentation correction; live acceptance
still requires Ailish P1 to show a combat/attack pose from Tal or Buki's view.

Confidence: high for the crash boundary and unsafe raw P2 attachment; strong
for the opposite-bank animation correction pending the focused live pose test.

### Tal/Moon Wolf invalidates Player 2 Spirit-history suppression

Build: exact supported GOG executable, SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.

The corrected cleanroom pass first restored the intended baseline: Tal as
Player 1, Ailish as Player 2, split screen and combat mode enabled, with both
actors visible and no activation crash. Ailish still held a static facing while
the Player 2 right stick orbited her camera; that is a separate presentation
issue. Activating Tal's Moon Wolf then stalled the process after the log's
`spirit_history_capture phase=player_two_suppressed` event.

Wine repeatedly reported the main game thread waiting for critical section VA
`0x00804BC0`. A live WineDbg backtrace placed the native instruction pointer at
VA `0x005DFAFB`, immediately after the blocking `EnterCriticalSection` call at
VA `0x005DFAF9` (RVA `0x001DFAF9`). Exact-build Ghidra analysis identifies
function RVA `0x001DFAD0` as a render-job queue consumer: it enters that global
lock, invokes the head job's virtual callback while the lock is held, and then
moves completed jobs between native lists. Companion consumer RVA `0x001DF8C0`
uses the same lock while transferring completed work.

This does not prove which individual native job corrupted or retained the lock,
but the timing and last mod event establish that returning early from the
already-completed Player 2 screenshot/history callback is not lifecycle-safe
for all Spirit Strikes. The effect-isolation option remains default-off and is
removed from the automatic cleanroom and real-time skill launch profiles.
Native Spirit presentation bleed is accepted temporarily rather than risking a
render-queue deadlock. Future isolation must preserve every native callback and
provide per-viewport history at a different ownership boundary.

Confidence: high for the blocked native queue and regression boundary; medium
for the exact callback-lifetime mechanism pending job-object tracing.

The recovery run with automatic temporal isolation disabled succeeded live.
Tal was Player 1, Ailish was Player 2, split screen and combat mode were active,
and Moon Wolf completed instead of stalling. The Spirit manager transitioned
from state `10` back to `0`, while the log retained Player 1 render state
`0x04353DA0` and Player 2 render state `0x05A5B310`. The user nevertheless saw
the same cross-viewport mirroring previously observed during Pure Land.

This confirms the recovery and sharpens the remaining fault: authored cameras
and the two render-state pointers remain separately owned, while the native
Spirit temporal/post-effect history is global and receives alternating camera
frames. At least Pure Land and Moon Wolf reproduce the shared-history defect;
other Spirit Strikes should be treated as exposed to the same architecture, but
their exact visible severity remains untested. The next safe pass must invoke
every native queued callback and swap or redirect its history/render resource
per viewport. Returning early from either callback is rejected.

Confidence: high.

### Native per-viewport Spirit history swap prototype prepared

Build: exact supported GOG executable, SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.

The callback-suppression experiment is replaced by a lifecycle-preserving
prototype. Ghidra tracing of the motion-blur constructor and callbacks found
that the shared post-effect stores a native history subobject at `+0x10`, while
the screenshot callback stores the same kind of subobject at `+0x04`. The
native resource factory at RVA `0x001F6C70` accepts Sudeki's `_RenderTarget`
name, the current render dimensions, format `0x15`, and the native zero flag.

When the Spirit presentation is active and the Player 2 viewport is being
rendered, the hook now creates one additional target through that factory and
temporarily substitutes only the relevant history slot around the complete
original callback. It restores the slot immediately after the callback. It
does not change the callback ABI, skip queued work, alter the completion byte,
or disable Player 2 rendering/input. The native wrapper is retained until
process exit because the exact destructor ABI and cache ownership are not yet
confirmed; this is intentionally limited to a focused test process.

The exact-build PE32 DLL builds cleanly, `git diff --check` and shell syntax
checks pass, and the inert-image regression executable completes under Wine.
No live claim is made yet. The next acceptance test is Tal/Ailish split
screen in the cleanroom: cast Moon Wolf, then Pure Land if available, and
report whether both callbacks complete without a stall, mirrored viewport, or
lost Player 2 movement/input. If the factory returns no target, the hook logs
`spirit_history_resource phase=create_failed` and leaves the native path
unchanged.

Confidence: strong for the callback ownership and factory call shape; pending
live validation of the native target's actual temporal isolation and
process-lifetime ownership.

### Spirit startup-camera gate containment prepared

The first native-history pass completed Moon Wolf/Pure Land without a stall,
but live observation still showed Iron Warrior's startup camera briefly appear
in Player 2. The runtime log showed Sudeki's transient UI gate becoming active
around Spirit startup. The split renderer previously treated every
`QuickMenuIsActive()` result as a Player-1-only frame, which is correct for the
actual Quick Menu but can suppress the Player 2 render during a Spirit camera
transition and leave its alternating cache showing the caster's startup frame.

The new exact-build change distinguishes those states: while the Spirit
manager is active, both viewport renders remain live even if the transient
native UI flag is set; ordinary Quick Menu and Quit-menu behavior remains
unchanged. `SetRenderCamera` routing still records Blade Dance's authored
`InitCam` and `SkillCam` requests against the caster's viewport and restores
the normal camera on completion. This build is ready for a focused Iron
Warrior/Blade Dance acceptance pass; no new live camera claim is made yet.

Confidence: medium for the gate diagnosis pending the focused visual test.

### Ailish Player 2 ranged pose remains unresolved; spawned-actor refresh is separate

Build: exact supported GOG executable, SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.

The focused cleanroom log that prompted the previous hypothesis was
misidentified. The user is reporting Ailish as Player 2: her weapon is already
equipped, but her visible body remains in an idle/static presentation during
combat. The log identifies the relevant boundary as
`attached=world first_person_active=0`; when combat input is submitted, the
world animation selectors and active channel state do change. This makes the
weapon-initialization path unrelated to the observed Ailish pose.

The raw Player 2 first-person-wrapper attachment remains rejected after the
exact-build renderer crash at RVA `0x0021BF17`. The current fallback therefore
keeps Camera 2 third-person and the world wrapper. That fallback is stable but
does not yet reproduce the first-person combat presentation that Ailish has in
the native Player 1 viewport.

A separate exploratory log did show Elco being spawned after Combat Mode was
already active, with no later native transition after Player 2 assignment. The
cleanroom now has an exact-gated, disabled-outside-cleanroom refresh diagnostic
for that independent case. It is not a fix for Ailish and has no live
acceptance claim.

The independent spawned-actor diagnostic invokes the exact native transition
at RVA `0x00024480` once, with `enabled=true`, only when a newly spawned actor
finishes native stats/weapon initialization while
`CGroupPlayers::InCombat()` already reports true. It does not toggle combat
off/on, write the state byte, or touch existing actors. The call remains behind
the exact export, entry signature, and combat-event sink-vtable checks already
used by the cleanroom. Its log reason is
`new_actor_initialized_during_combat`.

Automated validation: PE32 build, `SudekiMP.CleanroomEngineTest.exe`,
`SudekiMP.SkillTraceImageTest.exe` against the working exact image, shell
syntax, and `git diff --check` all pass. Live visual acceptance of the
spawned-actor diagnostic is still pending and must remain separate from the
Ailish P2 pose investigation. If it replays an unwanted global camera/UI
transition, reject it and trace the actor-specific arm helper instead.

Confidence: strong for the separate spawned-actor observation; pending its
focused live test. Confidence in the Ailish P2 pose cause is medium: the
world-wrapper state is confirmed to change, but the safe per-viewport combat
presentation path is still unresolved.

### Control-owner handoff now requests a native combat-arm refresh

Build: exact supported GOG executable, SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.

The read-only ranged trace showed that an actor could briefly expose armed
world selectors immediately before `AiOverrideControl`, then fall back to the
neutral world selector after Player 2 ownership was installed. The cleanroom
control-separation path now calls the same exact native group combat transition
at RVA `0x00024480` once after a verified AI-to-player handoff, with
`enabled=true` and `force=true`. It does not write animation selectors, weapon
state, camera state, or the nested AI mode byte. The call is available only
after the cleanroom engine's existing export, entry-signature, and event-sink
checks succeed; normal non-cleanroom control separation safely skips it.

The first rebuilt cleanroom run logged:

```text
control_separation event=override result=success ...
cleanroom_engine event=combat_mode phase=refresh state=enabled ...
control_separation event=combat_arm_refresh status=confirmed ...
```

That run had Ailish as Player 1 and Tal as Player 2, so it is an ABI/path
verification only. It is not evidence that Ailish's Player 2 ranged pose is
fixed. The required acceptance pass remains Tal Player 1 with Ailish Player 2,
then a visible wand action while comparing the world-wrapper trace and the
second viewport. The new low-cadence clock samples and one-time capture-failure
classifier remain read-only diagnostics.

Confidence: high that the guarded native refresh is installed and confirmed;
pending the correct Ailish-Player-2 visual test.

### Cleanroom ranged-readiness prime prepared

Build: exact supported GOG executable, SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.

The current cleanroom run used the native Ailish lead with Tal assigned to
Player 2. Combat Mode enabled movement and the split-screen handoff, but
Ailish could not perform a normal ranged attack until one Skill Strike had
been activated. After that first skill, normal fire worked. This confirms
that the group combat transition alone does not complete the ranged actor's
readiness transition; the first skill was acting as the initializer.

The next prototype reuses Sudeki's confirmed native UI/control transition at
RVA `0x0000AFD0`, with the UI manager supplied in ESI, and holds it for the
same 75 ms game-thread timer already validated by the ranged QuickSkill
experiment. Combat entry and a verified Player 2 control-owner handoff now
request this cycle; the callback exits through the unchanged native false
transition. No animation selector, weapon pointer, arbiter flag, camera
state, or simulation-time value is written. The entry bytes and UI-manager
global are exact-build gated, and pending timers are cancelled on combat-off
and unload.

Automated validation: PE32 build, `SudekiMP.CleanroomEngineTest.exe`,
`SudekiMP.SkillTraceImageTest.exe` against the working exact image, shell
syntax, and `git diff --check` all pass. Live acceptance is pending: repeat
the test with Tal as Player 1 and Ailish as Player 2, then verify that Ailish
can fire before any Skill Strike and that her observer-side body pose remains
the separate ranged-presentation question.

Confidence: strong for the observed missing initialization boundary and the
native-only implementation; pending live confirmation of the new cycle.

### Ranged action-to-world translation trace prepared

Build: exact supported GOG executable, SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.

The latest cleanroom observation remains: after pressing `U`, Ailish's body
is animated rather than completely frozen, but it stays in an idle-like pose;
switching to her electric weapon still runs a multi-step fallback-looking
animation cycle. The readiness prime is therefore not the missing pose fix.

The focused diagnostic now records the native presentation mapping at the
exact boundary where the current prototype copies first-person state to the
world wrapper. For every changed animation channel it logs the component,
animation ID, both authored resource handles (`+0x14` and `+0x20`), the
target/source world lookup results, the original first-person selector, and
the chosen world selector. It is gated by the existing exact-build ranged
model isolation path and makes no native resource, animation-ID, or authored
table writes.

This pass has one question and one acceptance test: capture Ailish entering
combat, firing the electric weapon through its full cycle, and returning to
idle; then determine whether the chosen world selector is an opposite-bank
world clip or an unresolved first-person fallback. Do not install a fixed
selector map until that correlation is recorded. Automated PE32 build,
`SudekiMP.SkillTraceImageTest.exe` against the exact working image, shell
syntax, and `git diff --check` pass. Live capture is pending.

Confidence: high that the readiness prime is unrelated to the remaining pose;
pending for the resource-to-world mapping cause.

### Live capture confirms first-person-only fallback selectors

The rebuilt exact-image cleanroom capture completed the requested idle →
electric weapon cycle → idle sequence. The translation trace recorded
first-person ranged IDs `0x8C`, `0x8E`, `0xC1`, `0xC2`, and `0xC3`. For each,
the second authored resource handle was the sentinel `0x0007FFFF`; the other
handle could not be resolved by the saved world model interface
(`source_lookup=-1`). Consequently the mirror copied first-person selectors
`2`, `4`, `8`, `9`, and `10` directly into the world wrapper. The world body
does advance, which matches the user's “animated but idle” observation, but
those selectors are not authored third-person combat clips.

This confirms the cause of both symptoms: Ailish is not failing to enter the
ranged-ready state, and the electric weapon is not independently corrupting
the model. The observer path has no world resource for those first-person
action IDs and is using an unsafe selector fallback. No mapping was guessed
or written in this pass. The required next capture is Tal as Player 1 with
Ailish as Player 2, using the same weapon actions, to record the native
third-person IDs/selectors for a semantic correlation.

Confidence: high for the fallback diagnosis; pending the native world-side
action correlation.

### Tal-P1/Ailish-P2 control case confirms native world animation

The requested control run used Tal as Player 1 and Ailish as Player 2. The
user pressed `U` after the setup. The log shows Ailish attached to the world
wrapper with `first_person_active=0`; her native weak attack selected
`animation_id=0x87` (`ANIMID_MISSILE_COMBO3`) and `world_selector=55`, then
returned to idle. No first-person fallback occurred.

This separates the two cases: Ailish-P2 already has the correct native
third-person combat path, while Ailish-P1 observer rendering incorrectly
copies first-person selectors. The `0x87/55` pair is a confirmed correlation
for the weak attack, not yet a universal mapping for the electric weapon's
`0xC1/0xC2/0xC3` sequence.

Confidence: high for the role-dependent asymmetry; electric-action mapping
remains open.

### Live follow-up: Ailish reload misalignment is presentation-state-only

The paired Ailish-Player-1 trace keeps one weapon object, one weapon item,
one first-person wrapper, one world wrapper, and the same first-person/world
render objects from idle through the electric weapon reload sequence. No
weapon selection, item attachment, wrapper pointer, or render-object pointer
changes when the visible weapon temporarily separates from the arms. This
rules out an equipment swap or attachment-slot replacement as the immediate
cause.

During the same cycle, the observer bridge maps the ordinary ranged action
`0x8E` to the confirmed world selector `55`, but the reload-stage IDs `0xC2`
and `0xC3` remain raw first-person selectors `9` and `10`. Both viewports can
therefore show the same bad arm/weapon relationship even though the weapon
identity and attachment objects remain stable: the shared character
presentation state is still being driven by first-person reload semantics.

This is high-confidence evidence for a role-dependent animation/presentation
translation failure, not a camera-only or weapon-ownership failure. Do not
guess selectors for `0xC2`/`0xC3`; the next controlled capture is the same
electric cycle with Tal as Player 1 and Ailish as Player 2, recording the
native third-person IDs/selectors before installing any semantic bridge.

### Live follow-up: Ailish weapon/arm alignment remains role-dependent

Build: exact supported GOG executable, SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.

The user reports that the current cleanroom build is visibly closer: when
Tal is Player 1 and Ailish is Player 2, Ailish's combat animation and weapon
placement appear aligned. When Ailish is Player 1, her weapon does not line up
with her arms. This is a new presentation finding, not evidence that the
native weapon-selection or projectile path is wrong.

The existing hook only borrows animation selectors, state, time, rate, and
blend into the observer-side world wrapper, and temporarily changes the
character position's attached wrapper at `+0xB4`. It does not copy a weapon
bone/attachment transform. The role asymmetry therefore makes an attachment
or per-viewport model-transform boundary a strong hypothesis, but the
affected viewport (Ailish's own first-person view versus the observing world
view) still needs to be identified explicitly before changing an offset.

Next focused acceptance test: repeat the same weapon action with Ailish as
Player 1 and Tal as Player 2, capture both viewports and the ranged wrapper
trace, then compare the first-person/world wrapper attachment and weapon
transform state against the Tal-Player-1/Ailish-Player-2 control case. Do not
install a guessed weapon offset or animation map from this observation alone.

Confidence: high that the role-dependent visual difference is real; medium on
the attachment-transform hypothesis pending the paired capture.

### Live control capture: Player 2 controller attack stays native world-side

With Tal as Player 1 and Ailish as Player 2, the Linux controller bridge's
`A` button submitted its intentionally supported weak-attack path. Ailish
remained attached to the native world wrapper (`first_person_active=0`) and
selected `animation_id=0x87` with `world_selector=55`; the sequence returned
to idle without a first-person fallback or wrapper change. This matches the
user's visual result: the Player 2 combat animation and weapon presentation
look correct.

At this capture, the bridge mapped `A` only to weak attack, so it did not
reproduce the first-person-only electric reload IDs `0xC2`/`0xC3`. It remains
a valid historical native world-side control reference, not proof of the
reload-stage mapping. The candidate semantic target for those observer-side
stages remains unconfirmed until the electric action can be triggered through
a native AI or weapon-specific input path.

Confidence: high for the native Player 2 control path; pending for the
electric reload-stage correspondence.

### Read-only companion capture prepared for the electric reload comparison

The existing ranged trace was tied to the split-camera ownership pointers,
so releasing Player 2 back to native AI also stopped the diagnostic from
seeing that character. The trace now falls back to the active party's first
non-front character through the same validated party/controller resolver when
the split camera is not owned. It only reads the character, component,
wrapper, weapon, and animation-interface state; it does not change AI,
attachment, rendering, input, or combat state.

The PE32 build and exact supported-image `SudekiMP.SkillTraceImageTest.exe`
regression pass. The next live capture can therefore let native AI perform
Ailish's electric action and record the world-side animation states directly,
without swapping her to Player 1 or adding a guessed `0xC2`/`0xC3` mapping.

### Live cleanroom control capture: F10 targets Ailish, not the spawned Tal

Build: exact supported GOG executable, SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.

The cleanroom log identifies the lead Ailish entity as `0x05B0FE10` and the
spawned Tal entity as `0x073D2FE0`. The current F10 path then records
`control_separation event=override slot=1 character=0x05b0fe10`, followed by
the same Ailish pointer on the control refresh and weak-attack submissions.
Therefore F10 is currently taking the Ailish entity for Player 2 control; it
is not enabling native AI for Tal. The user-visible behavior is consistent:
F10 supplies a second control path without split-screen, while the spawned Tal
is not the entity selected by this resolver.

During the same run, the Ailish world-side trace recorded authored ranged
clips `0x85`/selector `59` and `0x87`/selector `55`. The latter is the already
confirmed `ANIMID_MISSILE_COMBO3` weak-attack pair; the former is the mapped
`ANIMID_MISSILE_COMBO1` world clip. The two weapon observations therefore
confirm that Ailish's native world presentation can fire from the controlled
path. They do not constitute an AI attack test or prove the electric reload
mapping.

Confidence: high for the F10 entity-selection bug and the observed world-side
clips. Next fix/experiment must separate `P2 control override` from `AI
enable`, and resolve the spawned Tal entity explicitly before testing native
AI behavior.

### Live acceptance: selector fallback reduces pose corruption; weapon transform remains detached

The rebuilt observer-only fallback was exercised with Ailish as Player 1 and
the second viewport active. The user reports that the formerly frequent
jumbled animation sequences are substantially reduced, but the weapon still
floats to Ailish's right while her hands hold the expected pose.

The trace confirms the selector-side result: first-person reload IDs `0xC2`
and `0xC3` now emit `ranged_world_animation_fallback` and preserve the last
native world channel instead of copying selectors `9` and `10`. The character,
weapon, weapon item, first-person wrapper, world wrapper, and both render-object
pointers remain stable through the transition. Therefore this residual defect
is not an equipment swap, missing weapon object, or wrapper identity problem.

The current bridge copies animation selector/state/time/rate/blend and the
character render attachment only; it does not copy a weapon bone or model-local
attachment transform. The next pass is a read-only paired transform trace
against the native Tal-Player-1/Ailish-Player-2 control case. No weapon offset
or matrix will be guessed until that trace identifies the responsible native
attachment boundary.

Confidence: high for the reduced selector corruption and stable weapon
identity; strong for a missing observer-side weapon transform, pending the
paired transform trace.

### Observer vertical-aim seam triage rejects root, locators, and renderer slot 3/channel 4

Build: exact supported GOG executable, SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.

**Rejected — whole-root pitch.** Applying the Player 1 camera pitch to
Ailish's observer-side world root did make the complete model tilt vertically,
but it pivoted from the feet/bottom of the actor. The user accurately described
the result as a "seahorse toy": legs, torso, arms, weapon, and head moved as one
rigid object instead of the waist/upper body articulating around the aim. That
write is disabled and must not be reused as an upper-body solution.

**Confirmed — named locators are not the needed skeletal seam.** A read-only
13-sample neutral/up/down inventory queried the exact provider used by the
observer world render object. Only three requested names resolved:

- `WeaponFollow`, index `5`;
- `Staff1`, index `7`; and
- `WeaponParent`, index `6`.

Their matrices were invariant at every sampled pitch. `WeaponFollow` remained
identity at translation `(0,0,0)`, `Staff1` remained at
`(-0.46546,0.92584,0.35868)`, and `WeaponParent` remained at
`(0,1.48098,-0.01219)`. Requested waist, backbone, shoulder, clavicle, upper-
arm, lower-arm, and wrist names all returned `name_not_present`. This exact
named-locator interface exposes weapon/attachment anchors; it does not expose
the live skeleton articulation required for vertical aim.

**Confirmed — renderer configuration slot 3 and logical channel 4 do not
encode pitch.** A second read-only audit captured 15 complete first-person and
saved-world renderer sequences across camera pitch `-0.52360` through
`+0.26599`, including the accompanying normal-fire test. Both exact
`cAnimObjectRenderer` instances retained logical-channel-4 assignment `2` and
slot-3 source pair `0x8002,4`, mask `0x2`, blend `0.0`, and extra value `0`.
Channel 4's selector/state/rate/blend were likewise invariant with pitch. The
ordinary firing trace instead placed the active first-person work on channels
`0` and `2`. Slot 3/channel 4 is therefore an authored action-layer setup, not
the continuous vertical-aim parameter. No animation, pose, locator, or model
transform was written during either audit.

**Confirmed static limit — `Upper body` is metadata, not yet an actionable
runtime seam.** The authored `StateDetails+0x59` bit `0x08` is loaded from the
`Upper body` property by the state-details loader at RVA `0x00144B50` (property
callsite VA `0x00544BA5`). Exhaustive exact-image cross-reference and
disassembly review found the loader/writer but no confirmed runtime consumer
of that bit. It remains useful format evidence, but treating it as a live mask
or pose switch would be speculation.

**Open — next bounded experiment.** Perform one read-only runtime inventory of
the already-loaded resources for `0x97` `ANIMID_MISSILE_AIM_CIRCLE`, `0x98`
`ANIMID_MISSILE_AIM_STRAIGHT`, and `0x99` `ANIMID_MISSILE_AIM_STRAFE`. Resolve
their existing handles independently against Ailish's first-person and saved-
world wrappers. Do not call the state machine, play a selector, mutate authored
metadata, or write animation/pose state. Accept this route only if the inventory
proves that one of those authored resources supplies a distinct world-side
upper-body aim representation.

Confidence: high for both rejected runtime seams and for the absence of a
confirmed `Upper body` bit consumer; open for the `0x97..0x99` resource role.
### 2026-08-16 — Combat input/reload watcher remains Player 1-owned

The split-screen camera/facing pass improved Tal's observer presentation: the
second viewport now keeps a stable third-person combat framing and stationary
right-stick rotation no longer decays back to the last movement direction.

The remaining combat-HUD issue is separate from the already-correct party
portrait/HP/SP ownership seam.  The native reload/combo/input watcher (the
overlay that reports reload state, combo inputs, or a failed combo) still
resolves its source from Sudeki's single global active-controller/Player 1
state.  As a result, the widget can show Player 1/Ailish information while
the other viewport is owned by Tal, instead of following the character whose
camera is being rendered.

The shipped resource names support this identification but not yet the native
ownership boundary: `sui_combo_gizmo`, `sui_attackbar_gizmo`,
`sui_context_attack`, `sui_combo_1_miss`, and `sui_combo_2_miss` are present in
`SOLData.baf`.  They are presentation resources, not evidence that the
combat/animation compositor should be changed.

This is recorded as an open viewport-owned combat-HUD task.  The next trace
must identify the widget's exact source object and update boundary before any
pointer substitution is attempted; the existing party-slot pointer hooks must
remain unchanged.  Acceptance is one independent reload/combo/input state per
viewport, with no cross-viewport prompts, costs, or failure/result text.
### 2026-08-16 — Quick Menu is one global queued UI payload

The live split-screen trace captured the native Quick Menu render-submit path
at RVA `0x0009BBA0` during the Player 2 render phase. The menu object's
`+0x214` field is a UI/loadout state pointer, not a direct character pointer;
the native controller target at the same moment was the Player 1 Ailish
object. The menu submit is queued after the active viewport's scene flush and
consumed by the next viewport's flush. This explains the observed duplicated
menu and phase-split text: the current render hook isolates world/UI cadence,
but does not clone the Quick Menu's owner, skill list, weapon list, or Spirit
Strike state.

The safe next seam is to trace the `+0x214` state object's character/resource
references and the menu-open input owner before any pointer virtualization.
Do not write the global controller target or the Quick Menu state during a
render pass.

The Player 2 facing path now has a 0.5-degree hysteresis guard before issuing
the native position-orientation commit. This avoids repeatedly reapplying the
same orientation while stationary, which was the leading explanation for the
reported Tal micro-shiver. Build and exact-image regression passed.

The first widened owner inventory found no direct Ailish/Tal pointer in the
Quick Menu object, its `+0x214` owner object, the owner's `+0x3c0` nested state,
the `+0x208` resource object, or the `+0x218` auxiliary object. This strengthens
the conclusion that native Quick Menu content is a single global payload rather
than a hidden per-character menu instance. A bounded two-level pointer-graph
probe is installed for a later submit event, but no pointer substitution or
controller-target write has been attempted.

### 2026-08-16 — Owner-scoped Quick Menu submit prototype

The first safe implementation of per-player menu presentation is now in place.
At the ordinary-menu rising edge, SudekiMP latches the already-current native
controller target as the menu owner, primes the opposite viewport, and suppresses
only the owner's outgoing render-submit. Because native submission is queued for
the next viewport, this keeps the payload on its owner's camera without writing
the global controller target, loadout resource, or menu object. Early frames now
reuse the validated active-party resolution when cached viewport character
pointers are not ready. Exact-image regression and build passed.

This is presentation ownership only. The native payload remains global, so
independent skill/weapon/Spirit lists still require a future cloned loadout/UI
state layer once a safe native state boundary is identified.

### 2026-08-17 — Witches' Kiss camera trigger timeline

An observation-only camera trace was added around the exact native
`CameraManager::SetRenderCamera` hook and the split-screen render-state slot.
It records each requested camera name and caller, the native `IsUsingSkill`
owner flags, the routing decision, and the global/scene/Player 1/Player 2
camera matrices on every rendered skill frame. It does not write camera or
gameplay state.

Two consecutive Ailish Witches' Kiss casts produced the same native sequence:

- The Ailish skill flag became active at trace time `0 ms`.
- Native requested `InitCam` at `14–26 ms`. SudekiMP inferred Ailish as the
  only character using a skill and routed its render state to Player 1 only.
- Native requested `SkillCam` at `3268 ms`, again routed to Player 1 only.
- Native requested `default` at `9564 ms`; this restored Player 1's ordinary
  viewport camera. The skill flag cleared at about `9696 ms`.

All six calls returned through RVA `0x0023525F`. Static disassembly identifies
that address as the return site in a generic native indirect-call/event thunk,
not yet the upstream skill-state function. Camera names and timings are
confirmed; the higher logical caller remains open if a deeper stack trace is
needed.

Each cast contained 727 camera samples. Player 1 used three render states
(`default`, `InitCam`, and `SkillCam`). Player 2 used exactly one render state
for all 727 samples, and every one of its 356 rendered frames selected that
state without a scene-slot mismatch. The global native camera also remained
`default` throughout. This confirms that the current caster router does not
assign Witches' Kiss's authored cameras to Tal's viewport. If a duplicate is
still visible with this build, its remaining boundary is the cached-frame or
post-processing presentation path rather than native camera selection.

One deterministic handoff gap remains. After `InitCam` was routed, 14 Player 1
render-start samples (about `187–199 ms`) still displayed the ordinary native
camera before the scene slot began presenting `InitCam`. During this interval
the authored `InitCam` matrix was already rotating off-screen. This coincides
with the transient Quick Menu active state after the ordinary menu payload has
closed. Player 2 had zero camera-state mismatches. Treat this as a menu-to-skill
presentation-cadence issue, separate from caster ownership.

Confidence: high. Both casts repeated the same requests, timings, state
ownership, and scene-slot results on the supported executable SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.

### 2026-08-17 — Spirit non-caster movement boundary

Pure Land exposed a second global single-player assumption after its cameras
were isolated: Tal could rotate but the Spirit presentation state prevented his
normal movement submission from producing useful displacement. The confirmed
lock appears as arbiter bit `0x00080000`. SudekiMP now virtualizes only that bit
around Player 2's existing movement submission and, while the exact non-caster
Spirit predicate remains true, submits collision-aware absolute delta through
native `CMovementController::SetAbsoluteDeltaMovement` RVA `0x000030A0`.
Ordinary movement and the caster remain untouched.

Live traces separated motion from presentation. Tal's character position,
render-model root, and Camera 2 target all moved together, proving that the
fallback does not merely displace the camera or leave the visible model behind.
The initial one-unit scale looked extremely slow; a normal cleanroom control
trace measured roughly `6.4` world units per second at full input. Applying that
pace produced movement the user judged normal during Pure Land.

The remaining defect is animation ownership. During Spirit, Tal continued to
use idle selector `17` on channel 0 at rate `12`, with channel 1 dormant, even
while his transform moved. The corresponding normal full-speed combat trace
used selector `36` on channel 0 at approximately `37.17` and selector `32` on
channel 1 at approximately `30.98`. A narrowly gated Tal presentation
compositor now applies only that observed pair during the non-caster Spirit
window and never calls the high-level animation controller or manually ticks
the renderer. Its initial resource-type gate incorrectly used `0x00`; the live
party mapping proves Tal is `0x23`, Ailish `0x01`, Buki `0x05`, and Elco `0x0E`,
so the Tal gate has been corrected. Visible running remains a pending live
acceptance item; the confirmed result at this checkpoint is normal-speed,
collision-aware non-caster displacement.

The PE32 build and exact supported-image install/restore regression pass. This
prototype is intentionally limited to Tal until equivalent native locomotion
selectors are measured for the other characters.
### 2026-08-17 — Co-op role lobby and immutable session ownership

The split-screen resolver previously selected the first active non-lead party
member on every ownership poll. That was useful for experiments but allowed a
native character switch or party-order change to invalidate the camera, HUD,
input, and animation bindings independently.

The integrated cleanroom now opens a role-lobby overlay when the testroom is
ready. Ailish remains the native Player 1 lead for this first safe pass. The
user can spawn/select exactly one of Tal, Buki, or Elco as the Player 2
candidate, then activate `CO-OP READY`. The mod resolves both native entity
pointers, enables Player 2 input and split rendering, and records an immutable
role tuple for the session. The resolver accepts only that tuple and the
control/split APIs reject attempts to disable Player 2 or remove actors after
the lock. Multiple active P2 candidates are rejected rather than guessed.

This is a session lock, not a title-screen character-select implementation.
The testroom is already loaded underneath the overlay, so gameplay is not yet
paused by a native lobby state. Arbitrary Player 1 selection, controller join
readiness, and safe transition back to a new lobby remain open. These are
deliberately deferred until a native pre-game/party-commit boundary is found.

Evidence: supported executable SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
Build, `SudekiMP.CleanroomEngineTest.exe`, and
`SudekiMP.SkillTraceImageTest.exe` pass. No game binary or asset was modified.

### 2026-08-17 — Title-screen roster selection prototype

The role-lobby direction has been moved to the pre-game boundary. With
`EnableCoopRosterMenu=true`, SudekiMP installs a title roster overlay rather
than opening the cleanroom developer menu. Up/Down selects the P1 or P2 row,
Left/Right chooses Tal, Buki, Elco, or Ailish, and Enter rejects duplicate
choices and locks the pair for the process session. The selected resource types
are retained until DLL uninstall; no game asset or save file is modified.
Enabling this mode also provisions the already-tested split-camera/control
stack, but leaves it runtime-disabled until the roster is actually applied.

Once the native party group exists, the controller-update hook applies the
selection on Sudeki's own game thread. It pulses the native Next/Previous
character action fields (`controller+0xF4`/`+0xFC`) until the selected P1 is the
front party entry, then waits for the selected P2 to become available in the
story. Only after both identities are present does SudekiMP take the immutable
role lock used by split-screen/HUD/input ownership. A character who has not
joined the story yet therefore remains unavailable rather than being spawned or
silently substituted.

This is the first title-selection foothold, not a complete title UI replacement:
the overlay is intentionally a small SudekiMP texture and role selection is
process-session state. Save-file persistence, controller-ready prompts, and
story-specific “character unavailable” messaging remain future UI work. The
native party rotation is exact-build gated and fails closed if the party or
controller action fields are not valid.

### 2026-08-17 — Native title/front-end menu trace

Before attempting to make the roster look native, the exact GOG executable was
traced read-only with `tools/ghidra/TitleMenuReport.java`. The report confirms
that Sudeki's title screen and front-end settings are driven by a native menu
state machine rather than by a single composited image.

`FUN_004A1950` (RVA `0x000A1950`) rebuilds the menu entries. It registers
localized keys (`NewGame`, `Options`, `Credits`, `QuitGame`, and the save-aware
`Continue` path) through `FUN_005B9FC0`, binds the corresponding action names
through `FUN_004049C0`, and stores the active item count at controller
`+0x17D8` and the selected index at `+0x17D4`. Its callers are in the native
front-end setup/update routines at `FUN_004A0F40`, `FUN_004A0060`, and
`FUN_004A0360`.

`FUN_004A0360` is the action boundary. It compares the selected action string:
`Options` enters front-end state `6` with pending operation `+0x4C=1`, while
`Continue` enters the same state with `+0x4C=2`. `ShowFrontEndCredits` enters
state `10` and sets the credits flag; `QuitGame` calls `PostQuitMessage(0)`.
The executable's UTF-16 labels are at VA `0x006CB140` (`Options`),
`0x006CB164` (`Continue`), `0x006CB178` (`Credits`), and `0x006CB188`
(`QuitGame`).

This establishes the next safe direction: trace the native state-6 options
builder and its input/selection callback before attempting a native roster
replacement. No game memory or asset was changed by this pass, and the current
SudekiMP roster overlay remains a reversible diagnostic only.

### 2026-08-17 — Native New Game roster interception prototype

The first title roster prototype now sits on Sudeki's actual New Game action
rather than opening unconditionally at DLL startup. The exact action hook at RVA
`0x000A0360` recognizes the native `StartNewGame` item, retains the original
controller/phase/event arguments, and returns without starting save creation while
the two-player roster is selected. Enter rejects duplicate character choices and
locks the tuple; SudekiMP then applies the selected resource types and replays the
unchanged native action on the game thread.

The visible selector is still a small reversible SudekiMP overlay. It displays
character names and badge/initial markers for Ailish, Tal, Buki, and Elco. Chosen
roles are persisted in `SudekiMP-roster.ini` beside the working executable and are
loaded on the next launch. This is intentionally a sidecar profile, not a write to
Sudeki's copyrighted save format; binding the tuple to each actual save slot remains
open. The native title state, save creation, and subsequent loading are not replaced.

Evidence: exact executable SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`; native menu
builder/action boundaries from `TitleMenuReport.java`; `CallHookTest` and exact-image
regression pass; build and `git diff --check` pass. Live confirmation still needs a
New Game click, roster lock, and observation that the original game flow resumes.

### 2026-08-18 — Native title asset inventory

Live testing confirmed that the intercepted New Game flow now performs the
native fade to black, accepts a roster lock, and resumes the original game
start. The remaining visual problem is presentation: the selection page is
still drawn as queued text over black rather than as a resident Sudeki
front-end page.

The exact-build, read-only `TitleMenuAssetReport.java` now identifies the five
language-indexed title-label tables and traces their runtime initializers. The
English rows are `SFE_OPTION1.SQX` (Continue), `SFE_OPTION2.SQX` (New Game),
`SFE_OPTION3.SQX` (Options), `SFE_OPTION4.SQX` (Credits), and
`SFE_OPTION_QUIT.SQX` (Quit). Each table contains seven language variants in
English/French/German/Spanish/Italian/Japanese/Russian order. This confirms
that the visible stock words are baked into separate SQX presentation
resources; they are not generic native text fields that can safely be renamed.

The animated row geometry is reusable and separate from those labels.
Confirmed resources include `SFE_OPTION_START.HOM`, `SFE_OPTION1.HOM` through
`SFE_OPTION5.HOM`, `SFE_Option_Bar`, `SFE_Option_Bar_Highlight`, and
`SFE_Option_Bar_Select`. The numbered option objects expose `IDLE`, `ON`,
`OFF`, `HIGHLIGHT`, `SELECT`, and `UP` animation states. The front-end also
contains `SFE_Background_Characters`, `SFE_MenuPanel`, `sfe_menu_titlebar`,
`sfe_menu_listbar`, and `sfe_menu_infobar` nodes. Character portraits are
available as the uniform `SUI_PORTRAIT_{TAL,AILISH,BUKI,ELCO}.SQX` family.

The intended official-looking mode page can therefore use Sudeki's native
fade, backdrop, option-bar geometry, highlight/select animations, portrait
resources, and queued font renderer while supplying original labels such as
`Single Player` and `Co-op`. It must load every game-owned resource from the
user's installed archives at runtime; no extracted image, model, or archive is
added to the repository. The next engine question is the safe constructor and
ownership lifecycle for additional resident front-end row objects. Until that
is confirmed, blindly replacing one of the five native title item pointers is
rejected.

Confidence: high for the label-resource mapping, language order, available
bar animation names, and portrait family; medium for which panel/background
combination best matches the final roster layout; open for safe creation and
destruction of new title-row instances.

### 2026-08-18 — Independent Sudeki Together buttons rendered

Live probes closed the remaining title-row ambiguity. The cAnim renderer
vtable for the numbered option resources has one outer submodel and zero
separately addressable named components; `OptionN_textShape` cannot be hidden
through that public component interface. A controlled construction of
resource `0x60` also proved `SFE_OPTION_START.HOM` is not reusable blank
geometry: it visibly contains `Press START`. The numbered resources similarly
retain their localized stock words. Both asset-reuse approaches are rejected.

The accepted implementation recreates the label-free button at runtime from
the observed geometry and color language. The first single-contour capsule
was visually rejected as too flat and unlike the shipped button. The revised
renderer uses three original-code layers: a soft rear shadow, a dark rounded
outer rim, and a tighter, less-rounded inset gradient bar. The selected inset
has cyan illumination from the left and gold illumination from the right.
Both rounded boundaries use quarter-pixel coverage sampling rather than the
first prototype's rectangular border test. It is generated directly into a
dynamic D3D9 texture, so the repository contains no copied Sudeki texture. An
exact relative-call hook at
RVA `0x0000A760` draws that texture after the front-end backdrop and directly
before Sudeki's ordinary CUIScene UI flush at RVA `0x0000A820`; the existing
native queued-font labels are consequently rendered on top. Five private
numbered row objects remain OFF and temporarily replace the resident title
row pointers during rendering, preventing Continue/New Game/Options/Credits/
Quit from leaking through without mutating or destroying the stock objects.

Virtual-display acceptance confirmed both mode-page states: `Single Player`
and `Co-op` are the only visible rows, and moving selection transfers the
cyan/gold highlight without moving or revealing the underlying title menu.
The three-layer refinement was captured at
`/tmp/sudekimp-layered-button-pass.png`; this path is only a local research
capture and is not repository content. The exact supported executable
regression test and Linux MinGW build pass.

Confidence: high for the render order, isolation from the stock title rows,
native text ownership, and runtime-only artwork policy; medium for final
spacing/polish until character portraits are added to the subsequent roster
pages.

### 2026-08-18 — Resident native title-row prototype

`TitleMenuLifecycleReport.java` now confirms that a new constructor is not
required for the first official-looking roster page. The live PC front-end
controller already owns five resident animated rows at `+0x70..+0x80` and five
separate localized label presentations at `+0x88..+0x98`. Selection refresh
RVA `0x000A16F0` queues state `3` for the highlighted row and state `0` for
the remaining active rows; the shared UI-state request at RVA `0x00120260`
also supports the native state `2` used to turn unused rows off.

The opt-in roster build now borrows those resident objects. Each roster page
sets the controller's native count and selection, sends the real option bars
their `IDLE`, `OFF`, or `HIGHLIGHT` state, hides only the five stock localized
label presentations, and renders `Single Player`, `Co-op`, character names,
and confirmation text through Sudeki's queued PC-font helper. No archive asset
is extracted or copied, and no title object is allocated or destroyed. The
original label active states are snapshotted and restored before the unchanged
menu builder reconstructs the shipped page. Pointer/vtable/state-queue gates
fail back to the earlier text-only page.

The title roster still represents identity selection, not complete in-game
controller activation. It persists Player 1 and Player 2 character resource
types and waits for both selected characters to be present in the story party.
The current default configuration does not install control separation, the
external Razer input bridge, Player 2 movement, AI override, or split-screen;
there is consequently no missing activation key in this title-only build.
Those systems must be enabled as an explicit co-op runtime profile after the
roster handoff is made atomic.

Automated checks: the PE32 DLL builds, the exact supported-image inert hook
regression passes, and `git diff --check` is clean. Live acceptance must still
confirm that only the new labels are visible, bar highlighting follows roster
navigation, unused rows animate off, and the original title menu returns
unchanged after Single Player, Co-op lock-in, or cancellation.

### 2026-08-18 — Stock title bleed-through diagnosis

The first resident-row live test established that roster input ownership was
correct: Up/Down changed only `Single Player` and `Co-op`. Presentation was
not yet isolated. `Continue Game`, `New Game`, `Options`, `Credits`, and
`Quit` remained visible, the old Continue row remained highlighted behind the
roster, and queued stock row animations replayed underneath the custom page.

Two implementation assumptions were corrected against the exact native code:

- the pointers at controller `+0x88..+0x98` are title text wrappers, not
  generic active-state objects at `+0x1C`; `FUN_004A1950` controls their
  visible color through the nested renderer at wrapper `+0x34`, vslot
  `+0x2C`. The roster now sets those five native text renderers to zero alpha
  and restores visibility before rebuilding the vanilla page;
- `FUN_004A16F0` discards each resident row's pending animation queue by
  copying the write cursor at row `+0xB8` into the read cursor at `+0xB4`
  before requesting the next state. The roster now performs the same native
  queue reset before requesting `IDLE`, `OFF`, or `HIGHLIGHT`, preventing old
  title transitions from looping through the new page.

The correction remains render/presentation-only. It does not modify title
actions, save creation, controller ownership, or runtime co-op activation.
The DLL builds and `git diff --check` passes; live acceptance is still needed
for clean stock-label suppression, two visible animated bars, and unchanged
vanilla restoration.

The next live pass rejected two further assumptions. Zero alpha applied once
to the nested title-label renderer did not persist, and rewriting the
controller's localized records at `+0xD0` changed neither the visible stock
words nor their independent presentation. It only removed the separately
submitted Together choices. The exact render report explains why:
`FUN_004A3760` is the PC front-end controller render callback, but it submits
only the bottom instruction prompt for modes 5 through 7. The five stock title
labels are resident UI objects rendered outside that callback, while
`FUN_004A1950` can recolor them again during later native page transitions.

The record rewrite has therefore been removed. The next bounded experiment
keeps the working Together text and native option bars, then reapplies zero
alpha to all five exact-gated stock label renderers at the controller's
per-frame render boundary. This tests persistent presentation suppression
without altering localized records, actions, controller state, or resource
ownership. A successful clean page still requires live confirmation.

### 2026-08-18 — Borrowed title rows rejected; native Options page boundary

The subsequent live pass isolated roster navigation but confirmed that the
highlight bars still moved in exact synchrony with the underlying title
choices. This is expected from the object layout: SudekiMP was borrowing the
same five row objects at controller `+0x70..+0x80`. The prototype was a
replacement presentation over the title page, not a new native page. Further
alpha, label, or animation-state adjustments cannot make one object maintain
two independent selections.

Read-only exact-build analysis of the stock Options route found the proper
architecture. The action dispatcher `FUN_004A0360` performs the front-end
transition, sets controller `+0x4C` to operation `1`, enters state `6`, and
re-enters `FUN_004A0F40`. State 6 places the separate object stored at
controller `+0xB0` into the active-page pointer at `+0xAC` and activates it
through vtable slot `+0x48`; operation `2` instead selects `+0xB4`. This is a
real independent subpage and is now the model for Sudeki Together.

The next experiment is deliberately read-only: selecting native Options once
and returning with Back logs controller state/previous/mode plus the active,
Options, and alternate page pointers and vtable RVAs immediately before and
after activation. No new page is allocated until this confirms the exact
runtime class, activation transition, and restoration lifecycle.

The live Options/Back pass confirmed the boundary. Immediately before Options,
the title controller reported state `5`, previous `4`, mode `0`, and a null
active-page pointer. Immediately after the unchanged native action returned,
it reported state `6`, previous `5`, mode `1`, and active page `05CAC7F8`,
exactly equal to controller `+0xB0`. Both resolved to vtable RVA `0x002D1CB4`.
The alternate object at `+0xB4` remained separate with vtable RVA
`0x002CA89C`.

RTTI and the focused exact-hash `TitleOptionsPageReport.java` identify that
object as `UILayerOptionsMenu`, size `0x198`, constructor VA `0x0051A7B0`,
destructor VA `0x0051C050`, activation virtual `+0x48` VA `0x0051CC80`, and
release virtual `+0x4C` VA `0x0051CD00`. Its base hierarchy is
`UILayerSubMenu` -> `UILayer`, plus `UIGameSpeedListener` and
`UIAnimationListener`. Activation constructs Options-specific children, and
the constructor writes the native Options singleton at VA `0x007C3030`.
Consequently, duplicating or repurposing this concrete class is unsafe. The
new design target is a SudekiMP-owned `UILayerSubMenu`-compatible page with its
own native option-bar children, installed into the same active-page transition
and restored through the same Back lifecycle. This is a genuine sibling page,
not another presentation laid over state 5.

### 2026-08-18 — Independent title subpage state prototype

The first bounded implementation now leaves native title state `5` instead of
placing a second presentation over it. It validates the exact
`UILayerSubMenu` vtable at RVA `0x002CA834` and the resident Options object
vtable at RVA `0x002D1CB4`, copies the base page contract into mod-owned
storage, and overrides only the known input, close-query, activation, and
release slots. During New Game interception, controller `+0xB0` points to the
mod page only for the native state-6 activation call. The real Options pointer
is restored immediately afterward, while controller `+0xAC` retains the mod
page as the active subpage and controller state remains `6`.

This separates title-controller input ownership: the state-5 navigation and
menu-builder path no longer runs beneath Together navigation. All roster-page
events are consumed by SudekiMP until selection completes. On Single Player
or co-op lock-in, the original active-page pointer, Options pointer, state,
previous state, and mode are restored before the original New Game action is
replayed. A failed pointer, vtable, or activation check restores the captured
controller fields and declines the interception.

This pass still reuses the five resident animated bar objects for its visual
rows; it does not yet allocate independent native child-row objects. The live
acceptance question is now narrower: confirm that the original title choices
are absent and no longer move in sync while the Together page is active, then
confirm that Single Player restores and starts the untouched New Game path.
The PE32 DLL builds, the exact supported-image regression passes, and
`git diff --check` is clean.

### 2026-08-18 — Queued-font alignment semantics (roster centering bug fixed)

Static-only pass (no game launched) to resolve why the roster heading, option
labels, and `ENTER SELECTS` prompt were not centered while the button bars
were. Decompiled the queued-font path against the supported image (SHA256
`8ceb1d3c…bb94`).

Confirmed facts:
- `FUN_00409930` (RVA `0x00009930`) is the queued text submit: `CUIScene` in
  `ECX`, UTF-16 SSO text in `EAX`, five stack args font/alignment/x/y/color.
  `FUN_00409990` is the same with the draw-variant flag `[0x14]` set to `1`.
- Entry layout (0x54 bytes): `[0]` font, `[1]` alignment, `[2]` x, `[3]` y,
  `[4]` SSO length flag, `[5..]` UTF-16 text, `[0x13]` ARGB color, `[0x14]`
  draw-variant flag.
- `FUN_0040A820` (RVA `0x0000A820`, CUIScene render) consumes the queue and
  does not read `[1]` (alignment) in this pass; it passes `(x, y, text)` to
  the draw helpers.
- `FUN_005d11f0` (flag 0) / `FUN_005d12a0` (flag 1) take `(alignment, x, y)`
  and store the mode at layout object `+0x20`.
- Alignment modes: `0` = center at x, `1` = left at x, `2` = right at x.
- Text space is 640x480 units (right-align uses `0x1E0` = 480 height), so the
  horizontal center is `x = 320 = 0x140`.

Root cause: the roster page submitted `x = 145` with alignment `0`. Native
centered title labels submit `x = 0x140` (e.g. `FUN_00409990(1,0,0x140,0x122)`
in `FUN_00578C90`); `145` was the left edge of a centered heading, not the
API's center coordinate. Fixed by submitting `x = 0x140` (constant renamed to
`NATIVE_ROSTER_TEXT_CENTER_X`). PE32 build, `SkillTraceImageTest` under Wine,
`continue-research.sh --check`, and `git diff --check` all pass. Live visual
acceptance is pending a user-available run.

### 2026-08-19 — Four-character roster portrait discovery

The four shipped character portrait textures are confirmed in the user-owned
`SOLData.baf` archive:

- `SUI_PORTRAIT_TAL.SQX`
- `SUI_PORTRAIT_AILISH.SQX`
- `SUI_PORTRAIT_BUKI.SQX`
- `SUI_PORTRAIT_ELCO.SQX`

The archive's World Map UI configuration also explicitly maps these portraits
to the four party entries. This confirms that the roster can load them from
the legitimate local game installation at runtime; no extracted texture is
needed or may be stored in the repository.

Focused read-only exact-build Ghidra reports
`PortraitRosterUiReport.java` and `TitlePortraitSlotReport.java` established
an important ownership boundary. The title controller constructs a native
portrait group at `controller+0x84` through `FUN_00559490` (VA `0x00559490`),
and the constructor assigns that group during front-end setup
(`FUN_0049F110`, VA `0x0049F110`). This is not a generic image widget: it
allocates three complete `UIPortraitGizmo` objects (each 0xC00 bytes), a
0x6C0 subordinate state object, a UI block loader, and named-anchor bindings.

Title state `0x0F` activates and refreshes that existing group. It operates on
only the two embedded cycle-icon fields at group offsets `+0x30C8+0x600` and
`+0x30C8+0x640`; it does not expose a four-slot roster API. The narrow native
resource selector is `FUN_0055C070` (RVA `0x0015C070`), but assignment alone
is insufficient because the portrait/gizmo construction and attachment route
depends on its UI scene configuration and named anchors. The combat HUD
portrait route has the same constraint and must not be borrowed for title UI.

Conclusion: the portrait assets are known and legal to load at runtime, but
the current three-gizmo title group must not be resized, hijacked, or reused
as a four-player roster. The next bounded research target is the World Map's
four-party-marker presentation, or a generic title-scene `UIElementTexture`
construction/attachment route. Either must establish object ownership,
anchors, teardown, and resource assignment before SudekiMP creates its own
four centered roster portraits. Player-color rings/labels remain mod-owned
presentation around those runtime-loaded native images: P1 red, P2 blue, P3
yellow, P4 green.

### 2026-08-19 — Portrait handle boundary confirmed

The roster page successfully resolves each logical `SUI_Portrait_*` key through
Sudeki's native cache. The returned slot-4 value, however, is an opaque and
tagged Sudeki UI texture-table handle (observed examples include
`0x00AF575E`), not an aligned `IDirect3DTexture9*`. A short experiment that
fed those values into a mod-owned D3D9 textured quad produced four white
fallback rectangles. That draw path was removed rather than shipped.

`GenericCycleIconReport.java` confirms the correct owned path: a
`UIElementCycleIcon` receives the resource through `FUN_0055C070` /
`FUN_0055C0E0`, which starts the request with the icon's pending-job list and
then refreshes the named scene-anchor binding. The roster must use that native
widget lifecycle (or an equally verified generic UI attachment route) before
the shipped head art is displayed. The present page therefore keeps its
original-code character cards, names, and player-color selection ring, while
the user's local portrait assets remain unextracted and unredistributed.

### 2026-08-20 — Native Load Game portrait-selector trace

The split-screen camera trace was not required for the front-end. A temporary
observation-only inline hook was installed at the exact portrait/resource
selector `FUN_0055C070` (RVA `0x0015C070`) while the native **Load Game** page
was open. The hook was hash-gated, forwarded the original six-byte prologue
through a trampoline, and recorded raw register/stack values without changing
resource selection or UI state.

Opening the save page produced four selector calls after the native front-end
dispatch settled. The observed raw tuples were:

```text
ECX=0x7AD7C230 EAX=0x06D57B00 stack_receiver=0x06D57BF4
ECX=0x06D57F14 EAX=0x06D57F00 stack_receiver=0x06D57F14
ECX=0x00000009 EAX=0x06D57C00 stack_receiver=0x06D57C34
ECX=0x06D57F54 EAX=0x06D57F00 stack_receiver=0x06D57F54
```

This confirms that save-select portraits use the same native selector
boundary, but their inputs are runtime resource handles/pointers rather than
four simple character enum values. Static disassembly of the surrounding
save-page update function (`FUN_0048C710`) shows it selecting resource handles
from the save entry's party data and passing per-entry UI receivers at offsets
around `+0x2C0`/`+0x300`. The four calls establish the correct live path, but do
not yet prove which raw handle corresponds to Tal, Ailish, Buki, or Elco. A
follow-up hook at the save-entry update boundary can correlate each receiver
with its party record before roster-widget construction is attempted.

### 2026-08-20 — Native save-page action boundary correction

The first observation hook targeted `FUN_0048C710` (RVA `0x0008C710`), a
resource-refresh helper that expects its save-entry object in `EAX`. Exact-build
static tracing identified the actual save-page action dispatcher as
`FUN_004898A0` (RVA `0x000898A0`), with the controller in `ECX` and two action
arguments on the stack. Its cases for events `6` and `7` call `FUN_0048C710`
after validating the selected save and current page state.

SudekiMP now installs a hash/byte-gated, observation-only hook at
`0x000898A0` in addition to the portrait selector trace. It records the
controller, phase, event, argument, page mode, selection, and flags, then
tail-jumps through the original trampoline. No save data, UI state, or
resource handle is modified. Live callback capture remains pending because
the automated test window reached the native confirmation dialog without
accepting its final input; the next run should validate the event-6/7 records
before any portrait-to-character mapping is attempted.

### 2026-08-20 — Save-page focus callback located

The follow-up run disproved both save-page inline-hook candidates as the live
navigation boundary: neither `FUN_004898A0` nor `FUN_0048D970` emitted a
callback while the native Load Game list visibly moved. The active path is the
front-end action bridge already used by SudekiMP at `FUN_004A0360` (RVA
`0x000A0360`, native front-end action vtable slot RVA `0x002CB228`). Runtime
trace while opening Continue Game recorded this native phase/event sequence:

```text
phase=5 event=3 argument=1  stage=4 mode=5
phase=6 event=3 argument=0  stage=4 mode=5
phase=5 event=0 argument=1  stage=4 mode=5
phase=6 event=0 argument=0  stage=5 mode=6
phase=2 event=0 argument=0  stage=7 mode=8
```

The save-page focus object then advances through `FUN_0055E120` (previous),
`FUN_0055E190` (next), and `FUN_0055E2D0` (apply selected group). The latter
refreshes each portrait child through `FUN_0055C070` / `FUN_0055C0E0` and
updates the native save details. This explains why portrait-selector calls
appear when a slot changes even though the guessed save-page action hook stays
silent.

Confidence: high for the front-end action boundary and portrait-group update
chain; the event-number meaning and direct save-slot-to-character mapping are
still to be correlated from the save-page object, not guessed from raw
portrait handles.

### 2026-08-20 — Native roster portrait GPU boundary confirmed

The four Load Game `UIElementCycleIcon` widgets do expose the decoded portrait
textures, but not through the previously tested opaque resource handle. On the
supported executable the confirmed ownership chain is:

```text
UIElementCycleIcon+0x34
  -> cSOLMaterial (vtable RVA 0x002DEB7C)
  -> material+0x08 pointer array
  -> array[0] cResidentD3DTexture (vtable RVA 0x002DD80C)
  -> resident+0x04 backend
  -> backend+0x04 IDirect3DTexture9
```

Runtime validation found a Wine D3D9 COM object at the final pointer for each
of Ailish, Tal, Buki, and Elco. Drawing those game-owned textures after the
native UI flush produced all four correct heads in the centered roster cards.
The old white-square result is therefore closed: it was caused by treating a
resource identifier as a GPU texture pointer. SudekiMP does not extract,
serialize, copy, or redistribute the portrait data and does not take ownership
of the final COM objects; the native Load Game page remains their lifetime
owner.

The original portrait anchors can be hidden safely through the exact
`UIElementCycleIcon` visibility method at RVA `0x0015C020`. The broader Load
Game scene is a separate remaining presentation problem. Calling the page's
paired visual-release method at RVA `0x00080660` while the title controller
still retains the page caused a next-render crash, so that experiment was
rejected and removed. The stable build retains the owner page and borrows only
the validated resident textures.

Confidence: high for the complete material/resident/backend/GPU chain and the
four live portraits; high that page-wide destruction is not a safe hiding
mechanism under current title-controller ownership.

### 2026-08-20 — Roster backing-page isolation confirmed live

The Load Game page cannot be destroyed after lending the four portrait
materials: its native visual-release method at RVA `0x00080660` invalidates
objects still retained by the title controller and caused a next-render crash.
The safe boundary is instead the existing CUIScene queue consumer at RVA
`0x0000A820`.

The accepted render order is now:

```text
drain native title/Load Game queue
-> draw an opaque SudekiMP roster backdrop
-> draw independent roster cards
-> draw the four borrowed game-resident portrait textures
-> submit SudekiMP labels through Sudeki's native font queue
-> flush that fresh text queue
```

The four source `UIElementCycleIcon` anchors are also hidden again at this
last presentation boundary because native page updates may reactivate them.
This keeps the Load Game owner objects alive while preventing their page,
map preview, stock buttons, and original portrait positions from appearing.
When a resident head exists, its card no longer draws the fallback silhouette.

Live acceptance on the supported GOG build showed one clean full-screen roster
canvas with the four heads in their corresponding character slots, native-font
heading/names/prompt, and no Continue/Load page or duplicate title map beneath
it. Build and exact supported-image regression passed before the live run.

Confidence: high for the queue-order cause and the presentation-only fix.

### 2026-08-20 — Story-intro Wine crash isolated to accelerator-handle exhaustion

The long-standing crash during the orange opening-story movie is not a
SudekiMP exception. It reproduces with the untouched supported GOG executable
(`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`)
launched directly in the dedicated research prefix without the launcher or
injected DLL. Both the vanilla and modded runs terminate in Wine 11.0 Staging
with:

```text
dlls/win32u/window.c:97: alloc_user_handle:
Assertion `index < MAX_USER_HANDLES' failed.
```

A filtered Wine-server trace identifies the exhausted object class exactly.
The failing vanilla run made 32,693 `alloc_user_handle(type=0008)` requests,
all from Sudeki's main thread `0024`. Wine's public `ntuser.h` defines type
`0x0008` as `NTUSER_OBJ_ACCEL`. The final successful handle was `0x0001fff0`;
the following allocation exceeded Wine's 32,768-entry USER table and triggered
the assertion. Other allocations in the same trace were insignificant by
comparison: 20 icons, 10 input contexts, and 6 menus.

The executable-side cause is also confirmed. Sudeki's message-pump helper at
VA `0x0068BF50` / RVA `0x0028BF50` calls `LoadAcceleratorsA` at VA
`0x0068BF60` / RVA `0x0028BF60` every time the helper runs, always loading
integer resource `0x65` (101). The Bink/movie playback loop at VA
`0x00504D90` / RVA `0x00104D90` calls that helper repeatedly at VA
`0x00504DD1` and during its completion wait at VAs `0x00504E51` and
`0x00504E67`. `SUDEKI.exe` imports `LoadAcceleratorsA` and
`TranslateAcceleratorA`, but not `DestroyAcceleratorTable`.

Wine 11's public `dlls/user32/resource.c` implementation of
`LoadAcceleratorsW` parses the resource and calls
`NtUserCreateAcceleratorTable` for every invocation; it does not cache the
resource-backed table. Sudeki's tight movie loop therefore consumes a new Wine
USER handle on every message-pump call. This is a compatibility mismatch
between Sudeki's repeated resource-load behavior and Wine's per-call allocator,
not evidence that Bink itself corrupts memory.

The behavioral A/B test supports the same boundary. With SudekiMP skipping
only the three logo movies, one automated Escape skipped the opening-story
movie and reached the normal title page. That run remained healthy for several
minutes, well beyond the unskipped movie's failure interval, and exited cleanly
when the dedicated prefix was stopped. Allowing the story movie to run filled
the accelerator table and reproduced the assertion.

The compatibility fix is now implemented as an exact-build-gated
`LoadAcceleratorsA` IAT wrapper. It validates the supported message-pump
opcodes and their relocated import targets, then caches the first successful
handle only for Sudeki's own module/resource-101 pair. All other instance and
resource combinations forward unchanged to the real `user32` function. A
dedicated unit test confirms that the matching request calls the native API
once, repeated requests reuse the result, and nonmatching requests continue to
call the native API.

The first live install attempt safely failed before process resume because the
initial signature gate compared raw absolute operands without accounting for
normal PE base relocation. No patch was applied. The corrected gate resolves
both operands relative to the live module base and still requires the exact
IAT target. The isolated Wine/Gamescope rerun then logged one
`resource_loaded` event for handle `0x0002007e` and one `resource_reused` event
for the same handle. With `SkipStartupMovies=false` and no injected Escape,
the complete orange opening-story movie played successfully; the user also
confirmed the intro now plays in full. Sudeki remained alive past the former
failure interval and was stopped deliberately after acceptance.

Build, accelerator-cache unit test, exact supported-image regression, and the
normal research checkpoint all passed. The generated test configuration was
restored afterward; no game executable or archive was modified.

Confidence: high for the vanilla reproduction, leaking object class, exact
Sudeki call chain, Wine allocation behavior, cached-IAT remedy, and full-intro
live acceptance.

## 2026-08-20 - New Game roster lifecycle observation

Executable: supported GOG build SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.

Method: observation-only launch in a dedicated headless Gamescope window using
the research prefix. The three logo movies were skipped by the existing
startup hook. One automated `Escape` skipped the opening story movie so the
title controller could be reached without spending the run waiting on video.
No gameplay ownership, AI, split-screen, HUD, or camera override was enabled.

Findings:

- The title front-end controller was observed at vtable RVA `0x002cb2b4`.
- Activating New Game transitioned into an independent roster subpage rather
  than a second title-button overlay. The roster page reached native state 6,
  then performed a controlled backing-page leave and took over with its own
  controller/renderer state.
- Runtime-created roster rows use resource IDs `0x63` through `0x67` and the
  native renderer vtable. This is a native-compatible presentation path, not
  an extracted asset copy.
- The portrait trace resolved resident game-owned resources for all four
  actors: Ailish index `0x116`, Tal `0x115`, Buki `0x117`, and Elco `0x118`.
  Each produced a material, resident texture, backend/GPU texture, and icon
  object. The page can therefore display the actual game portraits without
  putting game assets in the repository.
- Save-page traces were installed at RVAs `0x0008c710`, `0x000898a0`, and
  `0x0008d970`; no save load was committed during this observation.

Confirmed fact: New Game -> independent native-compatible roster page ->
resident portrait resources.

Open boundary: roster choice/lock -> save/load completion -> party creation ->
controller target, AI mode, HUD ownership, and camera initialization. The next
pass should select Co-op and trace that boundary while keeping gameplay hooks
disabled.

## 2026-08-20 - Co-op lock and post-New-Game transition

Method: same isolated headless Gamescope/Wine workflow, with the automated
story-movie Escape path.  The run remained observation-only for gameplay.

Confirmed:

- Roster navigation reached the character pages and accepted a distinct
  Ailish/Tal pair.
- `split_screen_render event=co_op_roster` recorded player-one type `0x01`
  and player-two type `0x23`.
- The sidecar profile was written successfully.
- The native roster rows and borrowed resident portrait objects were released.
- The roster page backing state was restored.
- The original native New Game action was replayed with stage transition
  `stage=5/mode=10` followed by `stage=10/mode=0`.

Result after the replay: the isolated frame remained black.  No subsequent
party creation, level-load, controller-target, AI-mode, HUD, camera, or actor
spawn trace was observed before the run was stopped.  This is a confirmed
transition boundary, not evidence that gameplay ownership failed: native New
Game was invoked, but the research process did not reach a visible initialized
level.

Confidence: high for Co-op selection, sidecar persistence, page teardown, and
native action replay; unresolved for post-lock world initialization.  Next
pass should isolate the black transition (including whether a second native
movie/scene process is created) before adding any gameplay ownership writes.

Follow-up diagnosis: the isolated capture was stopped approximately 12 seconds
after the native action replay.  The working installation contains
`movies/FMA01_poem.bik`, the opening story movie, and its container reports a
duration of approximately 115.7 seconds.  `SkipStartupMovies=true` currently
intercepts only `Publisher.bik`, `ClimaxLogo.bik`, and `TWIMTBP.bik`; it does not
skip story movies.  Therefore a black frame during the first dozen seconds is
consistent with the normal New Game opening movie and is not evidence of a
failed level load.  The roster lock/replay logs still show no gameplay
initialization because the run ended before that movie completed.  An
observation-only movie-name/PID trace was added for the next run; no movie is
being skipped or altered by that trace.
## 2026-08-20 - Deferred roster availability and roster navigation recovery

The roster is now treated as a session contract rather than an instruction to
force whichever actor happens to be the initial native lead. The selected
Player 1/Player 2 resource types persist after the roster page closes. The
game-thread application path reports each selected actor independently as
`present` or `waiting` and does not enable split-screen or rotate the native
controller until both selected actors exist in the active party.

This supports the intended Tal-first flow: a save may begin with Tal as the
only available party member while the selected Ailish role remains pending;
when Ailish is created in the party, the same persistent roster contract can
finish the role handoff. If a level transition recreates party objects, the
old pointer lock is released and reacquired against the new actor instances;
the selected types are not discarded.

The native-compatible roster page now has a Back action on the mode, player,
and confirmation pages. Back returns one page at a time, and Back on the
Single Player/Co-op page restores the original title menu without replaying
New Game. This is reversible UI state only; it does not alter native movie
playback or save data.

The opening story movie remains observation-only. The movie/PID trace confirms
which BIK resource starts; autonomous research should continue using the
existing external Escape movie-skip path rather than spending the run waiting
for the full intro.

## 2026-08-20 - Story shortcut feasibility: position and door inventory

A read-only Graphify/source/archive pass was performed before attempting any
story shortcut. The executable and existing reports establish a promising but
incomplete teleport boundary:

- Script-facing `SetPlayerPosition(float,float,float)` is at RVA `0x00104ED0`
  and resolves the active group slot 0 before calling the internal position
  setter.
- The internal `CPosition` vector setter is at RVA `0x00003050`; it writes
  `CPosition+0x18/+0x1C/+0x20`, marks the object dirty, and records a change.
- Party entries use the same character layout and expose their position object
  through `character+0x44`, so arbitrary-character position access is plausible.
  The ABI, collision/grounding side effects, and safe multi-actor call sequence
  are not yet proven; no teleport write has been added.

The executable's native transition inventory includes `EnterZone`,
`SwitchZoneNOW`, `LoadZone`, `CWorld::SwitchMainZone`, and
`CWorld::LockActiveZone`. The user-owned archive also contains serialized
`CDoor` components, `LEVEL`/`INTERIOR`/`DOGLEG` zone records, and named portal
resources such as `Portal_Tal`, `Portal_Ailish`, `Portal_Buki`, and
`Portal_Elco`. These facts confirm that door/zone identities exist in the
engine/data model, but do not yet identify the exact story-door object or the
interaction ABI that checks combat completion.

Next safe research step: resolve the position-setter calling convention and
trace one `CDoor` interaction through its combat gate into `EnterZone` or the
equivalent native transition function. Only then should a disabled story-mode
teleport/door-assist command be implemented.

## 2026-08-20 - Cafu cleanroom crash and weapon compatibility boundary

All addresses below apply to the supported GOG executable SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.

The authored developer PC `PC_Cafu` can be created successfully. His actor,
Elco-compatible body resources, and hidden weapon item are not the immediate
crash. The repeatable access violation occurs at VA `0x00523638` (RVA
`0x00123638`) in the model-wrapper constructor called from the
`CCharacterWeapon` item transition. The instruction dereferences `ECX`, which
is null because the typed `.HOM:41` resource proxy has no loaded model payload.
The native constructor does not check this result before calling the model
interface.

Read-only live inventory inspection confirmed:

- Cafu's pistol is global item ID `48`, type `WeaponDarkElco`.
- It is deliberately absent from retail inventory category 5; category slot
  `48` returns null because the playable list ends at slot `47`.
- Item 48's primary model is `W033_CAFUSPISTOL.HOM:41`, runtime identifier
  `0xD4E32E13`.
- Its equipped-model override is the normal empty sentinel, matching retail
  Elco weapons; that field is not the fault.
- Elco's first weapon is global item ID `24`, and its working primary model
  identifier is `0xDF85ECE7`.
- `SOLData.baf` contains the W033 item reference and complete model data, but
  its resource index contains no entry for item 48's identifier
  `0xD4E32E13`.
- The archive's trailing bucketed index stores records as
  `{data offset, byte size, full hash}`. The one record whose extent contains
  the complete W033 model is hash `0xA4FE4833`, data offset `0x206F5000`, size
  `0x0002C888`. Its strings include `W033_CafusPistol_SolShader2SG`,
  `WeaponFollow`, the pistol mesh hierarchy, and its authored effects.
- The working Elco control hash `0xDF85ECE7` appears in that same index, while
  Cafu's stale `0xD4E32E13` value is absent from both the index and the entire
  archive byte stream. This is a shipped item-to-archive hash mismatch.

A private `--cafu-testroom` compatibility path now waits for the native item
database and asks Sudeki's own creating resource lookup at RVA `0x00011730`
to preload type `41`, hash `0xA4FE4833`. Only after that lookup exposes a
non-null loaded HOM payload does the mod save item 48's original 12-byte
`ResourceName` and replace only its stale identifier with the verified archive
identifier. Cafu retains item ID 48, `WeaponDarkElco` behavior, damage, and all
other item/actor data. The write is exact-build/resource-identity gated,
verified after application, and restored only if ownership still matches.

The earlier reversible Elco-pistol visual alias remains only as a delayed
safety fallback if the real W033 payload cannot load. It is not used when the
archive correction succeeds.

The isolated Wine acceptance run requested `0xA4FE4833`, received a non-null
proxy payload, logged `policy=actual_w033_archive_resource`, spawned Cafu, and
confirmed that item 48 remained his current weapon. The Elco alias did not run
and no access violation occurred. This closes the missing weapon-payload cause
and proves the actual shipped W033 can be loaded without editing
`SOLData.baf`.

The first firing tests then exposed a second, independent missing-resource
boundary. Cafu's selected missile record reached the native launch path, but
its presentation wrapper contained no render object. Native `CPosition`
transform processing dereferenced that null render object first at RVA
`0x00110F02` and, after a narrower dirty-flag experiment, at RVA `0x00111163`.
The Cafu-probe build now consumes only this impossible presentation update:
when the exact missile call presents a dirty `CPosition` whose attached
wrapper has a null render object, it clears the dirty flag and detaches that
wrapper before native transform processing. Repeated shots no longer crash;
their gameplay path, wall collision, and impact sparks continue, but the
projectile mesh is intentionally absent. This is containment, not a completed
projectile-visual repair.

Read-only weapon-presentation tracing established that the remaining floating
pistol is not caused by failure to run the native equip transition:

- `PC_CAFU.sol` uses `ELCO_CAFU_LORES.HOM:41`, `pc_elco.ani:43`, armed locator
  `WeaponLoc_Rhand`, holster locator `WeaponLoc_leg`, and fire locator `SFX`.
  The retail Elco record uses the same three locator names.
- Cafu's world model resolves `WeaponLoc_leg` as index `6` and
  `WeaponLoc_Rhand` as index `12`; the weapon slot changes from `6` to `12`
  during the native arm transition.
- The first-person model resolves `WeaponLoc_Rhand` as index `4`, and the
  weapon slot is rebound to `4` when the wrapper switches.
- The world renderer progresses through its movement/arm selectors, while the
  first-person renderer uses selector `5` for ranged idle and selector `3`
  for the firing action. The fire selector advances at rate `24.0` and the
  missile launch occurs during it.
- Both weapon attachment slots retain an identity local matrix; only slot zero
  is active. The gun therefore receives no mod-authored positional or rotation
  correction.

These facts moved the visible pistol fault below the item/config/controller
layer. A reversible, environment-gated A/B build then substituted Elco's
known-good pistol visual while leaving Cafu's actor, animation, controller,
locators, and missile data unchanged. Live visual acceptance confirmed that
the Elco pistol sits correctly in Cafu's hand. This rejects the Cafu
low-resolution skeleton and Elco animation set as the cause: the shipped W033
Cafu pistol's own mesh origin/orientation is incompatible with the otherwise
working attachment contract.

The safe compatibility path is therefore to retain Cafu's hidden item 48 and
all of its gameplay data while substituting only a known-good Elco pistol
visual. Restoring W033 itself requires either a verified local model-transform
correction or a future SudekiForge asset repair; copying or replacing Cafu's
body animations is neither necessary nor supported by this test.

Confidence: high for both crash boundaries, hidden item identity,
stale-versus-indexed hash mismatch, reversible W033 correction, native equip
and fire transitions, crash-free invisible projectile containment, and the
live locator indices. Confirmed by controlled A/B comparison: W033's visual
transform is the source of the floating/spinning gun. Open: visible projectile
construction and whether W033 should receive a runtime local-transform fix or
an offline SudekiForge asset correction.

Disposition: Cafu exploration is complete for the current SudekiMP scope. The
character is an unfinished Elco-derived developer variant rather than a fifth
independent playable-character implementation: he uses an Elco-derived model,
`pc_elco.ani`, Elco-compatible ranged control/locators, and a hidden Dark Elco
weapon with Cafu-specific visual resources. The remaining invisible projectile
and malformed W033 presentation are asset-completion problems, not blockers for
the four-character co-op architecture. Retain the compatibility probes for
future SudekiForge work, but do not spend the current multiplayer milestone on
finishing Cafu.
## 2026-08-21 - Observation-only native door and zone transition trace

The earlier door experiment was not a valid transition observation: it was
run with the character-switch trace and recorded action `0x32`, which is the
party/character action rather than a zone change. The exact-build Ghidra pass
confirmed the native entry-point ABIs:

- `EnterZone(char const *)`, RVA `0x00007970`, is `cdecl`.
- `SwitchZoneNOW(char const *)`, RVA `0x00007990`, is `cdecl`.
- `LoadZone(char const *)`, RVA `0x00007B80`, is `cdecl`.
- `CWorld::SwitchMainZone(char const *)`, RVA `0x00006380`, is `thiscall`.

SudekiMP now has an opt-in, observation-only trace for those boundaries plus
`SetZoneNOW` (RVA `0x00007910`) and the exported
`CDoor::ActivateFromScript` (RVA `0x000CE3A0`, `thiscall`,
`(bool requested, bool forced)`). It is enabled only by
`tools/continue-research.sh --zone-transition-trace`, which exports
`SUDEKIMP_ZONE_TRACE=1`. The hooks are exact-entry/signature gated against the
known GOG executable, log before/after calls, copy zone strings defensively,
and never alter a door, combat, position, or loading argument.
`CDoor` activation is the first boundary that observes the interaction request
itself; its native state fields include the door state at `+0x74`, flags at
`+0x73`, and a guarded availability field at `+0x70`, while its helper at RVA
`0x000CE300` prepares transition/timer state. The optional `LoadZone` hook may
be rejected by a runtime signature mismatch and is explicitly logged as
unavailable; the other required hooks must install successfully.

Archive inspection identifies the authored building-door system. Interior and
exterior entrances are commonly `.sol` trigger resources (type `38`) named
things such as `NB_Church_Door_Entry_Trig.sol`, `NB_Lhouse_Door_Entry.sol`,
`NB_Densi_Door_Entry.sol`, `NB_Inn_Door_Exit_Trig.sol`,
`ICS_Athlos_Shack_Door_Entry.sol`, and `ICS_Frappe_Door_Entry_Trig.sol`.
Their serialized script objects expose `OnEntry`, `OnExit`, and `OnAction`
handlers. Decorative/physical door assets also exist as type `12`, `15`, or
`34` resources (`IC_Door_CastleInteriorEntrance.sol`, `Door.sol`,
`DoorTAR.sol`, etc.), but the zone-changing behavior is authored in the
trigger script layer. World transition resources are separately named, for
example `WS_Country_SE_to_Bright_NE.sol` and
`WS_Country_NW_to_Castle_S.sol`, alongside their `Trigger1` and local-volume
(`LC1`/`LC2`) scripts. This gives us a static mapping source: stage/door
trigger -> authored destination script -> `SwitchZoneNOW` zone name.

This build is ready for the next live pass: launch the mode, load the Tal save,
approach a named door, and press Enter once. The log should distinguish a door
request (`EnterZone`/`SwitchZoneNOW`) from actual world loading
(`LoadZone`/`CWorld::SwitchMainZone`).

The first fresh run with the expanded hooks produced two real transitions while
the user entered and exited dungeon/building doors:
`SwitchZoneNOW("Illumina_Countryside_SE")`, followed by
`SwitchZoneNOW("Illumina_Countryside_Hub")`, each with matching before/after
records. No `CDoor::ActivateFromScript`, `EnterTemporaryZone`, or
`CWorld::SwitchMainZone` record appeared. This establishes `SwitchZoneNOW` as
the active boundary for the building/dungeon doorway progression; the exported
`CDoor` method is a script-side state helper and is not universally called by
the player interaction path. The next research target is therefore the caller
that selects the `SwitchZoneNOW` destination, not another door-state write.

### Countryside to Brightwater church mapping

Archive level metadata gives the authored route from the countryside to the
Brightwater church:

`Illumina_Countryside_SE` (level 148) -> `WS_Country_SE_to_Bright_NE`
(dogleg 50) -> `NewBrightwater` (level 11) -> `LNBr_Church` (interior 12).

The corresponding trigger resources are `NB_Church_Door_Entry_Trig.sol` and
`NB_Church_Door_Exit_Trig.sol`; the church also has `NB_Church_Start_Pos`
and `NB_Church_Cam` authored resources. These IDs are archive metadata, not
yet proven direct arguments for the runtime exports. A safe teleport/skip
command must use the authored destination and spawn/transition state rather
than blindly calling `SwitchZoneNOW("LNBr_Church")`, which may load only the
outer world or leave the player without the church entry placement.

Live confirmation at Brightwater’s Kamo entrance established the interior
mechanism: the game called `EnterTemporaryZone("LNBr_Kamo_shop")` while the
active world remained `NewBrightwater`; no `SwitchZoneNOW` event occurred for
that doorway. This strongly suggests the church entry should be tested as an
`EnterTemporaryZone("LNBr_Church")` transition after the correct exterior
world is active, with its authored start position and camera applied by the
native temporary-zone loader.

The church test confirmed that prediction. The doorway emitted
`ExitTemporaryZone()` followed by `EnterTemporaryZone("LNBr_Church")`, with
`NewBrightwater` still active as the main world. This is the confirmed native
building-entry path for the church and is the correct boundary for a future
observation-gated cleanroom skip command.

## 2026-08-21 — World-aware traversal subsystem

The first traversal subsystem is now implemented behind
`EnableZoneTraversalMenu`. F7 opens a developer-only menu with two pages:

- Persistent worlds. Enter invokes the exact-build `SwitchZoneNOW` path and
  relies on the authored default start for that world.
- Temporary areas. Right opens only interiors whose metadata parent matches
  the currently loaded main world. Enter constructs Sudeki's native
  reference-backed 12-byte `ResourceName` and calls the native
  `EnterTemporaryZone` path. An active temporary area is exited first.

The menu refuses to open an interior page while a persistent world transition
is still pending, preventing a stale-world pointer from being reused. The
initial registry contains the six confirmed countryside/Brightwater worlds and
the confirmed Brightwater and Countryside-SE interiors; more authored entries
can be added as their parent-world mappings are verified.

This is a reversible research tool, not a gameplay teleport system. It does
not write player coordinates, bypass doors, or fabricate spawn markers. Native
world loading, temporary-zone cleanup, start positions, cameras, and party
catch-up remain responsible for placement.

### Traversal first-live test: repeated world selection and Church crash

The first live F7 pass reached the expected `EnterTemporaryZone("LNBr_Church")`
call from `NewBrightwater`, but the operator rapidly repeated Enter/world
selection inputs first. The trace contained many repeated `SwitchZoneNOW`
requests (including a cycle through the countryside zones) before the Church
request. This is unsafe for an asynchronous loader and is treated as a failed
stress pass, not evidence that the authored Church mapping is invalid.

The traversal path now has two safety changes: a transition guard rejects
repeated activation while a world/area request is pending or inside a short
settling window, and the reference-backed native `ResourceName` for a temporary
area is retained until the corresponding temporary-zone exit. The latter avoids
an asynchronous load observing freed resource-name metadata. Build and both
offline regression tests pass after this change; a clean live Church retry is
still required. The follow-up retry showed why that is necessary: a raw
`EnterTemporaryZone("LNBr_Church")` call can load the temporary resource without
the door-authored start-position/camera context, leaving the party in the
skybox. Direct interior activation is therefore fail-closed until the native
door placement seam is identified; persistent-world switching remains available.

The follow-up observation-only run entered the Church through the real door.
The trace again showed only the expected `EnterTemporaryZone("LNBr_Church")`
boundary with `NewBrightwater` as the world pointer; the existing transition
hooks did not observe a start-position or camera call before the process ended.
This narrows the missing work to the native temporary-zone/door state machine
or its subsequent placement consumer. It does not justify re-enabling the raw
menu call.

### Church placement hook — internal position setter identified

The script-facing `SetPlayerPosition` export (`RVA 0x00104ED0`) is not used by
the authored Church door. A five-byte exact hook on the internal fastcall
setter (`RVA 0x00003050`) captured the real post-transition placement writes.
The setter receives `CPosition` in ECX and a float3 in EDX, then writes
`CPosition+0x18/+0x1C/+0x20`.

On the clean Church entry, the first captured vector was
`(0.0244926, 0.5283574, -0.0156175)`, followed by several zero/default
component writes and additional actor/scene vectors. This proves the door
does perform native placement after `EnterTemporaryZone`; the missing data is
which captured `CPosition` belongs to the active player and which later call
installs the authored `NB_Church_Start_Pos`/`NB_Church_Cam` context. The direct
temporary-zone menu remains disabled until that ownership/camera correlation is
complete.

The camera-aware retry added a second result. `EnterTemporaryZone` populated
the live `CWorld` start-state fields with `(X,Y,Z)=(-43.21291,-4.11155,
339.59406)`, orientation `(-0.013462,-0.0,0.999909)`, and camera-index value
`2820` (`0x0B04`). No `SetRenderCamera` call followed the Church load; only the
ordinary `default` camera was selected before the transition. Therefore the
interior camera is selected through temporary-world state/index consumption,
not by the named-camera API. This is the next static boundary to trace before
implementing a safe world teleport.

### 2026-08-22 — Full persistent-world transition boundary

The live comparison pass established why the first F7 world request appeared
to do nothing. Static analysis of the supported executable shows two distinct
exports:

- `SwitchZoneNOW` (RVA `0x00007990`) only marks a valid zone for switching by
  calling the internal `FUN_00405A70` path.
- `SetZoneNOW` (RVA `0x00007910`) calls `FUN_00405D20`, which performs the full
  teardown and authored world-transition setup.

The developer traversal menu had been calling `SwitchZoneNOW` directly. Its
live trace showed only `switch_zone_now` before/after and no subsequent
world-load/placement sequence. A real authored transition reaches the broader
pipeline and emits the expected `set_zone_now`/`enter_zone` behavior before
temporary-area placement.

The traversal world action now routes through the observation wrapper for
`SetZoneNOW`, preserving the exact native call trace while using the complete
authored transition boundary. Interior traversal remains disabled until the
door-specific temporary placement context is reproduced.

The first live `SetZoneNOW` attempt exposed a second safety boundary: the
cleanroom party is not automatically assigned the selected world's authored
start position/camera. Ailish remained at an invalid world-relative position
and appeared below the map while drifting upward. The trace also showed that
rapidly selecting multiple destinations stacked teardown requests. A native
completion latch now blocks repeated requests until an `EnterZone` or
`SwitchMainZone` confirmation is observed. Direct persistent-world traversal
is additionally fail-closed until its authored spawn/camera context is found;
the tool will no longer send either world or interior jumps blindly.

### 2026-08-22 — Native arrival-context cache for safe traversal

The next traversal pass adds a bounded, in-memory arrival-context cache keyed
by `(main world, destination)`. The cache is populated only while a real save
load or authored door transition is running. The internal CPosition setter
(RVA `0x00003050`) is correlated against the currently-present cleanroom actor
position pointers (`actor+0x44`), so actor-specific authored arrival positions
are retained instead of guessing from arbitrary frame coordinates. Temporary
world camera-index state is retained as diagnostic context as well.

F7 traversal now reuses a destination only when that native context has already
been observed. After the native world/interior load settles, the service applies
the cached actor-specific positions through the same internal setter and logs
the result. If no matching actor anchor exists, the request fails closed; it
never writes zero coordinates, invents a world position, or queues another
transition. This is the safe equivalent of using the nearby save-point/door
arrival anchor that normal loading establishes.

This is intentionally a first prototype. A destination must first be reached
through a normal save load or door transition so its authored anchor can be
captured. Generalizing the cache to every authored save-point resource and
camera route remains follow-up work. Offline build, exact-image regression, and
`git diff --check` pass; no game binary or asset is stored in the repository.

This manual-previsit rule was superseded later the same day by the automatic
first-use discovery pass below; the cache format and fail-closed behavior were
retained.

### 2026-08-22 — Automatic first-use arrival discovery

The manual pre-visit requirement has been removed for the confirmed developer
traversal registry. The registry contains the six known persistent worlds and
the twelve confirmed interior resources already exposed by the F7 page. A
first F7 request for one of those names is now allowed to enter Sudeki's normal
`SetZoneNOW`/`EnterTemporaryZone` pipeline even when no cached actor anchor
exists. The existing exact internal CPosition setter hook (`RVA 0x00003050`)
captures the resulting actor-specific positions automatically; subsequent
requests reuse those anchors.

Arrival application now retries on the game thread for up to 15 seconds while
the asynchronous load creates/rebinds party actors, rather than failing after
one 750 ms attempt. Unknown destination names remain fail-closed, and no
coordinates, camera indices, or archive assets are guessed or written. This
removes the need to manually walk through every known destination before using
the traversal menu, while preserving the native Church-style confined camera
behavior. Build, exact-image regression, and `git diff --check` pass.

### 2026-08-22 — Talos companion target eligibility

The natural final-battle transition removes Ailish, Buki, and Elco and spawns
Tal alone at zero position. The exact transition detector and deferred native
combat refresh now restore all four party actors successfully. Live testing
confirmed that the restored companions become combat-aware, but do not attack
Talos.

Static analysis explains the distinction. The ordinary `CTargeter` acquisition
at RVA `0x000BA1C0` asks the world candidate query at RVA `0x00034C20` for only
the type bits stored at `CTargeter+0x7C`. Candidate scoring itself performs no
faction comparison. The shipped exports identify the relevant category API:

- `CTargeter::IncludeAlliesAsTargets()` — RVA `0x0000F520`, sets mask bit
  `0x00000004`.
- `CTargeter::RemoveAlliesAsTargets()` — RVA `0x0000F560`, clears that bit.
- `CTargeter::IsTargettingAllies()` — RVA `0x0000F5A0`, reads that bit.

The final encounter data contains `ALLY_Talos.sol`, so Talos is absent from the
companions' normal monster/boss candidate sets even though their AI and combat
states are active. The prototype now resolves the exact live `ALLY_Talos`
entity and, only for the naturally restored Ailish/Buki/Elco party, enables the
native ally-target category for the lifetime of that entity. Each actor's
original setting is captured and restored when Talos disappears or the hook is
uninstalled. It does not rewrite global factions, widen every targeter, or raw
patch the intrusive current-target node at `CTargeter+0x54`.

Party snapshots now also record the targeter pointer, ordinary target node,
candidate mask at `+0x7C`, and flags at `+0x84`. Build, exact-image regression,
Graphify refresh, and `git diff --check` pass. Live confirmation that the three
companions acquire `ALLY_Talos` and attack him remains pending.
### Talos companion targeting gate correction

- The first live party-restoration build correctly returned Ailish, Buki,
  and Elco to native AI control and their targeters repeatedly acquired the
  same ordinary target as Tal. However, their masks remained `0x02000008`:
  the prototype never called native `CTargeter::IncludeAlliesAsTargets`.
- The cause was an unnecessary policy gate. `GetGenericEntity("ALLY_Talos")`
  and `GetGenericEntity("CC_Ally_Talos")` do not resolve the final encounter's
  live entity, even though Tal's native targeter identifies it. The resource
  lookup was therefore unsuitable as encounter ownership proof.
- The targeting policy is now bounded by the already confirmed retail
  lifecycle instead: an exact zero-position Tal spawn following the observed
  four-to-one void-party collapse, followed by a complete four-member native
  party restoration. The optional name lookup and Tal's ordinary target are
  diagnostics only.
- Expected next-run evidence is a one-time `target_policy status=confirmed`
  record and companion target masks changing from `0x02000008` to
  `0x0200000c`. This pass does not force target pointers or invoke attacks.
### Talos clone targeting rejects the attacker-reservation hypothesis

**Status:** live hypothesis rejected; boss-specific AI decision guard remains.

- In the restored four-character Talos encounter, companion target masks were
  confirmed as `0x0200000c`, their native AI remained enabled, and their
  targeters acquired the encounter target. They still did not attack the real
  Talos, while the same companions did attack his spawned clones.
- The shipped resources distinguish `BOSS_Talos.sol:3` from
  `BOSS_Talos_Fake.sol:3` / `BOSS_Talos_Fake2.sol:3`. The real entity is a
  boss AI unit while the clones are general monster AI units. This remains the
  strongest confirmed authored distinction relevant to companion behavior.
- Exact-image static analysis found the consumer rather than treating this as
  a faction problem. `AiState_PC_MeleeBase` uses its `+0x24` MaxAttackers
  field and `AiState_PC_MissileBase` uses `+0x28`. Their native request-build
  methods copy the values into the target request bytes at `+0xBE/+0xBF`.
  The class loaders are VA `0x005AEE80` (melee) and VA `0x005AE6A0`
  (missile).
- A read-only debugger snapshot of all four live AI-state objects per restored
  companion confirmed those exact layouts and vtables, but their live
  MaxAttackers values are already `-1` (unlimited), not `1`. The bounded
  prototype therefore made zero writes, and the capacity theory is rejected.
- Player-controlled party members can attack and damage real Talos, while the
  same actors do not initiate attacks under AI ownership. Companion AI remains
  enabled and intermittently commits real Talos as its ordinary target. The
  next boundary is therefore the boss-type-specific AI action/attack decision,
  not damage, animation, faction masks, target acquisition, or attacker count.

### Talos boss-type candidate rejection and scoped bypass

**Status:** exact native rejection confirmed; bounded live prototype accepted.

- A clean live replay confirmed the behavioral discriminator: Ailish, Buki,
  and Elco remain under native AI ownership, ignore the real Talos, then begin
  attacking as soon as his type-1 clones appear. Their target pointers switch
  between the real encounter entity and clone entities without losing AI
  ownership.
- Exact-build function VA `0x005B6EC0` (RVA `0x001B6EC0`) is the shared AI
  candidate validator. Its first authored-policy check reads request byte
  `+0x25`; when bit `0x04` is set, it rejects candidates whose `CAiUnit+0x148`
  AI Unit Type is `3`. The real `BOSS_Talos` is live type `3`; his clones are
  live type `1`.
- Both relevant companion request paths explicitly set the bit before native
  enumeration. The missile path VA `0x005AE820` sets request-base `+0x25`
  through state byte `+0x5D`; the melee path VA `0x005AEFA0` sets it through
  request byte `+0xBD` for the request rooted at state `+0x98`.
- The prototype hooks only the exact supported build's shared validator. It
  recognizes the exact restored companion `CAiUnit` as the source and the
  exact live `BOSS_Talos` type-3 `CAiUnit` as the candidate. It copies the
  validator's 0x28-byte request to stack storage, clears only bit `0x04` in
  the copy, invokes the native validator, and discards the copy. No persistent
  AI state, Talos type, clone behavior, faction, target pointer, or attack
  function is modified.
- The hook is installed only with `EnableTalosPartyPrototype=true`, uses the
  exact eight-byte function-entry signature, and logs one scoped-bypass record
  per restored companion. The exact-image regression installs this path and
  passes.
- Live acceptance confirmed all three restored companions attack the real
  Talos and continue attacking his type-1 clones normally. The log recorded
  one exact scoped bypass for slots 1, 2, and 3 with source request flags
  `0x04`, `0x44`, and `0x74`; the copied flags became `0x00`, `0x40`, and
  `0x70`. No exception, crash, persistent query write, or clone regression was
  observed.

## 2026-08-22 — Elco jetpack fuel and cleanroom infinite-fuel control

**Status:** exact static boundary confirmed; implementation built; focused
live flight acceptance pending.

- Executable SHA256:
  `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
- PE exports and exact-build decompilation identify
  `CElcoAbility::SetFuel(float)` at RVA `0x000CDF30`, `SetMaxFuel` at
  `0x000CDF80`, `GetFuel` at `0x000CDFE0`, `SetFillupRate` at `0x000CDFF0`,
  `SetEmptyRate` at `0x000CE0C0`, and `ResetFuel` at `0x000CE110`.
- `GetFuel` returns `this+0x6C`. `SetFuel` writes the argument to both maximum
  `+0x68` and current `+0x6C` and then runs the native refresh path.
  `SetMaxFuel` writes `+0x68` and optionally `+0x6C`; fill and empty select
  configured rates `+0x70/+0x74` into active rate `+0x7C`.
- The native fuel-point consumer at VA `0x0059C8C0` obtains the active Elco
  actor and reads `CElcoAbility*` at `actor+0x104` before invoking those
  methods. This confirms the live ownership edge; it is not inferred from a
  coincidental changing float.
- The cleanroom now exposes `INFINITE JETPACK`. It exact-gates the two native
  exports and their entry bytes, reacquires the ability from the current Elco
  actor, validates readable/writable storage plus finite sane values, and
  calls `SetFuel(ability, ability->maximum)` only after fuel drops. It never
  installs a drain-rate patch or raises the native maximum.
- `tools/ghidra/ElcoFuelReport.java` records the functions, decompilation, and
  callers under the supported SHA gate. The PE32 build and exact-image test
  pass. The isolated Wine cleanroom-engine test also passes. An in-game Elco
  flight test remains pending.

## 2026-08-22 — Talos defensive and co-op balance inventory

**Status:** authored stats and generic invulnerability mechanism confirmed;
Talos-specific invulnerability windows and knockback-session behavior require
a live damage trace.

- Exact read-only inspection of the supported `SOLData.baf` record for
  `BOSS_Talos.sol:3` found current and maximum HP `45,000`, Spirit Strike
  resistance `0.4`, and boss AI Unit Type `3`.
- Talos is authored immune to poison, slow, freeze, concussed, stun, weaken,
  and curse. Haste, boost, regen, and protect are not immunity entries.
- His `CCombat` record enables the knockback system and stores
  `Num KnockBacks in Session = 10` plus
  `KnockBack Session Length(seconds) = 10.0`. The exact consumer/state
  transition has not yet been traced, so these values are confirmed but the
  provisional interpretation as an accumulated super-armour threshold is not.
- `BOSS_Talos.ANI:43` includes front/back weak and strong hit reactions,
  front/back knockdowns, get-up animations, and deaths. A static or
  non-staggering Talos is therefore not explained by absent authored clips.
- The executable's generic `CCharacterArbiter::IsInvulnerable()` export at
  RVA `0x00008980` returns whether signed byte `arbiter+0x54` is positive.
  `GELSetInvulnerable(bool)` at RVA `0x000DCA10` increments/decrements that
  refcount and mirrors it to arbiter flag `+0x50 bit 0x00000800`. This is a
  confirmed engine-wide iframe mechanism; whether and when the Talos fight
  drives it remains open.
- For comparison, the same archive parser reads Nassaria at `9,000 HP` and
  Rhythicus at `1,500 HP`; Talos is already authored as an unusually durable
  solo finale. Co-op still increases combined damage uptime and divides his
  attention, so balance should scale by active human participants rather than
  replacing the retail values globally.
- Recommended first live trace: for each hit on real Talos, record incoming
  damage/knockback, HP before/after, arbiter `+0x54/+0x50`, current AI/action
  state, animation selector, and knockback-session state. Acceptance is a
  strict classification of (a) damage rejected by invulnerability, (b) damage
  applied without hit reaction, or (c) knockback threshold/session gating.

## 2026-08-23 — Talos native damage and knockback-session trace boundary

**Status:** exact static consumer, observation-only hooks, and focused live
attack classification confirmed.

- Executable SHA256:
  `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
- The accepted character receiver at RVA `0x000D21D0` is a normal x86
  `thiscall`: `ECX=CCombat`, stack argument 1 is `DamageStructure*`, and
  `CCombat+0x10` owns the target character. The trace records the packet's
  encoded damage at `+0x14`, source pointer at `+0x30`, and presentation fields
  `+0x60..+0x67`, then compares HP and combat state before/after the unchanged
  native call.
- The collision handler at RVA `0x00138870` is `stdcall` with seven stack
  arguments and `ret 0x1C`. Argument 1 is the `CCollisionDamage` object and
  argument 5 is the target character. Its configured damage is at `+0x48`,
  multiple-hit delay at `+0x50`, and active delay timer at `+0x54`. Comparing
  a nested accepted-damage sequence classifies whether each collision attempt
  reached the combat receiver without modifying either result.
- `CCombat` loads authored `Num KnockBacks in Session` into `+0x60` and
  `KnockBack Session Length(seconds)` into `+0x64`. Runtime session storage is
  the timer at `+0x68`, current count at `+0x70`, and flags at `+0x72`.
- Exact consumer VA `0x004D2170` receives a native reaction-family boolean.
  Callsite VA `0x004D2FEA` sets that boolean only when the selected reaction ID
  lies in inclusive range `0x2A..0x36`; arbitrary damage is therefore not a
  qualifying knockback-session hit.
- On the first qualifying reaction the consumer loads the authored session
  duration into `+0x68`, then increments `+0x70`. It sets `CCombat+0x72 bit 0`
  only when the previous count is greater than the configured `+0x60` limit.
  With Talos's configured value `10`, the threshold edge is consequently the
  **11th qualifying reaction** inside one live session, not the 10th raw hit.
- `CCombat+0x5C` packs three 9-bit presentation/reaction IDs. The live trace
  records all three alongside `+0x68/+0x70/+0x72`, HP, arbiter flags, and the
  `+0x54` invulnerability refcount.
- `EnableTalosDefenseTrace` is disabled by default and exact-entry gated at
  both native receivers. It resolves only the real `BOSS_Talos` entity and
  performs no damage, HP, AI, reaction, iframe, counter, timer, or animation
  writes. The PE32 build and exact-image hook install/restore regression pass.
- The Tal-only baseline identified Tal as the sole source pointer and showed
  normal 500-damage hits select reaction `0x2A`; the stronger 1500-damage hit
  selected `0x2C`. Both qualify. Waiting beyond the authored 10-second window
  returned the next sample to count 0 before its reaction was processed.
- Blade Dance supplied the decisive pressure case: it delivered 15 separate
  465-damage packets (6,975 total), each with the same Tal source. Hits 1–11
  selected qualifying reaction `0x2E` while the session counter advanced from
  0 through 11. Before hit 12, `CCombat+0x72 bit 0` was set; hits 12–15 still
  removed the full 465 HP but selected no reaction. Talos therefore gained
  **super armor, not damage immunity**.
- Talos's generic arbiter invulnerability refcount and mirrored `0x800` flag
  remained zero throughout the baseline and Blade Dance samples. No observed
  damage rejection was attributable to generic iframes.
- After more than 12 seconds, the final basic hit entered with timer `-0.112`,
  count 0, and threshold bit clear, removed 500 HP, and selected reaction
  `0x2A` again. This live-confirms the authored session expiry resets both the
  accumulated qualifying count and anti-stagger state.
- Several accepted zero-damage packets changed neither HP nor reaction state.
  Tal's observed attacks reached the lower damage receiver without traversing
  the instrumented `CCollisionDamage` handler, so that component is not the
  universal entry path for player melee/skill damage.
- Confirmed gameplay interpretation: within any 10-second window, the first
  11 qualifying hit reactions can play. Once reaction 11 completes the count,
  later qualifying damage in that session continues at full value but cannot
  stagger Talos. Expiry restores normal reactions on the next qualifying hit.
- A follow-up Blade Dance -> Spirit Strike test reached the decisive ordering.
  Blade Dance left the live session at count `11`, timer `7.788`, and
  `CCombat+0x72 bit 0` set. The immediately following Spirit Strike packet
  (`field_63=0x06`, `field_64=0x03`, `field_67=0x11`) applied `9,296` damage,
  reducing Talos from `19,425` to `10,129` HP while the threshold remained
  set. Unlike ordinary post-threshold Blade Dance packets, it still selected
  reaction `0x2F` (`ANIMID_GETHIT_FRONT_MEGA`). This proves that the Spirit
  Strike overrides the session's ordinary reaction suppression; it does not
  clear the threshold or use generic invulnerability.
- Talos maps both `ANIMID_GETHIT_FRONT_AIR` (`0x2E`) and
  `ANIMID_GETHIT_FRONT_MEGA` (`0x2F`) to
  `A015_TALOS_GETHITFRONTUP.CLM:64`. His authored floor-knockdown clip
  `A014_TALOS_GETHITFRONTKNOCKDOWN.CLM:64` is instead bound to
  `ANIMID_GETHIT_FRONT_OVER` (`0x2D`). The successful Spirit test therefore
  proves a super-armour-bypassing mega/up reaction, not a confirmed prone
  knockdown/get-up cycle.

## 2026-08-23 — Native SudekiMP settings page and Talos co-op tuning

**Status:** native title-page navigation and persistence live-confirmed;
final-battle balance acceptance pending.

- The independent Sudeki Together page now exposes a fourth root choice,
  `SudekiMP Settings`, and a five-row `TALOS CO-OP SETTINGS` subpage. It uses
  the same private native row objects, runtime capsule presentation, title
  font submission, fade lifecycle, input ownership, and Back behavior as the
  roster flow; it does not borrow the resident title button records.
- The first settings are `Talos Tuning`, `Health Scale`, `Armor Hits`, and
  `Armor Window`. Enter cycles bounded values. The existing
  `SudekiMP-roster.ini` sidecar stores them in section `TalosCoop`; invalid
  values fall back to vanilla-compatible defaults.
- A focused title pass opened the four-row Sudeki Together root page, entered
  the five-row settings page, navigated through every row, cycled all four
  settings, and wrote each result to the sidecar without a crash or native
  title-action leak. The trace also confirmed that enabled tuning remained
  inert while the saved profile mode was `Single`.
- Tuning is applied only when both the explicit toggle and a locked Co-op
  profile are active. Single Player and disabled tuning perform no game-data
  write. Supported health scales are `1x` through `4x`; stagger limits are
  `6/10/14/18`; windows are `5/10/15/20` seconds.
- Follow-up loaded-save testing exposed an interaction gap: an enabled tuning
  profile could retain `Mode=Single`, causing the safety gate to reject the
  override even though the settings page displayed `ON`. The explicit Talos
  toggle now establishes the Co-op sidecar profile, including migration of
  previously saved `Enabled=1` profiles. Choosing the visible Single Player
  action disables Talos tuning as well, so the vanilla opt-out remains clear.
- Three independent live Talos allocations established the exact embedded
  relation `CCombat = character+0xF64`. The runtime service resolves only the
  exact real `BOSS_Talos`, verifies `CCombat+0x10 == character`, validates
  writable committed storage and sane finite HP, and refuses to scale a
  maximum other than vanilla `45,000` or its own expected target.
- Maximum and current HP are scaled together while preserving the current HP
  percentage. The confirmed `CCombat+0x60/+0x64` fields receive the selected
  stagger limit/window; active counters, timers, flags, reactions, AI, and
  damage packets are untouched. The service revalidates the fields after save
  reload even if Sudeki reuses the same heap address.
- A follow-up HUD discrepancy was traced across both native presentation
  paths. `UILayerBossBar::Update` at VA `0x004A6AC0` reads the bound entity's
  live `CStats` current/max HP at `+0x2C/+0x30` every frame and optionally
  remaps it through boss-bar upper/lower fields `+0x94/+0x98`. Live Talos
  samples proved those limits were already the neutral `1.0/0.0`; changing
  them was neither needed nor retained as the fix.
- The overhead display is the character-owned `CStatDisplay*` at
  `character+0xB0`. Its native health callback at VA `0x00529780` receives the
  display in `ESI`, stores current HP at `CStatDisplay+0x16C`, computes the raw
  current/max ratio, and updates the embedded health renderer at `+0xD0`.
  Directly scaling the two `CStats` floats bypassed that callback, leaving the
  overhead fill cached at its prior state while the boss HUD read the new
  180,000 maximum live.
- The co-op balance service now exact-gates the `CStatDisplay` vtable and
  callback entry, then invokes the unchanged native callback whenever the
  resolved real Talos's current or maximum HP changes. It performs no direct
  overhead-renderer write. Live 4x acceptance showed both renderers agree:
  at `170,750/180,000` (~94.9%) each cached `0.47`; at
  `161,780/180,000` (~89.9%) each cached `0.44`; and at
  `152,805/180,000` (~84.9%) each cached `0.42`. HP, damage, AI, phase scripts,
  and combat state remain owned by the original game.

## 2026-08-23 — Native Windows roster-menu access violation isolated

**Status:** deterministic source defect fixed locally; native Windows visual
and New Game acceptance pending.

- Three earlier manual Windows runs crashed `SUDEKI.exe` in `SudekiMP.dll`
  with exception `0xc0000005` at DLL RVA `0x0000C106`. Symbolization of the
  exact installed DLL (SHA256 `eba6fd492a787afbd070ebf64f10482baf27e861848b56c688d31d2c1780a56c`)
  resolved the fault to `draw_roster_button_capsule` at the shadow-pixel write
  in `src/cleanroom/menu.c`. The same zero-save tree remained alive with the
  roster feature disabled, so missing saves and the native Load Game page are
  ruled out as causes.
- The original two-row capsule prototype began at texture row 387. Later the
  root page grew to four rows and the settings page to five, but the texture
  remained 640x480 and the drawing loop had no clipping. The third row wrote
  through row 482 and later rows went farther beyond the locked D3D9 surface.
  Wine tolerated that undefined write during prior testing; native Windows
  faulted at the first out-of-bounds shadow pixel.
- The five-row block now begins at row 319, placing its final shadow row at
  476. A C11 compile-time assertion prevents future row-count/layout changes
  from exceeding the 480-line texture. The capsule rasterizer also clips both
  axes, and the D3D lock path rejects a pitch smaller than one complete
  640-pixel ARGB row while still unlocking a successfully locked surface.
- Linux MinGW compilation, the supported-image launcher check, and
  `SudekiMP.SkillTraceImageTest` pass. Native Windows must still rebuild the
  DLL, manually select New Game, inspect all four/five rows, and confirm both
  crash freedom and acceptable layout before this item is closed.

## 2026-08-25 — Story Test Boost for accelerated progression

**Status:** implementation and offline regression accepted; first live story
toggle/transition acceptance pending.

- The roster research profile now exposes an F6 Story Test Boost. It always
  starts off and, while active and focused in a ready world, applies the
  configured multiplier through Sudeki's exported `SetMasterGameSpeed` entry
  at RVA `0x0028BE90`. The default is `2x`; values through `4x` are accepted
  but values above `2x` remain explicitly experimental because the engine
  clamps elapsed time before applying this multiplier.
- This is an engine/world fast-forward rather than a whole-process clock. It
  should accelerate traversal, animation, scripts, and engine-timed dialogue,
  but Bink movies, streamed audio, and wall-clock waits can retain their own
  timing.
- Party protection uses the native
  `CCharacterArbiter::GELSetInvulnerable(bool)` export at RVA `0x000DCA10`.
  The helper owns a signed reference count at arbiter `+0x54` and mirrors it
  into flag `0x800` at `+0x50`; SudekiMP therefore acquires exactly one native
  lease per validated live party member and never issues an unmatched release.
- Party identities are captured from `CGroupPlayers` (`+0x90`, stride `0x0C`,
  count `+0xCC`) and require reciprocal `arbiter+0x10 == character` ownership,
  the same live world/directory generation, and stable character position and
  combat pointers. Additions and reconstructions are reconciled without
  disturbing native nested invulnerability used by skills or scripts.
- Speed is gated on complete party coverage: if any declared party member
  cannot acquire and verify protection, normal speed remains in force. Failed
  release verification retains the owned lease record for a safe retry rather
  than forgetting a possible native increment. External/cutscene ownership of
  the master-speed global causes an atomic fail-safe disable instead of being
  overwritten on the following frame.
- Focus loss and title/loading states restore normal speed when observed.
  Protection remains requested for a still-live ready party during focus loss,
  and is safely reconciled when world objects are rebuilt. The feature does not
  patch the executable or write save data.
- MinGW DLL build, supported-image signature regression, Wine cleanroom-engine
  regression, launcher shell validation, and whitespace validation pass. Live
  acceptance still needs F6 on/off, damage immunity, party join, focus loss,
  and a door/level transition in a real story save.

## 2026-08-25 — Persisted Tal/Ailish roster activates automatically on arrival

**Status:** first live story-arrival acceptance passed; door/level-transition
survival remains to be tested.

- The persisted Co-op contract selected Tal (`0x23`) as Player 1 and Ailish
  (`0x01`) as Player 2. A Tal-only save remained full-screen while Ailish was
  absent. When native recruitment published Ailish, the roster service waited
  for the stable party/controller signature instead of committing during the
  recruitment transition.
- The earlier synthetic post-update F1 pulse was rejected. Sudeki preprocesses
  that action before its next exact `== 1` consumer check, so the injected
  value advanced without switching. The replacement exact-gates and calls the
  native `CGroupPlayers` Previous/Next consumers at RVAs `0x00023F60` and
  `0x00024060` with their shipped `ESI=group` ABI. This preserves native party
  rotation, controller reassignment, AI/arbiter ownership, and listeners.
- The call mirrors the native outer safety gates: stable controller and party
  state, target actor flags, player-switch permission, quit/UI/input blocks,
  and a 250 ms unchanged-identity window. Controller target and party front
  must agree before the call and must change coherently together afterward.
- Live trace sequence `957424..957583` proves the automatic path. The first
  native request deferred without mutation; the second moved both target and
  front from Ailish `0x086A2F18` to Tal `0x0836BAA0`. SudekiMP then claimed
  exactly Ailish for Player 2, enabled runtime split-screen, locked the
  Tal/Ailish identities, and activated the gameplay gate. No manual F1 or F10
  was used.
- The viewport portraits were distinct and the user confirmed the resulting
  split looked correct. Build, exact supported-image regression, deliberate
  Previous/Next signature-mismatch rejection tests, and whitespace validation
  pass. The next acceptance boundary is preserving the same controller, AI,
  HUD, and camera ownership across a door or level transition.
- At this checkpoint normal roster co-op was intentionally untethered. The
  `--party-lifecycle-trace` profile explicitly leaves
  `EnableSecondPlayerSeparationGuardPrototype=false`, so Player 2 movement
  bypasses the former 10-unit outward-only guard while camera-relative input,
  attacks, split cameras, and controller bridging remain active. The dedicated
  `--second-player-separation-test` retains the old guard for isolated research.
  A fresh live install logged `separation_guard=false` in both configuration
  and control-separation initialization.

  This temporary policy was superseded by the 2026-08-26 roaming-boundary
  milestone below; repository defaults remain off, while the lifecycle profile
  now opts into the visible, symmetric, exploration-only implementation.

## 2026-08-26 — Visible symmetric roaming boundary

**Status:** implemented and exact-image/pure-policy acceptance prepared; live
gameplay acceptance remains pending.

- The former Player-2-only separation check is replaced by one pure policy
  shared by both players. Exact relative-call hooks at controller movement
  submissions `0x00028E3F` and `0x00028E5E` gate Player 1 without skipping the
  rest of the native controller update; synthetic Player 2 movement uses the
  same evaluator. Live roaming exposed that projecting away only the outward
  component still allowed a tangential path into unloaded terrain. At the hard
  limit the revised policy therefore accepts only a clear inward radial
  request; outward, lateral, and near-lateral requests are blocked
  symmetrically.
- Eligibility requires a settled native party/controller state, two active
  human leases, world resources, non-combat state, and native Exploration
  camera mode for 250 ms. Combat, loading, cutscenes/authored cameras,
  transitions and votes, Player 2 drop-out, and external-controller loss clear
  the evaluator immediately.
- A full-frame transparent cleanroom overlay draws the same amber/red range
  panel and meter in both viewports beginning at 80% of the configured range.
  The hard clamp is armed only after that overlay reports a successful draw;
  device loss or presentation failure makes the boundary advisory and
  fail-open, preventing an invisible wall.
- `EnableSecondPlayerSeparationGuardPrototype` remains default-off for
  compatibility. The `--party-lifecycle-trace` and dedicated separation
  profiles opt in. `EnablePlayerMovementTrace` is rejected alongside the
  boundary because both deliberately own the same two exact callsites.

## 2026-08-26 — Roster participation and party-atomic TEMP transitions

**Status:** native whole-party placement is live-confirmed; balanced follower
presentation and authored TEMP-camera sharing are built and awaiting the next
visual entry/exit acceptance.

- Sudeki's temporary interiors are not independent worlds. `CWorld` retains
  the exterior descriptor, but `EnterTemporaryZone` deactivates its scene,
  update nodes, collision, registry bodies, and localized audio before making
  the TEMP descriptor current. A surviving Player 2 actor/camera at the old
  coordinates therefore sees a void; keeping both rooms fully live would
  require world/scene/entity/AI/script/audio virtualization.
- The multiplayer policy now treats roster identity, human participation, and
  the current actor/camera lease as separate state. Dropping out restores the
  locked Player 2 character to native AI, collapses to native full-screen, and
  keeps the Tal/Ailish-style type contract. F10 toggles this participation;
  controller Start requests rejoin and a one-second Back+Start hold drops out.
- `CGroupPlayers` TEMP entry normally places only slot zero. Exact call RVA
  `0x00005C59` invokes `PopToNamedLocation` (`0x000F63D0`) while the incoming
  descriptor is temporarily current. Exit call RVA `0x000068D3` invokes the
  native lead mover (`0x000F30A0`, stdcall/20-byte cleanup) after exterior
  roots reactivate.
- The opt-in transition prototype wraps those two relative calls and, after a
  verified lead `CPosition` write, invokes exported
  `AiPCFormationPopMembers` at RVA `0x000F6260`. The engine synchronously
  derives collision/navigation-safe formation offsets and moves every member.
  Exact formation membership is checked against the declared active party;
  transform-only fallback is deliberately disabled because an XYZ write cannot
  prove destination scene/entity membership.
- Before the native transition, Player 2 input, camera, HUD cache, pointer
  locks, and AI override are synchronously quiesced where verifiable. The
  roster types and participation intent survive. After placement, the service
  waits for a stable state-4 TEMP or state-3 exterior descriptor for 250 ms,
  invalidates old camera frames, then lets the existing roster transaction
  reacquire the same selected character by type.
- A 15-second timeout never reuses stale actor/camera pointers. It leaves the
  native Player 1 world playable, keeps the roster choice, and drops Player 2
  out until an explicit rejoin. The first live acceptance should use one
  authored New Brightwater door, verify both actors inside, move both players,
  exit, and then test F10 plus controller leave/rejoin.
- The first save-load acceptance exposed a false transition boundary before
  gameplay ownership existed. `SetZoneNow(NewBrightwater)` calls
  `ExitTemporaryZone` twice as cleanup with no active TEMP descriptor. The
  prototype armed an exit transaction from the persisted roster alone, never
  observed a lead placement, timed out, and cleared Player 2 participation;
  F10 only appeared to fix the split because it requested participation again.
- Party-atomic transitions now require an active roster role lock. The TEMP
  exit wrapper additionally rejects calls nested inside `SetZoneNow` and
  requires the current descriptor to be native state 4. Thus save loading
  preserves the pending Tal/Ailish join request, while a genuine active co-op
  interior exit still enters the fail-closed transition barrier. A pure policy
  regression distinguishes active state-4 exit from inactive runtime,
  SetZoneNow cleanup, and state-3 main-world cases.
- The first authored-door acceptance reached the exact native lead-placement
  call, but rejected the formation pop because `CGroupPlayers+0xD0` was
  nonzero. Static exports identify that field as the `SetModeLeadOnly` nesting
  count, and TEMP placement deliberately leaves the group in lead-only mode
  for the entire interior. Removing only the validation gate proved that the
  formation API cannot move a suspended follower: Tal was inside, while
  Ailish remained outside with `+0x2B=1` and `+0xD0=1`.
- At the exact lead-placement call Ailish still has two disable leases: the
  interior lead-only lease plus a short-lived loading lease. The hook records
  the proven lead write but defers the group operation until the state-4 TEMP
  descriptor and world-ready flags remain stable for 250 ms. It then requires
  exact depth one and exact follower disable refs of one, invokes native
  `CGroupPlayers::SetModeFullParty` at RVA `0x24850`, verifies depth/ref
  transitions to zero and unchanged formation identity, and finally invokes
  `AiPCFormationPopMembers`. If the synchronous radius check fails after the
  lease was consumed, exact `SetModeLeadOnly` at RVA `0x24720` restores the
  vanilla quarantine. Nested depth or inconsistent disable refs fail closed;
  exit never consumes an unrelated lead-only lease.
- The next authored-door run confirmed the deferred native sequence itself.
  `SetModeFullParty` changed group depth `1 -> 0`, formation placement moved
  Ailish to within the destination radius, the roster transaction reclaimed
  her exact actor, and split-screen resumed. Her transform, model wrapper,
  render object, and renderer all remained valid and advanced inside the room,
  but the body was invisible because render-object flag `0x4` remained set.
  This separates successful scene/actor placement from presentation ownership.
- Exact functions `CGroupPlayers::ShowPartyMembers` at RVA `0x00024950` and
  `HidePartyMembers` at RVA `0x00024A70` own that separate presentation layer.
  Both are `void __thiscall(CGroupPlayers *)` and share the stable 13-byte
  entry `83 EC 10 53 55 56 57 8D A9 9C 00 00 00`. They iterate fixed nonlead
  slots 1 through 3. Show decrements each body model's signed hide depth at
  `model+0x74`, clears render flag `0x4`, invokes the registered visibility
  callback, and unhides the two supported equipment presentation objects;
  Hide is the exact inverse.
- The transition now captures exact follower/model/render identities and a
  visible depth-zero baseline before native entry. After full-party mode and
  formation placement it requires a native `0 -> 1` hide-depth delta, calls
  Show exactly once, and verifies depth zero plus visible body/equipment before
  split can commit. The override is explicitly owned across the interior. On
  genuine state-4 exit, after Player 2 quarantine but before native Exit, it
  calls Hide once and verifies `0 -> 1`; Sudeki's own exit path then consumes
  that lease before the hooked lead mover. This avoids the signed counter
  underflow that a blind or repeated Show would cause.
- The clumped doorway/void Camera 2 was not stale cache state. Fresh Camera 2
  copied the persistent Player 1 matrix and then translated its eye by the
  actor-position delta, which is appropriate for an outdoor follow camera but
  wrong for authored fixed TEMP cameras such as Yemi's house camera index
  `2820`. While a settled state-4 TEMP descriptor is current, Camera 2 now
  copies the exact native room matrix and projection every frame, applies no
  actor translation, and ignores independent right-stick orbit. On exit the
  controller camera state resets and the outdoor translated/orbit policy is
  rebuilt from the current native camera.
- Static camera reconstruction identifies the outdoor mismatch as the same
  architectural shortcut: Camera 2's translated/manual matrix never entered
  Sudeki's Exploration obstruction solver. A disabled-by-default prototype now
  targets the named Camera 2 through `CCameraManager::SetCameraTarget`, installs
  its own Exploration state, and consumes that camera's independently scheduled
  native render-state generation only after exact target/mode/state checks.
  Player 1 remains the global render camera. The shared `CCamera` input vtable
  forwards every native camera except Camera 2, preventing Player 1 mouse events
  from rotating both views. This first cut intentionally has no synthetic
  Player 2 input bridge: while native Exploration is ready, independent P2
  right-stick orbit is disabled; manual fallback/combat/unsupported phases keep
  the existing right-stick path. Native input translation is a separate follow-up.
- The exact Exploration bootstrap tuple is not stored in the live P1
  `ExplorationStateData`. The supported image's native state-zero path at
  `0x004CF831` prepares `0.0f`, `FALSE`, and exact bits `0x47C34FF3`
  (`99999.8984375f`) for the internal transition installer called at
  `0x004CF847`; the public `SetCameraState` wrapper reaches that same installer
  after resolving the state name. Camera 2 uses that evidenced tuple. The
  previous draft's P1 `+0x40` field interpretation was rejected before live
  use. A changed active-group allocation also forces a one-frame manual
  fallback and exact party-slot rebind instead of remaining in fallback
  indefinitely.
- The first live native-Camera-2 bootstrap exposed an argument-identity bug:
  the code passed the address of the party's intrusive `TPtr<Entity>` slot to
  `SetCameraTarget`, but the native resolver immediately performs virtual
  dispatch on the supplied `GELPointer` entity itself. Treating the slot as the
  entity made it call the character pointer as though it were a vtable and the
  process exited before camera acquisition could log. The corrected path keeps
  the party-slot address only as a stable rebind token and passes the character
  stored in that slot to `SetCameraTarget`. The native-collision profile stays
  disabled in user-facing runs pending a fresh isolated live acceptance.
- The compass/minimap was still globally Player-1-owned even after portrait,
  name, HP, and SP routing. Two exact call hooks now route its per-frame data:
  `UIMapManager::Update` at RVA `0x00087760` resolves and latches the stable
  character assigned to the scheduled cached viewport, and the later
  `UIMapManager::Render` call at RVA `0x00087AF7` reuses that exact latch. This
  keeps the centered yellow facing pointer and highlighted party dot on one
  actor. The event-driven last-cluster snapshot at RVA `0x00087A27` remains
  byte-for-byte native; it is global history rather than viewport HUD state.
  A missing update latch or a mismatch with the camera actually rendered holds
  the previous valid cache. Party order and controller ownership are never
  rewritten for the map.
- A disabled-by-default natural-door consent gate now captures the exact
  native HidePartyMembers delta and retains the exact reference-backed
  ResourceName before deferring `EnterTemporaryZone`. With two active humans,
  it restores the exterior, freezes P1 and P2, requires a successfully drawn
  overlay, then starts a full visible five-second countdown. P2 `A` accepts,
  P2 `B` or P1 `Esc` vetoes, unanimous acceptance commits early, and silence
  commits at the deadline. A newer neutral controller packet is required before
  P2 consent, held Esc is fenced until release, and missing UI, stale source,
  or uncertain visibility ownership blocks the door rather than failing open.
  The remaining live risk is the opaque caller continuation after the deferred
  void native call returns, so the feature remains an isolated acceptance
  prototype rather than a default gameplay feature.
- MinGW build, call-hook ABI tests, exact supported-image install/restore, and
  independent Show/Hide signature-corruption rollback tests pass. Visual
  acceptance still requires one entry and exit: both models visible inside,
  shared authored room framing, no void, and automatic split/control recovery
  outside without F10.

## 2026-08-26 — Player-statehood and shared shop ownership audit

**Historical checkpoint, superseded later on 2026-08-26:** ownership policy and
request-only P2 feedback were implemented here; target-specific P2 world
dispatch and independent shop UI remained future work.

- A process-global coordinator now separates a human seat, its generation-bound
  actor lease, and an interaction session. The immutable request provenance is
  `(serial, player, actor, actor generation, target, source generation, kind,
  target-known)`. At this checkpoint, a five-second targetless
  `GENERIC_REQUEST` was attention-only and could not enter the native commit
  path.
- At this checkpoint, the P2 badge consumed that snapshot and showed
  `P2 INTERACT?` for a live P2 generic request. Texture invalidation followed
  request serial/state edges.
  Known or uncertain shared shop/blacksmith modals suppress the split-only P2
  badge and roaming-boundary overlay.
- Exact-image static analysis found one `CInventory` pointer (`0x00808D84`), one
  `CShopInventory` pointer (`0x00808D44`), and global shop/blacksmith UI state.
  Buy, sell, and forge confirmations mutate shared item/money/equipment state in
  sequential native calls without a re-entrant transaction or rollback seam.
- The resulting policy is one serialized shared mutation lane. Money, items,
  stock, forge bytes, and save data remain native party state. Future per-player
  UI instances may shadow only selection, quantity, preview, confirmation,
  stable merchant/item IDs, and catalog revision; every confirm must re-resolve
  and revalidate against native state on the game thread.
- Dialogue, travel, quests, save/load, cutscenes, and unknown interactions stay
  host-only. The complete authority matrix, save offsets, failure policy, and
  staged shop/blacksmith roadmap are recorded in
  [player-statehood-design.md](player-statehood-design.md).

## 2026-08-26 — Seat-neutral controller actions replace targetless X

**Status:** current controller contract implemented; exact world interaction,
per-seat Quick Menu, and per-seat Quickshot consumers remain future work.

- The controller-X targetless `GENERIC_REQUEST` generator and the
  `P2 INTERACT?` badge were removed. The generic player-statehood API remains
  available for isolated provenance research, but no runtime controller path
  creates that request.
- A pointer-free action router owns rising edges and reconnect-neutral fences
  independently for seats 0 through 3; the current bridge integration supplies
  P2 only. Modal context wins over transition consent, which wins over an exact
  known interaction target, which wins over ordinary gameplay.
- The base Xbox-style face-button contract is explicit. A reports an
  exact interaction intent only for a complete actor/target/source-generation
  tuple and otherwise submits native Weak. X submits native Strong for
  Tal/Buki. For Ailish/Elco, SudekiMP contextually uses X for a viewport-local
  camera perspective toggle: the exact ranged arbiter branch never reads its
  Strong argument, while the executable registers separate native
  `ac_FirstPersonMode` (`0x3C`) and `ac_FirstPersonModeToggle` (`0x3D`)
  actions. This contextual X rule is SudekiMP policy rather than the shipped
  first-person binding. The Player 2 consumer changes only its independent
  render matrix and preserved third-person orbit; it does not invoke the
  global native camera/model transition, provide first-person arms, or claim
  Ailish magical-sight behavior. Y reports
  `quick_menu` with `intent_only` delivery because no per-seat native menu
  consumer exists. B resolves modal/consent Cancel and submits native Sweep in
  combat; outside those contexts it is blocked rather than repurposed.
- The D-pad resolves per-seat Quickshot intents, and modal D-pad/shoulder edges
  resolve navigation/page intents. These remain intent-only until their owning
  per-seat consumers are connected. The sticks stay on the existing movement
  and camera paths.
- A, melee X, and combat B share the exact validated arbiter-combat ABI. The older
  external-bridge A polling path is bypassed, preventing double submission.
  Every routed edge logs the protocol button name, resolved intent, context,
  delivery, rejection reason, actor, arbiter, and native state flags, so a
  device/driver X-Y swap is visible without packet-level log spam.
- The host and Wine router tests, native combat-ABI test, player-statehood test,
  full MinGW DLL build, and exact supported-image regression passed. No live
  game acceptance is claimed by this noninteractive checkpoint.

## 2026-08-26 — Per-player blacksmith presentation experiment

**Status:** exact-gated, default-off preview implemented; native forge commits
remain intentionally disabled and live acceptance has not started.

**Superseded profile note (2026-08-27):** this section records the experiment's
historical focused-run configuration. The current `--party-lifecycle-trace`
profile explicitly leaves the custom preview and all forge commits disabled.

- `UIBlackSmithStart` (`0x00492C40`) only requests global UI mode `0x0D` and
  returns AL. SOL discards that result and polls `UIBlackSmithActive`
  (`0x00492C60`), which also returns AL only. A paired hook can therefore own
  the wait lifecycle without activating or cloning `UILayerBlackSmith`; a
  Start-only hook would strand or prematurely resume the script.
- The loaded-image gate validates both exported RVAs and ASLR-relocated `A1`
  operands, then detours the exact five-byte first instruction of both exports.
  Independent signature mismatch, injected second-hook failure, and uninstall
  tests require byte-for-byte restoration.
- An accepted host Start opens two native-inert blacksmith shadows with distinct
  P1/P2 page, category, cursor, revision, and close state. An exact-image,
  read-only adapter now resolves each stable roster actor's equipped item and
  inventory category, socket state, ordered blacksmith rune catalog, localized
  labels, prices, compatibility, and projected native stat formula into bounded
  pointer-free snapshots. Both panels still observe the same party money.
  Keyboard and raw controller edges are routed separately while gameplay input
  for both actors is frozen.
- Every native root is checked against its exact object vtable before field
  traversal. Actor provenance additionally requires the locked stable roster
  type, the same statehood actor/generation, and exactly one occurrence of that
  actor in the bounded live party. Catalog nodes require consistent head/tail
  and next/previous links plus unique node, payload, and component identities.
  Missing definitions, unsafe text, unresolved occupants, invalid stats, and
  load/world/lease changes fail closed rather than leaving stale selectable
  rows.
- Separate catalog, inventory/augmentation, and economy generations are
  observed. The inventory fingerprint includes ordered category entries and
  both saved forge-byte regions. A changed catalog or inventory refreshes every
  open seat and clears quote/confirmation state; selection is preserved by
  stable item/rune ID rather than list index. Zero is a valid native item/rune
  ID and is represented with explicit selection-valid state.
- The split modal inspector excludes the mod-owned lifecycle only when the
  adapter proves active and the real native layer/controller are inactive, so
  both cached cameras remain visible. Any disagreement fails closed to the
  existing native full-width policy.
- Confirm remains a visible `COMMIT DISABLED` action. The serialized commit
  adapter and its exact-image tests are built, but the native mutation backend
  and UI commit wiring remain disabled. Start still exposes no merchant target,
  so authoritative forging requires pre-Start target provenance and a fresh
  game-thread revalidation of merchant, funds, equipment, socket, compatibility,
  catalog price, inventory generation, and economy generation. The latter two
  must both advance before a claimed commit could be marked verified.
  At this historical checkpoint, `--party-lifecycle-trace` enabled only this
  preview for a focused run; the checked-in INI remained false. The superseding
  2026-08-27 profile leaves the preview disabled.
## 2026-08-26: per-player Blacksmith presentation target clarified

- The custom two-panel Blacksmith overlay is retained only as a default-off,
  read-only data-isolation research scaffold. It is not the intended player
  interface.
- The product target is one native Blacksmith window per player, preserving
  each viewport and independent build navigation while serializing all
  authoritative inventory, forge, and money mutations on the host/game thread.
- Sudeki's merchant catalog remains shared and is not multiplied: purchases do
  not deplete a listing and sales do not replenish one. Native inventory also
  remains shared for compatibility with scripts and saves.
- Personal wallets follow Tal, Ailish, Buki, and Elco. A purchase charges the
  initiating character, while a shared-item sale removes the selected quantity
  once and awards the full proceeds to all four character wallets.
- Native per-player UI construction/virtualization, exact actor/merchant SOL
  provenance, P2 target acquisition, live wallet persistence, and native commit
  wiring remain future milestones. All mutation paths stay disabled.

## 2026-08-26 — Passive actor/target provenance observation integrated

**Status:** exact-build observation groundwork is integrated, default-off, and
incapable of activating a world interaction.

- The legacy `EnablePlayerInteractionRequestsPrototype` key now requires the
  zone-transition observer and installs passive call-site hooks for native
  action dispatch (`0x0040D75B`), its accepted message path (`0x0040D951`), and
  the OnAction-to-SOL submission (`0x0040CAEB`). Exact instruction signatures
  are checked before any hook is installed, and a partial install rolls every
  provenance hook back.
- Each dispatch trace records the source actor, native front actor, bounded
  candidate count (maximum 15), overflow/ambiguity, accepted target/event, and
  native acceptance state. The accepted message and SOL thread are correlated
  only inside the same dispatch/thread context; repeated identical observations
  are suppressed in the production log.
- Zone observation initializes a nonzero source generation, advances it on
  every existing zone-generation bump, and invalidates all provenance during
  teardown. Generation zero is always invalid. Actor authority also requires a
  current statehood lease and matching actor generation.
- Native-front/P1 OnAction can be labelled native-validated. A non-front/P2
  accepted candidate is deliberately labelled accepted-but-unvalidated and
  cannot authorize activation. Controller A remains intent-only: there is no
  targetless request, GUI Select replay, controller swap, validator bypass, or
  native world action.
- The SOL hook preserves both native return registers consumed by the caller:
  the task-handle result in EAX and the post-call ECX later pushed at
  `0x0040CAFA`. The exact-image signature includes that ECX consumer, so a build
  without the proven continuation fails closed.
- This is merchant/session provenance groundwork, not native per-player
  Blacksmith UI. Commit wiring stays blocked until P2 has an independently
  validated target/eligibility path and the native per-player UI/session state
  can be virtualized safely.

## 2026-08-27 — Temporary-room interaction trace: Kamo's Shop

- The dedicated co-op observation profile reached a real native temporary-room
  entry for `LNBr_Kamo_shop` exactly once. `CWorld::EnterTemporaryZone` logged
  before and after with the native world and resource identities intact; no
  vote, delay, synthetic action, or replay was enabled.
- The entry was preceded by ordinary native-validated P1 interaction records,
  but this authored shop route did not emit the currently observed event-type-2
  OnAction/SOL handoff or `CDoor::ActivateFromScript` record. Therefore those
  existing probes are insufficient to classify every temporary-room trigger.
  A safe co-op consent gate must capture the earlier target-specific authored
  route and retain/replay that exact request, rather than treating
  `EnterTemporaryZone` or the CDoor activation state as a cancellable boundary.
- Product decision: authored campaign travel remains host-led rather than
  consent-gated. The already-proven party-atomic path stages the active
  follower, lets P1's native transition run unchanged, then reacquires P2 at
  the settled destination. The vote remains disabled research for future
  divergent/custom content only.

## 2026-08-27 — Player 2 native Exploration camera accepted

- The native Camera 2 bridge completed a live Tal=P1/Ailish=P2 acquisition:
  the engine-created GELGroupPtr wrapper resolved exactly to Ailish,
  `SetCameraTarget` and `SetCameraState(Exploration)` returned, and the camera
  advanced through `target_verified`, `state_verified`, and `ready` without a
  process failure. The co-op lifecycle and door-trace launch profiles now use
  this camera in ordinary Exploration so its authored obstruction logic pulls
  Camera 2 in at walls and terrain instead of translating through them.
- The current native-ready limitation is deliberate: P2's manual right-stick
  orbit is suspended until controller events can be translated safely into the
  named native camera. Manual orbit remains the fallback for combat and other
  unsupported phases.

## 2026-08-29 — Expanded Talos encounter safety checkpoint

**Status:** native-inert policy and exact transition research implemented; the
playable encounter remains unavailable and default-off.

- The former `EnableTalosPartyPrototype` path is retired. It restored Ailish,
  Buki, and Elco with `InternalSpawnPC` after the retail Void transition had
  already reduced the party to Tal, and a live movie-skip run ended in an
  R6025 pure-virtual failure. Its collapse trigger also mislabeled
  `0xA6D349CC` as Tal; the exact native hash is `PC_KAZEL`, so the old path was
  armed by the authored merge actor's zero-position spawn. The loader now
  rejects that key before installing Talos hooks. The reserved
  `EnableExpandedTalosEncounterPrototype` key is also rejected until a
  pre-transition four-hero lifecycle and all requested per-seat runtime owners
  are proven. Supported Windows and Linux co-op launch profiles force both keys
  off.
- Pointer-free coordinators now model the replacement without calling the
  game. `TalosEncounterSession` binds a monotonic encounter/transition serial,
  world and source generations, Tal's actor and lease generations, the four
  distinct existing hero actor/lifecycle-generation tuples, immutable
  hero-to-seat/AI assignments, physical controller slots, and input identity
  generations. Post-arrival admission requires those exact same four tuples;
  a replacement actor or boss/hero alias is rejected before HP authorization.
  Confirm, cancel, claim, replay, mismatch, quarantine, and generation-bound
  recovery are strict state transitions.
- `LocalViewportLayout` produces exact full-screen, left/right, top-wide plus
  two bottom, and 2x2 rectangles for one through four human seats. A separate
  pure activation gate requires actor, camera, render, HUD, input, and cache
  proof masks to equal the active seat mask. It has no production caller; the
  live renderer and control-separation service are still concretely two-seat.
- An opt-in `LocalInputHub` models P2 through P4 controller ownership with
  stable physical-slot reservations, reconnect identity generations, neutral
  fences, and independent suppression. It has no production start caller, so
  the existing P2 bridge path remains unchanged.
- `TalosEncounterAdmission` requires four distinct resource-confirmed hero
  actors, exact human or native-AI control ownership, a live native targeter,
  exact real-boss combat/stat/boss-bar identity, and a one-shot health ticket.
  The ticket scales vanilla `45,000` maximum HP to `180,000`, preserves the
  current-health ratio, and cannot complete until the exact post-write state is
  verified. A bounded WAITING/ADMITTED transaction can be abandoned before an
  HP ticket is claimed so future integration can release mod ownership and
  fall back without a partial mutation.
- Companion admission deliberately does not bind the current target to
  `BOSS_Talos`. A null target is accepted while native AI is idle/reacquiring;
  a non-null target must be classified as an authored Talos encounter threat,
  including real Talos or a clone. The selected target is not part of the
  immutable ticket, so Sudeki remains free to retarget. Earlier live research
  already observed Ailish, Buki, and Elco switching between the type-3 real
  boss and type-1 clones. Static selectors establish that proximity is
  material without promising a globally nearest result: world queries
  radius-filter by squared 3D distance; ordinary acquisition uses
  forward/near priority buckets and selects the smallest squared distance in
  the winning bucket; combat enumeration retains the smallest squared
  horizontal distance among candidates accepted by the authored validator.
  The dispatcher still supports multiple authored request modes, and no
  damage-source-to-target-selection link is proven. A strict closest-target or
  guaranteed hit-forces-retarget rule is therefore neither imposed nor
  advertised.
- The earlier transition containment report was corrected after a raw/logical
  GEX-offset error. The exact serialized bytecode base is raw `0x27E6C`.
  `CC_NPC_Caprine_TalkingT3|PP` (`0xFAC73F18`) starts `LoadTheVoid`
  (`0x70F470C2`) with opcode `0x29` at logical `0x21C0C`.
  `LoadTheVoid` calls `SetZone|S` (`0x76FC7114`) at logical `0x2196E`
  (raw `0x497DA`), and that wrapper
  invokes native `SetZoneNOW|S` (`0xBC8FDC32`) at `0x317C`.
  `LoadTheVoid` also contains the FMA07/Void sequence and three explicit
  `DeletePC|R` calls immediately before the zone wrapper: Buki resource 4 at
  raw/logical `0x497B9/0x2194D`, Ailish resource 3 at
  `0x497C4/0x21958`, and Elco resource 5 at `0x497CF/0x21963`. All false
  `TableScene`/GimpFemale attribution was removed, and
  `tools/inspect-sol-gex.pl` now derives the bytecode base from the version-4
  hash-index size instead of treating the function-table end as code.
- The new transition-lineage tracker remains passive. It can compare exact
  source, caller, wrapper, native binding, world/source/host provenance, and
  same/descendant SOL task generations, but production continuation is hard
  coded unsupported. A live scoped opcode-`0x29` task-construction trace and
  both opcode-`0x27` frames must prove ancestry and an exact replay boundary;
  generic native binding return RVA `0x002352B1` is not sufficient.
- Static lifecycle analysis identifies an all-existing-actor alternative to
  the crashed spawn path: omit only those three exact companion deletes after
  prevalidating the immutable group/formation, then, after Tal's native Void
  placement and a settled zone, use the existing native
  `AiPCFormationPopMembers()` route to place the preserved followers. A broad
  DeletePC hook is forbidden because `TalKazelMerge`, invoked by the same
  transition, separately spawns and deletes PC_KAZEL. Final
  `GameFinish -> RemoveAllPlayers` cleanup is count-generic and loops until the
  active group is empty. The formation route is proven in ordinary temporary
  zones, not yet in the Void; after the first omitted delete, fallback is no
  longer clean, so this remains observation-only until all preconditions and
  teardown are accepted live.
- Strict host tests, the full MinGW build, five Wine policy regressions, and the
  exact supported-image regression pass. No game was launched for this
  checkpoint. The next live work is observation-only: prove the SOL lineage,
  prove exact companion-delete task lineage, retained-actor formation placement,
  cinematic/global-camera recovery, and count-generic teardown. Native
  companion distance/threat selection will be observed without requiring a
  hit-forces-retarget rule before any feature activation.

### First passive retail Void lifecycle trace

The closed `--talos-lifecycle-observation` profile completed one unskipped,
one-human retail transition without an R6025, exception, or process exit. The
profile hash-gated both the supported executable and `SOLWORLDM.gex`; every
optional gameplay mutation remained off. `FMA07.bik` is 380.37 seconds long,
which accounts for the expected quiet interval between task creation and the
post-movie script calls.

- Exact opcode `0x29` at logical `0x21C0C` started `LoadTheVoid`; the scoped
  constructor observer then captured the returned child task/thread and bound
  generation-one opaque lineage. The original handler returned normally.
- The three exact opcode-`0x27` calls and nested native `DeletePC` returns were
  observed in authored order. Group and AI-formation identity sets matched at
  every edge: Buki changed both sets `4 -> 3`, Ailish `3 -> 2`, and Elco
  `2 -> 1`. The same opaque Tal token survived all three removals.
- The exact `SetZone|S` carrier and nested `SetZoneNOW|S("Void")` frames shared
  that lineage and both returned. After arrival, a separate out-of-scope
  `DeletePC` changed a transient two-member set back to Tal-only; its native
  identifier was `PC_KAZEL` (`0xA6D349CC`), consistent with the authored
  Tal/Kazel merge cleanup and distinct from the three companion call sites.
- The exact EndTSA opcode returned and live gameplay resumed with Tal alone,
  Tal's HUD present, and the native quit menu responsive. No
  `AiPCFormationPopMembers` call occurred in the retail path.
- The transient `PC_KAZEL` member is a carry-capacity blocker, not merely an
  out-of-scope delete to ignore. It occupied a second slot in both native
  four-entry rosters before its authored deletion. Preserving Tal, Ailish,
  Buki, and Elco in place would fill both structures before that Kazel step;
  exact-build analysis shows the raw group core has no four-member capacity
  guard and would write a fifth entry/count, while the paired formation core
  rejects a fifth member. That would create a divergent ownership state, so a
  live full-roster probe is forbidden. A guarded no-delete experiment remains
  blocked until a native-safe companion staging/reattachment lifecycle is
  proven.
- Later in the same process, exact `GameFinish` opcode context entered native
  `RemoveAllPlayers` once. Its original call returned with both independently
  sampled owners still readable and the active group plus AI formation each
  changing from Tal-only (`1`, mask `0x01`) to verified empty (`0`, mask
  `0x00`). The enclosing handler then returned, with no R6025, exception, or
  detach failure. This closes the vanilla count-generic final-cleanup baseline;
  it does not by itself prove a future retained four-person cleanup.

The first observer build mislabeled `ResourceName.identifier` as a small
script resource ID and compared the native values to `4/3/5`. The captured
values were actually the already-known PC hashes: Buki `0x019C1EBA`, Ailish
`0x8557D453`, and Elco `0x0180E1D4`. The later `0xA6D349CC` value recomputes
exactly from `PC_KAZEL`; `PC_Tal` is `0x0213755C`. The observer now records the
full 12-byte native shape, safely copies a bounded name from the shared backing
record, recomputes Sudeki's alternating-add/multiply uppercase identifier, and
requires the expected exact PC name and hash for passive correlation. Word
zero is logged but is not an identity gate: its low seven bits are mutable
lazy-resolution state, so generic textual `0x00000FFF` is not universal.
Focused and exact-image Wine regressions pass after that correction; a second
passive retail run must make the new `resource_matches=true` evidence live
before any deletion can be considered for guarded omission.

This run still does not authorize party carry. Opaque roster identities are
not backed by stable actor lifecycle generations; EndTSA reported the TSA flag
true at the handler's immediate after edge; and that build had not yet captured
the committed default-camera/native-controller settle edge. The hardened
observer now covers those latter edges, but they still need live confirmation
and do not provide allocator lifetime or frame-cache freshness authority.

Post-run observer hardening does not reinterpret that old log. A new exact
`TSASetPlaying(false)` entry detour is gated to the authored EndTSA operand,
same script-runtime generation, and same native thread; it accepts completion
only on the native `true -> false` edge after the original returns. A pure-read
hero classifier now validates the complete relocated main/secondary/resource
vtable triplet for each retail hero without invoking any virtual method. During
the next passive run it will require the copied PC name/hash and the same
hero-token set difference in both party structures, in Buki/Ailish/Elco order.
Its counters are explicitly roster-observation leases, not allocator-lifetime
authority. Full-function relocation gates, all hero RTTI/COL/method evidence,
foreign-hook rejection, rollback, and byte-for-byte restoration pass focused
and exact supported-image Wine regressions. None of this observer code can
suppress a call or activate the expanded encounter.

The next passive build adds the missing Kazel insertion edge without adding a
gameplay mutation. Exact SOL provenance is
`LoadTheVoid -> TalKazelMerge (0x219F8) -> SpawnPC (0xBC1B3) ->`
`InternalSpawnPC (0x3099)`. A relocation-gated relative-call observer at RVA
`0x000B15DB` forwards the native raw group-add core exactly once, preserves its
custom EAX/stack ABI and result registers, and samples both native rosters
before and after. The candidate actor must match the exact `DarkTalEntity`
vtable/RTTI triplet and produce one common new opaque token in the group and
formation. The later exact `DeletePC(PC_KAZEL)` must remove that same token.
No raw pointer is published or retained, and the evidence remains explicitly
non-authoritative for actor lifetime. The next unskipped retail run must prove
this `1 -> 2 -> 1` chain before staging design advances.

### Second passive retail Void lifecycle trace

Run `0000016000c7b2c6` completed the corrected, unskipped one-human baseline
through native TSA release and playable Tal-only Void without R6025, exception,
or process exit. It also disproved one observer assumption without changing
native behavior.

- Native `ResourceName` evidence is now exact and live. Buki
  (`PC_BUKI`, `0x019C1EBA`) reduced both native rosters `4 -> 3`; Ailish
  (`PC_AILISH`, `0x8557D453`) reduced them `3 -> 2`; Elco (`PC_ELCO`,
  `0x0180E1D4`) reduced them `2 -> 1`. Bounded text, stored identifier,
  recomputed identifier, and expected identifier all matched. The same Tal
  token survived, and the corroborated hero-removal mask reached `0x0E`.
- Exact `SetZone|S` and nested `SetZoneNOW("Void")` provenance committed the
  Void settle session. The later authored `TSASetPlaying(false)` call produced
  a native `true -> false` edge with the original Tal roster, default render
  camera, controller target/modes, and Tal AI-inactive state all revalidated.
  `settle_evidence_complete=true`, and live inspection confirmed Tal-only boss
  gameplay with HUD and boss bar.
- Native Kazel identity is also exact. Immediately before the authored
  `DeletePC`, both native rosters contained two identical member tokens. The
  bounded resource was `PC_KAZEL`; stored, recomputed, and expected identifiers
  were all `0xA6D349CC`. The original call removed the same second token from
  both structures and restored the original Tal-only token.
- The first Kazel insertion observer did not arm. Live opcode edges were
  serialized as `TalKazelMerge before/after`, `SpawnPC before/after`, then
  `InternalSpawnPC before/after`; they were not nested on the opcode-27 TLS
  stack. Consequently the old `previous`-frame test correctly failed closed,
  emitted no group-add acceptance, and left every original call untouched.
  The native `1 -> 2 -> 1` evidence remains valid at the deletion boundary,
  but that run does not prove the raw-add call correlation itself.

The observer now models those six exact opcode edges as an ordered serialized
state machine, still bound to one LoadVoid task/runtime/script-thread/native-
thread provenance tuple. Only the complete six-edge sequence may arm the
existing RVA `0x000B15DB` raw-group-add observer. Skips, duplicates, reordered
edges, stale generations, or provenance replacement quarantine evidence while
calling native code exactly once. Kazel evidence is deliberately one-shot per
process: a second LoadVoid session is quarantined so a delayed asynchronous
completion from the first can never be attributed to the second. A repeat
passive run must therefore start in a fresh process and is required to close
the raw-add correlation; expanded-party mutation remains disabled.

### Third passive retail Void lifecycle trace

Fresh process run `00000159a12b1dff` closed the raw Kazel insertion evidence
gap without changing native behavior. The exact supported-image profile
reached `status=ready`; every gameplay mutation and expanded-encounter key
remained disabled.

- LoadVoid task, runtime, script-thread, and native-thread provenance bound at
  generations `1/1`. The exact Buki, Ailish, and Elco deletes again reduced
  identical native group/formation sets `4 -> 3 -> 2 -> 1`, with hero removal
  mask `0x0E` and one stable Tal token.
- The ordered TalKazelMerge, SpawnPC wrapper, and InternalSpawnPC before/after
  edges advanced the serialized mask `0x01 -> 0x03 -> 0x07 -> 0x0F -> 0x1F
  -> 0x3F`. Only the sixth edge armed request generation `1`, after a second
  exact Tal-only roster check.
- The exact raw group-add call at RVA `0x000B15DB` observed the unchanged
  original move both native structures `1 -> 2`. The added actor matched the
  full `DarkTalEntity` RTTI/vtable identity, both sets contained opaque token
  `44C608D0579AD5DE`, and the evidence state reached corroborated.
- The later exact `DeletePC(PC_KAZEL)` used stored, recomputed, and expected
  identifier `0xA6D349CC`, removed that same token from both structures, and
  returned them `2 -> 1`. Request generation `1` reached delete-corroborated
  state with no ambiguity.
- Exact `TSASetPlaying(false)` then observed native `true -> false` and
  revalidated the original Tal survivor, default camera, Tal controller
  target/modes, and inactive Tal AI override. `settle_evidence_complete=true`.
  Read-only window acceptance then saw the cinematic finish and stable Tal
  gameplay with HUD and boss bar. No R6025, exception, trace error, or
  unexpected native-call suppression was observed. The evidence occupies log
  bytes `614332482..614370530` on inode `15327792`.

The observer does not record the movie-skip input itself. The roughly
23-second LoadVoid-to-TSA interval is consistent with a skipped movie, but is
not treated as direct proof that the skip button was pressed.

This proves the normal temporary Kazel lifecycle and closes the raw-add seam;
it does not make a five-member roster safe. The group core still writes a
fifth entry while formation rejects it. The next milestone is a separate,
default-off proof that one companion can be removed from and restored to both
native membership structures without losing actor lifetime or ownership.

### Companion staging direction after Kazel proof

The capacity solution will preserve Kazel's full native membership lifecycle,
not suppress its raw group add. Skipping RVA `0x000B15DB` would also skip
unknown group listeners, formation enrollment, combat-state propagation, an
actor-side update, and a spawn-completion membership branch. That route is not
fail-closed once a four-hero carry has begun.

The candidate instead stages the exact group-last nonlead hero. On the proven
roster this is Elco: group order is `[Tal,Ailish,Buki,Elco]`, while formation
order is independently `[Tal,Elco,Ailish,Buki]`. Public
`CGroupPlayers::RemovePlayer` at RVA `0x00023390` must synchronously remove the
same Elco actor from both structures; public `AddPlayer` at RVA `0x00023230`
must restore both exact original orders through native listener and formation
canonicalization. Kazel then remains the authored `3 -> 4 -> 3` transaction,
and Elco restoration returns the party to four.

This is not yet a rollback contract. Both public calls return void, group and
formation can diverge on a partial listener failure, and an intrusive `TPtr`
is a weak liveness witness rather than a strong lifetime owner. The
pointer-free coordinator and exact-image, default-off observation gates are
now implemented below. The next live milestone is observation only. Even a
valid observation does not authorize a disposable synchronous Elco
remove/re-add; that mutation would require a separate decision and proof
contract. Expanded Void mutation remains disabled.

## 2026-08-30 — Ordinary-world companion-staging groundwork

**Status:** the inert coordinator and the default-off, read-only native
observation pipeline are implemented and strictly tested. Native membership
mutation remains structurally absent from the production adapter; the profile
is still disabled in the checked-in configuration, and no live run has been
performed.

- A separate pure, pointer-free research coordinator now models exactly one
  process-lifetime Elco staging attempt. It accepts only group order
  `[Tal,Ailish,Buki,Elco]` and independent formation order
  `[Tal,Elco,Ailish,Buki]`, emits symbolic tickets for one public Remove and
  one public Add, rejects replay/non-wrapping authorization errors, and moves
  any post-remove drift into sticky quarantine with reload required. Its
  snapshots can never grant production, carry, or actor-lifetime authority.
- Strict tests cover preflight rejection, exact detached/restored membership,
  immediate stability, ticket tampering, serial half-range behavior, one-shot
  consumption, authority rejection, and continuity drift. Continuity includes
  source/world, group and formation owners, controller and Tal front actor,
  camera, selected render camera, render state, scene manager, and scene
  renderer. This does not establish camera-target authority.
- The pure membership-ABI validator is now version 3. It checks exact
  supported-build PE32 bytes, relocated operands, relative-call targets,
  calling conventions, public membership wrappers, formation/listener
  propagation, complete intrusive `TPtr` and wrapper ownership machinery,
  HUD/resource and stat-display paths, Elco arbiter effects, and stat-camera
  synchronization. `seams_valid` is deliberately narrower than image
  identity, performs no native call, and grants no mutation or lifetime
  authority.
- `GetPC` returns a `0x18`-byte GELPointer/PtrObj wrapper whose embedded weak
  `TPtr` begins at `+0x0C` and is `0x0C` bytes long. The proposed no-yield
  transaction retains the same wrapper for synchronous public Remove then
  public Add and calls the wrapper's scalar deleting destructor exactly once
  afterward. This is weak liveness observation, not proof that the wrapper
  owns or preserves Elco. Direct raw group add/remove calls are forbidden;
  only the public wrappers may drive their native listeners.
- Control separation now provides an exact service-post-original dispatch
  witness. The closed service profile forwards the original native controller
  update exactly once and then invokes its sole registered observer with the
  borrowed `controller` and `update_data`. Admission requires an outermost,
  non-overlapping native-thread dispatch, exact hook/slot ownership, one
  original call, a sole observer, stable registry generation, and synchronous
  revalidation of that same witness after capture. Co-op and gameplay services
  remain off.
- The native capture bridge performs discardable A/B planning captures to
  discover the mapped image and all required dynamic spans. It bounds each
  immutable view to 128 ranges and 5 MiB, revalidates every range and its
  native read/write permissions, then crosses a final boundary for independent
  A/barrier/B/barrier copies in the same callback with no yield. It performs no
  memory query after the final boundary. Planning results are never published;
  permission or content drift, an incomplete span, overlap, overflow,
  foreground loss, or witness drift fails closed before the adapter sink.
- The pure native sampler parses only those two immutable captured views and
  makes no OS, engine, hook, allocation, or mutation call. It requires exact
  group `[Tal,Ailish,Buki,Elco]` and independent formation
  `[Tal,Elco,Ailish,Buki]` order plus the complete finite side-effect closure:
  formation fields/control backpointers, intrusive `TPtr` heads, sole listener
  dispatch, each hero's control/HUD gizmo/label and bounded no-allocation HUD
  resource path, stat display/health-bar state, stat-camera sync nodes, Elco's
  arbiter coherence, and active camera/render/scene identity. Near-miss and
  exact-image tests cover those gates.
- The sampler output is wrapperless and pointer-free: it retains salted scalar
  equality tokens and diagnostics, not native addresses or continuations, and
  leaves Elco's wrapper token zero. All production authority bits remain zero.
  The adapter accepts at most the first valid result bound to the same dispatch
  witness and publishes it as a sticky observation without beginning the pure
  coordinator.
- The normal adapter backend has no `GetPC`, wrapper resolver,
  `RemovePlayer`, `AddPlayer`, destructor, raw-sample, or membership-mutation
  path and grants no actor-lifetime authority. The synthetic synchronous
  remove/add choreography remains test-only behind
  `SUDEKIMP_TALOS_STAGING_RESEARCH_ADAPTER_TESTING`; none of it is reachable
  from the observation profile.
- Exact modal and transition audits still disprove the broad central
  predicates as comprehensive authority. CUIScene may retain the native
  CharacterController through independent TSA, Quick Menu, shop, blacksmith,
  quit, dialog/conversation, and pause states. World, async, HD-cache, object,
  texture, door, script, and PVS transition work likewise has no single exact
  global predicate. These values remain non-authoritative diagnostics. The
  finite same-frame listener/HUD/stat/arbiter/camera closure together with the
  exact game-thread, post-original, no-yield witness supports observation of
  that one callback only; it does not authorize mutation or prove actor
  lifetime.
- The loader now exposes only the default-off
  `EnableTalosCompanionStagingObservation=false` key for this pipeline. Its
  closed profile rejects every other optional feature or trace, an enabled
  `SkipStartupMovies`, and the environment-owned zone trace. It locally
  authenticates `SUDEKI.exe` SHA256
  `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`
  and `Data/SOLWORLDM.gex` SHA256
  `e36a5974f9aedea5b5b428fe2445cf496c52911ff01d4934ea8ab8124abf1ff9`,
  installs the capture observer last, and tears it down only through gated
  disable/unregister/drain before resetting capture and adapter state.
- `tools/continue-research.sh --talos-staging-observation` now generates and
  verifies that exact closed profile without enabling the zone trace. Its live
  target is a settled ordinary-world four-hero save, not the Void handoff, so
  FMA07 and the Tal/Kazel cinematic are not part of this observation.
- Strict host, MinGW, Wine, exact-image, rollback, range/copy-drift, witness,
  sampler near-miss, and adapter-ingestion regressions cover the pipeline. The
  checked-in configuration remains false. No Sudeki process was launched and
  no ordinary-world membership mutation was attempted for this work.

## 2026-08-30 — Pivot to exact post-movie restore and dual views

The playable Talos direction no longer depends on carrying all four heroes
through the final cinematic. Roughly six hours of pre-cutscene preservation
work established useful lifecycle, membership, and capacity evidence, but it
also showed that suppressing retail deletes or staging a hero around Kazel
would introduce ownership and SOL-continuation risk that the actual local-co-op
goal does not require. That carry-through route remains inert research.

The replacement keeps the authored transition intact. Retail Sudeki deletes
Buki, Ailish, and Elco, enters the Void, adds and deletes the temporary Kazel,
and completes the TSA falling edge. A lifecycle-owned, process-terminal ticket
is claimable only after those exact same-generation facts and the settled Tal
state agree. The new restore module then spawns Ailish, Buki, and Elco once,
verifies the exact four-member group and formation, initializes the restored
actors, refreshes native combat state, and applies the narrow real-Talos AI
candidate policy. Kazel is already gone, so no fifth party entry is attempted.
Any identity, topology, control, hook-ownership, or witness failure terminates
the attempt and requires reload rather than guessing or retrying the spawn.

Live runs proved the corrected boundary. The restore reached
`valid=true state=active party_count=4`, with Tal as P1, Ailish acquired through
the existing P2 control lease, and Buki/Elco in native AI mode. The first
durability runs exposed that native skill/camera and scripted paths take nested
AI-control refcount leases. Admission still requires the exact baseline, while
ACTIVE now accepts only the balanced native count/mode relations; all actual
P2 movement and action consumers remain paused unless Ailish is at this mod's
exact action-ready lease. Release drops exactly one owned lease and never drains
another native owner. Repeated live camera cycles then retained the party and
recovered cleanly without R6025.

The first Talos presentation checkpoint reuses the proven alternating
full-frame compositor. Before restore, on uncertain cameras, and until both
fresh caches exist, the game remains one native full-width view. Once the
strict post-movie status authorizes it, Tal renders on the left and Ailish on
the right; the global gameplay camera stays native and every authorization edge
invalidates both caches. The P2 camera identity must equal the requested,
active Ailish control lease, so the generic first-companion fallback is not
permitted in this closed mode. Live logs recorded exact camera acquisition and
`dual_camera_cache_active` with no compositor failure.

Ailish navigation initially used raw world axes because the closed camera
profile left camera-relative movement disabled. The corrected first camera
profile paired camera-relative movement with split/P2-camera/dual-cache and
rejected every partial combination. While Camera 2 still copied the native
orientation, movement correctly used the equivalent native P1 basis.

The next exact profile added independent Camera-2 orbit as a fourth mandatory
bundle bit. The camera-input vtable route now filters P1 broadcasts from the
named P2 camera even when native P2 collision is disabled, while forwarding
every other camera unchanged. A seat-indexed transform API publishes the
Camera-2 basis independently of whether orbit input is enabled; it accepts only
the exact seat-1 actor/camera/render-state lease and fails closed for P3/P4.
Facing changes remain separately orbit-gated.

The live run proved isolation and view-relative navigation. Before P2 input,
Camera 2 forward was approximately `(-0.158,-0.302,0.940)` while P1 remained
approximately `(-0.223,-0.236,-0.946)`. A P2 right-stick X input advanced only
Camera 2 to approximately `(-0.716,-0.302,0.629)`; P1 stayed near its original
basis. A straight P2 movement submission changed from direction bits
`be2a384e,3f7c7032` to `bf405205,3f28f6d1` and consistently reported
`camera_relative=true camera_basis=player_two_render`. Native SpiritCam
temporarily returned presentation to full width, released Camera 2, and then
reacquired it with fresh caches after the default camera and exact control
lease returned. No R6025, reload requirement, or compositor failure occurred.

This establishes the intended ownership boundary for future seats: each human
seat consumes the orientation basis of its own presented view and rotates only
that camera. P1 already has this natively; P2 is now live. Buki/Elco remain AI,
and P3/P4 camera state, render caches, HUD passes, and control consumers remain
future work.

The same run exposed the next presentation defect. Ailish cannot open a
viewport-owned Skills menu. When Tal opens his Skills menu, Tal's own half is
blank while the complete menu is also rendered on Ailish's half. Menu input,
skill-data ownership, and render ownership are therefore still global or
scheduled to the wrong cached viewport. The next milestone is independent
per-seat skill-menu opening and population: Tal's native menu must appear only
on Tal's viewport with Tal's skills, and Ailish must be able to open and use a
separate menu containing Ailish's skills. The older final-fight camera-doubling
issue follows after that menu ownership work.
