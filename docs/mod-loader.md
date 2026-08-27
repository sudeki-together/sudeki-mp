# Minimal mod foothold

## Scope

Phase 4 currently produces two original 32-bit Windows PE artifacts:

- `SudekiMP.dll`: validates the loaded game, scans for confirmed signatures, reads configuration, optionally changes the Quick Menu mode request, and can install an opt-in Plasmatica diagnostic logger.
- `SudekiMP.Launcher.exe`: validates `SUDEKI.exe`, starts it suspended, loads the DLL without altering game files, calls the exported initializer, resumes only after initialization succeeds, and remains attached until the game exits.

`EnableQuickMenuNormalSpeed` defaults to `false`, and the DLL logs both the requested and applied state. When explicitly enabled, it changes only the immediate mode value in the confirmed Quick Menu activation instruction from `1` to `0`. It does not modify `SUDEKI.exe` on disk.

`EnablePlasmaticaTrace` also defaults to `false`. It is diagnostic instrumentation, not a gameplay patch. When enabled, it logs the activation lifetime and script-facing animation, camera, scripted-missile, direct-damage, and animation-speed calls made while the selected `SkillData` name is exactly `Plasmatica`. Relevant records include elapsed milliseconds from activation so normal-speed and adjusted-speed casts can be compared. All wrapper calls forward the original object, arguments, and return value unchanged.

`EnablePlasmaticaAnimationSpeed` defaults to `false`. The experimental `PlasmaticaAnimationSpeed` value is accepted only in the range `0.25` through `4.0`; invalid enabled configuration fails safely. The hook arms only when the confirmed Plasmatica task pushes `ANIMID_SKILL_02`, captures the native object at the interpreter's exact binding-dispatch call, and rejects it unless its vtable is the supported build's concrete `CNewMissileAimingGameModelAnimation` vtable. A successful application restores the model multiplier that existed before the cast. It does not alter the global game-speed state.

`EnableQuickSkillInputTrace` and `EnableRangedQuickSkillPrototype` default to `false`. The first observes Sudeki's native `ac_QuickSkill0..5` route. The second uses the game's own UI/control transition only when a ranged QuickSkill fails in the exact `Armed | Strafing` state, then retries through the unchanged validator after 75 ms. It does not write character-state flags or SP.

`EnableRealtimeMultiplayerSkillCombatPrototype` defaults to `false` and is rejected unless the normal-speed, Plasmatica trace, ranged QuickSkill, control-separation, Player 2 movement/attack, and dual-camera cache dependencies are all enabled. It adds four configurable Player 2 bindings (`SecondPlayerSkill1..4`, defaults `F1..F4`). The adapter reads that character's native `CSkill` and ordered six-entry SkillData list, calls the exact availability ABI (`EAX=SkillData`, `ESI=character+0xD4`), unchanged validator, and unchanged `CSkill::Use`. It never writes SP, damage, actor flags, or executable files. One combat context per local player records character, camera/render state, active skill, targeter, target node, and targeting/execution phase. Mod activations require game-speed current/requested modes `0` and pause byte `0`.

The supported executable currently exposes one global Skill Targeting active byte at RVA `0x003C2FCD` and one target-node pointer at RVA `0x003C3B44`. Until their entire writer/reader lifetime is virtualized, the prototype serializes that selection window: a conflicting rising edge is logged and discarded, never queued or forced. Once selection ends, another player's native skill execution may overlap. For Plasmatica, an exact six-byte inline detour at `CCameraManager::SetRenderCamera` RVA `0x00036FB0` captures only calls bracketed by the confirmed caster's script method. Cinematic render state is stored for that caster's cached viewport while `CCameraManager+0x20` and the other viewport remain on normal gameplay. Native normal-camera restore and skill cleanup both clear the viewport override. Animation and cinematic-rate multipliers remain `1.0` unless their separate per-skill experiments are explicitly enabled.

`tools/continue-research.sh --realtime-skill-coop-test` assembles those dependencies without changing repository defaults. It does not enable animation or camera acceleration. This mode has passed noninteractive PE32, state-machine, private-ABI, inline-hook, and exact-image installation/restoration tests; it still requires live two-player acceptance testing.

`EnableDirectSpiritStrikePrototype` defaults to `false`. When explicitly enabled, `[Bindings] SpiritStrike` (default `G`) submits a resolved definition through the native validator, a 75 ms native UI/control transition, a second validation, and the native activation implementation. The first parser accepts a single letter, digit, `F1..F24`, named navigation/numpad/punctuation key, or `Mouse1..Mouse5`; invalid enabled configuration fails initialization safely. Chords and controllers are intentionally deferred. `SpiritStrikeId=-1` derives the pair from the front character and uses `SpiritStrikeVariant=1|2`; a fixed ID `0..15` remains available for diagnosis. A live Ailish automatic-resolution test selected type `0x01`/ID `2` and completed normally. Native validation also rejected repeated activation attempts while a strike was active.

