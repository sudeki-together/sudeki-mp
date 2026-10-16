/* Actual runtime eligibility calculation with synthetic native lease proof.
 * No transport, input, game process, hooks or native gameplay calls execute.
 * The preceding service marks disconnected/replaced leases as draining; this
 * fixture tests the masks after that validation, not network acknowledgement.
 * Camera tests call the actual runtime callback with synthetic containment,
 * transport and native-camera spies; they do not execute game camera code. */
#include <windows.h>
static DWORD fixture_tick;
static DWORD fixture_get_tick_count(void) {return fixture_tick;}
#define GetTickCount fixture_get_tick_count
#include "../src/hooks/lan_story_runtime.c"
#undef GetTickCount
/* Exercise the actual scene/fence rules as well as the runtime policy. Their
 * private codec helpers have the same names in separate production units. */
#define put32 scene_test_put32
#define get32 scene_test_get32
#include "../src/network/lan_party_story.c"
#undef put32
#undef get32
#include "../src/network/lan_story_handoff.c"
#include <assert.h>
#include <stdio.h>

static SudekiMpControlUpdateDispatchWitness test_witness;
static SudekiMpLanStoryNativeRoster test_roster;
static SudekiMpLanPartyLease issued_keys[4];
static uint8_t actors[4],test_world;
static BOOL lease_exact[4];
static unsigned exact_calls;
static SudekiMpLanPartyPeerStatus test_peers[4];
static BOOL peer_known[4],control_acknowledged[4],menu_captures;
static unsigned assigned_character[4];
static BOOL exterior_bound,exterior_ready,exterior_steering;
static BOOL gap_direction_ok,gap_transport_ok;
static unsigned gap_direction_calls,gap_send_calls;
static SudekiMpLanStoryMovement gap_sent;
static unsigned eligibility_calls,eligible_mask;
static BOOL eligible_ready;
static SudekiMpLanStoryScene camera_remote,camera_native;
static SudekiMpLanStoryControlState camera_control;
static SudekiMpStoryCastView camera_selector;
static SudekiMpLanArenaOwnerViewLease camera_owner;
static BOOL camera_scene_known,camera_control_known,camera_selector_known;
static BOOL camera_roster_exact,camera_input_exact,camera_owner_exact;
static BOOL camera_continuity_ready,camera_tick_ok,camera_adopt_ok,camera_cast_active;
static BOOL camera_quick_menu,camera_main_menu,camera_world_recruiting,camera_client_recruiting;
static unsigned camera_tick_calls,camera_adopt_calls,camera_fail_adopt_call;
static float camera_last_dt,camera_look_x,camera_look_y,camera_last_look_x,camera_last_look_y;
static BOOL camera_look_ok;
BOOL SudekiMpLanPartyGetStoryScene(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyLease *lease,uint32_t now,SudekiMpLanStoryScene *out) {
    assert(s==session && lease && out && now==fixture_tick);
    if(!camera_scene_known) return FALSE;
    *out=camera_remote;return TRUE;
}
BOOL SudekiMpLanPartyGetStoryControl(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyLease *lease,uint32_t now,SudekiMpLanStoryControlState *out) {
    assert(s==session && lease && out && now==fixture_tick);
    if(!camera_control_known) return FALSE;
    *out=camera_control;return TRUE;
}
BOOL SudekiMpLanPartyGetStoryCastView(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyLease *lease,const SudekiMpLanStoryControlFence *fence,
    uint32_t now,uint32_t tick,SudekiMpStoryCastView *out) {
    assert(s==session && lease && fence && out && now==fixture_tick);
    /* The gap samples current scene time, never the old displayed frame. */
    assert(tick==camera_remote.observed_tick && tick!=presentation_sample.host_tick);
    if(!camera_selector_known) return FALSE;
    *out=camera_selector;return TRUE;
}
BOOL SudekiMpLanStoryClientRosterExact(const SudekiMpLanStoryNativeRoster *r) {
    return camera_roster_exact && r==&test_roster;
}
BOOL SudekiMpLanStoryInputExact(void *controller,void *actor) {
    return camera_input_exact && controller==test_roster.controller && actor==test_roster.actors[2];
}
BOOL SudekiMpLanStoryLocalControlViewOwner(SudekiMpLanArenaOwnerViewLease *out) {
    if(!camera_owner_exact) return FALSE;
    *out=camera_owner;return TRUE;
}
BOOL SudekiMpLanStoryLocalControlAdoptNative(const SudekiMpLanStoryNativeRoster *r,
    SudekiMpLanStoryLocalViewExact exact,void *context) {
    ++camera_adopt_calls;
    return camera_adopt_ok && camera_adopt_calls!=camera_fail_adopt_call &&
        exact && exact(r,context) && SudekiMpLanStoryInputExact(r->controller,r->actors[2]);
}
BOOL SudekiMpLanStoryClientCameraContinuityReady(const SudekiMpLanArenaOwnerViewLease *owner) {
    assert(owner && owner->camera==camera_owner.camera && owner->render_state==camera_owner.render_state);
    return camera_continuity_ready;
}
BOOL SudekiMpLanStoryClientCameraTick(const SudekiMpLanArenaOwnerViewLease *owner,
    float dt,float x,float y) {
    assert(owner && owner->camera==camera_owner.camera && owner->render_state==camera_owner.render_state);
    ++camera_tick_calls;camera_last_dt=dt;camera_last_look_x=x;camera_last_look_y=y;
    return camera_tick_ok && dt>0;
}
BOOL SudekiMpLanStoryInputLookAxes(float *x,float *y) {
    *x=camera_look_x;*y=camera_look_y;return camera_look_ok;
}
BOOL SudekiMpLanStoryLocalControlCastActive(void) {return camera_cast_active;}
BOOL SudekiMpLanStoryQuickMenuCapturesInput(void) {return camera_quick_menu;}
BOOL SudekiMpLanStoryMainMenuVisible(void) {return camera_main_menu;}
BOOL SudekiMpLanStoryWorldRecruiting(void) {return camera_world_recruiting;}
BOOL SudekiMpLanStoryClientRecruiting(void) {return camera_client_recruiting;}
void SudekiMpLanStorySplitSetEligibility(BOOL ready,uint8_t mask) {
    ++eligibility_calls;eligible_ready=ready;eligible_mask=mask;
}
BOOL SudekiMpLanStoryLocalControlDirection(void *controller,
    const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryNativeRoster *r,
    const SudekiMpLanStoryScene *scene,float x,float z,float *world_x,float *world_z) {
    assert(controller==test_roster.controller && w==&test_witness && r==&test_roster && scene);
    ++gap_direction_calls;
    if(!gap_direction_ok) return FALSE;
    *world_x=-z;*world_z=x;return TRUE;
}
BOOL SudekiMpLanPartySendStoryMovement(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyLease *peer,const SudekiMpLanStoryMovement *input) {
    assert(s==session && same_connection(peer,&client_control_connection));
    ++gap_send_calls;gap_sent=*input;return gap_transport_ok;
}

