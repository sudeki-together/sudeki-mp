#ifndef SUDEKIMP_LAN_STORY_AVATAR_PARTY_ROSTER_H
#define SUDEKIMP_LAN_STORY_AVATAR_PARTY_ROSTER_H

#include "hooks/lan_story_avatar_party.h"

/* A fresh, nonrecursive native-party owner check for the initial all-avatar
 * exterior route. This does not grant a controller dispatch or pause lease;
 * callers retain their Observer/presentation checks. Mixed hero/avatar groups
 * are deliberately not admitted here. */
static inline BOOL SudekiMpLanStoryAvatarPartyRosterExact(const SudekiMpLanStoryNativeRoster *r) {
    SudekiMpLanStoryAvatarPartyObservation p;
    if(!r || r->available_mask || r->leader_character!=SUDEKIMP_LAN_STORY_NO_SEAT ||
        !r->epoch || !r->revision || !r->native_leader ||
        !r->native_avatar_generation || r->native_avatar_player>=4u) return FALSE;
    for(unsigned c=0;c<4u;++c) if(r->actors[c] || r->ai[c]) return FALSE;
    if(!SudekiMpLanStoryAvatarPartyObserve(&p) || p.phase!=SUDEKIMP_AVATAR_PARTY_READY ||
        p.epoch!=r->epoch || p.hero_mask || p.leader_character!=SUDEKIMP_LAN_STORY_NO_SEAT ||
        p.world!=r->world || p.group!=r->group || p.controller!=r->controller ||
        p.native_leader!=r->native_leader || p.local_player!=r->native_avatar_player ||
        p.spawn_generation!=r->native_avatar_generation || p.member_count!=1u ||
        p.members[0]!=r->native_leader) return FALSE;
    for(unsigned c=0;c<4u;++c) if(p.heroes[c] || (c && p.members[c])) return FALSE;
    return TRUE;
}

#endif
