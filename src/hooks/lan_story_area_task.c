#include "hooks/lan_story_area_task.h"
#include "hooks/lan_story_area_finalise.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include <string.h>
#if !defined(__GNUC__) || !defined(__i386__)
#error "Story area jobs require the supported x86 ABI"
#endif
enum { WATCHES=8, ENQUEUE_SITE=0x10d4de, ENQUEUE_NATIVE=0x1bb330,
    TASK_VT=0x2cdde8, RUN_NATIVE=0x10d500, DESTROY_NATIVE=0x72340,
    WORKER_HANDLE=0x324008, ACTIVE=0x3c30dc, QUEUE=0x3c30e0,
    QUEUE_LOCK=0x34e750, WORKER_RUN=0x3c30b2, WORLD=0x408d10,
    PUBLISH_SITE=0x10d5f3 };
static const uint8_t publish_bytes[]={0x89,0x7e,0x10,0x8b,0x4b,0x10,0x89,0x69,0x18};
enum { ENQUEUE_BEGIN, ENQUEUE_END, RUN_BEGIN, RUN_END, DESTROY_BEGIN, DESTROY_END };
typedef struct Watch {
    SudekiMpLanStoryAreaTaskReceipt value;
    SudekiMpStoryAreas *policy;
    uint64_t pin;
    const void *descriptor,*world,*table,*request;
    unsigned count;
} Watch;
static Watch watches[WATCHES];
static SRWLOCK lock=SRWLOCK_INIT;
static uint8_t *base;
static uint64_t serial;
static DWORD startup_thread,producer_thread,worker_thread;
static HANDLE worker_copy,worker_native;
static unsigned callbacks;
static BOOL installed,stopping,unknown;
static SudekiMpRelativeCallHook enqueue_hook;
static SudekiMpPointerHook run_hook,destroy_hook;
static SudekiMpInlineHook publish_hook;
static void *enqueue_original __attribute__((used)),*run_original __attribute__((used)),
    *destroy_original __attribute__((used));
