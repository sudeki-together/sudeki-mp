# Research log

## 2026-09-12 — Buki/Elco native bank and presentation ownership corrections

Scope: configurable **two-seat** checkout at `239bcf0` plus preserved dirty
work. Tal/Elco and Buki/Elco isolated Wine test pairs; not four-character F1
switching. Supported executable SHA256:
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
Both running instances must use the identical rebuilt DLL; LA26 labels alone
do not distinguish the existing dirty wire changes.

### CONFIRMED_LIVE — native observations, not complete gameplay acceptance

- Elco combat readiness stopped first at `world_idle`, then `world_fire`, then
  `first_person_idle`: the checks and writers still expected Ailish's bank.
  Exact model-table lookup gives these semantic ID / selector mappings:

  | Semantic ID | Ailish | Elco | Renderer |
  | --- | --- | --- | --- |
  | `02` idle | 20 | 22 | World |
  | `06`, `07` movement | 22, 23 | 24, 25 | World |
  | `85` fire | 59 | 53 | World |
  | `05` idle | 1 | 5 | First person |
  | `8C`, `8D`, `8E` fire | 2, 3, 4 | 2, 3, 4 | First person |

- Elco's first-person `C1` weapon swap resolves to selector 8, length 14;
  `8C/8D/8E` lengths are 5/12/24. The actual model/renderer bank was inspected
  read-only with owner links rechecked and the relocated exact lookup code
  verified. No generic ANIMID play call was enabled.
- With corrected Elco readiness, an operator-held input produced client
  native ranged submissions and completions for host sequences 2, 3, 4.
  Physical-input pacing, every weapon, skills and Spirit Strikes are **not**
  accepted by that observation. The host used its existing world-arbiter
  fallback; its first-person-only object graph was absent.
- Buki's single weak and strong both use selector 53, but native semantic
  IDs are `72` and `74`. Second swings share selector 54, with IDs `73` and
  `75`. Both have observed playing states 1 and 65. Selector-only detection
  omitted these events entirely; observe the semantic channel AND matching
  renderer rather than guessing the input from selector 53/54.
- Buki's native locomotion table resolves only `02/06/07` to 20/23/24;
  `08..0D` have no loaded matching resources. Previous entries 64/67 labeled
  as settled idle were incorrect: 64 is WWW and 67 is SWS. An initial corrected
  journal run submitted the opening swings, but the locomotion writer still
  overwrote the combo and bypassed native action retirement.

### Correction and regression boundary

`SudekiMpLanArenaRangedCombatSelector` centralizes the verified ranged bank
mapping. `SudekiMpLanArenaBukiActionFromNativeAnimation` requires matching
semantic and renderer observations for opening swings; ambiguous selector-only
calls still reject them. Each journal edge submits **one** native input, as in
the proven Tal path, not a reconstructed full combo spaced at 450 ms.

Buki attacks are excluded from locomotion capture. Client native melee leases
are serviced before locomotion and retain presentation until positive native
idle; no forced lease clearing or weaker damage/owner guards were added.
Unsupported Buki locomotion clips reject rather than resolving to selector 0.

The focused combo/protocol/replica/authority/session/runtime-hook/owner-view
and exact-image suites passed on the final build. Component tests do not imply
complete gameplay acceptance; the native playback evidence and remaining
acceptance boundaries are recorded below.

### CONFIRMED_LIVE — final native playback spot checks

DLL SHA256 `469f46245c7f7930c6c14e4941a111e2510f86dddc45db7db8f81a9aa259d737`
was identical in both stages. The Buki/Elco run observed all eight three-input
patterns reach their expected native client final selectors and retire to
idle 20: WWW64, SSS65, WWS60, SWS67, SWW68, SSW56, WSW55, WSS66. Every opening
swing was journaled and submitted once. Commands made before native host
recovery could reject; the affected pattern was retried after idle. This is
not a claim of lossless input at arbitrary timing or every blend being ideal.

A final Tal/Ailish regression spot check on the same DLL observed client
WWW `50 -> 51 -> 62 -> 17` and WWS `50 -> 51 -> 54 -> 17`, including positive
retirement, plus Ailish native ranged submit/complete sequences. Elco likewise
completed repeated host-confirmed ranged sequences with first-person idle 5.
Full skill/Spirit/effect, weapon, physical-input pacing, disconnect/reconnect,
and arbitrary host/client character-role acceptance remain outside this proof.

Ordinary W-key movement in the final Buki host window changed host position
and produced matching host/client run selectors 6/7 at 37.17093/30.97577,
followed by idle 1. Elco's local movement similarly played 5/6 then idle 1.
Do not use `host-forward-hold` as an ordinary-walking test: that operator
rail supplies skill-aim direction. Subjective smoothness and exact cross-window
phase alignment still require dedicated capture/visual acceptance.

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

## 2026-09-01 — LAN session UI belongs to the Esc layer

The first LAN status panel hooked the native Quit renderer at callsite RVA
`0x0028D572`, invoked the unchanged renderer, and then queued `MULTIPLAYER`
text through `CUIScene`. Live capture proved that ordering is not a foreground
overlay: the queued text drains on the following frame before
`CPCQuitScreen`, so Sudeki's native center dialog paints over it. The result is
status text visible behind the dialog and no selectable Multiplayer row.

The exact-hash `QuitMenuInteractionReport.java` resolved the shipped menu's
full interaction path. Main state is `CPCQuitScreen+0x10 == 1`; cursor is
`+0x1C4`. Its text renderer uses a fixed three-element localized-ID array
`{0x52, 300, 0xDD}`. Navigation clamps the cursor to the maximum derived from
`+0x1C0`, yielding two or three native rows. Confirm dispatch handles only
indices 0, 1, and 2: Back closes the screen, index 1 enters the Exit-to-Windows
confirmation state 3, and index 2 enters the Quit-to-Title confirmation state
2. A fourth native row therefore requires a coordinated render, cursor-bound,
and confirm-dispatch extension; appending deferred text cannot create it.

The Q/QuickMenu is a worse LAN-session owner. It is one actor/gameplay
singleton, changes native UI/gameplay state, binds category and action data to
the active party actor, and is intentionally blocked on the authoritative LAN
client until host-routed gameplay actions exist. Its category/detail/action
paths contain many exhaustive state switches rather than an unused generic
tab slot. Hosting, joining, leaving, protocol errors, and peer status are
session-global pause operations, not actor inventory/skill operations.

The selected direction is therefore a real Multiplayer page under `Esc`.
Sudeki's ordinary Quit page remains the parent. A discoverable Multiplayer row
must either extend all three native main-page seams together or, for the closed
LAN arena where Quit-to-Title has no campaign value, replace that row and open
a mod-owned native-styled sibling page. The sibling page owns Host/Join,
editable `IP[:port]`, status/error text, End/Leave, and Back; it renders after
the native dialog or temporarily suppresses the parent while active, rather
than submitting deferred text behind it. The Q menu remains exclusively for
future host-authoritative skills, weapons, items, and Spirit work.

### LAN Esc-page rough draft

The first interactive draft replaces the LAN cleanroom profile's third Quit
row (`Quit To Title Screen`) with `MULTIPLAYER`. The native Back and
Exit-to-Windows rows remain untouched. Selection, navigation, and back use the
shipped Quit dispatch calls at RVAs `0x1DB71`, `0x1DBA4`, and `0x1DB64`; only
the selected third-row action is redirected.

Selecting Multiplayer suppresses the parent Quit renderer while a mod-owned
foreground sibling page is active. That page uses a dynamic ARGB texture and
an immediate post-native D3D draw, rather than the deferred CUIScene text queue
that caused the earlier status text to appear underneath the opaque Quit
dialog. Host/Join or End/Leave and Back are selectable with the same native
Enter/A, arrows/D-pad, and Esc/B events. Q/QuickMenu is unchanged.

## 2026-09-01 — LAN replica interpolation and visible-transform timing

The first client replica used the newest two snapshots with a 25 ms delay even
though the host publishes every 50 ms. It therefore advanced for half an
interval and stalled for the other half. The replacement is an ordered,
wrap-safe four-snapshot queue with a monotonic render clock and a two-interval
(approximately 100 ms) buffer. Facing uses normalized shortest-arc
interpolation, and a client-native movement-controller reset keeps both target
and current speed at zero while the host remains the sole movement authority.

Live synchronized traces then separated network state from native presentation.
The sampled position/facing and CPosition were correct, but Sudeki's attached
visible render matrix retained the prior native basis. The client now applies
one sampled transform before the first RenderStart and publishes it through the
exact native CPosition world-matrix path both before and after that boundary.
A final publication after the second RenderStart verifies the same transform
immediately before the world pass. The native updater, actor pointer, Position
pointer, wrapper, render object, writable generation/matrix storage, and
unparented cleanroom-root policy are all exact-leased and fail closed.

The accepted live build held both publication stages active without a reject,
disconnect, crash, or R6025. Across movement in every world quadrant,
authenticated sample, final CPosition, and visible render translation/facing
matched. This materially improved Ailish's client locomotion. One presentation
imperfection remains: the native movement-component accepted direction can
stay near default world +Z while the host-facing vector turns elsewhere. In
558 moving samples its average agreement with travel was about 0.57, compared
with about 0.95 for the replicated root; +Z was the visually strongest
direction. That follow-up must use a separately verified native direction
adapter rather than a raw field write, and is intentionally not included in
this accepted checkpoint.

## 2026-09-02 — Tal's LAN action identity is the native combo transition

The first LAN combat pass treated Mouse1 as a generic weak-action edge and
recognized only one isolated Mouse2 selector. That was insufficient for Tal:
Sudeki keeps a bounded weak/strong history and chooses the next authored
transition from the entire prefix. A client receiving only `weak` or `strong`
could show the correct first swing but not the heavy branches that follow it.
Worse, the synthetic Mouse1 fallback could transmit an extra attack when a
late fourth input had already been rejected by the native combo state.

Read-only analysis of the exact supported executable now follows controller
actions Weak `0x2C`, Strong `0x2D`, and Sweep `0x2E` through the per-arbiter
submission at RVA `0x000DB0E0`, attack-kind admission at `0x000DAC00`, combo
dispatch at `0x000D0730`, transition lookup at `0x000D04F0`, authored gate at
`0x000D13E0`, and accepted-result commit at `0x000D14D0`. The property loader
at RVA `0x00103190` parses `AttackWeak`, `AttackStrong`, and `AttackSweep`.
`tools/ghidra/TalMeleeComboReport.java` reproduces that evidence and refuses
any executable other than SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.

A deterministic prefix-local operator then exercised every three-input W/S
history without desktop mouse ambiguity. Live host selectors were
`WWW=62`, `WWS=54`, `SWW=60`, `SSS=61`, `SWS=63`, `SSW=65`, `WSW=68`, and
`WSS=69`, with stage selectors `W=50`, `S=52`, `WW=51`, and `WS=53`.
The LAN protocol first became `LA12` and carried each result as its own bounded
semantic variant in the four-edge journal. Host capture is native-result-only;
the old Tal synthetic weak timer/pending edge is gone. Client replay maps the
semantic result back to Tal's local selector/state without executing combat a
second time. A fourth click after a terminal selector therefore produces no
false network action.

The new `lan_arena_tal_combo_graph` module is the single translation table
used by host capture, protocol validation, interpolation combat state, and
client replay. Its pure truth-table test, protocol/replica tests, runtime hook
test, exact-image test, and full MinGW build pass. This also establishes the
graph rule for later combat features: add a path linking physical input,
native authority transition, semantic wire identity, and actor-local replay,
with exact static evidence and a deterministic live command. Do not add a LAN
action from an input binding alone.

The acknowledged follow-up exposed why even the complete W/S history is not
always a unique presentation key. A submitted `WSS` produced selector `70`
in one native context and selector `69` after a clean retirement interval.
The exact transition lookup iterates multiple authored candidates and applies
timing, target-distance, and direction gates before selecting one. Protocol
`LA13` therefore preserves selector `70` as a distinct WSS alternate instead
of dropping it or guessing that every WSS must replay selector `69`.

The same run exposed a separate client gating bug: one global presentation
boolean waited for both Tal and Ailish's combat render graphs. When Ailish's
first-person transition temporarily had no world renderer, Tal's valid combo
journal was received but suppressed. Combat transition readiness is now
actor-local. Tal replay proceeds after Tal's own armed renderer verifies;
Ailish remains fail-closed until her independent world/first-person graph is
ready. The live host/client journal then matched isolated `WSS` as
`1,7,14 -> 50,53,69` and `SWW` as `4,2,9 -> 52,51,60` one-for-one.

A fresh `LA13` host/client run then verified the complete three-input matrix
with a native retirement interval between patterns. Host semantic variants
and client selectors matched without a missing or duplicate action:
`WWW 1,2,3 -> 50,51,62`, `WWS 1,2,8 -> 50,51,54`,
`WSW 1,7,13 -> 50,53,68`, `WSS 1,7,14 -> 50,53,69`,
`SWW 4,2,9 -> 52,51,60`, `SWS 4,2,11 -> 52,51,63`,
`SSW 4,7,12 -> 52,53,65`, and `SSS 4,7,10 -> 52,53,61`.
Block also matched variant `6` to selector `20`. Selector `70` did not recur
in this clean arena state, so it remains a separately preserved,
exact-image-tested alternate from the earlier live native gate rather than an
assumed replacement for selector `69`.

The same replay initially exposed a distinct retirement defect. After the
host had returned from a terminal combo to Tal's combat-idle selector
`17/state 128`, the client retained the last finisher at state `65` until a
movement edge arrived. State `65` does not retire inside Tal's standalone
renderer; the native gameplay arbiter performs that transition, and the LAN
client intentionally does not run the authoritative arbiter. Tal now treats
the host snapshot's `ACTION -> IDLE` edge as authoritative. A fresh WSS run
verified client selectors `50 -> 53 -> 69 -> 17` and stable state `128`
without any client movement input. Ailish retains her separate bounded
first-person clip-completion policy because that renderer does self-retire.

Read-only animation-time telemetry then isolated the remaining visible Tal
retirement clip. The host's terminal WSS selector `69` reached time `34.392`,
then its native combat graph entered selector `17` on a separate idle clock.
The client changed to the same selector but inherited the terminal action
clock (the first observed idle time was `37.724`), so its correct semantic
state began from the wrong pose. The client now resets time only for Tal's
authoritative `ACTION -> IDLE` edge; ordinary movement endings, idle variants,
and Ailish retain their established clock policies. A rebuilt live WSS replay
verified `53 -> 69 -> 17`, with the first client idle witness at exactly time
`0.000` and subsequent idle time advancing normally. No movement edge was
needed to complete the transition.

That first fix also made a previously hidden two-stage retirement visible:
the client entered selector `17` at state `128`, then native animation changed
the same selector to running state `0`. On screen this looked like Tal settling
toward idle and then jumping into idle again. Tal's `ACTION -> IDLE` edge now
selects state `0` directly while restarting the clock. The next live WSS
replay retired from selector `69` straight to `17/state 0`; its throttled first
idle witness was time `6.396`, followed by continuous `18.432`, `30.408`, and
`42.564` samples, with no intervening state `128`.

A higher-rate comparison of Tal's native host transition showed that Sudeki
does not use a renderer crossfade for this handoff. During a WSW replay the
host advanced terminal selector `68/state 1` through time `35.256`, then
changed directly to selector `17/state 0` with a separate idle time near
`1.608`; channel 1 and the blend weight remained zero. The buffered LAN
client did not observe the semantic idle edge until its copied terminal clip
had advanced to roughly time `42`-`50`. A first 160 ms client-only retirement
blend removed the hard cut but visibly froze that late terminal pose. The
first calibrated bridge reduced that interval to 64 ms. A live WSW trace on
that build recorded start at selector `68/time 49.99990` and complete
retirement after 67 ms, followed by continuous selector `17/state 0` time,
but the terminal pose remained faintly visible. The final bounded experiment
therefore uses only 32 ms: idle starts immediately on channel 0 while the last
action pose occupies channel 1 and fades from `0.99` to zero over roughly two
rendered frames, after which channel 1 is reset. This is presentation
smoothing only; the host remains the sole action and combo authority.

The final four-case comparison proves why a fixed retirement blend cannot
fully match the host. Isolated Weak selector `50` and Strong selector `52`
both reach `state 65/time 17.99990` on the host, then enter idle selector
`17` at time `0.192` and `0.204`, respectively. WSW finishes through
`50 -> 53 -> 68`; the host leaves selector `68` at time `35.568` and enters
idle near `1.596`, while the client reaches the buffered idle edge with its
copied finisher already clamped at `49.99990`. SWS finishes through
`52 -> 51 -> 63`; the host leaves selector `63` at time `35.616`, while the
client reaches the edge at terminal time `37.99990`. The 32 ms bridge then
retires either client finisher in about 40-41 ms, but it cannot recover the
host phase that was lost before that bridge began. Action identities and
journal sequences match in all four cases. A complete fix must transmit an
authoritative action phase or retirement host tick on the same interpolation
timeline as transforms; further tuning of one presentation-only duration
would only trade terminal-pose hold against a hard cut.

Protocol `LA14` implements that conclusion. A current semantic action now
carries its host-observed animation phase as an unsigned 1/256-unit value;
native selector and state enums remain process-local. The replica interpolates
phase only across the same actor/action sequence, holds the last authoritative
phase until the first host IDLE endpoint, and retires at that endpoint. Client
presentation writes and immediately verifies the sampled phase on Tal's base
action channel and Ailish's world/first-person action channels. The fixed
32 ms Tal retirement bridge and its local clip-completion guess were removed.
Protocol, replica, runtime-hook, and exact-image tests pass on the initial
implementation.

This also corrects an authority-language error exposed during review. The
group `InCombat` bit is native world state: authored dungeon/world triggers
change it in normal gameplay. The shared simulation observes that state and
snapshots distribute it; a player role does not own the decision to enter
combat. The cleanroom has no authored dungeon trigger, so its Multiplayer/F8
control remains an explicitly test-only invocation of the same native group
transition. It must not be generalized into a campaign client combat toggle.

The first LA14 live trace also corrected the deterministic operator's notion
of acknowledgement. It previously acknowledged the synthetic controller
submission immediately, before Sudeki exposed the resulting selector. Under
Wine's background cadence, a following mixed W/S input could therefore reach
the native combo gate before the preceding stage existed and be rejected;
the LAN journal correctly omitted it. Sequence commands now wait for the
host-observed Tal selector edge before timing the next input. This affects
only the local research operator, never the LAN protocol or player input.

The rebuilt selector-ack operator then completed two three-stage native traces
without a guessed admission delay. `WSW` produced host semantic variants
`1 -> 7 -> 13` (actor-local selectors `50 -> 53 -> 68`), and `SWS` produced
`4 -> 2 -> 11` (selectors `52 -> 51 -> 63`). The LA14 client verified all six
semantic stages with nonzero host-phase samples on its own actor-local Tal
renderer. The same run entered combat through the cleanroom-only native group
transition and the client mirrored the resulting combat bit; no network role
authored a separate combat state. Both processes remained connected with no
fatal, exception, or `R6025` event through the trace.

Development then moved to `codex/shared-simulation`. The first extraction adds
a process-independent `lan_arena_shared_simulation` reducer with canonical
native-world and read-only replica node roles that are separate from Tal and
Ailish player roles. Canonical commits require a fresh session token, a
wrap-safe newer host tick, a valid frame, and an explicit native combat
observation; that observation overwrites the candidate combat bit. Replica
nodes can accept authenticated newer frames but cannot commit native world
state. The current host and client snapshot paths now pass through those two
respective nodes. Transaction, role, token, malformed-frame, stale-tick, and
tick-wrap tests pass, establishing a dedicated-server migration seam without
changing the current listen-server transport.

The next extraction changes acknowledgement ownership in protocol `LA15`.
The synchronized UDP worker may still validate, sequence, and coalesce an
input packet, but receipt no longer makes that sequence eligible for a
snapshot acknowledgement. The game-thread canonical simulation now admits a
bounded actor-scoped Tal or Ailish contribution, keeps their sequences and
revisions independent, and projects only the latest admitted Ailish sequence
into the current two-player snapshot. Duplicate, stale, malformed,
wrong-token, wrong-actor, and replica-side admissions fail transactionally.
The transport verifies that this simulation-owned acknowledgement never
points ahead of the receive stream, while replica admission rejects an
acknowledgement regression. This establishes the input side of the shared
simulation boundary without granting either player authority over native
combat, match, enemy, damage, or resource state.

The same reducer now enforces the native match lifecycle as a one-way state
machine. A session may progress from waiting to active to ended (or terminate
while waiting), but it cannot regress to waiting or revive after ending under
the same token. Non-active frames are invalid when combat remains enabled.
Both canonical commits and replica admissions use this rule, so a delayed or
malformed snapshot cannot resurrect an old arena transaction.

World-consequence ownership is now explicit rather than implied by where the
snapshot builder runs. Every canonical commit requires native observations for
Tal/Ailish HP and SP plus the bounded enemy set, in addition to combat and
match state. Those fields replace the candidate snapshot before the ordinary
wire validator runs. Tests deliberately forge candidate resources and omit
each observation capability; only the complete native observation commits.
This lets later actor-owned movement/presentation contributions remain narrow
without accidentally granting them damage, healing, enemy, or match authority.

Canonical frame composition is now structurally split as well. The runtime no
longer hands the reducer one mostly complete candidate snapshot. It supplies a
Tal actor observation, an Ailish actor observation, and the native-world
observation independently; the reducer builds a zeroed frame and copies only
the domain each source owns. Both actor observations must be present, exact,
and actor-typed before any commit. Focused tests reject missing observation
proof and verify that actor transforms survive while forged actor-resource
fields are replaced by native-world values.

Protocol `LA16` separates the handshake's player identity from its simulation
authority. Tal/Ailish remain the fixed playable role tuple, while a second
field identifies the canonical native-world node and the read-only replica.
The current session profile deliberately accepts only Tal/canonical and
Ailish/replica, but connected INPUT and SNAPSHOT direction is now authorized
from the node role rather than inferred from the character. A peer advertising
the correct actor with the wrong node authority is rejected during handshake;
a wrong-direction packet after connection terminates the session without
advancing its accepted sequence. This closes the transport side of the
dedicated-node migration seam while preserving the existing playable listen
server.

Protocol `LA17` makes player ownership explicit on every input contribution,
not only during the handshake. The client stamps its fixed Ailish actor type
into the normalized input payload. The synchronized transport rejects a
packet whose actor does not belong to the authenticated peer role before it
can replace the pending input, and the game-thread shared-simulation reducer
requires the same actor identity again before admission or acknowledgement.
Send-side role ownership is enforced as well. This preserves the current
Tal/canonical and Ailish/replica listen-server behavior while establishing a
bounded actor-owned contribution contract for later role negotiation; node
authority still owns packet direction and native-world consequences remain
canonical-only.

### 2026-09-03: LAN action retirement preserves both host animation clocks

Live Tal combo traces isolated the remaining client end-snap to the buffered
ACTION-to-IDLE boundary rather than packet loss. The replica held the last
sampled action phase for the whole final 50 ms segment, then forced combat
idle state `0` at time `0`. That omitted the tail of the authored action and
discarded the host's already-running idle entry.

Protocol `LA18` adds a latched retirement witness to idle actor snapshots: the
last host-observed action phase and the first host-observed idle phase, bound
to the unchanged actor action sequence. The host repeats that witness until a
new action begins, so render stalls, UDP loss, or mailbox coalescing cannot
erase the only handoff packet. The replica interpolates through the terminal
action phase while it consumes the buffered segment. At the endpoint the
actor-local client adapter selects idle and adopts the transmitted idle clock
exactly once. Invalid retirement flags, non-idle retirement snapshots,
unsequenced retirement data, and nonzero data without its validity witness are
rejected at the protocol boundary.

### 2026-09-03: Replica combat entry requires a post-arm native refresh

The first live LA18 combat-entry trace explained Tal moving while permanently
drawn in a running pose. The canonical snapshot had already entered combat and
continued to replicate translation, but the client Tal renderer remained in
the exploration idle (`4`) or combat run (`36`) selector. The client correctly
refused to write combat action selectors until Sudeki had attached the combat
weapon/animation graphs, so that missing readiness transition latched the last
native pose even though networking and transforms remained healthy.

