#include "hooks/lan_arena_cast_context.h"
#include "hooks/call_hook.h"
#include "engine/log.h"
#include <limits.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Cast context requires the verified x86 native bridges"
#endif

enum { ROOT_SKILL=0xb49f2, ROOT_SPIRIT=0x10de5, SKILL_STARTED=0xb4b0a, SUBMIT=0xfcfd0,
    CREATE_DIRECT=0x1c38f2, CREATE_CHILD=0x1c4db8, CREATE=0x1c3170,
    SKILL_CAMERA_START_CALL=0xb4b63, SKILL_CAMERA_END_CALL=0xb47ee,
    SKILL_CAMERA_START=0xb5330, SKILL_CAMERA_END=0xb5450,
    SKILL_UPDATE=0xb47a0, SKILL_UPDATE_VTABLE=0x2cbadc,
    STEP_IMMEDIATE=0x1c3968, STEP_SCHEDULED=0x1c338f, STEP=0x1c41d0,
    OPCODE_SLOT=0x323fa0, MAX_CASTS=4, MAX_TASKS=128 };
static const uint32_t opcode_rvas[3]={0x1c4970,0x1c4b10,0x1c4d30};
static const uint8_t submit_prefix[]={0x83,0xec,0x08,0x8b,0xcf,0xe8,0x06,0x03,0,0};
static const uint8_t create_prefix[]={0x83,0xec,0x08,0x53};
/* ECX thread, no stack arguments. Native yield=2 precedes opcode fetch and
 * thread+0c increment; the opcode-table hooks are too late to safely yield. */
static const uint8_t step_prefix[]={0xd9,0x41,0x48,0xd9,0x41,0x44,0xde,0xd9,
    0xdf,0xe0,0xf6,0xc4,5,0x7a,6,0xb8,2,0,0,0,0xc3};
/* CSkill's UpdateMgr subobject is this+18 (constructor 4b455f).
 * ECX=node, one float stack argument, ret 4, even though it ignores delta. */
static const uint8_t skill_update_prefix[]={0x55,0x8b,0xec,0x83,0xe4,0xf8,
    0x51,0x56,0x8b,0xf1,0x80,0x7e,0x54,0};
static const uint8_t skill_update_tail[]={0x5e,0x8b,0xe5,0x5d,0xc2,4,0};
/* CSkill::Use retains EBX=this, loads EDI=this->actor->script, ESI=out-cell.
 * This auxiliary submission precedes the native active-bit write. */
static const uint8_t skill_started_prefix[]={0x8b,0x43,0x10,0x8b,0x78,0x68,
    0x8d,0x54,0x24,0x48,0x52,0x8d,0x74,0x24,0x14};
typedef void (*RawFunction)(void);
typedef int (__attribute__((fastcall)) *OpcodeFunction)(void *, void *);
typedef void (__attribute__((stdcall)) *SkillCameraFunction)(void *);
typedef void (__attribute__((thiscall)) *SkillUpdateFunction)(void *,float);
typedef struct Cast {
    SudekiMpLanCastOwner owner;
    void *skill;
    uint32_t tasks, dispatches[3], roots, empty_launches;
    BOOL launching, skill_active_seen, skill_cleaned;
} Cast;
typedef struct Task {
    uint32_t *handle; /* One additional native pool-node reference. */
    void *thread;
    uint32_t cast_id;
    uint32_t function_hash;
    unsigned int cast_index;
} Task;
static uint8_t *cast_image;
static SudekiMpLanCastOwnerWitness owner_witness;
static RawFunction original_submit __attribute__((used));
static RawFunction original_create __attribute__((used));
static OpcodeFunction original_opcodes[3];
static OpcodeFunction original_step;
static SkillCameraFunction original_skill_camera[2];
static SkillUpdateFunction original_skill_update;
static SudekiMpPointerHook skill_update_hook;
static SudekiMpRelativeCallHook root_hooks[2], create_hooks[2];
static SudekiMpRelativeCallHook skill_started_hook;
static SudekiMpRelativeCallHook skill_camera_hooks[2];
static SudekiMpRelativeCallHook step_hooks[2];
static SudekiMpPointerHook opcode_hooks[3];
static Cast casts[MAX_CASTS];
static Task tasks[MAX_TASKS];
static Cast *current_cast;
static uint32_t next_cast_id;
static DWORD game_thread;
static unsigned int depth;
static BOOL lineage_fault;
static struct { Cast *previous, *root; unsigned int prior_depth; } root_scopes[16];
static unsigned int root_scope_count;
static SudekiMpLanCastTaskEnter task_enter;
static SudekiMpLanCastTaskLeave task_leave;
static struct { Cast *previous; unsigned int prior_depth; uint32_t cookie; } task_scopes[16];
static unsigned int task_scope_count;
static BOOL task_route_fault;
static SudekiMpInlineHook ui_trace_hook;
static void *ui_trace_trampoline __attribute__((used));
static unsigned int ui_trace_events;
static const uint8_t ui_recompute_prefix[]={0x55,0x8b,0xec,0x83,0xe4,0xf8};