static void *publish_continue __attribute__((used));
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
static BOOL call_exact(uint8_t *b,unsigned site,unsigned target) {
    int32_t offset;
    if(!readable(b+site,5) || b[site]!=0xe8) return FALSE;
    memcpy(&offset,b+site+1,4); return b+site+5+offset==b+target;
}
static void relocated_word(uint8_t *p,const uint8_t *b,unsigned rva) {
    uint32_t value=(uint32_t)(uintptr_t)(b+rva); memcpy(p,&value,4);
}
static BOOL worker_lock_exact(uint8_t *b) {
    /* Static constructor initializes this lock before the worker is created.
     * Worker publishes ACTIVE under it, drops it during Run, then reacquires
     * it for deletion and ACTIVE clearing. These are the teardown witnesses,
     * not a claim that all descendant resource jobs have finished. */
    uint8_t initialize[]={0x68,0,0,0,0,0xff,0x15,0,0,0,0,0x68,0,0,0,0,
        0xe8,0x52,0x31,0xfb,0xff,0x59,0xc3};
    relocated_word(initialize+1,b,QUEUE_LOCK); relocated_word(initialize+7,b,0x29a060);
    relocated_word(initialize+12,b,0x297310);
    uint8_t registers[]={0x8b,0x35,0,0,0,0,0x57,0x8b,0x3d,0,0,0,0,0x33,0xed};
    relocated_word(registers+2,b,0x29a068); relocated_word(registers+9,b,0x29a064);
    uint8_t enter[]={0x68,0,0,0,0,0xff,0xd6}; relocated_word(enter+1,b,QUEUE_LOCK);
    uint8_t selected[]={0x8b,0x48,4,0x68,0,0,0,0,0xa3,0,0,0,0,0x89,0x0d,0,0,0,0,
        0xff,0xd7,0x8b,0x0d,0,0,0,0,0x8b,0x11,0x8b,0x42,4,0xff,0xd0,
        0x68,0,0,0,0,0xff,0xd6,0x8b,0x0d,0,0,0,0,0x3b,0xcd,0x74,8};
    relocated_word(selected+4,b,QUEUE_LOCK); relocated_word(selected+9,b,ACTIVE);
    relocated_word(selected+15,b,QUEUE); relocated_word(selected+23,b,ACTIVE);
    relocated_word(selected+35,b,QUEUE_LOCK); relocated_word(selected+43,b,ACTIVE);
    uint8_t cleared[]={0xa1,0,0,0,0,0x89,0x2d,0,0,0,0,0x3b,0xc5,0x75,0xb6};
    relocated_word(cleared+1,b,QUEUE); relocated_word(cleared+7,b,ACTIVE);
    return bytes(b,0x2956d0,initialize,sizeof(initialize)) &&
        bytes(b,0x1bb20b,registers,sizeof(registers)) && bytes(b,0x1bb238,enter,sizeof(enter)) &&
        bytes(b,0x1bb250,selected,sizeof(selected)) && bytes(b,0x1bb28b,cleared,sizeof(cleared));
}
static BOOL image_exact(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    if(!readable(b,sizeof(IMAGE_DOS_HEADER))) return FALSE;
    IMAGE_DOS_HEADER *dos=(void *)b;
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 ||
        (uint32_t)dos->e_lfanew>SUDEKIMP_EXPECTED_IMAGE_SIZE-sizeof(IMAGE_NT_HEADERS32) ||
        !readable(b+dos->e_lfanew,sizeof(IMAGE_NT_HEADERS32)) ||
        !SudekiMpCheckLoadedExecutable(image)) return FALSE;
    static const uint8_t factory[]={0x56,0x6a,0x14,0xe8,0x42,0xb0,0x13,0,0x83,0xc4,4,
        0x85,0xc0,0x74,0x28,0x8b,0x4c,0x24,8,0xba,1,4,0,0};
    uint8_t fields[]={0xc7,0,0,0,0,0,0x89,0x48,0x10,0xc7,0x40,0xc,0x41,0,0,0,
        0x66,0x89,0x50,8,0x8b,0xf0};
    uint32_t vt=(uint32_t)(uintptr_t)(b+TASK_VT); memcpy(fields+2,&vt,4);
    static const uint8_t run[]={0x55,0x8b,0xec,0x83,0xe4,0xf8,0x83,0xec,0x38,
        0x53,0x55,0x8b,0xd9,0xb8,1,4,0,0,0x56,0x57,0x66,0x39,0x43,8};
    uint8_t destroy[]={0xf6,0x44,0x24,4,1,0x56,0x8b,0xf1,0xc7,6,0,0,0,0,
        0x74,9,0x56,0xe8,0xf9,0x60,0x1d,0,0x83,0xc4,4,0x8b,0xc6,0x5e,0xc2,4,0};
    vt=(uint32_t)(uintptr_t)(b+0x2c8870); memcpy(destroy+10,&vt,4);
    static const uint8_t dispatch[]={0x8b,0x11,0x8b,0x42,4,0xff,0xd0};
    static const uint8_t deletion[]={0x8b,0x11,0x8b,2,0x6a,1,0xff,0xd0};
    return worker_lock_exact(b) && bytes(b,0x10d4b0,factory,sizeof(factory)) && bytes(b,0x10d4c8,fields,sizeof(fields)) &&
        call_exact(b,ENQUEUE_SITE,ENQUEUE_NATIVE) && bytes(b,RUN_NATIVE,run,sizeof(run)) &&
        bytes(b,PUBLISH_SITE,publish_bytes,sizeof(publish_bytes)) &&
        /* EBP is either the checked zone interface minus four or NULL;
         * ESI is this request's descriptor and EDI the returned interface. */
        bytes(b,0x10d5c7,(const uint8_t[]){0x8b,0x7c,0x24,0x24,0x8b,0x17,0x8b,0x42,0x10,
            0x8b,0xcf,0xff,0xd0,0x83,0xf8,0x33,0x75,5,0x8d,0x6f,0xfc,0xeb,2,0x33,0xed,
            0x8b,0x73,0x10,0x8d,0x4e,4,0x8d,0x44,0x24,0x18},35) &&
        call_exact(b,0x10d5ea,0x4bc0) &&
        bytes(b,0x10d5ef,(const uint8_t[]){0x8b,0x44,0x24,0x20},4) &&
        bytes(b,DESTROY_NATIVE,destroy,sizeof(destroy)) &&
        bytes(b,0x1bb26b,dispatch,sizeof(dispatch)) && bytes(b,0x1bb283,deletion,sizeof(deletion)) &&
        readable(b+TASK_VT,8) && *(void **)(b+TASK_VT)==b+DESTROY_NATIVE &&
        *(void **)(b+TASK_VT+4)==b+RUN_NATIVE &&
        /* Retail's pre-CreateThread sentinel is -1; NULL means creation
         * failed, not proof of suspended pre-worker startup. */
        readable(b+WORKER_HANDLE,4) && *(HANDLE *)(b+WORKER_HANDLE)==INVALID_HANDLE_VALUE &&
        readable(b+WORLD,4) && !*(void **)(b+WORLD) &&
        readable(b+ACTIVE,8) && !*(void **)(b+ACTIVE) && !*(void **)(b+QUEUE);
}
static BOOL hooks_exact(void) {
    int32_t offset=0;
    if(base && readable(base+ENQUEUE_SITE,5)) memcpy(&offset,base+ENQUEUE_SITE+1,4);
    return installed && enqueue_hook.installed && run_hook.installed && destroy_hook.installed &&
        publish_hook.installed && bytes(base,PUBLISH_SITE,publish_hook.replacement,publish_hook.length) &&
        readable(base+ENQUEUE_SITE,5) && base[ENQUEUE_SITE]==0xe8 &&
        offset==enqueue_hook.replacement_displacement &&
        *(void **)run_hook.slot==run_hook.replacement_value &&
        *(void **)destroy_hook.slot==destroy_hook.replacement_value;
}
static Watch *ticket_find(uint64_t ticket) {
    if(ticket) for(unsigned i=0;i<WATCHES;++i) if(watches[i].value.ticket==ticket) return &watches[i];
    return NULL;
}
static Watch *request_find(const void *request,BOOL submitting) {
    for(unsigned i=0;i<WATCHES;++i) {
        Watch *w=&watches[i];
        if(w->value.ticket && w->request==request &&
            (submitting?w->value.submitting:(w->value.phase>=SUDEKIMP_AREA_JOB_SUBMITTED &&
                w->value.phase<SUDEKIMP_AREA_JOB_DESTROYED))) return w;
    }
    return NULL;
}
static BOOL worker_adopt(void) {
    HANDLE h=*(HANDLE *)(base+WORKER_HANDLE);
    if(worker_copy) return h==worker_native && GetThreadId(worker_copy)==worker_thread;
    HANDLE copy=NULL;
    if(!h || h==INVALID_HANDLE_VALUE || !DuplicateHandle(GetCurrentProcess(),h,GetCurrentProcess(),&copy,
        SYNCHRONIZE|THREAD_QUERY_LIMITED_INFORMATION,FALSE,0)) return FALSE;
    DWORD id=GetThreadId(copy);
    if(!id || id==GetCurrentThreadId() || h!=*(HANDLE *)(base+WORKER_HANDLE)) {
        CloseHandle(copy); return FALSE;
    }
    worker_copy=copy; worker_native=h; worker_thread=id; return TRUE;
}
static BOOL text_same(const char *p,const char *want) {
    if(!p || !want) return FALSE;
    for(unsigned i=0;i<SUDEKIMP_STORY_AREA_NAME;++i) {
        if(!readable(p+i,1)) return FALSE;
        unsigned char c=(unsigned char)p[i];
        if(c>='A' && c<='Z') c=(unsigned char)(c-'A'+'a');
        if(c!=(unsigned char)want[i]) return FALSE;
        if(!c) return i!=0;
    }
    return FALSE;
}
static BOOL name_readable(const char *p) {
    if(!p) return FALSE;
    for(unsigned i=0;i<SUDEKIMP_STORY_AREA_NAME;++i) {
        if(!readable(p+i,1)) return FALSE;
        unsigned char c=(unsigned char)p[i];
        if(!c) return i!=0;
        if(!((c>='a'&&c<='z') || (c>='A'&&c<='Z') || (c>='0'&&c<='9') || c=='_' || c=='-'))
            return FALSE;
    }
    return FALSE;
}
static BOOL descriptor_capture(Watch *w,const char *name) {
    const uint8_t *world=*(uint8_t **)(base+WORLD),*d=w->descriptor;
    if(!readable(world,0x390) || *(void *const *)world!=base+0x2c4c3c) return FALSE;
    const uint8_t *table=*(uint8_t *const *)(world+0x50);
    unsigned count=*(unsigned *)(world+0x54);
    uintptr_t first=(uintptr_t)table,p=(uintptr_t)d;
    if(!count || count>4096 || !readable(table,(size_t)count*0x54) || p<first ||
        (p-first)%0x54 || (p-first)/0x54>=count ||
        *(void *const *)d!=base+0x2c82a4 || *(unsigned *)(d+0x34)!=0 ||
        !text_same(*(const char *const *)(d+0x24),name)) return FALSE;
    unsigned matches=0;
    for(unsigned i=0;i<count;++i) {
        const uint8_t *row=table+i*0x54;
        const char *other=*(const char *const *)(row+0x24);
        if(*(void *const *)row!=base+0x2c82a4 || !name_readable(other)) return FALSE;
        if(text_same(other,name)) ++matches;
    }
    if(matches!=1 || world!=*(void **)(base+WORLD) || table!=*(void *const *)(world+0x50) ||
        count!=*(unsigned *)(world+0x54)) return FALSE;
    w->world=world; w->table=table; w->count=count; return TRUE;
}
static BOOL birth_exact(const Watch *w,const uint8_t *request) {
    const uint8_t *world=w->world,*d=w->descriptor;
    return readable(request,0x14) && *(void *const *)request==base+TASK_VT &&
        *(uint16_t *)(request+8)==0x401 && *(unsigned *)(request+0xc)==0x41 &&
        *(void *const *)(request+0x10)==d && readable(world,0x58) &&
        world==*(void **)(base+WORLD) && w->table==*(void *const *)(world+0x50) &&
        w->count==*(unsigned *)(world+0x54) && readable(d,0x54) &&
        *(void *const *)d==base+0x2c82a4 && *(unsigned *)(d+0x34)==1;
}
/* Worker callbacks compare opaque request identities only. They do not read
 * the request/descriptor (which may already be freed), call game methods or
 * touch the game-thread area policy. The native original keeps its own thread. */
