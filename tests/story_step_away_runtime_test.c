/* Actual runtime intent/mailbox/M routing; native and transport spies only. */
#include <windows.h>
static DWORD tick=1000;
static DWORD clock_now(void) { return tick; }
#define GetTickCount clock_now
#include "../src/hooks/lan_story_runtime.c"
#undef GetTickCount
#include "../src/network/lan_story_step_away.c"
#include "../src/engine/party_ownership.c"
#include <assert.h>
static SudekiMpLanPartyPresence remote_presence;
static SudekiMpLanPartyPeerStatus remote_peer;
static SudekiMpLanStoryScene remote_scene;
static BOOL have_presence=TRUE,have_scene=TRUE,raw_m,esc,quick,main_shown,shop_shown,examine_shown,queue_ok=TRUE;
static unsigned queues,opens,closes,clears;
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
static void reset(void) {
    tick=1000; session=(void *)1; local_seat=1; saved_profile=story_step_away=TRUE; runtime_thread=GetCurrentThreadId();
    main_menu_attempted=shop_attempted=TRUE; main_shown=shop_shown=examine_shown=esc=quick=FALSE;
    memset(&away_client,0,sizeof(away_client)); memset(&away_connection,0,sizeof(away_connection));
    away_pending_request=away_retry_at=story_next_request=story_pending_request=0; away_ui_seen=FALSE;
    raw_m=FALSE; queue_ok=have_presence=have_scene=TRUE; client_input_ready=TRUE; queues=opens=closes=clears=0; last_notice[0]=0;
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
    puts("actual step-away runtime intent, ACK isolation, retry, refusal, raw return, timeout and M routing passed"); return 0;
}
