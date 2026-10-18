#include "hooks/lan_story_spawn_trace.h"
#include "hooks/call_hook.h"
#include "hooks/story_spawn_sigil.h"
#include "hooks/story_death_visual.h"
#include "hooks/story_spawned_actor.h"
#include "hooks/lan_arena_spirit_visual_host.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include "network/lan_story_area_state.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

enum { CREATE=0, START1, START2, START3, DESPAWN, COUNT };
static const unsigned rvas[COUNT]={0x754a0u,0x75d70u,0x75e10u,0x75eb0u,0xb2300u};
static const uint8_t entries[COUNT][8]={
    {0x53,0x56,0x8b,0x74,0x24,0x0c,0x8b,0x16},  /* PUSH EBX; PUSH ESI; MOV ESI,[ESP+0xc]; MOV EDX,[ESI] */
    {0x56,0x8b,0x74,0x24,0x08,0x8b,0x16},       /* PUSH ESI; MOV ESI,[ESP+8]; MOV EDX,[ESI] */
    {0x56,0x8b,0x74,0x24,0x08,0x8b,0x16},
    {0x55,0x8b,0xec,0x83,0xe4,0xf8},            /* PUSH EBP; MOV EBP,ESP; AND ESP,-8 */
    {0x55,0x8b,0xec,0x83,0xe4,0xf8}};
static const size_t lengths[COUNT]={8,7,7,6,6};
static SudekiMpInlineHook hooks[COUNT];
static BOOL spawn_jobs_install(uint8_t *b),spawn_jobs_uninstall(void),spawn_jobs_can_uninstall(void);
static BOOL effect_retire_install(uint8_t *b),effect_retire_uninstall(void);
static BOOL gates_install(uint8_t *b),gates_uninstall(void);
static unsigned retire_callbacks;
static BOOL spawn_events_install(uint8_t *b),spawn_events_uninstall(void);
static BOOL death_interrupt_install(uint8_t *b),death_interrupt_uninstall(void);
static void spawn_event_constructed(void *job),spawn_event_step(void *job),spawn_event_destroyed(void *job);
static uint32_t spawn_event_job_identity(void *job);
static BOOL spawn_event_identity(const void *actor,uint32_t *id);
static void spawn_events_host_tick(void);
static SudekiMpPointerHook spawn_group_update_hook;
static void spawn_jobs_begin(uint8_t *group),spawn_jobs_end(void);
static unsigned logs;
static uint8_t *image_base;
static SudekiMpStorySpawnGroup started[SUDEKIMP_STORY_SPAWN_GROUP_MAX];
static unsigned started_count;
static uint32_t start_serial;
static volatile LONG replaying;
static BOOL client_role;
typedef struct Ticked Ticked;
static void track_group(const char *name,BOOL watch_only);

static BOOL copy_start_text(char out[SUDEKIMP_STORY_AREA_NAME],const char *text);
static void remember_start(const SudekiMpStorySpawnGroup *command) {
    unsigned i;
    for(i=0;i<started_count;++i) if(!_stricmp(started[i].name,command->name) &&
        !_stricmp(started[i].scope,command->scope)) break;
    if(i==started_count) {
        if(started_count==SUDEKIMP_STORY_SPAWN_GROUP_MAX) {
            memmove(started,started+1,(SUDEKIMP_STORY_SPAWN_GROUP_MAX-1u)*sizeof(*started)); i=--started_count;
        }
        ++started_count;
    }
    started[i]=*command;
}
static void record_start(const void *resource,unsigned method,int selection,int sequence,const char *scope) {
    char name[64]; uint32_t hash;
    extern void spawn_trace_name_of(const void *,char *,uint32_t *);
    spawn_trace_name_of(resource,name,&hash);
    if(!name[0] || name[0]=='?' || strlen(name)>=SUDEKIMP_STORY_AREA_NAME || start_serial==UINT32_MAX) return;
    SudekiMpStorySpawnGroup command={.method=method,.selection=selection,.sequence=sequence,.serial=start_serial+1u};
    strcpy(command.name,name);
    if(scope && !copy_start_text(command.scope,scope)) return;
    if(!SudekiMpStorySpawnGroupValid(&command)) return;
    ++start_serial; remember_start(&command);
}

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && VirtualQuery(p,&m,sizeof(m))==sizeof(m) && m.State==MEM_COMMIT &&
        !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) && a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL copy_start_text(char out[SUDEKIMP_STORY_AREA_NAME],const char *text) {
    for(unsigned i=0;i<SUDEKIMP_STORY_AREA_NAME;++i) {
        if(!text || !readable(text+i,1u)) return FALSE;
        if(!text[i]) return i>0u;
        if(text[i]<0x20 || text[i]>0x7e) return FALSE;
        out[i]=text[i];
    }
    return FALSE; /* Never replay a truncated scope/name. */
}
/* ResourceName: +0 kind bits, +4 hash, +8 shared name record; the record's
 * second word is the C string in the observed layout (entity+0x38 uses it). */
