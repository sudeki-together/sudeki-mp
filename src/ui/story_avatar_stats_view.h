#ifndef SUDEKIMP_STORY_AVATAR_STATS_VIEW_H
#define SUDEKIMP_STORY_AVATAR_STATS_VIEW_H
#include "ui/title_menu_view.h"

enum { SUDEKIMP_AVATAR_STATS_PLAYERS=4, SUDEKIMP_AVATAR_STATS_FRESH_MS=250 };
/* Copy-only render model. Native CCharacterStats resources are floats.
 * Rows are indexed by player, never by a native party/HUD slot. Runtime owns
 * roster/generation/authentication admission; received_tick is local receipt
 * time, not the host's observed_tick. The view retains no snapshot or actor. */
typedef struct SudekiMpStoryAvatarStatsRow {
    BOOL present;
    uint32_t epoch,revision,spawn_generation,sequence,received_tick;
    char name[32];
    float hp,max_hp,sp,max_sp;
} SudekiMpStoryAvatarStatsRow;
typedef struct SudekiMpStoryAvatarStatsSnapshot {
    uint32_t epoch,revision;
    unsigned local_player;
    SudekiMpStoryAvatarStatsRow rows[SUDEKIMP_AVATAR_STATS_PLAYERS];
} SudekiMpStoryAvatarStatsSnapshot;
/* Pure layout: stale, absent, wrong-scene and malformed rows are omitted.
 * Present rows use a Talos name tile: the title portrait resource lease does
 * not authorize borrowing those textures during gameplay. No native HUD slot
 * is selected or changed. Empty snapshots generate no draw calls. */
BOOL SudekiMpStoryAvatarStatsViewBuild(const SudekiMpStoryAvatarStatsSnapshot *,
    uint32_t now,SudekiMpTitleExtras *out);
BOOL SudekiMpStoryAvatarStatsViewRender(void *device,
    const SudekiMpStoryAvatarStatsSnapshot *,uint32_t now);
/* Optional gameplay-owned textures indexed by player. Caller proves their
 * current native/world/device lease and retains them across this call only.
 * These pointers never enter the snapshot or a UI cache. NULL entries use
 * the name tile; the title portrait lease is not a gameplay lease. */
BOOL SudekiMpStoryAvatarStatsViewRenderPortraits(void *device,
    const SudekiMpStoryAvatarStatsSnapshot *,uint32_t now,void *const portraits[4]);
#endif
