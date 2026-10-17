#ifndef SUDEKIMP_LAN_STORY_NPC_GESTURE_H
#define SUDEKIMP_LAN_STORY_NPC_GESTURE_H
#include <windows.h>
#include <stdint.h>
/* H1b Kamo talk gestures on this client's own game (owner 2026-10-17;
 * default off: [StoryAreas] KamoGestures=true). During the player's own
 * shopkeeper conversation the lines are animated as natively (CSpeech
 * animate=1: the speaker's TSA lock and BODY* gestures from the sentence's
 * cues), and the shopkeeper's native CNewGameModelAnimation update (vtable
 * 0x2C8504 slot +0x40 = RVA 0xE1930) is run each frame while a gesture lock is
 * held, because the paused client world never runs it. Before every update
 * the renderer's pending animation events are popped and discarded (its
 * event drain would play sounds and post script/camera events). Only for a
 * client-only shopkeeper of this client's own shop visit (never posed by a
 * host frame). Client, native thread, inside the presentation step. */
void SudekiMpLanStoryNpcGestureEnable(BOOL enabled);
/* Exact-image guards; FALSE leaves the feature off (not an install failure). */
BOOL SudekiMpLanStoryNpcGestureInstall(HMODULE image);
/* Refused (FALSE, ERROR_BUSY) until Idle. */
BOOL SudekiMpLanStoryNpcGestureUninstall(void);
/* A conversation with `npc` starts: TRUE when its lines may be animated
 * (guards hold; the gesture bank is requested if it is not resident). */
BOOL SudekiMpLanStoryNpcGestureBegin(const void *npc,uint32_t now);
/* The next line may be animated now (bank resident, nothing pending). */
BOOL SudekiMpLanStoryNpcGestureReady(void);
/* Every presentation step while a conversation or its end is running. */
void SudekiMpLanStoryNpcGestureTick(uint32_t now);
/* A line waits on the speaker's lock that does not come (bounded): the
 * caller stops that line and animates no further line. */
BOOL SudekiMpLanStoryNpcGestureStalled(const void *speech,uint32_t now);
/* The conversation is over: TRUE once the lock is released and the bank
 * dumped (bounded; then the feature stops touching the NPC). */
BOOL SudekiMpLanStoryNpcGestureEnd(uint32_t now);
/* Stop at once (teardown, NPC gone or no longer local): never touches it again. */
void SudekiMpLanStoryNpcGestureDrain(const char *why);
BOOL SudekiMpLanStoryNpcGestureIdle(void);
#endif
