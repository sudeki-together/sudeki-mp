#ifndef SUDEKIMP_LOBBY_INSTANCE_H
#define SUDEKIMP_LOBBY_INSTANCE_H
#include <windows.h>
/* Only the validated local Test Room child uses a process-specific startup
 * mutex while its title lobby waits for load acknowledgements. */
BOOL SudekiMpInstallLobbyInstance(HMODULE game);
BOOL SudekiMpUninstallLobbyInstance(void);
#endif
