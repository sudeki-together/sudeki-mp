/* Pure SolShopStock decoder test over small SYNTHETIC GEX images built here
 * from opcodes and name hashes. No game bytes, item lists or prices are used:
 * every item id, threshold and price below is invented. */
#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/sol_shop_stock.h"

static uint32_t fnv(const char *name) {
    uint32_t h = 0x811c9dc5u;
    for (; *name; ++name) { h ^= (uint8_t)tolower((unsigned char)*name); h *= 0x01000193u; }
    return h;
}

static uint32_t fbits(float value) { uint32_t out; memcpy(&out, &value, 4); return out; }

/* ---- bytecode builder -------------------------------------------------- */

typedef struct Body { uint8_t b[8192]; uint32_t n; } Body;

static void u8(Body *b, uint8_t v) { assert(b->n < sizeof(b->b)); b->b[b->n++] = v; }
static void u32(Body *b, uint32_t v) { for (int i = 0; i < 4; ++i) u8(b, (uint8_t)(v >> (8 * i))); }
static void op(Body *b, uint8_t o) { u8(b, o); }
static void hop(Body *b, uint8_t o, const char *name) { u8(b, o); u32(b, fnv(name)); }
static void immbits(Body *b, uint32_t bits) { u8(b, 0x06); u32(b, bits); }
static void imm(Body *b, float value) { immbits(b, fbits(value)); }
static void push_bool(Body *b, uint8_t v) { u8(b, 0x07); u8(b, v); }
static uint32_t jmp(Body *b) { u8(b, 0x20); uint32_t at = b->n; u32(b, 0); return at; }
static void patch(Body *b, uint32_t at, uint32_t target) {
    uint32_t rel = target - (at + 4u);
    for (int i = 0; i < 4; ++i) b->b[at + i] = (uint8_t)(rel >> (8 * i));
}
static void set32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i)); }
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

typedef struct Spec {
    uint16_t name;
    unsigned gates;
    int32_t thresholds[16];
    unsigned counts[17];          /* counts[gates] is the default list */
    uint16_t items[17][80];
    unsigned weapons;
    uint16_t weapon[16];
    unsigned sells;
    uint16_t sell_item[128];
    int32_t sell_price[128];
} Spec;

typedef struct Fn { const char *name; uint32_t hash, flags; Body body; } Fn;
typedef struct Image { Fn fn[24]; unsigned count; } Image;

static Fn *add(Image *im, const char *name, uint32_t flags) {
    assert(im->count < 24);
    Fn *f = &im->fn[im->count++];
    memset(f, 0, sizeof(*f));
    f->name = name; f->hash = fnv(name); f->flags = flags;
    return f;
}
static Fn *find(Image *im, const char *name) {
    for (unsigned i = 0; i < im->count; ++i) if (!strcmp(im->fn[i].name, name)) return &im->fn[i];
    assert(!"function");
    return NULL;
}

#define PLAIN 0x00170005u
#define ARGS  0x00170003u