void spawn_trace_name_of(const void *resource,char *out,uint32_t *hash);
static void name_of(const void *resource,char out[64],uint32_t *hash) { spawn_trace_name_of(resource,out,hash); }
void spawn_trace_name_of(const void *resource,char *out,uint32_t *hash) {
    const uint32_t *r=resource; out[0]='?'; out[1]=0; *hash=0;
    if(!readable(r,12u)) return;
    *hash=r[1];
    const uint32_t *rec=(const uint32_t *)(uintptr_t)r[2];
    if(!readable(rec,8u)) return;
    const char *s=(const char *)(uintptr_t)rec[1]; unsigned k=0;
    while(k<63u && readable(s+k,1u) && s[k]>=0x20 && s[k]<0x7f) { out[k]=s[k]; ++k; }
    out[k]=0;
}
static void trace(const char *what,const void *resource,int a,int b,const char *text) {
    char name[64]; uint32_t hash;
    if(logs>=20000u) return;
    ++logs; name_of(resource,name,&hash);
    SudekiMpLogFormat("story_spawn_trace event=%s group=%s hash=%08lx a=%d b=%d text=%s tick=%lu\r\n",
        what,name,(unsigned long)hash,a,b,text?text:"-",(unsigned long)GetTickCount());
}
typedef void (__cdecl *Create)(const void *);
typedef void (__cdecl *Start1)(const void *,int);
typedef void (__cdecl *Start2)(const void *,int,int);
typedef void (__cdecl *Start3)(const char *,const void *,int,int);
typedef void (__cdecl *Despawn)(void *);
static void __cdecl create_hook(const void *n) { trace("create_group",n,0,0,NULL); ((Create)hooks[CREATE].trampoline)(n); }
static void watch_started(const void *resource) {
    char name[64]; uint32_t hash;
    if(replaying) return;
    spawn_trace_name_of(resource,name,&hash);
    if(name[0] && name[0]!='?' && strlen(name)<SUDEKIMP_STORY_AREA_NAME) track_group(name,TRUE);
}
static void __cdecl start1_hook(const void *n,int a) { trace("start_sequence1",n,a,0,NULL); if(!replaying) record_start(n,1u,0,a,NULL); ((Start1)hooks[START1].trampoline)(n,a); watch_started(n); }
static void __cdecl start2_hook(const void *n,int a,int b) { trace("start_sequence2",n,a,b,NULL); if(!replaying) record_start(n,2u,a,b,NULL); ((Start2)hooks[START2].trampoline)(n,a,b); watch_started(n); }
static void __cdecl start3_hook(const char *t,const void *n,int a,int b) {
    char copy[48]={0};
    for(unsigned k=0;k<47u && t && readable(t+k,1u) && t[k]>=0x20 && t[k]<0x7f;++k) copy[k]=t[k];
    trace("start_sequence3",n,a,b,copy); if(!replaying) record_start(n,3u,a,b,t); ((Start3)hooks[START3].trampoline)(t,n,a,b); watch_started(n);
}
static SudekiMpStoryDespawn despawned[SUDEKIMP_STORY_DESPAWN_MAX];
static uint32_t despawn_serial;
static volatile LONG despawning;
BOOL SudekiMpLanStorySpawnIdentity(const void *monster,uint32_t *identity);
static BOOL cached_identity(const void *monster,uint32_t *identity);
static unsigned death_style(const uint8_t *entity);
static void record_despawn(uint32_t identity,unsigned style);
static void __cdecl despawn_hook(void *ptr) {
    void *entity=readable(ptr,4u)?*(void **)ptr:NULL;
    uint32_t identity=0;
    if(entity && !despawning && !client_role && (SudekiMpLanStorySpawnIdentity(entity,&identity) || cached_identity(entity,&identity)))
        record_despawn(identity,death_style(entity));
    if(logs<20000u) { ++logs; SudekiMpLogFormat("story_spawn_trace event=despawn handle=%p entity=%p tick=%lu\r\n",ptr,entity,(unsigned long)GetTickCount()); }
    ((Despawn)hooks[DESPAWN].trampoline)(ptr);
}
static const void *replacements[COUNT]={(const void *)create_hook,(const void *)start1_hook,
    (const void *)start2_hook,(const void *)start3_hook,(const void *)despawn_hook};

BOOL SudekiMpLanStorySpawnTraceInstall(HMODULE image,BOOL client) {
    uint8_t *b=(uint8_t *)image;
    if(!b || !SudekiMpCheckLoadedExecutable(image)) { SetLastError(ERROR_INVALID_DATA); return FALSE; }
    client_role=client;
    if(!effect_retire_install(b)) return FALSE;
    if(!spawn_jobs_install(b)) { (void)effect_retire_uninstall(); return FALSE; }
    for(unsigned i=0;i<COUNT;++i) {
        if(!SudekiMpInstallInlineHook(&hooks[i],b+rvas[i],entries[i],lengths[i],replacements[i])) {
            DWORD error=GetLastError();
            while(i--) (void)SudekiMpRestoreInlineHook(&hooks[i]);
            (void)spawn_jobs_uninstall();
            (void)effect_retire_uninstall();
            SetLastError(error); return FALSE;
        }
    }
    image_base=b;
    if(!gates_install(b)) { (void)SudekiMpLanStorySpawnTraceUninstall(); return FALSE; }
    if(!spawn_events_install(b)) { (void)SudekiMpLanStorySpawnTraceUninstall(); return FALSE; }
    if(!death_interrupt_install(b)) { (void)SudekiMpLanStorySpawnTraceUninstall(); return FALSE; }
    SudekiMpLogWrite("story_spawn_trace event=installed policy=host_events\r\n");
    return TRUE;
}
BOOL SudekiMpLanStorySpawnTraceUninstall(void) {
    if(retire_callbacks || !spawn_jobs_can_uninstall() || !death_interrupt_uninstall() ||
        !spawn_events_uninstall() || !gates_uninstall()) return FALSE;
    BOOL ok=TRUE;
    for(unsigned i=COUNT;i--;) if(hooks[i].installed && !SudekiMpRestoreInlineHook(&hooks[i])) ok=FALSE;
    return ok && spawn_jobs_uninstall() && effect_retire_uninstall();
}

unsigned SudekiMpLanStorySpawnTraceGroups(SudekiMpStorySpawnGroup *out,unsigned max) {
    unsigned n=started_count<max?started_count:max;
    if(out && n) memcpy(out,started,n*sizeof(*out));
    return n;
}

/* Client replay (#54): build a native ResourceName exactly as native code does
 * (FUN_005b9440: ECX=kind, EAX=C string, ESI=destination), start the group
 * through the hooked native export, release the name record. */
enum { RVA_NAME_CONSTRUCT=0x1b9440u, RVA_NAME_RELEASE=0x1b9760u };
static const uint8_t construct_entry[10]={0x57,0x8b,0xf8,0x8b,0x06,0x25,0x80,0xef,0xff,0xff};
static const uint8_t release_entry[8]={0x85,0xc0,0x74,0x2f,0x53,0x8d,0x58,0xfc};
struct Ticked { uint8_t *entity; char name[SUDEKIMP_STORY_AREA_NAME]; uint32_t started,ticks,dumped;
    BOOL watch_only; uint32_t last_state[(0x1b4u-0x140u)/4u]; };
