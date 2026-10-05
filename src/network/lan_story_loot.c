#include "network/lan_story_loot.h"
#include <string.h>
_Static_assert(SUDEKIMP_STORY_LOOT_MAX_CHUNKS<32,"bounded fragment mask");
static void put32(uint8_t *b,uint32_t x) { for(unsigned i=0;i<4;++i) b[i]=(uint8_t)(x>>(8*i)); }
static uint32_t get32(const uint8_t *b) {
    uint32_t x=0; for(unsigned i=0;i<4;++i) x|=(uint32_t)b[i]<<(8*i); return x;
}
static uint32_t digest(const uint8_t *b,size_t n) {
    uint32_t h=2166136261u; for(size_t i=0;i<n;++i) h=(h^b[i])*16777619u; return h;
}
static int chunk_valid(const SudekiMpLanStoryLootChunk *c) {
    if(!c || !c->epoch || !c->scene_revision || !c->revision ||
        c->total_size<SUDEKIMP_STORY_LOOT_HEADER_SIZE || c->total_size>SUDEKIMP_STORY_LOOT_MAX_SIZE ||
        c->count!=(c->total_size+SUDEKIMP_STORY_LOOT_CHUNK_DATA-1)/SUDEKIMP_STORY_LOOT_CHUNK_DATA ||
        c->index>=c->count) return 0;
    uint32_t n=c->total_size-c->index*SUDEKIMP_STORY_LOOT_CHUNK_DATA;
    if(n>SUDEKIMP_STORY_LOOT_CHUNK_DATA) n=SUDEKIMP_STORY_LOOT_CHUNK_DATA;
    return c->size==n;
}
unsigned SudekiMpLanStoryLootChunkCount(const SudekiMpStoryLootState *s) {
    size_t n=SudekiMpStoryLootEncodedSize(s);
    return (unsigned)((n+SUDEKIMP_STORY_LOOT_CHUNK_DATA-1)/SUDEKIMP_STORY_LOOT_CHUNK_DATA);
}
int SudekiMpLanStoryLootEncode(const SudekiMpStoryLootState *s,const SudekiMpLanStoryScene *scene,
    unsigned index,uint8_t *b,size_t capacity,size_t *written) {
    uint8_t bytes[SUDEKIMP_STORY_LOOT_MAX_SIZE]; size_t n;
    if(!b || !written || !SudekiMpLanStorySceneValid(scene) || scene->phase!=SUDEKIMP_LAN_STORY_READY ||
        !SudekiMpStoryLootEncode(s,bytes,sizeof(bytes),&n) || !s->visit) return 0;
    unsigned count=(unsigned)((n+SUDEKIMP_STORY_LOOT_CHUNK_DATA-1)/SUDEKIMP_STORY_LOOT_CHUNK_DATA);
    if(index>=count) return 0;
    size_t offset=index*SUDEKIMP_STORY_LOOT_CHUNK_DATA,part=n-offset;
    if(part>SUDEKIMP_STORY_LOOT_CHUNK_DATA) part=SUDEKIMP_STORY_LOOT_CHUNK_DATA;
    if(capacity<SUDEKIMP_STORY_LOOT_CHUNK_HEADER+part) return 0;
    memcpy(b,"SLT1",4); put32(b+4,scene->epoch); put32(b+8,scene->revision);
    put32(b+12,scene->observed_tick); put32(b+16,(uint32_t)s->revision);
    put32(b+20,(uint32_t)(s->revision>>32)); put32(b+24,(uint32_t)n);
    put32(b+28,digest(bytes,n)); put32(b+32,index); put32(b+36,count);
    memcpy(b+SUDEKIMP_STORY_LOOT_CHUNK_HEADER,bytes+offset,part);
    *written=SUDEKIMP_STORY_LOOT_CHUNK_HEADER+part; return 1;
}
int SudekiMpLanStoryLootDecode(const uint8_t *b,size_t n,SudekiMpLanStoryLootChunk *out) {
    if(!b || !out || n<=SUDEKIMP_STORY_LOOT_CHUNK_HEADER || n>SUDEKIMP_STORY_LOOT_CHUNK_MAX_SIZE ||
        memcmp(b,"SLT1",4)) return 0;
    SudekiMpLanStoryLootChunk c={0};
    c.epoch=get32(b+4); c.scene_revision=get32(b+8); c.host_tick=get32(b+12);
    c.revision=get32(b+16)|((uint64_t)get32(b+20)<<32); c.total_size=get32(b+24);
    c.digest=get32(b+28); c.index=get32(b+32); c.count=get32(b+36);
    c.size=(uint32_t)(n-SUDEKIMP_STORY_LOOT_CHUNK_HEADER);
    if(!chunk_valid(&c)) return 0;
    memcpy(c.bytes,b+SUDEKIMP_STORY_LOOT_CHUNK_HEADER,c.size); *out=c; return 1;
}
static int key_equal(const SudekiMpLanStoryLootChunk *a,const SudekiMpLanStoryLootChunk *b) {
    return a->epoch==b->epoch && a->scene_revision==b->scene_revision && a->host_tick==b->host_tick &&
        a->revision==b->revision && a->total_size==b->total_size && a->digest==b->digest && a->count==b->count;
}
int SudekiMpLanStoryLootAccept(SudekiMpLanStoryLootAssembly *a,const SudekiMpLanStoryLootChunk *c,
    uint32_t now,const uint8_t expected[32],SudekiMpStoryLootState *out) {
    if(!a || !expected || !out || !chunk_valid(c)) return 0;
    if(a->mask) {
        if(c->epoch!=a->key.epoch || c->scene_revision!=a->key.scene_revision ||
            c->revision<a->key.revision) return 0;
        if(c->revision==a->key.revision) {
            if(!key_equal(&a->key,c)) {
                /* A fresh host observation can retry the same immutable
                 * account revision after loss; an old fragment cannot. */
                if(c->total_size!=a->key.total_size || c->digest!=a->key.digest ||
                    c->count!=a->key.count || (int32_t)(c->host_tick-a->key.host_tick)<=0) return 0;
                a->mask=0;
            } else if((uint32_t)(now-a->started_at)>SUDEKIMP_STORY_LOOT_ASSEMBLY_AGE_MS) return 0;
        } else a->mask=0;
    }
    if(!a->mask) { a->key=*c; a->started_at=now; }
    size_t offset=c->index*SUDEKIMP_STORY_LOOT_CHUNK_DATA;
    if(a->mask&(1u<<c->index)) return !memcmp(a->bytes+offset,c->bytes,c->size);
    memcpy(a->bytes+offset,c->bytes,c->size); a->mask|=1u<<c->index;
    if(a->mask!=(1u<<c->count)-1u) return 1;
    SudekiMpStoryLootState decoded;
    if(digest(a->bytes,c->total_size)!=c->digest ||
        !SudekiMpStoryLootDecode(a->bytes,c->total_size,expected,&decoded) ||
        decoded.revision!=c->revision || !decoded.visit) return 0;
    *out=decoded; return 2;
}
