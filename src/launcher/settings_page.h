#ifndef SUDEKIMP_SETTINGS_PAGE_H
#define SUDEKIMP_SETTINGS_PAGE_H

#include <windows.h>

#include "player_options.h"

/* Win32 controls bound to Sudeki's own launcher options. Only options that
   exist in the loaded document are enabled; nothing is invented. */
typedef struct SudekiMpSettingsControls {
    HWND resolution;
    HWND refresh;
    HWND antialiasing;
    HWND fullscreen;
    HWND shadows;
    HWND gamma;
    HWND gamma_value;
    HWND audio_quality;
    HWND mouse;
    HWND invert_x;
    HWND invert_y;
    HWND force_feedback;
    HWND bindings;
} SudekiMpSettingsControls;

#define SUDEKIMP_GAMMA_STEPS 24
#define SUDEKIMP_GAMMA_MIN 0.5
#define SUDEKIMP_GAMMA_MAX 2.0

void SudekiMpSettingsInitialiseBindingColumns(HWND bindings);
/* options may be NULL: every bound control is then cleared and disabled. */
void SudekiMpSettingsPopulate(const SudekiMpSettingsControls *controls,
                              const SudekiMpPlayerOptions *options);
/* Re-lists refresh rates for the selected resolution, keeping the current
   rate when that mode still offers it. */
void SudekiMpSettingsResolutionChanged(const SudekiMpSettingsControls *controls);
void SudekiMpSettingsUpdateGammaLabel(const SudekiMpSettingsControls *controls);
/* Writes enabled controls into options. On failure *failed_setting names the
   option that could not be written and options may be partially edited. */
BOOL SudekiMpSettingsApply(const SudekiMpSettingsControls *controls,
                           SudekiMpPlayerOptions *options,
                           const char **failed_setting);

#endif
