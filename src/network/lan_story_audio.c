#include "network/lan_story_audio.h"
#include <string.h>
#include <math.h>
static void put(uint8_t *b,uint32_t v) {for(unsigned i=0;i<4;++i)b[i]=(uint8_t)(v>>(8*i));}
static uint32_t get(const uint8_t *b) {return b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);}
BOOL SudekiMpStoryAudioValid(const SudekiMpStoryAudio *s) {
    if(!s || !s->stream || !s->bank || s->character>=4 || s->count>SUDEKIMP_STORY_AUDIO_CAPACITY) return FALSE;
    for(unsigned i=0;i<s->count;++i) {
        const SudekiMpStoryAudioCue *c=s->cues+i; float gain;memcpy(&gain,&c->gain,4);
        if(!c->id || !c->hash || c->index>=8192 || c->mode>1 || c->spatial>1 ||
            !isfinite(gain) || gain<0 || gain>1 || (int32_t)(s->tick-c->started)<0) return FALSE;
        for(unsigned j=0;j<i;++j)if(s->cues[j].id==c->id)return FALSE;
    }
    return TRUE;
}
BOOL SudekiMpStoryAudioEncode(const SudekiMpStoryAudio *s,uint8_t *b,size_t capacity,size_t *written) {
    if(!b || !written || !SudekiMpStoryAudioValid(s)) return FALSE;
    size_t size=32u+20u*s->count;if(capacity<size)return FALSE;memset(b,0,size);
    put(b,s->epoch);put(b+4,s->revision);put(b+8,s->tick);put(b+12,s->sequence);
    put(b+16,s->stream);put(b+20,s->bank);b[24]=s->character;b[25]=s->count;
    for(unsigned i=0;i<s->count;++i) {
        const SudekiMpStoryAudioCue *c=s->cues+i;uint8_t *p=b+32+20*i;
        put(p,c->id);put(p+4,c->started);put(p+8,c->hash);put(p+12,c->gain);
        p[16]=(uint8_t)c->index;p[17]=(uint8_t)(c->index>>8);p[18]=c->mode;p[19]=c->spatial;
    }
    *written=size;return TRUE;
}
BOOL SudekiMpStoryAudioDecode(const uint8_t *b,size_t size,SudekiMpStoryAudio *s) {
    if(!b || !s || size<32 || b[25]>SUDEKIMP_STORY_AUDIO_CAPACITY || size!=32u+20u*b[25])return FALSE;
    for(unsigned i=26;i<32;++i)if(b[i])return FALSE;
    SudekiMpStoryAudio v={.epoch=get(b),.revision=get(b+4),.tick=get(b+8),.sequence=get(b+12),
        .stream=get(b+16),.bank=get(b+20),.character=b[24],.count=b[25]};
    for(unsigned i=0;i<v.count;++i) {
        const uint8_t *p=b+32+20*i;v.cues[i]=(SudekiMpStoryAudioCue){get(p),get(p+4),get(p+8),get(p+12),
            (uint16_t)(p[16]|((uint16_t)p[17]<<8)),p[18],p[19]};
    }
    if(!SudekiMpStoryAudioValid(&v))return FALSE;
    *s=v;return TRUE;
}
