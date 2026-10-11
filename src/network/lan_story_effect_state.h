#ifndef SUDEKIMP_LAN_STORY_EFFECT_STATE_H
#define SUDEKIMP_LAN_STORY_EFFECT_STATE_H
#include "network/lan_story_area_state.h"
#define SUDEKIMP_STORY_SPAWN_GROUP_MAX 8u
#define SUDEKIMP_STORY_SKY_KINDS 6u
#define SUDEKIMP_STORY_SKY_ARGS 5u
#define SUDEKIMP_STORY_SIGIL_MAX 32u
#define SUDEKIMP_STORY_DROP_MAX 16u
#define SUDEKIMP_STORY_EFFECT_CODEC_VERSION 2u
/* 32 retained per-spawn receipts and 16 authored drop records. No pointers. */
#define SUDEKIMP_STORY_EFFECT_WIRE_SIZE 2269u
#define SUDEKIMP_STORY_EFFECT_CHUNK_BYTES 1152u
#define SUDEKIMP_STORY_EFFECT_CHUNK_HEADER 28u
#define SUDEKIMP_STORY_EFFECT_CHUNKS ((SUDEKIMP_STORY_EFFECT_WIRE_SIZE+SUDEKIMP_STORY_EFFECT_CHUNK_BYTES-1u)/SUDEKIMP_STORY_EFFECT_CHUNK_BYTES)
#define SUDEKIMP_STORY_EFFECT_CHUNK_MAX (SUDEKIMP_STORY_EFFECT_CHUNK_HEADER+SUDEKIMP_STORY_EFFECT_CHUNK_BYTES)
/* Recent host lighting script calls in call order (#54), oldest first:
 * kind (SUDEKIMP_STORY_LIGHT_*), four raw argument words. serial 0 = empty. */
#define SUDEKIMP_STORY_LIGHT_MAX 12u
enum { SUDEKIMP_STORY_LIGHT_AMBIENT=1, SUDEKIMP_STORY_LIGHT_GROUP_SET, SUDEKIMP_STORY_LIGHT_GROUP_INC,
    SUDEKIMP_STORY_LIGHT_BRIGHTNESS, SUDEKIMP_STORY_LIGHT_COLOUR_ADJUST, SUDEKIMP_STORY_LIGHT_BACKGROUND,
    SUDEKIMP_STORY_LIGHT_CHANGE_RATE, SUDEKIMP_STORY_LIGHT_KIND_LAST=SUDEKIMP_STORY_LIGHT_CHANGE_RATE };
typedef struct SudekiMpStoryLightCall {
    uint32_t serial,kind,args[4];
} SudekiMpStoryLightCall;
/* Recent host despawns of group-spawned monsters by portable identity (#54),
 * oldest first; serial 0 = empty slot. A client removes its copy once. */
#define SUDEKIMP_STORY_DESPAWN_MAX 16u
typedef struct SudekiMpStoryDespawn {
    uint32_t serial,identity;
    /* 0: unknown; 1..4: authored actions 0x17..0x1A. */
    uint32_t death_style;
} SudekiMpStoryDespawn;

/* Latest native sky/fog script call per kind on the host (#54): colour,
 * blend, brightness, fog colour, fog range, vertical fog. serial 0 = none;
 * args are the raw 32-bit argument words (floats as bits). */
enum { SUDEKIMP_STORY_SKY_COLOUR=0, SUDEKIMP_STORY_SKY_BLEND, SUDEKIMP_STORY_SKY_BRIGHTNESS,
    SUDEKIMP_STORY_SKY_FOG_COLOUR, SUDEKIMP_STORY_SKY_FOG_RANGE, SUDEKIMP_STORY_SKY_VERTICAL_FOG };
typedef struct SudekiMpStorySkyCall {
    uint32_t serial;
    uint32_t args[SUDEKIMP_STORY_SKY_ARGS];
} SudekiMpStorySkyCall;

/* A native spawn group the host has started (InternalStartSpawnGroupSequence),
 * newest sequence per group name (#54). */
typedef struct SudekiMpStorySpawnGroup {
    char name[SUDEKIMP_STORY_AREA_NAME];
    int32_t sequence;
} SudekiMpStorySpawnGroup;

