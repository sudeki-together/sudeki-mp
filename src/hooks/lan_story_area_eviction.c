#include "hooks/lan_story_area_eviction.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include <string.h>
#if !defined(__GNUC__) || !defined(__i386__)
#error "Area eviction reservations require the supported x86 ABI"
#endif
enum { LIMIT=8, WORLD=0x408d10, WORLD_VT=0x2c4c3c, DESC_VT=0x2c82a4,
    CHANGE_STATE=0x10ad70, FIRST=0x5aba, NEIGHBOR=0x5b01, NAMED=0x7bc8,
    NONACTIVE=0x61c7, NONACTIVE_NEXT=0x635b, LAST_NEIGHBOR=0x7c13, LAST_ROOT=0x7c1e };
typedef struct Reservation {
    uint64_t ticket,pin;
    SudekiMpStoryAreas *policy;
    SudekiMpStoryAreaRef area;
    const uint8_t *world,*table,*descriptor;
    unsigned count;
    char name[SUDEKIMP_STORY_AREA_NAME];
} Reservation;
static Reservation reservations[LIMIT];
static SudekiMpRelativeCallHook first_hook,neighbor_hook,named_hook;
static SudekiMpRelativeCallHook last_neighbor_hook,last_root_hook;
static SudekiMpInlineHook nonactive_hook;
static SRWLOCK lock=SRWLOCK_INIT;
static uint8_t *base;
static void *original __attribute__((used));
static void *nonactive_original __attribute__((used)),*nonactive_next __attribute__((used));
static DWORD startup_thread,native_thread;
static uint64_t serial;
static BOOL installed,stopping,unknown;
static unsigned callbacks;
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
static BOOL call_exact(const uint8_t *b,unsigned at,unsigned target) {
    int32_t d;
    if(!readable(b+at,5) || b[at]!=0xe8) return FALSE;
    memcpy(&d,b+at+1,4); return b+at+5+d==b+target;
}
static BOOL image_exact(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    if(!readable(b,sizeof(IMAGE_DOS_HEADER))) return FALSE;
    IMAGE_DOS_HEADER *dos=(void *)b;
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 ||
        (uint32_t)dos->e_lfanew>SUDEKIMP_EXPECTED_IMAGE_SIZE-sizeof(IMAGE_NT_HEADERS32) ||
        !readable(b+dos->e_lfanew,sizeof(IMAGE_NT_HEADERS32)) ||
        !SudekiMpCheckLoadedExecutable(image)) return FALSE;
    static const uint8_t arguments[]={0x50,0x33,0xc0};
    static const uint8_t state_entry[]={0x55,0x8b,0xec,0x83,0xe4,0xf8,0x81,0xec,0x34,2,0,0,
        0x53,0x8b,0x5d,8,0x56,0x8b,0x73,0x34,0x57,0x8b,0xf8};
    static const uint8_t tail[]={0x5f,0x5e,0x5b,0x8b,0xe5,0x5d,0xc2,4,0};
    static const uint8_t next_first[]={0x8b,0x9b,0x94,3,0,0,0x33,0xf6};
    static const uint8_t next_neighbor[]={0x46,0xeb,0xc7};
    /* Exported UnloadZone performs only lookup then state-zero admission.
     * Unlike destructor internals, skipping this call leaves no subsequent
     * pointer clearing/free in the wrapper. No other teardown is intercepted. */
    static const uint8_t named_entry[]={0x8b,0x44,0x24,4,0x8b,0x0d};
    static const uint8_t named_lookup[]={0x50,0x51};
    static const uint8_t named_candidate[]={0x85,0xc0,0x74,8,0x50,0x33,0xc0};
    static const uint8_t named_return[]={0xc3};
    /* RemoveLastActiveZone walks the last area's neighbors (except the current
     * area), then requests state zero for the last area itself. Both calls are
     * before native state/resource mutations; the wrapper has no pending-slot
     * clear or other cleanup after them. The current-area exemption applies
     * only to neighbors, not the final root request. Keep native loop behavior. */
    static const uint8_t last_entry[]={0x55,0x8b,0xec,0x83,0xe4,0xf8,0x51,0xa1};
    static const uint8_t last_candidates[]={0x53,0x8b,0x58,0x0c,0x56,0x57,
        0x8b,0xb8,0x94,3,0,0,0x85,0xff,0x74,0x37,0x33,0xf6,0x8b,0xff,
        0x8b,0x47,0x3c,0x8b,0xc8,0x3b,0xf1,0x72,4,0x3b,0xc1,0x73,0x1e,
        0x3b,0xf0,0x0f,0x94,0xc0,0x84,0xc0,0x75,0x15,0x8b,0x47,0x44,
        0x8b,4,0xb0,0x3b,0xc3,0x74,8,0x50,0x33,0xc0};
    static const uint8_t last_advance[]={0x46,0xeb,0xd5,0x57,0x33,0xc0};
    static const uint8_t last_return[]={0x5f,0x5e,0x5b,0x8b,0xe5,0x5d,0xc3};
    /* Before the first per-descriptor mutation in RemoveAllNonActiveZones.
     * The loop's native state3 exemption is insufficient for retained state4
     * interiors. Skip only that descriptor, then use native loop advancement.
     * The function's pending-slot clears and global mode toggle are NOT owned
     * by this reservation: the coordinator must retain those contexts too. */
    static const uint8_t nonactive_candidate[]={0x8b,0x58,0x50,0x03,0x5c,0x24,0x0c,
        0xb9,3,0,0,0,0x39,0x4b,0x34,0x0f,0x84,0x86,1,0,0,
        0x8b,0x73,0x34,0x3b,0xf7,0x0f,0x84,0x7b,1,0,0};
    static const uint8_t nonactive_advance[]={0x83,0x44,0x24,0x0c,0x54,
        0xff,0x4c,0x24,0x14,0x0f,0x85,0x56,0xfe,0xff,0xff,
        0x89,0x78,0x18,0x89,0x78,0x14,0x5f,0x5e,0xc6,0x80,0x9d,3,0,0,1,
        0x5b,0x8b,0xe5,0x5d,0xc2,4,0};
    return bytes(b,FIRST-3,arguments,sizeof(arguments)) &&
        bytes(b,NEIGHBOR-3,arguments,sizeof(arguments)) &&
        call_exact(b,FIRST,CHANGE_STATE) && call_exact(b,NEIGHBOR,CHANGE_STATE) &&
        bytes(b,FIRST+5,next_first,sizeof(next_first)) &&
        bytes(b,NEIGHBOR+5,next_neighbor,sizeof(next_neighbor)) &&
        bytes(b,0x7bb0,named_entry,sizeof(named_entry)) && readable(b+0x7bb6,4) &&
        *(void **)(b+0x7bb6)==b+WORLD && bytes(b,0x7bba,named_lookup,sizeof(named_lookup)) &&
        call_exact(b,0x7bbc,0x59b0) && bytes(b,0x7bc1,named_candidate,sizeof(named_candidate)) &&
        call_exact(b,NAMED,CHANGE_STATE) && bytes(b,NAMED+5,named_return,sizeof(named_return)) &&
        bytes(b,0x7bd0,last_entry,sizeof(last_entry)) && readable(b+0x7bd8,4) &&
        *(void **)(b+0x7bd8)==b+WORLD && bytes(b,0x7bdc,last_candidates,sizeof(last_candidates)) &&
        call_exact(b,LAST_NEIGHBOR,CHANGE_STATE) && bytes(b,LAST_NEIGHBOR+5,last_advance,sizeof(last_advance)) &&
        call_exact(b,LAST_ROOT,CHANGE_STATE) && bytes(b,LAST_ROOT+5,last_return,sizeof(last_return)) &&
        bytes(b,CHANGE_STATE,state_entry,sizeof(state_entry)) && bytes(b,0x10b2b9,tail,sizeof(tail)) &&
        bytes(b,NONACTIVE-7,nonactive_candidate,sizeof(nonactive_candidate)) &&
        bytes(b,NONACTIVE_NEXT,nonactive_advance,sizeof(nonactive_advance)) &&
        readable(b+WORLD,4) && !*(void **)(b+WORLD);
}
static BOOL hooked(const SudekiMpRelativeCallHook *h,unsigned at) {
    int32_t d;
    if(!h->installed || !readable(base+at,5) || base[at]!=0xe8) return FALSE;
    memcpy(&d,base+at+1,4); return d==h->replacement_displacement;
}
static BOOL healthy(HMODULE image) {
    if(!base || base!=(uint8_t *)image || !installed || unknown) return FALSE;
    if(!hooked(&first_hook,FIRST) || !hooked(&neighbor_hook,NEIGHBOR) ||
        !hooked(&named_hook,NAMED) || !hooked(&last_neighbor_hook,LAST_NEIGHBOR) ||
        !hooked(&last_root_hook,LAST_ROOT) ||
        !nonactive_hook.installed || !readable(base+NONACTIVE,nonactive_hook.length) ||
        memcmp(base+NONACTIVE,nonactive_hook.replacement,nonactive_hook.length)) {
        unknown=TRUE;return FALSE;
    }
    return TRUE;
}
static BOOL witness_exact(const SudekiMpControlUpdateDispatchWitness *w) {
    return w && w->service_post_original_exact && w->dispatch_serial &&
        SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w);
}
static BOOL name_copy(char out[SUDEKIMP_STORY_AREA_NAME],const char *p) {
    if(!p) return FALSE;
    for(unsigned i=0;i<SUDEKIMP_STORY_AREA_NAME;++i) {
        if(!readable(p+i,1)) return FALSE;
        unsigned char c=(unsigned char)p[i];
        if(c>='A'&&c<='Z') c=(unsigned char)(c-'A'+'a');
        if(!c) {out[i]=0;return i!=0;}
        if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-')) return FALSE;
        out[i]=(char)c;
    }
    return FALSE;
}
static BOOL capture(Reservation *r) {
    const uint8_t *w=*(uint8_t **)(base+WORLD),*d=r->descriptor;
    if(!readable(w,0x398) || *(void *const *)w!=base+WORLD_VT) return FALSE;
    const uint8_t *table=*(uint8_t *const *)(w+0x50);
    unsigned count=*(unsigned *)(w+0x54);
    uintptr_t p=(uintptr_t)d,start=(uintptr_t)table;
    if(!count || count>4096 || !readable(table,(size_t)count*0x54) || p<start ||
        (p-start)%0x54 || (p-start)/0x54>=count || *(void *const *)d!=base+DESC_VT ||
        *(unsigned *)(d+0x34)>4) return FALSE;
    unsigned matches=0; char copy[SUDEKIMP_STORY_AREA_NAME];
    for(unsigned i=0;i<count;++i) {
        const uint8_t *row=table+i*0x54;
        if(*(void *const *)row!=base+DESC_VT ||
            !name_copy(copy,*(const char *const *)(row+0x24))) return FALSE;
        if(!strcmp(copy,r->name)) {if(row!=d)return FALSE;++matches;}
    }
    if(matches!=1 || w!=*(void **)(base+WORLD) || table!=*(void *const *)(w+0x50) ||
        count!=*(unsigned *)(w+0x54)) return FALSE;
    r->world=w;r->table=table;r->count=count;return TRUE;
}
static BOOL still(const Reservation *r) {
    /* No full-table scan on the eviction seam. The coordinator retains table
     * allocation/generation; this fresh check detects identity loss, not ABA
     * lifetime. It must not be used as a substitute for native ownership. */
    char name[SUDEKIMP_STORY_AREA_NAME];
    return *(void **)(base+WORLD)==r->world && readable(r->world,0x58) &&
        *(void *const *)r->world==base+WORLD_VT &&
        *(void *const *)(r->world+0x50)==r->table && *(unsigned *)(r->world+0x54)==r->count &&
        readable(r->descriptor,0x54) && *(void *const *)r->descriptor==base+DESC_VT &&
        *(unsigned *)(r->descriptor+0x34)<=4 &&
        name_copy(name,*(const char *const *)(r->descriptor+0x24)) && !strcmp(name,r->name);
}
/* C is entered with a clean floating-point environment. Borrowed native
 * identities are read only on the reserved controller thread. Unknown owner
 * state conservatively stops these descriptor evictions and retains pins; it
 * does not report a successful room load or alter any native completion bit. */
