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
| `CElcoAbility::GetFuel()` | `0x000CDFE0` | `0x004CDFE0` | Returns current jetpack fuel from `this+0x6C` |
| `CElcoAbility::SetFuel(float)` | `0x000CDF30` | `0x004CDF30` | Writes the supplied value to maximum `+0x68` and current `+0x6C`, then invokes the native presentation refresh |
| `CElcoAbility::SetMaxFuel(float,bool)` | `0x000CDF80` | `0x004CDF80` | Writes maximum `+0x68` and optionally current `+0x6C` |
| `CElcoAbility::SetFillupRate()` | `0x000CDFF0` | `0x004CDFF0` | Copies the configured fill rate `+0x70` into active rate `+0x7C` |
| `CElcoAbility::SetEmptyRate()` | `0x000CE0C0` | `0x004CE0C0` | Copies the configured drain rate `+0x74` into active rate `+0x7C` |
| `CElcoAbility::ResetFuel()` | `0x000CE110` | `0x004CE110` | Zeros active rate, current fuel, and maximum fuel |

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

The validation helper at RVA `0x000B4BC0` returns `0` when the skill may start. Confirmed nonzero meanings are `1` = insufficient SP, `2` = actor not ready to use a skill, `3` = actor not in combat, `4` = global skill lock or unusable skill, and `5` = skills globally unavailable. The first three map in order to the localized strings `Not enough SP to use this skill.`, `Not ready to use skill.`, and `Cannot use skill if not in combat.`

### Native QuickSkill activation

| Field | Value |
| --- | --- |
| Input handler | RVA `0x000277B0` / VA `0x004277B0` |
| Direct activation helper | RVA `0x00027BF0` / VA `0x00427BF0` |
| Helper call site | RVA `0x00027ACF` / VA `0x00427ACF` |
| Direct `CSkill::Use` call | RVA `0x00027CB1` / VA `0x00427CB1` |
| Actions | `ac_QuickSkill0..5`, IDs `0x7A..0x7F` |

The shipped and active PC configurations bind these six actions to DirectInput scan codes `6..11`, corresponding to top-row keys `5..0`. On key-down, the handler passes the action ID in `EAX` to the direct helper. The helper resolves the active character through the owner at VA `0x00808D94`, selects the requested ordinal among up to six unlocked/usable `CSkill` entries ordered by `CSkill+0x54`, validates the resolved slot through RVA `0x000B4BC0`, and calls `CSkill::Use` only when validation returns `0`.

Runtime observation confirmed top-row `5` generated action `0x7A`; the user saw Iron Will activate on a melee character without opening the Quick Menu. Three Elco attempts selected slot `0` but returned result `2` with arbiter flags `Armed | Strafing` (`0x00400002`). Two successful menu casts reached the same validator with `Idle | Armed` (`0x00000003`) plus `UsingUI`; the internal validation at RVA `0x000B4828` then passed with `UsingUI` cleared and `Idle | Armed` preserved.

Quick Menu activation calls RVA `0x0000AFD0` with true. This native UI transition toggles the actor control component, `UsingUI`, and registered UI listeners. The selection handler invokes two virtual menu transitions before `CSkill::Use`; the full Quick Menu deactivation routine at RVA `0x00099180` calls RVA `0x0000AFD0` with false. Runtime observation confirms `UsingUI` is cleared while idle is preserved before the internal validation. The ranged failure is therefore specifically the persistent strafe state, not keyboard routing, first-person mode, SP, or a different skill entry point.

An immediate true/false transition did not leave enough time for the actor update and safely aborted. Holding it for 75 ms via a game-thread Windows timer produced `Idle | Armed`; closing it removed `UsingUI` while preserving that state. Retrying RVA `0x00027BF0` then returned zero at both its validator call and `CSkill::Use`'s internal validation. The user confirmed the selected Elco skill executed normally without a visible Quick Menu.

### Native Spirit Strike activation

| Field | Value |
| --- | --- |
| Quick Menu handler | RVA `0x00099320` / VA `0x00499320`, category `1` |
| Validator | RVA `0x00010940` / VA `0x00410940` |
| Activation implementation | RVA `0x0000FBA0` / VA `0x0040FBA0` |
| Manager global | RVA `0x00408D30` / VA `0x00808D30` |
| Party/group owner global | RVA `0x00408D94` / VA `0x00808D94`; not a confirmed character pointer |

The menu passes `(manager, selected_id)` to both native functions using a callee-cleaned two-argument convention. IDs `0..15` form eight character pairs; primary party resource types `0x23`, `0x01`, `0x05`, and `0x0E` map to pair starts `0`, `2`, `4`, and `6`. A live Ailish variant-1 activation confirmed ID `2`, validation result `0`, and activation result `1`.

The main loop calls its normal frame update at RVA `0x0028DDBA`. The direct-input prototype wraps that call solely to poll a rising configured-key edge on the game thread while Sudeki owns the foreground window. `[Bindings] SpiritStrike` defaults to `G` and is converted to a Win32 virtual-key value during initialization. A live override to `H` logged virtual key `0x48` and activated successfully. On success it cycles native UI/control state for 75 ms, validates again, and calls the native activation implementation. The live test returned validator results `0` before and after the transition, activation result `1`, and completed normally. Repeated presses during the move were rejected by the native validator with result `4`.

For automatic definition selection, `SpiritStrikeId=-1` follows the same front-character path as native QuickSkills: active group VA `0x00808D94`, first entry at group `+0x90`, then embedded character-type component at character `+0x2C`. The component's virtual slot `+0x10` returns a resource type, which maps to the primary character pair starts `0`, `2`, `4`, or `6`; `SpiritStrikeVariant=1|2` selects within the pair. The function pointer is accepted only when it lies within the exact supported executable image, and the unchanged Spirit Strike validator still rejects unavailable definitions. A live Ailish test resolved resource type `0x01` to ID `2`, passed both validations, returned activation result `1`, and completed normally.

## Native character-control switching

| Role | RVA / VA | Confirmed behavior |
| --- | --- | --- |
| Character input-event handler | `0x000277B0` / `0x004277B0` | Maps `ac_PrevChar` (`0x32`) and `ac_NextChar` (`0x33`) into controller action states |
| Controller frame update | `0x00027CF0` / `0x00427CF0` | Consumes Previous at controller `+0xFC` and Next at `+0xF4` when each state equals `1` |
| Previous-character consumer | `0x00023F60` / `0x00423F60` | Checks switch eligibility, rotates the group toward the previous character, and invokes shared control reassignment |
| Next-character consumer | `0x00024060` / `0x00424060` | Checks switch eligibility, rotates the group toward the next character, and invokes shared control reassignment |
| Previous party rotation | `0x00023CE0` / `0x00423CE0` | Searches eligible entries in reverse order and rotates the group’s four intrusive character entries |
| Next party rotation | `0x00023B50` / `0x00423B50` | Searches eligible entries in forward order and rotates the group’s four intrusive character entries |
| Shared post-rotation reassignment | `0x000237B0` / `0x004237B0` | Receives `(group, old_front, new_front)`, updates the global controller target, transitions old/new character components, and notifies downstream camera/controller listeners |
| Old/new `character+0x94` transition | `0x000EF700` / `0x004EF700` | Sets the old component's nested AI mode byte to `1` and the new component's byte to `0`; live Previous/Next tests confirmed `0` on the sole human-controlled/front character and `1` on all three AI party members |
| `AiIsOverriden(TPtr<Entity>*)` | `0x000F60A0` / `0x004F60A0` | Exported native query; returns whether the referenced character's component `+0x16A` override refcount is positive |
| `AiOverrideControl(TPtr<Entity>*)` | `0x000F60D0` / `0x004F60D0` | Exported native entry point; resolves `character+0x94` and enters the refcounted override path at RVA `0x000EC350` |
| `AiDefaultControl(TPtr<Entity>*)` | `0x000F6100` / `0x004F6100` | Exported native entry point; releases one refcounted override and restores AI mode for a non-controlled character when the count reaches zero |

