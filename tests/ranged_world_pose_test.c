#include "engine/ranged_world_pose.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
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
