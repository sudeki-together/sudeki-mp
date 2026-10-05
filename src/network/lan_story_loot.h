#ifndef SUDEKIMP_LAN_STORY_LOOT_H
#define SUDEKIMP_LAN_STORY_LOOT_H
#include "engine/story_loot_codec.h"
#include "network/lan_party_story.h"

/* Absolute, host-owned snapshots, never reward deltas. Connection authority
 * belongs to the session envelope. This codec alone authenticates nothing. */
#define SUDEKIMP_STORY_LOOT_CHUNK_DATA 960u
#define SUDEKIMP_STORY_LOOT_CHUNK_HEADER 40u
#define SUDEKIMP_STORY_LOOT_CHUNK_MAX_SIZE 1000u
#define SUDEKIMP_STORY_LOOT_MAX_CHUNKS ((SUDEKIMP_STORY_LOOT_MAX_SIZE+959u)/960u)
#define SUDEKIMP_STORY_LOOT_ASSEMBLY_AGE_MS 1000u
typedef struct SudekiMpLanStoryLootChunk {
    uint32_t epoch,scene_revision,host_tick,total_size,digest,index,count;
    uint64_t revision;
    uint32_t size;
    uint8_t bytes[SUDEKIMP_STORY_LOOT_CHUNK_DATA];
} SudekiMpLanStoryLootChunk;
typedef struct SudekiMpLanStoryLootAssembly {
    uint32_t started_at,mask;
    SudekiMpLanStoryLootChunk key;
    uint8_t bytes[SUDEKIMP_STORY_LOOT_MAX_SIZE];
} SudekiMpLanStoryLootAssembly;
unsigned SudekiMpLanStoryLootChunkCount(const SudekiMpStoryLootState *);
int SudekiMpLanStoryLootEncode(const SudekiMpStoryLootState *,const SudekiMpLanStoryScene *,
    unsigned index,uint8_t *,size_t,size_t *written);
int SudekiMpLanStoryLootDecode(const uint8_t *,size_t,SudekiMpLanStoryLootChunk *);
/* 0 rejected, 1 buffered/same fragment, 2 COMPLETE. Rejected/missing batches
 * leave output untouched. Repeated fragments never renew the initial age.
 * A newer revision may replace an incomplete older one; older fragments
 * cannot displace it. Reset explicitly at a proved connection/scene fence. */
int SudekiMpLanStoryLootAccept(SudekiMpLanStoryLootAssembly *,const SudekiMpLanStoryLootChunk *,
    uint32_t now,const uint8_t expected_save[32],SudekiMpStoryLootState *out);
#endif