The global group pointer is VA `0x00808D94`; its active/front entry begins at `group+0x90` and the remaining entries are at 12-byte strides. The global character-controller pointer is VA `0x00808DA4`; its reference-counted target is at `controller+0x248`. A live Previous press rotated the four entries right and changed the controller target to the new slot 0 after approximately 64 ms. A live Next press rotated them left back to the original order with the same delay. The target equaled slot 0 in every captured phase.

The event handler itself did not change ownership: it only recorded the action state. `CGroupPlayers+0xD6` (the exported player-switching enable flag), controller modes `+0x80/+0x84`, every character byte `+0x2A`, actor snapshots, and all four `character+0x94` pointers remained unchanged across the observed switches. Pointer presence and byte `+0x2A` therefore cannot be used alone as the human/AI discriminator.

The focused live trace resolved the discriminator. Before switching, slot 0 and `controller+0x248` referred to the same character and only its nested `[character+0x94+0x3C]+0x0B` mode was `0`; the other three modes were `1`. Approximately 65 ms after Previous, the new slot 0 changed `1→0` and the old front changed `0→1`. Next reversed both changes. Thus mode `0` means AI inactive for the human-controlled character in this path, while mode `1` means AI active.

The executable's named AI-control exports provide the maintainable separation route. `AiOverrideControl` increments signed word `component+0x16A`; the first override clears current AI work and sets mode `0`. `AiDefaultControl` decrements it and, when it reaches zero on a character that is not `controller+0x248`, restores mode `1` and normal AI behavior. These functions accept a pointer to Sudeki's intrusive `TPtr<Entity>` object, so an active group slot address (`group+0x90+slot*0x0C`) is a valid argument. The disabled-by-default Buki prototype uses these reversible APIs rather than writing the nested byte directly.

## Player movement submission

| Role | RVA / VA | Confirmed behavior |
| --- | --- | --- |
| Controller movement-vector consumer | `0x00028B00` / `0x00428B00` | Reads axes at controller `+0x1A0/+0x1A4`, camera-transforms and normalizes a horizontal vector, resolves `controller+0x248`, and submits movement through the character arbiter |
| Player movement call sites | `0x00028E3F`, `0x00028E5E` | Two branches in the same controller consumer call the arbiter submission with five callee-cleaned arguments; an observation-only live wrapper forwarded both unchanged |
| Character-arbiter movement submission | `0x000DAE80` / `0x004DAE80` | Accepts arbiter, world direction, speed, turn-rate, and movement mode; applies native state gates, updates the character movement controller, and writes the accepted facing/movement vector to the character's component at `+0xAC` |
| Movement camera transform | `0x000291A0` / `0x004291A0` | Callee-cleaned `(controller, output, local_input)` helper used by Player 1; refreshes/copies the controller's camera matrix, removes translation, and calls `D3DXVec3TransformCoord` |
| `CCharacterArbiter::SetSpeed(float,float)` | `0x000DB070` / `0x004DB070` | Exported stop/speed path; writes movement-controller speed and turn rate when current arbiter state permits movement |
| `CMovementController::SetAbsoluteDeltaMovement(float,float,float)` | `0x000030A0` / `0x004030A0` | Alternate direct-delta movement mode used when controller movement mode `+0x23C` equals `1` |

The normal third-person path uses controller movement mode `0` and RVA `0x000DAE80`. A live `W/A/S/D` capture consistently supplied one controlled character and arbiter, normalized horizontal directions (`Y=0`), turn rate `1.0`, and movement mode `0`. Observed native speed magnitudes were `1.0`, approximately `1.500`, and approximately `1.803`. The wrapper did not replace the character, vector, or any argument. AI movement also reaches the same arbiter submission from RVA `0x000F4BB0`, which supports using it as the shared per-character movement boundary after AI is disabled.

The call at RVA `0x00028C60` passes the controller, output vector, and local movement vector to RVA `0x000291A0`; its `ret 0x0C` confirms callee cleanup. The prepared Player 2 follow-up calls this same helper on the game thread, removes vertical output, and horizontally normalizes before submitting Buki movement. Entry bytes `55 8B EC 83 E4 F0 8B 55 08 D9 EE` are independently gated. This option is disabled and exact-image tested, but not yet live-confirmed.

## Character world-position boundary

| Role | RVA / VA | Confirmed behavior |
| --- | --- | --- |
| Script-facing `SetPlayerPosition(float,float,float)` | `0x00104ED0` / `0x00504ED0` | Resolves active group slot 0, reads `character+0x44`, and calls the internal position setter |
| Internal `CPosition` vector setter | `0x00003050` / `0x00403050` | Compares and writes input X/Y/Z to `CPosition+0x18/+0x1C/+0x20`, increments change state, and marks the object dirty |

Because party entries share the same character layout, the two position objects provide a direct horizontal separation measurement without moving or retargeting either actor. The prepared guard compares Buki against the current `controller+0x248` character. At or beyond a configurable limit it rejects only directions with a positive outward dot product; inward or tangential movement remains available. No live guard result is claimed yet.

## Per-character combat-input submission

| Role | RVA / VA | Confirmed behavior |
| --- | --- | --- |
| Character input-event handler | `0x000277B0` / `0x004277B0` | Maps Weak `0x2C`, Strong `0x2D`, Sweep `0x2E`, Weapon Next `0x2F`, Weapon Previous `0x30`, and Block `0x31` into controller action records |
| Controller combat-state consumer | `0x000286C0` / `0x004286C0` | Resolves `controller+0x248`, obtains that character's arbiter at `character+0x90`, performs controller/camera handling, and calls the per-arbiter submission at RVA `0x0002891F` |
| Character-arbiter combat-input submission | `0x000DB0E0` / `0x004DB0E0` | Accepts a chosen arbiter plus Weak, Strong, Sweep, Block, Weapon Next, and Weapon Previous states; enforces the arbiter's native owner, capability, weapon/attack-state, and action-transition rules before dispatch |
| Provisional native melee dispatch | `0x000DAC00` / `0x004DAC00` | Receives attack kind `1` weak, `2` strong, or `3` sweep from the per-arbiter submission; deeper target/animation behavior remains to be traced |
| Native melee combo dispatcher | `0x000D0730` / `0x004D0730` | Appends an admitted weak/strong attack kind to the bounded native history through RVA `0x000D0640`, resolves the authored transition, and commits the accepted combo result |
| Native combo transition lookup | `0x000D04F0` / `0x004D04F0` | Compares the current attack history (bounded at six entries) with authored combo transitions and calls RVA `0x000D13E0` for native timing, distance, and direction eligibility |
| Accepted combo transition commit | `0x000D14D0` / `0x004D14D0` | Updates the native combo-result/UI state after a transition passes its authored gates; rejected inputs do not reach a new presentation selector |

