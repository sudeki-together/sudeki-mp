#include "hooks/lan_story_recruit.h"
#include "hooks/lan_story_recruit_setup.h"
#include "hooks/lobby_gameplay.h"
#include "hooks/call_hook.h"
#include "cleanroom/engine.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <stdint.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Story recruitment requires the supported x86 native ABI"
#endif

enum { NPC_DESTROY_CALL=0x14cb24u,NPC_DESTROY_BODY=0x14cb40u,
    GROUP_GLOBAL=0x408d94u,WORLD_GLOBAL=0x408d10u,REGISTRY_GLOBAL=0x409d8cu,
    SPEED_GLOBAL=0x408da0u,TASK_CREATED=0x409e4cu,MAX_ENTITIES=8192u };
typedef struct CodeIdentity { unsigned rva,size; uint32_t hash; } CodeIdentity;
typedef struct Relocation { unsigned rva,target; } Relocation;
static const CodeIdentity codes[]={
    {0x74500u,66u,0xbc6b9e42u},
    {0x73fe0u,153u,0xd80b12e4u},
    {0x156160u,278u,0x53659edcu},
    {0x156100u,94u,0xbb47ce96u},
    {0x3e840u,293u,0xd7dfa2c8u},
    {0x3ecf0u,123u,0x3b506a1fu},
    {0x13db60u,146u,0x029e3607u},
    {0x14cb20u,31u,0x87caf497u},
    {0x14cb40u,542u,0xd8855920u},
    {0x14d240u,8u,0x683d51e0u},
    {0x51310u,3u,0xcee55724u},
    {0x3e250u,564u,0xd2ad71a1u},
    {0x3a490u,4u,0x24b7177au},
    {0xb1ca0u,457u,0x01efbb96u},
    {0xfc650u,94u,0x9a411f82u},
    {0xb1900u,511u,0x7aa41c37u},
    {0xb1150u,315u,0x2bb9e27eu},
    {0x13dd40u,56u,0x4cdfbe87u},
};
static const Relocation relocations[]={
    {0x3e270u,0x29a084u},
    {0x3e27du,0x408da0u},
    {0x3e28fu,0x408d50u},
    {0x3e2abu,0x408d28u},
    {0x3e2cbu,0x29a088u},
    {0x3e2d9u,0x29a068u},
    {0x3e2f3u,0x29a064u},
    {0x3e378u,0x29a088u},
    {0x3e463u,0x29a088u},
    {0x3e476u,0x29a064u},
    {0x3e8a8u,0x29a068u},
    {0x3e8c4u,0x29a064u},
    {0x3e8f1u,0x29a068u},
    {0x3e93du,0x29a064u},
    {0x3e951u,0x29a064u},
    {0x3ecfdu,0x29a068u},
    {0x3ed1du,0x29a064u},
    {0x3ed62u,0x29a064u},
    {0xb1165u,0x2cb9a8u},
    {0xb1956u,0x408d94u},
    {0xb1964u,0x409d7cu},
    {0xb1969u,0x2c4ab8u},
    {0xb1982u,0x409d8cu},
    {0xb1d09u,0x34e6bcu},
    {0xb1d94u,0x34e6bcu},
    {0xb1da3u,0x34e6bcu},
    {0xb1db5u,0x34e6bcu},
    {0xb1dc5u,0x34e6c0u},
    {0xb1dceu,0x34e6c4u},
    {0xb1de2u,0x3c2fefu},
    {0xb1df4u,0x3cb848u},
    {0xb1e03u,0x3cb848u},
    {0xb1e14u,0x3cb848u},
    {0xb1e24u,0x3cb84cu},
    {0xb1e2cu,0x3cb850u},
    {0xb1e34u,0x3cb850u},
    {0xfc655u,0x409e8cu},
    {0x13dbb4u,0x409d8cu},
    {0x14cb49u,0x2d65a0u},
    {0x14cb50u,0x2d65c4u},
    {0x14cb57u,0x2d65e4u},
    {0x14cb62u,0x2c55acu},
    {0x14cb6fu,0x2c55ecu},
    {0x14cba7u,0x2c55ecu},
    {0x14cbd4u,0x2c55ecu},
    {0x14cc03u,0x2c55ecu},
    {0x14cc1au,0x2c55ecu},
    {0x14cc24u,0x409e14u},
    {0x14cc34u,0x2c887cu},
    {0x14cc3eu,0x2c88bcu},
    {0x14cc44u,0x2c88c4u},
    {0x14cc4au,0x2c5194u},
    {0x14cc5au,0x2c55acu},
    {0x14cc70u,0x2c55ecu},
    {0x14cc7au,0x409e14u},
    {0x14cc8au,0x2c887cu},
    {0x14cc94u,0x2c88bcu},
    {0x14cc9au,0x2c88c4u},
    {0x14cca0u,0x2c5194u},
    {0x14ccb0u,0x2c55acu},
    {0x14ccc6u,0x2c55ecu},
    {0x14cd0fu,0x2c55ecu},
    {0x1561cfu,0x409d8cu},
    {0x156204u,0x3c30d4u},
    {0x156255u,0x409d8cu},
};
static uint8_t *base; /* Immutable while a restored CALL can still be in flight. */
static void *original_destroy __attribute__((used));
static SudekiMpRelativeCallHook destroy_hook;
static SudekiMpLanStoryRecruitReport state;
static SudekiMpLanStoryNativeRoster before_roster;
static SudekiMpLanStoryRecruitSetup setup;
static const char *setup_reported_reason;
static DWORD setup_reported_error;
static uint32_t setup_reported_attempted,setup_reported_completed;
static unsigned setup_report_count;
static SudekiMpLanStoryRecruitExact owner_exact;
static void *owner_context,*registry,*entry;
static DWORD native_thread;
static volatile LONG callbacks,retained,observer_fault;
static BOOL installed,active,destroy_entered,committed;
static uint32_t initial_tasks;
static SudekiMpResourceName actor_name,placement_name;
static void npc_destroy_bridge(void);

