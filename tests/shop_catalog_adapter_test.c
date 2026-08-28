#include "hooks/shop_catalog_adapter.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

int main(void) {
    const size_t size = 0x17d46u;
    uint8_t *image = (uint8_t *)calloc(1u, size);
    uint32_t operand = 0x00808d44u;

    CHECK(image != NULL);
    if (image == NULL) return 1;
    image[0x17d40u] = 0xa1u;
    memcpy(image + 0x17d41u, &operand, sizeof(operand));
    image[0x17d45u] = 0xc3u;
    CHECK(SudekiMpShopCatalogAdapterSignaturesMatch(image, size));
    operand = 0x00908d44u;
    memcpy(image + 0x17d41u, &operand, sizeof(operand));
    CHECK(!SudekiMpShopCatalogAdapterSignaturesMatch(image, size));
    CHECK(SudekiMpShopCatalogAdapterLoadedSignaturesMatch(
        image, size, 0x00500000u));
    image[0x17d45u] = 0x90u;
    CHECK(!SudekiMpShopCatalogAdapterLoadedSignaturesMatch(
        image, size, 0x00500000u));
    free(image);
    if (failures != 0) {
        fprintf(stderr, "shop_catalog_adapter_test: %d failure(s)\n", failures);
        return 1;
    }
    puts("shop_catalog_adapter_test: PASS");
    return 0;
}
