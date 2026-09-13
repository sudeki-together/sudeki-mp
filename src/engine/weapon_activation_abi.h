#ifndef SUDEKIMP_WEAPON_ACTIVATION_ABI_H
#define SUDEKIMP_WEAPON_ACTIVATION_ABI_H

#include <windows.h>
#include <stdint.h>

/* Native weapon categories each have twelve actor-local slots. */
enum { SUDEKIMP_WEAPON_ACTIVATION_MAX_ROWS = 12u };

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
const char *SudekiMpWeaponActivationStatusName(
    SudekiMpWeaponActivationStatus status
);

#endif
