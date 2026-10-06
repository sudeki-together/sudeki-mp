#include "hooks/lan_story_area_intent.h"
#include "hooks/call_hook.h"
#include "hooks/save_book_intercept.h"
#include "engine/build_identity.h"
#include <limits.h>
#include <string.h>
#if !defined(__GNUC__) || !defined(__i386__)
#error "Story intent admission requires the supported x86 ABI"
#endif
enum { ACTION=0x164b40,NATIVE_INPUT=0x164f80,CALL=0x164ff5,RETURN=0x164ffa,
    QUIT=0xa2740,QUIT_GATE=0xa2745,REMOVE=0x7950,REMOVE_ALL=0x5f70,
    WORLD=0x408d10,SCENE=0x408d1c };
typedef SudekiMpLanStoryAreaIntentReceipt Receipt;
static uint8_t *base;
static SudekiMpInlineHook action_hook,quit_hook,remove_hook;
static void *action_original __attribute__((used));
static void *quit_original __attribute__((used));
static void *remove_original __attribute__((used));
static SRWLOCK lock=SRWLOCK_INIT;
static const void *consumer;
static SudekiMpLanStoryAreaIntentAdmission admission;
static Receipt records[SUDEKIMP_AREA_INTENTS];
static uint64_t serial;
static DWORD startup_thread,native_thread;
static unsigned callbacks;
static BOOL installed,stopping,unknown,ever_native,dispatching;
static BOOL load_attempted;
static int load_owner;
static const uint8_t entry_bytes[]={0x55,0x8b,0xec,0x83,0xe4,0xf8};
static const uint8_t quit_bytes[]={0x8b,0x80,0x74,1,0,0};
static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m;uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || VirtualQuery(p,&m,sizeof(m))!=sizeof(m) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
        a+n>(uintptr_t)m.BaseAddress+m.RegionSize) return FALSE;
    DWORD access=m.Protect&0xffu;
    return access==PAGE_READONLY || access==PAGE_READWRITE || access==PAGE_WRITECOPY ||
        access==PAGE_EXECUTE_READ || access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
}
static BOOL bytes(const uint8_t *b,unsigned rva,const void *expected,size_t size) {
    return readable(b+rva,size) && !memcmp(b+rva,expected,size);
}
static BOOL image_exact(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    if(!readable(b,sizeof(IMAGE_DOS_HEADER))) return FALSE;
    IMAGE_DOS_HEADER *dos=(void *)b;
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 ||
        (uint32_t)dos->e_lfanew>SUDEKIMP_EXPECTED_IMAGE_SIZE-sizeof(IMAGE_NT_HEADERS32) ||
        !readable(b+dos->e_lfanew,sizeof(IMAGE_NT_HEADERS32)) || !SudekiMpCheckLoadedExecutable(image))
        return FALSE;
    static const uint8_t dispatch[]={0x81,0xec,0xa4,0,0,0,0x53,0x56,0x33,0xdb,0x2b,0xc3,0x57,0x8b,0xf9,
        0x0f,0x84,0xa6,1,0,0,0x48,0x0f,0x84,0x29,1,0,0,0x48,0x0f,0x85,0xf4,3,0,0};
    static const uint8_t selector[]={0x33,0xc0,0x83,0x7b,0x50,0x0a,0x0f,0x95,0xc0,0x48,
        0x83,0xe0,2,0x8b,0xcb};
    static const uint8_t tail[]={0x32,0xc0,0x5f,0x5e,0x5b,0x8b,0xe5,0x5d,0xc2,0x0c,0};
    static const uint8_t quit_tail[]={0x85,0xc0,0x74,8,0x8b,0x80,0x94,0,0,0,
        0xeb,2,0x33,0xc0,0x85,0xc0,0x74,5,0xe9,0xbe,0xfe,0xff,0xff,0xc3};
    uint8_t quit_head[]={0xa1,0,0,0,0};void *scene_slot=b+SCENE;
    memcpy(quit_head+1,&scene_slot,4);
    uint8_t remove_head[]={0xa1,0,0,0,0,0x50,0xe8};void *world_slot=b+WORLD;
    memcpy(remove_head+1,&world_slot,4);
    int32_t remove_offset;
    if(!readable(b+REMOVE,12)) return FALSE;
    memcpy(&remove_offset,b+REMOVE+7,4);
    int32_t displacement;
    if(!readable(b+CALL,5) || b[CALL]!=0xe8) return FALSE;
    memcpy(&displacement,b+CALL+1,4);
    return bytes(b,ACTION,entry_bytes,sizeof(entry_bytes)) && bytes(b,ACTION+6,dispatch,sizeof(dispatch)) &&
        bytes(b,NATIVE_INPUT,entry_bytes,sizeof(entry_bytes)) && bytes(b,0x164fe6,selector,sizeof(selector)) &&
        b+CALL+5+displacement==b+ACTION && bytes(b,RETURN,tail,sizeof(tail)) &&
        bytes(b,QUIT,quit_head,sizeof(quit_head)) && bytes(b,QUIT_GATE,quit_bytes,sizeof(quit_bytes)) &&
        bytes(b,QUIT_GATE+sizeof(quit_bytes),quit_tail,sizeof(quit_tail)) &&
        bytes(b,REMOVE,remove_head,sizeof(remove_head)) && b[REMOVE+11]==0xc3 &&
        b+REMOVE+11+remove_offset==b+REMOVE_ALL &&
        readable(b+WORLD,4) && !*(void **)(b+WORLD) &&
        readable(b+SCENE,4) && !*(void **)(b+SCENE);
}
static BOOL healthy(HMODULE image) {
    if(!base || base!=(uint8_t *)image || !installed || unknown) return FALSE;
    if(!action_hook.installed || !bytes(base,ACTION,action_hook.replacement,action_hook.length) ||
        !quit_hook.installed || !bytes(base,QUIT_GATE,quit_hook.replacement,quit_hook.length) ||
        !remove_hook.installed || !bytes(base,REMOVE,remove_hook.replacement,remove_hook.length) ||
        !SudekiMpSaveBookStoryLoadExact(image,&load_owner)) {
        unknown=TRUE;return FALSE;
    }
    return TRUE;
}
static BOOL boundary(const SudekiMpControlUpdateDispatchWitness *w) {
    return w && w->service_post_original_exact && w->dispatch_serial &&
        w->native_thread_id==GetCurrentThreadId() && (!native_thread || native_thread==GetCurrentThreadId()) &&
        !callbacks && !dispatching && SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w);
}
static BOOL pending(void) {
    for(unsigned i=0;i<SUDEKIMP_AREA_INTENTS;++i) if(records[i].ticket) return TRUE;
    return FALSE;
}
/* This is a synchronous native call boundary, not a retained menu identity.
 * Only copied selectors/return address enter C; no game object is read here.
 * callbacks spans the original action, including its reentrant script calls. */
