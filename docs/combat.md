# Combat research

## Quick Menu slowdown

### Confirmed statically

- Quick Menu activation requests `CGameSpeed` mode `1` by writing `1` to the singleton's `+0x24` field at VA `0x00498EF0`.
- Quick Menu deactivation requests mode `0` by writing zero to the same field at VA `0x004991B6`.
- The main frame-time path transitions requested mode into current mode, selects `0.07f` for nonzero modes other than mode `4`, and multiplies frame delta by the selected value.
- Mode `0` selects the normal-speed global, initialized to `1.0f`.
- The full-pause byte is a separate `CGameSpeed+0x28` field; when set, the frame path supplies a zero delta.
- A separate master-speed multiplier is applied earlier in the timer path. The Quick Menu methods do not write it.

### Runtime-confirmed mechanism

The Quick Menu slowdown is a global simulation-delta scale implemented through `CGameSpeed` mode `1`. It is not a full pause, an AI-only slowdown, or merely an animation-speed adjustment. The vanilla scale is `0.07x`.

The coordinated Wine runtime trace confirmed:

```text
vanilla menu open:  current mode 1, requested mode 1, paused 0
patched menu open:  current mode 0, requested mode 0, paused 0
menu active bytes:  object+0x29 = 1, object+0xFC = 1
```

### Milestone 1 experiment

The activation instruction's immediate operand was changed only in live process memory from mode `1` to mode `0`:

```text
C7 40 24 01 00 00 00
            ↓
C7 40 24 00 00 00 00
```

With the Quick Menu visibly open, both of its active bytes were `1`, current/requested speed modes remained `0`, the full-pause state remained `0`, and the user observed the world moving at normal speed. The menu could be closed normally.

Result:

```text
Quick Menu remains visible
world simulation remains at normal speed
closing the menu leaves normal speed intact
```

The original process-memory bytes were restored after the test. Both executable copies retained the expected SHA256; no game file was patched. This completes Milestone 1.

Do not change the global `0.07f` constant in a future hook because modes `2` and `3` also use it and their purposes are unknown. Do not rely on `SetGameSpeed(1.0)` globally because mode `1` selects the fixed alternate scale instead.

### Runtime relocation note

Wine loaded `d3dx9_30.dll` at `0x00400000`, so `SUDEKI.exe` was relocated to `0x79CC0000` during this run. The activation and close instructions therefore appeared at `0x79D58EF0` and `0x79D591B6`. Future tools must use the loaded Sudeki module base plus RVA or scan its `.text` section; they must not assume the preferred image base is available.

## Phase 5 — Elco Plasmatica

### Confirmed activation flow

```text
Quick Menu call at RVA 0x000998A1
  -> CSkill::Use(slot)
  -> validate actor, skill, active state, and SP
  -> subtract SkillData+0x94 cost
  -> emit OnSkillStarted
  -> set active byte and selected equipped slot
  -> retain returned script/task handle
  -> per-frame completion check
  -> release actor skill state
  -> emit OnSkillEnded
```

The observed equipped-slot argument was `1`, while the selected data object directly contained the UTF-16 display name `Plasmatica`. Runtime hash-table lookup maps the live skill instruction offsets to compiled script `PC_Elco1__Skill|P`. Earlier `PC_Elco3` and `PC_Elco2` identifications were incorrect.

The first clean runtime trace observed no call to the generic attack entry points and no call to `SetAnimationSpeedMultiplier` during activation. Plasmatica therefore has a script/task-controlled path that still needs to be traced for animation requests, projectile/effect spawning, damage timing, and completion.

A broader follow-up GDB trace caused an access-violation crash immediately after activation and produced no valid downstream events. That method has been retired. Further live observation should be implemented as narrowly scoped, exact-build-gated in-process logging in `SudekiMP.dll`.

### First safe logger result

Two Plasmatica tasks completed normally through the in-process DLL logger. Each produced a successful `CSkill::Use`, an active task handle, and a matching completion event. No debugger was attached and the game remained running.

