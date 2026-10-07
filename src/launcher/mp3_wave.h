#ifndef SUDEKIMP_MP3_WAVE_H
#define SUDEKIMP_MP3_WAVE_H

#include <stddef.h>

/*
 * Wraps MPEG Layer III audio in a RIFF/WAVE container (WAVE_FORMAT_MPEGLAYER3)
 * so the waveaudio MCI device can play it through the ACM MP3 codec. Wine's
 * mpegvideo (DirectShow) path cannot open the launcher track, but its wave
 * mapper decodes MP3 with the built-in codec. ID3v2/ID3v1 tags are dropped;
 * the frame data is copied unchanged. The caller frees *wave with free().
 */
int SudekiMpWrapMp3InWave(const unsigned char *mp3,
                          size_t mp3_count,
                          unsigned char **wave,
                          size_t *wave_count);

#endif
