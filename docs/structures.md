# Structures

## Provisional `CGameSpeed` layout

Applies to executable SHA256 `8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`. The singleton pointer is stored at VA `0x00808DA0`.

| Offset | Type | Provisional name | Evidence | Confidence |
| ---: | --- | --- | --- | --- |
| `+0x1C` | `int32` | previous mode | Mode-transition function copies `+0x20` here before updating current mode | Medium |
| `+0x20` | `int32` | current mode | Named `GetStateAsInt` export returns this; frame loop reads it | High |
| `+0x24` | `int32` | requested mode | Named `SetSpeedMode` writes this; frame loop compares it to `+0x20` | High |
| `+0x28` | `bool` | paused | Named `IsGamePaused` returns this; frame loop substitutes zero delta when set | High |
| `+0x2A` | `uint16` | pause reference count | Pause helper adds/subtracts a reference and derives `+0x28` from nonzero state | Medium-high |
| `+0x2C` | `float` | one-time speed value | Named setter validates `[0.0, 1.0]`; frame loop uses it only for mode `4` | High |

Mode values established statically:

| Mode | Frame scale source | Known use |
| ---: | --- | --- |
| `0` | normal-speed global, currently `1.0f` | Quick Menu close requests this |
| `1` | fixed alternate scale, `0.07f` | Quick Menu open requests this |
| `2` | fixed alternate scale, `0.07f` | Use not yet identified |
| `3` | fixed alternate scale, `0.07f` | Use not yet identified |
| `4` | per-instance float at `+0x2C` | Variable one-time speed mode |

This layout remains isolated and provisional until a runtime inspection confirms the object values and transitions.

## Provisional character-switching layouts

These partial layouts apply to the exact supported executable. Names remain provisional where the native class export or live behavior does not establish semantics.

### `CGroupPlayers`

| Offset | Type | Provisional name | Evidence | Confidence |
| ---: | --- | --- | --- | --- |
| `+0x90` | intrusive character pointer | active/front party entry | Native QuickSkill resolves this entry; live controller target always matched it | High |
| `+0x9C`, `+0xA8`, `+0xB4` | intrusive character pointers | remaining ordered party entries | Previous/Next rotation routines rewrite these four entries at 12-byte strides | High |
| `+0xCC` | `int32` | party count | Switch consumers require it to exceed one; rotation specializes for counts 2, 3, and 4 | High |
| `+0xD0` | `int32` | switch-blocking state/refcount | Both switch consumers require zero | Medium-high |
| `+0xD6` | `bool` | player switching enabled | Exported Toggle/Get methods write/read it; controller update requires nonzero | High |

### Character controller singleton

The singleton pointer is stored at VA `0x00808DA4`.

| Offset | Type | Provisional name | Evidence | Confidence |
| ---: | --- | --- | --- | --- |
| `+0x80` | `int32` | current controller mode | Frame update branches on it; unchanged at `1` during the live switch | Medium-high |
| `+0x84` | `int32` | requested controller mode | Frame update reconciles it with `+0x80`; unchanged at `1` during the live switch | Medium-high |
| `+0xF4` | action state | Next Character state | Action `0x33` maps here; frame update calls the Next consumer when it equals `1` | High |
| `+0xFC` | action state | Previous Character state | Action `0x32` maps here; frame update calls the Previous consumer when it equals `1` | High |
| `+0x184`, `+0x188` | `float` | CameraU/CameraD input staging | Input actions `0x69/0x6A` write their payloads here. Late wheel injection was visible in logs but did not move the camera, so these are not a confirmed desired-distance control point. | High for writes; low for downstream role |
| `+0x248` | intrusive character pointer | controlled/front character | Shared reassignment writes the new front character; live snapshots matched group slot 0 | High |

Combat-state fields submitted by the controller consumer at RVA `0x000286C0`:

| Offset | Type | Provisional name | Evidence | Confidence |
| ---: | --- | --- | --- | --- |
| `+0x8C` | action state | Weak Attack | Passed as the first stack state to RVA `0x000DB0E0` | High |
| `+0x94` | action state | Strong Attack | Passed as the second stack state | High |
| `+0x9C` | action state | Sweep Attack | Passed as the third stack state | High |
| `+0xA4` | action state | Block | Loaded into `EAX` for the native mixed-ABI call | High |
| `+0xAC` | action state | Weapon Next | Passed as the fourth stack state | High |
| `+0xB4` | action state | Weapon Previous | Passed as the fifth stack state | High |

Movement-specific controller fields:

