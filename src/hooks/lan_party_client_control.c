#include "hooks/lan_party_client_control.h"
#include "hooks/lan_party_cast.h"
#include <stdlib.h>
#include <string.h>

struct SudekiMpLanPartyClientControl {
    SudekiMpLanPartySession *session;
    SudekiMpLanPartyControlDrainProbe drained;
    SudekiMpLanPartyRoster roster;
    SudekiMpLanPartyLease lease; /* Transport connection lifetime. */
    SudekiMpLanPartyLease native[4];
    void *actors[4];
    unsigned int local_seat, local_character;
    volatile LONG stopping, suspended;
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
    c->local_character=SudekiMpLanPartyLocalCharacter(session);
    if(c->local_character>=4u) { free(c); return NULL; }
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
        if (!SudekiMpLanPartyControlBeginClientSession(w,c->local_character)) return FALSE;
        c->initialized=TRUE;
    }
    if (!SudekiMpLanPartyPeerStatusGet(c->session,c->local_seat,&p)) return FALSE;
    /* No native ownership exists during the first handshake or a drained
     * retry. JOINING must not permanently latch the retiring state. */
    if(p.phase==SUDEKIMP_LAN_PARTY_JOINING && !c->lease.token &&
        !c->retiring && !InterlockedCompareExchange(&c->stopping,0,0))
        return TRUE;
    if (InterlockedCompareExchange(&c->stopping,0,0) ||
        (p.phase!=SUDEKIMP_LAN_PARTY_ACTIVE && p.phase!=SUDEKIMP_LAN_PARTY_PENDING) ||
        (c->lease.token && !same_lease(&c->lease,&p.lease))) c->retiring=TRUE;
    if (c->retiring || InterlockedCompareExchange(&c->suspended,0,0)) {
        /* No fresh session can overwrite an old native ownership record. */
        for (unsigned i=0;i<4;++i) {
            const SudekiMpLanPartyLease *key=&c->native[i];
            if (!c->actors[i]) continue;
            out->draining_mask|=(uint8_t)(1u<<i);
            (void)SudekiMpLanPartyControlQuiesce(w,key,c->actors[i]);
            if (SudekiMpLanPartyControlRelease(w,key,c->actors[i],c->drained)) {
                c->actors[i]=NULL; memset(&c->native[i],0,sizeof(c->native[i]));
            }
            else out->failed_mask|=(uint8_t)(1u<<i);
        }
        return TRUE;
    }
    /* Presentation must establish a baseline even if an assignment arrives
     * before the first loaded frame. Runtime separately fences input/ACKs
     * until the physical controller has completed the requested handoff. */
    if (!p.lease.token || !p.lease.generation ||
        c->local_character!=SudekiMpLanPartyControlLocalCharacter()) return TRUE;
    c->lease=p.lease; out->lease=p.lease;
    out->roster_status=SudekiMpLanPartyRosterService(&c->roster,w);
    if (out->roster_status==SUDEKIMP_LAN_PARTY_ROSTER_REPLACED) {
        c->retiring=TRUE; SudekiMpLanPartyClientControlRequestStop(c); return TRUE;
    }
    if (out->roster_status!=SUDEKIMP_LAN_PARTY_ROSTER_READY) return TRUE;
    for (unsigned i=0;i<4;++i) {
        SudekiMpLanPartyLease *key=&c->native[i];
        uint8_t bit=(uint8_t)(1u<<i);
        void *actor;
        if (i==c->local_character) continue;
        actor=SudekiMpLanPartyControlObserveActor(w,i);
        if (!c->actors[i]) {
            BOOL allocated=actor && SudekiMpLanPartyControlNextLease(w,&p.lease,i,key);
            BOOL acquired=allocated && SudekiMpLanPartyControlAcquire(w,key,actor,c->drained);
            if (acquired || (allocated && SudekiMpLanPartyControlRetains(w,key,actor)))
                c->actors[i]=actor;
            if (!acquired) {
                out->failed_mask|=bit;
                if (c->actors[i]) {
                    c->retiring=TRUE; SudekiMpLanPartyClientControlRequestStop(c);
                }
                continue;
            }
        }
        if (actor!=c->actors[i] || !SudekiMpLanPartyControlExact(w,key,actor)) {
            out->failed_mask|=bit; c->retiring=TRUE;
            SudekiMpLanPartyClientControlRequestStop(c); continue;
        }
        out->owned_mask|=bit;
    }
    out->ready=!c->retiring && out->owned_mask==(uint8_t)(15u&~(1u<<c->local_character)) &&
        SudekiMpLanPartyLeaseActive(c->session,&c->lease);
    return TRUE;
}
BOOL SudekiMpLanPartyClientControlRejoin(SudekiMpLanPartyClientControl *c,
    const SudekiMpControlUpdateDispatchWitness *w) {
    SudekiMpLanPartyRosterObservation current;
    if(!c || !w || !w->service_post_original_exact ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) ||
        !c->initialized || !c->retiring || c->roster.replaced ||
        InterlockedCompareExchange(&c->stopping,0,0) ||
        !SudekiMpLanPartyControlActorLeasesEmpty() ||
        !SudekiMpLanPartyControlObserveRoster(w,&current)) return FALSE;
    for(unsigned i=0;i<4;++i)
        if(c->actors[i] || (c->roster.bound.actors[i] &&
            c->roster.bound.actors[i]!=current.actors[i])) return FALSE;
    if(c->roster.bound_valid &&
        (current.group!=c->roster.bound.group ||
         current.controller!=c->roster.bound.controller)) return FALSE;
    if(c->local_character>=4u || !SudekiMpLanPartyControlNativeActorExact(&current,c->local_character) ||
        (SudekiMpLanPartyControlHasLeases() &&
         !SudekiMpLanPartyControlMenuInputExact(current.actors[c->local_character]))) return FALSE;
    if(!SudekiMpLanPartyClientRejoin(c->session)) return FALSE;
    memset(&c->lease,0,sizeof(c->lease));
    c->retiring=FALSE;
    return TRUE;
}

