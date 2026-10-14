#include "settings_page.h"

#include <commctrl.h>
#include <strsafe.h>

#include <stdlib.h>
#include <string.h>

#define SUDEKIMP_MAX_MODES 128u
#define SUDEKIMP_MAX_SETTINGS 160u

typedef struct DisplayMode {
    int width;
    int height;
} DisplayMode;

typedef struct BindingLabel {
    const char *setting_id;
    const WCHAR *label;
} BindingLabel;

typedef struct SettingList {
    char ids[SUDEKIMP_MAX_SETTINGS][SUDEKIMP_PLAYER_OPTION_ID_CAPACITY];
    size_t count;
} SettingList;

/* Labels follow the vanilla launcher's own control captions. */
static const BindingLabel binding_labels[] = {
    {"Forward", L"Forwards"},
    {"Back", L"Back"},
    {"Left", L"Left"},
    {"Right", L"Right"},
    {"Walk", L"Walk"},
    {"CameraU", L"Camera up / zoom in"},
    {"CameraD", L"Camera down / zoom out"},
    {"CameraL", L"Camera left"},
    {"CameraR", L"Camera right"},
    {"ResetCamera", L"Reset camera"},
    {"PrevCharacter", L"Previous character"},
    {"NextCharacter", L"Next character"},
    {"PrimaryAction", L"Primary action"},
    {"SecondaryAction", L"Secondary action"},
    {"MainMenu", L"Main menu"},
    {"Cancel", L"Cancel"},
    {"ShowHealth", L"Show health"},
    {"MenuAction1", L"Menu action"},
    {"Skip1", L"Skip"},
    {"SweepAttack", L"Sweep attack"},
    {"Block", L"Block"},
    {"1stPersonHold", L"1st person (hold)"},
    {"1stPersonToggle", L"1st person (toggle)"},
    {"NextWeapon", L"Next weapon"},
    {"PrevWeapon", L"Previous weapon"},
    {"QuickMenu", L"Quick menu"},
    {"QuickShot1", L"Quick shot 1"},
    {"QuickShot2", L"Quick shot 2"},
    {"QuickShot3", L"Quick shot 3"},
    {"QuickShot4", L"Quick shot 4"},
    {"QuickSkill", L"Skill 1"},
    {"QuickSkill11", L"Skill 2"},
    {"QuickSkill21", L"Skill 3"},
    {"QuickSkill31", L"Skill 4"},
    {"QuickSkill41", L"Skill 5"},
    {"QuickSkill51", L"Skill 6"}
};

static void utf8_to_wide(const char *text, WCHAR *wide, int wide_count) {
    if (MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, wide_count) == 0) {
        wide[0] = L'\0';
    }
}

static BOOL wide_to_utf8(const WCHAR *wide, char *text, int text_count) {
    return WideCharToMultiByte(CP_UTF8, 0, wide, -1, text, text_count, NULL, NULL) > 0;
}

static BOOL get_integer(const SudekiMpPlayerOptions *options,
                        const char *setting_id,
                        const char *variant_id,
                        int *value) {
    char text[SUDEKIMP_PLAYER_OPTION_VALUE_CAPACITY];
    char *end;
    long parsed;
    if (options == NULL ||
        !SudekiMpPlayerOptionsGet(options, setting_id, variant_id, text, sizeof(text),
                                  NULL)) {
        return FALSE;
    }
    parsed = strtol(text, &end, 10);
    if (end == text || *end != '\0' || parsed < -100000L || parsed > 100000L) {
        return FALSE;
    }
    *value = (int)parsed;
    return TRUE;
}

static int combo_add(HWND combo, const WCHAR *text, LPARAM data) {
    const int index = (int)SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)text);
    if (index >= 0) {
        SendMessageW(combo, CB_SETITEMDATA, (WPARAM)index, data);
    }
    return index;
}

static BOOL combo_selected_data(HWND combo, LPARAM *data) {
    const int index = (int)SendMessageW(combo, CB_GETCURSEL, 0, 0);
    if (index < 0) {
        return FALSE;
    }
    *data = (LPARAM)SendMessageW(combo, CB_GETITEMDATA, (WPARAM)index, 0);
    return TRUE;
}

