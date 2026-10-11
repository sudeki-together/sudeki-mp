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
    assert(SudekiMpRangedWorldSemantic(0x36)==0x2a);
    for(unsigned hit=0x2a;hit<0x36;++hit)
        assert(SudekiMpRangedWorldSemantic(hit)==hit);
    assert(SudekiMpRangedWorldSemantic(0x8c)==0x85);
    assert(SudekiMpRangedWorldSemantic(0x8d)==0x86);
    assert(SudekiMpRangedWorldSemantic(0x8e)==0x87);
    assert(SudekiMpRangedWorldSemantic(6)==6);
    assert(SudekiMpRangedWorldSemantic(0xc1)==0xc1); /* no invented reload */
    for(unsigned quarter=0;quarter<4;++quarter) {
        const float headings[4][3]={{0,0,1},{1,0,0},{0,0,-1},{-1,0,0}};
        const float *aim=headings[quarter];unsigned motion=0;
        assert(SudekiMpRangedWorldMoveSemantic(aim,aim[0],aim[2],&motion) && motion==6);
        assert(SudekiMpRangedWorldMoveSemantic(aim,-aim[0],-aim[2],&motion) && motion==8);
        /* Native targeting right is (-forward_z, forward_x). The Test
         * Room submits mode3 for that travel; C3B70 -> state4 -> E3D10 C/D.
         * Its opposite submits mode2 -> state3 -> A/B. */
        assert(SudekiMpRangedWorldMoveSemantic(aim,-aim[2],aim[0],&motion) && motion==0xc);
        assert(SudekiMpRangedWorldMoveSemantic(aim,aim[2],-aim[0],&motion) && motion==0xa);
    }
    unsigned motion=77;
    assert(SudekiMpRangedWorldMoveSemantic((float[]){0,.6f,.8f},-1,.5f,&motion) && motion==0xc);
    assert(SudekiMpRangedWorldMoveSemantic((float[]){0,-.6f,.8f},.5f,-1,&motion) && motion==8);
    motion=77;
    assert(!SudekiMpRangedWorldMoveSemantic((float[]){0,0,1},0,0,&motion) && motion==77);
    assert(!SudekiMpRangedWorldMoveSemantic((float[]){0,1,0},1,0,&motion) && motion==77);
    assert(!SudekiMpRangedWorldMoveSemantic((float[]){NAN,0,1},1,0,&motion) && motion==77);
    assert(!SudekiMpRangedWorldMoveSemantic((float[]){0,0,1},INFINITY,0,&motion) && motion==77);
    /* Authored Ailish directional lengths/speed multipliers. The earlier
     * forward-rate projection made a side cycle .582s instead of .835s.
     * Both channels must finish a cycle together at every native blend. */
    float rates[2]={-1,-1};
    assert(SudekiMpRangedWorldGaitRates((float[]){24,20},(uint16_t[]){0x3c00,0x3c00},.99f,rates));
    assert(fabsf(24/rates[0]-.835f)<.00001f && fabsf(20/rates[1]-.835f)<.00001f);
    assert(SudekiMpRangedWorldGaitRates((float[]){24,18},(uint16_t[]){0x3c00,0x3c00},.99f,rates));
    assert(fabsf(24/rates[0]-.7525f)<.00001f && fabsf(18/rates[1]-.7525f)<.00001f);
    for(unsigned i=0;i<=100;++i) {
        float blend=i/100.f;
        assert(SudekiMpRangedWorldGaitRates((float[]){24,20},(uint16_t[]){0x3c00,0x3c00},blend,rates));
        assert(fabsf(24/rates[0]-20/rates[1])<.00001f);
        assert(fabsf(24/rates[0]-(1-blend/6))<.00001f);
    }
    assert(SudekiMpRangedWorldGaitRates((float[]){24,18},(uint16_t[]){0x3c00,0x3d31},.99f,rates));
    assert(fabsf(24/rates[0]-.58210f)<.0001f);
    float kept=rates[0];
    assert(!SudekiMpRangedWorldGaitRates((float[]){0,20},(uint16_t[]){0x3c00,0x3c00},.5f,rates));
    assert(!SudekiMpRangedWorldGaitRates((float[]){NAN,20},(uint16_t[]){0x3c00,0x3c00},.5f,rates));
    for(unsigned bad=0;bad<4;++bad) {
        const uint16_t invalid[]={0,0xbc00,0x7c00,0x7fff};
        assert(!SudekiMpRangedWorldGaitRates((float[]){24,20},(uint16_t[]){0x3c00,invalid[bad]},.5f,rates));
    }
    assert(!SudekiMpRangedWorldGaitRates((float[]){24,20},(uint16_t[]){0x3c00,0x3c00},NAN,rates));
    assert(!SudekiMpRangedWorldGaitRates((float[]){24,20},(uint16_t[]){0x3c00,0x3c00},1.1f,rates));
    assert(rates[0]==kept);
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
