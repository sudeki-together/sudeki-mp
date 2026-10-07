#ifndef SUDEKIMP_ARMOUR_MENU_H
#define SUDEKIMP_ARMOUR_MENU_H
#include <windows.h>

/* Armour choice on the in-game Main Menu ([Menus] ArmourChoice, default on).
 *
 * The retail Armour page (UILayerArmourMenu) only shows the armour the story
 * last equipped. This adapter makes the Armor icon open the Weapons page
 * layer (UILayerWeaponsMenu) in "armour mode": the list shows the selected
 * character's owned armour (its normal, Dark and Merged armour item types),
 * Enter equips the highlighted one through the game's own armour equip
 * (0x560580: moves socketed runes, sets CEquipped, swaps the skin model), and
 * F3 More Info shows the item description natively. Only the layer instance
 * created from the Armor icon is affected; the Weapons page is unchanged.
 *
 * Owned seams (exact image): Main Menu dispatch jump-table entry for icon 2
 * (0x493CFC), UILayerWeaponsMenu vtable destructor slot (0x6D8908), and the
 * Weapons layer's list fill 0x56FC60, equip 0x5706D0 and details 0x56F5B0.
 * Changes equipment, so in LAN sessions the host must own the result
 * (single player first; see docs/armour-choice.md). */
BOOL SudekiMpArmourMenuInstall(HMODULE image, const wchar_t *config_path);
BOOL SudekiMpArmourMenuUninstall(void);
#endif