static void __attribute__((used,noinline)) event(unsigned kind,void *request,unsigned flags) {
    DWORD saved=GetLastError(),thread=GetCurrentThreadId();
    AcquireSRWLockExclusive(&lock);
    if(!(kind&1)) ++callbacks;
    if(!unknown && base) {
        Watch *w=NULL;
        if(kind==ENQUEUE_BEGIN) {
            if(!producer_thread) producer_thread=thread;
            if(producer_thread!=thread || !readable(request,0x14)) unknown=TRUE;
            else {
                const void *d=*(void **)((uint8_t *)request+0x10);
                for(unsigned i=0;i<WATCHES;++i) if(watches[i].value.ticket && watches[i].descriptor==d) {
                    w=&watches[i]; break;
                }
                if(w) {
                    if(w->value.phase!=SUDEKIMP_AREA_JOB_ARMED || !birth_exact(w,request) ||
                        request_find(request,FALSE)) unknown=TRUE;
                    else {w->request=request; w->value.phase=SUDEKIMP_AREA_JOB_SUBMITTED; w->value.submitting=TRUE;}
                }
            }
        } else if(kind==ENQUEUE_END) {
            w=request_find(request,TRUE);
            if(producer_thread!=thread) unknown=TRUE;
            else if(w) w->value.submitting=FALSE;
        } else if((w=request_find(request,FALSE))!=NULL) {
            BOOL worker=worker_copy && thread==worker_thread &&
                *(void **)(base+ACTIVE)==request;
            switch(kind) {
            case RUN_BEGIN:
                if(!worker || w->value.phase!=SUDEKIMP_AREA_JOB_SUBMITTED) unknown=TRUE;
                else {w->value.phase=SUDEKIMP_AREA_JOB_RUNNING; w->value.executed=TRUE;}
                break;
            case RUN_END:
                if(!worker || w->value.phase!=SUDEKIMP_AREA_JOB_RUNNING) unknown=TRUE;
                else w->value.phase=SUDEKIMP_AREA_JOB_RETURNED;
                break;
            case DESTROY_BEGIN:
                if(flags!=1 || !((worker && w->value.phase==SUDEKIMP_AREA_JOB_RETURNED) ||
                    (thread==producer_thread && w->value.phase==SUDEKIMP_AREA_JOB_SUBMITTED &&
                     *(void **)(base+ACTIVE)!=request))) unknown=TRUE;
                else w->value.phase=SUDEKIMP_AREA_JOB_DESTROYING;
                break;
            case DESTROY_END:
                if(flags!=1 || w->value.phase!=SUDEKIMP_AREA_JOB_DESTROYING ||
                    (w->value.executed?!worker:thread!=producer_thread)) unknown=TRUE;
                else w->value.phase=SUDEKIMP_AREA_JOB_DESTROYED;
                break;
            default: unknown=TRUE; break;
            }
        }
    }
    if(kind&1) {if(callbacks) --callbacks; else unknown=TRUE;}
    ReleaseSRWLockExclusive(&lock); SetLastError(saved);
}
/* Native stores have completed before this receipt is copied. No descriptor,
 * resource, interface or request is dereferenced here, including on a worker.
 * The enclosing Run bridge owns callback lifetime. Capture the generation now,
 * under the resource owner's lock, never by resolving this address later. */
