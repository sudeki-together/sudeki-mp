#include "modding/mod_manifest.h"
#include "engine/texture_mod_index.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)

static int load_text(SudekiMpModManifest *m, const char *text) {
    return SudekiMpModManifestLoad(text, strlen(text), m);
}
static size_t occurrences(const char *text, const char *needle) {
    size_t count = 0, n = strlen(needle);
    while ((text = strstr(text, needle)) != NULL) { ++count; text += n; }
    return count;
}
static void encoded_equals(const SudekiMpModManifest *m, const void *expected, size_t n) {
    uint8_t *raw = NULL;
    size_t size = 0;
    CHECK(SudekiMpModManifestEncode(m, &raw, &size));
    CHECK(size == n);
    if (raw && size == n) CHECK(!memcmp(raw, expected, n));
    free(raw);
}

typedef struct Inspections { int calls, textures, files, diagnostics; size_t last_line; } Inspections;
static int inspect_file(void *context, const char *relative, int texture, uint32_t *width, uint32_t *height) {
    Inspections *i = (Inspections *)context;
    ++i->calls;
    if (texture) ++i->textures; else ++i->files;
    if (strstr(relative, "missing")) return SUDEKIMP_MOD_FILE_MISSING;
    if (strstr(relative, "empty")) return SUDEKIMP_MOD_FILE_EMPTY;
    if (strstr(relative, "invalid")) return SUDEKIMP_MOD_FILE_BAD_IMAGE;
    *width = strstr(relative, "npot") ? 300u : 256u;
    *height = 128u;
    return SUDEKIMP_MOD_FILE_OK;
}
static void diagnostic(void *context, size_t line, int warning, const char *message) {
    Inspections *i = (Inspections *)context;
    ++i->diagnostics; i->last_line = line;
    CHECK(warning == 0 || warning == 1);
    CHECK(message && *message);
}

static void create_and_edit(void) {
    SudekiMpModManifest m = {0};
    SudekiMpModManifestValidation r;
    char value[256];
    uint8_t *raw = NULL;
    size_t size = 0;
    CHECK(SudekiMpModManifestCreate("Synthetic package", &m));
    CHECK(SudekiMpModManifestGetValue(&m, "mOd", "nAmE", value, sizeof(value)));
    CHECK(!strcmp(value, "Synthetic package"));
    CHECK(SudekiMpModManifestValidate(&m, NULL, NULL, NULL, &r));
    CHECK(!r.textures && !r.files && !r.errors && !r.warnings);
    CHECK(SudekiMpModManifestSetTexture(&m, 0x1234u, "textures/synthetic.png"));
    CHECK(SudekiMpModManifestSetFile(&m, "SYNTHETIC.HOM", "files/synthetic.HOM"));
    CHECK(SudekiMpModManifestSetFile(&m, "sound/Speech/test.xwb", "files/sound/test.xwb"));
    CHECK(SudekiMpModManifestSetEnabled(&m, 0));
    CHECK(SudekiMpModManifestGetValue(&m, "Mod", "Enabled", value, sizeof(value)) && !strcmp(value, "false"));
    CHECK(SudekiMpModManifestGetTexture(&m, 0x1234u, value, sizeof(value)) && !strcmp(value, "textures/synthetic.png"));
    CHECK(!SudekiMpModManifestGetTexture(&m, 0x1234u, value, 4));
    CHECK(SudekiMpModManifestGetFile(&m, "synthetic.hom", value, sizeof(value)) && !strcmp(value, "files/synthetic.HOM"));
    CHECK(SudekiMpModManifestGetFile(&m, "SOUND\\speech\\TEST.XWB", value, sizeof(value)) && !strcmp(value, "files/sound/test.xwb"));
    CHECK(SudekiMpModManifestValidate(&m, NULL, NULL, NULL, &r));
    CHECK(r.textures == 1 && r.files == 2);
    CHECK(SudekiMpModManifestEncode(&m, &raw, &size));
    CHECK(size == m.length && raw && raw[0] == ';');
    free(raw);
    CHECK(SudekiMpModManifestSetTexture(&m, 0x1234u, NULL));
    CHECK(!SudekiMpModManifestGetTexture(&m, 0x1234u, value, sizeof(value)));
    CHECK(SudekiMpModManifestSetFile(&m, "synthetic.hom", NULL));
    CHECK(!SudekiMpModManifestGetFile(&m, "SYNTHETIC.HOM", value, sizeof(value)));
    SudekiMpModManifestFree(&m);
    CHECK(!m.text && !m.length && !m.utf16);
}

