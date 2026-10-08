#include "player_options.h"

#include <stdlib.h>
#include <string.h>

typedef struct OptionTag {
    const char *begin;      /* '<' */
    const char *end;        /* '>' */
    const char *name;
    size_t name_length;
    int closing;
    int self_closing;
} OptionTag;

typedef struct OptionRange {
    const char *begin;
    const char *end;
} OptionRange;

static int is_space(char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

static int is_name_char(char value) {
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
           (value >= '0' && value <= '9') || value == '_' || value == '-' ||
           value == ':' || value == '.';
}

static const char *find_text(const char *cursor, const char *end, const char *needle) {
    const size_t length = strlen(needle);
    while (cursor != NULL && (size_t)(end - cursor) >= length) {
        if (memcmp(cursor, needle, length) == 0) {
            return cursor;
        }
        ++cursor;
    }
    return NULL;
}

/* Finds the next element tag at or after cursor, skipping comments, processing
   instructions and declarations. Quoted '>' characters stay inside the tag. */
static int next_tag(const char *cursor, const char *end, OptionTag *tag) {
    while (cursor < end) {
        const char *open = memchr(cursor, '<', (size_t)(end - cursor));
        const char *scan;
        char quote = 0;
        if (open == NULL) {
            return 0;
        }
        if ((size_t)(end - open) >= 4u && memcmp(open, "<!--", 4u) == 0) {
            const char *close = find_text(open + 4, end, "-->");
            if (close == NULL) {
                return 0;
            }
            cursor = close + 3;
            continue;
        }
        if (open + 1 < end && (open[1] == '?' || open[1] == '!')) {
            const char *close = memchr(open, '>', (size_t)(end - open));
            if (close == NULL) {
                return 0;
            }
            cursor = close + 1;
            continue;
        }
        tag->begin = open;
        tag->closing = open + 1 < end && open[1] == '/';
        tag->name = open + (tag->closing ? 2 : 1);
        tag->name_length = 0u;
        while (tag->name + tag->name_length < end &&
               is_name_char(tag->name[tag->name_length])) {
            ++tag->name_length;
        }
        for (scan = tag->name + tag->name_length; scan < end; ++scan) {
            if (quote != 0) {
                if (*scan == quote) {
                    quote = 0;
                }
            } else if (*scan == '\'' || *scan == '"') {
                quote = *scan;
            } else if (*scan == '>') {
                break;
            } else if (*scan == '<') {
                return 0;
            }
        }
        if (scan >= end || tag->name_length == 0u) {
            return 0;
        }
        tag->end = scan;
        tag->self_closing = scan > open && scan[-1] == '/';
        return 1;
    }
    return 0;
}

static int tag_is(const OptionTag *tag, const char *name) {
    const size_t length = strlen(name);
    return tag->name_length == length && memcmp(tag->name, name, length) == 0;
}

static int tag_attribute(const OptionTag *tag, const char *name, OptionRange *value) {
    const size_t length = strlen(name);
    const char *cursor = tag->name + tag->name_length;
    while (cursor < tag->end) {
        const char *attribute;
        size_t attribute_length = 0u;
        char quote;
        const char *close;
        while (cursor < tag->end && is_space(*cursor)) {
            ++cursor;
        }
        attribute = cursor;
        while (cursor < tag->end && is_name_char(*cursor)) {
            ++cursor;
            ++attribute_length;
        }
        if (attribute_length == 0u) {
            return 0;
        }
        while (cursor < tag->end && is_space(*cursor)) {
            ++cursor;
        }
        if (cursor >= tag->end || *cursor != '=') {
            return 0;
        }
        ++cursor;
        while (cursor < tag->end && is_space(*cursor)) {
            ++cursor;
        }
        if (cursor >= tag->end || (*cursor != '\'' && *cursor != '"')) {
            return 0;
        }
        quote = *cursor++;
        close = memchr(cursor, quote, (size_t)(tag->end - cursor));
        if (close == NULL) {
            return 0;
        }
        if (attribute_length == length && memcmp(attribute, name, length) == 0) {
            value->begin = cursor;
            value->end = close;
            return 1;
        }
        cursor = close + 1;
    }
    return 0;
}

static int range_equals(const OptionRange *range, const char *text) {
    const size_t length = strlen(text);
    return (size_t)(range->end - range->begin) == length &&
           memcmp(range->begin, text, length) == 0;
}

static int find_setting(const SudekiMpPlayerOptions *options,
                        const char *setting_id,
                        OptionRange *block) {
    const char *cursor;
    const char *end;
    OptionTag tag;
    if (options == NULL || options->text == NULL || setting_id == NULL) {
        return 0;
    }
    cursor = options->text;
    end = options->text + options->length;
    while (next_tag(cursor, end, &tag)) {
        OptionRange id;
        cursor = tag.end + 1;
        if (tag.closing || !tag_is(&tag, "setting") ||
            !tag_attribute(&tag, "id", &id) || !range_equals(&id, setting_id)) {
            continue;
        }
        if (tag.self_closing) {
            block->begin = cursor;
            block->end = cursor;
            return 1;
        }
        block->begin = cursor;
        while (next_tag(cursor, end, &tag)) {
            if (tag.closing && tag_is(&tag, "setting")) {
                block->end = tag.begin;
                return 1;
            }
            if (!tag.closing && tag_is(&tag, "setting")) {
                return 0;
            }
            cursor = tag.end + 1;
        }
        return 0;
    }
    return 0;
}

static int find_variant(const SudekiMpPlayerOptions *options,
                        const char *setting_id,
                        const char *variant_id,
                        OptionRange *value,
                        SudekiMpPlayerOptionsType *type) {
    OptionRange block;
    OptionTag tag;
    const char *cursor;
    if (variant_id == NULL || !find_setting(options, setting_id, &block)) {
        return 0;
    }
    cursor = block.begin;
    while (next_tag(cursor, block.end, &tag)) {
        OptionRange id;
        OptionRange type_text;
        cursor = tag.end + 1;
        if (tag.closing || !tag_is(&tag, "Variant") ||
            !tag_attribute(&tag, "id", &id) || !range_equals(&id, variant_id)) {
            continue;
        }
        if (!tag_attribute(&tag, "value", value)) {
            return 0;
        }
        if (type != NULL) {
            *type = SUDEKIMP_PLAYER_OPTION_UNKNOWN;
            if (tag_attribute(&tag, "type", &type_text)) {
                if (range_equals(&type_text, "Integer")) {
                    *type = SUDEKIMP_PLAYER_OPTION_INTEGER;
                } else if (range_equals(&type_text, "Bool")) {
                    *type = SUDEKIMP_PLAYER_OPTION_BOOL;
                } else if (range_equals(&type_text, "Float")) {
                    *type = SUDEKIMP_PLAYER_OPTION_FLOAT;
                } else if (range_equals(&type_text, "String")) {
                    *type = SUDEKIMP_PLAYER_OPTION_STRING;
                }
            }
        }
        return 1;
    }
    return 0;
}

static int utf8_is_valid(const unsigned char *bytes, size_t count) {
    size_t index = 0u;
    while (index < count) {
        const unsigned char lead = bytes[index];
        size_t extra;
        unsigned long code;
        size_t step;
        if (lead == 0u) {
            return 0;
        }
        if (lead < 0x80u) {
            ++index;
            continue;
        }
        if (lead >= 0xC2u && lead <= 0xDFu) {
            extra = 1u;
            code = lead & 0x1Fu;
        } else if (lead >= 0xE0u && lead <= 0xEFu) {
            extra = 2u;
            code = lead & 0x0Fu;
        } else if (lead >= 0xF0u && lead <= 0xF4u) {
            extra = 3u;
            code = lead & 0x07u;
        } else {
            return 0;
        }
        if (count - index <= extra) {
            return 0;
        }
        for (step = 1u; step <= extra; ++step) {
            if ((bytes[index + step] & 0xC0u) != 0x80u) {
                return 0;
            }
            code = (code << 6) | (bytes[index + step] & 0x3Fu);
        }
        if ((extra == 2u && code < 0x800u) || (extra == 3u && code < 0x10000u) ||
            code > 0x10FFFFu || (code >= 0xD800u && code <= 0xDFFFu)) {
            return 0;
        }
        index += extra + 1u;
    }
    return 1;
}

static int reserve(SudekiMpPlayerOptions *options, size_t length) {
    char *grown;
    size_t capacity;
    if (length + 1u <= options->capacity) {
        return 1;
    }
    capacity = options->capacity == 0u ? 256u : options->capacity;
    while (capacity < length + 1u) {
        if (capacity > ((size_t)-1) / 2u) {
            return 0;
        }
        capacity *= 2u;
    }
    grown = (char *)realloc(options->text, capacity);
    if (grown == NULL) {
        return 0;
    }
    options->text = grown;
    options->capacity = capacity;
    return 1;
}

static int append_utf8(SudekiMpPlayerOptions *options, unsigned long code) {
    char encoded[4];
    size_t length;
    if (code < 0x80u) {
        encoded[0] = (char)code;
        length = 1u;
    } else if (code < 0x800u) {
        encoded[0] = (char)(0xC0u | (code >> 6));
        encoded[1] = (char)(0x80u | (code & 0x3Fu));
        length = 2u;
    } else if (code < 0x10000u) {
        encoded[0] = (char)(0xE0u | (code >> 12));
        encoded[1] = (char)(0x80u | ((code >> 6) & 0x3Fu));
        encoded[2] = (char)(0x80u | (code & 0x3Fu));
        length = 3u;
    } else {
        encoded[0] = (char)(0xF0u | (code >> 18));
        encoded[1] = (char)(0x80u | ((code >> 12) & 0x3Fu));
        encoded[2] = (char)(0x80u | ((code >> 6) & 0x3Fu));
        encoded[3] = (char)(0x80u | (code & 0x3Fu));
        length = 4u;
    }
    if (!reserve(options, options->length + length)) {
        return 0;
    }
    memcpy(options->text + options->length, encoded, length);
    options->length += length;
    options->text[options->length] = '\0';
    return 1;
}

static int load_utf16le(SudekiMpPlayerOptions *options,
                        const unsigned char *bytes,
                        size_t count) {
    size_t index = 0u;
    if ((count % 2u) != 0u) {
        return 0;
    }
    while (index < count) {
        unsigned long code = (unsigned long)bytes[index] |
                             ((unsigned long)bytes[index + 1u] << 8);
        index += 2u;
        if (code == 0u) {
            return 0;
        }
        if (code >= 0xD800u && code <= 0xDBFFu) {
            unsigned long low;
            if (index + 2u > count) {
                return 0;
            }
            low = (unsigned long)bytes[index] | ((unsigned long)bytes[index + 1u] << 8);
            if (low < 0xDC00u || low > 0xDFFFu) {
                return 0;
            }
            index += 2u;
            code = 0x10000u + ((code - 0xD800u) << 10) + (low - 0xDC00u);
        } else if (code >= 0xDC00u && code <= 0xDFFFu) {
            return 0;
        }
        if (!append_utf8(options, code)) {
            return 0;
        }
    }
    return 1;
}

static int has_root(const SudekiMpPlayerOptions *options) {
    const char *cursor = options->text;
    const char *end = options->text + options->length;
    OptionTag tag;
    while (next_tag(cursor, end, &tag)) {
        if (!tag.closing && tag_is(&tag, "launcher_options")) {
            return 1;
        }
        cursor = tag.end + 1;
    }
    return 0;
}

int SudekiMpPlayerOptionsLoad(SudekiMpPlayerOptions *options,
                              const unsigned char *bytes,
                              size_t count) {
    int loaded;
    if (options == NULL || (bytes == NULL && count != 0u)) {
        return 0;
    }
    memset(options, 0, sizeof(*options));
    if (!reserve(options, count)) {
        return 0;
    }
    options->text[0] = '\0';
    if (count >= 2u && bytes[0] == 0xFFu && bytes[1] == 0xFEu) {
        options->encoding = SUDEKIMP_PLAYER_OPTIONS_UTF16LE;
        options->has_bom = 1;
        loaded = load_utf16le(options, bytes + 2, count - 2u);
    } else if (count >= 2u && bytes[0] == 0xFEu && bytes[1] == 0xFFu) {
        loaded = 0;
    } else if (count >= 2u && bytes[0] != 0u && bytes[1] == 0u) {
        options->encoding = SUDEKIMP_PLAYER_OPTIONS_UTF16LE;
        loaded = load_utf16le(options, bytes, count);
    } else {
        size_t offset = 0u;
        options->encoding = SUDEKIMP_PLAYER_OPTIONS_UTF8;
        if (count >= 3u && bytes[0] == 0xEFu && bytes[1] == 0xBBu && bytes[2] == 0xBFu) {
            options->has_bom = 1;
            offset = 3u;
        }
        loaded = utf8_is_valid(bytes + offset, count - offset);
        if (loaded) {
            memcpy(options->text, bytes + offset, count - offset);
            options->length = count - offset;
            options->text[options->length] = '\0';
        }
    }
    if (!loaded || !has_root(options)) {
        SudekiMpPlayerOptionsFree(options);
        return 0;
    }
    return 1;
}

void SudekiMpPlayerOptionsFree(SudekiMpPlayerOptions *options) {
    if (options == NULL) {
        return;
    }
    free(options->text);
    memset(options, 0, sizeof(*options));
}

int SudekiMpPlayerOptionsHas(const SudekiMpPlayerOptions *options,
                             const char *setting_id,
                             const char *variant_id) {
    OptionRange value;
    return find_variant(options, setting_id, variant_id, &value, NULL);
}

int SudekiMpPlayerOptionsGet(const SudekiMpPlayerOptions *options,
                             const char *setting_id,
                             const char *variant_id,
                             char *value,
                             size_t value_count,
                             SudekiMpPlayerOptionsType *type) {
    OptionRange range;
    size_t length;
    if (value == NULL || value_count == 0u ||
        !find_variant(options, setting_id, variant_id, &range, type)) {
        return 0;
    }
    length = (size_t)(range.end - range.begin);
    if (length >= value_count) {
        return 0;
    }
    memcpy(value, range.begin, length);
    value[length] = '\0';
    return 1;
}

static int all_digits(const char *cursor, size_t *count) {
    size_t digits = 0u;
    while (cursor[digits] >= '0' && cursor[digits] <= '9') {
        ++digits;
    }
    *count = digits;
    return digits > 0u;
}

int SudekiMpPlayerOptionsValueIsValid(SudekiMpPlayerOptionsType type,
                                      const char *value) {
    size_t length;
    size_t digits;
    const char *cursor;
    if (value == NULL) {
        return 0;
    }
    length = strlen(value);
    switch (type) {
        case SUDEKIMP_PLAYER_OPTION_INTEGER:
            cursor = value + (value[0] == '-' ? 1 : 0);
            return all_digits(cursor, &digits) && digits <= 10u &&
                   cursor[digits] == '\0';
        case SUDEKIMP_PLAYER_OPTION_BOOL:
            return strcmp(value, "True") == 0 || strcmp(value, "False") == 0;
        case SUDEKIMP_PLAYER_OPTION_FLOAT:
            if (length == 0u || length > 32u) {
                return 0;
            }
            cursor = value + (value[0] == '-' ? 1 : 0);
            if (!all_digits(cursor, &digits)) {
                return 0;
            }
            cursor += digits;
            if (*cursor == '.') {
                ++cursor;
                if (!all_digits(cursor, &digits)) {
                    return 0;
                }
                cursor += digits;
            }
            return *cursor == '\0';
        case SUDEKIMP_PLAYER_OPTION_STRING:
            if (length >= SUDEKIMP_PLAYER_OPTION_VALUE_CAPACITY ||
                !utf8_is_valid((const unsigned char *)value, length)) {
                return 0;
            }
            for (cursor = value; *cursor != '\0'; ++cursor) {
                const unsigned char character = (unsigned char)*cursor;
                if (character < 0x20u || character == '<' || character == '>' ||
                    character == '&' || character == '\'' || character == '"') {
                    return 0;
                }
            }
            return 1;
        default:
            return 0;
    }
}

int SudekiMpPlayerOptionsSet(SudekiMpPlayerOptions *options,
                             const char *setting_id,
                             const char *variant_id,
                             const char *value) {
    OptionRange range;
    SudekiMpPlayerOptionsType type;
    size_t offset;
    size_t old_length;
    size_t new_length;
    size_t tail;
    if (options == NULL ||
        !find_variant(options, setting_id, variant_id, &range, &type) ||
        !SudekiMpPlayerOptionsValueIsValid(type, value)) {
        return 0;
    }
    offset = (size_t)(range.begin - options->text);
    old_length = (size_t)(range.end - range.begin);
    new_length = strlen(value);
    tail = options->length - offset - old_length;
    if (new_length > old_length &&
        !reserve(options, options->length + (new_length - old_length))) {
        return 0;
    }
    memmove(options->text + offset + new_length,
            options->text + offset + old_length,
            tail + 1u);
    memcpy(options->text + offset, value, new_length);
    options->length = options->length - old_length + new_length;
    return 1;
}

size_t SudekiMpPlayerOptionsForEachSetting(const SudekiMpPlayerOptions *options,
                                           SudekiMpPlayerOptionsSettingCallback callback,
                                           void *context) {
    const char *cursor;
    const char *end;
    OptionTag tag;
    size_t visited = 0u;
    if (options == NULL || options->text == NULL || callback == NULL) {
        return 0u;
    }
    cursor = options->text;
    end = options->text + options->length;
    while (next_tag(cursor, end, &tag)) {
        OptionRange id;
        cursor = tag.end + 1;
        if (!tag.closing && tag_is(&tag, "setting") && tag_attribute(&tag, "id", &id)) {
            char copy[SUDEKIMP_PLAYER_OPTION_ID_CAPACITY];
            const size_t length = (size_t)(id.end - id.begin);
            if (length == 0u || length >= sizeof(copy)) {
                continue;
            }
            memcpy(copy, id.begin, length);
            copy[length] = '\0';
            callback(copy, context);
            ++visited;
        }
    }
    return visited;
}

int SudekiMpPlayerOptionsSerialize(const SudekiMpPlayerOptions *options,
                                   unsigned char **bytes,
                                   size_t *count) {
    unsigned char *output;
    size_t written = 0u;
    size_t index = 0u;
    if (options == NULL || options->text == NULL || bytes == NULL || count == NULL) {
        return 0;
    }
    *bytes = NULL;
    *count = 0u;
    if (options->encoding == SUDEKIMP_PLAYER_OPTIONS_UTF8) {
        output = (unsigned char *)malloc(options->length + 3u);
        if (output == NULL) {
            return 0;
        }
        if (options->has_bom) {
            output[written++] = 0xEFu;
            output[written++] = 0xBBu;
            output[written++] = 0xBFu;
        }
        memcpy(output + written, options->text, options->length);
        *bytes = output;
        *count = written + options->length;
        return 1;
    }
    /* Each UTF-8 byte yields at most one UTF-16 unit (4-byte sequences give two). */
    if (options->length > (((size_t)-1) - 2u) / 2u) {
        return 0;
    }
    output = (unsigned char *)malloc(options->length * 2u + 2u);
    if (output == NULL) {
        return 0;
    }
    if (options->has_bom) {
        output[written++] = 0xFFu;
        output[written++] = 0xFEu;
    }
    while (index < options->length) {
        const unsigned char lead = (unsigned char)options->text[index];
        unsigned long code;
        size_t extra;
        size_t step;
        if (lead < 0x80u) {
            code = lead;
            extra = 0u;
        } else if (lead < 0xE0u) {
            code = lead & 0x1Fu;
            extra = 1u;
        } else if (lead < 0xF0u) {
            code = lead & 0x0Fu;
            extra = 2u;
        } else {
            code = lead & 0x07u;
            extra = 3u;
        }
        if (index + extra >= options->length && extra != 0u) {
            free(output);
            return 0;
        }
        for (step = 1u; step <= extra; ++step) {
            code = (code << 6) | ((unsigned char)options->text[index + step] & 0x3Fu);
        }
        index += extra + 1u;
        if (code >= 0x10000u) {
            const unsigned long high = 0xD800u + ((code - 0x10000u) >> 10);
            const unsigned long low = 0xDC00u + ((code - 0x10000u) & 0x3FFu);
            output[written++] = (unsigned char)(high & 0xFFu);
            output[written++] = (unsigned char)(high >> 8);
            output[written++] = (unsigned char)(low & 0xFFu);
            output[written++] = (unsigned char)(low >> 8);
        } else {
            output[written++] = (unsigned char)(code & 0xFFu);
            output[written++] = (unsigned char)(code >> 8);
        }
    }
    *bytes = output;
    *count = written;
    return 1;
}
