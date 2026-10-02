#ifndef SUDEKIMP_LAN_STORY_RUNTIME_H
#define SUDEKIMP_LAN_STORY_RUNTIME_H
#include "network/lan_party_session.h"

/* Private opening-lifecycle probe. Scene transport and native observation,
 * not campaign gameplay or a replica. No actor/input/save authority changes. */
BOOL SudekiMpInstallLanStoryRuntime(HMODULE module,const SudekiMpLanPartyConfig *config);
BOOL SudekiMpUninstallLanStoryRuntime(void);
#endif