static BOOL memory_access(const void *p,size_t n,BOOL write) {
    MEMORY_BASIC_INFORMATION m;
    uintptr_t a=(uintptr_t)p;
    if(!p || !n || !VirtualQuery(p,&m,sizeof(m)) || m.State!=MEM_COMMIT ||
        (m.Protect&(PAGE_NOACCESS|PAGE_GUARD)) || a+n<a ||
        a+n>(uintptr_t)m.BaseAddress+m.RegionSize) return FALSE;
    return !write || (m.Protect&(PAGE_READWRITE|PAGE_WRITECOPY|
        PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));
}
static void fault(const char *why) {
    if(!lineage_fault) SudekiMpLogFormat(
        "lan_cast_context event=unknown reason=%s policy=no_owner_inference\r\n",why);
    lineage_fault=TRUE;
}
static BOOL actor_components_exact(const Cast *c) {
    uint8_t *actor, *script;
    if(!c || !c->owner.cast_id ||
        GetCurrentThreadId()!=game_thread) return FALSE;
    actor=c->owner.actor; script=c->owner.script_component;
    return memory_access(actor,0x6c,FALSE) &&
        *(void **)(actor+0x68)==script && memory_access(script,0x14,FALSE) &&
        *(void **)(script+0x10)==actor;
}
static BOOL owner_exact(const Cast *c) {
    uint64_t session=0;
    uint8_t type=0;
    return actor_components_exact(c) && owner_witness &&
        owner_witness(c->owner.actor,c->owner.kind,&session,&type) &&
        session==c->owner.session && type==c->owner.actor_type;
}

static BOOL skill_exact(const Cast *c) {
    const uint8_t *actor=c->owner.actor, *skill=c->skill;
    return c->owner.kind==1 && actor_components_exact(c) && memory_access(actor,0xdc,FALSE) &&
        *(void **)(actor+0xd8)==skill && memory_access(skill,0x78,FALSE) &&
        *(void **)(skill+0x10)==actor && skill[0x6c]<=1;
}

static BOOL retire_completed_task(Task *t) {
    Cast *c;
    if(!t->handle) return TRUE;
    if(!memory_access(t->handle,8,TRUE) || !t->handle[1] ||
        (t->handle[0] && (void *)(uintptr_t)t->handle[0]!=t->thread)) {
        fault("task_reference_replaced"); return FALSE;
    }
    if(t->handle[0]) return TRUE; /* Still live; address reuse is NOT completion. */
    if(t->cast_index>=MAX_CASTS ||
        (c=&casts[t->cast_index])->owner.cast_id!=t->cast_id || !c->tasks) {
        fault("task_generation_mismatch"); return FALSE;
    }
    --t->handle[1]; /* Release only our pinned cell; native pool owns deletion. */
    --c->tasks;
    memset(t,0,sizeof(*t));
    return TRUE;
}

BOOL SudekiMpLanCastContextPoll(void) {
    unsigned int i;
    if(!cast_image) return TRUE;
    if(!game_thread) game_thread=GetCurrentThreadId();
    if(game_thread!=GetCurrentThreadId() || depth) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    for(i=0;i<MAX_TASKS;++i) {
        Task *t=&tasks[i];
        if(!t->handle) continue;
        /* Constructor-proven pool cell is pinned by our reference. Null is
         * native completion; an inaccessible/replaced cell is never success. */
        (void)retire_completed_task(t);
    }
    for(i=0;i<MAX_CASTS;++i) {
        Cast *c=&casts[i];
        if(c->owner.cast_id && c->owner.kind==1 && !c->skill_cleaned) {
            /* The root VM may finish before CSkill's native update launches
             * its separate cleanup-camera script. Keep the same cast identity
             * through that update; task completion alone is not actor cleanup. */
            if(skill_exact(c)) {
                if(((uint8_t *)c->skill)[0x6c]) c->skill_active_seen=TRUE;
                else if(c->skill_active_seen) c->skill_cleaned=TRUE;
            }
        }
        if(c->owner.cast_id && !c->launching && !c->tasks &&
            (c->owner.kind!=1 || c->skill_cleaned)) {
            SudekiMpLogFormat("lan_cast_context event=drained cast=%lu actor=%u kind=%u "
                "roots=%lu globals=%lu methods=%lu children=%lu empty=%lu\r\n",
                (unsigned long)c->owner.cast_id,c->owner.actor_type,c->owner.kind,
                (unsigned long)c->roots,(unsigned long)c->dispatches[0],
                (unsigned long)c->dispatches[1],(unsigned long)c->dispatches[2],
                (unsigned long)c->empty_launches);
            memset(c,0,sizeof(*c));
        }
    }
    return !lineage_fault;
}

BOOL SudekiMpLanCastContextCurrent(SudekiMpLanCastOwner *owner) {
    if(!owner || lineage_fault || !owner_exact(current_cast)) return FALSE;
    *owner=current_cast->owner;
    return TRUE;
}
BOOL SudekiMpLanCastContextCurrentRetained(SudekiMpLanCastOwner *owner) {
    if(!owner || !depth || lineage_fault || !actor_components_exact(current_cast)) return FALSE;
    *owner=current_cast->owner;
    return TRUE;
}

