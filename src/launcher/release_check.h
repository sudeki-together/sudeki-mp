#ifndef SUDEKIMP_RELEASE_CHECK_H
#define SUDEKIMP_RELEASE_CHECK_H
#include <stddef.h>
/* Newest published launcher release, read from Gitea's public API
 * (GET /api/v1/repos/sudeki-together/sudeki-mp/releases/latest, which never
 * returns drafts or pre-releases). Pure parsing: no network, no Windows. */
#define SUDEKIMP_RELEASE_PAGE_PREFIX "https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/releases/"
typedef struct SudekiMpRelease {
    char tag[32];        /* e.g. "v0.6.0" */
    char version[32];    /* tag without the leading 'v' */
    char title[128];
    char page_url[256];  /* release page; always under SUDEKIMP_RELEASE_PAGE_PREFIX */
    char windows_zip_url[256]; /* "" when the release has no Windows zip */
} SudekiMpRelease;
/* 1 when the JSON is a published release with a version tag and a release
 * page under the official repository; 0 otherwise (nothing is trusted). */
int SudekiMpReleaseParse(const char *json, size_t length, SudekiMpRelease *out);
/* Dotted numeric compare ("0.10.0" > "0.9.1"); a leading 'v' is ignored.
 * Returns <0, 0 or >0; malformed versions compare as equal (no prompt). */
int SudekiMpVersionCompare(const char *a, const char *b);
#endif