All fourteen script-facing export-table wrappers reported zero calls during both task lifetimes. This is evidence about the export redirection method, not evidence that the underlying engine methods were unused. The next investigation must follow `PC_Elco3__Skill` into the script/native dispatcher or its cached binding table.

Static inspection identified opcode `0x27` as the script/global-call path. It consumes a 32-bit call hash, checks compiled script functions, and then falls back to Sudeki's internal native-binding registry. A disabled-by-default trace of this exact opcode slot passed inert-image installation and restoration tests, then captured the live call path described below.

### Opcode-dispatch capture

One live Plasmatica cast completed normally with the opcode `0x27` logger enabled. The primary skill thread was `0x00B50D8C`; its first Plasmatica instruction was at runtime bytecode offset `0x000AACBA`. Reconstructing the GEX function ranges from their uncompressed lengths places that address inside:

```text
PC_Elco1__Skill|P
hash:  0x2F41B420
range: 0x000AAC9F–0x000AAF66
```

This is runtime confirmation that Plasmatica is Elco skill script index `1`.

Confirmed calls on the primary thread or its synchronous nested path include:

- `CacheTSAFunction` and `PreLoadTsaAnimState|PR`
- `Speech_StopPlayerHelp`
- `PlaySpotCue|S` and `GetSound`
- `GetScriptedAnimationController`
- `StartConeTargetedSkill|P`, `SetTurnRate3rdPerson|PN`, and `PlayRailSkillTargettingEffect|P`
- `TsaPlayCamera|PRSNNBS`, `TestCameraCollision|PR`, and `SetRenderCamera|S`
- `GetCurrentTsaAnimation|P`
- `IsPlaying|P`, polled 233 times at Plasmatica bytecode operand offset `0x000AAEA4`
- `RemoveLightEffect|N`

The repeated `IsPlaying|P` loop proves that task duration waits on a playing object rather than only on a fixed script delay. A focused receiver capture identified the argument as a `GELGroupPtr` whose live pointee has RTTI `ElcoEntity`. The compiled `IsPlaying|P` wrapper then calls `GetComponent("CNewGameModelAnimation")` followed by `TsaIsPlaying`. The wait therefore tracks Elco's game-model animation state, not a free-standing camera or effect handle.

### `IsPlaying` receiver capture

One normal Plasmatica cast produced the same argument on every sampled poll:

```text
script argument: 0x07F5B390
RTTI class:      GELGroupPtr
wrapped pointer: 0x07CC15B0
wrapped RTTI:    ElcoEntity
skill task:      0x07F5B438
```

The addresses are observations from one run and are not stable patch targets. The receiver wrapper's vtable was at module-relative RVA `0x002C0098`; its RTTI type descriptor is `.?AVGELGroupPtr@@`. Reading the wrapper's pointer field at `+0x0C` led to an object whose vtable RTTI type descriptor is `.?AVElcoEntity@@`.

The opcode handler did not synchronously replace the stack argument with a Boolean result. The logger's original `result=` label was therefore incorrect; it represented the stack value immediately after dispatch and still contained the receiver address. The source now labels it `stack_after_dispatch=`. No true/false transition has yet been captured.

### `IsPlaying|P` wrapper

The runtime compiled-function table resolves hash `0x890F6EB1` to bytecode offset `0x00003E22`. Its next function begins at `0x00003E44`, giving this wrapper a length of `0x22` bytes. Decoding its two object-method operations gives:

```text
opcode 0x28 at operand 0x00003E38: GetComponent|N (0xB2F8076C)
  component type: CNewGameModelAnimation (0xC5D2509B)

opcode 0x28 at operand 0x00003E3D: TsaIsPlaying (0xDC187AFB)
```

Sudeki exports both `CNewGameModelAnimation::TsaIsPlaying` variants at RVA `0x0000F2D0`. The implementation returns true when the animation object's state byte at `+0x131` equals `3`.

### Plasmatica event and missile sequence

The primary-thread opcode `0x28` capture establishes this order inside `PC_Elco1__Skill|P`:

```text
enable skill targeting
  -> poll ShouldSkillTargettingModeBeActive (181 calls in this run)
  -> disable skill targeting
  -> disable auto targeting and clear its current target
  -> create and attach an event watch
  -> poll HasArbEventOccured at 0x000AAE2C (216 calls; final result true)
  -> clear the event list
  -> poll HasArbEventOccured at 0x000AAE60 (248 calls; final result true)
  -> GetComponent(CMissileManager)
  -> FireMissileScripted(10)
  -> clear the event list
  -> poll IsPlaying(Elco) / TsaIsPlaying (233 calls)
  -> unlock the animation controller and wait for it to become inactive
  -> restore the normal render camera
  -> re-enable auto targeting
  -> animation/effect cleanup
  -> destroy the event watch
  -> stop rumble, restore the control filter, and finish the skill task
```

The missile identifier is the script number literal `10.0` immediately before the `CMissileManager` component lookup and `FireMissileScripted|N` call. The launch call is at bytecode operand `0x000AAE89`. It occurs after the second watched animation event and before the long animation-completion wait.

### Target retention at missile launch

`CTargeter::StartSkillTargetting` at RVA `0x000B9E20` activates the global skill-targeting state and clears its refcounted target node. `CTargeter::EndSkillTargetting` at RVA `0x000B9EF0` deactivates that state and clears the node again. Independently, `CTargeter::EnableAutoTargetting(false)` at RVA `0x000B9CC0` clears the ordinary current-target node at `CTargeter+0x54` and clears auto-target flag bit `0x02` at `+0x84`.

A read-only snapshot at the exact `FireMissileScripted(10)` method call produced the same result in five casts, including one cast where the cinematic camera was rejected, one where it was accepted, and three later confirmation casts. At launch, the global skill-target node was null, skill-targeting was inactive, `CTargeter+0x54` was null, and auto-targeting was disabled. Camera acceptance did not change any of these values.

Plasmatica therefore does not retain a live enemy pointer in either target system until projectile launch. The selected line is committed before the execution animation, after which the script intentionally turns both targeting systems off. It must not be described as a homing target lock.

Native launch-direction resolver RVA `0x000C7AA0` has three ordered sources: a live ordinary target position when eligible; a ray derived from the active aiming camera when owner aim-mode flag `0x00400000` is set; otherwise the owner transform's forward vector at offsets `+0x50..+0x58`. In the final standard-combat capture, Elco's aim-mode flags were `0x00080812`, the camera-ray bit was clear, and the resolver selected the actor-forward branch. The captured normalized forward-vector bits were `3F55D7CE,00000000,3F0CBCEB`.

For the observed Plasmatica path, target selection therefore turns Elco toward the selected line, targeting is torn down, and the later missile launch uses the committed actor-facing vector. The resolver still supports live-target and camera-ray branches for other missile contexts.

### Recovery and restored state

After Elco's model animation reports finished, the shared animation helper calls `TsaUnlock` at bytecode operand `0x00003F84` and polls `TsaIsActive` at `0x00003F8F` until false. `PC_Elco1__Skill|P` then restores the normal render-camera resource at `0x000AAEC9`, calls `EnableAutoTargetting(true)` at `0x000AAEE2`, performs the remaining animation/effect reset, destroys the event watch at `0x000AAF08`, calls `StopRumble` at `0x000AAF2C`, and reaches the `SetControlFilterAll` wrapper at `0x00005880`. The setup side had called `SetControlFilterNone` at `0x00005857`.

Native `CSkill` completion then releases the task-owned invulnerability/skill-lock state described below. This establishes the normal recovery order without assuming that the script binding names describe the internal bit representation of the control filter.

No `DoDirectDamage|N`, `ModifyHitPoints|NB`, `HitEntity|PB`, or `CausePoison|PNNN` object-method call occurred on any script thread during a complete Plasmatica lifetime. This rules out those four known script-method paths for the observed cast. Static analysis independently follows `FireMissileScripted` into the native missile and `CCollisionDamage` path, confirming native impact and damage ownership.

### Native missile and attack data

`CMissileManager::FireMissileScripted(int)` at RVA `0x000C89C0` selects a `MissileData` entry and launches one or more native `CMissile` objects. Initialization at RVA `0x00186E10` registers the missile's collision body through RVA `0x000EC200`. The active update at RVA `0x001867D0` advances movement and lifetime and handles ground/wall impacts; its call through RVA `0x000DCD00` creates positional impact SFX and is not character damage. Missile termination is handled at RVA `0x00186610`.