RVA `0x000DB0E0` uses an unusual exact-build i386 ABI: `ECX` is the target `CCharacterArbiter`, `EAX` is Block, and five callee-cleaned stack arguments are Weak, Strong, Sweep, Weapon Next, and Weapon Previous. At the controller call site, those states come from `+0x8C`, `+0x94`, `+0x9C`, `+0xAC`, and `+0xB4`, with Block loaded from `+0xA4`. Its entry bytes are `55 8B 6C 24 08 56 57 8B F8 8B F1` on the supported executable.

This is a real per-character boundary: the arbiter is explicitly supplied rather than recovered from the global controller target, and another native caller exists at RVA `0x000DA816`. A one-shot weak request is therefore represented by Weak `1` and zero for the other five states. The disabled prototype uses a small isolated assembly adapter to reproduce the ABI and leaves targeting and every native rejection path unchanged. The adapter's register/stack/cleanup test and the inert exact-image hook test pass. The live battle kept one Buki arbiter and repeatedly changed `+0x50` from idle values into `0x00001002`; exported `CCharacterArbiter::IsAttacking()` at RVA `0x000088D0` tests exactly bit `0x1000`. Independent Buki attack input is therefore confirmed. The observed nearest-target lock remains native, although its underlying pointer/writer is not yet traced.

The supported Tal renderer exposes the accepted melee history as distinct
selectors. Live operator traces mapped stage 1/2 as Weak `50/51` and Strong
`52/53`; the eight three-input histories resolve to `WWW=62`, `WWS=54`,
`SWW=60`, `SSS=61`, `SWS=63`, `SSW=65`, `WSW=68`, and `WSS=69`. These are
presentation results after native admission, not inputs that should be
re-executed remotely. The LAN host therefore translates only observed native
selectors into actor-neutral action variants. The client maps those variants
back to its local Tal animation bank, while the host remains authoritative for
targeting, hit detection, movement impulse, and damage.

The same acknowledged `WSS` history also produced selector `70` in a separate
native context. RVA `0x000D04F0` searches multiple authored candidates and
calls RVA `0x000D13E0` with each candidate's timing, target-distance, and
direction gates, so input history alone is not a unique presentation key.
LAN protocol `LA17` therefore carries selectors `69` and `70` as separate WSS
presentation variants while preserving the host's native choice.

The exact property parser at RVA `0x00103190` recognizes `AttackWeak`,
`AttackStrong`, and `AttackSweep` when loading authored transition data. The
read-only reconstruction is repeatable with
`tools/ghidra/TalMeleeComboReport.java`; future combat work should extend this
same chain from input action, through native admission/transition, to the
client presentation adapter instead of assigning meaning from a single input
edge.

The same exact function also establishes the ranged capability boundary. When
the low state nibble at arbiter `+0x58` is melee mode `1`, Weak, Strong, and
Sweep select attack kinds `1`, `2`, and `3`. In ranged/missile mode `2`, the
branch checks Sweep, weapon-next/previous, and Weak states `1|2` before firing;
it never reads the Strong argument. Ailish and Elco therefore have no usable
normal `AttackStrong` through this native path even though the global input
action exists. First-person input is separate: the exact action registry maps
`0x3C` to `ac_FirstPersonMode` and `0x3D` to
`ac_FirstPersonModeToggle`, and the controller consumer routes the latter to
the process-global camera/presentation helper.

## Gameplay camera target ownership

| Role | RVA / VA | Confirmed behavior |
| --- | --- | --- |
| `CCameraManager::SetCameraTarget` | `0x00037170` / `0x00437170` | Converts an entity `GELPointer` into native ref-counted camera targets; does not store a raw vector |
| `GetGameCameraMode` | `0x0002A8B0` / `0x0042A8B0` | Returns singleton pointer stored at VA `0x00808DA8` |
| Front-character camera reassignment | `0x0002A370` / `0x0042A370` | Acquires the new character's cached `GameObjectTarget`, installs it into both active camera target slots, and starts the appropriate native transition |
| Camera target-slot installer | `0x000E84C0` / `0x004E84C0` | With `CCamera` in `ESI`, installs a target at `+0xB4 + slot*4`, releases the old target, and notifies the active camera state; the caller must retain once for the persistent slot reference |
| Native `MatrixTarget` create/list insert | `0x00134FB0` / `0x00534FB0` | Allocates 0x80 bytes, copies a 4×4 matrix, updates cached translation, links the target into the manager list, and returns one reference |
| Camera target zero-ref release | `0x00135340` / `0x00535340` | Finds the target in the manager's typed lists, unlinks it, and invokes its destructor when its reference count reaches zero |

The current `CGameCameraMode` singleton pointer lives at RVA `0x00408DA8`. Its `+0x0C` member points to `CCamera+0x2C`; subtracting `0x2C` recovers the camera whose target slots are `+0xB4/+0xB8`. The camera-target list owner pointer lives at RVA `0x003C2F30`; native creators receive its object plus `0x4C`.

`Camera::Target` virtual `+0x10` supplies position and virtual `+0x20` supplies a complete transform. `GameObjectTarget` resolves those values from the attached entity. Live exploration places an `OffsetTarget` (vtable RVA `0x002D436C`) in slot 0; it composes that entity transform with native framing while slot 1 keeps the underlying `GameObjectTarget`. `MatrixTarget` supplies the same interface from its owned matrix, with translation in D3DX row `_41/_42/_43`. The live-confirmed midpoint prototype preserves the slot-0 framing transform, shifts it to the two-character centroid, and restores both native slots. Zoom and camera distance remain separate unresolved state.

## Native title/front-end menu state machine

The exact-build, read-only `tools/ghidra/TitleMenuReport.java` traces the title/front-end menu without modifying the executable or UI state. The native menu is not a texture-only overlay: a front-end controller at `FUN_004A1950` (RVA `0x000A1950`) populates localized menu labels and their action names, while `FUN_004A0360` (RVA `0x000A0360`) consumes the selected action and advances the front-end state machine.

