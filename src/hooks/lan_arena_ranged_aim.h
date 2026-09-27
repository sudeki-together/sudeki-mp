#ifndef SUDEKIMP_LAN_ARENA_RANGED_AIM_H
#define SUDEKIMP_LAN_ARENA_RANGED_AIM_H
#include <windows.h>
#include <stdint.h>

/* Called only on the established game thread. Must revalidate session,
 * assignment, actor lifetime, input freshness and absence of a caster task.
 * Projectile authority and cosmetic pose admission are separate requests. */
typedef BOOL (*SudekiMpLanAimWitness)(void *actor, BOOL projectile, float direction[3]);
typedef BOOL (*SudekiMpLanAimTargetWitness)(void *actor, float target[3]);
/* Fresh authoritative trigger intent, not a request to create another shot.
 * FALSE means unknown/ineligible; TRUE with held=FALSE means released. */
typedef BOOL (*SudekiMpLanAimFireWitness)(void *actor, BOOL *held);
/* Existing projectile-direction seam, after a native projectile is allocated,
 * before its charge is spent. Observer must validate role, actor and weapon. */
typedef void (*SudekiMpLanWeaponShotObserver)(void *actor);
void SudekiMpLanAimSetShotObserver(SudekiMpLanWeaponShotObserver observer);
/* Local cosmetic idle ownership, independent of observer snapshot/aim
 * admission. Must freshly validate session, local actor lease and no cast.
 * Does not authorize a projectile, remote pose or gameplay mutation. */
typedef BOOL (*SudekiMpLanIdleWitness)(void *actor);
void SudekiMpLanAimSetIdleWitness(SudekiMpLanIdleWitness witness);
BOOL SudekiMpLanAimNormalize(const float input[3], float output[3]);
/* Pure geometry. Output remains untouched on invalid input. */
BOOL SudekiMpLanAimConverge(const float muzzle[3],const float target[3],float direction[3]);
BOOL SudekiMpLanAimTargetNearActor(const float actor[3],const float direction[3],const float target[3]);
/* Exact active named-camera basis used by retail missile direction. */
BOOL SudekiMpLanAimCameraTarget(float direction[3],float target[3]);
/* Pure additive quaternion layer; leaves translation/scale and unmasked bones
 * byte-identical. Input animation clocks/channels are never changed. */
BOOL SudekiMpLanAimOverlay(float *pose, const float *aim, const float *center,
    const uint8_t *mask, unsigned count, float amount);
BOOL SudekiMpLanAimImageMatches(HMODULE image);
BOOL SudekiMpLanAimInstall(HMODULE image, SudekiMpLanAimWitness witness,
    SudekiMpLanAimTargetWitness target_witness, SudekiMpLanAimFireWitness fire_witness);
BOOL SudekiMpLanAimUninstall(void);
/* Refresh candidate identities at a verified game-thread boundary. They are
 * only a fast filter; the witness and native ownership are checked at use. */
void SudekiMpLanAimActors(void *first, void *second);
#endif
