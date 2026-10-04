#ifndef SUDEKIMP_LAN_STORY_CINEMATIC_H
#define SUDEKIMP_LAN_STORY_CINEMATIC_H

#include "network/lan_story_frame.h"
#include "network/lan_story_presentation.h"

#define SUDEKIMP_STORY_CAMERA_NAME_SIZE 21u

/* Host-session observation only. These borrowed identities are compared to
 * freshly resolved native owners; they are never dereferenced from this
 * record or transmitted to another process. Reset only with the host session. */
typedef struct SudekiMpLanStoryCinematicCapture {
    uint32_t epoch,camera_serial;
    DWORD native_thread;
    void *manager,*camera,*render_state;
    char camera_name[SUDEKIMP_STORY_CAMERA_NAME_SIZE];
    BOOL continuous;
} SudekiMpLanStoryCinematicCapture;
typedef BOOL (*SudekiMpLanStoryCinematicExact)(void *context);

/* Read the actually selected registered native camera, including cinematic
 * cameras, without selecting a camera or invoking a native update/script.
 * The caller must prove its current host native observer scope before/after.
 * serial changes for observed owner/name/epoch changes or an observation gap;
 * it is not an authored cut marker for jumps within one camera animation.
 * Outputs are plain data; no protocol layout or client authority is implied. */
BOOL SudekiMpLanStoryCinematicCaptureView(SudekiMpLanStoryCinematicCapture *capture,
    uint32_t epoch,SudekiMpLanStoryView *view,uint32_t *serial,
    char name[SUDEKIMP_STORY_CAMERA_NAME_SIZE],
    SudekiMpLanStoryCinematicExact exact,void *context);

typedef struct SudekiMpLanStorySpeechCapture {
    DWORD native_thread;
    uint32_t epoch,conversation_serial,line_serial;
    void *speech;
    const void *text;
    BOOL continuous,dialogue_active,line_active;
    BOOL trace_known;
    unsigned trace_count,trace_flags;
    SudekiMpLanStoryPresentation previous;
} SudekiMpLanStorySpeechCapture;
/* Read-only current-line observation. A gap is unknown, never an invented
 * stop. Serials identify observed continuity, not native script event IDs.
 * Caller uses the same exact host roster scope as camera/party capture. */
BOOL SudekiMpLanStoryCinematicCaptureSpeech(SudekiMpLanStorySpeechCapture *capture,
    const SudekiMpLanStoryFrame *party,SudekiMpLanStoryPresentation *presentation,
    SudekiMpLanStoryCinematicExact exact,void *context);

/* Client-only cosmetic audio. Fresh retained-world/thread authority is a
 * caller obligation at every native call. One separately owned native voice;
 * native CSpeech state and voice handle are never changed. New late lines are
 * explicitly refused when elapsed >250 ms because native seek is unproven. */
BOOL SudekiMpLanStoryCinematicAudioPresent(const SudekiMpLanStoryPresentation *frame,
    SudekiMpLanStoryCinematicExact exact,void *context);
BOOL SudekiMpLanStoryCinematicAudioStop(SudekiMpLanStoryCinematicExact exact,void *context);
BOOL SudekiMpLanStoryCinematicAudioRetains(void);
/* No borrowed native object is forgotten while an owned voice remains. */
BOOL SudekiMpLanStoryCinematicAudioReset(void);

#endif
