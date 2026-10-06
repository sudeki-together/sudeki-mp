#ifndef SUDEKIMP_LAN_STORY_AREA_MEMBERSHIP_H
#define SUDEKIMP_LAN_STORY_AREA_MEMBERSHIP_H

#include "hooks/lan_story_observer.h"

/* Copied diagnostics, not a wire format or an area/actor lifetime lease.
 * A present resource, collision flag or READY roster alone does NOT prove
 * that the area's collision queries, scripts or simulation are operational. */
typedef struct SudekiMpLanStoryAreaMember {
    uint32_t native_state, collision_mask;
    /* CAiTracking+40, not the separate movement component's sector. A copy
     * here alone cannot prove that native ground queries changed area. */
    uint16_t sector;
    /* Native movement+BC is independently produced by collision. Valid here
     * means packed-number range only, not area residency or terrain proof.
     * Missing movement and its native FFFF sentinel remain explicit unknowns.
     * Flags are opaque copies of +BE/+BF, never permission to clear them. */
    uint16_t movement_sector;
    uint8_t movement_present,movement_sector_valid,movement_region_matches;
    uint8_t movement_flags[2];
    uint8_t pause_refs, zone_flags, is_current;
    uint8_t data_present, data_pending, navigation_present;
    uint8_t collision_present, collision_registration, collision_enabled;
    char name[SUDEKIMP_LAN_STORY_NAME_SIZE];
} SudekiMpLanStoryAreaMember;
typedef struct SudekiMpLanStoryAreaMembership {
    uint32_t epoch, revision;
    uint8_t available_mask;
    SudekiMpLanStoryAreaMember members[4];
} SudekiMpLanStoryAreaMembership;

/* READ ONLY on the verified post-controller game-thread dispatch. Resolves
 * each sparse party member's own packed region, not the global current area.
 * Checks exact owner/backreferences and re-observes topology before publishing.
 * No native calls, hooks, references or gameplay writes. Output is unchanged
 * on failure; failure is UNKNOWN, never permission to unpause or unload.
 * Uses the existing READY roster witness: loading-phase observation remains
 * unsupported until the observer has a separate proven lifetime contract. */
BOOL SudekiMpLanStoryAreaMembershipObserve(HMODULE image,
    const SudekiMpControlUpdateDispatchWitness *,const SudekiMpLanStoryNativeRoster *,
    SudekiMpLanStoryAreaMembership *);

enum { SUDEKIMP_STORY_ARRIVAL_NAME=128 };
/* Authored destination, copied from one resident area's AiLocEditable table.
 * This is NOT a retained native lease, movement permission or a wire format.
 * Zero authored facing is preserved: stock placement falls back to the actor's
 * existing facing. The future placement adapter must apply that rule itself. */
typedef struct SudekiMpLanStoryAreaDestination {
    uint64_t dispatch_serial;
    uint32_t epoch, revision, marker_identifier, native_state;
    uint16_t sector;
    uint8_t zone_flags;
    float position[3], facing[3];
    char zone[SUDEKIMP_LAN_STORY_NAME_SIZE], marker[SUDEKIMP_STORY_ARRIVAL_NAME];
} SudekiMpLanStoryAreaDestination;
/* Explicit area + exact native marker ID and text, obtained from an authored
 * door contract, never a client-provided coordinate. Reads resident state only:
 * no ResourceName construction/lookup, native load, reference edits, global
 * current-room changes or marker creation. Missing/ambiguous/changing markers
 * fail with output untouched. Requires a unique matching sector in that area's
 * resident navigation catalog; this is not proof of playable terrain or a
 * retained navigation lifetime. Does not prove collision/activation or authorize
 * travel; destination must be re-resolved under its native lifetime at actuation.
 * Uses the same READY roster/thread witness as MembershipObserve. */
BOOL SudekiMpLanStoryAreaDestinationResolve(HMODULE image,
    const SudekiMpControlUpdateDispatchWitness *,const SudekiMpLanStoryNativeRoster *,
    const char *zone,uint32_t marker_identifier,const char *marker,
    SudekiMpLanStoryAreaDestination *);

#endif
