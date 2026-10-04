#ifndef SUDEKIMP_LAN_STORY_CURVE_H
#define SUDEKIMP_LAN_STORY_CURVE_H

#include <windows.h>

typedef enum SudekiMpLanStoryCurveKind {
    SUDEKIMP_STORY_CURVE_WEIGHTS=0,
    SUDEKIMP_STORY_CURVE_UV_MATRIX=1,
    SUDEKIMP_STORY_CURVE_SCALAR=2,
    SUDEKIMP_STORY_CURVE_COLOR=3
} SudekiMpLanStoryCurveKind;

/* Pure observations only. No hooks, native calls, allocations or object
 * retention. The world adapter owns the counted bank/curve-table identity,
 * attachment target, output extent and current presentation transaction.
 * Initialize verifies the supported image and complete sampler/key-search
 * bodies once at its quiescent native initialization boundary. */
BOOL SudekiMpLanStoryCurveInitialize(HMODULE image);
/* Revalidates class/entry and bounded authored key storage. WEIGHTS requires
 * the variable-float sampler and an exact component count; scalar ATGT
 * targets use SCALAR, UV_MATRIX requires sixteen output floats, and COLOR
 * requires the native four-component float sampler. */
BOOL SudekiMpLanStoryCurveExact(HMODULE image,const void *curve,
    SudekiMpLanStoryCurveKind kind,unsigned output_floats);

#endif
