#ifndef SUDEKIMP_LAN_STORY_REMOTE_INTERIOR_H
#define SUDEKIMP_LAN_STORY_REMOTE_INTERIOR_H
#include <windows.h>
#include <stdint.h>
#include "hooks/lan_story_observer.h"
#include "network/lan_story_area_state.h"
/* H1b shop visits. Owner decisions: 2026-10-16 "Tal goes in alone"; 2026-10-17
 * story houses stay vanilla (the host lead goes in alone, others wait
 * outside), and any player may walk into a shop (door table) on their own,
 * several at once; players in the same shop see each other (D11, replacing
 * the first per-visitor instancing). One house at a time (issue #83):
 * members use the same shop, or the lead's own shop.
 *
 * Host, per member: walking onto a table shop's entry trigger starts a
 * visit. With the lead outside, the shop is background-loaded with
 * SwitchZoneNOW (resident, state 2, never current), its script lighting is
 * held, the exterior renderer is kept active and formation snap is switched
 * off; with the lead inside that shop (current), nothing loads. After the
 * client's fade the member is placed at the arrival spot (native setter
 * 0x3050) and the observer scene marks them inside (exterior epoch kept).
 * The shop's exit trigger reverses it at the door's return point. When the
 * last member leaves, the shared state is restored and a background shop is
 * unloaded. Any other zone call (including the lead's exit, which unloads a
 * house natively) evicts every member first (observer callback).
 *
 * Client: when the host scene marks this player's character inside a known
 * interior, the client enters it locally (native EnterTemporaryZone with the
 * shop marker) and leaves with ExitTemporaryZone; it fades to black on its
 * own door contact, with the door script's spot cue, and shows the zone-name
 * banner after the fade-in. Party members in the same shop are presented
 * normally (D11); while the lead is inside, the host shows those members to
 * her (split owner). While it holds a local entry, the area follower stands
 * down.
 *
 * Doors need a measured arrival spot and return point; only the shops in the
 * table are handled. Native thread only. */
BOOL SudekiMpLanStoryRemoteInteriorInitialize(HMODULE image);
/* Host, once per native frame with an exact READY roster. remote_characters:
 * characters driven by connected remote players; input_fresh: those whose
 * player's control input arrived recently (a member inside without input
 * for 15 s is ended: its client cannot present the house). */
void SudekiMpLanStoryRemoteInteriorHostService(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryScene *scene,uint8_t remote_characters,uint8_t input_fresh,uint32_t now);
/* Client, native thread: host = newest host scene, native = this client's
 * own observed scene, chosen = this player's character. */
void SudekiMpLanStoryRemoteInteriorClientService(const SudekiMpLanStoryScene *host,
    const SudekiMpStoryAreaState *area,const SudekiMpLanStoryScene *native,unsigned chosen,uint32_t now);
BOOL SudekiMpLanStoryRemoteInteriorClientHolds(void);
/* Client, every native frame regardless of host-scene availability: a door
 * fade-out never outlives its timeout. */
void SudekiMpLanStoryRemoteInteriorClientTick(uint32_t now);
/* Runtime teardown (host and client), native thread, before the observer and
 * sky owners uninstall: members back outside, shared native state restored,
 * background house unloaded, client fade balanced. */
void SudekiMpLanStoryRemoteInteriorReset(void);
/* TRUE once after this client entered or left a house locally: the owned
 * camera must be reseated at its character (its eye is far away). */
BOOL SudekiMpLanStoryRemoteInteriorClientTakeReseat(void);
/* TRUE once when this client's own door touch started a fade: the door
 * script's spot cue to play now (exact case; never NULL when TRUE). */
BOOL SudekiMpLanStoryRemoteInteriorClientTakeDoorCue(const char **cue);
/* Host: the zone data of the house this owner keeps background-resident
 * for members (state 2, not draining), else NULL. Not cached: re-derived. */
const void *SudekiMpLanStoryRemoteInteriorHostHouseData(void);
/* Client: the zone data of the shop this client entered and settled in
 * (its current zone), else NULL. */
const void *SudekiMpLanStoryRemoteInteriorClientShopData(void);
/* Client: the static camera position name the settled house's cluster
 * script would set (NULL when none or not settled in a table house). */
const char *SudekiMpLanStoryRemoteInteriorClientHouseCamera(void);
/* Client: the table-shop zone this client is settled in, else NULL. */
const char *SudekiMpLanStoryRemoteInteriorClientShopZone(void);
/* Client: the host no longer marks this player inside, but the local house
 * is still entered (an exit is due). */
BOOL SudekiMpLanStoryRemoteInteriorClientWantsOut(void);
/* Client: while TRUE, a due exit (and its fade) waits; a native modal of
 * this client is open in the house and must close first. */
void SudekiMpLanStoryRemoteInteriorClientHoldExit(BOOL hold);
/* Registry entity by ResourceName (cached pointer re-proved each call, full
 * scan at most every 2 s) and its position component; native thread. */
const void *SudekiMpLanStoryRemoteInteriorEntityNamed(const char *name,const void *cached,DWORD *last_scan,uint32_t now);
BOOL SudekiMpLanStoryRemoteInteriorEntityPosition(const void *entity,float out[3]);
/* Host, native thread. While the lead's own entry reloads a house a member
 * was already inside, the member's body is parked at the door outside:
 * HeldPose gives the spot they stood on (published instead of the parked
 * body, for that actor and scene epoch) and Parked tells the steering to
 * stay neutral until they are placed back there. */
BOOL SudekiMpLanStoryRemoteInteriorHeldPose(unsigned character,const void *actor,uint32_t epoch,float out[3]);
BOOL SudekiMpLanStoryRemoteInteriorMemberParked(unsigned character);
/* Host, native thread: may `character` keep steering while the lead's door
 * script enters `zone`, the house kept background-resident for a visit?
 * (Inside that visit and not leaving or parked, or outside it.) */
BOOL SudekiMpLanStoryRemoteInteriorHostLeadEntryMember(const SudekiMpLanStoryNativeRoster *roster,unsigned character,const char *zone);
/* Host, native thread: `character` is settled inside a background visit
 * kept by this module (not leaving, not parked): a body of its own. */
BOOL SudekiMpLanStoryRemoteInteriorHostVisitSettled(const SudekiMpLanStoryNativeRoster *roster,unsigned character);
#endif