static BOOL __attribute__((used,noinline)) hold(void *descriptor,unsigned state) {
    DWORD saved=GetLastError(); BOOL result=FALSE,any=FALSE;
    AcquireSRWLockExclusive(&lock);++callbacks;
    DWORD thread=GetCurrentThreadId();
    if(!native_thread) native_thread=thread;
    else if(native_thread!=thread) unknown=TRUE;
    for(unsigned i=0;i<LIMIT;++i) if(reservations[i].ticket) any=TRUE;
    if(any) {
        if(unknown || GetCurrentThreadId()!=native_thread || state!=0 ||
            !healthy((HMODULE)base)) unknown=TRUE;
        if(!unknown) for(unsigned i=0;i<LIMIT;++i) if(reservations[i].ticket) {
            if(!still(&reservations[i])) {unknown=TRUE;break;}
            if(reservations[i].descriptor==descriptor) result=TRUE;
        }
        if(unknown) result=TRUE;
    }
    --callbacks;ReleaseSRWLockExclusive(&lock);SetLastError(saved);return result;
}
static void __attribute__((naked,noinline)) entry(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;"
        "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld; sub $16,%esp;"
        "mov 40(%ebp),%eax; mov %eax,(%esp); mov 28(%ebp),%eax; mov %eax,4(%esp);"
        "call _hold; test %eax,%eax; jz 1f;"
        "add $16,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl; ret $4;"
        "1: add $16,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl; jmp *_original");
}
static void __attribute__((naked,noinline)) nonactive_entry(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;"
        "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld; sub $16,%esp;"
        "mov 16(%ebp),%eax; mov %eax,(%esp); movl $0,4(%esp); call _hold;"
        "test %eax,%eax; jz 1f;"
        "add $16,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl;"
        "mov $3,%ecx; jmp *_nonactive_next;"
        "1: add $16,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl; jmp *_nonactive_original");
}
BOOL SudekiMpLanStoryAreaEvictionReserve(HMODULE image,const SudekiMpControlUpdateDispatchWitness *w,
    SudekiMpStoryAreas *policy,SudekiMpStoryAreaRef area,const void *descriptor,uint64_t *out) {
    if(!out || !policy || !descriptor || area.slot>=SUDEKIMP_STORY_AREAS || !witness_exact(w)) return FALSE;
    AcquireSRWLockExclusive(&lock);BOOL ok=FALSE;
    if(healthy(image) && !stopping && !callbacks && serial!=UINT64_MAX &&
        (!native_thread || native_thread==GetCurrentThreadId()) &&
        SudekiMpStoryAreaRefSame(policy->areas[area.slot].ref,area) &&
        (policy->areas[area.slot].phase==SUDEKIMP_STORY_AREA_LOADING ||
         policy->areas[area.slot].phase==SUDEKIMP_STORY_AREA_READY)) {
        Reservation fresh={0},*free_slot=NULL;BOOL duplicate=FALSE;
        fresh.descriptor=descriptor;fresh.policy=policy;fresh.area=area;
        const SudekiMpStoryAreaRecord *a=&policy->areas[area.slot];
        BOOL named=name_copy(fresh.name,a->temporary[0]?a->temporary:a->world);
        for(unsigned i=0;i<LIMIT;++i) if(!reservations[i].ticket) {if(!free_slot)free_slot=&reservations[i];}
        else if(reservations[i].descriptor==descriptor || (reservations[i].policy==policy &&
            SudekiMpStoryAreaRefSame(reservations[i].area,area))) duplicate=TRUE;
        if(named && free_slot && !duplicate && capture(&fresh)) {
            for(unsigned i=0;i<LIMIT;++i) if(reservations[i].ticket &&
                (reservations[i].world!=fresh.world || reservations[i].table!=fresh.table ||
                 reservations[i].count!=fresh.count || !still(&reservations[i]))) duplicate=TRUE;
            if(!duplicate && witness_exact(w) && SudekiMpStoryAreaRetain(policy,area,&fresh.pin)) {
                native_thread=GetCurrentThreadId();fresh.ticket=++serial;*free_slot=fresh;*out=fresh.ticket;ok=TRUE;
            }
        }
    }
    ReleaseSRWLockExclusive(&lock);return ok;
}
BOOL SudekiMpLanStoryAreaEvictionRelease(HMODULE image,const SudekiMpControlUpdateDispatchWitness *w,uint64_t ticket) {
    if(!ticket || !witness_exact(w)) return FALSE;
    AcquireSRWLockExclusive(&lock);BOOL ok=FALSE;
    if(healthy(image) && !callbacks && native_thread==GetCurrentThreadId())
        for(unsigned i=0;i<LIMIT;++i) if(reservations[i].ticket==ticket) {
            Reservation *r=&reservations[i];
            if(!still(r)) unknown=TRUE;
            else if(SudekiMpStoryAreaRelease(r->policy,r->area,r->pin)) {memset(r,0,sizeof(*r));ok=TRUE;}
            else unknown=TRUE;
            break;
        }
    ReleaseSRWLockExclusive(&lock);return ok;
}
static BOOL pinned(DWORD error) {
    HMODULE self;
    (void)GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        (LPCSTR)(uintptr_t)&SudekiMpLanStoryAreaEvictionUninstall,&self);
    SetLastError(error?error:ERROR_BUSY);return FALSE;
}
BOOL SudekiMpLanStoryAreaEvictionUninstall(const SudekiMpControlUpdateDispatchWitness *w) {
    AcquireSRWLockExclusive(&lock);
    if(!base) {ReleaseSRWLockExclusive(&lock);return TRUE;}
    stopping=TRUE;DWORD error=ERROR_SUCCESS;
    if(callbacks || unknown || (native_thread?
        (native_thread!=GetCurrentThreadId() || !witness_exact(w)):
        (startup_thread!=GetCurrentThreadId() || *(void **)(base+WORLD)))) error=ERROR_BUSY;
    for(unsigned i=0;i<LIMIT;++i) if(reservations[i].ticket) error=ERROR_BUSY;
    if(!error) {
        if(!SudekiMpRestoreRelativeCallHook(&last_root_hook)) error=GetLastError();
        if(!SudekiMpRestoreRelativeCallHook(&last_neighbor_hook) && !error) error=GetLastError();
        if(!SudekiMpRestoreInlineHook(&nonactive_hook) && !error) error=GetLastError();
        if(!SudekiMpRestoreRelativeCallHook(&named_hook) && !error) error=GetLastError();
        if(!SudekiMpRestoreRelativeCallHook(&neighbor_hook) && !error) error=GetLastError();
        if(!SudekiMpRestoreRelativeCallHook(&first_hook) && !error) error=GetLastError();
    }
    if(!error) {
        base=NULL;original=nonactive_original=nonactive_next=NULL;
        installed=FALSE;startup_thread=native_thread=0;
    }
    ReleaseSRWLockExclusive(&lock);return error?pinned(error):TRUE;
}
BOOL SudekiMpLanStoryAreaEvictionInstall(HMODULE image) {
    AcquireSRWLockExclusive(&lock);
    if(base || installed || unknown || serial==UINT64_MAX || !image_exact(image)) {
        ReleaseSRWLockExclusive(&lock);SetLastError(ERROR_INVALID_STATE);return FALSE;
    }
    base=(uint8_t *)image;original=base+CHANGE_STATE;startup_thread=GetCurrentThreadId();stopping=FALSE;
    nonactive_next=base+NONACTIVE_NEXT;
    static const uint8_t candidate[]={0xb9,3,0,0,0};
    BOOL ok=SudekiMpInstallRelativeCallHook(&first_hook,base+FIRST,original,(void *)(uintptr_t)entry) &&
        SudekiMpInstallRelativeCallHook(&neighbor_hook,base+NEIGHBOR,original,(void *)(uintptr_t)entry) &&
        SudekiMpInstallRelativeCallHook(&named_hook,base+NAMED,original,(void *)(uintptr_t)entry) &&
        SudekiMpInstallInlineHook(&nonactive_hook,base+NONACTIVE,candidate,sizeof(candidate),
            (void *)(uintptr_t)nonactive_entry);
    nonactive_original=nonactive_hook.trampoline;
    if(ok) ok=SudekiMpInstallRelativeCallHook(&last_neighbor_hook,base+LAST_NEIGHBOR,original,(void *)(uintptr_t)entry);
    if(ok) ok=SudekiMpInstallRelativeCallHook(&last_root_hook,base+LAST_ROOT,original,(void *)(uintptr_t)entry);
    DWORD error=GetLastError();if(ok)installed=TRUE;
    ReleaseSRWLockExclusive(&lock);
    if(!ok) {(void)SudekiMpLanStoryAreaEvictionUninstall(NULL);SetLastError(error);}
    return ok;
}