void SudekiMpLanPartyClientControlSuspendBindings(SudekiMpLanPartyClientControl *c,BOOL suspend) {
    if(c) InterlockedExchange(&c->suspended,suspend?1:0);
}
BOOL SudekiMpLanPartyClientControlBindingsDrained(const SudekiMpLanPartyClientControl *c) {
    if(!c || !c->initialized || !InterlockedCompareExchange((LONG *)&c->suspended,0,0))
        return FALSE;
    for(unsigned i=0;i<4u;++i) if(c->actors[i]) return FALSE;
    return SudekiMpLanPartyControlActorLeasesEmpty();
}
BOOL SudekiMpLanPartyClientControlSetLocalCharacter(SudekiMpLanPartyClientControl *c,
    const SudekiMpControlUpdateDispatchWitness *w,unsigned character) {
    SudekiMpLanPartyRosterObservation observed;
    if(!c || character>=4u || !w || !w->service_post_original_exact || c->retiring ||
        InterlockedCompareExchange(&c->stopping,0,0) ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) ||
        !SudekiMpLanPartyClientControlBindingsDrained(c) ||
        (SudekiMpLanPartyLocalCharacter(c->session)<4u &&
         SudekiMpLanPartyLocalCharacter(c->session)!=character) ||
        SudekiMpLanPartyControlLocalCharacter()!=character ||
        !SudekiMpLanPartyLeaseActive(c->session,&c->lease) || c->roster.replaced ||
        !c->roster.bound_valid || !SudekiMpLanPartyControlObserveRoster(w,&observed) ||
        observed.present_mask!=15u || observed.group!=c->roster.bound.group ||
        observed.controller!=c->roster.bound.controller ||
        memcmp(observed.actors,c->roster.bound.actors,sizeof(observed.actors)) ||
        !SudekiMpLanPartyControlNativeActorExact(&observed,character) ||
        !SudekiMpLanPartyCastRebindLocal(w,character)) return FALSE;
    c->local_character=character;
    c->roster.bound=observed;
    return TRUE;
}
BOOL SudekiMpLanPartyClientControlFinishDisconnectedSwitch(SudekiMpLanPartyClientControl *c,
    const SudekiMpControlUpdateDispatchWitness *w,unsigned character) {
    SudekiMpLanPartyRosterObservation observed;
    SudekiMpLanPartyPeerStatus peer;
    if(!c || !w || !w->service_post_original_exact || character>=4u || !c->retiring ||
        InterlockedCompareExchange(&c->stopping,0,0) ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) ||
        !SudekiMpLanPartyClientControlBindingsDrained(c) || c->roster.replaced ||
        !c->roster.bound_valid ||
        !SudekiMpLanPartyPeerStatusGet(c->session,c->local_seat,&peer) ||
        (peer.phase!=SUDEKIMP_LAN_PARTY_FREE && peer.phase!=SUDEKIMP_LAN_PARTY_DRAINING) ||
        SudekiMpLanPartyControlLocalCharacter()!=character ||
        !SudekiMpLanPartyControlObserveRoster(w,&observed) || observed.present_mask!=15u ||
        observed.group!=c->roster.bound.group || observed.controller!=c->roster.bound.controller ||
        memcmp(observed.actors,c->roster.bound.actors,sizeof(observed.actors)) ||
        !SudekiMpLanPartyControlNativeActorExact(&observed,character) ||
        !SudekiMpLanPartyControlMenuInputExact(observed.actors[character]) ||
        !SudekiMpLanPartyCastRebindLocalDisconnected(w,character)) return FALSE;
    c->local_character=character;
    c->roster.bound=observed;
    return TRUE;
}