static void combo_select_data(HWND combo, LPARAM data) {
    const int count = (int)SendMessageW(combo, CB_GETCOUNT, 0, 0);
    int index;
    for (index = 0; index < count; ++index) {
        if ((LPARAM)SendMessageW(combo, CB_GETITEMDATA, (WPARAM)index, 0) == data) {
            SendMessageW(combo, CB_SETCURSEL, (WPARAM)index, 0);
            return;
        }
    }
}

static int compare_modes(const void *left, const void *right) {
    const DisplayMode *a = (const DisplayMode *)left;
    const DisplayMode *b = (const DisplayMode *)right;
    if (a->width != b->width) {
        return a->width - b->width;
    }
    return a->height - b->height;
}

static int compare_ints(const void *left, const void *right) {
    return *(const int *)left - *(const int *)right;
}

static LPARAM pack_mode(int width, int height) {
    return (LPARAM)(((DWORD)width << 16) | ((DWORD)height & 0xFFFFu));
}

static void unpack_mode(LPARAM data, int *width, int *height) {
    *width = (int)(((DWORD)data >> 16) & 0xFFFFu);
    *height = (int)((DWORD)data & 0xFFFFu);
}

static void populate_resolutions(HWND combo, int saved_width, int saved_height) {
    DisplayMode modes[SUDEKIMP_MAX_MODES];
    size_t count = 0u;
    size_t index;
    DWORD mode_index;
    DEVMODEW mode;
    BOOL saved_listed = FALSE;

    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    ZeroMemory(&mode, sizeof(mode));
    mode.dmSize = sizeof(mode);
    for (mode_index = 0u; EnumDisplaySettingsW(NULL, mode_index, &mode); ++mode_index) {
        const int width = (int)mode.dmPelsWidth;
        const int height = (int)mode.dmPelsHeight;
        BOOL duplicate = FALSE;
        if (width < 640 || height < 480 || width > 0xFFFF || height > 0xFFFF) {
            continue;
        }
        for (index = 0u; index < count; ++index) {
            if (modes[index].width == width && modes[index].height == height) {
                duplicate = TRUE;
                break;
            }
        }
        if (!duplicate && count < SUDEKIMP_MAX_MODES) {
            modes[count].width = width;
            modes[count].height = height;
            ++count;
        }
    }
    for (index = 0u; index < count; ++index) {
        if (modes[index].width == saved_width && modes[index].height == saved_height) {
            saved_listed = TRUE;
        }
    }
    if (!saved_listed && saved_width > 0 && saved_height > 0 &&
        saved_width <= 0xFFFF && saved_height <= 0xFFFF && count < SUDEKIMP_MAX_MODES) {
        modes[count].width = saved_width;
        modes[count].height = saved_height;
        ++count;
    }
    qsort(modes, count, sizeof(modes[0]), compare_modes);
    for (index = 0u; index < count; ++index) {
        WCHAR text[32];
        StringCchPrintfW(text, sizeof(text) / sizeof(text[0]), L"%d × %d",
                         modes[index].width, modes[index].height);
        combo_add(combo, text, pack_mode(modes[index].width, modes[index].height));
    }
    combo_select_data(combo, pack_mode(saved_width, saved_height));
}

static void populate_refresh_rates(HWND resolution, HWND refresh, int preferred) {
    int rates[SUDEKIMP_MAX_MODES];
    size_t count = 0u;
    size_t index;
    DWORD mode_index;
    DEVMODEW mode;
    LPARAM data;
    int width = 0;
    int height = 0;
    BOOL preferred_listed = FALSE;

    SendMessageW(refresh, CB_RESETCONTENT, 0, 0);
    if (combo_selected_data(resolution, &data)) {
        unpack_mode(data, &width, &height);
    }
    ZeroMemory(&mode, sizeof(mode));
    mode.dmSize = sizeof(mode);
    for (mode_index = 0u; EnumDisplaySettingsW(NULL, mode_index, &mode); ++mode_index) {
        const int rate = (int)mode.dmDisplayFrequency;
        BOOL duplicate = FALSE;
        if ((int)mode.dmPelsWidth != width || (int)mode.dmPelsHeight != height ||
            rate <= 1) {
            continue;
        }
        for (index = 0u; index < count; ++index) {
            if (rates[index] == rate) {
                duplicate = TRUE;
                break;
            }
        }
        if (!duplicate && count < SUDEKIMP_MAX_MODES) {
            rates[count++] = rate;
        }
    }
    for (index = 0u; index < count; ++index) {
        if (rates[index] == preferred) {
            preferred_listed = TRUE;
        }
    }
    /* A display that reports no rate for this mode keeps the saved rate
       rather than an invented one. */
    if (!preferred_listed && preferred > 1 && count == 0u) {
        rates[count++] = preferred;
        preferred_listed = TRUE;
    }
    qsort(rates, count, sizeof(rates[0]), compare_ints);
    for (index = 0u; index < count; ++index) {
        WCHAR text[24];
        StringCchPrintfW(text, sizeof(text) / sizeof(text[0]), L"%d Hz", rates[index]);
        combo_add(refresh, text, (LPARAM)rates[index]);
    }
    if (preferred_listed) {
        combo_select_data(refresh, (LPARAM)preferred);
    } else if (count > 0u) {
        BOOL has_sixty = FALSE;
        for (index = 0u; index < count; ++index) {
            has_sixty = has_sixty || rates[index] == 60;
        }
        if (has_sixty) {
            combo_select_data(refresh, (LPARAM)60);
        } else {
            SendMessageW(refresh, CB_SETCURSEL, (WPARAM)(count - 1u), 0);
        }
    }
}

