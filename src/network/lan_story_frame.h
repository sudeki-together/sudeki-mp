#ifndef SUDEKIMP_LAN_STORY_FRAME_H
#define SUDEKIMP_LAN_STORY_FRAME_H

#include "network/lan_arena_protocol.h"
#include "network/lan_party_story.h"
#include <windows.h>

#define SUDEKIMP_LAN_STORY_FRAME_VERSION 7u
#define SUDEKIMP_LAN_STORY_FRAME_HEADER_SIZE 108u
#define SUDEKIMP_LAN_STORY_ACTOR_WIRE_SIZE 248u
#define SUDEKIMP_LAN_STORY_FRAME_MAX_SIZE (108u + 4u * 248u)
#define SUDEKIMP_LAN_STORY_SHOT_HISTORY 4u

/* Positive host-native emissions only, never inferred from held input or an
 * animation. Namespace is session + frame epoch + actor generation. The
 * sequence never wraps in that namespace. Geometry is copied at emission,
 * not from the actor's later pose. It grants cosmetic replay only; host owns
 * charge, collisions, damage and other world consequences. */
typedef struct SudekiMpLanStoryShot {
    uint32_t sequence, host_tick;
    uint8_t item;
    uint16_t pre_charge_q8;
    float origin[3], direction[3];
} SudekiMpLanStoryShot;
typedef struct SudekiMpLanStoryShots {
    uint32_t latest;
    uint8_t count;
    SudekiMpLanStoryShot events[SUDEKIMP_LAN_STORY_SHOT_HISTORY];
} SudekiMpLanStoryShots;
BOOL SudekiMpLanStoryShotsValid(const SudekiMpLanStoryShots *shots,unsigned character);
/* Plain-data accepted-emission journal. Failure leaves it unchanged. */
BOOL SudekiMpLanStoryShotsAppend(SudekiMpLanStoryShots *shots,unsigned character,
    const SudekiMpLanStoryShot *emission);
/* Bounded replay cursor. Bootstrap must explicitly seed it to latest, so a
 * late join/reconnect cannot replay historical fire. Caller owns freshness,
 * authority, epoch/generation and positive native admission before advancing. */
const SudekiMpLanStoryShot *SudekiMpLanStoryShotNext(const SudekiMpLanStoryShots *shots,
    unsigned character,uint32_t host_tick,uint32_t cursor);

/* Host render view for unavailable-character spectators. No native pointers
 * or camera selection commands are transmitted. Missing view stays zero. */
typedef struct SudekiMpLanStoryView {
    uint8_t valid;
    uint32_t camera_serial; /* Host-issued selected-camera continuity fence. */
    float matrix[16], projection[3];
} SudekiMpLanStoryView;
/* Geometry-only check for a locally retained camera; it has no host serial. */
BOOL SudekiMpLanStoryViewGeometryValid(const SudekiMpLanStoryView *view);
BOOL SudekiMpLanStoryViewValid(const SudekiMpLanStoryView *view);

/* Canonical character identity, never a connection/player index. Only actors
 * actually in the host's exact native party are present. This closed first
 * profile carries noncombat movement or external native poses. Dialogue clips
 * are never described as idle semantic movement. */
typedef struct SudekiMpLanStoryActor {
    uint32_t generation;
    uint8_t character, animation_state;
    /* Zero means no equipped item; otherwise native item ID plus one.
     * This is NOT the inventory's variable row index. */
    uint8_t weapon_item_plus_one;
    /* Native equipment visibility, independent of the body's visibility.
     * A hidden holstered item is not an instruction to unequip it. */
    uint8_t weapon_visible;
    uint8_t weapon_attachment[2]; /* 0 hidden/detached, 1 hand, 2 sheath. */
    /* Native pose is supplied by the matching world record. Semantic fields
     * must be zero; this flag never authorizes a native object or action. */
    uint8_t native_pose;
    uint32_t hp, sp;
    float x, y, z, facing_x, facing_z;
    SudekiMpLanArenaLocomotion locomotion;
    SudekiMpLanStoryShots shots;
} SudekiMpLanStoryActor;

typedef struct SudekiMpLanStoryFrame {
    uint32_t epoch, revision, host_tick, sequence;
    uint8_t available_mask, leader_character;
    /* Authoritative mode, observed with this exact pose batch. This is a
     * presentation fact, never permission for client gameplay or damage. */
    uint8_t combat_mode;
    SudekiMpLanStoryView view;
    SudekiMpLanStoryActor actors[4]; /* Canonical indices; absent entries zero. */
} SudekiMpLanStoryFrame;

BOOL SudekiMpLanStoryActorValid(const SudekiMpLanStoryActor *actor,
    unsigned character);
BOOL SudekiMpLanStoryFrameValid(const SudekiMpLanStoryFrame *frame);
BOOL SudekiMpLanStoryFrameMatchesScene(const SudekiMpLanStoryFrame *frame,
    const SudekiMpLanStoryScene *scene);
/* Standalone payload only: authentication/lease/freshness belong to transport.
 * Decoder requires exact length, sorted unique characters and reserved zeros. */
BOOL SudekiMpLanStoryFrameEncode(const SudekiMpLanStoryFrame *frame,
    uint8_t *bytes,size_t capacity,size_t *written);
BOOL SudekiMpLanStoryFrameDecode(const uint8_t *bytes,size_t size,
    SudekiMpLanStoryFrame *frame);

/* Plain-data interpolation between authenticated frames from one unchanged
 * scene/roster/generation. Caller owns clock/history and session freshness.
 * Camera interpolation keeps an orthonormal basis and its handedness. Missing
 * views, changed projection or large/gapped view changes retain the prior view
 * until the endpoint. Camera serial changes fence selected-camera changes;
 * small cuts within one authored camera animation remain indistinguishable.
 * No extrapolation, actor creation, input admission or native calls. */
/* Shared binary32 position interpolation for the paired party/world codecs.
 * Callers first validate 0<=elapsed<=span and span>0. A single non-inlined
 * implementation prevents x87 excess-precision differences between units. */
void SudekiMpLanStoryInterpolatePosition(const float before[3],const float after[3],
    uint32_t elapsed,uint32_t span,float position[3]);
BOOL SudekiMpLanStoryFrameInterpolate(const SudekiMpLanStoryFrame *before,
    const SudekiMpLanStoryFrame *after,uint32_t host_tick,
    SudekiMpLanStoryFrame *sample);

#endif