Qualifying character contact reaches the collision geometry bridge at RVA `0x00032A80`, which directly calls the `CCollisionDamage` handler at RVA `0x00138870`. That handler observes the configured multiple-hit delay, constructs a `DamageStructure`, copies damage/knockback/effect/status data into it, and dispatches it through RVA `0x000DAB50`. The accepted packet reaches RVA `0x000D21D0`, where mitigation is applied, the result is clamped, and the target's current HP is written at combat-data offset `+0x2C`. This completes the native launch-to-HP chain with high confidence.

The user-owned `SOLData.baf` serializes Plasmatica as one projectile with velocity `17.0`, range `100.0`, wall and ground collision enabled, and no penetration or bouncing. Its primary attack record has serialized base damage `500` and links to `PlasmExplosion`, whose serialized base damage is `300`. This does not establish `800` damage: the primary record says the secondary starts `Never`, and the conditions that may activate the linked attack remain unconfirmed.

The script literal passed to `FireMissileScripted` is `10`, while Plasmatica is element `2` of the serialized `MissileCombos` list. Their mapping has not yet been reconstructed and they must not be treated as the same index.

### Caster protection

Plasmatica receives the engine's generic `CSkill` protection for the complete native task lifetime. `CSkill::Use` calls RVA `0x000DC200`, which increments `CCharacterArbiter+0x54`, sets arbiter state flag `0x800`, and sets `IsUsingSkill` bit `0x10`. Exported `CCharacterArbiter::IsInvulnerable()` at RVA `0x00008980` returns whether that same `+0x54` refcount is positive. The completion path at RVA `0x000B47A0` decrements the refcount, maintains or clears `0x800`, and clears `0x10`.

The bytecode for `PC_Elco1__Skill|P` contains neither the global nor class-method hash for `GELSetInvulnerable|B`; the protection is inherited from `CSkill::Use`, not applied specially by Plasmatica. No independent damage-reduction scalar has been identified. Static evidence therefore says the caster is invulnerable, and consequently protected from ordinary damaging hit reactions, until the skill task completes. A later hostile-contact runtime test can confirm whether any special damage category bypasses the generic flag.

### Plasmatica animation identity and speed control

The resource/hash dictionary maps the value pushed at bytecode operand `0x00004195` exactly:

```text
0xB0242A96 = ANIMID_SKILL_02
```

The receiver of `TsaPushAnimationState` is Elco's `CNewGameModelAnimation`. MSVC RTTI shows that class derives through `CSkinnedGameModelInterface` from `CSimpleGameModelInterface`, all at object offset zero. It is therefore valid to pass the observed receiver directly to the exported per-model speed functions.

`CSimpleGameModelInterface::SetAnimationSpeedMultiplier(float)` at RVA `0x000E0460` stores the supplied multiplier at object offset `+0x48`, multiplies it with the factors at `+0x44` and `+0x4C`, stores the effective speed at `+0x50`, and notifies the concrete model through a virtual call. Reset at RVA `0x000E04B0` supplies `1.0`; get at RVA `0x000E04F0` returns the effective value at `+0x50`.

The first 1.5x runtime experiment did not make Plasmatica meaningfully faster. Its log claimed that object `0x0838F818` had prior multiplier bits `0x00145DB0`; that implausible denormal value proved the object was the script-side wrapper, not the native animation component. The hook restored the same bits after the cast, and the cast's event timing remained essentially vanilla.

The next safe runtime capture obtained native object `0x07C5825C` with vtable RVA `0x002D5464` and rejected it without writing because the initial gate expected the base class's own vtable. RTTI identifies the actual concrete type as `CNewMissileAimingGameModelAnimation`; its hierarchy contains `CNewGameModelAnimation`, `CSkinnedGameModelInterface`, and `CSimpleGameModelInterface`, all at offset zero. Reading the untouched object confirmed `+0x48` held `1.0`. The cast again completed with vanilla timing.