`EnableControlSeparationPrototype` defaults to `false`. When enabled, `[Bindings] ToggleBukiAi` (default `J`) targets Buki only when she is not the front/controller character. It calls Sudeki's exported refcounted `AiOverrideControl` and `AiDefaultControl` functions, verifies both the override count and nested AI mode, and leaves the global controller target unchanged. Two live disable/restore cycles passed: Buki stopped with refcount/mode `1/0`, then resumed normal AI at `0/1`.

`EnableSecondPlayerMovementPrototype` also defaults to `false` and requires the control-separation prototype. While Buki's verified native override is active, it submits normalized fixed-world-axis `I/J/K/L` directions to Buki's own arbiter at RVA `0x000DAE80`; Player 1's normal controller and `W/A/S/D` route remain untouched. The dedicated test launcher temporarily moves `ToggleBukiAi` to `F10` to avoid overlap with `J`. Live testing confirmed independent two-character movement and a clean native AI restore. The prototype refuses to act when Buki is the controller target or no longer belongs to the active group.

`EnableExternalInputBridgePrototype` defaults to `false` and requires control separation plus Player 2 movement. It binds a nonblocking UDP receiver to `127.0.0.1:InputBridgePort` (default `26760`) and accepts only the exact version-1, 32-byte SudekiMP packet from loopback. `InputBridgeTimeoutMs` defaults to `250`; missing or stale packets immediately produce neutral axes/buttons rather than retaining the last action. `InputBridgeDeadzone` defaults to `0.20` and applies a radial rescale before the existing native camera transform and Player 2 arbiter movement call. The left and right sticks retain their movement and per-player camera roles outside the action router. Rising button edges use the shipped Xbox-style contract: A resolves an exact interaction intent when a complete actor/target/source-generation tuple is already known, otherwise it submits native Weak; X submits native Strong; Y reports a per-seat Quick Menu intent but does not claim success because no per-seat native menu consumer is connected; B resolves modal Cancel or submits native Sweep in combat; and the D-pad reports per-seat Quickshot intents that are likewise not connected yet. Triggers still cross the protocol without a gameplay consumer. Edge state is maintained independently for four seats, and a disconnected/reconnected device must report neutral before another action edge is accepted.

The native Linux sender is built at `build/linux/bin/sudekimp-input-bridge`. It reads joydev mappings with `JSIOCGAXMAP`/`JSIOCGBTNMAP`, so it uses Linux event codes instead of fixed device-specific indices, and sends at approximately 60 Hz. `tools/continue-research.sh --controller-bridge-test` starts the helper for `/dev/input/js0`, enables dual Ailish/Buki views, the F10 native AI override, analog camera-relative movement, the 10-unit separation guard, and weak attack, then terminates only that helper and restores configuration/anti-aliasing when the game exits. Set `SUDEKIMP_INPUT_DEVICE=/dev/input/jsN` or `SUDEKIMP_INPUT_BRIDGE_PORT=NNNNN` to override its two launch parameters.

`EnableSecondPlayerCameraRelativeMovementPrototype` defaults to `false` and requires the movement prototype. It passes the local `I/J/K/L` vector through Sudeki's own callee-cleaned movement-camera transform at RVA `0x000291A0`, clears vertical motion, normalizes the horizontal result, and then uses the already-proven Buki arbiter path. Its exact entry signature and inert-image installation pass; a live run confirmed Buki's direction rotates with Player 1's camera.

`EnableSecondPlayerSeparationGuardPrototype` retains its historical name, defaults to `false`, and requires Player 2 movement plus the integrated menu/split-screen overlay path. Standalone and single-camera combinations are rejected instead of enabling a boundary that can never become visible. It now enables a symmetric roaming-only co-op boundary for both players. The policy becomes eligible only after two human-controlled characters, the native party/controller state, world resources, Exploration camera, and non-combat state remain settled for 250 ms. It becomes inactive immediately for combat, loading, authored/non-Exploration cameras, travel/votes, drop-out, or a disconnected external input bridge. Both viewports receive an amber warning at 80% of `SecondPlayerMaximumSeparation` (experimental default `10.0` units). At the hard limit, outward, lateral, and numerically near-lateral requests are blocked for either player; only movement with a clear inward radial component is accepted. Hard blocking additionally requires the warning overlay to have rendered successfully; a missing/reset D3D device therefore fails open instead of creating an invisible wall. No position, ownership, speed, camera, or transition state is written.

