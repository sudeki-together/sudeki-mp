#include "hooks/lan_story_loot_trace.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <stdint.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Story loot observers require the exact retail x86 ABI"
#endif
#define HELPER __attribute__((noinline,used,force_align_arg_pointer))
/* Observation may call formatting/Win32 code. Preserve state not covered by
 * PUSHAD/PUSHFD too, including live x87 values and the thread's last error. */
typedef struct ObserveState {
    uint8_t fp[512] __attribute__((aligned(16)));
    DWORD error;
} ObserveState;
static void observe_leave(ObserveState *s) {
    SetLastError(s->error);
    __asm__ volatile("fxrstor %0" : : "m"(s->fp) : "memory");
}
#define OBSERVE_STATE \
    ObserveState saved_state __attribute__((cleanup(observe_leave))); \
    const uint32_t observe_mxcsr=0x1f80u; \
    __asm__ volatile("fxsave %0" : "=m"(saved_state.fp) : : "memory"); \
    __asm__ volatile("fninit; ldmxcsr %0" : : "m"(observe_mxcsr) : "memory"); \
    saved_state.error=GetLastError()
enum { DEPTH=16, JOBS=64, LOG_LIMIT=512 };
typedef struct Scope { void *actor,*component; uint32_t id; } Scope;
typedef struct Job {
    void *task,*resource;
    uint32_t id,source,item;
    BOOL used,entered,completed,destroying;
} Job;
static uint8_t *base;
static DWORD thread;
static SudekiMpLanStoryNativeRoster roster;
static BOOL bound,installed;
static volatile LONG callbacks,unknown;
static uint32_t serial,logs;
static Scope actor_scopes[DEPTH],break_scopes[DEPTH];
static unsigned actor_depth,break_depth;
static Job jobs[JOBS];
static SudekiMpPointerHook pointers[4];
static SudekiMpRelativeCallHook submit_hook,collect_hook;
static SudekiMpInlineHook finish_hook;
static void *actor_original __attribute__((used));
static void *break_original __attribute__((used));
static void *ready_original __attribute__((used));
static void *destroy_original __attribute__((used));
static void *submit_original __attribute__((used));
static void *finish_original __attribute__((used));
static void *collect_original __attribute__((used));
static void actor_entry(void),break_entry(void),ready_entry(void),destroy_entry(void);
static void submit_entry(void),finish_entry(void),collect_entry(void);
static const void *entry_address(void (*entry)(void)) {
    return (const void *)(uintptr_t)entry;
}

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || VirtualQuery(p,&m,sizeof(m))!=sizeof(m) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
        a+n>(uintptr_t)m.BaseAddress+m.RegionSize) return FALSE;
    DWORD access=m.Protect&0xffu;
    return access==PAGE_READONLY || access==PAGE_READWRITE || access==PAGE_WRITECOPY ||
        access==PAGE_EXECUTE_READ || access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
}
static BOOL bytes(const uint8_t *b,unsigned rva,const void *p,size_t n) {
    return readable(b+rva,n) && !memcmp(b+rva,p,n);
}
static BOOL slot(const uint8_t *b,unsigned at,unsigned target) {
    return readable(b+at,4) && *(void **)(b+at)==b+target;
}
static BOOL call(const uint8_t *b,unsigned at,unsigned target) {
    int32_t d;
    if(!readable(b+at,5) || b[at]!=0xe8) return FALSE;
    memcpy(&d,b+at+1,4); return b+at+5+d==b+target;
}
static BOOL image_exact(const uint8_t *b) {
    static const uint8_t hit[]={0x53,0x8b,0xd9,0x83,0x7b,0x64,0x4d};
    static const uint8_t actor[]={0x56,0x8b,0x74,0x24,8,0x57,0x8b,0xf9};
    /* The animation dispatcher passes event AND context. The decompiler may
     * omit the unused context; the caller and every native exit require RET8. */
    static const uint8_t actor_call[]={0x8b,0x11,0x8b,0x52,4,0x8d,0x44,0x24,0x24,0x50,
        0x8b,0x44,0x24,0x30,0x8b,0x40,4,0x50,0xff,0xd2};
    static const uint8_t actor_tail[]={0x5f,0x5e,0xc7,0x44,0x24,8,1,0,0,0,
        0x89,0x44,0x24,4,0xe9,0xae,0xcc,0x0a,0};
    static const uint8_t finish[]={0xf7,0x47,0x50,0,2,0,0};
    static const uint8_t ready[]={0x55,0x8b,0xec,0x83,0xe4,0xf8,0x83,0xec,0x18,0x53,0x8b,0xd9};
    static const uint8_t destroy[]={0xf6,0x44,0x24,4,1,0x56,0x8b,0xf1,0xc7,6};
    static const uint8_t submit[]={0x8b,0xce,0x50,0xba,1,0,0,0,0x8d,0x74,0x24,0x38};
    return b && SudekiMpCheckLoadedExecutable((HMODULE)b) &&
        bytes(b,0x150d70,hit,sizeof(hit)) && bytes(b,0xdbe40,actor,sizeof(actor)) &&
        bytes(b,0x18ac64,actor_call,sizeof(actor_call)) &&
        bytes(b,0xdbebf,actor_tail,sizeof(actor_tail)) &&
        bytes(b,0xdbef8,"\xc2\x08\0",3) && bytes(b,0xdbf04,"\xc2\x08\0",3) &&
        bytes(b,0xdbf56,"\xc2\x08\0",3) && bytes(b,0x188c2f,"\xc2\x08\0",3) &&
        bytes(b,0x188c50,"\xc2\x08\0",3) && bytes(b,0xdbe35,"\xc3",1) &&
        bytes(b,0xdbdc0,finish,sizeof(finish)) && bytes(b,0x1f860,ready,sizeof(ready)) &&
        bytes(b,0x3c7e0,destroy,sizeof(destroy)) && slot(b,0x3c7ea,0x2c55f4) &&
        bytes(b,0x1fc78,submit,sizeof(submit)) && call(b,0x1fc84,0x11730) &&
        bytes(b,0x117a7,"\xc2\x10\0",3) && bytes(b,0x1fa23,"\xc3",1) &&
        bytes(b,0x150de6,"\xc2\x04\0",3) && bytes(b,0x3c7fc,"\xc2\x04\0",3) &&
        slot(b,0x2cca18,0xdbe40) && slot(b,0x2c8700,0x150d70) &&
        slot(b,0x2c6c4c,0x1f860) && slot(b,0x2c6c48,0x3c7e0) &&
        call(b,0x3a14d,0x1338a0) && bytes(b,0x3a14c,"\x50",1) &&
        bytes(b,0x3a152,"\x83\xc6\x0c\x4f",4) &&
        bytes(b,0x1338a0,"\x53\x8b\x5c\x24\x08\x8b\x43\x10",8) &&
        bytes(b,0x1338eb,"\xc2\x04\0",3) && bytes(b,0x133913,"\xc2\x04\0",3);
}
static BOOL native_thread(void) { return bound && thread && thread==GetCurrentThreadId(); }
static void fault(void) { InterlockedExchange(&unknown,1); }
static uint32_t next(void) { if(serial==UINT32_MAX) {fault();return 0;} return ++serial; }
static void record(const char *event,uint32_t id,uint32_t parent,void *object,
    void *actor,uint32_t a,uint32_t b) {
    if(logs>=LOG_LIMIT) return;
    ++logs;
    SudekiMpLogFormat("story_loot_trace event=%s id=%lu parent=%lu object=%p actor=%p a=%lu b=%lu unknown=%u policy=observe_only\r\n",
        event,(unsigned long)id,(unsigned long)parent,object,actor,(unsigned long)a,
        (unsigned long)b,(unsigned)InterlockedCompareExchange(&unknown,0,0));
}
static void *actor_exact(void *arbiter) {
    static const unsigned vts[4]={0x2d5a88,0x2d66fc,0x2d5010,0x2d555c};
    if(!native_thread() || !readable(arbiter,0x64) || *(void **)arbiter!=base+0x2cc9ac) return NULL;
    uint8_t *a=*(uint8_t **)((uint8_t *)arbiter+0x10);
    for(unsigned c=0;c<4;++c) if(a && a==roster.actors[c] &&
        (roster.available_mask&(1u<<c)) && readable(a,0x134) &&
        *(void **)a==base+vts[c] && *(void **)(a+0x90)==arbiter &&
        readable(base+0x408d10,4) && *(void **)(base+0x408d10)==roster.world) return a;
    return NULL;
}
static BOOL component_exact(void *component) {
    uint8_t *c=component;
    if(!native_thread() || !readable(c,0x80) || *(void **)c!=base+0x2c869c) return FALSE;
    uint8_t *e=*(uint8_t **)(c+0x10);
    return readable(e,0x134) && *(void **)e==base+0x2c8a8c &&
        c==e+0x70c && *(void **)(e+0x128)==c && *(void **)(e+0x48)==c;
}
static Job *find_job(void *task) {
    for(unsigned i=0;i<JOBS;++i) if(jobs[i].used && jobs[i].task==task) return &jobs[i];
    return NULL;
}
static HELPER void __cdecl actor_before(void *sub,uint32_t event) {
    OBSERVE_STATE;
    InterlockedIncrement(&callbacks);
    if(!native_thread()) return;
    ++actor_depth;
    if(actor_depth>DEPTH) { fault();return; }
    Scope *s=&actor_scopes[actor_depth-1]; memset(s,0,sizeof(*s));
    s->actor=(uintptr_t)sub>=0x3c?actor_exact((uint8_t *)sub-0x3c):NULL;
    uint8_t *arb=(uint8_t *)sub-0x3c;
    if(s->actor && ((*(uint32_t *)(arb+0x50)&0x200u) ||
        ((*(uint32_t *)(arb+0x58)>>8)&0x1ffu)==0x4du)) {
        s->id=next();
        record("actor_event",s->id,0,arb,s->actor,event,*(uint32_t *)(arb+0x58));
    }
}
static HELPER void __cdecl actor_after(void) {
    OBSERVE_STATE;
    if(native_thread()) { if(actor_depth) --actor_depth; else fault(); }
    InterlockedDecrement(&callbacks);
}
static HELPER void __cdecl break_before(void *component,uint32_t event) {
    OBSERVE_STATE;
    InterlockedIncrement(&callbacks);
    if(!native_thread()) return;
    ++break_depth;
    if(break_depth>DEPTH) {fault();return;}
    Scope *s=&break_scopes[break_depth-1]; memset(s,0,sizeof(*s));
    if(!component_exact(component)) return;
    s->component=component; s->id=next();
    Scope *parent=actor_depth && actor_depth<=DEPTH?&actor_scopes[actor_depth-1]:NULL;
    s->actor=parent?parent->actor:NULL;
    record("break_enter",s->id,parent?parent->id:0,component,s->actor,event,
        *(uint32_t *)((uint8_t *)component+0x7c));
}
static HELPER void __cdecl break_after(void) {
    OBSERVE_STATE;
    if(native_thread()) {
        if(break_depth && break_depth<=DEPTH) {
            Scope *s=&break_scopes[break_depth-1];
            if(s->id && component_exact(s->component))
                record("break_return",s->id,0,s->component,s->actor,
                    *(uint32_t *)((uint8_t *)s->component+0x7c),
                    *(uint32_t *)((uint8_t *)s->component+0x60));
            else if(s->id) fault();
        }
        if(break_depth) --break_depth; else fault();
    }
    InterlockedDecrement(&callbacks);
}
static HELPER void __cdecl submitted(void *task) {
    OBSERVE_STATE;
    if(!native_thread() || !break_depth || break_depth>DEPTH) return;
    Scope *source=&break_scopes[break_depth-1]; uint8_t *t=task;
    if(!source->id) return;
    if(!readable(t,0x28) || *(void **)t!=base+0x2c6c48 || find_job(task)) {fault();return;}
    Job *j=NULL;
    for(unsigned i=0;i<JOBS;++i) if(!jobs[i].used) { j=&jobs[i]; break; }
    if(!j) {fault();return;}
    *j=(Job){.task=task,.resource=*(void **)(t+0x14),.id=next(),.source=source->id,
        .item=*(uint32_t *)(t+0x18),.used=TRUE};
    record("drop_submit",j->id,j->source,j->resource,source->actor,j->item,0);
}
static HELPER void __cdecl ready_before(void *task) {
    OBSERVE_STATE;
    InterlockedIncrement(&callbacks);
    if(!native_thread()) return;
    Job *j=find_job(task);
    if(!j) return;
    uint8_t *t=task;
    if(j->entered || j->completed || j->destroying || !readable(t,0x28) ||
        *(void **)t!=base+0x2c6c48 || *(void **)(t+0x14)!=j->resource ||
        *(uint32_t *)(t+0x18)!=j->item) {fault();return;}
    j->entered=TRUE; record("drop_setup_enter",j->id,j->source,j->resource,NULL,j->item,0);
}
static HELPER void __cdecl ready_after(void *task,uint32_t result) {
    OBSERVE_STATE;
    if(native_thread()) {
        Job *j=find_job(task);
        if(j && j->entered) {
            j->entered=FALSE; j->completed=(result&255u)!=0;
            record("drop_setup_return",j->id,j->source,j->resource,NULL,j->item,result&255u);
        }
    }
    InterlockedDecrement(&callbacks);
}
static HELPER void __cdecl destroy_before(void *task) {
    OBSERVE_STATE;
    InterlockedIncrement(&callbacks);
    if(!native_thread()) return;
    Job *j=find_job(task);
    if(j) {
        if(j->entered || j->destroying) fault();
        j->destroying=TRUE;
    }
}
static HELPER void __cdecl destroy_after(void *task) {
    OBSERVE_STATE;
    if(native_thread()) {
        Job *j=find_job(task);
        if(j && j->destroying) {
            /* Original destructor may already have freed task. Never read it. */
            record("drop_destructor_return",j->id,j->source,j->resource,NULL,j->item,j->completed);
            memset(j,0,sizeof(*j));
        }
    }
    InterlockedDecrement(&callbacks);
}
static HELPER void __cdecl finished(void *arbiter,uint32_t state,uint32_t returned) {
    OBSERVE_STATE;
    if(!returned) InterlockedIncrement(&callbacks);
    void *a=actor_exact(arbiter);
    if(a && (state==0x4du || (*(uint32_t *)((uint8_t *)arbiter+0x50)&0x200u)))
        record(returned?"actor_finish_return":"actor_finish_enter",next(),0,arbiter,a,state,
        *(uint32_t *)((uint8_t *)arbiter+0x58));
    if(returned) InterlockedDecrement(&callbacks);
}
static HELPER void __cdecl collected(void *component,uint32_t result,uint32_t returned) {
    OBSERVE_STATE;
    if(!returned) InterlockedIncrement(&callbacks);
    uint8_t *c=component;
    if(native_thread() && readable(c,0x50)) {
        uint8_t *e=*(uint8_t **)(c+0x10);
        if(readable(e,0x104) && *(void **)(e+0x100)==c)
            record(returned?"pickup_return":"pickup_enter",next(),0,e,NULL,
                returned?(result&255u):0,c[0x4c]);
    }
    if(returned) InterlockedDecrement(&callbacks);
}

