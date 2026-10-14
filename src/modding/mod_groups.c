#include "mod_groups.h"
#include <stdlib.h>
#include <string.h>

static unsigned char lower(unsigned char c) {
    return c >= 'A' && c <= 'Z' ? (unsigned char)(c + 'a' - 'A') : c;
}
static int equal(const char *a, const char *b) {
    while (*a && *b && lower((unsigned char)*a) == lower((unsigned char)*b)) { ++a; ++b; }
    return !*a && !*b;
}
int SudekiMpModGroupPatternMatch(const char *pattern, const char *name) {
    const char *star = NULL, *retry = NULL;
    if (!pattern || !name) return 0;
    while (*name) {
        if (*pattern == '?' || (*pattern && lower((unsigned char)*pattern) == lower((unsigned char)*name))) {
            ++pattern; ++name;
        } else if (*pattern == '*') {
            star = pattern++; retry = name;
        } else if (star) {
            pattern = star + 1; name = ++retry;
        } else return 0;
    }
    while (*pattern == '*') ++pattern;
    return !*pattern;
}
static char *trim(char *s) {
    char *end;
    while (*s == ' ' || *s == '\t') ++s;
    end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t')) *--end = 0;
    return s;
}
static int group_section(char *section, unsigned *character, unsigned *category) {
    static const char *characters[] = { "Tal", "Ailish", "Buki", "Elco", "World" };
    static const char *categories[] = { "Weapons", "Armour", "BodyFace", "OtherTextures", "Models" };
    char *dot = strchr(section, '.');
    unsigned i, j;
    if (!dot) return 0;
    *dot++ = 0;
    for (i = 0; i < 5; ++i) if (equal(trim(section), characters[i])) break;
    for (j = 0; j < 5; ++j) if (equal(trim(dot), categories[j])) break;
    if (i == 5 || j == 5) return 0;
    *character = i; *category = j;
    return 1;
}
int SudekiMpModGroupsLoad(SudekiMpModGroups *groups, const void *data, size_t size) {
    SudekiMpModGroups parsed = {0};
    const unsigned char *bytes = data;
    size_t cursor = 0;
    unsigned character = 4, category = 3;
    int active = 0;
    if (!groups || (!data && size)) return 0;
    while (cursor < size) {
        char line[512], *s, *eq;
        size_t n = 0;
        while (cursor < size && bytes[cursor] != '\n' && bytes[cursor] != '\r') {
            if (bytes[cursor] == 0 || bytes[cursor] >= 128 || n + 1 >= sizeof(line)) goto fail;
            line[n++] = (char)bytes[cursor++];
        }
        while (cursor < size && (bytes[cursor] == '\n' || bytes[cursor] == '\r')) ++cursor;
        line[n] = 0; s = trim(line);
        if (!*s || *s == ';' || *s == '#') continue;
        n = strlen(s);
        if (*s == '[') {
            if (n < 3 || s[n - 1] != ']') goto fail;
            s[n - 1] = 0;
            active = group_section(s + 1, &character, &category);
        } else if (active) {
            SudekiMpModGroupRule *grown;
            size_t i;
            eq = strchr(s, '=');
            if (!eq) goto fail;
            *eq++ = 0; s = trim(s); eq = trim(eq);
            if (strlen(s) < 7 || lower((unsigned char)s[0]) != 'p' ||
                lower((unsigned char)s[1]) != 'a' || lower((unsigned char)s[2]) != 't' ||
                lower((unsigned char)s[3]) != 't' || lower((unsigned char)s[4]) != 'e' ||
                lower((unsigned char)s[5]) != 'r' || lower((unsigned char)s[6]) != 'n') continue;
            for (i = 7; s[i]; ++i) if (s[i] < '0' || s[i] > '9') goto fail;
            if (!*eq || strlen(eq) >= sizeof(parsed.rules[0].pattern)) goto fail;
            if (parsed.count >= 4096) goto fail;
            grown = realloc(parsed.rules, (parsed.count + 1) * sizeof(*grown));
            if (!grown) goto fail;
            parsed.rules = grown;
            grown[parsed.count].character = character;
            grown[parsed.count].category = category;
            strcpy(grown[parsed.count++].pattern, eq);
        }
    }
    SudekiMpModGroupsFree(groups); *groups = parsed;
    return 1;
fail:
    SudekiMpModGroupsFree(&parsed);
    return 0;
}
void SudekiMpModGroupsFree(SudekiMpModGroups *groups) {
    if (groups) { free(groups->rules); memset(groups, 0, sizeof(*groups)); }
}
void SudekiMpModGroupsClassify(const SudekiMpModGroups *groups, const char *name,
                              int model, unsigned *character, unsigned *category) {
    size_t i;
    *character = SUDEKIMP_MOD_WORLD;
    *category = model ? SUDEKIMP_MOD_MODELS : SUDEKIMP_MOD_OTHER_TEXTURES;
    if (!groups || !name || !*name) return;
    for (i = 0; i < groups->count; ++i) {
        const SudekiMpModGroupRule *r = &groups->rules[i];
        if (!model && r->category == SUDEKIMP_MOD_MODELS) continue;
        if (SudekiMpModGroupPatternMatch(r->pattern, name)) {
            *character = r->character;
            *category = model ? SUDEKIMP_MOD_MODELS : r->category;
            return;
        }
    }
}
