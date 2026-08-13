#ifndef SUDEKIMP_BUILD_IDENTITY_H
#define SUDEKIMP_BUILD_IDENTITY_H

#include <windows.h>

#define SUDEKIMP_EXPECTED_SHA256 \
    "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94"

#define SUDEKIMP_EXPECTED_IMAGE_SIZE 0x0045f000u
#define SUDEKIMP_EXPECTED_TIMESTAMP 0x534d1533u

typedef struct SudekiMpBuildCheck {
    char actual_sha256[65];
    BOOL hash_matches;
    BOOL pe_matches;
} SudekiMpBuildCheck;

BOOL SudekiMpCheckExecutableFile(
    const wchar_t *path,
    SudekiMpBuildCheck *result
);

BOOL SudekiMpCheckLoadedExecutable(HMODULE module);

#endif
