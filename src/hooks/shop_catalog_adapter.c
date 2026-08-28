#include "hooks/shop_catalog_adapter.h"

#include <stdint.h>
#include <string.h>

enum {
    PREFERRED_IMAGE_BASE = 0x00400000u,
    RVA_SHOP_INVENTORY_GLOBAL = 0x00408d44u,
    RVA_ITEM_MANAGER_GLOBAL = 0x00408d80u,
    RVA_ITEM_MANAGER_VTABLE = 0x002c693cu,
    RVA_GET_SHOP_INVENTORY = 0x00017d40u,
    ITEM_DEFINITION_LIMIT = 999u,
    SHOP_VECTOR_BUY_OFFSET = 0x10u,
    SHOP_VECTOR_SELL_OFFSET = 0x20u,
    SHOP_VECTOR_COUNT_OFFSET = 0x04u,
    SHOP_VECTOR_DATA_OFFSET = 0x0cu,
    SHOP_MODE_OFFSET = 0x0cu,
    ITEM_DEFINITION_ID_OFFSET = 0x14u,
    ITEM_DEFINITION_PRICE_OFFSET = 0x80u,
    SHOP_ENTRY_ITEM_ID_OFFSET = 0x00u,
    SHOP_ENTRY_UNIT_PRICE_OFFSET = 0x04u
};

static uint8_t *game_base;
static SudekiMpShopCatalogCaptureFailure last_capture_failure;

static BOOL readable_memory(const void *pointer, size_t size) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t start;
    uintptr_t end;
    uintptr_t region_end;

    if (pointer == NULL || size == 0u ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0u ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0u) {
        return FALSE;
    }
    start = (uintptr_t)pointer;
    end = start + size;
    region_end = (uintptr_t)information.BaseAddress + information.RegionSize;
    return end >= start && end <= region_end;
}

static BOOL read_global(uint32_t rva, uint8_t **value) {
    if (game_base == NULL || value == NULL ||
        !readable_memory(game_base + rva, sizeof(*value))) {
        return FALSE;
    }
    *value = *(uint8_t **)(game_base + rva);
    return *value != NULL;
}

