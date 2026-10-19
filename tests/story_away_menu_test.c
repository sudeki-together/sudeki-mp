/* Actual saved-story Esc page model; no renderer, game or input. */
#include "../src/ui/party_menu.c"
#include <assert.h>
#include <stdio.h>
static unsigned count_action(unsigned action,const char *label) {
    unsigned n=0;
    for(unsigned i=0;i<count;++i) if(actions[i]==action) {
        assert(!strcmp(panel.controls[i].label,label)); ++n;
    }
    return n;
}
int main(void) {
    state=(SudekiMpPartyMenuState){.active=TRUE,.saved_story=TRUE,.story_step_away=TRUE,
        .local_player=1,.connected=3,.character={3,2,4,4}};
    page=HOME; rebuild();
    assert(count_action(SUDEKIMP_PARTY_MENU_AWAY,"Away - watch the party")==1);
    assert(!count_action(SUDEKIMP_PARTY_MENU_RETURN,"Return to Play"));
    state.away=2; rebuild(); assert(!count_action(SUDEKIMP_PARTY_MENU_AWAY,"Away - watch the party"));
    assert(count_action(SUDEKIMP_PARTY_MENU_RETURN,"Return to Play")==1);
    state.host=TRUE; state.local_player=0; rebuild();
    assert(!count_action(SUDEKIMP_PARTY_MENU_AWAY,"Away - watch the party") && !count_action(SUDEKIMP_PARTY_MENU_RETURN,"Return to Play"));
    state.host=FALSE;state.local_player=1;state.story_step_away=FALSE; rebuild();
    assert(!count_action(SUDEKIMP_PARTY_MENU_AWAY,"Away - watch the party") && !count_action(SUDEKIMP_PARTY_MENU_RETURN,"Return to Play"));
    puts("saved-story Esc Away/Return, host exclusion and ini opt-out passed");return 0;
}
