#ifndef SUDEKIMP_SOL_SHOP_STOCK_H
#define SUDEKIMP_SOL_SHOP_STOCK_H
/* Kamo's shop stock, decoded at runtime from the player's own verified
 * Data\SOLWORLDM.gex. Nothing game-derived is stored in the repository
 * (owner decision 2026-10-17): the item lists and prices exist only in the
 * caller's memory after a successful decode.
 *
 * Script shape (CONFIRMED_STATIC, supported SOLWORLDM.gex
 * e36a5974f9aedea5b5b428fe2445cf496c52911ff01d4934ea8ab8124abf1ff9):
 *   StartShopping  ShopClearInventory; ShopSetName|N(name_text);
 *                  ShopInventory; SetupShopSell; ShopStartBuy; wait while
 *                  IsShopActive; ShopClearInventory; ShowDialog.
 *   ShopInventory  if IsStoryBlockAfter(T0) {list0} else if ... else
 *                  {default list}; then WeaponCheck. Every entry is
 *                  ShopAddItemBuy|N(item), i.e. the native default price (-1).
 *   WeaponCheck    for each weapon: if GetQuantityOfItemInInventory(item)==0
 *                  then ShopAddItemBuy|N(item).
 *   SetupShopSell  ShopAddItemSell|NN(item, price) pairs.
 *
 * IsStoryBlockAfter|N(n) returns GetStoryBlock() >= n: a block equal to the
 * threshold is INSIDE the tier (CONFIRMED_STATIC: its GEX body returns false
 * only when VM opcode 0x0D, which pushes top > second, reports n > block). The
 * decoder verifies that body exactly, so SudekiMpSolShopStockTier() applies
 * the same >= rule. */
#include <windows.h>
#include <stdint.h>
enum { SUDEKIMP_SOL_SHOP_MAX_TIERS=8, SUDEKIMP_SOL_SHOP_MAX_ITEMS=64,
    SUDEKIMP_SOL_SHOP_MAX_SELL=96, SUDEKIMP_SOL_SHOP_MAX_WEAPONS=8 };
typedef struct SudekiMpSolShopTier {
    int32_t min_block;  /* tier applies when IsStoryBlockAfter(min_block); INT32_MIN for the default list */
    uint16_t count;
    uint16_t items[SUDEKIMP_SOL_SHOP_MAX_ITEMS];
} SudekiMpSolShopTier;
/* price is the explicit ShopAddItemSell|NN price (never the -1 default). */
typedef struct SudekiMpSolShopSell { uint16_t item; int32_t price; } SudekiMpSolShopSell;
typedef struct SudekiMpSolShopStock {
    uint16_t name_text;                 /* ShopSetName|N string id in StartShopping */
    unsigned tier_count;                /* highest threshold first; the last tier is the default (INT32_MIN) */
    SudekiMpSolShopTier tiers[SUDEKIMP_SOL_SHOP_MAX_TIERS];
    unsigned weapon_count;              /* WeaponCheck: added only when the party owns none of that item */
    uint16_t weapons[SUDEKIMP_SOL_SHOP_MAX_WEAPONS];
    unsigned sell_count;
    SudekiMpSolShopSell sell[SUDEKIMP_SOL_SHOP_MAX_SELL];
} SudekiMpSolShopStock;
/* Decode from a buffer holding the whole SOLWORLDM.gex. Pure, bounds-checked;
 * refuses (FALSE, *out zeroed) any shape it does not recognise exactly.
 * Item ids are only range-checked to uint16; callers must still validate them
 * against the native item table before use. */
BOOL SudekiMpSolShopStockDecode(const uint8_t *gex,size_t size,SudekiMpSolShopStock *out);
/* Read Data\SOLWORLDM.gex next to the module `game`, require the supported
 * SHA-256, then decode. */
BOOL SudekiMpSolShopStockLoad(HMODULE game,SudekiMpSolShopStock *out);
/* The tier the vanilla script chooses for story block `block`: the first tier
 * with block >= min_block. NULL for a NULL or empty stock. */
const SudekiMpSolShopTier *SudekiMpSolShopStockTier(const SudekiMpSolShopStock *stock,int block);
#endif