static void __attribute__((noinline,used)) observe_ui_recompute(void *ui,void *caller) {
    DWORD error=GetLastError();
    SudekiMpLanCastOwner owner={0};
    uint8_t *u=ui;
    BOOL owned;
    if(!cast_image || GetCurrentThreadId()!=game_thread || ui_trace_events>=128 ||
        !memory_access(u,0xc0,FALSE) || *(void **)u!=cast_image+0x2caf9c ||
        !memory_access(cast_image+0x3c2f88,4,FALSE) ||
        *(void **)(cast_image+0x3c2f88)!=ui) goto done;
    owned=SudekiMpLanCastContextCurrentRetained(&owner);
    ++ui_trace_events;
    SudekiMpLogFormat("lan_cast_ui event=recompute n=%u caller=%08lx count=%ld "
        "current=%lu requested=%lu attributed=%u actor=%u cast=%lu depth=%u\r\n",
        ui_trace_events,(unsigned long)((uintptr_t)caller-(uintptr_t)cast_image),
        *(long *)(u+0x54),(unsigned long)*(uint32_t *)(u+0xb8),
        (unsigned long)*(uint32_t *)(u+0xbc),owned,owner.actor_type,
        (unsigned long)owner.cast_id,depth);
done:
    SetLastError(error);
}
/* Native ESI=this; the first six bytes have no relative operands. Save ALL
 * GPRs/flags and x87/SSE state before logging, then replay the exact prologue.
 * This observer never calls a native UI method or adjusts the UI counter. */
static void __attribute__((naked,noinline)) ui_recompute_trace_bridge(void) {
    __asm__ volatile("pushfl\n\tpushal\n\tmovl %esp,%ebp\n\tsubl $528,%esp\n\t"
        "andl $-16,%esp\n\tfxsave (%esp)\n\tpushl 36(%ebp)\n\tpushl %esi\n\t"
        "call _observe_ui_recompute\n\taddl $8,%esp\n\tfxrstor (%esp)\n\t"
        "movl %ebp,%esp\n\tpopal\n\tpopfl\n\tjmp *_ui_trace_trampoline\n\t");
}
BOOL SudekiMpLanCastContextEnableUiTrace(void) {
    if(!cast_image || !game_thread || GetCurrentThreadId()!=game_thread ||
        ui_trace_hook.installed || !SudekiMpLanCastContextDrained()) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    if(!SudekiMpInstallInlineHook(&ui_trace_hook,cast_image+0x9e560,
        ui_recompute_prefix,sizeof(ui_recompute_prefix),ui_recompute_trace_bridge)) return FALSE;
    ui_trace_trampoline=ui_trace_hook.trampoline;
    ui_trace_events=0;
    return TRUE;
}

BOOL SudekiMpLanCastContextDrained(void) {
    unsigned int i;
    if(!cast_image) return TRUE;
    /* Draining is independent of admission. Even a faulted registry may
     * release its positively completed references, but never a live child. */
    if(depth || task_scope_count || (game_thread && GetCurrentThreadId()!=game_thread)) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    (void)SudekiMpLanCastContextPoll();
    for(i=0;i<MAX_TASKS;++i) if(tasks[i].handle) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    for(i=0;i<MAX_CASTS;++i) if(casts[i].owner.cast_id) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    return TRUE;
}

BOOL SudekiMpLanCastContextActorDrained(void *actor,uint64_t session) {
    unsigned int i;
    if(!cast_image || !game_thread || GetCurrentThreadId()!=game_thread ||
        !actor || !session || depth || task_scope_count || lineage_fault || task_route_fault) return FALSE;
    for(i=0;i<MAX_CASTS;++i)
        if(casts[i].owner.cast_id && casts[i].owner.actor==actor) return FALSE;
    /* Scan retained task cells too: a damaged root record must not turn an
     * outstanding native handle into apparent completion. */
    for(i=0;i<MAX_TASKS;++i) if(tasks[i].handle) {
        if(tasks[i].cast_index>=MAX_CASTS ||
            casts[tasks[i].cast_index].owner.cast_id!=tasks[i].cast_id) return FALSE;
        if(casts[tasks[i].cast_index].owner.actor==actor) return FALSE;
    }
    return TRUE;
}

BOOL SudekiMpLanCastContextSetTaskRouting(SudekiMpLanCastTaskEnter enter,
    SudekiMpLanCastTaskLeave leave) {
    if(!cast_image || !game_thread || game_thread!=GetCurrentThreadId() ||
        (!enter != !leave) || lineage_fault || task_route_fault || !SudekiMpLanCastContextDrained()) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    task_enter=enter; task_leave=leave;
    return TRUE;
}

/* Both root callsites pass the caster's script component in EDI. Only the
 * CSkill power-up submission and Spirit power-up submission establish roots;
 * Auxiliary scripts may inherit this identity, but never establish a root. */