static BOOL __attribute__((used,noinline)) intent_begin(unsigned selector,uintptr_t caller,unsigned source) {
    DWORD saved=GetLastError(),thread=GetCurrentThreadId();BOOL run=TRUE;
    AcquireSRWLockExclusive(&lock);ever_native=TRUE;
    if(callbacks==UINT_MAX) unknown=TRUE;else ++callbacks;
    if(!native_thread) native_thread=thread;
    else if(native_thread!=thread) unknown=TRUE;
    if(consumer && (selector==0 || selector==2 || source!=SUDEKIMP_AREA_INTENT_MENU)) {
        run=FALSE;Receipt *r=NULL;
        if(!healthy((HMODULE)base) || dispatching || thread!=native_thread ||
            (source==SUDEKIMP_AREA_INTENT_MENU && caller!=(uintptr_t)(base+RETURN)) ||
            (source!=SUDEKIMP_AREA_INTENT_MENU && source!=SUDEKIMP_AREA_INTENT_QUIT_EXPORT &&
             source!=SUDEKIMP_AREA_INTENT_LOAD_EXPORT && source!=SUDEKIMP_AREA_INTENT_REMOVE_EXPORT) ||
            serial==UINT64_MAX) unknown=TRUE;
        if(!unknown) {
            for(unsigned i=0;i<SUDEKIMP_AREA_INTENTS;++i) if(!records[i].ticket) {r=&records[i];break;}
            if(!r) unknown=TRUE;
        }
        if(!unknown) {
            *r=(Receipt){++serial,source==SUDEKIMP_AREA_INTENT_LOAD_EXPORT?SUDEKIMP_AREA_INTENT_LOAD_SAVE:
                (source==SUDEKIMP_AREA_INTENT_REMOVE_EXPORT?SUDEKIMP_AREA_INTENT_REMOVE_AREAS:
                 (selector==0?SUDEKIMP_AREA_INTENT_RELOAD:SUDEKIMP_AREA_INTENT_QUIT)),
                SUDEKIMP_AREA_INTENT_DISPATCHING,source,
                source==SUDEKIMP_AREA_INTENT_LOAD_EXPORT?(int32_t)selector:-1};
            Receipt copy=*r;const void *who=consumer;SudekiMpLanStoryAreaIntentAdmission fn=admission;
            dispatching=TRUE;ReleaseSRWLockExclusive(&lock);
            unsigned decision=fn(who,&copy);
            AcquireSRWLockExclusive(&lock);dispatching=FALSE;
            if(consumer!=who || admission!=fn || !healthy((HMODULE)base) ||
                r->ticket!=copy.ticket || r->phase!=SUDEKIMP_AREA_INTENT_DISPATCHING) unknown=TRUE;
            if(!unknown) {
                if(decision==SUDEKIMP_AREA_INTENT_RUN) {
                    for(unsigned i=0;i<SUDEKIMP_AREA_INTENTS;++i)
                        if(&records[i]!=r && records[i].ticket) unknown=TRUE;
                    if(!unknown) {memset(r,0,sizeof(*r));run=TRUE;}
                } else if(decision==SUDEKIMP_AREA_INTENT_DEFER) r->phase=SUDEKIMP_AREA_INTENT_DEFERRED;
                else unknown=TRUE;
            }
        }
    }
    ReleaseSRWLockExclusive(&lock);SetLastError(saved);return run;
}
static void __attribute__((used,noinline)) intent_end(void) {
    DWORD saved=GetLastError();AcquireSRWLockExclusive(&lock);
    if(callbacks) --callbacks;else unknown=TRUE;
    ReleaseSRWLockExclusive(&lock);SetLastError(saved);
}
static BOOL load_begin(const void *owner,int index) {
    (void)owner;return intent_begin((unsigned)index,0,SUDEKIMP_AREA_INTENT_LOAD_EXPORT);
}
static void load_end(const void *owner) {(void)owner;intent_end();}
#define SAVE "pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;" \
    "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld; sub $16,%esp;"