The corrected experimental hook remains disabled by default. It arms on the same exact Plasmatica method/animation tuple, obtains the resolved native object from the interpreter's binding-dispatch call at RVA `0x001C4C2F`, and requires the exact Elco concrete vtable RVA `0x002D5464` plus a finite, positive prior multiplier. It then preserves `+0x48`, applies the configured multiplier before the animation push, and restores the exact prior value during normal skill cleanup.

Two live casts at `2.0x` confirmed the mechanism. Both applied to the same persistent native animation component with previous bits `0x3F800000`, used multiplier bits `0x40000000`, completed normally, and restored exactly to `0x3F800000`. Compared with the preceding normal-speed cast, the first arbiter event changed from poll `201` to `101`, the second from `249` to `125`, and `TsaIsPlaying` completion from `233` to `117`. These near-halves are direct evidence that Elco's animation—not global simulation—was doubled.

### Plasmatica camera collision and timing

Follow-up observation-only traces resolved the apparent missing eye-view camera. The relevant primary-thread sequence is:

```text
TestCameraCollision
  collision-free -> TsaPlayCamera
                 -> TsaPushAnimationState
                 -> poll GetCurrentTsaAnimation
                 -> SetRenderCamera(0xEE1B1485)
  collision      -> SetRenderCamera(0x933B5BDE) immediately

caster animation and missile sequence
  -> SetRenderCamera(0x933B5BDE)
  -> finish skill
```

`TestCameraCollision|PRN` has hash `0x8232C4CA` and is called at bytecode operand `0x0002FC0A`. In the rejected cast it returned `1`; neither `TsaPlayCamera` nor the cinematic render-camera selection followed. In the accepted casts it returned `0`, followed by `TsaPlayCamera|PRSNNBS` (`0xF69C244A`) at `0x00038CEF`, `GetCurrentTsaAnimation|P` (`0xB6171BB6`) at `0x00038D1F`, and the cinematic `SetRenderCamera|S` (`0x61F821BD`) at `0x00038D56`. The normal-camera restore uses the same `SetRenderCamera` binding at `0x000AAEC9` with resource hash `0x933B5BDE`.

The user's visual observation agreed with this control flow: the cinematic view appeared only when Elco had enough surrounding space, and its authored movement presented a variable-looking three or four visible changes. Those visible changes were not three or four separate render-camera selections; each successful trace contained one cinematic selection and one normal-camera restore.

One accepted normal-speed cast and two accepted `2.0x` casts produced these wall-clock timings:

| Event from skill activation | `1.0x` | `2.0x` cast 1 | `2.0x` cast 2 |
| --- | ---: | ---: | ---: |
| Collision test / camera setup begins | `3.032 s` | `3.026 s` | `3.027 s` |
| Cinematic render camera selected | `3.298 s` | `3.279 s` | `3.281 s` |
| `FireMissileScripted(10)` | `10.746 s` | `7.014 s` | `7.014 s` |
| `TsaIsPlaying` becomes false | `14.628 s` | `8.949 s` | `8.948 s` |
| Normal render camera restored | `14.883 s` | `9.203 s` | `9.202 s` |
| Skill task ends | `15.003 s` | `9.323 s` | `9.322 s` |

The cinematic-camera selection delay remains approximately `0.25 s` after the collision test at both speeds. The interval for which the cinematic render camera remains selected changes from `11.585 s` at `1.0x` to `5.924 s` and `5.921 s` at `2.0x`, almost exactly one half. This confirms the speed hook does not skip the camera path: the collision gate determines whether it is selected, while its held presentation window ends with the independently accelerated caster-animation sequence. A preferred gameplay multiplier is still a balance and presentation decision; `2.0x` remains an experimental proof value.

### Plasmatica camera playback speed

The shorter render-camera window initially showed only three of the four authored camera angles. A focused trace resolved `TsaPlayCamera` to compiled bytecode offset `0x00004532` and identified its native handoff:

```text
CSpiritCam::SetTargetCamera(0xEE1B1485)
  -> SetCameraSwitching(false)
  -> SetStickOnEndFrame(false)
  -> StartCam(..., ANIMID_SKILL_02, -1, 1.0, ...)
  -> wait for ANIMID_SKILL_02
  -> SetRenderCamera(0xEE1B1485)
```

`StartCam|PPRNNS` has hash `0xEBDE4799` and is called at bytecode operand `0x000045EF`. The exported native implementation is `CSpiritCam::StartCam` at RVA `0x00012910`. Its signature contains one float; the observed value was `1.0`. Native code stores it at `CSpiritCam + 0x1D4`, then multiplies it by a second internal factor and `24.0` while configuring the camera animation.

Two broader candidates were disproved first. Changing the visible `1.0` in the higher-level `TsaPlayCamera` argument list did not alter the camera, and a hook on `Camera::PlaybackSequenceState` received no calls during accepted Plasmatica casts. Both failed patches were removed.

The corrected experiment changes only `StartCam`'s exact float argument. It requires the active Plasmatica primary script thread, the accepted-camera setup window, bytecode operand `0x000045EF`, method hash `0xEBDE4799`, seven stack words, `ANIMID_SKILL_02`, mode `-1`, and an original rate of exactly `1.0`. With both caster and camera configured at `2.0x`, three consecutive casts logged an effective camera rate of `2.0`, completed normally at approximately `9.23 s`, and restored Elco's model multiplier to `1.0`. The user confirmed all four cinematic camera angles appeared before control returned to Elco.

This option remains experimental and disabled by default. The value `2.0` proves independent control; it is not a final balance choice.

## Native real-time QuickSkill controls

The PC build already defines `ac_QuickSkill0..5` as action IDs `0x7A..0x7F`. Its active configuration binds them to top-row `5..0`. These are not menu-navigation shortcuts: a key-down event reaches a native helper at RVA `0x00027BF0`, which resolves the controlled character, finds the requested ordinal among six ordered usable skills, validates the selected native slot, and calls `CSkill::Use` directly.

The user confirmed top-row `5` activated Iron Will for a melee character while the Quick Menu remained closed. Observation-only instrumentation recorded action `0x7A`. This proves Phase 6 can retain Sudeki's native cost, availability, task, protection, animation, targeting, damage, and recovery machinery rather than synthesizing a separate skill implementation.

Elco exposes the next engine constraint. Three top-row `5` presses reached action `0x7A` and selected native slot `0`, but the common validator returned `2` with `CCharacterArbiter+0x50 = 0x00400002`: `Armed | Strafing`. The user reports the same lack of activation for Ailish, while Tal and Buki work.

Two menu Plasmatica casts supplied the comparison. The menu-level validation passed with `Idle | Armed | UsingUI`; an added observation point at the validation inside `CSkill::Use` passed with `Idle | Armed` after `UsingUI` was cleared. State `+0x58` retained low nibble `2`, and first-person mode was false in both paths. Quick Menu uses native UI transition RVA `0x0000AFD0(true)` to leave strafe, then its deactivation routine at RVA `0x00099180` calls the same transition with false while deliberately preserving idle for skill launch.

The first disabled prototype therefore holds that native transition for 75 ms only after a QuickSkill produces result `2` in the exact `Armed | Strafing` state. A Windows timer callback runs on the game thread through its normal message loop, verifies the actor naturally changed to `Idle | Armed`, closes the hidden UI state, and retries the unchanged native helper. The first Elco top-row `5` test succeeded: both the helper validator and the internal `CSkill::Use` validator returned zero, and the user confirmed the skill executed normally. The prototype never writes arbiter flags, bypasses validation, changes SP, or patches the executable.

The guard was then expanded across actions `0x7A..0x7F`. Top-row `7` produced action `0x7C`, ordinal `2`, and selected native slot `1`, the already identified Plasmatica slot. After the same transition, both validators returned zero, `CSkill::Use` returned success with an active task, and the task completed normally in `14.992 s`. The user confirmed Plasmatica behaved normally without opening the Quick Menu. Its visible cinematic sequence was absent or unobtrusive at the test location, while internal render-camera calls still occurred; this is consistent with the previously established location/collision-dependent presentation path.

