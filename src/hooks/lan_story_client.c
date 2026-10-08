#include "hooks/lan_story_client.h"
#include "hooks/lan_party_menu_native.h"
#include "hooks/lan_story_load.h"
#include "hooks/lobby_gameplay.h"
#include "hooks/lan_story_world.h"
#include "hooks/lan_story_input.h"
#include "hooks/lan_story_replica.h"
#include "hooks/lan_story_avatar_party_roster.h"
#include "hooks/call_hook.h"
#include "cleanroom/engine.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <stdint.h>
#include <string.h>

/* Native Pause(fd610) iterates registry+34/+3c on BOTH acquire and release.
 * Keep the complete observed membership, not just the player roster. A world
 * replacement or an entity born while held is not permission to decrement its
 * unowned pause reference. Recruitment changes are admitted only through the
 * native creation/retirement journal below; unrelated membership changes and
 * world replacements retain the pause and close playback. */
enum { MAX_ENTITIES=8192, WORLD_GLOBAL=0x408d10,
    GROUP_GLOBAL=0x408d94, CONTROLLER_GLOBAL=0x408da4,
    SPEED_GLOBAL=0x408da0, REGISTRY_GLOBAL=0x409d8c,
    SCHEDULER_GLOBAL=0x409e14, GEL_GLOBAL=0x408d9c,
    TASK_CREATED=0x409e4c, TRIGGER_GLOBAL=0x408d24,
    TRIGGER_SWEEP_CALL=0x3373f, TRIGGER_SWEEP=0xcfd0,
    TRIGGER_DISPATCH_CALL=0x77e24, TRIGGER_DISPATCH=0xc6e0,
    CLUSTER_GLOBAL=0x409d78, CLUSTER_EXIT_QUERY=0x3543f,
    CLUSTER_ENTRY_QUERY=0x35527, SCRIPT_QUERY=0x1c3a60,
    ACTIVITY_PAUSE_CALL=0x13d584, ACTIVITY_RESUME_CALL=0x13d5f2,
    UPDATE_PAUSE=0x1060c0, UPDATE_RESUME=0x106100 };
typedef struct EntityPauseObservation {
    uint8_t *entity;
    void *vtable, *update_vtable;
    uint8_t references, scheduled;
    int16_t activity_delta;
} EntityPauseObservation;
typedef struct WorldPauseObservation {
    uint8_t *registry, *scheduler, *speed, *gel, *manager, *trigger;
    uint8_t **entities;
    unsigned count;
    uint16_t speed_references;
    uint8_t gel_references;
    uint32_t created_tasks, gel_time[4];
    uint8_t *cluster;
} WorldPauseObservation;
static uint8_t *base;
static DWORD native_thread;
static SudekiMpLanStoryClientInputClosed input_closed;
static SudekiMpLanStoryNativeRoster retained_roster;
static SudekiMpLanStoryScene retained_scene;
static SudekiMpLanStoryTaskTraceStatus retained_tasks;
static SudekiMpStoryLoadResult retained_load;
static WorldPauseObservation world_pause;
static EntityPauseObservation entity_pause[MAX_ENTITIES];
static unsigned phase;
static BOOL attempted, installed, logged_unknown, presentation_active,exit_prepared,exit_released;
static SudekiMpLanStoryClientEffectsWitness effects_witness;
static void *effects_witness_context;
static uint64_t observation;
static volatile LONG busy, retained_reference;
static BOOL recruiting;
static SudekiMpLanStoryRecruitReport recruit_observation;
static uint32_t recruit_remote_before_epoch;
static const SudekiMpControlUpdateDispatchWitness *recruit_commit_witness;
static SudekiMpRelativeCallHook trigger_hooks[2];
static SudekiMpRelativeCallHook cluster_hooks[2];
static SudekiMpRelativeCallHook activity_hooks[2];
/* Immutable dependencies survive hook restoration: an already fetched native
 * call may still enter a wrapper after the active client lease has retired. */
static uint8_t *trigger_image;
static void *original_trigger_sweep __attribute__((used));
static void *original_trigger_dispatch __attribute__((used));
static void *original_cluster_query __attribute__((used));
static void *original_activity_pause __attribute__((used));
static void *original_activity_resume __attribute__((used));
static volatile LONG trigger_callbacks,trigger_fault;
static volatile LONG trigger_suppressed[2];
static volatile LONG cluster_suppressed[2];
static unsigned activity_observed[2],activity_faults;
static struct {
    void *manager,*world,*group,*controller,*cluster,*script_manager;
    BOOL pause_released;
} trigger_owner;
static volatile LONG trigger_held;

/* Region answers are reused only inside one exact presentation bracket
 * (SudekiMpLanStoryClientPresent) on the native thread: the native setters
 * admitted there are closed memory operations that never release or reprotect
 * these regions. Every range is still checked against a committed, accessible
 * region; the cache only removes repeated VirtualQuery calls into one region. */
enum { REGION_CACHE=32 };
typedef struct Region { uintptr_t lower,upper; } Region;
static Region regions[REGION_CACHE]; static unsigned region_count,region_next;
static BOOL region_cache_active;
static BOOL readable(const void *p,size_t n) {
    uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n) return FALSE;
    if(region_cache_active) for(unsigned i=0;i<region_count;++i)
        if(a>=regions[i].lower && a+n<=regions[i].upper) return TRUE;
    MEMORY_BASIC_INFORMATION m;
    if(VirtualQuery(p,&m,sizeof(m))!=sizeof(m) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
    DWORD access=m.Protect&0xffu;
    if(access!=PAGE_READONLY && access!=PAGE_READWRITE && access!=PAGE_WRITECOPY &&
        access!=PAGE_EXECUTE_READ && access!=PAGE_EXECUTE_READWRITE &&
        access!=PAGE_EXECUTE_WRITECOPY) return FALSE;
    if(a+n>(uintptr_t)m.BaseAddress+m.RegionSize) return FALSE;
    if(region_cache_active) {
        unsigned slot=region_count<REGION_CACHE?region_count++:region_next;
        region_next=(region_next+1u)%REGION_CACHE;
        regions[slot].lower=(uintptr_t)m.BaseAddress; regions[slot].upper=(uintptr_t)m.BaseAddress+m.RegionSize;
    }
    return TRUE;
}
static BOOL native_thread_exact(void) {
    return installed && native_thread && native_thread==GetCurrentThreadId();
}
static BOOL presentation_pause_exact(BOOL *paused) {
    if(recruit_commit_witness)
        return recruit_commit_witness->service_post_original_exact &&
            SudekiMpControlSeparationUpdateDispatchWitnessStillExact(recruit_commit_witness) &&
            SudekiMpLanPartyMenuNativeObserveOwnedPause(paused);
    if(!effects_witness) return SudekiMpLanPartyMenuNativePauseExact(paused);
    return effects_witness(effects_witness_context) &&
        SudekiMpLanPartyMenuNativeObserveOwnedPause(paused);
}
static BOOL call_target(const uint8_t *site,const void *target) {
    int32_t relative;
    if(!readable(site,5) || *site!=0xe8) return FALSE;
    memcpy(&relative,site+1,4); return site+5+relative==target;
}
static BOOL trigger_identity(void *manager) {
    uint8_t *p=manager;
    return base && p && manager==*(void **)(base+TRIGGER_GLOBAL) &&
        readable(p,0x1dau) && *(void **)p==base+0x2c5458u &&
        *(void **)(p+8)==base+0x2c5460u && *(void **)(p+0x10)==base+0x2c5470u;
}
static BOOL cluster_identity(void *manager) {
    uint8_t *p=manager;
    /* SystemSingletons constructs this exact root and both subobjects before
     * publishing the singleton. The cluster contents may change normally. */
    return base && p && manager==*(void **)(base+CLUSTER_GLOBAL) &&
        readable(p,0x40u) && *(void **)p==base+0x2c7b30u &&
        *(void **)(p+8u)==base+0x2c7b44u && *(void **)(p+0x1cu)==base+0x2c7b4cu;
}
static BOOL cluster_script_contract(uint8_t *image) {
    static const struct { unsigned rva,target; } relocations[]={
        {0x352d7u,0x408d94u},{0x35345u,0x408d10u},{0x353ceu,0x408d10u},
        {0x35402u,0x2bfff2u},{0x35434u,0x408d9cu},{0x3544eu,0x408da0u},
        {0x35454u,0x345f70u},{0x35466u,0x2c4018u},{0x35491u,0x2e3800u},
        {0x354b5u,0x408d9cu},{0x3551cu,0x408d9cu},{0x35536u,0x408da0u},
        {0x3553cu,0x345f70u},{0x3554eu,0x2c4018u},{0x35587u,0x2e3800u},
        {0x3559du,0x408d9cu},{0x355e3u,0x3c2f68u},{0x356a0u,0x408d40u},
        {0x356acu,0x2c621cu},{0x356c1u,0x2c621cu},
        {0x1060ccu,SCHEDULER_GLOBAL},{0x106107u,SCHEDULER_GLOBAL}
    };
    static const struct { unsigned rva,size; uint32_t hash; } bodies[]={
        {0x352d0u,0x410u,0x142e87c8u},{SCRIPT_QUERY,0x5du,0x58e7b053u},
        {0x13d4d0u,0x6bu,0x3ede24d1u},
        {0x13d540u,0x62u,0x1e23f927u},{0x13d5b0u,0x61u,0x50fe3b77u},
        {UPDATE_PAUSE,0x39u,0xe1cf64b1u},{UPDATE_RESUME,0x1cu,0x1f9c06e6u}
    };
    /* Prove the complete caller, not merely a matching CALL: EBX is its
     * retained CClusterManager, EAX the GEL manager and ECX the descriptor.
     * Both AL==0 branches bypass only script submission and leave descriptor
     * destruction, cluster visibility and listener/resource work intact. */
    for(unsigned b=0;b<sizeof(bodies)/sizeof(bodies[0]);++b) {
        if(!readable(image+bodies[b].rva,bodies[b].size)) return FALSE;
        uint32_t hash=2166136261u;
        for(unsigned i=0;i<bodies[b].size;++i) {
            uint8_t byte=image[bodies[b].rva+i];
            for(unsigned r=0;r<sizeof(relocations)/sizeof(relocations[0]);++r) {
                unsigned offset=bodies[b].rva+i-relocations[r].rva;
                if(offset>=4u) continue;
                uint32_t value; memcpy(&value,image+relocations[r].rva,4u);
                if(value!=(uintptr_t)image+relocations[r].target) return FALSE;
                byte=(uint8_t)((0x400000u+relocations[r].target)>>(8u*offset)); break;
            }
            hash=(hash^byte)*16777619u;
        }
        if(hash!=bodies[b].hash) return FALSE;
    }
    return memcmp(image+0x76beau,"\x89\x15",2u)==0 &&
        *(void **)(image+0x76becu)==image+CLUSTER_GLOBAL &&
        memcmp(image+0x76bf0u,"\xc7\x85\x24\x1b\x00\x00",6u)==0 &&
        *(void **)(image+0x76bf6u)==image+0x2c7b30u &&
        memcmp(image+0x76bfau,"\xc7\x85\x2c\x1b\x00\x00",6u)==0 &&
        *(void **)(image+0x76c00u)==image+0x2c7b44u &&
        memcmp(image+0x76c04u,"\xc7\x85\x40\x1b\x00\x00",6u)==0 &&
        *(void **)(image+0x76c0au)==image+0x2c7b4cu;
}
static BOOL trigger_queue_empty(void) {
    uint8_t *p=world_pause.trigger;
    return trigger_identity(p) && *(uint32_t *)(p+0x14)==0u;
}
/* Both native methods are void(manager), ret4. The first ages stale contacts
 * even when each actor is paused; the second immediately executes their GEL
 * callbacks outside the suspended GEL update node. Retain both native contact
 * and empty-queue state while this exact client owns its world pause. */
