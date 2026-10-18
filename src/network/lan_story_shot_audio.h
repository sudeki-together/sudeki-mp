#ifndef SUDEKIMP_LAN_STORY_SHOT_AUDIO_H
#define SUDEKIMP_LAN_STORY_SHOT_AUDIO_H
#include <windows.h>
#include <stdint.h>
#include "network/lan_story_frame.h"
#include "network/lan_story_vfx.h"

/* Client shot audio planner (pure; no native memory). Decides, from data the
 * client already receives, which ranged-shot cues its own game should play,
 * the 'Final Fantasy way': each game plays the cue itself, positionally.
 * Native semantics copied (CMissile init 0x186E10, CONFIRMED_STATIC): one
 * fire cue per shot, fixed at the launch point; one travel cue that follows
 * the projectile and stops when it retires.
 * - Party shots (Ailish owner type 0x01, Elco 0x0E): FIRE once per host shot
 *   journal entry (SudekiMpLanStoryShotNext, cursor seeded on a namespace
 *   change so nothing historical replays); TRAVEL for a party projectile VFX
 *   entry (skill sequence 0; Ailish kinds 43-51, Elco 59-66) paired with a
 *   journal shot of the same character within -30..+100 ms of its emitted
 *   host tick (the item comes from that shot).
 * - World missiles (owner type 255, skill 0, kinds SFXKK105_PROJECTILE02 and
 *   SFXKK106_PROJECTILE03 only): WORLD_FIRE and WORLD_TRAVEL, separate
 *   actions with separate commit state, first sight at most 250 ms old.
 * - MOVE when a live travel entry's position changes; RETIRE when it leaves
 *   a fresh roster, or for everything when the VFX roster has been stale
 *   for more than 250 ms (no replay afterwards: instances at or below the
 *   newest seen instance never start again).
 * The executor reports PLAYED, SILENT or DEFER per action (Commit). */
enum {
    SUDEKIMP_SHOT_AUDIO_PARTY_FIRE=1, SUDEKIMP_SHOT_AUDIO_PARTY_TRAVEL=2,
    SUDEKIMP_SHOT_AUDIO_WORLD_FIRE=3, SUDEKIMP_SHOT_AUDIO_WORLD_TRAVEL=4,
    SUDEKIMP_SHOT_AUDIO_MOVE=5, SUDEKIMP_SHOT_AUDIO_RETIRE=6
};
enum { SUDEKIMP_SHOT_AUDIO_PLAYED=0, SUDEKIMP_SHOT_AUDIO_SILENT=1, SUDEKIMP_SHOT_AUDIO_DEFER=2 };
enum { SUDEKIMP_SHOT_AUDIO_MAX_ACTIONS=32u };
typedef struct SudekiMpStoryShotAudioAction {
    uint8_t type;
    uint8_t character;      /* party: 0..3; world: 4 */
    uint8_t item;           /* party: weapon item id of the shot; world: 0 */
    uint8_t local;          /* party fire only: the shooter is this game's own controlled character */
    uint8_t vfx_kind;       /* travel/world: the VFX kind; party fire: 0 */
    uint32_t key;           /* executor record key: party fire = shot sequence; others = VFX instance */
    uint32_t emitted_tick;  /* VFX emitted host tick (travel/world) or shot host tick (party fire) */
    float position[3];      /* fire: launch point; travel/world: first position; move: new position */
} SudekiMpStoryShotAudioAction;
typedef struct SudekiMpStoryShotAudioInput {
    uint32_t now;                       /* local GetTickCount */
    uint64_t namespace_token;           /* session/lease/stream identity; a change resets everything */
    unsigned local_character;           /* this game's proven controlled character, or 4 */
    uint8_t available_mask;             /* proven local roster */
    const SudekiMpLanStoryFrame *journal; /* fresh party frame carrying the shot journals, or NULL */
    const SudekiMpStoryVfx *vfx;          /* fresh story VFX roster, or NULL */
} SudekiMpStoryShotAudioInput;
enum { SUDEKIMP_SHOT_AUDIO_SEEN=64u };
typedef struct SudekiMpStoryShotAudioSeen {
    uint32_t instance, emitted_tick;
    uint8_t kind, owner, character, item, state; /* state: 0 free, 1 pending, 2 live, 3 decided */
    uint8_t fire_state, travel_state;            /* world entries: per-cue commit state */
    float position[3];
} SudekiMpStoryShotAudioSeen;
typedef struct SudekiMpStoryShotAudioPlan {
    uint64_t namespace_token;
    uint32_t cursor[4], journal_epoch, generation[4];
    uint8_t seeded[4];
    uint64_t vfx_stream; uint32_t vfx_epoch, vfx_revision;
    uint32_t newest, last_fresh;
    uint8_t vfx_known, overflow_logged;
    /* Frame sequence of the newest journal seen: an older view (a lower
     * sequence in the same epoch) never reseeds a cursor downward. */
    uint32_t journal_sequence; uint8_t journal_sequence_known;
    SudekiMpStoryShotAudioSeen seen[SUDEKIMP_SHOT_AUDIO_SEEN];
} SudekiMpStoryShotAudioPlan;
void SudekiMpStoryShotAudioPlanReset(SudekiMpStoryShotAudioPlan *plan);
/* Fills at most `max` actions in a deterministic order (retires first, then
 * moves, then new cues by ascending instance or sequence). */
unsigned SudekiMpStoryShotAudioPlanStep(SudekiMpStoryShotAudioPlan *plan,
    const SudekiMpStoryShotAudioInput *input,SudekiMpStoryShotAudioAction *out,unsigned max);
/* Result of executing one action from the latest Step. */
void SudekiMpStoryShotAudioPlanCommit(SudekiMpStoryShotAudioPlan *plan,
    const SudekiMpStoryShotAudioAction *action,unsigned result);
#endif