static Ticked ticked[SUDEKIMP_STORY_SPAWN_GROUP_MAX];
static unsigned ticked_count;
static uint8_t *find_group(const char *group);
static BOOL in_registry(const uint8_t *entity);
static BOOL group_exact(const uint8_t *e);
/* Groups started on this machine (host natively, client by replay). Their
 * spawn points name each spawned monster portably: group identifier, point
 * index, monster type. Kept while the group entity stays registered. */
static uint8_t *known_groups[16];
static void know_group(uint8_t *entity) {
    unsigned free_slot=16u;
    for(unsigned i=0;i<16u;++i) {
        if(known_groups[i]==entity) return;
        if(free_slot==16u && (!known_groups[i] || !in_registry(known_groups[i]) || !group_exact(known_groups[i]))) free_slot=i;
    }
    if(free_slot<16u) known_groups[free_slot]=entity;
}
static void track_group(const char *name,BOOL watch_only) {
    uint8_t *entity=find_group(name);
    if(!entity) return;
    know_group(entity);
    unsigned i=0;
    for(;i<ticked_count && ticked[i].entity!=entity;++i) {}
    if(i==ticked_count) {
        if(ticked_count==SUDEKIMP_STORY_SPAWN_GROUP_MAX) {
            memmove(ticked,ticked+1,(SUDEKIMP_STORY_SPAWN_GROUP_MAX-1u)*sizeof(*ticked)); i=--ticked_count;
        }
        ++ticked_count;
    }
    memset(&ticked[i],0,sizeof(ticked[i])); ticked[i].entity=entity; ticked[i].watch_only=watch_only;
    strncpy(ticked[i].name,name,sizeof(ticked[i].name)-1u); ticked[i].started=GetTickCount();
}
void *spawn_trace_construct_fn __attribute__((used));
void *spawn_trace_release_fn __attribute__((used));
/* Unhooked native argument routing, checked by name in the image fixture. */
static const struct { const char *name; uint32_t rva,size,hash; } spawn_start_contracts[]={
    {"SpawnStartDefaultArguments",0x75ddcu,22u,0x0a9e906cu},
    {"SpawnStartSelectedArguments",0x75e7cu,24u,0x937f664du},
    {"SpawnStartScopedArguments",0x75f6fu,24u,0x3e36646fu}
};
static BOOL spawn_start_code(void) {
    for(unsigned i=0;i<3u;++i) {
        const uint8_t *p=image_base+spawn_start_contracts[i].rva;
        uint32_t hash=2166136261u;
        if(!readable(p,spawn_start_contracts[i].size)) return FALSE;
        for(unsigned j=0;j<spawn_start_contracts[i].size;++j) hash=(hash^p[j])*16777619u;
        if(hash!=spawn_start_contracts[i].hash) return FALSE;
    }
    return TRUE;
}
static void replay_start_native(const SudekiMpStorySpawnGroup *command,const void *resource) {
    if(command->method==1u) ((Start1)(image_base+rvas[START1]))(resource,command->sequence);
    else if(command->method==2u) ((Start2)(image_base+rvas[START2]))(resource,command->selection,command->sequence);
    else ((Start3)(image_base+rvas[START3]))(command->scope,resource,command->selection,command->sequence);
}
BOOL SudekiMpLanStorySpawnReplay(const SudekiMpStorySpawnGroup *command) {
    uint32_t resource[3]={0,0,0};
    if(!image_base || !SudekiMpStorySpawnGroupValid(command) ||
        !hooks[command->method].installed || !spawn_start_code() ||
        memcmp(image_base+RVA_NAME_CONSTRUCT,construct_entry,sizeof(construct_entry)) ||
        memcmp(image_base+RVA_NAME_RELEASE,release_entry,sizeof(release_entry))) return FALSE;
    const char *group=command->name;
    /* A missing group is retried after residency; it is not an acknowledged
     * start. Also exclude ambiguous names before calling native lookup. */
    uint8_t *entity=find_group(group);
    if(!entity) return FALSE;
    spawn_trace_construct_fn=image_base+RVA_NAME_CONSTRUCT; spawn_trace_release_fn=image_base+RVA_NAME_RELEASE;
    /* Register inputs only: no stack-relative operands across pushes. */
    {
        uint32_t *dest=resource; const char *text=group; unsigned kind=0x20u;
        __asm__ volatile("call *_spawn_trace_construct_fn"
            : "+S"(dest),"+a"(text),"+c"(kind) : : "edx","ebx","edi","memory","cc");
    }
    client_role=TRUE;
    InterlockedExchange(&replaying,1);
    spawn_jobs_begin(entity);
    replay_start_native(command,resource);
    spawn_jobs_end();
    InterlockedExchange(&replaying,0);
    entity=find_group(group);
    /* The native hook deliberately skips replay calls. Publish the observed
     * local start only after entering native code and finding the exact group,
     * so AreaFollow can acknowledge it instead of restarting it every frame. */
    BOOL observed=entity && *(const int32_t *)(entity+0x178u)==command->sequence &&
        *(const int32_t *)(entity+0x180u)==command->selection;
    if(observed) remember_start(command);
    uint32_t *record=(uint32_t *)(uintptr_t)resource[2];
    if(record && readable(record,4u) && --*record==0u) {
        uint32_t *r=record;
        __asm__ volatile("call *_spawn_trace_release_fn"
            : "+a"(r) : : "ecx","edx","ebx","esi","edi","memory","cc");
    }
    SudekiMpLogFormat("story_spawn_trace event=replayed group=%s sequence=%d selection=%d method=%lu serial=%lu scope=%s entity=%p observed=%d\r\n",
        group,command->sequence,command->selection,(unsigned long)command->method,
        (unsigned long)command->serial,command->scope,(void *)entity,observed);
    if(observed) track_group(group,FALSE);
    return observed;
}

