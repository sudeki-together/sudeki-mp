#ifndef SUDEKIMP_WEAPON_ACTIVATION_ABI_H
#define SUDEKIMP_WEAPON_ACTIVATION_ABI_H

#include <windows.h>
#include <stdint.h>

/* Category 5 contains the ordered weapon families for all four heroes. */
enum { SUDEKIMP_WEAPON_ACTIVATION_MAX_ROWS = 64u };

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
void SudekiMpResetWeaponActivationAbi(void);
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