static void build_after(Body *b) {
    op(b, 0x19); hop(b, 0x1b, "nStoryBlock"); hop(b, 0x0c, "nStoryBlock");
    hop(b, 0x27, "GetStoryBlock"); hop(b, 0x08, "nStoryBlock"); op(b, 0x0d); op(b, 0x26);
    uint32_t to_true = jmp(b); push_bool(b, 0); op(b, 0x2a); uint32_t to_pop = jmp(b);
    patch(b, to_true, b->n); push_bool(b, 1); op(b, 0x2a);
    patch(b, to_pop, b->n); op(b, 0x1a); op(b, 0x2b);
}
static void build_buy_n(Body *b) {
    op(b, 0x19); hop(b, 0x1b, "itemId"); hop(b, 0x0c, "itemId"); hop(b, 0x08, "itemId");
    imm(b, 1.0f); op(b, 0x05); hop(b, 0x27, "ShopAddItemBuy|NN"); op(b, 0x2a); op(b, 0x1a); op(b, 0x2b);
}
static void build_mode_nn(Body *b, uint8_t buy) {
    op(b, 0x19); hop(b, 0x1b, "price"); hop(b, 0x0c, "price"); hop(b, 0x1b, "itemId"); hop(b, 0x0c, "itemId");
    push_bool(b, buy); hop(b, 0x27, "ShopSetMode|B"); op(b, 0x0a);
    hop(b, 0x08, "itemId"); hop(b, 0x08, "price"); hop(b, 0x27, "ShopAddItemSleeper|NN");
    op(b, 0x2a); op(b, 0x1a); op(b, 0x2b);
}
static void build_sleeper(Body *b) {
    op(b, 0x19); hop(b, 0x1b, "price"); hop(b, 0x0c, "price"); hop(b, 0x1b, "itemId"); hop(b, 0x0c, "itemId");
    hop(b, 0x08, "g_shopSleeper"); imm(b, 1.0f); op(b, 0x01); hop(b, 0x0c, "g_shopSleeper");
    hop(b, 0x08, "g_shopSleeper"); imm(b, 15.0f); op(b, 0x11); op(b, 0x26); uint32_t skip = jmp(b);
    imm(b, 0.0f); op(b, 0x25); imm(b, 0.0f); hop(b, 0x0c, "g_shopSleeper");
    patch(b, skip, b->n);
    hop(b, 0x08, "itemId"); hop(b, 0x08, "price"); hop(b, 0x27, "ShopAddItem|NN");
    op(b, 0x2a); op(b, 0x1a); op(b, 0x2b);
}
static void build_start(Body *b, uint16_t name) {
    hop(b, 0x27, "ShopClearInventory"); op(b, 0x0a);
    imm(b, (float)name); hop(b, 0x27, "ShopSetName|N"); op(b, 0x0a);
    hop(b, 0x27, "ShopInventory"); op(b, 0x0a);
    hop(b, 0x27, "SetupShopSell"); op(b, 0x0a);
    hop(b, 0x27, "ShopStartBuy"); op(b, 0x0a);
    uint32_t loop = b->n;
    hop(b, 0x27, "IsShopActive"); push_bool(b, 0); op(b, 0x11); op(b, 0x17); op(b, 0x26);
    uint32_t out = jmp(b); op(b, 0x24); uint32_t back = jmp(b); patch(b, back, loop);
    patch(b, out, b->n);
    hop(b, 0x27, "ShopClearInventory"); op(b, 0x0a); hop(b, 0x27, "ShowDialog"); op(b, 0x0a); op(b, 0x2b);
}
static void build_item(Body *b, uint16_t item) { imm(b, (float)item); hop(b, 0x27, "ShopAddItemBuy|N"); op(b, 0x0a); }
static void build_inventory(Body *b, const Spec *s) {
    uint32_t ends[16];
    for (unsigned g = 0; g < s->gates; ++g) {
        imm(b, (float)s->thresholds[g]); hop(b, 0x27, "IsStoryBlockAfter|N");
        push_bool(b, 1); op(b, 0x11); op(b, 0x26);
        uint32_t next = jmp(b);
        for (unsigned i = 0; i < s->counts[g]; ++i) build_item(b, s->items[g][i]);
        ends[g] = jmp(b);
        patch(b, next, b->n);
    }
    for (unsigned i = 0; i < s->counts[s->gates]; ++i) build_item(b, s->items[s->gates][i]);
    for (unsigned g = 0; g < s->gates; ++g) patch(b, ends[g], b->n);
    hop(b, 0x27, "WeaponCheck"); op(b, 0x0a); op(b, 0x2b);
}
static void build_weapons(Body *b, const Spec *s) {
    for (unsigned w = 0; w < s->weapons; ++w) {
        imm(b, (float)s->weapon[w]); hop(b, 0x27, "GetQuantityOfItemInInventory|N");
        imm(b, 0.0f); op(b, 0x11); op(b, 0x26);
        uint32_t next = jmp(b);
        build_item(b, s->weapon[w]);
        patch(b, next, b->n);
    }
    op(b, 0x2b);
}
static void build_sell(Body *b, const Spec *s) {
    for (unsigned i = 0; i < s->sells; ++i) {
        imm(b, (float)s->sell_item[i]); imm(b, (float)s->sell_price[i]);
        hop(b, 0x27, "ShopAddItemSell|NN"); op(b, 0x0a);
    }
    op(b, 0x2b);
}