/* Spawn group entity (vtable 2C9E20): the update node is the second base at
 * entity+8 (vtable 2C9E48, pause count node+0x23 = entity+0x2B). Node slot 1
 * (473210, thiscall ECX=node, one stack argument, RET 4) runs the sequence
 * and spawns monsters; it reads only the argument's +0xC float (seconds).
 * The native GameUpdate scheduler never adds a paused node (534280 checks
 * node+0x23), so the contained client's group armed by the replay never
 * ran. Tick only that group, only while it is paused, for a bounded time
 * (its completion field is not identified yet). The host only watches its
 * natively started groups: research dumps of entity+0x140..0x1B4 compare
 * the two sides. */
enum { RVA_GROUP_VTABLE=0x2c9e20u, RVA_GROUP_NODE_VTABLE=0x2c9e48u, RVA_GROUP_UPDATE=0x73210u,
    RVA_REGISTRY=0x409d8cu, TICK_LIMIT_MS=300000u };
static const uint8_t update_entry[12]={0x55,0x8b,0xec,0x83,0xe4,0xf8,0x81,0xec,0xd4,0x00,0x00,0x00};
static void same_name(const uint8_t *e,char out[48]) {
    out[0]=0;
    const uint32_t *r=readable(e,0x3cu)?*(const uint32_t *const *)(e+0x38u):NULL;
    if(!readable(r,8u)) return;
    const char *s=(const char *)(uintptr_t)r[1]; unsigned k=0;
    while(k<47u && readable(s+k,1u) && s[k]>=0x20 && s[k]<0x7f) { out[k]=s[k]; ++k; }
    out[k]=0;
}
static BOOL group_exact(const uint8_t *e) {
    return readable(e,0x1b4u) && *(void *const *)e==image_base+RVA_GROUP_VTABLE &&
        *(void *const *)(e+8u)==image_base+RVA_GROUP_NODE_VTABLE &&
        readable(image_base+RVA_GROUP_NODE_VTABLE,8u) &&
        (((void *const *)(image_base+RVA_GROUP_NODE_VTABLE))[1]==image_base+RVA_GROUP_UPDATE ||
         (spawn_group_update_hook.installed && ((void *const *)(image_base+RVA_GROUP_NODE_VTABLE))[1]==spawn_group_update_hook.replacement_value)) &&
        !memcmp(image_base+RVA_GROUP_UPDATE,update_entry,sizeof(update_entry));
}
static BOOL in_registry(const uint8_t *entity) {
    const uint8_t *owner=readable(image_base+RVA_REGISTRY,4u)?*(const uint8_t *const *)(image_base+RVA_REGISTRY):NULL;
    if(!readable(owner,0x40u)) return FALSE;
    unsigned count=*(const uint32_t *)(owner+0x34u);
    const uint8_t *const *entries=*(const uint8_t *const *const *)(owner+0x3cu);
    if(!count || count>8192u || !readable(entries,count*sizeof(*entries))) return FALSE;
    for(unsigned i=0;i<count;++i) if(entries[i]==entity) return TRUE;
    return FALSE;
}
static uint8_t *find_group(const char *group) {
    const uint8_t *owner=image_base && readable(image_base+RVA_REGISTRY,4u)?*(const uint8_t *const *)(image_base+RVA_REGISTRY):NULL;
    if(!readable(owner,0x40u)) return NULL;
    unsigned count=*(const uint32_t *)(owner+0x34u);
    uint8_t *const *entries=*(uint8_t *const *const *)(owner+0x3cu);
    if(!count || count>8192u || !readable(entries,count*sizeof(*entries))) return NULL;
    uint8_t *found=NULL; char name[48];
    for(unsigned i=0;i<count;++i) {
        if(!group_exact(entries[i])) continue;
        same_name(entries[i],name);
        if(_stricmp(name,group)) continue;
        if(found) return NULL; /* ambiguous */
        found=entries[i];
    }
    return found;
}
void *spawn_trace_update_fn __attribute__((used));
/* CClusterManager singleton (global 809D78, primary vtable 2C7B30): +0x24 is
 * the player's current Cluster, whose ResourceName hash is Cluster+0xC (name
 * record +0x10). A spawn group (473210) only advances its delay timers while
 * that hash equals its own cluster (node+0x158 = entity+0x160) when the spawn
 * description's flag bit 0 is set. The cluster is recomputed from the leader's
 * sector by the manager's update (435120, thiscall ECX=manager, one stack
 * argument with seconds at +0xC, RET 4), which the contained client world
 * does not run. */
enum { RVA_CLUSTER_GLOBAL=0x409d78u, RVA_CLUSTER_VTABLE=0x2c7b30u, RVA_CLUSTER_UPDATE=0x35120u };
static const uint8_t cluster_entry[12]={0x55,0x8b,0xec,0x83,0xe4,0xf8,0x81,0xec,0xa0,0x00,0x00,0x00};
void *spawn_trace_cluster_fn __attribute__((used));
static uint8_t *cluster_manager(void) {
    uint8_t *m=image_base && readable(image_base+RVA_CLUSTER_GLOBAL,4u)?*(uint8_t **)(image_base+RVA_CLUSTER_GLOBAL):NULL;
    return readable(m,0x40u) && *(void **)m==image_base+RVA_CLUSTER_VTABLE?m:NULL;
}
static uint32_t cluster_now(char name[48]) {
    uint8_t *m=cluster_manager(); name[0]='-'; name[1]=0;
    const uint8_t *c=m?*(const uint8_t *const *)(m+0x24u):NULL;
    if(!readable(c,0x14u)) return 0;
    const uint32_t *rec=*(const uint32_t *const *)(c+0x10u);
    if(readable(rec,8u)) {
        const char *t=(const char *)(uintptr_t)rec[1]; unsigned k=0;
        while(k<47u && readable(t+k,1u) && t[k]>=0x20 && t[k]<0x7f) { name[k]=t[k]; ++k; }
        name[k]=0;
    }
    return *(const uint32_t *)(c+0xcu);
}
/* Cluster tables (4352D0/509440): world 808D10 +0x58 region table (index
 * < 0xCE); a region's zone is +0x18 when region+0x34==1, else +0x14; the
 * zone's +0x118 table holds {+4 count, +0xC array of 0x48-byte Cluster}
 * (Cluster vtable 2D4780, second base +4 vtable 2D4788, hash +0xC). */
