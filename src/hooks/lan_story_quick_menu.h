#ifndef SUDEKIMP_LAN_STORY_QUICK_MENU_H
#define SUDEKIMP_LAN_STORY_QUICK_MENU_H
#include "hooks/lan_story_client.h"

/* Client-only retail Q UI. The callback only reserves plain request data;
 * TRUE means queued, never that a native skill has started. */
typedef BOOL (*SudekiMpStoryQuickSkillRequest)(void *actor,unsigned slot);
BOOL SudekiMpLanStoryQuickMenuInstall(HMODULE image,SudekiMpStoryQuickSkillRequest request);
/* Inside ClientPresent, using that transaction's exact roster. Losing owned
 * closes through the balanced native path. A transient !admitted blocks new
 * opens/skill submissions, not the existing UI or its cancel/navigation. */
BOOL SudekiMpLanStoryQuickMenuService(const SudekiMpLanStoryNativeRoster *roster,
    uint32_t transaction,BOOL owned,BOOL admitted,BOOL toggle);
BOOL SudekiMpLanStoryQuickMenuCapturesInput(void);
/* Close through Service before teardown; failed restoration retains all
 * forwarding callbacks and script interception dependencies. */
BOOL SudekiMpLanStoryQuickMenuUninstall(void);
#endif