`EnableSharedGroupCameraPrototype` defaults to `false` and requires both control separation and second-player movement. It accepts the live-confirmed standard gameplay pair (`OffsetTarget` in `CCamera+0xB4`, `GameObjectTarget` in `+0xB8`) or the native same-`GameObjectTarget` pair. It retains both originals, creates Sudeki's native `MatrixTarget`, preserves Player 1's native framing offset, and translates focus to the Player 1/Buki midpoint. Every installed slot receives the caller-owned reference expected by RVA `0x000E84C0`; restoration changes only slots still owned by the prototype and releases all held references through the native manager. The user confirmed midpoint tracking and immediate, crash-free return to Ailish focus and Buki AI. This first prototype changes focus only; zoom, distance limits, collision, scope cameras, cinematics, and render views remain unchanged.

`EnableSecondPlayerCameraPrototype` defaults to `false` and requires `EnableSplitScreenRenderPrototype`. The first live version proved native named-camera selection/restoration but produced an invalid skybox view and held a doorway transition while Player 2 remained globally selected; that path is rejected. The replacement still creates `SudekiMP_P2`, but never assigns it to `CCameraManager+0x20`. At the exact main-render boundary it copies Player 1's already-valid native render matrix, translates its camera position from Player 1 to the first non-front party member, temporarily assigns only the scene renderer's camera-state pointer, and restores Player 1 before native `EndScene`. `[Bindings] ToggleSecondPlayerCamera` defaults to `F9`; both diagnostic halves still duplicate the selected finished frame. The live test produced a correctly centered Buki view with no skybox, freeze, missing geometry, or doorway failure while the log kept Ailish as the unchanged global camera. Close doorway framing remains a known consequence of inheriting Player 1 orientation/distance and translating only position. This is a render-isolation/framing proof, not simultaneous output or independent Player 2 rotation/zoom.

`EnableDualCameraFrameCachePrototype` defaults to `false` and requires both camera-render options above. It changes the focused test from manual F9 view selection to automatic alternating render states. Sudeki still executes exactly one native render per engine frame. After `EndScene`, the compositor copies the clean full-size result into matching Player 1 or Player 2 render-target textures; once both caches are valid, it scales Player 1 to the left half and Player 2 to the right. Each view updates every other engine frame and can be one frame old. Live testing showed distinct Ailish-left/Buki-right views with no noticeable jitter or abnormal rendering; native mouse rotation/zoom affected both views while each stayed centered on its assigned character. This remains a stepping stone to final same-frame rendering. While `CPCQuitScreen+0x1C2` is visible, the cache stops updating. A call hook immediately before the native Quit renderer draws both retained textures as a frozen split backdrop, restores all D3D state through `D3DSBT_ALL`, and then lets Sudeki draw one unchanged full-width interface. Live testing confirmed the frozen dual-camera background and clean Back/resume behavior. Viewport-owned HUD is not part of this test.

`EnableSpiritStrikeViewportEffectIsolationPrototype` defaults to `false`. The focused cleanroom profile may enable the current prototype after an exact-build check. Exact-build analysis found one global `cD3DMotionBlurPostEffect` history target shared by its composite callback (RVA `0x001DE0B0`) and screenshot callback (RVA `0x001DE7B0`). The rejected versions skipped callbacks and could stall the native render-job queue at RVA `0x001DFAF9`. The current prototype instead creates one additional native `_RenderTarget` through Sudeki's factory (RVA `0x001F6C70`) and temporarily swaps only the history pointer during the complete native callback, restoring it immediately afterward. During an active Spirit presentation, the transient native UI/Quick-Menu gate is overridden so both viewport renders remain live; ordinary Quick Menu and exit-menu behavior is unchanged. The callback lifecycle, completion byte, Player 2 render path, and input remain native. The resource is intentionally retained until process exit because its destructor ABI is not yet confirmed. This remains an exact-build experiment pending Iron Warrior startup-camera and Blade Dance live acceptance; it must remain disabled in general profiles until that test passes.

`EnableSecondPlayerWeakAttackPrototype` defaults to `false` and requires the same control-separation prototype. While Buki's verified native override is active, rising edges from `[Bindings] SecondPlayerWeakAttack` (default `U`) submit only the weak-attack state to Buki's own arbiter through RVA `0x000DB0E0`; all other combat states are zero. The ABI adapter and exact-image install/restore tests pass under Wine. In the live proof the user completed a battle while one stable Buki arbiter repeatedly entered the native `IsAttacking` state, and nearest-target lock-on remained visibly active. Target selection and all attack/state validation remain native.

`EnableSecondPlayerTargetTrace` defaults to `false`. While Buki's verified override is active and Sudeki owns the foreground, it samples her unchanged `CTargeter` at no more than 10 Hz and logs only changes to the ordinary target node and auto-target flag together with Buki's forward vector. The trace never calls targeting logic and never assigns, clears, scores, or replaces a target. Build, inert-image preflight, and the first live capture pass.