static void __attribute__((used,noinline)) publication(void *request,void *descriptor,
    uintptr_t resource_interface,uintptr_t resource) {
    DWORD saved=GetLastError(),thread=GetCurrentThreadId();
    AcquireSRWLockExclusive(&lock);
    if(base && !unknown) {
        Watch *w=request_find(request,FALSE);
        if(!w) {
            for(unsigned i=0;i<WATCHES;++i)
                if(watches[i].value.ticket && watches[i].descriptor==descriptor) unknown=TRUE;
        } else if(!callbacks || !worker_copy || thread!=worker_thread ||
            *(void **)(base+ACTIVE)!=request || w->value.phase!=SUDEKIMP_AREA_JOB_RUNNING ||
            w->descriptor!=descriptor || w->value.published || !resource_interface ||
            (resource && (resource>UINTPTR_MAX-4 || resource+4!=resource_interface))) unknown=TRUE;
        else {
            uint64_t generation=0;
            if(resource && !SudekiMpLanStoryAreaResourceCapture((HMODULE)base,resource,&generation)) unknown=TRUE;
            else {
                w->value.published=TRUE;
                w->value.published_descriptor=(uintptr_t)descriptor;
                w->value.published_interface=resource_interface;
                w->value.published_resource=resource;
                w->value.published_resource_generation=generation;
            }
        }
    }
    ReleaseSRWLockExclusive(&lock);SetLastError(saved);
}
#define SAVE "pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;" \
    "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld; sub $16,%esp;"
