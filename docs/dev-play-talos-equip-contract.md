# Talos damage and native equip checkpoint

Supported executable SHA256:
`8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94`.
Reviewed 2026-10-07. This records bounded native contracts and observations;
it does not enable an equip experiment or claim accepted damage.

## Damage inputs

**CONFIRMED_LIVE, read-only:** two independently spawned `ALLY_TALOS` actors
in each process of the current pair had the following values. Catalogue
membership, character class, component class and owner backlinks were checked.
The catalogue/world tuple was reread after each process snapshot. No game input
or memory write was performed by the sampler.

| Input | Each Talos | Tal in the same world |
| --- | --- | --- |
| Weapon current item `+0x268` | Null | Database item 7 |
| Current item ID `+0x264` | -1 | 7 |
| Pending item `+0x26C` | Null | Null |
| Weapon state `+0x330` | 3 | 3 |
| Fallback damage `+0x334/+0x338` | 0 / 0 | 0 / 0 |
| Equipped item damage `+0xF4/+0xF8` | No item | 25 / 0 |
| Stats physical base/multiplier `+0x18/+0x1C` | 0 / 1 | 10 / 1 |
| Stats special base/multiplier `+0x20/+0x24` | 0 / 1 | 5 / 10 |
| HP / maximum HP | 8000 / 8000 | 400 / 600 |
| SP / maximum SP | 999 / 999 | 0 / 20 |

**CONFIRMED_STATIC:** weapon initialization `0xD7540` starts fallback damage at
0 / 1, but the property reader `0xD7630` reads both authored damage properties
with a default of zero. `0xD8010` automatically chooses/equips an inventory item
only for its recognized hero-resource predicate; generic Ally does not qualify.
`0xD95C0` uses the item when present and otherwise the two fallback fields.
`0xD1CB0` adds the relevant stats product and subsequently applies modifiers.
The observed Talos values therefore supply zero to both base-damage branches.
This is stronger evidence than a null item alone, but an accepted native hit
and before/after target HP are still **UNKNOWN** for this candidate.

## Real item ownership

**CONFIRMED_STATIC:** the item definition is owned by `CItemManager`, not
allocated for each equipped actor. Global `0x408D80` points to the manager;
its exact primary/secondary vtables are `0x2C693C/0x2C6944`. The manager's
999-entry pointer array begins at `+0x0C`. Native inventory lookup `0x21CE0`
returns one of these definition pointers without adding an item reference.

The game-start loader `0x201E0` reads the native item-definition resource and
calls serializer `0x20000`. Definition loader `0x1FF50` obtains an object from
factory `0x1FD10`, assigns its ID/category, invokes its property reader, and
publishes it in the matching empty database slot. Weapon definitions use
constructor `0x12FA90`, size `0x12C`, primary vtable `0x2D3A28`, and secondary
vtable `0x2D3A64` at `+4`.
`0x1FA70` calls each definition's deleting destructor and clears every slot;
the witnessed caller is the game-singleton shutdown path `0x79B00`.
An equip adapter must revalidate manager/slot/ID/class and hold an appropriate
live world scope; readability does not extend a definition's lifetime.

**CONFIRMED_LIVE:** Tal's item 7 had the exact weapon-definition class, category
4, and appeared once in the manager array at index 7. Both live managers had
the exact primary and secondary classes. Native `0xD7C10` stores a borrowed
item pointer and does not allocate, clone, add-reference or destroy that item.
Its model/resource operations have separate native ownership.

The higher-level item methods have a different argument contract from the
setter. Exact disassembly of `0x12FE00` returns with 12 stack bytes removed;
`0x12FEF0` removes 16. Both receive and destroy a by-value 12-byte intrusive
entity handle through `0x15E0`. The use method's fourth word is unused by this
weapon implementation. The existing native copy helper `0x15B0` constructs a
handle at `EAX` from the source at `ECX` and registers it in the actor's handle
chain. A raw actor pointer alone is not a valid call to either item method.

## Talos setter and model compatibility

