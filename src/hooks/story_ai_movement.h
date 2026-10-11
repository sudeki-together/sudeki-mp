#ifndef SUDEKIMP_STORY_AI_MOVEMENT_H
#define SUDEKIMP_STORY_AI_MOVEMENT_H
#include <windows.h>
#include <stdint.h>

/* #54 C3, saved-story host only. The callback must freshly prove the active
 * actor/input/session/scene lease. Called only on the bound native thread. */
typedef BOOL (*SudekiMpStoryAiMovementLease)(void *actor,unsigned gate);
enum { STORY_AI_MOVE_GATE=0xf4cb3u, STORY_AI_SNAP_GATE=0xf50ffu };
BOOL SudekiMpStoryAiMovementImageExact(uint8_t *image);
BOOL SudekiMpStoryAiMovementInstall(HMODULE image,SudekiMpStoryAiMovementLease lease);
BOOL SudekiMpStoryAiMovementExact(void);
BOOL SudekiMpStoryAiMovementBindThread(void);
BOOL SudekiMpStoryAiMovementUninstall(void);
#endif
