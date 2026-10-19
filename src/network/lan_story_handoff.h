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

/* World direction times native controller magnitude, not a unit stick.
 * Lateral clamps to one; forward preserves the bounded input scale <=4.004.
 * sqrt(1 + 4.004^2) < 4.13. Wire layout and units remain unchanged. */
#define SUDEKIMP_STORY_MOVEMENT_MAX 4.13f
BOOL SudekiMpLanStoryMovementVectorValid(float x,float z);

#define SUDEKIMP_STORY_CONTROL_WIRE_SIZE 24u
#define SUDEKIMP_STORY_CONTROL_FENCE_SIZE 20u
#define SUDEKIMP_STORY_MOVEMENT_WIRE_SIZE 36u
#define SUDEKIMP_STORY_CONTROL_MAX_AGE_MS 250u

/* Additive ranged input message; existing movement/action layouts stay intact.
 * press is an owner-local monotonic edge journal; held enables retail repeat.
 * The host validates aim against its actor and owns all native emissions. */
typedef struct SudekiMpLanStoryRanged {
    SudekiMpLanStoryControlFence fence;
    uint32_t sequence,acknowledged_frame,press;
    uint8_t held;
    float direction[3],target[3];
} SudekiMpLanStoryRanged;
#define SUDEKIMP_STORY_RANGED_WIRE_SIZE 60u
BOOL SudekiMpLanStoryRangedValid(const SudekiMpLanStoryRanged *input);
BOOL SudekiMpLanStoryRangedEncode(const SudekiMpLanStoryRanged *input,uint8_t *bytes,size_t size);
BOOL SudekiMpLanStoryRangedDecode(const uint8_t *bytes,size_t size,SudekiMpLanStoryRanged *input);

/* Additive held-block channel for Tal/Buki. No attack journal or legacy
 * movement bytes are repurposed. Expiry closes held intent on the host. */
typedef struct SudekiMpLanStoryBlock {
    SudekiMpLanStoryControlFence fence;
    uint32_t sequence,acknowledged_frame;
    uint8_t held;
} SudekiMpLanStoryBlock;
#define SUDEKIMP_STORY_BLOCK_WIRE_SIZE 32u
BOOL SudekiMpLanStoryBlockValid(const SudekiMpLanStoryBlock *input);
BOOL SudekiMpLanStoryBlockEncode(const SudekiMpLanStoryBlock *input,uint8_t *bytes,size_t size);
BOOL SudekiMpLanStoryBlockDecode(const uint8_t *bytes,size_t size,SudekiMpLanStoryBlock *input);

/* Discrete saved-story actions have their own request/result journal.
 * Transport acceptance is never native execution. A request is consumed once
 * for one exact control fence; retransmission may recover its result but must
 * not submit another cast. Character skill slots are zero-based (0..5);
 * Spirit variants retain the native actor-specific one-based pair (1..2). */
typedef enum SudekiMpLanStoryActionKind {
    SUDEKIMP_STORY_ACTION_SKILL=1,
    SUDEKIMP_STORY_ACTION_SPIRIT,
    /* Tal and Buki. slot is 1 weak, 2 strong, 3 sweep.
     * A press requests native admission, never an animation or hit result. */
    SUDEKIMP_STORY_ACTION_MELEE,
    /* #75: equip a weapon the requesting player chose in their own M menu.
     * slot is the native weapon item id (0..47) and must belong to the
     * fence character's weapon family. The host grants a missing item to its
     * own game first (equipment follows the character), then equips natively;
     * the party frame carries the result back. Never armour, never unequip. */
    SUDEKIMP_STORY_ACTION_EQUIP_WEAPON,
    /* #75 / D10: use a consumable on the requesting player's OWN character
     * (never a target choice). The item id (0..998) is slot | extra<<8. The
     * host runs the Items page's native can-use / apply / ReduceItem path. */
    SUDEKIMP_STORY_ACTION_USE_ITEM,
    /* #40/#74: equip an armour the requesting player chose in their own M
     * menu (armour choice page). slot is the native armour item id
     * (100..139); the host checks the class for the fence character, grants
     * a missing item to its own game, then equips natively; the party frame
     * carries the worn armour back. Never unequip. */
    SUDEKIMP_STORY_ACTION_EQUIP_ARMOUR
} SudekiMpLanStoryActionKind;
#define SUDEKIMP_STORY_ACTION_ITEM(slot,extra) ((unsigned)(slot)|((unsigned)(extra)<<8))
/* Native weapon item ids per character: Tal 0..11, Ailish 12..23,
 * Elco 24..35, Buki 36..47 (CItem id +0x14). CONFIRMED_LIVE 2026-10-18:
 * Buki's own native Weapons page lists ids 36..47, and an item lookup in
 * Elco's category returned ids 26, 30 and 35. The weapon inventory category
 * follows the portrait order instead (RVA 0x43F430 portrait enum plus four:
 * Tal 4, Ailish 5, Buki 6, Elco 7; CONFIRMED_STATIC), so the id blocks of
 * Buki and Elco are not in category order. One rule for the menu, the quick
 * menu, the host request check and the frame validator. */
/* Equipment and item requests the host may run while its world is in
 * combat (the character's body must still be idle: not attacking, drawing
 * or casting). Owner's account of vanilla, 2026-10-18: weapons change and
 * items are used in and out of combat; armour changes only out of it. Pure. */
static inline BOOL SudekiMpStoryActionAllowedInCombat(unsigned kind) {
    return kind==SUDEKIMP_STORY_ACTION_EQUIP_WEAPON || kind==SUDEKIMP_STORY_ACTION_USE_ITEM;
}
static inline unsigned SudekiMpStoryWeaponCategoryForCharacter(unsigned character) {
    static const unsigned category[4]={6u,7u,4u,5u}; /* Buki, Elco, Tal, Ailish */
    return character<4u?category[character]:0u;
}
static inline BOOL SudekiMpStoryWeaponItemForCharacter(unsigned character,unsigned item) {
    static const unsigned first[4]={36u,24u,0u,12u}; /* Buki, Elco, Tal, Ailish */
    return character<4u && item>=first[character] && item<first[character]+12u;
}
typedef struct SudekiMpLanStoryActionRequest {
    SudekiMpLanStoryControlFence fence;
    uint32_t request,acknowledged_frame;
    uint8_t kind,slot,extra; /* extra: high item-id byte for USE_ITEM, else 0 */
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
    SUDEKIMP_STORY_ACTION_NO_SP /* Ordinary SP or shared Spirit SSP, according to kind. */
} SudekiMpLanStoryActionOutcome;
typedef struct SudekiMpLanStoryActionResult {
    SudekiMpLanStoryControlFence fence;
    uint32_t request,observed_tick;
    uint8_t kind,slot,outcome,extra;
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
/* Same epoch, fence revision not newer than the scene, character available
 * and not the leader. Later same-epoch revisions (e.g. another player's
 * split-area travel) keep the fence; actor identity is fenced separately by
 * actor_generation and native roster checks. */
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
