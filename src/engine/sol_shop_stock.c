/* Kamo shop stock decoder for the supported SOLWORLDM.gex.
 *
 * Evidence (CONFIRMED_STATIC, supported SUDEKI.exe and SOLWORLDM.gex
 * e36a5974f9aedea5b5b428fe2445cf496c52911ff01d4934ea8ab8124abf1ff9):
 *
 * Container. "Version:0004" at 0x00, hash-slot count at 0x0C, function count
 * at 0x10, then count 16-byte entries {flags, hash, logical start, length}
 * at 0x14, an 8-byte index header and slot-count u32 index entries. Bytecode
 * logical offset 0 is the byte after that index (0x27E6C in the supported
 * file); see tools/inspect-sol-gex.pl. Function hashes are FNV-1a-32 of the
 * lower-cased name, signature suffix included ("IsStoryBlockAfter|N").
 *
 * VM (dispatcher VA 0x5C41D0, opcode table VA 0x723F04). The call handler
 * (0x27, VA 0x5C4970) resolves a hash in the script-function table first and
 * only then in the native table, so a script function would shadow a native;
 * the decoder therefore requires every native it relies on to be absent from
 * the function table. Operand sizes used here: 0x06 push imm32 (float bits),
 * 0x07 push u8 bool, 0x08 push var hash, 0x0C store hash, 0x1B declare local
 * hash, 0x20 jmp rel32 from the end of the instruction, 0x27 call hash; the
 * opcodes 0x01 add, 0x05 neg, 0x0A pop, 0x0D (top > second, x87 fcompp),
 * 0x11 (u32 equality), 0x17 logical not, 0x19/0x1A scope push/pop,
 * 0x24 yield, 0x25 sleep, 0x26 (pop bool, skip the next 5 bytes when true),
 * 0x2A ret and 0x2B ret0 take no operand.
 *
 * Story-block boundary. IsStoryBlockAfter|N pushes GetStoryBlock() and then
 * its argument n; opcode 0x0D pops n (top) and the block (second), and its
 * handler (VA 0x5C4F20: fld second; fld top; fcompp; test ah,0x41) pushes
 * true only for n > block. The body returns false on that true result and
 * true otherwise, so IsStoryBlockAfter(n) == (block >= n). GetStoryBlock
 * (?GetStoryBlock@@YAFXZ, VA 0x4FDA00) returns the int16 at VA 0x7C2FEC and
 * the native return marshaller widens a short with movsx + fild, which is
 * exact; every accepted threshold is a float-exact int16, so the integer
 * comparison in SudekiMpSolShopStockTier() is identical to the script's.
 *
 * Every function the decoded meaning depends on is matched exactly: the four
 * Kamo functions by grammar (variable list lengths, exact instruction stream,
 * exact jump targets, stream ends exactly at the table length), and the
 * helpers IsStoryBlockAfter|N, ShopAddItemBuy|N, ShopAddItemBuy|NN,
 * ShopAddItemSell|NN and ShopAddItemSleeper|NN by fixed templates. The
 * function-table flags pinned below are the observed values; their meaning is
 * UNKNOWN, so any other value is refused. No game bytes are stored here: the
 * templates are spelled from opcodes and name hashes. */
#include "engine/sol_shop_stock.h"
#include "engine/sha256.h"

#include <string.h>
#include <wchar.h>

