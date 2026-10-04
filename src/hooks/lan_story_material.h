#ifndef SUDEKIMP_LAN_STORY_MATERIAL_H
#define SUDEKIMP_LAN_STORY_MATERIAL_H
#include <windows.h>

/* Read-only closure for the supported NPC skin model's native material
 * prepare/clone/retirement path. No references, hooks or allocations are
 * acquired. The caller separately proves this child belongs to its exact
 * parent renderer/bank and closes the selected property target and curve. */
BOOL SudekiMpLanStoryMaterialInitialize(HMODULE image);
BOOL SudekiMpLanStoryMaterialOwnerExact(HMODULE image,const void *child,
    const void *resource);
#endif
