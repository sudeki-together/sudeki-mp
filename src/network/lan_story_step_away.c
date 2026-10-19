#include "network/lan_story_step_away.h"
#include <stdio.h>
#include <string.h>

SudekiMpStoryCombat SudekiMpStoryCombatJudge(const SudekiMpStoryCombatSample *s,
    uint32_t now,int ready,uint32_t epoch,uint32_t revision) {
    if(!s || !s->valid || !ready || !epoch || !revision || s->epoch!=epoch ||
        s->revision!=revision || s->combat_mode>1u) return SUDEKIMP_STORY_COMBAT_UNKNOWN;
    int32_t age=(int32_t)(now-s->receipt);
    if(age< -16 || age>250) return SUDEKIMP_STORY_COMBAT_UNKNOWN;
    return s->combat_mode?SUDEKIMP_STORY_COMBAT_YES:SUDEKIMP_STORY_COMBAT_NO;
}
SudekiMpStoryMenuDecision SudekiMpStoryAwayMenuDecision(const SudekiMpStoryMenuInput *s,uint32_t now) {
    if(!s || (s->ui_seen && (uint32_t)(now-s->last_ui_at)<300u)) return SUDEKIMP_STORY_M_IGNORE;
    if(s->phase==SUDEKIMP_STORY_AWAY_REQUESTING || s->phase==SUDEKIMP_STORY_AWAY_HANDING_OVER ||
        s->phase==SUDEKIMP_STORY_AWAY_AWAY) return s->party_menu?SUDEKIMP_STORY_M_IGNORE:SUDEKIMP_STORY_M_RETURN;
    if(s->phase==SUDEKIMP_STORY_AWAY_RETURNING) return SUDEKIMP_STORY_M_IGNORE;
    if(s->main_menu) return SUDEKIMP_STORY_M_CLOSE;
    if(!s->owned || !s->admitted) return SUDEKIMP_STORY_M_IGNORE;
    if(s->combat==SUDEKIMP_STORY_COMBAT_YES)
        return (s->quick_menu || s->shop || s->examine || s->cinematic)?
            SUDEKIMP_STORY_M_IGNORE:SUDEKIMP_STORY_M_STEP_AWAY;
    return s->combat==SUDEKIMP_STORY_COMBAT_NO?SUDEKIMP_STORY_M_OPEN:SUDEKIMP_STORY_M_WAIT;
}
void SudekiMpStoryAwayIntent(SudekiMpStoryAwayClient *s,int away,uint32_t now) {
    if(!s) return;
    if(away) {
        if(s->want_away) return;
        s->want_away=1; s->phase=SUDEKIMP_STORY_AWAY_REQUESTING;
    } else if(s->phase!=SUDEKIMP_STORY_AWAY_NONE && s->phase!=SUDEKIMP_STORY_AWAY_RETURNING) {
        s->want_away=0; s->return_at=now; s->phase=SUDEKIMP_STORY_AWAY_RETURNING;
    }
}
int SudekiMpStoryAwayObserve(SudekiMpStoryAwayClient *s,int connected,int away,
    SudekiMpPartyControlPhase control,int drives,uint32_t now) {
    if(!s) return 0;
    if(!connected) { memset(s,0,sizeof(*s)); return 0; }
    if(s->phase==SUDEKIMP_STORY_AWAY_RETURNING) {
        if(!away && control==SUDEKIMP_PARTY_CONTROL_HUMAN && drives) {
            memset(s,0,sizeof(*s)); return 0;
        }
        if((uint32_t)(now-s->return_at)>=5000u) {
            s->want_away=1; s->phase=SUDEKIMP_STORY_AWAY_AWAY; return 1;
        }
    } else if(s->want_away) {
        s->phase=!away || drives?SUDEKIMP_STORY_AWAY_REQUESTING:
            control==SUDEKIMP_PARTY_CONTROL_AI?SUDEKIMP_STORY_AWAY_AWAY:SUDEKIMP_STORY_AWAY_HANDING_OVER;
    }
    return 0;
}
void SudekiMpStoryAwayRefused(SudekiMpStoryAwayClient *s,int away) {
    if(!s) return;
    memset(s,0,sizeof(*s)); s->want_away=(uint8_t)!!away;
    s->phase=away?SUDEKIMP_STORY_AWAY_AWAY:SUDEKIMP_STORY_AWAY_NONE;
}
void SudekiMpStoryAwayBanner(SudekiMpStoryAwayPhase phase,unsigned character,char *out,size_t n) {
    static const char *const names[4]={"Buki","Elco","Tal","Ailish"};
    if(!out || !n) return;
    const char *name=character<4u?names[character]:"Character";
    switch(phase) {
    case SUDEKIMP_STORY_AWAY_REQUESTING: snprintf(out,n,"Stepping away - waiting for the host..."); break;
    case SUDEKIMP_STORY_AWAY_HANDING_OVER: snprintf(out,n,"%s: handing over to AI...",name); break;
    case SUDEKIMP_STORY_AWAY_AWAY: snprintf(out,n,"%s: AI-controlled - press M to return",name); break;
    case SUDEKIMP_STORY_AWAY_RETURNING: snprintf(out,n,"Returning control..."); break;
    default: out[0]=0; break;
    }
}
SudekiMpPartySwapResult SudekiMpStoryAwayAdmit(const SudekiMpPartyOwnership *s,unsigned p,
    int away,const SudekiMpStoryAwayAdmission *a) {
    if(!s || !a || p<1u || p>3u || !SudekiMpPartyOwnershipValid(s) ||
        !(s->connected&(1u<<p)) || s->assignment.character[p]>=4u || (away!=0 && away!=1))
        return SUDEKIMP_PARTY_SWAP_UNAUTHORIZED;
    /* A return and an idempotent request never depend on combat or area. */
    if(!away || !!(s->away&(1u<<p))==away) return SUDEKIMP_PARTY_SWAP_OK;
    if(!a->same_area) return SUDEKIMP_PARTY_AWAY_OTHER_AREA;
    if(a->swap_involves || a->prepare || s->control[p]==SUDEKIMP_PARTY_CONTROL_ACQUIRING ||
        s->control[p]==SUDEKIMP_PARTY_CONTROL_WAIT_ACK) return SUDEKIMP_PARTY_SWAP_BUSY;
    return SUDEKIMP_PARTY_SWAP_OK;
}