The client now lets Sudeki finish its existing 75 ms ranged/UI arm cycle and
then reuses the proven native party combat refresh exactly once. It does not
guess a selector and does not refresh every frame. Live verification moved Tal
from selector `4` to armed idle `17`, resolved Ailish's world and first-person
combat renderers, and emitted `client_combat_presentation state=ready`. Until
those actor-local witnesses are present, presentation continues to fail closed
while position and resource replication remain active.

### 2026-09-03: Tal retirement must enter before animation evaluation

An exact live WSW trace disproved the proposed extra completed-action wire
edge. On the supported build, all three Tal submodels move directly from
selector `68`, state `65`, time `49.99990` to combat-idle selector `17`, state
`0`, time about `0.204`. No intermediate state `128` occurs. Protocol LA18's
terminal and idle clocks already describe the complete observable handoff, so
no native state enum was added to the process-independent packet.

The remaining mismatch was a local scheduling error. Installing the host's
post-update idle time before the client's first RenderStart advanced that
clock twice and made its first drawable idle witness roughly `0.35`. Moving
the write to the late pre-world seam produced the correct numeric clock but
changed the selector after Sudeki had evaluated the skeleton, leaving the
visible pose one frame behind. The client now installs selector `17/state 0`
before animation evaluation and backs the transmitted idle clock up by the
measured local frame duration at rate `12`. Sudeki's ordinary update then
advances the skeleton to the host-observed entry clock; the pre-world seam is
verification only and never samples the network again.

### 2026-09-03: native Tal presentation removes the replica idle snap

Repeated live traces showed that phase-perfect low-level renderer replay was
still visually inferior to the host. Selector, state, terminal clock, idle
entry clock, position, and visible root matrix could all match while the client
still exposed a terminal pose or a clipped transition. The missing owner was
Sudeki's higher-level combat animation state machine: direct renderer writes
bypassed the native transition that merges an authored attack back into combat
idle.

The client now treats the host action journal as semantic input to that native
state machine. A new Tal action sequence is mapped to exactly one native
weak/strong/sweep/block pulse on the exact leased arbiter. Presentation remains
client-local, while an authenticated inline guard at `ApplyDamage` RVA
`0x000D21D0` suppresses client combat consequences; the canonical host remains
the sole HP and world authority. The lease waits for the host-observed native
selector and then lets Sudeki retire naturally to selector `17`. A bounded
timeout restores the low-level snapshot presenter if native admission fails.

A live WSW sequence proved selectors `50`, `53`, and `68` were admitted on the
client and that selector `68` retired natively to selector `17`. Visual
acceptance confirmed the previous end snap was gone. This is the preferred
architecture for future player actions: transmit authenticated semantic
results, replay presentation through native client state machines, and guard
all client-side gameplay consequences instead of trying to network every
private animation blend field.

### 2026-09-04: the dual-instance slow clock is NVIDIA WineD3D throughput

Fresh exact LAN instances disproved a network or game-speed cause for the
roughly `0.2x` native animation cadence. Under the NVIDIA WineD3D/OpenGL path,
a rate-12 Tal animation advanced only `28.8` units in 12 seconds (`2.4/s`).
Both native speed globals remained exactly `1.0`, neither CPU nor disk was
saturated, and moving the two windows onto separate physical outputs did not
change the result. The main game threads spent their idle time behind Wine's
NT synchronization while the WineD3D command-stream thread waited in DRM
synchronization. This is render throughput feeding Sudeki's capped frame
delta, not a replicated animation clock running slowly.

The controlled Mesa software run advanced the same rate-12 animation by
`147.372` units in `12.016` seconds (`12.26/s`). Its client packet sequence
also rose from about `5/s` to `16.4/s`, while presentation generations rose
from about `10/s` to `146/s`. The loopback launcher now exposes the explicit
`SUDEKIMP_LAN_GRAPHICS_BACKEND=wined3d|software|dxvk` choice. `wined3d`
remains the default pending live DXVK acceptance; `software` injects the two
Mesa software-rendering variables into both game processes; and `dxvk`
verifies and transactionally installs the exact GE-Proton11-3 32-bit D3D9 DLL
after the normal runtime seed. DXVK pins both `wine` and `wineserver` to that
same GE-Proton build. Its preflight parses the NVIDIA i686 ICD's
`library_path` and ELF32-checks both that library and the host Vulkan loader,
rather than accepting any filename containing `i686`. The D3D9 override uses
role-local verified backups and atomic target replacement, restores WineD3D on
success, failure, or interruption, and retains its sole recovery copy until
the restored target checksum passes. Host/client DXVK log and state-cache
directories remain separate. The preceding baseline seed also stages,
checksum-verifies, and atomically renames every DirectX runtime DLL; it never
truncates a live target in place. A plain `--stop` probes at most eight
deduplicated pinned, ambient, or installed GE wineserver runners across the
two exact prefixes, so stopping a DXVK run does not depend on repeating its
backend environment variable.

The first DXVK run proved that this host's Vulkan path itself is viable: the
preserved host log selected `NVIDIA GeForce RTX 3050 (NVIDIA 610.43.3)` and
created its cache. The client failed later during exact LAN runtime
initialization, before producing a DXVK log. Therefore this is not yet live
two-process DXVK acceptance. The launcher archives existing console, runtime,
and DXVK logs before a later start can overwrite that evidence. The original
failed-run bundle is preserved at
`build/mingw32/lan-loopback/evidence/20260904T104505Z-3295298-28631/`; its
archived and source host DXVK logs both hash to
`8ef07b951a6586bb81385b38fde371aec9290850565518df827ee8e36714d749`.

The monitor audit identified DP-2 (`Acer X233H`) as
`1920x1080+0+360` and HDMI-A-1 (`LG FHD`) as
`1920x1080+1920+360`. KScreen and XWayland currently report scale 1, so the
launcher's observed 2x configure compensation is a Wine/KWin event rather
than a persistent desktop scale. A client window could still return to the
host output after the initial move because Wine completed another asynchronous
top-level configure during its 12-second D3D warmup. The harness now reasserts
the same compensated product placement after that warmup, logs each final
window geometry against its intended output, and warns without stealing focus
if a window remains outside that output.

### 2026-09-04: LAN Spirit client presentation deadlock and recovery

A bounded LA22 Tal-host/Ailish-client trace reproduced a presentation-only
deadlock. The host completed Tal's Spirit selectors `75 -> 114 -> 17`; the
client applied `75` and retained its terminal pre-vanish frame. Ailish
transforms continued advancing, but her renderer remained unavailable. Combat
telemetry reported Tal ready and Ailish not ready while transport and native
world time remained healthy. This was log/state evidence, not visual
acceptance.

Read-only supported-image analysis identified the initiating failure. The
ranged presentation wrapper at RVA `0x001888f0`, through the model switch at
RVA `0x00188a90`, resets animation channels before discovering that the desired
world model is already attached and returning zero. In the observed fallback,
`component+0x164` is null while `CPosition+0xb4` owns the world wrapper,
distinct from the first-person wrapper at `component+0x160`. Treating that
valid fallback as a failed one-shot transition left Ailish T-posed and kept the
shared readiness gate pending.

The replica now classifies the model topology as desired, opposite, or unknown.
Desired skips the destructive model switch, opposite permits it only with exact
writable ownership proof, and unknown fails closed. Native weapon reattachment
at RVA `0x000d8280` and `SetWeaponVisible` at RVA `0x000d7e30` remain required.
Admission proves actor/component/arbiter/model backpointers, the primary-parent
invariant `weapon+0xd4 == position+4`, locator and wrapper identities,
executable model/callback methods, writable mutation targets, and exact
postconditions. Recovery is scoped to one combat transition and bounded at 20
attempts separated by 100 ms.

Tal readiness is independent and survives only while its authenticated session,
transition, renderer, actor address, and runtime lifecycle generation remain
exact. Ailish recovery therefore cannot collapse an already valid Tal Spirit
presentation. Native action and `CSkill` barriers are armed before reentrant
native entry and drain positively before actor removal. Tal's asynchronous
spawn, pre-active PlayerTwo request, active generation, and asynchronous remove
are separate ordered leases; an unpublished spawn or pending removal cannot be
forgotten, duplicated, or reclaimed.

Automated verification against dirty HEAD `53623a0a2ad4` comprised the PE32
build, the exact supported-image `SkillTraceImageTest`,
`LanArenaRuntimeHookTest`, and the LAN protocol, replica, shared-simulation,
skill-handoff, Spirit-audio, owner-view, session, combo-graph, cleanroom, and
non-caster locomotion regressions. All passed on 2026-09-04.

Live evidence `LA22-20260904-ffa1abb913d4a1fc-software-logstate` used the
supported image, `testroom`, Tal/canonical host, Ailish/replica client, and the
Mesa software backend. After six bounded fail-closed observations while the
native weapon graph settled, combat reached
`client_combat_presentation state=ready` with Tal selector `17` and Ailish
selector `20`; Ailish movement then produced selectors `22,23`. Spirit sequence
1 produced client transitions `75 -> 114 -> 17`, exactly one authenticated
`spiritstrike_start` replay, and no retry exhaustion, model-switch rejection,
timeout, crash, or fatal event. Both game processes remained alive after the
sequence and the harness subsequently stopped both. The current evidence is in
`build/mingw32/lan-loopback/host-runtime.log` and
`build/mingw32/lan-loopback/client-runtime.log`. This is log/state-only
`CONFIRMED_LIVE`; the raw logs were preserved under local evidence key
`20260904T160725Z-2-30419`. It does not establish visual appearance, reconnect
behavior, or native teardown acceptance.

Visual Spirit VFX remain deferred. `CSpiritStrikeManager::FireSoul` owns
manager state and dynamically selected resources whose collision, damage,
script, camera, time, and lifecycle behavior is not closed. LA22 adds no raw
resource identifier or visual-effect event.

### 2026-09-04: LAN Tal opening Spirit VFX preload and replay

Claim: the fixed client opening effect reaches native playback during the
matching Tal Spirit animation, and the user confirms its rings are visible.
Classification: `CONFIRMED_STATIC`, `CONFIRMED_TEST`,
`CONFIRMED_EXACT_IMAGE`, and bounded log/state-only `CONFIRMED_LIVE`.
Profile/revision: LA22 `testroom`, Tal canonical host and Ailish replica client,
DXVK, dirty HEAD `53623a0a2ad4`; tested DLL SHA-256
`9d44b6ae07df0e5c74d7737469f6dfa0440e148edf24d330e77061932937d96e`.
Supported executable SHA-256 remains
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.

The earlier VFX deferral above is narrowed only for `SFXSS250_Initiate`.
The exact native/resource closure and exclusions are documented in
`research/signatures/spirit-strike-presentation.md`, under "Fixed opening
VFX". A separate cursor maps the existing authenticated START journal to this
fixed finite effect. Replay requires the same session, Tal runtime generation,
active Spirit sequence, actor-local combat readiness, selector `75` with two
channels, and successful visible-transform publication before RenderStart.
Late events are skipped. No resource identifier, native address, camera, damage,
or new VFX event is added to the protocol.

The first live attempt exposed a preload identity error missed by seam tests.
The extensionless script name hashes to `0x15fef04d`, which has no backing
archive entry. Native PreCache at RVA `0x00019540` uses that identifier
directly; unlike typed graphics loading it does not append `.hom`. The client
therefore held one reference with neither a pending request nor a loaded
object, while the animation advanced and the late VFX event was skipped.
The corrected fixed request is `SFXSS250_Initiate.HOM`, identifier
`0x3cef3b8f`. Construction rejects any other identifier; the generic resource
helper and strict loaded-without-pending readiness predicate are unchanged.

One native pre-cache reference is owned per exact manager identity. Replay
consumes its START on native call entry, even if subsequent observation fails.
A same-manager unconfirmed post-call release remains `RELEASE_PENDING` and
must complete before reacquisition; it cannot replay or silently abandon the
reference. Native effects retain their own resources and retirement ownership.

Verification: the full PE32 build, `LanArenaSpiritVfxTest` against the supported
image, `SkillTraceImageTest`, and `LanArenaRuntimeHookTest` passed. Focused
coverage includes wrong basename identifiers, acquired-reference accounting,
pending/ready/poisoned states, reentrant callbacks, retained release obligations,
and no replay before successful visible publication. The mapped-image fixture
also relocates both GetSound and GetSFXManager operands, matching the loader;
production image validation was not weakened to accommodate that fixture.

Live procedure/result: restart both DXVK instances, enable combat, and observe
one accepted Tal Spirit sequence. The client logged cache loading then ready,
one START/pending event, exactly one `spirit_vfx state=fired` at selector `75`,
cache loading then ready again, and animation recovery `75 -> 114 -> 17`.
The host's sequence completed. A further operator request during the active
native Spirit was correctly rejected, not treated as a second cast. A
non-pausing read-only post-cast observation confirmed the unique cache entry
`0x3cef3b8f` had loaded storage, no pending request, and reference count one.
Both game processes remained live; no disconnect, timeout, or crash was observed
in this bounded sequence.

Evidence snapshot: `20260904-spirit-vfx-hom-aal3VC`, with host runtime SHA-256
`192fa8401884d777926245bfa3677adab86eb942ef9941fc1a904af0e20c10cf`
and client runtime SHA-256
`a507780143e312b4f7fa37e1d78e7205a236f0198655098be456605cfc6f3368`.
This trace proves native call entry and cache accounting. In the same run the
user separately confirmed visible client rings, slightly later than the host,
and reported that the other Spirit effects remain absent. This is bounded
visual `CONFIRMED_LIVE` for the rings only, not full summon/effect parity.
Precise timing, repeat casts, effect retirement, disconnect/reconnect, native
teardown, and two-machine acceptance remain unproved.

Follow-up inspection corrects two preliminary resource claims: the last sound
cue at frame 40 does not establish the opening clip's duration, and its
floor/logo/pattern/cylinder mesh inventory does not establish the overhead
summon. The later variant resources contain the articulated summon mesh.
The duration field remains unverified. Later resources also include authored
attack events, visibility events, camera-related nodes, and manager-owned
moving effects; generic replay of the full Spirit transaction is not admitted.

LA22 currently omits Spirit variant, target/soul identities, and actual effect
emission timestamps. Its START-only journal requires a newer Spirit sequence
per entry and consumes only the newest event, so it cannot represent multiple
phases of one Spirit unchanged. The opening replay waits for the accepted
snapshot tick on the client render timeline rather than the native emission
time. These are concrete timing/coverage limits, not transport loss. A future
host-observed visual event format needs an owner-approved protocol change and
separate presentation-only resource/lifetime closure before implementation.

### 2026-09-04: owner-approved LA23 Spirit visual roster

Evidence level: `CONFIRMED_STATIC`, `CONFIRMED_TEST`, and
`CONFIRMED_EXACT_IMAGE`; full visual acceptance remains `UNKNOWN`.
Profile: experimental LAN arena, canonical Tal host and Ailish replica, exact
supported executable. Revision: `53623a0a2ad4ace2622f71334c121148552bd172`
with uncommitted work. The owner approved the breaking protocol change before
implementation; host gameplay/damage authority is unchanged.

LA23 preserves the START-only audio journal and adds eight bounded 56-byte
visual records. Each carries instance and cast identity, semantic resource
kind, native emission tick, position, orientation, scale, and animation phase.
Maximum encoded snapshot is 1196 bytes within a 1200-byte datagram. No native
pointers, cache handles, resource names, or gameplay events are transmitted.
Protocol/shared-state tests cover malformed fields, clock/sequence wrap,
immutable identity, complete removals, UNKNOWN retention, and session reset.

The host observes the unified native SFX finalizer at RVA `0x18830`, including
matrix and soul paths outside PlaySfx. Stable intrusive weak nodes use native
assignment at `0x1750`; native base destruction nulls them before free. A
32-entry local registry reports UNKNOWN rather than truncating a roster above
eight. Native sampling follows composed CPosition world matrices and proved
renderer channel storage. An inactive Spirit baseline is required on a new
session; joining midway through a cast does not fabricate an empty effect set.

The client now services the visual roster after visible actor publication.
Matching instances interpolate on the existing render clock; births obey
emission time and omissions retire at the complete-roster boundary. Missing
observations do not remove visuals. Parent-free clones avoid native event
forwarding to actor/gameplay listeners; authored numeric event types require
separate inspection, recorded in the Spirit presentation signature document.
Reset retains dependencies on failed retirement/unlink/cache cleanup, and
native-call barriers protect reentrant actor release. Actor replacement
fences the preceding cast while allowing a later authenticated cast.

Verification so far: protocol, shared simulation, replica interpolation,
runtime lifecycle, opening-cache and exact-image suites pass under MinGW/Wine.
The new host suite additionally executes the exact finalizer hook passthrough,
logical reset/rebind, weak insertion/unlink and native weak-null destruction
against isolated fixture objects. These checks do not prove client pixels,
complete phase coverage, repeated live casts, or two-machine latency. The
prior live evidence remains rings-only until the LA23 acceptance run.

