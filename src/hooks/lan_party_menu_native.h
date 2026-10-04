#ifndef SUDEKIMP_LAN_PARTY_MENU_NATIVE_H
#define SUDEKIMP_LAN_PARTY_MENU_NATIVE_H

#include <windows.h>

/* Admission is sampled on the native UI thread. Toggle receives the existing
 * native Escape press after its eligibility/debounce checks. Frame runs after
 * the Quit renderer, including during a native full pause. No actor mutation
 * is authorized by these callbacks: actor work still needs its control witness.
 * Callbacks must remain alive until Uninstall succeeds. */
typedef BOOL (*SudekiMpLanPartyMenuAdmission)(void);
typedef void (*SudekiMpLanPartyMenuCallback)(void);
BOOL SudekiMpLanPartyMenuNativeInstall(HMODULE image,
    SudekiMpLanPartyMenuAdmission admission,
    SudekiMpLanPartyMenuCallback toggle,
    SudekiMpLanPartyMenuCallback frame);
BOOL SudekiMpLanPartyMenuNativeUninstall(void);

/* Own one balanced reference to the same native pause transaction used by
 * Quit Show/Back. Only valid inside Frame; repeated requests are idempotent.
 * Foreign pause references are preserved. Failure retains the lease for retry.
 * Before leaving gameplay or uninstalling, request FALSE from Frame and wait
 * until OwnsPause is FALSE. Never unpause by directly clearing native fields. */
BOOL SudekiMpLanPartyMenuNativeSetPaused(BOOL paused);
BOOL SudekiMpLanPartyMenuNativeOwnsPause(void);
BOOL SudekiMpLanPartyMenuNativePauseExact(BOOL *paused);
/* Read-only pause proof for a separately verified render callsite on this
 * native thread. Requires this adapter's completely acquired, unchanged
 * reference and both installed hooks. Grants no SetPaused/exit permission. */
BOOL SudekiMpLanPartyMenuNativeObserveOwnedPause(BOOL *paused);

/* Saved-story exit only: prepare inside Frame while our pause is positively
 * held, after render-owned presentation has been restored. Finish on the same
 * native thread OUTSIDE every menu callback, immediately before native exit.
 * The caller must freshly validate its complete entity registry/input/trigger
 * owners; these APIs prove only the captured native pause singleton/counts.
 * Finish and Reacquire preserve partial-operation verification for retries.
 * Keep this adapter installed until either native exit commits or a failed
 * exit has reacquired the pause before returning to the normal native loop. */
BOOL SudekiMpLanPartyMenuNativePreparePauseExit(void);
BOOL SudekiMpLanPartyMenuNativeExitPauseExact(BOOL *paused);
BOOL SudekiMpLanPartyMenuNativeFinishPauseExit(void);
BOOL SudekiMpLanPartyMenuNativeReacquirePauseExit(void);

#endif
