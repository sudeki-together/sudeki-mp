#ifndef SUDEKIMP_LAN_STORY_MELEE_H
#define SUDEKIMP_LAN_STORY_MELEE_H

#include "network/lan_story_handoff.h"
#include <string.h>

/* Saved-story melee only. A sample carries the three native two-bit button
 * states together. Recent changes ride every datagram; admission never waits
 * for an attack result. This is bounded loss recovery, not a reliable stream. */
#define SUDEKIMP_STORY_MELEE_HISTORY 16u
#define SUDEKIMP_STORY_MELEE_WIRE_SIZE (36u+12u*SUDEKIMP_STORY_MELEE_HISTORY)
typedef struct SudekiMpStoryMeleeSample {
    uint32_t serial,acknowledged_frame;
    uint16_t age;
    uint8_t buttons;
} SudekiMpStoryMeleeSample;
typedef struct SudekiMpLanStoryMelee {
    SudekiMpLanStoryControlFence fence;
    uint32_t sequence,acknowledged_frame,cancel;
    uint8_t steady,count;
    SudekiMpStoryMeleeSample samples[SUDEKIMP_STORY_MELEE_HISTORY];
} SudekiMpLanStoryMelee;
static inline uint8_t SudekiMpStoryMeleeAdvance(unsigned buttons) {
    unsigned out=0;
    for(unsigned i=0;i<3u;++i) {
        unsigned v=(buttons>>(2u*i))&3u;
        out|=(v==1u?2u:v==3u?0u:v)<<(2u*i);
    }
    return (uint8_t)out;
}
static inline unsigned SudekiMpStoryMeleePress(unsigned buttons) {
    return ((buttons>>4u)&3u)==1u?3u:(buttons&3u)==1u?1u:
        ((buttons>>2u)&3u)==1u?2u:0u;
}
static inline BOOL SudekiMpLanStoryMeleeValid(const SudekiMpLanStoryMelee *v) {
    if(!v || !SudekiMpLanStoryControlFenceValid(&v->fence) ||
        (v->fence.character!=0u && v->fence.character!=2u) ||
        !v->sequence || !v->acknowledged_frame || v->steady>63u ||
        SudekiMpStoryMeleeAdvance(v->steady)!=v->steady || v->count>SUDEKIMP_STORY_MELEE_HISTORY) return FALSE;
    uint32_t previous=v->cancel;
    for(unsigned i=0;i<v->count;++i) {
        const SudekiMpStoryMeleeSample *s=&v->samples[i];
        if(s->serial<=previous || !s->acknowledged_frame || s->buttons>63u ||
            s->age>SUDEKIMP_STORY_CONTROL_MAX_AGE_MS ||
            (i && v->samples[i-1u].age<s->age)) return FALSE;
        previous=s->serial;
    }
    return TRUE;
}
static inline void story_melee_put32(uint8_t *p,uint32_t v) {
    for(unsigned i=0;i<4u;++i) p[i]=(uint8_t)(v>>(8u*i));
}
static inline uint32_t story_melee_get32(const uint8_t *p) {
    return p[0]|((uint32_t)p[1]<<8u)|((uint32_t)p[2]<<16u)|((uint32_t)p[3]<<24u);
}
static inline BOOL SudekiMpLanStoryMeleeEncode(const SudekiMpLanStoryMelee *v,uint8_t *p,size_t n) {
    if(!p || n!=SUDEKIMP_STORY_MELEE_WIRE_SIZE || !SudekiMpLanStoryMeleeValid(v)) return FALSE;
    memset(p,0,n);
    if(!SudekiMpLanStoryControlFenceEncode(&v->fence,p,20u)) return FALSE;
    story_melee_put32(p+20,v->sequence);story_melee_put32(p+24,v->acknowledged_frame);
    story_melee_put32(p+28,v->cancel);p[32]=v->steady;p[33]=v->count;
    for(unsigned i=0;i<v->count;++i) {
        const SudekiMpStoryMeleeSample *s=&v->samples[i];uint8_t *q=p+36u+12u*i;
        story_melee_put32(q,s->serial);story_melee_put32(q+4,s->acknowledged_frame);
        q[8]=(uint8_t)s->age;q[9]=(uint8_t)(s->age>>8);q[10]=s->buttons;
    }
    return TRUE;
}
static inline BOOL SudekiMpLanStoryMeleeDecode(const uint8_t *p,size_t n,SudekiMpLanStoryMelee *out) {
    SudekiMpLanStoryMelee v={0};
    if(!p || !out || n!=SUDEKIMP_STORY_MELEE_WIRE_SIZE || p[34] || p[35] ||
        !SudekiMpLanStoryControlFenceDecode(p,20u,&v.fence)) return FALSE;
    v.sequence=story_melee_get32(p+20);v.acknowledged_frame=story_melee_get32(p+24);
    v.cancel=story_melee_get32(p+28);v.steady=p[32];v.count=p[33];
    if(v.count>SUDEKIMP_STORY_MELEE_HISTORY) return FALSE;
    for(unsigned i=0;i<SUDEKIMP_STORY_MELEE_HISTORY;++i) {
        const uint8_t *q=p+36u+12u*i;
        if(q[11]) return FALSE;
        if(i>=v.count) {for(unsigned j=0;j<11u;++j) if(q[j]) return FALSE;continue;}
        v.samples[i]=(SudekiMpStoryMeleeSample){story_melee_get32(q),story_melee_get32(q+4),
            (uint16_t)(q[8]|((unsigned)q[9]<<8)),q[10]};
    }
    if(!SudekiMpLanStoryMeleeValid(&v)) return FALSE;
    *out=v;return TRUE;
}

