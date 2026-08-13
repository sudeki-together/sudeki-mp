#include "hooks/pattern_scan.h"

SudekiMpPatternResult SudekiMpFindPattern(
    const uint8_t *base,
    size_t size,
    const uint8_t *pattern,
    const char *mask,
    size_t pattern_size
) {
    SudekiMpPatternResult result = {0};
    size_t offset;
    size_t index;

    if (base == NULL || pattern == NULL || mask == NULL ||
        pattern_size == 0 || size < pattern_size) {
        return result;
    }

    for (offset = 0; offset <= size - pattern_size; ++offset) {
        for (index = 0; index < pattern_size; ++index) {
            if (mask[index] == 'x' && base[offset + index] != pattern[index]) {
                break;
            }
        }
        if (index == pattern_size) {
            if (result.address == NULL) {
                result.address = base + offset;
            }
            ++result.match_count;
        }
    }

    return result;
}
