#ifndef SUDEKIMP_STORY_LOOT_CODEC_H
#define SUDEKIMP_STORY_LOOT_CODEC_H
#include "engine/story_loot.h"
/* Canonical little-endian encoding: no compiler padding or native pointers.
 * Integrity checksum is corruption detection, NOT network authentication. */
#define SUDEKIMP_STORY_LOOT_HEADER_SIZE 120u
#define SUDEKIMP_STORY_LOOT_ITEM_SIZE 28u
#define SUDEKIMP_STORY_LOOT_SOURCE_SIZE 24u
#define SUDEKIMP_STORY_LOOT_MAX_SIZE (120u+128u*28u+512u*24u)
size_t SudekiMpStoryLootEncodedSize(const SudekiMpStoryLootState *);
int SudekiMpStoryLootEncode(const SudekiMpStoryLootState *,uint8_t *,size_t,size_t *);
/* Failure leaves output untouched. Expected save identity must be locally
 * trusted, never supplied by the same untrusted payload being decoded. */
int SudekiMpStoryLootDecode(const uint8_t *,size_t,const uint8_t expected_save[32],
    SudekiMpStoryLootState *);
#endif
