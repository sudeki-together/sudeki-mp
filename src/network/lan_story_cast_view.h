#ifndef SUDEKIMP_LAN_STORY_CAST_VIEW_H
#define SUDEKIMP_LAN_STORY_CAST_VIEW_H
#include "network/lan_story_frame.h"
#include "network/lan_story_handoff.h"
#include "network/lan_story_area_state.h"
/* Additive, pointer-free host camera observation. Existing messages are
 * unchanged. Remote roster Skill/Spirit casts, fenced to their controlling
 * player; no native camera commands. */
#define SUDEKIMP_STORY_CAST_VIEW_SIZE (128u+SUDEKIMP_STORY_AREA_NAME)
#define SUDEKIMP_STORY_CAST_VIEW_HISTORY 8u
typedef struct SudekiMpStoryCastView {
    SudekiMpLanStoryControlFence fence;
    uint64_t stream;
    uint32_t sequence,tick,revision;
    uint16_t cast_sequence;
    uint8_t kind; /* 0 positively normal, 1 InitCam, 2 SkillCam. */
    char current[SUDEKIMP_STORY_AREA_NAME];
    SudekiMpLanStoryView view;
} SudekiMpStoryCastView;
BOOL SudekiMpStoryCastViewValid(const SudekiMpStoryCastView *s);
BOOL SudekiMpStoryCastViewEncode(const SudekiMpStoryCastView *s,uint8_t *bytes,size_t size);
BOOL SudekiMpStoryCastViewDecode(const uint8_t *bytes,size_t size,SudekiMpStoryCastView *s);
typedef struct SudekiMpStoryCastViewHistory {
    SudekiMpStoryCastView samples[SUDEKIMP_STORY_CAST_VIEW_HISTORY];
    unsigned count;
} SudekiMpStoryCastViewHistory;
/* Caller has authenticated source, scene and control ownership. Reordering
 * never rewinds playback. A new ownership namespace discards old history. */
BOOL SudekiMpStoryCastViewPush(SudekiMpStoryCastViewHistory *h,const SudekiMpStoryCastView *s);
/* Sample on the presented world timeline; never show a future start/end.
 * Unknown/stale is FALSE, never an invented normal-camera observation. */
BOOL SudekiMpStoryCastViewSample(const SudekiMpStoryCastViewHistory *h,uint32_t tick,
    SudekiMpStoryCastView *out);
#endif
