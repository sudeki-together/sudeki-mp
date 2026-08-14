#ifndef SUDEKIMP_FREEROAM_CAMERA_INPUT_H
#define SUDEKIMP_FREEROAM_CAMERA_INPUT_H

#include <windows.h>

BOOL SudekiMpInstallFreeRoamCameraInput(HMODULE game_module, UINT modifier_key);
void SudekiMpUninstallFreeRoamCameraInput(void);

#endif
