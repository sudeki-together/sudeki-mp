#ifndef SUDEKIMP_LAN_STORY_DEV_PROTECT_H
#define SUDEKIMP_LAN_STORY_DEV_PROTECT_H
#include "hooks/lan_story_observer.h"
#include <windows.h>
/* Dev Play ([DevPlay] PartyInvulnerable=true, saved-story host only): the
 * party heroes and the Dev Play ally hold the native arbiter invulnerability
 * refcount (CCharacterArbiter::SetInvulnerable, RVA 0xdca10) while they exist,
 * so enemies can be fought without taking damage. Native refcount leases only:
 * acquire is proven by ref+1 and flag 0x800, release by ref-1; an entity that
 * is gone is dropped, never written. No damage code is patched. */
BOOL SudekiMpLanStoryDevProtectInstall(HMODULE game_module,const wchar_t *config_path);
BOOL SudekiMpLanStoryDevProtectEnabled(void);
/* Host game thread, once per story service tick: roster when exact (NULL
 * otherwise) and the proved ally entity (NULL when none). */
void SudekiMpLanStoryDevProtectService(const SudekiMpLanStoryNativeRoster *roster,void *ally_entity);
/* Release every lease that still resolves (runtime reset / teardown). */
void SudekiMpLanStoryDevProtectRelease(const char *why);
#endif
