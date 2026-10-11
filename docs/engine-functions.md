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

**CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE (2026-10-10):** the readiness
predicate at RVA `0xDB9B0` receives the arbiter in EAX. It requires arming bit 2
at `+0x50`, combat-request bit 2 at `+0x60`, a signed low-nibble mode other
than 1 or 3 at `+0x58`, and no flags in mask `0x02D9C6F0`. Its helper
`0xD0250` receives **the combat component at actor `+0xA4`**, after following
arbiter `+0x10`; it does not receive the arbiter. The helper rejects combat
`+0x72` bit 8 or action IDs 42 through 54 in the first nonzero signed 9-bit
lane of combat `+0x5C` (high, middle, then low). Decompilation alone obscures
that register change; the caller's instructions establish the component.
Neither predicate reads the leased AI count. `StoryCastReadinessImageTest`
executes both retail predicates and the native skill validator on inert
components from the hash-verified supported image, including these refusal
cases. It isolates readiness from resource availability/SP checks and does
not call `Use` or prove gameplay activation. Production validation remains
unchanged.

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

**CONFIRMED_STATIC / CONFIRMED_TEST (2026-10-10):** Spirit activation at
`0xFBA0` and the power-up script launch are different lifetime boundaries.
The manager's `Update(float)` at `0xF900` waits for participant readiness,
then calls `0x10C20`; its `ROOT_SPIRIT` call at `0x10DE5` submits the script
through `SUBMIT` at `0xFCFD0`. The saved-story remote owner therefore must
survive the request callback. The first remote candidate admitted activation
but rejected this later root because its short submission authority had ended.
The corrected owner permits one root only for the accepted, retained caster
inside that same live manager/camera generation, even while disconnect cleanup
is pending. Fresh requests still require current host admission.
`StoryCastAdmissionTest` reproduces the old rejection and passes the corrected
delayed-root, wrong-scope/unknown-identity, duplicate-root and retained-drain
checks. Native root and task hook guards are unchanged; this test is synthetic,
not proof of completed Spirit gameplay. Current live acceptance belongs in the
co-op plan progress log.

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
LAN protocol `LA18` therefore carries selectors `69` and `70` as separate WSS
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

## Saved-story remote melee auto-target refresh (#54 C1)

**CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE:** on the supported executable,
`0x4B8200` is the targeter scheduler callback (`ECX = CTargeter+0x18`). When
`CTargeter+0x84 bit 2` is enabled it calls `0x4B9AB0` with the targeter in
ESI. A current target with bit 1 set is retained while `AI+0x16A > 0`; simply
calling that dispatcher again does not replace the retained target.

The ordinary melee branch tail-calls `0x4BA1C0`: targeter in EAX, no stack
arguments, callee-saved registers preserved, plain RET. It queries up to 15
native world candidates, scores them against `targeter+0x48..0x50`, and calls
`0x4B9D20` (EDI = targeter, one callee-popped stack argument) to retain/release
the target through native intrusive-pointer operations. This leaf preserves
native dead/spawning-target filtering and the targeter's exclusion list.
It does not require a controller/lead swap or global lock-on UI changes.
Ranged acquisition is a different branch and is not rerouted by C1.

`0x4DAE80` writes its requested direction to `targeter+0x48..0x50` outside
ranged mode. With neutral input, C1 retains this accepted direction; a body
turn still catching up must not become the next target-search direction.
The candidate call `0x428031` actually invokes turn routine `0x4B6E50` with
the component at actor+0x8C; it is not the target-search call.

`StoryTargetRefreshImageTest` executes the native search and commit with inert
world candidates: opposite headings select opposite enemies, neutral input
retains the accepted heading, dead/no candidates clear the target, exclusions
remain native, and intrusive references retire correctly. It checks every byte
of the named acquisition/commit/exclusion contracts, including relocated
operands, and rejects foreign owners/worlds and unwritable target state.
The fixture substitutes only world enumeration, position preparation and vector
normalization; it is not live gameplay, world-enumeration or rendering proof.
See the C1 progress entry in `docs/coop-story-plan-54.md` for live status.

Ownership distinction: the saved-story observer's world is the singleton at
`0x808D10`; target enumeration uses the separate collision system at `0x808DD4`
and its registration vectors (`+0x70/+0x78` and `+0x80/+0x88`). C1 validates
these distinct identities; equating the two causes a fail-closed lease refusal.

## Saved-story leased player turning (#54 C2, 2026-10-10)

**CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE.** On the supported executable,
`0x4B73D0` (EDI = actor's `+0x8C` turn component; one float argument, `ret 4`)
selects player interpolation `0x4B7B90` only when AI is absent or
`ai+0x16A <= 0` and `*(ai+0x3C)+0x0B == 0`. The held remote-control reference
(count 1, mode 0) otherwise selects `0x4B7C50` or `0x4B7CC0`.

The turn function first calls `CanTurn` (`0x4DBB20`, ESI = arbiter). That
function has a separate AI-count decision: player rules reject an active
combo (`actor+0xB8 -> +0xA8`) or attacking (`arbiter+0x50 & 0x1000`). A change
to the interpolation branch alone misses this gate. The same `CanTurn` is
called by arbiter movement `0x4DAE80` and the separate turn mode `0x4B77F0`.

`story_player_turn.c` owns two whole-instruction branch seams: the compare
at RVA `0xB742E` and the compare at RVA `0xDBB3C`. For an exact HELD remote
lease with count 1/mode 0, their arithmetic flags emulate comparing zero;
the AI counter, mode, native registers, controller, leader and lock-on globals
are not changed. The second seam admits only return `0x4B73E8`, or return
`0x4DAEC0` inside the mod's witnessed movement call. Other callers, actors,
unknown ownership, unbound/wrong threads and game-raised counts retain native
comparisons. The game's `0x20000` forced-turn latch still follows native rules.

Authority is freshly checked against retained native roster/epoch/components,
HELD key/generation, live peer connection, acknowledged control fence, and
ordinary unpaused world. Research54's three turn observation callsites and
`RVA_ARBITER_MOVEMENT`'s existing entry-byte guard remain separate owners.
Install validates named turn-choice, CanTurn-choice, turn-gate, movement-gate,
player-argument and player-entry contracts. Failed reverse restoration retains
continuations and callbacks for retry.

`StoryPlayerTurnImageTest` executes the retail branches, CanTurn, player/AI
steps and native interpolation on inert components. It replaces only renderer
forward application, an actor-kind query and the D3DX normalization import.
It covers count/mode/lease/thread rejection, attack/combo gates, scoped versus
unscoped movement, FP/SSE/register/flag preservation, every-byte rejection,
second-hook failure rollback, restoration failure/retry and reinstall.
Observer callsite coexistence uses the same three callsites as Research54.
This is not network-authority, campaign-rendering or owner visual proof.


## Saved-story leased AI movement gating (#54 C3, 2026-10-10)

**CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE.** Supported executable SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
The `CAiDynamics` component is `actor+0x98`, vtable RVA `0x2D4994`, owner
`+0x10`. Constructor `0x53A300`, its actor-constructor callers, and the
movement/teleport request callers establish this identity. Its update
`0x4F4BB0` takes ECX = component and one timing-block pointer (`dt` at +0x0C),
returning with `ret 4`.

Before choosing movement, this update advances its fade (`0x4F4E00`), refreshes
AI state, measures displacement, and updates cached position. Its existing
local-player path `0x4F4CCD` copies current facing/speed into the node and
returns. The other path runs `0x4F4250`, then either submits an AI move at
`0x4F4D7F -> 0x4DAE80` or resets speed at `0x4F4DE3 -> 0x4DB070`.
A held remote actor fails the local-leader/count test even with the mod's
single AI hold. Skipping the whole update would also skip fade completion.

`story_ai_movement.c` owns the whole five-byte CMP at RVA `0xF4CB3`. For a
freshly admitted remote lease, it continues at the existing player-cache path
`0xF4CCD`. Otherwise it replays the original CMP and resumes at `0xF4CB8`.
Research54 retains sole ownership of the movement and SetSpeed callsites.

The shared placement helper `0x4F50C0` (one component argument, `ret 4`) also
serves scripted teleports. Request `0x4F30A0` records navigation adjustment at
node `+0xA9`; reviewed AI follow/catch-up callers pass 1. Explicit
`PopToNamedLocation`, `TeleportGroup`, temporary-zone restoration and authored
AI queue placement pass 0. A separate seven-byte CMP at RVA `0xF50FF` gates
only adjustment flag **exactly 1** for the admitted lease. It bypasses the
placement block to `0xF51CA`: position call `0x4F5176`, facing call `0x4F5185`,
turn-target call `0x4F51C5`, and associated native speed/dynamics writes are
skipped. Native intrusive-reference retirement, node cleanup and fade timers
still run. Flag 0/unknown requests retain native placement; this is not a
universal teleport blocker.

Both gates require the bound native thread, exact hook/image contracts,
reciprocal node/actor/AI ownership, AI count 1/mode 0, and the same fresh
HELD actor/roster/epoch/input/session/control-fence proof as C2. Game-raised
holds, other actors, revoked ownership, unknown contexts and menu/load states
retain native decisions. Main-update ESI/EBX/EDI ownership is checked as well.
No AI counter, mode, controller or leader is swapped. Registers, EFLAGS,
x87/SSE and LastError are preserved before the native continuation.

Named guards are `AI_MOVE_FRAME`, `AI_MOVE_CHOICE`, `AI_MOVE_PLAYER_CACHE`,
`AI_SNAP_FRAME`, `AI_SNAP_CHOICE`, and `AI_SNAP_CLEANUP`. Their spans contain no
image-base relocations. Reference scans found no interior branch targets in
either overwritten instruction; the existing branch to `0x4F50FF` lands at
its start. `party_native_entries_exact`, `RVA_ARBITER_MOVEMENT`, and
`RVA_MOVEMENT_CONTROLLER_SET_SPEED_IMMEDIATE` were checked by name and remain
unchanged. Installation is transactional; reverse restoration closes admission
and retains continuations/dependencies on failure for retry.

**CONFIRMED_TEST / CONFIRMED_EXACT_IMAGE:** `StoryAiMovementImageTest` executes
the retail update branches, player-cache path, placement block, cleanup and
fade timer on inert components. World/AI planning, rendering, movement payloads
and alpha application are fixture substitutes. Tests reproduce the original
held-actor AI stop/snap calls, check both command branches and observer
coexistence, preserve script-style placements, exercise intrusive retirement
and fade completion, and reject count/mode/owner/thread/lease changes. They
also cover registers/flags/x87/SSE/LastError, every-byte guard rejection,
failure at either install stage, restoration failure/retry and reinstall.
This does not establish visible movement, cutscene or whole-encounter parity.

### Story sky / cast-light coexistence (#54 combat follow-up)

**CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST**, supported EXE
SHA256 `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
`CLightManager::SetLightingChangeRate` at RVA `0x1F740` is a thiscall float
setter of manager `+0x30`, returning with `ret 4`. `light_image_exact` in
`cast_light_abi.c` requires its complete ten-byte native body. The story sky
adapter had replaced its first seven bytes, so private cast-light initialization
could not coexist with installed sky hooks. Search both the named guard and RVA
when changing either adapter; this was an ownership conflict, not a bad retail
executable.

The redundant sky rate hook is removed. `SudekiMpLanStorySkyCapture` already
captures the current rate and target every presentation report, and the client
applies them together under the existing restorable lease. No guard exception,
new seam owner or manual lighting tick is introduced. The legacy rate-call
message remains understood as a no-op; there is no protocol change.

`StorySkyLightImageTest` reproduces the old failure with sky installed, then
passes cast-light install/reset/reinstall alongside sky with the correction.
It rejects a damaged `SetLightingChangeRate` entry and checks rate-only state
capture and the existing client target/rate restoration cases. The fixture maps
the hash-verified image and uses inert manager/scheduler objects: it runs no
native constructor, effect, task, graphics-device call or gameplay cast.
Live story binding and remaining owner checks are recorded in the plan.

### Material color outputs and script timer yield (#54 Spirit follow-up)

**CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE**, supported executable identity
unchanged. D3D material property getter RVA 0x202E40 delegates argument 0,
members 0/1 to base getter RVA 0x1E6ED0: float4 storage at material +0x18/+0x28.
Argument 3, members 0/1 returns alpha storage at +0x24/+0x34; member 0 also
updates the established material flag/output flag contract. These properties
neither allocate nor invoke callbacks. Story material preflight admits these
outputs only inside the existing exact owner/material/curve closure. Missing
shader constant slots requiring definition replacement at 0x2032C0 were
refused in that candidate; the later SOL proof below narrows that refusal. StoryMaterialOutputImageTest invokes the retail getters on inert owned
storage, checks read-only preflight and negative bindings; it does not render.

**CONFIRMED_STATIC / CONFIRMED_TEST:** script STEP at RVA 0x1C41D0 compares
thread +0x44 against +0x48 before reading its opcode pointer at +0x0C.
An ordered strict-less comparison returns native yield 2 without a write,
callback or global access. The named step_prefix guard covers that entire
early-return path, and is checked at installation and for the waiting shortcut.
The cast-context adapter may call this native prefix without swapping global
namespaces only after checking thread/routing state and retained task identity.
Equal/expired/nonfinite clocks and substituted callbacks take full routing.
LanCastContextTest exercises the real prefix with an inert due-task body,
including stale handles, LastError, unowned tasks and refusal boundaries.
SkillTraceImageTest checks installation against the supported image. Frame
improvement and client Spirit visual parity still require live evidence.


### Material shader output: proven absent-slot branch (2026-10-10)

On the supported executable, D3DMaterialGetProperty (RVA202E40) maps
argument0/member2..5 through description+2C. An absent slot calls2032C0;
member shifts0,2,6,8 select a two-bit field in description+8. Nonzero fields
return -1 through20347F, with no allocation, replacement or object write;
the caller returns NULL. Zero fields reach the definition replacement path,
which that candidate refused. The later closed SOL proof below admits one
verified replacement family. Guarded entry2032C0,
return20347F and jump table20348C are checked with relocated pointers.
StoryMaterialOutputImageTest executes the real getter on inert materials and
proves unchanged material/description/flags on the absent-output branch.

The later C3 owner run takes the other branch: Tal's material token2,
argument0/member3 has an absent slot and descriptor flags `0x80000001`, so
bits2..3 are zero. It requires native shader-definition replacement; the
read-only absent-output allowance cannot fix this case. CONFIRMED_EXACT_IMAGE
read-only decompilation of the same supported executable shows
ShaderConstantEnsure `0x2032C0` calling ShaderDefinitionLookup `0x1E76D0`,
allocating a new float4 array, remapping existing constants, releasing the old
array/definition reference and retaining the replacement. Lookup searches a
flag-keyed cache and, on a miss, invokes registered factories. Those factory
and derived-definition lifetime paths were not admitted by that candidate.
The NULL-return test alone does not cover replacement; see the later proof below. See the C3 startup investigation in research-log.md.
CONFIRMED_EXACT_IMAGE; see the dated research-log entry for live limitations.


### Tal startup SOL definition expansion and scoped memory queries (2026-10-10)

CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST on supported SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`:
argument0/member3 of D3DMaterialGetProperty `0x202E40` can expand SOL flags
`0x80000001` to `0x8000000D`. ShaderConstantEnsure `0x2032C0` sets both
bits2..3, obtains the replacement via ShaderDefinitionLookup `0x1E76D0`,
allocates one zeroed float4, and transfers the material's counted definition
reference. The retained original remains unchanged. The read-only proof in
lan_story_material.c initially admitted only this pair (extended below), its exact registered factories,
cache membership, mapping arrays and native-owned constant storage.

Named exact code checks (RVA) cover ShaderConstantEnsure `0x2032C0`,
ShaderDefinitionLookup `0x1E76D0`, SolDefinitionFactory `0x202A30`,
SolDefinitionDelete `0x2014B0`, DefinitionUnlink `0x1E4080`,
BaseDefinitionFactory `0x1FB7C0`, WaterDefinitionFactory `0x216630`,
TextureCount `0x201670`, ConstantCount `0x2016D0`, TextureMapping `0x201730`
and ConstantMapping `0x2017D0`. Full routine hashes normalize only verified PE
relocations. Unknown factories, cyclic caches, cross-definition array aliases,
wrong maps and borrowed expanded constant storage still fail closed. Final
release of a derived definition is admitted only through the proved SOL
native destructor; originals required by replacement clones must survive.

StoryMaterialImageTest runs the retail factory, getter and derived-definition
release on inert owned objects with tracked allocator fixtures. Two creation /
release cycles preserve the original and release all tracked allocations.
StoryMaterialOutputImageTest separately verifies world preflight delegates with
the correct selected/original pair and refuses a rejected proof. Neither test
renders a cast or executes the entire native material destructor.

Spirit namespace Enter/Leave cache only OS memory-region/protection answers for
one synchronous transition. Every object, component, global and retained owner
value is still checked; no cache crosses native script execution or entry/exit.
SpiritInstanceAbiTest measures100 uncached versus12 cached VirtualQuery calls
for the same fixture's entry/exit pair and rejects changed identity/protection
at the next boundary. Failed leave retains its scope for retry. This proves
reduced query work, not live frame rate or visual acceptance.


### Tal SOL color closure and body-ready recovery (2026-10-10 follow-up)

CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST on the same supported SHA256 above:
Tal's animation can request argument0/members3,4,5 together. Their flag fields
start at bits2,6,8. The admitted closed family starts at `0x80000001` and each
field is either0 or3, giving eight definitions. Each enabled field adds one
float4; texture0 and the static layout remain unchanged. Before pose evaluation,
preflight checks every reachable cached definition, its private arrays, or the
exact registered factory chain for a missing entry. This also covers combined
states reached by several outputs during one native pose call. Intermediate
native definition deletion, constant remapping, original retention and final
release remain required. Unknown flag families and borrowed expanded buffers
are refused; no byte guard is relaxed.

StoryMaterialImageTest now executes all six output orders through the exact
retail factory/getter/destructor and checks preservation of prior colors,
cached-definition reuse, reference counts, cache unlink, ownership refusals
and complete tracked allocation release. The same eleven named native routine
checks above pass exact-image and deliberate byte-mutation checks.

CONFIRMED_STATIC / CONFIRMED_TEST: StoryCastBodyDrained distinguishes positively
quiescent native body/input/camera/tasks from a still-retained lighting tail.
Only movement and basic melee use this body gate. New casts, ownership transfer
and teardown retain the complete CastDrained gate. No completion flag is
forced and no native task is cancelled. Admission fixtures reject unknown or
stale identity, active body and retained task lineage in both paths. Visual
recovery timing and ending-effect parity remain owner checks.


### Tal startup with authored SOL texture fields (2026-10-10 correction)

The preceding closure was insufficient for Tal's full opening pose: material
token5 uses flag key `0x80000101`, with an authored texture in the member5
field. CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST on the same supported executable:
ShaderConstantEnsure `0x2032C0` expands only a zero field to3. An existing
texture field1 remains1 and its constant getter returns NULL without mutation.
SolDefinitionFactory `0x202A30`, TextureCount `0x201670`, ConstantCount
`0x2016D0`, TextureMapping `0x201730` and ConstantMapping `0x2017D0` determine
the exact counts, descriptors and mapping order. Their named full-code checks
are unchanged and still reject byte mutation.

The current closure admits eight original layouts: fixed texture0 plus an
optional texture at each field starting at bits2/6/8. Original constant count
must be zero. The corresponding 27 reachable definitions allow those fields
to be absent0, texture1 or constant3; value2 and other flag families remain
refused. Each actual derivation preserves all original texture fields and
only changes zero fields to3. Equal texture counts alone are insufficient:
moving a texture to a different field is rejected. Native factory/cache proof,
distinct owned arrays, writable constant storage and native deletion still
apply. Each original has at most eight reachable definitions, checked before
pose evaluation.

StoryMaterialImageTest executes all six output orders for all eight original
layouts. It checks all27 definitions, texture-wrapper identity/order, prior
color retention, NULL outputs for texture fields, exact cached reuse,
intermediate and final native release, and no outstanding tracked allocation.
This addresses the captured refusal contract; owner visual acceptance remains
required for startup, repeat casts and later effects.


The immediate live check of that 27-definition candidate exposed token4 with
flags `0x80800501`; it still held the opening frame. The current proof also
covers the factory's authored texture fields at bits4/10 and static-layout
selection at bit23. These fields are preserved exactly during expansion.
Bits4/10 remain0 or1; constant/unknown modes there are not admitted. Bit23
selects native static layout RVA0x323B40 instead of0x323AD0. Field4 texture1
creates the optional descriptor at definition+20; its exact contents, distinct
ownership and native destructor path are proved alongside the other arrays.

This gives64 original layouts and216 reachable definitions, with at most eight
reachable from any one original. The same full native routine byte guards
cover these factory branches. Exact-image tests execute all six output orders
for all64 layouts, preserve all textures and colors, and release every tracked
allocation through the verified native paths. Borrowed/malformed optional
arrays, changed layout selection and relocated textures are negative cases.
A bounded read-only Tal material inventory records the whole original table
before an individual output can refuse. This is diagnostic evidence only and
never grants permission to evaluate a pose. The owner reports the whole client
world freezing with Tal, consistent with atomic presentation refusal while
incoming host frames continue; visual acceptance remains pending.


Final inventory follow-up: all seven Tal originals use four flag keys:
`80000001`, `80000101`, `80800501`, `80802505`. The last also retains bit13;
its member4 expansion was the remaining live refusal. Native factory202A30
preserves that bit in the cache key without changing descriptor arrays.
The proof now retains it exactly, including a negative test for substituting
it. The exact-image fixture covers128 original layouts,432 reachable
definitions and all six output orders, including every observed Tal key.
All tracked allocations are released; the same eleven named byte guards
remain unchanged. This supersedes the insufficient216-definition candidate.

### Tal Spirit ending: renderer one-shot state versus entity lifetime

On supported image SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`,
`VisualChannelAdvance` RVA `0x221D70` takes EDI=renderer and one stack float
delta (RET4). For clip state bit0 clear it advances absolute time but samples
modulo clip duration. With bit0 set it clamps absolute and sampled time to
duration minus the native epsilon at RVA `0x2E3670`, then sets completion
bit `0x40`. `VisualStateSet` RVA `0x223240` is thiscall with channel, submodel
and state (RET12); `VisualTimeSet` RVA `0x223180` is thiscall with channel,
submodel, time and invalidation mode (RET16). TimeSet computes a modulo sample
even for one-shot clips, so seeking at/past duration is not a completion API.

CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE: the native factory finalizer
RVA `0x18830` separately records the entity's loop/lifetime flag. Native effect
update RVA `0x1319B0` requires entity loop clear before auto-retirement.
Thus the client can retain its existing host-owned entity lifetime while
selecting one-shot renderer playback through the native setter. No private
task completion or entity lifetime flag is overwritten.

The bounded correction applies to Tal's observed `SFXSS300` only: preserve
native completed state, catch up forward, cap a late seek just below duration,
and let the ordinary native update complete the clip. Unknown channel states
refuse. Host roster omission still retires the clone through the existing
identity and weak-reference path. Other looping resources retain their policy.

New full routine checks are named `VisualStateSet` (67 bytes), `VisualTimeSet`
(157 bytes) and `VisualChannelAdvance` (397 bytes). The only absolute operand
is checked and normalized; the native epsilon is checked too.
`SpiritVisualTimingImageTest` executes these retail methods on inert renderer
storage: old looping playback repeatedly samples the beginning during the
host's final hold, while the corrected playback keeps the terminal sample and
completion state. It covers late/exact-duration packets, repeat holds,
unsupported states, unchanged loop policy, every-byte mutation refusal and
the epsilon. This proves native timing, not rendered fade acceptance.


### Remote Tal Spirit camera observation and story playback

Supported retail SHA256: `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE: `CCameraCtor` RVA `0xE7110`
allocates a distinct 0x260-byte `RenderState` through allocator RVA `0x2484FA`
and `RenderStateCtor` RVA `0x1D1370`, then retains it at camera+0x34.
The read-only observer retains the existing exact Spirit generation, caster,
named-bank, selected-camera and scene-owner checks. A typed render state
(vtable RVA `0x2DD638`) must not alias another named camera. Its revision at
+0x2C, matrix at +0x90 and projection at +0xD0 are copied and rechecked.
No native camera update, registry mutation or render selection occurs.

The new native byte checks are tested by name against the supported image
and reject mutation of every byte in their checked instruction spans:

| Check | RVA | Meaning |
| --- | --- | --- |
| CCameraCtor.RenderStateSize | 0xE72BF | Allocate 0x260 bytes |
| CCameraCtor.RenderStateAllocate | 0xE72D1 | Call allocator RVA 0x2484FA |
| CCameraCtor.RenderStateConstruct | 0xE72DD | Call RenderStateCtor RVA 0x1D1370 |
| CCameraCtor.RenderStateResult | 0xE72E2 | Preserve constructor result |
| CCameraCtor.RenderStateStore | 0xE72FD | Store owned result at camera+0x34 |

`SudekiMpObserveSpiritInstanceView` reports the remote namespace's actual
selection: 0 native default, 1 InitCam, 2 SkillCam. FALSE means UNKNOWN.
The existing SetRenderCamera adapter remains the only selection-hook owner.
Native CSpiritCam state transition RVA `0x124E0` selects the configured
named camera at start and requests default at completion when its native
flags permit; resource/animation callbacks retain their existing namespace.
The host game's camera remains independently selected.

CONFIRMED_TEST: additive story message 37 carries only the observed Tal
Spirit camera, with control transaction, actor generation, scene epoch and
revision, stream/cast sequence, area and host time. It is 168 bytes plus the
28-byte envelope. An eight-sample history uses the world presentation time;
selection/end changes wait for their timestamp. Existing bounded matrix
interpolation preserves handedness and avoids blending large cuts. Older
messages and native saves are unchanged. No client gameplay Spirit task is
started. Native obstruction decisions are preserved by observing selection,
not by inventing a cinematic based on the cast timer.

The client's existing retained local camera receives the geometry. Its orbit
heading/distance stay saved; missing observations hold the last active view.
An observed default, confirmed ownership/area change, or host-cutscene
priority restores the normal view. Foreign camera/state writes or inaccessible
storage refuse restoration and retain the obligation for retry. Existing
native Quit completion is the final destruction witness. Visual camera
parity, obstruction behavior and live disconnect remain owner gates.


### Tal summon held particle clocks

Supported retail SHA256 `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE: renderer child dispatch RVA
`2228D0` reads model child count `+58`, renderer children `+64` and child flags
`+68`; bits 0/1 select BONA `+8` with channel-zero sampled time. Particle BONA
query `1FA070` returns child `+4`; `1F9F70` retains selector at child `+20` and
forwards the sample to particle instance `+14` through `231860`. The existing
story observer owns the call at `1F9FAE`; the new binding check stops before it.
`231860` forwards to custom nodes even when the supplied model sample is held.
Sampler `235590` treats a lower phase as a loop and derives a positive delta.

The story client's supplemental clock must not advance these native-driven
nodes during an intentional hold. Paired capture confirmed client-only wrap
and near-220-frame jumps at the completed Tal summon while host nodes held.
`SudekiMpLanArenaSpiritVfxCollectModelClocks` observes retained kind-8 weak
leases, exact root/wrapper/renderer, typed particle children, retained resource
slot, timing flags and selector. `SudekiMpLanStoryEffectsAdvance` confirms
unique main-manager membership and omits only the supplemental clock. Native
aging and host retirement are untouched; lists are local to one synchronous
verified presentation seam, and unknown ownership rejects the advance.

Additional named identities: VisualChildClockDispatch `2228D0`, 131 bytes,
FNV `9FAFB82F`; ParticleBoneTimeBinding `1F9F70`, 62 bytes, `7D6C4435`;
ParticleBoneQuery `1FA070`, 60 bytes, `47323B67`; ParticleModelTimeSet
`231860`, 179 bytes, `231AA289`. Each is exact-image and every-byte-mutation
tested. Existing `235520`/`235590` clock identities remain unchanged. Native
timing tests reproduce the old failure and corrected hold. The owner subsequently
accepted the ending spiral correction; see the 2026-10-10 C3 ordinary-skills
entry in the research log for the accepted build and remaining owner gate.

### Story sword-trail clock and ordinary skill presentation (2026-10-10)

CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE on supported executable SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`:
`cD3DTrailRenderer::Update` is RVA `225760`, primary vtable `2DFB14` slot 2.
The animated renderer's child dispatch calls it at `222A66`, return `222A68`;
ESI retains the parent renderer, EDI the child index, ECX the child, with
context and delta on the stack. Trail accumulation is independent of the
parent's animation channel time. Retail disassembly and passive paired
observations identify Tal's sword trail under his body renderer, not his
weapon mesh renderer. In the failing client, matching swing clips advance
while trail accumulation and bounds stay unchanged; the host advances both.

`lan_story_ambient.c` now owns this additional child-update seam. It replaces
only a zero delta during the current client presentation lease. A successful
same-frame world-pose publication, fresh roster/registry/actor/model identity,
unique child slot and retained typed resource must all still match. The
existing bounded render delta advances native trail history without replaying
body animation, events or gameplay. Unknown/stale ownership leaves the native
delta alone. Named guards: `TrailUpdateEntry`, RVA `225760`, length 6, FNV-1a
`235A3448`; `TrailChildDispatch`, RVA `222A50`, length 24, FNV-1a `A86BD27B`.
Both are checked by name/RVA against the retail image and every-byte mutation.
The inert continuation test exercises the actual x86 shim's register and
stack contract. Ownership fixtures cover stale frames, changed worlds,
duplicate actors/children, wrong resources and wrong threads. Live visual
acceptance and in-game teardown of the new seam remain pending.

The ordinary skill uses authored effects `SFXTS001_Shin_Splitter.HOM`
(`79853F03`) and `SFXSA002_Attack_Zone_Arc.HOM` (`55FE7CFB`). Both appeared on
the host during the owner's successful cast and were absent on the client.
The host observer previously admitted only Spirit lineage and its closed
resource set omitted both. Story-only kinds 33/34 now admit exact ordinary
cast lineage, including synchronous emissions before native Use returns.
The existing parent-free cosmetic backend uses the host transform, one-shot
phase and lifetime; it never connects authored events to a client actor.
LA42/SMP4 layouts and accepted kind ranges remain unchanged. Other ordinary
skill assets still require explicit resource research and admission.

Movement now uses the shared retained skill targeting phase observed through
the existing native `EnableSkillTargettingMode` (`29570`) contract. Only a
positive AIMING phase with remaining native time bypasses the ordinary skill
hold for movement. Execution flags, interactions, Spirit activity, cast
admission and strict drain remain guarded. This introduces no fixed aiming
timer and does not grant movement throughout a full skill execution.

### Ordinary Skill camera publication (2026-10-10)

CONFIRMED_STATIC: story cast camera capture previously published the retained
native selection only while `spirit_started` was set. Ordinary skills already
used the same private named-camera bank, render-selection routing and shared
cast sequence, but their observation was replaced with kind 0. The corrected
capture admits either an ordinary started skill or a started Spirit Strike.
It copies the native InitCam/SkillCam choice and geometry without choosing
an angle itself. Native obstruction/default choices, positively observed
cast completion and UNKNOWN observation behavior are preserved. This applies
to remote Tal's ordinary skills generally, not a Shin Splitter timer or
special case. At this checkpoint, other characters remained outside this
presentation contract; the subsequent full-roster extension is described below.

No native hook, instruction guard, camera-global owner or packet layout is
added or changed. Existing story message 37 carries the view on the presented
world timeline; the accepted client orbit restoration and host camera
independence remain the same. `StoryCastAdmissionTest` reproduces the former
ordinary-camera suppression before the correction and checks both cast
families afterward, including default, InitCam, SkillCam, drain, recast,
invalid geometry, changed owner and unknown observation without output writes.
The existing camera transport, native presenter and named byte-check tests
also pass. See the research log for the candidate identity and owner gate;
the owner subsequently confirmed the Shin Splitter camera worked on this
candidate. Other Tal skills and broader obstruction cases remain untested.
At that checkpoint, capture, camera message validation and per-peer
publication explicitly required character 2 (Tal).


### Full-roster Skill and Spirit camera publication (2026-10-10)

CONFIRMED_STATIC / CONFIRMED_TEST: `SudekiMpLanStoryCastCaptureView` now takes
an explicit roster character and observes only that character's retained
`SudekiMpSpiritInstance` camera bank. It refuses missing or stale actors and
the current host-local character. Both ordinary skills and Spirit Strikes
publish the owner's native default/InitCam/SkillCam selection, geometry and
cast sequence. Completion is positive native evidence; an unknown observation
still cannot manufacture an end. There is no skill-slot or Tal-only branch.

`lan_story_runtime.c` captures each character independently.
`SudekiMpLanPartyPublishStoryCastView` routes only to a ready peer with an
acknowledged control fence for the same character. `lan_story_cast_view.c`
accepts all four valid roster characters using the existing fence validator.
Wire message 37 and its layout are unchanged. Clients continue to require the
full current scene/control/actor fence and apply the camera on the presented
world timeline. Native fallback, host-cutscene priority and retryable prior-
orbit restoration use the existing implementation. Host-local native camera
ownership is unchanged.

Focused tests exercise all four typed native camera namespaces and both cast
families; a host plus three clients with every character as leader; independent
camera start/end; stale ownership refusal; same-character ownership release
and reacquisition; sparse roster capture; and per-character presenter
restoration, foreign-write refusal and retry. Existing named camera-constructor
byte checks still pass against the supported executable and every-byte
mutation. No native hook or byte signature changed. See the full-roster C3
entry in the research log for the candidate identity, exact checks and owner
gate. Camera visuals for all characters, live reassignment and individual
skills remain UNKNOWN pending owner tests. Other roster skill-effect assets
are outside this camera extension.


### Native single-player Research54 launch (2026-10-10)

`[StoryAreas] NativeResearch54=true` is a default-off exclusive loader profile
for native combat comparisons. It uses the existing service-only controller
vtable dispatch: native update runs once, then the owned observer samples
Research54 and the native scene/roster. It does not install a network session,
remote actor leases, movement corrections, cast routing or replica presentation.
Optional existing TestStart coordinates use the retained native position setter
after READY/load settling; without coordinates the profile only observes.

Area-follow identity checks for `RVA_SWITCH_ZONE_NOW` 7990 and `RVA_ENTER_ZONE`
7970 must run before `SudekiMpLanStoryObserverInstall` patches those entries.
Reversing this order caused a correctly refused startup; correcting the order
allowed native title startup and live diagnostics without relaxing the checks.
The owned callback has an entry gate; shutdown disables/unregisters/drains it
before removing its dependencies. Native-thread or restoration refusal retains
those dependencies for retry. Existing service-only lifecycle/image tests pass;
this profile's live gameplay comparison and teardown remain pending. See the
2026-10-10 C3 native comparison entry in the research log for build identity.

### Saved-story C4 native movement contract (2026-10-10)

CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE: the native controller at RVA
`0x28B00` submits movement during melee and delegates translation permission
to `CArbiter::Move` (`0xDAE80`, stdcall: arbiter, direction, speed, blend,
mode). Move calls `CanMove` (`0xDB800`); attack bit `0x1000` alone does not
prohibit movement. Combat `+0x72` bit `0x10` blocks it unless combo `+0xB1`
bit `4` permits it. Movement `+0xBE` bit `8` owns speed writes, but a disabled
speed write does not suppress Move's target-heading update. Do not replace
these decisions with an attack-wide idle check.

Ordinary neutral input uses `CArbiter::SetSpeed` (`0xDB070`, thiscall:
arbiter, zero speed, blend `1.0`). This changes target speed at movement
`+0x24` and blend at `+0x30`, preserving current speed at `+0x28`; the
movement write-enable remains authoritative. Lifecycle drain/acquisition is
a separate immediate-stop contract and has not changed.

Input action `0x2B` supplies scale `2.0 - magnitude`. The native controller
shapes the lateral/vertical pair at `0x289D0`, with vertical zero, so lateral
input clamps to one while forward input retains its scale. Speed is the
length of this shaped vector, before camera-relative direction normalization.
It is not a universal multiplier: scaled `(1.8,0)` requests speed `1.0`,
`(0,1.8)` requests `1.8`, and `(1.8,1.8)` requests approximately `2.059`.
Both horizontal axes at or below the stored float deadzone `0.001` are neutral.
The helper uses exact binary float constants to avoid x87 excess decimal
precision changing the inclusive boundary.

`story_player_movement.h` preserves these rules while retaining the client's
independent camera transform and the host's existing actor/lease and C2 turn
scope. Its eight named exact-image contracts cover input scaling, local
shaping, deadzone, vector assembly, magnitude, Move, blended stop and CanMove.
Host and client admission verify them before enabling this path; no new patch
seam is installed. `StoryPlayerMovementImageTest` executes the supported native
controller with inert camera/Move sinks, then executes native Move/SetSpeed
with synthetic actor components. It checks 22 input cases, native combo/cast
arbitration, write ownership, release blending and every-byte refusal for each
named code contract. This establishes request/arbiter parity, not visible
locomotion or complete engine-update parity. See the C4 research entry for
candidate identity, transport compatibility and the pending owner gate.

### Saved-story validation cost: scoped page queries (2026-10-10)

CONFIRMED_STATIC / CONFIRMED_TEST: `lan_story_client.c::registry_exact`
performs a complete retained-entity ownership/pause walk without native calls
or external callbacks. Each invocation now opens a fresh page-access scope and
clears it on every exit. If called from an existing presentation scope, that
outer scope resumes with an empty cache. Entity pointers, both vtables,
membership, scheduled bits and pause references are still read and compared
for every entry. This does not cache a successful registry observation.

`lan_story_effects.c::topology` likewise validates the entire manager,
instances, descriptors, nodes, methods, clocks and resource backlinks on every
call. A bounded page-access cache lives only inside that callback-free walk
on the native thread. It is discarded before any subsequent witness, custom
clock advancement or native particle update. No cached node validity or page
answer survives into another call or frame. Unsupported/guard/no-access pages,
overflowing ranges and changed object values retain their refusal behavior.

The production-walker regressions compare caching disabled/enabled on the same
synthetic objects: 512 entity entries require 528 versus 3 VirtualQuery calls;
318 particle instances require 320 versus 3. Changed fields are still refused,
including the last entry; changed page permissions, freed memory and retries
are re-observed. These query counts establish reduced validation work, not an
FPS prediction. Existing effect clock, spawn timing and hook tests pass. No
native patch seam, byte contract, network format or update frequency changes.
See the corresponding performance follow-up in the research log for the live
CPU evidence and pending owner check.


## CBattleScores encounter presentation (supported image)

| RVA | Function / contract | Evidence |
| --- | --- | --- |
| `0x39560` | BattleScoreLoad: ECX=score, native score name, RET4, bool AL; owns previous emitter/cue retirement | CONFIRMED_STATIC; named exact-image guard |
| `0x39A00` | BattleScoreReset: clears current emitter/cue and score name | CONFIRMED_STATIC; named exact-image guard |
| `0x39AD0` | Cluster battle listener: ECX=score+0x30, RET12; third argument battle boolean, first two unused; scalar-only flag/intensity change | CONFIRMED_EXACT_IMAGE; native entry/exit fixture |
| `0x391C0` | Update: ECX=score+8; native score playback, variation and fades | CONFIRMED_STATIC; named exact-image guard |
| `0x397E0` | BattleScoreIntensity: sets current intensity and peaceful default; unsuitable for a battle-only projection | CONFIRMED_STATIC |

Global score pointer: RVA `0x409D80`. See research-log's encounter music entry,
`src/hooks/lan_story_music.c`, and `tests/story_music_image_test.c`. No new
hook owns these entries; the saved-story adapter calls only on a verified
native thread. Image fixtures do not establish audible parity.


### Saved-story native melee timing display

`lan_story_combo_hud.c` observes accepted-input display call RVA `0xD06B0`
(to `0xD14D0`, EAX=melee component), reset `0xD0440` (EAX=component), and
window marker seam `0xD0A3B` (ESI=component+0x3C, marker at [ESP+8]).
This last seam precedes ESI restoration; the later `0xD0A75` call cannot
identify the component from ESI. `StoryComboHudImageTest` verifies that
`BukiReplicaNativeImageMatches` remains valid with these observers installed.

HUD mode hide `0xAED20` uses EAX=mode/ECX=HUD; show `0xAE9D0` uses
EAX=mode/ESI=HUD. Label `0xAE7C0` and icon refresh `0xAF110` use EAX=HUD.
All four return with plain RET. The native three-icon row uses 0/1 for
weak/strong, 2/3 for mistimed inputs and 7 for empty. The saved-story client
projects these native UI fields without enabling native gameplay combat.
See the 2026-10-10 combo/light entry in `docs/research-log.md` for evidence
and the remaining owner acceptance gate.

### Saved-story native character audio

`lan_story_character_audio.c` owns the saved-story observation detour at
actor PlayCue RVA `0xFC200`: ECX is the actor sound component, ESI points to
the output handle, and four stack arguments are cue name, attachment mode,
gain and distance-bypass flag; native returns with RET16. It observes only
successful exact roster-owned calls. Client playback uses this same ABI;
thread/roster/component/bank/cue and listener identities are independently
checked. The detour and trampoline remain pinned, with admission logically
closed when the scene/runtime is unavailable.

Actor+`0x5C` owns the sound component (vtable RVA `0x2CD0AC`); its embedded
emitter at +`0x4C` owns bank +`0x50` and the attachment tuple +`0x30` referenced
at +`0x54`. The cue tracker at global RVA `0x409E8C` uses 24-byte entries and
handles containing an 8-bit slot plus generation. Positive retirement or a
changed generation never grants further access to the old cue. Synchronous
bank Stop RVA `0x280C30` can immediately free the cue; handle retirement
`0x284AE0` is checked without any subsequent cue dereference. See
StoryCharacterAudioImageTest for named exact-image checks and synthetic ABI,
source-owner, deduplication and lifetime coverage. Audible acceptance remains
separate from these checks.


Client character audio additionally needs the presentation equivalent of
`ActorSoundTransform` RVA `0xFC560`. Native ActorPlayCue compares camera position
to sound-component `+0x84/+0x88/+0x8C`, not directly to actor CPosition. A paused
replica can therefore be correctly displayed yet silently rejected at its old
sound origin. Refresh both that origin and the attachment tuple at `+0x34`
from the exact actor pose. Nonzero sound locator `+0xD4` selects the current
model locator/bone composed with the body matrix; no actor/model task is run.
For a live owned attachment, native `AttachmentPosition` RVA `0x284520`
(ESI points to component `+0x30`) propagates position and existing velocity.
The attachment's native position/velocity setters are RVAs `0x281860` and
`0x2817F0`; its sound-list buffer getter is `0x22E940`. Exact registered
attachment, linked-list and sound identities are required. This path serves
all four roster actors and does not bypass range culling or alter gain.
The distance regression and active-emitter movement run against the supported
image in StoryCharacterAudioImageTest; audible acceptance is tracked in the
plan's client-emitter correction entry.
