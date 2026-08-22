#ifndef SUDEKIMP_ACCELERATOR_CACHE_H
#define SUDEKIMP_ACCELERATOR_CACHE_H

#include <windows.h>
#include <stdint.h>

#define SUDEKIMP_LOAD_ACCELERATORS_IAT_RVA 0x0029a1f0u
#define SUDEKIMP_LOAD_ACCELERATORS_IMPORT_NAME_RVA 0x0030bb7eu
#define SUDEKIMP_MESSAGE_PUMP_ACCELERATOR_CALL_RVA 0x0028bf55u
#define SUDEKIMP_ACCELERATOR_RESOURCE_ID 101u

typedef HACCEL (WINAPI *SudekiMpLoadAcceleratorsAFunction)(
    HINSTANCE instance,
    LPCSTR table_name
);

typedef struct SudekiMpAcceleratorCacheState {
    HMODULE game_module;
    void *volatile cached_handle;
} SudekiMpAcceleratorCacheState;

HACCEL SudekiMpAcceleratorCacheLoad(
    SudekiMpAcceleratorCacheState *state,
    SudekiMpLoadAcceleratorsAFunction original,
    HINSTANCE instance,
    LPCSTR table_name
);

BOOL SudekiMpInstallAcceleratorCache(HMODULE game_module);
BOOL SudekiMpUninstallAcceleratorCache(void);

#endif
