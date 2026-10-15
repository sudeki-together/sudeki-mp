#ifndef SUDEKIMP_LAN_STORY_MAIN_MENU_H
#define SUDEKIMP_LAN_STORY_MAIN_MENU_H
#include <windows.h>
#include <stdint.h>

/* Saved-story M menu scope (#75, owner rule 2026-10-14): every player gets the
 * native Main Menu for the characters they own only. The host owns its own
 * character and the AI companions; a client owns its character. Three exact
 * native seams, all observed live on the supported image:
 *  - the character cycle (RVA 0x96300: EAX = menu ui, stack = direction,
 *    RET 4) called from the menu input handler at RVA 0x93B24 (next) and
 *    0x93B47 (previous): the wrapper pre-positions ui+0x170 so the native
 *    +1/-1 lands on the next slot the player may edit;
 *  - the menu open (vtable slot RVA 0x2CAE4C -> RVA 0x956D0): after the
 *    native open the selection moves to the player's own slot;
 *  - the Weapons layer equip call (RVA 0x16F1A4 -> RVA 0x1706D0, layer on
 *    the stack, RET 4): a client turns it into a host request instead of
 *    equipping its paused replica locally.
 * Nothing here reads or writes inventory; the host applies requests natively. */

/* May the local player land on / edit this party actor? NULL = everything. */
typedef BOOL (*SudekiMpStoryMenuSlotAllowed)(void *actor);
enum {
    SUDEKIMP_STORY_MENU_EQUIP_NATIVE = 0, /* host: let the native equip run */
    SUDEKIMP_STORY_MENU_EQUIP_QUEUED,     /* client: request queued, skip native */
    SUDEKIMP_STORY_MENU_EQUIP_REFUSED     /* client: not queued, still skip native */
};
typedef unsigned (*SudekiMpStoryMenuEquipRequest)(void *actor,unsigned item);

/* Items page "use" (RVA 0x17761D -> 0x179A50, layer on the stack, RET 4): on
 * the client the selected consumable becomes a USE_ITEM request for the
 * player's own character (D10: no target choice); the native apply never runs
 * on the paused replica. Same NATIVE/QUEUED/REFUSED contract as equip. */
BOOL SudekiMpLanStoryMainMenuInstall(HMODULE image,SudekiMpStoryMenuSlotAllowed allowed,
    SudekiMpStoryMenuEquipRequest request,SudekiMpStoryMenuEquipRequest use_request);
BOOL SudekiMpLanStoryMainMenuUninstall(void);
/* Client: ask the gameplay HUD (`*(0x3C2F88)`) for its own "open main menu"
 * command (0xF) through its input handler, exactly what the native M key does
 * after the controller; every native precondition (HUD state, no modal, no
 * cutscene) still applies. TRUE = the command was delivered, not that the menu
 * is open. */
BOOL SudekiMpLanStoryMainMenuRequestOpen(void);
/* Client: deliver the Main Menu's own "back" command (input handler vtable
 * +0x2C, kind 5 / command 1 / value 0, RET 0xC) so the shown menu leaves
 * through its native state 6 path. Natively this arrives through the
 * controller listener the client swallows. TRUE = delivered. */
BOOL SudekiMpLanStoryMainMenuRequestClose(void);
/* Client, each dispatch while the menu is shown and this game is the
 * foreground window: in menu mode the controller listener receives no key
 * events (CONFIRMED_LIVE 2026-10-15), so M/Escape (back) and F3 (weapon info)
 * are read here and delivered as the matching native menu commands. */
void SudekiMpLanStoryMainMenuPollKeys(void);
/* Client confirmation prompt (owner, 2026-10-15): the Items list confirm
 * (RVA 0x177166, the switch into the native party-target view) is replaced by
 * a yes/no prompt for the player's own character. TRUE while shown; *choice
 * 0 = Yes, 1 = No. The runtime draws it and reports the first draw result. */
BOOL SudekiMpLanStoryMainMenuPrompt(unsigned *choice);
void SudekiMpLanStoryMainMenuPromptRendered(BOOL ok,DWORD error);
/* Client, after an accepted item use while the Items page is the active UI:
 * rebuild the Items list for its current category (RVA 0x178820, one stack
 * arg, RET 4 — what the native page runs after a category switch) and refresh
 * the party panel (`*(ui+0x218)`, RVA 0x15B570, EAX = its current cursor,
 * one stack arg, RET 4). TRUE when both ran. */
BOOL SudekiMpLanStoryMainMenuRefreshItems(void);
/* The Main Menu object reports itself shown (+0x29). */
BOOL SudekiMpLanStoryMainMenuVisible(void);
/* Natural-language counters for the research log. */
void SudekiMpLanStoryMainMenuCounters(unsigned *redirected,unsigned *requests);
#endif