enum { RVA_WORLD=0x408d10u, RVA_CLUSTER_OBJECT_VTABLE=0x2d4780u, RVA_CLUSTER_NAME_VTABLE=0x2d4788u };
static __attribute__((unused)) uint8_t *find_cluster(uint32_t hash) {
    uint8_t *world=readable(image_base+RVA_WORLD,4u)?*(uint8_t **)(image_base+RVA_WORLD):NULL, *found=NULL;
    if(!hash || !readable(world,0x58u+0xceu*4u)) return NULL;
    for(unsigned r=0;r<0xceu;++r) {
        uint8_t *region=*(uint8_t **)(world+0x58u+r*4u);
        if(!readable(region,0x38u)) continue;
        uint8_t *zone=*(uint8_t **)(region+(*(int32_t *)(region+0x34u)==1?0x18u:0x14u));
        if(!readable(zone,0x11cu)) continue;
        uint8_t *table=*(uint8_t **)(zone+0x118u);
        if(!readable(table,0x10u)) continue;
        unsigned count=*(uint32_t *)(table+4u); uint8_t *array=*(uint8_t **)(table+0xcu);
        if(!count || count>256u || !readable(array,count*0x48u)) continue;
        for(unsigned i=0;i<count;++i) {
            uint8_t *c=array+i*0x48u;
            if(*(void **)c!=image_base+RVA_CLUSTER_OBJECT_VTABLE ||
                *(void **)(c+4u)!=image_base+RVA_CLUSTER_NAME_VTABLE || *(uint32_t *)(c+0xcu)!=hash) continue;
            if(found && found!=c) return NULL; /* ambiguous */
            found=c;
        }
    }
    return found;
}
static void __attribute__((unused)) cluster_update(float dt) {
    uint8_t *m=cluster_manager();
    if(!m || memcmp(image_base+RVA_CLUSTER_UPDATE,cluster_entry,sizeof(cluster_entry))) return;
    spawn_trace_cluster_fn=image_base+RVA_CLUSTER_UPDATE;
    float timing[4]={0.0f,0.0f,0.0f,dt};
    void *ecx=m; const void *arg=timing;
    __asm__ volatile("push %1; call *_spawn_trace_cluster_fn"
        : "+c"(ecx) : "r"(arg) : "eax","edx","memory","cc");
}
static void __attribute__((unused)) group_update(uint8_t *node,const float *timing) {
    void *ecx=node; const void *arg=timing;
    __asm__ volatile("push %1; call *_spawn_trace_update_fn"
        : "+c"(ecx) : "r"(arg) : "eax","edx","memory","cc");
}
/* Spawn point layout (473210/4762B0): entity+0x168 count, entity+0x170
 * array of 0x80-byte points; point+0x6C holds the spawned entity (its +0x90
 * arbiter is what the group tests for death). */
/* Last identities seen per entity: a dead monster's spawn point may already
 * have dropped it (473210 marks the point) by the time it is despawned. */
static struct { const void *entity; uint32_t identity,death_style; } identity_cache[64];
static unsigned identity_next;
static void cache_identity(const void *monster,uint32_t identity) {
    for(unsigned i=0;i<64u;++i) if(identity_cache[i].entity==monster) {
        unsigned style=death_style(monster);
        if(identity_cache[i].identity!=identity || style) identity_cache[i].death_style=style;
        identity_cache[i].identity=identity; return;
    }
    identity_cache[identity_next].entity=monster; identity_cache[identity_next].identity=identity;
    identity_cache[identity_next].death_style=death_style(monster);
    identity_next=(identity_next+1u)%64u;
}
static unsigned death_style(const uint8_t *e) {
    if(!image_base || !readable(e,0xa8u) || !SudekiMpStorySpawnedClass(image_base,*(void *const *)e)) return 0;
    const uint8_t *combat=*(const uint8_t *const *)(e+0xa4u);
    const uint8_t *arb=*(const uint8_t *const *)(e+0x90u);
    if(!readable(combat,0x60u) || *(void *const *)combat!=image_base+0x2c8754u ||
        *(const void *const *)(combat+0x10u)!=e || !readable(arb,0x54u) ||
        *(void *const *)arb!=image_base+0x2cc9acu || *(const void *const *)(arb+0x10u)!=e ||
        !(*(const uint32_t *)(arb+0x50u)&0x4000u)) return 0;
    unsigned action=(*(const uint32_t *)(combat+0x5cu)>>18u)&0x1ffu;
    return action>=0x17u && action<=0x1au ? action-0x16u : 0u;
}
static BOOL cached_identity(const void *monster,uint32_t *identity) {
    return spawn_event_identity(monster,identity);
}
BOOL SudekiMpLanStorySpawnIdentity(const void *monster,uint32_t *identity) {
    if(!image_base || !monster || !identity || !spawn_event_identity(monster,identity)) return FALSE;
    cache_identity(monster,*identity);return TRUE;
}
/* Spawn sigils (#54): a group spawn also creates SpecialEffect entities
 * (vtable 2D3C7C; update node +8, vtable 2D3CA4, slot 1 = 5319B0, thiscall,
 * RET 4) that the paused client world freezes at their first frame. Every
 * exact paused effect receives its native update until native retirement. */
enum { RVA_FX_VTABLE=0x2d3c7cu, RVA_FX_NODE_VTABLE=0x2d3ca4u, RVA_FX_UPDATE=0x1319b0u,
    RVA_FX_RETIRE_TAIL=0x131ad3u };
/* On the host a spawn sigil is stopped about 3-4 s after it appears (live
 * trace) through its owner's SFX handle. Destroying the client's copy
 * directly (531DF0) left the monster holding that handle: its later despawn
 * stopped the freed effect and crashed (live, sudeki+0x131E16). So only the
 * visual first step of 531DF0 is repeated: entity+0x214 holder ->+8 render
 * object, flag 4 = hidden (renderer slot +0x14 notified with visible=0 when
 * flag 0x4000000 is set), then flag 1 cleared. The owner still releases it. */
