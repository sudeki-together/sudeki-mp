#ifndef SUDEKIMP_LAN_STORY_AVATAR_PARTY_H
#define SUDEKIMP_LAN_STORY_AVATAR_PARTY_H

#include "hooks/lan_story_observer.h"

/* Dev Play only. These APIs do not change a save or substitute a hero's model.
 * The selected actor must be an independently completed ALLY_TALOS spawn.
 * Composition must drain other actor/control/camera leases before Begin. */
typedef enum SudekiMpLanStoryAvatarPartyPhase {
    SUDEKIMP_AVATAR_PARTY_OFF = 0,
    SUDEKIMP_AVATAR_PARTY_PREPARING,
    SUDEKIMP_AVATAR_PARTY_LEAD_PENDING,
    SUDEKIMP_AVATAR_PARTY_DELETE_PENDING,
    SUDEKIMP_AVATAR_PARTY_READY,
    SUDEKIMP_AVATAR_PARTY_UNKNOWN,
    /* Native FilterNone requests +84; the original controller update later
     * commits +80. Keep existing phase values and observation layout stable. */
    SUDEKIMP_AVATAR_PARTY_FILTER_PENDING
} SudekiMpLanStoryAvatarPartyPhase;

typedef struct SudekiMpLanStoryAvatarPartyObservation {
    SudekiMpLanStoryAvatarPartyPhase phase;
    uint32_t epoch, spawn_generation, membership_revision;
    uint8_t local_player, hero_mask, leader_character, reserved;
    void *world, *group, *controller, *native_leader;
    void *heroes[4];
    /* Actual native group order; count includes the actual local Talos.
     * leader_character is 0..3 for a hero, 4 for the local Talos. */
    void *members[4];
    unsigned member_count;
} SudekiMpLanStoryAvatarPartyObservation;

BOOL SudekiMpLanStoryAvatarPartyImageMatches(HMODULE image);
BOOL SudekiMpLanStoryAvatarPartyInstall(HMODULE image);
BOOL SudekiMpLanStoryAvatarPartyBegin(
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanStoryNativeRoster *roster,
    unsigned local_player, uint32_t spawn_generation,
    uint8_t selected_hero_mask);
/* Invoked before ObserverSample in the same native controller callback.
 * Does not call the observer and never repeats an uncertain native request. */
BOOL SudekiMpLanStoryAvatarPartyService(
    const SudekiMpControlUpdateDispatchWitness *witness, void *controller);
/* Fresh native identity only; no recursive Observer validation. Returns FALSE
 * on an unexpected membership/world/actor change. A pending phase is not
 * gameplay admission. The epoch stays the caller's original scene epoch. */
BOOL SudekiMpLanStoryAvatarPartyObserve(
    SudekiMpLanStoryAvatarPartyObservation *out);
BOOL SudekiMpLanStoryAvatarPartyRetains(void);
/* Refuses while a changed native world remains live. After verified native
 * Quit, call TaskTraceForgetExitedWorld, then this, then SpawnShutdown. The
 * spawn owner's positive exit receipt retires this plain-data lease without
 * a second EntitySetup observer. Never delete the last actor to unload. */
BOOL SudekiMpLanStoryAvatarPartyUninstall(void);

#endif