typedef struct SudekiMpStoryMeleeSender {
    uint32_t sequence,serial,cancel;
    uint8_t last,count;
    BOOL active;
    struct {SudekiMpStoryMeleeSample sample;uint32_t at;} history[SUDEKIMP_STORY_MELEE_HISTORY];
} SudekiMpStoryMeleeSender;
static inline void SudekiMpStoryMeleeCancel(SudekiMpStoryMeleeSender *s) {
    s->cancel=s->serial;s->count=0;s->active=FALSE;s->last=0;
}
static inline BOOL SudekiMpStoryMeleeBuild(SudekiMpStoryMeleeSender *s,
    const SudekiMpLanStoryControlFence *f,uint32_t frame,uint32_t now,unsigned buttons,
    BOOL active,SudekiMpLanStoryMelee *out) {
    if(!s || !out || !frame || buttons>63u || s->sequence==UINT32_MAX || s->serial==UINT32_MAX) return FALSE;
    if(!active) {SudekiMpStoryMeleeCancel(s);buttons=0;}
    if(active && (!s->active || buttons!=s->last)) {
        if(s->count==SUDEKIMP_STORY_MELEE_HISTORY) {
            memmove(s->history,s->history+1,(--s->count)*sizeof(*s->history));
        }
        unsigned i=s->count++;
        s->history[i].sample=(SudekiMpStoryMeleeSample){++s->serial,frame,0,(uint8_t)buttons};
        s->history[i].at=now;
    }
    s->active=active;s->last=(uint8_t)buttons;
    *out=(SudekiMpLanStoryMelee){.fence=*f,.sequence=++s->sequence,
        .acknowledged_frame=frame,.cancel=s->cancel,.steady=SudekiMpStoryMeleeAdvance(buttons)};
    for(unsigned i=0;i<s->count;++i) {
        uint32_t age=now-s->history[i].at;
        if(age>SUDEKIMP_STORY_CONTROL_MAX_AGE_MS) continue;
        out->samples[out->count]=s->history[i].sample;
        out->samples[out->count++].age=(uint16_t)age;
    }
    return SudekiMpLanStoryMeleeValid(out);
}

typedef struct SudekiMpStoryMeleeReceiver {
    uint32_t sequence,serial,cancel,received_at,dropped;
    uint8_t steady,count;
    BOOL muted,known;
    struct {SudekiMpStoryMeleeSample sample;uint32_t at;} queue[SUDEKIMP_STORY_MELEE_HISTORY];
} SudekiMpStoryMeleeReceiver;
/* Retain replay floors through focus/menu/presence loss. Only an observed
 * neutral sample rearms this same control generation. */
static inline void SudekiMpStoryMeleeMute(SudekiMpStoryMeleeReceiver *s) {
    s->count=0;s->steady=0;s->known=FALSE;s->muted=TRUE;
}
static inline BOOL SudekiMpStoryMeleeReceive(SudekiMpStoryMeleeReceiver *s,
    const SudekiMpLanStoryMelee *v,uint32_t now,uint32_t host_frame) {
    if(!SudekiMpLanStoryMeleeValid(v) || v->sequence<=s->sequence || v->cancel<s->cancel) return FALSE;
    s->sequence=v->sequence;s->cancel=v->cancel;s->received_at=now;s->known=TRUE;
    if(s->serial<v->cancel) s->serial=v->cancel;
    unsigned keep=0;
    for(unsigned i=0;i<s->count;++i) if(s->queue[i].sample.serial>v->cancel) s->queue[keep++]=s->queue[i];
    s->count=(uint8_t)keep;
    for(unsigned i=0;i<v->count;++i) {
        const SudekiMpStoryMeleeSample *p=&v->samples[i];
        if(p->serial<=s->serial) continue;
        if(p->serial>s->serial+1u) {s->dropped+=p->serial-s->serial-1u;s->count=0;}
        s->serial=p->serial;
        if(s->muted || !host_frame || (int32_t)(p->acknowledged_frame-host_frame)>0 ||
            host_frame-p->acknowledged_frame>8u) {++s->dropped;continue;}
        if(s->count==SUDEKIMP_STORY_MELEE_HISTORY) {++s->dropped;continue;}
        s->queue[s->count].sample=*p;s->queue[s->count++].at=now-p->age;
    }
    if(s->muted && !v->steady && (!v->count || !v->samples[v->count-1u].buttons)) s->muted=FALSE;
    s->steady=s->muted?0:v->steady;
    return TRUE;
}
static inline BOOL SudekiMpStoryMeleeTake(SudekiMpStoryMeleeReceiver *s,uint32_t now,
    uint32_t host_frame,uint8_t *buttons,uint32_t *serial) {
    *buttons=0;*serial=0;
    if(!s->known || (int32_t)(now-s->received_at)<-16 ||
        (int32_t)(now-s->received_at)>(int32_t)SUDEKIMP_STORY_CONTROL_MAX_AGE_MS) {
        SudekiMpStoryMeleeMute(s);return FALSE;
    }
    while(s->count) {
        SudekiMpStoryMeleeSample p=s->queue[0].sample;uint32_t at=s->queue[0].at;
        memmove(s->queue,s->queue+1,(--s->count)*sizeof(*s->queue));
        if((int32_t)(now-at)<-16 || (int32_t)(now-at)>(int32_t)SUDEKIMP_STORY_CONTROL_MAX_AGE_MS ||
            (int32_t)(p.acknowledged_frame-host_frame)>0 || host_frame-p.acknowledged_frame>8u) {++s->dropped;continue;}
        *buttons=p.buttons;*serial=p.serial;return TRUE;
    }
    *buttons=s->steady;return !s->muted;
}
#endif
