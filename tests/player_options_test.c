#include "launcher/player_options.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Synthetic fixture in the shape of Sudeki's launcher option document. It is
   written for this test; no game-installed file is reproduced. */
static const char fixture[] =
    "<launcher_options>\r\n"
    "\t<!-- <setting id='Commented'><Variant id='Value' type='Bool' value='True' /></setting> -->\r\n"
    "\t<settings id='NotASetting'><Variant id='Value' type='Bool' value='True' /></settings>\r\n"
    "\t<setting id='Resolution'>\r\n"
    "\t\t<Variant id='Width' type='Integer' value='1366' />\r\n"
    "\t\t<Variant id='Height' type='Integer' value='768' />\r\n"
    "\t</setting>\r\n"
    "\t<setting id='FullScreen'>\r\n"
    "\t\t<Variant id='Value' type='Bool' value='False' />\r\n"
    "\t</setting>\r\n"
    "\t<setting id='Gamma'>\r\n"
    "\t\t<Variant id='Value' type='Float' value='1.000000' />\r\n"
    "\t</setting>\r\n"
    "\t<setting id=\"AudioQuality\">\r\n"
    "\t\t<Variant type=\"String\" id=\"Value\" value=\"High\" />\r\n"
    "\t</setting>\r\n"
    "\t<setting id='Forward'>\r\n"
    "\t\t<Variant id='ControlName0' type='String' value='ac_CharacterMoveForwards' />\r\n"
    "\t\t<Variant id='Key' type='Integer' value='200' />\r\n"
    "\t</setting>\r\n"
    "\t<setting id='Mystery'>\r\n"
    "\t\t<Variant id='Value' type='Matrix' value='1' />\r\n"
    "\t</setting>\r\n"
    "\t<setting id='Empty' />\r\n"
    "</launcher_options>\r\n";

static int failures;

static void check(int condition, const char *name) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", name);
        ++failures;
    }
}

static unsigned char *utf16le_from_ascii(const char *text, int bom, size_t *count) {
    const size_t length = strlen(text);
    unsigned char *bytes = (unsigned char *)malloc(length * 2u + 2u);
    size_t written = 0u;
    size_t index;
    if (bom) {
        bytes[written++] = 0xFFu;
        bytes[written++] = 0xFEu;
    }
    for (index = 0u; index < length; ++index) {
        bytes[written++] = (unsigned char)text[index];
        bytes[written++] = 0u;
    }
    *count = written;
    return bytes;
}

static void count_setting(const char *setting_id, void *context) {
    size_t *count = (size_t *)context;
    (void)setting_id;
    ++*count;
}

static void collect_setting(const char *setting_id, void *context) {
    char *joined = (char *)context;
    strcat(joined, setting_id);
    strcat(joined, ",");
}

static int load_text(SudekiMpPlayerOptions *options, const char *text) {
    return SudekiMpPlayerOptionsLoad(options,
                                     (const unsigned char *)text,
                                     strlen(text));
}

static void test_utf16_round_trip_and_edits(void) {
    SudekiMpPlayerOptions options;
    SudekiMpPlayerOptionsType type;
    unsigned char *source;
    unsigned char *output;
    unsigned char *expected;
    size_t source_count;
    size_t output_count;
    size_t expected_count;
    char value[SUDEKIMP_PLAYER_OPTION_VALUE_CAPACITY];
    char *edited_text;

    source = utf16le_from_ascii(fixture, 1, &source_count);
    check(SudekiMpPlayerOptionsLoad(&options, source, source_count), "utf16 load");
    check(options.encoding == SUDEKIMP_PLAYER_OPTIONS_UTF16LE && options.has_bom,
          "utf16 encoding detected");
    check(SudekiMpPlayerOptionsSerialize(&options, &output, &output_count) &&
              output_count == source_count &&
              memcmp(output, source, source_count) == 0,
          "unchanged utf16 round trip is byte exact");
    free(output);

    check(SudekiMpPlayerOptionsGet(&options, "Resolution", "Width", value,
                                   sizeof(value), &type) &&
              strcmp(value, "1366") == 0 && type == SUDEKIMP_PLAYER_OPTION_INTEGER,
          "get integer");
    check(SudekiMpPlayerOptionsGet(&options, "AudioQuality", "Value", value,
                                   sizeof(value), &type) &&
              strcmp(value, "High") == 0 && type == SUDEKIMP_PLAYER_OPTION_STRING,
          "get double-quoted string with reordered attributes");
    check(SudekiMpPlayerOptionsSet(&options, "Resolution", "Width", "1920"), "set width");
    check(SudekiMpPlayerOptionsSet(&options, "Resolution", "Height", "1080"),
          "set longer height");
    check(SudekiMpPlayerOptionsSet(&options, "FullScreen", "Value", "True"), "set bool");
    check(SudekiMpPlayerOptionsSet(&options, "Gamma", "Value", "1.250000"), "set float");
    check(SudekiMpPlayerOptionsSet(&options, "AudioQuality", "Value", "Low"),
          "set shorter string");

    edited_text = (char *)malloc(sizeof(fixture) + 16u);
    strcpy(edited_text, fixture);
    {
        static const char *const replacements[][2] = {
            {"value='1366'", "value='1920'"},
            {"value='768'", "value='1080'"},
            {"value='False'", "value='True'"},
            {"value='1.000000'", "value='1.250000'"},
            {"value=\"High\"", "value=\"Low\""}
        };
        size_t index;
        for (index = 0u; index < sizeof(replacements) / sizeof(replacements[0]); ++index) {
            char *found = strstr(edited_text, replacements[index][0]);
            const size_t old_length = strlen(replacements[index][0]);
            const size_t new_length = strlen(replacements[index][1]);
            memmove(found + new_length, found + old_length, strlen(found + old_length) + 1u);
            memcpy(found, replacements[index][1], new_length);
        }
    }
    expected = utf16le_from_ascii(edited_text, 1, &expected_count);
    check(SudekiMpPlayerOptionsSerialize(&options, &output, &output_count) &&
              output_count == expected_count &&
              memcmp(output, expected, expected_count) == 0,
          "edited utf16 output changes only the edited values");
    free(output);
    free(expected);
    free(edited_text);
    free(source);
    SudekiMpPlayerOptionsFree(&options);
}

