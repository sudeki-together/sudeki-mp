#ifndef SUDEKIMP_LAN_PARTY_SNAPSHOT_H
#define SUDEKIMP_LAN_PARTY_SNAPSHOT_H
#include "hooks/lan_party_control.h"
#include "hooks/lan_party_host_control.h"
void SudekiMpLanPartyCaptureReset(void);
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
/* Conservative movement-only cleanup proof. No party runtime creates native
 * cast/camera/effect tasks yet; if ANY native Spirit is active or unknown,
 * release waits for it. This is not the future concurrent-cast drain policy. */
BOOL SudekiMpLanPartyMovementDrained(const SudekiMpLanPartyLease *,void *,
    const SudekiMpControlUpdateDispatchWitness *);
/* Stricter native action retirement gate for releasing a retained actor lease.
 * Unknown melee/ranged presentation is not completion. */
BOOL SudekiMpLanPartyCombatActionsDrained(const SudekiMpLanPartyLease *,void *,
    const SudekiMpControlUpdateDispatchWitness *);
#endif
