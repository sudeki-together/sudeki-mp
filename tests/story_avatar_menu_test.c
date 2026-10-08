/* Plain menu presentation fixture. No native slot, input lease or camera is
 * acquired; Refresh receives only the caller's confirmed local readiness. */
#include "../src/hooks/lan_story_menu.c"
#include <assert.h>

static SudekiMpPartyMenuState shown;
void SudekiMpPartyMenuStateSet(const SudekiMpPartyMenuState *s) { shown=*s; }
static SudekiMpLobbyStatus lobby_for(unsigned local,BOOL dev) {
    initialized=TRUE; native_thread=GetCurrentThreadId(); local_player=local;
    menu_open=leave_requested=FALSE; notice[0]=0; notice_until=0;
    SudekiMpLobbyStatus lobby={0}; lobby.local_slot=(uint8_t)local;
    lobby.phase=local?SUDEKIMP_LOBBY_CONNECTED:SUDEKIMP_LOBBY_HOSTING;
    lobby.mode=dev?SUDEKIMP_LOBBY_MODE_DEV_PLAY:SUDEKIMP_LOBBY_MODE_MULTIPLAYER;
    for(unsigned p=0;p<4;++p) {
        lobby.members[p].character=dev?SUDEKIMP_LOBBY_TALOS:4;
        if(p<2) {
            lobby.members[p].present=lobby.members[p].reserved=lobby.members[p].locked=1;
            snprintf(lobby.members[p].name,sizeof(lobby.members[p].name),"Player %u",p+1u);
        }
    }
    return lobby;
}
int main(void) {
    SudekiMpLobbyStatus lobby=lobby_for(0,TRUE);
    SudekiMpLanStoryMenuRefresh(&lobby,3,8,TRUE,TRUE,FALSE,FALSE);
    assert(shown.character[0]==5 && shown.character[1]==5 && shown.input_ready==1);
    assert(shown.controlling==1 && !strcmp(shown.status,"Playing Talos"));
    assert(!strcmp(shown.story_detail[0],"Talos - Playing"));
    assert(!strcmp(shown.story_detail[1],"Talos - Avatar selected"));
    SudekiMpLanStoryMenuRefresh(&lobby,3,8,TRUE,FALSE,FALSE,FALSE);
    assert(!shown.input_ready && strstr(shown.status,"Waiting for Talos avatar"));
    assert(!strstr(shown.status,"Synchronizing host") && !strstr(shown.story_detail[0],"Ailish"));
    char tools[64]; tools_status(tools,sizeof(tools)); assert(!strcmp(tools,"WAITING FOR AVATAR"));

    lobby=lobby_for(1,TRUE);
    SudekiMpLanStoryMenuRefresh(&lobby,3,8,TRUE,FALSE,TRUE,TRUE);
    assert(shown.input_ready==2 && shown.controlling==2 && !strcmp(shown.status,"Playing Talos"));
    assert(!strcmp(shown.story_detail[0],"Talos - Host avatar"));
    assert(!strcmp(shown.story_detail[1],"Talos - Playing"));
    menu_open=TRUE;
    SudekiMpLanStoryMenuRefresh(&lobby,3,8,TRUE,FALSE,TRUE,TRUE);
    assert(!strcmp(shown.story_detail[1],"Talos - Menu open"));
    menu_open=FALSE;
    SudekiMpLanStoryMenuRefresh(&lobby,3,8,TRUE,FALSE,TRUE,FALSE);
    assert(!shown.input_ready && strstr(shown.status,"Waiting for Talos avatar"));
    assert(!strstr(shown.status,"Spectating") && !strstr(shown.story_detail[1],"Spectating"));
    lobby.members[1].name[0]=0;
    SudekiMpLanStoryMenuRefresh(&lobby,3,8,TRUE,FALSE,TRUE,FALSE);
    assert(!strcmp(shown.name[1],"Talos"));
    SudekiMpLanStoryMenuRefresh(&lobby,4,0,FALSE,FALSE,FALSE,FALSE);
    assert(!shown.input_ready && !strcmp(shown.status,"Waiting for the story..."));

    /* The native all-avatar party is READY with no canonical hero members.
     * Scene readiness alone never grants the separate local input lease. */
    for(unsigned local=0;local<2;++local) {
        lobby=lobby_for(local,TRUE);
        SudekiMpLanStoryMenuRefresh(&lobby,4,0,TRUE,FALSE,TRUE,FALSE);
        assert(!shown.input_ready && !shown.controlling &&
            !strcmp(shown.status,"Waiting for Talos avatar control..."));
        SudekiMpLanStoryMenuRefresh(&lobby,4,0,TRUE,!local,TRUE,local!=0);
        assert(shown.input_ready==(1u<<local) && shown.controlling==(1u<<local));
        assert(!strcmp(shown.status,"Playing Talos") &&
            !strcmp(shown.story_detail[local],"Talos - Playing"));
        tools_status(tools,sizeof(tools)); assert(!strcmp(tools,"PLAYING"));
        SudekiMpLanStoryMenuRefresh(&lobby,4,0,FALSE,TRUE,TRUE,TRUE);
        assert(!shown.input_ready && !strcmp(shown.status,"Waiting for the story..."));
        const unsigned invalid[][2]={{3,0},{4,8},{4,16},{5,0},{255,0}};
        for(unsigned n=0;n<sizeof(invalid)/sizeof(invalid[0]);++n) {
            SudekiMpLanStoryMenuRefresh(&lobby,invalid[n][0],invalid[n][1],TRUE,TRUE,TRUE,TRUE);
            assert(!shown.input_ready && !shown.controlling &&
                !strcmp(shown.status,"Waiting for the story..."));
        }
    }

    /* Regular mode still uses native hero availability/leader rules. */
    lobby=lobby_for(0,FALSE); lobby.members[0].character=3;
    SudekiMpLanStoryMenuRefresh(&lobby,4,0,TRUE,TRUE,TRUE,TRUE);
    assert(!shown.input_ready && !shown.controlling && !strcmp(shown.status,"Waiting for the story..."));
    SudekiMpLanStoryMenuRefresh(&lobby,3,8,TRUE,TRUE,FALSE,FALSE);
    assert(shown.input_ready==1 && !strcmp(shown.story_detail[0],"Ailish - Playing"));
    lobby=lobby_for(1,FALSE); lobby.members[1].character=2;
    SudekiMpLanStoryMenuRefresh(&lobby,3,8,TRUE,FALSE,TRUE,FALSE);
    assert(!shown.input_ready && !strcmp(shown.status,"Spectating Ailish"));
    lobby.members[1].character=5;
    SudekiMpLanStoryMenuRefresh(&lobby,3,8,TRUE,FALSE,TRUE,FALSE);
    assert(shown.character[1]==4 && !shown.input_ready);
    puts("StoryAvatarMenuTest PASS: zero-hero Dev host/client readiness, malformed scene refusal, regular hero/spectator rules preserved");
    return 0;
}