/* FNV-1a-32 of the lower-cased script names. */
#define H_START_SHOPPING      0x2f711747u /* StartShopping */
#define H_SHOP_INVENTORY      0xbf42f8ebu /* ShopInventory */
#define H_WEAPON_CHECK        0x8a0e26e3u /* WeaponCheck */
#define H_SETUP_SHOP_SELL     0x4c2e3a3au /* SetupShopSell */
#define H_IS_STORY_AFTER      0x20641865u /* IsStoryBlockAfter|N */
#define H_BUY_N               0x9191e707u /* ShopAddItemBuy|N */
#define H_BUY_NN              0x91af4a4bu /* ShopAddItemBuy|NN */
#define H_SELL_NN             0x33a5a6c5u /* ShopAddItemSell|NN */
#define H_SLEEPER_NN          0x54330ca3u /* ShopAddItemSleeper|NN */
#define H_SHOP_START_BUY      0x3b3e9af9u /* ShopStartBuy (script) */
#define H_SHOW_DIALOG         0x14f592bcu /* ShowDialog (script) */
/* Natives: must not be shadowed by a script function. */
#define H_GET_STORY_BLOCK     0x16e8157du /* GetStoryBlock */
#define H_SHOP_SET_NAME       0x002a8caau /* ShopSetName|N */
#define H_GET_QUANTITY        0xcbae465bu /* GetQuantityOfItemInInventory|N */
#define H_SHOP_SET_MODE       0x1703732au /* ShopSetMode|B */
#define H_SHOP_ADD_ITEM_NN    0xf6e92681u /* ShopAddItem|NN */
#define H_SHOP_CLEAR          0x8ed4fc6au /* ShopClearInventory */
#define H_IS_SHOP_ACTIVE      0x6625583bu /* IsShopActive */
/* Locals and globals. */
#define H_N_STORY_BLOCK       0x124a37b3u /* nStoryBlock */
#define H_ITEM_ID             0x13bddc7bu /* itemId */
#define H_PRICE               0x0b44452au /* price */
#define H_SHOP_SLEEPER        0xb839f51du /* g_shopSleeper */

#define FLAGS_PLAIN           0x00170005u /* observed: StartShopping etc. */
#define FLAGS_ARGS            0x00170003u /* observed: the |N / |NN helpers */

#define F32_ZERO              0x00000000u
#define F32_ONE               0x3f800000u
#define F32_FIFTEEN           0x41700000u

enum {
    OP_ADD=0x01, OP_NEG=0x05, OP_PUSH_IMM=0x06, OP_PUSH_BOOL=0x07,
    OP_PUSH_VAR=0x08, OP_POP=0x0a, OP_STORE=0x0c, OP_GREATER=0x0d,
    OP_EQUAL=0x11, OP_NOT=0x17, OP_SCOPE_PUSH=0x19, OP_SCOPE_POP=0x1a,
    OP_DECLARE=0x1b, OP_JMP=0x20, OP_YIELD=0x24, OP_SLEEP=0x25,
    OP_SKIP_IF=0x26, OP_CALL=0x27, OP_RET=0x2a, OP_RET0=0x2b
};

enum {
    HEADER_SIZE=0x14, ENTRY_SIZE=16, INDEX_HEADER_SIZE=8,
    MAX_HASH_SLOTS=0x100000, MAX_FILE_SIZE=16*1024*1024,
    STORY_BLOCK_MIN=-32768, STORY_BLOCK_MAX=32767,
    ITEM_MAX=0xffff, NAME_MAX=0xffff, PRICE_MAX=1<<24
};

static const char supported_sha256[] =
    "e36a5974f9aedea5b5b428fe2445cf496c52911ff01d4934ea8ab8124abf1ff9";

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* ---- function table ---------------------------------------------------- */

typedef struct Gex {
    const uint8_t *data;
    size_t size;
    uint32_t count;
    size_t code_base;
} Gex;

static BOOL gex_open(Gex *g, const uint8_t *data, size_t size) {
    if (!data || size < HEADER_SIZE || memcmp(data, "Version:0004", 12)) return FALSE;
    uint32_t slots = rd32(data + 0x0c), count = rd32(data + 0x10);
    /* The runtime places every function in a power-of-two linear-probe table
     * of `slots` entries (slot = hash & (slots - 1)). */
    if (!slots || slots > MAX_HASH_SLOTS || (slots & (slots - 1u)) || !count || count > slots)
        return FALSE;
    size_t code_base = HEADER_SIZE + (size_t)count * ENTRY_SIZE + INDEX_HEADER_SIZE + (size_t)slots * 4u;
    if (code_base > size) return FALSE;
    g->data = data; g->size = size; g->count = count; g->code_base = code_base;
    return TRUE;
}

