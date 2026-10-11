/* Portable host-authored pose interpolation only; no native execution. */
#include "network/lan_story_world_frame.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    for(unsigned character=0;character<4;++character) {
        SudekiMpLanStoryWorldFrame a={.epoch=1,.revision=2,.host_tick=1000,.sequence=10,.count=1};
        SudekiMpLanStoryWorldActor *x=&a.actors[0];
        *x=(SudekiMpLanStoryWorldActor){.kind=SUDEKIMP_LAN_STORY_WORLD_PC_KIND,
            .identifier=SudekiMpLanStoryWorldCharacterIdentifier(character),.generation=1,
            .animation_sequence=10,.bank_fingerprint=99,.submodels=1,.forward={0,0,1},
            .clip={10,11,12,13},.rate={24,24,12,12},.time={4,4,2,2},.blend={.99f,0,.2f}};
        BOOL ranged=character==1 || character==3;
        if(ranged) {x->clip[4]=14;x->state[4]=1;x->rate[4]=24;x->time[4]=4;x->blend[3]=1;}
        SudekiMpLanStoryWorldFrame b=a,out;
        b.host_tick=1020;b.sequence=11;
        SudekiMpLanStoryWorldActor *y=&b.actors[0];
        ++y->animation_sequence;y->blend[2]=.4f;
        for(unsigned k=0;k<4;++k) y->time[k]+=.4f;
        /* Recoil replacement for ranged PCs; an ordinary native loop edge
         * for melee PCs. Neither changes the underlying locomotion tree. */
        if(ranged) {y->clip[4]=15;y->time[4]=0;}
        else y->time[3]=0;
        assert(SudekiMpLanStoryWorldFrameInterpolate(&a,&b,1010,&out));
        assert(fabsf(out.actors[0].blend[2]-.3f)<.00001f);
        assert(fabsf(out.actors[0].time[0]-4.2f)<.00001f);
        assert(out.actors[0].animation_sequence==x->animation_sequence);
        if(ranged) assert(out.actors[0].clip[4]==14 && out.actors[0].time[4]==4);
        else assert(out.actors[0].time[3]==2); /* no guessed wrap length */
        assert(SudekiMpLanStoryWorldFrameInterpolate(&a,&b,1020,&out));
        assert(!memcmp(&out,&b,sizeof(out)));
        /* Changed subtree identities/states stay discrete, but the other
         * pair can still advance. Never blend through a reaction/clip cut. */
        y->clip[2]=19;
        assert(SudekiMpLanStoryWorldFrameInterpolate(&a,&b,1010,&out));
        assert(out.actors[0].clip[2]==12 && out.actors[0].blend[2]==x->blend[2]);
        assert(fabsf(out.actors[0].time[0]-4.2f)<.00001f);
        y->clip[2]=x->clip[2];++y->clip_occurrence[2];
        assert(SudekiMpLanStoryWorldFrameInterpolate(&a,&b,1010,&out) && out.actors[0].blend[2]==x->blend[2]);
        y->clip_occurrence[2]=0;y->state[2]=1;
        assert(SudekiMpLanStoryWorldFrameInterpolate(&a,&b,1010,&out) && out.actors[0].blend[2]==x->blend[2]);
        y->state[2]=0;++y->generation;
        assert(!SudekiMpLanStoryWorldFrameInterpolate(&a,&b,1010,&out));
        --y->generation;++y->bank_fingerprint;
        assert(!SudekiMpLanStoryWorldFrameInterpolate(&a,&b,1010,&out));
        --y->bank_fingerprint;++b.epoch;
        assert(!SudekiMpLanStoryWorldFrameInterpolate(&a,&b,1010,&out));
        printf("PASS roster %u: continuous independent fade, discrete changed channel, owner/bank/scene refusal\n",character);
    }
    return 0;
}
