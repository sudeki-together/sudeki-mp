#ifndef SUDEKIMP_LAN_STORY_AREA_EVICTION_H
#define SUDEKIMP_LAN_STORY_AREA_EVICTION_H
#include "engine/story_area.h"
#include "hooks/control_separation.h"

/* Experimental descriptor-eviction reservation. Not installed by runtime.
 * Owns the two state-zero calls in native automatic zone swapping, exported
 * named UnloadZone, both RemoveLastActiveZone state-zero calls, and
 * per-descriptor admission in RemoveAllNonActiveZones. Reserved descriptors
 * skip those evictions; unrelated candidates and all new load requests retain
 * native behaviour. Does not change the swapping flag.
 * This is NOT a general native area reference: other explicit teardown,
 * RemoveAllZones, UnloadNexus, world replacement, TEMP suspension and resource destruction
 * are NOT covered. A skipped named unload is not queued/replayed on release.
 * RemoveAllNonActiveZones still clears global pending world slots+0x14/+0x18
 * and toggles world+0x39D; the coordinator must separately own those contexts.
 * The coordinator must own those boundaries and native world/table lifetime
 * before using this as part of a playable occupied-area lease. */
BOOL SudekiMpLanStoryAreaEvictionInstall(HMODULE);
/* Suspended startup may pass NULL; after a native event or reservation, a fresh post-controller
 * witness on the established native thread is required. Outstanding tickets,
 * unknown observations or failed restoration retain all dependencies. */
BOOL SudekiMpLanStoryAreaEvictionUninstall(const SudekiMpControlUpdateDispatchWitness *);
/* Reserve BEFORE a covered path can evict this descriptor. Exact native
 * descriptor/name/world-table capture plus a policy pin; not native loading,
 * collision activation or proof of strong native retention. Policy and native
 * owner storage must outlive the ticket. Tickets never repeat on reinstall.
 * All reservations belong to one exact native world/table generation. */
BOOL SudekiMpLanStoryAreaEvictionReserve(HMODULE,const SudekiMpControlUpdateDispatchWitness *,
    SudekiMpStoryAreas *,SudekiMpStoryAreaRef,const void *descriptor,uint64_t *ticket);
/* Caller has ended the need for this descriptor-eviction reservation, while
 * still retaining its native owner. No deferred unload is replayed here.
 * Failure leaves the reservation/policy pin intact. */
BOOL SudekiMpLanStoryAreaEvictionRelease(HMODULE,const SudekiMpControlUpdateDispatchWitness *,uint64_t);
#endif
