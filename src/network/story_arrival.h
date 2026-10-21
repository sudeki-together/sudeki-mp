#ifndef SUDEKIMP_STORY_ARRIVAL_H
#define SUDEKIMP_STORY_ARRIVAL_H
#include "network/lan_party_story.h"
#include <string.h>
/* Closed castle arrival and its reverse. Transport admission, fresh frames,
 * actor generations and native containment remain separate mandatory proofs. */
static inline int SudekiMpStoryArrivalSolo(const SudekiMpLanStoryScene *s) {
    return s && s->phase==SUDEKIMP_LAN_STORY_READY && s->available_mask==4u &&
        s->leader_seat==2u && s->inside_mask==4u &&
        !strcmp(s->world,"newbrightwater") && !strcmp(s->temporary,"lica_tals_room");
}
static inline int SudekiMpStoryArrivalPair(const SudekiMpLanStoryScene *s) {
    return s && s->phase==SUDEKIMP_LAN_STORY_READY && s->available_mask==12u &&
        (s->leader_seat==2u || s->leader_seat==3u) && !s->temporary[0] && !strcmp(s->world,"newbrightwater");
}
static inline int SudekiMpStoryArrivalEdge(const SudekiMpLanStoryScene *before,
    const SudekiMpLanStoryScene *after) {
    return before && after && before->epoch && before->epoch<UINT32_MAX &&
        after->epoch==before->epoch+1u && after->revision>before->revision &&
        ((SudekiMpStoryArrivalPair(before) && SudekiMpStoryArrivalSolo(after)) ||
         (SudekiMpStoryArrivalSolo(before) && SudekiMpStoryArrivalPair(after)) ||
         (SudekiMpStoryArrivalPair(before) && SudekiMpStoryArrivalPair(after) &&
          before->leader_seat!=after->leader_seat));
}
#endif
