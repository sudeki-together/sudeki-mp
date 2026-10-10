/* Private implementation included by lan_story_spawn_trace.c. The constructor
 * journal owns ONLY jobs born during this adapter's native group update. */
#include "hooks/lan_story_client.h"
enum { SOLO_CTOR_CALL=0x762eau, SOLO_CTOR=0x72c30u, SOLO_VT=0x2c9eb0u,
    SOLO_STEP=0x72d50u, SOLO_DESTROY=0x72e10u, SOLO_JOBS=64u };
typedef struct SoloJob {
    uint8_t *job,*group,*point,*sequence,*actor;
    uint32_t group_id,actor_id,resource_id,serial;
    unsigned stage,result;
    DWORD native_error;
    BOOL entered,active;
} SoloJob;
static SoloJob solo_jobs[SOLO_JOBS];
static uint32_t solo_serial;
static DWORD solo_thread;
static BOOL solo_fault;
static unsigned solo_callbacks;
static uint8_t *solo_scope_group;
static SudekiMpRelativeCallHook solo_ctor_hook;
static SudekiMpPointerHook solo_step_hook,solo_destroy_hook;
static void *solo_ctor_original __attribute__((used));
typedef unsigned (__attribute__((thiscall)) *SoloStep)(void *);
typedef void *(__attribute__((thiscall)) *SoloDestroy)(void *,unsigned);
static SoloStep original_solo_step;
static SoloDestroy original_solo_destroy;
static const uint8_t solo_step_head[]={0x83,0xec,0x2c,0x56,0x8b,0xf1,0x8b,0x4e,0x04};
static const uint8_t solo_ctor_head[]={0x53,0x33,0xdb,0x8d,0x48,0x10};
static const uint8_t solo_destroy_head[]={0x56,0x8b,0xf1,0xe8,0x68,0xfe,0xff,0xff};
static BOOL solo_code(uint8_t *b) {
    return readable(b+SOLO_CTOR,0x4bu) && readable(b+SOLO_STEP,0xb6u) &&
        readable(b+SOLO_DESTROY,0x1eu) &&
        !memcmp(b+SOLO_CTOR,solo_ctor_head,sizeof(solo_ctor_head)) &&
        !memcmp(b+SOLO_CTOR+0x48u,"\xc2\x08\x00",3u) &&
        !memcmp(b+SOLO_STEP,solo_step_head,sizeof(solo_step_head)) &&
        b[SOLO_STEP+0x82u]==0xc3 && b[SOLO_STEP+0xb5u]==0xc3 &&
        !memcmp(b+SOLO_DESTROY,solo_destroy_head,sizeof(solo_destroy_head)) &&
        !memcmp(b+SOLO_DESTROY+0x1bu,"\xc2\x04\x00",3u);
}
static BOOL solo_group_fields(const SoloJob *s) {
    uint8_t *g=s->group;
    if(!g || !in_registry(g) || !group_exact(g) || *(uint32_t *)(g+0x34u)!=s->group_id) return FALSE;
    unsigned points=*(unsigned *)(g+0x168u),entries=*(unsigned *)(g+0x16cu);
    uintptr_t p=(uintptr_t)*(void **)(g+0x170u),q=(uintptr_t)*(void **)(g+0x174u);
    return points && points<=256u && entries && entries<=4096u &&
        readable((void *)p,points*0x80u) && readable((void *)q,entries*0x50u) &&
        (uintptr_t)s->point>=p && (uintptr_t)s->point-p<points*0x80u && !(((uintptr_t)s->point-p)%0x80u) &&
        (uintptr_t)s->sequence>=q && (uintptr_t)s->sequence-q<entries*0x50u && !(((uintptr_t)s->sequence-q)%0x50u) &&
        *(uint32_t *)(s->sequence+0x10u)==s->resource_id;
}
static SoloJob *solo_find(void *job) {
    for(unsigned i=0;i<SOLO_JOBS;++i) if(solo_jobs[i].job==job) return &solo_jobs[i];
    return NULL;
}
static void solo_record(void *raw) {
    if(!solo_scope_group) return;
    uint8_t *j=raw;
    if(GetCurrentThreadId()!=solo_thread || solo_fault || solo_find(j) ||
        !readable(j,0x28u) || *(void **)j!=image_base+SOLO_VT ||
        *(void **)(j+0x10u)!=solo_scope_group || j[0x24u] || solo_serial==UINT32_MAX) { solo_fault=TRUE; return; }
    SoloJob s={.job=j,.group=solo_scope_group,.point=*(void **)(j+0x1cu),
        .sequence=*(void **)(j+0x20u),.group_id=*(uint32_t *)(solo_scope_group+0x34u)};
    if(!readable(s.sequence,0x50u)) {solo_fault=TRUE;return;}
    s.resource_id=*(uint32_t *)(s.sequence+0x10u);
    if(!solo_group_fields(&s)) {solo_fault=TRUE;return;}
    for(unsigned i=0;i<SOLO_JOBS;++i) if(!solo_jobs[i].job) {
        s.serial=++solo_serial; solo_jobs[i]=s; return;
    }
    solo_fault=TRUE;
}
static void __attribute__((noinline,used,force_align_arg_pointer)) solo_constructed(void *raw) {
    DWORD error=GetLastError(); solo_record(raw); SetLastError(error);
}
/* Constructor: EAX=allocation, EDX=group, two stack arguments, RET8.
 * The observer runs after construction and preserves all native return state. */