static unsigned int __attribute__((noinline,used))
begin_root(void *script_component,unsigned int kind) {
    unsigned int i, cookie;
    uint8_t *script=script_component, *actor;
    uint64_t session=0;
    uint8_t type=0;
    void *skill=NULL;
    if(!cast_image || !game_thread || game_thread!=GetCurrentThreadId()) return 0;
    if(root_scope_count==16) { fault("root_scope_capacity"); return 0; }
    if(!depth) (void)SudekiMpLanCastContextPoll();
    cookie=++root_scope_count;
    root_scopes[cookie-1].previous=current_cast;
    root_scopes[cookie-1].root=NULL;
    root_scopes[cookie-1].prior_depth=depth;
    current_cast=NULL;
    ++depth;
    if(lineage_fault || root_scopes[cookie-1].prior_depth || !owner_witness ||
        !memory_access(script,0x14,FALSE)) return cookie;
    actor=*(uint8_t **)(script+0x10);
    if(!memory_access(actor,0x6c,FALSE) || *(void **)(actor+0x68)!=script ||
        !owner_witness(actor,(uint8_t)kind,&session,&type) || !session) return cookie;
    if(kind==1) {
        if(!memory_access(actor,0xdc,FALSE)) return cookie;
        skill=*(void **)(actor+0xd8);
        if(!memory_access(skill,0x78,FALSE) || *(void **)((uint8_t *)skill+0x10)!=actor)
            return cookie;
    }
    if(next_cast_id==UINT32_MAX) { fault("cast_generation_exhausted"); return cookie; }
    for(i=0;i<MAX_CASTS;++i) if(casts[i].owner.cast_id &&
        casts[i].owner.actor==actor) { fault("same_actor_root_overlap"); return cookie; }
    for(i=0;i<MAX_CASTS;++i) if(!casts[i].owner.cast_id) break;
    if(i==MAX_CASTS) { fault("cast_capacity"); return cookie; }
    casts[i].owner=(SudekiMpLanCastOwner){session,actor,script,++next_cast_id,type,(uint8_t)kind};
    casts[i].skill=skill;
    casts[i].launching=TRUE;
    current_cast=&casts[i];
    root_scopes[cookie-1].root=current_cast;
    SudekiMpLogFormat("lan_cast_context event=begin cast=%lu actor=%u kind=%u\r\n",
        (unsigned long)next_cast_id,type,kind);
    return cookie;
}
/* OnSkillStarted is launched separately from the power-up task. Attribute it
 * only through the exact CSkill::Use callsite and the already retained root;
 * never infer a caster from a name/hash or another actor's current scope. */
static unsigned int __attribute__((noinline,used))
begin_skill_started(void *script,void *skill) {
    unsigned int i,cookie;
    if(!cast_image || !game_thread || game_thread!=GetCurrentThreadId()) return 0;
    if(root_scope_count==16) { fault("root_scope_capacity"); return 0; }
    cookie=++root_scope_count;
    root_scopes[cookie-1].previous=current_cast;
    root_scopes[cookie-1].root=NULL;
    root_scopes[cookie-1].prior_depth=depth;
    current_cast=NULL; ++depth;
    if(lineage_fault || root_scopes[cookie-1].prior_depth) return cookie;
    for(i=0;i<MAX_CASTS;++i) {
        Cast *c=&casts[i];
        if(!c->owner.cast_id || c->skill!=skill) continue;
        if(c->owner.script_component!=script || !skill_exact(c) || !owner_exact(c) ||
            c->launching || c->skill_cleaned || ((uint8_t *)skill)[0x6c]) {
            fault("skill_started_owner_mismatch"); return cookie;
        }
        current_cast=c; root_scopes[cookie-1].root=c;
        SudekiMpLogFormat("lan_cast_context event=skill_started cast=%lu actor=%u\r\n",
            (unsigned long)c->owner.cast_id,c->owner.actor_type);
        break;
    }
    return cookie;
}
static void __attribute__((noinline,used)) end_root(unsigned int cookie) {
    if(!cookie) return;
    if(cookie!=root_scope_count || depth!=root_scopes[cookie-1].prior_depth+1 ||
        current_cast!=root_scopes[cookie-1].root) {
        fault("root_scope_mismatch"); return;
    }
    if(current_cast) current_cast->launching=FALSE;
    current_cast=root_scopes[cookie-1].previous;
    memset(&root_scopes[cookie-1],0,sizeof(root_scopes[0]));
    --root_scope_count;
    --depth;
}

