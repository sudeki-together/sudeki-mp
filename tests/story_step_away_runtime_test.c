/* Actual runtime intent/mailbox/M routing; native and transport spies only. */
#include <windows.h>
static DWORD tick=1000;
static DWORD clock_now(void) { return tick; }
#define GetTickCount clock_now
#include "../src/hooks/lan_story_runtime.c"
#undef GetTickCount
#include "../src/network/lan_story_step_away.c"
#include "../src/engine/party_ownership.c"
#define get32 handoff_get32
#define put32 handoff_put32
#include "../src/network/lan_story_handoff.c"
#undef get32
#undef put32
#include "../src/network/lan_party_story.c"
#include <assert.h>
static SudekiMpLanPartyPresence remote_presence;
static SudekiMpLanPartyPeerStatus remote_peer;
static SudekiMpLanStoryScene remote_scene;
static BOOL have_presence=TRUE,have_scene=TRUE,raw_m,esc,quick,main_shown,shop_shown,examine_shown,queue_ok=TRUE;
static unsigned queues,opens,closes,clears,prepare_acks;
static SudekiMpLanStoryControlState offered_control;
static BOOL have_control=TRUE;
static SudekiMpLanPartyCommand queued;
static char last_notice[128];
void SudekiMpLogFormat(const char *format,...) { (void)format; }
void SudekiMpLanStoryMenuNotice(const char *s) { snprintf(last_notice,sizeof(last_notice),"%s",s); }
BOOL SudekiMpLanPartyPeerStatusGet(SudekiMpLanPartySession *s,unsigned p,SudekiMpLanPartyPeerStatus *out) {
    assert(s==session && p==1); *out=remote_peer; return TRUE;
}
BOOL SudekiMpLanPartyGetPresence(SudekiMpLanPartySession *s,SudekiMpLanPartyPresence *out) {
    assert(s==session); *out=remote_presence; return have_presence;
}
unsigned SudekiMpLanPartyLocalCharacter(SudekiMpLanPartySession *s) { assert(s==session); return 2; }
BOOL SudekiMpLanPartyGetStoryScene(SudekiMpLanPartySession *s,const SudekiMpLanPartyLease *l,uint32_t now,SudekiMpLanStoryScene *out) {
    assert(s==session && l && now==tick); *out=remote_scene; return have_scene;
}
BOOL SudekiMpLanStoryMenuCapturesInput(void) { return esc; }
BOOL SudekiMpLanStoryQuickMenuCapturesInput(void) { return quick; }
BOOL SudekiMpLanStoryMainMenuVisible(void) { return main_shown; }
BOOL SudekiMpLanStoryShopActive(void) { return shop_shown; }
BOOL SudekiMpLanStoryZoneBannerExamineActive(void) { return examine_shown; }
BOOL SudekiMpLanStoryMainMenuRequestOpen(void) { ++opens; return TRUE; }
BOOL SudekiMpLanStoryMainMenuRequestClose(void) { ++closes; return TRUE; }
void SudekiMpLanStoryInputClear(void) { ++clears; }
BOOL SudekiMpLanStoryInputTakeMainMenuRaw(void) { BOOL value=raw_m; raw_m=FALSE; return value; }
BOOL SudekiMpLanPartyQueueCommand(SudekiMpLanPartySession *s,const SudekiMpLanPartyCommand *c) {
    assert(s==session); if(!queue_ok) return FALSE; queued=*c; ++queues; return TRUE;
}
BOOL SudekiMpLanPartyAcknowledgeStoryControl(SudekiMpLanPartySession *s,const SudekiMpLanPartyLease *l,const SudekiMpLanStoryControlFence *f) {
    assert(s==session && l->token==remote_peer.lease.token && f->transaction==1);++prepare_acks;return TRUE;
}
BOOL SudekiMpLanPartyGetStoryControl(SudekiMpLanPartySession *s,const SudekiMpLanPartyLease *l,uint32_t now,SudekiMpLanStoryControlState *out) {
    (void)s;(void)l;(void)now;*out=offered_control;return have_control;
}
BOOL SudekiMpLanStoryInputTakeQuickMenu(void *c,void *a,uint32_t t) {(void)c;(void)a;(void)t;return FALSE;}
BOOL SudekiMpLanStoryInputTakeMainMenu(void *c,void *a,uint32_t t) {(void)c;(void)a;(void)t;return FALSE;}
BOOL SudekiMpLanStoryQuickMenuService(const SudekiMpLanStoryNativeRoster *r,uint32_t t,BOOL o,BOOL a,BOOL toggle) {
    (void)r;(void)t;(void)o;(void)a;(void)toggle;return TRUE;
}
static void reset(void) {
    tick=1000; session=(void *)1; local_seat=1; saved_profile=story_step_away=TRUE; runtime_thread=GetCurrentThreadId();
    main_menu_attempted=shop_attempted=TRUE; main_shown=shop_shown=examine_shown=esc=quick=FALSE;
    memset(&away_client,0,sizeof(away_client)); memset(&away_connection,0,sizeof(away_connection));
    away_pending_request=away_retry_at=story_next_request=story_pending_request=0; away_ui_seen=FALSE;
    raw_m=FALSE; queue_ok=have_presence=have_scene=have_control=TRUE;prepare_acks=0; client_input_ready=TRUE; queues=opens=closes=clears=0; last_notice[0]=0;
    remote_peer=(SudekiMpLanPartyPeerStatus){.lease={.token=7,.generation=1,.seat=1},.phase=SUDEKIMP_LAN_PARTY_OBSERVING,.transport_confirmed=1};
    SudekiMpPartyAssignment a={.world=1,.revision=1,.generation={1,1,1,1},.humans=3,.available=15,.character={3,2,4,4}};
    memset(&remote_presence,0,sizeof(remote_presence)); assert(SudekiMpPartyOwnershipInitialize(&remote_presence.ownership,&a));
    remote_scene=(SudekiMpLanStoryScene){.epoch=1,.revision=1,.phase=SUDEKIMP_LAN_STORY_READY};
    away_combat=(SudekiMpStoryCombatSample){1000,1,1,1,1}; presentation_sample.valid=TRUE;
}
static void ack(unsigned result,unsigned away,SudekiMpPartyControlPhase control) {
    remote_presence.ack_request[1]=away_pending_request; remote_presence.ack_result[1]=result;
    remote_presence.ownership.away=away?2:0; remote_presence.ownership.control[1]=control;
}
int main(void) {
    reset(); away_m_decision(TRUE,TRUE,tick); assert(away_client.want_away && !opens);
    service_step_away(); assert(queues==1 && away_pending_request && queued.value==2 && !story_pending_request);
    service_step_away(); assert(queues==1); /* no duplicate local enqueue */
    raw_m=TRUE; service_step_away(); assert(!away_client.want_away && away_client.phase==SUDEKIMP_STORY_AWAY_RETURNING && !raw_m);
    assert(queues==1); /* second press preserved while first request owns the mailbox */
    ack(SUDEKIMP_PARTY_SWAP_OK,1,SUDEKIMP_PARTY_CONTROL_DRAINING); service_step_away();
    assert(queues==2 && queued.value==0 && away_pending_request==2);
    ack(SUDEKIMP_PARTY_SWAP_OK,0,SUDEKIMP_PARTY_CONTROL_HUMAN); tick+=21; service_step_away();
    assert(!away_client.phase && !away_pending_request);
    reset(); assert(SudekiMpLanStoryRuntimeRequestAway(TRUE)); service_step_away();
    story_pending_request=away_pending_request+100; /* distinct F1 id */
    ack(SUDEKIMP_PARTY_SWAP_BUSY,0,SUDEKIMP_PARTY_CONTROL_HUMAN); service_step_away();
    assert(queues==1 && story_pending_request==101); tick+=249; service_step_away(); assert(queues==1);
    ++tick; service_step_away(); assert(queues==2);
    ack(SUDEKIMP_PARTY_SWAP_STALE,0,SUDEKIMP_PARTY_CONTROL_HUMAN); ++remote_presence.ownership.assignment.revision; service_step_away();
    assert(queues==3 && queued.revision==2);
    ack(SUDEKIMP_PARTY_AWAY_OTHER_AREA,0,SUDEKIMP_PARTY_CONTROL_HUMAN); service_step_away();
    assert(!away_client.want_away && !away_client.phase && !strcmp(last_notice,"Too far from the party to step away"));
    tick+=5000; service_step_away(); assert(queues==3); /* refusal never loops */
    reset(); SudekiMpLanStoryRuntimeRequestAway(TRUE); service_step_away();
    ack(SUDEKIMP_PARTY_SWAP_OK,1,SUDEKIMP_PARTY_CONTROL_DRAINING); service_step_away();
    assert(away_client.phase==SUDEKIMP_STORY_AWAY_REQUESTING); /* still actually driving */
    client_input_ready=FALSE; service_step_away(); assert(away_client.phase==SUDEKIMP_STORY_AWAY_HANDING_OVER);
    remote_presence.ownership.control[1]=SUDEKIMP_PARTY_CONTROL_AI; service_step_away(); assert(away_client.phase==SUDEKIMP_STORY_AWAY_AWAY);
    SudekiMpLanStoryRuntimeRequestAway(FALSE); service_step_away(); ack(SUDEKIMP_PARTY_SWAP_OK,0,SUDEKIMP_PARTY_CONTROL_AI);
    tick+=5000; service_step_away(); assert(away_client.want_away && queued.value==2 && strstr(last_notice,"Couldn't take control"));
    remote_peer.phase=SUDEKIMP_LAN_PARTY_DRAINING; raw_m=TRUE; service_step_away();
    assert(!away_client.phase && !away_client.want_away && !away_pending_request && !raw_m);
    /* A denied RETURN uses return wording and stays away; both the menu
     * request and raw M use the real service/ACK mailbox. */
    for(unsigned menu=0;menu<2;++menu) {
        reset();client_input_ready=FALSE;
        remote_presence.ownership.away=2;remote_presence.ownership.control[1]=SUDEKIMP_PARTY_CONTROL_AI;
        SudekiMpStoryAwayIntent(&away_client,TRUE,tick);service_step_away();
        assert(away_client.phase==SUDEKIMP_STORY_AWAY_AWAY);
        if(menu) assert(SudekiMpLanStoryRuntimeRequestAway(FALSE)); else raw_m=TRUE;
        service_step_away();assert(queues==1 && queued.value==0 && away_pending_request);
        ack(SUDEKIMP_PARTY_SWAP_UNAVAILABLE,1,SUDEKIMP_PARTY_CONTROL_AI);service_step_away();
        assert(away_client.phase==SUDEKIMP_STORY_AWAY_AWAY && away_client.want_away);
        assert(!strcmp(last_notice,"Couldn't take control - press M to try again"));
        assert(!away_pending_request && queues==1);
        tick+=1000;service_step_away();assert(queues==1);
    }
    reset();assert(SudekiMpLanStoryRuntimeRequestAway(TRUE));service_step_away();
    ack(SUDEKIMP_PARTY_SWAP_UNAVAILABLE,0,SUDEKIMP_PARTY_CONTROL_HUMAN);service_step_away();
    assert(!away_client.phase && !strcmp(last_notice,"Could not step away - try again"));
    reset(); away_combat.combat_mode=0; away_m_decision(TRUE,TRUE,tick); assert(opens==1);
    tick+=251; away_m_decision(TRUE,TRUE,tick); assert(opens==1 && !strcmp(last_notice,"Waiting for the host..."));
    reset(); main_shown=TRUE; away_note_ui(tick); main_shown=FALSE; tick+=299;
    away_m_decision(TRUE,TRUE,tick); assert(!away_client.phase && !opens);
    ++tick; away_combat.receipt=tick; away_m_decision(TRUE,TRUE,tick); assert(away_client.want_away);
    reset(); SudekiMpLanStoryRuntimeRequestAway(TRUE); SudekiMpLanStoryRuntimeRequestAway(FALSE);
    have_presence=FALSE; tick+=5000; service_step_away();
    assert(away_client.want_away && strstr(last_notice,"Couldn't take control"));
    reset(); presentation_sample.valid=FALSE; away_m_decision(TRUE,TRUE,tick);
    assert(!away_client.phase && !strcmp(last_notice,"Waiting for the host..."));
    reset(); SudekiMpLanStoryRuntimeRequestAway(TRUE); service_step_away();
    remote_presence.ack_request[1]=away_pending_request+1; remote_presence.ack_result[1]=SUDEKIMP_PARTY_AWAY_OTHER_AREA;
    service_step_away(); assert(queues==2 && away_client.want_away && !last_notice[0]); /* newer F1 ACK is not our refusal */
    reset(); have_scene=FALSE; away_m_decision(TRUE,TRUE,tick); assert(!strcmp(last_notice,"Waiting for the host..."));
    reset(); esc=TRUE; SudekiMpLanStoryRuntimeRequestAway(TRUE); raw_m=TRUE; service_step_away(); assert(away_client.want_away && !raw_m);
    /* A2: the production early-return helper ACKs PREPARE while clearing
     * input, and holds READY without driving or acknowledging it. */
    reset();away_client.want_away=1;
    offered_control=(SudekiMpLanStoryControlState){.phase=SUDEKIMP_STORY_CONTROL_PREPARE,.fence={.transaction=1}};
    assert(away_hold_offer(TRUE,&remote_peer,&offered_control) && prepare_acks==1 && clears==1 && !client_input_ready);
    offered_control.phase=SUDEKIMP_STORY_CONTROL_READY;client_input_ready=TRUE;
    assert(away_hold_offer(TRUE,&remote_peer,&offered_control) && prepare_acks==1 && clears==2 && !client_input_ready);
    offered_control.phase=SUDEKIMP_STORY_CONTROL_PREPARE;remote_presence.ownership.away=2;
    assert(away_hold_offer(TRUE,&remote_peer,&offered_control) && prepare_acks==1);
    remote_presence.ownership.away=0;have_presence=FALSE;
    assert(away_hold_offer(TRUE,&remote_peer,&offered_control) && prepare_acks==1);
    have_presence=TRUE;assert(away_hold_offer(FALSE,NULL,NULL) && prepare_acks==1);
    away_client.want_away=0;assert(!away_hold_offer(TRUE,&remote_peer,&offered_control) && prepare_acks==1);
    /* A6: drive the real quick-menu coordinator, not just the pure M policy. */
    for(unsigned blocked=0;blocked<5;++blocked) {
        reset();away_combat.combat_mode=0;client_local_selected=TRUE;
        remote_scene.available_mask=15;remote_scene.leader_seat=3;strcpy(remote_scene.world,"fixture");
        SudekiMpLanStoryNativeRoster r={.epoch=1,.leader_character=2,.controller=(void *)2};r.actors[2]=(void *)3;
        client_control_connection=remote_peer.lease;
        client_control_fence=(SudekiMpLanStoryControlFence){1,1,1,1,1,2};
        offered_control=(SudekiMpLanStoryControlState){.phase=SUDEKIMP_STORY_CONTROL_READY,.fence=client_control_fence};
        presentation_sample.receipt=tick;
        if(blocked==1) esc=TRUE;
        if(blocked==2) offered_control.phase=SUDEKIMP_STORY_CONTROL_PREPARE;
        if(blocked==3) have_control=FALSE;
        if(blocked==4) ++offered_control.fence.transaction;
        raw_m=TRUE;assert(service_quick_menu(&r,&remote_scene,NULL));
        assert(!raw_m && opens==(blocked==0));
    }
    reset();client_input_ready=FALSE;presentation_sample.valid=FALSE;client_local_selected=TRUE;raw_m=TRUE;
    service_step_away();assert(!raw_m && !opens && !away_client.want_away && !strcmp(last_notice,"Waiting for the host..."));
    reset();client_input_ready=FALSE;presentation_sample.valid=FALSE;client_local_selected=TRUE;esc=TRUE;raw_m=TRUE;
    service_step_away();assert(!raw_m && !opens && !away_client.want_away && !last_notice[0]);
    reset();remote_scene.available_mask=15;remote_scene.leader_seat=3;strcpy(remote_scene.world,"world");
    SudekiMpLanStoryScene native=remote_scene;native.epoch=9;
    SudekiMpLanStoryFrame frame={.epoch=1};
    for(unsigned i=0;i<4;++i)frame.actors[i].generation=presented_generations[i]=10+i;
    assert(presentation_reseed_exact(&native,&remote_scene,&frame));
    remote_scene.phase=SUDEKIMP_LAN_STORY_LOADING;assert(!presentation_reseed_exact(&native,&remote_scene,&frame));remote_scene.phase=SUDEKIMP_LAN_STORY_READY;
    strcpy(native.world,"other");assert(!presentation_reseed_exact(&native,&remote_scene,&frame));strcpy(native.world,"world");
    native.available_mask=12;assert(!presentation_reseed_exact(&native,&remote_scene,&frame));native.available_mask=15;
    frame.epoch=2;assert(!presentation_reseed_exact(&native,&remote_scene,&frame));frame.epoch=1;
    for(unsigned i=0;i<4;++i){++frame.actors[i].generation;assert(!presentation_reseed_exact(&native,&remote_scene,&frame));--frame.actors[i].generation;}
    strcpy(remote_scene.temporary,"house");
    SudekiMpLanStoryAreaMembership membership={.epoch=1,.revision=1,.available_mask=15};
    membership.members[0].sector=0x101;membership.members[1].sector=0x201;
    membership.members[2].sector=0x1ff;membership.members[3].sector=0x120;
    assert(membership_inside_mask(&remote_scene,&membership,TRUE)==13);
    assert(!membership_inside_mask(&remote_scene,&membership,FALSE));
    ++membership.epoch;assert(!membership_inside_mask(&remote_scene,&membership,TRUE));--membership.epoch;
    ++membership.revision;assert(!membership_inside_mask(&remote_scene,&membership,TRUE));--membership.revision;
    membership.available_mask=12;assert(!membership_inside_mask(&remote_scene,&membership,TRUE));membership.available_mask=15;
    membership.members[3].sector=0xffff;membership.members[2].sector=0xff01;assert(!membership_inside_mask(&remote_scene,&membership,TRUE));
    membership.members[3].sector=0x120;membership.members[2].sector=0xffff;
    assert(membership_inside_mask(&remote_scene,&membership,TRUE)==9);
    membership.members[3].sector=0xff01;assert(membership_inside_mask(&remote_scene,&membership,TRUE)==8);
    puts("actual step-away runtime intent, ACK isolation, retry, refusal, raw return, timeout and M routing passed"); return 0;
}
