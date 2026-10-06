#include "hooks/lan_story_task_trace.h"
#include "hooks/lobby_gameplay.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include "hooks/call_hook.h"
#include <stdint.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Story task tracing requires the supported x86 native ABI"
#endif
#define TRACE_HELPER __attribute__((noinline,used,force_align_arg_pointer))

enum {
    LOAD_START=0x100939, LOAD_ON_LOAD=0x1009df, SUBMIT=0x1c37b0,
    CREATE_DIRECT=0x1c38f2, CREATE_CHILD=0x1c4db8, CREATE=0x1c3170,
    STEP_IMMEDIATE=0x1c3968, STEP_SCHEDULED=0x1c338f, STEP=0x1c41d0,
    ARGUMENT_CALL=0x1c393a, ARGUMENT_COPY=0x1c5970,
    SPAWN_RR=0xb1ca0, SPAWN_CTOR=0xb1150, SPAWN_COMPLETE_CALL=0xb27aa,
    SPAWN_COMPLETE=0xb1530, SPAWN_VTABLE=0x2cb9a8, SPAWN_DESTROY=0xfe030,
    SPEED_GLOBAL=0x408da0, TASK_CREATED_GLOBAL=0x409e4c,
    SPAWN_JOBS=32, SPAWN_EVENTS=64,
    RETIRE=0x1c3ac0, ADD_CALL=0x23260, ADD=0x23280,
    GEL_GLOBAL=0x408d9c, RUNTIME_GLOBAL=0x3c310c, GROUP_GLOBAL=0x408d94,
    TASK_CAPACITY=1024, MAX_EVENTS=384, MAX_CRITICAL_EVENTS=128, MAX_DEPTH=32,
    HASH_START=0xc04da6a9u, HASH_ON_LOAD=0xdddd1226u,
    HASH_LIGHTHOUSE_ENTRY=0x4fb91e97u
};
typedef void (*RawFunction)(void);
typedef int (__attribute__((fastcall)) *StepFunction)(void *,void *);
typedef struct Task {
    void *thread, *manager;
    uint32_t id, hash, parent, generation, spawn_construction;
    BOOL occupied, used, terminal, retirement_entered, recruitment,spawn_setup,admission_waiting;
} Task;
typedef struct AddObservation {
    void *group, *actor;
    uint32_t count, task_id, task_hash, instruction;
    BOOL exact, program_identity;
} AddObservation;
typedef struct ArgumentObservation {
    void *thread;
    uint32_t task_id, raw[2], used_before;
    BOOL exact;
} ArgumentObservation;
typedef struct SpawnScope {
    uint32_t request,task,generation,replay_transaction,replay_created_before;
    const void *actor_name,*placement_name;
    SudekiMpLanStoryResourceObservation placement;
} SpawnScope;
typedef struct SpawnJob {
    void *job,*completion_group,*completion_speed;
    uint32_t construction,destroy_flags;
    BOOL occupied,constructed,completion_entered,destroy_entered;
    SudekiMpLanStorySpawnObservation observation;
    SudekiMpLanStoryResourceObservation job_name;
    uint32_t placement[7];
} SpawnJob;

static uint8_t *image_base;
static SudekiMpRelativeCallHook submit_hooks[2],create_hooks[2],step_hooks[2],add_hook,argument_hook;
static SudekiMpInlineHook retire_hook;
static SudekiMpInlineHook spawn_rr_hook,spawn_ctor_hook,spawn_destroy_hook;
static SudekiMpRelativeCallHook spawn_complete_hook;
static RawFunction original_submit __attribute__((used));
static RawFunction original_create __attribute__((used));
static RawFunction original_retire __attribute__((used));
static RawFunction original_add __attribute__((used));
static RawFunction original_arguments __attribute__((used));
static RawFunction original_spawn_rr __attribute__((used));
static RawFunction original_spawn_ctor __attribute__((used));
static RawFunction original_spawn_complete __attribute__((used));
static RawFunction original_spawn_destroy __attribute__((used));
static StepFunction original_step;
static Task tasks[TASK_CAPACITY];
static AddObservation add_scopes[MAX_DEPTH];
static ArgumentObservation argument_scopes[MAX_DEPTH];
static SudekiMpLanStoryTaskTraceStatus status;
static SudekiMpLanStoryRecruitmentStatus recruitment_status;
static SudekiMpLanStorySpawnObservation spawn_observation;
static SpawnScope spawn_scopes[MAX_DEPTH];
static SpawnJob spawn_jobs[SPAWN_JOBS];
static unsigned completion_scopes[MAX_DEPTH]; /* zero masks an unrelated completion */
static unsigned spawn_depth,completion_depth,spawn_events;
static uint32_t next_spawn_request,next_construction;
static struct {
    uint32_t transaction,request,created_before;
    const void *actor,*placement;
    BOOL returned;
} spawn_replay;
static uint32_t spawn_setup_transaction,spawn_setup_construction,spawn_setup_count;
static volatile LONG spawn_fault;
static void *current_thread, *load_manager;
static DWORD native_thread;
static SudekiMpLanCastCreatedObserver cast_created;
static SudekiMpLanCastStepAdapter cast_step;
static const void *admission_consumer;
static SudekiMpStoryTaskDecide admission_decide;
static SudekiMpStoryTaskRetired admission_retired;
static unsigned admission_waiters,admission_depth;
static volatile LONG admission_fault;
static const SudekiMpStoryTaskAdmissionView *admission_current_view;
static uint32_t next_task_id, events, critical_events;
static unsigned truncated_quotas;
static unsigned submit_kind, submit_depth, add_depth, argument_depth;
static volatile LONG callbacks,trace_fault;
static BOOL installed;

static BOOL retain(DWORD error) {
    HMODULE self;
    (void)GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        (LPCSTR)(uintptr_t)&SudekiMpLanStoryTaskTraceUninstall,&self);
    SetLastError(error?error:ERROR_BUSY); return FALSE;
}

