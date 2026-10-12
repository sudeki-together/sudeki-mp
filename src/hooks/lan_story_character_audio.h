#ifndef SUDEKIMP_LAN_STORY_CHARACTER_AUDIO_H
#define SUDEKIMP_LAN_STORY_CHARACTER_AUDIO_H
#include "hooks/lan_story_observer.h"
#include "network/lan_story_audio.h"
typedef BOOL (*SudekiMpStoryCharacterAudioExact)(void *context);
BOOL SudekiMpStoryCharacterAudioInitialize(HMODULE image,BOOL host);
void SudekiMpStoryCharacterAudioClose(void);
void SudekiMpStoryCharacterAudioBind(const SudekiMpLanStoryNativeRoster *roster);
BOOL SudekiMpStoryCharacterAudioCapture(unsigned character,SudekiMpStoryAudio *state);
BOOL SudekiMpStoryCharacterAudioPresent(const SudekiMpLanStoryNativeRoster *roster,const SudekiMpStoryAudio *state,
    SudekiMpStoryCharacterAudioExact exact,void *context);
BOOL SudekiMpStoryCharacterAudioStop(unsigned character,SudekiMpStoryCharacterAudioExact exact,void *context);
BOOL SudekiMpStoryCharacterAudioRetains(void);
/* Caller proves a retained parent-free FX sound component inside the exact
 * client residency operation. Uses the native bank setter/refcount lifetime. */
BOOL SudekiMpStoryCharacterAudioBindEffect(const SudekiMpLanStoryNativeRoster *roster,
    unsigned character,void *sound);
#endif
