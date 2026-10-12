/* Private implementation included by lan_story_spawn_trace.c. Automatic zone
 * gates only: native InternalSpawnGates owns model/shape construction; the host
 * owns their animation and retirement. No client gate heartbeat is scheduled.
 * Intrusive weak observers never move and are unlinked before module release. */
enum { GATE_SPAWN=0x75f90u, GATE_CREATE=0x737e0u, GATE_HEARTBEAT_CALL=0x72e77u,
    GATE_HEARTBEAT=0xfcc40u, GATE_PAUSE=0x1060c0u, GATE_POINT_CLEAR=0x156100u,
    GATE_WEAK_ASSIGN=0x1750u, GATE_SLOTS=SUDEKIMP_STORY_GATE_GROUP_MAX };
typedef struct GateWeak { uint8_t *object; struct GateWeak *previous,*next; } GateWeak;
typedef struct GateLease {
    GateWeak group,actors[SUDEKIMP_STORY_GATE_POINTS];
    SudekiMpStoryGateGroup state;
    uint8_t *points,*zone;
    char area[SUDEKIMP_STORY_AREA_NAME];
    uint32_t group_id;
    BOOL used,owned;
} GateLease;
static GateLease gate_leases[GATE_SLOTS];
static uint8_t *gate_base;
static DWORD gate_thread;
static uint32_t gate_serial;
static BOOL gate_fault;
static unsigned gate_callbacks;
static GateLease *gate_creating;
static SudekiMpRelativeCallHook gate_heartbeat_hook;
typedef void (__attribute__((thiscall)) *GateHeartbeat)(void *);
static GateHeartbeat gate_heartbeat_original;
static BOOL gate_bytes(uint8_t *b,unsigned r,const void *p,unsigned n) {
    return readable(b+r,n) && !memcmp(b+r,p,n);
}
static BOOL gate_call(uint8_t *b,unsigned r,unsigned target) {
    int32_t d;
    if(!readable(b+r,5u) || b[r]!=0xe8u) return FALSE;
    memcpy(&d,b+r+1u,4u); return b+r+5u+d==b+target;
}
/* Named exact-image gate. No unknown image/ABI is accepted by installation. */
static BOOL gate_native_code(uint8_t *b) {
    return gate_bytes(b,GATE_SPAWN,"\x56\x8b\x74\x24\x08\x8b\x16",7u) &&
        gate_call(b,0x76000u,GATE_CREATE) && gate_call(b,0x7600bu,GATE_CREATE) &&
        gate_bytes(b,GATE_CREATE,"\x55\x8b\xec\x83\xe4\xf8",6u) &&
        gate_call(b,0x73907u,0x11730u) && gate_call(b,0x73adeu,0x1560a0u) &&
        gate_bytes(b,0x73a52u,"\xc7\x00",2u) &&
        *(void **)(b+0x73a54u)==b+0x2c9ec8u &&
        *(void **)(b+0x2c9eccu)==b+0x72e30u &&
        gate_bytes(b,0x72e30u,"\x8b\x44\x24\x04\x83\xb8\x40\x01\x00\x00\x04",11u) &&
        gate_bytes(b,0x72e6eu,"\x8b\x40\x68\x85\xc0\x74\x07\x8b\xc8",9u) &&
        gate_bytes(b,0x72e8au,"\x5f\xc2\x04\x00",4u) &&
        gate_call(b,GATE_HEARTBEAT_CALL,GATE_HEARTBEAT) &&
        gate_bytes(b,GATE_WEAK_ASSIGN,"\x8b\x08\x85\xc9\x74\x35\x57",7u) &&
        gate_bytes(b,0x1791u,"\x8b\x4a\x04\x85\xc9",5u) &&
        gate_bytes(b,0x17a4u,"\xc3",1u) &&
        gate_bytes(b,GATE_PAUSE,"\x57\x8b\xf8\x80\x7f\x23\x00\x75\x2b",9u) &&
        gate_bytes(b,0x1060f4u,"\xfe\x47\x23\x5f\xc3",5u) &&
        gate_bytes(b,GATE_POINT_CLEAR,"\x8b\x4a\x6c\x8d\x42\x6c\x85\xc9",8u) &&
        gate_bytes(b,0x156145u,"\x66\x8b\x42\x7a\xb9\xff\xe7\x00\x00",9u) &&
        gate_bytes(b,0x156159u,"\x66\x89\x42\x7a\xc3",5u);
}
static BOOL gate_weak_exact(const GateWeak *w) {
    if(!w->object) return !w->previous && !w->next;
    if(!readable(w->object,8u)) return FALSE;
    if(w->previous) {
        if(!readable(w->previous,sizeof(*w)) || w->previous->object!=w->object || w->previous->next!=w) return FALSE;
    } else if(*(void **)(w->object+4u)!=w) return FALSE;
    return !w->next || (readable(w->next,sizeof(*w)) && w->next->object==w->object && w->next->previous==w);
}
static void gate_bind(GateWeak *w,void *object) {
    uintptr_t a=(uintptr_t)w,d=(uintptr_t)object; void *fn=gate_base+GATE_WEAK_ASSIGN;
    __asm__ volatile("call *%2" : "+a"(a),"+d"(d) : "r"(fn) : "ecx","memory","cc");
}
static BOOL gate_clear_lease(GateLease *g) {
    if(!gate_weak_exact(&g->group)) return FALSE;
    for(unsigned i=0;i<SUDEKIMP_STORY_GATE_POINTS;++i) if(!gate_weak_exact(&g->actors[i])) return FALSE;
    for(unsigned i=0;i<SUDEKIMP_STORY_GATE_POINTS;++i) gate_bind(&g->actors[i],NULL);
    gate_bind(&g->group,NULL); memset(g,0,sizeof(*g)); return TRUE;
}
static BOOL gate_family(const uint8_t *e) {
    static const char *const names[]={"HC_AklorianBarrier","DB_Barrier_FireDoor",
        "NG_CombatBarrier","AO_StairSpikes1","TM_RockBarrier","TRC_GEN_Barrier"};
    char name[64]; uint32_t id;
    if(!readable(e,0x318u) || *(void *const *)e!=gate_base+0x2d5b00u ||
        *(void *const *)(e+8u)!=gate_base+0x2d5b24u ||
        *(void *const *)(e+0x2cu)!=gate_base+0x2d5b44u ||
        (*(uint32_t *)(e+0x30u)&0x1fffu)!=0xf8fu ||
        *(void *const *)(e+0x44u)!=e+0x210u || *(void *const *)(e+0x220u)!=e) return FALSE;
    name_of(e+0x30u,name,&id);
    for(unsigned i=0;i<sizeof(names)/sizeof(names[0]);++i) if(!strcmp(name,names[i])) return TRUE;
    return FALSE;
}
static uint8_t *gate_zone(uint8_t **model) {
    *model=NULL;
    uint8_t *w=readable(gate_base+0x408d10u,4u)?*(uint8_t **)(gate_base+0x408d10u):NULL;
    uint8_t *d=readable(w,0x58u)?*(uint8_t **)(w+0xcu):NULL;
    if(!readable(d,0x38u) || *(void **)d!=gate_base+0x2c82a4u) return NULL;
    uint8_t *z=*(uint8_t **)(d+(*(uint32_t *)(d+0x34u)==1u?0x18u:0x14u));
    if(!readable(z,0x11cu) || *(void **)z!=gate_base+0x2cdcdcu) return NULL;
    uint8_t *r=*(uint8_t **)(z+0xbcu);
    if(!r) {*model=NULL;return z;}
    if(!readable(r,0x14u) || *(void **)r!=gate_base+0x2d5b44u ||
        !gate_family(r-0x2cu) || !in_registry(r-0x2cu)) return z;
    *model=r-0x2cu; return z;
}
static BOOL gate_group(uint8_t *g,unsigned *count,uint8_t **points) {
    if(!g || !in_registry(g) || !group_exact(g)) return FALSE;
    *count=*(unsigned *)(g+0x168u); *points=*(uint8_t **)(g+0x170u);
    unsigned n=*(unsigned *)(g+0x16cu); void *entries=*(void **)(g+0x174u);
    return *count && *count<=SUDEKIMP_STORY_GATE_POINTS && readable(*points,*count*0x80u) &&
        n && n<=4096u && readable(entries,n*0x50u);
}
static BOOL gate_zone_member(uint8_t *zone,uint8_t *group) {
    if(!readable(zone,0x108u) || *(void **)zone!=gate_base+0x2cdcdcu) return FALSE;
    unsigned n=*(unsigned *)(zone+0xfcu);uint8_t *entries=*(uint8_t **)(zone+0x104u);
    if(!n || n>8192u || !readable(entries,n*0x24u)) return FALSE;
    unsigned matches=0;
    for(unsigned i=0;i<n;++i) if(*(void **)(entries+i*0x24u+0x1cu)==group+0x2cu) ++matches;
    return matches==1u;
}
static BOOL gate_lease_group(const GateLease *g) {
    unsigned count; uint8_t *points;
    return gate_weak_exact(&g->group) && gate_group(g->group.object,&count,&points) &&
        count==g->state.point_count && points==g->points && gate_zone_member(g->zone,g->group.object) &&
        *(uint32_t *)(g->group.object+0x34u)==g->group_id;
}
static BOOL gate_actor(const GateLease *g,unsigned i) {
    const GateWeak *a=&g->actors[i];
    return gate_weak_exact(a) && a->object && in_registry(a->object) && gate_family(a->object) &&
        *(uint32_t *)(a->object+0x34u)==g->state.resource;
}
static void __attribute__((thiscall)) gate_heartbeat(void *component) {
    DWORD error=GetLastError(); ++gate_callbacks;
    uint8_t *c=component,*e=readable(c,0x14u)?*(uint8_t **)(c+0x10u):NULL;
    BOOL owned=FALSE;
    if(gate_thread==GetCurrentThreadId() && e && gate_family(e) && *(void **)(e+0x68u)==c) {
        /* Synchronous scene completion happens before the native point TPtr
         * is assigned. The exact creator scope owns only new model instances. */
        uint8_t *model=NULL;
        if(gate_creating && gate_zone(&model)==gate_creating->zone && e!=model &&
            *(uint32_t *)(e+0x34u)==gate_creating->state.resource && in_registry(e)) owned=TRUE;
        for(unsigned g=0;!owned && g<GATE_SLOTS;++g) if(gate_leases[g].used && gate_leases[g].owned)
            for(unsigned i=0;i<gate_leases[g].state.point_count;++i)
                if(gate_leases[g].actors[i].object==e && gate_actor(&gate_leases[g],i)) owned=TRUE;
    }
    SetLastError(error);
    if(!owned) gate_heartbeat_original(component);
    --gate_callbacks;
}
static BOOL gates_install(uint8_t *b) {
    if(gate_heartbeat_hook.installed) return gate_base==b;
    if(!gate_native_code(b)) {SetLastError(ERROR_INVALID_DATA);return FALSE;}
    if(gate_base && gate_base!=b) return FALSE;
    gate_base=b; gate_heartbeat_original=(GateHeartbeat)(b+GATE_HEARTBEAT);
    BOOL ok=SudekiMpInstallRelativeCallHook(&gate_heartbeat_hook,b+GATE_HEARTBEAT_CALL,
        b+GATE_HEARTBEAT,(const void *)gate_heartbeat);
    if(ok) SudekiMpLogWrite("story_gates event=installed families=6 authority=host\r\n");
    return ok;
}
static BOOL gates_uninstall(void) {
    if(gate_callbacks || gate_creating || (gate_thread && gate_thread!=GetCurrentThreadId())) return FALSE;
    if(gate_heartbeat_hook.installed && !SudekiMpRestoreRelativeCallHook(&gate_heartbeat_hook)) return FALSE;
    for(unsigned i=0;i<GATE_SLOTS;++i) if(gate_leases[i].used && !gate_clear_lease(&gate_leases[i])) return FALSE;
    gate_base=NULL;gate_thread=0;gate_serial=0;gate_fault=FALSE;gate_heartbeat_original=NULL;return TRUE;
}
static BOOL gate_begin(void) {
    if(!gate_base || !gate_heartbeat_hook.installed || gate_fault) return FALSE;
    if(!gate_thread) gate_thread=GetCurrentThreadId();
    return gate_thread==GetCurrentThreadId();
}
static BOOL gate_observe_group(uint8_t *entity,uint8_t *zone,uint8_t *model,const char *area) {
    unsigned count; uint8_t *points;
    if(!gate_group(entity,&count,&points)) return TRUE; /* Not a supported gate group. */
    uint32_t mask=0; uint8_t *actors[SUDEKIMP_STORY_GATE_POINTS]={0};
    for(unsigned i=0;i<count;++i) {
        uint8_t *e=*(uint8_t **)(points+i*0x80u+0x6cu);
        if(!e) continue;
        if(!readable(e,0x38u)) return FALSE;
        if(*(void **)e!=gate_base+0x2d5b00u || !gate_family(e) ||
            e==model || *(uint32_t *)(e+0x34u)!=*(uint32_t *)(model+0x34u)) return TRUE;
        if(!in_registry(e) || !(*(uint16_t *)(points+i*0x80u+0x7au)&0x800u)) return FALSE;
        actors[i]=e;mask|=1u<<i;
    }
    GateLease *g=NULL,*free_slot=NULL;
    for(unsigned i=0;i<GATE_SLOTS;++i) {
        GateLease *v=&gate_leases[i];
        if(v->used && v->group.object==entity) g=v;
        if(!v->used && !free_slot) free_slot=v;
    }
    if(!g && !mask) return TRUE;
    BOOL fresh=!g;
    if(!g) {
        if(!free_slot || gate_serial==UINT32_MAX) return FALSE;
        g=free_slot; char name[64];uint32_t id;name_of(entity+0x30u,name,&id);
        if(!name[0] || strlen(name)>=sizeof(g->state.name)) return FALSE;
        g->used=TRUE;g->zone=zone;g->group_id=id;g->points=points;
        strcpy(g->area,area);strcpy(g->state.name,name);
        g->state.resource=*(uint32_t *)(model+0x34u);g->state.point_count=count;
        g->state.serial=++gate_serial;gate_bind(&g->group,entity);
    } else if(!gate_lease_group(g) || g->zone!=zone) return FALSE;
    BOOL replacement=FALSE,old_live=FALSE;
    for(unsigned i=0;i<count;++i) {
        if(!gate_weak_exact(&g->actors[i])) return FALSE;
        if(g->actors[i].object) old_live=TRUE;
        if(actors[i] && g->actors[i].object!=actors[i]) replacement=TRUE;
    }
    if(replacement && !fresh) {
        if(old_live || gate_serial==UINT32_MAX) return FALSE;
        g->state.serial=++gate_serial;
    }
    for(unsigned i=0;i<count;++i) {
        if(g->actors[i].object && actors[i]!=g->actors[i].object) return FALSE;
        if(actors[i] && !g->actors[i].object) gate_bind(&g->actors[i],actors[i]);
    }
    if(fresh || replacement || g->state.live_mask!=mask)
        SudekiMpLogFormat("story_gates event=observed group=%s serial=%lu resource=%08lx mask=%08lx\r\n",
            g->state.name,(unsigned long)g->state.serial,(unsigned long)g->state.resource,(unsigned long)mask);
    g->state.live_mask=mask;return TRUE;
}
BOOL SudekiMpLanStoryGatesCapture(SudekiMpStoryEffectState *out) {
    if(!out || !gate_begin()) return FALSE;
    uint8_t *model=NULL,*zone=gate_zone(&model);
    if(!zone || (!model && *(void **)(zone+0xbcu))) return FALSE;
    for(unsigned i=0;i<GATE_SLOTS;++i) if(gate_leases[i].used &&
        (gate_leases[i].zone!=zone || strcmp(gate_leases[i].area,out->current)))
        if(!gate_clear_lease(&gate_leases[i])) return FALSE;
    uint8_t *reg=*(uint8_t **)(gate_base+RVA_REGISTRY);
    if(!readable(reg,0x40u)) return FALSE;
    unsigned n=*(unsigned *)(reg+0x34u);uint8_t **items=*(uint8_t ***)(reg+0x3cu);
    if(!n || n>8192u || !readable(items,n*4u)) return FALSE;
    if(model) for(unsigned i=0;i<n;++i) {
        if(!readable(items[i],4u)) return FALSE;
        if(*(void **)items[i]==gate_base+0x2c9e20u && gate_zone_member(zone,items[i]) && !gate_observe_group(items[i],zone,model,out->current)) return FALSE;
    }
    SudekiMpStoryGateGroup result[GATE_SLOTS]={0}; unsigned result_count=0;
    for(unsigned i=0;i<GATE_SLOTS;++i) if(gate_leases[i].used) {
        GateLease *g=&gate_leases[i];uint32_t mask=0;
        for(unsigned p=0;p<g->state.point_count;++p) {
            if(!gate_weak_exact(&g->actors[p])) return FALSE;
            if(g->actors[p].object) {if(!gate_actor(g,p))return FALSE;mask|=1u<<p;}
        }
        g->state.live_mask=mask;result[result_count++]=g->state;
    }
    memcpy(out->gates,result,sizeof(result));out->gate_count=(uint8_t)result_count;
    out->gates_valid=1;return TRUE;
}
BOOL SudekiMpLanStoryGateIdentity(const void *entity,uint32_t *id) {
    if(!entity || !id || !gate_base || gate_thread!=GetCurrentThreadId()) return FALSE;
    for(unsigned g=0;g<GATE_SLOTS;++g) if(gate_leases[g].used)
        for(unsigned p=0;p<gate_leases[g].state.point_count;++p)
            if(gate_leases[g].actors[p].object==entity && gate_actor(&gate_leases[g],p)) {
                uint32_t words[]={0x47415445u,gate_leases[g].group_id,p,gate_leases[g].state.resource,gate_leases[g].state.serial},h=2166136261u;
                for(unsigned k=0;k<5u;++k) for(unsigned b=0;b<4u;++b) h=(h^((words[k]>>(b*8u))&255u))*16777619u;
                *id=h?h:1u;return TRUE;
            }
    return FALSE;
}
static BOOL gate_retire(GateLease *g,unsigned i,BOOL *entered) {
    if(!gate_weak_exact(&g->actors[i])) return FALSE;
    if(g->actors[i].object) {
        if(!gate_actor(g,i) || *(uint32_t *)(g->actors[i].object+0x140u)!=3u) return FALSE;
        /* Despawn only after native scene setup completed, never cancel a
         * pending load callback. The linked observer survives until destruction. */
        *entered=TRUE;((Despawn)(image_base+rvas[DESPAWN]))(&g->actors[i]);
        if(g->actors[i].object) return FALSE;
    }
    if(gate_lease_group(g)) {
        uint8_t *p=g->points+i*0x80u;
        if(*(void **)(p+0x6cu)) return FALSE;
        if(*(uint16_t *)(p+0x7au)&0x800u) {
            uintptr_t d=(uintptr_t)p;void *fn=gate_base+GATE_POINT_CLEAR;
            *entered=TRUE;
            __asm__ volatile("call *%1" : "+d"(d) : "r"(fn) : "eax","ecx","memory","cc");
        }
    }
    return TRUE;
}
static void gate_spawn_resource(void *resource,void *context) {
    GateLease *g=context;gate_creating=g;
    ((Create)(gate_base+GATE_SPAWN))(resource);
    gate_creating=NULL;
}
/* Runs only inside the runtime's fresh ClientResidencyCall. TRUE means a
 * native mutation was entered; FALSE does not renew the containment baseline. */
