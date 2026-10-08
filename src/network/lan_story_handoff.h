#ifndef SUDEKIMP_LAN_STORY_HANDOFF_H
#define SUDEKIMP_LAN_STORY_HANDOFF_H

#include "network/lan_party_story.h"
#include <windows.h>

/* Saved-story-only ownership protocol. These are plain host-issued fences,
 * never native pointers or a request to change the world's lead character. */
typedef struct SudekiMpLanStoryControlFence {
    uint32_t epoch,revision,transaction,actor_generation;
    uint8_t player,character;
} SudekiMpLanStoryControlFence;
/* Ally seat (Dev Play): the fence drives a host-spawned ALLY_TALOS entity that
 * is not one of the four heroes and never joins the native party group. It
 * is outside the scene's available_mask/leader rules; the host proves the
 * entity itself. Hero-only kinds (SKILL) stay unavailable for it. */
#define SUDEKIMP_STORY_CHARACTER_ALLY 4u
typedef enum SudekiMpLanStoryControlPhase {
    SUDEKIMP_STORY_CONTROL_PREPARE=1,
    SUDEKIMP_STORY_CONTROL_READY,
    SUDEKIMP_STORY_CONTROL_REVOKED
} SudekiMpLanStoryControlPhase;
typedef struct SudekiMpLanStoryControlState {
    SudekiMpLanStoryControlFence fence;
    uint32_t observed_tick;
    uint8_t phase;
} SudekiMpLanStoryControlState;
typedef struct SudekiMpLanStoryMovement {
    SudekiMpLanStoryControlFence fence;
    uint32_t sequence,acknowledged_frame;
    float world_x,world_z;
} SudekiMpLanStoryMovement;

#define SUDEKIMP_STORY_CONTROL_WIRE_SIZE 24u
#define SUDEKIMP_STORY_CONTROL_FENCE_SIZE 20u
#define SUDEKIMP_STORY_MOVEMENT_WIRE_SIZE 36u
#define SUDEKIMP_STORY_CONTROL_MAX_AGE_MS 250u

/* Discrete saved-story actions have their own request/result journal.
 * Transport acceptance is never native execution. A request is consumed once
 * for one exact control fence; retransmission may recover its result but must
 * not submit another cast. Character skill slots are zero-based (0..5);
 * Spirit variants retain the native actor-specific one-based pair (1..2). */
typedef enum SudekiMpLanStoryActionKind {
    SUDEKIMP_STORY_ACTION_SKILL=1,
    SUDEKIMP_STORY_ACTION_SPIRIT,
    /* Tal only in this closed story slice. slot is 1 weak, 2 strong, 3 sweep.
     * A press requests native admission, never an animation or hit result. */
    SUDEKIMP_STORY_ACTION_MELEE
} SudekiMpLanStoryActionKind;
typedef struct SudekiMpLanStoryActionRequest {
    SudekiMpLanStoryControlFence fence;
    uint32_t request,acknowledged_frame;
    uint8_t kind,slot;
} SudekiMpLanStoryActionRequest;
typedef enum SudekiMpLanStoryActionOutcome {
    SUDEKIMP_STORY_ACTION_STARTED=1,
    SUDEKIMP_STORY_ACTION_UNAVAILABLE,
    SUDEKIMP_STORY_ACTION_BUSY,
    SUDEKIMP_STORY_ACTION_EXPIRED,
    /* Native submission may have started: retain its owner, never retry Use. */
    SUDEKIMP_STORY_ACTION_RETAINED,
    /* The native melee entry was called once. Its void ABI cannot prove an
     * accepted combo: only subsequent host-observed poses/consequences can. */
    SUDEKIMP_STORY_ACTION_SUBMITTED,
    SUDEKIMP_STORY_ACTION_NO_SP
} SudekiMpLanStoryActionOutcome;
typedef struct SudekiMpLanStoryActionResult {
    SudekiMpLanStoryControlFence fence;
    uint32_t request,observed_tick;
    uint8_t kind,slot,outcome;
} SudekiMpLanStoryActionResult;
#define SUDEKIMP_STORY_ACTION_REQUEST_WIRE_SIZE 32u
#define SUDEKIMP_STORY_ACTION_RESULT_WIRE_SIZE 32u
BOOL SudekiMpLanStoryActionRequestValid(const SudekiMpLanStoryActionRequest *request);
BOOL SudekiMpLanStoryActionRequestSame(const SudekiMpLanStoryActionRequest *a,
    const SudekiMpLanStoryActionRequest *b);
