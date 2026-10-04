#ifndef SUDEKIMP_LAN_STORY_WORLD_FRAME_H
#define SUDEKIMP_LAN_STORY_WORLD_FRAME_H

#include "network/lan_story_frame.h"

/* A separate bounded presentation stream. These records grant no authority
 * to create objects, execute scripts, select assets or apply damage. Native
 * playback must resolve each identity to an already loaded exact NPC or
 * canonical party owner, or an already loaded generic scenery entity. */
#define SUDEKIMP_LAN_STORY_WORLD_VERSION 5u
#define SUDEKIMP_LAN_STORY_WORLD_MAX_ACTORS 64u
#define SUDEKIMP_LAN_STORY_WORLD_CHUNK_ACTORS 8u
#define SUDEKIMP_LAN_STORY_WORLD_MAX_CHUNKS 8u
#define SUDEKIMP_LAN_STORY_WORLD_HEADER_SIZE 28u
#define SUDEKIMP_LAN_STORY_WORLD_ACTOR_SIZE 140u
#define SUDEKIMP_LAN_STORY_WORLD_CHUNK_MAX_SIZE (28u + 8u * 140u)
/* The native bank has at most 4096 entries including the empty selector. */
#define SUDEKIMP_LAN_STORY_WORLD_MAX_CLIP_OCCURRENCES 4095u
#define SUDEKIMP_LAN_STORY_WORLD_NPC_KIND 0x0f9bu
#define SUDEKIMP_LAN_STORY_WORLD_SCENERY_KIND 0x0f8fu
#define SUDEKIMP_LAN_STORY_WORLD_HIDDEN 1u
/* Portable record category, NOT ResourceName encoded_kind: the native low
 * seven bits may change during lazy resolution. Canonical ID is independent. */
#define SUDEKIMP_LAN_STORY_WORLD_PC_KIND 0x0f81u
uint32_t SudekiMpLanStoryWorldCharacterIdentifier(unsigned character);
unsigned SudekiMpLanStoryWorldCharacter(uint32_t identifier);

typedef struct SudekiMpLanStoryWorldActor {
    uint16_t kind;
    uint8_t submodels;
    /* Generic scenery only; never carries native renderer flag words. */
    uint8_t visual_flags;
    uint32_t identifier, generation, animation_sequence;
    /* Deterministic compatibility fingerprint of the loaded bank topology.
     * It is not authentication or permission to load a remote asset. */
    uint64_t bank_fingerprint;
    float position[3], forward[3];
    uint32_t clip[5];
    /* Zero-based occurrence of this handle among nonempty ordered bank
     * entries. A repeated handle need not denote the same authored clip.
     * Resolve the pair only under the exact loaded-bank fingerprint. */
    uint16_t clip_occurrence[5];
    uint8_t state[5];
    float rate[5], time[5], blend[4];
} SudekiMpLanStoryWorldActor;

typedef struct SudekiMpLanStoryWorldFrame {
    uint32_t epoch, revision, host_tick, sequence;
    uint8_t count;
    SudekiMpLanStoryWorldActor actors[SUDEKIMP_LAN_STORY_WORLD_MAX_ACTORS];
} SudekiMpLanStoryWorldFrame;

typedef struct SudekiMpLanStoryWorldChunk {
    uint32_t epoch, revision, host_tick, sequence;
    uint8_t total, index, chunks, count;
    SudekiMpLanStoryWorldActor actors[SUDEKIMP_LAN_STORY_WORLD_CHUNK_ACTORS];
} SudekiMpLanStoryWorldChunk;

BOOL SudekiMpLanStoryWorldActorValid(const SudekiMpLanStoryWorldActor *actor);
BOOL SudekiMpLanStoryWorldFrameValid(const SudekiMpLanStoryWorldFrame *frame);
BOOL SudekiMpLanStoryWorldFrameMatches(const SudekiMpLanStoryWorldFrame *world,
    const SudekiMpLanStoryFrame *party);
BOOL SudekiMpLanStoryWorldFrameMatchesScene(const SudekiMpLanStoryWorldFrame *frame,
    const SudekiMpLanStoryScene *scene);
unsigned SudekiMpLanStoryWorldChunkCount(unsigned actors);
BOOL SudekiMpLanStoryWorldChunkEncode(const SudekiMpLanStoryWorldFrame *frame,
    unsigned index,uint8_t *bytes,size_t capacity,size_t *written);
BOOL SudekiMpLanStoryWorldChunkDecode(const uint8_t *bytes,size_t size,
    SudekiMpLanStoryWorldChunk *chunk);
/* Same complete identity set/generations only; no extrapolation or smoothing
 * through an animation edge. A discrete state change arrives at its endpoint. */
BOOL SudekiMpLanStoryWorldFrameInterpolate(const SudekiMpLanStoryWorldFrame *before,
    const SudekiMpLanStoryWorldFrame *after,uint32_t host_tick,
    SudekiMpLanStoryWorldFrame *sample);

#endif
