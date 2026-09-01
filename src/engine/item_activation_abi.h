#ifndef SUDEKIMP_ITEM_ACTIVATION_ABI_H
#define SUDEKIMP_ITEM_ACTIVATION_ABI_H

#include <windows.h>

/* Keep a full retail category snapshot rather than silently truncating a
 * long consumable inventory. */
enum { SUDEKIMP_ITEM_ACTIVATION_MAX_ROWS = 256u };

typedef enum SudekiMpItemActivationStatus {
    SUDEKIMP_ITEM_ACTIVATION_STARTED = 0,
    SUDEKIMP_ITEM_ACTIVATION_INVALID_CONTEXT,
    SUDEKIMP_ITEM_ACTIVATION_INVALID_SELECTION,
    SUDEKIMP_ITEM_ACTIVATION_NOT_AVAILABLE,
    SUDEKIMP_ITEM_ACTIVATION_REJECTED,
    SUDEKIMP_ITEM_ACTIVATION_UNVERIFIED
} SudekiMpItemActivationStatus;

typedef struct SudekiMpItemQuickRow {
    unsigned int slot;
    void *native_item;
} SudekiMpItemQuickRow;

typedef struct SudekiMpItemQuickList {
    unsigned int row_count;
    SudekiMpItemQuickRow rows[SUDEKIMP_ITEM_ACTIVATION_MAX_ROWS];
} SudekiMpItemQuickList;

typedef struct SudekiMpItemActivationResult {
    SudekiMpItemActivationStatus status;
    unsigned int slot;
    unsigned int target_party_slot;
    void *expected_item;
    void *observed_target;
} SudekiMpItemActivationResult;

/* Item inventory is retail-global, but the target application and removal
 * stay in the native order: validate/apply first, decrement only on success. */
BOOL SudekiMpInitializeItemActivationAbi(HMODULE game_module);
void SudekiMpResetItemActivationAbi(void);
BOOL SudekiMpDescribeQuickItems(SudekiMpItemQuickList *items);
SudekiMpItemActivationResult SudekiMpActivateCharacterItem(
    void *source_character,
    unsigned int item_slot,
    void *target_character,
    unsigned int target_party_slot
);
const char *SudekiMpItemActivationStatusName(
    SudekiMpItemActivationStatus status
);

#endif
