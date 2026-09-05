#ifndef SUDEKIMP_LAN_ARENA_CLIENT_INPUT_H
#define SUDEKIMP_LAN_ARENA_CLIENT_INPUT_H

#include <windows.h>

BOOL SudekiMpInstallLanArenaClientInput(HMODULE game_module);
/* Teardown is retryable. A FALSE result means at least one live hook could
 * not be restored; callback trampolines and operator events remain owned so
 * no surviving detour can observe cleared dependencies. */
BOOL SudekiMpUninstallLanArenaClientInput(void);
/* Called once from the post-controller game-thread observer. Held movement,
 * held fire, and the native client-camera aim are refreshed at a bounded
 * cadence; missing controller samples become an explicit neutral packet
 * before the host's safety timeout. */
void SudekiMpLanArenaClientInputService(void);

/* Sudeki's controller keeps each digital action as a four-state transition:
 * 0=up, 1=pressed, 2=held, 3=released.  LAN input samples this per-window
 * state instead of GetAsyncKeyState, whose Wine implementation is shared by
 * unrelated prefixes/windows on the same X display. */
BOOL SudekiMpLanArenaClientNativeWeakHeld(int transition_state);
/* Retail targeting submits local strafe axes, unlike ordinary world movement.
 * Convert only that branch, retaining analog magnitude and horizontal aim. */
BOOL SudekiMpLanArenaClientMovementWorldDirection(
    BOOL local_strafe, float x, float z, float forward_x, float forward_z,
    float *world_x, float *world_z);
/* Poll the current exact controller, not the age of the edge-only callback. */
BOOL SudekiMpLanArenaClientCurrentWeakHeld(
    BOOL owner_exact, int control_filter, int transition_state);
int SudekiMpLanArenaClientSuppressedWeakNextState(int transition_state);
/* Closed Ailish training inventory; only a fresh native press cycles it. */
BOOL SudekiMpLanArenaClientCycleWeaponSlot(unsigned int count,
    unsigned int current, int next_state, int previous_state,
    unsigned int *selected);
/* Ailish's LAN weak input is a ranged trigger.  Do not transmit it while the
 * native client is still entering/leaving its verified first-person graph;
 * otherwise the host can interpret the same held click as a third-person
 * combat action at the wrong cadence. */
BOOL SudekiMpLanArenaClientRangedWeakHeld(
    BOOL first_person_active,
    BOOL raw_weak_held
);
/* Pure camera ownership policy: during Ailish's authenticated native skill
 * camera, ordinary first-person/orbit events must not reach that camera. */
BOOL SudekiMpLanArenaClientCameraInputAllowed(
    BOOL authenticated,
    BOOL local_skill_camera_active
);
/* A local diagnostic forward hold may replace only neutral native axes. */
BOOL SudekiMpLanArenaClientOperatorForwardPolicy(
    BOOL physical_direction_held,
    BOOL operator_forward_held
);
/* A transport resend is not a new native movement sample. Missing controller
 * callbacks must expire the cached world vector even if raw axes stay held. */
BOOL SudekiMpLanArenaClientMovementSampleFresh(
    BOOL owner_exact,
    BOOL physical_direction_held,
    DWORD sampled_at_ms,
    DWORD now_ms
);

BOOL SudekiMpLanArenaClientRequestSkillSlot(unsigned int slot);

#endif