static BOOL __attribute__((noinline,used,force_align_arg_pointer))
hold_trigger_call(void *manager,unsigned which) {
    DWORD error=GetLastError();
    BOOL held=FALSE;
    InterlockedIncrement(&trigger_callbacks);
    if(InterlockedCompareExchange(&trigger_held,0,0) &&
        trigger_owner.manager && manager==trigger_owner.manager) {
        /* A failed observation must not reopen this retained manager's script
         * dispatch. No substitute result/task is manufactured: these exact
         * void phase calls remain suspended until our pause is balanced. */
        held=TRUE;
        if(!native_thread_exact() || !trigger_identity(manager) ||
            *(void **)(base+WORLD_GLOBAL)!=trigger_owner.world ||
            *(void **)(base+GROUP_GLOBAL)!=trigger_owner.group ||
            *(void **)(base+CONTROLLER_GLOBAL)!=trigger_owner.controller ||
            (!trigger_owner.pause_released && !SudekiMpLanPartyMenuNativeOwnsPause()))
            InterlockedExchange(&trigger_fault,1);
        if(which<2u) InterlockedIncrement(&trigger_suppressed[which]);
    }
    InterlockedDecrement(&trigger_callbacks);
    SetLastError(error); return held;
}
#define TRIGGER_BRIDGE(name,which,original) \
__attribute__((naked,noinline,used)) static void name(void) { \
    __asm__ volatile("pushfl\n\tpushal\n\tpushl $" #which "\n\tpushl 44(%esp)\n\t" \
        "call _hold_trigger_call\n\taddl $8,%esp\n\ttestl %eax,%eax\n\tjz 1f\n\t" \
        "popal\n\tpopfl\n\tret $4\n\t1: popal\n\tpopfl\n\tjmp *_" #original "\n\t"); }
TRIGGER_BRIDGE(trigger_sweep,0,original_trigger_sweep)
TRIGGER_BRIDGE(trigger_dispatch,1,original_trigger_dispatch)
static BOOL __attribute__((noinline,used,force_align_arg_pointer))
hold_cluster_script(void *cluster,void *manager,void *descriptor,unsigned which) {
    DWORD error=GetLastError(); BOOL held=FALSE;
    InterlockedIncrement(&trigger_callbacks);
    if(InterlockedCompareExchange(&trigger_held,0,0)) {
        /* Native cluster updates still run under Pause and immediately submit
         * OnClusterEntry/Exit outside the paused GEL scheduler. A client's
         * local selection/view must not execute host-owned story callbacks.
         * Unknown ownership retains the gate and faults playback; it cannot
         * reopen script submission on a replacement object. */
        held=TRUE;
        BOOL exact=native_thread_exact() && cluster==trigger_owner.cluster &&
            cluster_identity(cluster) && manager==trigger_owner.script_manager &&
            readable(descriptor,0x94u) &&
            *(void **)(base+WORLD_GLOBAL)==trigger_owner.world &&
            *(void **)(base+GROUP_GLOBAL)==trigger_owner.group &&
            *(void **)(base+CONTROLLER_GLOBAL)==trigger_owner.controller &&
            *(void **)(base+GEL_GLOBAL)==world_pause.gel &&
            readable(world_pause.gel,0x34u) && *(void **)(world_pause.gel+0x30u)==manager &&
            (trigger_owner.pause_released || SudekiMpLanPartyMenuNativeOwnsPause());
        if(!exact) InterlockedExchange(&trigger_fault,1);
        if(which<2u) {
            LONG count=InterlockedIncrement(&cluster_suppressed[which]);
            if(count>0 && count<=8 && native_thread_exact())
                SudekiMpLogFormat("lan_story_client event=cluster_script_held kind=%s exact=%u count=%ld authority=host\r\n",
                    which?"entry":"exit",(unsigned)exact,count);
        }
    }
    InterlockedDecrement(&trigger_callbacks);
    SetLastError(error); return held;
}
/* Native query: EAX=manager, ECX=descriptor, no stack arguments, AL result,
 * ordinary RET. Preserve the complete native input for the pass-through;
 * the held branch selects the caller's no-script path without a fake task,
 * handle or task-counter adjustment. */
#define CLUSTER_BRIDGE(name,which) \
__attribute__((naked,noinline,used)) static void name(void) { \
    __asm__ volatile("pushfl\n\tpushal\n\tpushl $" #which "\n\tpushl %ecx\n\t" \
        "pushl %eax\n\tpushl %ebx\n\tcall _hold_cluster_script\n\taddl $16,%esp\n\t" \
        "testl %eax,%eax\n\tjz 1f\n\tpopal\n\tpopfl\n\tmovb $0,%al\n\tret\n\t" \
        "1: popal\n\tpopfl\n\tjmp *_original_cluster_query\n\t"); }
CLUSTER_BRIDGE(cluster_exit_query,0)
CLUSTER_BRIDGE(cluster_entry_query,1)
/* Spawn-group cluster messages suspend through EAX=entity+8 and resume
 * through ESI=entity+8. Neither native helper has stack arguments. The held
 * path below calls the original once and journals its single owned delta;
 * the pass-through restores the original register/flags ABI exactly. */
#define ACTIVITY_BRIDGE(name,reg,which,original) \
__attribute__((naked,noinline,used)) static void name(void) { \
    __asm__ volatile("pushfl\n\tpushal\n\tpushl $" #which "\n\tpushl %" #reg "\n\t" \
        "call _account_activity_reference\n\taddl $8,%esp\n\t" \
        "testl %eax,%eax\n\tjz 1f\n\tpopal\n\tpopfl\n\tret\n\t" \
        "1: popal\n\tpopfl\n\tjmp *_" #original "\n\t"); }
ACTIVITY_BRIDGE(activity_pause,eax,0,original_activity_pause)
ACTIVITY_BRIDGE(activity_resume,esi,1,original_activity_resume)
__attribute__((naked,noinline,used)) static void call_activity_reference(
    void *update __attribute__((unused)),unsigned resume __attribute__((unused))) {
    __asm__ volatile("pushl %esi\n\tmovl 8(%esp),%eax\n\tcmpl $0,12(%esp)\n\t"
        "jne 1f\n\tcall *_original_activity_pause\n\tjmp 2f\n\t"
        "1: movl %eax,%esi\n\tcall *_original_activity_resume\n\t"
        "2: popl %esi\n\tret\n\t");
}
static BOOL trigger_hooks_exact(void) {
    return trigger_hooks[0].installed && trigger_hooks[1].installed &&
        call_target(base+TRIGGER_SWEEP_CALL,trigger_sweep) &&
        call_target(base+TRIGGER_DISPATCH_CALL,trigger_dispatch) &&
        cluster_hooks[0].installed && cluster_hooks[1].installed &&
        call_target(base+CLUSTER_EXIT_QUERY,cluster_exit_query) &&
        call_target(base+CLUSTER_ENTRY_QUERY,cluster_entry_query) &&
        activity_hooks[0].installed && activity_hooks[1].installed &&
        call_target(base+ACTIVITY_PAUSE_CALL,activity_pause) &&
        call_target(base+ACTIVITY_RESUME_CALL,activity_resume);
}
static BOOL pin(DWORD error) {
    HMODULE self;
    (void)GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        (LPCWSTR)(uintptr_t)&SudekiMpLanStoryClientUninstall,&self);
    SetLastError(error); return FALSE;
}
static BOOL same_roster(const SudekiMpLanStoryNativeRoster *a,
    const SudekiMpLanStoryNativeRoster *b) {
    return a && b && a->dispatch_serial==b->dispatch_serial && a->epoch==b->epoch &&
        a->revision==b->revision && a->available_mask==b->available_mask &&
        a->leader_character==b->leader_character && a->world==b->world &&
        a->descriptor==b->descriptor && a->group==b->group && a->controller==b->controller &&
        a->native_leader==b->native_leader &&
        a->native_avatar_generation==b->native_avatar_generation &&
        a->native_avatar_player==b->native_avatar_player &&
        !memcmp(a->actors,b->actors,sizeof(a->actors)) && !memcmp(a->ai,b->ai,sizeof(a->ai));
}
/* A fresh render-thread identity check, rooted in the controller observation
 * that enrolled these exact objects. No stale dispatch witness is passed to
 * ControlSeparation or treated as permission to submit a native action. */
static BOOL roster_matches_scope(const SudekiMpLanStoryNativeRoster *r,
    const SudekiMpLanStoryScene *scene,BOOL terminal) {
    static const SudekiMpCleanroomActor types[4]={SUDEKIMP_CLEANROOM_BUKI,
        SUDEKIMP_CLEANROOM_ELCO,SUDEKIMP_CLEANROOM_TAL,SUDEKIMP_CLEANROOM_AILISH};
    if(!native_thread_exact() || !r || !r->dispatch_serial ||
        (terminal && (!exit_prepared || presentation_active ||
            r!=&retained_roster || scene!=&retained_scene))) return FALSE;
    BOOL avatar=r->native_avatar_generation!=0;
    if((avatar?!SudekiMpLanStoryAvatarPartyRosterExact(r):r->leader_character>=4u) ||
        !SudekiMpLanStorySceneValidForPolicy(scene,avatar?
            SUDEKIMP_LAN_STORY_POLICY_DEV_AVATARS:SUDEKIMP_LAN_STORY_POLICY_REGULAR) ||
        scene->phase!=SUDEKIMP_LAN_STORY_READY ||
        *(void **)(base+WORLD_GLOBAL)!=r->world ||
        *(void **)(base+GROUP_GLOBAL)!=r->group ||
        *(void **)(base+CONTROLLER_GLOBAL)!=r->controller ||
        !readable(r->world,0x39bu) || !readable(r->descriptor,0x38u) ||
        !readable(r->group,0xd0u) || !readable(r->controller,0x24cu)) return FALSE;
    uint8_t *world=r->world,*group=r->group,*controller=r->controller;
    if(*(void **)(world+0x0c)!=r->descriptor || *(void **)(world+0x14) ||
        !world[0x399] || !world[0x39a] ||
        *(uint32_t *)((uint8_t *)r->descriptor+0x34)!=(scene->temporary[0]?4u:3u) ||
        !SudekiMpCleanroomEngineWorldReady() ||
        *(void **)(controller+0x248)!=(avatar?r->native_leader:r->actors[r->leader_character])) return FALSE;
    unsigned count=*(unsigned *)(group+0xcc),mask=0;
    if(avatar) {
        return !scene->available_mask && scene->leader_seat==SUDEKIMP_LAN_STORY_NO_SEAT &&
            r->epoch==scene->epoch && r->revision==scene->revision &&
            count==1u && *(void **)(group+0x90)==r->native_leader &&
            (terminal || SudekiMpLanStoryObserverNativeRosterExact(r)) &&
            SudekiMpLanStoryAvatarPartyRosterExact(r) &&
            *(unsigned *)(group+0xcc)==1u && *(void **)(group+0x90)==r->native_leader &&
            *(void **)(controller+0x248)==r->native_leader &&
            *(void **)(base+WORLD_GLOBAL)==world && *(void **)(base+GROUP_GLOBAL)==group &&
            *(void **)(base+CONTROLLER_GLOBAL)==controller;
    }
    if(!count || count>4u) return FALSE;
    for(unsigned i=0;i<count;++i) {
        void *actor=*(void **)(group+0x90+i*12u);
        unsigned c=0;
        for(;c<4u && (!actor || actor!=r->actors[c]);++c) {}
        if(c>=4u || (mask&(1u<<c)) || (!i && c!=r->leader_character) ||
            !readable(actor,0x98u) || !readable(r->ai[c],0x16cu) ||
            *(void **)((uint8_t *)actor+0x94)!=r->ai[c] ||
            *(void **)((uint8_t *)r->ai[c]+0x10)!=actor ||
            SudekiMpCleanroomEngineActorEntity(types[c])!=actor) return FALSE;
        mask|=1u<<c;
    }
    return mask==r->available_mask && mask==scene->available_mask &&
        count==*(unsigned *)(group+0xcc) &&
        *(void **)(base+WORLD_GLOBAL)==world && *(void **)(base+GROUP_GLOBAL)==group &&
        *(void **)(base+CONTROLLER_GLOBAL)==controller;
}
static BOOL roster_matches(const SudekiMpLanStoryNativeRoster *r,
    const SudekiMpLanStoryScene *scene) {
    return roster_matches_scope(r,scene,FALSE);
}
static BOOL roster_exact(void) {
    return roster_matches(&retained_roster,&retained_scene);
}
static BOOL exit_roster_exact(void) {
    /* PrepareExit enrolled this retained tuple while the Observer, complete
     * registry and owned pause were exact. The runtime retires the Observer
     * before balancing that pause and immediately entering native Quit.
     * Only terminal Drain/Reacquire may replace the retired Observer proof
     * with the still-live Party owner's generation/component checks, both
     * before and after these direct native identity reads. Their callers
     * still require the input fence, registry and prepared pause lease.
     * This grants no presentation or gameplay admission. */
    return roster_matches_scope(&retained_roster,&retained_scene,TRUE);
}
static BOOL load_ready(SudekiMpLanStoryTaskTraceStatus *tasks,SudekiMpStoryLoadResult *load) {
    return SudekiMpLanStoryTaskTraceGetStatus(tasks) && !tasks->unknown &&
        tasks->load_generation && tasks->start_task && tasks->on_load_task &&
        tasks->start_terminal && tasks->start_retired &&
        tasks->on_load_terminal && tasks->on_load_retired &&
        SudekiMpLanStoryLoadGetResult(load) && load->attempt &&
        load->state==SUDEKIMP_STORY_LOAD_RETURNED && !load->result &&
        load->native_called && load->files_retired &&
        (load->route==SUDEKIMP_STORY_LOAD_ROUTE_TITLE_INDEX ||
         load->route==SUDEKIMP_STORY_LOAD_ROUTE_PAGE_RECORD);
}
static BOOL same_load(void) {
    SudekiMpLanStoryTaskTraceStatus t;
    SudekiMpStoryLoadResult l;
    return load_ready(&t,&l) && t.load_generation==retained_tasks.load_generation &&
        t.start_task==retained_tasks.start_task && t.on_load_task==retained_tasks.on_load_task &&
        l.attempt==retained_load.attempt && l.route==retained_load.route &&
        l.folder_slot==retained_load.folder_slot && l.native_index==retained_load.native_index;
}
static BOOL world_identity(void) {
    WorldPauseObservation *p=&world_pause;
    return base && p->registry && *(void **)(base+REGISTRY_GLOBAL)==p->registry &&
        *(void **)(base+SCHEDULER_GLOBAL)==p->scheduler &&
        *(void **)(base+SPEED_GLOBAL)==p->speed && *(void **)(base+GEL_GLOBAL)==p->gel &&
        trigger_identity(p->trigger) && cluster_identity(p->cluster) &&
        readable(p->registry,0x40) && readable(p->scheduler,0x20) &&
        readable(p->speed,0x30) && readable(p->gel,0x4c) &&
        *(void **)(p->gel+0x30)==p->manager && readable(p->manager,0x1008c) &&
        *(unsigned *)(p->registry+0x34)==p->count &&
        *(void **)(p->registry+0x3c)==p->entities &&
        readable(p->entities,(size_t)p->count*sizeof(*p->entities));
}
static int entity_references(const EntityPauseObservation *e,unsigned extra) {
    return (int)e->references+e->activity_delta+(int)extra;
}
static BOOL entity_pause_exact(const EntityPauseObservation *e,unsigned extra) {
    int expected=entity_references(e,extra);
    return expected>=0 && expected<=UINT8_MAX && readable(e->entity,0x145u) &&
        *(void **)e->entity==e->vtable && *(void **)(e->entity+8u)==e->update_vtable &&
        (e->entity[0x144u]&4u)==e->scheduled && e->entity[0x2bu]==expected;
}
static BOOL registry_exact(unsigned extra) {
    WorldPauseObservation *p=&world_pause;
    if(extra>1u || !world_identity() ||
        *(uint16_t *)(p->speed+0x2a)!=(unsigned)p->speed_references+extra ||
        p->speed[0x28]!=((unsigned)p->speed_references+extra!=0u) ||
        p->gel[0x23]!=(unsigned)p->gel_references+extra) return FALSE;
    for(unsigned i=0;i<p->count;++i) {
        EntityPauseObservation *e=&entity_pause[i];
        if(p->entities[i]!=e->entity || !entity_pause_exact(e,extra)) return FALSE;
    }
    return world_identity();
}
static BOOL pause_counts_exact(void) {
    WorldPauseObservation *p=&world_pause;
    return world_identity() &&
        *(uint16_t *)(p->speed+0x2a)==(unsigned)p->speed_references+1u &&
        p->speed[0x28]==1u && p->gel[0x23]==(unsigned)p->gel_references+1u;
}
static BOOL scheduler_exact(void) {
    WorldPauseObservation *p=&world_pause;
    return world_identity() && trigger_hooks_exact() && trigger_queue_empty() &&
        !InterlockedCompareExchange(&trigger_fault,0,0) && !*(void **)(p->manager+0x10080) &&
        !memcmp(p->gel+0x38,p->gel_time,sizeof(p->gel_time)) &&
        *(uint32_t *)(base+TASK_CREATED)==p->created_tasks;
}
static BOOL __attribute__((noinline,used,force_align_arg_pointer))
account_activity_reference(void *update,unsigned resume) {
    DWORD error=GetLastError(); BOOL handled=FALSE;
    InterlockedIncrement(&trigger_callbacks);
    /* Initial load and the separate native recruitment transaction retain
     * their existing postconditions. Terminal native quit owns destruction
     * after our positively balanced pause release; never inspect its objects. */
    if(InterlockedCompareExchange(&trigger_held,0,0) && !recruiting &&
        !trigger_owner.pause_released && attempted) {
        handled=TRUE;
        EntityPauseObservation *e=NULL; unsigned index=0;
        const char *reason="owner";
        if(!native_thread_exact() || phase!=SUDEKIMP_STORY_CLIENT_PAUSED ||
            resume>1u || !SudekiMpLanPartyMenuNativeOwnsPause() ||
            !pause_counts_exact() || !scheduler_exact() ||
            *(void **)(base+WORLD_GLOBAL)!=trigger_owner.world ||
            *(void **)(base+GROUP_GLOBAL)!=trigger_owner.group ||
            *(void **)(base+CONTROLLER_GLOBAL)!=trigger_owner.controller) goto fault;
        for(;index<world_pause.count;++index)
            if(entity_pause[index].entity+8u==(uint8_t *)update) { e=&entity_pause[index]; break; }
        reason="entity";
        if(!e || world_pause.entities[index]!=e->entity || !entity_pause_exact(e,1u)) goto fault;
        int before=entity_references(e,1u),delta=resume?-1:1;
        reason="owned_reference";
        /* The native helpers are pure one-byte changes on these branches:
         * Pause cannot overflow; Resume cannot consume our final reference
         * and enter a scheduler/component callback. Foreign references are
         * accounted only at these two validated cluster-message callsites. */
        if((resume && before<=1) || (!resume && before>=UINT8_MAX)) goto fault;
        call_activity_reference(update,resume);
        reason="native_return";
        if(!world_identity() || world_pause.entities[index]!=e->entity ||
            !readable(e->entity,0x145u) || *(void **)e->entity!=e->vtable ||
            *(void **)(e->entity+8u)!=e->update_vtable ||
            (e->entity[0x144u]&4u)!=e->scheduled || e->entity[0x2bu]!=before+delta ||
            !pause_counts_exact() || !scheduler_exact()) goto fault;
        e->activity_delta=(int16_t)(e->activity_delta+delta);
        if(++activity_observed[resume]<=8u)
            SudekiMpLogFormat("lan_story_client event=activity_reference kind=%s entity=%p index=%u before=%d after=%d baseline=%u delta=%d owned_pause=1\r\n",
                resume?"resume":"pause",e->entity,index,before,before+delta,
                (unsigned)e->references,(int)e->activity_delta);
        goto done;
fault:
        InterlockedExchange(&trigger_fault,1);
        if(native_thread_exact() && ++activity_faults<=8u)
            SudekiMpLogFormat("lan_story_client event=activity_reference_refused kind=%u reason=%s update=%p phase=%u\r\n",
                resume,reason,update,phase);
    }
done:
    InterlockedDecrement(&trigger_callbacks);
    SetLastError(error); return handled;
}
static BOOL capture_registry(void) {
    WorldPauseObservation p={0};
    p.registry=*(uint8_t **)(base+REGISTRY_GLOBAL);
    p.scheduler=*(uint8_t **)(base+SCHEDULER_GLOBAL);
    p.speed=*(uint8_t **)(base+SPEED_GLOBAL);
    p.gel=*(uint8_t **)(base+GEL_GLOBAL);
    p.trigger=*(uint8_t **)(base+TRIGGER_GLOBAL);
    p.cluster=*(uint8_t **)(base+CLUSTER_GLOBAL);
    if(!readable(p.registry,0x40) || !readable(p.scheduler,0x20) ||
        !readable(p.speed,0x30) || !readable(p.gel,0x4c) || !trigger_hooks_exact() ||
        !trigger_identity(p.trigger) || !cluster_identity(p.cluster) ||
        *(uint32_t *)(p.trigger+0x14)!=0u) return FALSE;
    p.manager=*(uint8_t **)(p.gel+0x30);
    p.count=*(unsigned *)(p.registry+0x34);
    p.entities=*(uint8_t ***)(p.registry+0x3c);
    if(!p.count || p.count>MAX_ENTITIES || !readable(p.manager,0x1008c) ||
        *(void **)(p.manager+0x10080) ||
        !readable(p.entities,(size_t)p.count*sizeof(*p.entities))) return FALSE;
    p.speed_references=*(uint16_t *)(p.speed+0x2a);
    p.gel_references=p.gel[0x23];
    /* No foreign full-world pause is borrowed. Individual entity pause counts
     * are preserved; a script-specific paused NPC is not silently resumed. */
    if(p.speed_references || p.speed[0x28] || p.gel_references) return FALSE;
    p.created_tasks=*(uint32_t *)(base+TASK_CREATED);
    memcpy(p.gel_time,p.gel+0x38,sizeof(p.gel_time));
    for(unsigned i=0;i<p.count;++i) {
        uint8_t *e=p.entities[i];
        if(!readable(e,0x145u) || e[0x2b]==UINT8_MAX) return FALSE;
        /* A duplicate owner would receive more than one increment in native
         * Pause; do not misdescribe that as one retained reference. */
        for(unsigned j=0;j<i;++j) if(entity_pause[j].entity==e) return FALSE;
        entity_pause[i]=(EntityPauseObservation){e,*(void **)e,*(void **)(e+8),
            e[0x2b],(uint8_t)(e[0x144]&4u),0};
    }
    world_pause=p;
    return registry_exact(0) && scheduler_exact();
}
static void report_state(SudekiMpLanStoryClientReport *out,BOOL pause,BOOL roster,
    BOOL input,BOOL scheduler,BOOL entities) {
    if(!out) return;
    *out=(SudekiMpLanStoryClientReport){retained_tasks.load_generation,
        retained_scene.epoch,retained_scene.revision,world_pause.count,observation,
        phase,attempted && SudekiMpLanPartyMenuNativeOwnsPause(),pause,roster,input,scheduler,entities};
}
static BOOL unknown(const char *reason) {
    phase=SUDEKIMP_STORY_CLIENT_UNKNOWN;
    if(!logged_unknown) {
        SudekiMpLogFormat("lan_story_client event=unknown reason=%s pause_retained=%u gameplay_ready=0\r\n",
            reason,attempted && SudekiMpLanPartyMenuNativeOwnsPause());
        if(native_thread_exact() && world_identity()) {
            uint32_t time[4]; memcpy(time,world_pause.gel+0x38,sizeof(time));
            SudekiMpLogFormat("lan_story_client event=unknown_detail owners=1 task_created_expected=%lu task_created_actual=%lu gel_time_equal=%u gel_expected=%08lx,%08lx,%08lx,%08lx gel_actual=%08lx,%08lx,%08lx,%08lx task_current_empty=%u trigger_queue=%lu trigger_fault=%ld trigger_sweep_held=%ld trigger_dispatch_held=%ld load_exact=%u\r\n",
                (unsigned long)world_pause.created_tasks,(unsigned long)*(uint32_t *)(base+TASK_CREATED),
                !memcmp(time,world_pause.gel_time,sizeof(time)),
                (unsigned long)world_pause.gel_time[0],(unsigned long)world_pause.gel_time[1],
                (unsigned long)world_pause.gel_time[2],(unsigned long)world_pause.gel_time[3],
                (unsigned long)time[0],(unsigned long)time[1],(unsigned long)time[2],(unsigned long)time[3],
                !*(void **)(world_pause.manager+0x10080),
                (unsigned long)*(uint32_t *)(world_pause.trigger+0x14),
                InterlockedCompareExchange(&trigger_fault,0,0),
                InterlockedCompareExchange(&trigger_suppressed[0],0,0),
                InterlockedCompareExchange(&trigger_suppressed[1],0,0),same_load());
        }
        logged_unknown=TRUE;
    }
    SetLastError(ERROR_INVALID_STATE); return FALSE;
}
static void clear_observation(void) {
    memset(&retained_roster,0,sizeof(retained_roster));
    memset(&retained_scene,0,sizeof(retained_scene));
    memset(&retained_tasks,0,sizeof(retained_tasks));
    memset(&retained_load,0,sizeof(retained_load));
    memset(&world_pause,0,sizeof(world_pause));
    memset(entity_pause,0,sizeof(entity_pause));
    attempted=FALSE; logged_unknown=FALSE; presentation_active=FALSE;
    recruiting=FALSE; memset(&recruit_observation,0,sizeof(recruit_observation));
    exit_prepared=exit_released=FALSE;
    phase=SUDEKIMP_STORY_CLIENT_IDLE;
    InterlockedExchange(&retained_reference,0);
}

BOOL SudekiMpLanStoryClientInstall(HMODULE image,SudekiMpLanStoryClientInputClosed closed) {
    uint8_t *b=(uint8_t *)image;
    static const uint8_t gel_times[]={0x8b,0x44,0x24,8,0x8b,8,0x89,0x4e,0x38,
        0x8b,0x50,4,0x89,0x56,0x3c,0x8b,0x48,8,0x89,0x4e,0x40,
        0x8b,0x50,0x0c,0x57,0x8b,0x7e,0x30,0x89,0x56,0x44};
    if(!image || !closed || installed || base || (trigger_image && trigger_image!=b) ||
        InterlockedCompareExchange(&trigger_held,0,0) ||
        InterlockedCompareExchange(&trigger_callbacks,0,0) || InterlockedCompareExchange(&busy,0,0) ||
        !SudekiMpCheckLoadedExecutable(image)) { SetLastError(ERROR_INVALID_STATE); return FALSE; }
    /* Prove the read-only scheduler observation offsets against the supported
     * image. MenuNative separately verifies and owns the actual pause entry. */
    if(!cluster_script_contract(b) ||
        !call_target(b+CLUSTER_EXIT_QUERY,b+SCRIPT_QUERY) ||
        !call_target(b+CLUSTER_ENTRY_QUERY,b+SCRIPT_QUERY) ||
        !call_target(b+ACTIVITY_PAUSE_CALL,b+UPDATE_PAUSE) ||
        !call_target(b+ACTIVITY_RESUME_CALL,b+UPDATE_RESUME) ||
        memcmp(b+0x25e0c,gel_times,sizeof(gel_times)) ||
        memcmp(b+0x1c3219,"\x01\x3d",2) ||
        *(void **)(b+0x1c321b)!=b+TASK_CREATED ||
        memcmp(b+0xfd622,"\x8b\x4f\x34",3) ||
        memcmp(b+0xfd64c,"\x8b\x47\x3c\x8d\x04\x98\x8b\x00",8) ||
        !call_target(b+TRIGGER_SWEEP_CALL,b+TRIGGER_SWEEP) ||
        !call_target(b+TRIGGER_DISPATCH_CALL,b+TRIGGER_DISPATCH) ||
        memcmp(b+0x33717,"\x8b\x35",2) || *(void **)(b+0x33719)!=b+TRIGGER_GLOBAL ||
        b[0x3373e]!=0x56 || memcmp(b+0x77e1d,"\x81\xc7\x90\x0c\x00\x00\x57",7) ||
        memcmp(b+TRIGGER_SWEEP,"\x83\xec\x58\xa1",4) ||
        *(void **)(b+TRIGGER_SWEEP+4)!=b+0x408d1cu ||
        memcmp(b+TRIGGER_DISPATCH,"\x81\xec\xd8\x00\x00\x00\xa1",7) ||
        *(void **)(b+TRIGGER_DISPATCH+7)!=b+GEL_GLOBAL ||
        memcmp(b+0xd31a,"\x5f\x5e\x5d\x5b\x83\xc4\x58\xc2\x04\x00",10) ||
        memcmp(b+0xcb67,"\x5f\x5e\x5d\x5b\x81\xc4\xd8\x00\x00\x00\xc2\x04\x00",13) ||
        memcmp(b+0xc4ce,"\xc7\x00",2) || *(void **)(b+0xc4d0)!=b+0x2c5458u ||
        memcmp(b+0xc4d4,"\xc7\x40\x08",3) || *(void **)(b+0xc4d7)!=b+0x2c5460u ||
        memcmp(b+0xc4e2,"\xc7\x40\x10",3) || *(void **)(b+0xc4e5)!=b+0x2c5470u) {
        SetLastError(ERROR_BAD_EXE_FORMAT); return FALSE;
    }
    base=b; input_closed=closed; native_thread=0; observation=0;
    clear_observation(); installed=TRUE;
    memset(&trigger_owner,0,sizeof(trigger_owner));
    InterlockedExchange(&trigger_fault,0);
    InterlockedExchange(&trigger_suppressed[0],0); InterlockedExchange(&trigger_suppressed[1],0);
    InterlockedExchange(&cluster_suppressed[0],0); InterlockedExchange(&cluster_suppressed[1],0);
    activity_observed[0]=activity_observed[1]=activity_faults=0;
    if(!trigger_image) {
        trigger_image=b;
        original_trigger_sweep=b+TRIGGER_SWEEP; original_trigger_dispatch=b+TRIGGER_DISPATCH;
        original_cluster_query=b+SCRIPT_QUERY;
        original_activity_pause=b+UPDATE_PAUSE; original_activity_resume=b+UPDATE_RESUME;
    }
    if(!SudekiMpInstallRelativeCallHook(&trigger_hooks[0],b+TRIGGER_SWEEP_CALL,
            original_trigger_sweep,trigger_sweep) ||
        !SudekiMpInstallRelativeCallHook(&trigger_hooks[1],b+TRIGGER_DISPATCH_CALL,
            original_trigger_dispatch,trigger_dispatch) ||
        !SudekiMpInstallRelativeCallHook(&cluster_hooks[0],b+CLUSTER_EXIT_QUERY,
            original_cluster_query,cluster_exit_query) ||
        !SudekiMpInstallRelativeCallHook(&cluster_hooks[1],b+CLUSTER_ENTRY_QUERY,
            original_cluster_query,cluster_entry_query) ||
        !SudekiMpInstallRelativeCallHook(&activity_hooks[0],b+ACTIVITY_PAUSE_CALL,
            original_activity_pause,activity_pause) ||
        !SudekiMpInstallRelativeCallHook(&activity_hooks[1],b+ACTIVITY_RESUME_CALL,
            original_activity_resume,activity_resume)) {
        DWORD error=GetLastError();
        if(!SudekiMpLanStoryClientUninstall()) return FALSE;
        SetLastError(error); return FALSE;
    }
    return TRUE;
}
BOOL SudekiMpLanStoryClientObserve(void *controller,
    const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryScene *scene) {
    SudekiMpLanStoryNativeRoster roster;
    SudekiMpLanStoryTaskTraceStatus tasks;
    SudekiMpStoryLoadResult load;
    if(!installed || attempted || presentation_active || InterlockedCompareExchange(&trigger_held,0,0) ||
        (phase!=SUDEKIMP_STORY_CLIENT_IDLE &&
        phase!=SUDEKIMP_STORY_CLIENT_OBSERVED) ||
        (native_thread && native_thread!=GetCurrentThreadId()) ||
        !load_ready(&tasks,&load) || !input_closed || !input_closed(controller) ||
        !SudekiMpLanStoryObserverRoster(controller,w,scene,&roster) ||
        !SudekiMpLanStoryObserverRosterStillExact(w,&roster)) {
        SetLastError(ERROR_NOT_READY); return FALSE;
    }
    native_thread=GetCurrentThreadId(); retained_roster=roster; retained_scene=*scene;
    retained_tasks=tasks; retained_load=load; phase=SUDEKIMP_STORY_CLIENT_OBSERVED;
    SetLastError(ERROR_SUCCESS); return TRUE;
}
BOOL SudekiMpLanStoryClientAcquire(SudekiMpLanStoryClientReport *out) {
    BOOL paused=FALSE;
    if(!native_thread_exact() || presentation_active || InterlockedCompareExchange(&trigger_held,0,0) ||
        phase!=SUDEKIMP_STORY_CLIENT_OBSERVED || attempted ||
        !SudekiMpLanPartyMenuNativePauseExact(&paused) || paused ||
        SudekiMpLanPartyMenuNativeOwnsPause() || !roster_exact() || !same_load() ||
        !input_closed(retained_roster.controller) || !capture_registry()) {
        report_state(out,FALSE,FALSE,FALSE,FALSE,FALSE);
        SetLastError(ERROR_NOT_READY); return FALSE;
    }
    attempted=TRUE; phase=SUDEKIMP_STORY_CLIENT_ACQUIRING;
    trigger_owner.manager=world_pause.trigger; trigger_owner.world=retained_roster.world;
    trigger_owner.group=retained_roster.group; trigger_owner.controller=retained_roster.controller;
    trigger_owner.cluster=world_pause.cluster; trigger_owner.script_manager=world_pause.manager;
    trigger_owner.pause_released=FALSE;
    InterlockedExchange(&trigger_held,1);
    InterlockedExchange(&retained_reference,1);
    InterlockedIncrement(&busy);
    BOOL acquired=SudekiMpLanPartyMenuNativeSetPaused(TRUE);
    InterlockedDecrement(&busy);
    BOOL references=registry_exact(1),scheduler=scheduler_exact();
    BOOL roster=roster_exact(),input=input_closed(retained_roster.controller);
    BOOL exact=SudekiMpLanPartyMenuNativePauseExact(&paused) && paused &&
        SudekiMpLanPartyMenuNativeOwnsPause();
    if(!acquired || !references || !scheduler || !roster || !input || !exact || !same_load()) {
        unknown("pause_acquire_confirmation");
        report_state(out,exact,roster,input,scheduler,references); return FALSE;
    }
    phase=SUDEKIMP_STORY_CLIENT_PAUSED;
    SudekiMpLogFormat("lan_story_client event=native_pause_acquired load=%lu epoch=%lu revision=%lu entities=%u gameplay_ready=0\r\n",
        (unsigned long)retained_tasks.load_generation,(unsigned long)retained_scene.epoch,
        (unsigned long)retained_scene.revision,world_pause.count);
    return SudekiMpLanStoryClientService(out);
}
BOOL SudekiMpLanStoryClientService(SudekiMpLanStoryClientReport *out) {
    BOOL paused=FALSE;
    if(!native_thread_exact() || presentation_active || InterlockedCompareExchange(&busy,0,0) ||
        !presentation_pause_exact(&paused)) {
        report_state(out,FALSE,FALSE,FALSE,FALSE,FALSE);
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    if(recruiting) {
        report_state(out,paused && SudekiMpLanPartyMenuNativeOwnsPause(),FALSE,
            input_closed(retained_roster.controller),FALSE,FALSE);
        SetLastError(ERROR_IO_PENDING); return FALSE;
    }
    if((phase!=SUDEKIMP_STORY_CLIENT_PAUSED && phase!=SUDEKIMP_STORY_CLIENT_UNKNOWN) || !attempted) {
        report_state(out,FALSE,FALSE,FALSE,FALSE,FALSE);
        SetLastError(ERROR_NOT_READY); return FALSE;
    }
    BOOL exact=paused && SudekiMpLanPartyMenuNativeOwnsPause();
    BOOL roster=roster_exact(),input=input_closed(retained_roster.controller);
    BOOL entities=registry_exact(1),scheduler=scheduler_exact() && same_load();
    if(phase==SUDEKIMP_STORY_CLIENT_UNKNOWN) {
        report_state(out,exact,roster,input,scheduler,entities);
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    if(!exact || !roster || !input || !entities || !scheduler || observation==UINT64_MAX) {
        SudekiMpLogFormat("lan_story_client event=pause_invariant_failed pause=%u roster=%u input=%u entities=%u scheduler=%u activity_pause=%u activity_resume=%u\r\n",
            (unsigned)exact,(unsigned)roster,(unsigned)input,(unsigned)entities,(unsigned)scheduler,
            activity_observed[0],activity_observed[1]);
        unknown("paused_observation_changed");
        report_state(out,exact,roster,input,scheduler,entities); return FALSE;
    }
    ++observation;
    report_state(out,exact,roster,input,scheduler,entities);
    SetLastError(ERROR_SUCCESS); return TRUE;
}
/* Recruitment never grants the new registry blanket trust. Every surviving
 * pointer retains its original vtables, scheduler bit and owned pause count;
 * exactly one retiring NPC and the journal-proven PC may differ. */
static BOOL recruit_delta_exact(const SudekiMpLanStoryRecruitReport *r) {
    WorldPauseObservation *p=&world_pause; BOOL paused=FALSE;
    if(!r || !native_thread_exact() || !attempted || presentation_active ||
        !presentation_pause_exact(&paused) || !paused || !SudekiMpLanPartyMenuNativeOwnsPause() ||
        !same_load() || !input_closed(retained_roster.controller) ||
        !trigger_hooks_exact() || !trigger_queue_empty() || !cluster_identity(p->cluster) ||
        InterlockedCompareExchange(&trigger_fault,0,0) ||
        r->request.load_generation!=retained_tasks.load_generation ||
        r->request.before_epoch!=retained_scene.epoch || r->request.before_revision!=retained_scene.revision ||
        r->world!=retained_roster.world || r->group!=retained_roster.group ||
        *(void **)(base+WORLD_GLOBAL)!=retained_roster.world ||
        *(void **)(base+GROUP_GLOBAL)!=retained_roster.group ||
        *(void **)(base+CONTROLLER_GLOBAL)!=retained_roster.controller ||
        *(void **)(base+REGISTRY_GLOBAL)!=p->registry ||
        *(void **)(base+SCHEDULER_GLOBAL)!=p->scheduler ||
        *(void **)(base+SPEED_GLOBAL)!=p->speed || *(void **)(base+GEL_GLOBAL)!=p->gel ||
        !readable(p->registry,0x40u) || !readable(p->speed,0x30u) || !readable(p->gel,0x4cu) ||
        *(void **)(p->gel+0x30u)!=p->manager || !readable(p->manager,0x1008cu) ||
        *(void **)(p->manager+0x10080u) ||
        *(uint16_t *)(p->speed+0x2au)!=(unsigned)p->speed_references+1u || !p->speed[0x28u] ||
        p->gel[0x23u]!=(unsigned)p->gel_references+1u ||
        memcmp(p->gel+0x38u,p->gel_time,sizeof(p->gel_time)) ||
        !r->task_delta_exact || r->accounted_created_tasks<p->created_tasks ||
        *(uint32_t *)(base+TASK_CREATED)!=r->accounted_created_tasks) return FALSE;
    unsigned count=*(unsigned *)(p->registry+0x34u),removed=r->npc_registry_removed?1u:0u;
    BOOL added=r->added_actor && r->spawn.completion_returned && r->spawn.group_member_exact &&
        r->spawn.pause_inherited && r->spawn.world_pause==1u && r->spawn.actor_pause_after==1u &&
        r->spawn.actor==r->added_actor;
    BOOL pending=!added && r->pending_actor_exact && r->pending_actor &&
        r->spawn_entered && !r->spawn.completion_returned;
    void *created=added?r->added_actor:pending?r->pending_actor:NULL;
    uint8_t **entries=*(uint8_t ***)(p->registry+0x3cu);
    if(count!=p->count-removed+(created?1u:0u) || count>MAX_ENTITIES ||
        !readable(entries,count*sizeof(*entries))) return FALSE;
    unsigned found_old=0,found_added=0,found_removed=0;
    for(unsigned i=0;i<count;++i) {
        uint8_t *e=entries[i];
        for(unsigned j=0;j<i;++j) if(entries[j]==e) return FALSE;
        if(created && e==created) {
            if(++found_added!=1u || !readable(e,0x145u) ||
                e[0x2bu]!=(added?1u:r->pending_actor_pause)) return FALSE;
            continue;
        }
        unsigned j=0;
        for(;j<p->count && entity_pause[j].entity!=e;++j) {}
        if(j==p->count || (removed && e==r->removed_npc)) return FALSE;
        EntityPauseObservation *old=&entity_pause[j];
        if(!entity_pause_exact(old,1u)) return FALSE;
        ++found_old; if(e==r->removed_npc) ++found_removed;
    }
    BOOL retained_removed=FALSE;
    for(unsigned i=0;i<p->count;++i) if(entity_pause[i].entity==r->removed_npc) retained_removed=TRUE;
    if(!retained_removed || found_old!=p->count-removed || found_added!=(unsigned)(created!=NULL) ||
        found_removed!=(removed?0u:1u)) return FALSE;
    SudekiMpLanStoryNativeRoster roster=retained_roster;
    SudekiMpLanStoryScene scene=retained_scene;
    if(added) {
        if(!readable(r->added_actor,0x98u)) return FALSE;
        roster.actors[3]=r->added_actor; roster.ai[3]=*(void **)((uint8_t *)r->added_actor+0x94u);
        roster.available_mask=scene.available_mask=12u;
    }
    return roster_matches(&roster,&scene) && *(unsigned *)(p->registry+0x34u)==count &&
        *(void ***)(p->registry+0x3cu)==(void **)entries &&
        *(uint32_t *)(base+TASK_CREATED)==r->accounted_created_tasks;
}
static BOOL recruit_world_exact(const SudekiMpLanStoryNativeRoster *roster,void *context) {
    return same_roster(roster,&retained_roster) && recruit_delta_exact(context);
}
static BOOL recruit_exact(const SudekiMpLanStoryRecruitReport *r,void *unused) {
    (void)unused;
    if(!recruiting || !recruit_delta_exact(r)) return FALSE;
    /* The first native pre-mutation report provides the verified NPC owner.
     * Retire its presentation borrow before KillSpawnGroup can destroy it. */
    if(!SudekiMpLanStoryWorldRecruiting()) {
        if(r->removal_entered || r->npc_registry_removed || r->spawn_entered ||
            !SudekiMpLanStoryWorldBeginRecruitment(&retained_roster,r->removed_npc,
                recruit_remote_before_epoch,recruit_world_exact,(void *)r)) return FALSE;
    }
    recruit_observation=*r; return TRUE;
}
BOOL SudekiMpLanStoryClientRecruiting(void) { return recruiting; }
BOOL SudekiMpLanStoryClientRecruitBegin(const SudekiMpLanStoryRecruitRequest *request,
    uint32_t remote_before_epoch) {
    if(!request || !remote_before_epoch || recruiting ||
        !SudekiMpLanStoryClientService(NULL) || retained_roster.available_mask!=4u ||
        retained_roster.leader_character!=2u || request->load_generation!=retained_tasks.load_generation ||
        request->before_epoch!=retained_scene.epoch || request->before_revision!=retained_scene.revision)
        return FALSE;
    recruiting=TRUE; recruit_remote_before_epoch=remote_before_epoch;
    phase=SUDEKIMP_STORY_CLIENT_RECRUITING;
    BOOL result=SudekiMpLanStoryRecruitBegin(request,&retained_roster,recruit_exact,NULL);
    if(!result && !SudekiMpLanStoryRecruitRetains() && !SudekiMpLanStoryWorldRecruiting()) {
        recruiting=FALSE; recruit_remote_before_epoch=0; phase=SUDEKIMP_STORY_CLIENT_PAUSED;
    }
    return result;
}
BOOL SudekiMpLanStoryClientRecruitService(SudekiMpLanStoryRecruitReport *out) {
    if(!recruiting || !native_thread_exact() || presentation_active || exit_prepared) return FALSE;
    SudekiMpLanStoryRecruitReport r={0};
    BOOL result=SudekiMpLanStoryRecruitService(&r);
    if(SudekiMpLanStoryRecruitGetReport(&r)) recruit_observation=r;
    if(out) *out=r;
    return result && recruit_delta_exact(&r);
}
static BOOL recruit_commit(void *controller,
    const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryScene *scene) {
    SudekiMpLanStoryRecruitReport r; SudekiMpLanStoryNativeRoster next;
    if(!recruiting || !w || !scene || !w->service_post_original_exact ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) ||
        !SudekiMpLanStoryRecruitGetReport(&r) || r.phase!=SUDEKIMP_STORY_RECRUIT_READY ||
        !r.setup_exact || !r.npc_destructor_returned || !r.spawn.job_destructor_returned ||
        !r.spawn.job_storage_released || !recruit_delta_exact(&r) ||
        !SudekiMpLanStoryObserverRoster(controller,w,scene,&next) ||
        next.available_mask!=12u || next.leader_character!=2u ||
        next.world!=retained_roster.world || next.descriptor!=retained_roster.descriptor ||
        next.group!=retained_roster.group || next.controller!=retained_roster.controller ||
        next.actors[2]!=retained_roster.actors[2] || next.ai[2]!=retained_roster.ai[2] ||
        next.actors[3]!=r.added_actor ||
        strcmp(scene->world,retained_scene.world) || strcmp(scene->temporary,retained_scene.temporary)) return FALSE;
    /* Commit a permutation of the retained survivors plus this single native
     * PC. Its reference zero baseline is proven by inherited pause1, not
     * inferred by subtracting one from an arbitrary current entity. */
    static EntityPauseObservation adopted[MAX_ENTITIES];
    uint8_t **entries=*(uint8_t ***)(world_pause.registry+0x3cu);
    unsigned count=*(unsigned *)(world_pause.registry+0x34u);
    for(unsigned i=0;i<count;++i) {
        uint8_t *e=entries[i];
        if(e==r.added_actor) {
            adopted[i]=(EntityPauseObservation){e,*(void **)e,*(void **)(e+8u),0u,(uint8_t)(e[0x144u]&4u),0};
        } else {
            unsigned j=0; for(;j<world_pause.count && entity_pause[j].entity!=e;++j) {}
            if(j==world_pause.count || e==r.removed_npc) return FALSE;
            adopted[i]=entity_pause[j];
        }
    }
    if(!recruit_delta_exact(&r) || !SudekiMpLanStoryObserverRosterStillExact(w,&next) ||
        !SudekiMpLanStoryRecruitCommit()) return FALSE;
    memcpy(entity_pause,adopted,count*sizeof(*adopted));
    memset(adopted,0,count*sizeof(*adopted));
    world_pause.entities=entries; world_pause.count=count;
    world_pause.created_tasks=r.accounted_created_tasks;
    retained_roster=next; retained_scene=*scene;
    recruiting=FALSE; phase=SUDEKIMP_STORY_CLIENT_PAUSED;
    recruit_observation=r;
    SudekiMpLogFormat("lan_story_client event=recruitment_enrolled transaction=%lu local_epoch=%lu entities=%u task_delta=%lu input_admitted=0\r\n",
        (unsigned long)r.request.transaction,(unsigned long)next.epoch,count,
        (unsigned long)r.spawn.completion_tasks_created);
    return TRUE;
}

BOOL SudekiMpLanStoryClientRecruitCommit(void *controller,
    const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryScene *scene) {
    if(!w || recruit_commit_witness || effects_witness || presentation_active ||
        !w->service_post_original_exact || !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w))
        return FALSE;
    recruit_commit_witness=w;
    BOOL result=recruit_commit(controller,w,scene);
    recruit_commit_witness=NULL;
    return result;
}

static BOOL control_selection_exact(const SudekiMpLanStoryNativeRoster *r,
    const SudekiMpLanStoryScene *scene,void *unused) {
    (void)unused;
    BOOL paused=FALSE;
    return r && scene && recruit_commit_witness && native_thread_exact() &&
        phase==SUDEKIMP_STORY_CLIENT_PAUSED && !recruiting &&
        presentation_pause_exact(&paused) && paused && SudekiMpLanPartyMenuNativeOwnsPause() &&
        r->available_mask==retained_roster.available_mask && r->leader_character<4u &&
        (r->available_mask&(1u<<r->leader_character)) &&
        r->epoch==retained_roster.epoch && r->revision>=retained_roster.revision &&
        r->world==retained_roster.world && r->descriptor==retained_roster.descriptor &&
        r->group==retained_roster.group && r->controller==retained_roster.controller &&
        !memcmp(r->actors,retained_roster.actors,sizeof(r->actors)) &&
        !memcmp(r->ai,retained_roster.ai,sizeof(r->ai)) &&
        scene->epoch==retained_scene.epoch && scene->revision==r->revision &&
        scene->leader_seat==r->leader_character &&
        !strcmp(scene->world,retained_scene.world) && !strcmp(scene->temporary,retained_scene.temporary) &&
        roster_matches(r,scene) && SudekiMpLanStoryInputExact(r->controller,r->actors[r->leader_character]) &&
        registry_exact(1) && scheduler_exact() && same_load();
}
BOOL SudekiMpLanStoryClientSelectCharacter(void *controller,
    const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryScene *scene,unsigned character,
    SudekiMpLanStoryLocalControlReport *out) {
    SudekiMpLanStoryLocalControlReport report={0}; BOOL result=FALSE;
    if(!w || !scene || !native_thread_exact() || recruit_commit_witness || effects_witness ||
        presentation_active || InterlockedCompareExchange(&busy,0,0) ||
        !w->service_post_original_exact || !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) ||
        SudekiMpLanStoryWorldRecruiting() || SudekiMpLanStoryReplicaRetainsView()) return FALSE;
    recruit_commit_witness=w;
    SudekiMpLanStoryNativeRoster before;
    if(SudekiMpLanStoryObserverRoster(controller,w,scene,&before) &&
        control_selection_exact(&before,scene,NULL)) {
        InterlockedExchange(&busy,1);
        result=SudekiMpLanStoryLocalControlSwitch(controller,w,&before,scene,character,
            control_selection_exact,NULL,&report);
        /* The native physical front is independent of the authoritative host
         * leader. Adopt only this narrowly proved local permutation; all
         * pause references and task/registry baselines remain unchanged. */
        if(report.coherent &&
            control_selection_exact(&report.roster,&report.scene,NULL)) {
            retained_roster=report.roster; retained_scene=report.scene;
        } else if(report.entered && report.unknown) {
            unknown("local_selection_unconfirmed"); report.bound=FALSE; result=FALSE;
        } else if(report.coherent) {
            unknown("local_selection_enrollment_changed"); report.bound=FALSE; result=FALSE;
        }
        InterlockedExchange(&busy,0);
    }
    recruit_commit_witness=NULL;
    if(out) *out=report;
    return result;
}

BOOL SudekiMpLanStoryClientPausedRoster(SudekiMpLanStoryNativeRoster *roster,
    SudekiMpLanStoryScene *scene) {
    if(!roster || !scene || !SudekiMpLanStoryClientService(NULL)) return FALSE;
    *roster=retained_roster; *scene=retained_scene; return TRUE;
}
BOOL SudekiMpLanStoryClientRosterExact(const SudekiMpLanStoryNativeRoster *roster) {
    if(!same_roster(roster,&retained_roster)) return FALSE;
    if(!presentation_active) return SudekiMpLanStoryClientService(NULL);
    BOOL paused=FALSE;
    if(!native_thread_exact() || phase!=SUDEKIMP_STORY_CLIENT_PAUSED || !attempted ||
        InterlockedCompareExchange(&busy,0,0) ||
        !presentation_pause_exact(&paused) || !paused ||
        !SudekiMpLanPartyMenuNativeOwnsPause() || !pause_counts_exact() ||
        !roster_exact() || !input_closed(retained_roster.controller) ||
        !scheduler_exact() || !same_load()) return unknown("presentation_owner_changed");
    return TRUE;
}
BOOL SudekiMpLanStoryClientPresent(SudekiMpLanStoryClientPresentation callback,void *context) {
    if(!callback || presentation_active || exit_prepared || !SudekiMpLanStoryClientService(NULL)) return FALSE;
    /* The callback remains on this exact native render stack. It cannot carry
     * the fast proof into another frame: scope publication ends before the
     * complete post-operation registry observation, even on callback failure. */
    SudekiMpLanStoryNativeRoster roster=retained_roster;
    SudekiMpLanStoryScene scene=retained_scene;
    presentation_active=TRUE; region_count=0; region_next=0; region_cache_active=TRUE;
    BOOL result=callback(&roster,&scene,context);
    region_cache_active=FALSE; presentation_active=FALSE;
    BOOL still_exact=SudekiMpLanStoryClientService(NULL);
    return result && still_exact;
}
BOOL SudekiMpLanStoryClientUiPresent(SudekiMpLanStoryClientPresentation callback,
    void *context,SudekiMpLanStoryClientEffectsWitness witness,void *witness_context) {
    /* Reuse the existing exact native-callback bracket; never waive registry,
     * scheduler, roster, input or full-pause checks for UI. */
    return SudekiMpLanStoryClientEffectsPresent(callback,context,witness,witness_context);
}
BOOL SudekiMpLanStoryClientEffectsPresent(SudekiMpLanStoryClientPresentation callback,
    void *context,SudekiMpLanStoryClientEffectsWitness witness,void *witness_context) {
    if(!callback || !witness || effects_witness || presentation_active || exit_prepared ||
        exit_released || !native_thread_exact() || phase!=SUDEKIMP_STORY_CLIENT_PAUSED ||
        InterlockedCompareExchange(&busy,0,0) || !witness(witness_context)) return FALSE;
    effects_witness=witness; effects_witness_context=witness_context;
    BOOL result=SudekiMpLanStoryClientPresent(callback,context);
    effects_witness=NULL; effects_witness_context=NULL;
    return result;
}
BOOL SudekiMpLanStoryClientRenderDispatch(SudekiMpLanStoryClientRenderDispatcher dispatch,
    void *context,SudekiMpLanStoryClientEffectsWitness witness,void *witness_context) {
    if(!dispatch || !witness || effects_witness || presentation_active || exit_prepared ||
        exit_released || !native_thread_exact() || phase!=SUDEKIMP_STORY_CLIENT_PAUSED ||
        InterlockedCompareExchange(&busy,0,0) || !witness(witness_context)) return FALSE;
    effects_witness=witness; effects_witness_context=witness_context;
    BOOL result=SudekiMpLanStoryClientService(NULL);
    if(result) {
        dispatch(context);
        result=SudekiMpLanStoryClientService(NULL);
    }
    effects_witness=NULL; effects_witness_context=NULL;
    return result;
}
static BOOL cleanup_exact(void) {
    BOOL paused=FALSE;
    return native_thread_exact() && attempted && !presentation_active &&
        !InterlockedCompareExchange(&busy,0,0) &&
        SudekiMpLanPartyMenuNativePauseExact(&paused) && paused &&
        SudekiMpLanPartyMenuNativeOwnsPause() && roster_exact() &&
        input_closed(retained_roster.controller) && registry_exact(1);
}
BOOL SudekiMpLanStoryClientCleanupRoster(SudekiMpLanStoryNativeRoster *roster,
    SudekiMpLanStoryScene *scene) {
    if(!roster || !scene || !cleanup_exact()) return FALSE;
    *roster=retained_roster; *scene=retained_scene; return TRUE;
}
BOOL SudekiMpLanStoryClientCleanupRosterExact(const SudekiMpLanStoryNativeRoster *roster) {
    return same_roster(roster,&retained_roster) && cleanup_exact();
}
BOOL SudekiMpLanStoryClientPrepareExit(SudekiMpLanStoryClientReport *out) {
    if(!installed || (!attempted && !InterlockedCompareExchange(&busy,0,0))) {
        if(out) memset(out,0,sizeof(*out));
        return TRUE;
    }
    if(presentation_active || exit_released || !cleanup_exact() ||
        !trigger_hooks_exact() || !trigger_queue_empty() ||
        InterlockedCompareExchange(&trigger_callbacks,0,0) ||
        !SudekiMpLanPartyMenuNativePreparePauseExit()) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    exit_prepared=TRUE;
    report_state(out,TRUE,TRUE,TRUE,scheduler_exact() && same_load(),TRUE);
    SetLastError(ERROR_SUCCESS); return TRUE;
}
BOOL SudekiMpLanStoryClientDrain(SudekiMpLanStoryClientReport *out) {
    BOOL paused=FALSE;
    if(presentation_active) return pin(ERROR_BUSY);
    if(!installed) { if(out) memset(out,0,sizeof(*out)); return TRUE; }
    /* Untouched/partially initialized composition owns no native reference.
     * It need not wait for a world or render callback which may never exist. */
    if(!attempted && !exit_released && !InterlockedCompareExchange(&busy,0,0)) {
        if(native_thread && native_thread!=GetCurrentThreadId()) return pin(ERROR_BUSY);
        clear_observation(); report_state(out,FALSE,FALSE,FALSE,FALSE,FALSE); return TRUE;
    }
    if(!native_thread_exact() || !exit_prepared || InterlockedCompareExchange(&busy,0,0) ||
        !SudekiMpLanPartyMenuNativeExitPauseExact(&paused)) {
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    /* A changed GEL task clock need not make a positively balanced reference
     * impossible to retire. Still require the exact native registry and all
     * counts; never invoke Unpause on a replacement/new entity. */
    if(!exit_roster_exact() || (!exit_released && !input_closed(retained_roster.controller)) ||
        !trigger_hooks_exact() || !trigger_queue_empty() ||
        InterlockedCompareExchange(&trigger_callbacks,0,0)) return pin(ERROR_BUSY);
    if(!SudekiMpLanPartyMenuNativeOwnsPause()) {
        if(!registry_exact(0)) return pin(ERROR_BUSY);
    } else {
        /* NativeMenu may have completed the decrement but retained its
         * RELEASE_VERIFY state. Retry that verification without decrementing
         * a second time. Its internal lease owns which operation is pending. */
        BOOL release_returned=phase==SUDEKIMP_STORY_CLIENT_RELEASING && registry_exact(0);
        if(!release_returned && (!paused || !registry_exact(1))) return pin(ERROR_BUSY);
        phase=SUDEKIMP_STORY_CLIENT_RELEASING;
        InterlockedIncrement(&busy);
        BOOL released=SudekiMpLanPartyMenuNativeFinishPauseExit();
        InterlockedDecrement(&busy);
        if(!released || SudekiMpLanPartyMenuNativeOwnsPause() || !registry_exact(0))
            return pin(ERROR_BUSY);
    }
    if(!exit_released)
        SudekiMpLogWrite("lan_story_client event=native_pause_released references_balanced=1 boundary=terminal_ui gameplay_ready=0\r\n");
    /* Keep the empty trigger queue/contact hold across the immediately
     * following native quit. If quit cannot enter, the caller must reacquire
     * the full pause before yielding to another native update. */
    trigger_owner.pause_released=TRUE;
    attempted=FALSE; exit_released=TRUE;
    InterlockedExchange(&retained_reference,0);
    /* Keep the complete borrowed registry until FINAL Uninstall, so failure
     * of another teardown step can re-acquire before any world tick runs. */
    report_state(out,FALSE,TRUE,input_closed(retained_roster.controller),FALSE,TRUE);
    SetLastError(ERROR_SUCCESS); return TRUE;
}
BOOL SudekiMpLanStoryClientReacquireExit(SudekiMpLanStoryClientReport *out) {
    BOOL paused=FALSE;
    if(!native_thread_exact() || !exit_prepared || presentation_active ||
        InterlockedCompareExchange(&busy,0,0) ||
        InterlockedCompareExchange(&trigger_callbacks,0,0) ||
        !SudekiMpLanPartyMenuNativeExitPauseExact(&paused) ||
        !exit_roster_exact() || !input_closed(retained_roster.controller) ||
        !trigger_hooks_exact() || !trigger_queue_empty()) return pin(ERROR_BUSY);
    BOOL already=SudekiMpLanPartyMenuNativeOwnsPause() && registry_exact(1);
    if(!already && (paused || !registry_exact(0))) return pin(ERROR_BUSY);
    attempted=TRUE; phase=SUDEKIMP_STORY_CLIENT_ACQUIRING;
    trigger_owner.pause_released=FALSE;
    InterlockedExchange(&retained_reference,1);
    InterlockedIncrement(&busy);
    BOOL reacquired=SudekiMpLanPartyMenuNativeReacquirePauseExit();
    InterlockedDecrement(&busy);
    if(!reacquired || !SudekiMpLanPartyMenuNativeOwnsPause() || !registry_exact(1) ||
        !SudekiMpLanPartyMenuNativeExitPauseExact(&paused) || !paused)
        return unknown("exit_reacquire_confirmation");
    exit_released=FALSE; phase=SUDEKIMP_STORY_CLIENT_PAUSED;
    report_state(out,TRUE,TRUE,TRUE,scheduler_exact() && same_load(),TRUE);
    SetLastError(ERROR_SUCCESS); return TRUE;
}
BOOL SudekiMpLanStoryClientNativeExitReturned(void) {
    /* The bridge's cached result is the only destruction witness. Do not
     * inspect the old registry, roster, input target or trigger manager after
     * native quit: all of those were borrowed objects and may now be dead. */
    if(!installed || (native_thread && native_thread!=GetCurrentThreadId()) ||
        SudekiMpLobbyGameplayStoryExitStatus()!=1u || attempted ||
        presentation_active || InterlockedCompareExchange(&busy,0,0) ||
        InterlockedCompareExchange(&trigger_callbacks,0,0) ||
        InterlockedCompareExchange(&retained_reference,0,0) ||
        SudekiMpLanPartyMenuNativeOwnsPause() ||
        (InterlockedCompareExchange(&trigger_held,0,0) && !exit_released))
        return pin(ERROR_BUSY);
    InterlockedExchange(&trigger_held,0);
    memset(&trigger_owner,0,sizeof(trigger_owner));
    clear_observation();
    SetLastError(ERROR_SUCCESS); return TRUE;
}
BOOL SudekiMpLanStoryClientRetains(void) {
    return InterlockedCompareExchange(&retained_reference,0,0)!=0 ||
        InterlockedCompareExchange(&busy,0,0)!=0;
}
BOOL SudekiMpLanStoryClientUninstall(void) {
    if(!installed) return TRUE;
    if(attempted || exit_released || presentation_active ||
        InterlockedCompareExchange(&trigger_held,0,0) || InterlockedCompareExchange(&busy,0,0) ||
        InterlockedCompareExchange(&trigger_callbacks,0,0) ||
        (native_thread && native_thread!=GetCurrentThreadId())) return pin(ERROR_BUSY);
    BOOL restored=SudekiMpRestoreRelativeCallHook(&activity_hooks[1]);
    if(!SudekiMpRestoreRelativeCallHook(&activity_hooks[0])) restored=FALSE;
    if(!SudekiMpRestoreRelativeCallHook(&cluster_hooks[1])) restored=FALSE;
    if(!SudekiMpRestoreRelativeCallHook(&cluster_hooks[0])) restored=FALSE;
    if(!SudekiMpRestoreRelativeCallHook(&trigger_hooks[1])) restored=FALSE;
    if(!SudekiMpRestoreRelativeCallHook(&trigger_hooks[0])) restored=FALSE;
    if(!restored || InterlockedCompareExchange(&trigger_callbacks,0,0)) return pin(ERROR_BUSY);
    InterlockedExchange(&trigger_held,0);
    memset(&trigger_owner,0,sizeof(trigger_owner));
    clear_observation(); installed=FALSE; base=NULL; input_closed=NULL;
    native_thread=0; observation=0; return TRUE;
}