| Role | RVA / VA | Confirmed behavior |
| --- | --- | --- |
| Native menu population/refresh | `0x000A1950` / `0x004A1950` | Registers localized keys with `FUN_005B9FC0` and action names with `FUN_004049C0`; writes the active item count at controller `+0x17D8` and selected index at `+0x17D4` |
| Front-end state/update caller | `0x000A0F40` / `0x004A0F40` | Handles front-end states, calls the menu population routine, performs native sound/fade/transition work, and resets/rebuilds menu entries on state changes |
| Front-end action dispatcher | `0x000A0360` / `0x004A0360` | Compares the selected action name and routes `Options`, `Continue`, `DashBoard`, `ShowFrontEndCredits`, and `QuitGame` to native states |
| Native localized-label registration | `0x001B9FC0` / `0x005B9FC0` | Receives keys such as `NewGame`, `Options`, `Continue`, `Credits`, and `QuitGame` |
| Native action binding | `0x000049C0` / `0x004049C0` | Receives action names such as `StartNewGame`, `Options`, `ShowFrontEndCredits`, and `QuitGame` |

The executable stores the UTF-16 labels `Options`, `Continue`, `Credits`, and `QuitGame` at VA `0x006CB140`, `0x006CB164`, `0x006CB178`, and `0x006CB188`; all references resolve to the `FUN_004A1950` menu builder. The builder has distinct branches for a fresh title menu, a save-available menu, and the in-game/front-end return path. In the action dispatcher, selecting `Options` sets the pending operation at controller `+0x4C` to `1`, changes the state to `6`, and re-enters `FUN_004A0F40`; selecting `Continue` uses the same options state with pending operation `2`. Credits enter state `10` and set the front-end credits flag; `QuitGame` calls `PostQuitMessage(0)`.

This is the correct future seam for a native co-op roster: observe or extend the front-end state/action boundary after the title menu's own labels and callbacks are understood. The current SudekiMP roster remains an opt-in diagnostic overlay; it does not patch these native states, localized resources, or action callbacks. Confidence: high for the menu population/action routing, medium for the meaning of undocumented controller offsets beyond the state/index fields listed above.

### Native title-menu assets

The exact-build, read-only `tools/ghidra/TitleMenuAssetReport.java` traces the
language-indexed presentation resources used by the five stock title rows.
`FUN_005132B0` initializes the resource records at runtime; the corresponding
tables are zero-filled in the static image, so reading their uninitialized
values alone is insufficient.

| Row | Resource table VA | Indices | English resource |
| --- | --- | --- | --- |
| Continue | `0x006C2BEC` | `363–369` | `SFE_OPTION1.SQX` |
| New Game | `0x006C2C08` | `370–376` | `SFE_OPTION2.SQX` |
| Options | `0x006C2C24` | `377–383` | `SFE_OPTION3.SQX` |
| Credits | `0x006C2C40` | `384–390` | `SFE_OPTION4.SQX` |
| Quit | `0x006C2C5C` | `391–397` | `SFE_OPTION_QUIT.SQX` |

Each seven-entry family follows language order English, French, German,
Spanish, Italian, Japanese, and Russian. The non-English filenames use the
suffixes `_FRE`, `_GER`, `_SPA`, `_ITA`, `_JAP`, and `_RUS`. These SQX files
are label presentations rather than generic text slots; a new `Single Player`
or `Co-op` row must not pretend that the stock label can be renamed in place.

The reusable native geometry is separate. The executable and `SOLData.baf`
identify `SFE_OPTION_START.HOM`, `SFE_OPTION1.HOM` through
`SFE_OPTION5.HOM`, `SFE_Option_Bar`, `SFE_Option_Bar_Highlight`, and
`SFE_Option_Bar_Select`. Each numbered option object exposes native `IDLE`,
`ON`, `OFF`, `HIGHLIGHT`, `SELECT`, and `UP` animations; the start object
exposes `IDLE`, `ON`, and `OFF`. The front-end background exposes `IDLE`,
`ON`, `OFF`, `UP`, `UP_IDLE`, `UP2`, `UP2_IDLE`, and `DOWN`. Other reusable
front-end nodes include `SFE_Background_Characters`, `SFE_MenuPanel`,
`sfe_menu_titlebar`, `sfe_menu_listbar`, and `sfe_menu_infobar`.

Native character portraits are available uniformly as
`SUI_PORTRAIT_TAL.SQX`, `SUI_PORTRAIT_AILISH.SQX`,
`SUI_PORTRAIT_BUKI.SQX`, and `SUI_PORTRAIT_ELCO.SQX`. Their existing resource
route is resource type -> RVA `0x0003F430` -> table RVA `0x002C2A94` ->
narrow selector RVA `0x0015C070`. These are preferable to the unevenly named
character-logo resources for the first roster page.

Live construction refined that boundary. Each numbered `SFE_OPTION*.HOM`
renderer exposes one outer animation submodel and no separately addressable
named component for its text shape. `SFE_OPTION_START.HOM` is not a blank bar
either: its presentation includes the baked `Press START` label. Reusing
either resource therefore leaks a stock word into a new logical row.

The working boundary retains the native state-10 fade and queued-font
function at RVA `0x00009930`, keeps the resident rows hidden behind private
OFF-state objects, and recreates only the label-free button as a
runtime-generated D3D9 texture. Its original-code renderer composes a soft
shadow, rounded dark rim, and tighter inset gradient bar; the two rounded
boundaries use quarter-pixel coverage sampling, while selection adds the
measured cyan-left/gold-right color language. It is drawn immediately before
the native CUIScene text flush at call RVA `0x0000A760` and contains no
extracted game pixels. Portraits can still load from the user's installed
archives; SudekiMP does not copy or redistribute those resources.

The follow-up exact-build `TitleMenuLifecycleReport.java` removed the need to
construct extra row objects for the first page. The PC front-end controller
retains five native option-row animation objects at `+0x70..+0x80`, a separate
start object at `+0x84`, and five localized label-presentation objects at
`+0x88..+0x98`. `FUN_004A16F0` (RVA `0x000A16F0`) reads the active count at
`+0x17D8` and selection at `+0x17D4`, then queues animation state `3` on the
selected row and state `0` on the other active rows. The same native queue
helper at RVA `0x00120260` accepts state `2` for unused rows. Label resource
replacement and tinting remain separately owned by `FUN_004A1950`.

The first resident-page prototype therefore borrows those five existing rows,
never allocates or destroys a front-end object, hides the stock label
presentations through their common active-state notification, queues native
`IDLE`/`OFF`/`HIGHLIGHT` states on the bar rows, and submits only SudekiMP's
words through the native font queue. It snapshots and restores the five label
active states before calling the unchanged menu builder on exit. Every pointer,
vtable notification, and state-queue target is checked before use; a failed
gate falls back to the text-only roster page. Live visual acceptance remains
pending.

Live testing rejected that borrowed-row design as the final menu architecture.
Although roster input can be isolated, the bars remain the title page's same
five physical row objects, so their presentation necessarily moves in sync
with `Continue`, `New Game`, and the other stock choices. This cannot be fixed
by changing labels, colors, or queued row states.

