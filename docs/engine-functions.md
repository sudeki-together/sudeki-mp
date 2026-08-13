# Engine functions

All entries on this page apply only to executable SHA256 `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`, image base `0x00400000`.

## Quick Menu and game-speed path

### `Possible_QuickMenuActivate`

| Field | Value |
| --- | --- |
| RVA / VA | `0x00098EC0` / `0x00498EC0` |
| Key write | `0x00498EF0: mov dword ptr [eax+0x24], 1` |
| Effect | Requests `CGameSpeed` mode `1`, then sets the menu object's active byte at `+0x29` |
| Discovery | Followed the Quick Menu singleton and UI activation path; the speed write occurs immediately at method entry |
| Confirmation | Static only; runtime watchpoint/trace still required |
| Confidence | High for the write and requested mode; provisional function name |

The 18-byte context beginning at VA `0x00498EE5` occurs exactly once in the executable:

```text
A1 A0 8D 80 00 8B 35 1C 8D 80 00 C7 40 24 01 00 00 00
```

Relocation-tolerant form:

```text
A1 ?? ?? ?? ?? 8B 35 ?? ?? ?? ?? C7 40 24 01 00 00 00
```

### `Possible_QuickMenuDeactivate`

| Field | Value |
| --- | --- |
| RVA / VA | `0x00099180` / `0x00499180` |
| Key write | `0x004991B6: mov dword ptr [eax+0x24], ebx` where `ebx` is zero |
| Effect | Requests `CGameSpeed` mode `0`; later clears the menu object's active byte at `+0x29` |
| Discovery | Followed the Quick Menu close path and active-byte transition |
| Confirmation | Static only; runtime watchpoint/trace still required |
| Confidence | High for the write and requested mode; provisional function name |

The 14-byte context beginning at VA `0x004991AB` occurs exactly once:

```text
A1 A0 8D 80 00 8B 2D 88 2F 7C 00 89 58 24
```

Relocation-tolerant form:

```text
A1 ?? ?? ?? ?? 8B 2D ?? ?? ?? ?? 89 58 24
```

### `Possible_MainFrameTimeUpdate`

| Field | Value |
| --- | --- |
| Relevant RVA / VA | `0x0028CD05` / `0x0068DD05` |
| Mode transition | If current `+0x20` differs from requested `+0x24`, calls `0x00427230` then `0x00427190` |
| Scale selection | mode `0` -> global at `0x00745F70`; mode `4` -> `[CGameSpeed+0x2C]`; other nonzero modes -> float at `0x006C4018` |
| Delta application | `0x0068DD86: fmul [esp+0x14]` multiplies the selected scale by the frame delta |
| Pause behavior | A separate byte at `CGameSpeed+0x28` selects zero instead of the scaled delta |
| Confirmation | Static only; runtime observation still required |
| Confidence | High for control flow and arithmetic; provisional function name |

The initialized values are `1.0f` at VA `0x00745F70` and `0.07f` at VA `0x006C4018`. A separate master multiplier at VA `0x00725810`, initialized to `1.0f`, is applied earlier at VA `0x0068DCAF`.

## Named export anchors

| Export | RVA | VA | Confirmed behavior |
| --- | ---: | ---: | --- |
| `GetGameSpeed()` | `0x00027500` | `0x00427500` | Returns singleton pointer from `0x00808DA0` |
| `GetStateAsInt@CGameSpeed` | `0x000272E0` | `0x004272E0` | Returns current mode at `this+0x20` |
| `IsGamePaused@CGameSpeed` | `0x00027480` | `0x00427480` | Returns pause byte at `this+0x28` |
| `SetGamePaused@CGameSpeed(bool,bool)` | `0x000272F0` | `0x004272F0` | Updates separate pause/refcount state and related systems |
| `SetGameSpeed(float)` | `0x00027040` | `0x00427040` | Writes normal-speed global `0x00745F70` |
| `SetMasterGameSpeed(float)` | `0x0028BE90` | `0x0068BE90` | Writes master multiplier `0x00725810` |
| `SetSpeedMode@CGameSpeed(int)` | `0x00207560` | `0x00607560` | Writes requested mode at `this+0x24` |
| `SetVariableOneTimeSpeedValue@CGameSpeed(float)` | `0x00027510` | `0x00427510` | Writes `this+0x2C` only for values in `[0.0, 1.0]` |
| `QuickMenuIsActive()` | `0x0009C330` | `0x0049C330` | Tests Quick Menu object bytes `+0xFC` and `+0x29` |
| `QuickMenuClose()` | `0x0009C360` | `0x0049C360` | Dispatches close operations on the Quick Menu singleton |
| `UIStartQuickMenu()` | `0x0009C3A0` | `0x0049C3A0` | Dispatches UI event `0x10` into the menu system |

