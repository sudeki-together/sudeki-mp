#include "input/key_binding.h"

#include <wchar.h>
#include <wctype.h>

typedef struct SudekiMpNamedKey {
    const wchar_t *name;
    UINT virtual_key;
} SudekiMpNamedKey;

static const SudekiMpNamedKey named_keys[] = {
    {L"Backspace", VK_BACK},
    {L"Tab", VK_TAB},
    {L"Enter", VK_RETURN},
    {L"Shift", VK_SHIFT},
    {L"Ctrl", VK_CONTROL},
    {L"Control", VK_CONTROL},
    {L"Alt", VK_MENU},
    {L"Pause", VK_PAUSE},
    {L"CapsLock", VK_CAPITAL},
    {L"Escape", VK_ESCAPE},
    {L"Esc", VK_ESCAPE},
    {L"Space", VK_SPACE},
    {L"PageUp", VK_PRIOR},
    {L"PgUp", VK_PRIOR},
    {L"PageDown", VK_NEXT},
    {L"PgDn", VK_NEXT},
    {L"End", VK_END},
    {L"Home", VK_HOME},
    {L"Left", VK_LEFT},
    {L"Up", VK_UP},
    {L"Right", VK_RIGHT},
    {L"Down", VK_DOWN},
    {L"PrintScreen", VK_SNAPSHOT},
    {L"Insert", VK_INSERT},
    {L"Ins", VK_INSERT},
    {L"Delete", VK_DELETE},
    {L"Del", VK_DELETE},
    {L"LeftShift", VK_LSHIFT},
    {L"RightShift", VK_RSHIFT},
    {L"LeftCtrl", VK_LCONTROL},
    {L"RightCtrl", VK_RCONTROL},
    {L"LeftAlt", VK_LMENU},
    {L"RightAlt", VK_RMENU},
    {L"NumpadMultiply", VK_MULTIPLY},
    {L"NumpadAdd", VK_ADD},
    {L"NumpadSubtract", VK_SUBTRACT},
    {L"NumpadDecimal", VK_DECIMAL},
    {L"NumpadDivide", VK_DIVIDE},
    {L"Semicolon", VK_OEM_1},
    {L"Equals", VK_OEM_PLUS},
    {L"Comma", VK_OEM_COMMA},
    {L"Minus", VK_OEM_MINUS},
    {L"Period", VK_OEM_PERIOD},
    {L"Slash", VK_OEM_2},
    {L"Backtick", VK_OEM_3},
    {L"LeftBracket", VK_OEM_4},
    {L"Backslash", VK_OEM_5},
    {L"RightBracket", VK_OEM_6},
    {L"Quote", VK_OEM_7},
    {L"Mouse1", VK_LBUTTON},
    {L"Mouse2", VK_RBUTTON},
    {L"Mouse3", VK_MBUTTON},
    {L"Mouse4", VK_XBUTTON1},
    {L"Mouse5", VK_XBUTTON2}
};

static BOOL parse_decimal_suffix(
    const wchar_t *text,
    const wchar_t *prefix,
    unsigned int minimum,
    unsigned int maximum,
    unsigned int *result
) {
    size_t prefix_length = wcslen(prefix);
    const wchar_t *cursor;
    unsigned int value = 0;

    if (_wcsnicmp(text, prefix, prefix_length) != 0) {
        return FALSE;
    }
    cursor = text + prefix_length;
    if (*cursor == L'\0') {
        return FALSE;
    }
    while (*cursor != L'\0') {
        if (!iswdigit(*cursor)) {
            return FALSE;
        }
        value = value * 10u + (unsigned int)(*cursor - L'0');
        ++cursor;
    }
    if (value < minimum || value > maximum) {
        return FALSE;
    }
    *result = value;
    return TRUE;
}

BOOL SudekiMpParseInputKey(const wchar_t *text, UINT *virtual_key) {
    size_t index;
    size_t length;
    unsigned int number;
    wchar_t character;

    if (text == NULL || virtual_key == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    length = wcslen(text);
    if (length == 1u) {
        character = towupper(text[0]);
        if ((character >= L'A' && character <= L'Z') ||
            (character >= L'0' && character <= L'9')) {
            *virtual_key = (UINT)character;
            return TRUE;
        }
    }

    if (parse_decimal_suffix(text, L"F", 1u, 24u, &number)) {
        *virtual_key = VK_F1 + number - 1u;
        return TRUE;
    }
    if (parse_decimal_suffix(text, L"Numpad", 0u, 9u, &number)) {
        *virtual_key = VK_NUMPAD0 + number;
        return TRUE;
    }

    for (index = 0; index < sizeof(named_keys) / sizeof(named_keys[0]); ++index) {
        if (_wcsicmp(text, named_keys[index].name) == 0) {
            *virtual_key = named_keys[index].virtual_key;
            return TRUE;
        }
    }

    SetLastError(ERROR_INVALID_DATA);
    return FALSE;
}
