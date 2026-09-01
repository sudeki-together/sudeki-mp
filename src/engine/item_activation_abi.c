#include "engine/item_activation_abi.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Item activation ABI requires 32-bit GCC assembly support"
#endif

enum {
    RVA_INVENTORY_LOOKUP = 0x00021ce0u,
    RVA_INVENTORY_COUNT = 0x00021e80u,
    RVA_APPLY_ITEM_TO_PARTY_SLOT = 0x000dc110u,
    RVA_INVENTORY_REMOVE = 0x00021de0u,
    RVA_INVENTORY_GLOBAL = 0x00408d84u,
    RVA_GROUP_GLOBAL = 0x00408d94u,
    INVENTORY_ITEM_CATEGORY = 2u,
    GROUP_PARTY_OFFSET = 0x90u,
    PARTY_SLOT_STRIDE = 0x0cu,
    MAX_PARTY_SLOTS = 4u,
    /* The retail inventory category stores sparse raw slots.  Its own menu
     * enumerates up to this native safety bound and simply omits null slots. */
    INVENTORY_RAW_SLOT_LIMIT = 999u
};

static const uint8_t expected_inventory_lookup_entry[] = {
    /* The global operand after this prologue relocates at load time. */
    0x53, 0x8b, 0x5c, 0x24, 0x08, 0x55, 0x56, 0x57,
    0x8b, 0x3d
};
static const uint8_t expected_inventory_count_entry[] = {
    0x8b, 0x91, 0x2c, 0x01, 0x00, 0x00, 0x53, 0x33,
    0xc0, 0x56
};
static const uint8_t expected_apply_item_entry[] = {
    0x83, 0xec, 0x0c, 0x53, 0x8b, 0x5c, 0x24, 0x14,
    0x8b, 0x43, 0x50, 0x55
};
static const uint8_t expected_inventory_remove_entry[] = {
    0x8b, 0x44, 0x24, 0x04, 0x53, 0x33, 0xdb, 0x55,
    0x8b, 0x6c, 0x24, 0x10
};

typedef void *(__attribute__((thiscall)) *InventoryLookupFunction)(
    void *inventory, int category, int slot
);
typedef BOOL (__attribute__((stdcall)) *ApplyItemFunction)(
    void *source_character, void *item, int target_party_slot
);
typedef void (__attribute__((stdcall)) *InventoryRemoveFunction)(
    void *inventory, int slot
);

static HMODULE native_module;
static InventoryLookupFunction native_inventory_lookup;
static ApplyItemFunction native_apply_item;
static InventoryRemoveFunction native_inventory_remove;
static void *native_inventory_count __attribute__((used));

static BOOL readable_memory(const void *pointer, size_t size) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t start;
    uintptr_t end;
    uintptr_t region_end;

    if (pointer == NULL || size == 0u ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return FALSE;
    }
    start = (uintptr_t)pointer;
    end = start + size;
    region_end = (uintptr_t)information.BaseAddress + information.RegionSize;
    return end >= start && end <= region_end;
}

__attribute__((naked, noinline))
static int call_inventory_category_count(void *inventory, int category) {
    (void)inventory;
    (void)category;
    __asm__ volatile(
        "pushl %ebp\n\t"
        "movl %esp, %ebp\n\t"
        "pushl %edi\n\t"
        "movl 8(%ebp), %ecx\n\t"
        "movl 12(%ebp), %edi\n\t"
        "call *_native_inventory_count\n\t"
        "popl %edi\n\t"
        "popl %ebp\n\t"
        "ret\n\t"
    );
}

static SudekiMpItemActivationResult empty_result(
    SudekiMpItemActivationStatus status
) {
    SudekiMpItemActivationResult result;

    ZeroMemory(&result, sizeof(result));
    result.status = status;
    return result;
}

static BOOL inventory_context(void **inventory) {
    if (native_module == NULL || inventory == NULL ||
        !readable_memory((uint8_t *)native_module + RVA_INVENTORY_GLOBAL,
            sizeof(void *))) {
        return FALSE;
    }
    *inventory = *(void **)((uint8_t *)native_module + RVA_INVENTORY_GLOBAL);
    return *inventory != NULL && readable_memory(*inventory, 0x130u);
}

static BOOL party_target_exact(
    void *target_character,
    unsigned int target_party_slot
) {
    void *group;
    void **party_slot;

    if (native_module == NULL || target_character == NULL ||
        target_party_slot >= MAX_PARTY_SLOTS ||
        !readable_memory((uint8_t *)native_module + RVA_GROUP_GLOBAL,
            sizeof(void *))) {
        return FALSE;
    }
    group = *(void **)((uint8_t *)native_module + RVA_GROUP_GLOBAL);
    if (group == NULL || !readable_memory((uint8_t *)group +
            GROUP_PARTY_OFFSET +
            (target_party_slot * PARTY_SLOT_STRIDE), sizeof(void *))) {
        return FALSE;
    }
    party_slot = (void **)((uint8_t *)group + GROUP_PARTY_OFFSET +
        (target_party_slot * PARTY_SLOT_STRIDE));
    return *party_slot == target_character;
}