Named exports are evidence supplied by the PE, but the behavior descriptions above come from instruction-level inspection rather than names alone.

## Skill activation path

### `CSkill::Use(int)`

| Field | Value |
| --- | --- |
| RVA / VA | `0x000B4810` / `0x004B4810` |
| Quick Menu caller | call at RVA `0x000998A1` / VA `0x004998A1` |
| Selected data | pointer at `CSkill+0x3C+(slot*4)` |
| Cost | signed integer at `SkillData+0x94`, subtracted from the actor's SP float |
| Queued state | active byte `CSkill+0x6C`, slot `CSkill+0x70`, task handle `CSkill+0x74` |
| Events | emits `OnSkillStarted`; the completion path later emits `OnSkillEnded` |
| Confirmation | Static decompilation plus one successful Plasmatica activation trace |
| Confidence | High for the offsets and control flow; provisional field names |

The validation helper at RVA `0x000B4BC0` returns nonzero status codes for unavailable global state, an already active skill, invalid actor state, an unusable skill, or insufficient SP. The exact user-facing meaning of every status value is not yet mapped.

### Script interpreter call handlers

| Role | RVA / VA | Evidence |
| --- | --- | --- |
| Bytecode interpreter step | `0x001C41D0` / `0x005C41D0` | Reads the opcode through the runtime bytecode base and dispatches through the table at `0x00723F04` |
| Opcode `0x27` script/global call | `0x001C4970` / `0x005C4970` | Reads a 32-bit hash, checks compiled functions, then falls back to the internal binding registry |
| Opcode `0x27` table slot | `0x00323FA0` / `0x00723FA0` | Contains the relocatable absolute handler pointer used by the interpreter |
| Opcode `0x28` object-method call | `0x001C4B10` / `0x005C4B10` | Pops an object, resolves method metadata, and invokes a virtual method at `0x005C4C0F` |

The opcode `0x27` binding fallback explains why rewriting PE export slots after loader initialization did not observe the first two Plasmatica casts: the interpreter uses its own registered/cached binding objects.

### `Possible_CSkillUpdateCompletion`

| Field | Value |
| --- | --- |
| RVA / VA | `0x000B47A0` / `0x004B47A0` |
| Trigger | active byte is set and the task handle at `CSkill+0x74` is null or finished |
| Effect | clears actor skill-lock state, clears the global skill-active bit, and enters the end helper |
| End helper | RVA `0x000B4E80` / VA `0x004B4E80`; restores state and emits `OnSkillEnded` |
| Confirmation | Static decompilation |
| Confidence | High for completion control flow; provisional function name |

## Plasmatica native missile path

| Function | RVA / VA | Confirmed behavior | Confidence |
| --- | --- | --- | --- |
| `CMissileManager::FireMissileScripted(int)` | `0x000C89C0` / `0x004C89C0` | Validates the manager/index, selects a `MissileData` entry, prepares it, and starts native launch | High |
| `Possible_SelectMissileData` | `0x000C6DE0` / `0x004C6DE0` | Stores selected `MissileData` and its resource reference on the manager; applies its movement constraint | High for data flow; provisional name |
| `Possible_LaunchSelectedMissile` | `0x000C7160` / `0x004C7160` | Creates/configures missile entities and initializes active `CMissile` instances | High for control flow; provisional name |
| `Possible_TerminateMissile` | `0x00186610` / `0x00586610` | Removes/releases the missile entity/component and disables it | High; provisional name |
| `Possible_UpdateMissileCollision` | `0x001867D0` / `0x005867D0` | Advances active missile movement/range and submits collisions that satisfy `MissileData` flags | High; provisional name |
| `Possible_InitializeMissile` | `0x00186E10` / `0x00586E10` | Enables the missile and installs the standard or bouncing movement controller | High; provisional name |
| `Possible_SubmitNativeAttackCollision` | `0x000DCD00` / `0x004DCD00` | Wraps a call to RVA `0x00018B90` with attack/resource, collision, owner, and flag data | Medium; final damage behavior unresolved |