| Offset | Type | Provisional name | Evidence | Confidence |
| ---: | --- | --- | --- | --- |
| `+0x1A0`, `+0x1A4` | `float` | local movement axes | RVA `0x00028B00` reads them, preprocesses them, and camera-transforms the result | High |
| `+0x1BC` | `float` | one-shot/direct movement input | Replaces the first local axis when nonzero, then is cleared by the movement consumer | Medium-high |
| `+0x1D4` | `float` | direct movement speed | Exported setter/getter at RVAs `0x00029350/0x00029360`; used only in movement mode `1` | High |
| `+0x1F0` | 4x4 matrix | stored movement camera transform | RVA `0x000291A0` removes translation and transforms the local movement vector | High |
| `+0x230..+0x238` | `vec3` | last submitted world movement | Movement consumer stores its final normalized vector here | High |
| `+0x23C` | `int32` | movement mode | Exported setter/getter at RVAs `0x00029330/0x00029340`; mode `0` uses the arbiter path and mode `1` uses absolute delta movement | High |

### Party character position

| Offset | Type | Provisional name | Evidence | Confidence |
| ---: | --- | --- | --- | --- |
| character `+0x44` | `CPosition*` | world-position component | Script-facing `SetPlayerPosition` resolves active group slot 0 and passes this pointer to RVA `0x00003050` | High |
| `CPosition+0x18` | `float` | world X | Internal setter compares and writes the first supplied coordinate | High |
| `CPosition+0x1C` | `float` | world Y | Internal setter compares and writes the second supplied coordinate | High |
| `CPosition+0x20` | `float` | world Z | Internal setter compares and writes the third supplied coordinate | High |

The live-confirmed maximum-separation guard reads only X/Z from two party characters. It does not write `CPosition`; all movement continues through the native arbiter submission. At the 10-unit test limit, outward requests were repeatedly blocked near horizontal distance-squared `100`, while inward/within-limit requests were released immediately.

### `CTargeter`

The party character stores its native targeter pointer at `character+0xAC`.

| Offset | Type | Provisional name | Evidence | Confidence |
| ---: | --- | --- | --- | --- |
| `+0x54` | intrusive target pointer/node | ordinary current target | `EnableAutoTargetting(false)` releases and clears it; `GetGelCurrentTarget` copies and resolves it | High |
| `+0x84` bit `0x02` | flag | automatic targeting enabled | Native enable/disable function updates this bit; prior Plasmatica snapshots confirmed it | High |

The passive trace reads only these fields. During the first live Buki AI override, `+0x54` stayed non-null and stable while the `+0x84` auto-target bit remained enabled; Buki's AI then restored cleanly. This confirms retention of native targeting state, but not the node's final entity identity or the writer/scoring path. An experimental call to `GetGelCurrentTarget` returned changing wrapper addresses, so wrapper address identity was removed from the diagnostic.

### `character+0x94` component candidate

Every observed party character held a non-null pointer at `character+0x94`; pointer presence did not change during switching. Shared post-rotation reassignment passes the old and new values to RVA `0x000EF700`.

| Offset | Type | Provisional name | Evidence | Confidence |
| ---: | --- | --- | --- | --- |
| `+0x10` | pointer | owning character | Transition logic dereferences character components through it | Medium-high |
| `+0x3C` | pointer | nested mode state | The transition reads/writes its byte `+0x0B` | High for layout |
| nested `+0x0B` | `uint8` | AI-active/control mode | Live Previous/Next switches showed `0` only for the front/controller target and `1` for every AI party member; ownership transfer flipped the old `0→1` and new `1→0` | High |
| `+0x44` | flags | behavior flags | Selects internal behavior state during transition | Medium |
| `+0x16A` | `int16` | AI control-override refcount | Exported `AiIsOverriden`, `AiOverrideControl`, and `AiDefaultControl` query and modify this count; first acquire disables AI and final release restores it for a non-front character | High |

### Provisional `CCharacterArbiter` combat fields

The party character stores its arbiter pointer at `character+0x90`. The following partial fields are used directly by the per-arbiter combat-input function at RVA `0x000DB0E0`; precise flag names remain provisional.

| Offset | Type | Provisional name | Evidence | Confidence |
| ---: | --- | --- | --- | --- |
| `+0x10` | pointer | owning character | Combat submission follows it to movement/weapon/target components and validates their presence | High for ownership relationship |
| `+0x50` | `uint32` flags | arbiter state/capability flags | Combat submission requires bit `0x2`; bit `0x400000` selects ranged/weapon behavior; other confirmed systems also use this field | High for bits; provisional names |
| `+0x58` | `uint32` | attack/weapon state | Low and next nibbles select native combat branches; the submission updates weapon-related subfields | High for use; provisional semantics |
| `+0x60` | `uint32` flags | combat capability flags | Combat submission requires bit `0x2` and conditionally toggles bit `0x8` during weapon changes | High for bits; provisional names |