BOOL SudekiMpLanStoryActionResultValid(const SudekiMpLanStoryActionResult *result);
BOOL SudekiMpLanStoryActionResultMatches(const SudekiMpLanStoryActionResult *result,
    const SudekiMpLanStoryActionRequest *request);
BOOL SudekiMpLanStoryActionRequestEncode(const SudekiMpLanStoryActionRequest *request,
    uint8_t *bytes,size_t size);
BOOL SudekiMpLanStoryActionRequestDecode(const uint8_t *bytes,size_t size,
    SudekiMpLanStoryActionRequest *request);
BOOL SudekiMpLanStoryActionResultEncode(const SudekiMpLanStoryActionResult *result,
    uint8_t *bytes,size_t size);
BOOL SudekiMpLanStoryActionResultDecode(const uint8_t *bytes,size_t size,
    SudekiMpLanStoryActionResult *result);

/* Dev Play ally seat: the host's shadow of the native combo reader
 * (COMBO_GIZMO slots) for the ally entity, host -> client, latest wins. The
 * client writes these values into its own HUD controller; the record carries
 * no input, no actor identity and no permission to change gameplay. */
typedef struct SudekiMpLanStoryAllyHud {
    SudekiMpLanStoryControlFence fence; /* character == SUDEKIMP_STORY_CHARACTER_ALLY */
    uint32_t sequence,observed_tick;
    uint8_t slots[3];   /* native slot kinds 0..6; 7 = empty */
    uint8_t flags;      /* SUDEKIMP_STORY_ALLY_HUD_* bits */
} SudekiMpLanStoryAllyHud;
#define SUDEKIMP_STORY_ALLY_HUD_WIRE_SIZE 32u
#define SUDEKIMP_STORY_ALLY_HUD_SLOT_EMPTY 7u
#define SUDEKIMP_STORY_ALLY_HUD_FULL 0x01u      /* HUD +0x200: three slots filled */
#define SUDEKIMP_STORY_ALLY_HUD_ALT_ICONS 0x02u /* HUD +0x201: alternate icon set */
#define SUDEKIMP_STORY_ALLY_HUD_FLASH 0x04u     /* HUD +0x202: combo-complete flash */
#define SUDEKIMP_STORY_ALLY_HUD_ARMED 0x08u     /* HUD +0x203: slots accept pushes */
#define SUDEKIMP_STORY_ALLY_HUD_COMBAT 0x10u    /* ally arbiter in melee combat: HUD mode 3 */
#define SUDEKIMP_STORY_ALLY_HUD_FLAG_MASK 0x1fu
BOOL SudekiMpLanStoryAllyHudValid(const SudekiMpLanStoryAllyHud *hud);
BOOL SudekiMpLanStoryAllyHudEncode(const SudekiMpLanStoryAllyHud *hud,uint8_t *bytes,size_t size);
BOOL SudekiMpLanStoryAllyHudDecode(const uint8_t *bytes,size_t size,SudekiMpLanStoryAllyHud *hud);

/* Host-authoritative Dev Play avatar display record. This grants no native
 * control or stat-write authority. received_tick is a local receipt stamp;
 * it is never serialized or compared with the host's observed_tick. */
