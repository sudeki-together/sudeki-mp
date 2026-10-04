/* Coordinator policy fixture: mocked native owners, no Sudeki image or inputs. */
#include "../src/hooks/lan_party_client_control.c"
#include <assert.h>
#include <stdio.h>

static unsigned player=1u, assigned=2u, physical=2u, disconnected;
static BOOL release_ready=TRUE, cast_ready=TRUE, menu_owned, menu_exact=TRUE;
static SudekiMpLanPartyPeerStatus peer={{0x12345678u,17u,1u},SUDEKIMP_LAN_PARTY_ACTIVE,0,0,0,0,1};
static SudekiMpLanPartyRosterObservation world;
static SudekiMpLanPartyLease held[4];
static uint32_t generations[4];
static int objects[6];
static SudekiMpLanPartySession *const session=(SudekiMpLanPartySession *)&objects[0];

unsigned SudekiMpLanPartyLocalSeat(SudekiMpLanPartySession *s) { assert(s==session); return player; }
unsigned SudekiMpLanPartyLocalCharacter(SudekiMpLanPartySession *s) { assert(s==session); return assigned; }
unsigned SudekiMpLanPartyControlLocalCharacter(void) { return physical; }
BOOL SudekiMpLanPartyPeerStatusGet(SudekiMpLanPartySession *s,unsigned p,SudekiMpLanPartyPeerStatus *out) {
    assert(s==session && p==player); *out=peer; return TRUE;
}
BOOL SudekiMpLanPartyLeaseActive(SudekiMpLanPartySession *s,const SudekiMpLanPartyLease *key) {
    assert(s==session); return peer.phase==SUDEKIMP_LAN_PARTY_ACTIVE && same_lease(key,&peer.lease);
}
BOOL SudekiMpLanPartyDisconnect(SudekiMpLanPartySession *s,const SudekiMpLanPartyLease *key) {
    assert(s==session && same_lease(key,&peer.lease)); ++disconnected;
    peer.phase=SUDEKIMP_LAN_PARTY_DRAINING; return TRUE;
}
BOOL SudekiMpLanPartyClientRejoin(SudekiMpLanPartySession *s) {
    assert(s==session); peer.phase=SUDEKIMP_LAN_PARTY_JOINING; return TRUE;
}
BOOL SudekiMpControlSeparationUpdateDispatchWitnessStillExact(const SudekiMpControlUpdateDispatchWitness *w) {
    return w && w->service_post_original_exact;
}
BOOL SudekiMpLanPartyControlActorLeasesEmpty(void) {
    for(unsigned i=0;i<4u;++i) if(held[i].token) return FALSE;
    return TRUE;
}
BOOL SudekiMpLanPartyControlHasLeases(void) {
    return menu_owned || !SudekiMpLanPartyControlActorLeasesEmpty();
}
BOOL SudekiMpLanPartyControlMenuInputExact(void *actor) {
    return menu_owned && menu_exact && actor==world.actors[physical];
}
BOOL SudekiMpLanPartyControlBeginClientSession(const SudekiMpControlUpdateDispatchWitness *w,unsigned c) {
    return w && c==physical && !SudekiMpLanPartyControlHasLeases();
}
BOOL SudekiMpLanPartyControlObserveRoster(const SudekiMpControlUpdateDispatchWitness *w,SudekiMpLanPartyRosterObservation *out) {
    if(!w) return FALSE;
    *out=world; return TRUE;
}
void *SudekiMpLanPartyControlObserveActor(const SudekiMpControlUpdateDispatchWitness *w,unsigned c) {
    return w && c<4u?world.actors[c]:NULL;
}
BOOL SudekiMpLanPartyControlNativeActorExact(const SudekiMpLanPartyRosterObservation *r,unsigned c) {
    return r && c==physical && r->actors[c]==world.actors[c];
}
SudekiMpLanPartyRosterStatus SudekiMpLanPartyRosterService(SudekiMpLanPartyRoster *r,
    const SudekiMpControlUpdateDispatchWitness *w) {
    assert(w); r->bound=world; r->bound_valid=TRUE; r->initialized_mask=15u;
    return SUDEKIMP_LAN_PARTY_ROSTER_READY;
}
BOOL SudekiMpLanPartyControlNextLease(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *connection,unsigned c,SudekiMpLanPartyLease *key) {
    if(!w || c>=4u || c==physical || held[c].token || !same_lease(connection,&peer.lease)) return FALSE;
    *key=*connection; key->seat=(uint8_t)c; key->generation=generations[c]+1u; return TRUE;
}
BOOL SudekiMpLanPartyControlExact(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *key,void *actor) {
    return w && key->seat<4u && same_lease(key,&held[key->seat]) && actor==world.actors[key->seat];
}
BOOL SudekiMpLanPartyControlRetains(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *key,void *actor) {
    return SudekiMpLanPartyControlExact(w,key,actor);
}
BOOL SudekiMpLanPartyControlAcquire(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *key,void *actor,SudekiMpLanPartyControlDrainProbe ready) {
    assert(key->seat<4u && key->seat!=physical && actor==world.actors[key->seat]);
    assert(!held[key->seat].token && key->generation>generations[key->seat]);
    if(!ready(key,actor,w)) return FALSE;
    held[key->seat]=*key; generations[key->seat]=key->generation; return TRUE;
}
BOOL SudekiMpLanPartyControlQuiesce(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *key,void *actor) {
    return SudekiMpLanPartyControlExact(w,key,actor);
}
BOOL SudekiMpLanPartyControlRelease(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *key,void *actor,SudekiMpLanPartyControlDrainProbe drained) {
    assert(SudekiMpLanPartyControlExact(w,key,actor));
    if(!drained(key,actor,w)) return FALSE;
    memset(&held[key->seat],0,sizeof(held[key->seat])); return TRUE;
}
BOOL SudekiMpLanPartyCastRebindLocal(const SudekiMpControlUpdateDispatchWitness *w,unsigned c) {
    return w && cast_ready && c==physical && (assigned==c || assigned==4u);
}
BOOL SudekiMpLanPartyCastRebindLocalDisconnected(const SudekiMpControlUpdateDispatchWitness *w,unsigned c) {
    return w && cast_ready && c==physical &&
        (peer.phase==SUDEKIMP_LAN_PARTY_DRAINING || peer.phase==SUDEKIMP_LAN_PARTY_FREE);
}
static BOOL drained(const SudekiMpLanPartyLease *key,void *actor,
    const SudekiMpControlUpdateDispatchWitness *w) {
    assert(w && key->seat<4u && actor==world.actors[key->seat]); return release_ready;
}
int main(void) {
    world.group=&objects[0]; world.controller=&objects[1]; world.present_mask=15u;
    for(unsigned i=0;i<4u;++i) { world.actors[i]=&objects[i+2]; generations[i]=100u+i; }
    SudekiMpControlUpdateDispatchWitness w={0}; w.service_post_original_exact=1u;
    SudekiMpLanPartyClientControl *c=SudekiMpLanPartyClientControlCreate(session,player,drained);
    SudekiMpLanPartyClientControlReport report;
    assert(c && SudekiMpLanPartyClientControlService(c,&w,&report));
    assert(report.ready && report.owned_mask==11u && report.lease.seat==1u && report.lease.generation==17u);
    for(unsigned i=0;i<4u;++i) if(i!=physical) assert(c->native[i].seat==i && c->native[i].generation>100u);
    /* A new logical assignment cannot prevent the initial physical baseline.
     * Runtime, not this presentation coordinator, owns human input authority. */
    assigned=3u;
    assert(SudekiMpLanPartyClientControlService(c,&w,&report) && report.ready && report.owned_mask==11u);
    assigned=2u;
    SudekiMpLanPartyLease old[4]; memcpy(old,c->native,sizeof(old));
    menu_owned=TRUE; /* The persistent input fence must not block actor drain. */
    SudekiMpLanPartyClientControlSuspendBindings(c,TRUE); release_ready=FALSE;
    assert(SudekiMpLanPartyClientControlService(c,&w,&report) && report.failed_mask==11u);
    assert(!SudekiMpLanPartyClientControlBindingsDrained(c) && !disconnected);
    assert(!SudekiMpLanPartyClientControlSetLocalCharacter(c,&w,3u));
    release_ready=TRUE;
    assert(SudekiMpLanPartyClientControlService(c,&w,&report));
    assert(SudekiMpLanPartyClientControlBindingsDrained(c) && !disconnected);
    assigned=3u; assert(!SudekiMpLanPartyClientControlSetLocalCharacter(c,&w,3u));
    physical=3u; cast_ready=FALSE; assert(!SudekiMpLanPartyClientControlSetLocalCharacter(c,&w,3u));
    cast_ready=TRUE; assert(SudekiMpLanPartyClientControlSetLocalCharacter(c,&w,3u));
    SudekiMpLanPartyClientControlSuspendBindings(c,FALSE);
    assert(SudekiMpLanPartyClientControlService(c,&w,&report) && report.ready && report.owned_mask==7u);
    assert(c->native[0].generation>old[0].generation && c->native[1].generation>old[1].generation);
    assert(peer.phase==SUDEKIMP_LAN_PARTY_ACTIVE && !disconnected);
    SudekiMpLanPartyClientControlSuspendBindings(c,TRUE);
    assert(SudekiMpLanPartyClientControlService(c,&w,&report));
    assigned=4u; physical=0u; assert(SudekiMpLanPartyClientControlSetLocalCharacter(c,&w,0u));
    SudekiMpLanPartyClientControlSuspendBindings(c,FALSE);
    assert(SudekiMpLanPartyClientControlService(c,&w,&report) && report.ready && report.owned_mask==14u);
    assert(!disconnected && report.lease.seat==1u && report.lease.generation==17u);
    /* Transport loss drains actors but retains our exact UI-only fence. */
    peer.phase=SUDEKIMP_LAN_PARTY_DRAINING; release_ready=FALSE;
    assert(SudekiMpLanPartyClientControlService(c,&w,&report));
    assert(!SudekiMpLanPartyClientControlRejoin(c,&w));
    release_ready=TRUE; assert(SudekiMpLanPartyClientControlService(c,&w,&report));
    assert(SudekiMpLanPartyControlHasLeases() && SudekiMpLanPartyControlActorLeasesEmpty());
    menu_exact=FALSE; assert(!SudekiMpLanPartyClientControlRejoin(c,&w));
    menu_exact=TRUE; assert(SudekiMpLanPartyClientControlRejoin(c,&w));
    assert(menu_owned && !disconnected && peer.phase==SUDEKIMP_LAN_PARTY_JOINING);
    assert(SudekiMpLanPartyClientControlService(c,&w,&report) && !report.ready);
    peer.lease.token+=1u; peer.lease.generation+=1u; peer.phase=SUDEKIMP_LAN_PARTY_ACTIVE;
    assert(SudekiMpLanPartyClientControlService(c,&w,&report) && report.ready && report.owned_mask==14u);
    assert(report.lease.generation==18u && report.lease.seat==player && menu_owned);
    /* Losing UDP during a native switch preserves the physical result and
     * allows a later explicit reconnect without fabricating an ACTIVE lease.
     * The host's logical assignment may still describe the old character. */
    assigned=0u;
    SudekiMpLanPartyClientControlSuspendBindings(c,TRUE);
    peer.phase=SUDEKIMP_LAN_PARTY_DRAINING; release_ready=FALSE;
    assert(SudekiMpLanPartyClientControlService(c,&w,&report));
    assert(!SudekiMpLanPartyClientControlFinishDisconnectedSwitch(c,&w,2u));
    release_ready=TRUE; assert(SudekiMpLanPartyClientControlService(c,&w,&report));
    physical=2u;
    assert(!SudekiMpLanPartyClientControlSetLocalCharacter(c,&w,2u));
    menu_exact=FALSE;
    assert(!SudekiMpLanPartyClientControlFinishDisconnectedSwitch(c,&w,2u));
    menu_exact=TRUE; cast_ready=FALSE;
    assert(!SudekiMpLanPartyClientControlFinishDisconnectedSwitch(c,&w,2u));
    cast_ready=TRUE; peer.phase=SUDEKIMP_LAN_PARTY_ACTIVE;
    assert(!SudekiMpLanPartyClientControlFinishDisconnectedSwitch(c,&w,2u));
    peer.phase=SUDEKIMP_LAN_PARTY_DRAINING;
    assert(SudekiMpLanPartyClientControlFinishDisconnectedSwitch(c,&w,2u));
    assert(c->retiring && c->local_character==2u && c->lease.generation==18u && assigned==0u);
    SudekiMpLanPartyClientControlSuspendBindings(c,FALSE);
    assert(SudekiMpLanPartyClientControlService(c,&w,&report) && !report.ready);
    assert(SudekiMpLanPartyClientControlRejoin(c,&w));
    peer.lease.token+=1; peer.lease.generation+=1; peer.phase=SUDEKIMP_LAN_PARTY_ACTIVE;
    assert(SudekiMpLanPartyClientControlService(c,&w,&report) && report.ready && report.owned_mask==11u);
    assert(c->local_character==2u && report.lease.generation==19u && assigned==0u);
    SudekiMpLanPartyClientControlRequestStop(c);
    assert(disconnected==1u && SudekiMpLanPartyClientControlService(c,&w,&report));
    assert(SudekiMpLanPartyClientControlDestroy(c));
    puts("lan_party_client_control_test: PASS (mocked ownership/coordinator only; no native gameplay)");
    return 0;
}
