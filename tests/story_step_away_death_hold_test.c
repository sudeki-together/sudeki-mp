/* Actual hold decisions; synthetic actors/group, no installed native hooks. */
#include "../src/hooks/lan_story_death_hold.c"
#include <assert.h>
#include <stdio.h>
void SudekiMpLogFormat(const char *s,...) {(void)s;}
int main(void) {
    uint8_t group[0xc4]={0},actor[4]={0};void *peers[3]={actor,NULL,NULL};
    SudekiMpLanStoryDeathHoldSetRemoteActors(peers);
    *(void **)(group+0xc0)=actor;SudekiMpLanStoryDeathHoldSetActive(TRUE);
    death_hold_decide();assert(death_hold_skip && held==1);
    pending_hold_decide(group);assert(pending_hold_skip && pending_held==1);
    *(void **)(group+0xc0)=actor+1;pending_hold_decide(group);assert(!pending_hold_skip);
    *(void **)(group+0xc0)=NULL;pending_hold_decide(group);assert(!pending_hold_skip);
    *(void **)(group+0xc0)=actor;SudekiMpLanStoryDeathHoldSetActive(FALSE);
    death_hold_decide();pending_hold_decide(group);assert(!death_hold_skip && !pending_hold_skip);
    puts("away death and pending-lead hold decisions passed");return 0;
}