#define RESTORE "add $16,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl;"
static void __attribute__((naked,noinline)) action_entry(void) {
    __asm__ volatile(SAVE
        "mov 28(%ebp),%eax; mov %eax,(%esp); mov 36(%ebp),%eax; mov %eax,4(%esp);"
        "movl $1,8(%esp);"
        "call _intent_begin; test %eax,%eax; jz 1f;" RESTORE
        "call *_action_original;" SAVE "call _intent_end;" RESTORE "ret;"
        "1: call _intent_end;" RESTORE "ret;");
}
/* The public export has no stack arguments. Its first MOV of the global scene
 * pointer remains untouched so the existing lobby exit owner's identity check
 * still applies. No scene dereference, audio or world mutation precedes us.
 * The caller of this void export still returns normally after deferral; this
 * is not suspension of a script/C caller or replay of its earlier effects. */
static void __attribute__((naked,noinline)) quit_entry(void) {
    __asm__ volatile(SAVE
        "movl $2,(%esp); mov 36(%ebp),%eax; mov %eax,4(%esp); movl $2,8(%esp);"
        "call _intent_begin; test %eax,%eax; jz 1f;" RESTORE
        "call *_quit_original;" SAVE "call _intent_end;" RESTORE "ret;"
        "1: call _intent_end;" RESTORE "ret;");
}
/* No arguments and no native effects precede this public wrapper. Its original
 * world lookup and stdcall cleanup invocation execute only after RUN. A
 * deferred void return is not completion or a suspended calling script. */