Commands for the combined check (using a configured isolated Wine prefix and
the user's supported executable, not a committed game fixture):

```sh
tools/build-linux.sh
for test in LanArenaProtocolTest LanArenaSharedSimulationTest LanArenaReplicaTest \
    LanArenaRuntimeHookTest LanArenaSpiritVisualHostTest LanArenaSpiritVfxTest \
    SkillTraceImageTest; do
    wine "build/mingw32/bin/SudekiMP.${test}.exe" '<supported_SUDEKI.exe>' || exit
done
```

Expected and observed: all seven selected targets exit zero with PASS/passed;
this is a selected regression set, not the entire repository test collection.

First LA23 live attempt used DLL SHA-256
`d0d2cad37dcb3793d78a4a357b020014739e80aaaa1e2e949fac97de33cec915`
and the existing two-instance DXVK profile. Both peers connected and all eleven
client cache requests reached ready. Four manual casts were observed: three
variant-one summon resources (kind 8), then one variant-two resource (kind 9).
The host roster captured those native lifetimes and later removed them; the
client logged one successful factory result per instance. This is native-call
and cache `CONFIRMED_LIVE`, not visual parity.

The run exposed two gaps: host capture missed the other opening/transfer
resources, and the client rejected synchronization and retirement with error
13. Non-pausing read-only inspection isolated the latter to an incorrect
zero-listener assumption: each factory-created effect has one built-in sound
listener. The owned entity, weak links, resource identity, wrapper/interface,
model and clip checks otherwise matched. Failed cleanup correctly retained
the native weak/cache obligations, but repeated casts accumulated visuals.
The test processes were therefore closed through the scoped harness; this is
not evidence of successful in-process visual cleanup. The sound-listener
contract requires correction and a fresh live acceptance run before claiming
that the effects or retirement are resolved.

First-attempt log hashes: host
`6e2fc8cfba0fd17a94a46e9705570d122290be45356c35297b9011b8d4700908`;
client `86d26b0fd077a314f46b80e16098cac35e29d9d672ee144e374dfa11cbbc72be`.

The owner subsequently confirmed that the overhead summon appeared on Ailish's
client as Tal moved/vanished, but the floor circles/sigil were absent and the
summon replayed indefinitely after Tal returned. This adds bounded visual
`CONFIRMED_LIVE` for a client summon, together with confirmed missing-opening
and retirement failures; it does not prove either variant's complete sequence.

The corrected listener build, DLL SHA-256
`3952e957ba0fe6b4255531e7d0ac5f0cefdeae1aa737733ae6ce95b6be804b09`,
passed the same seven selected targets and was launched with matching peers
on the same DXVK profile. One operator-driven variant-one cast produced one
client summon spawn and one retirement entry, with no synchronization rejection
or pending retirement. Non-pausing read-only native inspection then found all
eight local visual slots and weak observers cleared and the native retirement
queue empty. This is bounded `CONFIRMED_LIVE` for the corrected native cleanup
path; user-visible disappearance and repeated-cast acceptance remain separate.
Closed-run log hashes: host
`8e3ca18d3f36d6c9b15e53de7118995db51f6c5b785b3dd0724b49b2c13e1ef6`;
client `172b36003c171c3df28d4e2157dac7d75cada165fe62c68d2b34c021bbd59c84`.

The host still omitted the opening. Bounded finalizer diagnostics identified
the actual request as `SFXSS900_generic_initate.HOM`, identifier `0x62dcc5a3`
(the spelling is authored), rather than sample-script resource 250. A second
unrecognized request was generic hit resource `SFXHT201_Hit_Magic.HOM`,
identifier `0x94b4876b`; it is not admitted merely because a Spirit cast is
active. No inactive-cast-witness rejection occurred in this cast. This narrows
the missing opening to resource coverage, not an observed late state witness.

The fixed retail opening was then admitted as semantic kind 12 without changing
existing kind numbers or packet layout. Its selected clip has duration 180
native units and one numeric type-22 sound channel (keys at 0, 7, 45, 55, 80),
with no type-23 global event or actor/gameplay channel. Full indexed resource
SHA-256 is `82bdcc5ed28e5e245ff0497ba5319e8af1f93f879bde4f91fcb77d2e37d0f955`.
Host and client retain fixed local name/hash mappings; no packet-controlled
asset string is introduced. This is static admission evidence, not yet visual
proof that the missing rings have returned.

Actual serialized Tal state resources differ from the sample Spirit script:
charge uses 900, morph-in uses 300, morph-out uses 350, and wait/receive name
teleport-out/in resources. The observed variant-one cast finalized only the
900 opening, 300 summon and generic magic hit above. The sample
250/251/112/800/252/801/802 chain must not be described as a confirmed retail
cast sequence; neither hypothetical teleport emissions nor unrelated generic
hits are admitted without their own bounded evidence.

The retail-opening build, DLL SHA-256
`bfc366e5486b0c8e930afa24220143da4030f7ff14771ae0193aa4dd8ab0090f`,
passed the same seven selected tests. Three operator-driven live casts ran in
order: variant one, variant two, variant one again. Each produced one opening
kind 12 plus one summon (kind 8, 9, 8 respectively). Client instances 1–6 each
logged exactly one spawn and one retirement, without synchronization rejection
or pending cleanup. Non-pausing post-cast inspection found cleared client slots
and an empty native retirement queue after each cast; after the third, the host
registry and both native queues were also empty. This establishes repeat-cast
native lifecycle behavior for these two visual components, not pixel/timing
parity or complete coverage.
Closed-run log hashes: host
`76061b3443f74f5cb7fcd4773f53eb9fcfd68ff0eec5aa7808e8c6a9596120f6`;
client `c6ff06c71a2db2f80aea80c855a6fe23182377b3ebda7fc35df7488f9b80b1b2`.

Variant two additionally emitted a Tal-specific secondary effect through a
typed extensionless request: `SFXSS351_Tal_Hit_Character`, identifier
`0x6696ab0a`. Its canonical `.HOM` backing identifier is `0xaeec0c83`.
Other skipped requests mapped to generic material miss and status-boost
resources. These observations identify remaining coverage gaps; generic
effects must not be treated as Spirit-owned solely because the global Spirit
transaction is active.

The Tal-specific secondary asset was admitted as fixed semantic kind 13 after
inspection: duration 180, one type-2 sound channel, no type-23 global event or
gameplay channel. Its full indexed resource SHA-256 is
`4b317e5009b49a3a0e861d4d0ddaf4c25200b6eea4ac15bc90eb3fdabe8da99a`.
The combined DLL SHA-256
`4f16a50502ae81cd32bcb5147f3ef4ba02e46bbf732d88e064ad6884c7119ac6`
passed the same seven selected tests before the next live run. Typed-name
normalization accepts only the exact fixed spelling/hash, and native cloning
uses the canonical backing resource; packet layout and earlier kind IDs remain
unchanged. Static admission and focused lifecycle tests alone do not establish
its visible world position or native parent-to-world behavior.

That final build then completed one operator-driven variant-two cast on the
matching DXVK pair. The host observed opening kind 12, summon kind 9, and two
kind-13 character-effect instances. The client spawned and retired each of
those four exactly once, without synchronization rejection or pending cleanup.
A bounded six-second, non-pausing dual-process inspection after the cast found
an empty host visual registry, no client-owned visual slots, and both native
retirement queues empty. This is `CONFIRMED_LIVE` for the bounded observed
lifecycles, including secondary effects that outlive the Spirit transaction.
Active pose/phase comparison was not captured, and no new user pixel/timing
confirmation had been received. Both games were left open for that acceptance.

Remaining generic magic/material hit and boost resources have safe-looking
clip event inventories, but the present global cast witness does not prove
their origin. They remain excluded rather than attributing unrelated effects
to Spirit by timing. Complete VFX coverage, latency quality, movement smoothness,
and disconnect/reconnect acceptance are not established by these checks.

### Client opening-orb phase rewind investigation

The owner subsequently reported that the attack was largely present, but its
eight rotating opening orbs were smooth on the host and severely flickered on
the client. Eight visible orbs is the owner's visual reference, not a count
inferred from emitter names. The fixed opening asset contains four named torus
emitter paths, three sphere paths and eight other emitter paths; emitter-node
count must not be confused with visible particle count.

On the same dirty revision and `4f16a505...` DLL above, non-pausing read-only
sampling of one user-triggered opening (instance 25) captured 1456 host samples
over 7.527 seconds and 1459 client samples over 7.542 seconds. Both reported one
animation submodel with no rejected identity reads. Host absolute phase advanced
from 0.384 to 179.9999 with 450 observed changes and zero backward steps. Client
phase advanced overall from 0.384 to 179.8805, but 204 of its 553 observed
changes went backward, including -0.024 and -0.312 native-unit steps. Existing
logs showed one opening spawn per cast, not repeated creation. This is
`CONFIRMED_LIVE` for client clock rewinds; their causal contribution to visible
flicker is a strong `INFERENCE` pending an A/B visual check.

The adapter had applied the interpolated host phase before RenderStart every
frame while native effect playback also advanced. The targeted correction now
allows only forward phase seeks for retail opening kind 12. Equal/older targets
leave native playback untouched. Other kinds retain their absolute phase and
loop-wrap behavior. Native identity checks and complete-roster retirement are
unchanged; this policy neither extends a cast nor authorizes client gameplay.
Forward-only progress can run slightly ahead of the interpolated host clock,
so exact phase equality is not claimed and visible end timing still needs
acceptance.

DLL `4568aaf0ca984fd4d25d69b050c544f18cc01ff0cbadfed4ee2e44889427fc9c`
passed the same seven selected suites, including new observed-rewind,
forward-catch-up, repeated-target, native-progress, tail, other-kind-wrap and
invalid-input policy tests. Graphify was refreshed after the code changes.
Pre-correction closed-run log hashes: host
`f1be942a933849b6fcfde06352aef4c07550db44be2b1973e0a6460998b3f4ce`;
client `864e845eb7597b617705f9eeeefb20b1f0d5b8098ad9fbc0375891902702c901`.
Corrected live timing and visual acceptance remain pending at this checkpoint.

The subsequent corrected user cast (opening instance 3) captured 1454 host
samples over 7.512 seconds and 1460 client samples over 7.542 seconds, with
zero backward phase steps on both peers and no rejected identity reads. Host
phase ended at 179.9999; client phase ended at 181.9196, consistent with native
progress continuing until the interpolated host removal. Thus monotonic
progress is proven, not exact host/client phase equality. The corrected client
opening and summon each logged one spawn and one retirement without pending
cleanup or rejection. The owner explicitly confirmed that the eight rotating
orbs' flicker was resolved. This adds bounded visual `CONFIRMED_LIVE` for the
client opening correction in this DXVK LAN profile; it does not establish
generic effect coverage, adverse-network timing or disconnect behavior.

## 2026-10-10 — #54 C1 native auto-target candidate

B2's zone/loading/lighting gate is accepted by the owner based on the testing
to date; no newly instrumented round trip is claimed. Work proceeds to C1.

**CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE:** the supported executable's
CTargeter dispatcher retains a bit-1 target under the remote actor's AI
override. Its ordinary melee leaf `0x4BA1C0` instead scores native candidates
against the targeter's accepted heading and commits through `0x4B9D20`.
The controller call at `0x428031` is a turn call, correcting that research lead
in the plan. The native contract and fixture limitations are recorded in
`docs/engine-functions.md` and `tests/story_target_refresh_image_test.c`.

C1 runs the native melee leaf once per leased remote actor's ordinary-world
control update, before body-idle movement filtering. Full current actor/key,
generation, thread and roster ownership remain mandatory; disabled targeting,
ranged, UI, skill and invalid-body states are skipped. Neutral frames preserve
the accepted heading. No new native patch, AI/lead/controller swap, lock-on
flag change or targeting algorithm is introduced. C2–C4 remain separate.

DLL build, `StoryTargetRefreshImageTest` and `StoryInteractionGuardImageTest`
passed on dirty `decbbc5`. The target regression reproduces the old retained
opposite target and exercises real native selection, replacement and release;
its world enumeration/position preparation/vector normalization are fixtures.
The new acquisition/commit/exclusion byte contracts were checked by name, and
every byte was independently corrupted to verify rejection. No visual parity
or teardown/reconnect result is claimed; the owner combat gate remains open.

The first C1 startup refused control because its added guard compared the
observer world (`0x808D10`) to the distinct collision system (`0x808DD4`).
The corrected guard proves both identities separately and bounds the collision
registration vectors; the fixture now models distinct objects and tests refusal
of missing/malformed collision storage. That regression passed. The corrected
pair reached client input readiness and logged successful native target refresh
for Tal (**CONFIRMED_LIVE**, startup/call completion only). C1 combat acceptance
and quantitative swing-direction results remain pending.

## 2026-10-10 — #54 C2 leased player turn candidate

Profile: private saved-story LAN, host authoritative remote Tal. Base `decbbc5`
with existing uncommitted prototype and C1/C2 changes; DLL SHA256 `d6fa911cce14057a51148d70f3828ad7e2555c00c84f10d544ed3fa7124c288b`.
Supported executable SHA256 `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.

**CONFIRMED_STATIC:** the remote AI hold affects both `0x4B73D0`'s choice of
turn step and its `0x4DBB20` CanTurn gate. Ghidra caller/branch inspection and
exact-image disassembly establish the register/stack contract; see
[the native contract](engine-functions.md#saved-story-leased-player-turning-54-c2-2026-10-10).
Use two scoped branch decisions rather than transient writes to the native
reference count. Movement inherits player CanTurn only inside the mod's own
call. C3 AI movement and C4 input-during-attack behavior remain separate.

**CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST:** DLL build,
`StoryPlayerTurnImageTest`, `SkillTraceImageTest`, `StoryTargetRefreshImageTest`
and `StoryInteractionGuardImageTest` passed. C2 fixture executes native player
interpolation, restores register/FP/SSE state, tests game override and native
attack gates, transactional install rollback, failed restore/retry, and
coexistence with the Research54 turn callsites. Named validators were checked
alongside `party_native_entries_exact` / `RVA_ARBITER_MOVEMENT`,
`RVA_ARBITER_COMBAT_INPUT` and existing turn-facing observers; no validator
was weakened to admit a patch overlap.

An additional `StoryMeleeAdmissionTest` run fails its available-interaction
BUSY expectation at line 68. The exact same failure was reproduced by building
and executing the preserved pre-C2 wrapper source with the unchanged fixture.
It predates this change and conflicts with the existing restorable interaction
behavior. Do not report this suite as passing or change gameplay to satisfy it.

**UNKNOWN:** live encounter turning parity and C2's zero unexpected AI-turn
criterion await owner play-test and Research54 review. C1 quantitative facing
checks are deferred by owner request to issue #56; qualitative acceptance
does not prove those numbers.

**C2 live follow-up:** candidate reached client input readiness after the
owner-requested reload. A bounded settled Research54 slice recorded
820 remote Tal player turns and zero AI turns across
7 complete windows (14005 ms), with count1/mode0
retained. Earlier/mixed windows are excluded. This is live native routing
evidence, not an encounter-wide measurement or owner visual acceptance.
The earlier blank-start host was observed waiting in Wine X11 pointer/focus
capture; it later progressed without a C2 code/config change.


### 2026-10-10 — #54 C3 AI movement candidate

Dirty revision `decbbc5` plus preserved prototype and C3 changes. Supported
EXE SHA256 `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`;
DLL SHA256 `f7b333d1ae16c00aed5316f7bf5c6dfb98fcbcc30ee87441264680e7cf5b7615`.

**CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE:** the AI movement update mixes
command submission with fade/state bookkeeping. C3 redirects only its movement
decision to the existing native player-cache path, and gates navigation-adjusted
placement separately while retaining native cleanup. Script-style placements
stay native. Native addresses, ABI, request-flag caller evidence, image guards
and lease scope are documented in `docs/engine-functions.md`, C3 section.
The alternative of skipping the whole node was rejected statically because it
would suppress fade progression. No such candidate was launched.

**CONFIRMED_TEST:** DLL build and StoryAiMovementImageTest passed. The new
mapped-retail fixture covers movement/stop/snap suppression, native fallback,
fade completion, intrusive cleanup, observer coexistence, machine state and
install/restore failure paths. StoryPlayerTurnImageTest, SkillTraceImageTest,
StoryTargetRefreshImageTest and StoryInteractionGuardImageTest also passed.
These are selected tests, not a complete suite; the pre-existing melee-admission
fixture debt noted under C2 remains unchanged.

Research54 adds `ai_move_bypassed` and `ai_snap_bypassed` counters through an
observation callback. It remains read-only and owns its existing callsites.
Only the saved-story host control adapter enables C3; binding still requires
the witnessed native update boundary. C4 attack movement is unchanged.

**UNKNOWN (live acceptance pending):** reduced AI SetSpeed traffic and visible
control improvement on the saved-story Ailish-host/Tal-client pair. Owner must
check moving/stopping and opposite-side attacks through the ambush and later
waves. Numerical swing parity remains in issue #56. No baseline commit or
unrelated staging; stop at C3 owner gate.


**CONFIRMED_LIVE (C3 startup/instrumentation only):** both roles loaded the
candidate DLL and the client reached acknowledged `input_ready=1`. Across
20 complete settled Research54 windows (40038 ms), after excluding the first
mixed admission window, Tal recorded 6101 AI movement bypasses, zero AI
SetSpeed calls and zero AI move calls. C2 also recorded 6101 player turns and
zero AI turns in this slice. No AI snap bypass was recorded in this slice, so live snap suppression
remains unproven. This is bounded startup
instrumentation, not an encounter-wide measurement or visible smoothness proof.
The client initially waited before its lobby; it progressed after the verified
host window was temporarily minimized and client focus restored, without a
code/config change. Both test windows were restored. **C3 owner gate is ready;
visual acceptance is pending. Stop before C4.**

### 2026-10-10 — Story combat feedback: monster blackout and casting bind

**CONFIRMED_STATIC / CONFIRMED_LIVE (diagnosis):** an Ailish ranged-pose
capture failure armed a global three-second monster exclusion even though the
failure was not an enemy. Matching host/client gap bursts coincide with both
Grotesque terminal sequences. Removed this timed exclusion; capture retries
retain all existing native validation. D4's transform-only fallback remains
open. The explosion's exact emitter/terminal transform is not yet established.

**CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST:** story sky's rate-setter detour
invalidated cast-light's `light_image_exact` check at `SetLightingChangeRate`,
RVA `0x1F740`. The new coexistence regression fails before removal and passes
afterward, without relaxing the guard. Target/rate state replication and the
client restoration lease remain. See the named contract in
`docs/engine-functions.md` and detailed evidence/limitations in
`docs/coop-story-plan-54.md` (C3 play-test combat reports).

The DLL and four focused tests pass. **CONFIRMED_LIVE (startup only):** the
candidate now binds the host's sparse cast namespaces and reaches client input
readiness. Ordinary skill selection, both-screen effects, beast alignment and
enemy timing still require owner checks. Remote Spirit submission is absent in
the story adapter and was not enabled. C3 visual acceptance remains pending;
C4 has not started. Exact candidate identity is in the plan's progress record.

**Additional CONFIRMED_LIVE:** one client Q skill selection reaches the host
activation adapter and is rejected by native validation 2 (actor not ready),
with no `Use` call; retained task routing subsequently drains. The exact failing
readiness predicate is unknown. This is stronger than startup binding but still
does not establish successful Skill Strike gameplay or both-screen effects.


### 2026-10-10 — Saved-story remote Spirit submission and asynchronous root

Implemented contained Q Spirit selection, actor-specific variant mapping,
authenticated host dispatch, native SSP feedback and retained body/task/effect
lifetime. Focused menu, transport, activation, ownership and exact-image tests
pass. The first owner run starts Tal's Spirit but freezes him and drops native
frame rate; it is explicitly not accepted. The ordinary skill attempt passes
readiness but fails native SP validation.

The delayed Spirit power-up root at 0x10DE5 must use the accepted cast's retained
manager scope after request authority expires. Its regression fails before and
passes after the correction. Native readiness helper 0xD0250 reads actor+0xA4
combat state, not arbiter state; exact-image inert tests exercise that contract.
See docs/engine-functions.md for native contracts and the co-op plan's Tal skill
readiness / Spirit retention entry for candidate identity and the pending owner
gate. No completed ordinary cast, Spirit completion or lag recovery is claimed.

### 2026-10-10 — Spirit control recovery and remaining visual slowdown

The next owner run confirms post-Spirit movement returns; native retained
root/task/body drain is also observed. This supersedes the preceding
completion-unknown status only for that tested action. Full client presentation
and performance still fail owner acceptance. A client material-control refusal
coincides with summon onset; its exact property was not present in the old
trace. Controller service and pose capture do not account for the slower native
frame interval.

Verified inline native material color/alpha outputs were missing from preflight.
The new exact-image regression fails before their addition and passes after,
without permitting shader definition replacement. A second narrow change
avoids namespace swapping for the exact native script timer-yield prefix,
retaining full routing for executable tasks and ownership failures. Tests
exercise the literal prefix and boundary cases. New bounded observations expose
refused property arguments and scoped task cost for the next play-test.

See the material/script contracts in docs/engine-functions.md and the plan's
Spirit movement/visual entry for evidence levels, five passing focused checks,
candidate identity and the outstanding owner gate. This is not yet a claim of
full Spirit visualization, smooth frame rate or ordinary Skill success.


## 2026-10-10 — Saved-story Tal Spirit presentation and scoped roster proof

Supported executable SHA256:
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
Dirty decbbc5 candidate; see the C3 progress entry in
[the co-op plan](coop-story-plan-54.md) for owner evidence and acceptance limits.

CONFIRMED_STATIC: saved-story previously retained native Spirit descendant
lifetimes but did not publish their cosmetic roster. The existing observer
seams Finalize18830, animation emissionE2810 and script parent call18F2B are
shared with lifetime tracking. Saved-story now arms its own retained Tal
witness after a positive inactive baseline, without legacy arena seat discovery.
Client parent-free SFX clones reuse the established renderer and exact image
resource closure. Cosmetic rows contain no native pointers or script requests.
Authenticated optional message36 carries area/scene/stream/sequence fences;
unknown, positive empty and populated rosters remain distinct. Failed cleanup
retains dependencies. Client lifecycle/zone behavior still needs live exercise.

CONFIRMED_EXACT_IMAGE: D3DMaterialGetProperty202E40 calls2032C0 for an absent
shader constant. For members2,3,4,5, description+8 shifts are0,2,6,8. A nonzero
two-bit field branches to20347F and returns -1 without heap/native callbacks;
the getter returns NULL without flags/storage changes. Zero still reaches
shader definition replacement and remains refused. Selected and retained
original graphs must both prove admissible outputs. New exact guards cover
entry2032C0, return20347F and dispatch20348C with relocation checks. Inert retail
getter tests cover all members/field values plus mismatched original refusal.
The owner's arg0/member3 rejection alone does not prove which branch it used.

CONFIRMED_STATIC / CONFIRMED_TEST: EnterSpiritInstance/LeaveSpiritInstance
transitions use bounded graph checks, retained witnesses and banking writes;
no intervening native script executes. The approved story adapter now shares
one fresh full roster proof within each transition only. Tests reject stale
actor/generation/thread, recursive reuse and lost identity after script work,
and clear the proof on failed enter/leave. Existing full native component and
namespace validation stays active. Runtime timing separately records entry,
native script and exit costs. No measured performance improvement is claimed.

Ten focused checks passed as listed in the plan. Synthetic/fixture evidence
does not establish visual completeness, native area cleanup or owner smoothness.


C3 integration follow-up (same date): CONFIRMED_LIVE startup established host
cosmetic publication; the first client integration was refused at paused_roster.
CONFIRMED_STATIC: ClientPausedRoster calls ClientService, whose ordinary pause
check is valid at the presentation seam. The post-controller path instead
requires ClientResidencyCall to install its exact dispatch witness before that
same roster/pause preflight. Runtime now uses its fresh controller scene for
area correlation and acquires full native proof inside the residency call.
StoryVfxDispatchTest covers actual helper sequencing with fake native APIs,
including refusal/retry and preservation of unknown observations. This does
not establish rendered Spirit visibility or smoothness.

## 2026-10-10 — C3 Spirit startup pose, transition cost and missing client camera

Same supported executable and dirty candidate as the preceding entry; no
runtime change in this investigation. Owner reports Tal's model holds still at
cast start, client playback appears slower, and the authored Spirit camera
angles do not appear on the client. Testing used the existing host resource
and combat toggles before the encounter.

CONFIRMED_LIVE / CONFIRMED_STATIC: at startup the host changes Tal's base clip
to selector75. Client preflight refuses Tal's token2 material binding at
argument0/member3 and rejects the whole prepared world frame. The client
therefore loses presented-frame admission while host snapshots continue to
arrive. Cosmetic Spirit clones still arrive on their separate stream. Playback
admission returns after the host moves to the later strike clip. This accounts
for the missing startup pose; it is not evidence of a missing startup request
or of camera obstruction. The logged shader field requires the replacement
branch described in engine-functions.md, not the previously admitted NULL
branch. Existing checks remain intact.

CONFIRMED_LIVE diagnostic, bounded to this run: across42 logged one-second
windows, median combined task namespace entry/exit time is about54% of the
window; native step execution is about0.22% of the measured scoped time.
This establishes a substantial transition cost, not an exact rendered speed
ratio or proof that all client lag has one cause. Repeated neutral tasks also
use the transition path while cast routing is active. No check has been
removed or carried across script execution.

CONFIRMED_STATIC: saved-story camera_select retains a remote caster's native
InitCam/SkillCam selection through RouteSpiritInstanceRenderCamera, avoiding
selection into the host's own view. StoryVfx carries only cosmetic instances,
and StoryLocalControlPresent continues to construct the client's ordinary
orbit view. There is no caster-camera publication/application path in this
candidate. The host reports an active private Spirit camera during the cast;
client camera completeness is unresolved. Native obstruction decisions must
remain authoritative in any future view transport.

Next bounded implementation work: prove and support the exact reachable
shader-definition replacement and subsequent override lifetime, then test
startup pose acceptance; measure and reduce namespace transition work with
fresh ownership checks preserved; add owner-fenced native Spirit view playback
and restoration. C3 visual acceptance remains open; C4 has not started.


## 2026-10-10 — C3 startup material and transition-cost candidate

Dirty codex/shared-simulation at decbbc5, saved-story profile; DLL SHA256
`1c485e64a67fbafdc920a716d01127f9b5abcd81df0c81c8dcdeffff72154120`.
Supported executable identity unchanged. This implements the two bounded
corrections identified in the preceding investigation. Client Spirit camera
publication remains absent and is not included in this candidate.

CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST: admitted Tal's exact SOL member3
replacement and derived-definition cleanup, using the closed factory/cache /
reference/mapping proof described in engine-functions.md. All11 newly named
native identities reject a byte mutation and accept the original supported
image. Native inert-object execution covers creation, zeroed output, later
color storage, original preservation, deletion/cache unlink and retry. Wrong
ownership, mappings and borrowed expanded storage refuse. This removes the
known preflight refusal in the modeled contract; live startup pose remains an
owner check.

CONFIRMED_TEST: transition-local OS range caching reduces the same namespace
fixture's entry/exit VirtualQuery count from100 to12. Fresh identity checks,
thread checks, failed-leave retention/retry and changed memory permissions
remain active. No proof is reused across native work. Actual cast speed and
live namespace timings have not yet been measured for this DLL.

Build and11 focused suites passed: SpiritInstanceAbiTest,
StoryMaterialImageTest, StoryMaterialOutputImageTest, StoryCastAdmissionTest,
LanCastContextTest, LanArenaSpiritVisualHostTest, LanArenaSpiritVfxTest,
StoryEffectTransportTest, StoryResidencyCallTest, StoryVfxDispatchTest and
SkillTraceImageTest. Exact-image suites used the supported retail executable.
These checks do not prove rendering, camera parity or full material teardown
inside the game. C3 owner gate remains open; C4 has not started.


## 2026-10-10 — C3 full color closure, summon cadence and recovery candidate

Dirty codex/shared-simulation at decbbc5, saved-story profile; DLL SHA256
`cdc3ad8cbcfcc706812234728d54096ad097d022f1bc25947e0ef4a5667e2e62`. Same supported executable SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
The owner rejected the prior candidate's startup: arms remain frozen, summon
playback clips/steps, movement returns several seconds after Tal reappears,
and ending effects are missing. The previous member3-only correction was
insufficient; C3 has not passed and C4 has not started.

CONFIRMED_LIVE (diagnostic only): the next refused startup material output is
member4 with flags80000001, after the previous candidate admitted member3.
CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST: the complete closed member3..5 SOL
family is now proved, including all eight reachable definitions and six
output orders; see engine-functions.md and structures.md. Native getter,
factory, remapping and intermediate/final definition release execute on inert
owned fixture objects. Every tracked allocation is released. The eleven
named native routine guards accept the supported image and reject mutation.
No rendered startup acceptance is inferred.

CONFIRMED_STATIC: saved-story VFX publication was inside the100ms area snapshot
branch and client servicing had a50ms throttle. The new path publishes and
services at the controller cadence, bounded by15ms, independently of the sky
snapshot. Each packet still needs fresh native area/scene/lease evidence.
Successfully adopted duplicate packets no longer seek the same phase again;
resource/adoption failures retry, and unknown state retains native instances.
CONFIRMED_TEST: actual runtime helper fixture covers21ms publication and client
updates, duplicate suppression, pending retry and failed scene cleanup.
Host-roster lifetime authority and the wire format are unchanged.

CONFIRMED_STATIC / CONFIRMED_TEST: movement and basic melee use positive native
body/task drain without waiting for retained lighting to finish. New casts,
transfer and teardown still require full drain. This is a candidate correction
for recovery delay, not proof of its full cause. The melee fixture also had a
stale expectation from the earlier interaction-available fix: available+1D9
bit2 is admitted through the restoring native wrapper; pending+1D8 remains
busy. That fixture expectation was corrected without changing the production
interaction contract.

The retail Tal variant1 emits generic initiation and the Tal summon; absence
of a separate sample-script ending kind does not itself establish a lost
emission. A bounded read-only summon trace now records host/native phase,
clip duration and submodel count through the existing ownership proof. Ending
visuals, summon smoothness, actual input-recovery delay and missing native
client Spirit camera playback remain unresolved owner checks.

Build and12 focused suites passed: SpiritInstanceAbiTest, StoryMaterialImageTest,
StoryMaterialOutputImageTest, StoryCastAdmissionTest, StoryMeleeAdmissionTest,
LanCastContextTest, LanArenaSpiritVisualHostTest, LanArenaSpiritVfxTest,
StoryEffectTransportTest, StoryResidencyCallTest, StoryVfxDispatchTest and
SkillTraceImageTest. Evidence levels are CONFIRMED_TEST and
CONFIRMED_EXACT_IMAGE; native gameplay/rendering parity is not established.


## 2026-10-10 — C3 client-only opening freeze: authored texture field

Dirty codex/shared-simulation at decbbc5, saved-story profile; DLL SHA256
`86f1df631bd268533d099d4c7268dc9f745dbcc9dcacc5ae26e37dae841bc850`. Supported executable identity unchanged.
The owner still sees Tal pause at the beginning while the host animates.
CONFIRMED_LIVE (diagnostic): incoming world/party frames continue during the
client freeze, but material token5 argument0/member3 is refused with original
flags80000101 and zero constants. Presentation stays refused across the
opening and resumes around the change from native selector75 to114. This is
a remaining client material proof gap, not evidence of a missing host cast.
The prior three-color closure covered only originals without these additional
texture fields and did not fix the complete opening pose.

CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST: native expansion preserves authored
texture fields1 while changing only absent fields0 to constant3. The proof
now covers all eight original texture layouts and their27 reachable
definitions, maintaining exact texture identity/order and native allocation,
remapping, reference and deletion contracts. See engine-functions.md and
structures.md. Field2, unrelated flag families, texture relocation, array
aliases and borrowed derived constant storage remain refused. No guard was
relaxed and no gameplay/camera/script behavior was changed.

Build and three focused suites passed: StoryMaterialImageTest,
StoryMaterialOutputImageTest and SkillTraceImageTest. The eleven named native
shader checks pass exact-image and mutation refusal. The retail material
fixture exercises all six output orders for every original layout, checks
NULL texture outputs, preserves existing colors/textures and releases all
tracked allocations. This is not rendered animation acceptance.

C3 remains at the owner gate: repeat Tal's Spirit Strike and compare the full
arm-opening motion from its first frame on both screens, then repeat once.
Ending-effect and client Spirit camera parity remain unresolved. C4 has not
started; unrelated dirty work is preserved.


### Same-session correction: alternate SOL layout still held the whole world

The immediately reloaded 27-definition candidate was rejected by the live
check: Tal token4 argument0/member3 has flags80800501. Incoming frames still
advance while atomic presentation refuses the material graph. The owner also
observes the whole client world stop and resume with Tal at the next animation
phase. This corroborates presentation refusal, not delayed host publication.
The rejected candidate and its passing inert tests remain separate evidence.

Current dirty decbbc5 candidate DLL SHA256
`fd3971b332e40d1a7f43a5af38152e77cb1ce5af8f146b54e08840b08c1adc99` proves the same native SOL factory's two remaining
texture fields (bits4/10) and alternate static layout (bit23), preserved across
every expansion. CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST: all64 original layouts,
216 reachable definitions and six output orders per layout execute through
retail factory/getter/destructor fixtures with complete allocation release.
Optional descriptor contents and non-aliasing are checked; changed retained
layout/texture fields, unknown modes and borrowed storage refuse. No native
byte guard was relaxed. A bounded read-only inventory observes every Tal
original material before the first individual-output refusal can hide others.
Build and the same three focused suites pass, including all eleven named
shader native byte checks. These tests do not establish rendered parity.
C3 startup, repeat-cast and full-effect acceptance remain owner gates.


Final inventory follow-up: all seven Tal originals use four flag keys:
`80000001`, `80000101`, `80800501`, `80802505`. The last also retains bit13;
its member4 expansion was the remaining live refusal. Native factory202A30
preserves that bit in the cache key without changing descriptor arrays.
The proof now retains it exactly, including a negative test for substituting
it. The exact-image fixture covers128 original layouts,432 reachable
definitions and all six output orders, including every observed Tal key.
All tracked allocations are released; the same eleven named byte guards
remain unchanged. This supersedes the insufficient216-definition candidate.

Final dirty decbbc5 candidate DLL SHA256 `6de2a94f8ed64d817f30fed2196038af2d7748c922b757f6b014ce0eb180bf21`.
Build and the three focused suites pass. Inventory and refusal evidence are
CONFIRMED_LIVE diagnostics; the complete family tests are
CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST. Owner startup/world-motion acceptance
is still pending. Prior candidates were not accepted as resolved.

CONFIRMED_LIVE diagnostic follow-up on the final candidate: two owner-triggered
Spirit casts each traversed the roughly 5.25-second selector75 opening. All ten
sampled client spectator records in those intervals reported `reason=presented`
and `replica_applied=1`, with received frames advancing and 37–41 applications
per sampling interval. Interval placement uses adjacent timestamped records.
Neither material-output nor material-graph refusal occurred in the captured
run. Both roles loaded the tested DLL; the requested combat/SP/SSP options
were enabled. This proves continued application in the sampled opening
intervals, not rendered smoothness, camera/end-effect parity or owner
acceptance. C3 remains open.

Owner follow-up confirms the opening freeze is resolved, the Spirit animation
appears on both client and host, and playback is substantially smoother.
This establishes owner visual acceptance for the opening fix. Lingering ending
effects remain reported; their cause and lifetime parity are not yet established.
Do not infer full C3 or client Spirit camera acceptance from this result.

### 2026-10-10 — C3 return flash: one-shot ending candidate

The owner narrows the remaining symptom to particles/light around returning
Tal: the ending light stays bright across the scene, then disappears abruptly.
Sampled world-light-manager current/target values remain equal on both peers.
The summon renderer differs: the host finishes its one-shot clip near native
time220, while the client's loop mode advances beyond it and wraps the sampled
time to the beginning. Repeated host end samples therefore replay the opening
portion until host roster removal. This supersedes the earlier, incomplete
hypothesis that backward corrections alone explain the tail.

CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST: see the named native
timing methods and fixture in `docs/engine-functions.md`. The candidate selects
one-shot playback for Tal's observed summon using the native state setter,
preserves native completion, catches up only forward and avoids seeking at
the modulo boundary. The clone's entity lifetime remains host-owned; no new
retirement timeout, private completion write or forced task cancellation.

Dirty `decbbc5`, DLL SHA256
`91d02523267ca3912efed41f6d70415576862d0ec8983545aec4d08a45845d1d`.
Build, SpiritVisualTimingImageTest, LanArenaSpiritVfxTest,
LanArenaSpiritVisualHostTest and SkillTraceImageTest pass. All three new named
routine guards reject every-byte mutation. The broader raw-section fixture
needed its original end-epsilon operand relocated; the production guard was
kept strict. Existing visual tests cover host omission, unknown retention and
cleanup retry. No live ending-fade acceptance yet; compare two casts and
movement after Tal returns. Client Spirit camera remains a separate open item.

CONFIRMED_LIVE diagnostic follow-up: two owner-triggered casts on the candidate
reach native state `0x41` with absolute/sample time `219.999893`, hold that
sample without repeated end seeks, then enter host-driven retirement once
each. Both peers loaded the tested DLL; no material-graph refusal was logged.
This establishes the intended timing change in gameplay but does not replace
owner acceptance of the visible fade.


### 2026-10-10 — C3 ending light accepted; client Spirit camera next

CONFIRMED_LIVE owner acceptance: the ending light is now good after the
owner replayed the sequence through its end. This closes the visible
return-flash fade gate on the tested dirty `decbbc5` candidate, DLL SHA256
`91d02523267ca3912efed41f6d70415576862d0ec8983545aec4d08a45845d1d`.
Opening animation and both-peer effect visibility remain accepted. The
owner also asked whether the ending switched scenes or reloaded; the agent
did not observe that event, so its nature is UNKNOWN.

The owner authorizes client Spirit Strike camera work next. C3 remains
open for native camera selection/obstruction parity and restoration to the
client's ordinary view, plus the sufficient-SP ordinary Skill check. C4
has not started.


### 2026-10-10 — C3 client Spirit camera candidate

Owner selected camera parity after accepting the ending light fade.
CONFIRMED_STATIC: the host's private Tal camera already follows native
selection, while the saved-story client always presented its orbit view.
The candidate observes that retained private camera and sends its geometry
only to Tal's acknowledged owner. Selection/end changes follow the same
presented world timeline; native obstruction choices remain authoritative.
The client preserves its orbit, holds unknown camera observations, and
restores on an observed ending or confirmed presentation-owner change.
Host cutscenes take priority through an explicit restoration before borrowing
the view. See the native identities and transport/lifecycle limits in
`docs/engine-functions.md`, Remote Tal Spirit camera observation.

Dirty `decbbc5`, DLL SHA256 `a2d4b18918737674372b00c08d0fbd715745a2565298eddce9dccc7e602cc1f1`.
Build and seven focused checks pass: SpiritInstanceAbiTest,
StoryCastViewTest, StoryCastViewNativeTest, StoryVfxDispatchTest,
StoryActionTransportTest, StoryEffectTransportTest and SkillTraceImageTest.
Five new native instruction checks were verified by name/RVA and every-byte
mutation refusal. Tests cover interpolation/cuts, no early start/end, packet
loss/reordering, stale/wrong control/actor leases, scene revision changes,
foreign or read-only camera restoration with retry, repeated casts and Quit
destruction. Initial fixture gaps (registry membership, scene heartbeat rules
and C structure padding comparison) were corrected without relaxing native
or transport guards. These establish CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST,
not rendered camera acceptance.

**Owner gate:** cast Tal's Spirit Strike in open space and compare the authored
camera movement with native behavior; repeat near an obstruction where native
play suppresses the cinematic. Check both start and ending transitions, return
to the previous orbit heading/distance, movement afterward, a second cast and
the following host cutscene. The host player's camera should keep its own
view. C3 remains open, including the sufficient-SP ordinary Skill check; C4
is not started.


Camera candidate startup verified: both peers loaded the tested DLL, host
cast ownership bound, and the client reached input-ready state. A native
default-camera observation was captured on the host and received/applied on
the client. Combat Mode, Infinite SP and Infinite Spirit are enabled. Host
overlays were visually confirmed closed and client focus restored. This is
startup/baseline evidence only; no cinematic camera visual acceptance is
claimed. Stop at the camera owner gate above. The new findings and prior
ending-light acceptance are saved in the graph and recalled successfully.


### 2026-10-10 — C3 client Spirit camera accepted; ending spiral follow-up

CONFIRMED_LIVE owner acceptance on the dirty `decbbc5` camera candidate
(DLL SHA256 `a2d4b18918737674372b00c08d0fbd715745a2565298eddce9dccc7e602cc1f1`):
client Spirit Strike camera angles are correct, including the native edge
guard. This closes the owner camera-angle/obstruction gate for the tested
Tal saved-story route. It does not establish untested disconnect behavior.

The owner separately reports flicker in the ending pair of lights spiraling
down to the ground. That effect remains unresolved; the previously accepted
return-flash fade remains accepted. Preserve the live run before investigating
playback and particle clocks. The apparent host map change at combat end has
not been visually observed by the agent; examine scene/area/cinematic evidence
before attributing it to a reload. C3 remains open, including the sufficient-SP
ordinary Skill completion/control check. C4 has not started.


### 2026-10-10 — C3 ending spiral particle-clock correction

CONFIRMED_LIVE diagnostic evidence on the accepted camera candidate: the
owner confirms flicker only on the client. A read-only paired capture of one
owner cast sampled 262 summon observations per peer. Both main renderers
reached one-shot completion at `219.999893`. The host's custom particle
clocks held that sample; 21 client observations contained a near-full-duration
particle delta after wrapping to approximately `0.6` and returning to the
held end. The client fallback custom clock mistook an intentional model hold
for a stopped clock. Native model playback then supplied the ending sample
again. This is distinct from the earlier corrected main-renderer loop.

The correction collects only native-driven particle instances under retained
Tal summon weak leases, with exact root, child, resource slot and BONA timing
ownership. The main manager independently confirms unique membership before
excluding those instances from the supplemental custom clock. Native particle
aging, host-owned lifetime and unrelated custom clocks remain in place. The
list is freshly observed and consumed on the same verified game-thread seam;
it is never retained. Failed ownership is unknown and rejects that cosmetic
advance. No new hooks or protocol changes are introduced.

CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST: five focused checks pass:
SpiritVisualTimingImageTest, StoryEffectClockImageTest, LanArenaSpiritVfxTest,
SkillTraceImageTest and StoryVfxDispatchTest. The native fixture reproduces
the particle wrap/jump and verifies stable held samples after exclusion;
independent clocks, invalid/duplicate ownership, resource mismatch, weak
destruction, retained retirement and unchanged outputs on failure are covered.
Four added byte checks are tested by name/RVA and every-byte mutation:
VisualChildClockDispatch `2228D0`, ParticleBoneTimeBinding `1F9F70`,
ParticleBoneQuery `1FA070`, ParticleModelTimeSet `231860`. Existing timing
guards remain tested. Dirty `decbbc5`, DLL SHA256
`ff13b600dd2da7fa4b3792fe3fe2394e230418511348df4ad8e0880688af381c`.

The reported combat-ending map event is now narrowed by saved diagnostics:
the host briefly enabled letterbox/cinematic mode and returned to normal
within roughly half a second; the client followed that transition. Recorded
area and scene epoch did not change and no additional load was recorded.
This supports a brief scripted camera transition, not a recorded map reload;
the agent still did not visually observe the original event.

**Owner gate:** replay the ending on both screens; the descending pair of
lights should stop flickering, retain the accepted fade and camera, and clear
with the host. Repeat once and check ordinary hit particles still expire.
Visual acceptance of this correction is pending. C3 remains open, including
the sufficient-SP ordinary Skill completion/control check. C4 has not started.


### 2026-10-10 — C3 spiral accepted; ordinary skills and melee presentation

CONFIRMED_LIVE owner acceptance: ending spiral flicker is gone on the tested
dirty `decbbc5` correction, DLL SHA256
`ff13b600dd2da7fa4b3792fe3fe2394e230418511348df4ad8e0880688af381c`.
The owner also confirms ordinary skills now fire. This does not close their
full parity gate: Shin Splitter lacks dust and sword trails on the client;
ordinary primary/secondary swings and combos also lack client sword trails.
The initial report placed the missing directional/range cone on the host;
the owner subsequently clarified it is missing on the client. Tal cannot
move during the short skill aiming window. Investigate shared native presentation and
phase-specific input ownership so other skills inherit the proper behavior.
C3 remains open; C4 has not started. Owner handles game commands and controls.


CONFIRMED_LIVE observation, same pre-candidate build: the owner clarified
that the missing orange cone is on the **client**, superseding the earlier
host-side report. The owner selected Shin Splitter from Q and confirmed it
fires. Passive paired observation recorded host trail accumulation and bounds
changing while the client's remained unchanged during replicated swing clips.
The host created the Shin Splitter dust and aiming arc; neither was observed
on the client. This proves the missing native presentations in that capture,
not a visual acceptance of the correction below.

CONFIRMED_STATIC correction: the contained client gives body children zero
delta, freezing native trail history independently of replicated body pose.
The existing ambient adapter now changes only the native trail child's zero
delta after a fresh same-frame actor/model/registry/child/resource proof.
It leaves the body animation and gameplay step contained. Separately, exact
ordinary-cast lineage and two verified resources now enter the story cosmetic
stream: Shin Splitter dust and its attack-zone arc. Client clones use the
existing isolated listener, host transforms, one-shot playback and host
retirement. Completed model-driven particle clocks retain the accepted
spiral hold behavior. LA42/SMP4 packet layouts and kind admission are unchanged.

Remote movement now follows the shared native ordinary-skill AIMING phase
with positive remaining time. Other skills using that phase inherit this
movement rule. Native execution flags and strict cast/drain ownership remain
in force; other skill-specific visuals still require closed resource admission.

CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST: the candidate builds with MinGW.
Eleven focused executable targets pass under Wine: StoryTrailClockImageTest,
StoryTrailOwnerImageTest, StorySkillVfxCodecTest, StoryCastAdmissionTest,
StoryMeleeAdmissionTest, StoryVfxDispatchTest, LanArenaSpiritVfxTest,
SpiritVisualTimingImageTest, LanArenaSpiritVisualHostTest, LanArenaProtocolTest
and LanPartySessionTest (each prefixed `SudekiMP.` in the build).
The image tests use the supported executable SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
The two new checks are verified by name/RVA plus every-byte mutation:
TrailUpdateEntry `225760` and TrailChildDispatch `222A50`. The actual x86 shim
passes inert stack/register argument tests. Rejected ownership, phase-bound
movement, ordinary synchronous emissions, codec isolation, effect lifecycle
and model-driven particle holds are covered. Existing protocol regressions
pass. In-game hook teardown/retry and visual parity of this candidate are
not yet tested.

Candidate: dirty `decbbc5`, DLL SHA256
`e8259d3dfa392749b083a3390f14b2eb17ae44fa724e1a8099655cb71d62ec45`.
The candidate is saved separately; the running pair was not reloaded or
changed. No game input was injected. The owner retains commands and controls.

**Owner gate:** after loading this candidate on both peers, compare M1, M2
and a combo for sword trails. Select Shin Splitter while armed and idle:
compare its dust, sword trail and orange aiming arc, and walk/turn during its
short aiming window. Check that the arc follows Tal, clears with the host,
and execution/control recovery still match. Repeat the skill once; then
confirm the accepted Spirit Strike camera, ending fade and spiral remain
unchanged. C3 stays open until the owner accepts this pass. C4 has not started.


### 2026-10-10 — C3 Shin Splitter accepted; ordinary skill camera candidate

CONFIRMED_LIVE owner report: Shin Splitter fires and the previously reported
effect/aiming issues are fixed. Both staged peers carry the skill-presentation
candidate, dirty `decbbc5`, DLL SHA256
`e8259d3dfa392749b083a3390f14b2eb17ae44fa724e1a8099655cb71d62ec45`.
The owner now requests ordinary skill cinematic angles with the same native
obstruction/edge behavior accepted for Spirit Strikes. This acceptance is
specific to the reported Shin Splitter test; it is not a new blanket claim
for all weapons, skills or maps.

CONFIRMED_STATIC root cause: the host already routes ordinary skills through
the retained native named-camera namespace, but story camera capture was
explicitly limited to `spirit_started`. It published normal-camera kind 0
for an ordinary skill even when the native observer selected InitCam or
SkillCam. A new regression fails on that exact case before the correction.

The correction permits either retained ordinary-skill or Spirit activity to
publish the observed native selection. It preserves native obstruction/default
choices, host-owned start/end timing, camera geometry and the existing client
orbit restoration. It does not choose angles from a cast timer. This is the
shared Tal skill path, not a Shin Splitter exception. Existing message 37,
actor/scene/control fences, UNKNOWN handling, cutscene priority and host
camera ownership remain unchanged. No new native hooks or byte guards.

CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE: five focused targets pass under Wine
following a successful MinGW build: `SudekiMP.StoryCastAdmissionTest`,
`SudekiMP.StoryCastViewTest`, `SudekiMP.StoryCastViewNativeTest`,
`SudekiMP.SpiritInstanceAbiTest` and `SudekiMP.SkillTraceImageTest`.
The new fixture checks ordinary and Spirit default/InitCam/SkillCam selection,
geometry, completion, recast, changed/unknown ownership, invalid geometry and
unchanged outputs on refusal. Existing checks cover timeline cuts, transport,
orbit restoration with retry and native camera ownership. The supported
retail image SHA256 remains
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
Existing native checks passed by name/RVA and every-byte mutation:
`CCameraCtor.RenderStateSize` E72BF, `CCameraCtor.RenderStateAllocate` E72D1,
`CCameraCtor.RenderStateConstruct` E72DD, `CCameraCtor.RenderStateResult` E72E2,
and `CCameraCtor.RenderStateStore` E72FD.

New candidate: dirty `decbbc5`, DLL SHA256
`32239b59ddd0acdda4a20fa36412d2a61b5614f8ef73f71a76c4ecee625dfe4b`.
Saved separately with an owner-run reload helper. The agent did not reload,
stage into the running pair or inject game controls. In-game appearance and
live teardown/retry of this candidate remain untested.

**Owner gate:** load the candidate on both peers, then cast Shin Splitter in
open space and near an obstruction/edge where native play suppresses the
cinematic. Check the authored skill angles, native fallback, return to the
previous camera heading/distance, aiming movement and effect cleanup. Repeat
once; verify the accepted Spirit camera/edge behavior and the host player's
independent view remain intact. C3 remains open; C4 has not started.


### 2026-10-10 — C3 Shin Splitter camera accepted; roster coverage clarified

CONFIRMED_LIVE owner report: the ordinary-skill camera correction worked for
Shin Splitter. This accepts the reported test of the dirty `decbbc5` candidate,
DLL SHA256 `32239b59ddd0acdda4a20fa36412d2a61b5614f8ef73f71a76c4ecee625dfe4b`.
It does not establish every other skill, obstruction case, character or map.

CONFIRMED_STATIC coverage: the camera gate applies to ordinary skills and
Spirit Strikes, with no Shin Splitter slot check. Other Tal skills using the
same native InitCam/SkillCam selection path inherit it, but their live results
remain UNKNOWN. Story capture selects `caster[2]`; camera packet validation
and host publication also require character 2. Ailish, Buki and Elco are
therefore not covered by this client camera replication feature. Extending
capture, validation and per-owner routing and testing each character remains
necessary before claiming full-roster camera parity. No implementation scope
was changed during this clarification; C3 remains open and C4 has not started.


### 2026-10-10 — C3 full-roster Skill and Spirit camera candidate

Owner priority: extend the accepted native cinematic camera behavior to Tal,
Ailish, Buki and Elco, for both ordinary Skill Strikes and Spirit Strikes.
This pass extends camera coverage in the saved-story direct-IP profile. It
preserves each skill's authored choice of cinematic or normal camera; it does
not add a cinematic to a move that has none. Other characters' effect assets
and all individual skills' live behavior are not established by this pass.

CONFIRMED_STATIC: the prior Tal-only restriction existed in three places:
retained cast capture, camera-message validation and host peer publication.
Capture now takes a character and observes that character's retained native
camera bank, cast activity and sequence. It refuses an absent/stale actor and
the host-local character. Runtime publication visits each roster slot
independently, so an unknown or missing observation cannot suppress another
character's camera. Both cast families use the same path.

Camera packets accept the existing valid character range. The host sends each
observation only to the ready peer whose acknowledged control fence owns that
character. It cannot relabel one character's camera for another player. Scene,
lease, actor-generation and control-transaction fences remain in force.
Native obstruction/default selection, unknown-observation holds, host-cutscene
priority and prior-orbit restoration remain unchanged. No native hook, byte
signature, global ownership rule or wire layout was added or changed.

CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE: MinGW builds the candidate and seven
focused executable targets pass under Wine: `SudekiMP.StoryCastAdmissionTest`,
`SudekiMP.StoryCastViewTest`, `SudekiMP.StoryCastViewNativeTest`,
`SudekiMP.StoryVfxDispatchTest`, `SudekiMP.SpiritInstanceAbiTest`,
`SudekiMP.SkillTraceImageTest` and `SudekiMP.LanPartySessionTest`.
Coverage includes every roster character with both cast families; per-owner
native default/InitCam/SkillCam observation; sparse/unknown captures; three
simultaneous client camera streams with every character as host; independent
end/restore; stale fences; release and reacquisition of ownership; restoration
refusal/retry; and private native camera namespaces with no host-camera writes.
The ownership test reacquires the same character; live character reassignment
is not claimed. The loopback fixture was corrected to respect the existing
scene heartbeat tick/revision contract; no production validation was relaxed.

The supported retail executable SHA256 is
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
Existing native checks pass by name/RVA against that image and every-byte
mutation: `CCameraCtor.RenderStateSize` E72BF,
`CCameraCtor.RenderStateAllocate` E72D1,
`CCameraCtor.RenderStateConstruct` E72DD,
`CCameraCtor.RenderStateResult` E72E2 and
`CCameraCtor.RenderStateStore` E72FD.

Candidate: dirty `decbbc5`, DLL SHA256
`6c392e1e0a91f48816d428949e92c69178bd644fafd4dff1659968f5f55b78c0`.
It is saved separately with an owner-run reload helper. The agent did not
reload or stage into the running pair, inject game controls, or change the
modmenu. Automated evidence is not full-roster visual acceptance. Live
teardown/retry, transitions and character reassignment remain untested for
this candidate.

**Owner gate:** load the candidate on both peers. For each available roster
character, use an ordinary skill and a Spirit Strike in open space and near
an obstruction/edge where native play suppresses its cinematic. Compare the
native camera choice, timing and return to the previous orbit; repeat a cast
and check that the host player's view stays independent. Skills without an
authored cinematic should keep the normal view. Check camera restoration
across a later area/owner change when that route is available. Record each
character and move tested; untested moves remain UNKNOWN. C3 remains open;
C4 has not started.


### 2026-10-10 — C3 roster camera and attack-effect verification tracked in #57

At the owner's request, opened [issue #57](https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/issues/57)
to check Tal, Ailish, Buki and Elco across ordinary Skill Strikes, Spirit
Strikes, regular M1/M2 attacks and combos. It records camera/fallback checks
separately from effect visibility, timing, targeting markers, trails,
projectiles, impacts and cleanup on the casting client, host and other
observers. Individual moves and weapons must be recorded; untested or locked
moves remain pending.

CONFIRMED_STATIC: the latest extension changes camera coverage. Effect
replication has a separate closed resource set, including shared and named
Tal/Buki/Elco Spirit assets plus Shin Splitter and its targeting arc. Existing
asset support and trail-clock handling do not prove complete roster effect
parity. Prior owner acceptance of the specific Tal tests is retained; this
issue is not full-roster visual acceptance. No implementation, running game,
DLL or modmenu was changed in this issue-recording pass. C3 remains open;
C4 has not started.


### 2026-10-10 — C3 native single-player comparison prepared

The owner requests a logged base-game comparison: load New Brightwater,
place the party at the same pre-ambush spot, and let the owner start combat.
Added default-off `[StoryAreas] NativeResearch54=true` in the loader. This
exclusive launch profile installs the existing service-only controller observer,
read-only scene/roster and Research54 observers, and the existing one-time test
placement. It starts no lobby or network session and installs no remote-control
leases, C1/C2/C3 gameplay adapters, cast-routing or client presentation. Combat,
party leadership and movement remain native. Save files are copied into a
separate runtime prefix; the original saves and multiplayer pair are unchanged.

CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE: MinGW build and
`SudekiMP.SkillTraceImageTest` pass, including the existing service-only
observer/lifetime tests and named native checks. No new patch seam or byte
signature is introduced. The first startup was refused before gameplay because
area-follow initialization checked `RVA_SWITCH_ZONE_NOW` (7990) and
`RVA_ENTER_ZONE` (7970) after the scene observer had hooked them. Initialization
now checks those native entries before installing their observer, matching the
established runtime order; guards remain intact and the failed run is retained.

CONFIRMED_LIVE startup only: the corrected candidate reports
`profile=native_research54 network=false party=native movement=native`, and
Research54 plus scene observations are arriving at the native title screen.
Dirty `decbbc5`, DLL SHA256
`60c60d5cc109ccd0d627de455a8aa6493717aa5a4203202ac26cf109522a61d4`;
supported executable SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
The owner loads the save and triggers combat. Post-load placement, combat
comparison and live teardown/retry are not yet accepted. C3's movement gate
remains pending; C4 has not started. Full-roster camera/effect checks remain
tracked separately in issue #57.


Native comparison follow-up: the owner loaded New Brightwater. Native logging
paused while the game lost focus to the chat; after focus returned, the target
area became resident and the existing test placement reported two actors
placed with zero refusals. This confirms post-load placement and continuing
instrumentation, not combat parity. The owner now controls combat. Native
leadership initially remains Ailish as stored in the save; Tal should be
selected through the game's normal controls for the remote-Tal comparison.

### 2026-10-10 — Native combat comparison identifies C4 movement differences

See the same-date comparison entry in `docs/coop-story-plan-54.md` for
candidate identity and evidence boundaries. CONFIRMED_LIVE instrumentation:
native Tal continues controller movement submission during active melee,
with speed parameters 1.0/1.5/1.8 and no movement-AI speed resets after the
scripted intro in complete sampled windows. CONFIRMED_STATIC: current
`SudekiMpLanStoryControlMove` still filters through `body_idle_common`;
`SudekiMpLanPartyControlStoryMove` caps magnitude/speed at 1.0 and
`story_control_stop` immediately stops. This is movement submission evidence,
not proof that every native attack permits translation or that the client is
a particular percentage slower. Historical co-op melee summaries show the
C3 AI bypass active, but do not replace a current-build owner gate. No new
code, native patch, gameplay action, or test was performed in this analysis.

### 2026-10-10 — C4 movement candidate from the native comparison

At the owner's request, implemented the movement differences identified by
the C3 native comparison. C3 visual acceptance remains open; this is explicit
authorization to prepare C4, not retrospective acceptance. Dirty `decbbc5`
with the preserved prototype and this pass; DLL SHA256
`711bdab26c99a2eb58c87f25327f5bb3a8e2019a71c94fe47487654ce9564cf3`.
The supported executable remains SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.

CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE: see **Saved-story C4 native movement
contract** in `docs/engine-functions.md`. Ordinary movement no longer treats
melee attack bit `0x1000` as a blanket hold. Each admitted remote movement
request reaches native Move/CanMove with its existing native combo/cast gates.
Neutral input calls native blended SetSpeed instead of the lifecycle stop.
Menu, pause, cutscene, interaction, cast execution, actor/lease and drain
checks remain. Native targeting receives unit direction while movement
retains speed, avoiding rejection of valid scaled requests by the targeter.

The input sample, client camera transform, movement codec and host now retain
the native shaped magnitude. Finite scaled-axis bounds and a derived maximum
world-vector length of `4.13` remain enforced. Saved-story gameplay profile
advances from 19 to 20 so an older peer refuses at handshake rather than
silently discarding movement above one. The 36-byte movement layout, party
envelope, arena protocol and save files are unchanged. Both peers require the
candidate. Research54 input summaries now record frames, fresh moving frames,
held moving frames and timestamp so C4's held-share gate has a denominator;
this does not measure translation refused internally by native CanMove.

CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE: DLL build and eight focused tests pass:
`StoryPlayerMovementImageTest`, `StoryMeleeAdmissionTest`,
`StoryActionTransportTest`, `LanPartySessionTest`, `SkillTraceImageTest`,
`StoryPlayerTurnImageTest`, `StoryAiMovementImageTest`, `StoryQuickInputTest`.
The movement image test checks **C4_INPUT_SCALE**, **C4_LOCAL_SHAPE**,
**C4_CONTROLLER_DEADZONE**, **C4_CONTROLLER_VECTOR**,
**C4_CONTROLLER_MAGNITUDE**, **C4_MOVE_ARBITRATION**, **C4_BLEND_STOP**,
**C4_CAN_MOVE**, and the shape constants by name with mutation refusal.
It executes 22 native input cases and verifies native arbitration and blended
release. The admission test preserves repeated movement submissions during
melee, aiming movement, cast/menu/interaction holds and strict attack drain.
Transport tests round-trip scaled movement through actual local peers, reject
invalid magnitude/nonfinite data and reject the older gameplay profile.
Existing hook restoration/refusal tests pass; no live teardown is claimed.

The new boundary regression initially exposed a mistaken deadzone comparison,
then x87 excess decimal precision; the final helper matches the native stored
float boundary without changing executable guards. No running game, input,
save, staged DLL or modmenu was changed. Candidate visual feel, numeric
held-share comparison, current-build C3 movement acceptance and live retry
remain pending. Full-roster effects/cameras remain tracked in issue #57;
the beast explosion-position mismatch and delayed enemy attacks remain open.

C4 owner-run follow-up (**CONFIRMED_LIVE instrumentation**): see the plan's
entry **C4 owner run: movement admission matches; smoothness unresolved**.
The ordinary-combat sample records zero mod-held movement among 9,525 fresh
moving-input frames, native-range requested speeds, and no Tal movement-AI
speed resets/turns/snaps in complete call windows. The client still advances
the native frame counter at approximately 35/s median versus the host's 56/s.
This supports the movement-admission changes while leaving visual smoothness,
physical motion alignment and the cause of the rate difference unresolved.
The scripted intro and ending Spirit are excluded from the ordinary-combat
counts. No code or gameplay actions were changed during this inspection.

### 2026-10-10 — C4 client performance: page-query hotspots and candidate

The owner requests investigation and further improvement of the client/host
update-rate gap. CONFIRMED_LIVE instrumentation on the C4 candidate: ordinary
combat presentation windows have median mean costs of approximately 8.03 ms
for applying the replicated world and 7.83 ms for the separate cosmetic-effect
dispatch. The sampled party/world buffers remain populated. These phase costs
do not establish zero network lock contention, RTT, rendered FPS or GPU cost.

A separate bounded 12-second user-space CPU sample of the still-running pair
after combat records 8,488 client-main-thread on-CPU samples. Its call chains
include 2,121 samples in the containment module's readable-memory helper and
1,872 in the particle module's helper (approximately 47% combined). The
sample contains no dropped records. The helpers repeatedly call VirtualQuery
for objects sharing mapped regions. This identifies substantial local
validation overhead; it does not assign every remaining millisecond or prove
networking/rendering irrelevant. The native comparison game remains open and
was not closed or modified for this sample.

Implemented fresh read-only page-access scopes in `registry_exact` and particle
`topology`. Both retain the full object/value proof and discard the cache on
every exit before native execution. The registry scope preserves an outer
presentation scope's enabled state but empties its cache. No guards, gameplay
pause references, clocks, native ownership, update rate or network layout were
relaxed. See `docs/engine-functions.md`, **Saved-story validation cost: scoped
page queries**, for the contract.

Dirty `decbbc5` plus the preserved prototype, C4 and this pass. Candidate DLL
SHA256 `cf66be37913a5bda489226093328ffdbff9a2b2b8a6be314c6bf485352994c53`;
supported executable SHA256 remains
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE: DLL build, `StoryClientRegistryTest`,
`StoryEffectClockImageTest`, `StorySpawnTimingImageTest` and
`SkillTraceImageTest` pass. Synthetic walks reduce page queries 528 to 3 for
512 retained entities, and 320 to 3 for 318 particle instances. Tests preserve
ownership/value mutation rejection, no-access/guard/free rejection, fresh
permission observation, nested-scope cleanup and retry. Existing named
`PARTICLE_CALL`/`PARTICLE_UPDATE` and `MODEL_CLOCK_CALL`/`MODEL_CLOCK_SET`
contracts, model-call mutation refusal, hook rollback/restoration and the
native-driven ending-clock hold regression pass unchanged. No live teardown
is inferred from those tests.

The candidate is saved but has not replaced the running pair. Performance
improvement in live play, C3/C4 visual acceptance, effects/trail/sigil behavior
and client containment across a later area change remain owner-gated. No input,
reload, commit, reset, stash or unrelated staging occurred in this pass.

### 2026-10-10 — Validation optimization: live checkpoint comparison

Owner-authorized pair reload on dirty `decbbc5`, using the validation candidate
SHA256 `cf66be37913a5bda489226093328ffdbff9a2b2b8a6be314c6bf485352994c53`.
Both staged DLL identities match, configuration bytes are unchanged, both
saved-story startups succeed, and the client acquires Tal with input ready.
Previous DLLs and logs were retained. The separate native comparison game
remains open. No game input was injected and no commit or Git cleanup occurred.

CONFIRMED_LIVE instrumentation at the pre-ambush checkpoint: 11 seconds from
the prior build and 22 settled seconds from the optimized build yield median
native update-counter rates of 34.41 to 74.33/s on the client, versus 61.57 to
60.58/s on the host. These contain 9/21 usable client one-second intervals
and 9/20 host intervals; a wrapped 16-bit counter difference is divided by
elapsed time, with intervals outside 0.5–2 seconds excluded. They are not
rendered-FPS measurements or a matched combat replay.

For client presentation windows wholly inside the selected bounds, median
per-window mean cosmetic-dispatch cost drops from 6.777 to 0.390 ms, and
replicated-world application from 7.773 to 6.897 ms (10 old / 20 new windows).
Preflight and world-body costs remain approximately 3.6–3.7 and 2.6 ms;
party/world buffers remain at 8 and all selected spectator samples present.
The three complete new host input windows have no stale or held-moving frames,
but also no moving input, so they cannot re-establish combat motion parity.

Loading and a transient host-publication interruption during startup are
outside the selected interval. The connection/control state recovered; the
interruption's cause is UNKNOWN. Raw snapshots, interval bounds, hashes and
analysis remain private. No new combat is present in this capture. The live
checkpoint result supports the page-query cost diagnosis, while sustained
combat pacing, visual effects and area-change containment remain owner-gated.

### 2026-10-10 — Validation optimization: subsequent owner combat capture

Same dirty `decbbc5` validation candidate and unchanged pair configuration as
the checkpoint comparison above. The owner's subsequent encounter spans
approximately 206 seconds in the sampled host combat flag. For ordinary
combat, exclude the scripted intro through the first resumed input summary
and stop at the final Spirit activation. The selected new interval covers
179.792 seconds; the old comparison covers 182.425 seconds. CONFIRMED_LIVE
instrumentation: median native update-counter rates are 35.05 to 66.73/s on
the client and 56.21 to 55.83/s on the host. There are 179/178 usable client
intervals before/after and 180/177 host intervals. Counter rates are not
rendered FPS or position/animation alignment, and owner inputs differ between
the two runs.

Client presented windows have median per-window mean cosmetic-dispatch cost
7.828 to 0.405 ms and world-application cost 8.059 to 7.326 ms (195/196 rows).
This supports a substantial validation-cost reduction during combat, with
the earlier client update-rate deficit absent in this sample. It does not
remove the existing 50–66 ms target presentation buffer or prove zero latency.
Party/world buffer counts remain 8. There are 41 transient `not_ready`
presentation reports in the new interval, versus 29 previously, despite
admitted input and populated buffers; duration, cause and visible impact are
not established by these event counts.

Thirty-five complete new host input windows total 175.292 seconds, 9,862
frames, 7,303 moving-input frames, zero held-moving frames and one stale-input
frame. The one stale window records a maximum input gap of 108 ms. Final
Spirit activation and body-drained are observed outside the ordinary-combat
comparison. Effects, physical motion, camera behavior and transition safety
are not visually re-accepted from these logs. Raw snapshots, explicit bounds,
analysis and hashes remain private. No source change or game input occurred
during this read-only inspection; C3/C4 owner gates remain open.


## 2026-10-10 — saved-story encounter music ownership

**CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE.** `CBattleScores` global RVA
`0x409D80`; score Update-node subobject +8, ClusterListener subobject +0x30.
`BattleScoreLoad` RVA `0x39560` takes ECX=this and one string argument
(RET4, bool in AL), retires its previous emitter/cue through native Reset
`0x39A00`, loads a registered native sound bank, and clears the battle flag.
Its cluster battle listener `0x39AD0` takes ECX=this+0x30 and three arguments
(RET12); the first two are unused, the third boolean sets score+0xAC and
selects score+0x78 from peaceful +0x7C or danger +0x80. The listener itself
only changes scalars. Update `0x391C0` advances the score's native audio.

Cluster-enter parameters at score+0xA0/+0xA4/+0xA8 control native variation
reset/ramp/hold; +0x84 and +0xAD hold its remaining time/phase. Replicating
only the battle flag would leave a contained client's zero ramp duration and
risk division by zero. The adapter transports validated curve and phase
alongside the requested intensity and score. `BattleScoreIntensity` at
`0x397E0` is unsuitable for this projection because it also replaces the
peaceful default. No track assets or cursor synchronization are involved.

**CONFIRMED_LIVE observation:** after the accepted castle-road encounter,
host/client native score names differed (`BS_IllumCountry`/`BS_Brightwater`)
and client cluster variation parameters were zero. **INFERENCE:** missing
area-score and encounter listener delivery explains exploration music
persisting on the client; hearing the candidate remains an owner gate.

**CONFIRMED_TEST:** `StoryMusicImageTest` verifies named native guards,
relocations and method slots, typed score/emitter/bank/pool/cue owners,
actual native scalar danger entry/exit and refusal paths. It does not run
native bank loading or audible playback. `StoryEffectTransportTest` covers
new music fields, validation, atomic assembly and old-version refusal.
See the encounter-music owner-gate entry in `docs/coop-story-plan-54.md`.


## 2026-10-10 — Saved-story Tal combo UI and non-neutral cast-light construction

**CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE:** native accepted input reaches
RVA D14D0 through call D06B0, with EAX = actor's typed melee component
(actor+B8, component owner+10, vtable 2D4BD4). Stage+64 selects one of six
input kinds at +68 and timing bytes at +A0. Native display maps weak/strong
to 0/1 or mistimed 2/3, clears at stage zero, and appends up to three icons.
It updates the global HUD only for the native party leader. Remote Tal needs
his own copied display state rather than the host Ailish widget's contents.

Reset D0440 takes EAX=component. Combo-window D0A10 takes ECX=component+3C
and one callee-cleaned marker argument. The observation seam D0A3B is after
any queued input dispatch; ESI still owns the subobject and [ESP+8] contains
the marker. By the later D0A75 HUD call, ESI has already been restored to its
caller, so that call cannot identify the component. D0A3B's seven displaced
bytes are complete non-relative instructions. BukiReplicaNativeImageMatches
still accepts its guarded entry prefixes and interrupt calls with all three
new observers installed. No observation changes native admission or timing.

Native HUD global 3C2FC4 / vtable 2CB784 has mode+48, flags+58, icon row
+1F4/+1F8/+1FC, blink clock+1EC/+1F0 and flags+200/+201, marker+202.
Hide AED20 takes EAX=mode, ECX=HUD; Show AE9D0 takes EAX=mode, ESI=HUD;
label AE7C0 and icon refresh AF110 take EAX=HUD. Existing native gizmos and
blink update draw the replicated state. The story codec carries only plain
validated UI state; full rows recover from missed packets without replaying
pressed buttons. See StoryComboHudImageTest and StoryShotsTest for fixture
scope and the plan's owner gate for the pending visible result.

**CONFIRMED_STATIC / CONFIRMED_LIVE observation:** the captured cast-binding
failure at `cast_lighting` preceded Ailish's KO. Authored ambush lighting has
three active effects and RGB (0.6, 0.75, 1.1). The former cast-light validator
incorrectly restricted these native gain values to [0,1] and demanded one
neutral effect. Init 1F210 seeds a new manager from light-group publication
404CAC, while 1F280 resets only that new manager. **CONFIRMED_TEST:** creation
with this non-neutral, active world now preserves the complete world object,
all effect records and publication; mismatched publication, negative or
nonfinite values still refuse, and drained teardown succeeds. Whether a
separate downed-leader restriction exists remains UNKNOWN pending the owner
cast comparison. This corrects the earlier KO-causation suspicion.


## 2026-10-11 — Automatic zone combat-gate instances and native lifetime

**CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE:** on the supported executable,
`InternalSpawnGates` at RVA `75F90` takes a cdecl resource-name argument,
resolves a spawn group (kind `20`) and calls creator `737E0`. Current world
global `408D10` leads through descriptor `+0C` (vtable `2C82A4`) to CZone
(vtable `2CDCDC`): descriptor state `+34 == 1` selects `+18`, otherwise `+14`.
CZone `+BC` points to the loaded gate template's Resource subobject,
entity `+2C` (vtable `2D5B44`). This is a template, not a placed doorway.
Zone resource count `+FC`, table `+104`, stride `24`, resource at entry `+1C`
establish a group's membership in the current zone. `+C0/+C8` instead refer
to the zone's named Locations object and do not identify gate placement.

Spawn groups (vtable `2C9E20`) carry point count `+168`, array `+170`, stride
`80`. Each point has a native weak actor reference at `+6C`; flags `+7A` bit
`800` marks a spawned gate. The native creator supplies model, collision shape,
authored transform and scene registration. `GateSpawnSetup` vtable `2C9EC8`,
callback `72E30`, completes scene state `entity+140 = 3` and calls the entity
heartbeat through callsite `72E77 -> FCC40`. The six zone-configured barrier
families' authored heartbeats select an opening animation, wait, then select
a looping animation. The host must retain its script execution; an owned
paused client replica instead receives host animation state.

Native intrusive weak-reference assignment `1750` takes EAX=destination,
EDX=object; each reference stores object/previous/next. Native destruction
`4D30` clears observers, allowing address reuse to receive a fresh occurrence
identity. Native UpdateNode pause `1060C0` takes EAX=entity+8. Spawn-point clear
`156100` takes EDX=point, unlinks `+6C` and clears the spawned-gate flag.
Existing native `DespawnEntity` at `B2300` accepts a linked weak reference.
Retirement is refused while scene setup has not positively completed; this
does not invent a cancellation contract for an asynchronous setup callback.

**CONFIRMED_LIVE observation:** both pre-candidate processes had the same zone
gate template and matching authored road/farm spawn groups in their native
zone resource rosters. During the observed encounter, only the host acquired
a placed road vine. Its opening, hold and closing animation selectors were
observed before native destruction; the template remained separate. This
establishes missing instance replication, not a generic CDoor defect.

**CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE:** `StorySpawnTimingImageTest` exercises
the new named `gate_native_code` check, rejects 21 altered native sites, and
checks native weak destruction/address reuse, distinct occurrence identities,
template exclusion, owned-only heartbeat suppression, pending-setup refusal,
spawn-point reset and hook restoration. `StoryEffectTransportTest` covers
complete gate-group records, invalid masks/counts/duplicates, unknown versus
empty state and atomic three-chunk assembly. `StoryResidencyCallTest`,
`SkillTraceImageTest` and `LanPartySessionTest` also pass for this candidate.
The implementation is in `src/hooks/lan_story_gates.h`, included by the spawn
trace owner, with existing world pose and scoped client residency integration.

Live pair installation/readiness is observed; candidate barrier creation,
visible parity, closing/removal and retry/teardown are still owner gates.
The six automatic families are the current allowlist, not a claim that every
scripted gate shares this lifecycle. See `docs/coop-story-plan-54.md` for the
inventory, candidate identity and acceptance route.


## 2026-10-11 — Saved-story Spirit audio omission, before correction

**CONFIRMED_LIVE observation:** with dirty `fc43d78` barrier candidate recorded
in the plan, Tal's native host sound pool contains the Spirit startup cue,
initiation, roar, atmosphere, strike/travel and return cues. A paired 59-second
read-only capture finds none of those cues in the client pool, while Tal's
strike vocal and ordinary attack cues occur on both. The owner separately
confirms the missing Spirit sounds are client-only. This distinguishes the
missing sound layer from a general silent client or failed cast.

**CONFIRMED_STATIC:** `lan_story_character_audio.c` observes native actor
PlayCue at RVA `FC200` and requires the exact party actor's sound component.
`lan_arena_spirit_audio.c` separately observes CSound::PlayCue at RVA `17090`
and the legacy arena relays `spiritstrike_start`; saved-story cast binding
does not install that observer. That cue uses `spot_sounds.xsb`, whereas the
observed Tal summon cues use `TalSoundScript.xsb`.

On the supported executable (SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`), native
effect update `1319B0` separately supplies fallback sound-bank setup, sound
origin update and animation-component event delivery. Renderer update
`222B50` advances animation clocks without performing that entity update.
Native TimeSet `223180` also writes the previous-time value; seeking a visual
does not prove that intervening authored sound events were delivered.
The replica's one verified listener is its embedded sound listener, with no
actor-forwarding listener, as checked by the existing VFX identity guard.

