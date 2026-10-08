#include "release_check.h"
#include <string.h>

/* Minimal JSON reader for the release object: finds string/bool values of
 * keys at the top level (depth 1) and inside the "assets" array's objects. */
static const char *skip_string(const char *p, const char *end) {
    for (++p; p < end; ++p) {
        if (*p == '\\') { ++p; continue; }
        if (*p == '"') return p + 1;
    }
    return NULL;
}
/* Copies a JSON string at p (pointing at the opening quote); handles the
 * simple escapes and drops \u sequences (titles stay readable ASCII/UTF-8). */
static int copy_string(const char *p, const char *end, char *out, size_t capacity) {
    size_t n = 0;
    if (p >= end || *p != '"' || !capacity) return 0;
    for (++p; p < end; ++p) {
        char c = *p;
        if (c == '"') { out[n] = 0; return 1; }
        if (c == '\\') {
            if (++p >= end) return 0;
            c = *p;
            if (c == 'u') { p += 4; if (p >= end) return 0; continue; }
            c = c == 'n' ? ' ' : c == 't' ? ' ' : c == 'r' ? ' ' : c;
        }
        if (n + 1 < capacity) out[n++] = c;
    }
    return 0;
}
static const char *skip_ws(const char *p, const char *end) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) ++p;
    return p;
}
/* Scans one object starting at '{'. For each key at this object's depth,
 * calls visit(key, value pointer). Returns the pointer after the object. */
typedef void (*Visit)(void *ctx, const char *key, const char *value, const char *end);
static const char *scan_object(const char *p, const char *end, Visit visit, void *ctx) {
    char key[48];
    if (p >= end || *p != '{') return NULL;
    ++p;
    for (;;) {
        p = skip_ws(p, end);
        if (p >= end) return NULL;
        if (*p == '}') return p + 1;
        if (*p == ',') { ++p; continue; }
        if (*p != '"' || !copy_string(p, end, key, sizeof(key))) return NULL;
        p = skip_string(p, end);
        if (!p) return NULL;
        p = skip_ws(p, end);
        if (p >= end || *p != ':') return NULL;
        p = skip_ws(p + 1, end);
        if (visit) visit(ctx, key, p, end);
        /* Skip the value. */
        if (p < end && *p == '"') { p = skip_string(p, end); if (!p) return NULL; continue; }
        if (p < end && (*p == '{' || *p == '[')) {
            int depth = 0;
            for (; p < end; ++p) {
                if (*p == '"') { p = skip_string(p, end); if (!p) return NULL; --p; continue; }
                if (*p == '{' || *p == '[') ++depth;
                else if ((*p == '}' || *p == ']') && --depth == 0) { ++p; break; }
            }
            if (depth) return NULL;
            continue;
        }
        while (p < end && *p != ',' && *p != '}') ++p;
    }
}
typedef struct Context { SudekiMpRelease *out; int draft, prerelease, saw_draft, saw_prerelease; } Context;
typedef struct AssetContext { char name[128], url[256]; } AssetContext;
static void visit_asset(void *ctx, const char *key, const char *value, const char *end) {
    AssetContext *a = ctx;
    if (!strcmp(key, "name")) copy_string(value, end, a->name, sizeof(a->name));
    else if (!strcmp(key, "browser_download_url")) copy_string(value, end, a->url, sizeof(a->url));
}
static void visit_release(void *ctx, const char *key, const char *value, const char *end) {
    Context *c = ctx;
    if (!strcmp(key, "tag_name")) copy_string(value, end, c->out->tag, sizeof(c->out->tag));
    else if (!strcmp(key, "name")) copy_string(value, end, c->out->title, sizeof(c->out->title));
    else if (!strcmp(key, "html_url")) copy_string(value, end, c->out->page_url, sizeof(c->out->page_url));
    else if (!strcmp(key, "draft")) { c->saw_draft = 1; c->draft = end - value >= 4 && !strncmp(value, "true", 4); }
    else if (!strcmp(key, "prerelease")) { c->saw_prerelease = 1; c->prerelease = end - value >= 4 && !strncmp(value, "true", 4); }
    else if (!strcmp(key, "assets") && value < end && *value == '[') {
        const char *p = value + 1;
        for (;;) {
            p = skip_ws(p, end);
            if (p >= end || *p == ']') break;
            if (*p == ',') { ++p; continue; }
            AssetContext a = {{0}, {0}};
            p = scan_object(p, end, visit_asset, &a);
            if (!p) break;
            if (!c->out->windows_zip_url[0] && !strncmp(a.name, "sudekimp-windows-launcher", 25) &&
                !strncmp(a.url, SUDEKIMP_RELEASE_PAGE_PREFIX, sizeof(SUDEKIMP_RELEASE_PAGE_PREFIX) - 1))
                strcpy(c->out->windows_zip_url, a.url);
        }
    }
}
static int version_valid(const char *v) {
    int digits = 0, parts = 1;
    if (*v == 'v' || *v == 'V') ++v;
    for (; *v; ++v) {
        if (*v >= '0' && *v <= '9') ++digits;
        else if (*v == '.' && digits) { ++parts; digits = 0; }
        else return 0;
    }
    return digits && parts >= 2;
}
int SudekiMpReleaseParse(const char *json, size_t length, SudekiMpRelease *out) {
    Context c;
    const char *end = json + length, *p;
    if (!json || !out) return 0;
    memset(out, 0, sizeof(*out));
    memset(&c, 0, sizeof(c)); c.out = out;
    p = skip_ws(json, end);
    if (!scan_object(p, end, visit_release, &c)) return 0;
    if (!c.saw_draft || !c.saw_prerelease || c.draft || c.prerelease || !version_valid(out->tag) ||
        strncmp(out->page_url, SUDEKIMP_RELEASE_PAGE_PREFIX, sizeof(SUDEKIMP_RELEASE_PAGE_PREFIX) - 1))
        return 0;
    strcpy(out->version, out->tag + ((out->tag[0] == 'v' || out->tag[0] == 'V') ? 1 : 0));
    if (!out->title[0]) strcpy(out->title, out->tag);
    return 1;
}
int SudekiMpVersionCompare(const char *a, const char *b) {
    if (!a || !b || !version_valid(a) || !version_valid(b)) return 0;
    if (*a == 'v' || *a == 'V') ++a;
    if (*b == 'v' || *b == 'V') ++b;
    while (*a || *b) {
        unsigned long x = 0, y = 0;
        while (*a >= '0' && *a <= '9') x = x * 10u + (unsigned long)(*a++ - '0');
        while (*b >= '0' && *b <= '9') y = y * 10u + (unsigned long)(*b++ - '0');
        if (x != y) return x < y ? -1 : 1;
        if (*a == '.') ++a;
        if (*b == '.') ++b;
    }
    return 0;
}
