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
    SUDEKIMP_STORY_ACTION_SPIRIT
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
    SUDEKIMP_STORY_ACTION_RETAINED
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
BOOL SudekiMpLanStoryControlMatchesScene(const SudekiMpLanStoryControlFence *fence,
    const SudekiMpLanStoryScene *scene);
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