static void populate_antialiasing(HWND combo, const SudekiMpPlayerOptions *options) {
    int saved = 0;
    const BOOL has_saved = get_integer(options, "AntiAliasing", "Value", &saved);
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    combo_add(combo, L"Off", 0);
    combo_add(combo, L"2x", 2);
    combo_add(combo, L"4x", 4);
    if (has_saved && saved != 0 && saved != 2 && saved != 4 && saved > 0 && saved <= 16) {
        WCHAR text[32];
        StringCchPrintfW(text, sizeof(text) / sizeof(text[0]), L"Custom (%dx)", saved);
        combo_add(combo, text, (LPARAM)saved);
    }
    combo_select_data(combo, has_saved ? (LPARAM)saved : 0);
}

static void populate_audio_quality(HWND combo, const SudekiMpPlayerOptions *options) {
    static const WCHAR *const qualities[] = {L"High", L"Medium", L"Low"};
    char saved[SUDEKIMP_PLAYER_OPTION_VALUE_CAPACITY];
    WCHAR wide[SUDEKIMP_PLAYER_OPTION_VALUE_CAPACITY];
    size_t index;
    int selected = -1;
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    for (index = 0u; index < sizeof(qualities) / sizeof(qualities[0]); ++index) {
        combo_add(combo, qualities[index], (LPARAM)index);
    }
    if (options != NULL &&
        SudekiMpPlayerOptionsGet(options, "AudioQuality", "Value", saved, sizeof(saved),
                                 NULL)) {
        utf8_to_wide(saved, wide, (int)(sizeof(wide) / sizeof(wide[0])));
        selected = (int)SendMessageW(combo, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)wide);
        if (selected < 0 && wide[0] != L'\0') {
            selected = combo_add(combo, wide, (LPARAM)index);
        }
    }
    SendMessageW(combo, CB_SETCURSEL, (WPARAM)(selected < 0 ? 0 : selected), 0);
}

static void populate_checkbox(HWND checkbox,
                              const SudekiMpPlayerOptions *options,
                              const char *setting_id) {
    char value[16];
    const BOOL present = options != NULL &&
        SudekiMpPlayerOptionsGet(options, setting_id, "Value", value, sizeof(value), NULL);
    EnableWindow(checkbox, present);
    SendMessageW(checkbox, BM_SETCHECK,
                 present && strcmp(value, "True") == 0 ? BST_CHECKED : BST_UNCHECKED, 0);
}

static void populate_gamma(const SudekiMpSettingsControls *controls,
                           const SudekiMpPlayerOptions *options) {
    char value[SUDEKIMP_PLAYER_OPTION_VALUE_CAPACITY];
    double gamma = 1.0;
    int position;
    SendMessageW(controls->gamma, TBM_SETRANGE, TRUE, MAKELPARAM(0, SUDEKIMP_GAMMA_STEPS));
    if (options != NULL &&
        SudekiMpPlayerOptionsGet(options, "Gamma", "Value", value, sizeof(value), NULL)) {
        char *end;
        const double parsed = strtod(value, &end);
        if (end != value && *end == '\0') {
            gamma = parsed;
        }
    }
    position = (int)((gamma - SUDEKIMP_GAMMA_MIN) /
                     ((SUDEKIMP_GAMMA_MAX - SUDEKIMP_GAMMA_MIN) / SUDEKIMP_GAMMA_STEPS) + 0.5);
    if (position < 0) {
        position = 0;
    } else if (position > SUDEKIMP_GAMMA_STEPS) {
        position = SUDEKIMP_GAMMA_STEPS;
    }
    SendMessageW(controls->gamma, TBM_SETPOS, TRUE, position);
}