typedef void (__attribute__((thiscall)) *FxVisible)(void *,int);
static BOOL fx_sigil(const uint8_t *e);
static BOOL fx_hide_visual(uint8_t *e);
static BOOL fx_hide(uint8_t *e) {
    /* Re-prove immediately before the visual write, including when called
     * outside the follow path. New SpecialEffect instances include hits,
     * pickups and level-up effects; their class alone grants no hide lease. */
    if(!fx_sigil(e)) return FALSE;
    return fx_hide_visual(e);
}
/* Caller has just proved registry membership, the exact class and the
 * resource that grants this hide. */
static BOOL fx_hide_visual(uint8_t *e) {
    uint8_t *holder=readable(e,0x218u)?*(uint8_t **)(e+0x214u):NULL;
    uint8_t *object=readable(holder,0xcu)?*(uint8_t **)(holder+8u):NULL;
    if(!readable(object,0x38u)) return FALSE;
    uint32_t flags=*(uint32_t *)(object+0x34u),hidden=flags|4u;
    if((flags&0x4000000u) && !(flags&4u)) {
        uint8_t *renderer=*(uint8_t **)(object+0x14u);
        if(!readable(renderer,4u) || *(void **)renderer!=image_base+0x2df8ecu) return FALSE;
        ((FxVisible)((void *const *)(image_base+0x2df8ecu))[5])(renderer,0);
    }
    *(uint32_t *)(object+0x34u)=hidden&~1u;
    return TRUE;
}
static const uint8_t fx_entry[7]={0x83,0xec,0x10,0x53,0x8b,0xd9,0xf6};
/* Ordinary effects remain entirely native; sigil receipts only govern hiding. */
static void sigil_tick(void);
static void sigil_constructed(void *raw),sigil_job_step(void *raw),sigil_job_destroyed(void *raw);
void *spawn_trace_fx_fn __attribute__((used));
/* A sigil removes itself (5319B0 -> 531DF0) only once its render animation
 * reports done, and that animation advances in the native per-object update
 * (1D64D0, thiscall ECX=render object, one stack dt in seconds, RET 4; the
 * same seam lan_story_split drives), which the paused world skips. */
enum { RVA_OBJECT_UPDATE=0x1d64d0u };
static const uint8_t object_update_head[6]={0x55,0x8b,0xec,0x83,0xe4,0xf8};
void *spawn_trace_object_fn __attribute__((used));
static void fx_animate(uint8_t *entity,float dt) {
    /* 5319B0 reads the animation component's owner at entity+0x280.
     * For a SpecialEffect this points back to the entity itself. */
    uint8_t *model=readable(entity,0x284u)?*(uint8_t **)(entity+0x280u):NULL;
    uint8_t *position=readable(model,0x48u)?*(uint8_t **)(model+0x44u):NULL;
    if(!readable(position,0xb8u)) return;
    uint8_t *wrapper=*(uint8_t **)(position+0xb4u);
    if(!readable(wrapper,0x14u)) return;
    uint8_t *object=*(uint8_t **)(wrapper+8u),*renderer=*(uint8_t **)(wrapper+0x10u);
    if(!readable(object,0xd0u) || !renderer || *(uint8_t **)(object+0x14u)!=renderer ||
        memcmp(image_base+RVA_OBJECT_UPDATE,object_update_head,sizeof(object_update_head))) return;
    /* 5D64D0 itself skips objects already stamped this frame. The ambient
     * adapter must supply dt on the first native renderer visit; this call
     * only helps effects the render walk has not visited. Never clear the
     * native stamp to force a second renderer/particle update. */
    spawn_trace_object_fn=image_base+RVA_OBJECT_UPDATE;
    uint32_t dt_bits; memcpy(&dt_bits,&dt,4);
    void *ecx=object;
    __asm__ volatile("push %1; call *_spawn_trace_object_fn" : "+c"(ecx) : "r"(dt_bits) : "eax","edx","memory","cc");
}
static BOOL fx_exact(const uint8_t *e) {
    return image_base && readable(e,0x3e4u) && *(void *const *)e==image_base+RVA_FX_VTABLE &&
        *(void *const *)(e+0x58u)==e+0x270u &&
        *(void *const *)(e+0x270u)==image_base+0x2c83f4u &&
        *(void *const *)(e+0x280u)==e &&
        *(void *const *)(e+8u)==image_base+RVA_FX_NODE_VTABLE &&
        ((void *const *)(image_base+RVA_FX_NODE_VTABLE))[1]==image_base+RVA_FX_UPDATE &&
        !memcmp(image_base+RVA_FX_UPDATE,fx_entry,sizeof(fx_entry));
}
/* Reveal markers (#79): fireflies, SFXAR006_10_REVEALED (.hom hash), mark
 * an invisible chest or door until Ailish's dispel retires them on the host
 * and fades the object in. The client's own script never runs the dispel;
 * the world relay mirrors the fade (render alpha). A marker standing at an
 * object the host has revealed is hidden like a sigil: visual flags only,
 * its script owner keeps and releases it (hide CONFIRMED_LIVE 2026-10-16). */
