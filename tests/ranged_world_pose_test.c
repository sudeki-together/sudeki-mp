#include "engine/ranged_world_pose.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    float prior[3]={1,0,0},root[3]={9,9,9};
    for(int i=-89;i<=89;++i) {
        float pitch=i*.01745329252f,aim[3]={0,sinf(pitch),cosf(pitch)};
        assert(SudekiMpRangedWorldRoot(aim,prior,root));
        assert(root[0]==0 && root[1]==0 && fabsf(root[2]-1)<.00001f);
        assert(fabsf(aim[1]-sinf(pitch))<.00001f); /* full aim not discarded */
    }
    assert(SudekiMpRangedWorldRoot((float[]){0,1,0},prior,root) && root[0]==1 && root[1]==0);
    assert(SudekiMpRangedWorldRoot((float[]){0,-1,0},prior,root) && root[0]==1 && root[1]==0);
    assert(!SudekiMpRangedWorldRoot((float[]){0,1,0},(float[]){0,1,0},root) && root[0]==1);
    assert(!SudekiMpRangedWorldRoot((float[]){NAN,0,1},prior,root) && root[0]==1);
    assert(!SudekiMpRangedWorldRoot((float[]){0,0,0},prior,root) && root[0]==1);
    assert(!SudekiMpRangedWorldRoot(NULL,prior,root));
    assert(SudekiMpRangedWorldSemantic(5)==2);
    assert(SudekiMpRangedWorldSemantic(0x8c)==0x85);
    assert(SudekiMpRangedWorldSemantic(0x8d)==0x86);
    assert(SudekiMpRangedWorldSemantic(0x8e)==0x87);
    assert(SudekiMpRangedWorldSemantic(6)==6);
    assert(SudekiMpRangedWorldSemantic(0xc1)==0xc1); /* no invented reload */
    float t=-1,r=-1;
    assert(SudekiMpRangedWorldClock(60,55,30,12,&t,&r));
    assert(fabsf(t-27.5f)<.0001f && fabsf(r-11)<.0001f);
    assert(SudekiMpRangedWorldClock(4,26,4,24,&t,&r));
    assert(t==26 && r==156);
    assert(SudekiMpRangedWorldClock(4,26,0,0,&t,&r));
    assert(t==0 && r==0);
    assert(!SudekiMpRangedWorldClock(0,55,0,12,&t,&r));
    assert(!SudekiMpRangedWorldClock(60,0,0,12,&t,&r));
    assert(!SudekiMpRangedWorldClock(60,55,61,12,&t,&r));
    assert(!SudekiMpRangedWorldClock(60,55,-1,12,&t,&r));
    assert(!SudekiMpRangedWorldClock(NAN,55,0,12,&t,&r));
    assert(!SudekiMpRangedWorldClock(60,55,0,INFINITY,&t,&r));
    assert(!SudekiMpRangedWorldClock(60,55,0,12,NULL,&r));
    puts("ranged world semantic/clock tests passed");
    return 0;
}
