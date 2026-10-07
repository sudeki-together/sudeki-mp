# Armour choice on the Main Menu (`[Menus] ArmourChoice`)

Status: `IMPLEMENTED` / `EXPERIMENTAL`, single player. The list, equip,
in-menu preview refresh, armour icon and runes are `CONFIRMED_LIVE` (owner,
2026-10-07). LAN play is not handled yet: equipment is game state, and the
host must own changes.

## What it does

Retail Sudeki equips armour through the story, and the Armour page only shows
what is worn. With `ArmourChoice` (default on) the Main Menu Armor icon opens
a Weapons-style page instead:

- a list of the armour this character owns and can wear, with the worn one
  highlighted;
- Enter equips the selected armour, using the game's own equip routine;
- the details panel shows the name, description, the armour's own icon and
  its rune Enchantments; F3 More Info works as on the Weapons page;
- the 3D character in the menu updates at once.

Each armour keeps its own runes. Gameplay reads the rune effects of the armour
worn at that moment, so protections follow the swap immediately.

```ini
[Menus]
ArmourChoice=true
; Research only: writes each static label's widget offset as its text.
ArmourChoiceLabelProbe=false
```

## Native model (`CONFIRMED_STATIC`, supported image)

- Equipment: `character+0xE0` is `CEquipped`; `+0x18` is the worn
  `CItemArmour*`, `+0x1C` its item ID ("Armour Item ID").
- Item classes, by item type ID: weapons 3–17, armour 18–32 (hero armour
  19–22, Dark 23–26, Merged 27–30), quest 33, trade 35–38, money 39.
- Who can wear what (`CItemArmour::CanUse`, `0x530080`): character type 0x23
  (Tal), 1 (Ailish), 5 (Buki), 0xE (Elco) wear 19/20/21/22. The shadow
  counterparts (character types 8–11, for example Alexine) wear the Dark
  armour, and merged forms (character types 0x13–0x16) the Merged armour. The
  adapter applies these pairs itself, because `0x530080` takes a character
  weak reference rather than the object.
- Inventory: global `0x808D84`. Categories are keyed by item type (count
  `+0x12C`, array `+0xC`); each category has `+0x8` type, `+0xE` last index and
  `+0x4` entries of `{int16 item ID, …}`.
- Native equip `0x560580` (`EAX` = armour, stack = `CEquipped`, `RET 4`):
  - copies the outgoing armour's runes into free compatible sockets of the
    incoming one without clearing them;
  - sets `+0x18`/`+0x1C`;
  - swaps the body skin (owner `+0x12C`, model vtable `+0x64`, "Skin Model
    Name" `item+0xEC`).

  The adapter clears `+0x18` first, so that copy is skipped and runes are
  never duplicated.
- Runes: an armour slot holds a built-in rune (slot `+0x8` ≠ -1) or a socket
  stored per armour ID at `inventory+0x10+id*3` (IDs 100–139 at `+0xB2`).
  Gameplay sums rune effects live from `character+0x10 → +0xE0 → +0x18`
  (`0x5308C0`, resistances `0x5309B0`; callers `0x4A6060`, `0x4AAC90`,
  `0x4BF360`, `0x4D3BA0`, `0x4D95C0`–`0x4D96B0`, `0x4BDA40`). Nothing is
  cached at equip time.
- Menu preview: `UIModelManager` at `ui+0x178` (`ui = [0x7C2F78]`).
  `0x560F90` (stack: manager plus a 12-byte party weak reference, `RET 0x10`)
  loads the body, the worn armour's skin and the weapon. After it, copy
  `ui+0x230` to `manager+0xB` and call `0x561B50(manager)`, as the Main Menu
  character switch does at `0x496300`.

## Seams (`src/hooks/armour_menu.c`)

| Seam | Kind | Role |
| --- | --- | --- |
| `0x493CFC` | Main Menu dispatch jump table [2] (Armor icon) | Builds `UILayerWeaponsMenu` (`0x56DBD0`) instead of `UILayerArmourMenu` and records it as the armour layer; title "ARMOR", hint "View and equip armor from the list." |
| `0x6D8908` | `UILayerWeaponsMenu` destructor vtable slot | Forgets the armour layer |
| `0x56FC60` | inline, list fill | Armour layer: owned wearable armour via the native list API (`0x57D720` clear, `0x57D7A0` add, `0x57D3C0` select) |
| `0x5706D0` | inline, equip | Armour layer: wearability check, native equip, preview refresh, refill |
| `0x56F5B0` | inline, details | Native panel, then the armour's own icon (`0x55C0E0` with `item+0x78`) and the weapon-only stats blanked |

Only the layer created from the Armor icon is affected; the Weapons page is
unchanged.

The Weapons page has two weapon-only parts:

- Its icon comes from a per-ID weapon table (171 + item ID), which shows
  unrelated weapon icons for armour IDs.
- Its section label group shares one text ID across four entries created in
  layout order: Enchantments, Lethality, Strength, Critical (`CONFIRMED_LIVE`).
  The adapter keeps the first and blanks the rest, which works in any language.

The title and hint words are English constants (`UNKNOWN`: localisation).

## Evidence

- `CONFIRMED_LIVE` (2026-10-07, owner, single player, New Brightwater save,
  branch `codex/texture-mods`):
  - Tal's list showed Haskilian Plate, Shadow Armor and Knightly Armor, with
    runes shown;
  - equips switch the worn armour in the world and, after the preview fix, in
    the menu at once;
  - the label order was logged.
- Fixed during the bring-up:
  - the native string object is about 60 bytes (an 8-byte buffer overflowed);
  - `CanUse` expects a weak reference;
  - Dark armour belongs to the shadow characters, not the heroes.
- Not yet exercised: merged forms, shadow characters, LAN sessions,
  localisations other than English, and controller-only navigation.
