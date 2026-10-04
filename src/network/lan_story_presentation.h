#ifndef SUDEKIMP_LAN_STORY_PRESENTATION_H
#define SUDEKIMP_LAN_STORY_PRESENTATION_H

#include "network/lan_story_frame.h"

/* Native speech parser accepts 1024 UTF-16 code units. No text is truncated.
 * Cue is an asset stem, never a filesystem path or a native object identity. */
#define SUDEKIMP_STORY_SUBTITLE_UNITS 1024u
#define SUDEKIMP_STORY_SPEAKER_UNITS 64u
#define SUDEKIMP_STORY_CUE_BYTES 260u
#define SUDEKIMP_STORY_PRESENTATION_HEADER_SIZE 56u
#define SUDEKIMP_STORY_PRESENTATION_MAX_SIZE (56u+2u*1024u+2u*64u+260u)
#define SUDEKIMP_STORY_PRESENTATION_CHUNK_BYTES 1120u
#define SUDEKIMP_STORY_PRESENTATION_CHUNK_HEADER 36u
#define SUDEKIMP_STORY_PRESENTATION_MAX_CHUNKS 3u
#define SUDEKIMP_STORY_PRESENTATION_CHUNK_MAX_SIZE (36u+1120u)
enum {
    SUDEKIMP_STORY_DIALOGUE_ACTIVE=1u,
    SUDEKIMP_STORY_LINE_ACTIVE=2u,
    SUDEKIMP_STORY_SPEAKER_PRESENT=4u,
    SUDEKIMP_STORY_CUE_PRESENT=8u,
    SUDEKIMP_STORY_LETTERBOX_PRESENT=16u
};
typedef struct SudekiMpLanStoryPresentation {
    uint32_t epoch,revision,host_tick,sequence;
    uint32_t conversation_serial,line_serial;
    uint32_t elapsed_ms,duration_ms;
    uint8_t flags;
    /* Observed native visible bar extent, normalized per viewport edge.
     * Zero unless LETTERBOX_PRESENT; unknown geometry is not fabricated. */
    float letterbox,letterbox_bottom;
    uint16_t subtitle[SUDEKIMP_STORY_SUBTITLE_UNITS+1u];
    uint16_t speaker[SUDEKIMP_STORY_SPEAKER_UNITS+1u];
    char cue[SUDEKIMP_STORY_CUE_BYTES+1u];
} SudekiMpLanStoryPresentation;
typedef struct SudekiMpLanStoryPresentationChunk {
    uint32_t epoch,revision,host_tick,sequence,digest;
    uint16_t total_size,offset,size;
    uint8_t index,chunks;
    uint8_t bytes[SUDEKIMP_STORY_PRESENTATION_CHUNK_BYTES];
} SudekiMpLanStoryPresentationChunk;

BOOL SudekiMpLanStoryPresentationCueValid(const char *cue);
BOOL SudekiMpLanStoryPresentationValid(const SudekiMpLanStoryPresentation *frame);
BOOL SudekiMpLanStoryPresentationMatches(const SudekiMpLanStoryPresentation *frame,
    const SudekiMpLanStoryFrame *party);
BOOL SudekiMpLanStoryPresentationEncode(const SudekiMpLanStoryPresentation *frame,
    uint8_t *bytes,size_t capacity,size_t *written);
BOOL SudekiMpLanStoryPresentationDecode(const uint8_t *bytes,size_t size,
    SudekiMpLanStoryPresentation *frame);
unsigned SudekiMpLanStoryPresentationChunkCount(const SudekiMpLanStoryPresentation *frame);
BOOL SudekiMpLanStoryPresentationChunkEncode(const SudekiMpLanStoryPresentation *frame,
    unsigned index,uint8_t *bytes,size_t capacity,size_t *written);
BOOL SudekiMpLanStoryPresentationChunkDecode(const uint8_t *bytes,size_t size,
    SudekiMpLanStoryPresentationChunk *chunk);
/* Caller admits only authenticated same-session chunks. This operation also
 * proves complete unique coverage, identical identity/shape/digest, and exact
 * whole-record decoding. Digest detects mixed fragments; it is not security. */
BOOL SudekiMpLanStoryPresentationAssemble(const SudekiMpLanStoryPresentationChunk *chunks,
    unsigned count,SudekiMpLanStoryPresentation *frame);

#endif