static void test_rejections(void) {
    SudekiMpPlayerOptions options;
    char value[8];
    size_t count = 0u;
    char joined[256] = "";

    check(load_text(&options, fixture), "ascii load");
    check(options.encoding == SUDEKIMP_PLAYER_OPTIONS_UTF8 && !options.has_bom,
          "ascii encoding detected");
    check(!SudekiMpPlayerOptionsSet(&options, "FullScreen", "Value", "yes"),
          "bool rejects non-canonical text");
    check(!SudekiMpPlayerOptionsSet(&options, "Resolution", "Width", "12a"),
          "integer rejects letters");
    check(!SudekiMpPlayerOptionsSet(&options, "Resolution", "Width", "12345678901"),
          "integer rejects overlong text");
    check(!SudekiMpPlayerOptionsSet(&options, "Gamma", "Value", "1."),
          "float rejects empty fraction");
    check(!SudekiMpPlayerOptionsSet(&options, "AudioQuality", "Value", "Hi' x='1"),
          "string rejects attribute injection");
    check(!SudekiMpPlayerOptionsSet(&options, "AudioQuality", "Value", "<b>"),
          "string rejects markup");
    check(!SudekiMpPlayerOptionsSet(&options, "Mystery", "Value", "1"),
          "unknown declared type is never written");
    check(!SudekiMpPlayerOptionsSet(&options, "EnableMouse", "Value", "True"),
          "absent setting is never invented");
    check(!SudekiMpPlayerOptionsSet(&options, "Resolution", "Depth", "32"),
          "absent variant is never invented");
    check(!SudekiMpPlayerOptionsHas(&options, "Commented", "Value"),
          "commented setting ignored");
    check(!SudekiMpPlayerOptionsHas(&options, "NotASetting", "Value"),
          "settings element is not a setting");
    check(!SudekiMpPlayerOptionsHas(&options, "Empty", "Value"),
          "self-closing setting has no variants");
    check(!SudekiMpPlayerOptionsGet(&options, "Forward", "ControlName0", value,
                                    sizeof(value), NULL),
          "get rejects value larger than destination");
    check(SudekiMpPlayerOptionsGet(&options, "Forward", "Key", value, sizeof(value),
                                   NULL) && strcmp(value, "200") == 0,
          "get control key");
    check(SudekiMpPlayerOptionsForEachSetting(&options, count_setting, &count) == 7u &&
              count == 7u,
          "enumerates real settings only");
    SudekiMpPlayerOptionsForEachSetting(&options, collect_setting, joined);
    check(strcmp(joined,
                 "Resolution,FullScreen,Gamma,AudioQuality,Forward,Mystery,Empty,") == 0,
          "settings enumerated in document order");
    SudekiMpPlayerOptionsFree(&options);
}