static unsigned gex_matches(const Gex *g, uint32_t hash, const uint8_t **entry) {
    unsigned matches = 0;
    for (uint32_t i = 0; i < g->count; ++i) {
        const uint8_t *e = g->data + HEADER_SIZE + (size_t)i * ENTRY_SIZE;
        if (rd32(e + 4) != hash) continue;
        if (!matches && entry) *entry = e;
        ++matches;
    }
    return matches;
}

static BOOL gex_function(const Gex *g, uint32_t hash, uint32_t flags,
    const uint8_t **body, uint32_t *length) {
    const uint8_t *e = NULL;
    if (gex_matches(g, hash, &e) != 1 || rd32(e) != flags) return FALSE;
    uint32_t start = rd32(e + 8), raw = rd32(e + 12);
    /* The length field's top bit (set on 32 retail functions) has UNKNOWN
     * meaning; none of the functions read here carries it. */
    if (!raw || (raw & 0x80000000u)) return FALSE;
    size_t code_size = g->size - g->code_base;
    if (start > code_size || raw > code_size - start) return FALSE;
    *body = g->data + g->code_base + start;
    *length = raw;
    return TRUE;
}

/* ---- instruction cursor ------------------------------------------------ */

typedef struct Cursor {
    const uint8_t *code;
    uint32_t length;
    uint32_t at;
    BOOL ok;
} Cursor;

static int peek(const Cursor *c) {
    return c->ok && c->at < c->length ? c->code[c->at] : -1;
}

static void op(Cursor *c, uint8_t opcode) {
    if (!c->ok) return;
    if (c->at >= c->length || c->code[c->at] != opcode) { c->ok = FALSE; return; }
    ++c->at;
}

static uint32_t operand(Cursor *c, uint8_t opcode) {
    if (!c->ok) return 0;
    if (c->length - c->at < 5 || c->code[c->at] != opcode) { c->ok = FALSE; return 0; }
    uint32_t value = rd32(c->code + c->at + 1);
    c->at += 5;
    return value;
}

static void op_value(Cursor *c, uint8_t opcode, uint32_t expected) {
    uint32_t value = operand(c, opcode);
    if (c->ok && value != expected) c->ok = FALSE;
}

static void op_bool(Cursor *c, uint8_t expected) {
    if (!c->ok) return;
    if (c->length - c->at < 2 || c->code[c->at] != OP_PUSH_BOOL || c->code[c->at + 1] != expected) {
        c->ok = FALSE; return;
    }
    c->at += 2;
}

/* Logical target of a 0x20 jmp, relative to the function start. */
static uint32_t jmp_target(Cursor *c) {
    uint32_t raw = operand(c, OP_JMP);
    if (!c->ok) return 0;
    int64_t target = (int64_t)c->at + (int32_t)raw;
    if (target < 0 || target >= (int64_t)c->length) { c->ok = FALSE; return 0; }
    return (uint32_t)target;
}

/* An imm32 holding the canonical float encoding of an integer in [lo, hi]
 * (|lo|, |hi| <= 2^24, so every such integer is float-exact). Refuses
 * fractions, NaN, infinities and -0.0. */
static BOOL imm_int(Cursor *c, int32_t lo, int32_t hi, int32_t *out) {
    uint32_t bits = operand(c, OP_PUSH_IMM);
    if (!c->ok) return FALSE;
    float value;
    memcpy(&value, &bits, sizeof(value));
    if (!(value >= (float)lo && value <= (float)hi)) { c->ok = FALSE; return FALSE; }
    int32_t whole = (int32_t)value;
    float back = (float)whole;
    uint32_t back_bits;
    memcpy(&back_bits, &back, sizeof(back_bits));
    if (back_bits != bits) { c->ok = FALSE; return FALSE; }
    *out = whole;
    return TRUE;
}

