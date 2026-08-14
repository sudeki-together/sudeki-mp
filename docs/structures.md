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

Movement-specific controller fields:

| Offset | Type | Provisional name | Evidence | Confidence |
| ---: | --- | --- | --- | --- |
| `+0x1A0`, `+0x1A4` | `float` | local movement axes | RVA `0x00028B00` reads them, preprocesses them, and camera-transforms the result | High |
| `+0x1BC` | `float` | one-shot/direct movement input | Replaces the first local axis when nonzero, then is cleared by the movement consumer | Medium-high |
| `+0x1D4` | `float` | direct movement speed | Exported setter/getter at RVAs `0x00029350/0x00029360`; used only in movement mode `1` | High |
| `+0x1F0` | 4x4 matrix | stored movement camera transform | RVA `0x000291A0` removes translation and transforms the local movement vector | High |
| `+0x230..+0x238` | `vec3` | last submitted world movement | Movement consumer stores its final normalized vector here | High |
| `+0x23C` | `int32` | movement mode | Exported setter/getter at RVAs `0x00029330/0x00029340`; mode `0` uses the arbiter path and mode `1` uses absolute delta movement | High |

### `character+0x94` component candidate

Every observed party character held a non-null pointer at `character+0x94`; pointer presence did not change during switching. Shared post-rotation reassignment passes the old and new values to RVA `0x000EF700`.

| Offset | Type | Provisional name | Evidence | Confidence |
| ---: | --- | --- | --- | --- |
| `+0x10` | pointer | owning character | Transition logic dereferences character components through it | Medium-high |
| `+0x3C` | pointer | nested mode state | The transition reads/writes its byte `+0x0B` | High for layout |
| nested `+0x0B` | `uint8` | AI-active/control mode | Live Previous/Next switches showed `0` only for the front/controller target and `1` for every AI party member; ownership transfer flipped the old `0→1` and new `1→0` | High |
| `+0x44` | flags | behavior flags | Selects internal behavior state during transition | Medium |
| `+0x16A` | `int16` | AI control-override refcount | Exported `AiIsOverriden`, `AiOverrideControl`, and `AiDefaultControl` query and modify this count; first acquire disables AI and final release restores it for a non-front character | High |

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