## Provisional `CSkill` fields

Applies to the same exact executable build. These offsets are supported by `CSkill::Use` and its per-frame completion path.

| Offset | Type | Provisional name | Evidence | Confidence |
| ---: | --- | --- | --- | --- |
| `+0x10` | pointer | owning actor/component data | Used throughout validation, SP access, model state, and events | Medium-high |
| `+0x3C` | `SkillData*[6]` | skill data table | Indexed by the `Use(int)` argument; constructor allocates six entries | High |
| `+0x6C` | `bool` | skill active/queued | Set to `1` by `Use`; tested by the per-frame completion function | High |
| `+0x70` | `int32` | active equipped slot | Written by `Use`; used to select data for `OnSkillEnded` | High |
| `+0x74` | reference-counted pointer | script/task handle | Assigned from script invocation; completion waits for it to finish | High |

### Provisional `SkillData` fields

| Offset | Type | Provisional name | Evidence | Confidence |
| ---: | --- | --- | --- | --- |
| `+0x0C` | resource/string field | script-name suffix or component | Loaded while constructing the invoked skill function name | Medium |
| `+0x10` | inline resource/string storage | display/configured name | Runtime object contained UTF-16 `Plasmatica` here | Medium-high |
| `+0x94` | `int32` | SP cost | Compared against current SP and subtracted on successful use | High |
| `+0xA0` | pointer | owning `CSkill` | Constructor assigns the parent to each of six `SkillData` entries | High |

## `CSimpleGameModelInterface` animation-speed fields

This is a partial layout for the exact supported build. `CNewGameModelAnimation` derives from this interface at offset zero. Elco's live concrete `CNewMissileAimingGameModelAnimation` derives through that same chain at offset zero; two successful 2.0x casts read and restored its `+0x48` field as `1.0`.

| Offset | Type | Meaning | Evidence | Confidence |
| ---: | --- | --- | --- | --- |
| `+0x44` | `float` | animation-speed factor A | Multiplied by the public setter/reset functions | High for arithmetic; role unnamed |
| `+0x48` | `float` | public animation-speed multiplier input | Written by `SetAnimationSpeedMultiplier`; reset to `1.0` | High |
| `+0x4C` | `float` | animation-speed factor B | Multiplied by the public setter/reset functions | High for arithmetic; role unnamed |
| `+0x50` | `float` | effective animation speed | Product of `+0x44`, `+0x48`, and `+0x4C`; returned by the public getter | High |

## Camera target structures

### Partial `CCamera`

| Offset | Type | Meaning | Evidence | Confidence |
| ---: | --- | --- | --- | --- |
| `+0x3C` | pointer | active camera state/listener | Target installer calls virtual `+0x40` when non-null | Medium-high |
| `+0x40` | ref-counted pointer | state notification companion | Retained while notifying target changes | Medium |
| `+0xB4` | `Camera::Target*` | composed/current target slot 0 | Live exploration held an `OffsetTarget`; shared character reassignment can install the new front target directly | High |
| `+0xB8` | `Camera::Target*` | underlying/destination target slot 1 | Live exploration held the front character's `GameObjectTarget`; reassignment can install the same target in both slots | High |

`CGameCameraMode+0x0C` points to `CCamera+0x2C` in the confirmed reassignment path.

### Partial `Camera::OffsetTarget` (size 0xD0)

| Offset | Type | Meaning | Evidence | Confidence |
| ---: | --- | --- | --- | --- |
| `+0x00` | vtable pointer | `Camera::OffsetTarget` vtable | Static VA `0x006D436C`; matched live slot 0 after relocation | High |
| `+0x04` | `uint32` | intrusive reference count | Same native install/release convention as other targets | High |
| `+0x20` | `Camera::Target*` | wrapped source target | Virtual methods delegate identity/state to it | High |
| `+0x70` | `float[16]` | offset matrix | Update multiplies the wrapped target transform by this matrix | High |
| `+0x80` | `float[16]` | composed output matrix | Virtual `+0x1C/+0x20` returns this matrix | High |
| `+0xB0` | `float[3]` | cached composed position | Virtual `+0x10` returns this vector | High |

### Partial `Camera::MatrixTarget` (size 0x80)