static double gamma_from_position(HWND trackbar) {
    const int position = (int)SendMessageW(trackbar, TBM_GETPOS, 0, 0);
    return SUDEKIMP_GAMMA_MIN +
           position * ((SUDEKIMP_GAMMA_MAX - SUDEKIMP_GAMMA_MIN) / SUDEKIMP_GAMMA_STEPS);
}

void SudekiMpSettingsUpdateGammaLabel(const SudekiMpSettingsControls *controls) {
    WCHAR text[16];
    StringCchPrintfW(text, sizeof(text) / sizeof(text[0]), L"%.2f",
                     gamma_from_position(controls->gamma));
    SetWindowTextW(controls->gamma_value, text);
}

/* Type 1 is a DirectInput key scan code (DIK_*). Type 2 is a mouse input;
   the names below are inferred from the vanilla default bindings (camera axes,
   weapon cycling on the wheel, actions on the buttons). */
static void format_binding(int key, int type, WCHAR *text, size_t text_count) {
    static const WCHAR *const mouse_inputs[] = {
        L"Mouse X axis", L"Mouse Y axis", L"Mouse wheel up",
        L"Mouse wheel down", L"Left mouse button", L"Right mouse button"
    };
    if (type == 0) {
        StringCchCopyW(text, text_count, L"—");
    } else if (type == 1 && key > 0 && key < 0x100) {
        const LONG parameter = ((LONG)(key & 0x7F) << 16) |
                               ((key & 0x80) != 0 ? (1L << 24) : 0L);
        if (GetKeyNameTextW(parameter, text, (int)text_count) <= 0) {
            StringCchPrintfW(text, text_count, L"Key %d", key);
        }
    } else if (type == 2 && key >= 0 &&
               key < (int)(sizeof(mouse_inputs) / sizeof(mouse_inputs[0]))) {
        StringCchCopyW(text, text_count, mouse_inputs[key]);
    } else {
        StringCchPrintfW(text, text_count, L"Input %d (device %d)", key, type);
    }
}

static void binding_text(const SudekiMpPlayerOptions *options,
                         const char *setting_id,
                         WCHAR *text,
                         size_t text_count) {
    int key = 0;
    int type = 0;
    if (!get_integer(options, setting_id, "Key", &key) ||
        !get_integer(options, setting_id, "Type", &type)) {
        StringCchCopyW(text, text_count, L"—");
        return;
    }
    format_binding(key, type, text, text_count);
}

static void collect_setting(const char *setting_id, void *context) {
    SettingList *list = (SettingList *)context;
    if (list->count < SUDEKIMP_MAX_SETTINGS) {
        StringCchCopyA(list->ids[list->count], SUDEKIMP_PLAYER_OPTION_ID_CAPACITY,
                       setting_id);
        ++list->count;
    }
}

static BOOL is_input(const SudekiMpPlayerOptions *options,
                     const char *setting_id,
                     char *control_name,
                     size_t control_name_count) {
    char type[16];
    return SudekiMpPlayerOptionsGet(options, setting_id, "ControlType", type, sizeof(type),
                                    NULL) &&
           strcmp(type, "Input") == 0 &&
           SudekiMpPlayerOptionsGet(options, setting_id, "ControlName0", control_name,
                                    control_name_count, NULL);
}

static void binding_label(const char *setting_id, WCHAR *label, size_t label_count) {
    size_t index;
    for (index = 0u; index < sizeof(binding_labels) / sizeof(binding_labels[0]); ++index) {
        if (strcmp(binding_labels[index].setting_id, setting_id) == 0) {
            StringCchCopyW(label, label_count, binding_labels[index].label);
            return;
        }
    }
    utf8_to_wide(setting_id, label, (int)label_count);
}

