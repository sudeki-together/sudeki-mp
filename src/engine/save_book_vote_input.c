#include "engine/save_book_vote_input.h"

#include <string.h>

static int sequence_is_newer(uint32_t candidate, uint32_t baseline) {
    const uint32_t delta = candidate - baseline;

    return delta != 0u && delta < UINT32_C(0x80000000);
}

void SudekiMpSaveBookVoteInputInitialize(
    SudekiMpSaveBookVoteInputFence *fence
) {
    if (fence != NULL) {
        memset(fence, 0, sizeof(*fence));
    }
}

void SudekiMpSaveBookVoteInputBegin(
    SudekiMpSaveBookVoteInputFence *fence,
    uint32_t serial,
    int baseline_valid,
    const SudekiMpInputBridgeState *baseline
) {
    if (fence == NULL) {
        return;
    }
    memset(fence, 0, sizeof(*fence));
    fence->serial = serial;
    if (baseline_valid && baseline != NULL) {
        fence->baseline_valid = 1u;
        fence->last_sequence = baseline->sequence;
        fence->previous_buttons = baseline->buttons;
    }
}

int SudekiMpSaveBookVoteInputIsNeutral(
    const SudekiMpInputBridgeState *sample
) {
    return sample != NULL &&
        sample->left_x == 0 && sample->left_y == 0 &&
        sample->right_x == 0 && sample->right_y == 0 &&
        sample->left_trigger <=
            SUDEKIMP_INPUT_BRIDGE_TRIGGER_NEUTRAL_MAXIMUM &&
        sample->right_trigger <=
            SUDEKIMP_INPUT_BRIDGE_TRIGGER_NEUTRAL_MAXIMUM &&
        sample->buttons == 0u;
}

SudekiMpSaveBookVoteInputAction SudekiMpSaveBookVoteInputAdvance(
    SudekiMpSaveBookVoteInputFence *fence,
    uint32_t serial,
    const SudekiMpInputBridgeState *sample,
    int overlay_acknowledged
) {
    uint32_t buttons;
    uint32_t rising;

    if (fence == NULL || sample == NULL || serial == 0u ||
        serial != fence->serial) {
        return SUDEKIMP_SAVE_BOOK_VOTE_INPUT_NONE;
    }
    if (!fence->baseline_valid) {
        fence->baseline_valid = 1u;
        fence->last_sequence = sample->sequence;
        fence->previous_buttons = sample->buttons;
        return SUDEKIMP_SAVE_BOOK_VOTE_INPUT_NONE;
    }
    if (!sequence_is_newer(sample->sequence, fence->last_sequence)) {
        return SUDEKIMP_SAVE_BOOK_VOTE_INPUT_NONE;
    }

    fence->last_sequence = sample->sequence;
    buttons = sample->buttons;
    if (!fence->consent_armed) {
        fence->previous_buttons = buttons;
        if (SudekiMpSaveBookVoteInputIsNeutral(sample)) {
            fence->consent_armed = 1u;
            return SUDEKIMP_SAVE_BOOK_VOTE_INPUT_ARMED;
        }
        return SUDEKIMP_SAVE_BOOK_VOTE_INPUT_NONE;
    }

    rising = buttons & ~fence->previous_buttons;
    fence->previous_buttons = buttons;
    if (!overlay_acknowledged) {
        return SUDEKIMP_SAVE_BOOK_VOTE_INPUT_NONE;
    }
    if ((rising & SUDEKIMP_BRIDGE_BUTTON_B) != 0u) {
        return SUDEKIMP_SAVE_BOOK_VOTE_INPUT_VETO;
    }
    if ((rising & SUDEKIMP_BRIDGE_BUTTON_A) != 0u) {
        return SUDEKIMP_SAVE_BOOK_VOTE_INPUT_ACCEPT;
    }
    return SUDEKIMP_SAVE_BOOK_VOTE_INPUT_NONE;
}