#define ARG "mov 36(%ebp),%eax; mov %eax,4(%esp);"
#define RESTORE "call _event; add $16,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl;"
static void __attribute__((naked,noinline)) enqueue_entry(void) {
    __asm__ volatile("push %eax;" SAVE ARG "movl $0,(%esp); movl $0,8(%esp);" RESTORE
        "call *_enqueue_original;" SAVE ARG "movl $1,(%esp); movl $0,8(%esp);" RESTORE
        "lea 4(%esp),%esp; ret");
}
static void __attribute__((naked,noinline)) run_entry(void) {
    __asm__ volatile("push %ecx;" SAVE ARG "movl $2,(%esp); movl $0,8(%esp);" RESTORE
        "call *_run_original;" SAVE ARG "movl $3,(%esp); movl $0,8(%esp);" RESTORE
        "lea 4(%esp),%esp; ret");
}
static void __attribute__((naked,noinline)) destroy_entry(void) {
    __asm__ volatile("push %ecx;" SAVE ARG "movl $4,(%esp); mov 44(%ebp),%eax; mov %eax,8(%esp);" RESTORE
        "push 8(%esp); call *_destroy_original;" SAVE ARG
        "movl $5,(%esp); mov 44(%ebp),%eax; mov %eax,8(%esp);" RESTORE
        "lea 4(%esp),%esp; ret $4");
}
static void __attribute__((naked,noinline)) publish_entry(void) {
    /* Exact displaced native instructions; MOV preserves incoming flags. */
    __asm__ volatile("mov %edi,0x10(%esi); mov 0x10(%ebx),%ecx; mov %ebp,0x18(%ecx);"
        SAVE "mov 16(%ebp),%eax; mov %eax,(%esp); mov 4(%ebp),%eax;"
        /* Both native stores must have addressed the armed descriptor. The
         * request reloads its descriptor between stores; compare the copied
         * register identities without another native-object read. */
        "cmp 24(%ebp),%eax; je 1f; xor %eax,%eax; 1: mov %eax,4(%esp);"
        "mov 0(%ebp),%eax; mov %eax,8(%esp); mov 8(%ebp),%eax; mov %eax,12(%esp);"
        "call _publication; add $16,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl;"
        "jmp *_publish_continue");
}
#undef SAVE
#undef ARG
#undef RESTORE
static BOOL healthy(HMODULE image) {
    if(!base || base!=(uint8_t *)image || !installed || unknown) return FALSE;
    if(!hooks_exact()) {unknown=TRUE; return FALSE;}
    return TRUE;
}
BOOL SudekiMpLanStoryAreaTaskArm(HMODULE image,const SudekiMpControlUpdateDispatchWitness *w,
    SudekiMpStoryAreas *policy,SudekiMpStoryAreaRef area,const void *descriptor,uint64_t *out) {
    if(!out || !policy || !descriptor || !w || !w->service_post_original_exact || !w->dispatch_serial ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) || area.slot>=SUDEKIMP_STORY_AREAS)
        return FALSE;
    AcquireSRWLockExclusive(&lock); BOOL ok=FALSE;
    DWORD thread=GetCurrentThreadId();
    if(healthy(image) && !stopping && (!producer_thread || producer_thread==thread) && serial!=UINT64_MAX &&
        SudekiMpLanStoryAreaResourceJournalReady(image) &&
        SudekiMpStoryAreaRefSame(policy->areas[area.slot].ref,area) &&
        policy->areas[area.slot].phase==SUDEKIMP_STORY_AREA_LOADING && worker_adopt() &&
        WaitForSingleObject(worker_copy,0)==WAIT_TIMEOUT) {
        Watch fresh={0}; fresh.descriptor=descriptor; fresh.policy=policy; fresh.value.area=area;
        const SudekiMpStoryAreaRecord *a=&policy->areas[area.slot];
        const char *name=a->temporary[0]?a->temporary:a->world;
        Watch *free_slot=NULL; BOOL duplicate=FALSE;
        for(unsigned i=0;i<WATCHES;++i) {
            if(!watches[i].value.ticket) {if(!free_slot) free_slot=&watches[i];}
            else if(watches[i].descriptor==descriptor || (watches[i].policy==policy &&
                SudekiMpStoryAreaRefSame(watches[i].value.area,area))) duplicate=TRUE;
        }
        if(free_slot && !duplicate && descriptor_capture(&fresh,name) &&
            SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) &&
            SudekiMpStoryAreaRetain(policy,area,&fresh.pin)) {
            producer_thread=thread; fresh.value.ticket=++serial; fresh.value.phase=SUDEKIMP_AREA_JOB_ARMED;
            *free_slot=fresh; *out=fresh.value.ticket; ok=TRUE;
        }
    }
    ReleaseSRWLockExclusive(&lock); return ok;
}
BOOL SudekiMpLanStoryAreaTaskRead(HMODULE image,uint64_t ticket,SudekiMpLanStoryAreaTaskReceipt *out) {
    if(!out) return FALSE;
    AcquireSRWLockExclusive(&lock); Watch *w=ticket_find(ticket);
    BOOL ok=healthy(image) && producer_thread==GetCurrentThreadId() && w;
    if(ok) *out=w->value;
    ReleaseSRWLockExclusive(&lock); return ok;
}
static BOOL release_watch(HMODULE image,uint64_t ticket,unsigned phase) {
    AcquireSRWLockExclusive(&lock); Watch *w=ticket_find(ticket);
    BOOL ok=healthy(image) && producer_thread==GetCurrentThreadId() && w &&
        w->value.phase==phase && !w->value.submitting;
    if(ok) {
        ok=SudekiMpStoryAreaRelease(w->policy,w->value.area,w->pin);
        if(ok) memset(w,0,sizeof(*w)); else unknown=TRUE;
    }
    ReleaseSRWLockExclusive(&lock); return ok;
}
BOOL SudekiMpLanStoryAreaTaskDisarm(HMODULE image,uint64_t ticket) {
    return release_watch(image,ticket,SUDEKIMP_AREA_JOB_ARMED);
}
BOOL SudekiMpLanStoryAreaTaskRelease(HMODULE image,uint64_t ticket) {
    return release_watch(image,ticket,SUDEKIMP_AREA_JOB_DESTROYED);
}
static BOOL pinned(DWORD error) {
    HMODULE self;
    (void)GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        (LPCSTR)(uintptr_t)&SudekiMpLanStoryAreaTaskUninstall,&self);
    SetLastError(error?error:ERROR_BUSY); return FALSE;
}
BOOL SudekiMpLanStoryAreaTaskUninstall(void) {
    AcquireSRWLockExclusive(&lock);
    if(!base) {ReleaseSRWLockExclusive(&lock); return TRUE;}
    stopping=TRUE; DWORD error=ERROR_SUCCESS,thread=GetCurrentThreadId(); BOOL queue_locked=FALSE;
    if((producer_thread?producer_thread:startup_thread)!=thread || callbacks || unknown) error=ERROR_BUSY;
    for(unsigned i=0;i<WATCHES;++i) if(watches[i].value.ticket) error=ERROR_BUSY;
    if(!error && *(HANDLE *)(base+WORKER_HANDLE)!=INVALID_HANDLE_VALUE) {
        if(!worker_adopt()) error=ERROR_BUSY;
        else {
            DWORD state=WaitForSingleObject(worker_copy,0);
            if(state==WAIT_TIMEOUT) {
                /* Native worker holds this lock while selecting and retiring
                 * requests. Nonblocking acquisition avoids lock inversion with
                 * its destructor callback. Active==NULL under the lock proves
                 * no worker has fetched a slot target but not entered us yet. */
                queue_locked=TryEnterCriticalSection((CRITICAL_SECTION *)(base+QUEUE_LOCK));
                if(!queue_locked || *(void **)(base+ACTIVE)) error=ERROR_BUSY;
            } else if(state!=WAIT_OBJECT_0 || *(void **)(base+ACTIVE)) error=ERROR_BUSY;
        }
    } else if(!error && (worker_copy || producer_thread || *(void **)(base+WORLD) ||
        *(void **)(base+QUEUE) || *(void **)(base+ACTIVE))) error=ERROR_BUSY;
    if(!error) {
        if(!SudekiMpRestoreRelativeCallHook(&enqueue_hook)) error=GetLastError();
        if(!SudekiMpRestoreInlineHook(&publish_hook) && !error) error=GetLastError();
        if(!SudekiMpRestorePointerHook(&run_hook) && !error) error=GetLastError();
        if(!SudekiMpRestorePointerHook(&destroy_hook) && !error) error=GetLastError();
    }
    if(queue_locked) LeaveCriticalSection((CRITICAL_SECTION *)(base+QUEUE_LOCK));
    if(!error && worker_copy && !CloseHandle(worker_copy)) error=GetLastError();
    if(!error) {
        installed=FALSE; base=NULL; worker_copy=worker_native=NULL;
        producer_thread=startup_thread=worker_thread=0;
        enqueue_original=run_original=destroy_original=NULL;
        publish_continue=NULL;
    }
    ReleaseSRWLockExclusive(&lock); return error?pinned(error):TRUE;
}
BOOL SudekiMpLanStoryAreaTaskInstall(HMODULE image) {
    AcquireSRWLockExclusive(&lock);
    if(base || installed || unknown || worker_copy || serial==UINT64_MAX || !image_exact(image)) {
        ReleaseSRWLockExclusive(&lock); SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    base=(uint8_t *)image; startup_thread=GetCurrentThreadId(); stopping=FALSE;
    enqueue_original=base+ENQUEUE_NATIVE; run_original=base+RUN_NATIVE; destroy_original=base+DESTROY_NATIVE;
    publish_continue=base+PUBLISH_SITE+sizeof(publish_bytes);
    BOOL ok=SudekiMpInstallPointerHook(&destroy_hook,(void **)(base+TASK_VT),destroy_original,
        (void *)(uintptr_t)destroy_entry) &&
        SudekiMpInstallPointerHook(&run_hook,(void **)(base+TASK_VT+4),run_original,(void *)(uintptr_t)run_entry) &&
        SudekiMpInstallInlineHook(&publish_hook,base+PUBLISH_SITE,publish_bytes,sizeof(publish_bytes),
            (void *)(uintptr_t)publish_entry) &&
        SudekiMpInstallRelativeCallHook(&enqueue_hook,base+ENQUEUE_SITE,enqueue_original,(void *)(uintptr_t)enqueue_entry);
    DWORD error=GetLastError(); if(ok) installed=TRUE;
    ReleaseSRWLockExclusive(&lock);
    if(!ok) {(void)SudekiMpLanStoryAreaTaskUninstall(); SetLastError(error);}
    return ok;
}
