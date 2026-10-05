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
/* Same established direction seam after allocation. The native manager's
 * constructor-proven missile references are still available here. Runs
 * before the ordinary emission observer; does not authorize another shot. */
typedef void (*SudekiMpLanProjectileObserver)(void *actor,void *manager);
void SudekiMpLanAimSetProjectileObserver(SudekiMpLanProjectileObserver observer);
/* Passive host-only mode, mutually exclusive with AimInstall. Owns only the
 * existing direction seam; no pose/sampler hook, direction override or fire
 * input is installed. Geometry is the native pre-spread emission result.
 * Caller freshly validates its world/actor namespace inside the callback. */
typedef void (*SudekiMpLanEmissionObserver)(void *actor,void *manager,
    const float origin[3],const float direction[3]);
BOOL SudekiMpLanAimObserveEmissionsInstall(HMODULE image,SudekiMpLanEmissionObserver observer);
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
/* Contained story client: Ailish's authored upper-body pose only. The same
 * adapter owns the matrix-build seam; no direction, shot or base-sampler
 * hook is installed. The witness must establish a fresh paused-world scope
 * using PoseWitness and validate the current authenticated body observation.
 * Mutually exclusive with the two existing modes. */
BOOL SudekiMpLanAimPoseOnlyInstall(HMODULE image,SudekiMpLanAimWitness witness);
BOOL SudekiMpLanAimPoseWitness(void *unused);
BOOL SudekiMpLanAimUninstall(void);
/* Refresh candidate identities at a verified game-thread boundary. They are
 * only a fast filter; the witness and native ownership are checked at use. */
void SudekiMpLanAimActors(void *first, void *second);
#endif
