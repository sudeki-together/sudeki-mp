#ifndef SUDEKIMP_LAN_STORY_REMOTE_USE_H
#define SUDEKIMP_LAN_STORY_REMOTE_USE_H
#include <windows.h>
#include <stdint.h>
/* H1a: a remote player opens a chest with the native treasure path, as that
 * player (owner decisions 2026-10-16: the host keeps full control; only the
 * opener is held).
 *
 * The native chest path always acts on "the front", the handle at
 * party_group+0x90 (CONFIRMED_STATIC): the usable listener 0xB5C30 compares
 * the event's actor with it, 0xB5CE0 checks its arbiter, the treasure use
 * export (vtable slot RVA 0x2D51A0 -> 0x184EC0) checks its position/facing,
 * sets its arbiter bit 0x200 and switches the controller off; the treasure
 * state machine (node vtable slot RVA 0x2D51C8 -> 0x1849D0) turns and
 * animates it and switches the controller back on.
 *
 * This adapter owns, for the remote opener only:
 *  - relative-call hooks on the 16 front-handle copies (CALL 0x15B0 after
 *    `mov ecx,[0x408D94]; add ecx,0x90`) in the usable/facing/turn/animation
 *    and treasure code: inside an active scope, on the scope's thread, the
 *    copy is redirected to the opener's own linked party-slot handle;
 *  - relative-call hooks on the three controller switches (CALL 0x295B0 at
 *    0x184F7E, 0x184B69, 0x184CA0): skipped inside a scope;
 *  - a pointer hook on the treasure node's update slot: the scope is active
 *    while a chest opened by a remote player updates.
 * Outside a scope every hook passes through unchanged. Game thread only. */
BOOL SudekiMpLanStoryRemoteUseInstall(HMODULE image);
BOOL SudekiMpLanStoryRemoteUseUninstall(void);
/* Host, game thread: the remote player's weak-attack press. When an
 * unopened, visible chest is within reach of `actor`, run the native usable
 * listener for it with `actor` as the front. TRUE only when the treasure
 * state left idle (the open started); FALSE leaves the press to melee. */
BOOL SudekiMpLanStoryRemoteUseTry(void *actor);
/* Host, game thread: 0 none; 1 or 2 when `actor` is in reach of and facing an
 * unopened, visible chest: the native prompt flag 4 (1) or 0x200 (2). */
unsigned SudekiMpLanStoryRemoteUseHint(const void *actor);
/* Client: verify the UI scene flag routines; then show/hide the native
 * interact icon (flag 0x200) from the host's use hint, game thread only. */
BOOL SudekiMpLanStoryRemoteUseIconInitialize(HMODULE image);
void SudekiMpLanStoryRemoteUseIconApply(unsigned hint);
/* Client: this game's own prompt bits (flag 2, a shopkeeper talk prompt),
 * combined with the host's chest hint; re-assert every frame while wanted. */
BOOL SudekiMpLanStoryRemoteUseIconSetLocal(uint32_t bits); /* FALSE: no icon owner */
/* Host, game thread, once per frame: step remote opens whose chest the
 * native update skipped (paused area), including the timeout release. */
void SudekiMpLanStoryRemoteUseService(void);
/* TRUE while a chest open started by `actor` is still running. */
BOOL SudekiMpLanStoryRemoteUseActive(const void *actor);
#endif
