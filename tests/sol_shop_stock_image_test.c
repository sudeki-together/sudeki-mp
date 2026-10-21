/* SolShopStock against the player's own supported SOLWORLDM.gex (argv[1],
 * a Windows path). Asserts SHAPE ONLY: tier counts and thresholds, list
 * sizes, the name text id and the item-id bound. The decoded item lists and
 * prices are game data and are neither asserted nor printed here.
 *
 * Includes the module source so the static load_path() (read + SHA-256 +
 * decode, the body of SudekiMpSolShopStockLoad) runs on the given path. */
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "../src/engine/sol_shop_stock.c"

static uint32_t fnv(const char *name) {
    uint32_t h = 0x811c9dc5u;
    for (; *name; ++name) { h ^= (uint8_t)tolower((unsigned char)*name); h *= 0x01000193u; }
    return h;
}

static int all_zero(const void *p, size_t n) {
    const uint8_t *b = p;
    for (size_t i = 0; i < n; ++i) if (b[i]) return 0;
    return 1;
}

static int same(const SudekiMpSolShopStock *a, const SudekiMpSolShopStock *b) {
    if (a->name_text != b->name_text || a->tier_count != b->tier_count ||
        a->weapon_count != b->weapon_count || a->sell_count != b->sell_count) return 0;
    for (unsigned t = 0; t < a->tier_count; ++t) {
        if (a->tiers[t].min_block != b->tiers[t].min_block || a->tiers[t].count != b->tiers[t].count) return 0;
        for (unsigned i = 0; i < a->tiers[t].count; ++i) if (a->tiers[t].items[i] != b->tiers[t].items[i]) return 0;
    }
    for (unsigned w = 0; w < a->weapon_count; ++w) if (a->weapons[w] != b->weapons[w]) return 0;
    for (unsigned i = 0; i < a->sell_count; ++i)
        if (a->sell[i].item != b->sell[i].item || a->sell[i].price != b->sell[i].price) return 0;
    return 1;
}

static uint8_t *read_all(const wchar_t *path, size_t *size) {
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    assert(f != INVALID_HANDLE_VALUE);
    DWORD n = GetFileSize(f, NULL), got = 0;
    uint8_t *raw = malloc(n);
    assert(raw && ReadFile(f, raw, n, &got, NULL) && got == n);
    CloseHandle(f);
    *size = n;
    return raw;
}

/* Body offset and length of a function, read independently of the decoder. */
static void locate(const uint8_t *raw, size_t size, const char *name, size_t *body, uint32_t *length) {
    uint32_t slots = rd32(raw + 0x0c), count = rd32(raw + 0x10), hash = fnv(name);
    size_t code_base = 0x14u + (size_t)count * 16u + 8u + (size_t)slots * 4u;
    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t *e = raw + 0x14u + (size_t)i * 16u;
        if (rd32(e + 4) != hash) continue;
        *body = code_base + rd32(e + 8);
        *length = rd32(e + 12) & 0x7fffffffu;
        assert(*length && *body + *length <= size);
        return;
    }
    assert(!"function not found");
}

int main(int argc, char **argv) {
    static const unsigned counts[] = {19, 16, 13, 9, 6};
    static const int32_t thresholds[] = {6100, 5000, 3200, 2200, INT32_MIN};
    wchar_t path[32768];
    SudekiMpSolShopStock loaded, decoded;

    assert(argc == 2 && MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, path, 32768));
    memset(&loaded, 0xa5, sizeof(loaded));
    assert(load_path(path, &loaded));

    assert(loaded.name_text == 5687);
    assert(loaded.tier_count == 5);
    for (unsigned t = 0; t < 5; ++t) {
        assert(loaded.tiers[t].count == counts[t] && loaded.tiers[t].min_block == thresholds[t]);
        for (unsigned i = 0; i < loaded.tiers[t].count; ++i) assert(loaded.tiers[t].items[i] < 999);
    }
    assert(loaded.weapon_count == 4);
    for (unsigned w = 0; w < loaded.weapon_count; ++w) assert(loaded.weapons[w] < 999);
    assert(loaded.sell_count == 53);
    for (unsigned i = 0; i < loaded.sell_count; ++i) assert(loaded.sell[i].item < 999);
    printf("SolShopStock image shape: name=5687 tiers=19/16/13/9/6 "
           "thresholds=6100/5000/3200/2200/default weapons=4 sell=53 ids<999: PASS\n");

    /* IsStoryBlockAfter(n) == (block >= n) on the decoded thresholds. */
    assert(SudekiMpSolShopStockTier(&loaded, 6100) == &loaded.tiers[0]);
    assert(SudekiMpSolShopStockTier(&loaded, 6099) == &loaded.tiers[1]);
    assert(SudekiMpSolShopStockTier(&loaded, 5000) == &loaded.tiers[1]);
    assert(SudekiMpSolShopStockTier(&loaded, 4999) == &loaded.tiers[2]);
    assert(SudekiMpSolShopStockTier(&loaded, 3200) == &loaded.tiers[2]);
    assert(SudekiMpSolShopStockTier(&loaded, 2200) == &loaded.tiers[3]);
    assert(SudekiMpSolShopStockTier(&loaded, 2199) == &loaded.tiers[4]);
    assert(SudekiMpSolShopStockTier(&loaded, 0) == &loaded.tiers[4]);
    assert(SudekiMpSolShopStockTier(&loaded, INT32_MAX) == &loaded.tiers[0]);
    assert(SudekiMpSolShopStockTier(&loaded, INT32_MIN) == &loaded.tiers[4]);
    printf("SolShopStock image tier boundaries (block == threshold is inside): PASS\n");

    size_t size;
    uint8_t *raw = read_all(path, &size);
    assert(SudekiMpSolShopStockDecode(raw, size, &decoded) && same(&loaded, &decoded));

    /* One-byte mutations: the first opcode, the first operand byte (a canonical
     * integer float becomes fractional) and the final ret0 of each function. */
    static const char *const names[] = {"ShopInventory", "WeaponCheck", "SetupShopSell",
        "StartShopping", "IsStoryBlockAfter|N"};
    for (unsigned n = 0; n < sizeof(names) / sizeof(names[0]); ++n) {
        size_t body = SIZE_MAX; uint32_t length = 0;
        locate(raw, size, names[n], &body, &length);
        assert(body < size && length >= 2u && length <= size - body);
        const size_t offsets[] = {body, body + 1u, body + length - 1u};
        for (unsigned k = 0; k < 3; ++k) {
            raw[offsets[k]] ^= 0x01;
            memset(&decoded, 0xa5, sizeof(decoded));
            assert(!SudekiMpSolShopStockDecode(raw, size, &decoded) && all_zero(&decoded, sizeof(decoded)));
            raw[offsets[k]] ^= 0x01;
        }
        assert(SudekiMpSolShopStockDecode(raw, size, &decoded) && same(&loaded, &decoded));
        printf("SolShopStock image refuses one-byte mutations in %s: PASS\n", names[n]);
    }
    free(raw);

    /* The SHA-256 gate: this executable is not the supported GEX. */
    wchar_t self[MAX_PATH];
    DWORD n = GetModuleFileNameW(NULL, self, MAX_PATH);
    assert(n && n < MAX_PATH);
    memset(&decoded, 0xa5, sizeof(decoded));
    assert(!load_path(self, &decoded) && all_zero(&decoded, sizeof(decoded)));
    printf("SolShopStock image load refuses a file with another SHA-256: PASS\n");

    printf("SolShopStockImageTest: PASS\n");
    return 0;
}
