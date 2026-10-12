/* Synthetic UI state only; no session, game, or native menu is opened. */
#include "../src/hooks/lan_story_menu.c"
#include <assert.h>
static SudekiMpPartyMenuState published;
void SudekiMpPartyMenuStateSet(const SudekiMpPartyMenuState *value) { published=*value; }
static BOOL protection,query_ok=TRUE,reconcile_ok=TRUE;
static unsigned protection_calls;
BOOL SudekiMpCleanroomEnginePartyInvulnerable(BOOL *enabled) {
    if(!query_ok) return FALSE;
    *enabled=protection; return TRUE;
}
BOOL SudekiMpCleanroomEngineMaintainPartyInvulnerability(BOOL enabled) {
    ++protection_calls; protection=enabled; return reconcile_ok;
}
void SudekiMpLogFormat(const char *format,...) { (void)format; }
static DWORD WINAPI foreign_protection(void *unused) {
    (void)unused; service_test_invulnerability(TRUE); return 0;
}
static void protection_cases(void) {
    initialized=TRUE; native_thread=GetCurrentThreadId(); local_player=0;
    service_test_invulnerability(TRUE); assert(!protection_calls); /* Default off. */
    initialized=FALSE; SudekiMpLanStoryMenuTestInvulnerable(TRUE); initialized=TRUE;
    local_player=1; service_test_invulnerability(TRUE); assert(!protection_calls);
    local_player=0; native_tools_only=TRUE; service_test_invulnerability(TRUE);
    assert(!protection_calls); native_tools_only=FALSE;
    service_test_invulnerability(FALSE); assert(!protection_calls);
    leave_requested=TRUE; service_test_invulnerability(TRUE); assert(!protection_calls);
    leave_requested=FALSE; tools_stopping=TRUE; service_test_invulnerability(TRUE);
    assert(!protection_calls); tools_stopping=FALSE;
    HANDLE t=CreateThread(NULL,0,foreign_protection,NULL,0,NULL); assert(t);
    assert(WaitForSingleObject(t,5000)==WAIT_OBJECT_0); CloseHandle(t);
    assert(!protection_calls);
    query_ok=FALSE; service_test_invulnerability(TRUE);
    assert(!test_invulnerability_owned && !protection_calls); query_ok=TRUE;
    reconcile_ok=FALSE; service_test_invulnerability(TRUE);
    assert(test_invulnerability_owned && protection && !test_invulnerability_confirmed);
    assert(!drain_test_invulnerability() && test_invulnerability_owned);
    reconcile_ok=TRUE; assert(drain_test_invulnerability() && !protection);
    service_test_invulnerability(TRUE);
    assert(protection && test_invulnerability_confirmed && test_invulnerability_owned);
    assert(drain_test_invulnerability() && !protection && !test_invulnerability_owned);
    /* Preserve protection already requested by another native tool owner. */
    protection=TRUE; service_test_invulnerability(TRUE);
    assert(test_invulnerability_before && drain_test_invulnerability() && protection);
    initialized=FALSE; SudekiMpLanStoryMenuTestInvulnerable(FALSE);
}
int main(void) {
    protection_cases();
    initialized=TRUE; native_thread=GetCurrentThreadId(); local_player=1;
    SudekiMpLobbyStatus lobby={0}; lobby.local_slot=1;
    /* Use a nonterminal phase; refresh has no authority to change it. */
    lobby.phase=SUDEKIMP_LOBBY_CONNECTING;
    lobby.members[0].present=lobby.members[0].reserved=lobby.members[0].locked=TRUE;
    lobby.members[0].character=3;
    lobby.members[1].present=lobby.members[1].reserved=lobby.members[1].locked=TRUE;
    lobby.members[1].character=2;
    SudekiMpLanStoryMenuRefresh(&lobby,3,12,TRUE,FALSE,TRUE,FALSE);
    assert(!strcmp(published.status,"Waiting for Tal control..."));
    assert(!strcmp(published.story_detail[1],"Tal - Waiting for control"));
    assert(!published.input_ready);
    SudekiMpLanStoryMenuRefresh(&lobby,3,12,TRUE,FALSE,FALSE,FALSE);
    assert(!strcmp(published.status,"Waiting for host world synchronization..."));
    assert(!strstr(published.story_detail[1],"Spectating Ailish"));
    SudekiMpLanStoryMenuRefresh(&lobby,3,12,TRUE,FALSE,TRUE,TRUE);
    assert(!strcmp(published.status,"Playing Tal") && (published.input_ready&2));
    lobby.members[1].reserved=FALSE; lobby.members[1].locked=FALSE;
    SudekiMpLanStoryMenuRefresh(&lobby,3,12,TRUE,FALSE,TRUE,FALSE);
    assert(!strcmp(published.status,"Spectating Ailish"));
    puts("story control status tests passed"); return 0;
}