/* push imm32; call `hash` */
static BOOL at_imm_call(const Cursor *c, uint32_t hash) {
    return c->ok && c->length >= 10 && c->at <= c->length - 10 &&
        c->code[c->at] == OP_PUSH_IMM && c->code[c->at + 5] == OP_CALL &&
        rd32(c->code + c->at + 6) == hash;
}

/* ---- fixed templates --------------------------------------------------- */

enum { T_OP, T_VALUE, T_BOOL, T_CAPTURE, T_JMP };
typedef struct Step { uint8_t kind, opcode, target; uint32_t value; } Step;
#define S_OP(o)       {T_OP, (o), 0, 0}
#define S_VAL(o, v)   {T_VALUE, (o), 0, (v)}
#define S_BOOL(v)     {T_BOOL, OP_PUSH_BOOL, 0, (v)}
#define S_CAPTURE()   {T_CAPTURE, OP_PUSH_IMM, 0, 0}
#define S_JMP(step)   {T_JMP, OP_JMP, (step), 0}
#define S_COUNT(a)    ((unsigned)(sizeof(a) / sizeof((a)[0])))

/* if (GetStoryBlock() < n) return false; else return true; */
static const Step is_story_block_after[] = {
    S_OP(OP_SCOPE_PUSH), S_VAL(OP_DECLARE, H_N_STORY_BLOCK), S_VAL(OP_STORE, H_N_STORY_BLOCK),
    S_VAL(OP_CALL, H_GET_STORY_BLOCK), S_VAL(OP_PUSH_VAR, H_N_STORY_BLOCK), S_OP(OP_GREATER),
    S_OP(OP_SKIP_IF), S_JMP(11), S_BOOL(0), S_OP(OP_RET), S_JMP(13),
    /* 11 */ S_BOOL(1), S_OP(OP_RET), /* 13 */ S_OP(OP_SCOPE_POP), S_OP(OP_RET0)
};
/* return ShopAddItemBuy(itemId, -1); */
static const Step shop_add_item_buy_n[] = {
    S_OP(OP_SCOPE_PUSH), S_VAL(OP_DECLARE, H_ITEM_ID), S_VAL(OP_STORE, H_ITEM_ID),
    S_VAL(OP_PUSH_VAR, H_ITEM_ID), S_VAL(OP_PUSH_IMM, F32_ONE), S_OP(OP_NEG),
    S_VAL(OP_CALL, H_BUY_NN), S_OP(OP_RET), S_OP(OP_SCOPE_POP), S_OP(OP_RET0)
};
/* ShopSetMode(buy); return ShopAddItemSleeper(itemId, price); */
#define MODE_TEMPLATE(mode) { \
    S_OP(OP_SCOPE_PUSH), S_VAL(OP_DECLARE, H_PRICE), S_VAL(OP_STORE, H_PRICE), \
    S_VAL(OP_DECLARE, H_ITEM_ID), S_VAL(OP_STORE, H_ITEM_ID), S_BOOL(mode), \
    S_VAL(OP_CALL, H_SHOP_SET_MODE), S_OP(OP_POP), S_VAL(OP_PUSH_VAR, H_ITEM_ID), \
    S_VAL(OP_PUSH_VAR, H_PRICE), S_VAL(OP_CALL, H_SLEEPER_NN), S_OP(OP_RET), \
    S_OP(OP_SCOPE_POP), S_OP(OP_RET0) }
static const Step shop_add_item_buy_nn[] = MODE_TEMPLATE(1);
static const Step shop_add_item_sell_nn[] = MODE_TEMPLATE(0);
/* if (++g_shopSleeper == 15) { script wait 0 (opcode 0x25); g_shopSleeper = 0; }
 * return ShopAddItem(itemId, price); */
