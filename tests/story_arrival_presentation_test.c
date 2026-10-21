/* Real runtime scene gate and reseed proof, no native or transport playback. */
#include "../src/hooks/lan_story_runtime.c"
#include "../src/network/lan_party_story.c"
#include "../src/engine/party_ownership.c"
#define get32 arrival_get32
#define put32 arrival_put32
#include "../src/network/lan_story_handoff.c"
#undef get32
#undef put32
#include "../src/network/lan_story_step_away.c"

#include <assert.h>
unsigned SudekiMpLanPartyLocalCharacter(SudekiMpLanPartySession *s) {assert(s==session);return 2;}
static SudekiMpLanStoryNativeRoster expected_roster;
static BOOL native_exact=TRUE;
BOOL SudekiMpLanStoryClientRosterExact(const SudekiMpLanStoryNativeRoster *r) {
    return native_exact && r && !memcmp(r,&expected_roster,sizeof(*r));
}
/* Native writers and transport are spies here; WorldCapacity's arrival case
 * separately runs real prepare/apply. These assertions cover runtime routing. */
static SudekiMpLanStoryScene apply_remote;
static SudekiMpLanPartyLease apply_connection;
static unsigned apply_step,apply_cancel;
static BOOL apply_frame_exact=TRUE,apply_party_ok=TRUE,apply_world_ok=TRUE,apply_prepare_ok=TRUE;
BOOL SudekiMpLanPartyPeerStatusGet(SudekiMpLanPartySession *s,unsigned p,SudekiMpLanPartyPeerStatus *out) {
    assert(s==session && p==local_seat);memset(out,0,sizeof(*out));out->lease=apply_connection;return TRUE;
}
BOOL SudekiMpLanPartyGetStoryScene(SudekiMpLanPartySession *s,const SudekiMpLanPartyLease *l,
    uint32_t now,SudekiMpLanStoryScene *out) {
    (void)now;assert(s==session && same_connection(l,&apply_connection));*out=apply_remote;return TRUE;
}
BOOL SudekiMpLanStoryFrameMatchesScene(const SudekiMpLanStoryFrame *f,const SudekiMpLanStoryScene *sc) {
    assert(f->available_mask==4 && sc->available_mask==4);return apply_frame_exact;
}
static void apply_check(const SudekiMpLanStoryNativeRoster *r,SudekiMpLanStoryReplicaExact exact,void *c,unsigned step) {
    assert(apply_step++==step && r->available_mask==4 && r->leader_character==2 &&
        r->actors[2]==expected_roster.actors[2] && !r->actors[3] && exact(r,c));
    assert(expected_roster.available_mask==12 && expected_roster.actors[3]);
}
BOOL SudekiMpLanStoryReplicaPrepareEquipment(const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanStoryFrame *f,
    SudekiMpLanStoryReplicaExact x,void *c) {(void)f;apply_check(r,x,c,0);return TRUE;}
BOOL SudekiMpLanStoryWorldPrepare(const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanStoryWorldFrame *f,
    SudekiMpLanStoryReplicaExact x,void *c,BOOL v) {
    (void)f;assert(v);apply_check(r,x,c,1);if(!apply_prepare_ok) SetLastError(ERROR_IO_PENDING);return apply_prepare_ok;
}
BOOL SudekiMpLanStoryReplicaApply(const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanStoryFrame *f,BOOL v,
    SudekiMpLanStoryReplicaExact x,void *c) {(void)f;assert(v);apply_check(r,x,c,2);return apply_party_ok;}
BOOL SudekiMpLanStoryWorldApplyPrepared(const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanStoryWorldFrame *f,
    SudekiMpLanStoryReplicaExact x,void *c) {
    (void)f;apply_check(r,x,c,3);SetLastError(ERROR_RETRY);return apply_world_ok;
}
BOOL SudekiMpLanStoryWorldCancelPrepared(void) {++apply_cancel;return TRUE;}
void SudekiMpLogFormat(const char *fmt,...) {(void)fmt;}
static BOOL restore_cast_ok=TRUE,restore_room_ok=TRUE;
static unsigned cast_restores,room_restores;
BOOL SudekiMpLanStoryLocalControlRestoreCastView(const SudekiMpLanStoryNativeRoster *r,
    SudekiMpLanStoryReplicaExact x,void *c) {assert(r==&expected_roster && x==replica_exact && !c);++cast_restores;return restore_cast_ok;}
