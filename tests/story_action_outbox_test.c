/* Actual runtime outbox, synthetic transport: no native game or input. */
#include "../src/hooks/lan_story_runtime.c"
#include "../src/network/lan_story_handoff.c"
#include <assert.h>
#include <stdio.h>
static BOOL have_result;
static unsigned sends,notices;
static SudekiMpLanStoryActionRequest sent;
static SudekiMpLanStoryActionResult terminal;
void SudekiMpLogFormat(const char *format,...) {(void)format;}
void SudekiMpLanStoryMenuNotice(const char *text) {(void)text;++notices;}
/* Client-side result effects (equip, item reduction): not exercised here. */
BOOL SudekiMpEquipCharacterWeaponItem(void *character,unsigned int item) {(void)character;(void)item;return FALSE;}
BOOL SudekiMpEquipCharacterArmourItem(void *character,unsigned int index,unsigned int item) {(void)character;(void)index;(void)item;return FALSE;}
BOOL SudekiMpReduceLocalItem(unsigned int item) {(void)item;return FALSE;}
BOOL SudekiMpLanStoryInventoryPrepare(uint32_t item,uint32_t quantity,SudekiMpStoryInventoryGrant *grant) {(void)item;(void)quantity;(void)grant;return FALSE;}
BOOL SudekiMpLanStoryInventoryApply(SudekiMpStoryInventoryGrant *grant,SudekiMpStoryInventoryGrantResult *result) {(void)grant;(void)result;return FALSE;}
BOOL SudekiMpLanPartyGetStoryActionResult(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyLease *connection,SudekiMpLanStoryActionResult *result) {
    assert(s==session && same_connection(connection,&client_action_connection));
    if(!have_result) return FALSE;
    *result=terminal; return TRUE;
}
BOOL SudekiMpLanPartySendStoryAction(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyLease *connection,const SudekiMpLanStoryActionRequest *request) {
    assert(s==session && same_connection(connection,&client_action_connection));
    sent=*request;++sends;return TRUE;
}
int main(void) {
    session=(void *)1;local_seat=1;client_input_ready=TRUE;
    client_seed.actors[2]=(void *)2;
    client_control_connection=(SudekiMpLanPartyLease){.token=7,.generation=1,.seat=1};
    client_control_fence=(SudekiMpLanStoryControlFence){1,1,1,1,1,2};
    presentation_sample.valid=TRUE;presentation_sample.receipt=GetTickCount();
    presentation_sample.sequence=20;presentation_sample.lease=client_control_connection;
    assert(queue_client_action((void *)2,SUDEKIMP_STORY_ACTION_MELEE,1));
    service_client_action(&client_control_connection,&client_control_fence,1000);
    assert(sends==1 && sent.request==1);
    /* Missing observations neither prove a new owner nor a terminal action.
     * Old behavior abandoned #1 here; transport still owned #1, so #2 could
     * never send and every later M1/M2 stayed behind it. */
    for(unsigned i=0;i<20;++i) service_client_action(NULL,NULL,1100+i*100);
    assert(client_action_pending && client_action.request==1 && sends==1);
    service_client_action(&client_control_connection,NULL,4000);
    service_client_action(NULL,&client_control_fence,4000);
    assert(client_action_pending && sends==1);
    assert(!queue_client_action((void *)2,SUDEKIMP_STORY_ACTION_MELEE,2));
    service_client_action(&client_control_connection,&client_control_fence,4100);
    assert(sends==2 && sent.request==1 && sent.acknowledged_frame==20);
    terminal=(SudekiMpLanStoryActionResult){client_action.fence,1,4200,
        client_action.kind,client_action.slot,SUDEKIMP_STORY_ACTION_EXPIRED};
    have_result=TRUE;
    service_client_action(&client_control_connection,&client_control_fence,4200);
    assert(!client_action_pending && sends==2 && !notices);
    presentation_sample.receipt=GetTickCount();presentation_sample.sequence=21;
    assert(queue_client_action((void *)2,SUDEKIMP_STORY_ACTION_MELEE,2));
    have_result=FALSE;
    service_client_action(&client_control_connection,&client_control_fence,4300);
    assert(sends==3 && sent.request==2 && sent.slot==2);
    /* A positively different fence/connection retires the old plain outbox;
     * never retransmit it into a replacement actor or reconnected peer. */
    SudekiMpLanStoryControlFence next=client_control_fence;++next.transaction;
    service_client_action(&client_control_connection,&next,4400);
    assert(!client_action_pending && sends==3);
    presentation_sample.receipt=GetTickCount();
    assert(queue_client_action((void *)2,SUDEKIMP_STORY_ACTION_SPIRIT,1));
    SudekiMpLanPartyLease reconnect=client_control_connection;++reconnect.generation;
    service_client_action(&reconnect,&client_control_fence,4500);
    assert(!client_action_pending && sends==3);
    puts("PASS actual action outbox: unknown offer retained, same-ID recovery, later attack, positive owner change");
    return 0;
}
