#ifndef SUDEKIMP_LAN_STORY_WORLD_FRAME_H
#define SUDEKIMP_LAN_STORY_WORLD_FRAME_H

#include "network/lan_story_frame.h"

/* A separate bounded presentation stream. These records grant no authority
 * to create objects, execute scripts, select assets or apply damage. Native
 * playback must resolve each identity to an already loaded exact NPC or
 * canonical party owner, or an already loaded generic scenery entity. */
#define SUDEKIMP_LAN_STORY_WORLD_VERSION 11u
#define SUDEKIMP_LAN_STORY_WORLD_MAX_ACTORS 64u
#define SUDEKIMP_LAN_STORY_WORLD_CHUNK_ACTORS 8u
#define SUDEKIMP_LAN_STORY_WORLD_MAX_CHUNKS 8u
#define SUDEKIMP_LAN_STORY_WORLD_HEADER_SIZE 28u
#define SUDEKIMP_LAN_STORY_WORLD_ACTOR_SIZE 144u
#define SUDEKIMP_LAN_STORY_WORLD_CHUNK_MAX_SIZE (28u + 8u * 144u)
/* The native bank has at most 4096 entries including the empty selector. */
#define SUDEKIMP_LAN_STORY_WORLD_MAX_CLIP_OCCURRENCES 4095u
#define SUDEKIMP_LAN_STORY_WORLD_NPC_KIND 0x0f9bu
/* Group-spawned monster (#54); identifier is the portable spawn identity. */
#define SUDEKIMP_LAN_STORY_WORLD_MONSTER_KIND 0x0f99u
#define SUDEKIMP_LAN_STORY_WORLD_SCENERY_KIND 0x0f8fu
/* DoorEntity (#67): native door model pose follows the host like scenery. */
#define SUDEKIMP_LAN_STORY_WORLD_DOOR_KIND 0x0f8cu
/* Chest entity (#66): open animation follows the host like a door. */
#define SUDEKIMP_LAN_STORY_WORLD_CHEST_KIND 0x0f9cu
#define SUDEKIMP_LAN_STORY_WORLD_HIDDEN 1u
#define SUDEKIMP_LAN_STORY_WORLD_STAT_VISIBLE 2u
#define SUDEKIMP_LAN_STORY_WORLD_DEAD 4u
#define SUDEKIMP_LAN_STORY_WORLD_DEATH_SHIFT 3u
#define SUDEKIMP_LAN_STORY_WORLD_DEATH_MASK (7u << SUDEKIMP_LAN_STORY_WORLD_DEATH_SHIFT)
/* Scenery, doors and chests: `alpha` carries the host render object's
 * colour alpha (object+0x13). A script fade (an invisible chest revealed by
 * Ailish's dispel) changes only that byte; the client's own script never
 * runs it (CONFIRMED_LIVE 2026-10-16, HC_TutorialInvChest). */
#define SUDEKIMP_LAN_STORY_WORLD_ALPHA 0x40u
/* Portable record category, NOT ResourceName encoded_kind: the native low
 * seven bits may change during lazy resolution. Canonical ID is independent. */
#define SUDEKIMP_LAN_STORY_WORLD_PC_KIND 0x0f81u
uint32_t SudekiMpLanStoryWorldCharacterIdentifier(unsigned character);
unsigned SudekiMpLanStoryWorldCharacter(uint32_t identifier);

typedef struct SudekiMpLanStoryWorldActor {
    uint16_t kind;
    uint8_t submodels;
    /* Category-specific presentation bits; never native flag words. */
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
    /* Monsters only (#54): host native HitPoints (CStats+0x2C); 0 otherwise. */
    float hit_points;
    /* Valid only with SUDEKIMP_LAN_STORY_WORLD_ALPHA; 0 otherwise. */
    uint8_t alpha;
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
/* Same complete identity set/generations only; no extrapolation. Discrete
 * channels arrive at their endpoints; unchanged PC channels/blend subtrees
 * may interpolate while a different channel changes or wraps. */
BOOL SudekiMpLanStoryWorldFrameInterpolate(const SudekiMpLanStoryWorldFrame *before,
    const SudekiMpLanStoryWorldFrame *after,uint32_t host_tick,
    SudekiMpLanStoryWorldFrame *sample);

#endif
