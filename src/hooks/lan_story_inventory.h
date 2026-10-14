#ifndef SUDEKIMP_LAN_STORY_INVENTORY_H
#define SUDEKIMP_LAN_STORY_INVENTORY_H
#include <windows.h>
#include <stdint.h>

/* Host inventory observer (#66 / D6 ledger groundwork). Observation only: the
 * six exact call sites of CInventory::AddItem (RVA 0x217E0; the three
 * AddItemToInventory exports, the shop/script path, the pickup path and the
 * chest reward path) are redirected to a thiscall stub that records
 * (site, item, quantity, show) and tail-calls the original. Nothing is
 * added, refused or re-routed. Exact-image gated; installed on the host only. */
BOOL SudekiMpLanStoryInventoryInstall(HMODULE image);
BOOL SudekiMpLanStoryInventoryUninstall(void);
typedef struct SudekiMpStoryInventoryEvent { uint32_t serial,tick,site; int32_t item,quantity,show; } SudekiMpStoryInventoryEvent;
/* Latest observed add (serial 0 = none). */
BOOL SudekiMpLanStoryInventoryLast(SudekiMpStoryInventoryEvent *out);
#endif