static void __attribute__((noinline,used))
created_task(uint32_t function_hash,void **out_cell) {
    uint32_t *handle;
    void *thread;
    unsigned int i, free_slot=MAX_TASKS;
    Cast *c=current_cast;
    /* A child created by an already authenticated task must retain that task's
     * lineage even during disconnect cleanup. New roots still require fresh
     * authority in begin_root; this does not admit a new player action. */
    if(!c || !depth || lineage_fault || !actor_components_exact(c)) return;
    if(!memory_access(out_cell,4,FALSE)) { fault("constructor_out_cell_unknown"); return; }
    handle=*out_cell;
    /* 5c3170 explicitly writes NULL when function lookup or thread allocation
     * fails. There is no task to attribute in that case; do not invent either
     * a child or completion of the parent. A non-null malformed result remains
     * a fault. Observe the constructor result before immediate execution. */
    if(!handle) {
        ++c->empty_launches;
        SudekiMpLogFormat("lan_cast_context event=no_task cast=%lu actor=%u kind=%u function=%08lx\r\n",
            (unsigned long)c->owner.cast_id,c->owner.actor_type,c->owner.kind,(unsigned long)function_hash);
        return;
    }
    if(!memory_access(handle,8,TRUE) || !handle[0] || !handle[1] ||
        handle[1]>=INT_MAX) { fault("constructor_result_unknown"); return; }
    thread=(void *)(uintptr_t)handle[0];
    if(!memory_access(thread,0x50,FALSE)) { fault("thread_unknown"); return; }
    for(i=0;i<MAX_TASKS;++i) {
        if(!tasks[i].handle) { if(free_slot==MAX_TASKS) free_slot=i; continue; }
        if(tasks[i].handle==handle || tasks[i].thread==thread) {
            /* The native thread allocation can be reused before the next
             * controller Poll, while our OLD pool cell remains pinned with
             * a null pointee. Only that positive terminal state permits
             * retirement here. A same/live/replaced handle still faults. */
            if(!retire_completed_task(&tasks[i])) return;
            if(tasks[i].handle) { fault("constructor_identity_reused"); return; }
            if(free_slot==MAX_TASKS) free_slot=i;
        }
    }
    if(free_slot==MAX_TASKS) { fault("task_capacity"); return; }
    ++handle[1];
    tasks[free_slot]=(Task){handle,thread,c->owner.cast_id,function_hash,(unsigned int)(c-casts)};
    ++c->tasks;
    if(c->launching) ++c->roots;
    SudekiMpLogFormat("lan_cast_context event=task cast=%lu actor=%u kind=%u function=%08lx\r\n",
        (unsigned long)c->owner.cast_id,c->owner.actor_type,c->owner.kind,(unsigned long)function_hash);
}

static Cast *task_owner(void *thread,BOOL *known,BOOL require_authority) {
    unsigned int i;
    *known=FALSE;
    for(i=0;i<MAX_TASKS;++i) if(tasks[i].thread==thread && tasks[i].handle) {
        Task *t=&tasks[i];
        *known=TRUE;
        if(!lineage_fault && memory_access(t->handle,8,FALSE) &&
            (void *)(uintptr_t)t->handle[0]==thread && t->handle[1] &&
            t->cast_index<MAX_CASTS && casts[t->cast_index].owner.cast_id==t->cast_id &&
            (require_authority ? owner_exact(&casts[t->cast_index]):
                actor_components_exact(&casts[t->cast_index]))) return &casts[t->cast_index];
        break;
    }
    return NULL;
}

static int __attribute__((fastcall)) task_step(void *thread,void *edx) {
    Cast *previous=current_cast, *owner;
    unsigned int n=task_scope_count;
    uint32_t cookie;
    BOOL known;
    int result;
    DWORD error=GetLastError(), result_error;
    /* Disabled in ordinary builds until a runtime owner installs the pair.
     * No native context is silently inferred from the last active character. */
    if(!task_enter) return original_step(thread,edx);
    if(task_route_fault || !game_thread || GetCurrentThreadId()!=game_thread || n==16) {
        SetLastError(ERROR_BUSY); return 2;
    }
    /* Previously authenticated tasks keep their lifetime context after peer
     * loss so native cleanup can drain. This grants no new action admission;
     * the router still validates the retained cast/session identity. */
    owner=task_owner(thread,&known,FALSE);
    if(known && !owner) {
        /* A stale retained task is NOT an unrelated task. Do not send it to
         * the primary manager or advance it under a replacement session. */
        SetLastError(ERROR_INVALID_DATA); return 2;
    }
    current_cast=owner;
    cookie=task_enter(owner ? &owner->owner:NULL);
    if(!cookie) { current_cast=previous; return 2; }
    task_scopes[n].previous=previous;
    task_scopes[n].prior_depth=depth;
    task_scopes[n].cookie=cookie;
    ++task_scope_count; ++depth;
    SetLastError(error);
    result=original_step(thread,edx);
    result_error=GetLastError();
    if(task_scope_count!=n+1 || depth!=task_scopes[n].prior_depth+1 ||
        !task_leave(cookie)) {
        task_route_fault=TRUE;
        fault("task_context_restore_retained");
        /* The original opcode already executed. Returning a made-up yield or
         * rewinding here could duplicate a side effect. Retain dependencies
         * and let the native caller consume its real result. */
        SetLastError(ERROR_INVALID_DATA); return result;
    }
    --task_scope_count; --depth;
    current_cast=previous;
    memset(&task_scopes[n],0,sizeof(task_scopes[n]));
    SetLastError(result_error);
    return result;
}