| Offset | Type | Meaning | Evidence | Confidence |
| ---: | --- | --- | --- | --- |
| `+0x00` | vtable pointer | `Camera::MatrixTarget` vtable | Constructor writes VA `0x006D43BC` | High |
| `+0x04` | `uint32` | intrusive reference count | Create/install/release paths increment and decrement it | High |
| `+0x14` | `float[3]` | cached translation/position | Virtual update copies the position returned through slot `+0x10` | High |
| `+0x20` | `float[16]` | owned D3DX matrix | Native creator copies all 16 values here | High |
| `+0x50` | `float[3]` | matrix translation `_41/_42/_43` | Virtual `+0x10` returns owned matrix `+0x30`, which is object `+0x50` | High |
| `+0x60` | `float*` | active matrix pointer | Creator points this back to owned matrix at `+0x20` | High |
| `+0x70` | target pointer | next/older list link | Creator links previous list head here | High |
| `+0x74` | target pointer | previous/newer list link | Creator writes the new target into the old head's `+0x74` | High |

All offsets are exact-build facts. The midpoint prototype writes only the owned matrix and cached translation of a target it created through Sudeki's own manager; it does not reinterpret an existing `GameObjectTarget` as this type.

## Partial `CElcoAbility` fuel state

The live Elco actor stores its `CElcoAbility*` at `actor+0x104`. This ownership
edge is used by the native fuel-crystal update at VA `0x0059C8C0` before it
calls the exported fill/empty/max methods.

| Offset | Type | Meaning | Evidence | Confidence |
| ---: | --- | --- | --- | --- |
| `+0x68` | `float` | maximum jetpack fuel | Written by `SetFuel` and `SetMaxFuel`; zeroed by `ResetFuel` | High |
| `+0x6C` | `float` | current jetpack fuel | Returned by `GetFuel`; written by `SetFuel`; optionally written by `SetMaxFuel` | High |
| `+0x70` | `float` | configured refill rate | Copied to active rate by `SetFillupRate` | High |
| `+0x74` | `float` | configured drain rate | Copied to active rate by `SetEmptyRate` | High |
| `+0x7C` | `float` | active fuel rate | Selected by fill/empty methods and zeroed by `ResetFuel` | High |

The cleanroom infinite-fuel option validates the live actor/ability and finite
current/maximum values on every maintenance pass. It uses the native setter
and preserves the authored maximum instead of writing an arbitrary capacity.

## Partial `CCombat` knockback-session state

This is the exact supported-build layout used by the disabled-by-default
Talos defense trace. `CCombat+0x10` points to the owning character.
Three independent live Talos instances placed this embedded `CCombat` at
`character+0xF64`; the co-op balance service verifies that owner link before
writing any setting.

| Offset | Type | Meaning | Evidence | Confidence |
| ---: | --- | --- | --- | --- |
| `+0x5C` | `uint32` | three packed 9-bit presentation/reaction IDs | Native reaction selection shifts/masks the three fields | High |
| `+0x60` | `uint16` | authored qualifying-reaction limit | Loaded from `Num KnockBacks in Session`; Talos value is 10 | High |
| `+0x64` | `float` | authored session duration | Loaded from `KnockBack Session Length(seconds)`; Talos value is 10.0 | High |
| `+0x68` | `float` | active session timer | Loaded from `+0x64` on the first qualifying reaction | High |
| `+0x70` | `uint16` | current qualifying-reaction count | Incremented only for reaction IDs `0x2A..0x36` | High |
| `+0x72` | `uint8` flags | bit 3 qualifying-family active; bit 0 threshold tripped | Exact consumer VA `0x004D2170` | High |

The threshold comparison uses the count before incrementing. Thus configured
limit 10 trips bit 0 on reaction 11 within the same active session.
Live Blade Dance testing confirmed the layout: count reached 11, bit 0 set,
four later damage packets produced no reaction while still reducing HP, and
both count and bit reset after the 10-second session expired.

### CTargeter ordinary melee selection (#54 C1)

Exact-image/static follow-up to the earlier target trace: `CTargeter+0x48`,
`+0x4C`, `+0x50` hold the accepted targeting direction. Native arbiter movement
updates it outside ranged mode; ordinary search `0x4BA1C0` scores against it.
`+0x84 bit 1` retains the current target in dispatcher `0x4B9AB0` when the
actor's AI override count is positive. C1's native melee-leaf call preserves
that flag and uses the normal commit helper for the intrusive slot at `+0x54`.
The exclusion count is at `+0x88`, with a pointer to 12-byte entries at `+0x90`.
These facts do not establish remote combat parity; see the exact-image test
and owner gate recorded in the co-op story plan.