enum { RES_REVEAL_MARKER=0x3026f213u };
BOOL SudekiMpLanStoryRevealMarkersHide(const float position[3]) {
    if(!image_base || !position || !client_role) return FALSE;
    const uint8_t *owner=readable(image_base+RVA_REGISTRY,4u)?*(const uint8_t *const *)(image_base+RVA_REGISTRY):NULL;
    if(!readable(owner,0x40u)) return FALSE;
    unsigned count=*(const uint32_t *)(owner+0x34u),hidden=0;
    uint8_t *const *entries=*(uint8_t *const *const *)(owner+0x3cu);
    if(!count || count>8192u || !readable(entries,count*sizeof(*entries))) return FALSE;
    for(unsigned i=0;i<count;++i) {
        uint8_t *e=entries[i];
        if(!fx_exact(e) || (*(const uint32_t *)(e+0x29cu)&0x7fu)!=0x29u ||
            *(const uint32_t *)(e+0x2a0u)!=RES_REVEAL_MARKER ||
            *(uint8_t **)(e+0x44u)!=e+0x160u || *(void **)(e+0x160u)!=image_base+0x2cdefcu ||
            *(uint8_t **)(e+0x170u)!=e) continue;
        const float *at=(const float *)(e+0x160u+0x18u);
        float dx=at[0]-position[0],dy=at[1]-position[1],dz=at[2]-position[2];
        if(!isfinite(dx) || !isfinite(dy) || !isfinite(dz) || dx*dx+dz*dz>1.0f || fabsf(dy)>2.0f) continue;
        if(fx_hide_visual(e)) ++hidden;
    }
    SudekiMpLogFormat("story_spawn_trace event=reveal_marker_hidden count=%u at=%.2f,%.2f,%.2f\r\n",
        hidden,(double)position[0],(double)position[1],(double)position[2]);
    return hidden!=0u;
}
static BOOL fx_sigil(const uint8_t *e) {
    return image_base && in_registry(e) && fx_exact(e) &&
        *(void *const *)(e+0x58u)==e+0x270u &&
        *(void *const *)(e+0x270u)==image_base+0x2c83f4u &&
        *(void *const *)(e+0x280u)==e &&
        SudekiMpStoryIsSpawnSigilResource(*(const uint32_t *)(e+0x29cu),
            *(const uint32_t *)(e+0x2a0u));
}
static BOOL fx_tick(float dt) {
    /* 5319B0 may retire an entity and relocate the registry array. Snapshot
     * only identities, then freshly prove membership/owners before every call. */
    static struct { uint8_t *entity; uint32_t identifier; } pending[8192];
    unsigned pending_count=0; BOOL entered=FALSE;
    if(!image_base || !isfinite(dt) || dt<=0.0f) dt=0.0f;
    else if(dt>0.1f) dt=0.1f;
    const uint8_t *owner=image_base && readable(image_base+RVA_REGISTRY,4u)?
        *(const uint8_t *const *)(image_base+RVA_REGISTRY):NULL;
    if(!readable(owner,0x40u)) return FALSE;
    unsigned count=*(const uint32_t *)(owner+0x34u);
    uint8_t *const *entries=*(uint8_t *const *const *)(owner+0x3cu);
    if(count>8192u || (count && !readable(entries,count*sizeof(*entries)))) return FALSE;
    for(unsigned i=0;i<count;++i) {
        uint8_t *e=entries[i];
        if(!fx_exact(e)) continue;
        pending[pending_count].entity=e;
        pending[pending_count++].identifier=*(uint32_t *)(e+0x34u);
    }
    spawn_trace_fx_fn=image_base+RVA_FX_UPDATE;
    for(unsigned i=0;i<pending_count;++i) {
        uint8_t *e=pending[i].entity;
        if(!in_registry(e) || !fx_exact(e) || *(uint32_t *)(e+0x34u)!=pending[i].identifier) continue;
        if(!e[0x2bu] || dt<=0.0f) continue;
        float timing[4]={0.0f,0.0f,0.0f,dt};
        const void *arg=timing; void *ecx=e+8u;
        entered=TRUE;
        __asm__ volatile("push %1; call *_spawn_trace_fx_fn" : "+c"(ecx) : "r"(arg) : "eax","edx","memory","cc");
        if(in_registry(e) && fx_exact(e) && *(uint32_t *)(e+0x34u)==pending[i].identifier) fx_animate(e,dt);
    }
    return entered;
}
/* Host: native removes dead monsters without the DespawnEntity export (live:
 * zero export calls in a whole ambush). A monster whose identity was seen and
 * that left the registry is recorded as despawned for clients. */
static unsigned death_style(const uint8_t *entity);
static void record_despawn(uint32_t identity,unsigned style);
static void __attribute__((unused)) host_removals(void) {
    for(unsigned i=0;i<64u;++i) {
        if(!identity_cache[i].entity || !identity_cache[i].identity) continue;
        if(in_registry(identity_cache[i].entity)) continue;
        record_despawn(identity_cache[i].identity,identity_cache[i].death_style);
        identity_cache[i].entity=NULL; identity_cache[i].identity=0;
    }
}
BOOL SudekiMpLanStorySpawnTick(uint32_t now,BOOL client) {
    client_role=client;
    static uint32_t last;
    float dt=last && now-last<1000u?(float)(now-last)/1000.0f:0.0f;
    last=now?now:1u;
    if(image_base && !client_role) spawn_events_host_tick();
    if(!image_base) return FALSE;
    if(dt>0.1f) dt=0.1f;
    spawn_trace_update_fn=image_base+RVA_GROUP_UPDATE;
    sigil_tick();
    BOOL entered=client?fx_tick(dt):FALSE,ticking=FALSE;
    for(unsigned i=0;i<ticked_count;++i) if(!ticked[i].watch_only) ticking=TRUE;
    {   static uint32_t reported; char cname[48]; uint32_t h=cluster_now(cname);
        if(h!=reported) { reported=h;
            SudekiMpLogFormat("story_spawn_trace event=cluster side=%s hash=%08lx name=%s ticking=%u\r\n",
                client_role?"client":"host",(unsigned long)h,cname,(unsigned)ticking); } }
    for(unsigned i=0;i<ticked_count;) {
        Ticked *t=&ticked[i]; char name[48]; const char *why=NULL;
        if(!in_registry(t->entity) || !group_exact(t->entity)) why="entity_gone";
        else { same_name(t->entity,name); if(_stricmp(name,t->name)) why="name_changed"; }
        if(why) {}
        else if(now-t->started>TICK_LIMIT_MS) why="time_limit";
        if(why) {
            SudekiMpLogFormat("story_spawn_trace event=tick_stopped group=%s reason=%s ticks=%lu\r\n",t->name,why,(unsigned long)t->ticks);
            memmove(ticked+i,ticked+i+1,(ticked_count-i-1u)*sizeof(*ticked)); --ticked_count; continue;
        }
        if(SudekiMpLogResearchEnabled() && t->dumped<600u && now-t->started>=t->dumped*1000u) {
            const uint32_t *state=(const uint32_t *)(t->entity+0x140u);
            if(memcmp(state,t->last_state,sizeof(t->last_state))) {
                char hex[sizeof(t->last_state)*9u+1u]; unsigned k=0;
                for(unsigned w=0;w<sizeof(t->last_state)/4u;++w) k+=(unsigned)wsprintfA(hex+k,"%08lx ",(unsigned long)state[w]);
                const uint8_t *settings=readable(image_base+0x408d94u,4u)?*(const uint8_t *const *)(image_base+0x408d94u):NULL;
                char cname[48]; uint32_t chash=cluster_now(cname);
                SudekiMpLogFormat("story_spawn_trace event=group_state side=%s group=%s cluster=%08lx/%s group_cluster=%08lx ms=%lu ticks=%lu refs=%u enabled=%u level=%ld clock=%d state140=%s\r\n",
                    t->watch_only?"host":"client",t->name,(unsigned long)chash,cname,(unsigned long)*(const uint32_t *)(t->entity+0x160u),(unsigned long)(now-t->started),(unsigned long)t->ticks,
                    (unsigned)t->entity[0x2bu],(unsigned)image_base[0x3217c7u],
                    readable(settings,0xd0u)?(long)*(const int32_t *)(settings+0xccu):-1L,
                    (int)*(const int16_t *)(image_base+0x3c2fecu),hex);
                memcpy(t->last_state,state,sizeof(t->last_state));
            }
            ++t->dumped;
        }
        /* D2: the client never runs random selection or spawn timers. Native
         * setup jobs and effects continue; authenticated events create actors. */
        ++i;
    }
    return entered;
}