static int dispatch(void *thread,void *edx,unsigned int opcode) {
    Cast *previous=current_cast, *owner=NULL;
    int result;
    DWORD error=GetLastError(), result_error;
    BOOL known;
    if(!game_thread || game_thread!=GetCurrentThreadId())
        return original_opcodes[opcode](thread,edx);
    /* A nested unrelated task must not inherit the enclosing native binding's
     * owner. Match its own retained constructor result, never just current_cast. */
    if(!lineage_fault) owner=task_owner(thread,&known,FALSE);
    current_cast=owner;
    ++depth;
    if(owner) ++owner->dispatches[opcode];
    SetLastError(error);
    result=original_opcodes[opcode](thread,edx);
    result_error=GetLastError();
    --depth;
    current_cast=previous;
    SetLastError(result_error);
    return result;
}
static int __attribute__((fastcall)) opcode27(void *t,void *d) { return dispatch(t,d,0); }
static int __attribute__((fastcall)) opcode28(void *t,void *d) { return dispatch(t,d,1); }
static int __attribute__((fastcall)) opcode29(void *t,void *d) { return dispatch(t,d,2); }

static void __attribute__((thiscall)) skill_update(void *node,float delta) {
    Cast *previous=current_cast,*owner=NULL;
    uint8_t *skill;
    unsigned int i,n=task_scope_count;
    uint32_t cookie;
    DWORD error=GetLastError(),result_error;
    if(!task_enter) { original_skill_update(node,delta); return; }
    if(task_route_fault || !game_thread || GetCurrentThreadId()!=game_thread ||
        n==16 || (uintptr_t)node<0x18) return;
    skill=(uint8_t *)node-0x18;
    if(!memory_access(skill,0x78,FALSE) ||
        *(void **)node!=cast_image+SKILL_UPDATE_VTABLE) return;
    for(i=0;i<MAX_CASTS;++i) {
        Cast *c=&casts[i];
        if(c->owner.cast_id && c->skill==skill) {
            if(!skill_exact(c)) return; /* Replaced is not neutral. */
            owner=c; break;
        }
    }
    /* An untracked active CSkill cannot clear another actor's busy bank or
     * release its menu state. An idle unrelated node still ticks normally. */
    if(!owner && skill[0x6c]) { fault("skill_update_owner_unknown"); return; }
    current_cast=owner;
    cookie=task_enter(owner ? &owner->owner:NULL);
    if(!cookie) { current_cast=previous; return; }
    task_scopes[n].previous=previous;
    task_scopes[n].prior_depth=depth;
    task_scopes[n].cookie=cookie;
    ++task_scope_count; ++depth;
    SetLastError(error);
    original_skill_update(node,delta);
    result_error=GetLastError();
    if(task_scope_count!=n+1 || depth!=task_scopes[n].prior_depth+1 || !task_leave(cookie)) {
        task_route_fault=TRUE;
        fault("skill_update_restore_retained");
        return; /* Native cleanup already ran; never execute it again. */
    }
    --task_scope_count; --depth;
    current_cast=previous;
    memset(&task_scopes[n],0,sizeof(task_scopes[n]));
    SetLastError(result_error);
}

static void skill_camera(void *skill,unsigned int ending) {
    Cast *previous=current_cast, *owner=NULL;
    unsigned int i;
    DWORD error=GetLastError(), result_error;
    if(!game_thread || GetCurrentThreadId()!=game_thread) {
        original_skill_camera[ending](skill); return;
    }
    if(!lineage_fault) for(i=0;i<MAX_CASTS;++i) {
        Cast *c=&casts[i];
        if(c->owner.cast_id && c->skill==skill && !c->skill_cleaned &&
            skill_exact(c)) {
            /* Exact native callsites: start follows +6c=1, end follows +6c=0.
             * Neither callback is authority to revive a replaced component. */
            if(((uint8_t *)skill)[0x6c] == (ending ? 0:1)) owner=c;
            break;
        }
    }
    current_cast=owner;
    ++depth;
    if(owner) {
        owner->skill_active_seen=TRUE;
        SudekiMpLogFormat("lan_cast_context event=skill_camera cast=%lu actor=%u phase=%s\r\n",
            (unsigned long)owner->owner.cast_id,owner->owner.actor_type,ending ? "end":"start");
    }
    SetLastError(error);
    original_skill_camera[ending](skill);
    result_error=GetLastError();
    if(owner && ending) owner->skill_cleaned=TRUE;
    --depth;
    current_cast=previous;
    SetLastError(result_error);
}
static void __attribute__((stdcall)) skill_camera_start(void *skill) { skill_camera(skill,0); }
static void __attribute__((stdcall)) skill_camera_end(void *skill) { skill_camera(skill,1); }

/* Preserve native ESI out-cell, EDI script owner, all input registers, original
 * result registers/flags and ret 4. The descriptor is the sole stack argument. */