`EnablePlayerMovementTrace` defaults to `false`. It wraps only the two exact calls from the global controller's normal movement consumer to the per-character arbiter movement routine, samples the unchanged world direction and movement parameters, and forwards immediately. The live trace confirmed the expected normalized horizontal direction, speed, turn rate, movement mode, and controlled character. It does not synthesize or redirect movement.

`EnableFreeRoamCameraModifierPrototype` defaults to `false` and remains an unsuccessful diagnostic prototype. It gates native mouse-Y `CameraU/CameraD` events outside combat behind configurable `[Bindings] FreeRoamCameraModifier` (`LeftCtrl` by default), while leaving combat input unchanged. The modifier edge was observed live, but the user did not obtain a useful camera result. Earlier wheel remap, held-pulse, and late controller-field injection attempts also failed visibly. This option must not be treated as a completed camera feature; further work belongs at the native desired-distance/profile update path.

`tools/continue-research.sh --spirit-strike-test H` temporarily selects `H` in the generated test configuration, then restores the repository default after Sudeki exits. This provides a direct live check that the configured key—not a compiled-in `G` constant—drives activation.

`tools/continue-research.sh --control-separation-test` temporarily enables only the guarded Buki toggle and restores the generated configuration after Sudeki exits. `--party-lifecycle-trace` opts into the symmetric roaming boundary together with roster/drop-in and party-atomic TEMP transitions; the repository INI remains default-off. It deliberately leaves `EnableTransitionVotePrototype=false`: the current late `EnterTemporaryZone` adapter cannot safely veto an approach/script that Sudeki has already started.

`tools/continue-research.sh --second-player-movement-test` temporarily enables the guarded Buki toggle and second movement source, uses `F10` for the toggle, and restores the generated configuration after Sudeki exits.

`tools/continue-research.sh --second-player-attack-test` adds the disabled weak-attack prototype to that setup and binds it to `U`. Control anyone except Buki, use `F10` to acquire/release the native AI override, move Buki with `I/J/K/L`, and tap `U` once to request her weak attack.

`tools/continue-research.sh --second-player-camera-movement-test` selects only the shared-camera movement follow-up. Its live run confirmed that Buki's independent input rotates with Player 1's shared camera while Player 1 retains camera focus. `--second-player-separation-test` additionally enables the experimental visible 10-unit roaming boundary. At the hard limit the current policy accepts only a clear inward radial request; outward and lateral requests are blocked for both players.

`tools/continue-research.sh --shared-group-camera-test` adds the live-confirmed native midpoint target to the camera-relative movement and separation setup. Use `F10` to acquire/release Buki's AI override; the camera follows the P1/Buki midpoint and restores native P1 focus when the override ends. The launcher does not enable adaptive zoom in this mode.

`tools/continue-research.sh --second-player-target-trace` enables only Buki's native AI override plus the passive target trace. It is intended to compare target changes before attacks, during Player 1 combat, and around enemy death without synthesizing Player 2 input. The first live run retained one stable non-null node and the enabled auto-target flag throughout the override, then restored Buki's AI cleanly.

That live check passed: initialization logged virtual key `0x48`, and pressing `H` completed the same validated Ailish Spirit Strike with activation result `1`.

The runtime now also has a process-global player-statehood coordinator for
actor leases and interaction provenance. The old controller-X targetless
`GENERIC_REQUEST` and `P2 INTERACT?` badge path has been retired. The legacy
`EnablePlayerInteractionRequestsPrototype` key now gates a passive exact-build
trace only when the zone-transition observer is also active. It records the
native Select/OnAction source actor, the bounded candidate set, the accepted
message path, and the same-thread SOL submission against a nonzero world-source
generation. Controller A publishes intent only: the runtime never replays GUI
Select, swaps the global controller, bypasses native eligibility, or generates
a targetless/native world action. A non-front/P2 candidate remains explicitly
unvalidated and cannot authorize activation. Known shops and blacksmiths remain
one serialized shared modal, while travel, dialogue, quests, saves, and
cutscenes remain host-owned. See
[player-statehood-design.md](player-statehood-design.md) for the ownership
contract and staged interaction plan.

`EnablePerPlayerBlacksmithUiExperiment=false` is an exact-build, default-off,
historical data-isolation experiment. Its custom P1/P2 panels proved that two
actor-specific read models can coexist, including equipment, sockets,
compatible native rune rows, prices, and projected stats. Those panels are not
the intended interface. The product target is a separate native Blacksmith
window and native-style interaction lifecycle for each player. Forge/purchase
commits and all native inventory/money mutation remain intentionally disabled
until the interaction carries exact merchant/actor provenance, native UI-layer
state is safely virtualized, and the serialized commit path is live-verified.
`--party-lifecycle-trace` temporarily enables the old preview and prints
`preview only, no forge commits`; the checked-in INI remains false and every
failed prerequisite falls back to Sudeki's native full-width blacksmith.

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