The native `Options` route demonstrates the required independent-page seam.
`FUN_004A0360` first performs the normal front-end transition, writes pending
operation `1` at controller `+0x4C`, enters state `6`, and calls
`FUN_004A0F40`. In that state, `FUN_004A0F40` assigns the separate page object
at controller `+0xB0` to the active-page slot at `+0xAC`, then activates that
object through virtual slot `+0x48`. Pending operation `2` selects the
alternate object at `+0xB4`. Therefore a proper Sudeki Together page must own
its own page object and child rows, and participate in this activation and
restoration lifecycle; it must not borrow the resident title rows. A read-only
runtime trace now records the active/options/alternate pointers and vtable RVAs
immediately before and after one native Options activation so the exact page
class and cleanup path can be identified before allocation is attempted.

That live trace passed. Before activation, controller state was `5`, mode was
`0`, and active page `+0xAC` was null. After activation, state was `6`, previous
state was `5`, mode was `1`, and `+0xAC` exactly matched the Options object at
`+0xB0`. Its vtable is RVA `0x002D1CB4`; RTTI identifies the class as
`UILayerOptionsMenu`. The alternate `+0xB4` object has a distinct vtable at RVA
`0x002CA89C`.

`UILayerOptionsMenu` is a specialized `0x198`-byte object constructed at VA
`0x0051A7B0`. Its RTTI hierarchy contains `UILayerSubMenu`, `UILayer`,
`UIGameSpeedListener`, and `UIAnimationListener`. Virtual `+0x48` at VA
`0x0051CC80` is the activation boundary observed from `FUN_004A0F40`; it
allocates Options-specific child objects before dispatching virtual `+0x60`.
Virtual `+0x4C` at VA `0x0051CD00` releases those children and dispatches
virtual `+0x68`. The destructor path begins at VA `0x0051C050`. Construction
also publishes a singleton at VA `0x007C3030`, so constructing another raw
`UILayerOptionsMenu` would collide with native Options ownership and is
rejected. The supported design is a SudekiMP-owned `UILayerSubMenu`-compatible
page with separately owned native row children, registered through the same
active-page lifecycle.

### Native queued-font text submission and alignment

Read-only decompilation of the queued-font path (`TextSubmitReport.java` plus
follow-up traces) resolves the argument semantics used by the roster page.

| Role | RVA / VA | Confirmed behavior |
| --- | --- | --- |
| Queued text submit | `0x00009930` / `0x00409930` | `FUN_00409930` takes the `CUIScene` in `ECX`, a UTF-16 SSO text record in `EAX`, and five stack args: font, alignment, x, y, color. Writes entry `[0..3]=font,alignment,x,y`, `[0x13]=color`, `[0x14]=0`. |
| Submit variant | `0x00009990` / `0x00409990` | Identical layout but writes `[0x14]=1`. |
| Queue entry allocator | `0x00009810` / `0x00409810` | Returns the next `0x54`-byte entry from the CUIScene ring buffer (`scene+0x124` array, `+0x11c`/`+0x128` read/write cursors, backing pointer `scene+0x12C`). |
| Queue consumer | `0x0000A820` / `0x0040A820` | `FUN_0040A820` (CUIScene render) iterates the queue: reads `[0]` as font, `[2]` as x, `[3]` as y, `[0x13]` as ARGB color, `[0x14]` as the draw-variant flag. It does **not** read `[1]` here. |
| Text draw (flag 0) | `0x001D11F0` / `0x005D11F0` | `FUN_005d11f0(alignment, x, y)`; sets the layout object `+0x20` to the mode. |
| Text draw (flag 1) | `0x001D12A0` / `0x005D12A0` | `FUN_005d12a0(alignment, x, y)`; same mode field, minor layout variant. |

Entry layout (0x54 bytes): `[0]` font, `[1]` alignment, `[2]` x, `[3]` y,
`[4]` SSO length flag (`0x80000000 | n`), `[5..]` inline UTF-16 text,
`[0x13]` ARGB color, `[0x14]` draw-variant flag.

