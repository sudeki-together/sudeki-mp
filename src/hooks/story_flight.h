#ifndef SUDEKIMP_STORY_FLIGHT_H
#define SUDEKIMP_STORY_FLIGHT_H

#include <windows.h>
#include <stdint.h>

/* Player flight (research prototype). While enabled for the controlled
 * character, the native movement controller's gravity bit is held clear and
 * its vertical velocity follows the ascend/descend keys. Toggle, ascend and
 * descend are virtual keys; speed is vertical units per second; while flying
 * the frame's horizontal displacement is scaled by forward_multiplier. When
 * ailish_only is TRUE the toggle is accepted only while Ailish is controlled. */
typedef struct SudekiMpStoryFlightConfig {
    UINT toggle_key;
    UINT ascend_key;
    UINT descend_key;
    float speed;
    BOOL ailish_only;
    /* Horizontal displacement multiplier while flying (1 = walking pace). */
    float forward_multiplier;
} SudekiMpStoryFlightConfig;

/* Called once per frame on the game thread for the controlled character, from
 * the same movement-update seam flight uses (owner entity, its movement
 * controller, world position, frame dt). One observer; NULL clears it. The
 * observer may call native world functions; it must not retain `movement`. */
typedef void (*SudekiMpStoryFlightControlledObserver)(void *owner, uint8_t *movement, const float position[3], float dt);
void SudekiMpStoryFlightSetControlledObserver(SudekiMpStoryFlightControlledObserver observer);
/* Up to four observers; Add is idempotent, Remove tolerates unknown entries. */
BOOL SudekiMpStoryFlightAddControlledObserver(SudekiMpStoryFlightControlledObserver observer);
void SudekiMpStoryFlightRemoveControlledObserver(SudekiMpStoryFlightControlledObserver observer);

BOOL SudekiMpInstallStoryFlight(HMODULE game_module, const SudekiMpStoryFlightConfig *config);
BOOL SudekiMpUninstallStoryFlight(void);

#endif
