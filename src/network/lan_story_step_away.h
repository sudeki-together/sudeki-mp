#ifndef SUDEKIMP_LAN_STORY_STEP_AWAY_H
#define SUDEKIMP_LAN_STORY_STEP_AWAY_H
#include <stdint.h>
#include <stddef.h>
#include "engine/party_ownership.h"

/* Pure policy. Times are caller-supplied local receipt/observation times. */
typedef enum SudekiMpStoryCombat { SUDEKIMP_STORY_COMBAT_UNKNOWN,
    SUDEKIMP_STORY_COMBAT_NO, SUDEKIMP_STORY_COMBAT_YES } SudekiMpStoryCombat;
typedef struct SudekiMpStoryCombatSample {
    uint32_t receipt, epoch, revision;
    uint8_t valid, combat_mode;
} SudekiMpStoryCombatSample;
SudekiMpStoryCombat SudekiMpStoryCombatJudge(const SudekiMpStoryCombatSample *,
    uint32_t now, int scene_ready, uint32_t epoch, uint32_t revision);

typedef enum SudekiMpStoryAwayPhase { SUDEKIMP_STORY_AWAY_NONE,
    SUDEKIMP_STORY_AWAY_REQUESTING, SUDEKIMP_STORY_AWAY_HANDING_OVER,
    SUDEKIMP_STORY_AWAY_AWAY, SUDEKIMP_STORY_AWAY_RETURNING } SudekiMpStoryAwayPhase;
typedef enum SudekiMpStoryMenuDecision { SUDEKIMP_STORY_M_IGNORE,
    SUDEKIMP_STORY_M_RETURN, SUDEKIMP_STORY_M_CLOSE, SUDEKIMP_STORY_M_STEP_AWAY,
    SUDEKIMP_STORY_M_OPEN, SUDEKIMP_STORY_M_WAIT } SudekiMpStoryMenuDecision;
typedef struct SudekiMpStoryMenuInput {
    SudekiMpStoryAwayPhase phase;
    SudekiMpStoryCombat combat;
    uint32_t last_ui_at;
    int ui_seen, party_menu, main_menu, owned, admitted, quick_menu, shop, examine, cinematic;
} SudekiMpStoryMenuInput;
SudekiMpStoryMenuDecision SudekiMpStoryAwayMenuDecision(const SudekiMpStoryMenuInput *,uint32_t now);

typedef struct SudekiMpStoryAwayClient {
    SudekiMpStoryAwayPhase phase;
    uint32_t return_at;
    uint8_t want_away;
} SudekiMpStoryAwayClient;
void SudekiMpStoryAwayIntent(SudekiMpStoryAwayClient *,int away,uint32_t now);
/* Returns one on the 5 s fallback. drives must reflect actual local input
 * admission, not merely a stale HUMAN presence. A disconnect resets intent. */
int SudekiMpStoryAwayObserve(SudekiMpStoryAwayClient *,int connected,int away,
    SudekiMpPartyControlPhase control,int drives,uint32_t now);
void SudekiMpStoryAwayRefused(SudekiMpStoryAwayClient *,int confirmed_away);
void SudekiMpStoryAwayBanner(SudekiMpStoryAwayPhase,unsigned character,char *,size_t);

typedef struct SudekiMpStoryAwayAdmission {
    int same_area, swap_involves, prepare;
} SudekiMpStoryAwayAdmission;
SudekiMpPartySwapResult SudekiMpStoryAwayAdmit(const SudekiMpPartyOwnership *,unsigned player,
    int away,const SudekiMpStoryAwayAdmission *);
#endif
