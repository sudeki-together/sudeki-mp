#ifndef SUDEKIMP_STORY_SWITCH_TRACE_H
#define SUDEKIMP_STORY_SWITCH_TRACE_H
#include <stdint.h>
enum { SUDEKIMP_SWITCH_TRACE_MS=20000u, SUDEKIMP_SWITCH_TRACE_SAMPLES=64u };
typedef struct SudekiMpStorySwitchTrace {
    uint32_t window,last; unsigned samples,state; int state_known;
} SudekiMpStorySwitchTrace;
/* Plain-data cadence/budget; every sample emits at most four member rows. */
static inline int SudekiMpStorySwitchTraceTake(SudekiMpStorySwitchTrace *s,
    uint32_t window,uint32_t now,int client,unsigned state) {
    if(!window || now-window>=SUDEKIMP_SWITCH_TRACE_MS) return 0;
    if(s->window!=window) *s=(SudekiMpStorySwitchTrace){.window=window};
    int changed=!s->state_known || s->state!=state;
    s->state=state;s->state_known=1;
    if(client && state==4u) return 0;
    if(s->samples>=SUDEKIMP_SWITCH_TRACE_SAMPLES ||
        (s->samples && now-s->last<1000u && !(client && changed))) return 0;
    ++s->samples;s->last=now;return 1;
}
#endif
