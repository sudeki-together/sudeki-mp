#ifndef SUDEKIMP_LAN_STORY_ARBITER_TRACE_H
#define SUDEKIMP_LAN_STORY_ARBITER_TRACE_H
#include <windows.h>
/* Research probe (log only): observes every CCharacterArbiter animation-state
 * event (RVA 0xDBC90: the function that arms/disarms on WEAPON_OUT/BACK) and
 * every combo dispatch (RVA 0xD0730: PC admission and CComboManager::PlayCombo)
 * for all characters, with the owning entity's resource name. Installed only
 * with [StoryAreas] ArbiterTrace=true. Never changes native state. */
BOOL SudekiMpLanStoryArbiterTraceInstall(HMODULE image,const wchar_t *config_path);
BOOL SudekiMpLanStoryArbiterTraceUninstall(void);
BOOL SudekiMpLanStoryArbiterTraceInstalled(void);
#endif
