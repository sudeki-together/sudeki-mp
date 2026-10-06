#include "hooks/lan_story_collision_lifetime.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Story collision lifetime requires the supported retail x86 ABI"
#endif
enum { BIRTH=0x315bb,SOURCE_DELETE=0x31920,CELLS=16384 };
typedef struct Cell {const void *source;uint64_t incarnation;BOOL used,live;} Cell;
static Cell cells[CELLS];
static uint8_t *base;
static BOOL installed,stopping;
static DWORD startup_thread;
static volatile LONG native_thread,callbacks,unknown;
static uint64_t revision;
static const void *consumer;
static SudekiMpInlineHook birth_hook,delete_hook;
static void *birth_resume __attribute__((used)),*delete_resume __attribute__((used));
static const uint8_t birth_bytes[]={0x8b,0xc7,0x5f,0x5b,0x8b,0xe5,0x5d};
static const uint8_t delete_bytes[]={0x83,0xec,0x18,0x55,0x8b,0x6c,0x24,0x20};
static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || VirtualQuery(p,&m,sizeof(m))!=sizeof(m) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
        a+n>(uintptr_t)m.BaseAddress+m.RegionSize) return FALSE;
    DWORD access=m.Protect&0xffu;
    return access==PAGE_READONLY || access==PAGE_READWRITE || access==PAGE_WRITECOPY ||
        access==PAGE_EXECUTE_READ || access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
}
static BOOL bytes(uint8_t *b,unsigned rva,const void *p,size_t n) {
    return readable(b+rva,n) && !memcmp(b+rva,p,n);
}
static BOOL image_exact(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    if(!readable(b,sizeof(IMAGE_DOS_HEADER))) return FALSE;
    const IMAGE_DOS_HEADER *dos=(void *)b;
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 ||
        (uint32_t)dos->e_lfanew>SUDEKIMP_EXPECTED_IMAGE_SIZE-sizeof(IMAGE_NT_HEADERS32) ||
        !readable(b+dos->e_lfanew,sizeof(IMAGE_NT_HEADERS32)) ||
        !SudekiMpCheckLoadedExecutable(image)) return FALSE;
    static const uint8_t factory[]={0x55,0x8b,0xec,0x83,0xe4,0xf8,0x83,0xec,0x10,
        0x53,0x57,0x68,0x10,1,0,0,0xe8,0x22,0x79,0x21,0};
    static const uint8_t init[]={0xe8,0x3d,0x75,0x19,0,0x89,0x98,0,1,0,0};
    static const uint8_t ret8[]={0xc2,8,0};
    static const uint8_t free_tail[]={0x56,0xe8,0x0b,0x74,0x21,0,0x83,0xc4,4,0x5f,0x5e,0x5d,
        0x83,0xc4,0x18,0xc2,8,0};
    return bytes(b,0x314e0,factory,sizeof(factory)) && bytes(b,0x314fe,init,sizeof(init)) &&
        bytes(b,BIRTH,birth_bytes,sizeof(birth_bytes)) && bytes(b,BIRTH+7,ret8,sizeof(ret8)) &&
        bytes(b,SOURCE_DELETE,delete_bytes,sizeof(delete_bytes)) && bytes(b,0x319cc,free_tail,sizeof(free_tail)) &&
        readable(b+0x408dd4,4) && !*(void **)(b+0x408dd4) &&
        readable(b+0x408d10,4) && !*(void **)(b+0x408d10);
}
static BOOL hooks_exact(void) {
    return installed && birth_hook.installed && delete_hook.installed &&
        bytes(base,BIRTH,birth_hook.replacement,birth_hook.length) &&
        bytes(base,SOURCE_DELETE,delete_hook.replacement,delete_hook.length);
}
static Cell *find(const void *source,BOOL vacant) {
    uintptr_t key=(uintptr_t)source; unsigned start=(unsigned)((key>>4)^(key>>13))&(CELLS-1);
    Cell *empty=NULL;
    for(unsigned n=0;n<CELLS;++n) {
        Cell *c=&cells[(start+n)&(CELLS-1)];
        if(c->used && c->source==source) return c;
        if(!c->live && !empty) empty=c;
        if(!c->used) break;
    }
    return vacant?empty:NULL;
}
static BOOL owner(void) {
    DWORD thread=GetCurrentThreadId();
    LONG prior=InterlockedCompareExchange(&native_thread,(LONG)thread,0);
    if(prior && (DWORD)prior!=thread) {InterlockedExchange(&unknown,1);return FALSE;}
    return TRUE;
}
static void __attribute__((used,noinline)) observed(void *source,BOOL birth) {
    DWORD saved=GetLastError(); InterlockedIncrement(&callbacks);
    if(owner() && !InterlockedCompareExchange(&unknown,0,0)) {
        Cell *c=source?find(source,birth):NULL;
        if(!installed || !c || (birth?c->live:!c->live) || revision==UINT64_MAX)
            InterlockedExchange(&unknown,1);
        else {
            ++revision;
            if(birth) *c=(Cell){.source=source,.incarnation=revision,.used=TRUE,.live=TRUE};
            else c->live=FALSE; /* Invalid before deregistration, cleanup and free. */
        }
    }
    InterlockedDecrement(&callbacks); SetLastError(saved);
}
#define SAVE_STATE "pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;" \
    "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld; sub $16,%esp;"
