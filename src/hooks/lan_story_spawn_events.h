/* D2: one host occurrence per native SoloSpawnSetup. The existing adapter owns
 * the constructor/step/destructor seams; these are observers, not new detours.
 * Stable intrusive weak nodes distinguish destruction from address reuse. */
#include "hooks/story_spawn_event_guard.h"
enum { SPAWN_EVENT_SLOTS=128u };
typedef struct SpawnEventLease {
    GateWeak group,actor;
    SudekiMpStorySpawnEvent state;
    uint8_t *job;
    uint32_t epoch,death,emitted;
    char area[SUDEKIMP_STORY_AREA_NAME];
    BOOL used,owned,terminal;
} SpawnEventLease;
static SpawnEventLease spawn_events[SPAWN_EVENT_SLOTS],*spawn_creating;
static uint32_t spawn_event_serial;
static DWORD spawn_event_thread;
static BOOL spawn_event_fault;
static unsigned spawn_event_callbacks;
static void spawn_event_death_emitted(void *component,void *effect);
static void spawn_event_refuse(unsigned line) {
    if(!spawn_event_fault) SudekiMpLogFormat("story_spawn_event event=refused line=%u client=%u\r\n",line,client_role);
    spawn_event_fault=TRUE;
}
typedef void (__attribute__((thiscall)) *SpawnGroupUpdate)(void *,const float *);
static SpawnGroupUpdate original_spawn_group_update;
static BOOL spawn_event_begin(void) {
    if(!image_base || !spawn_group_update_hook.installed || spawn_event_fault) return FALSE;
    if(!spawn_event_thread) spawn_event_thread=GetCurrentThreadId();
    return spawn_event_thread==GetCurrentThreadId();
}
static void __attribute__((thiscall)) spawn_group_update(void *node,const float *timing) {
    /* Role is set before hook installation, including the client's own load.
     * Gate setup has a separate task; this node owns monster random selection. */
    ++spawn_event_callbacks;
    if(!client_role) original_spawn_group_update(node,timing);
    --spawn_event_callbacks;
}
static BOOL spawn_events_install(uint8_t *b) {
    if(spawn_group_update_hook.installed) return image_base==b && SudekiMpLanStoryDeathObserverSet(spawn_event_death_emitted);
    if(!spawn_event_code(b)) {SetLastError(ERROR_INVALID_DATA);return FALSE;}
    original_spawn_group_update=(SpawnGroupUpdate)(b+RVA_GROUP_UPDATE);
    if(!SudekiMpInstallPointerHook(&spawn_group_update_hook,
        (void **)(b+RVA_GROUP_NODE_VTABLE+4u),b+RVA_GROUP_UPDATE,(void *)spawn_group_update)) return FALSE;
    if(SudekiMpLanStoryDeathObserverSet(spawn_event_death_emitted)) return TRUE;
    (void)SudekiMpRestorePointerHook(&spawn_group_update_hook);return FALSE;
}
static BOOL spawn_event_clear(SpawnEventLease *e) {
    if(e->job || !gate_weak_exact(&e->actor) || !gate_weak_exact(&e->group)) return FALSE;
    gate_bind(&e->actor,NULL);gate_bind(&e->group,NULL);memset(e,0,sizeof(*e));return TRUE;
}
static BOOL spawn_events_uninstall(void) {
    if(spawn_event_callbacks || spawn_creating ||
        (spawn_event_thread && spawn_event_thread!=GetCurrentThreadId())) return FALSE;
    for(unsigned i=0;i<SPAWN_EVENT_SLOTS;++i) if(spawn_events[i].job) return FALSE;
    if(spawn_group_update_hook.installed && !SudekiMpRestorePointerHook(&spawn_group_update_hook)) return FALSE;
    if(!SudekiMpLanStoryDeathObserverSet(NULL)) return FALSE;
    for(unsigned i=0;i<SPAWN_EVENT_SLOTS;++i) if(spawn_events[i].used && !spawn_event_clear(&spawn_events[i])) return FALSE;
    return TRUE;
}
static SpawnEventLease *spawn_event_for_job(void *raw) {
    if(!raw) return NULL;
    for(unsigned i=0;i<SPAWN_EVENT_SLOTS;++i) if(spawn_events[i].used && spawn_events[i].job==raw) return &spawn_events[i];
    return NULL;
}
static BOOL spawn_event_actor(const SpawnEventLease *e) {
    uint8_t *a=e->actor.object;
    return gate_weak_exact(&e->actor) && a && in_registry(a) && readable(a,0x145u) &&
        *(void **)a==image_base+0x2d5fa0u && *(uint32_t *)(a+0x34u)==e->state.resource;
}
static BOOL spawn_event_identity(const void *actor,uint32_t *id) {
    if(!actor || !id || !spawn_event_begin()) return FALSE;
    SpawnEventLease *found=NULL;
    for(unsigned i=0;i<SPAWN_EVENT_SLOTS;++i) {
        SpawnEventLease *e=&spawn_events[i];
        if(!e->used || e->actor.object!=actor || !spawn_event_actor(e)) continue;
        if(found) {spawn_event_refuse(__LINE__);return FALSE;}
        found=e;
    }
    if(!found) return FALSE;
    *id=found->state.serial;return TRUE;
}
/* A receipt comes from the existing 4E2810 observer after native emission,
 * not from HP=0 or a guessed delay. Resolve only the exact leased monster's
 * independent authored death resource; unrelated effects cannot arm it. */
