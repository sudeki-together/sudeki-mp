#ifndef SUDEKIMP_LAN_PARTY_SNAPSHOT_H
#define SUDEKIMP_LAN_PARTY_SNAPSHOT_H
#include "hooks/lan_party_control.h"
#include "hooks/lan_party_host_control.h"
void SudekiMpLanPartyCaptureReset(void);
/* Existing exact native emission observer, game thread only. Caller proves
 * the active host Elco lease; inputs never manufacture journal entries. */
void SudekiMpLanPartyCaptureRangedShot(const SudekiMpLanPartyLease *,void *actor);
/* Read-only game-thread capture for initial four-window movement validation.
 * Requires all four exact native identities before AND after observing them.
 * Refuses active/unknown skills, Spirit, combat or unknown animation banks.
 * Host transport assigns sequence and each peer's admitted-input ACK. */
BOOL SudekiMpLanPartyCaptureMovement(const SudekiMpControlUpdateDispatchWitness *,
    SudekiMpLanPartySession *, uint32_t now, SudekiMpLanPartyFrame *);
/* Bounded combat capture. It observes all four leased native actors and the
 * host-only training dummy journal; it never accepts client damage state. */
BOOL SudekiMpLanPartyCaptureBasicCombat(const SudekiMpControlUpdateDispatchWitness *,
    SudekiMpLanPartySession *,SudekiMpLanPartyHostControl *,uint32_t now,
    SudekiMpLanPartyFrame *);
/* Full cleanup proof for movement-only transitions and actor release. Retains
 * native task/camera/effect obligations, including the private lighting tail.
 * Combat capture/playback use their narrower presentation proof below. */
BOOL SudekiMpLanPartyMovementDrained(const SudekiMpLanPartyLease *,void *,
    const SudekiMpControlUpdateDispatchWitness *);
/* Combat snapshot/renderer handoff after positive native body/task cleanup.
 * An independently owned lighting fade may remain. This is not release proof. */
BOOL SudekiMpLanPartyCombatPresentationReady(const SudekiMpLanPartyLease *,void *,
    const SudekiMpControlUpdateDispatchWitness *);
/* Stricter native action retirement gate for releasing a retained actor lease.
 * Unknown melee/ranged presentation is not completion. */
BOOL SudekiMpLanPartyCombatActionsDrained(const SudekiMpLanPartyLease *,void *,
    const SudekiMpControlUpdateDispatchWitness *);
#endif
