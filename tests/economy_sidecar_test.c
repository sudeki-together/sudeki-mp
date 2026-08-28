#include "engine/economy_sidecar.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

int main(void) {
    SudekiMpEconomyCoordinator coordinator;
    SudekiMpEconomyCoordinatorSnapshot source;
    SudekiMpEconomyCoordinatorSnapshot decoded;
    uint8_t identity[32] = {9u};
    uint8_t wrong_identity[32] = {10u};
    uint8_t *bytes;
    size_t written = 0u;
    size_t size = SudekiMpEconomySidecarSize();

    bytes = (uint8_t *)malloc(size);
    CHECK(bytes != NULL);
    if (bytes == NULL) {
        return 1;
    }
    SudekiMpEconomyCoordinatorInitialize(&coordinator, identity);
    CHECK(SudekiMpEntitlementLedgerRegisterEligible(
        &coordinator.entitlements, 12u, 4u) == SUDEKIMP_ENTITLEMENT_REGISTERED);
    CHECK(SudekiMpEconomyCoordinatorExport(&coordinator, &source));
    CHECK(SudekiMpEconomySidecarEncode(&source, bytes, size, &written));
    CHECK(written == size);
    memset(&decoded, 0, sizeof(decoded));
    CHECK(SudekiMpEconomySidecarDecode(bytes, written, identity, &decoded));
    CHECK(decoded.entitlements.entry_count == 1u);
    CHECK(!SudekiMpEconomySidecarDecode(bytes, written, wrong_identity, &decoded));
    bytes[size - 1u] ^= 1u;
    CHECK(!SudekiMpEconomySidecarDecode(bytes, written, identity, &decoded));

    CHECK(SudekiMpEconomySidecarWriteAtomic("economy-sidecar-test.tmp", &source));
    CHECK(SudekiMpEconomySidecarReadFile("economy-sidecar-test.tmp", identity,
        &decoded));
    CHECK(!SudekiMpEconomySidecarReadFile("economy-sidecar-test.tmp",
        wrong_identity, &decoded));
    CHECK(DeleteFileA("economy-sidecar-test.tmp") != FALSE);
    free(bytes);
    if (failures != 0) {
        fprintf(stderr, "economy_sidecar_test: %d failure(s)\n", failures);
        return 1;
    }
    puts("economy_sidecar_test: PASS");
    return 0;
}
