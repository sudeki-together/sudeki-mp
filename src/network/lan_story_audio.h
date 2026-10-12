#ifndef SUDEKIMP_LAN_STORY_AUDIO_H
#define SUDEKIMP_LAN_STORY_AUDIO_H
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
/* Additive cosmetic snapshot, one complete character roster per datagram.
 * Cue names/pointers never cross the wire; index and digest must match the
 * locally loaded bank of the exact corresponding native party actor. */
#define SUDEKIMP_STORY_AUDIO_CAPACITY 64u
#define SUDEKIMP_STORY_AUDIO_HEADER 32u
#define SUDEKIMP_STORY_AUDIO_ROW 20u
#define SUDEKIMP_STORY_AUDIO_MAX (32u+20u*SUDEKIMP_STORY_AUDIO_CAPACITY)
typedef struct SudekiMpStoryAudioCue {
    uint32_t id,started,hash,gain;
    uint16_t index;
    uint8_t mode,spatial;
} SudekiMpStoryAudioCue;
typedef struct SudekiMpStoryAudio {
    uint32_t epoch,revision,tick,sequence,stream,bank;
    uint8_t character,count;
    uint16_t start_sequence;
    uint32_t start_tick; /* Last positively observed native global Spirit cue. */
    SudekiMpStoryAudioCue cues[SUDEKIMP_STORY_AUDIO_CAPACITY];
} SudekiMpStoryAudio;
BOOL SudekiMpStoryAudioValid(const SudekiMpStoryAudio *state);
BOOL SudekiMpStoryAudioEncode(const SudekiMpStoryAudio *state,uint8_t *bytes,size_t capacity,size_t *written);
BOOL SudekiMpStoryAudioDecode(const uint8_t *bytes,size_t size,SudekiMpStoryAudio *state);
#endif
