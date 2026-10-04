#ifndef SUDEKIMP_LAN_PARTY_FOLLOW_H
#define SUDEKIMP_LAN_PARTY_FOLLOW_H
#include "hooks/lan_party_control.h"
/* Host-only native Formation/IdleFormation targeting. No actor, controller or
 * formation identity is changed; existing combat/path/interaction rules stay
 * native. Publish only positive active HUMAN characters, canonical mask. */
BOOL SudekiMpLanPartyFollowInstall(HMODULE);
BOOL SudekiMpLanPartyFollowPublish(const SudekiMpControlUpdateDispatchWitness *,
    uint8_t active_characters,unsigned host_character);
void SudekiMpLanPartyFollowClear(void);
BOOL SudekiMpLanPartyFollowUninstall(void);
#ifdef SUDEKIMP_LAN_PARTY_FOLLOW_TESTING
void *SudekiMpLanPartyFollowTestLeader(void *ai,void *native_leader);
float *SudekiMpLanPartyFollowTestGoal(void *formation,float *out,void *actor);
float SudekiMpLanPartyFollowTestDistance(void *formation,void *actor);
void SudekiMpLanPartyFollowTestFacing(void *ai,float out[3]);
#endif
#endif
