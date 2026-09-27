#include "network/lan_party_replica.h"
#include <string.h>

static BOOL same_lease(const SudekiMpLanPartyLease *a, const SudekiMpLanPartyLease *b) {
    return a && b && a->seat == b->seat && a->token == b->token &&
        a->generation == b->generation;
}
static BOOL valid_lease(const SudekiMpLanPartyLease *l) {
    return l && l->seat > 0 && l->seat < 4 && l->token && l->generation;
}
void SudekiMpLanPartyReplicaReset(SudekiMpLanPartyReplica *r) {
    if (r) memset(r,0,sizeof(*r));
}
BOOL SudekiMpLanPartyReplicaPush(SudekiMpLanPartyReplica *r,
    const SudekiMpLanPartyLease *lease, const SudekiMpLanPartyFrame *frame) {
    SudekiMpLanPartyReplica next;
    BOOL discontinuity = FALSE;
    unsigned int i;
    if (!r || !valid_lease(lease) || !SudekiMpLanPartyFrameValid(frame)) return FALSE;
    if (same_lease(&r->lease,lease) && r->chunks[0].latest_valid &&
        (int32_t)(frame->chunk[0].host_tick-r->chunks[0].latest.host_tick)<=0) return FALSE;
    next = *r;
    if (!same_lease(&r->lease,lease)) {
        SudekiMpLanPartyReplicaReset(&next); next.lease = *lease;
    }
    for (i=0;i<2;++i) {
        SudekiMpLanArenaCodecRoster roster = {{
            SudekiMpLanPartyActorType(i*2),SudekiMpLanPartyActorType(i*2+1)},1};
        uint32_t generation = next.chunks[i].stream_generation;
        if (!SudekiMpLanArenaReplicaPushForRoster(&next.chunks[i],&frame->chunk[i],&roster))
            return FALSE; /* Do not publish a half-applied frame. */
        if (generation && generation != next.chunks[i].stream_generation)
            discontinuity = TRUE;
    }
    if (discontinuity) {
        /* Even a chunk-zero-only dummy reset resets BOTH actor histories. */
        SudekiMpLanArenaReplicaRenderClockReset(&next.clock);
        for (i=0;i<2;++i) {
            SudekiMpLanArenaCodecRoster roster = {{
                SudekiMpLanPartyActorType(i*2),SudekiMpLanPartyActorType(i*2+1)},1};
            SudekiMpLanArenaReplicaReset(&next.chunks[i]);
            if (!SudekiMpLanArenaReplicaPushForRoster(&next.chunks[i],&frame->chunk[i],&roster))
                return FALSE;
        }
    }
    next.ailish_weapon = frame->ailish_weapon;
    *r = next; return TRUE;
}
BOOL SudekiMpLanPartyReplicaConsume(SudekiMpLanPartyReplica *r,
    SudekiMpLanPartySession *session, unsigned int local_seat) {
    SudekiMpLanPartyPeerStatus peer;
    SudekiMpLanPartyFrame frame;
    if (!r || !session || !local_seat || local_seat >= 4) return FALSE;
    if (!SudekiMpLanPartyPeerStatusGet(session,local_seat,&peer) ||
        peer.phase != SUDEKIMP_LAN_PARTY_ACTIVE || !valid_lease(&peer.lease)) {
        SudekiMpLanPartyReplicaReset(r); return FALSE;
    }
    if (!same_lease(&r->lease,&peer.lease)) {
        SudekiMpLanPartyReplicaReset(r); r->lease = peer.lease;
    }
    while (SudekiMpLanPartyTakeFrame(session,&frame)) {
        if (!SudekiMpLanPartyLeaseActive(session,&peer.lease) ||
            !SudekiMpLanPartyReplicaPush(r,&peer.lease,&frame)) {
            SudekiMpLanPartyReplicaReset(r); return FALSE;
        }
    }
    if (!SudekiMpLanPartyLeaseActive(session,&peer.lease)) {
        SudekiMpLanPartyReplicaReset(r); return FALSE;
    }
    return TRUE;
}
BOOL SudekiMpLanPartyReplicaSample(SudekiMpLanPartyReplica *r,
    const SudekiMpLanPartyLease *lease, uint32_t now,
    SudekiMpLanPartyFrame *sample, uint32_t *render_tick) {
    SudekiMpLanArenaReplicaRenderClock clock;
    SudekiMpLanPartyFrame next;
    uint32_t tick;
    BOOL action;
    if (!r || !sample || !render_tick || !valid_lease(lease) ||
        !same_lease(&r->lease,lease)) return FALSE;
    clock = r->clock;
    action = SudekiMpLanArenaReplicaActionTimelineBuffered(&r->chunks[0]) ||
        SudekiMpLanArenaReplicaActionTimelineBuffered(&r->chunks[1]);
    if (!SudekiMpLanArenaReplicaRenderClockAdvanceWithCatchup(&r->chunks[0],
            &clock,now,!action,&tick) ||
        !SudekiMpLanArenaReplicaSampleAllActorMotion(&r->chunks[0],tick,&next.chunk[0]) ||
        !SudekiMpLanArenaReplicaSampleAllActorMotion(&r->chunks[1],tick,&next.chunk[1]))
        return FALSE;
    next.ailish_weapon = r->ailish_weapon;
    if (!SudekiMpLanPartyFrameValid(&next)) return FALSE;
    r->clock = clock; *sample = next; *render_tick = tick; return TRUE;
}
