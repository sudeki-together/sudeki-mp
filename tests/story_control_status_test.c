/* Synthetic UI state only; no session, game, or native menu is opened. */
#include "../src/hooks/lan_story_menu.c"
#include <assert.h>
static SudekiMpPartyMenuState published;
void SudekiMpPartyMenuStateSet(const SudekiMpPartyMenuState *value) { published=*value; }
int main(void) {
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