BOOL SudekiMpLanStoryGatesFollow(const SudekiMpStoryEffectState *host) {
    if(!host || !gate_begin()) return FALSE;
    BOOL entered=FALSE;uint8_t *model=NULL,*zone=gate_zone(&model);
    if(!zone) return FALSE;
    for(unsigned i=0;i<GATE_SLOTS;++i) if(gate_leases[i].used) {
        GateLease *g=&gate_leases[i];const SudekiMpStoryGateGroup *h=NULL;
        BOOL same_area=g->zone==zone && !strcmp(g->area,host->current);
        if(same_area && !host->gates_valid) continue; /* Unknown is not retirement. */
        for(unsigned j=0;j<host->gate_count;++j) if(!strcmp(host->gates[j].name,g->state.name)) h=&host->gates[j];
        BOOL same=host->gates_valid && h && same_area &&
            h->serial==g->state.serial && h->resource==g->state.resource && h->point_count==g->state.point_count;
        BOOL retired=TRUE;
        for(unsigned p=0;p<g->state.point_count;++p) if(!same || !(h->live_mask&(1u<<p)))
            if(!gate_retire(g,p,&entered)) retired=FALSE;
        if(!same) {if(retired && !gate_clear_lease(g)) gate_fault=TRUE;return entered;}
        g->state.live_mask=h->live_mask;
    }
    if(!host->gates_valid) return entered;
    for(unsigned k=0;k<host->gate_count;++k) {
        const SudekiMpStoryGateGroup *h=&host->gates[k];GateLease *g=NULL,*free_slot=NULL;
        for(unsigned i=0;i<GATE_SLOTS;++i) {
            if(gate_leases[i].used && !strcmp(gate_leases[i].state.name,h->name))g=&gate_leases[i];
            if(!gate_leases[i].used && !free_slot) free_slot=&gate_leases[i];
        }
        if(g || !h->live_mask) continue;
        if(!free_slot || !model || *(uint32_t *)(model+0x34u)!=h->resource) return entered;
        uint8_t *entity=find_group(h->name),*points;unsigned count;
        if(!gate_group(entity,&count,&points) || !gate_zone_member(zone,entity) || count!=h->point_count) return entered;
        for(unsigned p=0;p<count;++p)
            if(*(void **)(points+p*0x80u+0x6cu) || (*(uint16_t *)(points+p*0x80u+0x7au)&0x800u)) return entered;
        g=free_slot;g->used=g->owned=TRUE;g->state=*h;g->zone=zone;g->points=points;
        g->group_id=*(uint32_t *)(entity+0x34u);strcpy(g->area,host->current);gate_bind(&g->group,entity);
        entered=SudekiMpLanStoryWithResourceName(h->name,0x20u,gate_spawn_resource,g) || entered;
        if(!gate_lease_group(g)) {gate_fault=TRUE;return entered;}
        for(unsigned p=0;p<count;++p) {
            uint8_t *e=*(uint8_t **)(points+p*0x80u+0x6cu);
            if(!e || !gate_family(e) || e==model || *(uint32_t *)(e+0x34u)!=h->resource || !in_registry(e)) {
                gate_fault=TRUE;continue;
            }
            gate_bind(&g->actors[p],e);
            /* InternalSpawnGates intentionally resumes the new gate. Acquire
             * the client's one world-pause reference before scoped adoption. */
            if(!e[0x2bu]) {
                uintptr_t a=(uintptr_t)(e+8u);void *fn=gate_base+GATE_PAUSE;
                __asm__ volatile("call *%1" : "+a"(a) : "r"(fn) : "ecx","edx","memory","cc");
            }
        }
        SudekiMpLogFormat("story_gates event=created group=%s serial=%lu points=%u fault=%u\r\n",
            h->name,(unsigned long)h->serial,count,gate_fault);
        return entered;
    }
    return entered;
}