static const Step shop_add_item_sleeper_nn[] = {
    S_OP(OP_SCOPE_PUSH), S_VAL(OP_DECLARE, H_PRICE), S_VAL(OP_STORE, H_PRICE),
    S_VAL(OP_DECLARE, H_ITEM_ID), S_VAL(OP_STORE, H_ITEM_ID),
    S_VAL(OP_PUSH_VAR, H_SHOP_SLEEPER), S_VAL(OP_PUSH_IMM, F32_ONE), S_OP(OP_ADD),
    S_VAL(OP_STORE, H_SHOP_SLEEPER), S_VAL(OP_PUSH_VAR, H_SHOP_SLEEPER),
    S_VAL(OP_PUSH_IMM, F32_FIFTEEN), S_OP(OP_EQUAL), S_OP(OP_SKIP_IF), S_JMP(18),
    S_VAL(OP_PUSH_IMM, F32_ZERO), S_OP(OP_SLEEP), S_VAL(OP_PUSH_IMM, F32_ZERO),
    S_VAL(OP_STORE, H_SHOP_SLEEPER),
    /* 18 */ S_VAL(OP_PUSH_VAR, H_ITEM_ID), S_VAL(OP_PUSH_VAR, H_PRICE),
    S_VAL(OP_CALL, H_SHOP_ADD_ITEM_NN), S_OP(OP_RET), S_OP(OP_SCOPE_POP), S_OP(OP_RET0)
};
/* The capture is the ShopSetName|N string id. */
static const Step start_shopping[] = {
    S_VAL(OP_CALL, H_SHOP_CLEAR), S_OP(OP_POP),
    S_CAPTURE(), S_VAL(OP_CALL, H_SHOP_SET_NAME), S_OP(OP_POP),
    S_VAL(OP_CALL, H_SHOP_INVENTORY), S_OP(OP_POP),
    S_VAL(OP_CALL, H_SETUP_SHOP_SELL), S_OP(OP_POP),
    S_VAL(OP_CALL, H_SHOP_START_BUY), S_OP(OP_POP),
    /* 11 */ S_VAL(OP_CALL, H_IS_SHOP_ACTIVE), S_BOOL(0), S_OP(OP_EQUAL), S_OP(OP_NOT),
    S_OP(OP_SKIP_IF), S_JMP(19), S_OP(OP_YIELD), S_JMP(11),
    /* 19 */ S_VAL(OP_CALL, H_SHOP_CLEAR), S_OP(OP_POP),
    S_VAL(OP_CALL, H_SHOW_DIALOG), S_OP(OP_POP), S_OP(OP_RET0)
};

enum { MAX_STEPS=32 };

static BOOL match_template(const uint8_t *code, uint32_t length, const Step *steps,
    unsigned count, int32_t capture_lo, int32_t capture_hi, int32_t *capture) {
    uint32_t start[MAX_STEPS], target[MAX_STEPS];
    Cursor c = {code, length, 0, TRUE};
    if (count > MAX_STEPS) return FALSE;
    for (unsigned i = 0; i < count; ++i) {
        start[i] = c.at; target[i] = 0;
        switch (steps[i].kind) {
        case T_OP: op(&c, steps[i].opcode); break;
        case T_VALUE: op_value(&c, steps[i].opcode, steps[i].value); break;
        case T_BOOL: op_bool(&c, (uint8_t)steps[i].value); break;
        case T_CAPTURE:
            if (!capture || !imm_int(&c, capture_lo, capture_hi, capture)) c.ok = FALSE;
            break;
        case T_JMP: target[i] = jmp_target(&c); break;
        default: c.ok = FALSE; break;
        }
        if (!c.ok) return FALSE;
    }
    if (c.at != length) return FALSE;
    for (unsigned i = 0; i < count; ++i)
        if (steps[i].kind == T_JMP && (steps[i].target >= count || target[i] != start[steps[i].target]))
            return FALSE;
    return TRUE;
}

static BOOL template_function(const Gex *g, uint32_t hash, uint32_t flags, const Step *steps,
    unsigned count, int32_t capture_lo, int32_t capture_hi, int32_t *capture) {
    const uint8_t *body; uint32_t length;
    return gex_function(g, hash, flags, &body, &length) &&
        match_template(body, length, steps, count, capture_lo, capture_hi, capture);
}

