#include "cleanroom/audio.h"

#include <mmsystem.h>
#include <stdint.h>
#include <string.h>

enum {
    SAMPLE_RATE = 22050,
    DURATION_MS = 260,
    SAMPLE_COUNT = SAMPLE_RATE * DURATION_MS / 1000,
    WAVE_HEADER_SIZE = 44,
    WAVE_BUFFER_SIZE = WAVE_HEADER_SIZE + SAMPLE_COUNT * 2
};

static uint8_t despawn_wave[WAVE_BUFFER_SIZE];
static BOOL audio_initialized;

static void write_u16(uint8_t *destination, uint16_t value) {
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8);
}

static void write_u32(uint8_t *destination, uint32_t value) {
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8);
    destination[2] = (uint8_t)(value >> 16);
    destination[3] = (uint8_t)(value >> 24);
}

static int triangle_sample(uint32_t phase) {
    uint32_t position = phase >> 16;
    int sample;

    if (position < 16384u) {
        sample = (int)position * 2;
    } else if (position < 49152u) {
        sample = 32767 - (int)(position - 16384u) * 2;
    } else {
        sample = -32767 + (int)(position - 49152u) * 2;
    }
    return sample;
}

BOOL SudekiMpCleanroomAudioInitialize(void) {
    uint32_t main_phase = 0u;
    uint32_t shimmer_phase = 0u;
    unsigned int index;

    if (audio_initialized) {
        return TRUE;
    }
    memcpy(despawn_wave + 0, "RIFF", 4u);
    write_u32(despawn_wave + 4, WAVE_BUFFER_SIZE - 8u);
    memcpy(despawn_wave + 8, "WAVEfmt ", 8u);
    write_u32(despawn_wave + 16, 16u);
    write_u16(despawn_wave + 20, 1u);
    write_u16(despawn_wave + 22, 1u);
    write_u32(despawn_wave + 24, SAMPLE_RATE);
    write_u32(despawn_wave + 28, SAMPLE_RATE * 2u);
    write_u16(despawn_wave + 32, 2u);
    write_u16(despawn_wave + 34, 16u);
    memcpy(despawn_wave + 36, "data", 4u);
    write_u32(despawn_wave + 40, SAMPLE_COUNT * 2u);

    for (index = 0u; index < SAMPLE_COUNT; ++index) {
        unsigned int remaining = SAMPLE_COUNT - index;
        unsigned int frequency = 780u - 520u * index / SAMPLE_COUNT;
        unsigned int shimmer_frequency = 1170u -
            690u * index / SAMPLE_COUNT;
        int main_value;
        int shimmer_value;
        int mixed;

        main_phase += (uint32_t)(((uint64_t)frequency << 32) / SAMPLE_RATE);
        shimmer_phase += (uint32_t)(
            ((uint64_t)shimmer_frequency << 32) / SAMPLE_RATE
        );
        main_value = triangle_sample(main_phase);
        shimmer_value = triangle_sample(shimmer_phase);
        mixed = (main_value * 3 + shimmer_value) / 4;
        mixed = mixed * (int)remaining / SAMPLE_COUNT;
        mixed = mixed * 3 / 5;
        write_u16(
            despawn_wave + WAVE_HEADER_SIZE + index * 2u,
            (uint16_t)(int16_t)mixed
        );
    }
    audio_initialized = TRUE;
    return TRUE;
}

void SudekiMpCleanroomPlayDespawnCue(void) {
    if (SudekiMpCleanroomAudioInitialize()) {
        PlaySoundA(
            (LPCSTR)despawn_wave,
            NULL,
            SND_ASYNC | SND_MEMORY | SND_NODEFAULT
        );
    }
}
