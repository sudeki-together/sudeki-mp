#ifndef SUDEKIMP_LAN_STORY_SPLIT_H
#define SUDEKIMP_LAN_STORY_SPLIT_H
#include <windows.h>
#include <stdint.h>

/* Host-only split-area owner for saved-story (first milestone: the host's
 * lead uses an authored TEMP door while other party characters stay in the
 * exterior). Joins three existing pieces under one policy:
 *  - story observer split callbacks (exterior epoch retained, lead inside);
 *  - lan_story_temp_exterior keep-live consumer (no whole-exterior freeze);
 *  - CGroupPlayers::SetModeLeadOnly/SetModeFullParty entry wrappers that
 *    exempt the outside characters from the script's follower suspension.
 *    The exemption removes exactly the one reference native lead-only added
 *    (native single-node resume) and restores it immediately before the
 *    matching full-party release (native single-node pause), so native
 *    counters stay balanced.
 * Not a background loader, client door path or collision filter. Remote
 * players cannot initiate doors; the host must be the one entering. */
BOOL SudekiMpLanStorySplitInstall(HMODULE);
/* Refused while a split, exemption or kept exterior is outstanding. */
BOOL SudekiMpLanStorySplitUninstall(void);
/* Host runtime, once per native dispatch. outside_mask: characters that may
 * stay outside (remote-controlled, currently held by their players). */
void SudekiMpLanStorySplitSetEligibility(BOOL host_ready,uint8_t outside_mask);
BOOL SudekiMpLanStorySplitActive(void);
struct SudekiMpLanStoryNativeRoster;
/* Read-only proof for an already leased exterior follower while the host is
 * settled inside TEMP. Revalidates the observer roster, retained world/party
 * identities and this owner's exact lead-only exemption. Entry/exit gaps,
 * replacements and non-exempt actors refuse. Does not grant input authority,
 * change native pause counters or establish a controller dispatch witness. */
BOOL SudekiMpLanStorySplitExteriorActorExact(
    const struct SudekiMpLanStoryNativeRoster *roster,unsigned character);
/* Read-only native identity proof for the same exterior follower after this
 * owner's matching balanced TEMP exit and positive ordinary settlement.
 * Expires after five seconds; native EXIT gaps and any replacement refuse.
 * Does not grant action authority or bypass native body/world pauses. */
BOOL SudekiMpLanStorySplitReturnedActorExact(
    const struct SudekiMpLanStoryNativeRoster *roster,unsigned character);
/* Game-thread plain-state retirement when actual host input readiness resumes.
 * Preserves an active TEMP visit's provisional receipt; uninstall retires all
 * state. SetEligibility's ready means scene eligibility, not input readiness,
 * and therefore cannot retire a completed-return token itself. */
void SudekiMpLanStorySplitClearReturned(void);
/* Read-only game-thread admission for the opt-in native TEMP test start.
 * Requires an installed/attached split owner and a held outside character;
 * it neither starts a split nor changes native state. */
BOOL SudekiMpLanStorySplitTestCanEnterTemporary(const char *zone,unsigned leader_character);
/* Host world capture: native zone data whose authored spawns belong to the
 * occupied TEMP while a split is active, else NULL. Copy-only identity. */
const void *SudekiMpLanStorySplitTemporaryData(void);
/* H1b, host native thread: show a member standing inside the lead's own
 * shop (settled split), undoing the door script's party hide for that member
 * only (body via the ClrROFlags export, weapon render objects in native
 * order; the native hide depth is left for ShowPartyMembers). Called every
 * frame while inside (upkeep). 1 shown/visible, 0 not yet, -1 refused. */
int SudekiMpLanStorySplitShowMember(const struct SudekiMpLanStoryNativeRoster *roster,unsigned character);
/* Undo a show while the native hide is still outstanding (or retire it when
 * the native show already ran, or the world/group/member is gone). Safe to
 * call for a member without a show. Never calls the observer. */
void SudekiMpLanStorySplitReleaseMember(unsigned character,const char *reason);
BOOL SudekiMpLanStorySplitMemberLeasesHeld(void);
#endif
