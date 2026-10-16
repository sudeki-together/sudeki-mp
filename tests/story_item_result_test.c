/* Actual client result consumer with transport/inventory spies. No native
 * script, actor, effect or game process executes in this policy fixture. */
#include "../src/hooks/lan_story_runtime.c"
#include "../src/network/lan_story_handoff.c"
#include <assert.h>
#include <stdio.h>

static SudekiMpLanStoryActionResult reply;
static BOOL have_reply,menu_visible,reduce_ok;
static unsigned debits,refreshes,notices,sends,last_item;
void SudekiMpLogFormat(const char *format,...) {(void)format;}
BOOL SudekiMpLanStoryMainMenuVisible(void) {return menu_visible;}
BOOL SudekiMpLanStoryMainMenuRefreshItems(void) {++refreshes;return TRUE;}
void SudekiMpLanStoryMenuNotice(const char *text) {assert(text && *text);++notices;}
BOOL SudekiMpReduceLocalItem(unsigned item) {++debits;last_item=item;return reduce_ok;}
BOOL SudekiMpUseItemOnCharacter(void *actor,unsigned item,unsigned *stage,BOOL validated,BOOL reduce) {
    (void)actor;(void)item;(void)stage;(void)validated;(void)reduce;
    assert(!"Accepted client item results must never execute native item scripts");return FALSE;
}
BOOL SudekiMpLanPartyGetStoryActionResult(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyLease *connection,SudekiMpLanStoryActionResult *out) {
    assert(s==session && same_connection(connection,&client_action_connection));
    if(!have_reply) return FALSE;
    *out=reply;return TRUE;
}
BOOL SudekiMpLanPartySendStoryAction(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyLease *connection,const SudekiMpLanStoryActionRequest *request) {
    assert(s==session && same_connection(connection,&client_action_connection));
    assert(SudekiMpLanStoryActionRequestSame(request,&client_action));++sends;return TRUE;
}
static void setup(unsigned item,unsigned outcome) {
    session=(void *)1;local_seat=1;main_menu_attempted=TRUE;
    client_action_connection=(SudekiMpLanPartyLease){.token=9,.generation=3,.seat=1};
    client_action=(SudekiMpLanStoryActionRequest){.fence={1,2,3,4,1,2},
        .request=5,.acknowledged_frame=6,.kind=SUDEKIMP_STORY_ACTION_USE_ITEM,
        .slot=(uint8_t)item,.extra=(uint8_t)(item>>8)};
    reply=(SudekiMpLanStoryActionResult){.fence=client_action.fence,.request=5,
        .observed_tick=90,.kind=client_action.kind,.slot=client_action.slot,
        .extra=client_action.extra,.outcome=(uint8_t)outcome};
    client_action_pending=have_reply=menu_visible=reduce_ok=TRUE;
    client_action_sent_at=0;debits=refreshes=notices=sends=last_item=0;
}
static void consume(void) {
    service_client_action(&client_action_connection,&client_action.fence,100);
}
int main(void) {
    for(unsigned open=0;open<2;++open) {
        setup(204,SUDEKIMP_STORY_ACTION_STARTED);menu_visible=(BOOL)open;
        consume();assert(debits==1 && last_item==204 && refreshes==open && !notices && !client_action_pending);
        consume();consume();assert(debits==1 && refreshes==open);
    }
    puts("PASS ItemResultBookkeepingOnly: M/open and Q/closed menu debit once, refresh only open Items, no native apply; duplicate result inert");
    setup(320,SUDEKIMP_STORY_ACTION_STARTED);consume();assert(last_item==320 && debits==1);
    setup(204,SUDEKIMP_STORY_ACTION_BUSY);consume();assert(!debits && !refreshes && notices==1 && !client_action_pending);
    setup(204,SUDEKIMP_STORY_ACTION_UNAVAILABLE);consume();assert(!debits && notices==1);
    setup(204,SUDEKIMP_STORY_ACTION_EXPIRED);consume();assert(!debits && notices==1);
    puts("PASS ItemResultRefusal: full item ID preserved; busy/unavailable/expired results never debit or run scripts");
    setup(204,SUDEKIMP_STORY_ACTION_STARTED);have_reply=FALSE;consume();
    assert(client_action_pending && sends==1 && !debits);have_reply=TRUE;consume();assert(debits==1 && !client_action_pending);
    setup(204,SUDEKIMP_STORY_ACTION_STARTED);++reply.request;consume();assert(client_action_pending && !debits && sends==1);
    setup(204,SUDEKIMP_STORY_ACTION_STARTED);++reply.slot;consume();assert(client_action_pending && !debits);
    setup(204,SUDEKIMP_STORY_ACTION_STARTED);++reply.fence.transaction;consume();assert(client_action_pending && !debits);
    setup(204,SUDEKIMP_STORY_ACTION_STARTED);service_client_action(NULL,&client_action.fence,100);assert(client_action_pending && !debits);
    SudekiMpLanPartyLease changed=client_action_connection;++changed.generation;
    service_client_action(&changed,&client_action.fence,100);assert(!client_action_pending && !debits);
    setup(204,SUDEKIMP_STORY_ACTION_STARTED);SudekiMpLanStoryControlFence fence=client_action.fence;++fence.actor_generation;
    service_client_action(&client_action_connection,&fence,100);assert(!client_action_pending && !debits);
    puts("PASS ItemResultOwnership: missing/mismatched result cannot debit; same request retry, absent proof and replaced peer/actor fences retained");
    setup(204,SUDEKIMP_STORY_ACTION_STARTED);reduce_ok=FALSE;consume();consume();
    assert(debits==1 && !client_action_pending); /* No ambiguous debit retry or script fallback. */
    puts("PASS ItemResultNoFallback: refused bookkeeping never falls back to native item execution");
    return 0;
}
