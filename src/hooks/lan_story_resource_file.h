#ifndef SUDEKIMP_LAN_STORY_RESOURCE_FILE_H
#define SUDEKIMP_LAN_STORY_RESOURCE_FILE_H

#include <windows.h>
#include <stdint.h>

/* Read-only proof for the supported archive-backed synchronous stream path.
 * No file is opened, no resource is acquired, and no native call is made here. */
BOOL SudekiMpLanStoryResourceFileInitialize(HMODULE image);

/* Re-evaluate immediately before the caller's owned native resource request.
 * The caller separately owns the package, native thread and paused world.
 * Unknown mounted implementations and resource-name fallback are rejected. */
BOOL SudekiMpLanStoryResourceFileExact(uint32_t handle,uint32_t offset,uint32_t size);

#endif
