/* Runtime composition policy only. Native adapters and transport observations
 * are controlled below; this neither loads nor executes the game image. */
#include "../src/hooks/lan_story_runtime.c"
#include "../src/network/lan_story_handoff.c"
#include <assert.h>
#include <stdio.h>

static SudekiMpControlUpdateDispatchWitness current_witness;
static SudekiMpLanStoryNativeRoster current_roster;
static SudekiMpLanStoryScene current_scene;
static SudekiMpLanPartyLease expected_key;
static BOOL control_exact,move_ok,restore_ok,drain_ok,camera_retained;
static BOOL dev_play,avatar_ready;
static unsigned clear_calls,move_calls,restore_calls,drain_calls;
static char operations[32];
static unsigned operation_count;
static int fixture_session,fixture_actor,fixture_world;
static SudekiMpLanPartyPeerStatus current_peer;
static SudekiMpLanStoryControlState current_offer;
static SudekiMpLanStoryNativeRoster retained_pause;
static SudekiMpLanStoryScene retained_pause_scene;
static BOOL pause_available,pause_exact,fresh_exact,registry_exact;
static BOOL menu_frame_fixture,presentation_active_fixture,direction_ok,revoke_pause_in_present;
static unsigned revoke_after_direction,direction_calls,presentation_calls,direction_serial;
static unsigned fresh_checks;
static BOOL startup_roster_available,startup_select_ok,startup_ready,startup_revoke_ready;
static unsigned startup_select_calls;
static SudekiMpLanStoryAvatarPartyObservation native_party;
static BOOL native_party_exact,native_input_exact=TRUE;
static void *native_input_controller,*native_input_actor;
BOOL SudekiMpLanStoryAvatarPartyObserve(SudekiMpLanStoryAvatarPartyObservation *out) {
    if(!native_party_exact) return FALSE;
    *out=native_party; return TRUE;
}
BOOL SudekiMpLanStoryInputExact(void *controller,void *actor) {
    return native_input_exact && controller==native_input_controller && actor==native_input_actor;
}
BOOL SudekiMpLanStoryInputHostFenceExact(void *controller,void *actor) {
    return SudekiMpLanStoryInputExact(controller,actor);
}
BOOL SudekiMpLanStoryHostControlBound(void *controller,const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryScene *scene) {
    (void)controller; (void)w; (void)scene; return FALSE;
}
void SudekiMpLogFormat(const char *format,...) { (void)format; }
BOOL SudekiMpLanStoryObserverRoster(void *controller,const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryScene *scene,SudekiMpLanStoryNativeRoster *r) {
    assert(w==&current_witness && scene==&current_scene && r);
    if(!startup_roster_available || controller!=current_roster.controller) return FALSE;
    *r=current_roster; return TRUE;
}
BOOL SudekiMpLanStoryHostControlReady(void *controller,const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryScene *scene) {
    assert(controller==current_roster.controller && w==&current_witness && scene==&current_scene);
    if(startup_revoke_ready) fresh_exact=FALSE;
    return startup_ready;
}
BOOL SudekiMpLanStoryHostControlSelect(void *controller,const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryScene *scene,unsigned target) {
    assert(controller==current_roster.controller && w==&current_witness && scene==&current_scene);
    assert(target==host_startup_target && !host_binding_ready && !runtime_ready);
    ++startup_select_calls; return startup_select_ok;
}
BOOL SudekiMpLanStoryObserverRosterStillExact(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r) {
    ++fresh_checks;
    return fresh_exact && w==&current_witness && r && r->dispatch_serial==w->dispatch_serial &&
        !memcmp(r,&current_roster,sizeof(*r));
}
BOOL SudekiMpLanStoryClientPausedRoster(SudekiMpLanStoryNativeRoster *r,SudekiMpLanStoryScene *scene) {
    if(!menu_frame_fixture || !pause_available || !pause_exact || !registry_exact) return FALSE;
    *r=retained_pause; *scene=retained_pause_scene;
    return TRUE;
}
BOOL SudekiMpLanStoryClientRosterExact(const SudekiMpLanStoryNativeRoster *r) {
    return (menu_frame_fixture || presentation_active_fixture) && pause_exact && r &&
        !memcmp(r,&retained_pause,sizeof(*r));
}
BOOL SudekiMpLanStoryClientEffectsPresent(SudekiMpLanStoryClientPresentation callback,
    void *context,SudekiMpLanStoryClientEffectsWitness witness,void *witness_context) {
    /* Model the real bracket's full pre/post pause/registry checks, and its
     * scoped authorization outside a menu frame. This proves composition,
     * not the native checks implemented by the real pause adapter. */
    ++presentation_calls;
    if(!callback || !witness || presentation_active_fixture || !witness(witness_context) ||
        !pause_available || !pause_exact || !registry_exact) return FALSE;
    presentation_active_fixture=TRUE;
    if(revoke_pause_in_present) pause_exact=FALSE;
    BOOL result=callback(&retained_pause,&retained_pause_scene,context);
    presentation_active_fixture=FALSE;
    BOOL still_exact=pause_available && pause_exact && registry_exact && witness(witness_context);
    return result && still_exact;
}
BOOL SudekiMpLanStoryAvatarCameraDirection(const SudekiMpLanStoryAvatarCameraIdentity *identity,
    SudekiMpLanStoryAvatarCameraExact exact,void *context,float x,float z,float *world_x,float *world_z) {
    (void)exact; const AvatarCameraScope *scope=context;
    assert(identity==&avatar_camera_identity && scope && !scope->cleanup && !scope->witness);
    assert(presentation_active_fixture && !menu_frame_fixture && scope->scene==&retained_pause_scene);
    assert(SudekiMpLanStoryClientRosterExact(scope->roster));
    direction_serial=scope->roster->dispatch_serial; ++direction_calls;
    *world_x=z; *world_z=-x; /* Provisional output must not escape a failed postcheck. */
    switch(revoke_after_direction) {
    case 1: pause_exact=FALSE; break;
    case 2: registry_exact=FALSE; break;
    case 3: fresh_exact=FALSE; break;
    case 4: current_witness.service_post_original_exact=FALSE; break;
    }
    return direction_ok;
}
static void record(char operation) {
    assert(operation_count+1u<sizeof(operations));
    operations[operation_count++]=operation; operations[operation_count]=0;
}
void SudekiMpLanStoryInputClear(void) { ++clear_calls; record('C'); }
BOOL SudekiMpLanStoryControlExact(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanPartyLease *key) {
    assert(w==&current_witness && r==&current_roster);
    assert(!memcmp(key,&expected_key,sizeof(*key)));
    return control_exact;
}
BOOL SudekiMpLanStoryControlMove(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanPartyLease *key,float x,float z,BOOL *held) {
    assert(w==&current_witness && r==&current_roster && held);
    assert(!memcmp(key,&expected_key,sizeof(*key)));
    assert(!host_control[0].ready && x==0 && z==0);
    ++move_calls; record('M'); *held=FALSE; return move_ok;
}
BOOL SudekiMpLanStoryAvatarCameraRetains(void) { return camera_retained; }
BOOL SudekiMpLanStoryAvatarCameraRestore(const SudekiMpLanStoryAvatarCameraIdentity *identity,
    SudekiMpLanStoryAvatarCameraExact exact,void *context) {
    (void)exact; const AvatarCameraScope *scope=context;
    assert(identity==&avatar_camera_identity && scope && scope->cleanup);
    assert(scope->witness==&current_witness && scope->roster==&current_roster && scope->scene==&current_scene);
    assert(!host_control[0].ready && host_control[0].draining);
    ++restore_calls; record('R');
    if(restore_ok) camera_retained=FALSE;
    return restore_ok;
}
BOOL SudekiMpLanStoryControlDrain(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanPartyLease *key) {
    assert(w==&current_witness && r==&current_roster && !camera_retained && !avatar_camera_bound);
    assert(!memcmp(key,&expected_key,sizeof(*key)));
    assert(!host_control[0].ready && host_control[0].draining);
    ++drain_calls; record('D'); return drain_ok;
}
BOOL SudekiMpLanPartyDevPlay(SudekiMpLanPartySession *s) {
    assert(s==(SudekiMpLanPartySession *)&fixture_session); return dev_play;
}
BOOL SudekiMpLanStoryAvatarSeatChosen(unsigned player) { return player==1u && avatar_ready; }
BOOL SudekiMpLanStoryAvatarSeatReady(unsigned player,const SudekiMpLanStoryNativeRoster *r,
    void **actor,uint32_t *generation) {
    assert(player==1u && r==&client_seed);
    if(actor) *actor=&fixture_actor;
    if(generation) *generation=77u;
    return avatar_ready;
}
BOOL SudekiMpLanPartyPeerStatusGet(SudekiMpLanPartySession *s,unsigned player,SudekiMpLanPartyPeerStatus *out) {
    assert(s==session && player==1u && out); *out=current_peer; return TRUE;
}
BOOL SudekiMpLanPartyGetStoryControl(SudekiMpLanPartySession *s,const SudekiMpLanPartyLease *key,
    uint32_t now,SudekiMpLanStoryControlState *out) {
    (void)now; assert(s==session && key && out);
    assert(same_connection(key,&current_peer.lease)); *out=current_offer; return TRUE;
}
static unsigned revoke_calls;
BOOL SudekiMpLanPartyRevokeStoryControl(SudekiMpLanPartySession *s,const SudekiMpLanPartyLease *key) {
    assert(s==session && key==&host_control[1].connection); ++revoke_calls; return TRUE;
}
static void reset_observations(void) {
    clear_calls=move_calls=restore_calls=drain_calls=operation_count=0;
    memset(operations,0,sizeof(operations));
}
static void seed_host(void) {
    memset(host_control,0,sizeof(host_control));
    current_witness=(SudekiMpControlUpdateDispatchWitness){.dispatch_serial=9,.service_post_original_exact=TRUE};
    current_roster=(SudekiMpLanStoryNativeRoster){.world=&fixture_world,.epoch=3,.revision=8};
    current_scene=(SudekiMpLanStoryScene){.epoch=3,.revision=8};
    expected_key=(SudekiMpLanPartyLease){.token=71,.generation=12,.seat=4};
    host_control[0].native_key=expected_key; host_control[0].transaction=19;
    host_control[0].ready=host_control[0].acquired=TRUE;
    host_control[1].native_key=(SudekiMpLanPartyLease){.token=72,.generation=13,.seat=5};
    host_control[1].transaction=20; host_control[1].ready=TRUE;
    avatar_camera_bound=camera_retained=TRUE;
    control_exact=move_ok=restore_ok=drain_ok=TRUE;
    reset_observations();
}
static void party_readiness_loss(void) {
    seed_host(); local_seat=0; host_binding_ready=TRUE; runtime_ready=1; revoke_calls=0;
    close_avatar_party_admission(&current_witness);
    assert(!runtime_ready && !host_binding_ready && !host_control[0].ready);
    assert(!host_control[1].ready && host_control[1].draining && revoke_calls==1);
    assert(host_control[0].native_key.token==71 && host_control[1].native_key.token==72);
    assert(camera_retained && avatar_camera_bound && !move_calls && !restore_calls && !drain_calls);
    assert(clear_calls==1); /* Unknown native identity is never dereferenced. */
    seed_host(); local_seat=1; client_input_ready=TRUE; runtime_ready=1; revoke_calls=0;
    close_avatar_party_admission(&current_witness);
    assert(!client_input_ready && !runtime_ready && !host_binding_ready && !revoke_calls);
    assert(clear_calls==1 && !move_calls && camera_retained);
    local_seat=0;
}
static void remote_unchanged(void) {
    assert(host_control[1].ready && !host_control[1].draining && host_control[1].transaction==20);
    assert(host_control[1].native_key.token==72 && host_control[1].native_key.seat==5);
}
static void readiness_loss(void) {
    seed_host();
    host_avatar_hold(&current_witness,&current_roster);
    assert(!host_control[0].ready && !host_control[0].draining && host_control[0].acquired);
    assert(!strcmp(operations,"CM") && move_calls==1 && !restore_calls && !drain_calls);
    assert(!memcmp(&host_control[0].native_key,&expected_key,sizeof(expected_key)));
    remote_unchanged();
    reset_observations(); control_exact=FALSE; host_control[0].ready=TRUE;
    host_avatar_hold(&current_witness,&current_roster);
    assert(!host_control[0].ready && !strcmp(operations,"C") && !move_calls);
    reset_observations(); control_exact=TRUE; move_ok=FALSE;
    host_avatar_hold(&current_witness,&current_roster);
    assert(host_control[0].draining && !strcmp(operations,"CM"));
    reset_observations();
    host_avatar_hold(&current_witness,NULL);
    assert(!strcmp(operations,"C") && host_control[0].native_key.token==expected_key.token);
    remote_unchanged();
}
static void drain_retries(void) {
    seed_host(); restore_ok=FALSE;
    assert(!host_avatar_drain(&current_witness,&current_roster,&current_scene));
    assert(!strcmp(operations,"CMR") && !drain_calls && camera_retained && avatar_camera_bound);
    assert(host_control[0].draining && host_control[0].acquired && host_control[0].transaction==19);
    assert(!memcmp(&host_control[0].native_key,&expected_key,sizeof(expected_key)));
    reset_observations(); restore_ok=TRUE; drain_ok=FALSE;
    assert(!host_avatar_drain(&current_witness,&current_roster,&current_scene));
    assert(!strcmp(operations,"CMRD") && !camera_retained && !avatar_camera_bound);
    assert(host_control[0].draining && host_control[0].native_key.token==expected_key.token);
    reset_observations(); drain_ok=TRUE;
    assert(host_avatar_drain(&current_witness,&current_roster,&current_scene));
    assert(!strcmp(operations,"CMD") && !restore_calls);
    assert(!host_control[0].native_key.token && !host_control[0].acquired && !host_control[0].draining);
    assert(!host_control[0].ready && host_control[0].transaction==19);
    remote_unchanged();
    reset_observations();
    assert(host_avatar_drain(&current_witness,&current_roster,&current_scene));
    assert(!strcmp(operations,"C") && !move_calls && !restore_calls && !drain_calls);
    seed_host();
    assert(!host_avatar_drain(&current_witness,NULL,&current_scene));
    assert(!strcmp(operations,"C") && host_control[0].native_key.token==expected_key.token);
    assert(camera_retained && avatar_camera_bound && host_control[0].draining);
    remote_unchanged();
}
static void unknown_observation(void) {
    seed_host(); control_exact=FALSE;
    /* Both unknown scene and unknown fresh roster dispatch paths call this
     * boundary with NULL, never reusing the preceding frame's native roster. */
    host_avatar_hold(&current_witness,NULL);
    assert(!host_control[0].ready && !host_control[0].draining && host_control[0].acquired);
    assert(!strcmp(operations,"C") && !move_calls && !restore_calls && !drain_calls);
    assert(camera_retained && avatar_camera_bound && host_control[0].transaction==19);
    assert(!memcmp(&host_control[0].native_key,&expected_key,sizeof(expected_key)));
    remote_unchanged();
    /* A later fresh proof can quiesce the retained lease normally. */
    reset_observations(); control_exact=TRUE;
    host_avatar_hold(&current_witness,&current_roster);
    assert(!strcmp(operations,"CM") && !host_control[0].ready && !host_control[0].draining);
}
static void seed_client(void) {
    session=(SudekiMpLanPartySession *)&fixture_session; local_seat=1;
    avatar_seats_attempted=avatar_camera_bound=client_local_selected=TRUE;
    dev_play=avatar_ready=TRUE; stopping=0;
    client_control_connection=(SudekiMpLanPartyLease){.token=81,.generation=22,.seat=1};
    client_control_fence=(SudekiMpLanStoryControlFence){3,8,10,77,1,SUDEKIMP_STORY_CHARACTER_ALLY};
    memset(&presentation_sample,0,sizeof(presentation_sample));
    presentation_sample.valid=TRUE; presentation_sample.receipt=GetTickCount();
    presentation_sample.lease=client_control_connection;
    presentation_sample.scene.epoch=3; presentation_sample.scene.revision=8;
    current_peer=(SudekiMpLanPartyPeerStatus){.lease=client_control_connection,
        .transport_confirmed=TRUE,.phase=SUDEKIMP_LAN_PARTY_OBSERVING};
    current_offer=(SudekiMpLanStoryControlState){.fence=client_control_fence,.phase=SUDEKIMP_STORY_CONTROL_PREPARE};
    ++current_offer.fence.transaction;
    client_input_ready=client_action_pending=TRUE;
    client_input_sequence=33; client_input_sent_at=44;
    reset_observations();
}
static void replacement_offer(void) {
    seed_client();
    /* The callback rejects the new transaction until composition refreshes
     * the validated plain fence; InputArmAvatar invokes this same callback. */
    assert(!avatar_input_exact(&fixture_actor,77,current_offer.fence.transaction,NULL));
    assert(client_avatar_refresh_offer(&current_peer.lease,&current_offer.fence));
    assert(avatar_input_exact(&fixture_actor,77,current_offer.fence.transaction,NULL));
    assert(!avatar_input_exact(&fixture_actor,77,current_offer.fence.transaction-1u,NULL));
    assert(!strcmp(operations,"C") && !client_input_ready && !client_action_pending);
    assert(!client_input_sequence && !client_input_sent_at);
    assert(SudekiMpLanStoryControlFenceSame(&client_control_fence,&current_offer.fence));
    reset_observations(); client_input_ready=TRUE; client_input_sequence=5;
    assert(client_avatar_refresh_offer(&current_peer.lease,&current_offer.fence));
    assert(!clear_calls && client_input_ready && client_input_sequence==5);
}
static void rejected_offers(void) {
    for(unsigned case_id=0;case_id<10u;++case_id) {
        seed_client();
        SudekiMpLanPartyLease next=current_peer.lease;
        SudekiMpLanStoryControlFence offer=current_offer.fence,old=client_control_fence;
        switch(case_id) {
        case 0: ++next.generation; break;
        case 1: ++next.token; break;
        case 2: presentation_sample.receipt-=1000u; break;
        case 3: ++presentation_sample.lease.generation; break;
        case 4: ++offer.epoch; break;
        case 5: ++offer.revision; break;
        case 6: offer.player=2; break;
        case 7: offer.character=2; break;
        case 8: dev_play=FALSE; break;
        case 9: client_local_selected=FALSE; break;
        }
        assert(!client_avatar_refresh_offer(&next,&offer));
        assert(SudekiMpLanStoryControlFenceSame(&client_control_fence,&old));
        assert(!clear_calls && client_input_ready && client_action_pending);
        assert(client_input_sequence==33 && client_input_sent_at==44);
    }
}
static int camera_objects[16];
static void seed_camera_rosters(void) {
    current_witness=(SudekiMpControlUpdateDispatchWitness){.dispatch_serial=88,.service_post_original_exact=TRUE};
    current_roster=(SudekiMpLanStoryNativeRoster){.dispatch_serial=88,.epoch=3,.revision=8,
        .available_mask=15,.leader_character=2,.world=&camera_objects[0],.descriptor=&camera_objects[1],
        .group=&camera_objects[2],.controller=&camera_objects[3]};
    for(unsigned i=0;i<4u;++i) { current_roster.actors[i]=&camera_objects[4+i]; current_roster.ai[i]=&camera_objects[8+i]; }
    current_scene=(SudekiMpLanStoryScene){.epoch=3,.revision=8,.observed_tick=100,
        .phase=SUDEKIMP_LAN_STORY_READY,.available_mask=15,.leader_seat=2,.world="camera_fixture"};
    retained_pause=current_roster; retained_pause.dispatch_serial=11;
    retained_pause_scene=current_scene; retained_pause_scene.observed_tick=50;
    pause_available=pause_exact=fresh_exact=registry_exact=direction_ok=TRUE;
    menu_frame_fixture=presentation_active_fixture=revoke_pause_in_present=FALSE;
    revoke_after_direction=direction_calls=presentation_calls=direction_serial=fresh_checks=0;
}
static void retained_camera_identity(void) {
    seed_camera_rosters();
    SudekiMpLanStoryNativeRoster output,fresh_before=current_roster;
    SudekiMpLanStoryScene scene_output;
    float x=123,z=456;
    /* The live controller call is outside the menu frame: direct lookup is
     * unavailable even though the retained enrollment remains exact. */
    assert(!SudekiMpLanStoryClientPausedRoster(&output,&scene_output));
    assert(!SudekiMpLanStoryClientRosterExact(&current_roster)); /* actual serial-sensitive contract */
    assert(client_avatar_camera_direction(&current_witness,&current_roster,&current_scene,2,3,&x,&z));
    assert(x==3 && z==-2 && direction_calls==1 && direction_serial==11 && presentation_calls==1);
    assert(fresh_checks>=4 && !presentation_active_fixture && !menu_frame_fixture);
    assert(!SudekiMpLanStoryClientRosterExact(&retained_pause)); /* bracket cannot leak */
    assert(retained_pause.dispatch_serial==11 && retained_pause_scene.observed_tick==50);
    assert(!memcmp(&current_roster,&fresh_before,sizeof(current_roster)) && current_witness.dispatch_serial==88);
    assert(client_avatar_camera_direction(&current_witness,&current_roster,&current_scene,0,0,&x,&z));
    assert(x==0 && z==0 && direction_calls==2); /* readiness read uses the same bracket */
    /* Every roster identity field and semantic scene field participates;
     * only independently proved dispatch/timestamp metadata may differ. */
    for(unsigned changed=0;changed<29u;++changed) {
        seed_camera_rosters();
        switch(changed) {
        case 0: ++current_roster.epoch; break;
        case 1: ++current_roster.revision; break;
        case 2: current_roster.available_mask=7; break;
        case 3: current_roster.leader_character=1; break;
        case 4: current_roster.world=&camera_objects[12]; break;
        case 5: current_roster.descriptor=&camera_objects[12]; break;
        case 6: current_roster.group=&camera_objects[12]; break;
        case 7: current_roster.controller=&camera_objects[12]; break;
        case 8: case 9: case 10: case 11: current_roster.actors[changed-8]=&camera_objects[12]; break;
        case 12: case 13: case 14: case 15: current_roster.ai[changed-12]=&camera_objects[12]; break;
        case 16: ++current_scene.epoch; break;
        case 17: ++current_scene.revision; break;
        case 18: current_scene.available_mask=7; break;
        case 19: current_scene.leader_seat=1; break;
        case 20: current_scene.phase=SUDEKIMP_LAN_STORY_LOADING; break;
        case 21: current_scene.inside_mask=1; break;
        case 22: strcpy(current_scene.world,"other_world"); break;
        case 23: strcpy(current_scene.temporary,"other_interior"); break;
        case 24: retained_pause.dispatch_serial=0; break;
        case 25: ++current_roster.dispatch_serial; break; /* no longer current witness */
        case 26: current_roster.native_leader=&camera_objects[12]; break;
        case 27: ++current_roster.native_avatar_generation; break;
        case 28: ++current_roster.native_avatar_player; break;
        }
        x=123; z=456;
        assert(!client_avatar_camera_direction(&current_witness,&current_roster,&current_scene,2,3,&x,&z));
        assert(x==0 && z==0 && !direction_calls && !presentation_active_fixture);
    }
    for(unsigned revoked=0;revoked<6u;++revoked) {
        seed_camera_rosters();
        switch(revoked) {
        case 0: pause_available=FALSE; break;
        case 1: pause_exact=FALSE; break;
        case 2: fresh_exact=FALSE; break;
        case 3: registry_exact=FALSE; break;
        case 4: current_witness.service_post_original_exact=FALSE; break;
        case 5: revoke_pause_in_present=TRUE; break;
        }
        x=123; z=456;
        assert(!client_avatar_camera_direction(&current_witness,&current_roster,&current_scene,2,3,&x,&z));
        assert(x==0 && z==0 && !direction_calls && !presentation_active_fixture);
    }
    for(unsigned revoked=0;revoked<5u;++revoked) {
        seed_camera_rosters(); revoke_after_direction=revoked;
        if(!revoked) direction_ok=FALSE;
        x=123; z=456;
        assert(!client_avatar_camera_direction(&current_witness,&current_roster,&current_scene,2,3,&x,&z));
        assert(x==0 && z==0 && direction_calls==1 && !presentation_active_fixture);
    }
}
static void typed_native_anchor(void) {
    seed_client(); seed_camera_rosters();
    memset(current_roster.actors,0,sizeof(current_roster.actors));
    memset(current_roster.ai,0,sizeof(current_roster.ai));
    current_roster.available_mask=0; current_roster.leader_character=4;
    current_roster.native_leader=&fixture_actor;
    current_roster.native_avatar_generation=77; current_roster.native_avatar_player=1;
    current_scene.available_mask=0; current_scene.leader_seat=4;
    client_seed=current_roster;
    retained_pause=current_roster; retained_pause.dispatch_serial=11;
    retained_pause_scene=current_scene;
    native_party=(SudekiMpLanStoryAvatarPartyObservation){.phase=SUDEKIMP_AVATAR_PARTY_READY,
        .epoch=current_roster.epoch,.spawn_generation=77,.local_player=1,.leader_character=4,
        .world=current_roster.world,.group=current_roster.group,.controller=current_roster.controller,
        .native_leader=&fixture_actor,.members={&fixture_actor},.member_count=1};
    native_party_exact=native_input_exact=TRUE;
    native_input_controller=current_roster.controller; native_input_actor=&fixture_actor;
    startup_roster_available=TRUE; avatar_party_required=TRUE;
    assert(native_anchor(&client_seed)==&fixture_actor);
    assert(input_closed(current_roster.controller));
    assert(host_binding_exact(current_roster.controller,&current_witness,&current_scene));
    native_input_exact=FALSE;
    assert(!input_closed(current_roster.controller));
    assert(!host_binding_exact(current_roster.controller,&current_witness,&current_scene));
    native_input_exact=TRUE;
    native_party_exact=FALSE;
    assert(!native_anchor(&client_seed) && !input_closed(current_roster.controller));
    assert(!host_binding_exact(current_roster.controller,&current_witness,&current_scene));
    native_party_exact=TRUE; ++native_party.spawn_generation;
    assert(!native_anchor(&client_seed) && !input_closed(current_roster.controller));
    --native_party.spawn_generation;
    assert(client_avatar_refresh_offer(&current_peer.lease,&current_offer.fence));
    assert(avatar_input_exact(&fixture_actor,77,current_offer.fence.transaction,NULL));
    assert(!avatar_input_exact(&fixture_actor,78,current_offer.fence.transaction,NULL));
    float x=123,z=456;
    assert(client_avatar_camera_direction(&current_witness,&current_roster,&current_scene,2,3,&x,&z));
    assert(x==3 && z==-2 && direction_serial==11);
    ++retained_pause.native_avatar_generation; x=123; z=456;
    assert(!client_avatar_camera_direction(&current_witness,&current_roster,&current_scene,2,3,&x,&z));
    assert(x==0 && z==0);
    avatar_party_required=FALSE; native_party_exact=FALSE;
    client_seed=(SudekiMpLanStoryNativeRoster){.leader_character=2,.actors={[2]=&fixture_actor}};
    assert(native_anchor(&client_seed)==&fixture_actor); /* regular route needs no Dev owner */
    client_seed.leader_character=4; assert(!native_anchor(&client_seed));
}
static void seed_startup(void) {
    seed_camera_rosters();
    host_native_character=2; host_startup_target=1;
    host_startup_pending=TRUE; host_startup_requested=FALSE;
    memset(&host_startup_roster,0,sizeof(host_startup_roster));
    startup_roster_available=startup_select_ok=startup_ready=TRUE;
    startup_revoke_ready=FALSE; startup_select_calls=0;
    host_binding_ready=TRUE; runtime_ready=1;
}
static BOOL startup_step(BOOL complete) {
    return host_startup_select(current_roster.controller,&current_witness,&current_scene,complete);
}
static void startup_selection(void) {
    seed_startup(); startup_select_ok=FALSE;
    assert(startup_step(FALSE)); /* native veto/busy can progress normally */
    assert(!host_binding_ready && !runtime_ready && host_startup_pending && !host_startup_requested);
    assert(startup_select_calls==1 && !startup_step(TRUE));
    startup_select_ok=TRUE;
    assert(startup_step(FALSE) && host_startup_requested && startup_select_calls==2);
    assert(!startup_step(TRUE) && host_startup_pending);
    current_roster.leader_character=current_scene.leader_seat=1;
    assert(!startup_step(TRUE)); /* even a changed leader needs another dispatch */
    ++current_witness.dispatch_serial; current_roster.dispatch_serial=current_witness.dispatch_serial;
    current_roster.leader_character=current_scene.leader_seat=0; /* intermediate native rotation */
    assert(startup_step(FALSE) && startup_select_calls==2); /* adapter owns queued target/retries */
    assert(!startup_step(TRUE) && host_startup_pending && !host_binding_ready && !runtime_ready);
    current_roster.leader_character=current_scene.leader_seat=1; ++current_roster.revision;
    current_scene.revision=current_roster.revision;
    startup_ready=FALSE;
    assert(!startup_step(TRUE) && host_startup_pending); /* owned filter still retiring */
    startup_ready=TRUE;
    assert(startup_step(TRUE) && !host_startup_pending && host_native_character==1);
    assert(!host_binding_ready && !runtime_ready); /* caller still needs its own fresh admission proof */

    for(unsigned failure=0;failure<5u;++failure) {
        seed_startup();
        switch(failure) {
        case 0: startup_roster_available=FALSE; break;
        case 1: fresh_exact=FALSE; break;
        case 2: current_witness.service_post_original_exact=FALSE; break;
        case 3: current_roster.available_mask&=~2u; break;
        case 4: current_scene.phase=SUDEKIMP_LAN_STORY_LOADING; break;
        }
        assert(!startup_step(FALSE) && !startup_select_calls);
        assert(host_startup_pending && !host_startup_requested && !host_binding_ready && !runtime_ready);
    }
    /* A queued startup choice cannot migrate to another native party, even
     * when the adapter's Ready stub accepts the desired leader. */
    for(unsigned changed=0;changed<15u;++changed) {
        seed_startup(); assert(startup_step(FALSE));
        ++current_witness.dispatch_serial; current_roster.dispatch_serial=current_witness.dispatch_serial;
        current_roster.leader_character=current_scene.leader_seat=1;
        switch(changed) {
        case 0: ++current_roster.epoch; break;
        case 1: --current_roster.revision; break;
        case 2: current_roster.world=&camera_objects[12]; break;
        case 3: current_roster.descriptor=&camera_objects[12]; break;
        case 4: current_roster.group=&camera_objects[12]; break;
        case 5: current_roster.controller=&camera_objects[12]; break;
        case 6: current_roster.available_mask=7; break;
        case 7: case 8: case 9: case 10: current_roster.actors[changed-7]=&camera_objects[12]; break;
        case 11: case 12: case 13: case 14: current_roster.ai[changed-11]=&camera_objects[12]; break;
        }
        assert(!startup_step(FALSE) && !startup_step(TRUE));
        assert(host_startup_pending && startup_select_calls==1 && !host_binding_ready && !runtime_ready);
    }
    seed_startup(); assert(startup_step(FALSE));
    ++current_witness.dispatch_serial; current_roster.dispatch_serial=current_witness.dispatch_serial;
    current_roster.leader_character=current_scene.leader_seat=1; startup_revoke_ready=TRUE;
    assert(!startup_step(TRUE) && host_startup_pending); /* fresh proof revoked after Ready */
    host_startup_pending=FALSE; host_binding_ready=TRUE; runtime_ready=1;
    assert(host_startup_select(NULL,NULL,NULL,FALSE));
    assert(host_binding_ready && runtime_ready); /* no startup work changes existing routes */
}
int main(void) {
    party_readiness_loss(); readiness_loss(); drain_retries(); unknown_observation(); replacement_offer(); rejected_offers(); retained_camera_identity();
    typed_native_anchor(); startup_selection();
    puts("PASS: avatar runtime hold/drain/re-offer, paused direction pre/post guards and native host startup selection admission (synthetic)");
    return 0;
}
