#ifndef SUDEKIMP_LAN_STORY_SNAPSHOT_H
#define SUDEKIMP_LAN_STORY_SNAPSHOT_H

#include "hooks/lan_story_observer.h"
#include "hooks/lan_story_cinematic.h"
#include "network/lan_story_frame.h"
#include "network/lan_party_session.h"

typedef struct SudekiMpLanStoryCapture {
    uint32_t epoch, revision, sequence, last_tick;
    uint32_t generation[4];
    void *actors[4];
    SudekiMpLanArenaLocomotion motion[4];
    SudekiMpLanStoryCinematicCapture cinematic;
    SudekiMpLanStorySpeechCapture speech;
} SudekiMpLanStoryCapture;

/* Context belongs to one host session. Reset only after that session retires;
 * generations/sequence do not wrap within a session. No native state changed. */
void SudekiMpLanStoryCaptureReset(SudekiMpLanStoryCapture *capture);
BOOL SudekiMpLanStoryCaptureMovement(SudekiMpLanStoryCapture *capture,
    SudekiMpLanPartySession *session,void *controller,
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanStoryScene *scene,BOOL native_poses,uint32_t now,SudekiMpLanStoryFrame *frame);
BOOL SudekiMpLanStoryCapturePresentation(SudekiMpLanStoryCapture *capture,
    void *controller,const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanStoryScene *scene,const SudekiMpLanStoryFrame *party,
    SudekiMpLanStoryPresentation *presentation);

#endif