#define ROOT_BRIDGE(name,begin,arg) \
__attribute__((naked,noinline,used)) static void name(void) { __asm__ volatile( \
    "pushl %ebp\n\tmovl %esp,%ebp\n\tpushl %ebx\n\tpushl %esi\n\tpushl %edi\n\tsubl $20,%esp\n\t" \
    "movl %eax,-16(%ebp)\n\tmovl %ecx,-20(%ebp)\n\tmovl %edx,-24(%ebp)\n\t" \
    "pushl " arg "\n\tpushl %edi\n\tcall _" #begin "\n\taddl $8,%esp\n\tmovl %eax,-28(%ebp)\n\t" \
    "movl -16(%ebp),%eax\n\tmovl -20(%ebp),%ecx\n\tmovl -24(%ebp),%edx\n\t" \
    "pushl 8(%ebp)\n\tcall *_original_submit\n\t" \
    "movl %eax,-16(%ebp)\n\tmovl %ecx,-20(%ebp)\n\tmovl %edx,-24(%ebp)\n\tpushfl\n\tpopl -32(%ebp)\n\t" \
    "pushl -28(%ebp)\n\tcall _end_root\n\taddl $4,%esp\n\t" \
    "pushl -32(%ebp)\n\tpopfl\n\tmovl -16(%ebp),%eax\n\tmovl -20(%ebp),%ecx\n\tmovl -24(%ebp),%edx\n\t" \
    "leal -12(%ebp),%esp\n\tpopl %edi\n\tpopl %esi\n\tpopl %ebx\n\tpopl %ebp\n\tret $4\n\t"); }
ROOT_BRIDGE(skill_root,begin_root,"$1")
ROOT_BRIDGE(spirit_root,begin_root,"$2")
ROOT_BRIDGE(skill_started,begin_skill_started,"%ebx")

__attribute__((naked,noinline,used)) static void create_task(void) {
    __asm__ volatile(
        "pushl %ebp\n\tmovl %esp,%ebp\n\tpushl %ebx\n\tpushl %esi\n\tpushl %edi\n\tsubl $20,%esp\n\t"
        "movl %eax,-16(%ebp)\n\t"
        "pushl 24(%ebp)\n\tpushl 20(%ebp)\n\tpushl 16(%ebp)\n\tpushl 12(%ebp)\n\tpushl 8(%ebp)\n\t"
        "call *_original_create\n\t"
        "movl %eax,-20(%ebp)\n\tmovl %ecx,-24(%ebp)\n\tmovl %edx,-28(%ebp)\n\tpushfl\n\tpopl -32(%ebp)\n\t"
        "pushl 12(%ebp)\n\tpushl -16(%ebp)\n\tcall _created_task\n\taddl $8,%esp\n\t"
        "pushl -32(%ebp)\n\tpopfl\n\tmovl -28(%ebp),%edx\n\tmovl -24(%ebp),%ecx\n\tmovl -20(%ebp),%eax\n\t"
        "leal -12(%ebp),%esp\n\tpopl %edi\n\tpopl %esi\n\tpopl %ebx\n\tpopl %ebp\n\tret $20\n\t");
}

BOOL SudekiMpUninstallLanCastContext(void) {
    unsigned int i;
    BOOL restored=TRUE;
    if(!cast_image) return TRUE;
    if(!SudekiMpLanCastContextDrained()) return FALSE;
    if(!SudekiMpRestoreInlineHook(&ui_trace_hook)) restored=FALSE;
    if(!SudekiMpRestorePointerHook(&skill_update_hook)) restored=FALSE;
    if(!SudekiMpRestoreRelativeCallHook(&skill_started_hook)) restored=FALSE;
    for(i=2;i>0;--i) if(!SudekiMpRestoreRelativeCallHook(&root_hooks[i-1])) restored=FALSE;
    for(i=2;i>0;--i) if(!SudekiMpRestoreRelativeCallHook(&skill_camera_hooks[i-1])) restored=FALSE;
    for(i=2;i>0;--i) if(!SudekiMpRestoreRelativeCallHook(&step_hooks[i-1])) restored=FALSE;
    for(i=3;i>0;--i) if(!SudekiMpRestorePointerHook(&opcode_hooks[i-1])) restored=FALSE;
    for(i=2;i>0;--i) if(!SudekiMpRestoreRelativeCallHook(&create_hooks[i-1])) restored=FALSE;
    if(!restored) return FALSE;
    ui_trace_trampoline=NULL; ui_trace_events=0;
    cast_image=NULL; owner_witness=NULL; original_submit=NULL; original_create=NULL;
    memset(original_opcodes,0,sizeof(original_opcodes));
    memset(original_skill_camera,0,sizeof(original_skill_camera));
    original_step=NULL; task_enter=NULL; task_leave=NULL;
    original_skill_update=NULL;
    memset(casts,0,sizeof(casts)); game_thread=0; current_cast=NULL;
    lineage_fault=FALSE; next_cast_id=0; root_scope_count=0;
    return TRUE;
}

