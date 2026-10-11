/* Production registry walk with synthetic retained objects. No game image,
 * native calls, pause acquisition, hooks or process injection. */
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned memory_queries;
static SIZE_T WINAPI counted_query(LPCVOID p,PMEMORY_BASIC_INFORMATION m,SIZE_T n) {
    ++memory_queries; return VirtualQuery(p,m,n);
}
#define VirtualQuery counted_query
#include "../src/hooks/lan_story_client.c"
#undef VirtualQuery

/* Retained assembly bridge dependencies must never be reached by this walk. */
BOOL SudekiMpLanPartyMenuNativeOwnsPause(void) { assert(0); return FALSE; }
void SudekiMpLogFormat(const char *format,...) { (void)format; assert(0); }

enum { COUNT=512, STRIDE=0x160 };
static uint8_t registry[0x40],scheduler[0x20],speed[0x30],gel[0x4c];
static uint8_t manager[0x1008c],trigger[0x200],cluster[0x40];
static uint8_t *items[COUNT],*entities;
static void setup(void) {
    base=VirtualAlloc(NULL,0x420000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    entities=VirtualAlloc(NULL,COUNT*STRIDE,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(base && entities); installed=TRUE; native_thread=GetCurrentThreadId();
    world_pause=(WorldPauseObservation){.registry=registry,.scheduler=scheduler,
        .speed=speed,.gel=gel,.manager=manager,.trigger=trigger,.cluster=cluster,
        .entities=items,.count=COUNT};
    *(void **)(base+REGISTRY_GLOBAL)=registry; *(void **)(base+SCHEDULER_GLOBAL)=scheduler;
    *(void **)(base+SPEED_GLOBAL)=speed; *(void **)(base+GEL_GLOBAL)=gel;
    *(void **)(base+TRIGGER_GLOBAL)=trigger; *(void **)(base+CLUSTER_GLOBAL)=cluster;
    *(void **)(gel+0x30)=manager; *(unsigned *)(registry+0x34)=COUNT;
    *(void **)(registry+0x3c)=items; *(uint16_t *)(speed+0x2a)=1;
    speed[0x28]=gel[0x23]=1;
    *(void **)trigger=base+0x2c5458; *(void **)(trigger+8)=base+0x2c5460;
    *(void **)(trigger+0x10)=base+0x2c5470;
    *(void **)cluster=base+0x2c7b30; *(void **)(cluster+8)=base+0x2c7b44;
    *(void **)(cluster+0x1c)=base+0x2c7b4c;
    for(unsigned i=0;i<COUNT;++i) {
        uint8_t *e=items[i]=entities+i*STRIDE;
        *(void **)e=base+0x200; *(void **)(e+8)=base+0x300; e[0x2b]=1; e[0x144]=4;
        entity_pause[i]=(EntityPauseObservation){e,base+0x200,base+0x300,0,4,0};
    }
}
static void queries_and_mutations(void) {
    installed=FALSE; memory_queries=0; assert(registry_exact(1));
    unsigned uncached=memory_queries; installed=TRUE; memory_queries=0;
    assert(registry_exact(1)); unsigned cached=memory_queries;
    assert(uncached>=COUNT && cached<16 && !region_cache_active && !region_count);
    printf("client registry: %u entities, page queries %u -> %u; complete entity walk retained\n",
        COUNT,uncached,cached);
    uint8_t *last=items[COUNT-1];
    *(void **)last=base+0x400; assert(!registry_exact(1) && !region_cache_active);
    *(void **)last=base+0x200;
    *(void **)(last+8)=base+0x400; assert(!registry_exact(1));
    *(void **)(last+8)=base+0x300;
    last[0x2b]=2; assert(!registry_exact(1)); last[0x2b]=1;
    last[0x144]=0; assert(!registry_exact(1)); last[0x144]=4;
    items[COUNT-1]=items[0]; assert(!registry_exact(1)); items[COUNT-1]=last;
    *(unsigned *)(registry+0x34)=COUNT-1; assert(!registry_exact(1));
    *(unsigned *)(registry+0x34)=COUNT;
    *(uint16_t *)(speed+0x2a)=2; assert(!registry_exact(1)); *(uint16_t *)(speed+0x2a)=1;
    assert(registry_exact(1));
    /* Each whole walk starts afresh even within an existing presentation. */
    region_cache_active=TRUE; assert(readable(last,0x145));
    assert(registry_exact(1) && region_cache_active && !region_count);
    last[0x2b]=2; assert(!registry_exact(1) && region_cache_active && !region_count);
    last[0x2b]=1; region_cache_active=FALSE;
    puts("client registry: owner, update vtable, pause, scheduler bit and membership mutations refused");
}
static void permissions_and_retry(void) {
    DWORD old; assert(registry_exact(1));
    assert(VirtualProtect(entities,4096,PAGE_NOACCESS,&old));
    assert(!registry_exact(1) && !region_cache_active && !region_count);
    assert(VirtualProtect(entities,4096,PAGE_READWRITE|PAGE_GUARD,&old));
    assert(!registry_exact(1) && !region_cache_active);
    assert(VirtualProtect(entities,4096,PAGE_READWRITE,&old));
    memory_queries=0; assert(registry_exact(1) && memory_queries);
    assert(!readable(NULL,1) && !readable(entities,0) &&
        !readable((void *)(uintptr_t)(UINTPTR_MAX-1u),8));
    assert(VirtualFree(entities,0,MEM_RELEASE));
    assert(!registry_exact(1) && !region_cache_active && !region_count);
    assert(VirtualFree(base,0,MEM_RELEASE));
    puts("client registry: fresh permission proof, guard/free refusal and retry pass");
}
int main(void) {
    setup(); queries_and_mutations(); permissions_and_retry();
    puts("story_client_registry_test: PASS"); return 0;
}