The user subsequently tested Elco's top-row `5` through `9` bindings and reported that all five activated. Top-row `0` and Ailish's six ranged slots remain unconfirmed; this distinction is retained rather than treating the shared code path as proof for untested cases.

## Native Spirit Strike activation

Sudeki has no `ac_SpiritStrike` input action in either the executable action table or the active PC controls. Vanilla requires Quick Menu category `1`. Its selection handler at RVA `0x00099320` passes the selected entry's integer ID to the native validator at RVA `0x00010940`, closes the menu only on result `0`, and then calls the normal activation implementation at RVA `0x0000FBA0`.

The validator rejects disabled Spirit Strikes, an already active strike, missing/unavailable participants, insufficient SSP, dead or weapon-changing initiators, and blocked actor states. The activation function invokes that same validator again, gathers eligible party members, records the selected ID, starts the native Spirit Strike state, and emits `OnSpiritStrikeStarted`. A direct prototype must call this route rather than forcing the exported wrapper, changing SSP, or bypassing readiness.

IDs `0..15` are accepted by the definition lookup in eight character pairs. The first four pairs resolve the four primary party-character resource types; the exact live menu ID for the current Elco save is intentionally not guessed. Observation-only hooks at the Quick Menu validator and activation calls now log the selected ID and result, and inert-image tests confirm both call sites restore exactly on unload.

The available save did not expose an Elco Spirit Strike, so the first live capture used Ailish. Her first menu entry supplied ID `2`; validation returned `0`, activation returned `1`, and the user reported normal execution. This confirms the static mapping of Ailish's pair to IDs `2` and `3` without making a claim about an unavailable Elco entry.

The confirmed direct prototype uses the otherwise-unbound `G` key by default and polls it from the existing main-thread frame call at RVA `0x0028DDBA`. The key is read from `[Bindings] SpiritStrike`; the first input layer accepts one named keyboard or mouse button and rejects invalid enabled configuration. On a rising edge it calls the unchanged validator, enters Sudeki's native UI/control transition, waits 75 ms on the game thread, exits the transition, validates again, and calls the native activation implementation only on result `0`. Polling is ignored while Sudeki is not the foreground process. The prototype neither displays the Quick Menu nor patches SSP, state flags, or executable bytes.

The final live test passed both validations with result `0`, activation returned `1`, and the user confirmed Ailish's full stage-clearing Spirit Strike played and returned to normal control. Additional `G` presses during the active move returned native validation result `4` and did not start overlapping activations.

The first configurable-input test replaced the generated binding with `H`. Initialization logged virtual key `0x48`; pressing `H` again produced both validator results `0`, activation result `1`, and the full Spirit Strike. This confirms the configured binding drives the poller rather than a compiled-in `G` constant.

Two failed iterations establish why the transition is required. Calling activation immediately after validation returned success but left the game in an incomplete Spirit Strike state with deeper slow time, frozen allies, and locked character switching. Adding the native transition fixed that missing context. A later diagnostic gate was also removed after it incorrectly treated the party/group owner at VA `0x00808D94` as a character and interpreted pointer-like fields as arbiter flags. No active-character resolver was claimed at that checkpoint.

Follow-up static reconstruction separates the two objects correctly. VA `0x00808D94` holds the active party/group, while the native QuickSkill helper obtains its front character from the first intrusive entry at group offset `+0x90`. Spirit Strike's own party loop calls the embedded character-type component at character offset `+0x2C`, virtual slot `+0x10`, to obtain the resource type. The guarded automatic prototype maps primary resource types `0x23`, `0x01`, `0x05`, and `0x0E` to pair starts `0`, `2`, `4`, and `6`, then adds configured variant `1` or `2`. `SpiritStrikeId=-1` selects this mode; fixed IDs remain diagnostic overrides.

The automatic live test controlled Ailish. Resolution logged group `0x0596E954`, front character `0x07CAAAB0`, resource type `0x01`, variant `1`, and ID `2`. Both native validations returned `0`, activation returned `1`, and the user confirmed the move fired normally. This confirms the corrected front-character chain for Ailish; other character resource types retain their static mapping until tested live.
