#ifndef SUDEKIMP_SHOP_CATALOG_ADAPTER_H
#define SUDEKIMP_SHOP_CATALOG_ADAPTER_H

#include "engine/merchant_checkout.h"

#include <stddef.h>
#include <windows.h>

typedef enum SudekiMpShopCatalogCaptureFailure {
    SUDEKIMP_SHOP_CATALOG_CAPTURE_NONE = 0,
    SUDEKIMP_SHOP_CATALOG_CAPTURE_INVALID_ARGUMENT,
    SUDEKIMP_SHOP_CATALOG_CAPTURE_UNINITIALIZED,
    SUDEKIMP_SHOP_CATALOG_CAPTURE_ROOT,
    SUDEKIMP_SHOP_CATALOG_CAPTURE_VTABLE,
    SUDEKIMP_SHOP_CATALOG_CAPTURE_MODE,
    SUDEKIMP_SHOP_CATALOG_CAPTURE_VECTOR,
    SUDEKIMP_SHOP_CATALOG_CAPTURE_ENTRY,
    SUDEKIMP_SHOP_CATALOG_CAPTURE_DEFINITION,
    SUDEKIMP_SHOP_CATALOG_CAPTURE_DUPLICATE
} SudekiMpShopCatalogCaptureFailure;

/* Reads the shared native CShopInventory into a pointer-free catalog. The
 * caller must provide a separately proven merchant interaction provenance;
 * CShopInventory itself deliberately has no merchant/actor identity. */
BOOL SudekiMpShopCatalogAdapterInitialize(HMODULE game_module);
BOOL SudekiMpShopCatalogAdapterCapture(
    uint64_t merchant_provenance,
    uint32_t merchant_generation,
    BOOL require_buy_mode,
    SudekiMpMerchantCatalogSnapshot *snapshot
);
void SudekiMpShopCatalogAdapterReset(void);
SudekiMpShopCatalogCaptureFailure SudekiMpShopCatalogAdapterLastFailure(void);

/* Exact preferred-base and ASLR-aware loader gates. */
BOOL SudekiMpShopCatalogAdapterSignaturesMatch(
    const uint8_t *image,
    size_t image_size
);
BOOL SudekiMpShopCatalogAdapterLoadedSignaturesMatch(
    const uint8_t *image,
    size_t image_size,
    uintptr_t loaded_image_base
);

#endif