static void test_malformed_documents(void) {
    SudekiMpPlayerOptions options;
    static const unsigned char odd_utf16[] = {0xFFu, 0xFEu, '<', 0u, 'a'};
    static const unsigned char lone_surrogate[] = {0xFFu, 0xFEu, '<', 0u, 0x00u, 0xD8u, '>', 0u};
    static const unsigned char big_endian[] = {0xFEu, 0xFFu, 0u, '<'};
    static const unsigned char bad_utf8[] = {'<', 0xC3u, '>'};
    static const unsigned char embedded_nul[] = {'<', 'a', '>', 0u, '<', '/', 'a', '>'};

    check(!SudekiMpPlayerOptionsLoad(&options, odd_utf16, sizeof(odd_utf16)),
          "odd utf16 length rejected");
    check(!SudekiMpPlayerOptionsLoad(&options, lone_surrogate, sizeof(lone_surrogate)),
          "lone surrogate rejected");
    check(!SudekiMpPlayerOptionsLoad(&options, big_endian, sizeof(big_endian)),
          "big-endian document rejected");
    check(!SudekiMpPlayerOptionsLoad(&options, bad_utf8, sizeof(bad_utf8)),
          "invalid utf8 rejected");
    check(!SudekiMpPlayerOptionsLoad(&options, embedded_nul, sizeof(embedded_nul)),
          "embedded NUL rejected");
    check(!load_text(&options, "<other><setting id='A'></setting></other>"),
          "document without launcher_options root rejected");
    check(!load_text(&options, ""), "empty document rejected");
    check(!SudekiMpPlayerOptionsLoad(NULL, (const unsigned char *)"x", 1u),
          "null options rejected");
}

static void test_non_ascii_and_bom_round_trips(void) {
    SudekiMpPlayerOptions options;
    /* "é" (2-byte UTF-8) and U+1F600 (surrogate pair) inside a comment. */
    static const unsigned char utf16[] = {
        0xFFu, 0xFEu,
        '<', 0u, 'l', 0u, 'a', 0u, 'u', 0u, 'n', 0u, 'c', 0u, 'h', 0u, 'e', 0u, 'r', 0u,
        '_', 0u, 'o', 0u, 'p', 0u, 't', 0u, 'i', 0u, 'o', 0u, 'n', 0u, 's', 0u, '>', 0u,
        '<', 0u, '!', 0u, '-', 0u, '-', 0u, 0xE9u, 0x00u, 0x3Du, 0xD8u, 0x00u, 0xDEu,
        '-', 0u, '-', 0u, '>', 0u
    };
    static const char utf8_bom[] =
        "\xEF\xBB\xBF<launcher_options><setting id='Gamma'>"
        "<Variant id='Value' type='Float' value='1.0' /></setting></launcher_options>";
    unsigned char *output;
    size_t output_count;

    check(SudekiMpPlayerOptionsLoad(&options, utf16, sizeof(utf16)), "non-ascii utf16 load");
    check(strstr(options.text, "\xC3\xA9\xF0\x9F\x98\x80") != NULL,
          "non-ascii utf16 decoded to utf8");
    check(SudekiMpPlayerOptionsSerialize(&options, &output, &output_count) &&
              output_count == sizeof(utf16) && memcmp(output, utf16, sizeof(utf16)) == 0,
          "non-ascii utf16 round trip");
    free(output);
    SudekiMpPlayerOptionsFree(&options);

    check(load_text(&options, utf8_bom) && options.has_bom, "utf8 bom load");
    check(SudekiMpPlayerOptionsSet(&options, "Gamma", "Value", "0.750000"),
          "utf8 bom edit");
    check(SudekiMpPlayerOptionsSerialize(&options, &output, &output_count) &&
              output_count >= 3u && output[0] == 0xEFu && output[1] == 0xBBu &&
              output[2] == 0xBFu &&
              strstr((const char *)output + 3, "value='0.750000'") != NULL,
          "utf8 bom preserved on save");
    free(output);
    SudekiMpPlayerOptionsFree(&options);
}

static void test_value_validation(void) {
    check(SudekiMpPlayerOptionsValueIsValid(SUDEKIMP_PLAYER_OPTION_INTEGER, "-4"),
          "negative integer");
    check(SudekiMpPlayerOptionsValueIsValid(SUDEKIMP_PLAYER_OPTION_FLOAT, "2"),
          "integral float");
    check(!SudekiMpPlayerOptionsValueIsValid(SUDEKIMP_PLAYER_OPTION_FLOAT, ".5"),
          "float needs leading digit");
    check(SudekiMpPlayerOptionsValueIsValid(SUDEKIMP_PLAYER_OPTION_STRING, ""),
          "empty string allowed");
    check(!SudekiMpPlayerOptionsValueIsValid(SUDEKIMP_PLAYER_OPTION_STRING, "a\tb"),
          "control characters rejected");
    check(!SudekiMpPlayerOptionsValueIsValid(SUDEKIMP_PLAYER_OPTION_BOOL, "true"),
          "bool is case sensitive like the vanilla launcher output");
    check(!SudekiMpPlayerOptionsValueIsValid(SUDEKIMP_PLAYER_OPTION_UNKNOWN, "1"),
          "unknown type rejected");
}

int main(void) {
    test_utf16_round_trip_and_edits();
    test_rejections();
    test_malformed_documents();
    test_non_ascii_and_bom_round_trips();
    test_value_validation();
    if (failures != 0) {
        fprintf(stderr, "%d player options checks failed\n", failures);
        return 1;
    }
    puts("player options tests passed");
    return 0;
}