**CONFIRMED_STATIC:** `0xD7C10` is `thiscall(weapon,item)`, returning with four
stack bytes removed. It requires a different non-null item and a valid item
model ResourceName, choosing override `+0x104/+0x108/+0x10C` or base
`+0x44/+0x48/+0x4C`. The immediate branch writes current ID/pointer and creates
the native model through `0xD7880`; the deferred branch stores `+0x26C` and
enters swap state 4. The native item-use method `0x12FEF0` calls this setter
when the target has a weapon component.

An empty override (`+0x108 == 0x7FFFF`) selects the base model. If that base is
also empty (`+0x48 == 0x7FFFF`), the setter returns before writing the item
ID or pointer. Consequently an authored weapon with no selected model cannot
serve as a damage-only equip through this setter. A null string-reference
pointer is not the same as an empty ResourceName: an indexed model can have
a valid ID and no resident name string.

**CONFIRMED_STATIC, authored data:** a bounded read-only parse of the native
item database found 206 items and 37 weapons, including 10 category-4 Tal
weapons. Every weapon has a nonempty selected model; none of these authored
weapon records names Talos. All category-4 overrides are empty and select
their nonempty base models. Item 7 selects the native Mojo sword model and
has base/random damage 25 / 0. There is no authored empty-model weapon in
this database that avoids the setter's model requirement. This finding does
not establish any equip behavior or damage result.

The separate item-use availability method `0x12FE00` maps its default actor
resource family to category 4. It also requires the matching native inventory
category to contain more than one item; `0xD79A0` returns that inventory count,
not a weapon animation state. The direct setter has no such inventory-count
check. This distinction must not be hidden by claiming the higher-level use
method has admitted an actor when it has not run.

The generic availability dispatcher `0x12E1E0` adds another policy layer:
after its arbiter checks it routes known actor resource types to item virtual
methods. Tal's type `0x23` is mapped to the hero branch and calls item virtual
`+0x10`; Ally type `0x82` falls through with unsupported result 6 without
calling that method. Directly calling the weapon-specific availability method
would bypass this generic actor-type policy, even if its family/count checks
passed. It is therefore only a possible explicitly scoped experiment, not
proof that the ordinary native inventory route supports Talos.

**CONFIRMED_LIVE:** Talos shares `CCharacterWeapon` vtable `0x2D4D3C` and
`CCharacterModel` vtable `0x2C8504` with Tal, with exact owner backlinks and
weapon `+0x3AC == model + 4`. Its model instance is `0x2DF8EC`. However, all
four authored armed/sheathed weapon locator strings are empty, and both
weapon model slots are null. Tal has valid `WeaponFollow` and
`Weapon_Shoulders` locators. Talos's stats name used by the setter's texture
refresh is a valid native `Talos` string.

**CONFIRMED_STATIC:** the empty sheath-locator branch in `0xD8470` skips socket
dereferences and sets state 3. The empty armed-locator branch in `0xD80B0`
skips attachment and sets state 0. Outside the setter's temporary equip flag,
that draw path calls `SetWeaponVisible(true)`. The normal sheath visibility
predicate `0xD87D0` excludes Ally resources. Thus successfully storing a real
Tal weapon item is not proof of correct Talos presentation: a later draw can
expose an unattached weapon model. No locator, item, stats, damage or visibility
field has been changed to work around this.

The native visibility setter `0xD7E30` is `thiscall(weapon,bool)`, with four
stack bytes removed. Its false branch hides/deactivates the separate weapon
render objects; it does not access the actor's body-model component. A future
adapter would still need exact wrapper/render-object ownership and a proved
before-render boundary after native weapon updates. The branch can call an
additional model virtual method when render flags contain `0x04000000`; that
path must either be proved or explicitly refused. Continuous visibility
ownership and clean restoration remain research, not an enabled workaround.

The setter also refuses a null item, so calling it with null is not a proved
restoration operation for Talos's initial unarmed state. A bounded equip
experiment still needs an accepted-hit baseline, explicit model/presentation
ownership through combat transitions, exact native completion/readback, and
a proved cleanup path. No callable Talos equip adapter is enabled at this
checkpoint.

`tools/ghidra/DevPlayWeaponEquipReport.java` reproduces the source contracts
against the supported executable, including constructor/vtable ownership,
item methods, generic availability, model attachment, and damage inputs. Run
it read-only; generated decompilation stays private.
