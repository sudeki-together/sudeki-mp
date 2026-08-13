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
- The active missile update at RVA `0x001867D0` handles movement/range, tests the `MissileData` collision flags, and submits a qualified native attack/collision record through RVA `0x000DCD00`. RVA `0x00186610` terminates the missile.
- The `CMissile` vtable is at VA `0x006D915C`; its inherited/interface entries include exported `HitEntity` at RVA `0x000A2900`. No direct code reference reaches exported `DoDirectDamage` or `ModifyHitPoints`; their known references are binding/export data.
- Read-only archive inspection found Plasmatica's serialized missile entry: one projectile, velocity `17.0`, range `100.0`, wall and ground collision enabled, penetration disabled, bouncing disabled, and model reference `SFXES005_PLASMATICA_PROJECTILE.HOM:41`.
- Its primary attack record is `Area`, `Magic`, `asSkill`, and has serialized base damage `500`. It links to `PlasmExplosion`, whose serialized base damage is `300`, but the primary record's `Secondary Starts` value is `Never`.

### Interpretation

For this observed cast, damage is not initiated through any of the four known script method bindings on any script thread. Together with the native collision/update path, this makes native missile/collision ownership a strong conclusion. The function at RVA `0x000DCD00` is provisionally a native attack/collision submission wrapper; the downstream target at RVA `0x00018B90` still needs analysis before naming the final damage application function.

The script's `FireMissileScripted(10)` literal is not the same visible index as Plasmatica's element `2` position in `MissileCombos`; the mapping remains undocumented. The two serialized damage values must not be added together until secondary-attack activation is observed.

### Next target

Resolve the animation state pushed at bytecode operand `0x00004195` and trace the independent per-character animation-speed setter. Then test whether changing only Elco's animation multiplier advances the two watched launch events without changing world speed.