static void standard(Image *im, const Spec *s) {
    memset(im, 0, sizeof(*im));
    /* Unrelated script functions around the shop, as in a real table. */
    Fn *f = add(im, "SomeOtherScript", PLAIN); op(&f->body, 0x24); op(&f->body, 0x2b);
    f = add(im, "ShowDialog", PLAIN); op(&f->body, 0x2b);
    f = add(im, "ShopStartBuy", PLAIN); op(&f->body, 0x2b);
    build_sleeper(&add(im, "ShopAddItemSleeper|NN", ARGS)->body);
    build_mode_nn(&add(im, "ShopAddItemSell|NN", ARGS)->body, 0);
    build_mode_nn(&add(im, "ShopAddItemBuy|NN", ARGS)->body, 1);
    build_buy_n(&add(im, "ShopAddItemBuy|N", ARGS)->body);
    build_inventory(&add(im, "ShopInventory", PLAIN)->body, s);
    build_start(&add(im, "StartShopping", PLAIN)->body, s->name);
    build_sell(&add(im, "SetupShopSell", PLAIN)->body, s);
    build_weapons(&add(im, "WeaponCheck", PLAIN)->body, s);
    build_after(&add(im, "IsStoryBlockAfter|N", ARGS)->body);
    f = add(im, "YetAnotherScript|N", ARGS); op(&f->body, 0x2b);
}

enum { SLOTS = 64 };

static size_t entry_offset(const Image *im, const char *name) {
    for (unsigned i = 0; i < im->count; ++i) if (!strcmp(im->fn[i].name, name)) return 0x14u + i * 16u;
    assert(!"entry");
    return 0;
}

static uint8_t *assemble(const Image *im, size_t *size) {
    size_t code_base = 0x14u + im->count * 16u + 8u + SLOTS * 4u, code = 3; /* 3 padding bytes */
    for (unsigned i = 0; i < im->count; ++i) code += im->fn[i].body.n;
    *size = code_base + code + 16u;  /* trailing bytes, like the string dictionary */
    uint8_t *out = calloc(1, *size);
    assert(out);
    memcpy(out, "Version:0004", 12);
    set32(out + 0x0c, SLOTS); set32(out + 0x10, im->count);
    uint32_t at = 3;
    for (unsigned i = 0; i < im->count; ++i) {
        uint8_t *e = out + 0x14u + i * 16u;
        set32(e, im->fn[i].flags); set32(e + 4, im->fn[i].hash);
        set32(e + 8, at); set32(e + 12, im->fn[i].body.n);
        memcpy(out + code_base + at, im->fn[i].body.b, im->fn[i].body.n);
        at += im->fn[i].body.n;
    }
    return out;
}

static SudekiMpSolShopStock stock;

static int all_zero(const void *p, size_t n) {
    const uint8_t *b = p;
    for (size_t i = 0; i < n; ++i) if (b[i]) return 0;
    return 1;
}
static BOOL decode_image(const Image *im) {
    size_t size; uint8_t *raw = assemble(im, &size);
    memset(&stock, 0xa5, sizeof(stock));
    BOOL ok = SudekiMpSolShopStockDecode(raw, size, &stock);
    if (!ok) assert(all_zero(&stock, sizeof(stock)));
    free(raw);
    return ok;
}
static void refuse(const Image *im, const char *what) {
    assert(!decode_image(im));
    printf("SolShopStock refuses %s: PASS\n", what);
}

/* A small invented shop: 2 gated tiers + default, 2 weapons, 3 sell pairs. */
static void minimal(Spec *s) {
    memset(s, 0, sizeof(*s));
    s->name = 42;
    s->gates = 2; s->thresholds[0] = 300; s->thresholds[1] = 100;
    s->counts[0] = 3; s->items[0][0] = 510; s->items[0][1] = 511; s->items[0][2] = 512;
    s->counts[1] = 2; s->items[1][0] = 510; s->items[1][1] = 511;
    s->counts[2] = 1; s->items[2][0] = 0;
    s->weapons = 2; s->weapon[0] = 7; s->weapon[1] = 65535;
    s->sells = 3;
    s->sell_item[0] = 900; s->sell_price[0] = 1;
    s->sell_item[1] = 901; s->sell_price[1] = 0;
    s->sell_item[2] = 0; s->sell_price[2] = 1 << 24;
}

