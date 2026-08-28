#ifndef SUDEKIMP_MERCHANT_PROVENANCE_ADAPTER_H
#define SUDEKIMP_MERCHANT_PROVENANCE_ADAPTER_H

#include <windows.h>

#include "engine/merchant_provenance.h"
#include "engine/merchant_checkout.h"

/* Passive SOL opcode observer. It is deliberately independent from native
 * ShopStart activation: it never suppresses, redirects, or calls ShopStart. */
BOOL SudekiMpInstallMerchantProvenanceAdapter(HMODULE game_module, BOOL enabled);
void SudekiMpUninstallMerchantProvenanceAdapter(void);
BOOL SudekiMpMerchantProvenanceAdapterGet(
    SudekiMpMerchantProvenance *provenance
);
/* Returns only the catalog captured in the same trusted ShopStart session.
 * Its item rows are copied scalar data, never native inventory pointers. */
BOOL SudekiMpMerchantProvenanceAdapterGetCatalog(
    SudekiMpMerchantCatalogSnapshot *catalog
);

/* Returns a scalar browse-only checkout session only while the originating
 * trusted merchant interaction remains valid.  It never exposes a native
 * transaction path. */
BOOL SudekiMpMerchantProvenanceAdapterGetCheckoutSession(
    SudekiMpMerchantCheckoutSession *session
);

#endif
