#ifndef SUDEKIMP_LAN_PARTY_STORY_H
#define SUDEKIMP_LAN_PARTY_STORY_H

#include <stddef.h>
#include <stdint.h>

/* Scene metadata only. Neither presence nor connectivity grants actor input.
 * World epochs are host-issued values, never process pointers. */
#define SUDEKIMP_LAN_STORY_NAME_SIZE 64u
#define SUDEKIMP_LAN_STORY_WIRE_SIZE 144u
#define SUDEKIMP_LAN_STORY_NO_SEAT 4u
#define SUDEKIMP_LAN_STORY_MAX_AGE_MS 1000u

typedef enum SudekiMpLanStoryPhase {
    SUDEKIMP_LAN_STORY_UNKNOWN = 0,
    SUDEKIMP_LAN_STORY_LOADING,
    SUDEKIMP_LAN_STORY_READY
} SudekiMpLanStoryPhase;

typedef struct SudekiMpLanStoryScene {
    uint32_t epoch, revision, observed_tick;
    uint8_t phase, available_mask, leader_seat;
    char world[SUDEKIMP_LAN_STORY_NAME_SIZE];
    char temporary[SUDEKIMP_LAN_STORY_NAME_SIZE];
    /* Characters (bit = character index) currently inside `temporary`; the
     * rest of available_mask are in the exterior `world`. When READY,
     * temporary is non-empty exactly when inside_mask is non-zero, and
     * inside_mask is a subset of available_mask. Vanilla whole-party travel is
     * inside_mask==available_mask. Never set outside READY. */
    uint8_t inside_mask;
    /* The host keeps the exterior `world` loaded and live while characters
     * are in `temporary` (split door visits, H1b shop visits), so the
     * exterior's lifetime (epoch) continues even when nobody stands in it.
     * Only READY with a temporary; vanilla whole-party travel never sets it.
     * Wire: bit 7 of the inside_mask byte. */
    uint8_t exterior_retained;
} SudekiMpLanStoryScene;

int SudekiMpLanStorySceneValid(const SudekiMpLanStoryScene *scene);
int SudekiMpLanStorySceneSame(const SudekiMpLanStoryScene *a,
    const SudekiMpLanStoryScene *b);
/* A repeated packet cannot renew observation freshness. A revised roster
 * cannot reuse its revision. A changed exterior world needs a new epoch. The
 * occupied temporary may change within an epoch only while at least one
 * available character stays in the exterior on both sides (the exterior's
 * lifetime is continuous); otherwise it also needs a new epoch. */
int SudekiMpLanStorySceneAdvances(const SudekiMpLanStoryScene *prior,
    const SudekiMpLanStoryScene *next);
int SudekiMpLanStorySceneEncode(const SudekiMpLanStoryScene *scene,
    uint8_t *bytes, size_t size);
int SudekiMpLanStorySceneDecode(const uint8_t *bytes, size_t size,
    SudekiMpLanStoryScene *scene);
/* Intended view policy, not input authorization. Unknown/loading returns
 * NO_SEAT. Unavailable players may follow a present companion. */
/* Area of one character in a READY scene: copies world and, when that
 * character is inside, temporary (else empty). Fails for unavailable characters. */
int SudekiMpLanStorySceneCharacterArea(const SudekiMpLanStoryScene *scene,
    unsigned int character, char world[SUDEKIMP_LAN_STORY_NAME_SIZE],
    char temporary[SUDEKIMP_LAN_STORY_NAME_SIZE]);
/* TRUE when a native (world, temporary) pair equals that character's area. */
int SudekiMpLanStorySceneCharacterAreaMatches(const SudekiMpLanStoryScene *scene,
    unsigned int character, const char *world, const char *temporary);
/* TRUE when both characters are available and share an area. */
int SudekiMpLanStorySceneSameArea(const SudekiMpLanStoryScene *scene,
    unsigned int a, unsigned int b);
unsigned int SudekiMpLanStoryViewSeat(const SudekiMpLanStoryScene *scene,
    unsigned int player_seat, unsigned int preferred_companion);
/* Is a host conversation (the lead's) presented to this viewer? Interim
 * rule (owner 2026-10-17): not to a viewer inside its own house visit while
 * the lead is elsewhere; otherwise as before (same area, nobody inside, or
 * the viewer waiting outside the lead's house). Unknown (invalid scene, no
 * lead, no view seat) is FALSE. */
int SudekiMpLanStorySceneConversationShared(const SudekiMpLanStoryScene *scene,
    unsigned int viewer_character, unsigned int preferred_companion);

#endif