These static boundaries identify the next investigation; they do not yet
establish which bank/origin/event condition fails in the live client summon.
A focused read-only model/audio capture awaits a repeat owner cast. No native
audio mutation, runtime fix, new test result or audible acceptance is claimed.

## 2026-10-11 — Spirit model sound-bank inheritance and saved-story startup candidate

Supported executable SHA256:
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.

**CONFIRMED_LIVE (observation, before correction):** Tal's initiation resource
`62DCC5A3` and summon `449D201B` use `TalSoundScript.xsb` on the host and
`sfx_generic.xsb` on the contained client. Immutable model event tables contain
the same cue names. The client emitter origin agrees with the presented effect;
its bank choice differs. This resolves the prior unknown bank/origin cause.

**CONFIRMED_STATIC:** `PlaySFXWithAll` RVA `18E70`, call site `1924C`, reads the
parent sound mapper's resource text at `+60/+64`, passes the effect sound
component to `SoundBankSet` RVA `FC3B0`, and sets hearing radius 50. Its ABI is
EDX=bank name, one stack sound-component argument, RET4; AL is always zero and
is not a success result. `EmitterBankSet` RVA `28AE50` performs the native
old-bank release and new-bank acquisition. `ManagerBankFind` RVA `289A10`
uses `SoundBankNameId` RVA `16550`, then searches registered banks by native ID.
The client parent-free factory lacks this inheritance tail and native effect
update `1319B0` chooses the generic fallback. The existing `fx_tick` in
`lan_story_spawn_trace.c` already calls that native update and event delivery.

**CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST:** the candidate uses the native setter
only with the exact local actor's already-loaded bank and isolated retained FX
sound listener. Native lookup ID, unique manager membership, image, thread,
roster and fallback reference proofs precede the call. Native destruction owns
the resulting reference. The four new full-body guards pass every-byte mutation
refusal; executing the retail setter on inert owned bank fixtures verifies ABI,
initial/replacement references, repeat behavior, hearing radius, and final-ref
refusal. The global startup observer at RVA `17090` is reused rather than
installing a competing detour. Saved-story scope/character/cast witnesses bind
its original emission timestamp to one semantic startup cue; protocol 23
uses the formerly reserved audio-header bytes for sequence and tick.

Validation details, candidate identity and pending audible acceptance are in
`docs/coop-story-plan-54.md`, “Tal Spirit audio bank and startup candidate.”
This does not prove all authored event timing, acoustic mixing, full-roster
Spirit audio, or cleanup during a live disconnect/zone change. No audible
acceptance is claimed before the owner's next listening test.

## 2026-10-11 — saved-story native binding busy refusal and recovery

`CONFIRMED_LIVE` observation of the unaccepted Spirit audio candidate: host
cast initialization failed at the combined native-namespace stage with
`ERROR_BUSY`, successful rollback and no subsequent bound event. Client Tal
requests were unavailable and host VFX publication did not initialize. The new
global sound observer was not reached. `CONFIRMED_STATIC`: every rolled-back
bind failure set permanent fault/stopping, including recoverable construction
busy boundaries. The preflight observes native Spirit manager state but does
not prove the original Spirit camera's idle state; native instance construction
separately requires both. Which predicate was busy in that run is `UNKNOWN`.

