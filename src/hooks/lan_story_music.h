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
/* Client, native thread, every frame: inside = this client holds a local
 * house entry (H1b). Mirrors the interior door scripts: entering fades the
 * score to 0 (remembering whether it played), leaving fades it back to 1
 * when it had been playing. Converges; FALSE = not applied yet (retried). */
BOOL SudekiMpLanStoryMusicInterior(BOOL inside,const SudekiMpControlUpdateDispatchWitness *witness);
/* Runtime teardown, native thread: restore a score this module muted.
 * FALSE (logged) when the owner cannot be proved; the state is kept. */
BOOL SudekiMpLanStoryMusicInteriorReset(void);
#endif