static void record_despawn(uint32_t identity,unsigned style) {
    if(!style) for(unsigned i=0;i<64u;++i)
        if(identity_cache[i].identity==identity && identity_cache[i].death_style) {
            style=identity_cache[i].death_style; break;
        }
    for(unsigned i=0;i<SUDEKIMP_STORY_DESPAWN_MAX;++i)
        if(despawned[i].identity==identity && despawned[i].serial && (int32_t)(despawn_serial-despawned[i].serial)<2) return;
    memmove(despawned,despawned+1,(SUDEKIMP_STORY_DESPAWN_MAX-1u)*sizeof(*despawned));
    if(++despawn_serial==0u) despawn_serial=1u;
    despawned[SUDEKIMP_STORY_DESPAWN_MAX-1u].serial=despawn_serial;
    despawned[SUDEKIMP_STORY_DESPAWN_MAX-1u].identity=identity;
    despawned[SUDEKIMP_STORY_DESPAWN_MAX-1u].death_style=style;
    for(unsigned i=0;i<64u;++i) if(identity_cache[i].identity==identity) { identity_cache[i].entity=NULL; identity_cache[i].identity=0; }
    if(logs<20000u) { ++logs; SudekiMpLogFormat("story_spawn_trace event=despawn_recorded identity=%08lx serial=%lu style=%u\r\n",
        (unsigned long)identity,(unsigned long)despawn_serial,style); }
}
void SudekiMpLanStorySpawnDespawnCapture(SudekiMpStoryDespawn *out) {
    if(out) memcpy(out,despawned,sizeof(despawned));
}
/* Shared native death cosmetics; D2 retires only completed spawn jobs. */
static BOOL visual_memory(const void *p,size_t n,BOOL write) {
    MEMORY_BASIC_INFORMATION m;
    if(!readable(p,n) || !write) return readable(p,n);
    if(VirtualQuery(p,&m,sizeof(m))!=sizeof(m)) return FALSE;
    DWORD access=m.Protect&0xffu;
    return access==PAGE_READWRITE || access==PAGE_WRITECOPY ||
        access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
}
static BOOL death_visual(uint8_t *entity,unsigned style) {
    SudekiMpStoryDeathVisual v;
    if(!in_registry(entity) || !SudekiMpStoryDeathVisualPrepare(image_base,entity,style,&v,visual_memory)) return FALSE;
    uint32_t *record=(uint32_t *)(uintptr_t)v.resource[2];
    if(record) ++*record;
    uint8_t *effect=SudekiMpStoryDeathVisualCall(&v,image_base+0x17da0u);
    SudekiMpLogFormat("story_spawn_trace event=death_visual style=%u asset=%08lx created=%u\r\n",
        style,(unsigned long)v.resource[1],effect && in_registry(effect) && fx_exact(effect));
    return TRUE; /* Native creation entered, including a failed allocation. */
}
/* Build a native ResourceName (FUN_005B9440: ECX=kind, EAX=C string,
 * ESI=destination), hand it to use(), release the name record. Game thread. */
BOOL SudekiMpLanStoryWithResourceName(const char *name,unsigned kind,
    void (*use)(void *resource,void *context),void *context) {
    uint32_t resource[3]={0,0,0};
    if(!image_base || !name || !name[0] || !use ||
        memcmp(image_base+RVA_NAME_CONSTRUCT,construct_entry,sizeof(construct_entry)) ||
        memcmp(image_base+RVA_NAME_RELEASE,release_entry,sizeof(release_entry))) return FALSE;
    spawn_trace_construct_fn=image_base+RVA_NAME_CONSTRUCT; spawn_trace_release_fn=image_base+RVA_NAME_RELEASE;
    {
        uint32_t *dest=resource; const char *text=name; unsigned k=kind;
        __asm__ volatile("call *_spawn_trace_construct_fn"
            : "+S"(dest),"+a"(text),"+c"(k) : : "edx","ebx","edi","memory","cc");
    }
    use(resource,context);
    uint32_t *record=(uint32_t *)(uintptr_t)resource[2];
    if(record && readable(record,4u) && --*record==0u) {
        uint32_t *r=record;
        __asm__ volatile("call *_spawn_trace_release_fn"
            : "+a"(r) : : "ecx","edx","ebx","esi","edi","memory","cc");
    }
    return TRUE;
}

#include "hooks/lan_story_spawn_jobs.h"

#include "hooks/lan_story_sigils.h"

#include "hooks/lan_story_effect_retire.h"

#include "hooks/lan_story_gates.h"

static BOOL visual_memory(const void *,size_t,BOOL);
#include "hooks/lan_story_spawn_events.h"
#include "hooks/lan_story_death_interrupt.h"
