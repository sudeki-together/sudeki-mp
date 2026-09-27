#include "hooks/lan_party_roster.h"
#include <math.h>

SudekiMpLanPartyRosterStatus SudekiMpLanPartyRosterService(
    SudekiMpLanPartyRoster *r, const SudekiMpControlUpdateDispatchWitness *w) {
    static const float offsets[4][2] = {{0,0},{1.5f,-1.5f},{-1.5f,1.5f},{1.5f,1.5f}};
    SudekiMpLanPartyRosterObservation current;
    unsigned int seat;
    if (!r || r->replaced) return SUDEKIMP_LAN_PARTY_ROSTER_REPLACED;
    if (!SudekiMpLanPartyControlObserveRoster(w,&current))
        return SUDEKIMP_LAN_PARTY_ROSTER_WAITING;
    if (r->bound_valid) {
        if (current.group != r->bound.group || current.controller != r->bound.controller)
            r->replaced = TRUE;
        for (seat=0; seat<4; ++seat)
            if (r->bound.actors[seat] && current.actors[seat] != r->bound.actors[seat])
                r->replaced = TRUE;
        if (r->replaced) return SUDEKIMP_LAN_PARTY_ROSTER_REPLACED;
    } else {
        r->bound = current;
        r->bound_valid = TRUE;
    }
    /* Preserve the initial room anchor, not the player's moving position. */
    for (seat=0; seat<4; ++seat) r->bound.actors[seat] = current.actors[seat];
    r->bound.present_mask = current.present_mask;
    r->spawn_pending_mask &= (uint8_t)~current.present_mask;
    if (current.present_mask == 15 && r->initialized_mask == 15)
        return SUDEKIMP_LAN_PARTY_ROSTER_READY;
    /* No inventory, group arm pass, or spawn while native combat is active.
     * A completed roster stays ready in combat; this is only a startup gate. */
    if (current.combat || r->spawn_pending_mask)
        return SUDEKIMP_LAN_PARTY_ROSTER_WAITING;
    for (seat=0; seat<4; ++seat) {
        uint8_t bit = (uint8_t)(1u << seat);
        if (!(current.present_mask & bit)) {
            float position[3] = {r->bound.anchor[0]+offsets[seat][0],
                r->bound.anchor[1],r->bound.anchor[2]+offsets[seat][1]};
            if (!isfinite(position[0]) || !isfinite(position[1]) || !isfinite(position[2]))
                return SUDEKIMP_LAN_PARTY_ROSTER_WAITING;
            /* Keep the pending bit across arbitrarily many callbacks. A
             * successful native call is not evidence of task completion. */
            r->spawn_pending_mask |= bit;
            if (!SudekiMpLanPartyControlSpawnActor(w,&current,seat,position))
                r->spawn_pending_mask &= (uint8_t)~bit;
            return SUDEKIMP_LAN_PARTY_ROSTER_WAITING;
        }
        if (!(r->initialized_mask & bit)) {
            if (SudekiMpLanPartyControlInitializeActor(w,&current,seat))
                r->initialized_mask |= bit;
            return SUDEKIMP_LAN_PARTY_ROSTER_WAITING;
        }
    }
    return SUDEKIMP_LAN_PARTY_ROSTER_WAITING;
}