BOOL SudekiMpLanStoryHostControlExteriorNativeBound(const SudekiMpLanStoryNativeRoster *r,unsigned c) {
    return exterior_bound && c<4 && r->epoch==test_roster.epoch &&
        r->world==test_roster.world && r->actors[c]==test_roster.actors[c] &&
        c!=r->leader_character && (r->available_mask&(1u<<c));
}
BOOL SudekiMpLanStoryHostControlExteriorNativeReady(const SudekiMpLanStoryNativeRoster *r,unsigned c) {
    return exterior_ready && SudekiMpLanStoryHostControlExteriorNativeBound(r,c);
}

BOOL SudekiMpLanStoryHostControlExteriorNativeSteeringReady(const SudekiMpLanStoryNativeRoster *r,unsigned c) {
    return exterior_steering && SudekiMpLanStoryHostControlExteriorNativeBound(r,c);
}

void SudekiMpLogFormat(const char *format,...) {(void)format;}
BOOL SudekiMpLanStoryMenuCapturesInput(void) {return menu_captures;}
unsigned SudekiMpLanPartyPlayerCharacter(SudekiMpLanPartySession *s,unsigned player) {
    assert(s==session && player<4);return assigned_character[player];
}
BOOL SudekiMpLanPartyPeerStatusGet(SudekiMpLanPartySession *s,unsigned player,
    SudekiMpLanPartyPeerStatus *peer) {
    assert(s==session && player<4 && peer);
    if(!peer_known[player]) return FALSE;
    *peer=test_peers[player];return TRUE;
}
BOOL SudekiMpLanPartyStoryControlAcknowledged(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyLease *connection,const SudekiMpLanStoryControlFence *fence) {
    assert(s==session && connection && fence);
    for(unsigned p=1;p<4;++p)
        if(same_connection(connection,&test_peers[p].lease) && fence==&host_control[p].fence)
            return control_acknowledged[p];
    return FALSE;
}
BOOL SudekiMpLanStoryControlExact(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,const SudekiMpLanPartyLease *key) {
    ++exact_calls;
    /* Invalid character indices must be rejected before deriving a mask bit. */
    assert(key && key->seat<4u);
    if(w!=&test_witness || w->dispatch_serial!=31 || !w->service_post_original_exact ||
        r!=&test_roster || r->dispatch_serial!=31 || r->epoch!=9 || r->world!=&test_world ||
        !(r->available_mask&(1u<<key->seat)) || r->leader_character==key->seat ||
        r->actors[key->seat]!=&actors[key->seat]) return FALSE;
    for(unsigned p=1;p<4;++p)
        if(lease_exact[p] && key->token && key->generation &&
            key->token==issued_keys[p].token && key->generation==issued_keys[p].generation &&
            key->seat==issued_keys[p].seat) return TRUE;
    return FALSE;
}
static void fixture(unsigned available,unsigned leader) {
    memset(host_control,0,sizeof(host_control));memset(issued_keys,0,sizeof(issued_keys));
    memset(lease_exact,0,sizeof(lease_exact));memset(&test_witness,0,sizeof(test_witness));
    memset(&test_roster,0,sizeof(test_roster));exact_calls=0;
    memset(test_peers,0,sizeof(test_peers));memset(peer_known,0,sizeof(peer_known));
    memset(control_acknowledged,0,sizeof(control_acknowledged));
    memset(&story_presence,0,sizeof(story_presence));story_policy_initialized=FALSE;
    session=(SudekiMpLanPartySession *)&test_world;saved_profile=control_attempted=host_binding_ready=TRUE;
    runtime_thread=GetCurrentThreadId();local_seat=0;stopping=0;menu_captures=FALSE;
    split_attempted=FALSE;exterior_bound=exterior_ready=exterior_steering=FALSE;
    test_witness.dispatch_serial=31;test_witness.service_post_original_exact=TRUE;
    test_roster.dispatch_serial=31;test_roster.epoch=9;test_roster.world=&test_world;
    test_roster.available_mask=(uint8_t)available;test_roster.leader_character=(uint8_t)leader;
    for(unsigned c=0;c<4;++c) {
        assigned_character[c]=4;
        if(available&(1u<<c)) test_roster.actors[c]=&actors[c];
    }
}
static void acquire(unsigned player,unsigned character,BOOL ready) {
    assert(player>0 && player<4 && character<4);
    issued_keys[player]=(SudekiMpLanPartyLease){.token=100u+player,.generation=11u+player,
        .seat=(uint8_t)character};
    host_control[player].native_key=issued_keys[player];
    host_control[player].acquired=TRUE;host_control[player].ready=ready;
    host_control[player].transaction=5u+player;
    lease_exact[player]=TRUE;
    host_control[player].connection=(SudekiMpLanPartyLease){.token=100u+player,.generation=2,
        .seat=(uint8_t)player};
    test_peers[player].lease=host_control[player].connection;
    test_peers[player].phase=SUDEKIMP_LAN_PARTY_OBSERVING;
    test_peers[player].transport_confirmed=peer_known[player]=TRUE;
    assigned_character[player]=character;
}
static void masks(unsigned expected_activity,unsigned expected_split) {
    unsigned activity=~0u,split=~0u;
    unsigned char before[sizeof(host_control)];memcpy(before,host_control,sizeof(before));
    host_control_masks(&test_witness,&test_roster,&activity,&split);
    assert(activity==expected_activity && split==expected_split);
    assert(!(activity&~split));
    /* Deriving residency permission must never promote READY or modify input. */
    assert(!memcmp(before,host_control,sizeof(before)));
}
static void prepare_and_acknowledgement(void) {
    fixture(12,3);acquire(1,2,FALSE);
    masks(0,4);assert(exact_calls==1 && !host_control[1].ready);
    host_control[1].ready=TRUE;masks(4,4);
    host_control[1].ready=FALSE;masks(0,4);
    host_control[1].draining=TRUE;masks(0,0);
    puts("PASS PREPARE retains exterior eligibility without activity readiness; ACK and readiness loss preserve the same lease");
}
static void refused_leases(void) {
    fixture(12,3);acquire(1,2,TRUE);
    host_control[1].acquired=FALSE;masks(0,0);assert(!exact_calls);
    host_control[1].acquired=TRUE;lease_exact[1]=FALSE;masks(0,0);
    lease_exact[1]=TRUE;
    ++host_control[1].native_key.generation;masks(0,0);
    host_control[1].native_key=issued_keys[1];host_control[1].native_key.token=0;masks(0,0);
    host_control[1].native_key=issued_keys[1];host_control[1].native_key.seat=4;
    unsigned calls=exact_calls;masks(0,0);assert(exact_calls==calls);
    host_control[1].native_key.seat=255;masks(0,0);assert(exact_calls==calls);
    host_control[1].native_key=issued_keys[1];test_roster.actors[2]=&actors[1];masks(0,0);
    test_roster.actors[2]=&actors[2];test_roster.available_mask=8;masks(0,0);
    test_roster.available_mask=12;test_roster.leader_character=2;masks(0,0);
    test_roster.leader_character=3;test_witness.service_post_original_exact=FALSE;masks(0,0);
    test_witness.service_post_original_exact=TRUE;test_witness.dispatch_serial=32;masks(0,0);
    test_witness.dispatch_serial=31;test_roster.epoch=10;masks(0,0);
    test_roster.epoch=9;masks(4,4);
    /* Disconnect/connection replacement is rejected by the preceding service
     * loop, which marks this retained lease draining before masks are derived. */
    host_control[1].draining=TRUE;masks(0,0);
    puts("PASS failed acquisition, draining, stale key, invalid seat and unknown native identity cannot retain exterior eligibility");
}
static void multiple_clients(void) {
    fixture(15,3);acquire(1,2,TRUE);acquire(2,0,FALSE);acquire(3,1,TRUE);
    masks(6,7);
    lease_exact[1]=FALSE;masks(2,3);
    host_control[3].draining=TRUE;masks(0,1);
    host_control[2].ready=TRUE;masks(1,1);
    host_control[2].acquired=FALSE;masks(0,0);
    /* The host slot never contributes to the remote character masks. */
    host_control[0]=host_control[1];lease_exact[1]=TRUE;
    memset(&host_control[1],0,sizeof(host_control[1]));masks(0,0);
    puts("PASS sparse and full party masks use character seats, isolate each remote lease, and exclude the host slot");
}
static void settled_eligibility_prepass(void) {
    fixture(12,3);acquire(1,2,TRUE);split_attempted=TRUE;
    SudekiMpLanStoryScene scene={.epoch=9,.revision=5,.observed_tick=1000,
        .phase=SUDEKIMP_LAN_STORY_READY,.available_mask=12,.leader_seat=3,.inside_mask=8};
    strcpy(scene.world,"newbrightwater");strcpy(scene.temporary,"lnbr_kamo_shop");
    test_roster.revision=scene.revision;
    host_control[1].fence=(SudekiMpLanStoryControlFence){9,5,6,1,1,2};
    host_control[1].last_input_at=987;
    host_control[1].movement.world_x=0.5f;host_control[1].movement.world_z=0.25f;
    assert(SudekiMpLanStorySceneValid(&scene));
    unsigned char original[sizeof(host_control)];memcpy(original,host_control,sizeof(original));
    eligible_mask=0;eligible_ready=TRUE;eligibility_calls=0;
#define PREPASS(mask) do { \
    refresh_split_eligibility(&test_witness,&scene,&test_roster); \
    assert(eligible_mask==(mask)); \
} while(0)
    /* First exact settled dispatch restores the retained outside bit without
     * waiting for input service to send a neutral packet and publish it later. */
    PREPASS(4);assert(eligibility_calls==1 && !eligible_ready);
    assert(!memcmp(original,host_control,sizeof(original)));
    host_control[1].ready=FALSE;PREPASS(4);assert(!host_control[1].ready);
    host_control[1].ready=TRUE;
    refresh_split_eligibility(&test_witness,&scene,NULL);assert(!eligible_mask && !eligible_ready);
    PREPASS(4);
    refresh_split_eligibility(&test_witness,NULL,&test_roster);assert(!eligible_mask && !eligible_ready);
    SudekiMpLanStoryScene loading={.epoch=10,.revision=6,.observed_tick=1001,
        .phase=SUDEKIMP_LAN_STORY_LOADING,.leader_seat=SUDEKIMP_LAN_STORY_NO_SEAT};
    assert(SudekiMpLanStorySceneValid(&loading));
    refresh_split_eligibility(&test_witness,&loading,&test_roster);assert(!eligible_mask && !eligible_ready);
    ++scene.epoch;PREPASS(0);--scene.epoch;
    test_roster.world=NULL;PREPASS(0);test_roster.world=&test_world;
    PREPASS(4);
    peer_known[1]=FALSE;PREPASS(0);peer_known[1]=TRUE;
    test_peers[1].transport_confirmed=FALSE;PREPASS(0);test_peers[1].transport_confirmed=TRUE;
    test_peers[1].phase=SUDEKIMP_LAN_PARTY_PENDING;PREPASS(0);
    test_peers[1].phase=SUDEKIMP_LAN_PARTY_OBSERVING;
    ++test_peers[1].lease.generation;PREPASS(0);--test_peers[1].lease.generation;
    host_control[1].acquired=FALSE;PREPASS(0);host_control[1].acquired=TRUE;
    host_control[1].draining=TRUE;PREPASS(0);host_control[1].draining=FALSE;
    assigned_character[1]=0;PREPASS(0);assigned_character[1]=2;
    ++host_control[1].native_key.generation;PREPASS(0);--host_control[1].native_key.generation;
    host_control[1].native_key.seat=255;PREPASS(0);host_control[1].native_key.seat=2;
    ++host_control[1].fence.epoch;PREPASS(0);--host_control[1].fence.epoch;
    host_control[1].fence.character=0;PREPASS(0);host_control[1].fence.character=2;
    host_control[1].fence.player=2;PREPASS(0);host_control[1].fence.player=1;
    lease_exact[1]=FALSE;PREPASS(0);lease_exact[1]=TRUE;
    ++test_roster.revision;PREPASS(0);assert(!eligible_ready);--test_roster.revision;
    test_witness.service_post_original_exact=FALSE;PREPASS(0);
    test_witness.service_post_original_exact=TRUE;
    story_policy_initialized=TRUE;story_presence.ownership.phase=SUDEKIMP_PARTY_SWAP_HANDOFF;
    story_presence.ownership.player=1;PREPASS(0);
    story_presence.ownership.player=0;PREPASS(0);story_policy_initialized=FALSE;
    PREPASS(4);
    /* Ordinary-world eligibility also uses this fresh proof; a missing peer
     * cannot gain a bit from retained fields before the later drain loop. */
    scene.inside_mask=0;memset(scene.temporary,0,sizeof(scene.temporary));
    PREPASS(4);assert(eligible_ready);
    peer_known[1]=FALSE;PREPASS(0);peer_known[1]=TRUE;
    unsigned calls=eligibility_calls;stopping=1;PREPASS(0);assert(eligibility_calls==calls);stopping=0;
    local_seat=1;PREPASS(0);assert(eligibility_calls==calls);local_seat=0;
    PREPASS(4);
    assert(host_control[1].last_input_at==987 && host_control[1].movement.world_x==0.5f &&
        host_control[1].movement.world_z==0.25f);