#define RESTORE_STATE "call _observed; add $16,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl;"
static void __attribute__((naked,noinline)) birth_entry(void) {
    __asm__ volatile(SAVE_STATE "mov 0(%ebp),%eax; mov %eax,(%esp); movl $1,4(%esp);"
        RESTORE_STATE "mov %edi,%eax; pop %edi; pop %ebx; mov %ebp,%esp; pop %ebp; jmp *_birth_resume");
}
static void __attribute__((naked,noinline)) delete_entry(void) {
    __asm__ volatile(SAVE_STATE "mov 44(%ebp),%eax; mov %eax,(%esp); movl $0,4(%esp);"
        RESTORE_STATE "sub $0x18,%esp; push %ebp; mov 0x20(%esp),%ebp; jmp *_delete_resume");
}
#undef SAVE_STATE
#undef RESTORE_STATE
static BOOL access_thread(BOOL startup) {
    DWORD thread=GetCurrentThreadId(),native=(DWORD)InterlockedCompareExchange(&native_thread,0,0);
    return native?native==thread:(startup && startup_thread==thread);
}
static BOOL healthy(void) {
    return installed && !stopping && !InterlockedCompareExchange(&unknown,0,0) &&
        !InterlockedCompareExchange(&callbacks,0,0);
}
static BOOL pin(DWORD error) {
    HMODULE self;
    (void)GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        (LPCSTR)(uintptr_t)&SudekiMpLanStoryCollisionLifetimeUninstall,&self);
    SetLastError(error?error:ERROR_BUSY);return FALSE;
}
BOOL SudekiMpLanStoryCollisionLifetimeUninstall(void) {
    if(!base) return TRUE;
    if(!access_thread(TRUE) || consumer || InterlockedCompareExchange(&callbacks,0,0) ||
        InterlockedCompareExchange(&unknown,0,0)) return pin(ERROR_BUSY);
    stopping=TRUE;
    DWORD error=ERROR_SUCCESS;
    if(!SudekiMpRestoreInlineHook(&birth_hook)) error=GetLastError();
    if(!SudekiMpRestoreInlineHook(&delete_hook) && !error) error=GetLastError();
    if(error) return pin(error);
    installed=FALSE; base=NULL; birth_resume=delete_resume=NULL; startup_thread=0;
    InterlockedExchange(&native_thread,0); memset(cells,0,sizeof(cells)); return TRUE;
}
BOOL SudekiMpLanStoryCollisionLifetimeInstall(HMODULE image) {
    if(base || installed || birth_hook.installed || delete_hook.installed || consumer ||
        InterlockedCompareExchange(&unknown,0,0) || revision==UINT64_MAX || !image_exact(image)) {
        SetLastError(ERROR_INVALID_STATE);return FALSE;
    }
    base=(uint8_t *)image; startup_thread=GetCurrentThreadId(); ++revision;
    birth_resume=base+BIRTH+7; delete_resume=base+SOURCE_DELETE+8;
    if(!SudekiMpInstallInlineHook(&delete_hook,base+SOURCE_DELETE,delete_bytes,sizeof(delete_bytes),
        (const void *)(uintptr_t)delete_entry) ||
        !SudekiMpInstallInlineHook(&birth_hook,base+BIRTH,birth_bytes,sizeof(birth_bytes),
        (const void *)(uintptr_t)birth_entry)) {
        DWORD error=GetLastError(); (void)SudekiMpLanStoryCollisionLifetimeUninstall();
        SetLastError(error);return FALSE;
    }
    stopping=FALSE;installed=TRUE;return TRUE;
}
BOOL SudekiMpLanStoryCollisionLifetimeOwnerThread(HMODULE image) {
    return base && base==(uint8_t *)image && access_thread(TRUE);
}
BOOL SudekiMpLanStoryCollisionLifetimeAttach(HMODULE image,const void *who) {
    if(!who || consumer || base!=(uint8_t *)image || !healthy() || !access_thread(TRUE) || !hooks_exact()) return FALSE;
    consumer=who;return TRUE;
}
BOOL SudekiMpLanStoryCollisionLifetimeDetach(HMODULE image,const void *who) {
    if(!who || consumer!=who || base!=(uint8_t *)image || !access_thread(TRUE) ||
        InterlockedCompareExchange(&callbacks,0,0)) return FALSE;
    consumer=NULL;return TRUE;
}
BOOL SudekiMpLanStoryCollisionLifetimeRevision(HMODULE image,uint64_t *out) {
    if(!out || base!=(uint8_t *)image || !healthy() || !access_thread(FALSE) || !hooks_exact()) return FALSE;
    *out=revision;return TRUE;
}
BOOL SudekiMpLanStoryCollisionLifetimeObserve(HMODULE image,const void *source,SudekiMpLanStoryCollisionStamp *out) {
    uint64_t current;
    if(!source || !out || !SudekiMpLanStoryCollisionLifetimeRevision(image,&current)) return FALSE;
    const Cell *c=find(source,FALSE);if(!c || !c->live) return FALSE;
    *out=(SudekiMpLanStoryCollisionStamp){current,c->incarnation};return TRUE;
}
BOOL SudekiMpLanStoryCollisionLifetimeMatches(const void *source,SudekiMpLanStoryCollisionStamp stamp) {
    if(!source || !stamp.revision || !stamp.incarnation || !healthy() || !access_thread(FALSE) ||
        stamp.revision!=revision) return FALSE;
    const Cell *c=find(source,FALSE);
    return c && c->live && c->incarnation==stamp.incarnation;
}
