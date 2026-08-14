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

### Prepared experiment — not yet live-confirmed

`EnableSharedGroupCameraPrototype=false` is a new disabled-by-default option. While the verified Buki AI override and second-player movement are active, it:

1. Requires both current camera slots to contain the same native `GameObjectTarget`; non-gameplay/cinematic target types are left alone.
2. Retains both original slot references.
3. Creates one engine-owned `MatrixTarget`, preserving Player 1's target orientation and replacing only its translation with the Player 1/Buki midpoint.
4. Installs that target into both native camera slots and updates its matrix each controller frame.
5. Restores only slots still owned by the prototype, then releases every retained/native reference if AI is restored, the camera changes, the engine replaces a target, or required state becomes invalid.

The first proof changes focus only. It deliberately does not change zoom, distance limits, collision, skill-camera ownership, ranged scope behavior, or render view count. `tools/continue-research.sh --shared-group-camera-test` prepares the proven camera-relative movement and 10-unit outward guard plus this midpoint target. It has not been launched because the user was unavailable for visual confirmation. The warning-clean PE32 build, exact-build launcher check, isolated register/stack ABI test, and inert image hook install/restore test pass.