Alignment semantics (the draw helpers' mode field `+0x20`): **0 = center at x,
1 = left at x, 2 = right at x.** The front-end text space is 640x480 units
(the right-align branch uses `0x1E0` = 480 as canvas height), so the horizontal
center is **x = 320 = `0x140`**. Native call sites confirm this: centered title
labels submit `x = 0x140` with alignment `0` (e.g. `FUN_00409990(1, 0, 0x140,
0x122, …)` in `FUN_00578C90`), the right-side version string uses `x = 0x22A`
(554) with alignment `2` (`FUN_004A3760`), and left-anchored rows use alignment
`1` with x values 86/220/342/496/515. The earlier roster value of `x = 145`
was a misread of the left edge of centered text (320 minus roughly half a
heading's width), not the API's center coordinate; the corrected value is
`0x140`. Confidence: high (decompiler-confirmed mode field plus consistent
native call sites); executable SHA256 `8ceb1d3c…bb94`.

## D3D9 frame and split-screen render seam

| Role | RVA / VA | Confirmed behavior |
| --- | --- | --- |
| Main frame/render loop | `0x0028D3F0` / `0x0068D3F0` | Runs the global graphics layer, invokes the gameplay-world renderer at VA `0x0068D546`, and ends the frame |
| D3D device global | `0x003C31DC` / `0x007C31DC` | Holds the active `IDirect3DDevice9` used for viewport reads/writes |
| D3D viewport wrapper | `0x001DCE30` / `0x005DCE30` | Native viewport-setting boundary |
| Frame begin | `0x001DD200` / `0x005DD200` | Begins the native graphics frame before scene submission |
| Frame end | `0x001DD540` / `0x005DD540` | Completes callbacks and calls D3D9 `EndScene` through device-vtable byte offset `0xA8` |
| Gameplay-world render owner | `0x0000A5B0` / `0x0040A5B0` | Owns the three gameplay-world graphics-phase submissions; receives the world context in `EDI` and a stack float from the frame loop |
| Graphics render phase | `0x001D4750` / `0x005D4750` | Render helper called with renderer in `EAX` and world context in `EDI`; no stack arguments |
| `CPCQuitScreenShow(bool)` | `0x0001DBE0` / `0x0041DBE0` | Exported wrapper around internal show/hide path at RVA `0x0001D700`; resolves the singleton at VA `0x00808D68` |
| `CPCQuitScreenEnable(bool)` | `0x0001DC00` / `0x0041DC00` | Exported enable/refcount gate at `CPCQuitScreen+0x1CC`; this is not the visible-state test |
| Quit-screen per-frame render/update | `0x0001D690` / `0x0041D690` | Tests `CPCQuitScreen+0x1C2` before drawing; main render-loop callsite RVA `0x0028D572` is the confirmed pre-interface backdrop seam |

The exact-build graphics-phase call sites are:

| Call-site RVA / VA | Scope | Split policy |
| --- | --- | --- |
| `0x0028D473` / `0x0068D473` | Primary render call used by menu and visible gameplay | Native and single-run; render replay is rejected |
| `0x0000A62D` / `0x0040A62D` | Conditional world subpass/layer | Single-run; replay contributed to the first run's corruption and did not produce the desired gameplay split alone |
| `0x0000A689` / `0x0040A689` | Conditional world subpass/layer | Single-run |
| `0x0000A738` / `0x0040A738` | Conditional offset/secondary world layer | Single-run |
| `0x0028D58C` / `0x0068D58C` | Main call to frame end RVA `0x001DD540` | Current compositor hook; original frame end/`EndScene` runs first, then the finished native surface is copied during verified gameplay and before the following D3D9 `Present` call |

The first live prototype duplicated all four calls. It proved that the same Sudeki process can submit a genuine left/right viewport pair, but both halves used the same selected native camera. It also split the main menu, duplicated the native HUD, produced large black shadow/visibility regions, and omitted some door geometry from the second view. The visual defects are confirmed; their exact ownership is not. The leading hypothesis is that a global/prepass producer or transient visibility queue was executed or consumed twice.

The second live version left the primary call single-run and replayed only the three world subpasses. Contrary to the initial static classification, the user observed that the title/menu still split while loaded gameplay did not. The good result was equally important: the black regions disappeared and the previously missing door rendered correctly. This identifies replay of the primary call as necessary for the visible gameplay split and confirms that the three subpasses should remain single-run.

The third disabled prototype therefore hooked only RVA `0x0028D473`. It forwarded that call once at full width unless the active-group global was valid, party slot 0 equaled the character controller's target at `+0x248`, and the current game-camera mode exposed a readable camera pointer. Only when all three ownership checks agreed was the call replayed into left/right viewports. This is an exact-build gameplay-state gate, not a guessed timer or menu flag. A second render camera, per-viewport projection/aspect, independent culling, and HUD ownership are not implemented.

The third live run confirmed that gate: title and main menu remained full-width, gameplay split after loading, and the log changed from `gameplay_gate state=inactive` to `active`. The black regions and missing right-view door returned, proving the final primary draw itself is not replay-safe without its preparation lifecycle.

The full main primary sequence is first helper RVA `0x001D48C0` at call site `0x0028D45B`, middle helper RVA `0x001D4820` at `0x0028D46B`, then final helper RVA `0x001D4750` at `0x0028D473`. The first two receive the renderer in `EAX`, world context in `EDI`, and one callee-cleaned float. Their object preparation uses render-generation `uint16` at RVA `0x003C3150`; frame begin increments it, and per-object generation fields prevent duplicate preparation within one generation.

The final helper calls RVA `0x00226F30`, which flushes shared render callback queues through RVA `0x00226E90`. The same queue path carries `cShadowRenderCallback`, explaining why a second final draw sees consumed shadow/visibility work. A fourth experiment set the left viewport before native preparation, then advanced the render generation and reran first/middle with float bits `0` before drawing right. In the live result all player and NPC motion appeared frozen, while the black shadow figures and missing door remained. The generation therefore participates in state that cannot safely be treated as a second-view-only render stamp; this entire replay policy is rejected.

The current experiment hooks only the main call to frame end at RVA `0x0028D58C`. It calls the native frame-end function first and does not modify or replay any prepare, culling, shadow, visibility, world-subpass, or draw call. During the proven gameplay gate it captures render target 0 into a same-size non-multisampled surface, then scales that finished image into the left and right halves. If any D3D operation fails, the native full frame is retained or restored and one diagnostic is logged. The initial proof requires native anti-aliasing `0`; the research launcher changes it only for that run and restores the previous numeric setting at exit. Both halves deliberately receive the same finished camera image. Independent cameras will later render into separate full-size targets that feed this compositor.

The Quit-screen singleton pointer is exact-build RVA `0x00408D68`. Its internal show/hide path stores the true presentation state at object offset `+0x1C2`; the ordinary per-frame Quit renderer checks that same byte. This is narrower than generic game pause state and does not confuse unrelated pauses, loading, or cinematics with the shared interface. The split-screen gate verifies the loader-relocated `CPCQuitScreenShow(bool)` entry, stops camera-cache updates while `+0x1C2` is nonzero, and retains the last valid Player 1/Player 2 render-target textures. At the native Quit callsite RVA `0x0028D572`, it draws those textures as two pre-transformed screen quads, restores the complete device state from a `D3DSBT_ALL` state block, and then calls the unchanged native renderer with the original `EAX` receiver. Live testing confirmed one full-width Quit interface over the frozen two-camera background and clean return to live split gameplay.

## Named native cameras

| Role | RVA / VA | Confirmed behavior |
| --- | --- | --- |
| `CCameraManager::AddCamera` | `0x00036C10` / `0x00436C10` | Accepts camera name and configuration name, rejects duplicates/full 10-slot table, allocates a distinct `0x108`-byte `CCamera`, initializes it, copies the name to `+0x4C`, and applies the requested configuration |
| `CCameraManager::RemoveCamera` | `0x00036DE0` / `0x00436DE0` | Finds the named slot, restores scene-camera state if necessary, destroys the camera, and clears the manager slot; callers must first select another render camera |
| `CCameraManager::GELGetCamera` | `0x00036ED0` / `0x00436ED0` | Returns the named camera from the ten-slot manager table; empty input returns the current render camera |
| `CCameraManager::GELGetRenderCamera` | `0x000272E0` / `0x004272E0` | Returns `CCameraManager+0x20` |
| `CCameraManager::SetRenderCamera` | `0x00036FB0` / `0x00436FB0` | Resolves a named camera, notifies registered camera listeners, updates the scene render-camera state, stores it at manager `+0x20`, and preserves the outgoing camera's `+0x105` mode byte |

`SetRenderCamera` performs two materially different writes: gameplay/global ownership goes to `CCameraManager+0x20`, while the scene renderer receives the selected camera's render-state pointer from `CCamera+0x34` at `scene_manager->+0x40->+0x7C`. The world-render path reads that latter pointer. `CCamera`'s native matrix handoff copies 16 floats to render state `+0x90` and increments the render-state generation at `+0x2C`; debug output identifies matrix translation `+0xC0/+0xC4/+0xC8` as camera position.

The first live Player 2 proof selected the new camera globally. It confirmed lifecycle ownership/restoration but produced invalid framing and interfered with a doorway transition. The replacement keeps `CCameraManager+0x20` on Player 1 at all times. Main render-start call site RVA `0x0028D443` temporarily changes only the scene renderer's `+0x7C` pointer to `SudekiMP_P2+0x34`; the existing frame-end hook restores Player 1 before native `EndScene` at RVA `0x0028D58C`. Camera 2 receives Player 1's complete valid render matrix with its camera-position row translated by the Player 1-to-Player 2 world-position delta. This preserves current orientation and distance for the next proof; independent rotation/zoom remains later work.

The replacement passed live testing. Both diagnostic halves rendered a sensible Buki-centered view while the log retained one unchanged global Ailish camera, and Ailish movement plus the castle doorway transition completed without skybox, freeze, or missing geometry. Toggling back restored normal Ailish framing. The close post-doorway Buki framing is a known limitation of translating position while inheriting Player 1 orientation/distance, not an ownership failure.

The prepared simultaneous-view experiment alternates this render-only selection at the same render-start boundary, but still executes the world renderer exactly once per engine frame. After native `EndScene`, the existing safe compositor caches the finished frame by camera owner and presents the latest Player 1 cache on the left and Player 2 cache on the right. The views are temporally staggered by at most one engine frame and each refreshes every other frame; no render-generation or callback queue is advanced manually.

## Viewport HUD character ownership

| Role | RVA / VA | Evidence |
| --- | --- | --- |
| Party smart-pointer copy helper | `0x000015B0` / `0x004015B0` | Copies a 12-byte intrusive party-slot pointer from `ECX` into the destination at `EAX`; RVA `0x000015E0` unlinks the temporary |
| Main HUD HP/SP numeric source | call at `0x00181517` / `0x00581517` | `UIPortraitGroup` copies party slot 0 at `CGroupPlayers+0x90`, then reads current HP/SP through character `+0x4C` |
| Portrait-gizmo portrait source | call at `0x000AAB3A` / `0x004AAB3A` | Native portrait refresh RVA `0x000AAB00` copies the indexed party slot, identifies its character, and assigns the corresponding `SUI_PORTRAIT_*.SQX` resource to the gizmo's cycle icon |
| Character type to portrait enum | `0x0003F430` / `0x0043F430` | Maps the character resource-type value returned by virtual slot `+0x10` on embedded character object `+0x2C` into portrait enum `0..7` |
| Portrait resource-index table | `0x002C2A94` / `0x006C2A94` | Maps portrait enum (plus alternate-state offset 8 when selected) to the cycle-icon resource index |
| Narrow cycle-icon resource selector | `0x0015C070` / `0x0055C070` | Loads one resource-table entry and forwards it to RVA `0x0015C0E0`; internal convention is resource index in `ECX`, completion flag in `EAX`, and cycle-icon receiver as its one stack argument |
| Portrait resource assignment | `0x0015C0E0` / `0x0055C0E0`, native refresh call at `0x000AAC08` / `0x004AAC08` | Replaces the cycle icon's resource and accepts a completion flag as its fourth stack argument. Flag `1` drains the native pending-resource list before returning. |
| Portrait-gizmo HP/SP source | call at `0x000A9D5B` / `0x004A9D5B` | Selects `CGroupPlayers+0x90+(UIPortraitGizmo+0x32C)*0x0C` before calculating HP/SP ratios |
| Portrait-gizmo name source | call at `0x000A9E15` / `0x004A9E15` | Uses the same indexed party slot before resolving the displayed character name |
| Portrait-gizmo status source | call at `0x000AACAB` / `0x004AACAB` | Uses the same indexed party slot before updating status-effect bits |

The first exact-build viewport-HUD prototype redirected the four data callsites and live-confirmed independent names, HP, SP, and companion ordering. `HudPortraitBindingReport.java` established that `UIPortraitGizmo+0x2C` is a separate `UIElementCycleIcon` and gameplay HUD global RVA `0x003C2F9C` owns four live gizmo pointers at `+0x138`. Calling broad refresh RVA `0x000AAB00` per viewport was rejected: its asynchronous assignment initially left the art blank, and its broader UI behavior contributed to party-presentation churn. The accepted implementation leaves callsites `0x000AAB3A` and `0x000AAC08` native, maps the selected stable character identity through RVA `0x0003F430`/table RVA `0x002C2A94`, and calls the narrow RVA `0x0015C070` selector with synchronous completion. The user confirmed correct Ailish/Buki art on the left/right views.

The data source hooks also resolve the desired stable character to whichever live group slot currently contains it, rather than assuming slot 0 always means Player 1. Sudeki reverses the same Ailish/Buki pair as part of this presentation path. The camera poll now treats only that exact reversed pair as an internal order rotation, not a multiplayer reassignment; any missing or different character still triggers safe camera teardown. This removed the per-frame Camera 2 release/cache invalidation that briefly exposed a full-width frame and enlarged the minimap.

## Free-roam camera configuration and input staging

| Role | RVA / VA | Evidence |
| --- | --- | --- |
| `CGroupPlayers::InCombat()` | `0x00004FA0` / `0x00404FA0` | Exported const method; returns group byte `+0xD4` |
| Character input handler | `0x000277B0` / `0x004277B0` | Action `0x69` writes controller `+0x184`; action `0x6A` writes `+0x188` |
| `GetCameraManager()` | `0x00038C40` / `0x00438C40` | Returns the singleton at VA `0x00809D7C` |
| `CCameraManager::LoadConfig` | `0x000375F0` / `0x004375F0` | Export anchor for named camera-profile loading |
| `CCameraManager::SetCameraConfig` | `0x00037CD0` / `0x00437CD0` | Export anchor for selecting a loaded camera configuration |
| `CCamera::GetConfigFloat` | `0x000E8D50` / `0x004E8D50` | Resolves float fields through the camera's current config wrapper at `CCamera+0x38` |

The active PC control file maps `CameraU/CameraD` to mouse Y and weapon next/previous to the two wheel directions. The `DEFAULT` camera profile contains exploration default/min/max/absolute-max distances `3.5/3.5/6.2/8.5`, exploration user-distance scale `8.0`, and combat values `6.0/6.0/10.0/10.0`.

These facts do not yet identify the durable desired/current camera-distance field. Live wheel remapping and late writes to controller `+0x184/+0x188` were observed but did not move the camera. The next reverse-engineering target is therefore the downstream consumer and clamp/profile state reached during a visibly working vanilla mouse-Y distance change.

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
| `Possible_UpdateMissileCollision` | `0x001867D0` / `0x005867D0` | Advances movement/range, handles ground/wall collision and bounce termination, and requests environment-impact SFX; it does not directly apply character damage | High; provisional name |
| `Possible_InitializeMissile` | `0x00186E10` / `0x00586E10` | Enables the missile, installs its movement controller, and registers its collision body through RVA `0x000EC200` | High; provisional name |
| `Possible_SubmitMissileImpactSfx` | `0x000DCD00` / `0x004DCD00` | Submits the configured environment-impact effect through RVA `0x00018B90`, which constructs an `SfxSetupPosition`; this is not the damage path | High; provisional name |
| `Possible_CollisionDamageBridge` | `0x00032A80` / `0x00432A80` | Tests collision geometry and directly invokes the `CCollisionDamage` callback at RVA `0x00138870` for qualifying contacts | High; provisional name |
| `Possible_ApplyCollisionDamage` | `0x00138870` / `0x00538870` | Enforces the configured multiple-hit delay, checks target state, builds a `DamageStructure` containing damage, knockback, effect, and status data, then calls RVA `0x000DAB50` | High; provisional name |
| `Possible_DispatchDamageStructure` | `0x000DAB50` / `0x004DAB50` | Applies target/state filters and routes an accepted `DamageStructure` to the target combat component at RVA `0x000D21D0` | High; provisional name |
| `Possible_ApplyDamageToCharacter` | `0x000D21D0` / `0x004D21D0` | Applies mitigation and clamps the result, stores the resulting current HP at combat-data offset `+0x2C`, then handles hit reactions, status effects, and death follow-up | High for HP flow; provisional name and structure labels |
| `Possible_UpdateKnockbackSession` | `0x000D2170` / `0x004D2170` | Receives whether the chosen hit reaction lies in ID range `0x2A..0x36`; starts/increments the configured session and sets the threshold flag once the previous count exceeds the authored limit | High for counter/flag behavior; provisional name |

The `CMissile` vtable begins at VA `0x006D915C`. Movement-controller implementations used by missile initialization begin at RVAs `0x00187600` and `0x00187710`. The collision-damage component has RTTI and vtables at VAs `0x006D45D4`, `0x006D4614`, and `0x006D461C`. Entries that point to exported `HitEntity` at RVA `0x000A2900` resolve to a shared no-op/export thunk and are not evidence of damage delivery. The confirmed native chain instead reaches RVA `0x00138870` directly from RVA `0x00032A80`.

Exact-build byte contexts for the three damage functions are recorded in `research/signatures/plasmatica-damage.md`.

The Talos defense trace exact-gates the accepted receiver with bytes
`55 8B EC 83 E4 F8` and the collision handler with
`83 EC 78 53 55`. It observes the unchanged calls through inline trampolines.
For Talos's authored limit `10`, `Possible_UpdateKnockbackSession` sets the
threshold flag on qualifying reaction 11 because it compares the old count
against the limit before incrementing.

Focused live testing confirms the consequence. Tal's Blade Dance delivered
15 packets of 465 damage. The first 11 selected reaction `0x2E`; the remaining
four still reduced HP by 465 but selected no hit reaction while
`CCombat+0x72 bit 0` remained set. After the 10-second session expired, the
counter and flag cleared and a basic hit selected reaction `0x2A` normally.
The arbiter invulnerability refcount remained zero throughout, classifying
this boundary as anti-stagger/super armor rather than an iframe mechanism.
In a follow-up live test, Blade Dance first left Talos at session count `11`
with the threshold flag active. A Spirit Strike then dealt `9,296` damage and
still selected reaction `0x2F` (`ANIMID_GETHIT_FRONT_MEGA`) without clearing
the threshold. Talos's data maps that reaction to
`A015_TALOS_GETHITFRONTUP.CLM:64`; the distinct authored floor-knockdown clip
is mapped to reaction `0x2D`. Spirit Strike therefore bypasses ordinary
post-threshold reaction suppression, but this capture does not establish that
Talos enters a prone knockdown/get-up state.

## Skill targeting and launch handoff

| Function / field | RVA / VA | Confirmed behavior | Confidence |
| --- | --- | --- | --- |
| `CTargeter::StartSkillTargetting` | `0x000B9E20` / `0x004B9E20` | Sets global skill-targeting active, clears the prior skill-target node, and stores targeting parameters | High |
| `CTargeter::EndSkillTargetting` | `0x000B9EF0` / `0x004B9EF0` | Clears global skill-targeting active and releases the global skill-target node and associated targeting presentation | High |
| `CTargeter::GetSkillTargettingModeTarget` | `0x000B9E00` / `0x004B9E00` | Resolves the GEL held by the global refcounted skill-target node at VA `0x007C3B44` | High |
| `CTargeter::EnableAutoTargetting(bool)` | `0x000B9CC0` / `0x004B9CC0` | Stores enabled state in bit `0x02` at `CTargeter+0x84`; disabling also releases and clears the ordinary target node at `+0x54` | High |
| `CTargeter::GetGelCurrentTarget` | `0x000B9DC0` / `0x004B9DC0` | Copies/resolves the targeter's ordinary current-target node; the returned wrapper address is not a stable target-identity key and is no longer used by the passive trace | High for non-mutating behavior; exact returned object semantics remain provisional |
| `Possible_ResolveMissileLaunchDirection` | `0x000C7AA0` / `0x004C7AA0` | Chooses a normalized live-target direction when eligible, otherwise an aiming-camera ray when owner flag `0x00400000` is set, otherwise copies the owner transform's forward vector from `+0x50..+0x58` | High for branch structure; provisional name |

Five exact-launch runtime snapshots found the global skill-target node and `CTargeter+0x54` both null, with skill targeting and auto targeting both disabled, immediately before Plasmatica's `FireMissileScripted(10)` dispatch. The same state occurred with and without the cinematic camera. A sixth snapshot captured owner aim-mode flags `0x00080812`, which excludes camera-ray bit `0x00400000`, and the owner transform forward vector. Combined with RVA `0x000C7AA0`, this confirms that the observed standard Plasmatica launch uses committed actor facing rather than a live target or the camera.

## Generic Skill Strike protection

| Function | RVA / VA | Confirmed behavior | Confidence |
| --- | --- | --- | --- |
| `Possible_BeginSkillProtection` | `0x000DC200` / `0x004DC200` | Called by `CSkill::Use`; increments arbiter byte `+0x54`, sets state flag `0x800`, and sets `IsUsingSkill` flag `0x10` | High; provisional name |
| `CCharacterArbiter::IsInvulnerable()` | `0x00008980` / `0x00408980` | Returns true when signed arbiter byte `+0x54` is greater than zero | High; named export and inspected implementation |
| `CCharacterArbiter::IsUsingSkill()` | `0x000089E0` / `0x004089E0` | Tests arbiter state bit `0x10` at `+0x50` | High; named export and inspected implementation |
| `CCharacterArbiter::GELSetInvulnerable(bool)` | `0x000DCA10` / `0x004DCA10` | Increments/decrements arbiter byte `+0x54` and sets/clears state flag `0x800` according to the resulting refcount | High; named export and inspected implementation |
| `Possible_CSkillUpdateCompletion` | `0x000B47A0` / `0x004B47A0` | On task completion, decrements arbiter `+0x54`, maintains/clears flag `0x800`, clears `IsUsingSkill` bit `0x10`, and continues normal skill cleanup | High; provisional name |

`CSkill::Use` therefore enters the same refcounted state exposed by the engine's named `IsInvulnerable`/`GELSetInvulnerable` API. This protection is generic to the native `CSkill` lifetime; the observed `PC_Elco1__Skill|P` bytecode contains no call to `GELSetInvulnerable|B`. Plasmatica does not appear to own a separate skill-specific damage-reduction value.

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

## Front-end portrait texture ownership

| Object / function | RVA / offset | Confirmed behavior | Confidence |
| --- | ---: | --- | --- |
| `cSOLMaterial` vtable | `0x002DEB7C` | Identifies the material stored at `UIElementCycleIcon+0x34`; `material+0x08` points to the resident-texture array | High |
| `cResidentD3DTexture` vtable | `0x002DD80C` | Identifies `material_texture_array[0]`; `+0x04` points to its resident backend | High |
| Resident backend GPU field | `backend+0x04` | Holds a valid Wine `IDirect3DTexture9*` for the decoded portrait | High |
| `UIElementCycleIcon` visibility | `0x0015C020` / `0x0055C020` | State `2` hides the original native anchor without releasing the material or resident texture | High |

This chain was live-confirmed for the Load Game page's Ailish, Tal, Buki, and
Elco portraits. It is a borrowing boundary only: the game continues to own the
material, resident wrapper, backend, and COM texture.