static void __attribute__((naked,noinline,used)) solo_ctor_entry(void) {
    __asm__ volatile("pushl %ebp\n\tmovl %esp,%ebp\n\tpushl 12(%ebp)\n\tpushl 8(%ebp)\n\t"
        "call *_solo_ctor_original\n\tpushfl\n\tpushal\n\tpushl 28(%esp)\n\tcall _solo_constructed\n\t"
        "addl $4,%esp\n\tpopal\n\tpopfl\n\tpopl %ebp\n\tret $8\n\t");
}
static uint8_t *solo_actor(const SoloJob *s) {
    uint8_t *resource=*(uint8_t **)(s->job+4u);
    if(!readable(resource,0x14u)) return NULL;
    uint8_t *table=*(uint8_t **)resource,*e=(uint8_t *)((uintptr_t)resource-0x2cu);
    unsigned primary=table==image_base+0x2d5fe4u?0x2d5fa0u:
        table==image_base+0x2d5618u?0x2d55d4u:0;
    /* Native getter 43A490 is LEA EAX,[ECX-2C]; RET. No call is needed. */
    if(!primary || !readable(e,0x145u) || *(void **)e!=image_base+primary ||
        *(void **)table!=image_base+0x3a490u || memcmp(image_base+0x3a490u,"\x8d\x41\xd4\xc3",4u) ||
        *(uint32_t *)(resource+8u)!=s->resource_id) return NULL;
    return e;
}
static BOOL solo_witness(void *context) {
    SoloJob *s=context; uint8_t *j=s->job;
    return !solo_fault && s->active && solo_callbacks && GetCurrentThreadId()==solo_thread &&
        solo_step_hook.installed && *solo_step_hook.slot==solo_step_hook.replacement_value &&
        readable(j,0x28u) && *(void **)j==image_base+SOLO_VT &&
        *(void **)(j+0x10u)==s->group && *(void **)(j+0x1cu)==s->point &&
        *(void **)(j+0x20u)==s->sequence && j[0x24u]==s->stage && s->stage<=4u &&
        solo_group_fields(s) && solo_actor(s)==s->actor &&
        *(uint32_t *)(s->actor+0x34u)==s->actor_id;
}
static BOOL solo_operation(void *context) {
    SoloJob *s=context; s->entered=TRUE;
    s->result=original_solo_step(s->job); s->native_error=GetLastError();
    if(readable(s->job,0x28u)) s->stage=s->job[0x24u];
    return TRUE;
}
static unsigned __attribute__((thiscall)) solo_step(void *raw) {
    DWORD error=GetLastError();
    ++solo_callbacks; SoloJob *s=solo_find(raw); unsigned result;
    if(!s) {SetLastError(error);result=original_solo_step(raw);error=GetLastError();}
    else {
        uint8_t *actor=readable(s->job,0x28u)?solo_actor(s):NULL;
        if(!s->actor && actor) {s->actor=actor;s->actor_id=*(uint32_t *)(actor+0x34u);}
        s->active=TRUE; s->entered=FALSE;
        unsigned previous=s->stage;
        BOOL exact=solo_witness(s),accounted=FALSE;
        if(exact && solo_scope_group==s->group) {solo_operation(s);accounted=TRUE;}
        else if(exact) accounted=SudekiMpLanStoryClientSpawnResidencyCall(solo_witness,s,actor,solo_operation);
        /* Native work must still run exactly once if admission is lost. This
         * retains containment failure; it does not invent cancellation. */
        if(!s->entered) (void)solo_operation(s);
        result=s->result; error=s->native_error; s->active=FALSE;
        SudekiMpLogFormat("story_spawn_trace event=solo_step serial=%lu stage=%u->%u exact=%u accounted=%u terminal=%u\r\n",
            (unsigned long)s->serial,previous,s->stage,exact,accounted,result&255u);
    }
    --solo_callbacks; SetLastError(error); return result;
}
static void * __attribute__((thiscall)) solo_destroy(void *raw,unsigned flags) {
    ++solo_callbacks; SoloJob *s=solo_find(raw);
    void *result=original_solo_destroy(raw,flags); DWORD error=GetLastError();
    if(s) { /* Positive destructor return; do not read released storage. */
        SudekiMpLogFormat("story_spawn_trace event=solo_retired serial=%lu\r\n",(unsigned long)s->serial);
        memset(s,0,sizeof(*s));
    }
    --solo_callbacks;SetLastError(error);return result;
}
static BOOL spawn_jobs_install(uint8_t *b) {
    if(!solo_code(b)) return FALSE;
    solo_ctor_original=b+SOLO_CTOR; original_solo_step=(SoloStep)(b+SOLO_STEP);
    original_solo_destroy=(SoloDestroy)(b+SOLO_DESTROY);
    if(!SudekiMpInstallRelativeCallHook(&solo_ctor_hook,b+SOLO_CTOR_CALL,b+SOLO_CTOR,solo_ctor_entry)) return FALSE;
    if(!SudekiMpInstallPointerHook(&solo_step_hook,(void **)(b+SOLO_VT+4u),b+SOLO_STEP,(void *)solo_step)) goto fail;
    if(!SudekiMpInstallPointerHook(&solo_destroy_hook,(void **)(b+SOLO_VT),b+SOLO_DESTROY,(void *)solo_destroy)) goto fail;
    return TRUE;
fail:
    (void)SudekiMpRestorePointerHook(&solo_step_hook);
    (void)SudekiMpRestoreRelativeCallHook(&solo_ctor_hook);
    return FALSE;
}
static BOOL spawn_jobs_can_uninstall(void) {
    if(solo_callbacks || solo_scope_group) {SetLastError(ERROR_BUSY);return FALSE;}
    for(unsigned i=0;i<SOLO_JOBS;++i) if(solo_jobs[i].job) {SetLastError(ERROR_BUSY);return FALSE;}
    return TRUE;
}
static BOOL spawn_jobs_uninstall(void) {
    if(!spawn_jobs_can_uninstall()) return FALSE;
    BOOL ok=SudekiMpRestorePointerHook(&solo_destroy_hook);
    if(!SudekiMpRestorePointerHook(&solo_step_hook)) ok=FALSE;
    if(!SudekiMpRestoreRelativeCallHook(&solo_ctor_hook)) ok=FALSE;
    return ok;
}
static void spawn_jobs_begin(uint8_t *group) {
    if(solo_scope_group) {solo_fault=TRUE;return;}
    if(!solo_thread) solo_thread=GetCurrentThreadId();
    if(solo_thread==GetCurrentThreadId() && !solo_fault) solo_scope_group=group;
}
static void spawn_jobs_end(void) {solo_scope_group=NULL;}
