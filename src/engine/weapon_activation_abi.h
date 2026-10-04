#ifndef SUDEKIMP_WEAPON_ACTIVATION_ABI_H
#define SUDEKIMP_WEAPON_ACTIVATION_ABI_H

#include <windows.h>
#include <stdint.h>

/* Native weapon categories each have twelve actor-local slots. */
enum { SUDEKIMP_WEAPON_ACTIVATION_MAX_ROWS = 12u };

/* Exact retail first-person C1: selector 8, fourteen frames at 24 fps.
 * Presentation additionally verifies the loaded resource before playing it.
 * Round upward when the host reserves this interval against another shot. */
enum {
    SUDEKIMP_RANGED_WEAPON_SWAP_FRAMES = 14u,
    SUDEKIMP_RANGED_WEAPON_SWAP_RATE = 24u,
    SUDEKIMP_RANGED_WEAPON_SWAP_MS =
        (1000u * SUDEKIMP_RANGED_WEAPON_SWAP_FRAMES +
            SUDEKIMP_RANGED_WEAPON_SWAP_RATE - 1u) /
        SUDEKIMP_RANGED_WEAPON_SWAP_RATE
};

typedef enum SudekiMpWeaponActivationStatus {
    SUDEKIMP_WEAPON_ACTIVATION_STARTED = 0,
    SUDEKIMP_WEAPON_ACTIVATION_INVALID_CONTEXT,
    SUDEKIMP_WEAPON_ACTIVATION_INVALID_SELECTION,
    SUDEKIMP_WEAPON_ACTIVATION_NOT_AVAILABLE,
    SUDEKIMP_WEAPON_ACTIVATION_UNVERIFIED
} SudekiMpWeaponActivationStatus;

typedef struct SudekiMpWeaponQuickRow {
    unsigned int slot;
    void *native_item;
    uint8_t equipped;
    uint8_t reserved[3];
} SudekiMpWeaponQuickRow;

typedef struct SudekiMpWeaponQuickList {
    unsigned int inventory_category;
    unsigned int row_count;
    SudekiMpWeaponQuickRow rows[SUDEKIMP_WEAPON_ACTIVATION_MAX_ROWS];
} SudekiMpWeaponQuickList;

typedef struct SudekiMpWeaponActivationResult {
    SudekiMpWeaponActivationStatus status;
    unsigned int slot;
    void *expected_item;
    void *observed_item;
} SudekiMpWeaponActivationResult;

BOOL SudekiMpInitializeWeaponActivationAbi(HMODULE game_module);
BOOL SudekiMpWeaponActivationAbiInitialized(void);
void SudekiMpResetWeaponActivationAbi(void);
BOOL SudekiMpWeaponFamily(unsigned int resource_type,
    unsigned int *category, unsigned int *starter_item_id);
/* Startup only: preserve an equipped owned-family item, otherwise equip the
 * owned starter. Never grants inventory, cancels a pending swap, or resets kit. */
BOOL SudekiMpEnsureCharacterStarterWeapon(void *character);
/* Four-player noncombat bootstrap only, on the caller's exact game-thread
 * roster lease. Show already-attached native sheathed weapons without
 * changing equipment, locators, animation, or Ailish's hidden staff policy.
 * Unknown/pending/first-person attachment topology fails closed. */
BOOL SudekiMpInitializeSheathedWeaponVisibility(void *character);
/* Replica-only host visibility for already equipped, attached, noncombat
 * weapons. Same exact topology/callback checks as startup, with native
 * setter/readback. Does not expose Ailish's intentionally hidden staff.
 * Caller owns the current authenticated actor/scene and game-thread lease. */
BOOL SudekiMpReconcileSheathedWeaponVisibility(void *character,BOOL visible);
/* Read-only portable attachment semantics for an owned equipped weapon:
 * 0 detached/hidden, 1 authored hand locator, 2 authored sheath locator.
 * Indices are resolved independently in each process's loaded model bank. */
BOOL SudekiMpObserveCharacterWeaponAttachment(void *character,uint8_t slots[2]);
/* Contained story replica only. Caller proves the paused actor/scene before
 * and after this synchronous operation. Reattaches existing owned wrappers;
 * does not arm the native controller, run gameplay, or change equipment. */