static void expect_matches(const Spec *s) {
    assert(stock.name_text == s->name);
    assert(stock.tier_count == s->gates + 1u);
    for (unsigned t = 0; t <= s->gates; ++t) {
        assert(stock.tiers[t].min_block == (t < s->gates ? s->thresholds[t] : INT32_MIN));
        assert(stock.tiers[t].count == s->counts[t]);
        for (unsigned i = 0; i < s->counts[t]; ++i) assert(stock.tiers[t].items[i] == s->items[t][i]);
    }
    assert(stock.weapon_count == s->weapons);
    for (unsigned w = 0; w < s->weapons; ++w) assert(stock.weapons[w] == s->weapon[w]);
    assert(stock.sell_count == s->sells);
    for (unsigned i = 0; i < s->sells; ++i)
        assert(stock.sell[i].item == s->sell_item[i] && stock.sell[i].price == s->sell_price[i]);
}

static Image im;
static Spec spec;

static void rebuild(void) { standard(&im, &spec); }

static void body_imm(Fn *f, uint32_t offset, uint32_t bits) { assert(f->body.b[offset] == 0x06); set32(f->body.b + offset + 1, bits); }

static void replace_hash(Fn *f, const char *from, const char *to) {
    uint32_t old = fnv(from);
    for (uint32_t i = 0; i + 5 <= f->body.n; ++i)
        if (f->body.b[i] == 0x27 && get32(f->body.b + i + 1) == old) { set32(f->body.b + i + 1, fnv(to)); return; }
    assert(!"hash");
}

static void test_success(void) {
    minimal(&spec); rebuild();
    assert(decode_image(&im));
    expect_matches(&spec);
    printf("SolShopStock decodes a minimal synthetic layout: PASS\n");

    /* Only a default list: no gated tier. */
    minimal(&spec); spec.gates = 0; spec.counts[0] = 2; spec.items[0][0] = 3; spec.items[0][1] = 4; rebuild();
    assert(decode_image(&im)); expect_matches(&spec);
    assert(stock.tier_count == 1 && stock.tiers[0].min_block == INT32_MIN);
    printf("SolShopStock decodes a default-only inventory: PASS\n");

    /* Every limit reached exactly. */
    memset(&spec, 0, sizeof(spec)); spec.name = 65535;
    spec.gates = SUDEKIMP_SOL_SHOP_MAX_TIERS - 1;
    for (unsigned g = 0; g < spec.gates; ++g) spec.thresholds[g] = 32767 - 9000 * (int32_t)g;
    spec.thresholds[spec.gates - 1] = -32768;
    for (unsigned t = 0; t <= spec.gates; ++t) {
        spec.counts[t] = SUDEKIMP_SOL_SHOP_MAX_ITEMS;
        for (unsigned i = 0; i < spec.counts[t]; ++i) spec.items[t][i] = (uint16_t)(1000 + t * 100 + i);
    }
    spec.weapons = SUDEKIMP_SOL_SHOP_MAX_WEAPONS;
    for (unsigned w = 0; w < spec.weapons; ++w) spec.weapon[w] = (uint16_t)(20 + w);
    spec.sells = SUDEKIMP_SOL_SHOP_MAX_SELL;
    for (unsigned i = 0; i < spec.sells; ++i) { spec.sell_item[i] = (uint16_t)(3000 + i); spec.sell_price[i] = (int32_t)(i * 7); }
    rebuild();
    assert(decode_image(&im)); expect_matches(&spec);
    printf("SolShopStock decodes every limit (8 tiers x 64, 8 weapons, 96 sells): PASS\n");

    /* Arguments. */
    assert(!SudekiMpSolShopStockDecode(NULL, 100, &stock) && all_zero(&stock, sizeof(stock)));
    { uint8_t one = 0; assert(!SudekiMpSolShopStockDecode(&one, 1, NULL)); }
    assert(!SudekiMpSolShopStockLoad(NULL, &stock) && all_zero(&stock, sizeof(stock)));
    assert(!SudekiMpSolShopStockLoad(GetModuleHandleW(NULL), NULL));
    /* This test executable has no Data\SOLWORLDM.gex beside it. */
    memset(&stock, 0xa5, sizeof(stock));
    assert(!SudekiMpSolShopStockLoad(GetModuleHandleW(NULL), &stock) && all_zero(&stock, sizeof(stock)));
    printf("SolShopStock rejects NULL arguments and a missing file: PASS\n");
}

