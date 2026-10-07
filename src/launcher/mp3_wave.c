#include "mp3_wave.h"

#include <stdlib.h>
#include <string.h>

/* RIFF header (12) + fmt chunk header (8) + MPEGLAYER3WAVEFORMAT (30)
   + data chunk header (8). */
#define WAVE_HEADER_BYTES 58u
#define MPEGLAYER3_FORMAT_BYTES 30u

typedef struct FrameInfo {
    unsigned long sample_rate;
    unsigned long bitrate_kbps;
    unsigned int channels;
    unsigned int block_size;
} FrameInfo;

static void put16(unsigned char *out, unsigned long value) {
    out[0] = (unsigned char)(value & 0xFFu);
    out[1] = (unsigned char)((value >> 8) & 0xFFu);
}

static void put32(unsigned char *out, unsigned long value) {
    put16(out, value & 0xFFFFu);
    put16(out + 2, (value >> 16) & 0xFFFFu);
}

static int parse_frame(const unsigned char *header, FrameInfo *info) {
    static const unsigned short mpeg1_bitrates[16] = {
        0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0
    };
    static const unsigned short mpeg2_bitrates[16] = {
        0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0
    };
    static const unsigned long mpeg1_rates[3] = {44100u, 48000u, 32000u};
    unsigned int version;
    unsigned int bitrate_index;
    unsigned int rate_index;
    if (header[0] != 0xFFu || (header[1] & 0xE0u) != 0xE0u) {
        return 0;
    }
    version = (header[1] >> 3) & 0x03u;          /* 3 = MPEG1, 2 = MPEG2, 0 = 2.5 */
    if (version == 1u || ((header[1] >> 1) & 0x03u) != 1u) {  /* Layer III only */
        return 0;
    }
    bitrate_index = (header[2] >> 4) & 0x0Fu;
    rate_index = (header[2] >> 2) & 0x03u;
    if (bitrate_index == 0u || bitrate_index == 15u || rate_index == 3u) {
        return 0;
    }
    info->sample_rate = mpeg1_rates[rate_index] >> (version == 3u ? 0 : version == 2u ? 1 : 2);
    info->bitrate_kbps = version == 3u ? mpeg1_bitrates[bitrate_index] :
                                         mpeg2_bitrates[bitrate_index];
    info->channels = ((header[3] >> 6) & 0x03u) == 3u ? 1u : 2u;
    info->block_size = (unsigned int)((version == 3u ? 144000ul : 72000ul) *
                                      info->bitrate_kbps / info->sample_rate);
    return 1;
}

int SudekiMpWrapMp3InWave(const unsigned char *mp3,
                          size_t mp3_count,
                          unsigned char **wave,
                          size_t *wave_count) {
    size_t start = 0u;
    size_t end = mp3_count;
    size_t data_count;
    size_t padded;
    FrameInfo info;
    unsigned char *out;
    if (mp3 == NULL || wave == NULL || wave_count == NULL) {
        return 0;
    }
    *wave = NULL;
    *wave_count = 0u;
    if (mp3_count >= 10u && memcmp(mp3, "ID3", 3u) == 0) {
        const size_t tag = ((size_t)(mp3[6] & 0x7Fu) << 21) |
                           ((size_t)(mp3[7] & 0x7Fu) << 14) |
                           ((size_t)(mp3[8] & 0x7Fu) << 7) | (size_t)(mp3[9] & 0x7Fu);
        start = 10u + tag + ((mp3[5] & 0x10u) != 0u ? 10u : 0u);
    }
    if (end >= 128u && end - 128u >= start && memcmp(mp3 + end - 128u, "TAG", 3u) == 0) {
        end -= 128u;
    }
    /* Find the first frame whose successor (when present) is also a frame. */
    while (start + 4u <= end) {
        if (parse_frame(mp3 + start, &info)) {
            const size_t next = start + info.block_size +
                                ((mp3[start + 2u] >> 1) & 0x01u);
            FrameInfo following;
            if (next + 4u > end || parse_frame(mp3 + next, &following)) {
                break;
            }
        }
        ++start;
    }
    if (start + 4u > end || !parse_frame(mp3 + start, &info)) {
        return 0;
    }
    data_count = end - start;
    if (data_count > 0x7FFFFFFFu - WAVE_HEADER_BYTES) {
        return 0;
    }
    padded = data_count + (data_count & 1u);
    out = (unsigned char *)malloc(WAVE_HEADER_BYTES + padded);
    if (out == NULL) {
        return 0;
    }
    memcpy(out, "RIFF", 4u);
    put32(out + 4, (unsigned long)(WAVE_HEADER_BYTES - 8u + padded));
    memcpy(out + 8, "WAVEfmt ", 8u);
    put32(out + 16, MPEGLAYER3_FORMAT_BYTES);
    put16(out + 20, 0x0055u);                      /* WAVE_FORMAT_MPEGLAYER3 */
    put16(out + 22, info.channels);
    put32(out + 24, info.sample_rate);
    put32(out + 28, info.bitrate_kbps * 1000ul / 8ul);
    put16(out + 32, 1u);                           /* nBlockAlign */
    put16(out + 34, 0u);                           /* wBitsPerSample */
    put16(out + 36, 12u);                          /* cbSize */
    put16(out + 38, 1u);                           /* MPEGLAYER3_ID_MPEG */
    put32(out + 40, 2u);                           /* MPEGLAYER3_FLAG_PADDING_OFF */
    put16(out + 44, info.block_size);
    put16(out + 46, 1u);                           /* nFramesPerBlock */
    put16(out + 48, 0u);                           /* nCodecDelay */
    memcpy(out + 50, "data", 4u);
    put32(out + 54, (unsigned long)data_count);
    memcpy(out + WAVE_HEADER_BYTES, mp3 + start, data_count);
    if (padded != data_count) {
        out[WAVE_HEADER_BYTES + data_count] = 0u;
    }
    *wave = out;
    *wave_count = WAVE_HEADER_BYTES + padded;
    return 1;
}