BOOL SudekiMpInitializeItemActivationAbi(HMODULE game_module) {
    uint8_t *base = (uint8_t *)game_module;

    if (native_module != NULL || base == NULL) {
        SetLastError(ERROR_ALREADY_INITIALIZED);
        return FALSE;
    }
    if (memcmp(base + RVA_INVENTORY_LOOKUP, expected_inventory_lookup_entry,
            sizeof(expected_inventory_lookup_entry)) != 0 ||
        memcmp(base + RVA_INVENTORY_COUNT, expected_inventory_count_entry,
            sizeof(expected_inventory_count_entry)) != 0 ||
        memcmp(base + RVA_APPLY_ITEM_TO_PARTY_SLOT, expected_apply_item_entry,
            sizeof(expected_apply_item_entry)) != 0 ||
        memcmp(base + RVA_INVENTORY_REMOVE, expected_inventory_remove_entry,
            sizeof(expected_inventory_remove_entry)) != 0 ||
        !readable_memory(base + RVA_INVENTORY_GLOBAL, sizeof(void *)) ||
        !readable_memory(base + RVA_GROUP_GLOBAL, sizeof(void *))) {
        SetLastError(ERROR_BAD_EXE_FORMAT);
        return FALSE;
    }
    native_module = game_module;
    native_inventory_lookup = (InventoryLookupFunction)(base +
        RVA_INVENTORY_LOOKUP);
    native_inventory_count = base + RVA_INVENTORY_COUNT;
    native_apply_item = (ApplyItemFunction)(base + RVA_APPLY_ITEM_TO_PARTY_SLOT);
    native_inventory_remove = (InventoryRemoveFunction)(base +
        RVA_INVENTORY_REMOVE);
    return TRUE;
}

void SudekiMpResetItemActivationAbi(void) {
    native_module = NULL;
    native_inventory_lookup = NULL;
    native_inventory_count = NULL;
    native_apply_item = NULL;
    native_inventory_remove = NULL;
}

BOOL SudekiMpDescribeQuickItems(SudekiMpItemQuickList *items) {
    void *inventory;
    int count;
    unsigned int raw_slot;
    unsigned int row_count;

    if (items == NULL || !inventory_context(&inventory)) {
        return FALSE;
    }
    count = call_inventory_category_count(inventory, INVENTORY_ITEM_CATEGORY);
    if (count < 0 || (unsigned int)count > INVENTORY_RAW_SLOT_LIMIT) {
        return FALSE;
    }
    ZeroMemory(items, sizeof(*items));
    row_count = 0u;
    for (raw_slot = 0u; raw_slot < (unsigned int)count; ++raw_slot) {
        void *item = native_inventory_lookup(inventory,
            INVENTORY_ITEM_CATEGORY, (int)raw_slot);
        /* Retail's QuickMenu tests the lookup result before constructing a
         * row.  Empty inventory slots are normal, not a failed snapshot. */
        if (item == NULL) {
            continue;
        }
        if (!readable_memory(item, sizeof(void *)) ||
            row_count >= SUDEKIMP_ITEM_ACTIVATION_MAX_ROWS) {
            ZeroMemory(items, sizeof(*items));
            return FALSE;
        }
        items->rows[row_count].slot = raw_slot;
        items->rows[row_count].native_item = item;
        ++row_count;
    }
    items->row_count = row_count;
    return TRUE;
}

SudekiMpItemActivationResult SudekiMpActivateCharacterItem(
    void *source_character,
    unsigned int item_slot,
    void *target_character,
    unsigned int target_party_slot
) {
    SudekiMpItemActivationResult result = empty_result(
        SUDEKIMP_ITEM_ACTIVATION_INVALID_CONTEXT);
    void *inventory;
    int before_count;
    int after_count;

    if (source_character == NULL || !readable_memory(source_character, 0x64u) ||
        !inventory_context(&inventory) || !party_target_exact(target_character,
            target_party_slot)) {
        return result;
    }
    before_count = call_inventory_category_count(inventory,
        INVENTORY_ITEM_CATEGORY);
    if (before_count < 0 || item_slot >= (unsigned int)before_count) {
        result.status = SUDEKIMP_ITEM_ACTIVATION_INVALID_SELECTION;
        return result;
    }
    result.slot = item_slot;
    result.target_party_slot = target_party_slot;
    result.expected_item = native_inventory_lookup(inventory,
        INVENTORY_ITEM_CATEGORY, (int)item_slot);
    if (result.expected_item == NULL ||
        !readable_memory(result.expected_item, sizeof(void *))) {
        result.status = SUDEKIMP_ITEM_ACTIVATION_NOT_AVAILABLE;
        return result;
    }
    if (!native_apply_item(source_character, result.expected_item,
            (int)target_party_slot)) {
        result.status = SUDEKIMP_ITEM_ACTIVATION_REJECTED;
        return result;
    }
    native_inventory_remove(inventory, (int)item_slot);
    after_count = call_inventory_category_count(inventory,
        INVENTORY_ITEM_CATEGORY);
    result.observed_target = target_character;
    result.status = after_count == before_count - 1 ?
        SUDEKIMP_ITEM_ACTIVATION_STARTED :
        SUDEKIMP_ITEM_ACTIVATION_UNVERIFIED;
    return result;
}

const char *SudekiMpItemActivationStatusName(
    SudekiMpItemActivationStatus status
) {
    switch (status) {
    case SUDEKIMP_ITEM_ACTIVATION_STARTED: return "started";
    case SUDEKIMP_ITEM_ACTIVATION_INVALID_CONTEXT: return "invalid_context";
    case SUDEKIMP_ITEM_ACTIVATION_INVALID_SELECTION: return "invalid_selection";
    case SUDEKIMP_ITEM_ACTIVATION_NOT_AVAILABLE: return "not_available";
    case SUDEKIMP_ITEM_ACTIVATION_REJECTED: return "rejected";
    case SUDEKIMP_ITEM_ACTIVATION_UNVERIFIED: return "unverified";
    default: return "unknown";
    }
}
