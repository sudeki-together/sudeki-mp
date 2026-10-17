/* H1b: a member already inside goes back to where they stood after the
 * lead's own entry reloads the house. Pure. */
#include "hooks/lan_story_rejoin.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>

int main(void) {
    float out[3]; const float lead[3]={0.08f,0.11f,0.98f};
    const float desk[3]={0.55f,0.08f,-1.86f};
    assert(SudekiMpLanStoryRejoinSpot(desk,lead,1.2f,out)==SUDEKIMP_REJOIN_RECORDED &&
        out[0]==desk[0] && out[1]==desk[1] && out[2]==desk[2]);
    const float near_lead[3]={0.38f,0.10f,0.98f};      /* 0.3 from the lead along +x */
    assert(SudekiMpLanStoryRejoinSpot(near_lead,lead,1.2f,out)==SUDEKIMP_REJOIN_CLEARED);
    assert(fabsf(out[0]-(0.08f+1.2f))<1e-5f && fabsf(out[2]-0.98f)<1e-5f && out[1]==near_lead[1]);
    const float on_lead[3]={0.08f,0.2f,0.98f};
    assert(SudekiMpLanStoryRejoinSpot(on_lead,lead,1.2f,out)==SUDEKIMP_REJOIN_CLEARED);
    float dx=out[0]-lead[0],dz=out[2]-lead[2];
    assert(fabsf(sqrtf(dx*dx+dz*dz)-1.2f)<1e-5f && out[1]==on_lead[1]);
    assert(SudekiMpLanStoryRejoinSpot(near_lead,NULL,1.2f,out)==SUDEKIMP_REJOIN_RECORDED && out[0]==near_lead[0]);
    const float bad[3]={NAN,0,0};
    assert(SudekiMpLanStoryRejoinSpot(bad,lead,1.2f,out)==SUDEKIMP_REJOIN_INVALID);
    assert(SudekiMpLanStoryRejoinSpot(NULL,lead,1.2f,out)==SUDEKIMP_REJOIN_INVALID);
    const float lead_nan[3]={NAN,0,NAN};
    assert(SudekiMpLanStoryRejoinSpot(desk,lead_nan,1.2f,out)==SUDEKIMP_REJOIN_RECORDED);
    puts("Rejoin spot: recorded kept, overlap pushed to clearance, height kept, invalid refused: PASS");
    puts("StoryRejoinSpotTest: PASS");
    return 0;
}