#undef PREPASS
    puts("PASS named SettledEligibilityPrepass first settled dispatch, unknown-roster recovery, PREPARE retention and fresh peer/fence/native/swap refusals; no movement or readiness writes");
}
static void lease_permissions(BOOL ai,BOOL input) {
    assert(!!SudekiMpLanStoryRuntimeAiLeaseExact(&issued_keys[1])==!!ai);
    assert(!!SudekiMpLanStoryRuntimeTurnLeaseExact(&issued_keys[1])==!!input);
}
static DWORD WINAPI foreign_thread(void *unused) {
    (void)unused;lease_permissions(FALSE,FALSE);return 0;
}
static void ai_reservation_without_input(void) {
    fixture(12,3);acquire(1,2,FALSE);
    lease_permissions(TRUE,FALSE);masks(0,4);
    host_control[1].ready=TRUE;lease_permissions(TRUE,FALSE);
    control_acknowledged[1]=TRUE;lease_permissions(TRUE,TRUE);masks(4,4);
    host_control[1].ready=FALSE;lease_permissions(TRUE,FALSE);masks(0,4);
    host_control[1].ready=TRUE;

    host_control[1].draining=TRUE;lease_permissions(FALSE,FALSE);masks(0,0);
    host_control[1].draining=FALSE;host_control[1].acquired=FALSE;lease_permissions(FALSE,FALSE);
    host_control[1].acquired=TRUE;
    ++host_control[1].native_key.generation;lease_permissions(FALSE,FALSE);
    host_control[1].native_key=issued_keys[1];
    ++test_peers[1].lease.generation;lease_permissions(FALSE,FALSE);
    test_peers[1].lease=host_control[1].connection;
    peer_known[1]=FALSE;lease_permissions(FALSE,FALSE);peer_known[1]=TRUE;
    test_peers[1].transport_confirmed=FALSE;lease_permissions(FALSE,FALSE);
    test_peers[1].transport_confirmed=TRUE;
    test_peers[1].phase=0;lease_permissions(FALSE,FALSE);
    test_peers[1].phase=SUDEKIMP_LAN_PARTY_OBSERVING;
    assigned_character[1]=0;lease_permissions(FALSE,FALSE);assigned_character[1]=2;
    menu_captures=TRUE;lease_permissions(FALSE,FALSE);menu_captures=FALSE;
    host_binding_ready=FALSE;lease_permissions(FALSE,FALSE);host_binding_ready=TRUE;
    story_policy_initialized=TRUE;story_presence.ownership.phase=SUDEKIMP_PARTY_SWAP_HANDOFF;
    story_presence.ownership.player=1;lease_permissions(FALSE,FALSE);
    story_presence.ownership.player=0;lease_permissions(FALSE,FALSE);
    story_presence.ownership.player=2;lease_permissions(TRUE,TRUE);
    story_policy_initialized=FALSE;
    stopping=1;lease_permissions(FALSE,FALSE);stopping=0;
    local_seat=1;lease_permissions(FALSE,FALSE);local_seat=0;
    saved_profile=FALSE;lease_permissions(FALSE,FALSE);saved_profile=TRUE;
    control_attempted=FALSE;lease_permissions(FALSE,FALSE);control_attempted=TRUE;
    HANDLE worker=CreateThread(NULL,0,foreign_thread,NULL,0,NULL);assert(worker);
    assert(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0);CloseHandle(worker);
    assert(!SudekiMpLanStoryRuntimeAiLeaseExact(NULL) && !SudekiMpLanStoryRuntimeTurnLeaseExact(NULL));
    lease_permissions(TRUE,TRUE);
    puts("PASS acquired PREPARE permits AI suppression while player turn still requires READY plus ACK");
    puts("PASS AI and input lease gates reject draining, stale peer/key, wrong thread, swap, menu and host-binding mismatch");
}
static void exterior_shop_permissions(void) {
    fixture(12,3);acquire(1,2,TRUE);control_acknowledged[1]=TRUE;
    host_control[1].fence.epoch=test_roster.epoch;
    host_control[1].exterior_roster=test_roster;
    host_binding_ready=FALSE;split_attempted=TRUE;exterior_bound=TRUE;
    /* A native body pause retains AI suppression, not player steering. */
    lease_permissions(TRUE,FALSE);assert(!remote_input_ready(1));
    assert(!remote_steering_ready(1));
    /* Once the native actor resumes, steering no longer waits for the host's
     * modal state. Actions still require their own positive admission. */
    exterior_steering=TRUE;
    lease_permissions(TRUE,TRUE);assert(remote_steering_ready(1));
    assert(!remote_input_ready(1));
    assert(!remote_action_ready(1,SUDEKIMP_STORY_ACTION_USE_ITEM));
    assert(!remote_action_ready(1,SUDEKIMP_STORY_ACTION_SKILL));
    assert(!remote_action_ready(1,SUDEKIMP_STORY_ACTION_SPIRIT));
    assert(!remote_action_ready(1,SUDEKIMP_STORY_ACTION_EQUIP_WEAPON));
    assert(!remote_action_ready(1,SUDEKIMP_STORY_ACTION_MELEE));
    exterior_ready=TRUE;lease_permissions(TRUE,TRUE);assert(remote_input_ready(1));
    assert(remote_action_ready(1,SUDEKIMP_STORY_ACTION_USE_ITEM));
    exterior_steering=FALSE;lease_permissions(TRUE,FALSE);
    assert(!remote_steering_ready(1)); /* Native pause cannot be overridden by a known shop. */
    exterior_steering=TRUE;
    assert(!host_binding_ready); /* The host's shop filter is never promoted. */
    host_control[1].ready=FALSE;lease_permissions(TRUE,FALSE);
    host_control[1].ready=TRUE;control_acknowledged[1]=FALSE;lease_permissions(TRUE,FALSE);
    control_acknowledged[1]=TRUE;
    /* Positive per-use native proof is required after the dispatch snapshot. */
    exterior_bound=FALSE;lease_permissions(FALSE,FALSE);assert(!remote_input_ready(1));
    assert(!remote_steering_ready(1));
    exterior_bound=TRUE;split_attempted=FALSE;lease_permissions(FALSE,FALSE);split_attempted=TRUE;
    ++host_control[1].fence.epoch;lease_permissions(FALSE,FALSE);--host_control[1].fence.epoch;
    host_control[1].exterior_roster.world=&actors[0];lease_permissions(FALSE,FALSE);
    host_control[1].exterior_roster=test_roster;
    host_control[1].exterior_roster.actors[2]=&actors[0];lease_permissions(FALSE,FALSE);
    host_control[1].exterior_roster=test_roster;
    host_control[1].draining=TRUE;lease_permissions(FALSE,FALSE);host_control[1].draining=FALSE;
    menu_captures=TRUE;lease_permissions(FALSE,FALSE);menu_captures=FALSE;
    peer_known[1]=FALSE;lease_permissions(FALSE,FALSE);peer_known[1]=TRUE;
    ++test_peers[1].lease.generation;lease_permissions(FALSE,FALSE);
    test_peers[1].lease=host_control[1].connection;
    assigned_character[1]=0;lease_permissions(FALSE,FALSE);assigned_character[1]=2;
    host_control[1].acquired=FALSE;lease_permissions(FALSE,FALSE);host_control[1].acquired=TRUE;
    assert(!exterior_control_allowed(0,EXTERIOR_ACTION) && !exterior_control_allowed(4,EXTERIOR_ACTION));
    assert(!exterior_control_allowed(0,EXTERIOR_STEERING) && !exterior_control_allowed(4,EXTERIOR_STEERING));
    lease_permissions(TRUE,TRUE);
    puts("PASS settled exterior steering is independent of host modal readiness; actions, native pause, READY/ACK and current split proof remain gated");
}
static void client_revision_continuity(void) {
    fixture(12,3);acquire(1,2,TRUE);
    local_seat=1;local_control_attempted=client_switch_prepared=client_local_selected=TRUE;
    client_control_trace_state=5;client_input_sequence=7;
    client_revision_gap=TRUE;presentation_changed=FALSE;
    memset(&presentation_sample,0,sizeof(presentation_sample));
    SudekiMpLanStoryScene prior={.epoch=9,.revision=5,.observed_tick=1000,
        .phase=SUDEKIMP_LAN_STORY_READY,.available_mask=12,.leader_seat=3};
    strcpy(prior.world,"newbrightwater");
    SudekiMpLanStoryScene remote=prior,native=prior;
    remote.revision=6;remote.observed_tick=1010;remote.inside_mask=8;
    strcpy(remote.temporary,"lnbr_kamo_shop");
    native.epoch=77;native.revision=3;native.leader_seat=2;
    static uint8_t descriptor,group,controller,ai[4];
    test_roster.epoch=native.epoch;test_roster.revision=native.revision;test_roster.leader_character=2;
    test_roster.descriptor=&descriptor;test_roster.group=&group;test_roster.controller=&controller;
    test_roster.ai[2]=&ai[2];test_roster.ai[3]=&ai[3];
    SudekiMpLanPartyPeerStatus peer=test_peers[1];
    SudekiMpLanStoryControlState state={.fence={9,5,6,42,1,2},
        .observed_tick=1010,.phase=SUDEKIMP_STORY_CONTROL_READY};
    client_control_connection=peer.lease;client_control_fence=state.fence;
    presented_epoch=9;presented_generations[2]=42;
    presentation_sample.scene=prior;presentation_sample.lease=peer.lease;
    presentation_sample.native_roster=test_roster;presentation_sample.sequence=100;
    presentation_sample.host_tick=990;presentation_sample.receipt=1000;
    presentation_sample.valid=effects_ready=client_input_ready=FALSE;runtime_ready=0;
    unsigned char original[sizeof(presentation_sample)];memcpy(original,&presentation_sample,sizeof(original));
#define GAP(t) client_revision_gap_matches(&test_roster,&native,&peer,&remote,&state,(t))
    assert(GAP(1050) && GAP(1133) && GAP(1249) && GAP(1250));
    assert(!GAP(1251) && !GAP(999));
    /* More revision notifications cannot renew the last complete pair's age. */
    remote.revision=7;assert(GAP(1250) && !GAP(1251));remote.revision=6;
    assert(!memcmp(original,&presentation_sample,sizeof(original)));
    assert(!presentation_sample.valid && !effects_ready && !client_input_ready && !runtime_ready);
    /* Host returning outside is equally continuous for the exterior client. */
    presentation_sample.scene=remote;remote=prior;remote.revision=7;remote.observed_tick=1020;
    assert(GAP(1100));
    remote=presentation_sample.scene;presentation_sample.scene=prior;

    state.phase=SUDEKIMP_STORY_CONTROL_PREPARE;assert(!GAP(1050));state.phase=SUDEKIMP_STORY_CONTROL_READY;
    client_control_trace_state=3;assert(!GAP(1050));client_control_trace_state=5;
    client_input_sequence=0;assert(!GAP(1050));client_input_sequence=7;
    client_switch_prepared=FALSE;assert(!GAP(1050));client_switch_prepared=TRUE;
    client_local_selected=FALSE;assert(!GAP(1050));client_local_selected=TRUE;
    client_revision_gap=FALSE;assert(!GAP(1050));client_revision_gap=TRUE;
    presentation_sample.valid=TRUE;assert(!GAP(1050));presentation_sample.valid=FALSE;
    presentation_changed=TRUE;assert(!GAP(1050));presentation_changed=FALSE;
    stopping=1;assert(!GAP(1050));stopping=0;
    remote.inside_mask=12;assert(!GAP(1050));remote.inside_mask=8;
    native.inside_mask=4;strcpy(native.temporary,"another_room");assert(!GAP(1050));
    native.inside_mask=0;memset(native.temporary,0,sizeof(native.temporary));
    ++remote.epoch;assert(!GAP(1050));--remote.epoch;
    strcpy(remote.world,"another_world");assert(!GAP(1050));strcpy(remote.world,"newbrightwater");
    remote.revision=prior.revision;assert(!GAP(1050));remote.revision=6;
    remote.available_mask=8;assert(!GAP(1050));remote.available_mask=12;
    remote.leader_seat=2;assert(!GAP(1050));remote.leader_seat=3;
    ++state.fence.transaction;assert(!GAP(1050));--state.fence.transaction;
    ++state.fence.actor_generation;assert(!GAP(1050));--state.fence.actor_generation;
    ++presented_generations[2];assert(!GAP(1050));--presented_generations[2];
    ++peer.lease.generation;assert(!GAP(1050));--peer.lease.generation;
    ++peer.lease.token;assert(!GAP(1050));--peer.lease.token;
    peer.lease.seat=2;assert(!GAP(1050));peer.lease.seat=1;
    peer.transport_confirmed=FALSE;assert(!GAP(1050));peer.transport_confirmed=TRUE;
    peer.phase=SUDEKIMP_LAN_PARTY_PENDING;assert(!GAP(1050));peer.phase=SUDEKIMP_LAN_PARTY_OBSERVING;
    ++test_roster.epoch;assert(!GAP(1050));--test_roster.epoch;
    ++test_roster.revision;assert(!GAP(1050));--test_roster.revision;
    test_roster.leader_character=3;assert(!GAP(1050));test_roster.leader_character=2;
    test_roster.available_mask=8;assert(!GAP(1050));test_roster.available_mask=12;
    void **identities[]={&test_roster.world,&test_roster.descriptor,&test_roster.group,
        &test_roster.controller,&test_roster.actors[2],&test_roster.actors[3],&test_roster.ai[2]};
    for(unsigned i=0;i<sizeof(identities)/sizeof(identities[0]);++i) {
        void *saved=*identities[i];*identities[i]=NULL;assert(!GAP(1050));*identities[i]=saved;
    }
    assert(GAP(1050) && !memcmp(original,&presentation_sample,sizeof(original)));
    /* Exercise the actual gap sender: each new local sample is transformed,
     * original frame identity is preserved, and no presentation becomes ready. */
    gap_direction_ok=gap_transport_ok=TRUE;gap_direction_calls=gap_send_calls=0;
    client_input_sent_at=0;
#define SEND_GAP(t,x,z) client_revision_gap_send_motion(test_roster.controller,&test_witness, \
    &test_roster,&native,&peer.lease,&state.fence,(t),(x),(z))
    assert(SEND_GAP(1050,0.5f,0.25f));
    assert(gap_sent.sequence==8 && gap_sent.acknowledged_frame==100 &&
        gap_sent.world_x==-0.25f && gap_sent.world_z==0.5f &&
        SudekiMpLanStoryControlFenceSame(&gap_sent.fence,&state.fence));
    assert(!SEND_GAP(1064,0,0) && gap_send_calls==1 && gap_direction_calls==1);
    assert(SEND_GAP(1065,0,0) && gap_sent.sequence==9 &&
        gap_sent.world_x==0 && gap_sent.world_z==0); /* release is fresh too */
    gap_direction_ok=FALSE;
    assert(!SEND_GAP(1080,1,0) && gap_send_calls==2 && client_input_sequence==9);
    gap_direction_ok=TRUE;gap_transport_ok=FALSE;
    assert(!SEND_GAP(1080,1,0) && gap_send_calls==3 && client_input_sent_at==1065);
    gap_transport_ok=TRUE;
    assert(SEND_GAP(1081,-0.5f,0.5f) && gap_sent.sequence==11 &&
        gap_sent.world_x==-0.5f && gap_sent.world_z==-0.5f);
    client_input_sequence=UINT32_MAX;
    assert(!SEND_GAP(1100,1,0) && gap_send_calls==4);
    assert(!memcmp(original,&presentation_sample,sizeof(original)) &&
        !presentation_sample.valid && !effects_ready && !client_input_ready && !runtime_ready);