/* ---- variable-length Kamo functions ------------------------------------ */

static BOOL contains(const uint16_t *items, unsigned count, uint16_t item) {
    for (unsigned i = 0; i < count; ++i) if (items[i] == item) return TRUE;
    return FALSE;
}

/* { push item; call ShopAddItemBuy|N; pop }+ ; distinct items. */
static BOOL buy_list(Cursor *c, SudekiMpSolShopTier *tier) {
    while (at_imm_call(c, H_BUY_N)) {
        int32_t item;
        if (tier->count >= SUDEKIMP_SOL_SHOP_MAX_ITEMS || !imm_int(c, 0, ITEM_MAX, &item)) return FALSE;
        op_value(c, OP_CALL, H_BUY_N); op(c, OP_POP);
        if (!c->ok || contains(tier->items, tier->count, (uint16_t)item)) return FALSE;
        tier->items[tier->count++] = (uint16_t)item;
    }
    return c->ok && tier->count > 0;
}

/* { push T; call IsStoryBlockAfter|N; push true; eq; skip_if; jmp next;
 *   list; jmp end; next: }*  default list;  end: call WeaponCheck; pop; ret0 */
static BOOL shop_inventory(const uint8_t *code, uint32_t length, SudekiMpSolShopStock *s) {
    Cursor c = {code, length, 0, TRUE};
    uint32_t ends[SUDEKIMP_SOL_SHOP_MAX_TIERS];
    while (at_imm_call(&c, H_IS_STORY_AFTER)) {
        if (s->tier_count >= SUDEKIMP_SOL_SHOP_MAX_TIERS - 1u) return FALSE;
        SudekiMpSolShopTier *tier = &s->tiers[s->tier_count];
        int32_t threshold;
        if (!imm_int(&c, STORY_BLOCK_MIN, STORY_BLOCK_MAX, &threshold)) return FALSE;
        if (s->tier_count && threshold >= s->tiers[s->tier_count - 1u].min_block) return FALSE;
        op_value(&c, OP_CALL, H_IS_STORY_AFTER); op_bool(&c, 1); op(&c, OP_EQUAL); op(&c, OP_SKIP_IF);
        uint32_t next = jmp_target(&c);
        if (!buy_list(&c, tier)) return FALSE;
        ends[s->tier_count] = jmp_target(&c);
        if (!c.ok || next != c.at) return FALSE;
        tier->min_block = threshold;
        ++s->tier_count;
    }
    SudekiMpSolShopTier *fallback = &s->tiers[s->tier_count];
    fallback->min_block = INT32_MIN;
    if (!buy_list(&c, fallback)) return FALSE;
    uint32_t end = c.at;
    op_value(&c, OP_CALL, H_WEAPON_CHECK); op(&c, OP_POP); op(&c, OP_RET0);
    if (!c.ok || c.at != length) return FALSE;
    for (unsigned i = 0; i < s->tier_count; ++i) if (ends[i] != end) return FALSE;
    ++s->tier_count;
    return TRUE;
}

/* { push w; call GetQuantityOfItemInInventory|N; push 0.0; eq; skip_if;
 *   jmp next; push w; call ShopAddItemBuy|N; pop; next: }+ ret0 */
