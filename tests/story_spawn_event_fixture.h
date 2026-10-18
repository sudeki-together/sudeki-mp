/* Exact-image creation with inert allocation/resource endpoints. The retail
 * creator, constructor and weak ownership execute; no gameplay/IO is run. */
static uint8_t event_group[0x1b4],event_points[2][0x80],event_entries[2][0x50];
static uint8_t event_jobs[8][0x28],event_actors[8][0x200],event_resource[0x14];
static uint8_t event_world[0x60],event_desc[0x38],event_zone[0x11c],event_zone_refs[0x24];
static void *event_registry[10];
static BOOL event_ally; /* E2: the stand-in creates an AllyEntity */
static uint32_t event_name[2];
static unsigned event_allocations,event_resource_calls,event_retire_calls,event_host_ticks;
static void * __cdecl event_allocate(unsigned size) {
    assert(size==0x28 && event_allocations<8);return event_jobs[event_allocations++];
}
static void * __attribute__((noinline,used)) event_resource_request(void *raw,unsigned sync,uint32_t *name) {
    assert(sync==1 && event_resource_calls<8);
    /* 411730 returns ESI's 20-byte resource handle: ResourceName at +4,
     * including the +0xC record released by 476377, actor reference at +0x10.
     * An uninitialized output here makes the native fixture stack-dependent. */
    name[0]=0;name[1]=0xfc0;name[2]=0x7ffff;name[3]=0;
    unsigned i=event_resource_calls++;assert(raw==event_jobs[i]);
    uint8_t *a=event_actors[i];*(void **)a=image_base+(event_ally?0x2d55d4:0x2d5fa0);
    *(void **)(a+0x2c)=image_base+(event_ally?0x2d5618:0x2d5fe4);*(uint32_t *)(a+0x34)=93;
    *(uint32_t *)(a+0x140)=3;*(void **)((uint8_t *)raw+4)=a+0x2c;
    *(void **)(event_resource+0x10)=a+0x2c;
    event_registry[i+1]=a;*(unsigned *)(registry+0x34)=i+2;
    spawn_event_step(raw);
    name[4]=(uint32_t)(uintptr_t)(a+0x2c);return name;
}
static void __attribute__((naked,noinline)) event_resource_endpoint(void) {
    __asm__ volatile("pushl %esi\n\tpushl %edx\n\tpushl %ecx\n\tcall _event_resource_request\n\taddl $12,%esp\n\tret $16\n\t");
}
static void event_destroy_weak(void *actor) {
    typedef void (__attribute__((thiscall)) *WeakDestroy)(void *);
    ((WeakDestroy)(image_base+0x4d30))(actor);
}
static void __cdecl event_remove(void *handle) {
    void *actor=*(void **)handle;assert(actor);++event_retire_calls;event_destroy_weak(actor);
}
static void __attribute__((thiscall)) event_group_tick(void *node,const float *timing) {
    assert(node==event_group+8 && timing[3]>0);++event_host_ticks;
}
static void event_redirect(unsigned rva,void *target,uint8_t saved[5]) {
    memcpy(saved,image_base+rva,5);image_base[rva]=0xe9;
    *(int32_t *)(image_base+rva+1)=(intptr_t)target-(intptr_t)(image_base+rva+5);
}
static void event_finish_job(unsigned i) {
    /* Stand-in resource endpoint leaves the native constructor's group weak
     * handle linked, exactly as a pending job does. Retire that handle through
     * retail TPtr unlink before delivering positive destructor completion. */
    typedef void (__attribute__((thiscall)) *WeakUnlink)(void *);
    ((WeakUnlink)(image_base+0x15e0))(event_jobs[i]+0x10);
    spawn_event_destroyed(event_jobs[i]);sigil_job_destroyed(event_jobs[i]);
    SoloJob *s=solo_find(event_jobs[i]);if(s) memset(s,0,sizeof(*s));
}
static void spawn_event_fixture(void) {
    assert(spawn_event_code(image_base));
    for(unsigned i=0;i<sizeof(spawn_event_contracts)/sizeof(*spawn_event_contracts);++i) {
        const SpawnEventContract *c=&spawn_event_contracts[i];
        for(unsigned j=0;j<c->size;++j) {
            image_base[c->rva+j]^=1;assert(!spawn_event_code(image_base));image_base[c->rva+j]^=1;
        }
        printf("%s every-byte mutation refusals PASS\n",c->name);
    }
    gate_base=image_base;assert(spawn_events_install(image_base));assert(spawn_jobs_install(image_base));
    uint8_t *g=event_group;
    *(void **)g=image_base+RVA_GROUP_VTABLE;*(void **)(g+8)=image_base+RVA_GROUP_NODE_VTABLE;
    *(uint32_t *)(g+0x34)=41;event_name[0]=1;event_name[1]=(uint32_t)(uintptr_t)"ambush";
    *(void **)(g+0x38)=event_name;*(unsigned *)(g+0x168)=2;*(unsigned *)(g+0x16c)=2;
    *(void **)(g+0x170)=event_points;*(void **)(g+0x174)=event_entries;
    for(unsigned i=0;i<2;++i) {*(uint32_t *)(event_entries[i]+0xc)=0x19;*(uint32_t *)(event_entries[i]+0x10)=93;}
    *(void **)(image_base+0x408d10)=event_world;*(void **)(event_world+0xc)=event_desc;
    *(void **)event_desc=image_base+0x2c82a4;*(void **)(event_desc+0x14)=event_zone;
    *(void **)event_zone=image_base+0x2cdcdc;*(unsigned *)(event_zone+0xfc)=1;
    *(void **)(event_zone+0x104)=event_zone_refs;*(void **)(event_zone_refs+0x1c)=g+0x2c;
    event_registry[0]=g;*(void **)(registry+0x3c)=event_registry;*(unsigned *)(registry+0x34)=1;
    *(void **)(image_base+RVA_REGISTRY)=registry;assert(group_exact(g));
    uint8_t save_alloc[5],save_resource[5],save_remove[5];
    event_redirect(0x2484fa,(void *)event_allocate,save_alloc);
    event_redirect(0x11730,(void *)event_resource_endpoint,save_resource);
    event_redirect(rvas[DESPAWN],(void *)event_remove,save_remove);
    uint8_t assign=image_base[0x4bc0];image_base[0x4bc0]=0xc3;
    assert(FlushInstructionCache(GetCurrentProcess(),image_base,0x45f000));
    client_role=TRUE;SudekiMpStoryEffectState h={.epoch=1,.current="fixture",.spawns_valid=1,.spawn_count=1};
    h.spawns[0]=(SudekiMpStorySpawnEvent){.group="ambush",.serial=401,.group_id=41,.resource=93,
        .point=1,.entry=1,.passes=2,.position={0x3f800000,0,0xbf800000},.forward={0,0,0x3f800000}};
    puts("fixture: client create");
    assert(SudekiMpLanStorySpawnEventsFollow(&h));
    assert(event_allocations==1 && event_resource_calls==1 && !spawn_event_fault);
    assert(*(void **)(event_points[1]+0x6c)==event_actors[0]);
    assert((*(uint16_t *)(event_points[1]+0x7a)&0x7ff)==1);
    assert(*(unsigned *)(g+0x17c)==1 && *(unsigned *)(g+0x190)==0); /* Native pass advance. */
    assert(!memcmp(event_points[1]+0x40,h.spawns[0].position,12));
    uint32_t id;assert(SudekiMpLanStorySpawnIdentity(event_actors[0],&id) && id==401);
    assert(!SudekiMpLanStorySpawnEventsFollow(&h) && event_allocations==1); /* Retransmit. */
    SudekiMpStoryEffectState unknown={.epoch=1,.current="fixture"};
    assert(!SudekiMpLanStorySpawnEventsFollow(&unknown) && !event_retire_calls);
    h.spawn_count=0;memset(h.spawns,0,sizeof(h.spawns));
    assert(!SudekiMpLanStorySpawnEventsFollow(&h) && !event_retire_calls); /* Pending load drains. */
    assert(!spawn_events_uninstall());event_finish_job(0);
    assert(SudekiMpLanStorySpawnEventsFollow(&h) && event_retire_calls==1);
    assert(!SudekiMpLanStorySpawnIdentity(event_actors[0],&id));
    /* A later wave reuses the same native point, entry and type. Its host
     * serial owns a distinct identity; stale local pointers cannot revive it. */
    h.spawn_count=1;h.spawns[0]=(SudekiMpStorySpawnEvent){.group="ambush",.serial=402,.group_id=41,
        .resource=93,.point=1,.entry=1,.passes=1};
    assert(SudekiMpLanStorySpawnEventsFollow(&h) && event_allocations==2);
    assert(*(int32_t *)(g+0x190)==-1 && *(int32_t *)(g+0x17c)==0);
    assert(SudekiMpLanStorySpawnIdentity(event_actors[1],&id) && id==402);
    event_finish_job(1);
    h.spawns[0].entry=0;assert(!SudekiMpLanStorySpawnEventsFollow(&h) && spawn_event_fault);
    h.spawns[0].entry=1;spawn_event_fault=FALSE;
    /* Complete host absence retires; unknown never does. */
    h.spawn_count=0;memset(h.spawns,0,sizeof(h.spawns));
    assert(SudekiMpLanStorySpawnEventsFollow(&h) && event_retire_calls==2);
    SpawnGroupUpdate original=original_spawn_group_update;original_spawn_group_update=event_group_tick;
    float dt[4]={0,0,0,0.02f};spawn_group_update(g+8,dt);assert(!event_host_ticks);
    client_role=FALSE;spawn_group_update(g+8,dt);assert(event_host_ticks==1);
    original_spawn_group_update=original;
    /* Host observes the real constructor path and publishes its own serial. */
    *(unsigned *)(g+0x18c)=0;*(unsigned *)(g+0x190)=0;*(unsigned *)(g+0x17c)=1;
    puts("fixture: host create");
    spawn_event_create_native(g);assert(event_allocations==3 && !spawn_event_fault);
    SudekiMpStoryEffectState captured={.current="fixture"};
    assert(SudekiMpLanStorySpawnEventsCapture(&captured) && captured.spawn_count==1);
    assert(captured.spawns[0].serial==1 && captured.spawns[0].entry==0 && captured.spawns[0].point==0);
    event_finish_job(2);
    puts("fixture: death presentation");
    death_visual_fixture();
    uint8_t *a=event_actors[2],arb[0x64]={0},combat[0x60]={0};
    *(void **)(a+0x58)=death_anim;*(void **)(death_anim+0x10)=a;
    *(void **)(a+0x44)=death_position;*(void **)(death_position+0x10)=a;
    *(void **)(a+0x90)=arb;*(void **)arb=image_base+0x2cc9ac;*(void **)(arb+0x10)=a;
    *(void **)(a+0xa4)=combat;*(void **)combat=image_base+0x2c8754;*(void **)(combat+0x10)=a;
    *(uint32_t *)(arb+0x50)=0x4000;*(uint32_t *)(combat+0x5c)=0x18u<<18;
    BOOL dead=FALSE;unsigned emitted=99;
    assert(SudekiMpLanStorySpawnDeathState(a,&dead,&emitted) && dead && !emitted);
    memset(entity,0,0x400);*(void **)entity=image_base+RVA_FX_VTABLE;
    *(void **)(entity+8)=image_base+RVA_FX_NODE_VTABLE;
    *(void **)(entity+0x58)=entity+0x270;*(void **)(entity+0x270)=image_base+0x2c83f4;
    *(void **)(entity+0x280)=entity;
    *(uint32_t *)(entity+0x29c)=0xfa9;*(uint32_t *)(entity+0x2a0)=0x87654321;
    event_registry[4]=entity;*(unsigned *)(registry+0x34)=5;
    assert(test_death_observer);test_death_observer(death_anim,entity);
    assert(SudekiMpLanStorySpawnDeathState(a,&dead,&emitted) && !emitted); /* Unrelated FX. */
    *(uint32_t *)(entity+0x2a0)=0x12345678;test_death_observer(death_anim,entity);
    assert(SudekiMpLanStorySpawnDeathState(a,&dead,&emitted) && emitted==2);
    test_death_observer(death_anim,entity);
    assert(SudekiMpLanStorySpawnEventsCapture(&captured) && captured.spawn_count==1); /* Still alive in roster. */
    SpawnEventLease *lease=NULL;
    for(unsigned i=0;i<SPAWN_EVENT_SLOTS;++i) if(spawn_events[i].actor.object==a) lease=&spawn_events[i];
    assert(lease);lease->owned=TRUE;lease->death=0;client_role=TRUE;
    assert(!SudekiMpLanStorySpawnDeathPresent(a,999,2));
    assert(SudekiMpStoryDeathVisualPrepare(image_base,a,2,&death_request,visual_memory));
    image_base[0x4bc0]=assign; /* Death factory needs native ResourceName copying. */
    uint8_t effect_save[5];event_redirect(0x18a80,effect_create_endpoint,effect_save);
    assert(FlushInstructionCache(GetCurrentProcess(),image_base,0x45f000));
    unsigned prior=death_calls;
    puts("fixture: native visual create");
    assert(SudekiMpLanStorySpawnDeathPresent(a,lease->state.serial,2));
    assert(!SudekiMpLanStorySpawnDeathPresent(a,lease->state.serial,2) && death_calls==prior+1);
    assert(event_retire_calls==2 && lease->actor.object==a); /* No early gameplay removal. */
    BOOL entered=FALSE;
    h.despawned[0]=(SudekiMpStoryDespawn){.identity=lease->state.serial,.serial=1,.death_style=2};
    assert(spawn_event_retire(lease,&h,&entered) && entered && death_calls==prior+1);
    memcpy(image_base+0x18a80,effect_save,5);client_role=FALSE;
    puts("Host-emitted death receipt, wrong-asset refusal, retransmission and despawn deduplication PASS");
    assert(SudekiMpLanStorySpawnEventsCapture(&captured) && !captured.spawn_count && captured.spawns_valid);
    /* E2: allies (AllyEntity, entry type 0x02: the castle-road gate guards)
     * take the same path; a lease binds only the class its entry creates. */
    puts("fixture: ally create");
    image_base[0x4bc0]=0xc3;assert(FlushInstructionCache(GetCurrentProcess(),image_base,0x45f000));
    event_ally=TRUE;*(uint32_t *)(event_entries[0]+0xc)=0x02;
    client_role=TRUE;
    SudekiMpStoryEffectState ha={.epoch=1,.current="fixture",.spawns_valid=1,.spawn_count=1};
    ha.spawns[0]=(SudekiMpStorySpawnEvent){.group="ambush",.serial=501,.group_id=41,.resource=93,
        .point=0,.entry=0,.passes=1};
    unsigned base_alloc=event_allocations;
    assert(SudekiMpLanStorySpawnEventsFollow(&ha) && event_allocations==base_alloc+1 && !spawn_event_fault);
    uint8_t *ally=event_actors[base_alloc];
    assert(*(void **)ally==image_base+0x2d55d4 && SudekiMpLanStorySpawnIdentity(ally,&id) && id==501);
    *(void **)ally=image_base+0x2d5fa0;assert(!SudekiMpLanStorySpawnIdentity(ally,&id)); /* class must match */
    *(void **)ally=image_base+0x2d55d4;assert(SudekiMpLanStorySpawnIdentity(ally,&id) && id==501);
    event_finish_job(base_alloc);
    unsigned retired=event_retire_calls;
    ha.spawn_count=0;memset(ha.spawns,0,sizeof(ha.spawns));
    assert(SudekiMpLanStorySpawnEventsFollow(&ha) && event_retire_calls==retired+1);
    assert(!SudekiMpLanStorySpawnIdentity(ally,&id));
    /* Host: the real constructor path publishes an ally like a monster. */
    client_role=FALSE;
    *(unsigned *)(g+0x18c)=0;*(unsigned *)(g+0x190)=0;*(unsigned *)(g+0x17c)=1;
    spawn_event_create_native(g);assert(event_allocations==base_alloc+2 && !spawn_event_fault);
    assert(SudekiMpLanStorySpawnEventsCapture(&captured) && captured.spawn_count==1);
    assert(SudekiMpLanStorySpawnIdentity(event_actors[base_alloc+1],&id) && id==captured.spawns[0].serial);
    uint8_t *host_ally=event_actors[base_alloc+1];uint32_t host_ally_serial=id;
    event_finish_job(base_alloc+1);
    /* An ally entry whose job yields a monster binds nothing and faults
     * nothing; the lease clears once the job is over. */
    event_ally=FALSE;
    *(unsigned *)(g+0x18c)=1;*(unsigned *)(g+0x190)=0;*(unsigned *)(g+0x17c)=1;
    spawn_event_create_native(g);assert(event_allocations==base_alloc+3 && !spawn_event_fault);
    assert(!SudekiMpLanStorySpawnIdentity(event_actors[base_alloc+2],&id));
    event_finish_job(base_alloc+2);
    assert(SudekiMpLanStorySpawnEventsCapture(&captured) && captured.spawn_count==1 && !spawn_event_fault);
    event_ally=TRUE;base_alloc+=1;
    /* Any other entry type (a gate, 0x0F) keeps its own owner: no lease. */
    *(uint32_t *)(event_entries[1]+0xc)=0x0f;
    *(unsigned *)(g+0x18c)=1;*(unsigned *)(g+0x190)=1;*(unsigned *)(g+0x17c)=1;
    spawn_event_create_native(g);assert(event_allocations==base_alloc+3 && !spawn_event_fault);
    assert(!SudekiMpLanStorySpawnIdentity(event_actors[base_alloc+2],&id));
    event_finish_job(base_alloc+2);
    assert(SudekiMpLanStorySpawnEventsCapture(&captured) && captured.spawn_count==1);
    /* Departure: a dead actor leaves the native registry while its native
     * delete is still pending (the weak stays bound). That is a despawn now:
     * recorded once, out of the roster, and the capture keeps working. A
     * live actor that goes missing (a registry rebuild) is unknown. */
    unsigned slot=0;for(;slot<10 && event_registry[slot]!=host_ally;++slot) {}
    assert(slot<10);
    uint32_t before=despawn_serial;
    event_registry[slot]=NULL;
    for(unsigned k=0;k<3;++k) assert(!SudekiMpLanStorySpawnEventsCapture(&captured)); /* alive: unknown, as before */
    assert(despawn_serial==before && !spawn_event_fault);
    event_registry[slot]=host_ally;
    static uint8_t ally_arb[0x64];*(void **)ally_arb=image_base+0x2cc9ac;*(void **)(ally_arb+0x10)=host_ally;
    *(uint32_t *)(ally_arb+0x50)=0x4000;*(void **)(host_ally+0x90)=ally_arb;
    assert(SudekiMpLanStorySpawnEventsCapture(&captured) && captured.spawn_count==1); /* proven dead, still present */
    event_registry[slot]=NULL;
    assert(!SudekiMpLanStorySpawnEventsCapture(&captured) && despawn_serial==before); /* one tick is not enough */
    assert(SudekiMpLanStorySpawnEventsCapture(&captured) && captured.spawn_count==0 && !spawn_event_fault);
    assert(despawn_serial==before+1 && despawned[SUDEKIMP_STORY_DESPAWN_MAX-1].identity==host_ally_serial);
    assert(SudekiMpLanStorySpawnEventsCapture(&captured) && despawn_serial==before+1); /* once */
    { unsigned saved_count=*(unsigned *)(registry+0x34);*(unsigned *)(registry+0x34)=0;
      assert(!spawn_event_registry_lacks(host_ally)); /* an empty list proves nothing */
      *(unsigned *)(registry+0x34)=saved_count; }
    event_registry[slot]=host_ally; /* a published despawn returns: fail closed */
    assert(!SudekiMpLanStorySpawnEventsCapture(&captured) && spawn_event_fault);
    spawn_event_fault=FALSE;event_registry[slot]=NULL;
    void *saved_registry=*(void **)(image_base+RVA_REGISTRY);
    *(void **)(image_base+RVA_REGISTRY)=NULL;assert(!spawn_event_registry_lacks(host_ally)); /* unknown */
    *(void **)(image_base+RVA_REGISTRY)=saved_registry;assert(spawn_event_registry_lacks(host_ally));
    event_destroy_weak(host_ally); /* the deferred native delete */
    assert(SudekiMpLanStorySpawnEventsCapture(&captured) && despawn_serial==before+1 && !spawn_event_fault);
    for(unsigned i=0;i<SPAWN_EVENT_SLOTS;++i) assert(!spawn_events[i].used || spawn_events[i].state.serial!=host_ally_serial);
    puts("Departure: out of the registry is a despawn once, the roster keeps publishing; an unreadable registry is unknown PASS");
    event_ally=FALSE;for(unsigned i=0;i<2;++i) *(uint32_t *)(event_entries[i]+0xc)=0x19;
    image_base[0x4bc0]=assign;
    puts("Ally spawn events: client create, exact-class binding, retire, host publish; other types ignored PASS");
    assert(spawn_events_uninstall() && spawn_jobs_uninstall());
    memcpy(image_base+0x2484fa,save_alloc,5);memcpy(image_base+0x11730,save_resource,5);
    memcpy(image_base+rvas[DESPAWN],save_remove,5);image_base[0x4bc0]=assign;
    assert(FlushInstructionCache(GetCurrentProcess(),image_base,0x45f000));
    memset(spawn_events,0,sizeof(spawn_events));spawn_event_serial=0;spawn_event_thread=0;spawn_event_fault=FALSE;
    memset(sigils,0,sizeof(sigils));memset(sigil_occurrences,0,sizeof(sigil_occurrences));sigil_serial=0;sigil_thread=0;sigil_fault=FALSE;
    memset(known_groups,0,sizeof(known_groups));memset(identity_cache,0,sizeof(identity_cache));
    memset(despawned,0,sizeof(despawned));despawn_serial=0;gate_base=NULL;
    fixture();registry_entries[0]=entity;
    puts("SpawnEvent native creator ABI, entry/pass packing, weak lifetime, retransmit, pending drain, collision and wave reuse PASS");
}
