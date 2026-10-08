#ifndef SUDEKIMP_MOD_ITEMS_H
#define SUDEKIMP_MOD_ITEMS_H
#include <stddef.h>
#include <stdint.h>

/* The game's item definitions (the CItemManager field tree in SOLData.baf)
 * and its UTF-16 text table: which weapon or armour each model and icon
 * belongs to, under the name the player sees. Pure parsing; no file I/O. */

enum {
    SUDEKIMP_MOD_ITEM_TEXT_MAX = 96,  /* UTF-8 item name */
    SUDEKIMP_MOD_ITEM_FILE_MAX = 64   /* resource name, e.g. W005_MRCHOPPY.HOM */
};
/* English text table on the supported build (archive key). */
#define SUDEKIMP_MOD_ITEM_TEXT_KEY 0xFD092E42u

typedef enum SudekiMpModItemKind {
    SUDEKIMP_MOD_ITEM_WEAPON = 1,
    SUDEKIMP_MOD_ITEM_ARMOUR = 2
} SudekiMpModItemKind;

typedef struct SudekiMpModItem {
    unsigned id;                /* "Item NNN" */
    SudekiMpModItemKind kind;
    int hero;                   /* 0 Tal, 1 Ailish, 2 Buki, 3 Elco */
    int shadow;                 /* Dark* item type (the hero's shadow form) */
    char name[SUDEKIMP_MOD_ITEM_TEXT_MAX];       /* "" when the text is unknown */
    char model[SUDEKIMP_MOD_ITEM_FILE_MAX];      /* weapon model, or armour's appearance model */
    char icon[SUDEKIMP_MOD_ITEM_FILE_MAX];       /* menu icon texture */
    char picture[SUDEKIMP_MOD_ITEM_FILE_MAX];    /* weapons: SUI_Wnnn_FULL.SQX; "" otherwise */
} SudekiMpModItem;

typedef struct SudekiMpModItems {
    SudekiMpModItem *items;
    size_t count;
} SudekiMpModItems;

/* True when data is the item manager resource. */
int SudekiMpModItemsIsManager(const void *data, size_t size);
/* Weapons and armour of the four heroes (and their shadow forms). text may be
 * NULL (names stay empty). Output must be zero-initialized; 1 on success. */
int SudekiMpModItemsParse(const void *manager, size_t manager_size,
    const void *text, size_t text_size, SudekiMpModItems *items,
    char *error, size_t error_capacity);
void SudekiMpModItemsFree(SudekiMpModItems *items);
#endif
