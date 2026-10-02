#ifndef SUDEKIMP_LAN_PARTY_REPLICA_H
#define SUDEKIMP_LAN_PARTY_REPLICA_H
#include "network/lan_party_session.h"
#include "network/lan_arena_replica.h"

typedef struct SudekiMpLanPartyReplica {
    SudekiMpLanPartyLease lease;
    SudekiMpLanArenaReplica chunks[2];
    SudekiMpLanPartyFrame latest_frame;
    SudekiMpLanPartyAilishWeaponState ailish_weapon;
    SudekiMpLanArenaReplicaRenderClock clock;
    struct {
        uint32_t tick;
        SudekiMpLanPartyRangedPresentation ranged[2];
    } presentation[32];
    unsigned presentation_count;
    uint32_t received_at;
} SudekiMpLanPartyReplica;

/* Plain data only. Complete frames use one render clock, shared action-time
 * protection and atomic admission/reset; no native writes occur here. */
void SudekiMpLanPartyReplicaReset(SudekiMpLanPartyReplica *replica);
BOOL SudekiMpLanPartyReplicaPush(SudekiMpLanPartyReplica *replica,
    const SudekiMpLanPartyLease *lease, const SudekiMpLanPartyFrame *frame);
/* Drain authenticated transport frames on the game thread. End/rejoin closes
 * the old stream before any new token/generation can be sampled. */
BOOL SudekiMpLanPartyReplicaConsume(SudekiMpLanPartyReplica *replica,
    SudekiMpLanPartySession *session, unsigned int local_seat);
/* Newest complete host-confirmed events/resources, independent of the delayed
 * body render clock. Same actor/token/generation and 500ms receive-age fence. */
BOOL SudekiMpLanPartyReplicaLatestFrame(const SudekiMpLanPartyReplica *replica,
    const SudekiMpLanPartyLease *lease,uint32_t now,SudekiMpLanPartyFrame *frame);
BOOL SudekiMpLanPartyReplicaSample(SudekiMpLanPartyReplica *replica,
    const SudekiMpLanPartyLease *lease, uint32_t now,
    SudekiMpLanPartyFrame *sample, uint32_t *render_tick);
#endif
