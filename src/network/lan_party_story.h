#ifndef SUDEKIMP_LAN_PARTY_STORY_H
#define SUDEKIMP_LAN_PARTY_STORY_H

#include <stddef.h>
#include <stdint.h>

/* Scene metadata only. Neither presence nor connectivity grants actor input.
 * World epochs are host-issued values, never process pointers. */
#define SUDEKIMP_LAN_STORY_NAME_SIZE 64u
#define SUDEKIMP_LAN_STORY_WIRE_SIZE 143u
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
} SudekiMpLanStoryScene;

int SudekiMpLanStorySceneValid(const SudekiMpLanStoryScene *scene);
int SudekiMpLanStorySceneSame(const SudekiMpLanStoryScene *a,
    const SudekiMpLanStoryScene *b);
/* A repeated packet cannot renew observation freshness. A revised roster
 * cannot reuse its revision; a changed area cannot reuse its world epoch. */
int SudekiMpLanStorySceneAdvances(const SudekiMpLanStoryScene *prior,
    const SudekiMpLanStoryScene *next);
int SudekiMpLanStorySceneEncode(const SudekiMpLanStoryScene *scene,
    uint8_t *bytes, size_t size);
int SudekiMpLanStorySceneDecode(const uint8_t *bytes, size_t size,
    SudekiMpLanStoryScene *scene);
/* Intended view policy, not input authorization. Unknown/loading returns
 * NO_SEAT. Unavailable players may follow a present companion. */
unsigned int SudekiMpLanStoryViewSeat(const SudekiMpLanStoryScene *scene,
    unsigned int player_seat, unsigned int preferred_companion);

#endif