static void populate_bindings(HWND list, const SudekiMpPlayerOptions *options) {
    SettingList *settings;
    size_t index = 0u;
    int row = 0;
    ListView_DeleteAllItems(list);
    if (options == NULL) {
        return;
    }
    settings = (SettingList *)calloc(1u, sizeof(*settings));
    if (settings == NULL) {
        return;
    }
    SudekiMpPlayerOptionsForEachSetting(options, collect_setting, settings);
    while (index < settings->count) {
        char control[SUDEKIMP_PLAYER_OPTION_VALUE_CAPACITY];
        char next_control[SUDEKIMP_PLAYER_OPTION_VALUE_CAPACITY];
        WCHAR label[64];
        WCHAR primary[64];
        WCHAR secondary[64];
        LVITEMW item;
        if (!is_input(options, settings->ids[index], control, sizeof(control))) {
            ++index;
            continue;
        }
        binding_label(settings->ids[index], label, sizeof(label) / sizeof(label[0]));
        binding_text(options, settings->ids[index], primary,
                     sizeof(primary) / sizeof(primary[0]));
        StringCchCopyW(secondary, sizeof(secondary) / sizeof(secondary[0]), L"—");
        /* Sudeki stores each action's alternate binding as the next setting
           with the same native action name (Forward/Forward2, Skip1/Skip2). */
        if (index + 1u < settings->count &&
            is_input(options, settings->ids[index + 1u], next_control,
                     sizeof(next_control)) &&
            strcmp(control, next_control) == 0) {
            binding_text(options, settings->ids[index + 1u], secondary,
                         sizeof(secondary) / sizeof(secondary[0]));
            index += 2u;
        } else {
            ++index;
        }
        ZeroMemory(&item, sizeof(item));
        item.mask = LVIF_TEXT;
        item.iItem = row;
        item.pszText = label;
        if (ListView_InsertItem(list, &item) < 0) {
            break;
        }
        ListView_SetItemText(list, row, 1, primary);
        ListView_SetItemText(list, row, 2, secondary);
        ++row;
    }
    free(settings);
}

void SudekiMpSettingsInitialiseBindingColumns(HWND bindings) {
    static const WCHAR *const titles[] = {L"Action", L"Primary", L"Secondary"};
    static const int widths[] = {210, 170, 170};
    LVCOLUMNW column;
    int index;
    ZeroMemory(&column, sizeof(column));
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    for (index = 0; index < 3; ++index) {
        column.pszText = (LPWSTR)titles[index];
        column.cx = widths[index];
        column.iSubItem = index;
        ListView_InsertColumn(bindings, index, &column);
    }
}

void SudekiMpSettingsPopulate(const SudekiMpSettingsControls *controls,
                              const SudekiMpPlayerOptions *options) {
    int width = 0;
    int height = 0;
    int refresh = 0;
    const BOOL has_resolution = get_integer(options, "Resolution", "Width", &width) &&
                                get_integer(options, "Resolution", "Height", &height);
    const BOOL has_refresh = get_integer(options, "Refresh", "Refresh", &refresh);

    populate_resolutions(controls->resolution, width, height);
    populate_refresh_rates(controls->resolution, controls->refresh, refresh);
    EnableWindow(controls->resolution, has_resolution);
    EnableWindow(controls->refresh, has_refresh);

    populate_antialiasing(controls->antialiasing, options);
    EnableWindow(controls->antialiasing,
                 options != NULL && SudekiMpPlayerOptionsHas(options, "AntiAliasing", "Value"));
    populate_checkbox(controls->fullscreen, options, "FullScreen");
    populate_checkbox(controls->shadows, options, "Shadows");
    populate_gamma(controls, options);
    EnableWindow(controls->gamma,
                 options != NULL && SudekiMpPlayerOptionsHas(options, "Gamma", "Value"));
    SudekiMpSettingsUpdateGammaLabel(controls);
    populate_audio_quality(controls->audio_quality, options);
    EnableWindow(controls->audio_quality,
                 options != NULL && SudekiMpPlayerOptionsHas(options, "AudioQuality", "Value"));

    populate_checkbox(controls->mouse, options, "EnableMouse");
    populate_checkbox(controls->invert_x, options, "InvertX");
    populate_checkbox(controls->invert_y, options, "InvertY");
    populate_checkbox(controls->force_feedback, options, "ForceFeedback");
    populate_bindings(controls->bindings, options);
}

