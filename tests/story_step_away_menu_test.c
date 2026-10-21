/* Actual Esc command state: no native menu installation. */
#include "../src/hooks/lan_story_menu.c"
#include <assert.h>
#include <stdarg.h>
static BOOL request_ok;static unsigned requests,closed;
BOOL SudekiMpLanStoryRuntimeRequestAway(BOOL away) {(void)away;++requests;return request_ok;}
void SudekiMpPartyMenuStateSet(const SudekiMpPartyMenuState *s) {(void)s;}
void SudekiMpLogFormat(const char *fmt,...) {if(strstr(fmt,"event=closed")) ++closed;}
int main(void) {
    initialized=TRUE;native_thread=GetCurrentThreadId();local_player=1;
    const SudekiMpPartyMenuAction actions[]={SUDEKIMP_PARTY_MENU_AWAY,SUDEKIMP_PARTY_MENU_RETURN};
    for(unsigned i=0;i<2;++i) {
        menu_open=TRUE;request_ok=FALSE;unsigned before=closed;
        assert(!command(actions[i],1) && menu_open && closed==before);
        request_ok=TRUE;assert(command(actions[i],1) && !menu_open && closed==before+1);
        menu_open=TRUE;unsigned count=requests;
        assert(!command(actions[i],2) && menu_open && requests==count);
    }
    puts("Esc Away/Return close state only on success passed");return 0;
}