static void __attribute__((naked,noinline)) remove_entry(void) {
    __asm__ volatile(SAVE
        "movl $0,(%esp); mov 36(%ebp),%eax; mov %eax,4(%esp); movl $4,8(%esp);"
        "call _intent_begin; test %eax,%eax; jz 1f;" RESTORE
        "call *_remove_original;" SAVE "call _intent_end;" RESTORE "ret;"
        "1: call _intent_end;" RESTORE "ret;");
}
#undef SAVE
#undef RESTORE
BOOL SudekiMpLanStoryAreaIntentAttach(HMODULE image,const void *who,
    const SudekiMpControlUpdateDispatchWitness *w,SudekiMpLanStoryAreaIntentAdmission fn) {
    AcquireSRWLockExclusive(&lock);
    /* Partial startup restoration deliberately leaves only some hooks installed.
     * Closed admission must reject before a health probe could quarantine that
     * retryable restore state as unexpected ownership loss. */
    BOOL ok=who && fn && !consumer && !pending() && !stopping && healthy(image) && boundary(w);
    if(ok) {native_thread=GetCurrentThreadId();consumer=who;admission=fn;}
    ReleaseSRWLockExclusive(&lock);return ok;
}
BOOL SudekiMpLanStoryAreaIntentSnapshot(HMODULE image,const void *who,
    const SudekiMpControlUpdateDispatchWitness *w,Receipt *out,unsigned capacity,unsigned *count) {
    if(!who || !out || !count) return FALSE;
    AcquireSRWLockExclusive(&lock);unsigned n=0;
    BOOL ok=consumer==who && healthy(image) && boundary(w);
    if(ok) {
        for(unsigned i=0;i<SUDEKIMP_AREA_INTENTS;++i) if(records[i].ticket) ++n;
        ok=n<=capacity;
        if(ok) {n=0;for(unsigned i=0;i<SUDEKIMP_AREA_INTENTS;++i) if(records[i].ticket) out[n++]=records[i];*count=n;}
    }
    ReleaseSRWLockExclusive(&lock);return ok;
}
BOOL SudekiMpLanStoryAreaIntentAcknowledge(HMODULE image,const void *who,
    const SudekiMpControlUpdateDispatchWitness *w,uint64_t ticket) {
    AcquireSRWLockExclusive(&lock);BOOL ok=FALSE;
    if(who && ticket && consumer==who && healthy(image) && boundary(w))
        for(unsigned i=0;i<SUDEKIMP_AREA_INTENTS;++i)
            if(records[i].ticket==ticket && records[i].phase==SUDEKIMP_AREA_INTENT_DEFERRED) {
                memset(&records[i],0,sizeof(records[i]));ok=TRUE;break;
            }
    ReleaseSRWLockExclusive(&lock);return ok;
}
BOOL SudekiMpLanStoryAreaIntentDetach(HMODULE image,const void *who,const SudekiMpControlUpdateDispatchWitness *w) {
    AcquireSRWLockExclusive(&lock);
    BOOL ok=who && consumer==who && healthy(image) && boundary(w) && !pending();
    if(ok) {consumer=NULL;admission=NULL;}
    ReleaseSRWLockExclusive(&lock);return ok;
}
static BOOL pinned(DWORD error) {
    HMODULE self;(void)GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        (LPCSTR)(uintptr_t)&SudekiMpLanStoryAreaIntentUninstall,&self);
    SetLastError(error?error:ERROR_BUSY);return FALSE;
}
BOOL SudekiMpLanStoryAreaIntentUninstall(void) {
    AcquireSRWLockExclusive(&lock);
    if(!base) {ReleaseSRWLockExclusive(&lock);return TRUE;}
    stopping=TRUE;DWORD error=0;
    if(ever_native || consumer || callbacks || dispatching || unknown || pending() ||
        GetCurrentThreadId()!=startup_thread || *(void **)(base+WORLD) || *(void **)(base+SCENE)) error=ERROR_BUSY;
    if(!error) {
        if(!SudekiMpRestoreInlineHook(&remove_hook)) {
            error=GetLastError();if(!error) error=ERROR_WRITE_FAULT;
        }
        if(load_attempted && !SudekiMpSaveBookStoryLoadUninstall(&load_owner)) {
            if(!error) {error=GetLastError();if(!error) error=ERROR_WRITE_FAULT;}
        }
        if(!SudekiMpRestoreInlineHook(&quit_hook) && !error) {error=GetLastError();if(!error) error=ERROR_WRITE_FAULT;}
        if(!SudekiMpRestoreInlineHook(&action_hook) && !error) {error=GetLastError();if(!error) error=ERROR_WRITE_FAULT;}
    }
    if(!error) {base=NULL;action_original=quit_original=remove_original=NULL;installed=load_attempted=FALSE;startup_thread=native_thread=0;}
    ReleaseSRWLockExclusive(&lock);return error?pinned(error):TRUE;
}
BOOL SudekiMpLanStoryAreaIntentInstall(HMODULE image) {
    AcquireSRWLockExclusive(&lock);
    if(base || installed || unknown || ever_native || serial==UINT64_MAX || !image_exact(image)) {
        ReleaseSRWLockExclusive(&lock);SetLastError(ERROR_INVALID_STATE);return FALSE;
    }
    base=(uint8_t *)image;startup_thread=GetCurrentThreadId();stopping=FALSE;
    BOOL ok=SudekiMpInstallInlineHook(&action_hook,base+ACTION,entry_bytes,sizeof(entry_bytes),(void *)(uintptr_t)action_entry);
    action_original=action_hook.trampoline;
    if(ok) {
        ok=SudekiMpInstallInlineHook(&quit_hook,base+QUIT_GATE,quit_bytes,sizeof(quit_bytes),(void *)(uintptr_t)quit_entry);
        quit_original=quit_hook.trampoline;
    }
    if(ok) {
        load_attempted=TRUE;
        ok=SudekiMpSaveBookStoryLoadInstall(image,&load_owner,load_begin,load_end);
    }
    if(ok) {
        uint8_t remove_bytes[]={0xa1,0,0,0,0};void *world_slot=base+WORLD;
        memcpy(remove_bytes+1,&world_slot,4);
        ok=SudekiMpInstallInlineHook(&remove_hook,base+REMOVE,remove_bytes,sizeof(remove_bytes),(void *)(uintptr_t)remove_entry);
        remove_original=remove_hook.trampoline;
    }
    DWORD error=GetLastError();if(ok) installed=TRUE;
    ReleaseSRWLockExclusive(&lock);
    if(!ok) {(void)SudekiMpLanStoryAreaIntentUninstall();SetLastError(error);}return ok;
}
