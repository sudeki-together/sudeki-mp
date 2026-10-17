#ifndef SUDEKIMP_LAN_STORY_SHOP_H
#define SUDEKIMP_LAN_STORY_SHOP_H
#include <windows.h>
#include <stdint.h>
#include "hooks/lan_story_observer.h"
/* H1b client shop (owner decisions 2026-10-17, plan D11): a player inside a
 * table shop uses its shopkeeper on their OWN game. First stage: the native
 * talk prompt near Kamo and, on the interact press, Kamo's native shop screen
 * on this game, buying and selling with this game's own purse (nothing is
 * sent to the host). Kamo's greeting conversation comes in a later stage.
 *
 * Native pieces (CONFIRMED_STATIC, supported image): the talk prompt is
 * CUIScene context flag 2 (NPC_Kamo_NBvolume__OnEntry -> SetContextIconTalk);
 * the stock comes from the player's verified SOLWORLDM.gex (sol_shop_stock);
 * the shop exports ShopClearInventory 0x17C50, ShopAddItem 0x17C70/0x17C90,
 * ShopSetMode 0x17CD0, ShopSetName(int) 0x8D160, ShopStart 0x8D1A0; the HUD
 * (*(0x3C2F88)) goes 1 -> 7/8 while the shop layer (*(0x3C2F70)) is shown and
 * ShowIngameMenus 0xB880 returns it to 1. The shop's Show calls 0x40AFD0
 * (controller listener pause + front movement) at 0x489E70; on the client
 * that call is skipped (one owned relative-call hook), as the Main Menu's
 * are (#75), so the controller dispatch the mod presents from keeps running.
 * With it skipped, back (cmd 1) and buy/sell (cmd 10) do not pass the
 * CUIScene command gate, so they are polled here and delivered to the shop
 * layer's own input handler (0x4898A0). Client only, native thread only. */
BOOL SudekiMpLanStoryShopInstall(HMODULE image);
/* Refused (FALSE, ERROR_BUSY) while a shop is open or closing. */
BOOL SudekiMpLanStoryShopUninstall(void);
/* Client, inside a ClientPresent step once per frame. `actor` is this
 * player's own character; `in_shop` = the shop zone name this client is
 * settled in (NULL when none); `blocked` = another modal or a host
 * presentation owns the screen. Shows/hides the talk prompt and runs the
 * open/close lease. */
void SudekiMpLanStoryShopService(void *actor,const char *in_shop,BOOL blocked,uint32_t now);
/* TRUE while the talk prompt is shown: the weak-attack press is this
 * module's (the caller must not send it to the host). */
BOOL SudekiMpLanStoryShopPromptShown(void);
/* The player pressed interact with the prompt shown. */
void SudekiMpLanStoryShopRequestOpen(void);
/* TRUE from the accepted request until the HUD is positively back in game
 * state after the shop closed (movement muted, melee withheld, UI tasks
 * tolerated, the session menu key ignored). */
BOOL SudekiMpLanStoryShopActive(void);
/* Runtime teardown, native thread, repeatedly until TRUE: an unopened
 * request ends, an open shop leaves through its own back command and the
 * HUD returns to game state. TRUE once the lease is idle. */
BOOL SudekiMpLanStoryShopDrain(uint32_t now);
/* No lease (nothing for teardown to drain). */
BOOL SudekiMpLanStoryShopIdle(void);
/* TRUE while this player's own Kamo conversation lets the native speech
 * camera be selected (or is returning from it): the client camera gives
 * way and must not re-seat until it is the selected camera again. */
BOOL SudekiMpLanStoryShopSpeechCameraHeld(void);
/* The player's interact press while this game's own Kamo conversation is
 * up (the runtime never sends it to the host); it closes the farewell. */
void SudekiMpLanStoryShopInteractPress(void);
/* The area is changing under an open shop: leave it through its own back
 * command (rate-limited) until it is observed closed. */
void SudekiMpLanStoryShopRequestClose(const char *why);
/* Each dispatch while active and this game is the foreground window:
 * Escape/M = back, F3 = buy/sell, delivered only to the focused shop layer. */
void SudekiMpLanStoryShopPollKeys(void);
#endif
