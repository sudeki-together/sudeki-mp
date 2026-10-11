#ifndef SUDEKIMP_LAN_STORY_MATERIAL_H
#define SUDEKIMP_LAN_STORY_MATERIAL_H
#include <windows.h>

/* Read-only closure for the supported NPC skin model's native material
 * prepare/clone/retirement path. No references, hooks or allocations are
 * acquired. A bounded, nonblocking inspection borrows the initialized native
 * texture-job lock only while observing the actual backing dependencies.
 * ERROR_IO_PENDING means a required resource/lock is busy, not lost ownership.
 * The caller separately proves this child belongs to its exact
 * parent renderer/bank and closes the selected property target and curve. */
BOOL SudekiMpLanStoryMaterialInitialize(HMODULE image);
BOOL SudekiMpLanStoryMaterialOwnerExact(HMODULE image,const void *child,
    const void *resource);
/* Read-only closure of the exact SOL members3..5 constant expansion with
 * authored texture fields retained, including
 * cache/factory dispatch and derived-definition cleanup. Parent/slot ownership
 * and native prepare/clone admission remain the caller's separate proof. */
BOOL SudekiMpLanStoryMaterialShaderOutputExact(HMODULE image,const void *material,
    const void *original,unsigned member);
#endif
