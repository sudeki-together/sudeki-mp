#ifndef SUDEKIMP_LAN_ARENA_RANGED_AIM_H
#define SUDEKIMP_LAN_ARENA_RANGED_AIM_H
#include <windows.h>
#include <stdint.h>

/* Called only on the established game thread. Must revalidate session,
 * assignment, actor lifetime, input freshness and absence of a caster task.
 * Projectile authority and cosmetic pose admission are separate requests. */
typedef BOOL (*SudekiMpLanAimWitness)(void *actor, BOOL projectile, float direction[3]);
BOOL SudekiMpLanAimNormalize(const float input[3], float output[3]);
/* Pure additive quaternion layer; leaves translation/scale and unmasked bones
 * byte-identical. Input animation clocks/channels are never changed. */
BOOL SudekiMpLanAimOverlay(float *pose, const float *aim, const float *center,
    const uint8_t *mask, unsigned count, float amount);
BOOL SudekiMpLanAimImageMatches(HMODULE image);
BOOL SudekiMpLanAimInstall(HMODULE image, SudekiMpLanAimWitness witness);
BOOL SudekiMpLanAimUninstall(void);
/* Refresh candidate identities at a verified game-thread boundary. They are
 * only a fast filter; the witness and native ownership are checked at use. */
void SudekiMpLanAimActors(void *first, void *second);
#endif