static BOOL weapon_check(const uint8_t *code, uint32_t length, SudekiMpSolShopStock *s) {
    Cursor c = {code, length, 0, TRUE};
    while (peek(&c) == OP_PUSH_IMM) {
        int32_t item, again;
        if (s->weapon_count >= SUDEKIMP_SOL_SHOP_MAX_WEAPONS || !imm_int(&c, 0, ITEM_MAX, &item)) return FALSE;
        op_value(&c, OP_CALL, H_GET_QUANTITY); op_value(&c, OP_PUSH_IMM, F32_ZERO);
        op(&c, OP_EQUAL); op(&c, OP_SKIP_IF);
        uint32_t next = jmp_target(&c);
        if (!imm_int(&c, 0, ITEM_MAX, &again) || again != item) return FALSE;
        op_value(&c, OP_CALL, H_BUY_N); op(&c, OP_POP);
        if (!c.ok || next != c.at || contains(s->weapons, s->weapon_count, (uint16_t)item)) return FALSE;
        s->weapons[s->weapon_count++] = (uint16_t)item;
    }
    op(&c, OP_RET0);
    return c.ok && c.at == length && s->weapon_count > 0;
}

/* { push item; push price; call ShopAddItemSell|NN; pop }+ ret0 */
static BOOL setup_shop_sell(const uint8_t *code, uint32_t length, SudekiMpSolShopStock *s) {
    Cursor c = {code, length, 0, TRUE};
    while (peek(&c) == OP_PUSH_IMM) {
        int32_t item, price;
        if (s->sell_count >= SUDEKIMP_SOL_SHOP_MAX_SELL || !imm_int(&c, 0, ITEM_MAX, &item) ||
            !imm_int(&c, 0, PRICE_MAX, &price)) return FALSE;
        op_value(&c, OP_CALL, H_SELL_NN); op(&c, OP_POP);
        if (!c.ok) return FALSE;
        for (unsigned i = 0; i < s->sell_count; ++i) if (s->sell[i].item == (uint16_t)item) return FALSE;
        s->sell[s->sell_count].item = (uint16_t)item;
        s->sell[s->sell_count].price = price;
        ++s->sell_count;
    }
    op(&c, OP_RET0);
    return c.ok && c.at == length && s->sell_count > 0;
}

static BOOL decode(const uint8_t *gex, size_t size, SudekiMpSolShopStock *s) {
    static const uint32_t natives[] = {
        H_GET_STORY_BLOCK, H_SHOP_SET_NAME, H_GET_QUANTITY, H_SHOP_SET_MODE,
        H_SHOP_ADD_ITEM_NN, H_SHOP_CLEAR, H_IS_SHOP_ACTIVE
    };
    Gex g;
    const uint8_t *body; uint32_t length;
    int32_t name;
    if (!gex_open(&g, gex, size)) return FALSE;
    for (unsigned i = 0; i < S_COUNT(natives); ++i)
        if (gex_matches(&g, natives[i], NULL)) return FALSE;
    if (!template_function(&g, H_IS_STORY_AFTER, FLAGS_ARGS, is_story_block_after,
            S_COUNT(is_story_block_after), 0, 0, NULL) ||
        !template_function(&g, H_BUY_N, FLAGS_ARGS, shop_add_item_buy_n,
            S_COUNT(shop_add_item_buy_n), 0, 0, NULL) ||
        !template_function(&g, H_BUY_NN, FLAGS_ARGS, shop_add_item_buy_nn,
            S_COUNT(shop_add_item_buy_nn), 0, 0, NULL) ||
        !template_function(&g, H_SELL_NN, FLAGS_ARGS, shop_add_item_sell_nn,
            S_COUNT(shop_add_item_sell_nn), 0, 0, NULL) ||
        !template_function(&g, H_SLEEPER_NN, FLAGS_ARGS, shop_add_item_sleeper_nn,
            S_COUNT(shop_add_item_sleeper_nn), 0, 0, NULL) ||
        !template_function(&g, H_START_SHOPPING, FLAGS_PLAIN, start_shopping,
            S_COUNT(start_shopping), 0, NAME_MAX, &name)) return FALSE;
    s->name_text = (uint16_t)name;
    if (!gex_function(&g, H_SHOP_INVENTORY, FLAGS_PLAIN, &body, &length) ||
        !shop_inventory(body, length, s)) return FALSE;
    if (!gex_function(&g, H_WEAPON_CHECK, FLAGS_PLAIN, &body, &length) ||
        !weapon_check(body, length, s)) return FALSE;
    /* WeaponCheck runs after every tier list; an overlap would add an item
     * twice, a native behaviour this decoder does not model. */
    for (unsigned t = 0; t < s->tier_count; ++t)
        for (unsigned w = 0; w < s->weapon_count; ++w)
            if (contains(s->tiers[t].items, s->tiers[t].count, s->weapons[w])) return FALSE;
    if (!gex_function(&g, H_SETUP_SHOP_SELL, FLAGS_PLAIN, &body, &length) ||
        !setup_shop_sell(body, length, s)) return FALSE;
    return TRUE;
}

