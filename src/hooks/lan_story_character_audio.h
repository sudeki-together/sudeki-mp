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
/* Client: the character this game positively controls, or 4 while unproven.
 * Present uses it only to decide which replayed cues are another player's. */
void SudekiMpStoryCharacterAudioSetLocalCharacter(unsigned character);
/* Another player's reload is not this listener's business (owner 2026-10-18:
 * Tal need not hear Ailish or Elco reload). Reload and recharge cues in the
 * ranged characters' banks: "reload", "ailish_reload_N", "Elco_reload_N" and
 * "<weapon>_recharge" (ALICESoundScript.xsb, ELCOSoundScript.xsb). The own
 * character keeps them; an unproven own character counts as another. Pure. */
static inline int SudekiMpStoryAudioSilentForListener(unsigned character,unsigned local_character,
    const char *name) {
    if(!name || (character<4u && character==local_character)) return 0;
    size_t n=0; while(name[n]) ++n;
    for(size_t i=0;i+6u<=n;++i) {
        const char *r="reload"; size_t k=0;
        while(k<6u && (name[i+k]|0x20)==r[k]) ++k;
        if(k==6u) return 1;
    }
    const char *suffix="recharge"; size_t s=8u;
    if(n<s+1u || name[n-s-1u]!='_') return 0;
    for(size_t k=0;k<s;++k) if((name[n-s+k]|0x20)!=suffix[k]) return 0;
    return 1;
}
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