static uint64_t hash_u32(uint64_t hash, uint32_t value) {
    uint32_t shift;

    for (shift = 0u; shift < 32u; shift += 8u) {
        hash ^= (value >> shift) & 0xffu;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static BOOL signatures_match(
    const uint8_t *image,
    size_t image_size,
    uint32_t expected_global
) {
    uint32_t operand;

    if (image == NULL || image_size < RVA_GET_SHOP_INVENTORY + 6u) {
        return FALSE;
    }
    memcpy(&operand, image + RVA_GET_SHOP_INVENTORY + 1u, sizeof(operand));
    return image[RVA_GET_SHOP_INVENTORY] == 0xa1u &&
        operand == expected_global &&
        image[RVA_GET_SHOP_INVENTORY + 5u] == 0xc3u;
}

BOOL SudekiMpShopCatalogAdapterSignaturesMatch(
    const uint8_t *image,
    size_t image_size
) {
    return signatures_match(image, image_size,
        PREFERRED_IMAGE_BASE + RVA_SHOP_INVENTORY_GLOBAL);
}

BOOL SudekiMpShopCatalogAdapterLoadedSignaturesMatch(
    const uint8_t *image,
    size_t image_size,
    uintptr_t loaded_image_base
) {
    if (loaded_image_base == 0u ||
        loaded_image_base > UINT32_MAX - RVA_SHOP_INVENTORY_GLOBAL) {
        return FALSE;
    }
    return signatures_match(image, image_size,
        (uint32_t)(loaded_image_base + RVA_SHOP_INVENTORY_GLOBAL));
}

BOOL SudekiMpShopCatalogAdapterInitialize(HMODULE game_module) {
    uint8_t *base = (uint8_t *)game_module;

    SudekiMpShopCatalogAdapterReset();
    if (base == NULL || !SudekiMpShopCatalogAdapterLoadedSignaturesMatch(
            base, RVA_GET_SHOP_INVENTORY + 6u, (uintptr_t)base)) {
        return FALSE;
    }
    game_base = base;
    last_capture_failure = SUDEKIMP_SHOP_CATALOG_CAPTURE_NONE;
    return TRUE;
}

void SudekiMpShopCatalogAdapterReset(void) {
    game_base = NULL;
    last_capture_failure = SUDEKIMP_SHOP_CATALOG_CAPTURE_NONE;
}

SudekiMpShopCatalogCaptureFailure SudekiMpShopCatalogAdapterLastFailure(void) {
    return last_capture_failure;
}

BOOL SudekiMpShopCatalogAdapterCapture(
    uint64_t merchant_provenance,
    uint32_t merchant_generation,
    BOOL require_buy_mode,
    SudekiMpMerchantCatalogSnapshot *snapshot
) {
    uint8_t *shop;
    uint8_t *item_manager;
    uint8_t *vector;
    uint8_t **entry_slots;
    uint32_t mode;
    uint32_t count;
    uint32_t index;
    uint64_t fingerprint = UINT64_C(1469598103934665603);
    SudekiMpMerchantCatalogSnapshot candidate;

    last_capture_failure = SUDEKIMP_SHOP_CATALOG_CAPTURE_NONE;
    if (snapshot == NULL || merchant_provenance == 0u ||
        merchant_generation == 0u) {
        last_capture_failure = SUDEKIMP_SHOP_CATALOG_CAPTURE_INVALID_ARGUMENT;
        return FALSE;
    }
    if (game_base == NULL) {
        last_capture_failure = SUDEKIMP_SHOP_CATALOG_CAPTURE_UNINITIALIZED;
        return FALSE;
    }
    if (!read_global(RVA_SHOP_INVENTORY_GLOBAL, &shop) ||
        !read_global(RVA_ITEM_MANAGER_GLOBAL, &item_manager) ||
        !readable_memory(shop, SHOP_VECTOR_SELL_OFFSET + 0x10u) ||
        !readable_memory(item_manager, 0x10u)) {
        last_capture_failure = SUDEKIMP_SHOP_CATALOG_CAPTURE_ROOT;
        return FALSE;
    }
    if (*(void **)item_manager != game_base + RVA_ITEM_MANAGER_VTABLE) {
        last_capture_failure = SUDEKIMP_SHOP_CATALOG_CAPTURE_VTABLE;
        return FALSE;
    }
    mode = *(const uint32_t *)(shop + SHOP_MODE_OFFSET);
    /*
     * The script-owned CShopInventory mode is not the visible Shop-layer
     * Buy/Sell tab.  A merchant's authored stock always resides in its first
     * vector, so a trusted ShopStart capture must not reject a valid merchant
     * merely because that auxiliary field has another value.
     */
    if (!require_buy_mode && mode > 1u) {
        last_capture_failure = SUDEKIMP_SHOP_CATALOG_CAPTURE_MODE;
        return FALSE;
    }
    vector = shop + (require_buy_mode ? SHOP_VECTOR_BUY_OFFSET :
        (mode == 0u ? SHOP_VECTOR_BUY_OFFSET : SHOP_VECTOR_SELL_OFFSET));
    count = *(const uint32_t *)(vector + SHOP_VECTOR_COUNT_OFFSET);
    entry_slots = *(uint8_t ***)(vector + SHOP_VECTOR_DATA_OFFSET);
    if (count == 0u || count > SUDEKIMP_MERCHANT_CHECKOUT_MAX_ENTRIES ||
        !readable_memory(entry_slots, count * sizeof(*entry_slots))) {
        last_capture_failure = SUDEKIMP_SHOP_CATALOG_CAPTURE_VECTOR;
        return FALSE;
    }
    ZeroMemory(&candidate, sizeof(candidate));
    candidate.valid = 1;
    candidate.merchant_provenance = merchant_provenance;
    candidate.merchant_generation = merchant_generation;
    candidate.entry_count = count;
    for (index = 0u; index < count; ++index) {
        uint8_t *entry = entry_slots[index];
        uint32_t item_id;
        uint32_t unit_price;
        uint8_t **definition_slot;
        uint8_t *definition;
        uint32_t prior;

        if (!readable_memory(entry, 8u)) {
            last_capture_failure = SUDEKIMP_SHOP_CATALOG_CAPTURE_ENTRY;
            return FALSE;
        }
        item_id = *(const uint32_t *)(entry + SHOP_ENTRY_ITEM_ID_OFFSET);
        unit_price = *(const uint32_t *)(entry + SHOP_ENTRY_UNIT_PRICE_OFFSET);
        if (item_id >= ITEM_DEFINITION_LIMIT || unit_price == 0u) {
            last_capture_failure = SUDEKIMP_SHOP_CATALOG_CAPTURE_ENTRY;
            return FALSE;
        }
        definition_slot = (uint8_t **)(item_manager + 0x0cu + item_id * 4u);
        if (!readable_memory(definition_slot, sizeof(*definition_slot))) {
            last_capture_failure = SUDEKIMP_SHOP_CATALOG_CAPTURE_DEFINITION;
            return FALSE;
        }
        definition = *definition_slot;
        if (!readable_memory(definition, ITEM_DEFINITION_ID_OFFSET + 4u) ||
            *(const uint32_t *)(definition + ITEM_DEFINITION_ID_OFFSET) != item_id) {
            last_capture_failure = SUDEKIMP_SHOP_CATALOG_CAPTURE_DEFINITION;
            return FALSE;
        }
        for (prior = 0u; prior < index; ++prior) {
            if (candidate.entries[prior].item_id == item_id) {
                last_capture_failure = SUDEKIMP_SHOP_CATALOG_CAPTURE_DUPLICATE;
                return FALSE;
            }
        }
        candidate.entries[index].item_id = item_id;
        candidate.entries[index].unit_price = unit_price;
        /* CShopInventory is an authored, non-depleting listing: +4 is its
         * price, not stock.  Stack-cap validation remains a separate native
         * checkout precondition. */
        candidate.entries[index].listed_quantity = UINT32_MAX;
        fingerprint = hash_u32(fingerprint, item_id);
        fingerprint = hash_u32(fingerprint, unit_price);
    }
    candidate.catalog_generation = (uint32_t)(fingerprint ^ (fingerprint >> 32u));
    if (candidate.catalog_generation == 0u) {
        candidate.catalog_generation = 1u;
    }
    *snapshot = candidate;
    return TRUE;
}