The correction retries only busy camera-banking/instance-creation failures
after positive complete rollback, with delayed fresh preflight. Exact-image
failure, unknown failure without a meaningful error, or failed restoration
still closes admission. Separate stage/caster diagnostics preserve the next
failure's origin; health changes remain logged beyond initial startup samples.
`CONFIRMED_TEST`: the real binding/rollback coordinator now has synthetic
coverage of a busy second caster, complete rollback, bounded retry cadence,
recovered cast and VFX readiness, identity/menu revalidation, invalid errors,
stale-error refusal and retained rollback failure. Eight focused suites pass;
details and pending live gate are in the plan's recovery entry. No change to
native constructors, animation clocks, accepted barriers or visual resources.

## 2026-10-11 — ordinary roster skill visual ownership and Witch’s Kiss

Supported image SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.

`CONFIRMED_LIVE` observation before correction: Ailish’s Witch’s Kiss emits
shared startup `SFXSA005_INTO_SFX` (`038BDFB5`) and
`SFXAS011_WITCHES_KISS` (`E76CA04D`) on the host; neither appears in the client
native effect census. The host’s character cue also identifies Witch’s Kiss.
This is an ordinary skill, not an Ailish Spirit Strike.

`CONFIRMED_STATIC`: `lan_story_cast.c:visual_owner` formerly admitted only
Tal’s cast sequence, apart from zero-sequence Ailish/Elco ordinary shots.
Neither observed model was in the renderer/host resource tables. Shipped
`PC_*.ANI` skill states identify six skill slots for each roster character;
Tal Blade Dance and Buki’s spinning attack have additional into/loop/out
states. The audited closed descriptors are now shared in
`src/network/lan_story_skill_resources.h`. Every admitted descriptor has a
backing HOM; animation event types across these models include 1, 2, 13, 19
and 22. They therefore require the existing exact isolated sound listener,
not a native actor-event-forwarding listener on the client.

The correction retains native namespace generation AND emission-time cast
sequence in effect/projectile lineage. Factory RVA `18760`, animation emitter
`E2810`, script-parent creation `18C90` and finalizer `18830` remain owned by
the existing observers. Positional setup that completes after its projectile
scope keeps the captured source. Buff recipients do not become casters.
Native skill missiles are observed as cosmetic model/pose/phase entries;
clients never construct their collision or damage components. Saved-story
caster counters are independent; Tal’s historical header frontier must not
reject another caster’s effects. Existing stream/epoch/packet and immutable
instance identity checks still fence replay.

`CONFIRMED_EXACT_IMAGE` / `CONFIRMED_TEST`: extended inert native fixtures
exercise delayed finalization and native weak destruction for all four owners;
all 62 newly admitted resources pass cache/spawn/retire policy tests. Cache
release waits for complete host withdrawal and successful local retirement,
and retains leases on unknown input or failed cleanup. Named sound-bank,
visual clock, cast readiness, effect and projectile image checks pass. The
plan’s roster-skill entry lists candidate identity, all fourteen suites and
owner acceptance route. Witch’s Kiss visual acceptance and every other move’s
completeness remain pending under #57; static resource coverage is not live
full-roster proof.

The reloaded roster candidate has bounded `CONFIRMED_LIVE` native evidence for
one Witch’s Kiss: startup and main resource instances appear on both peers,
advance their animation data, inherit Ailish’s bank and leave both native
registries after the host ends them. This is not a visual/audible acceptance
claim. `StoryRangedTransportTest` additionally confirms all four caster skill
entries reach every one of three real loopback clients, while clients cannot
publish the authoritative roster.

## 2026-10-11 — remote combat idle height and pose investigation

Supported image SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
See the plan's **remote idle lift report** entry for unchanged candidate
identity, observations and the remaining owner comparison.

`CONFIRMED_LIVE`, bounded read-only observation: native `CPosition+0x18`
and attached world-render translation do not show a recurring Tal height
correction during the captured combat idle. His height varies by less than
0.000008 units. Ailish has roughly 0.0041 units of host-origin height variation.
Renderer channel rows (`+0x98`, 24-byte rows) continue advancing their original
idle clips. Final matrices at renderer `+0x0C`, resolved through the validated
bank hierarchy/palette, show constant model roots and varying upper-body/hip
poses with nearly planted lower leg-chain endpoints. Captured loop boundaries
do not show a substantial vertical pose reset. Reads validate actor/component
ownership, renderer identity, channel/clip bounds and the supported image.

`CONFIRMED_STATIC`: original animation metadata identifies Tal selector 17 as
`C01F000_Tal_Lockon_idle`; Ailish selector 20 is `c02alice_idle_w`, with selector
60 `C02Alice_Arm_StartPose` used by the existing ranged overlay. Tal's observed
46-frame, 12-frames/second state agrees with the native reference. All 79 bone
translation triples closely match at nearby phases: maximum component
differences below 0.001 units on the host and 0.0002 on the client. The reference
is stationary, however; this does not prove continuous vanilla parity.

`UNKNOWN`: the cause of the owner's visible lift-and-settle report. The combat
idle samples do not justify flattening authored motion, changing rate, or
adding a ground correction. Ailish's dormant first-person host body cannot
establish native third-person parity. A running native comparison and precise
stance/transition identification remain open. No runtime change or new native
hook was made for this pass.

Follow-up `CONFIRMED_LIVE` camera observation after the owner's whole-scene
clarification: camera manager global RVA `409D7C`, its active camera `+0x20`,
camera render state `+0x34`, and matrix `+0x90` were checked against their
existing exact-image identities and `CGameCameraMode+0x0C` backlink.
Ailish's camera translation repeatedly changes vertically by up to roughly
0.0041 units alongside her host `CPosition`, with zero movement demand and
effectively unchanged orientation. Tal's camera height stays within 0.000009
units. Invalid camera samples are excluded (62 host failures; 934 validated
host samples). This is a position/camera lead separate from the idle-bone
comparison, not proof of a camera bug or native divergence. The cause and
running native Ailish comparison remain open; no smoothing patch was applied.

`CONFIRMED_LIVE` follow-up: an eight-write hardware watch on the verified host
Ailish `CPosition+0x1C` catches the upward excursion and downward return in
the native position setter at RVA `3084` (watch stops after the instruction at
`3087`). The caller returns through `110C51`. Exact-image inspection resolves
that path to collision publication `32320`, callsite `329FD` -> `110BD0`, then
`3050`; in this sample the position has a native parent and the publication
converts the resolved world point back through the parent's inverse matrix.
The debugger detached after the bounded observation; it did not write gameplay
values. Export-nearest names in debugger backtraces were not used as function
identity evidence.

Native movement update `C3200` subtracts gravity into `CMovement+0x38` and
calls `C3B70`. In the direct-delta branch (`CMovement+0x79` nonzero), that
helper multiplies the movement vector by elapsed time. The 696-sample paired
actor observation finds this branch on Ailish and not Tal, zero requested and
current speed, zero horizontal/secondary accumulators, and all four model
speed factors equal to one. This does not prove either integration branch is
incorrect. `UNKNOWN`: why the reported presentation differs from vanilla;
the available native reference remains stationary Tal. Preserve that limit
before selecting a physics or presentation correction.

`CONFIRMED_LIVE` running native follow-up: a fresh native-research window
produces 816 consecutive samples over 42.87 seconds with neutral Ailish input
and Tal's native idle clock advancing by 514.59 frames. Ailish's height range
is 0.000010 units and camera-height range 0.000017 units. Long paused intervals
are excluded. A separate 369-sample native/host movement comparison has no read
errors and finds the same Ailish direct-delta branch and gravity value in both
processes. This reference stands at the loaded New Brightwater location, not
the multiplayer pre-ambush ground; frame durations are also uncontrolled.

The owner's subsequent multiplayer walk is observed. After discarding two
seconds of settling following the last movement demand, 2,289 validated host
samples over 126.48 seconds retain a 0.004090-unit Ailish height range, including
924 adjacent sampled changes greater than 0.001 units. The longer camera range
also includes view changes and is not used as a fixed-angle comparison.
`INFERENCE`: a transient bad teleport landing is less likely. `UNKNOWN`:
location-dependent native collision versus a multiplayer regression. A native
research restart uses the existing, exact-image-validated test-start placement
at the pre-ambush location; the owner's matching aiming sample is pending.
No production physics, animation, camera or network correction was made.

### 2026-10-11 — ranged idle tiny-contact correction, owner gate pending

