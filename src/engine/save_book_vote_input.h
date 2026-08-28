#ifndef SUDEKIMP_SAVE_BOOK_VOTE_INPUT_H
#define SUDEKIMP_SAVE_BOOK_VOTE_INPUT_H

#include "input/bridge_protocol.h"

#include <stdint.h>

typedef enum SudekiMpSaveBookVoteInputAction {
    SUDEKIMP_SAVE_BOOK_VOTE_INPUT_NONE = 0,
    SUDEKIMP_SAVE_BOOK_VOTE_INPUT_ARMED,
    SUDEKIMP_SAVE_BOOK_VOTE_INPUT_ACCEPT,
    SUDEKIMP_SAVE_BOOK_VOTE_INPUT_VETO
} SudekiMpSaveBookVoteInputAction;

typedef struct SudekiMpSaveBookVoteInputFence {
    uint32_t serial;
    uint32_t last_sequence;
    uint32_t previous_buttons;
    uint8_t baseline_valid;
    uint8_t consent_armed;
    uint8_t reserved[2];
} SudekiMpSaveBookVoteInputFence;

void SudekiMpSaveBookVoteInputInitialize(
    SudekiMpSaveBookVoteInputFence *fence
);

/* Opening captures the latest raw packet as an epoch boundary. No packet at
 * or before that boundary can arm consent or produce an answer. */
void SudekiMpSaveBookVoteInputBegin(
    SudekiMpSaveBookVoteInputFence *fence,
    uint32_t serial,
    int baseline_valid,
    const SudekiMpInputBridgeState *baseline
);

/* Consent arms only after a strictly newer, fully neutral packet. A/B rising
 * edges are consumed while the overlay is hidden, so a held pre-ACK answer
 * cannot leak into a newly visible prompt. B has deterministic veto priority
 * when both face-button edges arrive together. */
SudekiMpSaveBookVoteInputAction SudekiMpSaveBookVoteInputAdvance(
    SudekiMpSaveBookVoteInputFence *fence,
    uint32_t serial,
    const SudekiMpInputBridgeState *sample,
    int overlay_acknowledged
);

int SudekiMpSaveBookVoteInputIsNeutral(
    const SudekiMpInputBridgeState *sample
);

#endif
