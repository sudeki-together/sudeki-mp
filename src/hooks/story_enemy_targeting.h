#ifndef SUDEKIMP_STORY_ENEMY_TARGETING_H
#define SUDEKIMP_STORY_ENEMY_TARGETING_H
#include "hooks/lan_story_observer.h"
#include "network/story_threat.h"
#include "network/lan_party_session.h"
/* Saved-story host, bound game thread only. Tier0 retains cap/Lead-PC/Defend;
 * cumulative tier1/15/2 add decaying threat, human-shooter alerts and idle pushes.
 * Live/shadow designation sets are separate; watch never rechecks or writes out.
 * Memory/table entries contain serials, never retained native pointers. Ranged
 * capability uses +46 bit 0 only; no state-vtable scan or unleased clustering.
 * Only modes 0/B substitute after the native filter; E is diagnostic only.
 * Decisions 10/11: human-only higher picks, one active alert per shooter/set.
 * Live acceptance is owner-gated in order; no tier3 or audible bark. */
void SudekiMpStoryEnemyTargetingConfigure(const wchar_t *ini);
BOOL SudekiMpStoryEnemyTargetingImageExact(uint8_t *image);
BOOL SudekiMpStoryEnemyTargetingInstall(HMODULE image);
BOOL SudekiMpStoryEnemyTargetingBindThread(DWORD verified_thread);
void SudekiMpStoryEnemyTargetingSuspend(void);
void SudekiMpStoryEnemyTargetingService(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryScene *scene,const SudekiMpStoryThreatSeat seats[4],
    const unsigned firing[4],const SudekiMpLanPartyLease away_connections[4],BOOL coop,uint32_t now);
BOOL SudekiMpStoryEnemyTargetingUninstall(void);
#endif