The `CMissile` vtable begins at VA `0x006D915C`. Movement-controller implementations used by missile initialization begin at RVAs `0x00187600` and `0x00187710`. The vtable includes exported `HitEntity` at RVA `0x000A2900`, but launch reaches later behavior indirectly; there is no direct call from `FireMissileScripted` to `HitEntity`, `DoDirectDamage`, or `ModifyHitPoints`.

## Per-model animation speed

| Export | RVA / VA | Confirmed behavior |
| --- | ---: | --- |
| `CSimpleGameModelInterface::SetAnimationSpeedMultiplier(float)` | `0x000E0460` / `0x004E0460` | Stores input at `+0x48`; recomputes `+0x44 * +0x48 * +0x4C` into effective speed `+0x50`; invokes virtual slot `+0x5C` when changed |
| `CSimpleGameModelInterface::ResetAnimationSpeedMultiplier()` | `0x000E04B0` / `0x004E04B0` | Sets input multiplier `+0x48` to `1.0` and recomputes effective speed |
| `CSimpleGameModelInterface::GetAnimationSpeedMultiplier()` | `0x000E04F0` / `0x004E04F0` | Returns effective speed at `+0x50` |

RTTI confirms `CNewGameModelAnimation -> CSkinnedGameModelInterface -> CSimpleGameModelInterface`, with each base at offset zero. Consequently, Plasmatica's live `CNewGameModelAnimation` receiver is directly compatible with these functions.

Opcode `0x28`'s handler resolves a script wrapper to its native object immediately before the relative call at RVA `0x001C4C2F`. That call targets the private binding dispatcher at RVA `0x002351C0`; its second stack argument is the resolved native object. The dispatcher also receives the binding record in `ECX` and argument count in `EAX`, and returns with `ret 0x0C`. Plasmatica speed instrumentation must preserve that private convention. The base `CNewGameModelAnimation` primary vtable is RVA `0x002C8504`; Elco's live concrete `CNewMissileAimingGameModelAnimation` vtable is RVA `0x002D5464`, and RTTI places the relevant base chain at offset zero.

## Plasmatica scripted camera path

| Binding | Hash | Observed bytecode operand | Confirmed role |
| --- | ---: | ---: | --- |
| `TestCameraCollision|PRN` | `0x8232C4CA` | `0x0002FC0A` | Spatial gate; result `0` allowed the cinematic path and result `1` rejected it |
| `TsaPlayCamera|PRSNNBS` | `0xF69C244A` | `0x00038CEF` | Starts the accepted scripted-camera path |
| `CSpiritCam::StartCam|PPRNNS` | `0xEBDE4799` | `0x000045EF` | Receives the two positions, camera resource, playback-rate float, integer mode, and animation name |
| `GetCurrentTsaAnimation|P` | `0xB6171BB6` | `0x00038D1F` | Polled during cinematic camera setup |
| `SetRenderCamera|S` | `0x61F821BD` | `0x00038D56` | Selects observed cinematic resource hash `0xEE1B1485` |
| `SetRenderCamera|S` | `0x61F821BD` | `0x000AAEC9` | Restores observed normal resource hash `0x933B5BDE` |

The compiled `TsaPlayCamera` wrapper starts at bytecode offset `0x00004532`. It configures `CSpiritCam`, calls `StartCam` at `0x000045EF`, waits for the selected TSA animation, and then selects the cinematic render camera. Native `CSpiritCam::StartCam` is exported at RVA `0x00012910`. Its float argument is stored at `CSpiritCam + 0x1D4`; camera initialization multiplies that value by another local factor and `24.0` before configuring playback.

Successful casts execute one cinematic selection and one normal-camera restore. Accelerating only Elco shortens the selected interval from `11.585 s` to approximately `5.92 s`, causing the normal restore to interrupt the authored presentation after three visible angles. An exact-gated `2.0x` change to the `StartCam` float let the presentation complete all four angles within the same approximately `5.92 s` interval. This rate is independent of global simulation speed and Elco's model-animation multiplier.
