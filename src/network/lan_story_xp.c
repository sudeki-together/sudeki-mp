#include "network/lan_story_xp.h"
#include <math.h>
#include <string.h>
static void put(uint8_t *b,uint32_t v) {for(unsigned i=0;i<4;++i)b[i]=(uint8_t)(v>>(8*i));}
static uint32_t get(const uint8_t *b) {return b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);}
static BOOL finite_range(float f,float lo,float hi) {return isfinite(f) && f>=lo && f<=hi;}
BOOL SudekiMpStoryXpValid(const SudekiMpStoryXp *s) {
    if(!s || !s->epoch || !s->revision || !s->stream || !s->current[0])return FALSE;
    unsigned end=0;
    for(;end<40 && s->current[end];++end)
        if(s->current[end]<0x20 || s->current[end]>0x7e)return FALSE;
    if(end==40)return FALSE;
    for(;end<40;++end)if(s->current[end])return FALSE;
    for(unsigned i=0;i<SUDEKIMP_STORY_XP_SLOTS;++i) {
        const SudekiMpStoryXpPopup *p=s->slots+i;
        if(p->amount>9999 || !finite_range(p->height,0,10000) ||
            !finite_range(p->duration,0,60) || !finite_range(p->elapsed,0,p->duration) ||
            (p->serial && p->duration<=0))return FALSE;
        for(unsigned k=0;k<3;++k)if(!finite_range(p->position[k],-1000000,1000000))return FALSE;
        if(!p->serial && (p->amount || p->height || p->elapsed || p->duration ||
            p->position[0] || p->position[1] || p->position[2]))return FALSE;
    }
    return TRUE;
}
BOOL SudekiMpStoryXpEncode(const SudekiMpStoryXp *s,uint8_t *b,size_t capacity,size_t *written) {
    if(written)*written=0;
    if(!b || !written || capacity<SUDEKIMP_STORY_XP_SIZE || !SudekiMpStoryXpValid(s))return FALSE;
    put(b,s->epoch);put(b+4,s->revision);put(b+8,s->tick);put(b+12,s->sequence);put(b+16,s->stream);
    memcpy(b+20,s->current,40);
    for(unsigned i=0;i<SUDEKIMP_STORY_XP_SLOTS;++i) {
        const SudekiMpStoryXpPopup *p=s->slots+i;uint8_t *out=b+60+32*i;
        put(out,p->serial);put(out+4,p->amount);
        float values[6]={p->position[0],p->position[1],p->position[2],p->height,p->elapsed,p->duration};
        for(unsigned k=0;k<6;++k) {uint32_t bits;memcpy(&bits,values+k,4);put(out+8+4*k,bits);}
    }
    *written=SUDEKIMP_STORY_XP_SIZE;return TRUE;
}
BOOL SudekiMpStoryXpDecode(const uint8_t *b,size_t n,SudekiMpStoryXp *out) {
    if(!b || !out || n!=SUDEKIMP_STORY_XP_SIZE)return FALSE;
    SudekiMpStoryXp s={.epoch=get(b),.revision=get(b+4),.tick=get(b+8),.sequence=get(b+12),.stream=get(b+16)};
    memcpy(s.current,b+20,40);
    for(unsigned i=0;i<SUDEKIMP_STORY_XP_SLOTS;++i) {
        const uint8_t *in=b+60+32*i;SudekiMpStoryXpPopup *p=s.slots+i;
        p->serial=get(in);p->amount=get(in+4);float values[6];
        for(unsigned k=0;k<6;++k) {uint32_t bits=get(in+8+4*k);memcpy(values+k,&bits,4);}
        memcpy(p->position,values,12);p->height=values[3];p->elapsed=values[4];p->duration=values[5];
    }
    if(!SudekiMpStoryXpValid(&s))return FALSE;
    *out=s;return TRUE;
}