static void preservation_and_aliases(void) {
    static const char text[] =
        "; original preamble\r\n[Mod]\r\nFormat=SudekiMP.Mod/1.2\r\nName=Original\r\n"
        "Custom=value ; untouched\r\nEnabled=false\r\n; metadata note\r\n"
        "[Textures]\r\n; keep texture comments\r\n0X0000000a=old.dds\r\n"
        "0xa=last.dds\r\n0xB=other.dds\r\n\r\n[Future]\n0xA=leave-me\n"
        "[textures]\n0x000A=third.dds\n[MOD]\nEnabled=true\n[Files]\n"
        "synthetic.hom=old.hom\n; keep file comment\n"
        "sound/Speech/test.xwb=old.xwb\nSOUND\\speech\\TEST.xwb=last.xwb\n";
    SudekiMpModManifest m = {0};
    SudekiMpModManifestValidation r;
    char value[256], alias[16];
    CHECK(load_text(&m, text));
    encoded_equals(&m, text, sizeof(text) - 1u);
    CHECK(SudekiMpModManifestGetTexture(&m, 10, value, sizeof(value)) && !strcmp(value, "third.dds"));
    CHECK(SudekiMpModManifestValidate(&m, NULL, NULL, NULL, &r));
    CHECK(r.textures == 2 && r.files == 2 && r.warnings == 3);
    CHECK(SudekiMpModManifestSetTexture(&m, 10, "textures/new.dds"));
    CHECK(occurrences(m.text, "0x0000000A=textures/new.dds") == 1);
    CHECK(!strstr(m.text, "old.dds") && !strstr(m.text, "last.dds") && !strstr(m.text, "third.dds"));
    CHECK(strstr(m.text, "; keep texture comments\r\n") != NULL);
    CHECK(strstr(m.text, "[Future]\n0xA=leave-me\n") != NULL);
    CHECK(strstr(m.text, "Custom=value ; untouched\r\n") != NULL);
    CHECK(SudekiMpModManifestSetEnabled(&m, 0));
    CHECK(occurrences(m.text, "Enabled=") == 1);
    CHECK(SudekiMpModManifestGetValue(&m, "Mod", "Enabled", value, sizeof(value)) && !strcmp(value, "false"));
    snprintf(alias, sizeof(alias), "0x%08lX", (unsigned long)SudekiMpModResourceKey("synthetic.hom"));
    CHECK(SudekiMpModManifestSetFile(&m, alias, "files/new.hom"));
    CHECK(!strstr(m.text, "synthetic.hom=old.hom"));
    CHECK(SudekiMpModManifestGetFile(&m, "SYNTHETIC.HOM", value, sizeof(value)) && !strcmp(value, "files/new.hom"));
    CHECK(SudekiMpModManifestSetFile(&m, "Sound/SPEECH/Test.xwb", "files/new.xwb"));
    CHECK(!strstr(m.text, "old.xwb") && !strstr(m.text, "last.xwb"));
    CHECK(strstr(m.text, "; keep file comment\n") != NULL);
    CHECK(SudekiMpModManifestSetFile(&m, "synthetic.hom", NULL));
    CHECK(!SudekiMpModManifestGetFile(&m, alias, value, sizeof(value)));
    CHECK(SudekiMpModManifestSetFile(&m, "sound/speech/test.xwb", NULL));
    CHECK(SudekiMpModManifestValidate(&m, NULL, NULL, NULL, &r));
    CHECK(r.textures == 2 && r.files == 0 && r.warnings == 0);
    SudekiMpModManifestFree(&m);
    CHECK(load_text(&m, "[Unknown]\nThing=preserve"));
    CHECK(SudekiMpModManifestSetTexture(&m, 0, "a.dds"));
    CHECK(strstr(m.text, "[Unknown]\nThing=preserve\n\n[Textures]\n0x00000000=a.dds\n"));
    CHECK(SudekiMpModManifestSetFile(&m, "0x0", "a.hom"));
    CHECK(SudekiMpModManifestSetEnabled(&m, 1));
    CHECK(strstr(m.text, "[Mod]\nEnabled=true\n"));
    SudekiMpModManifestFree(&m);
    CHECK(load_text(&m, ""));
    CHECK(SudekiMpModManifestSetTexture(&m, 1, NULL));
    encoded_equals(&m, "", 0);
    SudekiMpModManifestFree(&m);
    CHECK(load_text(&m, "[Textures]\n0x1=valid.dds\n0X0001=../bad.dds\n[Files]\nA.HOM=valid.hom\na.hom=../bad.hom\n"));
    CHECK(SudekiMpModManifestGetTexture(&m, 1, value, sizeof(value)) && !strcmp(value, "valid.dds"));
    CHECK(SudekiMpModManifestGetFile(&m, "a.hom", value, sizeof(value)) && !strcmp(value, "valid.hom"));
    CHECK(SudekiMpModManifestSetTexture(&m, 1, NULL));
    CHECK(SudekiMpModManifestSetFile(&m, "a.hom", NULL));
    CHECK(!strstr(m.text, "bad.dds") && !strstr(m.text, "bad.hom"));
    SudekiMpModManifestFree(&m);
}