BOOL SudekiMpInstallLanCastContext(HMODULE image,SudekiMpLanCastOwnerWitness witness) {
    uint8_t *candidate=(uint8_t *)image;
    unsigned int i;
    OpcodeFunction hooks[3]={opcode27,opcode28,opcode29};
    uint32_t camera_rvas[2]={SKILL_CAMERA_START,SKILL_CAMERA_END};
    static const uint8_t camera_tail[]={0x81,0xec,0xac,0,0,0,0x53,0x56,0x57};
    if(cast_image || !candidate || !witness ||
        !memory_access(candidate+SUBMIT,sizeof(submit_prefix),FALSE) ||
        memcmp(candidate+SUBMIT,submit_prefix,sizeof(submit_prefix)) ||
        !memory_access(candidate+CREATE,sizeof(create_prefix),FALSE) ||
        memcmp(candidate+CREATE,create_prefix,sizeof(create_prefix)) ||
        !memory_access(candidate+STEP,sizeof(step_prefix),FALSE) ||
        memcmp(candidate+STEP,step_prefix,sizeof(step_prefix)) ||
        !memory_access(candidate+SKILL_STARTED-sizeof(skill_started_prefix),sizeof(skill_started_prefix),FALSE) ||
        memcmp(candidate+SKILL_STARTED-sizeof(skill_started_prefix),skill_started_prefix,sizeof(skill_started_prefix)) ||
        !memory_access(candidate+OPCODE_SLOT,12,FALSE) ||
        !memory_access(candidate+SKILL_UPDATE,0x6e,FALSE) ||
        memcmp(candidate+SKILL_UPDATE,skill_update_prefix,sizeof(skill_update_prefix)) ||
        memcmp(candidate+SKILL_UPDATE+0x67,skill_update_tail,sizeof(skill_update_tail)) ||
        !memory_access(candidate+SKILL_UPDATE_VTABLE+4,4,FALSE) ||
        *(void **)(candidate+SKILL_UPDATE_VTABLE+4)!=candidate+SKILL_UPDATE) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    for(i=0;i<3;++i) if(*(void **)(candidate+OPCODE_SLOT+i*4)!=candidate+opcode_rvas[i]) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    for(i=0;i<2;++i) {
        uint8_t *entry=candidate+camera_rvas[i];
        uint32_t address;
        if(!memory_access(entry,5+sizeof(camera_tail),FALSE)) {
            SetLastError(ERROR_INVALID_DATA); return FALSE;
        }
        memcpy(&address,entry+1,4);
        if(entry[0]!=0xa1 || address!=(uint32_t)(uintptr_t)(candidate+0x408d94) ||
            memcmp(entry+5,camera_tail,sizeof(camera_tail))) {
            SetLastError(ERROR_INVALID_DATA); return FALSE;
        }
    }
    cast_image=candidate; owner_witness=witness;
    original_submit=(RawFunction)(candidate+SUBMIT);
    original_create=(RawFunction)(candidate+CREATE);
    original_step=(OpcodeFunction)(candidate+STEP);
    original_skill_update=(SkillUpdateFunction)(candidate+SKILL_UPDATE);
    for(i=0;i<3;++i) original_opcodes[i]=(OpcodeFunction)(candidate+opcode_rvas[i]);
    original_skill_camera[0]=(SkillCameraFunction)(candidate+SKILL_CAMERA_START);
    original_skill_camera[1]=(SkillCameraFunction)(candidate+SKILL_CAMERA_END);
    if(!SudekiMpInstallRelativeCallHook(&create_hooks[0],candidate+CREATE_DIRECT,original_create,create_task) ||
        !SudekiMpInstallRelativeCallHook(&create_hooks[1],candidate+CREATE_CHILD,original_create,create_task)) goto failed;
    if(!SudekiMpInstallRelativeCallHook(&step_hooks[0],candidate+STEP_IMMEDIATE,original_step,task_step) ||
        !SudekiMpInstallRelativeCallHook(&step_hooks[1],candidate+STEP_SCHEDULED,original_step,task_step)) goto failed;
    for(i=0;i<3;++i) if(!SudekiMpInstallPointerHook(&opcode_hooks[i],
        (void **)(candidate+OPCODE_SLOT+i*4),original_opcodes[i],hooks[i])) goto failed;
    if(!SudekiMpInstallRelativeCallHook(&skill_camera_hooks[0],candidate+SKILL_CAMERA_START_CALL,
            original_skill_camera[0],skill_camera_start) ||
        !SudekiMpInstallRelativeCallHook(&skill_camera_hooks[1],candidate+SKILL_CAMERA_END_CALL,
            original_skill_camera[1],skill_camera_end)) goto failed;
    if(!SudekiMpInstallRelativeCallHook(&root_hooks[0],candidate+ROOT_SKILL,original_submit,skill_root) ||
        !SudekiMpInstallRelativeCallHook(&root_hooks[1],candidate+ROOT_SPIRIT,original_submit,spirit_root)) goto failed;
    if(!SudekiMpInstallRelativeCallHook(&skill_started_hook,candidate+SKILL_STARTED,
        original_submit,skill_started)) goto failed;
    if(!SudekiMpInstallPointerHook(&skill_update_hook,(void **)(candidate+SKILL_UPDATE_VTABLE+4),
        original_skill_update,skill_update)) goto failed;
    return TRUE;
failed: {
        DWORD error=GetLastError();
        if(!SudekiMpUninstallLanCastContext()) return FALSE;
        SetLastError(error); return FALSE;
    }
}