BOOL SudekiMpLanStoryReplicaRestoreView(const SudekiMpLanStoryNativeRoster *r,
    SudekiMpLanStoryReplicaExact x,void *c) {assert(r==&expected_roster && x==replica_exact && !c);++room_restores;return restore_room_ok;}
void SudekiMpLogWrite(const char *text) {(void)text;}
static void shared_view_lifetime_cases(void) {
    client_local_selected=TRUE;presented_cast_view.kind=1;
    restore_cast_ok=FALSE;unsigned cancels=apply_cancel;
    assert(!begin_shared_view(&expected_roster,TRUE) && presented_cast_view.kind==1 && apply_cancel==cancels+1);
    restore_cast_ok=TRUE;assert(begin_shared_view(&expected_roster,TRUE));
    assert(!presented_cast_view.kind && cast_restores==2);
    assert(finish_shared_view(&expected_roster,FALSE,TRUE) && host_room_view && !host_cinematic_view && !room_restores);
    restore_room_ok=FALSE;assert(!finish_shared_view(&expected_roster,FALSE,FALSE));
    assert(host_room_view && !host_cinematic_view && room_restores==1);
    restore_room_ok=TRUE;assert(finish_shared_view(&expected_roster,FALSE,FALSE));
    assert(!host_room_view && !host_cinematic_view && room_restores==2);
    assert(finish_shared_view(&expected_roster,FALSE,FALSE) && room_restores==2);
    assert(finish_shared_view(&expected_roster,TRUE,FALSE) && host_cinematic_view);
    assert(finish_shared_view(&expected_roster,FALSE,TRUE) && host_room_view && !host_cinematic_view && room_restores==2);
    assert(finish_shared_view(&expected_roster,FALSE,FALSE) && room_restores==3);
    unsigned casts=cast_restores;assert(begin_shared_view(&expected_roster,FALSE) && casts==cast_restores);
    apply_cancel=0;
}
static void arrival_room_camera_cases(void) {
    session=(void *)1;local_seat=1;client_lead_spectate=client_local_selected=TRUE;
    apply_remote=(SudekiMpLanStoryScene){.epoch=7,.revision=8,.phase=SUDEKIMP_LAN_STORY_READY,
        .available_mask=4,.leader_seat=2,.inside_mask=4,.world="newbrightwater",.temporary="lica_tals_room"};
    SudekiMpLanStoryScene native=apply_remote;native.available_mask=12;
    apply_connection=(SudekiMpLanPartyLease){.token=1,.generation=1,.seat=1};
    SudekiMpLanPartyLease connection=apply_connection;
    SudekiMpLanStoryFrame party={.epoch=7,.revision=8,.available_mask=4,.leader_character=2,.view={.valid=1}};
    StoryPresentation f={.party=&party,.connection=&connection};
    assert(arrival_room_view(&native,&f));
    for(unsigned fault=0;fault<11;++fault) {
        SudekiMpLanStoryScene saved=apply_remote,native_saved=native;
        if(fault==0)client_lead_spectate=FALSE;
        if(fault==1)client_local_selected=FALSE;
        if(fault==2)party.view.valid=0;
        if(fault==3)native.phase=SUDEKIMP_LAN_STORY_LOADING;
        if(fault==4)++connection.token;
        if(fault==5)strcpy(apply_remote.temporary,"other");
        if(fault==6)strcpy(native.world,"other");
        if(fault==7)strcpy(native.temporary,"other");
        if(fault==8)apply_frame_exact=FALSE;
        if(fault==9)apply_remote.leader_seat=3;
        if(fault==10)apply_remote.inside_mask=0;
        assert(!arrival_room_view(&native,&f));
        apply_remote=saved;native=native_saved;connection=apply_connection;
        client_lead_spectate=client_local_selected=TRUE;party.view.valid=1;apply_frame_exact=TRUE;
    }
    assert(!host_cinematic_view && !host_room_view); /* Selection grants no input hold or view write. */
    client_lead_spectate=FALSE;
}
static void arrival_apply_cases(void) {
    apply_remote=(SudekiMpLanStoryScene){.epoch=7,.revision=8,.phase=SUDEKIMP_LAN_STORY_READY,
        .available_mask=4,.leader_seat=2,.inside_mask=4,.world="newbrightwater",.temporary="lica_tals_room"};
    SudekiMpLanStoryScene native=apply_remote;native.available_mask=12;
    expected_roster=(SudekiMpLanStoryNativeRoster){.available_mask=12,.leader_character=2,
        .actors={NULL,NULL,(void *)20,(void *)30},.ai={NULL,NULL,(void *)21,(void *)31}};
    apply_connection=(SudekiMpLanPartyLease){.token=1,.generation=1,.seat=1};
    SudekiMpLanPartyLease connection=apply_connection;
    SudekiMpLanStoryFrame party={.available_mask=4};SudekiMpLanStoryWorldFrame world={0};
    StoryPresentation f={.party=&party,.world=&world,.connection=&connection};BOOL pa,wa;
    client_local_selected=TRUE;
    assert(apply_arrival_party_world(&expected_roster,&native,&f,TRUE,&pa,&wa) && pa && wa && apply_step==4);
    for(unsigned failure=0;failure<3;++failure) {
        apply_step=0;
        if(failure==0) ++connection.token;
        if(failure==1) apply_frame_exact=FALSE;
        if(failure==2) native_exact=FALSE;
        assert(!apply_arrival_party_world(&expected_roster,&native,&f,TRUE,&pa,&wa) && !pa && !wa && !apply_step);
        connection=apply_connection;apply_frame_exact=native_exact=TRUE;
    }
    apply_prepare_ok=FALSE;
    assert(!apply_arrival_party_world(&expected_roster,&native,&f,TRUE,&pa,&wa) && !pa && !wa &&
        f.resources_waiting && apply_step==2);apply_prepare_ok=TRUE;apply_step=0;
    apply_party_ok=FALSE;
    assert(!apply_arrival_party_world(&expected_roster,&native,&f,TRUE,&pa,&wa) && !pa && !wa &&
        apply_cancel==1 && apply_step==3);apply_party_ok=TRUE;apply_step=0;
    apply_world_ok=FALSE;
    assert(!apply_arrival_party_world(&expected_roster,&native,&f,TRUE,&pa,&wa) && pa && !wa &&
        GetLastError()==ERROR_RETRY && apply_step==4);apply_world_ok=TRUE;
    puts("PASS named ArrivalApply projected writer ordering, fresh proof, cancellation and error routing");
}

