#ifndef SUDEKIMP_STORY_PLAYER_TURN_H
#define SUDEKIMP_STORY_PLAYER_TURN_H
#include <windows.h>
#include <stdint.h>

/* #54 C2, saved-story host only. The owner must freshly prove its retained
 * native actor/input/session/scene lease on the game thread. This adapter
 * changes two branch decisions, never the AI reference count or mode. */
typedef BOOL (*SudekiMpStoryPlayerTurnLease)(void *actor);
enum { STORY_TURN_BRANCH=0xb742eu, STORY_CAN_TURN_BRANCH=0xdbb3cu };
BOOL SudekiMpStoryPlayerTurnImageExact(uint8_t *image);
BOOL SudekiMpStoryPlayerTurnInstall(HMODULE image,SudekiMpStoryPlayerTurnLease lease);
BOOL SudekiMpStoryPlayerTurnExact(void);
/* Called only after the owner proves its post-controller native boundary. */
BOOL SudekiMpStoryPlayerTurnBindThread(void);
/* Only the mod's witnessed native movement call gets player CanTurn rules.
 * Native AI/controller/other callers retain their original decisions. */
BOOL SudekiMpStoryPlayerTurnMoveBegin(void *actor);
void SudekiMpStoryPlayerTurnMoveEnd(void);
BOOL SudekiMpStoryPlayerTurnUninstall(void);
#endif
