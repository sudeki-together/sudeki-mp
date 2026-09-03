#ifndef SUDEKIMP_LAN_ARENA_HOST_INPUT_H
#define SUDEKIMP_LAN_ARENA_HOST_INPUT_H

#include <windows.h>

/* Host-only native Tal input and presentation-trace adapter. */
BOOL SudekiMpInstallLanArenaHostInput(HMODULE game_module);
void SudekiMpUninstallLanArenaHostInput(void);
void SudekiMpLanArenaHostInputServiceCombatToggle(void);
BOOL SudekiMpLanArenaHostInputRequestRemoteCombatToggle(void);
BOOL SudekiMpLanArenaHostInputDiagnosticTraceActive(void);
/* Test/operator acknowledgement is emitted only after Sudeki exposes a new
 * native Tal action selector, never merely because an input was submitted. */
void SudekiMpLanArenaHostInputNotifyNativeActionObserved(void);
BOOL SudekiMpLanArenaHostInputTakeSkillSlot(unsigned int *slot);

#endif
