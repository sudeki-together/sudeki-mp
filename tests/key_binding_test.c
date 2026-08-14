#include "input/key_binding.h"

#include <stdio.h>

typedef struct KeyCase {
    const wchar_t *text;
    UINT expected;
} KeyCase;

int main(void) {
    static const KeyCase valid_cases[] = {
        {L"G", 'G'},
        {L"g", 'G'},
        {L"0", '0'},
        {L"F1", VK_F1},
        {L"f24", VK_F24},
        {L"Numpad0", VK_NUMPAD0},
        {L"numpad9", VK_NUMPAD9},
        {L"Space", VK_SPACE},
        {L"LeftShift", VK_LSHIFT},
        {L"Mouse5", VK_XBUTTON2},
        {L"Semicolon", VK_OEM_1}
    };
    static const wchar_t *invalid_cases[] = {
        L"", L"F0", L"F25", L"Numpad10", L"GG", L"NotAKey"
    };
    size_t index;
    UINT value;
    int failures = 0;

    for (index = 0; index < sizeof(valid_cases) / sizeof(valid_cases[0]); ++index) {
        value = 0;
        if (!SudekiMpParseInputKey(valid_cases[index].text, &value) ||
            value != valid_cases[index].expected) {
            fprintf(stderr, "FAIL: valid key case %lu\n", (unsigned long)index);
            ++failures;
        }
    }
    for (index = 0; index < sizeof(invalid_cases) / sizeof(invalid_cases[0]); ++index) {
        value = 0;
        if (SudekiMpParseInputKey(invalid_cases[index], &value)) {
            fprintf(stderr, "FAIL: invalid key case %lu\n", (unsigned long)index);
            ++failures;
        }
    }
    if (SudekiMpParseInputKey(NULL, &value) ||
        SudekiMpParseInputKey(L"G", NULL)) {
        fputs("FAIL: null argument accepted\n", stderr);
        ++failures;
    }

    if (failures != 0) {
        fprintf(stderr, "%d key-binding test(s) failed\n", failures);
        return 1;
    }
    puts("key_binding_test: PASS");
    return 0;
}