typedef struct SudekiMpLanStoryAvatarStatus {
    uint32_t epoch,revision,spawn_generation,sequence,observed_tick,received_tick;
    float hp,max_hp,sp,max_sp;
    uint8_t player,present;
    char name[32];
} SudekiMpLanStoryAvatarStatus;
#define SUDEKIMP_STORY_AVATAR_STATUS_WIRE_SIZE 72u
#define SUDEKIMP_STORY_AVATAR_STATUS_MAX_AGE_MS 250u
BOOL SudekiMpLanStoryAvatarStatusValid(const SudekiMpLanStoryAvatarStatus *status);
BOOL SudekiMpLanStoryAvatarStatusEncode(const SudekiMpLanStoryAvatarStatus *status,uint8_t *bytes,size_t size);
BOOL SudekiMpLanStoryAvatarStatusDecode(const uint8_t *bytes,size_t size,SudekiMpLanStoryAvatarStatus *status);

/* A host-observed authored recruitment result, limited to one closed route.
 * Load generation is intentionally absent: local native load journals have
 * independent counters. The client maps this to its retained matching save. */
typedef struct SudekiMpLanStoryRecruitment {
    uint32_t route,transaction,before_epoch,before_revision;
    uint32_t after_epoch,after_revision,actor_generation,observed_tick;
} SudekiMpLanStoryRecruitment;
#define SUDEKIMP_STORY_RECRUITMENT_WIRE_SIZE 32u
BOOL SudekiMpLanStoryRecruitmentValid(const SudekiMpLanStoryRecruitment *recruitment);
BOOL SudekiMpLanStoryRecruitmentSame(const SudekiMpLanStoryRecruitment *a,
    const SudekiMpLanStoryRecruitment *b);
BOOL SudekiMpLanStoryRecruitmentMatchesScene(const SudekiMpLanStoryRecruitment *recruitment,
    const SudekiMpLanStoryScene *scene);
BOOL SudekiMpLanStoryRecruitmentEncode(const SudekiMpLanStoryRecruitment *recruitment,
    uint8_t *bytes,size_t size);
BOOL SudekiMpLanStoryRecruitmentDecode(const uint8_t *bytes,size_t size,
    SudekiMpLanStoryRecruitment *recruitment);

BOOL SudekiMpLanStoryControlFenceValid(const SudekiMpLanStoryControlFence *fence);
BOOL SudekiMpLanStoryControlFenceSame(const SudekiMpLanStoryControlFence *a,
    const SudekiMpLanStoryControlFence *b);
/* Same epoch, fence revision not newer than the scene, character available
 * and not the leader. Later same-epoch revisions (e.g. another player's
 * split-area travel) keep the fence; actor identity is fenced separately by
 * actor_generation and native roster checks. */
BOOL SudekiMpLanStoryControlMatchesScene(const SudekiMpLanStoryControlFence *fence,
    const SudekiMpLanStoryScene *scene);
BOOL SudekiMpLanStoryControlMatchesSceneForPolicy(const SudekiMpLanStoryControlFence *fence,
    const SudekiMpLanStoryScene *scene,SudekiMpLanStoryPolicy policy);
BOOL SudekiMpLanStoryControlStateValid(const SudekiMpLanStoryControlState *state);
BOOL SudekiMpLanStoryMovementValid(const SudekiMpLanStoryMovement *input);
BOOL SudekiMpLanStoryControlEncode(const SudekiMpLanStoryControlState *state,
    uint8_t *bytes,size_t size);
BOOL SudekiMpLanStoryControlDecode(const uint8_t *bytes,size_t size,
    SudekiMpLanStoryControlState *state);
BOOL SudekiMpLanStoryControlFenceEncode(const SudekiMpLanStoryControlFence *fence,
    uint8_t *bytes,size_t size);
BOOL SudekiMpLanStoryControlFenceDecode(const uint8_t *bytes,size_t size,
    SudekiMpLanStoryControlFence *fence);
BOOL SudekiMpLanStoryMovementEncode(const SudekiMpLanStoryMovement *input,
    uint8_t *bytes,size_t size);
BOOL SudekiMpLanStoryMovementDecode(const uint8_t *bytes,size_t size,
    SudekiMpLanStoryMovement *input);

#endif
