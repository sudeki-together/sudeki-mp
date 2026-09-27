#ifndef SUDEKIMP_LAN_PARTY_MOTION_H
#define SUDEKIMP_LAN_PARTY_MOTION_H
#include "network/lan_party_session.h"

/* Retail noncombat bank descriptions, keyed by CHARACTER, never player or
 * authority. Closed diagnostic locomotion surface; no combat/action fallback. */
typedef struct SudekiMpLanPartyMotion {
    int primary,secondary,state;
    float primary_rate,secondary_rate;
    BOOL moving,ranged;
} SudekiMpLanPartyMotion;
BOOL SudekiMpLanPartyMotionDescribe(uint8_t actor_type,uint8_t animation,
    SudekiMpLanPartyMotion *motion);
BOOL SudekiMpLanPartyMotionObserve(uint8_t actor_type,int primary,
    uint8_t *animation);
/* SMP4 v2 noncombat phase namespace in the existing bounded locomotion
 * payload: empty, idle, walk, run, idle variant one, idle variant two.
 * This is NOT LA42's combat clip namespace. The movement profile validates
 * it separately and never forwards it to the legacy two-seat writer. */
unsigned SudekiMpLanPartyMotionChannels(uint8_t type);
int SudekiMpLanPartyMotionSelector(uint8_t type,unsigned clip);
BOOL SudekiMpLanPartyMotionValid(uint8_t type,const SudekiMpLanArenaLocomotion *);
/* Elco's gun-play idle temporarily changes attachment. Once it has left
 * BOTH crossfade pairs, the replica must run the native interruption cleanup.
 * Unknown motion never grants cleanup authority. */
BOOL SudekiMpLanPartyMotionElcoIdleEnded(uint8_t type,
    const SudekiMpLanArenaLocomotion *motion);
BOOL SudekiMpLanPartyMotionCapture(uint8_t type,const int selectors[4],
    const uint8_t states[4],const float rates[4],const float times[4],
    const float blends[3],const SudekiMpLanArenaLocomotion *previous,
    SudekiMpLanArenaLocomotion *out);
/* Closed basic-combat selector namespace carried by the existing LA42
 * locomotion record. It reuses verified actor-local selector identities; it
 * does not grant input, combo, weapon, or damage authority. */
int SudekiMpLanPartyCombatMotionSelector(uint8_t type,unsigned clip);
BOOL SudekiMpLanPartyCombatMotionValid(uint8_t type,
    const SudekiMpLanArenaLocomotion *motion);
BOOL SudekiMpLanPartyCombatMotionCapture(uint8_t type,const int selectors[4],
    const uint8_t states[4],const float rates[4],const float times[4],
    const float blends[3],const SudekiMpLanArenaLocomotion *previous,
    SudekiMpLanArenaLocomotion *out);
/* Convert a verified fifth-channel Elco/Ailish firing observation into the
 * bounded ranged action event. Unknown families fail; native selectors never
 * come from client input. */
BOOL SudekiMpLanPartyRangedActionObserve(uint8_t type,int selector,
    uint8_t state,uint8_t *action);
/* Positive native terminal observation for a previously applied actor-local
 * action. Unknown variants/selectors/states never authorize lease release. */
BOOL SudekiMpLanPartyActionTerminalObserved(uint8_t type,uint8_t variant,
    int selector,uint8_t state);
BOOL SudekiMpLanPartyRangedActionChannelDrained(uint8_t type,int selector,
    uint8_t state);
BOOL SudekiMpLanPartyMovementFrameValid(const SudekiMpLanPartyFrame *frame);
BOOL SudekiMpLanPartyBasicCombatFrameValid(const SudekiMpLanPartyFrame *frame);
#endif