static void test_refusals(void) {
    size_t size; uint8_t *raw;

    /* Wrong hashes. */
    minimal(&spec); rebuild(); find(&im, "StartShopping")->hash ^= 1; refuse(&im, "a missing StartShopping");
    rebuild(); find(&im, "ShopInventory")->hash = fnv("ShopInventoryX"); refuse(&im, "a missing ShopInventory");
    rebuild(); replace_hash(find(&im, "ShopInventory"), "ShopAddItemBuy|N", "ShopAddItemSell|N");
    refuse(&im, "a sell call inside ShopInventory");
    rebuild(); replace_hash(find(&im, "ShopInventory"), "IsStoryBlockAfter|N", "IsStoryBlockBefore|N");
    refuse(&im, "IsStoryBlockBefore as a tier gate");
    rebuild(); replace_hash(find(&im, "WeaponCheck"), "GetQuantityOfItemInInventory|N", "GetQuantityOfItemInInventoryX|N");
    refuse(&im, "a wrong WeaponCheck quantity native");
    rebuild(); replace_hash(find(&im, "SetupShopSell"), "ShopAddItemSell|NN", "ShopAddItemBuy|NN");
    refuse(&im, "a buy call inside SetupShopSell");
    rebuild(); replace_hash(find(&im, "StartShopping"), "ShopSetName|N", "ShopSetNameX|N");
    refuse(&im, "a wrong ShopSetName hash");
    rebuild(); replace_hash(find(&im, "ShopAddItemSleeper|NN"), "ShopAddItem|NN", "ShopAddItemX|NN");
    refuse(&im, "a wrong native ShopAddItem in the sleeper");
    rebuild(); { Fn *d = add(&im, "WeaponCheck", PLAIN); op(&d->body, 0x2b); } refuse(&im, "a duplicate function hash");
    rebuild(); { Fn *d = add(&im, "GetStoryBlock", PLAIN); push_bool(&d->body, 0); op(&d->body, 0x2a); }
    refuse(&im, "a script shadowing the GetStoryBlock native");
    rebuild(); { Fn *d = add(&im, "ShopAddItem|NN", ARGS); op(&d->body, 0x2b); }
    refuse(&im, "a script shadowing the ShopAddItem native");

    /* Flags and the length field's top bit. */
    rebuild(); find(&im, "ShopInventory")->flags = 0x00170004u; refuse(&im, "unexpected ShopInventory flags");
    rebuild(); find(&im, "IsStoryBlockAfter|N")->flags = PLAIN; refuse(&im, "unexpected helper flags");
    rebuild(); raw = assemble(&im, &size);
    { size_t e = entry_offset(&im, "SetupShopSell"); set32(raw + e + 12, get32(raw + e + 12) | 0x80000000u); }
    memset(&stock, 0xa5, sizeof(stock));
    assert(!SudekiMpSolShopStockDecode(raw, size, &stock) && all_zero(&stock, sizeof(stock))); free(raw);
    printf("SolShopStock refuses a length with the top bit set: PASS\n");

    /* Truncation. */
    const char *const bodies[] = {"StartShopping", "ShopInventory", "WeaponCheck", "SetupShopSell",
        "IsStoryBlockAfter|N", "ShopAddItemBuy|N", "ShopAddItemBuy|NN", "ShopAddItemSell|NN", "ShopAddItemSleeper|NN"};
    for (unsigned i = 0; i < sizeof(bodies) / sizeof(bodies[0]); ++i) {
        rebuild(); assert(decode_image(&im));
        raw = assemble(&im, &size);
        size_t e = entry_offset(&im, bodies[i]);
        uint32_t length = get32(raw + e + 12);
        set32(raw + e + 12, length - 1u);
        assert(!SudekiMpSolShopStockDecode(raw, size, &stock) && all_zero(&stock, sizeof(stock)));
        set32(raw + e + 12, length + 1u);
        assert(!SudekiMpSolShopStockDecode(raw, size, &stock));
        set32(raw + e + 12, length);
        assert(SudekiMpSolShopStockDecode(raw, size, &stock));
        set32(raw + e + 8, 0xfffffff0u);
        assert(!SudekiMpSolShopStockDecode(raw, size, &stock));
        free(raw);
        printf("SolShopStock refuses a truncated or overlong %s: PASS\n", bodies[i]);
    }

    /* IsStoryBlockAfter|N is the last body the decoder reads in this layout:
     * cutting the file one byte inside it refuses, at its end decodes. */
    rebuild(); raw = assemble(&im, &size);
    {
        size_t e = entry_offset(&im, "IsStoryBlockAfter|N");
        size_t end = 0x14u + im.count * 16u + 8u + SLOTS * 4u + get32(raw + e + 8) + get32(raw + e + 12);
        assert(end < size && !SudekiMpSolShopStockDecode(raw, end - 1u, &stock) && all_zero(&stock, sizeof(stock)));
        assert(SudekiMpSolShopStockDecode(raw, end, &stock));
    }
    free(raw);
    printf("SolShopStock refuses a function body cut by the end of the file: PASS\n");

    /* Container header. */
    rebuild(); raw = assemble(&im, &size);
    assert(SudekiMpSolShopStockDecode(raw, size, &stock));
    for (size_t cut = 0; cut < 0x14u + 16u; ++cut) assert(!SudekiMpSolShopStockDecode(raw, cut, &stock));
    raw[11] = '5'; assert(!SudekiMpSolShopStockDecode(raw, size, &stock)); raw[11] = '4';
    set32(raw + 0x0c, 48); assert(!SudekiMpSolShopStockDecode(raw, size, &stock));
    set32(raw + 0x0c, 0); assert(!SudekiMpSolShopStockDecode(raw, size, &stock));
    set32(raw + 0x0c, 8); assert(!SudekiMpSolShopStockDecode(raw, size, &stock)); /* count > slots */
    set32(raw + 0x0c, SLOTS); set32(raw + 0x10, 0); assert(!SudekiMpSolShopStockDecode(raw, size, &stock));
    set32(raw + 0x10, im.count); assert(SudekiMpSolShopStockDecode(raw, size, &stock));
    free(raw);
    printf("SolShopStock refuses a bad or truncated container header: PASS\n");

    /* Unexpected opcodes (operand-free swaps keep every length intact). */
    rebuild(); { Fn *f = find(&im, "ShopInventory"); uint32_t pop = 14u + 5u + 5u + 5u; /* first item's pop */
        assert(f->body.b[pop] == 0x0a); f->body.b[pop] = 0x24; } refuse(&im, "a yield in place of a pop");
    rebuild(); { Fn *f = find(&im, "ShopInventory"); f->body.b[f->body.n - 1u] = 0x2a; } refuse(&im, "ret in place of ret0");
    rebuild(); { Fn *f = find(&im, "IsStoryBlockAfter|N"); assert(f->body.b[21] == 0x0d); f->body.b[21] = 0x0e; }
    refuse(&im, "an IsStoryBlockAfter compare other than opcode 0x0D");
    rebuild(); { Fn *f = find(&im, "IsStoryBlockAfter|N"); assert(f->body.b[29] == 0x00 && f->body.b[37] == 0x01);
        f->body.b[29] = 1; f->body.b[37] = 0; } refuse(&im, "an inverted IsStoryBlockAfter result");
    rebuild(); body_imm(find(&im, "ShopAddItemBuy|N"), 16, fbits(2.0f)); refuse(&im, "a buy helper whose default price is not -1");
    rebuild(); { Fn *f = find(&im, "ShopAddItemSell|NN"); assert(f->body.b[21] == 0x07 && f->body.b[22] == 0); f->body.b[22] = 1; }
    refuse(&im, "a sell helper in buy mode");
    rebuild(); { Fn *f = find(&im, "StartShopping"); assert(f->body.b[17] == 0x27);
        /* call SetupShopSell before ShopInventory */
        uint8_t tmp[5]; memcpy(tmp, f->body.b + 17, 5); memcpy(f->body.b + 17, f->body.b + 23, 5); memcpy(f->body.b + 23, tmp, 5); }
    refuse(&im, "StartShopping calling the lists out of order");
    rebuild(); { Fn *f = find(&im, "SetupShopSell"); f->body.n = 0; op(&f->body, 0x2b); } refuse(&im, "an empty sell list");
    rebuild(); { Fn *f = find(&im, "WeaponCheck"); f->body.n = 0; op(&f->body, 0x2b); } refuse(&im, "an empty WeaponCheck");
    rebuild(); { Fn *f = find(&im, "WeaponCheck"); assert(f->body.b[f->body.n - 1u] == 0x2b);
        f->body.b[f->body.n - 1u] = 0x0a; op(&f->body, 0x2b); } refuse(&im, "a trailing extra instruction");

    /* Too many entries and empty lists. */
    minimal(&spec); spec.counts[1] = SUDEKIMP_SOL_SHOP_MAX_ITEMS + 1;
    for (unsigned i = 0; i < spec.counts[1]; ++i) spec.items[1][i] = (uint16_t)(600 + i);
    rebuild(); refuse(&im, "65 items in a gated tier");
    minimal(&spec); spec.counts[2] = SUDEKIMP_SOL_SHOP_MAX_ITEMS + 1;
    for (unsigned i = 0; i < spec.counts[2]; ++i) spec.items[2][i] = (uint16_t)(600 + i);
    rebuild(); refuse(&im, "65 items in the default tier");
    minimal(&spec); spec.gates = SUDEKIMP_SOL_SHOP_MAX_TIERS;
    for (unsigned g = 0; g <= spec.gates; ++g) {
        if (g < spec.gates) spec.thresholds[g] = 5000 - 100 * (int32_t)g;
        spec.counts[g] = 1; spec.items[g][0] = (uint16_t)(700 + g);
    }
    rebuild(); refuse(&im, "nine tiers");
    minimal(&spec); spec.weapons = SUDEKIMP_SOL_SHOP_MAX_WEAPONS + 1;
    for (unsigned w = 0; w < spec.weapons; ++w) spec.weapon[w] = (uint16_t)(30 + w);
    rebuild(); refuse(&im, "nine weapons");
    minimal(&spec); spec.sells = SUDEKIMP_SOL_SHOP_MAX_SELL + 1;
    for (unsigned i = 0; i < spec.sells; ++i) { spec.sell_item[i] = (uint16_t)(4000 + i); spec.sell_price[i] = 5; }
    rebuild(); refuse(&im, "97 sell pairs");
    minimal(&spec); spec.counts[0] = 0; rebuild(); refuse(&im, "an empty gated tier");
    minimal(&spec); spec.counts[2] = 0; rebuild(); refuse(&im, "an empty default tier");

    /* Tier order. */
    minimal(&spec); spec.thresholds[0] = 100; spec.thresholds[1] = 300; rebuild(); refuse(&im, "ascending thresholds");
    minimal(&spec); spec.thresholds[0] = 300; spec.thresholds[1] = 300; rebuild(); refuse(&im, "equal thresholds");

    /* Jump targets. */
    minimal(&spec); rebuild();
    { Fn *f = find(&im, "ShopInventory"); uint32_t skip = 5u + 5u + 2u + 1u + 1u; /* first gate's jmp */
        assert(f->body.b[skip] == 0x20); set32(f->body.b + skip + 1, get32(f->body.b + skip + 1) + 11u); }
    refuse(&im, "a gate skip jump past the next gate");
    rebuild(); { Fn *f = find(&im, "ShopInventory"); uint32_t end = 5u + 5u + 2u + 1u + 1u + 5u + 3u * 11u;
        assert(f->body.b[end] == 0x20); set32(f->body.b + end + 1, get32(f->body.b + end + 1) - 11u); }
    refuse(&im, "a tier end jump into the default list");
    rebuild(); { Fn *f = find(&im, "WeaponCheck"); uint32_t skip = 5u + 5u + 5u + 1u + 1u;
        assert(f->body.b[skip] == 0x20); set32(f->body.b + skip + 1, get32(f->body.b + skip + 1) + 1u); }
    refuse(&im, "a WeaponCheck skip jump off the next block");
    rebuild(); { Fn *f = find(&im, "IsStoryBlockAfter|N"); assert(f->body.b[23] == 0x20); f->body.b[24] = 0x0a; }
    refuse(&im, "an IsStoryBlockAfter jump to the wrong instruction");
    rebuild(); { Fn *f = find(&im, "StartShopping"); assert(f->body.b[51] == 0x20); f->body.b[52] ^= 1; }
    refuse(&im, "a broken StartShopping wait loop");

    /* Operand values. */
    minimal(&spec); rebuild(); body_imm(find(&im, "SetupShopSell"), 0, fbits(1.5f)); refuse(&im, "a fractional sell item id");
    rebuild(); body_imm(find(&im, "SetupShopSell"), 0, 0x80000000u); refuse(&im, "a -0.0 item id");
    rebuild(); body_imm(find(&im, "SetupShopSell"), 0, 0x7fc00000u); refuse(&im, "a NaN item id");
    rebuild(); body_imm(find(&im, "SetupShopSell"), 0, fbits(65536.0f)); refuse(&im, "an item id above uint16");
    rebuild(); body_imm(find(&im, "SetupShopSell"), 5, fbits(-1.0f)); refuse(&im, "the -1 default as an explicit sell price");
    rebuild(); body_imm(find(&im, "SetupShopSell"), 5, fbits(16777218.0f)); refuse(&im, "a sell price above 2^24");
    rebuild(); body_imm(find(&im, "ShopInventory"), 0, fbits(32768.0f)); refuse(&im, "a threshold outside int16");
    rebuild(); body_imm(find(&im, "ShopInventory"), 0, fbits(300.5f)); refuse(&im, "a fractional threshold");
    rebuild(); body_imm(find(&im, "StartShopping"), 6, fbits(65536.0f)); refuse(&im, "a shop name id above uint16");
    rebuild(); body_imm(find(&im, "WeaponCheck"), 10, fbits(-0.0f)); refuse(&im, "a WeaponCheck compare against -0.0");
    rebuild(); body_imm(find(&im, "WeaponCheck"), 22, fbits(8.0f)); refuse(&im, "a WeaponCheck adding another item");

    /* Duplicates. */
    minimal(&spec); spec.items[0][2] = 510; rebuild(); refuse(&im, "a duplicate item inside a tier");
    minimal(&spec); spec.weapon[1] = 7; rebuild(); refuse(&im, "a duplicate weapon");
    minimal(&spec); spec.weapon[1] = 511; rebuild(); refuse(&im, "a weapon already in a tier list");
    minimal(&spec); spec.sell_item[2] = 900; rebuild(); refuse(&im, "a duplicate sell item");
}

