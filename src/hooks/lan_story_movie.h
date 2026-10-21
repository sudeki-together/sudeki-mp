#ifndef SUDEKIMP_LAN_STORY_MOVIE_H
#define SUDEKIMP_LAN_STORY_MOVIE_H
#include "network/lan_party_session.h"
BOOL SudekiMpLanStoryMovieInstall(HMODULE image,SudekiMpLanPartySession *session);
BOOL SudekiMpLanStoryMovieUninstall(void);
void SudekiMpLanStoryMovieClose(void);
/* Call only from the runtime's verified controller post-update dispatch.
 * prepare uses a fresh scoped controller witness to restore borrowed views
 * and close input. It must finish its containment scope before playback. */
BOOL SudekiMpLanStoryMovieService(BOOL ready,BOOL (*prepare)(void));
BOOL SudekiMpLanStoryMovieImageMatches(HMODULE image);
#endif
