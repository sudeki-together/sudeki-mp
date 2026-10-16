#ifndef SUDEKIMP_LAN_STORY_LOOT_GRANT_H
#define SUDEKIMP_LAN_STORY_LOOT_GRANT_H
#include <stddef.h>
#include <stdint.h>

/* D9 inventory grants are per game, unlike the inactive four-character
 * story_loot account model. Only these positively classified native sources
 * are eligible. Shops and generic script AddItem calls have no wire value. */
enum { SUDEKIMP_STORY_LOOT_GRANT_PICKUP=1, SUDEKIMP_STORY_LOOT_GRANT_CHEST=2,
    SUDEKIMP_STORY_LOOT_GRANT_CAPACITY=128, SUDEKIMP_STORY_LOOT_GRANT_WIRE_SIZE=24,
    SUDEKIMP_STORY_LOOT_GRANT_RETRY_MS=100 };
typedef struct SudekiMpStoryLootGrant {
    uint32_t serial,previous,item,quantity;
    uint8_t source,show;
} SudekiMpStoryLootGrant;
typedef struct SudekiMpStoryLootGrantOutbox {
    SudekiMpStoryLootGrant records[SUDEKIMP_STORY_LOOT_GRANT_CAPACITY];
    uint32_t last_serial,last_sent_at;
    unsigned head,count;
} SudekiMpStoryLootGrantOutbox;
typedef struct SudekiMpStoryLootGrantInbox {
    SudekiMpStoryLootGrant committed,pending;
    uint8_t available,taken,quarantined;
} SudekiMpStoryLootGrantInbox;
int SudekiMpStoryLootGrantValid(const SudekiMpStoryLootGrant *);
int SudekiMpStoryLootGrantSame(const SudekiMpStoryLootGrant *,const SudekiMpStoryLootGrant *);
int SudekiMpStoryLootGrantEncode(const SudekiMpStoryLootGrant *,int acknowledgement,uint8_t *,size_t);
int SudekiMpStoryLootGrantDecode(const uint8_t *,size_t,int acknowledgement,SudekiMpStoryLootGrant *);
/* No overwrite: enqueue failure leaves the outbox untouched. */
int SudekiMpStoryLootGrantEnqueue(SudekiMpStoryLootGrantOutbox *,const SudekiMpStoryLootGrant *);
int SudekiMpStoryLootGrantAcknowledge(SudekiMpStoryLootGrantOutbox *,const SudekiMpStoryLootGrant *);
/* 0 refused, 1 pending (including repeat), 2 already committed: ACK again.
 * Predecessor chaining rejects gaps; repeats never clear a taken claim. */
int SudekiMpStoryLootGrantReceive(SudekiMpStoryLootGrantInbox *,const SudekiMpStoryLootGrant *);
int SudekiMpStoryLootGrantPeek(const SudekiMpStoryLootGrantInbox *,SudekiMpStoryLootGrant *);
int SudekiMpStoryLootGrantTake(SudekiMpStoryLootGrantInbox *,const SudekiMpStoryLootGrant *);
/* Call only after the game thread claimed the exact record. Unknown/failed
 * native outcome quarantines it; a resend can never execute it again. */
int SudekiMpStoryLootGrantComplete(SudekiMpStoryLootGrantInbox *,const SudekiMpStoryLootGrant *,int applied);
#endif