/* All wrappers preserve native registers/flags around observations. Native
 * arguments, stack cleanup and return registers remain the original's. */
__attribute__((naked)) static void actor_entry(void) {
    __asm__ volatile("pushfl; pushal; push 40(%esp); push %ecx; call _actor_before; add $8,%esp; popal; popfl;"
        /* Copy context first, then event (ESP moves after the first PUSH).
         * Both normal returns and the native tail-call exit clean eight bytes.
         * Do not use the separate breakable callback's one-argument ABI here. */
        "push 8(%esp); push 8(%esp); call *_actor_original;"
        "pushfl; pushal; call _actor_after; popal; popfl; ret $8");
}
__attribute__((naked)) static void break_entry(void) {
    __asm__ volatile("pushfl; pushal; push 40(%esp); push %ecx; call _break_before; add $8,%esp; popal; popfl;"
        "push 4(%esp); call *_break_original;"
        "pushfl; pushal; call _break_after; popal; popfl; ret $4");
}
__attribute__((naked)) static void submit_entry(void) {
    __asm__ volatile("pushfl; pushal; push %ecx; call _submitted; add $4,%esp; popal; popfl; jmp *_submit_original");
}
__attribute__((naked)) static void ready_entry(void) {
    __asm__ volatile("push %ebx; mov %ecx,%ebx;"
        "pushfl; pushal; push %ecx; call _ready_before; add $4,%esp; popal; popfl;"
        "call *_ready_original;"
        "pushfl; pushal; push %eax; push %ebx; call _ready_after; add $8,%esp; popal; popfl; pop %ebx; ret");
}
__attribute__((naked)) static void destroy_entry(void) {
    __asm__ volatile("push %ebx; mov %ecx,%ebx;"
        "pushfl; pushal; push %ecx; call _destroy_before; add $4,%esp; popal; popfl;"
        "push 8(%esp); call *_destroy_original;"
        "pushfl; pushal; push %ebx; call _destroy_after; add $4,%esp; popal; popfl; pop %ebx; ret $4");
}
__attribute__((naked)) static void finish_entry(void) {
    __asm__ volatile("pushfl; pushal; push $0; push %esi; push %edi; call _finished; add $12,%esp; popal; popfl;"
        /* Replay the exact seven-byte TEST before the unmodified branch.
         * The continuation is published BEFORE the entry patch, so no live
         * wrapper can observe an unpublished trampoline. */
        "testl $0x200,0x50(%edi); call *_finish_original;"
        "pushfl; pushal; push $1; push %esi; push %edi; call _finished; add $12,%esp; popal; popfl; ret");
}
__attribute__((naked)) static void collect_entry(void) {
    __asm__ volatile("push %ebx; mov 8(%esp),%ebx;"
        "pushfl; pushal; push $0; push $0; push %ebx; call _collected; add $12,%esp; popal; popfl;"
        "push %ebx; call *_collect_original;"
        "pushfl; pushal; push $1; push %eax; push %ebx; call _collected; add $12,%esp; popal; popfl; pop %ebx; ret $4");
}
static BOOL retained(DWORD error) {
    HMODULE self;
    (void)GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        (LPCSTR)(uintptr_t)&SudekiMpLanStoryLootTraceUninstall,&self);
    SetLastError(error?error:ERROR_BUSY); return FALSE;
}
BOOL SudekiMpLanStoryLootTraceInstall(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    if(base || !IsProcessorFeaturePresent(PF_XMMI_INSTRUCTIONS_AVAILABLE) || !image_exact(b)) {
        SetLastError(ERROR_INVALID_DATA);return FALSE;
    }
    base=b; thread=0; bound=installed=FALSE; serial=logs=0;
    memset(jobs,0,sizeof(jobs)); actor_depth=break_depth=0;
    InterlockedExchange(&unknown,0); InterlockedExchange(&callbacks,0);
    actor_original=b+0xdbe40; break_original=b+0x150d70; ready_original=b+0x1f860;
    destroy_original=b+0x3c7e0; submit_original=b+0x11730; finish_original=b+0xdbdc7;
    collect_original=b+0x1338a0;
    static const uint8_t prefix[]={0xf7,0x47,0x50,0,2,0,0};
    if(!SudekiMpInstallPointerHook(&pointers[0],(void **)(b+0x2cca18),actor_original,entry_address(actor_entry)) ||
        !SudekiMpInstallPointerHook(&pointers[1],(void **)(b+0x2c8700),break_original,entry_address(break_entry)) ||
        !SudekiMpInstallPointerHook(&pointers[2],(void **)(b+0x2c6c4c),ready_original,entry_address(ready_entry)) ||
        !SudekiMpInstallPointerHook(&pointers[3],(void **)(b+0x2c6c48),destroy_original,entry_address(destroy_entry)) ||
        !SudekiMpInstallRelativeCallHook(&submit_hook,b+0x1fc84,submit_original,entry_address(submit_entry)) ||
        !SudekiMpInstallRelativeCallHook(&collect_hook,b+0x3a14d,collect_original,entry_address(collect_entry)) ||
        !SudekiMpInstallInlineHook(&finish_hook,b+0xdbdc0,prefix,sizeof(prefix),entry_address(finish_entry))) {
        DWORD error=GetLastError(); (void)SudekiMpLanStoryLootTraceUninstall(); SetLastError(error);return FALSE;
    }
    installed=TRUE; return TRUE;
}
BOOL SudekiMpLanStoryLootTraceBind(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r) {
    if(!installed || !w || !r || !w->service_post_original_exact ||
        (thread && thread!=GetCurrentThreadId()) || InterlockedCompareExchange(&callbacks,0,0) ||
        !SudekiMpLanStoryObserverRosterStillExact(w,r)) return FALSE;
    if(bound && roster.world!=r->world) {fault();return FALSE;}
    thread=GetCurrentThreadId(); roster=*r; bound=TRUE; return TRUE;
}
BOOL SudekiMpLanStoryLootTraceUninstall(void) {
    if(!base) return TRUE;
    if((thread && thread!=GetCurrentThreadId()) || InterlockedCompareExchange(&callbacks,0,0) ||
        actor_depth || break_depth) return retained(ERROR_BUSY);
    for(unsigned i=0;i<JOBS;++i) if(jobs[i].used) return retained(ERROR_BUSY);
    BOOL ok=TRUE; DWORD error=ERROR_SUCCESS;
    if(!SudekiMpRestoreInlineHook(&finish_hook)) {ok=FALSE;error=GetLastError();}
    if(!SudekiMpRestoreRelativeCallHook(&collect_hook)) {if(ok) error=GetLastError();ok=FALSE;}
    if(!SudekiMpRestoreRelativeCallHook(&submit_hook)) {if(ok) error=GetLastError();ok=FALSE;}
    for(unsigned i=4;i-->0;) if(!SudekiMpRestorePointerHook(&pointers[i])) {if(ok) error=GetLastError();ok=FALSE;}
    if(!ok) return retained(error);
    /* Every slot is restored on the native thread, outside every wrapper.
     * No pending native setup can still depend on these observations. */
    base=NULL; installed=bound=FALSE; thread=0; return TRUE;
}