static void arrival_cases(void) {
    SudekiMpLanStoryScene before={.epoch=6,.revision=7,.phase=SUDEKIMP_LAN_STORY_READY,
        .available_mask=12,.leader_seat=3};strcpy(before.world,"newbrightwater");
    SudekiMpLanStoryScene solo=before;solo.epoch=7;solo.revision=8;solo.available_mask=4;
    solo.leader_seat=2;solo.inside_mask=4;strcpy(solo.temporary,"lica_tals_room");
    SudekiMpLanStoryScene native=solo;native.available_mask=12;
    assert(SudekiMpStoryArrivalEdge(&before,&solo));
    SudekiMpLanStoryScene between=before;between.epoch++;between.revision++;between.leader_seat=2;
    assert(SudekiMpStoryArrivalEdge(&before,&between));
    SudekiMpLanStoryScene reverse_pair=between;reverse_pair.epoch++;reverse_pair.revision++;reverse_pair.leader_seat=3;
    assert(SudekiMpStoryArrivalEdge(&between,&reverse_pair));
    assert(presentation_membership(&native,&solo));
    arrival_presented_scene=before;arrival_generation[2]=10;arrival_generation[3]=20;
    SudekiMpLanStoryFrame f={.epoch=7,.revision=8,.available_mask=4,.leader_character=2};
    f.actors[2].generation=11;
    assert(arrival_generations(&solo,&f) && presentation_reseed_exact(&native,&solo,&f));
    for(unsigned c=0;c<4;++c) {++f.actors[c].generation;assert(!arrival_generations(&solo,&f));--f.actors[c].generation;}
    ++f.epoch;assert(!arrival_generations(&solo,&f));--f.epoch;
    SudekiMpLanStoryScene bad=solo;strcpy(bad.temporary,"other");assert(!SudekiMpStoryArrivalEdge(&before,&bad));
    bad=solo;++bad.epoch;assert(!SudekiMpStoryArrivalEdge(&before,&bad));
    bad=solo;bad.available_mask=8;assert(!SudekiMpStoryArrivalEdge(&before,&bad));
    bad=solo;bad.leader_seat=3;assert(!SudekiMpStoryArrivalEdge(&before,&bad));
    bad=solo;bad.inside_mask=0;assert(!SudekiMpStoryArrivalEdge(&before,&bad));
    arrival_presented_scene=solo;arrival_generation[2]=11;
    SudekiMpLanStoryScene after=before;after.epoch=8;after.revision=9;
    f.epoch=8;f.revision=9;f.available_mask=12;f.leader_character=3;f.actors[2].generation=12;f.actors[3].generation=21;
    assert(arrival_generations(&after,&f));
    assert(client_foreign_characters(&solo)==8 && client_foreign_characters(&after)==0);
    expected_roster=(SudekiMpLanStoryNativeRoster){.available_mask=12,.leader_character=2,
        .actors={NULL,NULL,(void *)20,(void *)30},.ai={NULL,NULL,(void *)21,(void *)31}};
    StoryArrivalProjection p;SudekiMpLanStoryNativeRoster r=expected_roster;
    assert(arrival_project(&r,&native,&solo,&p));
    assert(p.projected.available_mask==4 && p.projected.actors[2]==r.actors[2] && !p.projected.actors[3]);
    assert(!memcmp(&r,&expected_roster,sizeof(r)) && arrival_projected_exact(&p.projected,&p));
    native_exact=FALSE;assert(!arrival_projected_exact(&p.projected,&p));native_exact=TRUE;
    p.projected.ai[2]=NULL;assert(!arrival_projected_exact(&p.projected,&p));
    /* A fresh join may not yet have selected Tal; only presentation projects
     * the leader. Native input/camera selection still uses the real roster. */
    r.leader_character=expected_roster.leader_character=native.leader_seat=3;
    assert(arrival_project(&r,&native,&solo,&p) && p.projected.leader_character==2 && r.leader_character==3);
    SudekiMpPartyAssignment a={.world=1,.revision=1,.humans=3,.available=12,
        .character={3,2,4,4},.generation={1,1,1,1}};
    SudekiMpPartyOwnership o;assert(SudekiMpPartyOwnershipInitializePresence(&o,&a,3,1));
    o.away=2;SudekiMpPartyOwnership old=o;
    assert(!arrival_resume_owner(&o,&solo) && !memcmp(&o,&old,sizeof(o))); /* default off */
    client_lead_spectate=TRUE;
    assert(arrival_resume_owner(&o,&solo) && !o.away && o.assignment.revision==2 &&
        o.last_request[1]==old.last_request[1] && o.control[1]==SUDEKIMP_PARTY_CONTROL_AI);
    assert(!arrival_resume_owner(&o,&solo)); /* idempotent; handshake still required */
    away_client=(SudekiMpStoryAwayClient){.phase=SUDEKIMP_STORY_AWAY_AWAY,.want_away=1};
    away_pending_request=50;away_retry_at=100;
    SudekiMpLanStoryControlState offer={.fence={7,8,1,11,1,2},.phase=SUDEKIMP_STORY_CONTROL_PREPARE};
    assert(arrival_return_intent(&o,1,&solo,&offer,200) && !away_client.want_away &&
        away_client.phase==SUDEKIMP_STORY_AWAY_RETURNING && !away_pending_request && !away_retry_at);
    for(unsigned fault=0;fault<5;++fault) {
        away_client.want_away=1;SudekiMpLanStoryControlState stale=offer;
        if(fault==0) --stale.fence.epoch;
        if(fault==1) stale.fence.player=2;
        if(fault==2) stale.fence.character=3;
        if(fault==3) stale.phase=0;
        if(fault==4) o.away=2;
        assert(!arrival_return_intent(&o,1,&solo,&stale,200) && away_client.want_away);o.away=0;
    }
    host_seed.transaction=1;host_seed.seed_scene=host_live_scene=solo;
    host_live_receipt=500;host_binding_ready=TRUE;loaded_party_mask=12;
    assert(host_catchup_available(500));
    host_seed.seed_scene=before;assert(!host_catchup_available(500));host_seed.seed_scene=solo;
    --host_seed.seed_scene.epoch;assert(!host_catchup_available(500));++host_seed.seed_scene.epoch;
    --host_seed.seed_scene.revision;assert(!host_catchup_available(500));++host_seed.seed_scene.revision;
    assert(!host_catchup_available(1000));

    o=old;o.connected=1;o.away=0;assert(!arrival_resume_owner(&o,&solo));
    o=old;assert(!arrival_resume_owner(&o,&after));
    client_lead_spectate=FALSE;
    puts("PASS named ArrivalMembershipGenerationProjection AwayResume (runtime functions, no native calls)");
}
int main(void) {
    arrival_room_camera_cases();
    shared_view_lifetime_cases();
    session=(void *)1;presented_epoch=6;presentation_changed=FALSE;
    SudekiMpLanStoryScene native={.epoch=6,.revision=6,.phase=SUDEKIMP_LAN_STORY_READY,.available_mask=12,.leader_seat=2};
    strcpy(native.world,"newbrightwater");SudekiMpLanStoryScene remote=native;remote.epoch=7;
    for(unsigned condition=0;condition<3;++condition) {
        remote=native;remote.epoch=7;history_count=world_history_count=1;
        if(condition==0) {remote.phase=SUDEKIMP_LAN_STORY_LOADING;remote.available_mask=0;}
        if(condition==1) {strcpy(remote.temporary,"lica_tals_room");remote.inside_mask=4;}
        if(condition==2) remote.available_mask=4;
        assert(presentation_scene_gate(&native,&remote)==1);
        assert(!presentation_changed && !history_count && !world_history_count && presented_epoch==6);
        remote=native;remote.epoch=7;
        assert(presentation_scene_gate(&native,&remote)==0 && !presentation_changed);
    }
    SudekiMpLanStoryFrame frame={.epoch=7};
    for(unsigned c=0;c<4;++c) frame.actors[c].generation=presented_generations[c]=10+c;
    assert(presentation_reseed_exact(&native,&remote,&frame));
    for(unsigned c=0;c<4;++c) {
        ++frame.actors[c].generation;assert(!presentation_reseed_exact(&native,&remote,&frame));--frame.actors[c].generation;
    }
    --frame.epoch;assert(!presentation_reseed_exact(&native,&remote,&frame));++frame.epoch;
    presentation_changed=TRUE;history_count=world_history_count=1;
    assert(presentation_scene_gate(&native,&remote)==5 && presentation_changed);
    remote.phase=SUDEKIMP_LAN_STORY_LOADING;
    assert(presentation_scene_gate(&native,&remote)==5 && presentation_changed);
    arrival_cases();arrival_apply_cases();
    puts("StoryArrivalPresentationTest: PASS (runtime gate/reseed only)");return 0;
}
