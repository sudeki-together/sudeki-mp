#include "launcher/mp3_wave.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FRAME_BYTES 417u  /* MPEG1 Layer III, 128 kbps, 44.1 kHz, no padding */
#define FRAME_COUNT 3u

static int failures;

static void check(int condition, const char *name) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", name);
        ++failures;
    }
}

static unsigned long get16(const unsigned char *in) {
    return (unsigned long)in[0] | ((unsigned long)in[1] << 8);
}

static unsigned long get32(const unsigned char *in) {
    return get16(in) | (get16(in + 2) << 16);
}

/* Synthetic frames: valid Layer III headers followed by filler, mono. */
static size_t write_frames(unsigned char *out) {
    size_t frame;
    for (frame = 0u; frame < FRAME_COUNT; ++frame) {
        unsigned char *header = out + frame * FRAME_BYTES;
        memset(header, (int)(0x10u + frame), FRAME_BYTES);
        header[0] = 0xFFu;
        header[1] = 0xFBu;
        header[2] = 0x90u;
        header[3] = 0xC4u;
    }
    return FRAME_BYTES * FRAME_COUNT;
}

static void test_tagged_stream(void) {
    unsigned char input[64u + FRAME_BYTES * FRAME_COUNT + 128u];
    unsigned char *wave = NULL;
    size_t wave_count = 0u;
    size_t offset = 0u;
    size_t frames_count;
    /* ID3v2.4 tag of 20 payload bytes containing a false frame sync. */
    static const unsigned char id3[10] = {'I', 'D', '3', 4, 0, 0x40, 0, 0, 0, 20};
    memcpy(input, id3, sizeof(id3));
    offset = sizeof(id3);
    memset(input + offset, 0, 20u);
    input[offset + 4u] = 0xFFu;
    input[offset + 5u] = 0xFBu;
    input[offset + 6u] = 0x90u;
    input[offset + 7u] = 0xC4u;
    offset += 20u;
    /* Junk with a lone sync whose successor is not a frame. */
    input[offset++] = 0xFFu;
    input[offset++] = 0xFBu;
    input[offset++] = 0x90u;
    input[offset++] = 0xC4u;
    frames_count = write_frames(input + offset);
    offset += frames_count;
    memcpy(input + offset, "TAG", 3u);
    memset(input + offset + 3u, 'x', 125u);
    offset += 128u;

    check(SudekiMpWrapMp3InWave(input, offset, &wave, &wave_count), "tagged stream wraps");
    if (wave == NULL) {
        return;
    }
    check(wave_count == 58u + frames_count + (frames_count & 1u), "wave length");
    check(memcmp(wave, "RIFF", 4u) == 0 && memcmp(wave + 8, "WAVEfmt ", 8u) == 0,
          "riff and fmt markers");
    check(get32(wave + 4) == wave_count - 8u, "riff size");
    check(get32(wave + 16) == 30u, "fmt chunk size");
    check(get16(wave + 20) == 0x55u, "mpeg layer 3 tag");
    check(get16(wave + 22) == 1u, "mono channel count");
    check(get32(wave + 24) == 44100u, "sample rate");
    check(get32(wave + 28) == 16000u, "average bytes per second");
    check(get16(wave + 44) == FRAME_BYTES, "block size");
    check(memcmp(wave + 50, "data", 4u) == 0 && get32(wave + 54) == frames_count,
          "data chunk covers frames only");
    check(memcmp(wave + 58, input + 34u, frames_count) == 0,
          "frames copied unchanged after tag and false sync");
    free(wave);
}

static void test_rejections(void) {
    unsigned char junk[512];
    unsigned char frames[FRAME_BYTES * FRAME_COUNT];
    unsigned char *wave = (unsigned char *)1;
    size_t wave_count = 7u;
    memset(junk, 0x55, sizeof(junk));
    check(!SudekiMpWrapMp3InWave(junk, sizeof(junk), &wave, &wave_count) &&
              wave == NULL && wave_count == 0u,
          "non-mp3 rejected and outputs cleared");
    write_frames(frames);
    frames[2] = 0xF0u;  /* bad bitrate index in every candidate */
    frames[FRAME_BYTES + 2u] = 0xF0u;
    frames[FRAME_BYTES * 2u + 2u] = 0xF0u;
    check(!SudekiMpWrapMp3InWave(frames, sizeof(frames), &wave, &wave_count),
          "invalid bitrate rejected");
    check(!SudekiMpWrapMp3InWave(NULL, 4u, &wave, &wave_count), "null input rejected");
    check(!SudekiMpWrapMp3InWave(frames, 2u, &wave, &wave_count), "short input rejected");
}

int main(void) {
    test_tagged_stream();
    test_rejections();
    if (failures != 0) {
        fprintf(stderr, "%d mp3 wave checks failed\n", failures);
        return 1;
    }
    puts("mp3 wave tests passed");
    return 0;
}