static void test_tiers(void) {
    minimal(&spec); rebuild(); assert(decode_image(&im));
    const SudekiMpSolShopTier *t0 = &stock.tiers[0], *t1 = &stock.tiers[1], *d = &stock.tiers[2];
    /* IsStoryBlockAfter(n) == (block >= n): equality is inside the tier. */
    assert(SudekiMpSolShopStockTier(&stock, 300) == t0);
    assert(SudekiMpSolShopStockTier(&stock, 299) == t1);
    assert(SudekiMpSolShopStockTier(&stock, 301) == t0);
    assert(SudekiMpSolShopStockTier(&stock, 100) == t1);
    assert(SudekiMpSolShopStockTier(&stock, 99) == d);
    assert(SudekiMpSolShopStockTier(&stock, 101) == t1);
    assert(SudekiMpSolShopStockTier(&stock, 0) == d);
    assert(SudekiMpSolShopStockTier(&stock, -1) == d);
    assert(SudekiMpSolShopStockTier(&stock, INT32_MAX) == t0);
    assert(SudekiMpSolShopStockTier(&stock, INT32_MIN) == d);
    assert(SudekiMpSolShopStockTier(&stock, INT32_MIN + 1) == d);
    printf("SolShopStock tier boundaries: block == threshold selects the tier: PASS\n");

    /* int16 threshold extremes. */
    minimal(&spec); spec.thresholds[0] = 32767; spec.thresholds[1] = -32768; rebuild(); assert(decode_image(&im));
    t0 = &stock.tiers[0]; t1 = &stock.tiers[1]; d = &stock.tiers[2];
    assert(SudekiMpSolShopStockTier(&stock, 32767) == t0);
    assert(SudekiMpSolShopStockTier(&stock, 32766) == t1);
    assert(SudekiMpSolShopStockTier(&stock, 32768) == t0);
    assert(SudekiMpSolShopStockTier(&stock, INT32_MAX) == t0);
    assert(SudekiMpSolShopStockTier(&stock, -32768) == t1);
    assert(SudekiMpSolShopStockTier(&stock, -32767) == t1);
    assert(SudekiMpSolShopStockTier(&stock, -32769) == d);
    assert(SudekiMpSolShopStockTier(&stock, INT32_MIN) == d);
    printf("SolShopStock tier boundaries at the int16 and INT32 extremes: PASS\n");

    SudekiMpSolShopStock empty;
    memset(&empty, 0, sizeof(empty));
    assert(!SudekiMpSolShopStockTier(NULL, 0));
    assert(!SudekiMpSolShopStockTier(&empty, 0));
    empty.tier_count = SUDEKIMP_SOL_SHOP_MAX_TIERS + 1;
    assert(!SudekiMpSolShopStockTier(&empty, 0));
    printf("SolShopStock tier lookup rejects a NULL or empty stock: PASS\n");
}

int main(void) {
    test_success();
    test_refusals();
    test_tiers();
    printf("SolShopStockTest: PASS\n");
    return 0;
}
