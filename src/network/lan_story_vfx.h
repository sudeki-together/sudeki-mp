#ifndef SUDEKIMP_LAN_STORY_VFX_H
#define SUDEKIMP_LAN_STORY_VFX_H
#include "network/lan_arena_protocol.h"
#include "network/lan_story_area_state.h"
/* Additive saved-story cosmetic message. Existing packet layouts/profile stay
 * unchanged; older peers ignore this message. No script or resource string. */
#define SUDEKIMP_STORY_VFX_CAPACITY 64u
#define SUDEKIMP_STORY_VFX_WIRE_MAX (32u+SUDEKIMP_STORY_AREA_NAME+57u*SUDEKIMP_STORY_VFX_CAPACITY)
#define SUDEKIMP_STORY_VFX_CHUNK_BYTES 1152u
#define SUDEKIMP_STORY_VFX_CHUNK_HEADER 32u
#define SUDEKIMP_STORY_VFX_CHUNK_MAX (SUDEKIMP_STORY_VFX_CHUNK_HEADER+SUDEKIMP_STORY_VFX_CHUNK_BYTES)
#define SUDEKIMP_STORY_VFX_CHUNKS ((SUDEKIMP_STORY_VFX_WIRE_MAX+SUDEKIMP_STORY_VFX_CHUNK_BYTES-1u)/SUDEKIMP_STORY_VFX_CHUNK_BYTES)
/* Frozen v1 format. Large complete rosters use a separate additive message. */
#define SUDEKIMP_STORY_VFX_SIZE (32u+SUDEKIMP_STORY_AREA_NAME+57u*SUDEKIMP_LAN_ARENA_SPIRIT_VFX_CAPACITY)
typedef struct SudekiMpStoryVfx {
    uint32_t epoch,revision,tick,sequence;
    uint64_t stream;
    uint16_t cast_sequence;
    uint8_t observed,count;
    char current[SUDEKIMP_STORY_AREA_NAME];
    SudekiMpLanArenaSpiritVfxSnapshot entries[SUDEKIMP_STORY_VFX_CAPACITY];
} SudekiMpStoryVfx;
BOOL SudekiMpStoryVfxValid(const SudekiMpStoryVfx *s);
BOOL SudekiMpStoryVfxEncode(const SudekiMpStoryVfx *s,uint8_t *bytes,size_t size);
BOOL SudekiMpStoryVfxDecode(const uint8_t *bytes,size_t size,SudekiMpStoryVfx *s);
typedef struct SudekiMpStoryVfxChunk {
    uint32_t epoch,revision,tick,sequence,digest,total,index;
    uint8_t bytes[SUDEKIMP_STORY_VFX_CHUNK_BYTES];
} SudekiMpStoryVfxChunk;
typedef struct SudekiMpStoryVfxAssembly {
    uint32_t sequence,mask,received_at;
    SudekiMpStoryVfxChunk chunks[SUDEKIMP_STORY_VFX_CHUNKS];
} SudekiMpStoryVfxAssembly;
unsigned SudekiMpStoryVfxChunkCount(const SudekiMpStoryVfx *state);
BOOL SudekiMpStoryVfxChunkEncode(const SudekiMpStoryVfx *state,unsigned index,
    uint8_t *bytes,size_t capacity,size_t *written);
BOOL SudekiMpStoryVfxChunkDecode(const uint8_t *bytes,size_t size,SudekiMpStoryVfxChunk *chunk);
BOOL SudekiMpStoryVfxAssemble(const SudekiMpStoryVfxChunk *chunks,unsigned count,SudekiMpStoryVfx *state);
#endif
