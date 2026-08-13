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