static void encodings(void) {
    static const uint8_t utf16[] = {0xff,0xfe,';',0,0xe9,0,' ',0,0x3d,0xd8,0x42,0xde,'\r',0,'\n',0,
        '[',0,'M',0,'o',0,'d',0,']',0,'\n',0};
    static const uint8_t bad_high[] = {0xff,0xfe,0x00,0xd8};
    static const uint8_t bad_low[] = {0xff,0xfe,0x00,0xdc};
    static const uint8_t bad_pair[] = {0xff,0xfe,0x00,0xd8,'x',0};
    static const uint8_t odd[] = {0xff,0xfe,'x'};
    static const uint8_t nul[] = {'a',0,'b'};
    static const uint8_t bom_be[] = {0xfe,0xff,0,'a'};
    SudekiMpModManifest m = {0}, reloaded = {0};
    uint8_t *raw = NULL;
    size_t size = 0;
    char value[100];
    CHECK(SudekiMpModManifestLoad(utf16, sizeof(utf16), &m));
    CHECK(m.utf16 && strstr(m.text, ";\xc3\xa9 \xf0\x9f\x99\x82\r\n"));
    encoded_equals(&m, utf16, sizeof(utf16));
    CHECK(SudekiMpModManifestSetEnabled(&m, 1));
    CHECK(SudekiMpModManifestEncode(&m, &raw, &size));
    CHECK(raw && raw[0] == 0xff && raw[1] == 0xfe);
    CHECK(SudekiMpModManifestLoad(raw, size, &reloaded));
    CHECK(!strcmp(m.text, reloaded.text));
    free(raw); raw = NULL;
    SudekiMpModManifestFree(&m); SudekiMpModManifestFree(&reloaded);
    CHECK(!SudekiMpModManifestLoad(bad_high, sizeof(bad_high), &m));
    CHECK(!SudekiMpModManifestLoad(bad_low, sizeof(bad_low), &m));
    CHECK(!SudekiMpModManifestLoad(bad_pair, sizeof(bad_pair), &m));
    CHECK(!SudekiMpModManifestLoad(odd, sizeof(odd), &m));
    CHECK(!SudekiMpModManifestLoad(nul, sizeof(nul), &m));
    CHECK(!SudekiMpModManifestLoad(bom_be, sizeof(bom_be), &m));
    CHECK(!load_text(&m, "\xc3\xa9")); /* Unmarked UTF-8 is outside the package format. */
    CHECK(!SudekiMpModManifestCreate("bad\nname", &m));
    CHECK(!SudekiMpModManifestCreate("\xed\xa0\x80", &m));
    CHECK(SudekiMpModManifestCreate("Caf\xc3\xa9 \xf0\x9f\x99\x82", &m));
    CHECK(SudekiMpModManifestSetTexture(&m, 1, "textures/caf\xc3\xa9.dds"));
    CHECK(SudekiMpModManifestEncode(&m, &raw, &size));
    CHECK(raw && raw[0] == 0xff && raw[1] == 0xfe);
    CHECK(SudekiMpModManifestLoad(raw, size, &reloaded));
    CHECK(SudekiMpModManifestGetValue(&reloaded, "Mod", "Name", value, sizeof(value)) && !strcmp(value, "Caf\xc3\xa9 \xf0\x9f\x99\x82"));
    CHECK(SudekiMpModManifestGetTexture(&reloaded, 1, value, sizeof(value)) && !strcmp(value, "textures/caf\xc3\xa9.dds"));
    free(raw); SudekiMpModManifestFree(&m); SudekiMpModManifestFree(&reloaded);
}

