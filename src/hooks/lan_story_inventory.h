#ifndef SUDEKIMP_LAN_STORY_INVENTORY_H
#define SUDEKIMP_LAN_STORY_INVENTORY_H
#include <windows.h>
#include <stdint.h>

/* One owner for the six exact AddItem callsites. Only pickup/chest sites
 * publish loot; export and merchant invocations remain observation-only. */
typedef struct SudekiMpStoryInventoryEvent {
    uint32_t serial,tick,site;
    int32_t item,quantity,show;
    uint32_t source,before,after,money,capture_valid;
} SudekiMpStoryInventoryEvent;
/* capture_valid=0 reports an observed native loot call with unknown ownership/
 * result; close replication admission instead of treating it as a grant. */
typedef void (*SudekiMpStoryInventoryObserver)(
    const SudekiMpStoryInventoryEvent *event,void *context);
/* Initialize is sufficient on a client; Install additionally owns the host
 * callsites. Bind must be called on the verified game thread. */
BOOL SudekiMpLanStoryInventoryInitialize(HMODULE image);
BOOL SudekiMpLanStoryInventoryInstall(HMODULE image);
BOOL SudekiMpLanStoryInventoryBind(DWORD thread,
    SudekiMpStoryInventoryObserver observer,void *context);
BOOL SudekiMpLanStoryInventoryUninstall(void);
BOOL SudekiMpLanStoryInventoryLast(SudekiMpStoryInventoryEvent *out);
/* Shared world-pickup guard accepts only retail target or this owner. */
BOOL SudekiMpLanStoryInventoryPickupCallExact(HMODULE image);

typedef struct SudekiMpStoryInventoryGrant {
    void *inventory,*database,*definition;
    uint32_t generation,item,quantity,kind,before,unit_value,used;
} SudekiMpStoryInventoryGrant;
typedef enum SudekiMpStoryInventoryGrantStatus {
    SUDEKIMP_STORY_INVENTORY_REFUSED=0,
    SUDEKIMP_STORY_INVENTORY_GRANTED=1,
    SUDEKIMP_STORY_INVENTORY_CAPPED=2,
    SUDEKIMP_STORY_INVENTORY_UNCERTAIN=3
} SudekiMpStoryInventoryGrantStatus;
typedef struct SudekiMpStoryInventoryGrantResult {
    SudekiMpStoryInventoryGrantStatus status;
    uint32_t before,after,requested,money;
} SudekiMpStoryInventoryGrantResult;
/* Prepare performs no native mutation. Consume the transport serial between
 * Prepare and Apply. Apply returns FALSE only if native AddItem was NOT entered;
 * TRUE includes an uncertain post-call observation and must never be retried.
 * Each lease is single-use. Neither lease nor pointers may cross threads. */
BOOL SudekiMpLanStoryInventoryPrepare(uint32_t item,uint32_t quantity,
    SudekiMpStoryInventoryGrant *grant);
BOOL SudekiMpLanStoryInventoryApply(SudekiMpStoryInventoryGrant *grant,
    SudekiMpStoryInventoryGrantResult *result);
#endif
