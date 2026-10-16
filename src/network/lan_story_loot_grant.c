#include "network/lan_story_loot_grant.h"
#include <string.h>
static void put32(uint8_t *p,uint32_t n) { for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(n>>(8u*i)); }
static uint32_t get32(const uint8_t *p) { uint32_t n=0;for(unsigned i=0;i<4;++i)n|=(uint32_t)p[i]<<(8u*i);return n; }
int SudekiMpStoryLootGrantValid(const SudekiMpStoryLootGrant *g) {
    return g && g->serial && g->previous<g->serial && g->item<999u &&
        g->quantity && g->quantity<=999u && g->show<=1u &&
        (g->source==SUDEKIMP_STORY_LOOT_GRANT_PICKUP || g->source==SUDEKIMP_STORY_LOOT_GRANT_CHEST);
}
int SudekiMpStoryLootGrantSame(const SudekiMpStoryLootGrant *a,const SudekiMpStoryLootGrant *b) {
    return a && b && a->serial==b->serial && a->previous==b->previous && a->item==b->item &&
        a->quantity==b->quantity && a->source==b->source && a->show==b->show;
}
int SudekiMpStoryLootGrantEncode(const SudekiMpStoryLootGrant *g,int ack,uint8_t *b,size_t n) {
    if(!b || n<SUDEKIMP_STORY_LOOT_GRANT_WIRE_SIZE || !SudekiMpStoryLootGrantValid(g)) return 0;
    memcpy(b,ack?"LGA1":"LGG1",4);put32(b+4,g->serial);put32(b+8,g->previous);
    put32(b+12,g->item);put32(b+16,g->quantity);b[20]=g->source;b[21]=g->show;b[22]=b[23]=0;return 1;
}
int SudekiMpStoryLootGrantDecode(const uint8_t *b,size_t n,int ack,SudekiMpStoryLootGrant *g) {
    if(!b || !g || n!=SUDEKIMP_STORY_LOOT_GRANT_WIRE_SIZE || memcmp(b,ack?"LGA1":"LGG1",4) || b[22] || b[23]) return 0;
    SudekiMpStoryLootGrant next={get32(b+4),get32(b+8),get32(b+12),get32(b+16),b[20],b[21]};
    if(!SudekiMpStoryLootGrantValid(&next)) return 0;
    *g=next;return 1;
}
int SudekiMpStoryLootGrantEnqueue(SudekiMpStoryLootGrantOutbox *q,const SudekiMpStoryLootGrant *g) {
    if(!q || !SudekiMpStoryLootGrantValid(g) || q->count>=SUDEKIMP_STORY_LOOT_GRANT_CAPACITY || g->previous!=q->last_serial) return 0;
    q->records[(q->head+q->count)%SUDEKIMP_STORY_LOOT_GRANT_CAPACITY]=*g;
    ++q->count;q->last_serial=g->serial;return 1;
}
int SudekiMpStoryLootGrantAcknowledge(SudekiMpStoryLootGrantOutbox *q,const SudekiMpStoryLootGrant *g) {
    if(!q || !q->count || !SudekiMpStoryLootGrantSame(&q->records[q->head],g)) return 0;
    memset(&q->records[q->head],0,sizeof(q->records[0]));q->head=(q->head+1u)%SUDEKIMP_STORY_LOOT_GRANT_CAPACITY;
    --q->count;q->last_sent_at=0;return 1;
}
int SudekiMpStoryLootGrantReceive(SudekiMpStoryLootGrantInbox *q,const SudekiMpStoryLootGrant *g) {
    if(!q || q->quarantined || !SudekiMpStoryLootGrantValid(g)) return 0;
    if(g->serial==q->committed.serial) return SudekiMpStoryLootGrantSame(g,&q->committed)?2:0;
    if(q->available) return SudekiMpStoryLootGrantSame(g,&q->pending)?1:0;
    if(g->serial<=q->committed.serial || g->previous!=q->committed.serial) return 0;
    q->pending=*g;q->available=1;q->taken=0;return 1;
}
int SudekiMpStoryLootGrantPeek(const SudekiMpStoryLootGrantInbox *q,SudekiMpStoryLootGrant *g) {
    if(!q || !g || q->quarantined || !q->available || q->taken) return 0;
    *g=q->pending;return 1;
}
int SudekiMpStoryLootGrantTake(SudekiMpStoryLootGrantInbox *q,const SudekiMpStoryLootGrant *g) {
    if(!q || q->quarantined || !q->available || q->taken || !SudekiMpStoryLootGrantSame(g,&q->pending)) return 0;
    q->taken=1;return 1;
}
int SudekiMpStoryLootGrantComplete(SudekiMpStoryLootGrantInbox *q,const SudekiMpStoryLootGrant *g,int applied) {
    if(!q || q->quarantined || !q->available || !q->taken || !SudekiMpStoryLootGrantSame(g,&q->pending)) return 0;
    if(!applied) {q->quarantined=1;return 0;}
    q->committed=q->pending;memset(&q->pending,0,sizeof(q->pending));q->available=q->taken=0;return 1;
}