static void rejected_edits(void) {
    static const char *const bad_paths[] = {"", "../x", "a/../x", "a/./x", "a//x", "/x", "\\x", "C:x", "C:/x", "a/", "a\\", "a\nx", "a?x", "a*x", "a\"x", "a<x", "a>x", "a|x", "a:x", "\xc0\xaf"};
    static const char *const bad_names[] = {"", "NODOT", "a.b.c", "../A.HOM", "textures/a.hom", "sound/../a", "0x", "0x100000000", "a.hom=x", " a.hom", "a.hom ", "a.hom\n", "sound/a=x"};
    SudekiMpModManifest m = {0};
    char *saved;
    CHECK(SudekiMpModManifestCreate("Safety", &m));
    saved = (char *)malloc(m.length + 1u);
    CHECK(saved != NULL);
    if (!saved) { SudekiMpModManifestFree(&m); return; }
    memcpy(saved, m.text, m.length + 1u);
    for (size_t i = 0; i < sizeof(bad_paths) / sizeof(bad_paths[0]); ++i) {
        CHECK(!SudekiMpModManifestSetTexture(&m, 1, bad_paths[i]));
        CHECK(!SudekiMpModManifestSetFile(&m, "a.hom", bad_paths[i]));
        CHECK(!strcmp(m.text, saved));
    }
    for (size_t i = 0; i < sizeof(bad_names) / sizeof(bad_names[0]); ++i) {
        CHECK(!SudekiMpModManifestSetFile(&m, bad_names[i], "files/a.hom"));
        CHECK(!strcmp(m.text, saved));
    }
    free(saved); SudekiMpModManifestFree(&m);
}

static void validation(void) {
    SudekiMpModManifest m = {0};
    SudekiMpModManifestValidation r;
    Inspections i = {0};
    CHECK(load_text(&m, "[Mod]\nFormat=SudekiMP.Mod/1\n[Textures]\n0x1=ok.dds\n0X0001=npot.dds\n"
        "0x2=invalid.dds\n0x3=missing.dds\nBAD=bad.dds\n0x4=../escape\n[Files]\nA.HOM=empty.hom\nB.HOM=ok.hom\n"));
    CHECK(!SudekiMpModManifestValidate(&m, inspect_file, diagnostic, &i, &r));
    CHECK(r.textures == 3 && r.files == 2 && r.errors == 5 && r.warnings == 2);
    CHECK(i.calls == 6 && i.textures == 4 && i.files == 2 && i.diagnostics == 7);
    CHECK(!SudekiMpModManifestValidate(&m, NULL, NULL, NULL, &r));
    CHECK(r.errors == 2 && r.warnings == 1);
    SudekiMpModManifestFree(&m);
    CHECK(load_text(&m, "[Mod]\nFormat=SudekiMP.Mod/2\n[Textures]\n0x1=a.dds\n"));
    CHECK(!SudekiMpModManifestValidate(&m, NULL, NULL, NULL, &r) && r.errors == 1);
    SudekiMpModManifestFree(&m);
    CHECK(load_text(&m, "[Mod]\nName=No format\n[Files]\n0x1=a.hom\n[broken\n"));
    CHECK(!SudekiMpModManifestValidate(&m, NULL, NULL, NULL, &r) && r.errors == 2);
    SudekiMpModManifestFree(&m);
    CHECK(load_text(&m, "[Mod]\nFormat=SudekiMP.Mod/1.9\nFormat=SudekiMP.Mod/1.10\n[Future]\nUnparsed future value\n"));
    CHECK(SudekiMpModManifestValidate(&m, NULL, NULL, NULL, &r) && r.warnings == 1);
    SudekiMpModManifestFree(&m);
}

static void larger_pack(void) {
    SudekiMpModManifest m = {0};
    SudekiMpModManifestValidation r;
    char text[12000], value[100];
    size_t used = (size_t)snprintf(text, sizeof(text), "[Mod]\nFormat=SudekiMP.Mod/1\n[Textures]\n");
    for (unsigned key = 0; key < 256u; ++key)
        used += (size_t)snprintf(text + used, sizeof(text) - used, "0x%X=textures/%u.dds\n", key, key);
    used += (size_t)snprintf(text + used, sizeof(text) - used, "0X00000080=textures/last.dds\n");
    CHECK(used < sizeof(text));
    CHECK(load_text(&m, text));
    CHECK(SudekiMpModManifestValidate(&m, NULL, NULL, NULL, &r));
    CHECK(r.textures == 256u && r.warnings == 1u && !r.errors);
    CHECK(SudekiMpModManifestGetTexture(&m, 128u, value, sizeof(value)) && !strcmp(value, "textures/last.dds"));
    CHECK(SudekiMpModManifestSetTexture(&m, 128u, "textures/replaced.dds"));
    CHECK(SudekiMpModManifestValidate(&m, NULL, NULL, NULL, &r));
    CHECK(r.textures == 256u && !r.warnings && !r.errors);
    SudekiMpModManifestFree(&m);
}

int SudekiMpModManifestTests(void) {
    failures = 0;
    create_and_edit(); preservation_and_aliases(); encodings(); rejected_edits(); validation(); larger_pack();
    return failures;
}
#ifdef SUDEKIMP_MODDING_STANDALONE
int main(void) {
    int result = SudekiMpModManifestTests();
    if (!result) puts("mod manifest tests passed");
    return result ? 1 : 0;
}
#endif