static void spawn_event_death_emitted(void *component,void *effect) {
    if(!spawn_event_begin() || client_role || !readable(component,0xe0u) ||
        *(void **)component!=image_base+0x2c8504u || !fx_exact(effect) || !in_registry(effect)) return;
    uint8_t *actor=*(void **)((uint8_t *)component+0x10u);uint32_t id;
    if(!spawn_event_identity(actor,&id) || *(void **)(actor+0x58u)!=component) {
        /* Research (#54 D1): a group monster whose receipt cannot be leased.
         * Only registry monsters of the exact class are worth a line. */
        static unsigned unleased_logs;
        if(unleased_logs<32u && readable(actor,0x5cu) && *(void **)actor==image_base+0x2d5fa0u && in_registry(actor)) {
            ++unleased_logs; uint32_t style_now=death_style(actor);
            SudekiMpLogFormat("story_spawn_event event=death_receipt_unleased actor=%p style=%u backlink=%u effect_kind=%02lx effect_resource=%08lx ms=%lu\r\n",
                (void *)actor,style_now,*(void **)(actor+0x58u)==component,
                (unsigned long)(*(uint32_t *)((uint8_t *)effect+0x29cu)&0x7fu),(unsigned long)*(uint32_t *)((uint8_t *)effect+0x2a0u),(unsigned long)GetTickCount());
        }
        return;
    }
    unsigned style=death_style(actor);SudekiMpStoryDeathVisual v;
    BOOL prepared=SudekiMpStoryDeathVisualPrepare(image_base,actor,style,&v,visual_memory);
    if(!prepared || (*(uint32_t *)((uint8_t *)effect+0x29cu)&0x7fu)!=0x29u ||
        *(uint32_t *)((uint8_t *)effect+0x2a0u)!=v.resource[1]) {
        /* Research (#54 D1): a leased monster's receipt that does not match its
         * authored death resource. Observation only; nothing is emitted. */
        static unsigned refused_logs;
        if(refused_logs<64u) { ++refused_logs; SudekiMpLogFormat("story_spawn_event event=death_receipt_refused serial=%lu style=%u prepared=%u effect_kind=%02lx effect_resource=%08lx expected=%08lx ms=%lu\r\n",
            (unsigned long)id,style,prepared,(unsigned long)(*(uint32_t *)((uint8_t *)effect+0x29cu)&0x7fu),
            (unsigned long)*(uint32_t *)((uint8_t *)effect+0x2a0u),(unsigned long)(prepared?v.resource[1]:0u),(unsigned long)GetTickCount()); }
        return;
    }
    for(unsigned i=0;i<SPAWN_EVENT_SLOTS;++i) {
        SpawnEventLease *e=&spawn_events[i];
        if(!e->used || e->state.serial!=id || e->actor.object!=actor) continue;
        if(!e->emitted) {
            e->emitted=style;
            SudekiMpLogFormat("story_spawn_event event=death_emitted serial=%lu style=%u ms=%lu\r\n",
                (unsigned long)id,style,(unsigned long)GetTickCount());
        }
        return;
    }
}
BOOL SudekiMpLanStorySpawnDeathState(const void *actor,BOOL *dead,unsigned *emitted) {
    uint32_t id;
    if(!dead || !emitted || client_role || !spawn_event_identity(actor,&id)) return FALSE;
    const uint8_t *a=*(const uint8_t *const *)((const uint8_t *)actor+0x90u);
    if(!readable(a,0x54u) || *(void *const *)a!=image_base+0x2cc9acu ||
        *(const void *const *)(a+0x10u)!=actor) return FALSE;
    for(unsigned i=0;i<SPAWN_EVENT_SLOTS;++i) if(spawn_events[i].used && spawn_events[i].state.serial==id) {
        *dead=(*(const uint32_t *)(a+0x50u)&0x4000u)!=0;
        *emitted=spawn_events[i].emitted;return TRUE;
    }
    return FALSE;
}
BOOL SudekiMpLanStorySpawnDeathPresent(void *actor,uint32_t id,unsigned style) {
    if(!client_role || !spawn_event_begin() || style<1u || style>4u) return FALSE;
    for(unsigned i=0;i<SPAWN_EVENT_SLOTS;++i) {
        SpawnEventLease *e=&spawn_events[i];
        if(!e->used || !e->owned || e->state.serial!=id || e->actor.object!=actor || e->death ||
            e->job || !e->terminal || !spawn_event_actor(e) || *(uint32_t *)(e->actor.object+0x140u)!=3u) continue;
        if(!death_visual(actor,style)) return FALSE;
        e->death=style;
        SudekiMpLogFormat("story_spawn_event event=death_presented serial=%lu style=%u ms=%lu\r\n",
            (unsigned long)id,style,(unsigned long)GetTickCount());
        return TRUE;
    }
    return FALSE;
}
static uint32_t spawn_event_job_identity(void *job) {
    SpawnEventLease *e=spawn_event_for_job(job);return e?e->state.serial:0;
}
static void spawn_event_constructed(void *raw) {
    if(!spawn_event_begin() || !readable(raw,0x28u) || *(void **)raw!=image_base+SOLO_VT) return;
    uint8_t *j=raw;
    SoloJob s={.job=j,.group=*(void **)(j+0x10u),.point=*(void **)(j+0x1cu),.sequence=*(void **)(j+0x20u)};
    if(!readable(s.group,0x1b4u) || !readable(s.sequence,0x50u)) {spawn_event_refuse(__LINE__);return;}
    /* Gates/other resources use related setup tasks. They keep their own owner. */
    if((*(uint32_t *)(s.sequence+0xcu)&0x7fu)!=0x19u) return;
    s.group_id=*(uint32_t *)(s.group+0x34u);s.resource_id=*(uint32_t *)(s.sequence+0x10u);
    if(!solo_group_fields(&s) || spawn_event_for_job(raw)) {spawn_event_refuse(__LINE__);return;}
    SudekiMpStorySpawnEvent v={0}; char name[64];uint32_t id;
    name_of(s.group+0x30u,name,&id);
    if(!name[0] || name[0]=='?' || strlen(name)>=sizeof(v.group)) {spawn_event_refuse(__LINE__);return;}
    strcpy(v.group,name);v.group_id=id;v.resource=s.resource_id;
    v.point=(unsigned)(s.point-*(uint8_t **)(s.group+0x170u))/0x80u;
    v.entry=(unsigned)(s.sequence-*(uint8_t **)(s.group+0x174u))/0x50u;
    v.passes=*(int32_t *)(s.group+0x17cu);v.flags=*(uint16_t *)(s.point+0x78u);
    memcpy(v.position,s.point+0x40u,12);memcpy(v.forward,s.point+0xcu,12);
    SpawnEventLease *e=NULL;
    if(client_role) {
        e=spawn_creating;
        if(!e || e->job || e->group.object!=s.group) {spawn_event_refuse(__LINE__);return;}
        v.serial=e->state.serial;
        if(memcmp(&v,&e->state,sizeof(v))) {spawn_event_refuse(__LINE__);return;}
    } else {
        if(spawn_event_serial==UINT32_MAX) {spawn_event_refuse(__LINE__);return;}
        v.serial=++spawn_event_serial;
        if(!SudekiMpStorySpawnEventValid(&v)) {spawn_event_refuse(__LINE__);return;}
        for(unsigned i=0;i<SPAWN_EVENT_SLOTS;++i) if(!spawn_events[i].used) {e=&spawn_events[i];break;}
        if(!e) {spawn_event_refuse(__LINE__);return;}
        e->used=TRUE;e->state=v;gate_bind(&e->group,s.group);
    }
    e->job=raw;know_group(s.group);
    SudekiMpLogFormat("story_spawn_event event=constructed side=%s serial=%lu group=%s point=%lu entry=%lu passes=%ld type=%08lx\r\n",
        client_role?"client":"host",(unsigned long)v.serial,v.group,(unsigned long)v.point,
        (unsigned long)v.entry,(long)v.passes,(unsigned long)v.resource);
}
static void spawn_event_step(void *raw) {
    SpawnEventLease *e=spawn_event_for_job(raw);
    if(!e || !spawn_event_begin() || !gate_weak_exact(&e->group) || !gate_weak_exact(&e->actor)) return;
    uint8_t *j=raw;
    if(!readable(j,0x28u) || *(void **)j!=image_base+SOLO_VT ||
        *(void **)(j+0x10u)!=e->group.object) {spawn_event_refuse(__LINE__);return;}
    SoloJob s={.job=j,.resource_id=e->state.resource};uint8_t *a=solo_actor(&s);
    if(!a) return; /* Pending native resource, never absence. */
    if(*(void **)a!=image_base+0x2d5fa0u || !in_registry(a)) return;
    if(e->actor.object && e->actor.object!=a) {spawn_event_refuse(__LINE__);return;}
    if(!e->actor.object) gate_bind(&e->actor,a);
}
static void spawn_event_destroyed(void *raw) {
    SpawnEventLease *e=spawn_event_for_job(raw);
    if(e) {e->job=NULL;e->terminal=TRUE;}
}
static void spawn_events_host_tick(void) {
    if(!spawn_event_begin() || client_role) return;
    for(unsigned i=0;i<SPAWN_EVENT_SLOTS;++i) {
        SpawnEventLease *e=&spawn_events[i];if(!e->used) continue;
        if(!gate_weak_exact(&e->actor) || !gate_weak_exact(&e->group)) {spawn_event_refuse(__LINE__);return;}
        if(e->actor.object) {
            if(!spawn_event_actor(e)) continue; /* Registry change is unknown. */
            unsigned style=death_style(e->actor.object);if(style) e->death=style;
        } else if(!e->job && e->terminal) {
            record_despawn(e->state.serial,e->death);
            if(!spawn_event_clear(e)) {spawn_event_refuse(__LINE__);return;}
        }
    }
}
BOOL SudekiMpLanStorySpawnEventsCapture(SudekiMpStoryEffectState *out) {
    if(!out || !spawn_event_begin() || client_role) return FALSE;
    uint8_t *model=NULL,*zone=gate_zone(&model);if(!zone) return FALSE;
    spawn_events_host_tick();if(spawn_event_fault) return FALSE;
    SudekiMpStorySpawnEvent result[SUDEKIMP_STORY_SPAWN_MAX]={0};unsigned n=0;
    for(unsigned i=0;i<SPAWN_EVENT_SLOTS;++i) {
        SpawnEventLease *e=&spawn_events[i];if(!e->used) continue;
        if(!gate_weak_exact(&e->group) || !gate_weak_exact(&e->actor)) return FALSE;
        if(!e->group.object || !gate_zone_member(zone,e->group.object)) continue;
        if(!group_exact(e->group.object) || !in_registry(e->group.object) ||
            (e->actor.object && !spawn_event_actor(e))) return FALSE;
        if(n==SUDEKIMP_STORY_SPAWN_MAX) return FALSE; /* No truncated complete roster. */
        result[n++]=e->state;
    }
    /* Creation order, independent of recycled local journal slots. */
    for(unsigned i=1;i<n;++i) for(unsigned j=i;j && result[j].serial<result[j-1].serial;--j) {
        SudekiMpStorySpawnEvent swap=result[j];result[j]=result[j-1];result[j-1]=swap;
    }
    memcpy(out->spawns,result,sizeof(result));out->spawn_count=(uint8_t)n;out->spawns_valid=1;return TRUE;
}
static BOOL spawn_event_retire(SpawnEventLease *e,const SudekiMpStoryEffectState *host,BOOL *entered) {
    if(e->job) return FALSE; /* Drain async work; never cancel its callback. */
    if(!gate_weak_exact(&e->actor)) return FALSE;
    if(e->actor.object) {
        if(!spawn_event_actor(e) || *(uint32_t *)(e->actor.object+0x140u)!=3u) return FALSE;
        unsigned style=0;
        for(unsigned i=0;i<SUDEKIMP_STORY_DESPAWN_MAX;++i)
            if(host->despawned[i].identity==e->state.serial) style=host->despawned[i].death_style;
        *entered=SudekiMpLanStorySpawnDeathPresent(e->actor.object,e->state.serial,style) || *entered;
        if(!spawn_event_actor(e)) return FALSE;
        *entered=TRUE;((Despawn)(image_base+rvas[DESPAWN]))(&e->actor);
        if(e->actor.object) return FALSE;
    }
    SudekiMpLogFormat("story_spawn_event event=retired serial=%lu\r\n",(unsigned long)e->state.serial);
    return spawn_event_clear(e);
}
static void spawn_event_create_native(uint8_t *group) {
    /* Native 4762B0: EDI=group, no stack arguments, RET; it owns allocation,
     * resource loading, point weak assignment, entry packing and pass advance. */
    void *fn=image_base+0x762b0u;uintptr_t di=(uintptr_t)group;
    __asm__ volatile("call *%1" : "+D"(di) : "r"(fn) : "eax","ecx","edx","memory","cc");
}
BOOL SudekiMpLanStorySpawnEventsFollow(const SudekiMpStoryEffectState *host) {
    if(!host || !SudekiMpStoryEffectStateValid(host) || !host->spawns_valid || !client_role || !spawn_event_begin()) return FALSE;
    BOOL entered=FALSE;uint8_t *model=NULL,*zone=gate_zone(&model);if(!zone) return FALSE;
    for(unsigned i=0;i<SPAWN_EVENT_SLOTS;++i) {
        SpawnEventLease *e=&spawn_events[i];if(!e->used || !e->owned) continue;
        const SudekiMpStorySpawnEvent *h=NULL;
        if(e->epoch==host->epoch && !_stricmp(e->area,host->current))
            for(unsigned k=0;k<host->spawn_count;++k) if(host->spawns[k].serial==e->state.serial) h=&host->spawns[k];
        if(h) {
            if(memcmp(h,&e->state,sizeof(*h))) {spawn_event_refuse(__LINE__);return entered;}
        } else (void)spawn_event_retire(e,host,&entered);
    }
    for(unsigned k=0;k<host->spawn_count;++k) {
        const SudekiMpStorySpawnEvent *h=&host->spawns[k];SpawnEventLease *e=NULL;BOOL seen=FALSE;
        for(unsigned i=0;i<SPAWN_EVENT_SLOTS;++i) {
            SpawnEventLease *v=&spawn_events[i];
            if(v->used && v->epoch==host->epoch && v->state.serial==h->serial) seen=TRUE;
            if(!v->used && !e) e=v;
        }
        if(seen) continue;
        if(!e) return entered;
        uint8_t *g=find_group(h->group);
        if(!g || !gate_zone_member(zone,g) || *(uint32_t *)(g+0x34u)!=h->group_id) continue;
        unsigned np=*(unsigned *)(g+0x168u),ne=*(unsigned *)(g+0x16cu);
        uint8_t *points=*(uint8_t **)(g+0x170u),*entries=*(uint8_t **)(g+0x174u);
        if(np>256u || ne>2048u || h->point>=np || h->entry>=ne ||
            !readable(points,np*0x80u) || !readable(entries,ne*0x50u)) continue;
        uint8_t *p=points+h->point*0x80u,*entry=entries+h->entry*0x50u;
        if((*(uint32_t *)(entry+0xcu)&0x7fu)!=0x19u || *(uint32_t *)(entry+0x10u)!=h->resource) continue;
        BOOL pending=FALSE;
        for(unsigned i=0;i<SPAWN_EVENT_SLOTS;++i) if(spawn_events[i].used && spawn_events[i].job &&
            spawn_events[i].group.object==g && spawn_events[i].state.point==h->point) pending=TRUE;
        if(pending) continue; /* The job still reads its native point. */
        /* An occupied point is acceptable only with our own positive lease;
         * never overwrite an unrelated locally created actor. */
        void *old=*(void **)(p+0x6cu);uint32_t old_id;
        if(old && !spawn_event_identity(old,&old_id)) continue;
        e->used=e->owned=TRUE;e->epoch=host->epoch;e->state=*h;strcpy(e->area,host->current);gate_bind(&e->group,g);
        *(uint32_t *)(g+0x18cu)=h->point;*(uint32_t *)(g+0x190u)=h->entry;*(int32_t *)(g+0x17cu)=h->passes;
        memcpy(p+0x40u,h->position,12);memcpy(p+0xcu,h->forward,12);*(uint16_t *)(p+0x78u)=(uint16_t)h->flags;
        spawn_creating=e;spawn_jobs_begin(g);entered=TRUE;spawn_event_create_native(g);spawn_jobs_end();spawn_creating=NULL;
        /* A synchronous task may already have completed and destroyed itself.
         * Allocation failure never gets retried as a second occurrence. */
        if(!e->job && !e->terminal) {
            spawn_event_refuse(__LINE__);SudekiMpLogWrite("story_spawn_event event=create_unobserved\r\n");return entered;
        }
        if((*(uint16_t *)(p+0x7au)&0x7ffu)!=h->entry) {spawn_event_refuse(__LINE__);return entered;}
        /* Native creation may synchronously assign the actor after its last
         * task callback. Bind the exact resource witness before returning. */
        if(!e->actor.object && e->job) spawn_event_step(e->job);
    }
    return entered;
}