#undef SEND_GAP
#undef GAP
    puts("PASS established client binding survives same-area entry/exit pair gaps without presenting or refreshing the original 250 ms horizon");
    puts("PASS initial PREPARE, own-area/world/epoch/roster/actor/fence changes and stale connections cannot retain client binding");
    puts("PASS named ClientRevisionGapMotion fresh direction/release, cadence, failed mapping/send and sequence exhaustion; no media/readiness promotion");
}
static void camera_fixture(void) {
    fixture(12,3);acquire(1,2,TRUE);
    local_seat=1;local_control_attempted=client_switch_prepared=client_local_selected=TRUE;
    client_control_trace_state=5;client_input_sequence=7;client_revision_gap=TRUE;
    presentation_changed=FALSE;client_camera_attempted=TRUE;main_menu_attempted=TRUE;
    host_cinematic_view=FALSE;memset(&presented_cast_view,0,sizeof(presented_cast_view));
    memset(&presentation_sample,0,sizeof(presentation_sample));
    memset(&native_camera_clock,0,sizeof(native_camera_clock));
    memset(&client_catchup,0,sizeof(client_catchup));client_catchup_seeded=FALSE;
    SudekiMpLanStoryScene prior={.epoch=9,.revision=5,.observed_tick=1000,
        .phase=SUDEKIMP_LAN_STORY_READY,.available_mask=12,.leader_seat=3};
    strcpy(prior.world,"newbrightwater");
    camera_remote=camera_native=prior;
    camera_remote.revision=6;camera_remote.observed_tick=1010;camera_remote.inside_mask=8;
    strcpy(camera_remote.temporary,"lnbr_kamo_shop");
    camera_native.epoch=77;camera_native.revision=3;camera_native.leader_seat=2;
    static uint8_t descriptor,group,controller,ai[4],owners[6];
    test_roster.epoch=77;test_roster.revision=3;test_roster.leader_character=2;
    test_roster.descriptor=&descriptor;test_roster.group=&group;test_roster.controller=&controller;
    test_roster.ai[2]=&ai[2];test_roster.ai[3]=&ai[3];
    camera_control=(SudekiMpLanStoryControlState){.fence={9,5,6,42,1,2},
        .observed_tick=1010,.phase=SUDEKIMP_STORY_CONTROL_READY};
    client_control_connection=test_peers[1].lease;client_control_fence=camera_control.fence;
    presented_epoch=9;presented_generations[2]=42;
    presentation_sample.scene=prior;presentation_sample.lease=test_peers[1].lease;
    presentation_sample.native_roster=test_roster;presentation_sample.sequence=100;
    presentation_sample.host_tick=990;presentation_sample.receipt=1000;
    presentation_sample.valid=effects_ready=client_input_ready=FALSE;runtime_ready=0;
    camera_owner=(SudekiMpLanArenaOwnerViewLease){.camera_mode=&owners[0],.camera=&owners[1],
        .render_state=&owners[2],.scene_manager=&owners[3],.scene_renderer=&owners[4],
        .scene_render_state_slot=(void **)&owners[5],.valid=TRUE};
    camera_selector=(SudekiMpStoryCastView){.fence=camera_control.fence,.stream=1,
        .sequence=1,.tick=1010,.revision=6};
    camera_scene_known=camera_control_known=camera_selector_known=TRUE;
    camera_roster_exact=camera_input_exact=camera_owner_exact=TRUE;
    camera_continuity_ready=camera_tick_ok=camera_adopt_ok=camera_look_ok=TRUE;
    camera_cast_active=camera_quick_menu=camera_main_menu=FALSE;
    camera_world_recruiting=camera_client_recruiting=FALSE;
    camera_tick_calls=camera_adopt_calls=camera_fail_adopt_call=0;
    camera_look_x=0.25f;camera_look_y=-0.5f;
    camera_last_dt=camera_last_look_x=camera_last_look_y=0;
    dialogue_count=1;memset(dialogue_history,0,sizeof(dialogue_history));
    dialogue_history[0]=(SudekiMpLanStoryPresentation){.epoch=9,.revision=6,.host_tick=1010,.sequence=1};
    dialogue_receipts[0]=1000;
    fixture_tick=1000;
    /* An ordinary presentation establishes the camera clock. Native Tick
     * rejects its zero-duration first sample, then advances on the next one. */
    assert(!present_native_camera(&test_roster,fixture_tick,0,0));
    assert(camera_tick_calls==1 && camera_last_dt==0);
    fixture_tick=1016;assert(present_native_camera(&test_roster,fixture_tick,0,0));
    assert(camera_tick_calls==2 && fabsf(camera_last_dt-0.016f)<0.00001f);
}
static BOOL camera_gap_at(uint32_t now) {
    fixture_tick=now;
    return client_revision_gap_camera(&test_roster,&camera_native,NULL);
}
static void camera_continuity_clock(void) {
    camera_fixture();
    unsigned char sample[sizeof(presentation_sample)],selector[sizeof(presented_cast_view)];
    unsigned char dialogue[sizeof(dialogue_history)],roster[sizeof(test_roster)];
    memcpy(sample,&presentation_sample,sizeof(sample));memcpy(selector,&presented_cast_view,sizeof(selector));
    memcpy(dialogue,dialogue_history,sizeof(dialogue));memcpy(roster,&test_roster,sizeof(roster));
    SudekiMpLanStoryControlFence fence=client_control_fence;
    unsigned sequence=client_input_sequence,sends=gap_send_calls;
    assert(camera_gap_at(1032));
    assert(camera_tick_calls==3 && fabsf(camera_last_dt-0.016f)<0.00001f);
    assert(camera_last_look_x==0.25f && camera_last_look_y==-0.5f);
    unsigned adopts=camera_adopt_calls;
    assert(camera_gap_at(1032) && camera_tick_calls==3 && camera_adopt_calls>adopts);
    assert(camera_gap_at(1048) && camera_tick_calls==4);
    assert(fabsf(camera_last_dt-0.016f)<0.00001f);
    /* A current full frame resumes through the same helper and clock. This
     * fixture does not replay a full world/resource frame to test the camera. */
    fixture_tick=1064;
    assert(present_native_camera(&test_roster,fixture_tick,0,0) && camera_tick_calls==5);
    assert(fabsf(camera_last_dt-0.016f)<0.00001f);
    assert(!memcmp(sample,&presentation_sample,sizeof(sample)) &&
        !memcmp(selector,&presented_cast_view,sizeof(selector)) &&
        !memcmp(dialogue,dialogue_history,sizeof(dialogue)) && !memcmp(roster,&test_roster,sizeof(roster)));
    assert(SudekiMpLanStoryControlFenceSame(&fence,&client_control_fence));
    assert(client_input_sequence==sequence && gap_send_calls==sends &&
        !presentation_sample.valid && !effects_ready && !client_input_ready && !runtime_ready);
    /* A failed native call cannot be retried at the same frame timestamp. */
    camera_fixture();camera_tick_ok=FALSE;
    assert(!camera_gap_at(1032) && camera_tick_calls==3 && !native_camera_clock.applied);
    camera_tick_ok=TRUE;assert(!camera_gap_at(1032) && camera_tick_calls==3);
    assert(camera_gap_at(1048) && camera_tick_calls==4 && fabsf(camera_last_dt-0.016f)<0.00001f);
    /* A failed post-native ownership check is a failed publication too. */
    camera_fixture();camera_fail_adopt_call=camera_adopt_calls+3;
    assert(!camera_gap_at(1032) && camera_tick_calls==3 && !native_camera_clock.applied);
    camera_fail_adopt_call=0;assert(!camera_gap_at(1032) && camera_tick_calls==3);
    assert(camera_gap_at(1048) && camera_tick_calls==4);
    puts("PASS named RevisionGapCameraClock: ordinary-gap-ordinary keeps 16 ms, duplicate and failed ticks do not replay; no receipt, media, fence or readiness promotion");
}
static DWORD WINAPI camera_foreign_thread(void *unused) {
    (void)unused;
    assert(!client_revision_gap_camera(&test_roster,&camera_native,NULL));return 0;
}
static void camera_continuity_refusals(void) {
    camera_fixture();fixture_tick=1032;
#define CAMERA_REFUSES(change,restore) do { \
    unsigned before=camera_tick_calls;change; \
    assert(!client_revision_gap_camera(&test_roster,&camera_native,NULL)); \
    assert(camera_tick_calls==before);restore; \
} while(0)
    CAMERA_REFUSES(host_cinematic_view=TRUE,host_cinematic_view=FALSE);
    CAMERA_REFUSES(presented_cast_view.kind=1,presented_cast_view.kind=0);
    CAMERA_REFUSES(camera_cast_active=TRUE,camera_cast_active=FALSE);
    CAMERA_REFUSES(menu_captures=TRUE,menu_captures=FALSE);
    CAMERA_REFUSES(camera_quick_menu=TRUE,camera_quick_menu=FALSE);
    CAMERA_REFUSES(camera_main_menu=TRUE,camera_main_menu=FALSE);
    CAMERA_REFUSES(camera_world_recruiting=TRUE,camera_world_recruiting=FALSE);
    CAMERA_REFUSES(camera_client_recruiting=TRUE,camera_client_recruiting=FALSE);
    CAMERA_REFUSES(camera_scene_known=FALSE,camera_scene_known=TRUE);
    CAMERA_REFUSES(camera_control_known=FALSE,camera_control_known=TRUE);
    CAMERA_REFUSES(peer_known[1]=FALSE,peer_known[1]=TRUE);
    CAMERA_REFUSES(++test_peers[1].lease.generation,--test_peers[1].lease.generation);
    CAMERA_REFUSES(camera_roster_exact=FALSE,camera_roster_exact=TRUE);
    CAMERA_REFUSES(camera_input_exact=FALSE,camera_input_exact=TRUE);
    CAMERA_REFUSES(camera_owner_exact=FALSE,camera_owner_exact=TRUE);
    CAMERA_REFUSES(camera_owner.valid=FALSE,camera_owner.valid=TRUE);
    CAMERA_REFUSES(camera_selector_known=FALSE,camera_selector_known=TRUE);
    CAMERA_REFUSES(camera_selector.kind=3,camera_selector.kind=0);
    CAMERA_REFUSES(camera_selector.view.valid=1,camera_selector.view.valid=0);
    CAMERA_REFUSES(--camera_selector.revision,++camera_selector.revision);
    CAMERA_REFUSES(++camera_selector.fence.transaction,--camera_selector.fence.transaction);
    CAMERA_REFUSES(dialogue_count=0,dialogue_count=1);
    CAMERA_REFUSES(dialogue_receipts[0]=700,dialogue_receipts[0]=1000);
    CAMERA_REFUSES(--dialogue_history[0].revision,++dialogue_history[0].revision);
    CAMERA_REFUSES(++dialogue_history[0].epoch,--dialogue_history[0].epoch);
    CAMERA_REFUSES(dialogue_history[0].flags=SUDEKIMP_STORY_LETTERBOX_PRESENT,dialogue_history[0].flags=0);
    CAMERA_REFUSES(camera_adopt_ok=FALSE,camera_adopt_ok=TRUE);
    CAMERA_REFUSES(camera_continuity_ready=FALSE,camera_continuity_ready=TRUE);
    CAMERA_REFUSES(camera_control.phase=SUDEKIMP_STORY_CONTROL_PREPARE,camera_control.phase=SUDEKIMP_STORY_CONTROL_READY);
    CAMERA_REFUSES(camera_remote.inside_mask=12,camera_remote.inside_mask=8);
    CAMERA_REFUSES(presentation_sample.receipt=700,presentation_sample.receipt=1000);
    CAMERA_REFUSES(presentation_sample.valid=TRUE,presentation_sample.valid=FALSE);
    CAMERA_REFUSES(client_revision_gap=FALSE,client_revision_gap=TRUE);
    CAMERA_REFUSES(client_camera_attempted=FALSE,client_camera_attempted=TRUE);
    CAMERA_REFUSES(stopping=1,stopping=0);
    void **identities[]={&camera_owner.camera_mode,&camera_owner.camera,&camera_owner.render_state,
        &camera_owner.scene_manager,&camera_owner.scene_renderer};
    for(unsigned i=0;i<sizeof(identities)/sizeof(identities[0]);++i) {
        void *saved=*identities[i];
        CAMERA_REFUSES(*identities[i]=&test_world,*identities[i]=saved);
    }
    void **slot=camera_owner.scene_render_state_slot;
    CAMERA_REFUSES(camera_owner.scene_render_state_slot=NULL,camera_owner.scene_render_state_slot=slot);
    HANDLE thread=CreateThread(NULL,0,camera_foreign_thread,NULL,0,NULL);assert(thread);
    unsigned before=camera_tick_calls;
    assert(WaitForSingleObject(thread,5000)==WAIT_OBJECT_0);CloseHandle(thread);
    assert(camera_tick_calls==before);
    assert(camera_gap_at(1032));
    /* Rechecking a duplicate must still refuse a changed native binding. */
    CAMERA_REFUSES(camera_input_exact=FALSE,camera_input_exact=TRUE);
    CAMERA_REFUSES(camera_continuity_ready=FALSE,camera_continuity_ready=TRUE);
    assert(!camera_gap_at(1251)); /* original pair receipt, never renewed */
    assert(!presentation_sample.valid && !effects_ready && !client_input_ready && !runtime_ready);
#undef CAMERA_REFUSES
    puts("PASS named RevisionGapCameraAdmission: current normal selector/dialogue required; cast, menus, recruitment, stale identities, replaced owner, foreign thread and original age refuse before native tick");
}
int main(void) {
    prepare_and_acknowledgement();refused_leases();multiple_clients();ai_reservation_without_input();
    settled_eligibility_prepass();exterior_shop_permissions();client_revision_continuity();
    camera_continuity_clock();camera_continuity_refusals();
    puts("PASS actual runtime split/activity mask policy; native AI suppression and live TEMP remain separate evidence");
    return 0;
}
