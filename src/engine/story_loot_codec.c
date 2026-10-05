#include "engine/story_loot_codec.h"
#include <string.h>
static void put32(uint8_t *b,uint32_t x) { for(unsigned i=0;i<4;++i) b[i]=(uint8_t)(x>>(8*i)); }
static uint32_t get32(const uint8_t *b) {
    uint32_t x=0; for(unsigned i=0;i<4;++i) x|=(uint32_t)b[i]<<(8*i); return x;
}
static void put64(uint8_t *b,uint64_t x) { put32(b,(uint32_t)x); put32(b+4,(uint32_t)(x>>32)); }
static uint64_t get64(const uint8_t *b) { return get32(b)|((uint64_t)get32(b+4)<<32); }
static uint32_t checksum(const uint8_t *b,size_t n) {
    uint32_t hash=2166136261u;
    for(size_t i=0;i<n;++i) if(i<116 || i>=120) hash=(hash^b[i])*16777619u;
    return hash;
}
size_t SudekiMpStoryLootEncodedSize(const SudekiMpStoryLootState *s) {
    return SudekiMpStoryLootValid(s)?SUDEKIMP_STORY_LOOT_HEADER_SIZE+
        s->item_count*SUDEKIMP_STORY_LOOT_ITEM_SIZE+s->source_count*SUDEKIMP_STORY_LOOT_SOURCE_SIZE:0;
}
int SudekiMpStoryLootEncode(const SudekiMpStoryLootState *s,uint8_t *b,size_t capacity,size_t *written) {
    size_t n=SudekiMpStoryLootEncodedSize(s);
    if(!n || !b || !written || capacity<n) return 0;
    memset(b,0,n); memcpy(b,"SMPLOOT1",8);
    put32(b+8,SUDEKIMP_STORY_LOOT_SCHEMA); put32(b+12,(uint32_t)n);
    memcpy(b+16,s->save_identity,32); put64(b+48,s->revision);
    put64(b+56,s->visit); put64(b+64,s->operation); put32(b+72,s->reserve);
    for(unsigned c=0;c<4;++c) {
        put32(b+76+c*8,(uint32_t)SudekiMpPersonalWalletCharacterIdAt(c));
        put32(b+80+c*8,s->money[c]);
    }
    put32(b+108,s->item_count); put32(b+112,s->source_count);
    size_t at=SUDEKIMP_STORY_LOOT_HEADER_SIZE;
    for(unsigned i=0;i<s->item_count;++i,at+=SUDEKIMP_STORY_LOOT_ITEM_SIZE) {
        const SudekiMpStoryLootItem *item=&s->items[i];
        put32(b+at,item->id); put32(b+at+4,item->cap); put32(b+at+8,item->kind);
        for(unsigned c=0;c<4;++c) put32(b+at+12+c*4,item->quantity[c]);
    }
    for(unsigned i=0;i<s->source_count;++i,at+=SUDEKIMP_STORY_LOOT_SOURCE_SIZE) {
        put64(b+at,s->sources[i].id); put64(b+at+8,s->sources[i].operation);
        put32(b+at+16,s->sources[i].kind); /* final four bytes reserved zero */
    }
    put32(b+116,checksum(b,n)); *written=n; return 1;
}
int SudekiMpStoryLootDecode(const uint8_t *b,size_t n,const uint8_t expected[32],SudekiMpStoryLootState *out) {
    if(!b || !out || !expected || n<SUDEKIMP_STORY_LOOT_HEADER_SIZE ||
        n>SUDEKIMP_STORY_LOOT_MAX_SIZE || memcmp(b,"SMPLOOT1",8) ||
        get32(b+8)!=SUDEKIMP_STORY_LOOT_SCHEMA || get32(b+12)!=n ||
        memcmp(b+16,expected,32) || get32(b+116)!=checksum(b,n)) return 0;
    uint32_t items=get32(b+108),sources=get32(b+112);
    if(items>SUDEKIMP_STORY_LOOT_MAX_ITEMS || sources>SUDEKIMP_STORY_LOOT_MAX_SOURCES ||
        n!=SUDEKIMP_STORY_LOOT_HEADER_SIZE+items*SUDEKIMP_STORY_LOOT_ITEM_SIZE+
            sources*SUDEKIMP_STORY_LOOT_SOURCE_SIZE) return 0;
    SudekiMpStoryLootState s={0};
    memcpy(s.save_identity,b+16,32); s.revision=get64(b+48); s.visit=get64(b+56);
    s.operation=get64(b+64); s.reserve=get32(b+72);
    for(unsigned c=0;c<4;++c) {
        if(get32(b+76+c*8)!=(uint32_t)SudekiMpPersonalWalletCharacterIdAt(c)) return 0;
        s.money[c]=get32(b+80+c*8);
    }
    s.item_count=items; s.source_count=sources;
    size_t at=SUDEKIMP_STORY_LOOT_HEADER_SIZE;
    for(unsigned i=0;i<items;++i,at+=SUDEKIMP_STORY_LOOT_ITEM_SIZE) {
        s.items[i].id=get32(b+at); s.items[i].cap=get32(b+at+4); s.items[i].kind=get32(b+at+8);
        for(unsigned c=0;c<4;++c) s.items[i].quantity[c]=get32(b+at+12+c*4);
    }
    for(unsigned i=0;i<sources;++i,at+=SUDEKIMP_STORY_LOOT_SOURCE_SIZE) {
        if(get32(b+at+20)) return 0;
        s.sources[i].id=get64(b+at); s.sources[i].operation=get64(b+at+8); s.sources[i].kind=get32(b+at+16);
    }
    if(!SudekiMpStoryLootValid(&s)) return 0;
    *out=s; return 1;
}
