#include "network/lan_story_cast_view.h"
#include <string.h>
static void put(uint8_t *p,uint32_t x) {for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(x>>(8*i));}
static uint32_t get(const uint8_t *p) {return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
BOOL SudekiMpStoryCastViewValid(const SudekiMpStoryCastView *s) {
    if(!s || !s->stream || !s->sequence || !SudekiMpLanStoryControlFenceValid(&s->fence) ||
        s->revision<s->fence.revision || s->kind>2u || !SudekiMpLanStoryViewValid(&s->view) ||
        s->view.valid!=!!s->kind || (s->kind && (!s->cast_sequence ||
            s->view.camera_serial!=((uint32_t)s->cast_sequence*4u+s->kind)))) return FALSE;
    unsigned end=0;
    for(;end<sizeof(s->current) && s->current[end];++end)
        if(s->current[end]<0x20 || s->current[end]>0x7e) return FALSE;
    if(!end || end==sizeof(s->current)) return FALSE;
    for(;end<sizeof(s->current);++end) if(s->current[end]) return FALSE;
    return TRUE;
}
BOOL SudekiMpStoryCastViewEncode(const SudekiMpStoryCastView *s,uint8_t *p,size_t n) {
    if(!p || n!=SUDEKIMP_STORY_CAST_VIEW_SIZE || !SudekiMpStoryCastViewValid(s)) return FALSE;
    memset(p,0,n); put(p,1); put(p+4,s->fence.epoch); put(p+8,s->fence.revision);
    put(p+12,s->fence.transaction); put(p+16,s->fence.actor_generation);
    p[20]=s->fence.player; p[21]=s->fence.character; p[22]=s->kind;
    put(p+24,(uint32_t)s->stream); put(p+28,(uint32_t)(s->stream>>32));
    put(p+32,s->sequence); put(p+36,s->tick); p[40]=(uint8_t)s->cast_sequence;
    p[41]=(uint8_t)(s->cast_sequence>>8);
    put(p+44,s->revision);
    memcpy(p+48,s->current,sizeof(s->current));
    put(p+48+SUDEKIMP_STORY_AREA_NAME,s->view.camera_serial);
    for(unsigned i=0;i<19;++i) {
        uint32_t bits; const float *f=i<16?&s->view.matrix[i]:&s->view.projection[i-16];
        memcpy(&bits,f,4); put(p+52+SUDEKIMP_STORY_AREA_NAME+4*i,bits);
    }
    return TRUE;
}
BOOL SudekiMpStoryCastViewDecode(const uint8_t *p,size_t n,SudekiMpStoryCastView *out) {
    if(!p || !out || n!=SUDEKIMP_STORY_CAST_VIEW_SIZE || get(p)!=1 || p[23] || p[42] || p[43]) return FALSE;
    SudekiMpStoryCastView s={0};
    s.fence=(SudekiMpLanStoryControlFence){get(p+4),get(p+8),get(p+12),get(p+16),p[20],p[21]};
    s.kind=p[22]; s.stream=(uint64_t)get(p+24)|((uint64_t)get(p+28)<<32);
    s.sequence=get(p+32); s.tick=get(p+36); s.cast_sequence=(uint16_t)(p[40]|p[41]<<8);
    s.revision=get(p+44); memcpy(s.current,p+48,sizeof(s.current)); s.view.valid=!!s.kind;
    s.view.camera_serial=get(p+48+SUDEKIMP_STORY_AREA_NAME);
    for(unsigned i=0;i<19;++i) {
        uint32_t bits=get(p+52+SUDEKIMP_STORY_AREA_NAME+4*i);
        memcpy(i<16?&s.view.matrix[i]:&s.view.projection[i-16],&bits,4);
    }
    if(!SudekiMpStoryCastViewValid(&s)) return FALSE;
    *out=s; return TRUE;
}
BOOL SudekiMpStoryCastViewPush(SudekiMpStoryCastViewHistory *h,const SudekiMpStoryCastView *s) {
    if(!h || h->count>SUDEKIMP_STORY_CAST_VIEW_HISTORY || !SudekiMpStoryCastViewValid(s)) return FALSE;
    if(h->count) {
        const SudekiMpStoryCastView *last=&h->samples[h->count-1];
        if(!SudekiMpLanArenaSequenceNewer(s->sequence,last->sequence) || s->stream<last->stream ||
            (int32_t)(s->tick-last->tick)<0) return FALSE;
        if(s->revision!=last->revision || s->stream!=last->stream || !SudekiMpLanStoryControlFenceSame(&s->fence,&last->fence) ||
            strcmp(s->current,last->current)) h->count=0;
        else if(s->cast_sequence<last->cast_sequence) return FALSE;
    }
    if(h->count==SUDEKIMP_STORY_CAST_VIEW_HISTORY) {
        memmove(h->samples,h->samples+1,(--h->count)*sizeof(*s));
    }
    h->samples[h->count++]=*s; return TRUE;
}
BOOL SudekiMpStoryCastViewSample(const SudekiMpStoryCastViewHistory *h,uint32_t tick,SudekiMpStoryCastView *out) {
    if(!h || !out || !h->count || h->count>SUDEKIMP_STORY_CAST_VIEW_HISTORY) return FALSE;
    int lower=-1;
    for(unsigned i=0;i<h->count;++i) if((int32_t)(tick-h->samples[i].tick)>=0) lower=(int)i;
    if(lower<0) return FALSE;
    SudekiMpStoryCastView s=h->samples[lower];
    if(tick-s.tick>SUDEKIMP_STORY_CONTROL_MAX_AGE_MS) return FALSE;
    if((unsigned)(lower+1)<h->count) {
        const SudekiMpStoryCastView *next=&h->samples[lower+1];
        if(s.cast_sequence==next->cast_sequence && s.kind==next->kind &&
            !SudekiMpLanStoryViewInterpolate(&s.view,&next->view,s.tick,next->tick,tick,&s.view)) return FALSE;
    }
    *out=s; return TRUE;
}
