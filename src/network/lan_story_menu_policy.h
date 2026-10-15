#ifndef SUDEKIMP_LAN_STORY_MENU_POLICY_H
#define SUDEKIMP_LAN_STORY_MENU_POLICY_H
#include <windows.h>
/* Pure M-menu character policy (#75): the native Main Menu cycles its
 * selected party slot (ui+0x170) with +1/-1 modulo the group count; a player
 * may only land on slots they own (own character; the host also owns AI
 * companions). Returns the slot to land on from `current` in `direction`
 * (TRUE = forward) given a bitmask of allowed slots; `current` when the mask
 * admits nothing else. count is 1..4. */
unsigned SudekiMpStoryMenuNextSlot(unsigned count,unsigned current,BOOL forward,unsigned allowed_mask);
/* The slot the menu should start on: the first allowed slot, else `fallback`. */
unsigned SudekiMpStoryMenuInitialSlot(unsigned count,unsigned allowed_mask,unsigned fallback);
#endif