BOOL SudekiMpSolShopStockDecode(const uint8_t *gex, size_t size, SudekiMpSolShopStock *out) {
    if (!out) return FALSE;
    memset(out, 0, sizeof(*out));
    if (decode(gex, size, out)) return TRUE;
    memset(out, 0, sizeof(*out));
    return FALSE;
}

/* Read the whole file through one handle that denies write and delete
 * sharing, and hash it by path while that handle is still open: no writer can
 * change the file between the read and the SHA-256 check. */
static BOOL load_path(const wchar_t *path, SudekiMpSolShopStock *out) {
    BOOL okay = FALSE;
    uint8_t *data = NULL;
    HANDLE file;
    LARGE_INTEGER size;
    char hash[65];
    if (!out) return FALSE;
    memset(out, 0, sizeof(*out));
    if (!path) return FALSE;
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    if (!GetFileSizeEx(file, &size) || size.QuadPart < HEADER_SIZE || size.QuadPart > MAX_FILE_SIZE)
        goto done;
    data = HeapAlloc(GetProcessHeap(), 0, (SIZE_T)size.QuadPart);
    if (!data) goto done;
    for (DWORD got = 0; got < (DWORD)size.QuadPart;) {
        DWORD chunk = 0;
        if (!ReadFile(file, data + got, (DWORD)size.QuadPart - got, &chunk, NULL) || !chunk) goto done;
        got += chunk;
    }
    {
        uint8_t probe; DWORD extra = 1;
        if (!ReadFile(file, &probe, 1, &extra, NULL) || extra) goto done;
    }
    if (!SudekiMpSha256File(path, hash) || strcmp(hash, supported_sha256)) {
        SetLastError(ERROR_INVALID_DATA); goto done;
    }
    okay = SudekiMpSolShopStockDecode(data, (size_t)size.QuadPart, out);
    if (!okay) SetLastError(ERROR_INVALID_DATA);
done:
    if (data) HeapFree(GetProcessHeap(), 0, data);
    CloseHandle(file);
    if (!okay) memset(out, 0, sizeof(*out));
    return okay;
}

BOOL SudekiMpSolShopStockLoad(HMODULE game, SudekiMpSolShopStock *out) {
    static const wchar_t suffix[] = L"Data\\SOLWORLDM.gex";
    wchar_t path[MAX_PATH];
    if (!out) return FALSE;
    memset(out, 0, sizeof(*out));
    if (!game) return FALSE;
    DWORD n = GetModuleFileNameW(game, path, MAX_PATH);
    if (!n || n >= MAX_PATH) return FALSE;
    wchar_t *tail = wcsrchr(path, L'\\');
    if (!tail || (size_t)(tail + 1 - path) + sizeof(suffix) / sizeof(suffix[0]) > MAX_PATH) return FALSE;
    memcpy(tail + 1, suffix, sizeof(suffix));
    return load_path(path, out);
}

const SudekiMpSolShopTier *SudekiMpSolShopStockTier(const SudekiMpSolShopStock *stock, int block) {
    if (!stock || !stock->tier_count || stock->tier_count > SUDEKIMP_SOL_SHOP_MAX_TIERS) return NULL;
    for (unsigned i = 0; i < stock->tier_count; ++i)
        if ((int32_t)block >= stock->tiers[i].min_block) return &stock->tiers[i];
    return NULL;
}