BOOL SudekiMpReconcileCharacterWeaponAttachment(void *character,
    const uint8_t slots[2],BOOL visible);
/* Replica-only, exact game/render-thread actor lease required at the caller.
 * After the replicated Elco gun-play idle leaves both animation pairs,
 * perform its native hand-to-holster interruption cleanup if still pending.
 * No equipment, combat, resource, or animation changes; no-op when cleared. */
BOOL SudekiMpRestoreElcoInterruptedIdleWeapon(void *character);
/* Testroom bootstrap only, game thread. Add missing authored Buki/Elco weapons
 * through native inventory; preserve equipped items and existing quantities.
 * The caller supplies the actual process command line, never a network string. */
BOOL SudekiMpGrantTestroomCharacterWeapons(void *character, const char *command);
/* Host game-thread only, after the remote actor lease is validated. Proton
 * Phaser misses FP recharge/blend while the local camera owns another
 * hero. Uses native recharge, never resets charge or forces reload complete.
 * Other weapons/rates return FALSE and retain their native admission path. */
BOOL SudekiMpServiceRemoteRapidWeapon(void *character, void *local_character,
    float delta, uint32_t *repeat_ms);
typedef struct SudekiMpElcoWeaponObservation {
    uint8_t item, stage;
    float charge, required_charge, reload_seconds;
} SudekiMpElcoWeaponObservation;
/* Game-thread observations; callers retain the session/actor lease. */
BOOL SudekiMpObserveElcoWeapon(void *character, SudekiMpElcoWeaponObservation *out);
/* Direction-hook observation of an already admitted native emission, before
 * charge is spent. Requires the selected record to own the active shot.
 * This does not authorize starting a shot; keep ElcoWeaponReady for that. */
BOOL SudekiMpObserveElcoWeaponEmission(void *character,
    SudekiMpElcoWeaponObservation *out);
/* Explicit actor type (Elco 0x0e / Ailish 0x01), exact family and native
 * record ownership. Legacy Elco entry points retain their narrower contract. */
BOOL SudekiMpObserveRangedWeapon(void *character,uint8_t actor_type,
    SudekiMpElcoWeaponObservation *out);
BOOL SudekiMpSetRangedPresentationResources(void *character,uint8_t actor_type,
    void *local_actor,uint8_t item,uint16_t charge_q8,uint16_t reload_ms,
    BOOL preserve_reload_edge);
BOOL SudekiMpElcoWeaponReady(const SudekiMpElcoWeaponObservation *state);
/* Replica-only resource reconciliation, never called for authoritative actors.
 * Also requires that this actor owns the native local controller. */
BOOL SudekiMpSetElcoPresentationResources(void *character, uint8_t item,
    uint16_t charge_q8, uint16_t reload_ms);
/* Ongoing snapshot reconciliation must preserve the native positive-to-zero
 * reload-completion edge. SetElcoPresentationResources remains the exact
 * pre-shot seed, used only after the caller proves no native clip is active. */
BOOL SudekiMpSyncElcoPresentationResources(void *character, uint8_t item,
    uint16_t charge_q8, uint16_t reload_ms);
/* Replica observer variant. Caller retains both exact actor leases and the
 * client damage guard. The explicit local actor must own the native
 * controller; this never changes that controller or either actor identity. */
BOOL SudekiMpSetObservedElcoPresentationResources(void *character,void *local_actor,
    uint8_t item,uint16_t charge_q8,uint16_t reload_ms,BOOL preserve_reload_edge);
float SudekiMpRapidWeaponRechargeAmount(uint16_t rate, uint16_t charge,
    uint8_t flags, float cooldown, float delta);
uint32_t SudekiMpRapidWeaponCycleMs(unsigned int item_id, float frames,
    float delta);
BOOL SudekiMpDescribeCharacterWeapons(void *character,
    SudekiMpWeaponQuickList *weapons);
SudekiMpWeaponActivationResult SudekiMpActivateCharacterWeapon(
    void *character,
    unsigned int slot
);
/* Exact actor-local inventory observation used by borrowed-control drain:
 * do not release/rejoin while Sudeki still owns an asynchronous weapon swap. */
BOOL SudekiMpWeaponActivationPending(void *character, BOOL *pending);
const char *SudekiMpWeaponActivationStatusName(
    SudekiMpWeaponActivationStatus status
);

#endif
