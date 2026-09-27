#include "hooks/lan_party_client_control.h"
#include <stdlib.h>
#include <string.h>

struct SudekiMpLanPartyClientControl {
    SudekiMpLanPartySession *session;
    SudekiMpLanPartyControlDrainProbe drained;
    SudekiMpLanPartyRoster roster;
    SudekiMpLanPartyLease lease;
    void *actors[4];
    unsigned int local_seat;
    volatile LONG stopping;
    BOOL initialized, retiring;
};
static PVOID volatile client_coordinator_owner;
static BOOL same_lease(const SudekiMpLanPartyLease *a,const SudekiMpLanPartyLease *b) {
    return a->seat==b->seat && a->token==b->token && a->generation==b->generation;
}
SudekiMpLanPartyClientControl *SudekiMpLanPartyClientControlCreate(
    SudekiMpLanPartySession *session,unsigned int local_seat,
    SudekiMpLanPartyControlDrainProbe drained) {
    SudekiMpLanPartyPeerStatus self;
    SudekiMpLanPartyClientControl *c;
    if (!session || !drained || !local_seat || local_seat>=4 ||
        SudekiMpLanPartyControlHasLeases() ||
        SudekiMpLanPartyLocalSeat(session)!=local_seat ||
        !SudekiMpLanPartyPeerStatusGet(session,local_seat,&self)) return NULL;
    c=calloc(1,sizeof(*c));
    if (!c) return NULL;
    c->session=session; c->local_seat=local_seat; c->drained=drained;
    if (InterlockedCompareExchangePointer(&client_coordinator_owner,c,NULL)) {
        free(c); return NULL;
    }
    return c;
}
void SudekiMpLanPartyClientControlRequestStop(SudekiMpLanPartyClientControl *c) {
    SudekiMpLanPartyPeerStatus p;
    if (!c) return;
    InterlockedExchange(&c->stopping,1);
    if (SudekiMpLanPartyPeerStatusGet(c->session,c->local_seat,&p) &&
        (p.phase==SUDEKIMP_LAN_PARTY_ACTIVE || p.phase==SUDEKIMP_LAN_PARTY_PENDING))
        (void)SudekiMpLanPartyDisconnect(c->session,&p.lease);
}
BOOL SudekiMpLanPartyClientControlDestroy(SudekiMpLanPartyClientControl *c) {
    if (!c) return TRUE;
    if (!InterlockedCompareExchange(&c->stopping,0,0)) return FALSE;
    for (unsigned i=0;i<4;++i) if(c->actors[i]) return FALSE;
    if (InterlockedCompareExchangePointer(&client_coordinator_owner,NULL,c)!=c) return FALSE;
    free(c); return TRUE;
}
BOOL SudekiMpLanPartyClientControlService(SudekiMpLanPartyClientControl *c,
    const SudekiMpControlUpdateDispatchWitness *w,SudekiMpLanPartyClientControlReport *out) {
    SudekiMpLanPartyPeerStatus p;
    if (!c || !out || !w || !w->service_post_original_exact ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w)) return FALSE;
    memset(out,0,sizeof(*out));
    if (!c->initialized) {
        if (!SudekiMpLanPartyControlBeginClientSession(w,c->local_seat)) return FALSE;
        c->initialized=TRUE;
    }
    if (!SudekiMpLanPartyPeerStatusGet(c->session,c->local_seat,&p)) return FALSE;
    if (InterlockedCompareExchange(&c->stopping,0,0) ||
        (p.phase!=SUDEKIMP_LAN_PARTY_ACTIVE && p.phase!=SUDEKIMP_LAN_PARTY_PENDING) ||
        (c->lease.token && !same_lease(&c->lease,&p.lease))) c->retiring=TRUE;
    if (c->retiring) {
        /* No fresh session can overwrite an old native ownership record. */
        for (unsigned i=0;i<4;++i) {
            SudekiMpLanPartyLease key=c->lease; key.seat=(uint8_t)i;
            if (!c->actors[i]) continue;
            out->draining_mask|=(uint8_t)(1u<<i);
            (void)SudekiMpLanPartyControlQuiesce(w,&key,c->actors[i]);
            if (SudekiMpLanPartyControlRelease(w,&key,c->actors[i],c->drained))
                c->actors[i]=NULL;
            else out->failed_mask|=(uint8_t)(1u<<i);
        }
        return TRUE;
    }
    if (!p.lease.token || !p.lease.generation) return TRUE;
    c->lease=p.lease; out->lease=p.lease;
    out->roster_status=SudekiMpLanPartyRosterService(&c->roster,w);
    if (out->roster_status==SUDEKIMP_LAN_PARTY_ROSTER_REPLACED) {
        c->retiring=TRUE; SudekiMpLanPartyClientControlRequestStop(c); return TRUE;
    }
    if (out->roster_status!=SUDEKIMP_LAN_PARTY_ROSTER_READY) return TRUE;
    for (unsigned i=0;i<4;++i) {
        SudekiMpLanPartyLease key=p.lease; key.seat=(uint8_t)i;
        uint8_t bit=(uint8_t)(1u<<i);
        void *actor;
        if (i==c->local_seat) continue;
        actor=SudekiMpLanPartyControlObserveActor(w,i);
        if (!c->actors[i]) {
            BOOL acquired=actor && SudekiMpLanPartyControlAcquire(w,&key,actor,c->drained);
            if (acquired || (actor && SudekiMpLanPartyControlRetains(w,&key,actor)))
                c->actors[i]=actor;
            if (!acquired) {
                out->failed_mask|=bit;
                if (c->actors[i]) {
                    c->retiring=TRUE; SudekiMpLanPartyClientControlRequestStop(c);
                }
                continue;
            }
        }
        if (actor!=c->actors[i] || !SudekiMpLanPartyControlExact(w,&key,actor)) {
            out->failed_mask|=bit; c->retiring=TRUE;
            SudekiMpLanPartyClientControlRequestStop(c); continue;
        }
        out->owned_mask|=bit;
    }
    out->ready=!c->retiring && out->owned_mask==(uint8_t)(15u&~(1u<<c->local_seat)) &&
        SudekiMpLanPartyLeaseActive(c->session,&c->lease);
    return TRUE;
}
