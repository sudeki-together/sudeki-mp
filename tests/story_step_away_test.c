#include "network/lan_story_step_away.h"
#include "hooks/story_control_epoch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void judge(void) {
    const int ages[]={-17,-16,0,250,251,10000};
    for(unsigned valid=0;valid<2;++valid) for(unsigned ready=0;ready<2;++ready)
    for(unsigned epoch=0;epoch<2;++epoch) for(unsigned rev=0;rev<2;++rev)
    for(unsigned combat=0;combat<3;++combat) for(unsigned i=0;i<6;++i) {
        SudekiMpStoryCombatSample s={10000u,2,3,(uint8_t)valid,(uint8_t)combat};
        SudekiMpStoryCombat want=valid && ready && epoch && rev && combat<2u && ages[i]>=-16 && ages[i]<=250?
            (combat?SUDEKIMP_STORY_COMBAT_YES:SUDEKIMP_STORY_COMBAT_NO):SUDEKIMP_STORY_COMBAT_UNKNOWN;
        assert(SudekiMpStoryCombatJudge(&s,10000u+ages[i],ready,epoch?2:4,rev?3:4)==want);
    }
    assert(SudekiMpStoryCombatJudge(NULL,0,1,2,3)==SUDEKIMP_STORY_COMBAT_UNKNOWN);
    SudekiMpStoryCombatSample wrap={UINT32_MAX-50,2,3,1,1};
    assert(SudekiMpStoryCombatJudge(&wrap,20,1,2,3)==SUDEKIMP_STORY_COMBAT_YES);
}
static void decisions(void) {
    /* Full cross product, not merely the happy-path ordering. */
    for(unsigned phase=0;phase<=4;++phase) for(unsigned combat=0;combat<=2;++combat)
    for(unsigned flags=0;flags<512;++flags) {
        SudekiMpStoryMenuInput s={.phase=phase,.combat=combat,.last_ui_at=900,
            .ui_seen=!!(flags&1),.party_menu=!!(flags&2),.main_menu=!!(flags&4),
            .owned=!!(flags&8),.admitted=!!(flags&16),.quick_menu=!!(flags&32),
            .shop=!!(flags&64),.examine=!!(flags&128),.cinematic=!!(flags&256)};
        unsigned want=SUDEKIMP_STORY_M_IGNORE;
        if(!s.ui_seen) {
            if(phase>=1 && phase<=3) want=s.party_menu?SUDEKIMP_STORY_M_IGNORE:SUDEKIMP_STORY_M_RETURN;
            else if(phase!=4) {
                if(s.main_menu) want=SUDEKIMP_STORY_M_CLOSE;
                else if(s.owned && s.admitted) {
                    if(combat==2) want=(flags&480)?SUDEKIMP_STORY_M_IGNORE:SUDEKIMP_STORY_M_STEP_AWAY;
                    else want=combat==1?SUDEKIMP_STORY_M_OPEN:SUDEKIMP_STORY_M_WAIT;
                }
            }
        }
        assert((unsigned)SudekiMpStoryAwayMenuDecision(&s,1000)==want);
    }
    SudekiMpStoryMenuInput s={.owned=1,.admitted=1,.ui_seen=1,.last_ui_at=100,.combat=SUDEKIMP_STORY_COMBAT_YES};
    assert(SudekiMpStoryAwayMenuDecision(&s,399)==SUDEKIMP_STORY_M_IGNORE);
    assert(SudekiMpStoryAwayMenuDecision(&s,400)==SUDEKIMP_STORY_M_STEP_AWAY);
}
static void phases(void) {
    SudekiMpStoryAwayClient s={0}; char text[128];
    SudekiMpStoryAwayIntent(&s,1,100); assert(s.phase==SUDEKIMP_STORY_AWAY_REQUESTING && s.want_away);
    SudekiMpStoryAwayObserve(&s,1,0,SUDEKIMP_PARTY_CONTROL_HUMAN,1,200); assert(s.phase==1);
    SudekiMpStoryAwayObserve(&s,1,1,SUDEKIMP_PARTY_CONTROL_DRAINING,1,201); assert(s.phase==1);
    SudekiMpStoryAwayObserve(&s,1,1,SUDEKIMP_PARTY_CONTROL_DRAINING,0,202); assert(s.phase==2);
    SudekiMpStoryAwayObserve(&s,1,1,SUDEKIMP_PARTY_CONTROL_AI,0,203); assert(s.phase==3);
    for(unsigned phase=1;phase<=3;++phase) {
        s.phase=phase; s.want_away=1;
        SudekiMpStoryAwayIntent(&s,0,1000); assert(s.phase==4 && !s.want_away);
        SudekiMpStoryAwayIntent(&s,0,2000); assert(s.return_at==1000);
        assert(!SudekiMpStoryAwayObserve(&s,1,0,SUDEKIMP_PARTY_CONTROL_AI,0,5999));
        assert(SudekiMpStoryAwayObserve(&s,1,0,SUDEKIMP_PARTY_CONTROL_AI,0,6000));
        assert(s.phase==3 && s.want_away);
    }
    SudekiMpStoryAwayIntent(&s,0,7000);
    SudekiMpStoryAwayObserve(&s,1,0,SUDEKIMP_PARTY_CONTROL_HUMAN,0,7000); assert(s.phase==4);
    SudekiMpStoryAwayObserve(&s,1,0,SUDEKIMP_PARTY_CONTROL_HUMAN,1,7001); assert(!s.phase && !s.want_away);
    for(unsigned phase=0;phase<=4;++phase) {
        s.phase=phase; s.want_away=1;
        SudekiMpStoryAwayObserve(&s,0,1,SUDEKIMP_PARTY_CONTROL_AI,0,8000); assert(!s.phase && !s.want_away);
        SudekiMpStoryAwayBanner(phase,2,text,sizeof(text)); assert(!!text[0]==!!phase);
        for(const unsigned char *p=(unsigned char *)text;*p;++p) assert(*p<128);
    }
    SudekiMpStoryAwayBanner(SUDEKIMP_STORY_AWAY_AWAY,2,text,sizeof(text));
    assert(!strcmp(text,"Tal: AI-controlled - press M to return"));
    SudekiMpStoryAwayRefused(&s,0); assert(!s.phase && !s.want_away);
    SudekiMpStoryAwayRefused(&s,1); assert(s.phase==3 && s.want_away);
}
static void admission(void) {
    SudekiMpPartyAssignment a={.world=1,.revision=1,.generation={1,1,1,1},.humans=3,.available=15,.character={2,0,4,4}};
    SudekiMpPartyOwnership s; assert(SudekiMpPartyOwnershipInitialize(&s,&a));
    SudekiMpStoryAwayAdmission in={.same_area=1};
    assert(SudekiMpStoryAwayAdmit(&s,1,1,&in)==SUDEKIMP_PARTY_SWAP_OK);
    in.same_area=0;
    assert(SudekiMpStoryAwayAdmit(&s,1,1,&in)==SUDEKIMP_PARTY_AWAY_OTHER_AREA);
    in.same_area=1; in.swap_involves=1; assert(SudekiMpStoryAwayAdmit(&s,1,1,&in)==SUDEKIMP_PARTY_SWAP_BUSY);
    in.swap_involves=0; in.prepare=1; assert(SudekiMpStoryAwayAdmit(&s,1,1,&in)==SUDEKIMP_PARTY_SWAP_BUSY);
    in.prepare=0; s.controlling=1; s.control[1]=SUDEKIMP_PARTY_CONTROL_ACQUIRING;
    assert(SudekiMpStoryAwayAdmit(&s,1,1,&in)==SUDEKIMP_PARTY_SWAP_BUSY);
    s.controlling=3; s.control[1]=SUDEKIMP_PARTY_CONTROL_WAIT_ACK;
    assert(SudekiMpStoryAwayAdmit(&s,1,1,&in)==SUDEKIMP_PARTY_SWAP_BUSY);
    in.same_area=0; in.prepare=1; in.swap_involves=1;
    assert(SudekiMpStoryAwayAdmit(&s,1,0,&in)==SUDEKIMP_PARTY_SWAP_OK);
    s.away=2; s.control[1]=SUDEKIMP_PARTY_CONTROL_DRAINING;
    assert(SudekiMpStoryAwayAdmit(&s,1,0,&in)==SUDEKIMP_PARTY_SWAP_OK);
    assert(SudekiMpStoryAwayAdmit(&s,1,1,&in)==SUDEKIMP_PARTY_SWAP_OK);
    assert(SudekiMpStoryAwayAdmit(&s,0,1,&in)==SUDEKIMP_PARTY_SWAP_UNAUTHORIZED);
    assert(SudekiMpStoryAwayAdmit(&s,4,1,&in)==SUDEKIMP_PARTY_SWAP_UNAUTHORIZED);
}
static void epoch(void) {
    for(unsigned observed=0;observed<=8;++observed) for(unsigned flags=0;flags<8;++flags)
        assert(!!SudekiMpStoryControlEpochMayRebind(4,observed,flags&1,flags&2,flags&4)==(observed>4 && flags==7));
    assert(!SudekiMpStoryControlEpochMayRebind(0,1,1,1,1));
}
int main(void) { epoch(); judge(); decisions(); phases(); admission(); puts("story step-away policy tests passed"); return 0; }