/* occurrence is the constructor count for this portable spawn identity;
 * spawn_serial is assigned by the host. A previous occurrence cannot end a
 * later wave. ended is positive native owner-release evidence, never a timer. */
typedef struct SudekiMpStorySigilState {
    uint32_t identity,occurrence,spawn_serial,ended;
} SudekiMpStorySigilState;
/* Wire foundation for Phase D. Empty until native drop capture is proved.
 * Position words are finite IEEE floats; collected is host authoritative. */
typedef struct SudekiMpStoryDropRecord {
    uint32_t serial,spawn_serial,resource,quantity,position[3],collected;
} SudekiMpStoryDropRecord;
typedef struct SudekiMpStoryEffectState {
    uint32_t epoch,revision,observed_tick,sequence;
    char current[SUDEKIMP_STORY_AREA_NAME];
    uint8_t group_count;
    SudekiMpStorySpawnGroup groups[SUDEKIMP_STORY_SPAWN_GROUP_MAX];
    SudekiMpStorySkyCall sky[SUDEKIMP_STORY_SKY_KINDS];
    char vertical_fog[SUDEKIMP_STORY_AREA_NAME];
    SudekiMpStoryDespawn despawned[SUDEKIMP_STORY_DESPAWN_MAX];
    SudekiMpStoryLightCall light[SUDEKIMP_STORY_LIGHT_MAX];
    /* Host CLightManager lighting target (+0x40..+0x48, set by its light
     * effects) and change rate (+0x30) as float bits; valid 0 = unknown. */
    uint32_t light_valid,light_target[3],light_rate;
    /* Latest host CSceneManager::SetSkyDome resource name; serial 0 = none. */
    uint32_t sky_dome_serial;
    char sky_dome[SUDEKIMP_STORY_AREA_NAME];
    /* Host sky dome play speed (scene+0x68, float bits; 0 = unknown). */
    uint32_t sky_dome_speed;
    uint32_t cluster_valid,cluster_hash,sector,in_combat,flag36,suspended;
    SudekiMpStorySigilState sigils[SUDEKIMP_STORY_SIGIL_MAX];
    SudekiMpStoryDropRecord drops[SUDEKIMP_STORY_DROP_MAX];
    /* Native battle-score presentation: bit 0 observed, bit 1 danger.
     * Score is a registered native BS_* name, intensity is finite [0,1].
     * Unknown is all zero; never infer peaceful music from a missing sample. */
    uint32_t music_flags,music_intensity;
    uint32_t music_variation,music_curve[6]; /* peaceful, battle, remaining, reset, ramp, hold */
    char music_score[SUDEKIMP_STORY_AREA_NAME];
} SudekiMpStoryEffectState;
typedef struct SudekiMpStoryEffectChunk {
    uint32_t epoch,revision,sequence,observed_tick,digest,index;
    uint8_t bytes[SUDEKIMP_STORY_EFFECT_CHUNK_BYTES];
} SudekiMpStoryEffectChunk;
/* Bounded assembly. A failed/truncated/duplicate/stale packet cannot publish
 * partial state or refresh its age. The session resets this on scene change. */
typedef struct SudekiMpStoryEffectAssembly {
    uint32_t sequence,mask,received_at;
    SudekiMpStoryEffectChunk chunks[SUDEKIMP_STORY_EFFECT_CHUNKS];
} SudekiMpStoryEffectAssembly;
BOOL SudekiMpStoryEffectStateValid(const SudekiMpStoryEffectState *state);
BOOL SudekiMpStoryEffectStateEncode(const SudekiMpStoryEffectState *state,uint8_t *bytes,size_t size);
BOOL SudekiMpStoryEffectStateDecode(const uint8_t *bytes,size_t size,SudekiMpStoryEffectState *state);
BOOL SudekiMpStoryEffectChunkEncode(const SudekiMpStoryEffectState *state,unsigned index,uint8_t *bytes,size_t capacity,size_t *written);
BOOL SudekiMpStoryEffectChunkDecode(const uint8_t *bytes,size_t size,SudekiMpStoryEffectChunk *chunk);
BOOL SudekiMpStoryEffectAssemble(const SudekiMpStoryEffectChunk *chunks,unsigned count,SudekiMpStoryEffectState *state);
#endif