The owner identifies the **native same-ground Ailish window** as visually
steady. This does not accept the multiplayer candidate. Supported executable
SHA256 `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`;
dirty `fc43d78`, protocol 23. See the [candidate and named validation checks](
coop-story-plan-54.md#2026-10-11--ranged-idle-terrain-contact-candidate).

`CONFIRMED_STATIC`: collision RVA `32320` skips squared motion below `1e-5`
unless position byte `+103` requests a solve. First-person movement `+79=1`
uses the `C3200`/`C3B70` gravity/direct-delta path. Contact routine `33010`,
called at `3265A`, adds `0.01` normalized clearance at `33288` even when
penetration is less than `1e-5`. Its native EBP `+8/+C` identify the query
manager/collider; EBX holds that manager, EDI selects its current candidate,
and native ESP `+14/+18/+1C` contain the freshly computed surface normal.
Terrain candidate type is `+20=1`. The adapter borrows this current record
within the call; it does not lease the retained ground-source pointer.

`CONFIRMED_LIVE`: eight bounded observations hit that branch for stationary
host Ailish, with terrain type 1, penetration roughly `0.000001–0.000004`,
and normal Y `0.999925`. The debugger detached successfully. Passive captures
show negative vertical-only movement deltas crossing the outer threshold.
`INFERENCE`: longer multiplayer frame times expose a native clearance cycle;
the clearance minus the gravity step agrees with the measured `0.0041` lift.
A controlled frame-rate comparison remains unperformed.

`CONFIRMED_EXACT_IMAGE` / `CONFIRMED_TEST`: five focused suites pass. The
original contact routine executed over synthetic geometry reduces the tiny
floor lift while retaining tested air, large-penetration, movement and
nonterrain results. Named byte mutation checks, ABI state preservation,
rollback/restore retry and host roster/control refusal checks also pass.
The first large synthetic triangle lost its tiny penetration through floating
point cancellation; a smaller triangle establishes the intended native branch.
No live behavior is inferred from this fixture. Automatic review initially
rejected runtime integration before validation; after isolated tests passed,
the reviewed integration succeeded. Deployment and owner acceptance remain
separate gates. No commit, push or merge.

### 2026-10-11 — idle-contact boundary miss and second candidate

Owner rejects the first idle-contact candidate: visible lift remains. Paired
capture agrees with residual roughly `0.0041` height/camera excursions.
Hardware observations show the first margin correction active while an
adjacent native branch remains unchanged: `33010` front-side separation at
RVA `334A8`, continuation `334AE`, receives depth `0.000011444` and adds the
old clearance. This crosses the `1e-5` boundary only slightly. Native stack
`+44/+48/+4C` holds the normalized separation; ST0 at the seam contains half
the positive penetration. `CONFIRMED_STATIC` and `CONFIRMED_LIVE`, supported
image SHA256 `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.

The revised, isolated-then-integrated adapter covers bounded shallow contact
in that path, preserving the original backside and deeper-penetration paths.
Eight named byte contracts and both ABI paths pass; synthetic native contact
tests reproduce the boundary miss and verify the correction at x87 control
words `007F` and `037F`. See the [full revision, limits and owner gate](
coop-story-plan-54.md#2026-10-11--idle-contact-first-candidate-failed-acceptance).

Diagnostic correction: software breakpoints within the adapter's byte-checked
span cause correct fail-closed behavior. Those preliminary admission results
are invalid; the replacement bounded hardware trace leaves code bytes intact
and detached successfully. Do not weaken byte validation to accommodate a
debugger. `UNKNOWN`: second-candidate live visual acceptance, full-roster and
terrain/lifecycle coverage. Native comparison acceptance remains separate.


## 2026-10-11 — C5 grouped melee states and isolated native combat entry

Supported image SHA256:
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
Revision: dirty `fc43d78`, saved-story LAN, Tal/Buki remote melee only.

CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE: retail combat input RVA `DB0E0`
handles active interactions before combat validation. Weak press can set
trigger `+1D8` from `+1D9`; any melee release can clear it. These branches
cannot safely own a remote player's complete button snapshots. The join at
`DB189` begins ordinary native combat validation after these interaction
branches. `SudekiMpSubmitArbiterMeleeSnapshot` reproduces the original prologue
(EBP=weak, ESI=arbiter, EDI=block; five callee-popped stack arguments), enters
that join, and supplies zero weapon-switch states. Runtime admission requires
melee mode and excludes the special mode-4 path. It does not patch retail
code, replace the host controller, or borrow the trigger singleton.

`StoryInteractionGuardImageTest` checks the named selection/return/rejection
contracts and mutates every checked byte. With only the downstream attack
and block tasks replaced by inert sinks in the isolated test mapping, the
actual retail selection handles all 64 weak/strong/sweep states. Simultaneous
priority remains sweep > weak > strong, block retains priority, and
`1,2,2,2,3,0` submits just one melee request. Across 256 availability-byte
values and three latch values, the combat-only entry preserves the host
interaction fields and returns with balanced stack. This proves native input
routing and ABI, not downstream combo admission or visible gameplay.

CONFIRMED_TEST / CONFIRMED_STATIC: `lan_story_melee.h` transports grouped state
changes with a bounded redundant history, cancellation/replay floors and
source age. `StoryMeleeTransportTest` exercises codec bounds, packet loss and
reordering, no replay on repetition, stale-history rejection, neutral rearm,
clock wrap, overflow and concurrent in-flight inputs through actual loopback
peers for both Tal and Buki. Saved-story profile 21 separates this from the
old edge-only input peers; `LanPartySessionTest` verifies profiles 19/20 refuse.
A first integration test caught the dispatcher still capping message kinds at
the former audio channel; its corrected bound now admits the new melee
channel and the loopback passes. The exact-image and policy tests did not
require changing native byte contracts to obtain admission.

Runtime and ten focused suites pass; see the C5 candidate entry in
`docs/coop-story-plan-54.md` for the executable list, candidate identity and
owner gate. No additional native hook lifecycle exists for this adapter;
control-generation cancellation and retirement are tested, while live
transition/disconnect and combo timing remain UNKNOWN. Bounded recovery can
drop inputs outside its age/history window; no exact network-latency parity
is asserted. This supersedes the earlier statement that C5's optional native
snapshot rewrite was unimplemented, without extending that earlier owner
acceptance to the new implementation.
## 2026-10-12 — protected saved-story death/loot test setup

Owner-requested test setup adds `[StoryAreas] TestPartyInvulnerable`, disabled
by default, using the existing native party invulnerability lease reconciler.
The saved-story menu service owns acquisition/restoration on its witnessed
host game thread; no new executable patch, damage rule or speed override.
Client/native-reference profiles are excluded. Partial acquisition retains
cleanup responsibility until the reconciler confirms restoration.

CONFIRMED_TEST: build and five focused suites pass; CONFIRMED_LIVE: both peers
loaded at the established encounter approach and host Tal/Ailish protection
references and flags are present. This does not yet prove combat survival,
D1 death/reward containment, live teardown, or the revised C5 gameplay gate.
See the 2026-10-12 protected-observation entry in `coop-story-plan-54.md` for
candidate identity, checks and acceptance scope. Death/loot registry sampling
is read-only and cannot establish absence of every native death/XP call.

## 2026-10-12 — preserve native spawn sequence selection beyond the ambush

Supported executable SHA256:
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
Dirty `fc43d78`, saved-story research profile. Owner identifies the farmhouse
fight as the encounter missing from the client. The prior bounded capture
contains host farmhouse monsters and no corresponding client monsters;
19 subsequent host removals have no client copy. This is CONFIRMED_LIVE
failure evidence for that build, not a renderer-only diagnosis.

CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE: native start exports at RVAs
`75D70`, `75E10`, `75EB0` are distinct cdecl calls. The first chooses sequence
zero; the second accepts a separate sequence selection; the third also
accepts a scope string. Their pass-count argument goes to group `+178`;
the core at `73F00` receives selection in EAX and group in ECX, and stores
selection at `+180`. Group update `73210` compares that selection with the
spawn entry's byte at `+4D`. The observed farmhouse command uses selection
one and one pass. The old client recorded only passes and always called the
first export, losing selection and scope. The native negative-one pass
sentinel is also meaningful; it must not be rejected as a malformed count.

The shared command now preserves method, selection, pass count, scope and a
monotonic occurrence serial. Repeated identical starts get new serials.
Replay dispatches the matching native export within the existing residency
and spawn-job ownership paths, checks group residency and the resulting
selection/pass fields before acknowledging, and retries unavailable groups.
It adds no enemy-name list or new executable hook. Ambiguous group names
remain refused. The retained command and group tables are still bounded;
this does not replace D2 with a per-enemy host-event journal.

CONFIRMED_TEST: `StorySpawnTimingImageTest`, `StoryEffectTransportTest` and
`LanPartySessionTest` pass; runtime builds. Named exact-image checks
`SpawnStartDefaultArguments`, `SpawnStartSelectedArguments` and
`SpawnStartScopedArguments` pass with every-byte mutation refusal. The
fixture executes the real default/selected routing to an inert core sink;
scoped dispatch uses an inert export substitute and validates the native
argument-tail contract separately. It does not prove live scoped lookup.
Tests cover pass values including -1, repeated start serials, malformed
commands, codec round trips, UDP loss/reordering and old-profile refusal.
The effects fixture's obsolete invalid-kind value 14 was corrected to 255;
14 is already admitted by the earlier roster visual extension.

Presentation codec 4 and saved-story gameplay profile 22 require matched
peers; session protocol stays 23 and saves are unchanged. The payload is
3135 bytes in three bounded datagrams. Exact tests establish routing and
transport, not all-game spawning. Random spawn-point identity, independently
ticked group timers, scoped-name ambiguity, bosses/non-group spawns, revisit
and reconnect behavior still need their respective D2 proof. Owner farmhouse
visibility/removal acceptance remains open.

### 2026-10-12 — native spawn pause ownership and host world cosmetics

Supported image SHA256:
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.

CONFIRMED_EXACT_IMAGE: SoloSpawnSetup `72D50` reaches attachment `73660` through
call `72DF9`, then `737B4` calls `B1530`. Attachment floors pause references
against speed-manager `408DA0 +2A +1`, then `B17D0` releases one initialization
reference; only zero resumes its scheduler. The client registry previously
required unchanged references during adoption. `native_pause_delta=-1` is now
accepted only for a witnessed successful terminal stage, the same exact actor,
and a positive retained pause count. Named `SpawnAttachPauseRelease` mutation
checks and actual native decrement execution cover input references 2..254.
Three focused registry/residency/spawn suites pass. Owner confirms control no
longer freezes; independently randomized point identity still fails D2.

CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE: DroppedItem primary vtable `2D3E68`,
component at entity `+1C0` (pointer `+100`, vtable `2D411C`), position `+210`,
stats `+320` (pointer `+4C`) and item number as float at stats `+50`. Item manager
`408D80` has vtable `2C693C`, definition table `+0C`; definition `+14` is the item
ID and model resource `+44`. Drop component `+40` references that definition,
`+48` is retry cooldown and `+4C` marks collection. `1338A0` takes the component
as stdcall with RET4 on success and refusal. It invokes native CanAdd `219D0`,
Add `217E0` and consume `133920`; cap refusal preserves the drop and sets its
cooldown. The existing loot trace retains sole ownership of the vanilla
collector callsite; this observer installs no competing hook.

Drop destruction `133150 ->133170 ->13DB00` reaches TPtr cleanup `4D30`;
CMissile's secondary base at `+4` reaches the same cleanup through vtable
`2D919C`, adjustment thunk `187960` and destructor `1862F0`. The world observer
uses the already validated intrusive assignment `1750` to hold weak witnesses,
not raw lifetime pointers. Missiles additionally require entity/component
backlinks and a native MonsterEntity owner. SpecialEffects require the existing
exact inline component/position proof and a closed resource identity. World
transforms use native matrix `111CC0`; renderer phase requires clip zero with
consistent part clocks. Rendering copies remain cosmetic only.

`WorldVisualWeakDestruction` and `WorldDropCollect` named checks pass exact
relocated-image and all checked-byte/pointer mutations. The new image fixture
executes real weak assignment/destruction and real pickup branching with inert
inventory/consume/render endpoints. The first reuse fixture failed because it
omitted the new constructor's restored secondary vtable; that fixture was
corrected, not the lifetime guard. Eight focused suites listed in the plan pass.
No live visual or actual inventory acceptance is inferred from these tests.
The one-unit center-distance collection gate is narrower evidence than the
native collector's radius-one collision query; boundary parity remains open.

The client catalog is a closed metadata list, not transferred native assets:
79 resolved item model names/hashes, 22 encounter attack/block/missile models,
six pickup effects. Two unresolved item identifiers are excluded. This is not
all-game effect coverage. Profile 23 rejects older saved-story gameplay peers.
Full D2 individual host spawn events, D3 live visibility/pickup acceptance and
D6 reward-ledger activation remain open; see the plan's current progress log.

### 2026-10-12 — live farmhouse identity intersection strands six loaded models

Owner reports enemies frozen in mid-air or never becoming visible on the current
protected candidate. CONFIRMED_LIVE (read-only native state, not a screenshot):
over twenty paired samples with both game frame counters advancing, all fifteen
host monsters change pose. Nine client copies receive matching world updates;
six (three scarecrows and three gnats) retain identical positions and all animation
channels. All fifteen client models have render objects, and the six frozen
objects have the renderer hidden bit clear. Native SoloSpawnSetup jobs complete
with the corrected pause accounting; party/world presentation continues. This
rules out the earlier whole-client spawn pause failure for this sample.

The frozen copies have no host identity match because current identity hashes
include locally randomized spawn-point indices. 7 of the nine matching IDs
also refer to different native entry indices across peers, so matching counts
alone are insufficient acceptance evidence. A stalled early pose can explain
an invisible-looking model, but that visual interpretation remains INFERENCE.
No missing model-resource failure was observed in this sample.

**D2 owner gate FAILED / remains OPEN.** Correct individual host spawn events,
entry/pass/occurrence identity and client creation before another farmhouse
acceptance run. Do not unpause client AI, force visibility, pair by proximity,
or treat group-start replay as sufficient. No production source change or
reload was made during this diagnosis. The separate D3 loot/effect candidate
and its pending pickup/block acceptance are unchanged.

### 2026-10-12 — D2 individual host spawn implementation, owner gate pending

Implemented in the dirty `fc43d78` worktree for saved-story LAN. Each observed
native SoloSpawnSetup construction records the host's group, selected point,
low-11-bit entry index, remaining pass count, resource, placement and a unique
monotonic occurrence serial. The monster's wire identity is that serial;
reusing a point/type/entry in another wave cannot reuse its identity. Native
intrusive weak observers retain group and actor lifetime, including asynchronous
loading. Serial conflicts and journal overflow fail closed and are not published
as an empty or truncated roster.

The authenticated effects stream now carries a complete active/pending spawn
roster (maximum 64). The client invokes the native single-spawn creation path
for each previously unseen occurrence. Its spawn-group update is suppressed,
so it no longer makes independent random choices or advances spawn timers.
Complete host absence removes only completed native jobs; pending work drains.
The older point-search despawn shortcut is removed. Spawn sigils share the new
occurrence identity while retaining host-observed end timing. Automatic combat
gates keep their existing separate lifecycle adapter.

CONFIRMED_EXACT_IMAGE: supported executable SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
Native RVA `0x762B0` accepts the group in EDI with no stack arguments and returns
without popping arguments. It owns allocation, SoloSpawnSetup construction,
resource request, point weak assignment, packed entry flags and pass advance.
The existing constructor/step/destructor adapter remains the single seam owner.
Named contracts `SpawnEventChooseEntry`, `SpawnEventResourceName`,
`SpawnEventCreateAndAttach`, `SpawnEventWeakAssign`, `SpawnEventWeakDestroy`,
`SpawnEventMonsterDestroy`, `SpawnEventMonsterBaseDestroy`,
`SpawnEventGroupDestroy`, `SpawnEventGroupBaseDestroy`,
`SpawnEventEntityBaseDestroy` and `SpawnEventGroupUpdateABI` all pass, including
mutation refusal for every checked byte. The fixture executes native creation,
constructor and weak cleanup with inert allocation/resource endpoints; it does
not execute gameplay or demonstrate rendering.

CONFIRMED_TEST: six suites pass: StorySpawnTimingImageTest,
StoryEffectTransportTest, LanPartySessionTest, StoryWorldVisualImageTest,
StoryCastAdmissionTest and StoryVfxDispatchTest. Coverage includes duplicate
replay, entry/pass packing, point reuse across waves, pending-load retirement,
conflicting serial rejection, positive destruction, unknown preservation,
64-entry atomic transport, lost/reordered/stale packets, and older peer refusal.
Existing spawn-pause, sigil, gate, death-effect, cast and world-cosmetic checks
remain passing. Build and whitespace checks pass.

Saved-story gameplay profile 24 and effects codec 5 require both peers to update;
the outer envelope and save format are unchanged. Candidate DLL SHA256:
`ce1d4e49d4c02aede1678a2c4c9122876b4c4609167731820ed95bcd99827e42`.
The protected pair was reopened and read-only occurrence/pose logging armed.
**D2 remains OPEN at its owner gate:** verify the ambush and farmhouse through
at least two waves, equal host/client occurrence sets and entry/type/point
choices, advancing poses, no missing/frozen/leftover enemies, sigil endings and
continued Tal control. Other areas, exceptional non-group spawns, reconnect
while a native load is pending, and runtime teardown failure still need live
coverage. This does not establish all-game enemy coverage or close D1, D3 loot
acceptance, or the revised C5 gate. No commit, push or merge.

### 2026-10-12 — D2 farmhouse delayed health-bar/death presentation candidate

Owner reports delayed client health-bar cleanup and death effects after returning
to the farmhouse. CONFIRMED_LIVE (observation, previous dirty D2 candidate):
matched farmhouse spawn/removal observations are within one 0.2-second sampling
interval; final removal is not the measured multi-second bottleneck there.
CONFIRMED_STATIC: client damage intentionally reserves positive HP, while death
cosmetics were only replayed at final host object removal. The recorded fatal-hit
to-removal intervals span several seconds. Ambush transport delays seen separately
are not explained by this farmhouse finding and remain a follow-up.

The candidate separates presentation from removal. The existing native animation
emission adapter observes the matched monster's independent authored death effect;
its spawn occurrence retains that receipt. World snapshots now carry host death,
model visibility and the observed death style. The client creates the visual once
at the presented pose, keeps the native object until complete host absence, and
suppresses duplicate creation at retirement. No client native death, loot roll,
XP award or async task cancellation is introduced. Pending native loads still drain.

Health/name widgets use the native hidden state after feedback updates; floating
killing-hit numbers and local number settings remain separate. Exact-image native
UI entries are RVA `0x15C2D0` and `0x15C230`, thiscall/no stack arguments/RET,
through owned CStatDisplay `+0xD0` and `+0x12C`. The full native `0x129360` cleanup
also clears floating numbers, so replaying that whole routine would regress #55.

CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST: eight suites pass:
StorySpawnTimingImageTest, StoryMonsterDamageGuardImageTest,
StoryMonsterDamageFlushImageTest, LanPartySessionTest,
LanArenaSpiritVisualHostTest, StoryWorldVisualImageTest,
StoryCastAdmissionTest and StoryVfxDispatchTest. The six named health-widget
entry/return/vtable checks and eleven existing spawn contracts reject every checked
byte mutation. Native widget propagation preserves queued damage numbers and HP;
death tests cover unrelated effects, the still-present host actor, wrong identity,
repeated receipts, and no second effect at final removal. The resource test endpoint
was corrected to initialize native `0x411730`'s complete 20-byte ESI output handle,
including its ResourceName record at `+0xC`; the prior incomplete fixture depended
on stack contents. These fixture results do not prove rendered timing.

Revision: dirty `fc43d78` on the existing shared-simulation worktree; supported
executable SHA256 `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
Candidate DLL SHA256 `3df9c68d3d42d6034290cb93f85858c4dce3d3d45fe75129aa7d8eb544f86ad4`.
Saved-story gameplay profile 25 / world frame version 9 reject the prior profile;
record sizes, effects codec 5, outer envelope and save format are unchanged.

**Owner gate OPEN:** compare ordinary and chunk-producing farmhouse kills on both
screens: health bar clears promptly, killing-hit numbers remain visible when enabled,
death effects start with the host and do not repeat at removal, models disappear
with the host, later waves and Tal control continue. The ambush and other enemy
families still need coverage. No broader D2/D3 acceptance, commit, push or merge.

The pair was reloaded with this candidate. Both native spawn adapters installed;
host party invulnerability and pre-ambush placement were confirmed. Original
configuration files were restored after startup. Fresh read-only health/death
logging is active for the owner gate; this is readiness, not visual acceptance.

CONFIRMED_LIVE (native observation, same candidate): the first ambush pass
records 22 host-authored death emissions and 22 matching client creations,
77–113 ms later by the paired native clocks, with no duplicate occurrence.
For sampled dying actors, client health/name widgets are hidden within one
0.2-second sample interval; native floating-number state 2 remains observable
while those widgets are hidden. No spawn/solo fault was sampled. This is
ambush structural/timing evidence, not farmhouse or visual owner acceptance.

### 2026-10-12 — client left outside freezes on temporary house entry

Owner reports Tal's world freezing when Ailish enters the farmhouse or another
house, then world motion returning without client movement after she exits.
The earlier farmhouse health/death presentation gate is still unconfirmed.
CONFIRMED_LIVE (observed failure): the outdoor zone follower requested the host's
state-4 temporary interior through SwitchZoneNOW. The client entered a loading
scene and advanced its local epoch; presentation resumed on return, while local
control remained pending. CONFIRMED_STATIC: the retained control selection
requires the original native party epoch. A zone residency report does not
transfer a character into a temporary interior.

The candidate restricts the ordinary follower to positively observed state-3
host and local zones. A state-4 entry, missing current descriptor or ambiguous
name report grants no outdoor zone call. The runtime uses the same report check
before releasing exterior effects. Temporary travel remains with its existing
per-character owner; the control epoch check is preserved. No native pause
reference, task or retained control lease is forcibly cleared.

CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE: StoryAreaFollowImageTest,
StoryAreaStateTest, StorySceneAreaTest and StoryAreaTest pass. SwitchZoneNOW
(RVA 0x7990) and EnterZone (RVA 0x7970) byte guards are checked by name and reject
mutations. A hash-verified image fixture with recording zone-call stubs covers
host interior entry while the client stays outside, an already resident interior,
return to the same exterior, ordinary outdoor background load/entry, lighting
release refusal and an admitted local interior. It executes no real loading or
renderer. Protocol/profile and save format are unchanged.

Revision: dirty fc43d78; candidate SHA256 0e4dcfd057c57dc366a4a0d95cd873d5fc6dbd0aed9c0ced9b3e778e0dc15179.
**Owner gate OPEN:** leave Tal outside and moving while Ailish enters/exits the
farmhouse, repeat at another house, and verify his world and movement throughout
and after return. Then recheck farmhouse health-bar/death timing; the owner could
not confirm that preceding gate. No broader D2/D3 acceptance, commit, push or merge.

### 2026-10-12 — one delayed death after Tal's Spirit Strike

Owner noticed one enemy missing the other enemies' death cue. CONFIRMED_LIVE
(native observation, current house-transition candidate): the first ambush's
four actors reach zero host HP in the same 0.2-second sample. Three emit death
visuals together; the fourth emits 1.902 seconds later on the host, then the
client presents its matching receipt 86 ms later. That actor continues pose
channel 31 to its end before switching to death channel 20. All four receipts
arrive; this observation does not show a lost client death message. Why the host
continues that animation, and whether vanilla does the same, remain UNKNOWN.
No death-timing override is applied. House-entry/return and farmhouse death
visual acceptance remain open.

### 2026-10-12 — Kurian Trooper delayed death follows the authored combo transition

CONFIRMED_LIVE: the delayed actor's native resource identity resolves to
`MON_Kurian_Trooper`. The preceding capture shows host HP zero, selector 29
finishing, then selector 31 advancing to frame 42 at 24 frames/second before
selector 20 and the death emission. The client presents that emission 86 ms
after the host; the 1.902-second outlier already exists on the host.

CONFIRMED_STATIC: the shipped trooper definition selects `KURIANKNIGHT.HOM`
and `MON_Kurian.ANI`. The model animation bank maps selector 29 to combo
thrust, 31 to combo slash (42 frames), and 20 to the front big-death clip.
The animation definition maps slash to `ANIMID_ATTACK_COMBO3` (0x7A).
Its four front/back, small/big death connections all set `No interrupt = 1`
and blend time approximately 0.149902 seconds. The preceding combo-thrust
death connections instead allow interruption with zero blend.

CONFIRMED_EXACT_IMAGE: on the supported executable, the definition loader
reads `No interrupt` at VA 0x544F7A and stores it in connection byte +3,
bit 0 at VA 0x544FDC–0x544FE8. The native transition selector at VA
0x4E3B65–0x4E3B6F skips such a connection while the current channel's byte
+3, bit 0 is clear. These are read-only disassembly observations; no native
patch or injected call was used for this investigation.

INFERENCE: the authored non-interrupting slash explains the observed wait:
42 / 24 = 1.75 seconds plus approximately 0.15 seconds of transition matches
the 1.902-second delay. This resolves the preceding unknown animation-rule
cause without treating it as a lost client death event. It does not establish
whether an untouched vanilla run enters this same slash at the same lethal
moment; that live comparison remains UNKNOWN. The party-only
`lan_story_death_hold.c` is not this monster transition owner.

Scope remains the same dirty revision and saved-story candidate as the
preceding observation. No production changes, reload, death-timing override,
or additional gameplay acceptance. House-entry/return and farmhouse
health/death visual owner gates remain open.

### 2026-10-12 — scoped host exception for Kurian lethal interruption (#62)

Owner authorization: change the identified Kurian delay so death interrupts
the slash; open an audit issue for other creatures. Issue #62 tracks that
broader work. This is an explicit gameplay-policy change, not a claim that
the original non-interrupting rule was a networking defect.

CONFIRMED_STATIC: the saved-story spawn adapter owns a new host-only seam
at RVA 0xE3B65. The named KurianDeathTransitionSelect contract checks the full
253-byte native selector at RVA 0xE3AF0 (relocation-free normalized FNV
0x0E6D9CBE). EDI supplies CAnimation, ESI the four-byte connection and ECX the
current channel array. The replacement resumes native selection at RVA
0xE3B71 only for an exact host spawn lease whose resource is Kurian Trooper,
current semantic animation is COMBO3 (0x7A), HP is exactly zero, the native
arbiter is dead and CCombat selects the candidate death (0x17–0x1A).
It also requires a finished spawn job and no prior death emission.
Unknown identity, other actors/animations, positive HP, wrong component owners,
client ownership and pending jobs keep the original branch decision.

The hook preserves integer registers and x87/SSE state; common native paths
avoid the C callback. Shared animation data, channel completion flags and death
effects are not written. The existing native death predicate still chooses
the action; the authored blend and subsequent effect/loot/removal path remain.
No new client death command or wire/profile change. Failed restoration closes
override admission while retaining the live hook's continuation dependencies
for retry.

CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST: StorySpawnTimingImageTest checks
KurianDeathTransitionSelect by name with every-byte mutation refusal, then
executes the retail selector and death predicate with synthetic leased actors.
Resource lookup and transition execution are inert recording endpoints.
Original incomplete slash waits; the candidate admits each of the four selected
death actions without modifying actor/channel state or emitting a synthetic
effect. Living, other-creature, party, client, unknown/stale ownership and
pending-job cases are refused. Already-finished/naturally interruptible native
paths, foreign-patch restore refusal, retry and reinstallation pass.
Three adjacent regressions also pass: StoryWorldVisualImageTest,
StoryMonsterDamageGuardImageTest and StoryMonsterDamageFlushImageTest.

Fixture correction: the enlarged fixture exposed compiler optimization of
assembly-only references. Existing constructor arrays now retain their symbols,
and the new native call wrapper explicitly declares its referenced memory and
callback counters. The first regression failure was in that fixture contract;
the final checks execute the intended native inputs and assertions.

Revision/profile: dirty fc43d78, saved-story gameplay profile 25/world 9/effects 5,
supported executable SHA256
8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94.
Candidate DLL SHA256 4202b15dc1ef56613387efaad2a6dbfc84003f4b926801642cb63340f3ccd79e.
Build and whitespace checks pass. CONFIRMED_LIVE/owner visual acceptance remains
pending: lethal ordinary/Spirit hits during the Kurian slash, prompt death on
both screens, one effect/loot outcome and continuing waves. Other creatures and
scripted exceptions are unverified under #62. Earlier house/farmhouse gates
remain open; no commit, push or merge.

### 2026-10-12 — owner accepts Kurian outcome and requests zero-HP comparison

CONFIRMED_LIVE (owner observation): the owner reports the preceding Kurian
symptom resolved on candidate
4202b15dc1ef56613387efaad2a6dbfc84003f4b926801642cb63340f3ccd79e.
The run records installation but no override-admission event; that distinction
limits the native branch proof to its exact-image tests, without reopening the
owner's accepted visual outcome. Other creature families remain under #62.
New issue #63 records the requested vanilla/host/client comparison of zero-HP
enemies, health-bar disappearance, damage numbers, death presentation and later
waves. The owner moves the active work to the separate house-exit freeze.

### 2026-10-12 — retired farmhouse target caused the client house-return freeze

CONFIRMED_LIVE / CONFIRMED_STATIC: the failing round trip leaves the host's
current/loaded exterior at state 3 and its retained WORLD_TARGET (+0x394)
farmhouse descriptor at state 0. Story area capture omits state-0 entries but
retains the target name. The exterior predicate correctly allows the returned
host report; the follower then incorrectly promotes that stale name into a
SwitchZoneNOW request. The client moves to farmhouse state 4 and loses its
exterior party binding, while the host continues outside. The state-4 guard
already covering entry cannot distinguish this returned exterior report.

Correction in lan_story_area_follow.c: background loading requires a matching
host target residency entry in states 1–3. Missing/retired target evidence grants
no load. Positive host-current exterior fallback remains independent, allowing
ordinary outdoor convergence even with a stale background name. Native control
epoch, pause, pointer ownership and asynchronous lifetime rules are unchanged.

CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE: StoryAreaFollowImageTest reproduces the
unwanted load against the preceding implementation, then passes after the
correction. It covers retired target with local state 0 or 2, empty target,
ordinary load/entry, lighting release failure, local interior refusal and
missing-current-exterior recovery. SwitchZoneNOW and EnterZone are verified by
name with each of their five checked bytes independently mutated and refused.
StoryAreaStateTest, StorySceneAreaTest and StoryAreaTest also pass.

Revision: dirty fc43d78; saved-story profile 25/world 9/effects 5 unchanged.
Supported executable SHA256
8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94.
Candidate DLL SHA256 ba6989b68a869c32551c74257d64bd8e6b86337771ebf0cc5f6ea5f568b1f05f.
Build and whitespace checks pass. Owner acceptance remains UNKNOWN for this
candidate: farmhouse entry/stay/exit with Tal outside, another house, and real
outdoor travel. No protocol or save-format change; #63 remains a separate gate.

### 2026-10-12 — native XP popup presentation without client reward execution

Owner request: show XP above host-confirmed enemy deaths on the client.
CONFIRMED_STATIC, supported executable
8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94:
monster reward RVA D9C40 reads CStats +74, calls party experience at D99B0,
then calls the separate display entry AFB90 at D9CE9. The display ABI is
ESI=manager, five stack words (amount, XYZ, height), RET 20. It obtains a free
native pool slot through AFBE0 and calls visual setup 182E20 (stdcall, RET 24).
That path contains no gameplay death, experience award or loot execution.

CStatDisplayManager is the global at 409DBC, vtable 2CA348; its 24 owned popup
pointers begin at +1C, cursor is +7C, display enable +80, initialized flag +81.
Popup vtable is 2D9058; active +8, displayed amount +64, duration +7C, height
+80, authored vertical offset +84, elapsed +88 and initial position +8C.
Setup clamps displayed values to 9999 and adds the vertical offset. Manager
update AFA80 advances elapsed against each popup's own duration and hides the
completed visual through 182FA0. The duration can be loaded from the authored
visual; it must not be replaced with a fixed timer.

CONFIRMED_LIVE (read-only, preceding candidate): host pool slots contain
completed XP presentations; client pool slots remain unused. This corroborates
the missing display path, but does not classify the reported 128/64/0 damage
numbers. That separate comparison remains open under #63.

Implementation: lan_story_xp captures a complete positively owned native pool,
assigns a serial on activation/reuse, and emits an additive host-only cosmetic
snapshot. Reuse is detected by active state, object identity, changed immutable
popup values, or an elapsed-clock rewind. Area/manager changes suppress old
active visuals. Client presentation rechecks native pool/area ownership, uses
the native popup entry and retains elapsed lifetime; matching authored duration
is required. Per-slot serials suppress retransmission and older events. It
creates no monster, kill, XP award or drop. The existing host death receipts
continue to control enemy presentation independently.

CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE: StoryXpImageTest executes the retail
popup entry/free-slot selector with an inert recording renderer leaf. It tests
capture, same-value slot reuse, unknown owner, area fences, elapsed lifetime,
deduplication, native display disable, full pool and retry. Named contracts
XpPopup (79 bytes), XpFreeSlot (58), XpSetupHead (25) and XpSetupReturn (3) reject
each checked-byte mutation. StoryXpTransportTest proves delivery to three
observers and rejects client publication, malformed values, wrong scene,
expired receipt and stale lease. LanPartySessionTest, StoryAreaFollowImageTest
and StoryMonsterDamageFlushImageTest pass as adjacent regressions.

Dirty fc43d78; candidate DLL
4ed6a92f9162ebc2003d0d2a505f709ffcc13033da1e81409eb4c71886f108f8.
Build and whitespace checks pass. No native hook added; no save-format or
existing packet-layout change. The 828-byte XP snapshot is optional under the
unchanged saved-story profile. Live rendered fade/timing and gameplay regression
acceptance remain UNKNOWN; candidate is built, not deployed to the running pair.
Polling can miss a native popup wholly created and retired between captures;
it does not invent a reward for a death without a positively observed popup.


## 2026-10-12 — Tal stationary slope clearance and wall rebound

Supported image SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
Revision: dirty `199956d`, saved-story host correction; client presentation is
unchanged. See the [candidate, tests and owner gate](
coop-story-plan-54.md#2026-10-12--tal-lateral-idle-drift-beside-scenery).

CONFIRMED_LIVE: the prior paired capture records approximately 0.006 units of
Tal lateral excursion with roughly one-second resets. A fresh ten-second
observation has zero requested/current speed and horizontal/secondary movement
accumulators. Native velocity mode is zero; gravity velocity ranges down to
-0.2548. All 56 bounded X writes hit the native position setter (RVA 3081 after
the X store), via world-to-position publication 110C51 from collision 329FD.
Debugger export-nearest labels are not function-identity evidence.

Hardware-only contact observation identifies terrain type 1, shallow depth near
1e-6, and normal approximately (0.04070, 0.99872, 0.03002). Native 33288 adds
0.01 clearance before multiplying by the normal. Adjacent type-2 scenery
contacts have horizontal separation near (-0.845, 0, -0.534). Three observed
334A8-to-33562 responses at depths around 0.0039 produce X corrections of
-0.010078 to -0.010117 normalized units. Tal's 0.6 collider X scale explains the
roughly 0.006 world-unit reverse step. The floor clearance/wall-contact cycle is
the observed displacement source; the client mirrors the host. Watches detached
and did not write gameplay values. This does not establish vanilla behavior on
identical geometry/frame timing.

The existing stationary contact adapter now admits Tal and Buki's native
velocity mode, bounded separately from ranged integrated movement, with current
host roster/control authority. Gentle normals down to Y=0.998 are eligible;
only type-1 shallow contact receives the existing small positive clearance.
No hook seam, packet, save field or gameplay reward path is added. Wall, prop,
steep, significant-penetration and nonneutral responses remain native.

CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST: named guards IDLE_CONTACT_CALL,
IDLE_CONTACT_FRAME, IDLE_CONTACT_NORMAL, IDLE_CONTACT_SEPARATION, IDLE_CONTACT_END,
IDLE_CONTACT_THRESHOLD, IDLE_CONTACT_MARGIN and IDLE_CONTACT_HALF all reject
every checked-byte mutation. The new melee-mode regression fails before the
extension. Native synthetic slope tests pass for all four actor identities,
small and adjacent front-separation branches, both x87 precisions, movement and
nonterrain passthrough. The adjacent branch already zeros tiny horizontal
responses; an initial test assumption that both branches pushed laterally was
corrected to match executed retail behavior. Four adjacent movement/admission
suites also pass; runtime build passes with existing native-ABI warnings.

UNKNOWN: live visual result after deployment, broader terrain and roster parity.
The patch reduces excess clearance without freezing position; fixture results do
not close the owner gate or establish that every source of stationary drift is
eliminated.


## 2026-10-13 — Exterior client follows the host's loading house

Supported image only. Live (owner, castle-road farmhouse, third exit): the client
left outside background-loaded the house the host was entering and froze. The
host area report named the house as `target` with residency state 1–3 during the
native TEMP load; the follower's positive-target rule (`lan_story_area_follow.c`)
cannot tell a loading house from a loading outdoor zone, and the temporary name
travels on the presentation channel, which is not ordered against the area
report. CONFIRMED_LIVE (logs: switch → registry 35→12 → `lead=4 count=0` →
`pause_invariant_failed`) and CONFIRMED_STATIC (follower rule 1).

Host-side correction: the TEMP zone observer (`lan_story_observer.c`, exact-byte
hook already installed) notes the destination name; the area capture withholds
it as the target. Residency entries unchanged. CONFIRMED_TEST +
CONFIRMED_EXACT_IMAGE via StoryAreaFollowImageTest; live acceptance pending.


## 2026-10-15 — D6 source-specific inventory copying and native ownership

Supported image as identified in `docs/executable.md`; RVAs below.
CONFIRMED_STATIC and CONFIRMED_EXACT_IMAGE via `StoryInventoryImageTest`:
`CInventory::AddItem` 0x217E0 is thiscall `(inventory,id,quantity,show)`, RET12,
void. Globals inventory 0x408D84 / database 0x408D80 have vtables 0x2CA378 /
0x2C693C; native categories use 0x2D4184. Pickup 0x1338DB and chest 0x1850BD
call AddItem after CanAddItem, with quantity/show 1. Three export calls and
merchant 0x89DA4 are deliberately excluded. No AddItem entry hook is installed.
Money definitions are kind39, denomination +0xDC; AddItem invokes AddMoney
0x21C50 and caps the purse at 99,999. Category40 is separately established by
native inventory initialization at 0x2157E and an executed native fixture.

The earlier six-site observer overlapped `world_pickup_code`'s literal call
bytes. Collector proof now consults the seam owner for original/owned-call
identity, retaining surrounding signatures. The world image test exercises
native, owned and foreign targets. Native mutation is game-thread-only,
prepared against current globals/definition/count, then claimed once by the
transport before entry. A failed post-call proof is uncertain, never a retry.
Unknown host capture is reported to the runtime quarantine rather than silently
skipping a consumed pickup.

CONFIRMED_TEST: `StoryLootGrantTransportTest` exercises the separate D9 grant
journal, source allowlist, repeated/lost delivery and ACK, predecessor order,
capacity/backpressure, recipient lease changes and scene/disconnect behavior.
Profile26 rejects older saved-story peers. This is session-local delivery;
crash/persistent recovery is not implemented. It does not activate the older
four-character personal-wallet or entitlement foundation.

Merchant stock is non-depleting (`shop_catalog_adapter.c`); native BUY
0x89D70 calls AddItem at0x89DA4 and debits inventory+0x134. SELL is0x89DF0.
Remote item-use helpers now leave host counts untouched while the client's
accepted-result path debits its own inventory. Exact owner-gate procedure and
six executed test targets are recorded in the plan's D6 progress entry.
At initial deployment all live candidate inventory, menu, personal-use and shop
checks remained UNKNOWN; subsequent bounded pickup/chest acceptance is recorded
in the plan and the entry below.

## 2026-10-15 — D6 reward refresh on the active Weapons page

CONFIRMED_LIVE on profile26, dirty `4370296`, Ailish host / Tal client:
the farmhouse sword was awarded once to both inventories and equipped on Tal,
but the client had to reopen Weapons to see it. The active Items refresh was
already accepted. The loot handler called the Items-only refresh for every
reward, so Weapons retained its old list. Owner report and the bounded paired
trace agree; see the plan's D6 chest acceptance entry.

CONFIRMED_STATIC on the supported image: Weapons rebuild RVA0x16FC60 is
stdcall(layer), RET4; it fills the selected character's inventory family,
selects the equipped weapon and calls details RVA0x16F5B0, also stdcall(layer).
Native equip calls rebuild at0x1707E3 then ListSelect at0x1707F5. ListSelect
RVA0x17D3C0 is thiscall(list,row), RET4. The layer vtable is0x2D8904; the widget
block at layer+0x8C embeds list vtable0x2D8CF4 at+0x7D0. Selected row is+0x858,
count+0x884, row array+0x88C, item ID row+0x84. Layer+0x48=2 is browsing;
More Info uses state5. Rebuild retains its actor reference locally, not in the
layer. The existing armour-choice adapter owns rebuild/details entry hooks.

The correction dispatches reward refresh to the shown Items or Weapons page.
Weapons rebuilds through the existing owner, restores the highlighted item by
ID after rows may move, then refreshes its details. Other Weapons states defer
until browsing; only a plain pending flag survives frames. Actor ownership,
active layer and widget identity are checked around the native rebuild. Armour
choice's instance is excluded, and no second hook owns the shared native entry.
No inventory grant or equipment action is repeated by refreshing the page.

CONFIRMED_EXACT_IMAGE: `StoryMainMenuImageTest` checks the refresh signatures,
call targets, RET4 sites and input slot by name/byte mutation, plus installation
with the existing armour hooks and restoration. `ArmourMenuRefreshImageTest`
checks original/owned signatures, foreign detours, retained trampoline bytes,
metadata and executable protection, image identity, armour exclusion, and that
the proof function installs no patch. CONFIRMED_TEST:
`StoryMainMenuRefreshTest` executes retained native prologues with synthetic UI
bodies, both original and ArmourChoice-owned entries. It verifies item-ID
selection across reordered rows, More Info deferral exactly once while
unfocused, actor/inventory refusal, changed layer/widgets/actor after rebuild,
armour exclusion, missing-item fallback and cancellation on closing the menu.
These do not establish live menu behavior.
The owner must still verify an already-open Weapons page updates without
reopening and retains its selection, including a reward arriving in More Info.

Player-name substitution in acquisition notices is deferred to
[#77](https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/issues/77);
native weapon-family and equip restrictions remain unchanged.


## 2026-10-15 — Kamo exterior retention before client input ACK

The saved-story Kamo freeze was preceded by an unacknowledged client input
handoff. Host acquisition already owned Tal, but the shared activity/split
mask required READY. With outside mask zero the ordinary native TEMP path
paused the exterior and advanced the epoch, leaving client presentation
closed. The TEMP adapter is destination-agnostic; shipped SOL level metadata
contains six Kamo interiors, and none needs a new map-specific registration.

The correction derives exterior ownership from exact acquired non-draining
leases and keeps activity/input on READY. The existing AI move/snap adapter
uses a separate acquired-lease predicate during PREPARE; native player turn
still requires READY and the matching ACK. Both paths retain connection,
character, thread, host binding, menu, swap and lifetime checks. No new patch
seam, native call, or wire version. Seven focused suites pass, including named
AI byte-contract mutation checks and native TEMP enter/exit fixtures. Evidence
is bounded to tests until the owner accepts shop entry/stay/return. See the
[plan evidence and gate](coop-story-plan-54.md#2026-10-15--kamo-interiors-during-a-pending-client-handoff).

### Kamo shop interaction and independent exterior control

CONFIRMED_LIVE: the corrected delayed-ACK entry preserves the exterior epoch
and reaches client READY. Starting the merchant interaction separately raises
the native lead AI override to one and uses controller filters two/two.
`HostControlBound` treated that as lost binding and drained the remote actor;
client world presentation continued. The prior entry fix did not establish
shop-modal control acceptance.

CONFIRMED_STATIC / CONFIRMED_TEST: exterior retention now has its own exact
settled split/actor proof. The lead's script counter is not borrowed as remote
AI ownership. Exterior input additionally requires the ordinary controller
state or an exact native shop singleton/state witness. IsShopActive at RVA
`0x8D1C0` establishes the frontend/shop state fields; the supported-image test
checks its relocated singleton operand and refuses every byte mutation.
Host global readiness, host filters, new acquisitions and swap gates remain
unchanged. Runtime callbacks revalidate native identities without retaining a
dispatch witness. Opening/closing states retain suppression but gate requests.
The three focused follow-up suites and DLL build pass; live modal movement,
return/revisit, other shops/characters and multiplayer disconnect remain open.
See the same plan entry for exact test names, the fixture correction and limits.

### Kamo residency capture: executable copy-on-write pages

CONFIRMED_LIVE: the owner recheck still lost Tal and NPC movement during the
shop modal. The exact split residency snapshot was empty despite a settled
TEMP and retained exemption. Read-only Windows page queries showed the three
supported executable singleton slots at RVAs `0x408D10`, `0x408D94`, `0x408DA4`
were committed `MEM_IMAGE` / `PAGE_WRITECOPY` with the executable allocation
base. Heap world/descriptor/name pages were `PAGE_READWRITE`. The capture's
new strict heap reader rejected those image globals before copying identity.
Tal then drained and all 14 remote NPC activity leases were released; effects
continued through the separate split animation path. Closing the modal
restored a new control transaction.

The slots-only reader now accepts readable COW protections, while requiring
the original verified image allocation, exact known RVA, bounded committed
region and no guard/noaccess. Heap and native owner checks are unchanged.
CONFIRMED_TEST: `StorySplitPreflightTest` now runs the complete residency
capture on real `FILE_MAP_COPY` globals, before/after private writes, and
refuses guarded/inaccessible/wrong-allocation/unmapped cases. The previous
private-image fixture masked this production failure. The DLL build and named
`SplitImageGlobalsReadable` checks pass; the live owner gate remains open.


### Kamo doorway control delay and independent exterior steering

CONFIRMED_LIVE, saved-story profile26, dirty `4370296`: the owner accepts Tal
and NPC movement during the open shop after the executable COW-slot correction.
The remaining report concerns building entry/exit. The entry capture contains
about two seconds of host-filter hold after positive TEMP settlement; Tal's
native pause clears before that filter, leaving about1.85 seconds of avoidable
wait. Network input continues normally. No exit occurs in that capture.

CONFIRMED_STATIC / CONFIRMED_TEST: settled exterior steering now depends on its
own positive actor/residency lease and zero native pause count, independently
of host modal readiness. The native node is actor+8; Pause RVA0x1060C0 and
Resume RVA0x106100 use node+0x23 (actor+0x2B). ControlMove proves its exact lease
before treating nonzero pause as held, and submits no target/dodge/neutral move
while paused. Movement/turn use this narrower permission; block, melee,
ranged, item, equip and cast requests retain existing action-modal gates.
World/body, session/ACK, packet freshness and all identity checks remain.
No native filter or pause reference is changed.

The four focused suites, named supported-image counter/turn checks, and DLL
build pass for candidate `0aaf5b94…`; see the plan entry for names and the
floating-point fixture correction. This removes the proven policy wait only
after settled exterior proof. Pre-entry scripted travel and native entry/exit
gaps remain held; begin_exit retires the current residency proof immediately.
Return-trip timing and a live comparison remain owner gates, as do other shops,
full-roster and multi-client coverage. No claim of fully uninterrupted doorway
travel follows from these tests.

### Stationary client interrupted by host area travel

CONFIRMED_LIVE / CONFIRMED_STATIC, saved-story profile26, dirty `4370296`:
the owner's clarification concerns the client remaining outside while the
host changes areas. The return capture records a 575ms host steering hold
with fresh nonzero remote input, unchanged READY/fence/transaction, zero
sampled native actor pause, and continuing client presentation. The maximum
packet gap in that window is39ms. The current entry capture finishes initial
client admission only after the host is inside; it cannot establish an
already-controlled client's entry interruption.

The completed-return correction requires the original kept-exterior ticket,
balanced exit receipt, positive ordinary-world settlement and exact native
world, descriptor/data, party, leader, actor/AI and epoch identity. It expires
after five seconds or when ordinary host readiness returns. A provisional
entry ticket survives ordinary readiness while still inside. Actions keep
their original modal gates; native filters and pause references are unchanged.

Separately, the client preserves an already armed binding for at most250ms
from the original presentation receipt while a new global revision waits for
its matching party/world pair, only with unchanged own area and fresh exact
native owner, peer, control fence and actor generation. It sends no input,
admits no actions and replays no old presentation during that gap. Transient
combat/menu presses are consumed, including the ranged physical press count.

CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE: candidate `3c35be26…` builds, and
the split-control, split-preflight, host-filter and native TEMP suites pass
(TEMP normal and mismatch modes). Named supported-image entry/exit descriptor
ordering, shop and pause contracts actually execute; see the
[plan evidence and owner gate](coop-story-plan-54.md#2026-10-15--kamo-interiors-during-a-pending-client-handoff)
for exact names. Client input routing is source-reviewed in addition to
predicate tests. Continuous live control, pre-entry script gating, genuine
native transition gaps, other shops and multi-client recovery remain open.


### Independent exterior entry: host script scope and fresh movement

CONFIRMED_LIVE, saved-story profile26, dirty `4370296`: the owner reports that
exit is much shorter but still visible, while entry still blocks Tal outside.
Established-control entry holds repeat at about2.1 seconds, mostly before
native TEMP starts, with Tal's sampled native pause zero and fresh input
packets arriving. The latest stationary comparison contains no stale packets;
therefore input starvation does not explain that long scripted hold. Native
loading/return still contains a separate short pause of about42–46ms.

CONFIRMED_STATIC: shipped `EnterInterior|SRSBS` (hash `5BCA8FB4`, logical
`3171A`) executes the host-local DisableGamepad before its fade wait. A passive
observation in the existing shared task STEP owner now records its exact
load/task/program, argument stack, return scope, destination catalog, world,
controller and own UIStartTSA/actual native filter return. Existing remote
exterior steering can consume that proof only until native TEMP begins.
Foreign filter/TSA calls, changed pending filter, unverified child instructions,
retired/replaced task, changed program/world/roster and unknown state refuse it.
No native task, filter, pause, wait or scheduler is rewritten. Action admission
retains the prior modal restrictions. Frontend filter calls before the task
thread is known cannot arm or poison an observation; known foreign callers
still revoke it.

The client separately sends fresh movement during an unchanged-own-area scene
revision gap. Session and runtime independently require the same connection,
control fence/READY acknowledgement, actor generation and own area, with an
unextended250ms bound from original receipt. Neither layer revives old media
or enables combat/menu actions. The movement sequence and original admitted
frame acknowledgement remain distinct; no protocol change is introduced.

CONFIRMED_STATIC: `EnableFastLoad|B` (hash `C27825FA`, logical `326B`) invokes
native PauseGame/UnpauseGame (RVAs `104BE0`/`104BF0`) around loading. Their
GameSpeed::SetGamePaused route (RVA `272F0`, global pause helper `27490`) pauses
both entity scheduling and the global simulation clock. Exempting only Tal's
entity count would not provide continuous simulation. Existing split animation
ends before the final release and cannot safely be reused as an independent
physics tick. A presentation buffer can hide only its remaining admitted tail;
it cannot remove a real host-position pause. Independent clock, collision,
object lifetime and capture contracts remain open. The read-only hardware
watch expired without a transition and detached; it provides no live caller
attribution. No global pause bypass or stale-frame replay is implemented.


CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE: candidate `ea987d31…` builds and the
seven focused suites pass, with actual named supported EXE/GEX byte checks,
loopback fresh-movement checks, shared observer regression and refusal cases.
See the [plan evidence](coop-story-plan-54.md#2026-10-15--independent-client-travel-continuity-remains-open)
for exact names, test-fixture failures/corrections, bounded timing sample and
remaining limits. The final genuine COW singleton slice proves isolation from
a second shared view. Live entry/return movement remains an owner gate; the
long-hold correction does not claim continuous simulation during FastLoad.


### First settled exterior dispatch: eligibility before steering

CONFIRMED_STATIC / CONFIRMED_LIVE, saved-story profile26, dirty `4370296`:
the later owner-tested entry with `ea987d31…` retains the passive approach
proof until TEMP begins, removing the scripted two-second wait in that case.
After the unknown-roster interval, native residency is exact but the outside
eligibility mask remains zero until the end of input service. The first
settled dispatch therefore substitutes a zero movement command for fresh
nonzero input. CArbiter::SetSpeed at RVA `DB070` writes target speed and blend;
zero initiates a stop rather than preserving motion. Brief idle-to-run clips
are observed afterward; their entire duration is not proven to originate
exclusively from that command.

`refresh_split_eligibility` now validates peer/assignment/control fence and
fresh native roster/lease before the steering queries. Final post-service
mask recomputation and all native pause/action guards remain. The actual-helper
refusal cases and named supported-image host-filter/shop/pause contracts pass
in the two focused suites for candidate `bce72ec0…`. See the
[plan evidence](coop-story-plan-54.md#2026-10-15--first-settled-dispatch-submitted-a-stop)
for timings, scope and remaining owner gate. This does not claim removal of
the separate client presentation gap or global FastLoad simulation pause.

### Kamo transition: qualified owner acceptance

CONFIRMED_LIVE (owner report), saved-story profile26, dirty `4370296`, candidate
`bce72ec0…`: after testing the settled-eligibility correction, the owner reports
a minuscule flicker that is much more acceptable, explicitly not seamless.
This accepts the improvement for continuing D6 checks; the residual flicker
remains a follow-up. It does not establish zero native pause, resolve the
earlier entry before initial READY, or cover other shops/characters and
multi-client recovery. No new code or tests accompany this documentation-only
record. See the [qualified acceptance and next checks](coop-story-plan-54.md#2026-10-15--kamo-transition-improvement-accepted-with-residual-flicker).

### Residual Kamo flicker: independent camera continuation

The owner requests further reduction before D6 continues. CONFIRMED_STATIC:
the missing-pair path skips camera updates and accumulates time for the next
solve; the input-gap path also cleared camera-look axes. Candidate `f919367d…`
on dirty `4370296`, saved-story profile26, adds a separately guarded camera
callback with the normal camera's shared clock and original receipt limit.
It requires current-revision normal camera/cinematic observations plus fresh
selected-actor/native-view ownership. It neither promotes media readiness nor
replays old actors, effects or resource changes. A strict Exploration-only
preflight cannot take over unknown native states; all target checks precede
native input/refresh. The gap input discard preserves only independently fresh
camera axes, with unchanged focus and expiry rules.

CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE: the DLL and three focused suites pass;
see the [named checks and owner gate](coop-story-plan-54.md#2026-10-15--residual-flicker-independent-camera-continuation-candidate).
No live camera solve is exercised by these fixtures. The camera contribution
to the visible flicker remains an INFERENCE pending the owner test. Native
FastLoad simulation holds and the first new-revision frame's buffering behavior
remain separate limits; no zero-freeze claim is made.

### Kamo return bump: separate native movement from presentation timing

CONFIRMED_LIVE, saved-story profile26, dirty `4370296`, candidate `f919367d…`:
the owner reports entry interruption is no longer apparent, with a slight
exit bump remaining. Two returns have explicit host holds of 48/44 ms and
unchanged running clips. A positive native pause sample and its position later
displayed on the client support a genuine host-side contribution. There is no
missing-pair state on those exits, and no successful camera-only continuation
anywhere in this run; neither the visual improvement nor the exit bump is
proof that the new callback ran.

The first new-revision pair's immediate display followed by a restored
interpolation buffer can also produce a jump/hold in principle. Its contribution
to these exits remains a HYPOTHESIS. A read-only timing capture on the existing
pair will compare displayed source ticks with native movement rather than
changing buffering semantics speculatively. See the
[owner report and remaining gate](coop-story-plan-54.md#2026-10-15--entry-imperceptible-small-exit-bump-still-under-investigation).


### Small camera dip: timing correlation outside shop travel

CONFIRMED_LIVE, saved-story profile26, dirty `4370296`, candidate `f919367d…`:
a completed read-only capture confirms a new-revision displayed-tick jump
followed by rebuffering on entry and return. A separate approximately 0.01022
world-unit downward eye step on return occurs with negligible actor height
change. Four similar steps occur without a revision change; all five coincide
with 23–25 ms camera-clock increments. This narrows the investigation to the
native camera update but does not establish exclusive causation or justify
clamping its final geometry. See the [measured timing and limits](coop-story-plan-54.md#2026-10-15--return-timing-measured-camera-dip-also-occurs-while-idle).

### Exploration camera height quantization and scoped continuation

CONFIRMED_EXACT_IMAGE: `158110` computes the height spring correction using
working elapsed time squared; `157E60` clamps it to the remaining error.
The comparisons at `158019`/`158026` use approximately 0.0101/0.0099 thresholds.
Below-threshold corrections are discarded. The apply continuation at `15805C`
removes the absolute value, adds the signed correction and retains the native
collision check. Collision retries pass height-enable zero and do not reenter
this correction. This mechanism is consistent with the live step timing;
exclusive causation of the owner's return bump remains an INFERENCE.

The camera timing prefix contains input-ring interval bounds, not two missing
elapsed-time fields. The wrapper's zero bounds suppress duplicate queued input
replay; elapsed time is still supplied. Native game timing has no fixed 45 Hz
floor. A later live sample showed zero inner/outer target velocities and no
comparable dip, so stale target velocity was not demonstrated. No speculative
target refresh or timing clamp is part of the candidate.

Candidate `2d6c862c…`, dirty `4370296`, profile26 implements a single exact
six-byte seam through `lan_story_client_camera_height`. Only fresh owned-client
Exploration scope with verified native caller/context/geometry admits tiny
finite nonzero steps into the original apply/collision continuation. The bridge
preserves the floating-point environment, registers, flags and LastError.
Checked restoration runs before dependency retirement and retains the module
on failure. Host cameras do not install this adapter in this profile.

CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE: three focused executables pass, including
all new named byte-mutation checks and actual synthetic x87-branch execution,
patch failure, thread/identity refusal and restoration retry. The fixture
does not execute full-world collision or establish arbitrary concurrent unload.
See the [named checks and pending owner gate](coop-story-plan-54.md#2026-10-15--small-camera-height-correction-candidate).

### 2026-10-15 — Client item result must not replay the native script

CONFIRMED_LIVE / CONFIRMED_STATIC, saved-story profile26, dirty `4370296`:
an accepted M-item result locally calls `SudekiMpUseItemOnCharacter` with
validation bypassed. In the reproduced failure, this adds a registry entity
outside the client presentation bracket; the immediately following entity and
scheduler invariant checks fail and retain UNKNOWN containment. The host has
already started the authoritative item action. Client resources and
`SFXM005_HEAL` have separate replication paths, so native script execution is
an additional mutation, not a safe presentation operation.

Candidate `3b8f95fe…` limits accepted item-result handling to one local inventory
debit and an active Items-page refresh. A synthetic fixture executes the actual
runtime result consumer and refuses any native apply call, including a fallback
after failed bookkeeping. The native menu and world-visual image suites also
pass; named checks and evidence limits are in the
[D6 failure record](coop-story-plan-54.md#2026-10-15--d6-q-debit-without-heal-local-item-replay-freezes-client).
Live effects/control acceptance remains pending.

A separate Q attempt debited the client without a matching host request or
heal. Read-only hook inspection found the Q adapter installed and retained,
with no owned menu/open trace in that run. Its bypass route remains UNKNOWN;
do not equate removing duplicate result execution with fixing Q submission.

### 2026-10-15 — Owned Q item request and result containment accepted live

CONFIRMED_LIVE, saved-story profile26, dirty `4370296`, candidate `3b8f95fe…`:
the owned Q Items selection submits item204 to the host. One accepted result
reduces client stock3→2 without local native apply; the host action heals Tal
100→350 and the client receives that health. Host stock remains4. The owner
confirms healing and continued control. This establishes the observed Q route
and result containment, not the cause of the earlier unowned-Q debit or visual
effect parity. Corrected M-path and host-zero-stock checks remain open. See the
[qualified acceptance and next florin gate](coop-story-plan-54.md#2026-10-15--d6-q-healing-and-continued-control-accepted).

### 2026-10-15 — Chest money replication observed live

CONFIRMED_LIVE on the current saved-story D6 candidate: a native chest reward
(source2, item321, quantity1) queues one host grant and completes one client
grant. Each native purse increases by150 once, with no immediate repeat in the
27-second observation. This is chest-source currency evidence; ground-money
collection and longer dedupe transitions were not exercised. See the
[plan acceptance](coop-story-plan-54.md#2026-10-15--d6-chest-florins-credited-once-to-both-purses).

### 2026-10-15 — Tal-collected ground money shares once

CONFIRMED_LIVE, saved-story profile26, dirty `4370296`, candidate `3b8f95fe…`:
the owner completes the prepared loose-bag pickup. Native proximity collection
identifies Tal and consumes item321; one source1 grant credits150 to each
independent purse. Host655→805 and client515→665 occur once, with balances
unchanged at a fresh55-second follow-up. The host drop and client cosmetic
instance retire. This establishes ground-money sharing independently of the
earlier source2 chest reward; full area unload/re-interaction/reconnect are
still untested. See the [qualified acceptance](coop-story-plan-54.md#2026-10-15--d6-tal-ground-florin-pickup-credited-once-to-both-purses).
