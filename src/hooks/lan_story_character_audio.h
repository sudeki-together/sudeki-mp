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
/* Client, native thread, inside the caller's exact residency operation: play
 * a 2D spot cue (an interior door sound) once through the spot emitter's
 * guarded native play, as CSound::PlayCue does. TRUE when a cue started. */
BOOL SudekiMpStoryCharacterAudioPlaySpot(const char *cue);
/* Client shot audio (ranged weapons and Kurian Shooter missiles): the
 * planner's actions played on module-owned emitter records with the same
 * native routines CMissile uses (EmitterBankSet, EmitterPlay, attachment
 * position/release). Native thread, inside the client Present callback.
 * Failure means silence; nothing is ever force-stopped without a proof. */
#include "network/lan_story_shot_audio.h"
BOOL SudekiMpStoryShotAudioPresent(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpStoryShotAudioInput *input,SudekiMpStoryCharacterAudioExact exact,void *context);
/* A record needs a service pass (the Present callback has not run for 250 ms). */
BOOL SudekiMpStoryShotAudioNeedsService(uint32_t now);
/* Stop travel cues, let fire cues finish, release finished records. */
BOOL SudekiMpStoryShotAudioDrain(SudekiMpStoryCharacterAudioExact exact,void *context);
/* Stop everything and release; TRUE only when no retryable record remains. */
BOOL SudekiMpStoryShotAudioStop(SudekiMpStoryCharacterAudioExact exact,void *context);
/* Any record that still needs cleanup (quarantined records excluded). */
BOOL SudekiMpStoryShotAudioRetains(void);
#endif