static BOOL retain(DWORD error) {
    HMODULE self;
    (void)GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        (LPCSTR)(uintptr_t)&SudekiMpLanStoryRecruitUninstall,&self);
    SetLastError(error?error:ERROR_BUSY); return FALSE;
}
static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || VirtualQuery(p,&m,sizeof(m))!=sizeof(m) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
    DWORD access=m.Protect&0xffu;
    return (access==PAGE_READONLY || access==PAGE_READWRITE || access==PAGE_WRITECOPY ||
        access==PAGE_EXECUTE_READ || access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY) &&
        a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static uint32_t dword(const void *p) { uint32_t v; memcpy(&v,p,4u); return v; }
static BOOL call_target(const uint8_t *p,const void *target) {
    int32_t v;
    if(!readable(p,5u) || *p!=0xe8u) return FALSE;
    memcpy(&v,p+1u,4u); return p+5u+v==target;
}
static BOOL hook_exact(void) {
    return destroy_hook.installed && destroy_hook.instruction==base+NPC_DESTROY_CALL &&
        original_destroy==base+NPC_DESTROY_BODY &&
        call_target(base+NPC_DESTROY_CALL,npc_destroy_bridge);
}
static BOOL code_exact(const CodeIdentity *c) {
    static const uint8_t rr[]={0x83,0xec,0x3c,0x56,0x57};
    static const uint8_t ctor[]={0x8b,0x54,0x24,8,0x83,0xec,8};
    uint32_t hash=2166136261u;
    if(!readable(base+c->rva,c->size)) return FALSE;
    for(unsigned i=0;i<c->size;++i) {
        unsigned rva=c->rva+i; uint8_t value=base[rva];
        /* Each substituted patch is independently proved to have its sole
         * exact owner before this normalized complete-body comparison. */
        if(rva>=0xb1ca0u && rva<0xb1ca0u+sizeof(rr)) value=rr[rva-0xb1ca0u];
        else if(rva>=0xb1150u && rva<0xb1150u+sizeof(ctor)) value=ctor[rva-0xb1150u];
        else if(rva>=NPC_DESTROY_CALL && rva<NPC_DESTROY_CALL+5u) {
            unsigned offset=rva-NPC_DESTROY_CALL;
            value=offset?(uint8_t)((NPC_DESTROY_BODY-NPC_DESTROY_CALL-5u)>>((offset-1u)*8u)):0xe8u;
        } else for(unsigned r=0;r<sizeof(relocations)/sizeof(relocations[0]);++r) {
            unsigned offset=rva-relocations[r].rva;
            if(offset>=4u) continue;
            if(dword(base+relocations[r].rva)!=(uintptr_t)base+relocations[r].target) return FALSE;
            value=(uint8_t)((0x400000u+relocations[r].target)>>(offset*8u)); break;
        }
        hash=(hash^value)*16777619u;
    }
    return hash==c->hash;
}
static BOOL methods_exact(BOOL hooked) {
    if(!base || !SudekiMpLanStoryTaskTraceSpawnEntryExact((HMODULE)base) ||
        (hooked?!hook_exact():!call_target(base+NPC_DESTROY_CALL,base+NPC_DESTROY_BODY))) return FALSE;
    for(unsigned i=0;i<sizeof(codes)/sizeof(codes[0]);++i) if(!code_exact(&codes[i])) return FALSE;
    return *(void **)(base+0x2d65a0u)==base+0x14cb20u &&
        *(void **)(base+0x2d65e4u)==base+0x3a490u &&
        *(void **)(base+0x2d65e4u+8u)==base+0x14d240u &&
        *(void **)(base+0x2d65e4u+0x18u)==base+0x13dd40u &&
        *(void **)(base+0x2d65e4u+0x1cu)==base+0x51310u &&
        *(void **)(base+0x2d65e4u+0x24u)==base+0x13db60u &&
        *(void **)(base+0x2cd0acu+0x1cu)==base+0xfc650u &&
        *(void **)(base+0x2d55a0u)==base+0x3a490u;
}
static BOOL resource_is(const void *p,unsigned kind,uint32_t identifier,const char *text) {
    if(!readable(p,12u)) return FALSE;
    uint32_t words[3]; memcpy(words,p,sizeof(words));
    if((words[0]&0x1fffu)!=kind || words[1]!=identifier || !words[2]) return FALSE;
    uint32_t *reference=(uint32_t *)(uintptr_t)words[2]; size_t n=strlen(text)+1u;
    if(!readable(reference,8u) || !reference[0] || reference[0]==UINT32_MAX ||
        !readable((void *)(uintptr_t)reference[1],n) ||
        memcmp((void *)(uintptr_t)reference[1],text,n)) return FALSE;
    return !memcmp(words,p,sizeof(words));
}
static BOOL registry_members(void *needle,unsigned *matches) {
    if(!base || registry!=*(void **)(base+REGISTRY_GLOBAL) || !readable(registry,0x26cu)) return FALSE;
    unsigned count=dword((uint8_t *)registry+0x34u);
    void **items=*(void ***)((uint8_t *)registry+0x3cu);
    if(!count || count>MAX_ENTITIES || !readable(items,count*4u)) return FALSE;
    *matches=0;
    for(unsigned i=0;i<count;++i) {
        if(!items[i]) return FALSE;
        for(unsigned j=0;j<i;++j) if(items[j]==items[i]) return FALSE;
        *matches+=items[i]==needle;
    }
    return dword((uint8_t *)registry+0x34u)==count &&
        *(void ***)((uint8_t *)registry+0x3cu)==items;
}
static BOOL group_exact(void) {
    uint8_t *g=state.spawn_group;
    unsigned matches;
    return g && registry_members(g,&matches) && matches==1u && readable(g,0x1b1u) &&
        *(void **)g==base+0x2c9e20u &&
        resource_is(g+0x30u,0xfa0u,0x2bc14ec5u,"NB_AilishTSASpawn") &&
        dword(g+0x168u)==1u && *(void **)(g+0x170u)==entry &&
        readable(entry,0x80u) && !dword(g+0x17cu) && !dword(g+0x1acu);
}
static BOOL retiring_npc_queued(void *actor) {
    unsigned registered=0;
    if(!state.removal_entered || !state.npc_registry_removed ||
        !registry_members(actor,&registered) || registered || !group_exact() ||
        *(void **)((uint8_t *)entry+0x6cu) || *(void **)((uint8_t *)entry+0x70u) ||
        *(void **)((uint8_t *)entry+0x74u)) return FALSE;
    unsigned count=dword((uint8_t *)registry+0x260u);
    void **queued=*(void ***)((uint8_t *)registry+0x268u);
    if(!count || count>MAX_ENTITIES || !readable(queued,count*sizeof(*queued))) return FALSE;
    unsigned matches=0;
    for(unsigned i=0;i<count;++i) matches+=queued[i]==(uint8_t *)actor+0x2cu;
    /* Resource service43E250 removes this queue entry only AFTER the native
     * deleting destructor/factory returns. No dereference after that return. */
    return matches==1u && dword((uint8_t *)registry+0x260u)==count &&
        *(void ***)((uint8_t *)registry+0x268u)==queued;
}
static BOOL npc_exact(void *actor,BOOL retiring) {
    uint8_t *a=actor;
    if(!readable(a,0x145u) || *(void **)a!=base+0x2d65a0u ||
        *(void **)(a+8u)!=base+0x2d65c4u || *(void **)(a+0x2cu)!=base+0x2d65e4u ||
        !resource_is(a+0x30u,0xf9bu,0x882ab028u,"NPC_Ailish") ||
        dword(a+0x140u)!=3u || !a[0x2bu] ||
        (retiring && !retiring_npc_queued(actor))) return FALSE;
    uint8_t *script=*(uint8_t **)(a+0x5cu),*combat=*(uint8_t **)(a+0x90u);
    if(!readable(script,0xb4u) || *(void **)script!=base+0x2cd0acu ||
        *(void **)(script+0x10u)!=actor) return FALSE;
    /* Stop53DB60 leaves an already resident state3 resource in state3; its
     * early return bypasses the state4 assignment used for unfinished loads.
     * Ready53DD40 separately waits for its components before destruction.
     * The authored NPC is a scheduled creature. Kill(0,0) bypasses its death
     * behavior and queues native resource retirement only if these native
     * protection flags are clear. Preserve native refusal when protected. */
    if(!(a[0x144u]&4u) || (combat && (!readable(combat,0x54u) ||
        *(void **)(combat+0x10u)!=actor || (dword(combat+0x50u)&0x14000u)))) return FALSE;
    return TRUE;
}
static BOOL base_owner(void) {
    return installed && native_thread && native_thread==GetCurrentThreadId() &&
        hook_exact() && state.world==*(void **)(base+WORLD_GLOBAL) &&
        state.group==*(void **)(base+GROUP_GLOBAL) && registry==*(void **)(base+REGISTRY_GLOBAL) &&
        readable(state.world,0x39bu) && *(void **)((uint8_t *)state.world+0xcu)==before_roster.descriptor &&
        !*(void **)((uint8_t *)state.world+0x14u) &&
        !InterlockedCompareExchange(&observer_fault,0,0);
}
static BOOL update_observation(void) {
    unsigned matches;
    if(!base_owner() || !group_exact() || !registry_members(state.removed_npc,&matches)) return FALSE;
    if(state.removal_entered) {
        if(matches || *(void **)((uint8_t *)entry+0x6cu) ||
            *(void **)((uint8_t *)entry+0x70u) || *(void **)((uint8_t *)entry+0x74u)) return FALSE;
        state.npc_registry_removed=TRUE;
    } else if(matches!=1u || *(void **)((uint8_t *)entry+0x6cu)!=state.removed_npc) return FALSE;
    state.task_delta_exact=dword(base+TASK_CREATED)==initial_tasks;
    state.accounted_created_tasks=initial_tasks;
    if(state.spawn_entered) {
        SudekiMpLanStorySpawnObservation spawn;
        if(!SudekiMpLanStoryTaskTraceGetSpawnObservation(&spawn) || spawn.unknown ||
            spawn.replay_transaction!=state.request.transaction ||
            spawn.load_generation!=state.request.load_generation || !spawn.construction_exact ||
            !SudekiMpLanStoryTaskTraceSpawnCounter(state.request.transaction,
                &state.accounted_created_tasks,&state.task_delta_exact)) return FALSE;
        state.spawn=spawn;
        state.pending_actor=spawn.pending_actor; state.pending_actor_exact=spawn.pending_actor_exact;
        state.pending_actor_pause=spawn.pending_pause; state.pending_setup_stage=spawn.pending_stage;
        if(spawn.completion_returned) {
            if(!spawn.group_member_exact || !spawn.pause_inherited || spawn.world_pause!=1u ||
                spawn.actor_pause_after!=1u || spawn.group_before!=1u || spawn.group_after!=2u ||
                !readable(spawn.actor,0x145u) || *(void **)spawn.actor!=base+0x2d555cu ||
                !resource_is((uint8_t *)spawn.actor+0x30u,0xf81u,0x8557d453u,"PC_AILISH") ||
                ((uint8_t *)spawn.actor)[0x2bu]!=1u ||
                !registry_members(spawn.actor,&matches) || matches!=1u) return FALSE;
            state.added_actor=spawn.actor;
        }
    }
    return state.task_delta_exact;
}
static BOOL exact_now(void) {
    return update_observation() && owner_exact && owner_exact(&state,owner_context);
}
static BOOL unknown(const char *why) {
    if(state.phase!=SUDEKIMP_STORY_RECRUIT_UNKNOWN)
        SudekiMpLogFormat("lan_story_recruit event=unknown transaction=%lu phase=%u reason=%s retained=1\r\n",
            (unsigned long)state.request.transaction,state.phase,why);
    state.phase=SUDEKIMP_STORY_RECRUIT_UNKNOWN; state.reason=why;
    SetLastError(ERROR_INVALID_STATE); return FALSE;
}
static void destructor_fault(const char *why,void *actor) {
    if(!InterlockedCompareExchange(&observer_fault,1,0))
        SudekiMpLogFormat("lan_story_recruit event=destructor_refused transaction=%lu reason=%s actor=%p expected=%p thread=%lu expected_thread=%lu\r\n",
            (unsigned long)state.request.transaction,why,actor,state.removed_npc,
            (unsigned long)GetCurrentThreadId(),(unsigned long)native_thread);
}
__attribute__((noinline,used,force_align_arg_pointer))
static unsigned npc_destroy_begin(void *actor) {
    DWORD error=GetLastError(); unsigned matched=0;
    InterlockedIncrement(&callbacks);
    if(InterlockedCompareExchange(&retained,0,0)) {
        if(native_thread!=GetCurrentThreadId()) destructor_fault("thread",actor);
        else if(actor==state.removed_npc) {
            if(!state.removal_entered || destroy_entered || state.npc_destructor_returned)
                destructor_fault("transaction",actor);
            else if(!hook_exact()) destructor_fault("hook",actor);
            else if(!base_owner()) destructor_fault("world_owner",actor);
            else if(!npc_exact(actor,TRUE)) destructor_fault("queued_npc_identity",actor);
            else {
                destroy_entered=TRUE; matched=1;
                SudekiMpLogFormat("lan_story_recruit event=npc_destructor_body_entered transaction=%lu resident_state=3 queued_resource_exact=1\r\n",
                    (unsigned long)state.request.transaction);
            }
        }
    }
    SetLastError(error); return matched;
}
__attribute__((noinline,used,force_align_arg_pointer))
static void npc_destroy_end(unsigned matched) {
    DWORD error=GetLastError();
    if(matched) {
        /* No native object reads after its destructor body returned. The
         * enclosing native deleting wrapper owns any subsequent free. */
        if(native_thread!=GetCurrentThreadId() || !destroy_entered || state.npc_destructor_returned)
            InterlockedExchange(&observer_fault,1);
        else {
            state.npc_destructor_returned=TRUE;
            SudekiMpLogFormat("lan_story_recruit event=npc_destructor_body_returned transaction=%lu actor=%p native_free_owned=1\r\n",
                (unsigned long)state.request.transaction,state.removed_npc);
        }
    }
    InterlockedDecrement(&callbacks); SetLastError(error);
}
__attribute__((naked,noinline,used)) static void npc_destroy_bridge(void) {
    /* CALL54CB24 passes one actor pointer; original body and this bridge ret4.
     * Save integer state/flags around diagnostics and preserve LastError. */
    __asm__ volatile("pushl %ebp\n\tmovl %esp,%ebp\n\tpushl %ebx\n\tpushl %esi\n\tpushl %edi\n\tsubl $20,%esp\n\t"
        "movl %eax,-16(%ebp)\n\tmovl %ecx,-20(%ebp)\n\tmovl %edx,-24(%ebp)\n\tpushfl\n\tpopl -28(%ebp)\n\t"
        "pushl 8(%ebp)\n\tcall _npc_destroy_begin\n\taddl $4,%esp\n\tmovl %eax,-32(%ebp)\n\t"
        "pushl -28(%ebp)\n\tpopfl\n\tmovl -24(%ebp),%edx\n\tmovl -20(%ebp),%ecx\n\tmovl -16(%ebp),%eax\n\t"
        "pushl 8(%ebp)\n\tcall *_original_destroy\n\t"
        "movl %eax,-16(%ebp)\n\tmovl %ecx,-20(%ebp)\n\tmovl %edx,-24(%ebp)\n\tpushfl\n\tpopl -28(%ebp)\n\t"
        "pushl -32(%ebp)\n\tcall _npc_destroy_end\n\taddl $4,%esp\n\t"
        "pushl -28(%ebp)\n\tpopfl\n\tmovl -24(%ebp),%edx\n\tmovl -20(%ebp),%ecx\n\tmovl -16(%ebp),%eax\n\t"
        "leal -12(%ebp),%esp\n\tpopl %edi\n\tpopl %esi\n\tpopl %ebx\n\tpopl %ebp\n\tret $4\n\t");
}
BOOL SudekiMpLanStoryRecruitInstall(HMODULE image) {
    if(installed || destroy_hook.installed || InterlockedCompareExchange(&callbacks,0,0) ||
        InterlockedCompareExchange(&retained,0,0) || !image ||
        (base && base!=(uint8_t *)image) || !SudekiMpCheckLoadedExecutable(image) ||
        !SudekiMpCleanroomEngineImageExact(image)) { SetLastError(ERROR_INVALID_STATE); return FALSE; }
    base=(uint8_t *)image;
    if(!methods_exact(FALSE) || !SudekiMpLanStoryRecruitSetupInitialize(image)) {
        SetLastError(ERROR_BAD_EXE_FORMAT); return FALSE;
    }
    /* Install is composed on the native startup/title boundary before any
     * recruitment admission. Publish immutable dependencies before the CALL. */
    original_destroy=base+NPC_DESTROY_BODY;
    if(!SudekiMpInstallRelativeCallHook(&destroy_hook,base+NPC_DESTROY_CALL,
        original_destroy,npc_destroy_bridge)) return retain(GetLastError());
    installed=TRUE; native_thread=0; active=FALSE; destroy_entered=FALSE; committed=FALSE;
    memset(&state,0,sizeof(state)); memset(&setup,0,sizeof(setup));
    setup_reported_reason=NULL; setup_reported_error=ERROR_SUCCESS;
    setup_reported_attempted=setup_reported_completed=0;
    setup_report_count=0;
    owner_exact=NULL; owner_context=NULL; registry=entry=NULL;
    InterlockedExchange(&observer_fault,0);
    SetLastError(ERROR_SUCCESS); return TRUE;
}
BOOL SudekiMpLanStoryRecruitBegin(const SudekiMpLanStoryRecruitRequest *request,
    const SudekiMpLanStoryNativeRoster *roster,SudekiMpLanStoryRecruitExact exact,void *context) {
    SudekiMpLanStoryTaskTraceStatus trace;
    if(!installed || active || committed || InterlockedCompareExchange(&retained,0,0) ||
        InterlockedCompareExchange(&callbacks,0,0) || !request || !roster || !exact ||
        request->route!=SUDEKIMP_STORY_RECRUIT_LIGHTHOUSE_AILISH || !request->transaction ||
        !request->actor_generation || !request->load_generation || !request->before_epoch ||
        !request->before_revision || !request->after_epoch || !request->after_revision ||
        request->before_epoch!=roster->epoch || request->before_revision!=roster->revision ||
        roster->available_mask!=4u || roster->leader_character!=2u ||
        !roster->actors[2] || roster->actors[0] || roster->actors[1] || roster->actors[3] ||
        !SudekiMpLanStoryTaskTraceGetStatus(&trace) || trace.unknown ||
        trace.load_generation!=request->load_generation ||
        !trace.start_terminal || !trace.start_retired || !trace.on_load_terminal || !trace.on_load_retired ||
        !methods_exact(TRUE)) { SetLastError(ERROR_INVALID_STATE); return FALSE; }
    native_thread=GetCurrentThreadId(); before_roster=*roster;
    state=(SudekiMpLanStoryRecruitReport){.request=*request,.phase=SUDEKIMP_STORY_RECRUIT_REMOVING,
        .world=roster->world,.group=roster->group,.reason="removing_authored_npc"};
    registry=*(void **)(base+REGISTRY_GLOBAL); owner_exact=exact; owner_context=context;
    initial_tasks=dword(base+TASK_CREATED); state.accounted_created_tasks=initial_tasks;
    state.task_delta_exact=TRUE;
    if(!base_owner() || !readable(roster->descriptor,0x54u) ||
        !readable(*(void **)((uint8_t *)roster->descriptor+0x24u),15u) ||
        ((const char *)*(void **)((uint8_t *)roster->descriptor+0x24u))[14] ||
        _stricmp(*(const char **)((uint8_t *)roster->descriptor+0x24u),"newbrightwater") ||
        !readable(registry,0x26cu) || *(void **)((uint8_t *)registry+0x30u) ||
        dword((uint8_t *)registry+0x260u)) goto reject;
    unsigned count=dword((uint8_t *)registry+0x34u);
    void **items=*(void ***)((uint8_t *)registry+0x3cu);
    if(!count || count>MAX_ENTITIES || !readable(items,count*4u)) goto reject;
    unsigned group_matches=0,npc_matches=0;
    for(unsigned i=0;i<count;++i) {
        uint8_t *e=items[i];
        if(!readable(e,0x3cu)) goto reject;
        if(*(void **)e==base+0x2c9e20u &&
            resource_is(e+0x30u,0xfa0u,0x2bc14ec5u,"NB_AilishTSASpawn")) {
            state.spawn_group=e; ++group_matches;
        }
        if(*(void **)e==base+0x2d65a0u &&
            resource_is(e+0x30u,0xf9bu,0x882ab028u,"NPC_Ailish")) {
            state.removed_npc=e; ++npc_matches;
        }
        if(resource_is(e+0x30u,0xf81u,0x8557d453u,"PC_AILISH")) goto reject;
    }
    if(group_matches!=1u || npc_matches!=1u || !readable(state.spawn_group,0x1b1u)) goto reject;
    entry=*(void **)((uint8_t *)state.spawn_group+0x170u);
    if(!group_exact() || !npc_exact(state.removed_npc,FALSE) ||
        *(void **)((uint8_t *)entry+0x6cu)!=state.removed_npc ||
        *(void **)((uint8_t *)entry+0x70u) || *(void **)((uint8_t *)entry+0x74u)) goto reject;
    InterlockedExchange(&retained,1);
    active=TRUE;
    if(!exact_now()) { active=FALSE; return unknown("before_native_removal"); }
    state.removal_entered=TRUE;
    void *group=state.spawn_group,*kill=base+0x74500u;
    /* Exact target of authored KillSpawnGroup, EAX=validated native group.
     * Callee preserves EBX/ESI/EDI/EBP; no manually edited object state. */
    __asm__ volatile("call *%[function]" : "+a"(group) : [function]"r"(kill)
        : "ecx","edx","memory","cc");
    active=FALSE;
    if(!exact_now()) return unknown("native_removal_return");
    SudekiMpLogFormat("lan_story_recruit event=removal_requested transaction=%lu local_generation=%lu npc=%p spawn_group=%p registry_removed=1 pause_retained=1\r\n",
        (unsigned long)request->transaction,(unsigned long)request->load_generation,
        state.removed_npc,state.spawn_group);
    SetLastError(ERROR_SUCCESS); return TRUE;
 reject:
    memset(&state,0,sizeof(state)); owner_exact=NULL; owner_context=NULL; registry=entry=NULL;
    SetLastError(ERROR_NOT_SUPPORTED); return FALSE;
}
static BOOL names_release(void) {
    SudekiMpResourceName *actor=&actor_name,*placement=&placement_name;
    /* Caller-owned names survive the synchronous RR call. The native job
     * copied and retained its own backing during construction. */
    if((actor->text_reference && (!readable(actor->text_reference,8u) || !actor->text_reference[0])) ||
        (placement->text_reference && (!readable(placement->text_reference,8u) || !placement->text_reference[0])))
        return FALSE;
    SudekiMpCleanroomEngineReleaseResourceName(placement);
    SudekiMpCleanroomEngineReleaseResourceName(actor); return TRUE;
}
static BOOL start_spawn(void) {
    if(!state.npc_registry_removed || !state.npc_destructor_returned ||
        dword((uint8_t *)registry+0x260u) || !methods_exact(TRUE) || !exact_now()) return FALSE;
    if(actor_name.text_reference || placement_name.text_reference ||
        !SudekiMpCleanroomEngineResourceNameFromText(&actor_name,"PC_AILISH") ||
        !SudekiMpCleanroomEngineResourceNameFromText(&placement_name,"TSA_AILISH_LIGHTHOUSE")) {
        (void)names_release(); return unknown("resource_name_allocation");
    }
    if(!exact_now() || !SudekiMpLanStoryTaskTraceBeginSpawnReplay(state.request.transaction,&actor_name,&placement_name)) {
        (void)names_release(); return unknown("spawn_scope_admission");
    }
    state.phase=SUDEKIMP_STORY_RECRUIT_SPAWNING; state.spawn_entered=TRUE;
    state.reason="native_entity_setup_pending";
    ((void (__cdecl *)(const SudekiMpResourceName *,const SudekiMpResourceName *))(base+0xb1ca0u))(&actor_name,&placement_name);
    BOOL observed=SudekiMpLanStoryTaskTraceEndSpawnReplay(state.request.transaction);
    BOOL released=names_release();
    if(!observed || !released || !exact_now()) return unknown("native_spawn_return");
    SudekiMpLogFormat("lan_story_recruit event=spawn_requested transaction=%lu construction=%lu authority=contained_native_route\r\n",
        (unsigned long)state.request.transaction,(unsigned long)state.spawn.construction);
    return TRUE;
}
static BOOL setup_exact(void *actor,void *unused) {
    (void)unused;
    return actor==state.added_actor && state.spawn.completion_returned &&
        state.spawn.job_destructor_returned && state.spawn.pause_inherited && exact_now();
}
BOOL SudekiMpLanStoryRecruitService(SudekiMpLanStoryRecruitReport *out) {
    BOOL result=FALSE;
    if(!installed || !InterlockedCompareExchange(&retained,0,0) || active ||
        InterlockedCompareExchange(&callbacks,0,0) || native_thread!=GetCurrentThreadId()) {
        SetLastError(ERROR_BUSY); goto done;
    }
    if(state.phase==SUDEKIMP_STORY_RECRUIT_UNKNOWN) { SetLastError(ERROR_INVALID_STATE); goto done; }
    active=TRUE;
    if(!exact_now()) { unknown("retained_native_delta"); goto finish; }
    switch(state.phase) {
    case SUDEKIMP_STORY_RECRUIT_REMOVING:
        if(!state.npc_destructor_returned || dword((uint8_t *)registry+0x260u)) {
            SetLastError(ERROR_IO_PENDING); break;
        }
        result=start_spawn(); break;
    case SUDEKIMP_STORY_RECRUIT_SPAWNING:
        if(!state.spawn.completion_returned || !state.spawn.job_destructor_returned ||
            !state.spawn.job_storage_released || !state.added_actor) {
            SetLastError(ERROR_IO_PENDING); break;
        }
        state.phase=SUDEKIMP_STORY_RECRUIT_SETUP; state.reason="authored_actor_setup";
        /* Fall through only after positive native job disposal and pause
         * inheritance. The setup module independently validates the actor. */
        /* fall through */
    case SUDEKIMP_STORY_RECRUIT_SETUP: {
        if(!SudekiMpLanStoryTaskTraceBeginSpawnSetup(state.request.transaction)) {
            unknown("setup_observation_begin"); break;
        }
        BOOL applied=SudekiMpLanStoryRecruitSetupApply(&setup,state.added_actor,setup_exact,NULL);
        DWORD error=GetLastError();
        BOOL observed=SudekiMpLanStoryTaskTraceEndSpawnSetup(state.request.transaction);
        if(!observed || !exact_now()) { unknown("setup_delta"); break; }
        if(!applied) {
            const char *reason=setup.reason?setup.reason:"setup_busy";
            if(setup_report_count<32u && (setup_reported_reason!=reason || setup_reported_error!=error ||
                setup_reported_attempted!=setup.attempted || setup_reported_completed!=setup.completed)) {
                ++setup_report_count;
                SudekiMpLogFormat("lan_story_recruit event=setup_refused transaction=%lu reason=%s error=%lu initialized=%u attempted=%lu completed=%lu failed=%u retained=1 input_admitted=0\r\n",
                    (unsigned long)state.request.transaction,reason,(unsigned long)error,
                    (unsigned)setup.initialized,(unsigned long)setup.attempted,
                    (unsigned long)setup.completed,(unsigned)setup.failed);
                setup_reported_reason=reason; setup_reported_error=error;
                setup_reported_attempted=setup.attempted; setup_reported_completed=setup.completed;
            }
            if(setup.failed) unknown("authored_setup_failed");
            else SetLastError(error?error:ERROR_RETRY);
            break;
        }
        state.setup_exact=TRUE; state.phase=SUDEKIMP_STORY_RECRUIT_READY;
        state.reason="awaiting_registry_adoption";
        SudekiMpLogFormat("lan_story_recruit event=ready transaction=%lu actor=%p tasks=%lu sp_before=%.1f max_sp_before=%.1f sp_after=%.1f max_sp_after=%.1f native_setup_exact=1 input_admitted=0\r\n",
            (unsigned long)state.request.transaction,state.added_actor,
            (unsigned long)state.spawn.observed_task_count,
            (double)setup.sp,(double)setup.max_sp,(double)setup.expected_sp,(double)setup.expected_max_sp);
        result=TRUE; SetLastError(ERROR_SUCCESS); break;
    }
    case SUDEKIMP_STORY_RECRUIT_READY: result=TRUE; SetLastError(ERROR_SUCCESS); break;
    default: unknown("phase"); break;
    }
 finish:
    active=FALSE;
 done:
    if(out) *out=state;
    return result;
}
BOOL SudekiMpLanStoryRecruitGetReport(SudekiMpLanStoryRecruitReport *out) {
    if(!out || !installed || native_thread!=GetCurrentThreadId() || active ||
        InterlockedCompareExchange(&callbacks,0,0)) return FALSE;
    *out=state; return TRUE;
}
BOOL SudekiMpLanStoryRecruitCommit(void) {
    if(!installed || active || native_thread!=GetCurrentThreadId() ||
        InterlockedCompareExchange(&callbacks,0,0) || !InterlockedCompareExchange(&retained,0,0) ||
        state.phase!=SUDEKIMP_STORY_RECRUIT_READY || !state.setup_exact || !exact_now()) return FALSE;
    committed=TRUE; owner_exact=NULL; owner_context=NULL;
    InterlockedExchange(&retained,0);
    SetLastError(ERROR_SUCCESS); return TRUE;
}
BOOL SudekiMpLanStoryRecruitRetains(void) {
    return InterlockedCompareExchange(&retained,0,0)!=0 ||
        InterlockedCompareExchange(&callbacks,0,0)!=0;
}
BOOL SudekiMpLanStoryRecruitNativeExitReturned(void) {
    if(!installed || active || (native_thread && native_thread!=GetCurrentThreadId()) ||
        InterlockedCompareExchange(&callbacks,0,0) || SudekiMpLobbyGameplayStoryExitStatus()!=1u) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    if(!names_release()) return retain(ERROR_INVALID_DATA);
    /* The bridge proved world destruction; never read old actors/jobs now. */
    memset(&state,0,sizeof(state)); memset(&before_roster,0,sizeof(before_roster));
    memset(&setup,0,sizeof(setup)); owner_exact=NULL; owner_context=NULL; registry=entry=NULL;
    setup_reported_reason=NULL; setup_reported_error=ERROR_SUCCESS;
    setup_reported_attempted=setup_reported_completed=0;
    setup_report_count=0;
    destroy_entered=FALSE; committed=FALSE; InterlockedExchange(&retained,0);
    SetLastError(ERROR_SUCCESS); return TRUE;
}
BOOL SudekiMpLanStoryRecruitUninstall(void) {
    if(!destroy_hook.installed && !installed) return TRUE;
    if(active || (native_thread && native_thread!=GetCurrentThreadId()) ||
        SudekiMpLanStoryRecruitRetains()) return retain(ERROR_BUSY);
    if(!SudekiMpRestoreRelativeCallHook(&destroy_hook) ||
        InterlockedCompareExchange(&callbacks,0,0)) return retain(GetLastError());
    installed=FALSE; owner_exact=NULL; owner_context=NULL;
    /* Keep original_destroy/base immutable for a fetched old CALL. */
    SetLastError(ERROR_SUCCESS); return TRUE;
}
