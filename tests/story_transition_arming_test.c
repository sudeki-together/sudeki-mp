/* Real begin_zone vanilla branches; clock and native world storage are synthetic. */
#define main prior_temporary_cases
#include "story_temporary_context_test.c"
#undef main
BOOL SudekiMpLanStoryAreaFollowCalling(void) {return FALSE;}
int main(void) {
    base=VirtualAlloc(NULL,0x410000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(base);
    for(unsigned kind=TEMP;kind<=EXIT;++kind) {
        fixture();installed=TRUE;split_owner=NULL;split_active=FALSE;call_depth=0;
        background_switch_tick=hold_started=hold_ended=0;holding=FALSE;
        vanilla_transition_pending=FALSE;vanilla_transition=0;fixture_tick=1000;
        observed.phase=SUDEKIMP_LAN_STORY_READY;strcpy(observed.world,"brightwater");
        strcpy(observed.temporary,"church");*(uint32_t *)(table+0x54+0x34)=0;
        assert(!SudekiMpLanStoryObserverHolding());
        begin_zone(kind,kind==TEMP?"church":NULL);
        assert(SudekiMpLanStoryObserverIdentityUnknown());
        if(kind==EXIT)assert(SudekiMpLanStoryObserverWorldLoading());
        fixture_tick+=20000;
        assert(!SudekiMpLanStoryObserverIdentityUnknown());
        if(kind==EXIT)assert(SudekiMpLanStoryObserverWorldLoading());
    }
    fixture_tick=30000;background_switch_tick=30000;
    assert(SudekiMpLanStoryObserverBackgroundSwitchTick()==30000);
    fixture_tick=50000;assert(!SudekiMpLanStoryObserverBackgroundSwitchTick());
    uint8_t actor[0x98]={0},ai[0x16c]={0},position[0x24]={0},mode[0x0c]={0};
    last_world=world;last_actors[2]=actor;*(void **)(actor+0x94)=ai;*(void **)(ai+0x10)=actor;
    *(void **)(actor+0x44)=position;*(void **)(position+0x10)=actor;*(void **)(ai+0x3c)=mode;
    *(float *)(position+0x18)=12;*(float *)(position+0x1c)=13;*(float *)(position+0x20)=14;
    actor[0x2b]=3;*(int16_t *)(ai+0x16a)=-2;mode[0xb]=5;
    SudekiMpLanStorySwitchMember member;
    assert(SudekiMpLanStoryObserverSwitchMember(2,&member) && member.position[0]==12 && member.position[2]==14 &&
        member.pause_refs==3 && member.ai_override==-2 && member.ai_mode==5);
    *(void **)(ai+0x10)=NULL;assert(!SudekiMpLanStoryObserverSwitchMember(2,&member));*(void **)(ai+0x10)=actor;
    *(void **)(ai+0x3c)=(void *)1;assert(!SudekiMpLanStoryObserverSwitchMember(2,&member));
    VirtualFree(base,0,MEM_RELEASE);puts("StoryTransitionArmingTest PASS: actual TEMP/EXIT arming and deadline");return 0;
}
