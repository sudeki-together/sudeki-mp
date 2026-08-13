#ifndef SUDEKIMP_PATTERN_SCAN_H
#define SUDEKIMP_PATTERN_SCAN_H

#include <stddef.h>
#include <stdint.h>

typedef struct SudekiMpPatternResult {
    const uint8_t *address;
    size_t match_count;
} SudekiMpPatternResult;

SudekiMpPatternResult SudekiMpFindPattern(
    const uint8_t *base,
    size_t size,
    const uint8_t *pattern,
    const char *mask,
    size_t pattern_size
);

#endif
