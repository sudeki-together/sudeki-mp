#ifndef SUDEKIMP_LAN_STORY_MUSIC_H
#define SUDEKIMP_LAN_STORY_MUSIC_H
#include "hooks/control_separation.h"
#include "network/lan_story_effect_state.h"
/* Saved-story audio presentation only. No hooks, music assets, global combat
 * writes or owned native tasks. Original battle-score update renders the mix. */
BOOL SudekiMpLanStoryMusicInitialize(HMODULE image);
void SudekiMpLanStoryMusicCapture(SudekiMpStoryEffectState *out,
    const SudekiMpControlUpdateDispatchWitness *witness);
BOOL SudekiMpLanStoryMusicFollow(const SudekiMpStoryEffectState *host,
    const SudekiMpControlUpdateDispatchWitness *witness);
#endif