static BOOL readable(const void *p,size_t size) {
    MEMORY_BASIC_INFORMATION m;
    uintptr_t a=(uintptr_t)p;
    if(!p || !size || a>UINTPTR_MAX-size ||
        VirtualQuery(p,&m,sizeof(m))!=sizeof(m) || m.State!=MEM_COMMIT ||
        (m.Protect&(PAGE_NOACCESS|PAGE_GUARD))) return FALSE;
    DWORD protection=m.Protect&0xffu;
    return (protection==PAGE_READONLY || protection==PAGE_READWRITE ||
        protection==PAGE_WRITECOPY || protection==PAGE_EXECUTE_READ ||
        protection==PAGE_EXECUTE_READWRITE || protection==PAGE_EXECUTE_WRITECOPY) &&
        a+size<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL call_target(const uint8_t *site,const void *target) {
    int32_t displacement;
    if(!readable(site,5) || *site!=0xe8) return FALSE;
    memcpy(&displacement,site+1,4);
    return site+5+displacement==target;
}
static BOOL jump_target(const uint8_t *site,const void *target) {
    int32_t displacement;
    if(!readable(site,5) || *site!=0xe9) return FALSE;
    memcpy(&displacement,site+1,4);
    return site+5+displacement==target;
}
static BOOL spawn_destroy_body_exact(const uint8_t *base) {
    /* Full 85-byte supported body, FNV-1a with the only two PE HIGHLOW
     * relocations normalized to its preferred image base. Keep native bytes
     * in private evidence; the hook below needs only its 9-byte prefix. */
    const uint8_t *body=base+SPAWN_DESTROY;
    if(!readable(body,85) || *(void *const *)(body+5)!=base+SPAWN_VTABLE ||
        *(void *const *)(body+64)!=base+0x2c55f4) return FALSE;
    uint32_t hash=2166136261u;
    for(unsigned i=0;i<85;++i) {
        uint8_t value=body[i];
        if(i>=5 && i<9) value=(uint8_t)(0x6cb9a8u>>(8u*(i-5u)));
        else if(i>=64 && i<68) value=(uint8_t)(0x6c55f4u>>(8u*(i-64u)));
        hash=(hash^value)*16777619u;
    }
    return hash==0x6b38d3f5u;
}
static BOOL thread_exact(void) {
    DWORD thread=GetCurrentThreadId();
    DWORD owner=(DWORD)InterlockedCompareExchange((volatile LONG *)&native_thread,(LONG)thread,0);
    return !owner || owner==thread;
}
static BOOL log_allowed(BOOL critical) {
    uint32_t *count=critical?&critical_events:&events;
    uint32_t maximum=critical?MAX_CRITICAL_EVENTS:MAX_EVENTS;
    unsigned quota=critical?2u:1u;
    if(*count<maximum) { ++*count; return TRUE; }
    if(!(truncated_quotas&quota)) {
        truncated_quotas|=quota;
        status.log_truncated=TRUE;
        SudekiMpLogFormat("lan_story_task event=log_limit quota=%s observation_continues=1 authority=none\r\n",
            critical?"critical":"general");
    }
    return FALSE;
}
static void fault(const char *reason) {
    /* A foreign native caller must not race the owner's borrowed task table,
     * status structure or log quota. It only latches atomic uncertainty. */
    if(!InterlockedExchange(&trace_fault,1) &&
        native_thread==GetCurrentThreadId() && log_allowed(TRUE))
        SudekiMpLogFormat("lan_story_task event=unknown reason=%s authority=none\r\n",reason);
}
static Task *find_task(void *thread,BOOL vacant) {
    if(!thread) return NULL;
    uintptr_t key=(uintptr_t)thread;
    unsigned first=TASK_CAPACITY,begin=(unsigned)((key>>4)^(key>>12))&(TASK_CAPACITY-1u);
    for(unsigned n=0;n<TASK_CAPACITY;++n) {
        unsigned i=(begin+n)&(TASK_CAPACITY-1u);
        Task *t=&tasks[i];
        if(t->occupied && t->thread==thread) return t;
        if(!t->occupied && first==TASK_CAPACITY) first=i;
        if(!t->used) break;
    }
    return vacant && first<TASK_CAPACITY?&tasks[first]:NULL;
}
static BOOL manager_exact(void *manager) {
    uint8_t *engine=image_base?*(uint8_t **)(image_base+GEL_GLOBAL):NULL;
    return readable(engine,0x34) && *(void **)(engine+0x30)==manager &&
        readable(manager,0x1008c);
}
static BOOL descriptor_exact(void *value,unsigned kind) {
    const uint8_t *descriptor=value;
    const char *expected=kind==1?"StartLoadedGame":"OnLoadGame";
    size_t length=strlen(expected)+1;
    if(!readable(descriptor,0x90)) return FALSE;
    const char *name=(*(uint32_t *)descriptor&0x80000000u)?
        (const char *)(descriptor+4):*(const char **)(descriptor+4);
    return readable(name,length) && !memcmp(name,expected,length) &&
        *(uint32_t *)(descriptor+0x80)==(kind==1?1u:0u) &&
        (kind!=1 || *(uint32_t *)(descriptor+0x20)==1u);
}

/* This journal never owns a resource/job/entity reference. Borrowed objects
 * are inspected only on the native construction/completion stack which owns
 * them. An observed constructor at a reused address invalidates its old tag. */
static BOOL spawn_owner_thread(void) {
    return native_thread && native_thread==GetCurrentThreadId();
}
static BOOL spawn_log_allowed(void) {
    if(spawn_events>=SPAWN_EVENTS) return FALSE;
    ++spawn_events; return TRUE;
}
static void spawn_unknown(const char *reason) {
    if(!InterlockedExchange(&spawn_fault,1) && spawn_owner_thread() && spawn_log_allowed())
        SudekiMpLogFormat("lan_story_spawn event=unknown reason=%s authority=none\r\n",reason);
}
static BOOL copy_resource(const void *value,SudekiMpLanStoryResourceObservation *out) {
    uint32_t words[3],backing[2];
    memset(out,0,sizeof(*out));
    if(!readable(value,sizeof(words))) return FALSE;
    memcpy(words,value,sizeof(words));
    out->encoded_kind=words[0]; out->identifier=words[1];
    if(words[1]==0x7ffffu) return FALSE;
    if(words[2]) {
        const void *reference=(const void *)(uintptr_t)words[2];
        if(!readable(reference,sizeof(backing))) return FALSE;
        memcpy(backing,reference,sizeof(backing));
        if(!backing[0] || backing[0]==UINT32_MAX || !backing[1]) return FALSE;
        const char *text=(const char *)(uintptr_t)backing[1];
        for(unsigned i=0;i<sizeof(out->text);++i) {
            if(!readable(text+i,1)) return FALSE;
            unsigned char c=(unsigned char)text[i];
            if(!c) {
                if(!i || !readable(text,i+1u) || memcmp(text,out->text,i) ||
                    !readable(reference,sizeof(backing)) || memcmp(reference,backing,sizeof(backing)))
                    return FALSE;
                out->text_known=TRUE; break;
            }
            if(c<0x20u || c>0x7eu) return FALSE;
            out->text[i]=(char)c;
        }
        if(!out->text_known) { memset(out->text,0,sizeof(out->text)); return FALSE; }
    }
    out->exact=readable(value,sizeof(words)) && !memcmp(value,words,sizeof(words));
    return out->exact;
}
static void resource_log_text(const SudekiMpLanStoryResourceObservation *value,char out[193]) {
    unsigned used=0;
    if(value->text_known) for(unsigned i=0;i<sizeof(value->text) && value->text[i];++i) {
        char c=value->text[i];
        if(c=='"' || c=='\\') out[used++]='\\';
        out[used++]=c;
    }
    out[used]=0;
}
static BOOL same_resource(const SudekiMpLanStoryResourceObservation *a,
    const SudekiMpLanStoryResourceObservation *b) {
    return a->exact && b->exact && (a->encoded_kind&0x1fffu)==(b->encoded_kind&0x1fffu) &&
        a->identifier==b->identifier && a->text_known==b->text_known &&
        !memcmp(a->text,b->text,sizeof(a->text));
}
static BOOL same_placement(const void *value,const uint32_t recorded[7]) {
    /* EntitySetup copies the room word and six float words. The two bytes
     * between them are padding which its native constructor never writes. */
    return !memcmp(value,recorded,2) &&
        !memcmp((const uint8_t *)value+4,(const uint8_t *)recorded+4,24);
}
static void reset_spawn_observation(void) {
    memset(spawn_jobs,0,sizeof(spawn_jobs)); memset(spawn_scopes,0,sizeof(spawn_scopes));
    memset(completion_scopes,0,sizeof(completion_scopes)); completion_depth=0;
    memset(&spawn_replay,0,sizeof(spawn_replay));
    spawn_setup_transaction=0; spawn_setup_construction=0; spawn_setup_count=0;
    memset(&spawn_observation,0,sizeof(spawn_observation)); spawn_depth=0;
    InterlockedExchange(&spawn_fault,0);
}
static unsigned TRACE_HELPER spawn_rr_begin(const void *actor_name,const void *placement_name) {
    DWORD error=GetLastError(); unsigned cookie=0;
    InterlockedIncrement(&callbacks);
    if(!spawn_owner_thread()) {
        if(native_thread) spawn_unknown("spawn_foreign_thread");
        goto done;
    }
    Task *t=find_task(current_thread,FALSE);
    BOOL replay=spawn_replay.transaction && !spawn_replay.request && !current_thread &&
        actor_name==spawn_replay.actor && placement_name==spawn_replay.placement &&
        manager_exact(load_manager);
    if(!replay && (!t || t->hash!=HASH_LIGHTHOUSE_ENTRY ||
        t->generation!=status.load_generation || !manager_exact(t->manager))) goto done;
    if(spawn_depth>=MAX_DEPTH || next_spawn_request==UINT32_MAX) {
        spawn_unknown("request_capacity"); goto done;
    }
    SpawnScope *scope=&spawn_scopes[spawn_depth++]; cookie=spawn_depth;
    *scope=(SpawnScope){.request=++next_spawn_request,.task=replay?0:t->id,
        .generation=status.load_generation,.replay_transaction=replay?spawn_replay.transaction:0,
        .replay_created_before=replay?spawn_replay.created_before:0,
        .actor_name=actor_name,.placement_name=placement_name};
    if(replay) spawn_replay.request=scope->request;
    (void)copy_resource(placement_name,&scope->placement);
    if(spawn_log_allowed()) SudekiMpLogFormat(
        "lan_story_spawn event=rr_begin generation=%lu task=%lu request=%lu root_hash=%08lx replay_transaction=%lu authority=none\r\n",
        (unsigned long)scope->generation,(unsigned long)scope->task,
        (unsigned long)scope->request,(unsigned long)(replay?0:t->hash),
        (unsigned long)scope->replay_transaction);
 done:
    SetLastError(error); return cookie;
}
static void TRACE_HELPER spawn_rr_end(unsigned cookie) {
    DWORD error=GetLastError();
    if(cookie) {
        if(!spawn_owner_thread() || cookie!=spawn_depth) spawn_unknown("request_return_scope");
        else {
            SpawnScope *scope=&spawn_scopes[--spawn_depth];
            SudekiMpLanStoryResourceObservation actor,placement;
            BOOL exact_actor=copy_resource(scope->actor_name,&actor);
            BOOL exact_placement=copy_resource(scope->placement_name,&placement);
            unsigned canonical_jobs=0;
            char actor_text[193],placement_text[193];
            resource_log_text(&actor,actor_text); resource_log_text(&placement,placement_text);
            for(unsigned i=0;i<SPAWN_JOBS;++i) if(spawn_jobs[i].occupied &&
                spawn_jobs[i].observation.request==scope->request) {
                /* RR caller storage is not proved to retain its ResourceName
                 * through the native return. Its return-time visibility is
                 * diagnostic only. Keep the canonical bounded
                 * copy validated against the live job at constructor return. */
                if(spawn_jobs[i].constructed && spawn_jobs[i].job_name.exact) ++canonical_jobs;
                if(exact_placement) spawn_jobs[i].observation.placement_resource=placement;
                else if(scope->placement.exact)
                    spawn_jobs[i].observation.placement_resource=scope->placement;
                if(spawn_observation.construction==spawn_jobs[i].construction) {
                    spawn_observation=spawn_jobs[i].observation;
                }
            }
            if(spawn_log_allowed()) SudekiMpLogFormat(
                "lan_story_spawn event=rr_return generation=%lu task=%lu request=%lu actor_kind=%08lx actor_id=%08lx actor_name=\"%s\" actor_exact=%u actor_text=%u placement_kind=%08lx placement_id=%08lx placement_name=\"%s\" placement_exact=%u placement_text=%u canonical_constructed_jobs=%u actor_return_visibility_only=1 authority=none\r\n",
                (unsigned long)scope->generation,(unsigned long)scope->task,(unsigned long)scope->request,
                (unsigned long)actor.encoded_kind,(unsigned long)actor.identifier,
                actor_text,exact_actor,actor.text_known,
                (unsigned long)placement.encoded_kind,(unsigned long)placement.identifier,
                placement_text,exact_placement,placement.text_known,canonical_jobs);
            if(!canonical_jobs || (!exact_placement && !scope->placement.exact))
                spawn_unknown("resource_observation");
            if(scope->replay_transaction && scope->replay_transaction==spawn_replay.transaction &&
                scope->request==spawn_replay.request) spawn_replay.returned=TRUE;
            memset(scope,0,sizeof(*scope));
        }
    }
    InterlockedDecrement(&callbacks); SetLastError(error);
}
static unsigned TRACE_HELPER spawn_ctor_begin(void *job,const void *placement) {
    DWORD error=GetLastError(); unsigned cookie=0,slot=SPAWN_JOBS;
    InterlockedIncrement(&callbacks);
    if(!spawn_owner_thread()) {
        if(native_thread) spawn_unknown("spawn_foreign_thread");
        goto done;
    }
    for(unsigned i=0;i<SPAWN_JOBS;++i) {
        if(spawn_jobs[i].occupied && spawn_jobs[i].job==job) {
            if(!spawn_jobs[i].observation.job_destructor_returned)
                spawn_unknown("job_address_reconstructed");
            /* Keep retired copies for attributed tasks that can outlive the
             * setup job. An address reuse gets its own construction identity. */
        }
        if(slot==SPAWN_JOBS && !spawn_jobs[i].occupied) slot=i;
    }
    if(!spawn_depth) goto done;
    if(slot==SPAWN_JOBS || next_construction==UINT32_MAX || !readable(placement,28)) {
        spawn_unknown("construction_capacity_or_placement"); goto done;
    }
    const SpawnScope *scope=&spawn_scopes[spawn_depth-1];
    SpawnJob *j=&spawn_jobs[slot]; memset(j,0,sizeof(*j));
    j->occupied=TRUE; j->job=job; j->construction=++next_construction;
    j->observation.load_generation=scope->generation; j->observation.task=scope->task;
    j->observation.request=scope->request; j->observation.construction=j->construction;
    j->observation.replay_transaction=scope->replay_transaction;
    j->observation.replay_created_before=scope->replay_created_before;
    memcpy(j->placement,placement,sizeof(j->placement)); cookie=slot+1u;
 done:
    SetLastError(error); return cookie;
}
static void TRACE_HELPER spawn_ctor_end(unsigned cookie,void *result) {
    DWORD error=GetLastError();
    if(cookie) {
        if(!spawn_owner_thread() || cookie>SPAWN_JOBS) spawn_unknown("construction_return_thread");
        else {
            SpawnJob *j=&spawn_jobs[cookie-1]; uint8_t *job=j->job;
            SudekiMpLanStoryResourceObservation requested;
            const SpawnScope *scope=spawn_depth?&spawn_scopes[spawn_depth-1]:NULL;
            j->constructed=j->occupied && result==job && readable(job,0x58) &&
                *(void **)job==image_base+SPAWN_VTABLE && !job[0x54] &&
                same_placement(job+0x1c,j->placement) &&
                copy_resource(job+0x10,&j->job_name) && scope &&
                scope->request==j->observation.request &&
                copy_resource(scope->actor_name,&requested) && same_resource(&requested,&j->job_name);
            j->observation.construction_exact=j->constructed;
            if(j->constructed) {
                j->observation.actor_resource=j->job_name;
                if(scope->placement.exact) j->observation.placement_resource=scope->placement;
            }
            spawn_observation=j->observation;
            if(!j->constructed) spawn_unknown("construction_return_identity");
            char actor_text[193]; resource_log_text(&j->job_name,actor_text);
            if(spawn_log_allowed()) SudekiMpLogFormat(
                "lan_story_spawn event=constructed generation=%lu task=%lu request=%lu construction=%lu job=%p exact=%u actor_kind=%08lx actor_id=%08lx actor_name=\"%s\" actor_exact=%u authority=none\r\n",
                (unsigned long)j->observation.load_generation,(unsigned long)j->observation.task,
                (unsigned long)j->observation.request,(unsigned long)j->construction,job,j->constructed,
                (unsigned long)j->job_name.encoded_kind,(unsigned long)j->job_name.identifier,
                actor_text,j->job_name.exact);
        }
    }
    InterlockedDecrement(&callbacks); SetLastError(error);
}
static unsigned TRACE_HELPER spawn_complete_begin(void *job,void *actor) {
    DWORD error=GetLastError(); unsigned cookie=0;
    InterlockedIncrement(&callbacks);
    if(!spawn_owner_thread()) {
        if(native_thread) spawn_unknown("spawn_foreign_thread");
        goto done;
    }
    if(completion_depth>=MAX_DEPTH) { spawn_unknown("completion_capacity"); goto done; }
    completion_scopes[completion_depth++]=0; cookie=completion_depth;
    for(unsigned i=0;i<SPAWN_JOBS;++i) {
        SpawnJob *j=&spawn_jobs[i];
        if(!j->occupied || j->job!=job || j->observation.job_destructor_returned) continue;
        SudekiMpLanStoryResourceObservation resource;
        uint8_t *speed=*(uint8_t **)(image_base+SPEED_GLOBAL);
        uint8_t *group=*(uint8_t **)(image_base+GROUP_GLOBAL);
        if(!j->constructed || j->completion_entered ||
            j->observation.load_generation!=status.load_generation ||
            !readable(job,0x58) || *(void **)job!=image_base+SPAWN_VTABLE ||
            ((uint8_t *)job)[0x54]!=4u || !readable(actor,0x145) ||
            !readable(speed,0x2c) || !readable(group,0xd0) ||
            !copy_resource((uint8_t *)job+0x10,&resource) ||
            !same_resource(&resource,&j->job_name) ||
            !same_placement((uint8_t *)job+0x1c,j->placement)) {
            spawn_unknown("completion_identity"); break;
        }
        j->completion_entered=TRUE; completion_scopes[completion_depth-1]=i+1u;
        j->completion_group=group; j->completion_speed=speed;
        j->observation.actor=actor;
        j->observation.actor_pause_before=((uint8_t *)actor)[0x2b];
        j->observation.world_pause=*(uint16_t *)(speed+0x2a);
        j->observation.group_before=*(uint32_t *)(group+0xcc);
        j->observation.created_before=*(uint32_t *)(image_base+TASK_CREATED_GLOBAL);
        break;
    }
 done:
    SetLastError(error); return cookie;
}
static void TRACE_HELPER spawn_complete_end(unsigned cookie) {
    DWORD error=GetLastError();
    if(cookie) {
        if(!spawn_owner_thread() || cookie!=completion_depth) spawn_unknown("completion_return_thread");
        else {
            unsigned slot=completion_scopes[--completion_depth];
            completion_scopes[completion_depth]=0;
            if(!slot) goto done;
            SpawnJob *j=&spawn_jobs[slot-1];
            SudekiMpLanStorySpawnObservation *o=&j->observation;
            uint8_t *actor=o->actor,*group=*(uint8_t **)(image_base+GROUP_GLOBAL);
            uint8_t *speed=*(uint8_t **)(image_base+SPEED_GLOBAL);
            BOOL exact=j->occupied && j->completion_entered && readable(actor,0x145) &&
                readable(group,0xd0) && readable(speed,0x2c) &&
                group==j->completion_group && speed==j->completion_speed &&
                *(uint16_t *)(speed+0x2a)==o->world_pause;
            unsigned matches=0;
            o->completion_returned=TRUE;
            o->created_after=*(uint32_t *)(image_base+TASK_CREATED_GLOBAL);
            o->group_after=exact?*(uint32_t *)(group+0xcc):UINT32_MAX;
            if(exact && o->group_after<=4u) {
                o->actor_pause_after=actor[0x2b];
                for(unsigned i=0;i<o->group_after;++i)
                    if(*(void **)(group+0x90+i*12u)==actor) ++matches;
                o->group_member_exact=matches==1u;
                o->pause_inherited=o->group_member_exact && o->actor_pause_after==o->world_pause;
            } else exact=FALSE;
            if(!exact) spawn_unknown("completion_return_identity");
            o->unknown=InterlockedCompareExchange(&spawn_fault,0,0)!=0;
            spawn_observation=*o;
            if(spawn_log_allowed()) SudekiMpLogFormat(
                "lan_story_spawn event=completed generation=%lu task=%lu request=%lu construction=%lu actor=%p group_before=%lu group_after=%lu group_exact=%u pause_world=%u pause_before=%u pause_after=%u pause_inherited=%u tasks_before=%lu tasks_after=%lu completion_lineage_created=%lu completion_lineage_retired=%lu unknown=%u job_retired=unproven initialization_complete=unproven authority=none\r\n",
                (unsigned long)o->load_generation,(unsigned long)o->task,(unsigned long)o->request,
                (unsigned long)o->construction,actor,(unsigned long)o->group_before,
                (unsigned long)o->group_after,o->group_member_exact,o->world_pause,
                o->actor_pause_before,o->actor_pause_after,o->pause_inherited,
                (unsigned long)o->created_before,(unsigned long)o->created_after,
                (unsigned long)o->completion_tasks_created,(unsigned long)o->completion_tasks_retired,o->unknown);
        }
    }
 done:
    InterlockedDecrement(&callbacks); SetLastError(error);
}

static SpawnJob *spawn_construction(uint32_t construction) {
    if(construction) for(unsigned i=0;i<SPAWN_JOBS;++i)
        if(spawn_jobs[i].occupied && spawn_jobs[i].construction==construction)
            return &spawn_jobs[i];
    return NULL;
}
static unsigned TRACE_HELPER spawn_destroy_begin(void *job,unsigned flags) {
    DWORD error=GetLastError(); unsigned cookie=0;
    InterlockedIncrement(&callbacks);
    if(!spawn_owner_thread()) {
        if(native_thread) spawn_unknown("spawn_destroy_foreign_thread");
        goto done;
    }
    for(unsigned i=0;i<SPAWN_JOBS;++i) {
        SpawnJob *j=&spawn_jobs[i];
        if(!j->occupied || j->job!=job || j->observation.job_destructor_returned) continue;
        SudekiMpLanStoryResourceObservation resource;
        if(!j->constructed || j->destroy_entered ||
            j->observation.load_generation!=status.load_generation ||
            !readable(job,0x58) || *(void **)job!=image_base+SPAWN_VTABLE ||
            !copy_resource((uint8_t *)job+0x10,&resource) ||
            !same_resource(&resource,&j->job_name) ||
            !same_placement((uint8_t *)job+0x1c,j->placement)) {
            spawn_unknown("job_destroy_identity"); break;
        }
        j->destroy_entered=TRUE; j->destroy_flags=flags; cookie=i+1u; break;
    }
 done:
    SetLastError(error); return cookie;
}
static void TRACE_HELPER spawn_destroy_end(unsigned cookie,void *result) {
    DWORD error=GetLastError();
    if(cookie) {
        if(!spawn_owner_thread() || cookie>SPAWN_JOBS) spawn_unknown("job_destroy_return_thread");
        else {
            SpawnJob *j=&spawn_jobs[cookie-1];
            /* Native deleting destructor has returned. Only compare copied
             * identities; the job and its resource backing can be freed. */
            if(!j->occupied || !j->destroy_entered || j->job!=result ||
                j->observation.job_destructor_returned)
                spawn_unknown("job_destroy_return_identity");
            else {
                SudekiMpLanStorySpawnObservation *o=&j->observation;
                o->job_destructor_returned=TRUE;
                o->job_storage_released=(j->destroy_flags&1u)!=0;
                o->unknown=InterlockedCompareExchange(&spawn_fault,0,0)!=0;
                if(spawn_observation.construction==j->construction) spawn_observation=*o;
                if(spawn_log_allowed()) SudekiMpLogFormat(
                    "lan_story_spawn event=job_destructor_returned generation=%lu task=%lu request=%lu construction=%lu job=%p flags=%lu storage_released=%u completion_returned=%u completion_lineage_created=%lu completion_lineage_retired=%lu initialization_complete=unproven authority=none\r\n",
                    (unsigned long)o->load_generation,(unsigned long)o->task,
                    (unsigned long)o->request,(unsigned long)o->construction,j->job,
                    (unsigned long)j->destroy_flags,o->job_storage_released,o->completion_returned,
                    (unsigned long)o->completion_tasks_created,(unsigned long)o->completion_tasks_retired);
            }
        }
    }
    InterlockedDecrement(&callbacks); SetLastError(error);
}

/* Called only by the two exact saved-loader script callsites. Kind is a
 * diagnostic scope, never a substitute for a native task/scene lease. */
static unsigned TRACE_HELPER
story_submit_begin(unsigned kind,void *manager,void *descriptor) {
    DWORD error=GetLastError();
    InterlockedIncrement(&callbacks);
    if(!thread_exact()) {
        fault("saved_submit_thread"); SetLastError(error); return 0;
    }
    unsigned prior=submit_kind;
    if(submit_depth>=MAX_DEPTH ||
        !manager_exact(manager) || !descriptor_exact(descriptor,kind)) {
        fault("saved_submit_identity");
        SetLastError(error); return 0;
    }
    if(kind==1) {
        if(submit_depth || status.load_generation==UINT32_MAX) {
            fault("saved_submit_overlap"); SetLastError(error); return 0;
        }
        uint32_t generation=status.load_generation+1;
        BOOL truncated=status.log_truncated;
        memset(&status,0,sizeof(status));
        memset(&recruitment_status,0,sizeof(recruitment_status));
        reset_spawn_observation();
        status.load_generation=generation; status.log_truncated=truncated;
        /* Discard ONLY borrowed diagnostic addresses from the preceding
         * world. No native handles/references were acquired. This is not
         * evidence that any previous task completed. */
        memset(tasks,0,sizeof(tasks));
        load_manager=manager;
    } else if(!status.load_generation || load_manager!=manager) {
        fault("on_load_without_start"); SetLastError(error); return 0;
    }
    ++submit_depth; submit_kind=kind;
    if(log_allowed(TRUE)) SudekiMpLogFormat(
        "lan_story_task event=saved_submit generation=%lu kind=%u manager=%p authority=none\r\n",
        (unsigned long)status.load_generation,kind,manager);
    SetLastError(error);
    return (prior<<16)|submit_depth;
}
static void TRACE_HELPER story_submit_end(unsigned cookie) {
    DWORD error=GetLastError();
    if(cookie) {
        if(!thread_exact() || (cookie&0xffffu)!=submit_depth) fault("submit_scope_order");
        else { --submit_depth; submit_kind=cookie>>16; }
    }
    InterlockedDecrement(&callbacks);
    SetLastError(error);
}
static void TRACE_HELPER story_create_begin(void) {
    InterlockedIncrement(&callbacks);
}

static void TRACE_HELPER
story_created(uint32_t hash,void *manager,void **out_cell) {
    DWORD error=GetLastError();
    if(!thread_exact()) { fault("task_thread"); goto done; }
    if(!status.load_generation || manager!=load_manager || !manager_exact(manager)) goto done;
    if(!readable(out_cell,4)) { fault("task_result_cell"); goto done; }
    uint32_t *handle=*out_cell;
    if(!handle) {
        if((submit_kind==1 && hash==HASH_START) || (submit_kind==2 && hash==HASH_ON_LOAD))
            fault("saved_root_allocation_failed");
        goto done;
    }
    if(!readable(handle,8) || !handle[0] || !handle[1]) {
        fault("task_result_identity"); goto done;
    }
    void *thread=(void *)(uintptr_t)handle[0];
    if(!readable(thread,0x50)) { fault("task_body_identity"); goto done; }
    Task *t=find_task(thread,TRUE),*parent=find_task(current_thread,FALSE);
    if(!t || t->occupied || next_task_id==UINT32_MAX) {
        fault("task_capacity_or_reuse"); goto done;
    }
    *t=(Task){.thread=thread,.manager=manager,.id=++next_task_id,.hash=hash,
        .parent=parent?parent->id:0,.generation=status.load_generation,.occupied=TRUE,.used=TRUE};
    if(!InterlockedCompareExchange(&spawn_fault,0,0)) {
        /* A zero top entry explicitly masks an unrelated nested completion.
         * Outside that stack, only a positively attributed native task can
         * pass its construction identity to an actual child creation. */
        unsigned slot=completion_depth?completion_scopes[completion_depth-1]:0;
        uint32_t construction=completion_depth?(slot?spawn_jobs[slot-1].construction:0):
            (parent?parent->spawn_construction:spawn_setup_construction);
        BOOL setup=!completion_depth && (parent?parent->spawn_setup:spawn_setup_transaction!=0);
        SpawnJob *j=spawn_construction(construction);
        if(j && j->observation.load_generation==t->generation) {
            SudekiMpLanStorySpawnObservation *o=&j->observation;
            uint32_t *count=setup?&o->setup_tasks_created:&o->completion_tasks_created;
            if(*count==UINT32_MAX || o->observed_task_count>=SUDEKIMP_STORY_SPAWN_TASKS)
                spawn_unknown("completion_task_count");
            else {
                t->spawn_construction=construction; t->spawn_setup=setup; ++*count;
                o->observed_tasks[o->observed_task_count++]=(SudekiMpLanStorySpawnTask){
                    .id=t->id,.hash=t->hash,.parent=t->parent,.setup_scope=setup};
                if(spawn_observation.construction==construction) spawn_observation=*o;
                if(spawn_log_allowed()) SudekiMpLogFormat(
                    "lan_story_spawn event=completion_task_created generation=%lu construction=%lu id=%lu hash=%08lx parent=%lu origin=%s initialization_complete=unproven authority=none\r\n",
                    (unsigned long)t->generation,(unsigned long)construction,
                    (unsigned long)t->id,(unsigned long)t->hash,(unsigned long)t->parent,
                    completion_depth?"completion_stack":(parent?"observed_task_parent":"setup_stack"));
            }
        } else if(construction) spawn_unknown("completion_task_lineage");
    }
    if(submit_kind==1 && hash==HASH_START) {
        if(status.start_task) fault("duplicate_start_root");
        else status.start_task=t->id;
    }
    if(submit_kind==2 && hash==HASH_ON_LOAD) {
        if(status.on_load_task) fault("duplicate_on_load_root");
        else status.on_load_task=t->id;
    }
    if(log_allowed(t->id==status.start_task || t->id==status.on_load_task)) SudekiMpLogFormat(
        "lan_story_task event=create generation=%lu id=%lu hash=%08lx parent=%lu thread=%p root_kind=%u\r\n",
        (unsigned long)t->generation,(unsigned long)t->id,(unsigned long)hash,
        (unsigned long)t->parent,thread,
        t->id==status.start_task?1u:t->id==status.on_load_task?2u:0u);
done:
    /* The independent cast journal pins its own task handle here, after the
     * story observation and before immediate native execution. Neither
     * journal may infer the other's ownership or terminal state. */
    if(cast_created && native_thread==GetCurrentThreadId()) {
        SetLastError(error); cast_created(hash,out_cell);
    }
    InterlockedDecrement(&callbacks);
    SetLastError(error);
}

static SudekiMpStoryTaskAdmissionView admission_view(const Task *t,void *thread) {
    SudekiMpStoryTaskAdmissionView v={.thread=thread};
    if(t) {
        v.load_generation=t->generation;v.task_id=t->id;v.function_hash=t->hash;
        v.parent_id=t->parent;v.tracked=TRUE;v.waiting=t->admission_waiting;
    }
    return v;
}
static void admission_unknown(const char *reason) {
    InterlockedExchange(&admission_fault,1);fault(reason);
}
/* Table entries must belong to the corresponding native contiguous pool;
 * readable arbitrary addresses are not table membership. All probes are bounded. */
static BOOL instruction_pool_entry(uintptr_t item,uintptr_t pool,unsigned count,unsigned stride) {
    return count && count<=65536u && pool && pool<=UINTPTR_MAX-(size_t)count*stride &&
        item>=pool && item<pool+(size_t)count*stride && (item-pool)%stride==0;
}
static BOOL inspect_instruction(const Task *t,SudekiMpStoryTaskInstruction *out) {
    uint8_t *manager=t->manager,*thread=t->thread;
    if(!manager_exact(manager) || !readable(thread,0x50) ||
        *(void **)(image_base+0x3c3108)!=manager ||
        *(void **)(image_base+RUNTIME_GLOBAL)!=manager+0x20) return FALSE;
    uintptr_t code=*(uintptr_t *)(manager+0x34),end=*(uintptr_t *)(manager+0x38),
        limit=*(uintptr_t *)(manager+0x3c);
    uint32_t pc=*(uint32_t *)(thread+0xc);
    if(!code || end<=code || limit<end || pc>=end-code || !readable((void *)(code+pc),1))
        return FALSE;
    SudekiMpStoryTaskInstruction v={.offset=pc,.opcode=*(uint8_t *)(code+pc),
        .kind=SUDEKIMP_STORY_INSTRUCTION_OTHER};
    if(v.opcode!=0x27 && v.opcode!=0x28 && v.opcode!=0x29) { *out=v;return TRUE; }
    unsigned width=v.opcode==0x29?9u:5u;
    if(width>end-code-pc || !readable((void *)(code+pc),width)) return FALSE;
    memcpy(&v.call_hash,(void *)(code+pc+1),4);
    if(v.opcode==0x28 || v.opcode==0x29) {
        v.kind=v.opcode==0x28?SUDEKIMP_STORY_INSTRUCTION_METHOD:SUDEKIMP_STORY_INSTRUCTION_CHILD;
        if(v.opcode==0x29) memcpy(&v.argument_count,(void *)(code+pc+5),4);
        *out=v;return TRUE;
    }
    unsigned slots=*(unsigned *)(manager+0x20),count=*(unsigned *)(manager+0x2c),
        capacity=*(unsigned *)(manager+0x30);
    uintptr_t *table=*(uintptr_t **)(manager+0x24),pool=*(uintptr_t *)(manager+0x28);
    if(!slots || slots>65536u || (slots&(slots-1u)) || count>capacity || capacity>65536u ||
        !readable(table,(size_t)slots*4)) return FALSE;
    unsigned slot=v.call_hash&(slots-1u),probes;
    for(probes=0;probes<slots;++probes,slot=(slot+1)&(slots-1u)) {
        uintptr_t item=table[slot];
        if(!item) break;
        if(!instruction_pool_entry(item,pool,count,16) || !readable((void *)item,16)) return FALSE;
        if(*(uint32_t *)(item+4)==v.call_hash) {
            v.script_offset=*(uint32_t *)(item+8);
            if(v.script_offset>=end-code) return FALSE;
            v.kind=SUDEKIMP_STORY_INSTRUCTION_COMPILED;*out=v;return TRUE;
        }
    }
    if(probes==slots) return FALSE; /* Native lookup would loop forever. */
    count=*(unsigned *)(manager+0x70);capacity=*(unsigned *)(manager+0x74);
    uintptr_t bindings=*(uintptr_t *)(manager+0x78);
    pool=*(uintptr_t *)(manager+0x7c);table=(uintptr_t *)(manager+0x80);
    if(!count || count>capacity || capacity>16384u || !pool ||
        pool>UINTPTR_MAX-(size_t)count*12 || !bindings ||
        bindings>UINTPTR_MAX-(size_t)count*28 || !readable(table,16384u*4u)) return FALSE;
    slot=v.call_hash&0x3fffu;
    for(probes=0;probes<16384u;++probes,slot=(slot+1)&0x3fffu) {
        uintptr_t item=table[slot];
        if(!item) return FALSE;
        if(!instruction_pool_entry(item,pool,count,12) || !readable((void *)item,12)) return FALSE;
        if(*(uint32_t *)item!=v.call_hash || *(uint32_t *)(item+4)!=0) continue;
        unsigned index=*(uint16_t *)(item+8);
        if(index>=count) return FALSE;
        uint8_t *binding=(uint8_t *)(bindings+(size_t)index*28);
        if(!readable(binding,28)) return FALSE;
        unsigned abi=*(uint32_t *)(binding+12)&0x7fu;
        if(abi>1u) return FALSE; /* Not a supported global cdecl/stdcall record. */
        uintptr_t target=(uint32_t)(*(uint32_t *)binding+*(uint32_t *)(manager+0x6c));
        if(target<(uintptr_t)image_base+0x1000u ||
            target>=(uintptr_t)image_base+0x299000u || !readable((void *)target,1)) return FALSE;
        v.native_rva=(uint32_t)(target-(uintptr_t)image_base);
        v.argument_count=*(uint32_t *)(binding+24)>>16;
        if(v.argument_count>16u) return FALSE;
        v.kind=SUDEKIMP_STORY_INSTRUCTION_NATIVE;*out=v;return TRUE;
    }
    return FALSE;
}
BOOL SudekiMpLanStoryTaskInspectInstruction(HMODULE image,const void *consumer,
    const SudekiMpStoryTaskAdmissionView *view,SudekiMpStoryTaskInstruction *out) {
    if(!out || !view || view!=admission_current_view || !consumer || consumer!=admission_consumer ||
        admission_depth!=1 || !InterlockedCompareExchange(&callbacks,0,0) ||
        !SudekiMpLanStoryTaskHostExact(image) || InterlockedCompareExchange(&admission_fault,0,0) ||
        InterlockedCompareExchange(&trace_fault,0,0) || !view->tracked || !view->thread ||
        current_thread!=view->thread) { SetLastError(ERROR_INVALID_STATE);return FALSE; }
    Task *t=find_task(view->thread,FALSE);
    if(!t || t->id!=view->task_id || t->generation!=view->load_generation ||
        t->generation!=status.load_generation || t->manager!=load_manager ||
        t->terminal || t->retirement_entered || !inspect_instruction(t,out)) {
        SetLastError(ERROR_INVALID_DATA);return FALSE;
    }
    SetLastError(ERROR_SUCCESS);return TRUE;
}
static BOOL admission_run(void *thread,BOOL exact,uint32_t *waiting_id) {
    *waiting_id=0;
    if(!admission_decide) return TRUE;
    if(!exact || admission_depth || InterlockedCompareExchange(&admission_fault,0,0) ||
        InterlockedCompareExchange(&trace_fault,0,0)) {
        admission_unknown("admission_context");return FALSE;
    }
    Task *t=find_task(thread,FALSE);
    if(t && (t->generation!=status.load_generation || t->manager!=load_manager ||
        t->terminal || t->retirement_entered || !manager_exact(t->manager))) {
        admission_unknown("admission_task_identity");return FALSE;
    }
    SudekiMpStoryTaskAdmissionView v=admission_view(t,thread);
    ++admission_depth;
    admission_current_view=&v;
    SudekiMpStoryTaskAdmissionDecision decision=admission_decide(admission_consumer,&v);
    admission_current_view=NULL;
    --admission_depth;
    if(InterlockedCompareExchange(&admission_fault,0,0) ||
        InterlockedCompareExchange(&trace_fault,0,0)) return FALSE;
    if(decision==SUDEKIMP_STORY_TASK_RUN) {
        if(t && t->admission_waiting) *waiting_id=t->id;
        return TRUE;
    }
    if(decision!=SUDEKIMP_STORY_TASK_WAIT || !t) {
        admission_unknown("admission_unknown_decision");return FALSE;
    }
    if(!t->admission_waiting) {
        if(admission_waiters>=TASK_CAPACITY) {
            admission_unknown("admission_wait_capacity");return FALSE;
        }
        t->admission_waiting=TRUE;++admission_waiters;
    }
    return FALSE;
}
static void admission_returned(void *thread,uint32_t id,int result) {
    if(!id || result==2) return; /* Cast routing can yield without fetching. */
    if(InterlockedCompareExchange(&admission_fault,0,0) ||
        InterlockedCompareExchange(&trace_fault,0,0)) return;
    Task *t=find_task(thread,FALSE);
    /* Original execution may retire this task. That separate callback owns
     * the pending-intent notice; never dereference a returned native thread. */
    if(!t || t->id!=id) return;
    if((result!=0 && result!=1) || !t->admission_waiting || !admission_waiters) {
        admission_unknown("admission_return_identity");return;
    }
    t->admission_waiting=FALSE;--admission_waiters;
}
static int __attribute__((fastcall,force_align_arg_pointer)) story_step(void *thread,void *edx) {
    DWORD error=GetLastError();
    InterlockedIncrement(&callbacks);
    BOOL exact=thread_exact();
    void *previous=exact?current_thread:NULL;
    if(exact) current_thread=thread;
    else fault("step_thread");
    uint32_t waiting_id=0;
    BOOL run=admission_run(thread,exact,&waiting_id);
    SetLastError(error);
    int result=run?(cast_step?cast_step(thread,edx):original_step(thread,edx)):2;
    error=GetLastError();
    if(run && exact) admission_returned(thread,waiting_id,result);
    if(exact) {
        Task *t=find_task(thread,FALSE);
        if(t && result==1) {
            t->terminal=TRUE;
            SpawnJob *j=spawn_construction(t->spawn_construction);
            if(j) {
                for(unsigned i=0;i<j->observation.observed_task_count;++i)
                    if(j->observation.observed_tasks[i].id==t->id)
                        j->observation.observed_tasks[i].terminal=TRUE;
                if(spawn_observation.construction==j->construction)
                    spawn_observation=j->observation;
            }
            if(t->id==status.start_task) status.start_terminal=TRUE;
            if(t->id==status.on_load_task) status.on_load_terminal=TRUE;
            if(t->id==recruitment_status.task) recruitment_status.terminal=TRUE;
        }
        current_thread=previous;
    }
    InterlockedDecrement(&callbacks);
    SetLastError(error);
    return result;
}

static uint32_t TRACE_HELPER
story_retire_begin(void *manager,void *thread) {
    DWORD error=GetLastError();
    uint32_t result=0;
    InterlockedIncrement(&callbacks);
    if(!thread_exact()) fault("retire_thread");
    else {
        Task *t=find_task(thread,FALSE);
        if(t) {
            if(t->manager!=manager || t->retirement_entered || !manager_exact(manager))
                fault("retire_identity");
            else { t->retirement_entered=TRUE; result=t->id; }
        }
    }
    SetLastError(error); return result;
}
static void TRACE_HELPER
story_retire_end(uint32_t id,void *thread) {
    DWORD error=GetLastError();
    /* Native 5C3AC0 has returned after clearing every matching task cell and
     * calling its native pool/task cleanup. Do not dereference that retired
     * thread or an unpinned pool cell here. Identity is our call-local id. */
    if(id && !thread_exact()) fault("retire_return_thread");
    else if(id) {
        Task *t=find_task(thread,FALSE);
        if(!t || t->id!=id || !t->retirement_entered)
            fault("retire_return_identity");
        else {
            if(t->id==status.start_task) status.start_retired=TRUE;
            if(t->id==status.on_load_task) status.on_load_retired=TRUE;
            if(t->id==recruitment_status.task) recruitment_status.retired=TRUE;
            if(t->spawn_construction) {
                SpawnJob *j=spawn_construction(t->spawn_construction);
                uint32_t *created=j?(t->spawn_setup?&j->observation.setup_tasks_created:
                    &j->observation.completion_tasks_created):NULL;
                uint32_t *retired=j?(t->spawn_setup?&j->observation.setup_tasks_retired:
                    &j->observation.completion_tasks_retired):NULL;
                if(!j || j->observation.load_generation!=t->generation ||
                    *retired>=*created)
                    spawn_unknown("completion_task_retirement");
                else {
                    ++*retired;
                    unsigned matches=0;
                    for(unsigned i=0;i<j->observation.observed_task_count;++i)
                        if(j->observation.observed_tasks[i].id==t->id) {
                            j->observation.observed_tasks[i].retired=TRUE; ++matches;
                        }
                    if(matches!=1u) spawn_unknown("completion_task_retired_identity");
                    if(spawn_observation.construction==j->construction)
                        spawn_observation=j->observation;
                    if(spawn_log_allowed()) SudekiMpLogFormat(
                        "lan_story_spawn event=completion_task_retired generation=%lu construction=%lu id=%lu hash=%08lx normal_terminal=%u native_retire_returned=1 initialization_complete=unproven authority=none\r\n",
                        (unsigned long)t->generation,(unsigned long)t->spawn_construction,
                        (unsigned long)t->id,(unsigned long)t->hash,t->terminal);
                }
            }
            if((t->id==status.start_task || t->id==status.on_load_task || t->recruitment) && log_allowed(TRUE))
                SudekiMpLogFormat(
                    "lan_story_task event=retired generation=%lu id=%lu hash=%08lx normal_terminal=%u native_retire_returned=1 scene_ready=unproven\r\n",
                    (unsigned long)t->generation,(unsigned long)t->id,(unsigned long)t->hash,t->terminal);
            BOOL retire_admission=TRUE;
            if(t->admission_waiting) {
                SudekiMpStoryTaskAdmissionView v=admission_view(t,NULL);
                if(!admission_retired || !admission_waiters || admission_depth ||
                    InterlockedCompareExchange(&admission_fault,0,0) ||
                    InterlockedCompareExchange(&trace_fault,0,0)) {
                    admission_unknown("admission_retirement_context");retire_admission=FALSE;
                } else {
                    ++admission_depth;
                    retire_admission=admission_retired(admission_consumer,&v);
                    --admission_depth;
                    if(InterlockedCompareExchange(&admission_fault,0,0) ||
                        InterlockedCompareExchange(&trace_fault,0,0) || !retire_admission) {
                        admission_unknown("admission_retirement_unacknowledged");retire_admission=FALSE;
                    } else --admission_waiters;
                }
            }
            if(retire_admission) {memset(t,0,sizeof(*t)); t->used=TRUE;}
        }
    }
    InterlockedDecrement(&callbacks);
    SetLastError(error);
}

static unsigned TRACE_HELPER story_add_begin(void *group,void *actor) {
    DWORD error=GetLastError();
    unsigned cookie=0;
    InterlockedIncrement(&callbacks);
    if(!thread_exact() || add_depth>=MAX_DEPTH) fault("add_scope");
    else {
        AddObservation *a=&add_scopes[add_depth++]; cookie=add_depth;
        memset(a,0,sizeof(*a)); a->group=group; a->actor=actor;
        a->instruction=UINT32_MAX;
        a->exact=group==*(void **)(image_base+GROUP_GLOBAL) && readable(group,0xd0);
        if(a->exact) { a->count=*(uint32_t *)((uint8_t *)group+0xcc); a->exact=a->count<=4; }
        Task *t=find_task(current_thread,FALSE);
        if(t) {
            t->recruitment=TRUE; a->task_id=t->id; a->task_hash=t->hash;
            uint8_t *runtime=*(uint8_t **)(image_base+RUNTIME_GLOBAL);
            /* Native manager construction stores manager+20 in this global.
             * task+c is the VM's NEXT instruction index. This observation
             * does not establish a containing function or code-array bounds. */
            a->program_identity=manager_exact(t->manager) &&
                runtime==(uint8_t *)t->manager+0x20;
            if(a->program_identity && readable(current_thread,0x10))
                a->instruction=*(uint32_t *)((uint8_t *)current_thread+0xc);
        }
    }
    SetLastError(error); return cookie;
}
static void TRACE_HELPER story_add_end(unsigned cookie) {
    DWORD error=GetLastError();
    if(cookie) {
        if(!thread_exact() || cookie!=add_depth) fault("add_return_scope");
        else {
            AddObservation *a=&add_scopes[--add_depth];
            BOOL exact=a->exact && a->group==*(void **)(image_base+GROUP_GLOBAL) && readable(a->group,0xd0);
            uint32_t after=exact?*(uint32_t *)((uint8_t *)a->group+0xcc):UINT32_MAX;
            unsigned matches=0;
            if(exact && after<=4) {
                for(unsigned i=0;i<after;++i)
                    if(*(void **)((uint8_t *)a->group+0x90+i*0xc)==a->actor) ++matches;
            } else exact=FALSE;
            if(a->task_id && a->task_hash==HASH_LIGHTHOUSE_ENTRY) {
                /* Entry volumes legitimately start overlapping/no-op roots.
                 * Select the actual Add-bearing task, never the newest hash
                 * match, and preserve it through later no-op entries. */
                if(a->task_id!=recruitment_status.task && recruitment_status.task &&
                    !recruitment_status.retired) recruitment_status.unknown=TRUE;
                else {
                    if(a->task_id!=recruitment_status.task)
                        recruitment_status=(SudekiMpLanStoryRecruitmentStatus){
                            .load_generation=status.load_generation,.task=a->task_id,
                            .hash=a->task_hash};
                    if(recruitment_status.party_add_count==UINT32_MAX)
                        recruitment_status.unknown=TRUE;
                    else ++recruitment_status.party_add_count;
                    recruitment_status.party_add_exact=exact && matches==1u &&
                        recruitment_status.party_add_count==1u;
                    recruitment_status.party_add_before=a->count;
                    recruitment_status.party_add_after=after;
                    recruitment_status.added_actor=a->actor;
                }
                if(spawn_observation.load_generation==status.load_generation &&
                    spawn_observation.task==a->task_id && spawn_observation.actor==a->actor &&
                    spawn_observation.completion_returned) {
                    spawn_observation.party_add_matches=exact && matches==1u;
                    SpawnJob *j=spawn_construction(spawn_observation.construction);
                    if(j) j->observation.party_add_matches=spawn_observation.party_add_matches;
                    if(spawn_log_allowed()) SudekiMpLogFormat(
                        "lan_story_spawn event=public_add generation=%lu task=%lu request=%lu construction=%lu actor=%p matches=%u authority=none\r\n",
                        (unsigned long)status.load_generation,(unsigned long)a->task_id,
                        (unsigned long)spawn_observation.request,
                        (unsigned long)spawn_observation.construction,a->actor,
                        spawn_observation.party_add_matches);
                }
            }
            if(log_allowed(TRUE)) SudekiMpLogFormat(
                "lan_story_task event=party_add generation=%lu task=%lu root_hash=%08lx instruction=%08lx instruction_kind=next_vm_pc program_identity=%u code_bounds=unproven actor=%p before=%lu after=%lu exact=%u occurrences=%u recruitment_ready=unproven\r\n",
                (unsigned long)status.load_generation,(unsigned long)a->task_id,
                (unsigned long)a->task_hash,(unsigned long)a->instruction,a->program_identity,a->actor,
                (unsigned long)a->count,(unsigned long)after,exact,matches);
            memset(a,0,sizeof(*a));
        }
    }
    InterlockedDecrement(&callbacks);
    SetLastError(error);
}

/* Observe the native counted-argument transfer before its first task step.
 * These are raw VM values: a |PP signature does not prove they are direct
 * entity pointers. No argument, descriptor, task or reference is modified. */
static unsigned TRACE_HELPER story_arguments_begin(void *thread,void *arguments) {
    DWORD error=GetLastError();
    unsigned cookie=0;
    InterlockedIncrement(&callbacks);
    if(!thread_exact()) { fault("arguments_thread"); goto done; }
    Task *t=find_task(thread,FALSE);
    if(!t || t->hash!=HASH_LIGHTHOUSE_ENTRY) goto done;
    if(argument_depth>=MAX_DEPTH || t->generation!=status.load_generation ||
        t->manager!=load_manager || !manager_exact(t->manager) ||
        !readable(thread,0x2c) || !readable(arguments,0xc)) {
        fault("recruitment_arguments_identity"); goto done;
    }
    ArgumentObservation *a=&argument_scopes[argument_depth++]; cookie=argument_depth;
    memset(a,0,sizeof(*a)); a->thread=thread; a->task_id=t->id;
    a->used_before=*(uint32_t *)((uint8_t *)thread+0x28);
    uint32_t count=*(uint32_t *)arguments;
    const uint32_t *values=*(const uint32_t **)((uint8_t *)arguments+8);
    a->exact=count==2 && a->used_before<=UINT32_MAX-2u && readable(values,8);
    if(a->exact) memcpy(a->raw,values,sizeof(a->raw));
    else fault("recruitment_arguments_shape");
    t->recruitment=TRUE;
done:
    SetLastError(error); return cookie;
}
static void TRACE_HELPER story_arguments_end(unsigned cookie) {
    DWORD error=GetLastError();
    if(cookie) {
        if(!thread_exact() || cookie!=argument_depth) fault("arguments_return_scope");
        else {
            ArgumentObservation *a=&argument_scopes[--argument_depth];
            Task *t=find_task(a->thread,FALSE);
            BOOL exact=a->exact && t && t->id==a->task_id &&
                t->manager==load_manager && manager_exact(t->manager) && readable(a->thread,0x2c);
            const uint32_t *stack=exact?*(const uint32_t **)((uint8_t *)a->thread+0x20):NULL;
            exact=exact && *(uint32_t *)((uint8_t *)a->thread+0x28)==a->used_before+2u &&
                readable(stack,8) && stack[0]==a->raw[1] && stack[1]==a->raw[0];
            if(!exact) fault("recruitment_arguments_copy");
            if(log_allowed(TRUE)) SudekiMpLogFormat(
                "lan_story_task event=recruitment_arguments generation=%lu task=%lu root_hash=%08lx count=%u raw0=%08lx raw1=%08lx native_copy_exact=%u value_identity=unproven authority=none\r\n",
                (unsigned long)status.load_generation,(unsigned long)a->task_id,
                (unsigned long)HASH_LIGHTHOUSE_ENTRY,a->exact?2u:0u,
                (unsigned long)a->raw[0],(unsigned long)a->raw[1],exact);
            memset(a,0,sizeof(*a));
        }
    }
    InterlockedDecrement(&callbacks); SetLastError(error);
}

/* SUBMIT: descriptor EAX, seven stack arguments, native ret28. All bridge
 * scratch is local; nested native calls cannot overwrite another return. */
#define SUBMIT_BRIDGE(name,kind) \
__attribute__((naked,noinline,used)) static void name(void) { \
    __asm__ volatile( \
        "pushl %ebp\n\tmovl %esp,%ebp\n\tpushl %ebx\n\tpushl %esi\n\tpushl %edi\n\tsubl $28,%esp\n\t" \
        "movl %eax,-16(%ebp)\n\tmovl %ecx,-20(%ebp)\n\tmovl %edx,-24(%ebp)\n\tpushfl\n\tpopl -28(%ebp)\n\t" \
        "pushl %eax\n\tpushl 8(%ebp)\n\tpushl $" #kind "\n\tcall _story_submit_begin\n\taddl $12,%esp\n\tmovl %eax,-32(%ebp)\n\t" \
        "pushl -28(%ebp)\n\tpopfl\n\tmovl -24(%ebp),%edx\n\tmovl -20(%ebp),%ecx\n\tmovl -16(%ebp),%eax\n\t" \
        "pushl 32(%ebp)\n\tpushl 28(%ebp)\n\tpushl 24(%ebp)\n\tpushl 20(%ebp)\n\tpushl 16(%ebp)\n\tpushl 12(%ebp)\n\tpushl 8(%ebp)\n\tcall *_original_submit\n\t" \
        "movl %eax,-16(%ebp)\n\tmovl %ecx,-20(%ebp)\n\tmovl %edx,-24(%ebp)\n\tpushfl\n\tpopl -28(%ebp)\n\t" \
        "pushl -32(%ebp)\n\tcall _story_submit_end\n\taddl $4,%esp\n\t" \
        "pushl -28(%ebp)\n\tpopfl\n\tmovl -24(%ebp),%edx\n\tmovl -20(%ebp),%ecx\n\tmovl -16(%ebp),%eax\n\t" \
        "leal -12(%ebp),%esp\n\tpopl %edi\n\tpopl %esi\n\tpopl %ebx\n\tpopl %ebp\n\tret $28\n\t"); }
SUBMIT_BRIDGE(story_start_submit,1)
SUBMIT_BRIDGE(story_on_load_submit,2)

__attribute__((naked,noinline,used)) static void story_create(void) {
    __asm__ volatile(
        "pushl %ebp\n\tmovl %esp,%ebp\n\tpushl %ebx\n\tpushl %esi\n\tpushl %edi\n\tsubl $20,%esp\n\t"
        "movl %eax,-16(%ebp)\n\t"
        "pushfl\n\tpushal\n\tcall _story_create_begin\n\tpopal\n\tpopfl\n\t"
        "pushl 24(%ebp)\n\tpushl 20(%ebp)\n\tpushl 16(%ebp)\n\tpushl 12(%ebp)\n\tpushl 8(%ebp)\n\tcall *_original_create\n\t"
        "movl %eax,-20(%ebp)\n\tmovl %ecx,-24(%ebp)\n\tmovl %edx,-28(%ebp)\n\tpushfl\n\tpopl -32(%ebp)\n\t"
        "pushl 12(%ebp)\n\tpushl 8(%ebp)\n\tpushl -16(%ebp)\n\tcall _story_created\n\taddl $12,%esp\n\t"
        "pushl -32(%ebp)\n\tpopfl\n\tmovl -28(%ebp),%edx\n\tmovl -24(%ebp),%ecx\n\tmovl -20(%ebp),%eax\n\t"
        "leal -12(%ebp),%esp\n\tpopl %edi\n\tpopl %esi\n\tpopl %ebx\n\tpopl %ebp\n\tret $20\n\t");
}

/* RETIRE: ECX manager, ESI task, no stack arguments, native plain ret.
 * ADD: EAX group, one by-value actor pointer stack argument, ret4. */
#define OBSERVE_BRIDGE(name,before,after,arguments,call_args,native,tail) \
__attribute__((naked,noinline,used)) static void name(void) { \
    __asm__ volatile( \
        "pushl %ebp\n\tmovl %esp,%ebp\n\tpushl %ebx\n\tpushl %esi\n\tpushl %edi\n\tsubl $20,%esp\n\t" \
        "movl %eax,-16(%ebp)\n\tmovl %ecx,-20(%ebp)\n\tmovl %edx,-24(%ebp)\n\tpushfl\n\tpopl -28(%ebp)\n\t" \
        arguments "call _" #before "\n\taddl $8,%esp\n\tmovl %eax,-32(%ebp)\n\t" \
        "pushl -28(%ebp)\n\tpopfl\n\tmovl -24(%ebp),%edx\n\tmovl -20(%ebp),%ecx\n\tmovl -16(%ebp),%eax\n\t" \
        call_args "call *_" #native "\n\t" \
        "movl %eax,-16(%ebp)\n\tmovl %ecx,-20(%ebp)\n\tmovl %edx,-24(%ebp)\n\tpushfl\n\tpopl -28(%ebp)\n\t" \
        after \
        "pushl -28(%ebp)\n\tpopfl\n\tmovl -24(%ebp),%edx\n\tmovl -20(%ebp),%ecx\n\tmovl -16(%ebp),%eax\n\t" \
        "leal -12(%ebp),%esp\n\tpopl %edi\n\tpopl %esi\n\tpopl %ebx\n\tpopl %ebp\n\t" tail); }
OBSERVE_BRIDGE(story_retire,story_retire_begin,
    "pushl -8(%ebp)\n\tpushl -32(%ebp)\n\tcall _story_retire_end\n\taddl $8,%esp\n\t",
    "pushl %esi\n\tpushl %ecx\n\t","",original_retire,"ret\n\t")
OBSERVE_BRIDGE(story_add,story_add_begin,
    "pushl -32(%ebp)\n\tcall _story_add_end\n\taddl $4,%esp\n\t",
    "pushl 8(%ebp)\n\tpushl %eax\n\t","pushl 8(%ebp)\n\t",original_add,"ret $4\n\t")
OBSERVE_BRIDGE(story_arguments,story_arguments_begin,
    "pushl -32(%ebp)\n\tcall _story_arguments_end\n\taddl $4,%esp\n\t",
    "pushl 12(%ebp)\n\tpushl 8(%ebp)\n\t",
    "pushl 12(%ebp)\n\tpushl 8(%ebp)\n\t",original_arguments,"ret $8\n\t")

/* RR is cdecl2; constructor preserves ESI=job/EAX=placement and ret44;
 * completion is cdecl13. LEA discards copied cdecl arguments without changing
 * the original flags saved by OBSERVE_BRIDGE. No native helper is called twice. */
OBSERVE_BRIDGE(story_spawn_rr,spawn_rr_begin,
    "leal 8(%esp),%esp\n\tpushl -32(%ebp)\n\tcall _spawn_rr_end\n\taddl $4,%esp\n\t",
    "pushl 12(%ebp)\n\tpushl 8(%ebp)\n\t",
    "pushl 12(%ebp)\n\tpushl 8(%ebp)\n\t",original_spawn_rr,"ret\n\t")
OBSERVE_BRIDGE(story_spawn_ctor,spawn_ctor_begin,
    "pushl -16(%ebp)\n\tpushl -32(%ebp)\n\tcall _spawn_ctor_end\n\taddl $8,%esp\n\t",
    "pushl %eax\n\tpushl %esi\n\t",
    "pushl 48(%ebp)\n\tpushl 44(%ebp)\n\tpushl 40(%ebp)\n\tpushl 36(%ebp)\n\t"
    "pushl 32(%ebp)\n\tpushl 28(%ebp)\n\tpushl 24(%ebp)\n\tpushl 20(%ebp)\n\t"
    "pushl 16(%ebp)\n\tpushl 12(%ebp)\n\tpushl 8(%ebp)\n\t",original_spawn_ctor,"ret $44\n\t")
OBSERVE_BRIDGE(story_spawn_complete,spawn_complete_begin,
    "leal 52(%esp),%esp\n\tpushl -32(%ebp)\n\tcall _spawn_complete_end\n\taddl $4,%esp\n\t",
    "pushl 8(%ebp)\n\tleal -28(%esi),%eax\n\tpushl %eax\n\t",
    "pushl 56(%ebp)\n\tpushl 52(%ebp)\n\tpushl 48(%ebp)\n\tpushl 44(%ebp)\n\t"
    "pushl 40(%ebp)\n\tpushl 36(%ebp)\n\tpushl 32(%ebp)\n\tpushl 28(%ebp)\n\t"
    "pushl 24(%ebp)\n\tpushl 20(%ebp)\n\tpushl 16(%ebp)\n\tpushl 12(%ebp)\n\t"
    "pushl 8(%ebp)\n\t",original_spawn_complete,"ret\n\t")
/* EntitySetup's scalar deleting destructor: ECX=job, stack flags, ret4.
 * EAX is the old address even when flag1 frees storage. Preserve that return,
 * every volatile register/flag, and the original call's LastError. */
OBSERVE_BRIDGE(story_spawn_destroy,spawn_destroy_begin,
    "pushl -16(%ebp)\n\tpushl -32(%ebp)\n\tcall _spawn_destroy_end\n\taddl $8,%esp\n\t",
    "pushl 8(%ebp)\n\tpushl %ecx\n\t",
    "pushl 8(%ebp)\n\t",original_spawn_destroy,"ret $4\n\t")

BOOL SudekiMpLanStoryTaskTraceGetStatus(SudekiMpLanStoryTaskTraceStatus *out) {
    if(!out || !installed || !native_thread || native_thread!=GetCurrentThreadId() ||
        InterlockedCompareExchange(&callbacks,0,0)) return FALSE;
    *out=status;
    out->unknown=InterlockedCompareExchange(&trace_fault,0,0)!=0;
    return TRUE;
}
BOOL SudekiMpLanStoryTaskTraceGetRecruitmentStatus(SudekiMpLanStoryRecruitmentStatus *out) {
    if(!out || !installed || !native_thread || native_thread!=GetCurrentThreadId() ||
        InterlockedCompareExchange(&callbacks,0,0)) return FALSE;
    *out=recruitment_status;
    out->unknown=out->unknown || InterlockedCompareExchange(&trace_fault,0,0)!=0;
    return TRUE;
}
BOOL SudekiMpLanStoryTaskTraceGetSpawnObservation(SudekiMpLanStorySpawnObservation *out) {
    if(!out || !installed || !spawn_owner_thread() ||
        InterlockedCompareExchange(&callbacks,0,0)) return FALSE;
    *out=spawn_observation;
    SpawnJob *j=spawn_construction(out->construction);
    /* The recorded job owns its resource component until its observed native
     * destructor. Native 4B2600/4B2650 uses this exact +4 field and 43A490
     * getter. Never dereference a disposed job or call the virtual getter. */
    if(j && j->constructed && !j->destroy_entered && !out->job_destructor_returned &&
        readable(j->job,0x58u) && *(void **)j->job==image_base+SPAWN_VTABLE &&
        same_placement((uint8_t *)j->job+0x1cu,j->placement)) {
        SudekiMpLanStoryResourceObservation resource;
        uint8_t *component=*(uint8_t **)((uint8_t *)j->job+4u);
        if(component && readable(component,16u) && *(void **)component==image_base+0x2d55a0u &&
            *(void **)(image_base+0x2d55a0u)==image_base+0x3a490u &&
            copy_resource(component+4u,&resource) &&
            (resource.encoded_kind&0x1fffu)==0xf81u && resource.identifier==0x8557d453u &&
            same_resource(&resource,&j->job_name) && (uintptr_t)component>=0x2cu) {
            uint8_t *actor=component-0x2cu;
            uint8_t *registry=*(uint8_t **)(image_base+0x409d8cu);
            if(readable(actor,0x145u) && *(void **)actor==image_base+0x2d555cu &&
                readable(registry,0x40u)) {
                unsigned count=*(unsigned *)(registry+0x34u),matches=0;
                void **entities=*(void ***)(registry+0x3cu);
                if(count && count<=8192u && readable(entities,count*4u)) {
                    for(unsigned i=0;i<count;++i) matches+=entities[i]==actor;
                    if(matches==1u) {
                        out->pending_actor=actor; out->pending_pause=actor[0x2bu];
                        out->pending_stage=((uint8_t *)j->job)[0x54u];
                        out->pending_actor_exact=TRUE;
                    }
                }
            }
        }
    }
    out->unknown=out->unknown || InterlockedCompareExchange(&spawn_fault,0,0)!=0 ||
        InterlockedCompareExchange(&trace_fault,0,0)!=0;
    return TRUE;
}
static BOOL replay_idle(void) {
    return installed && image_base && spawn_owner_thread() &&
        SudekiMpLanStoryTaskTraceSpawnEntryExact((HMODULE)image_base) &&
        !InterlockedCompareExchange(&callbacks,0,0) && !current_thread &&
        !submit_kind && !submit_depth && !add_depth && !argument_depth &&
        !spawn_depth && !completion_depth && manager_exact(load_manager) &&
        status.load_generation && status.start_terminal && status.start_retired &&
        status.on_load_terminal && status.on_load_retired &&
        !InterlockedCompareExchange(&trace_fault,0,0) &&
        !InterlockedCompareExchange(&spawn_fault,0,0);
}
BOOL SudekiMpLanStoryTaskTraceSpawnEntryExact(HMODULE image) {
    static const uint8_t rr[]={0x83,0xec,0x3c,0x56,0x57};
    static const uint8_t ctor[]={0x8b,0x54,0x24,8,0x83,0xec,8};
    return installed && image_base==(uint8_t *)image && spawn_rr_hook.installed &&
        spawn_rr_hook.target==image_base+SPAWN_RR && spawn_rr_hook.length==sizeof(rr) &&
        original_spawn_rr==(RawFunction)spawn_rr_hook.trampoline &&
        readable(spawn_rr_hook.trampoline,sizeof(rr)+5u) &&
        !memcmp(spawn_rr_hook.trampoline,rr,sizeof(rr)) &&
        jump_target((uint8_t *)spawn_rr_hook.trampoline+sizeof(rr),image_base+SPAWN_RR+sizeof(rr)) &&
        jump_target(image_base+SPAWN_RR,story_spawn_rr) &&
        spawn_ctor_hook.installed && spawn_ctor_hook.target==image_base+SPAWN_CTOR &&
        spawn_ctor_hook.length==sizeof(ctor) && original_spawn_ctor==(RawFunction)spawn_ctor_hook.trampoline &&
        readable(spawn_ctor_hook.trampoline,sizeof(ctor)+5u) &&
        !memcmp(spawn_ctor_hook.trampoline,ctor,sizeof(ctor)) &&
        jump_target((uint8_t *)spawn_ctor_hook.trampoline+sizeof(ctor),image_base+SPAWN_CTOR+sizeof(ctor)) &&
        jump_target(image_base+SPAWN_CTOR,story_spawn_ctor) &&
        spawn_complete_hook.installed && original_spawn_complete==(RawFunction)(image_base+SPAWN_COMPLETE) &&
        call_target(image_base+SPAWN_COMPLETE_CALL,story_spawn_complete) &&
        spawn_destroy_hook.installed && spawn_destroy_hook.target==image_base+SPAWN_DESTROY &&
        spawn_destroy_hook.length==9u && original_spawn_destroy==(RawFunction)spawn_destroy_hook.trampoline &&
        readable(spawn_destroy_hook.trampoline,14u) &&
        !memcmp(spawn_destroy_hook.trampoline,spawn_destroy_hook.original,9u) &&
        jump_target((uint8_t *)spawn_destroy_hook.trampoline+9u,image_base+SPAWN_DESTROY+9u) &&
        jump_target(image_base+SPAWN_DESTROY,story_spawn_destroy);
}
BOOL SudekiMpLanStoryTaskTraceBeginSpawnReplay(uint32_t transaction,
    const void *actor_resource,const void *placement_resource) {
    SudekiMpLanStoryResourceObservation actor,placement;
    if(!transaction || !replay_idle() || spawn_replay.transaction || spawn_setup_transaction ||
        !copy_resource(actor_resource,&actor) || !copy_resource(placement_resource,&placement) ||
        !actor.text_known || !placement.text_known || actor.identifier!=0x8557d453u ||
        placement.identifier!=0x3d608dc8u || strcmp(actor.text,"PC_AILISH") ||
        strcmp(placement.text,"TSA_AILISH_LIGHTHOUSE")) {
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    spawn_replay.transaction=transaction;
    spawn_replay.created_before=*(uint32_t *)(image_base+TASK_CREATED_GLOBAL);
    spawn_replay.actor=actor_resource; spawn_replay.placement=placement_resource;
    SetLastError(ERROR_SUCCESS); return TRUE;
}
BOOL SudekiMpLanStoryTaskTraceEndSpawnReplay(uint32_t transaction) {
    /* Clear only the synchronous observation scope after exact RR return.
     * Native job/actor observations remain retained even on reported failure. */
    if(!installed || !spawn_owner_thread() || !transaction ||
        transaction!=spawn_replay.transaction || !spawn_replay.returned ||
        InterlockedCompareExchange(&callbacks,0,0) || current_thread || spawn_depth || completion_depth) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    BOOL exact=spawn_observation.replay_transaction==transaction &&
        spawn_observation.request==spawn_replay.request && spawn_observation.construction_exact &&
        spawn_observation.load_generation==status.load_generation &&
        !InterlockedCompareExchange(&trace_fault,0,0) && !InterlockedCompareExchange(&spawn_fault,0,0);
    memset(&spawn_replay,0,sizeof(spawn_replay));
    SetLastError(exact?ERROR_SUCCESS:ERROR_INVALID_DATA); return exact;
}
BOOL SudekiMpLanStoryTaskTraceBeginSpawnSetup(uint32_t transaction) {
    SpawnJob *j=spawn_construction(spawn_observation.construction);
    if(!transaction || !replay_idle() || spawn_replay.transaction || spawn_setup_transaction ||
        !j || j->observation.replay_transaction!=transaction ||
        j->observation.load_generation!=status.load_generation ||
        !j->observation.completion_returned || !j->observation.job_destructor_returned ||
        j->observation.unknown) {
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    spawn_setup_transaction=transaction; spawn_setup_construction=j->construction;
    spawn_setup_count=j->observation.setup_tasks_created;
    j->observation.setup_created_before=*(uint32_t *)(image_base+TASK_CREATED_GLOBAL);
    spawn_observation=j->observation;
    SetLastError(ERROR_SUCCESS); return TRUE;
}
BOOL SudekiMpLanStoryTaskTraceEndSpawnSetup(uint32_t transaction) {
    if(!installed || !spawn_owner_thread() || !transaction ||
        transaction!=spawn_setup_transaction || InterlockedCompareExchange(&callbacks,0,0) ||
        current_thread || spawn_depth || completion_depth) { SetLastError(ERROR_BUSY); return FALSE; }
    SpawnJob *j=spawn_construction(spawn_setup_construction);
    BOOL exact=j && j->observation.replay_transaction==transaction &&
        j->observation.load_generation==status.load_generation;
    if(exact) {
        j->observation.setup_created_after=*(uint32_t *)(image_base+TASK_CREATED_GLOBAL);
        exact=j->observation.setup_created_after-j->observation.setup_created_before==
            j->observation.setup_tasks_created-spawn_setup_count;
        if(!exact) spawn_unknown("setup_task_counter_coverage");
        spawn_observation=j->observation;
    } else spawn_unknown("setup_scope_return_identity");
    spawn_setup_transaction=0; spawn_setup_construction=0; spawn_setup_count=0;
    exact=exact && !InterlockedCompareExchange(&trace_fault,0,0) &&
        !InterlockedCompareExchange(&spawn_fault,0,0);
    SetLastError(exact?ERROR_SUCCESS:ERROR_INVALID_DATA); return exact;
}
BOOL SudekiMpLanStoryTaskTraceSpawnCounter(uint32_t transaction,
    uint32_t *accounted_created_tasks,BOOL *exact) {
    if(!transaction || !accounted_created_tasks || !exact || !installed ||
        !spawn_owner_thread() || InterlockedCompareExchange(&callbacks,0,0) ||
        !manager_exact(load_manager) || spawn_observation.replay_transaction!=transaction ||
        spawn_observation.load_generation!=status.load_generation) {
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    *accounted_created_tasks=spawn_observation.replay_created_before+
        spawn_observation.observed_task_count;
    *exact=!InterlockedCompareExchange(&trace_fault,0,0) &&
        !InterlockedCompareExchange(&spawn_fault,0,0) &&
        spawn_observation.observed_task_count==spawn_observation.completion_tasks_created+
            spawn_observation.setup_tasks_created &&
        *accounted_created_tasks==*(uint32_t *)(image_base+TASK_CREATED_GLOBAL);
    SetLastError(ERROR_SUCCESS); return TRUE;
}
BOOL SudekiMpLanStoryTaskTraceForgetExitedWorld(void) {
    /* Native Quit has already destroyed old actors/tasks. Its verified cached
     * return is required BEFORE discarding any borrowed address; never inspect
     * old task bodies or a possibly replaced manager here. A busy observer
     * retains everything for a later UI retry after its original returns. */
    if(cast_created || cast_step || admission_decide || !installed || !image_base || !native_thread ||
        native_thread!=GetCurrentThreadId() ||
        SudekiMpLobbyGameplayStoryExitStatus()!=1u ||
        InterlockedCompareExchange(&callbacks,0,0) || current_thread ||
        submit_kind || submit_depth || add_depth || argument_depth || spawn_depth || completion_depth ||
        spawn_replay.transaction || spawn_setup_transaction) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    if(!load_manager) { SetLastError(ERROR_SUCCESS); return TRUE; }
    uint32_t generation=status.load_generation;
    BOOL truncated=status.log_truncated;
    unsigned discarded=0;
    for(unsigned i=0;i<TASK_CAPACITY;++i) discarded+=tasks[i].occupied!=FALSE;
    /* Disarm before clearing the journal. Frontend tasks may reuse addresses
     * from the destroyed world; they do not belong to that saved generation.
     * The next exact StartLoadedGame scope installs its own manager again. */
    load_manager=NULL;
    memset(tasks,0,sizeof(tasks));
    memset(add_scopes,0,sizeof(add_scopes));
    memset(argument_scopes,0,sizeof(argument_scopes));
    memset(&recruitment_status,0,sizeof(recruitment_status));
    memset(spawn_jobs,0,sizeof(spawn_jobs));
    memset(spawn_scopes,0,sizeof(spawn_scopes));
    memset(completion_scopes,0,sizeof(completion_scopes));
    memset(&spawn_observation,0,sizeof(spawn_observation));
    memset(&status,0,sizeof(status));
    status.load_generation=generation; status.log_truncated=truncated;
    /* Preserve monotonic ids, generation, and all atomic uncertainty. A new
     * attempt alone is never evidence that an earlier observation was safe. */
    if(log_allowed(TRUE)) SudekiMpLogFormat(
        "lan_story_task event=world_journal_forgotten generation=%lu borrowed_tasks=%u native_world_exit_verified=1 task_terminal_claim=none faults_preserved=1 frontend_observation=disarmed\r\n",
        (unsigned long)generation,discarded);
    SetLastError(ERROR_SUCCESS); return TRUE;
}
BOOL SudekiMpLanStoryTaskTraceUninstall(void) {
    if(!image_base) return TRUE;
    if(cast_created || cast_step || admission_decide || admission_waiters ||
        InterlockedCompareExchange(&admission_fault,0,0) ||
        (native_thread && native_thread!=GetCurrentThreadId()) ||
        InterlockedCompareExchange(&callbacks,0,0) || spawn_replay.transaction ||
        spawn_setup_transaction) return retain(ERROR_BUSY);
    BOOL ok=SudekiMpRestoreInlineHook(&spawn_rr_hook);
    if(!SudekiMpRestoreInlineHook(&spawn_ctor_hook)) ok=FALSE;
    if(!SudekiMpRestoreRelativeCallHook(&spawn_complete_hook)) ok=FALSE;
    if(!SudekiMpRestoreInlineHook(&spawn_destroy_hook)) ok=FALSE;
    if(!SudekiMpRestoreRelativeCallHook(&argument_hook)) ok=FALSE;
    if(!SudekiMpRestoreRelativeCallHook(&add_hook)) ok=FALSE;
    if(!SudekiMpRestoreInlineHook(&retire_hook)) ok=FALSE;
    for(unsigned i=2;i>0;--i) {
        if(!SudekiMpRestoreRelativeCallHook(&step_hooks[i-1])) ok=FALSE;
        if(!SudekiMpRestoreRelativeCallHook(&create_hooks[i-1])) ok=FALSE;
        if(!SudekiMpRestoreRelativeCallHook(&submit_hooks[i-1])) ok=FALSE;
    }
    if(!ok || InterlockedCompareExchange(&callbacks,0,0)) return retain(GetLastError());
    /* Every observer is synchronous. No native object ever stores a mod
     * callback, borrowed diagnostic address, or additional task reference. */
    installed=FALSE; image_base=NULL; native_thread=0; current_thread=NULL; load_manager=NULL;
    original_submit=NULL; original_create=NULL; original_step=NULL; original_retire=NULL; original_add=NULL;
    original_arguments=NULL;
    original_spawn_rr=NULL; original_spawn_ctor=NULL; original_spawn_complete=NULL;
    original_spawn_destroy=NULL;
    reset_spawn_observation(); spawn_events=0; next_spawn_request=0; next_construction=0;
    memset(tasks,0,sizeof(tasks)); memset(add_scopes,0,sizeof(add_scopes));
    memset(argument_scopes,0,sizeof(argument_scopes));
    memset(&status,0,sizeof(status)); next_task_id=0; events=0; critical_events=0; truncated_quotas=0;
    memset(&recruitment_status,0,sizeof(recruitment_status));
    submit_kind=0; submit_depth=0; add_depth=0; argument_depth=0;
    InterlockedExchange(&trace_fault,0);
    return TRUE;
}
BOOL SudekiMpLanStoryTaskHostExact(HMODULE image) {
    return image_base==(uint8_t *)image && installed && native_thread &&
        native_thread==GetCurrentThreadId() &&
        create_hooks[0].installed && create_hooks[1].installed && step_hooks[0].installed && step_hooks[1].installed &&
        call_target(image_base+CREATE_DIRECT,story_create) && call_target(image_base+CREATE_CHILD,story_create) &&
        call_target(image_base+STEP_IMMEDIATE,story_step) && call_target(image_base+STEP_SCHEDULED,story_step);
}
BOOL SudekiMpLanStoryTaskHostAttach(HMODULE image,SudekiMpLanCastCreatedObserver created,
    SudekiMpLanCastStepAdapter step) {
    if(!created || !step || cast_created || cast_step || InterlockedCompareExchange(&callbacks,0,0) ||
        !SudekiMpLanStoryTaskHostExact(image)) { SetLastError(ERROR_INVALID_STATE); return FALSE; }
    cast_created=created; cast_step=step; return TRUE;
}
BOOL SudekiMpLanStoryTaskAdmissionAttach(HMODULE image,const void *consumer,
    SudekiMpStoryTaskDecide decide,SudekiMpStoryTaskRetired retired) {
    if(!consumer || !decide || !retired || admission_decide || admission_waiters || admission_depth ||
        InterlockedCompareExchange(&admission_fault,0,0) || InterlockedCompareExchange(&trace_fault,0,0) ||
        InterlockedCompareExchange(&callbacks,0,0) || !SudekiMpLanStoryTaskHostExact(image) ||
        !status.load_generation || !load_manager || !manager_exact(load_manager)) {
        SetLastError(ERROR_INVALID_STATE);return FALSE;
    }
    admission_consumer=consumer;admission_retired=retired;admission_decide=decide;return TRUE;
}
BOOL SudekiMpLanStoryTaskAdmissionDetach(HMODULE image,const void *consumer,
    SudekiMpStoryTaskDecide decide,SudekiMpStoryTaskRetired retired) {
    if(!consumer || !decide || !retired || consumer!=admission_consumer || decide!=admission_decide ||
        retired!=admission_retired || admission_waiters || admission_depth ||
        InterlockedCompareExchange(&admission_fault,0,0) || InterlockedCompareExchange(&trace_fault,0,0) ||
        InterlockedCompareExchange(&callbacks,0,0) || !SudekiMpLanStoryTaskHostExact(image)) {
        SetLastError(ERROR_BUSY);return FALSE;
    }
    admission_decide=NULL;admission_retired=NULL;admission_consumer=NULL;return TRUE;
}
BOOL SudekiMpLanStoryTaskHostDetach(SudekiMpLanCastCreatedObserver created,SudekiMpLanCastStepAdapter step) {
    if(!created || !step || cast_created!=created || cast_step!=step ||
        InterlockedCompareExchange(&callbacks,0,0) || !native_thread || native_thread!=GetCurrentThreadId()) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    cast_created=NULL; cast_step=NULL; return TRUE;
}
BOOL SudekiMpLanStoryTaskTraceInstall(HMODULE game) {
    uint8_t spawn_destroy_prefix[]={0x56,0x8b,0xf1,0xc7,0x06,0,0,0,0};
    static const uint8_t spawn_rr_prefix[]={0x83,0xec,0x3c,0x56,0x57};
    static const uint8_t spawn_ctor_prefix[]={0x8b,0x54,0x24,8,0x83,0xec,8};
    static const uint8_t spawn_ctor_tail[]={0x8b,0xc6,0x5b,0x83,0xc4,8,0xc2,0x2c,0};
    static const uint8_t spawn_completion_args[]={0x83,0xc6,0x1c,0x56,0x55};
    static const uint8_t spawn_pause_count[]={0x0f,0xb7,0x41,0x2a,0x0f,0xb6,0x4e,0x2b,
        0x40,0x3b,0xc8,0x0f,0x8d,0x8b,0,0,0};
    static const uint8_t submit_prefix[]={0x83,0xec,8,0x53,0x55,0x56,0x8b,0xf0,0x57};
    static const uint8_t create_prefix[]={0x83,0xec,8,0x53,0x8b,0x5c,0x24,0x10};
    static const uint8_t step_prefix[]={0xd9,0x41,0x48,0xd9,0x41,0x44,0xde,0xd9,
        0xdf,0xe0,0xf6,0xc4,5,0x7a,6,0xb8,2,0,0,0,0xc3};
    static const uint8_t retire_prefix[]={0x8b,0x91,0x88,0,1,0};
    static const uint8_t add_prefix[]={0x55,0x8b,0xec,0x83,0xe4,0xf8,0x8b,0x55,8};
    static const uint8_t argument_prefix[]={0x53,0x55,0x8b,0x6c,0x24,0x10,0x56,0x57,0x33,0xff};
    static const uint8_t argument_fields[]={0x8b,0x45,8,0x8b,0x74,0x24,0x14,
        0x83,0xc6,0x1c,0x8d,0x1c,0xb8,0x8b,0x46,8,0x39,0x46,0xc};
    static const uint8_t argument_copy[]={0xff,0x46,0xc,0x83,0x46,4,0xfc,
        0x8b,0x76,4,0x8b,0xb,0x89,0xe,0x47,0xeb,0xbf};
    uint8_t *b=(uint8_t *)game;
    const unsigned submit_sites[2]={LOAD_START,LOAD_ON_LOAD},create_sites[2]={CREATE_DIRECT,CREATE_CHILD},
        step_sites[2]={STEP_IMMEDIATE,STEP_SCHEDULED};
    const void *submit_replacements[2]={story_start_submit,story_on_load_submit};
    if(!game || image_base || installed || InterlockedCompareExchange(&callbacks,0,0) ||
        !SudekiMpCheckLoadedExecutable(game)) { SetLastError(ERROR_INVALID_STATE); return FALSE; }
    uint32_t destroy_vtable=(uint32_t)(uintptr_t)(b+SPAWN_VTABLE);
    memcpy(spawn_destroy_prefix+5,&destroy_vtable,4);
    if(memcmp(b+SUBMIT,submit_prefix,sizeof(submit_prefix)) ||
        !spawn_destroy_body_exact(b) ||
        *(void **)(b+SPAWN_VTABLE)!=b+SPAWN_DESTROY ||
        memcmp(b+SPAWN_RR,spawn_rr_prefix,sizeof(spawn_rr_prefix)) ||
        memcmp(b+SPAWN_CTOR,spawn_ctor_prefix,sizeof(spawn_ctor_prefix)) ||
        memcmp(b+0xb1282,spawn_ctor_tail,sizeof(spawn_ctor_tail)) ||
        memcmp(b+0xb27a5,spawn_completion_args,sizeof(spawn_completion_args)) ||
        !call_target(b+SPAWN_COMPLETE_CALL,b+SPAWN_COMPLETE) ||
        !call_target(b+0xb1e4a,b+0xb1900) ||
        !call_target(b+0xb1a78,b+SPAWN_CTOR) ||
        memcmp(b+0xb1e63,"\x5f\x5e\x83\xc4\x3c\xc3",6) ||
        memcmp(b+0xb27af,"\x83\xc4\x34",3) ||
        memcmp(b+SPAWN_COMPLETE,"\x51\x8b\x4c\x24\x08\x8b\x41\x2c",8) ||
        memcmp(b+0xb18bd,"\x5d\x5b\x59\xc3",4) ||
        memcmp(b+0xb1163,"\xc7\x06",2) || *(void **)(b+0xb1165)!=b+SPAWN_VTABLE ||
        memcmp(b+0xb1683,"\x8b\x0d",2) || *(void **)(b+0xb1685)!=b+SPEED_GLOBAL ||
        memcmp(b+0xb1689,spawn_pause_count,sizeof(spawn_pause_count)) ||
        memcmp(b+0xb17d0,"\xfe\x4e\x2b\x75\x19",5) ||
        memcmp(b+0x1b96a8,"\xc7\x03\x01\x00\x00\x00",6) ||
        memcmp(b+0x1b96db,"\x89\x43\x04\x5b\xc2\x04\x00",7) ||
        memcmp(b+CREATE,create_prefix,sizeof(create_prefix)) ||
        memcmp(b+STEP,step_prefix,sizeof(step_prefix)) ||
        memcmp(b+RETIRE,retire_prefix,sizeof(retire_prefix)) ||
        memcmp(b+ADD,add_prefix,sizeof(add_prefix)) ||
        memcmp(b+ARGUMENT_COPY,argument_prefix,sizeof(argument_prefix)) ||
        memcmp(b+0x1c5994,argument_fields,sizeof(argument_fields)) ||
        memcmp(b+0x1c59b0,argument_copy,sizeof(argument_copy)) ||
        memcmp(b+0x1c59c1,"\x5f\x5e\x5d\x5b\xc2\x08\x00",7) ||
        memcmp(b+0x1c3938,"\x53\x51",2) ||
        !call_target(b+ARGUMENT_CALL,b+ARGUMENT_COPY) ||
        memcmp(b+0x1c2ebe,"\x8d\x7e\x20",3) ||
        memcmp(b+0x1c2ef2,"\x89\x3d",2) ||
        *(uint32_t *)(b+0x1c2ef4)!=(uint32_t)(uintptr_t)(b+RUNTIME_GLOBAL) ||
        memcmp(b+0x1c38b2,"\xc2\x1c\x00",3) ||
        memcmp(b+0x1c3243,"\xc2\x14\x00",3) ||
        memcmp(b+0x23382,"\x5f\x5e\x5b\x8b\xe5\x5d\xc2\x04\x00",9) ||
        memcmp(b+0x1c396d,"\x85\xc0\x74\xf5\x48\x75\x58",7) ||
        memcmp(b+0x1c3394,"\x48\x74\x05\x48\x75\x19",6) ||
        memcmp(b+0x1c3ad5,"\xc7\x40\x08\x00\x00\x00\x00",7) ||
        !call_target(b+ADD_CALL,b+ADD) ||
        !call_target(b+0x1c3995,b+RETIRE) ||
        !call_target(b+0x1c33a4,b+RETIRE) ||
        !call_target(b+0x1c3afd,b+0x1c3cc0) ||
        !call_target(b+0x1c3b21,b+0x1c3cc0) ||
        !jump_target(b+0x1c3b04,b+0x1c5910) ||
        !jump_target(b+0x1c3b28,b+0x1c5910)) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    for(unsigned i=0;i<2;++i) if(!call_target(b+submit_sites[i],b+SUBMIT) ||
        !call_target(b+create_sites[i],b+CREATE) || !call_target(b+step_sites[i],b+STEP)) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    image_base=b; original_submit=(RawFunction)(b+SUBMIT); original_create=(RawFunction)(b+CREATE);
    original_step=(StepFunction)(b+STEP); original_add=(RawFunction)(b+ADD);
    original_arguments=(RawFunction)(b+ARGUMENT_COPY);
    original_spawn_complete=(RawFunction)(b+SPAWN_COMPLETE);
    /* Install retirement before creation: a partially installed observer can
     * never record a new task without the matching retirement seam present. */
    /* Installed only during suspended process startup, before native callbacks
     * are admitted. Publish a retained trampoline even if the patch helper
     * reports failure after allocating/patching part of its transaction. */
    BOOL retirement_installed=SudekiMpInstallInlineHook(&retire_hook,b+RETIRE,
        retire_prefix,sizeof(retire_prefix),story_retire);
    original_retire=(RawFunction)retire_hook.trampoline;
    if(!retirement_installed) goto failed;
    for(unsigned i=0;i<2;++i) {
        if(!SudekiMpInstallRelativeCallHook(&step_hooks[i],b+step_sites[i],b+STEP,story_step) ||
            !SudekiMpInstallRelativeCallHook(&create_hooks[i],b+create_sites[i],b+CREATE,story_create) ||
            !SudekiMpInstallRelativeCallHook(&submit_hooks[i],b+submit_sites[i],b+SUBMIT,submit_replacements[i])) goto failed;
    }
    if(!SudekiMpInstallRelativeCallHook(&add_hook,b+ADD_CALL,b+ADD,story_add) ||
        !SudekiMpInstallRelativeCallHook(&argument_hook,b+ARGUMENT_CALL,b+ARGUMENT_COPY,story_arguments)) goto failed;
    /* Publish destruction/completion before construction/request, so a
     * partial startup cannot record a new job without both lifetime seams. */
    BOOL destroy_installed=SudekiMpInstallInlineHook(&spawn_destroy_hook,b+SPAWN_DESTROY,
        spawn_destroy_prefix,sizeof(spawn_destroy_prefix),story_spawn_destroy);
    original_spawn_destroy=(RawFunction)spawn_destroy_hook.trampoline;
    if(!destroy_installed) goto failed;
    if(!SudekiMpInstallRelativeCallHook(&spawn_complete_hook,b+SPAWN_COMPLETE_CALL,
            b+SPAWN_COMPLETE,story_spawn_complete)) goto failed;
    BOOL ctor_installed=SudekiMpInstallInlineHook(&spawn_ctor_hook,b+SPAWN_CTOR,
        spawn_ctor_prefix,sizeof(spawn_ctor_prefix),story_spawn_ctor);
    original_spawn_ctor=(RawFunction)spawn_ctor_hook.trampoline;
    if(!ctor_installed) goto failed;
    BOOL rr_installed=SudekiMpInstallInlineHook(&spawn_rr_hook,b+SPAWN_RR,
        spawn_rr_prefix,sizeof(spawn_rr_prefix),story_spawn_rr);
    original_spawn_rr=(RawFunction)spawn_rr_hook.trampoline;
    if(!rr_installed) goto failed;
    installed=TRUE;
    SudekiMpLogWrite("lan_story_task event=install profile=saved_load_observe authority=none task_refs=unchanged\r\n");
    return TRUE;
failed: {
    DWORD error=GetLastError();
    if(!SudekiMpLanStoryTaskTraceUninstall()) return FALSE;
    SetLastError(error); return FALSE;
    }
}