void SudekiMpSettingsResolutionChanged(const SudekiMpSettingsControls *controls) {
    LPARAM rate = 0;
    (void)combo_selected_data(controls->refresh, &rate);
    populate_refresh_rates(controls->resolution, controls->refresh, (int)rate);
}

static BOOL set_integer(SudekiMpPlayerOptions *options,
                        const char *setting_id,
                        const char *variant_id,
                        long value) {
    char text[24];
    return SUCCEEDED(StringCchPrintfA(text, sizeof(text), "%ld", value)) &&
           SudekiMpPlayerOptionsSet(options, setting_id, variant_id, text);
}

static BOOL apply_checkbox(HWND checkbox,
                           SudekiMpPlayerOptions *options,
                           const char *setting_id) {
    if (!IsWindowEnabled(checkbox)) {
        return TRUE;
    }
    return SudekiMpPlayerOptionsSet(
        options, setting_id, "Value",
        SendMessageW(checkbox, BM_GETCHECK, 0, 0) == BST_CHECKED ? "True" : "False");
}

BOOL SudekiMpSettingsApply(const SudekiMpSettingsControls *controls,
                           SudekiMpPlayerOptions *options,
                           const char **failed_setting) {
    LPARAM data;
    *failed_setting = NULL;
    if (IsWindowEnabled(controls->resolution) &&
        combo_selected_data(controls->resolution, &data)) {
        int width;
        int height;
        unpack_mode(data, &width, &height);
        if (!set_integer(options, "Resolution", "Width", width) ||
            !set_integer(options, "Resolution", "Height", height)) {
            *failed_setting = "Resolution";
            return FALSE;
        }
    }
    if (IsWindowEnabled(controls->refresh) && combo_selected_data(controls->refresh, &data) &&
        !set_integer(options, "Refresh", "Refresh", (long)data)) {
        *failed_setting = "Refresh";
        return FALSE;
    }
    if (IsWindowEnabled(controls->antialiasing) &&
        combo_selected_data(controls->antialiasing, &data) &&
        !set_integer(options, "AntiAliasing", "Value", (long)data)) {
        *failed_setting = "AntiAliasing";
        return FALSE;
    }
    if (IsWindowEnabled(controls->gamma)) {
        char text[24];
        if (FAILED(StringCchPrintfA(text, sizeof(text), "%.6f",
                                    gamma_from_position(controls->gamma))) ||
            !SudekiMpPlayerOptionsSet(options, "Gamma", "Value", text)) {
            *failed_setting = "Gamma";
            return FALSE;
        }
    }
    if (IsWindowEnabled(controls->audio_quality)) {
        WCHAR wide[SUDEKIMP_PLAYER_OPTION_VALUE_CAPACITY];
        char text[SUDEKIMP_PLAYER_OPTION_VALUE_CAPACITY];
        const int index = (int)SendMessageW(controls->audio_quality, CB_GETCURSEL, 0, 0);
        if (index >= 0 &&
            SendMessageW(controls->audio_quality, CB_GETLBTEXTLEN, (WPARAM)index, 0) <
                (LRESULT)(sizeof(wide) / sizeof(wide[0])) &&
            SendMessageW(controls->audio_quality, CB_GETLBTEXT, (WPARAM)index,
                         (LPARAM)wide) != CB_ERR &&
            (!wide_to_utf8(wide, text, (int)sizeof(text)) ||
             !SudekiMpPlayerOptionsSet(options, "AudioQuality", "Value", text))) {
            *failed_setting = "AudioQuality";
            return FALSE;
        }
    }
    if (!apply_checkbox(controls->fullscreen, options, "FullScreen")) {
        *failed_setting = "FullScreen";
    } else if (!apply_checkbox(controls->shadows, options, "Shadows")) {
        *failed_setting = "Shadows";
    } else if (!apply_checkbox(controls->mouse, options, "EnableMouse")) {
        *failed_setting = "EnableMouse";
    } else if (!apply_checkbox(controls->invert_x, options, "InvertX")) {
        *failed_setting = "InvertX";
    } else if (!apply_checkbox(controls->invert_y, options, "InvertY")) {
        *failed_setting = "InvertY";
    } else if (!apply_checkbox(controls->force_feedback, options, "ForceFeedback")) {
        *failed_setting = "ForceFeedback";
    }
    return *failed_setting == NULL;
}
