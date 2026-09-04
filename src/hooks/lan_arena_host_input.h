#ifndef SUDEKIMP_LAN_ARENA_HOST_INPUT_H
#define SUDEKIMP_LAN_ARENA_HOST_INPUT_H

#include <windows.h>
#include <stdint.h>

typedef void (*SudekiMpLanArenaHostNativeSkillStartObserver)(
    void *character,
    void *skill,
    int slot,
    uint32_t cost,
    BOOL active_seen
);

/* Host-only native Tal input and presentation-trace adapter. */
BOOL SudekiMpInstallLanArenaHostInput(HMODULE game_module);
/* Teardown is retryable. A FALSE result retains callback trampolines and
 * operator events until every installed input hook has been restored. */
BOOL SudekiMpUninstallLanArenaHostInput(void);
void SudekiMpLanArenaHostInputServiceCombatToggle(void);
BOOL SudekiMpLanArenaHostInputDiagnosticTraceActive(void);
/* Test/operator acknowledgement is emitted only after Sudeki exposes a new
 * native Tal action selector, never merely because an input was submitted. */
void SudekiMpLanArenaHostInputNotifyNativeActionObserved(void);
BOOL SudekiMpLanArenaHostInputTakeSkillSlot(unsigned int *slot);
/* Drains both host-only auto-reset Spirit rails atomically from the runtime's
 * game-thread service. A simultaneous pair is rejected and fully consumed so
 * event ordering can never silently choose a different native variant. */
BOOL SudekiMpLanArenaHostInputTakeSpiritVariant(unsigned int *variant);
/* Consumes any currently queued Spirit variants without choosing or executing
 * one. Runtime session-generation fences use this before admitting requests. */
BOOL SudekiMpLanArenaHostInputDiscardSpiritRequests(void);
/* Re-proves that the installed Player-1 controller global is readable and
 * targets this exact Tal actor. Spirit activation ultimately enters a
 * process-global native manager, so actor description alone is insufficient
 * proof of the initiating seat. */
BOOL SudekiMpLanArenaHostInputTalControllerLeaseExact(void *tal);
/* The host input adapter owns the exact QuickSkill and QuickMenu CSkill::Use
 * callsites.  The observer is invoked only after native execution has
 * returned nonzero for an exact inactive-before Tal/CSkill/slot admission.
 * `active_seen` distinguishes an immediately readable active byte from the
 * known STARTED-before-active startup gap. A successful adapter teardown
 * clears the observer; a failed teardown retains it with the still-live
 * callbacks for an exact retry. */
void SudekiMpLanArenaHostInputSetNativeSkillStartObserver(
    SudekiMpLanArenaHostNativeSkillStartObserver observer
);

#endif