### SOL startup material definition (C3, 2026-10-10)

CONFIRMED_EXACT_IMAGE / CONFIRMED_TEST; supported executable and named routines
are recorded in engine-functions.md. The initial SOL pair uses definition
vtable RVA `0x2DEB70`, references +04, flag key +08, texture count +0C,
constant count +10, descriptor count +14, static layout +18, owned descriptor
array +1C, optional array +20, cache-next +24, six texture mappings +28 and
four constant mappings +2C. The closed flags `0x80000001` / `0x8000000D`
retain one texture; the latter adds the member3 float4 at constant slot0.
Material +38 owns that constant buffer after expansion; borrowed-storage flag
`0x200` is refused for the derived material. This is a narrow validated family,
not permission to accept other readable descriptions or mapping arrays.


The C3 follow-up extends that initial pair to the eight closed combinations of
member3/4/5 fields at flag bits2/6/8, each either0 or3. Texture mappings remain
`[0,-1,-1,-1,-1,-1]`; constant mapping0 remains -1, and mappings1..3 assign
successive slots only to enabled fields. Constant count is0..3. The seven-word
owned descriptor is `[0x103, member4?2:0, member5?2:0, 1,0,0,0]`; optional
array+20 remains NULL and static layout+18 remains image RVA0x323AD0. Expanded
material+38 must own `16 * constant_count` writable bytes. Exact-image tests
cover all output orders, intermediate releases and mapping-preserved colors.
This does not admit other SOL texture/color modes or arbitrary definitions.


The later C3 textured-material correction supersedes the zero/constant-only
restriction above. For fixed base `0x80000001`, fields at bits2/6/8 may be
0 (absent),1 (authored texture) or3 (derived constant);2 remains unsupported.
Texture count is1 plus the number of fields equal1. Six texture mappings assign
successive slots to field1 in native shift order0/2/4/6/8/10; constant mappings
use only fields equal3 at shifts0/2/6/8. The descriptor's first word is0x203
when bits2..3 equal1, otherwise0x103. Words1/2 are0x103 for texture1,2 for
constant3, or0 for absent0. The remaining words stay `[1,0,0,0]`. A derived
material retains the original texture fields and mapping order exactly.
All27 reachable descriptors and all eight original texture layouts have
exact-image test coverage; arbitrary SOL families are still not admitted.


The subsequent alternate-layout extension also retains authored fields at
bits4/10 (each0 or1) and bit23. Bit23 selects static layout RVA0x323B40; when
clear the pointer remains RVA0x323AD0. Descriptor word3 is0x103 for field10
texture1, otherwise1; words4..6 are0x103 for field4 texture1, otherwise0.
With field4 texture1, optional array+20 is a separately owned seven-word copy
of the first descriptor with words2/3 set to1 and words4..6 zero. Without it,
+20 must be NULL. The optional array participates in same-definition and
cross-definition alias checks and native release. These retained fields raise
the tested family to64 original layouts and216 reachable definitions; they
are never changed by color expansion.


Final inventory follow-up: all seven Tal originals use four flag keys:
`80000001`, `80000101`, `80800501`, `80802505`. The last also retains bit13;
its member4 expansion was the remaining live refusal. Native factory202A30
preserves that bit in the cache key without changing descriptor arrays.
The proof now retains it exactly, including a negative test for substituting
it. The exact-image fixture covers128 original layouts,432 reachable
definitions and all six output orders, including every observed Tal key.
All tracked allocations are released; the same eleven named byte guards
remain unchanged. This supersedes the insufficient216-definition candidate.

### Native trail renderer timing (2026-10-10, story C3)

CONFIRMED_STATIC / CONFIRMED_EXACT_IMAGE on executable SHA256
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`:
the trail renderer primary vtable is RVA `2DFB14`, with Update at `225760`.
Its retained resource at `+14` has primary vtable `2DFAF0`; resource `+14`
points to the authored definition, whose `+4` identifies the child slot.
The renderer's `+68` is its step interval, `+6C` its accumulated delta and
`+70` the prior accumulator. Its native update consumes accumulated time
and then adds the incoming delta; copied body animation times do not advance
this history. Tal's observed body renderer owns this trail as child 1.

These offsets alone grant no access. The story adapter requires the current
world/roster, unique entity registry membership, exact actor-model-renderer
chain, native frame and thread, unique renderer child slot, and positive
typed resource retention before replacing an incoming zero delta. See
`StoryTrailOwnerImageTest`, `StoryTrailClockImageTest` and the corresponding
engine-functions entry. Fixture acceptance is not live trail appearance proof.
