#include "hooks/lan_story_collision_query.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include <math.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Story collision query requires the supported retail x86 ABI"
#endif
enum { ADMISSION=0x1c89d1,RESUME=0x1c89da,NEXT=0x1c89e6,
    QUERY=0x1c90a0,MAX_NATIVE_CANDIDATES=500 };
static uint8_t *base;
static DWORD native_thread,startup_thread;
static BOOL lifetime_attached,stopping;
static BOOL installed,active,incomplete,normal_return;
static SudekiMpInlineHook admission_hook,completion_hook;
static void *admit_resume __attribute__((used)),*skip_resume __attribute__((used));
static void *complete_resume __attribute__((used));
typedef uint32_t (__stdcall *NativeQuery)(void *,const float *,float,void *,void *);
static NativeQuery native_query;
static struct {
    SudekiMpStoryAreas *policy;
    SudekiMpStoryAreaRef areas[SUDEKIMP_STORY_AREAS],query_area;
    uint64_t pins[SUDEKIMP_STORY_AREAS];
    void *grid,**grid_items;
    uint32_t count,capacity;
    uint64_t lifetime_revision;
    SudekiMpLanStoryCollisionSourceArea sources[SUDEKIMP_STORY_COLLISION_QUERY_SOURCES];
} scope;
static const uint8_t admission_bytes[]={0x8b,0x4e,0x10,0x81,0xf9,0xf4,1,0,0};
static const uint8_t completion_bytes[]={0x8b,0x47,0x10,0x5f,0x5e,0x5d,0x5b};
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
static BOOL exact_image(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    if(!readable(b,sizeof(IMAGE_DOS_HEADER))) return FALSE;
    const IMAGE_DOS_HEADER *dos=(void *)b;
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 ||
        (uint32_t)dos->e_lfanew>SUDEKIMP_EXPECTED_IMAGE_SIZE-sizeof(IMAGE_NT_HEADERS32) ||
        !readable(b+dos->e_lfanew,sizeof(IMAGE_NT_HEADERS32)) ||
        !SudekiMpCheckLoadedExecutable(image)) return FALSE;
    static const uint8_t entry[]={0x83,0xec,0x14,0x53,0x55,0x57,0x56,0xe8,4,0xff,0xff,0xff};
    static const uint8_t loop[]={0x8b,0,0x85,0xc0,0x75,0xe5,0x8b,0x4c,0x24,0x28};
    static const uint8_t finish[]={0x8b,0x46,0x10,0xdd,0xda,0x5f,0xdd,0xd8,0x5d,
        0xdd,0xd8,0x5b,0x83,0xc4,0x14,0xc2,0x0c,0};
    static const uint8_t query_start[]={0x81,0xec,0xc0,0,0,0,0xd9,0x84,0x24,0xcc,0,0,0};
    static const uint8_t query_call[]={0xe8,0x89,0xf7,0xff,0xff};
    static const uint8_t query_end[]={0x8b,0x47,0x10,0x5f,0x5e,0x5d,0x5b,0x81,0xc4,0xc0,0,0,0,0xc2,0x14,0};
    uint8_t append[]={0x7d,0x10,0x89,4,0x8d,0,0,0,0,1,0x7e,0x10};
    uint32_t p=(uint32_t)(uintptr_t)(b+0x362660); memcpy(append+5,&p,4);
    return bytes(b,ADMISSION,admission_bytes,sizeof(admission_bytes)) &&
        bytes(b,RESUME,append,sizeof(append)) && bytes(b,NEXT,loop,sizeof(loop)) &&
        bytes(b,0x1c8880,entry,sizeof(entry)) && bytes(b,0x1c8a23,finish,sizeof(finish)) &&
        bytes(b,QUERY,query_start,sizeof(query_start)) && bytes(b,0x1c90f2,query_call,sizeof(query_call)) &&
        bytes(b,0x1c96ff,query_end,sizeof(query_end));
}
static BOOL hook_exact(void) {
    return installed && admission_hook.installed && completion_hook.installed &&
        bytes(base,ADMISSION,admission_hook.replacement,admission_hook.length) &&
        bytes(base,0x1c96ff,completion_hook.replacement,completion_hook.length);
}
static int source_index(const void *p) {
    uint32_t lo=0,hi=scope.count; uintptr_t key=(uintptr_t)p;
    while(lo<hi) {
        uint32_t middle=lo+(hi-lo)/2; uintptr_t value=(uintptr_t)scope.sources[middle].source;
        if(value<key) lo=middle+1; else hi=middle;
    }
    return lo<scope.count && scope.sources[lo].source==p?(int)lo:-1;
}
static BOOL area_ready(SudekiMpStoryAreaRef r) {
    return scope.policy && r.slot<SUDEKIMP_STORY_AREAS &&
        r.session==scope.policy->session &&
        SudekiMpStoryAreaRefSame(scope.policy->areas[r.slot].ref,r) &&
        scope.policy->areas[r.slot].phase==SUDEKIMP_STORY_AREA_READY;
}
/* No callback/native dereference/allocator inside this hot seam. Unknown is
 * retained and marks the query incomplete, never silently treated as absent. */
static int __attribute__((used,noinline)) skip_source(void *grid,void *source,const void *caller) {
    DWORD saved=GetLastError(); int skip=0;
    if(active && GetCurrentThreadId()==native_thread) {
        int index=source_index(source);
        if(caller!=base+0x1c90f7 || grid!=scope.grid || index<0) incomplete=TRUE;
        else skip=!SudekiMpStoryAreaRefSame(scope.areas[scope.sources[index].area_slot],scope.query_area);
    }
    SetLastError(saved); return skip;
}
static void __attribute__((naked,noinline)) admission_entry(void) {
    /* Three x87 registers are live in 5C8880. Save ALL x87/SSE state, provide
     * an empty/masked x87 stack to C, and restore native state before either
     * continuation. PUSHAD's unused saved-ESP slot holds the branch result. */
    __asm__ volatile("pushfl; cmpl $0,_active; jne 2f; popfl;"
        "mov 0x10(%esi),%ecx; cmp $500,%ecx; jmp *_admit_resume;"
        "2: popfl; pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;"
        "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld;"
        "sub $16,%esp; mov 4(%ebp),%eax; mov %eax,(%esp);"
        "mov 28(%ebp),%eax; mov %eax,4(%esp); mov 68(%ebp),%eax; mov %eax,8(%esp);"
        "call _skip_source; add $16,%esp;"
        "mov %eax,12(%ebp); fxrstor (%esp); mov %ebp,%esp; cmpl $0,12(%esp); jne 1f;"
        "popal; popfl; mov 0x10(%esi),%ecx; cmp $500,%ecx; jmp *_admit_resume;"
        "1: popal; popfl; jmp *_skip_resume");
}
static void __attribute__((used,noinline)) completed(void) {
    DWORD saved=GetLastError();
    if(active && native_thread==GetCurrentThreadId()) normal_return=TRUE;
    SetLastError(saved);
}
static void __attribute__((naked,noinline)) completion_entry(void) {
    /* 5C970F and 5C9726 return partial geometry after the native 1000-entry
     * scratch limit. Only the normal epilogue proves full enumeration. */
    __asm__ volatile("pushfl; cmpl $0,_active; jne 1f; popfl;"
        "mov 0x10(%edi),%eax; pop %edi; pop %esi; pop %ebp; pop %ebx; jmp *_complete_resume;"
        "1: popfl; pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;"
        "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld;"
        "call _completed; fxrstor (%esp); mov %ebp,%esp;"
        "popal; popfl; mov 0x10(%edi),%eax; pop %edi; pop %esi; pop %ebp; pop %ebx;"
        "jmp *_complete_resume");
}
static BOOL retained(void) {
    for(unsigned i=0;i<SUDEKIMP_STORY_AREAS;++i) if(scope.pins[i]) return TRUE;
    return FALSE;
}
static BOOL release_scope(void) {
    BOOL ok=TRUE;
    for(unsigned i=SUDEKIMP_STORY_AREAS;i>0;--i) if(scope.pins[i-1]) {
        if(SudekiMpStoryAreaRelease(scope.policy,scope.areas[i-1],scope.pins[i-1])) scope.pins[i-1]=0;
        else ok=FALSE;
    }
    if(ok) memset(&scope,0,sizeof(scope));
    return ok;
}
BOOL SudekiMpLanStoryCollisionQueryUninstall(void) {
    if(!base) return TRUE;
    BOOL owner=lifetime_attached?SudekiMpLanStoryCollisionLifetimeOwnerThread((HMODULE)base):
        startup_thread==GetCurrentThreadId();
    if(!owner || active) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    stopping=TRUE;
    DWORD error=release_scope()?ERROR_SUCCESS:ERROR_INVALID_STATE;
    if(!SudekiMpRestoreInlineHook(&admission_hook) && !error) error=GetLastError();
    if(!SudekiMpRestoreInlineHook(&completion_hook) && !error) error=GetLastError();
    if(error) { SetLastError(error); return FALSE; }
    if(lifetime_attached && !SudekiMpLanStoryCollisionLifetimeDetach((HMODULE)base,&admission_hook)) return FALSE;
    lifetime_attached=FALSE;
    installed=FALSE; base=NULL; native_query=NULL;
    admit_resume=skip_resume=complete_resume=NULL; native_thread=startup_thread=0; return TRUE;
}
BOOL SudekiMpLanStoryCollisionQueryInstall(HMODULE image) {
    if(base || installed || admission_hook.installed || completion_hook.installed || retained() || !exact_image(image)) {
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    base=(uint8_t *)image; startup_thread=GetCurrentThreadId();
    if(!SudekiMpLanStoryCollisionLifetimeAttach(image,&admission_hook)) {
        (void)SudekiMpLanStoryCollisionQueryUninstall(); return FALSE;
    }
    lifetime_attached=TRUE;
    admit_resume=base+RESUME; skip_resume=base+NEXT;
    complete_resume=base+0x1c9706;
    native_query=(NativeQuery)(uintptr_t)(base+QUERY);
    if(!SudekiMpInstallInlineHook(&completion_hook,base+0x1c96ff,completion_bytes,
        sizeof(completion_bytes),(const void *)(uintptr_t)completion_entry) ||
        !SudekiMpInstallInlineHook(&admission_hook,base+ADMISSION,admission_bytes,
        sizeof(admission_bytes),(const void *)(uintptr_t)admission_entry)) {
        DWORD error=GetLastError(); (void)SudekiMpLanStoryCollisionQueryUninstall();
        SetLastError(error); return FALSE;
    }
    stopping=FALSE; installed=TRUE; return TRUE;
}
static BOOL grid_exact(void) {
    const uint8_t *g=scope.grid;
    if(!readable(g,0x15ac) || *(void *const *)(g+0x15a0)!=scope.grid_items ||
        *(const uint32_t *)(g+0x15a4)!=scope.count ||
        *(const uint32_t *)(g+0x15a8)!=scope.capacity ||
        !readable(scope.grid_items,scope.count*sizeof(void *))) return FALSE;
    uint8_t seen[SUDEKIMP_STORY_COLLISION_QUERY_SOURCES]={0};
    for(unsigned i=0;i<scope.count;++i) {
        const uint8_t *source=scope.grid_items[i]; int index=source_index(source);
        if(index<0 || seen[index] || !readable(source,0x110) ||
            *(void *const *)(source+8)!=scope.grid || source[0xf2]<1 || source[0xf2]>2) return FALSE;
        seen[index]=1;
    }
    return TRUE;
}
static BOOL prepare(SudekiMpStoryAreas *policy,const SudekiMpLanStoryCollisionQuerySet *set,
    SudekiMpStoryAreaRef query_area,void *query_source) {
    if(!policy || !set || !set->grid || !set->count ||
        set->count>SUDEKIMP_STORY_COLLISION_QUERY_SOURCES ||
        !readable(set->sources,set->count*sizeof(*set->sources)) || retained()) return FALSE;
    scope.policy=policy; scope.grid=set->grid; scope.count=set->count; scope.query_area=query_area;
    scope.lifetime_revision=set->lifetime_revision;
    memcpy(scope.areas,set->areas,sizeof(scope.areas));
    memcpy(scope.sources,set->sources,set->count*sizeof(*set->sources));
    if(!area_ready(query_area)) return FALSE;
    for(unsigned i=0;i<scope.count;++i) {
        unsigned slot=scope.sources[i].area_slot;
        SudekiMpLanStoryCollisionStamp stamp={scope.lifetime_revision,scope.sources[i].incarnation};
        if(!SudekiMpLanStoryCollisionLifetimeMatches(scope.sources[i].source,stamp)) return FALSE;
        if(!scope.sources[i].source || slot>=SUDEKIMP_STORY_AREAS || scope.areas[slot].slot!=slot ||
            !area_ready(scope.areas[slot]) || (i &&
            (uintptr_t)scope.sources[i-1].source>=(uintptr_t)scope.sources[i].source)) return FALSE;
    }
    int own=source_index(query_source);
    if(own<0 || !SudekiMpStoryAreaRefSame(scope.areas[scope.sources[own].area_slot],query_area) ||
        !readable(scope.grid,0x15ac)) return FALSE;
    scope.grid_items=*(void ***)((uint8_t *)scope.grid+0x15a0);
    scope.capacity=*(uint32_t *)((uint8_t *)scope.grid+0x15a8);
    if(scope.capacity<scope.count || scope.capacity>SUDEKIMP_STORY_COLLISION_QUERY_SOURCES || !grid_exact()) return FALSE;
    for(unsigned i=0;i<scope.count;++i) {
        unsigned slot=scope.sources[i].area_slot;
        if(!scope.pins[slot] && !SudekiMpStoryAreaRetain(policy,scope.areas[slot],&scope.pins[slot])) return FALSE;
    }
    return TRUE;
}
SudekiMpLanStoryCollisionQueryResult SudekiMpLanStoryCollisionQueryRun(SudekiMpStoryAreas *policy,
    const SudekiMpLanStoryCollisionQuerySet *set,SudekiMpStoryAreaRef query_area,void *query_source,
    const float position[3],float radius,void *hit_buffer,uint32_t *hits) {
    if(!hits || !hit_buffer || !readable(position,3*sizeof(float)) || !isfinite(radius) || radius<0 ||
        !isfinite(position[0]) || !isfinite(position[1]) || !isfinite(position[2]) ||
        (native_thread && native_thread!=GetCurrentThreadId()) || stopping || active || retained() || !hook_exact())
        return SUDEKIMP_STORY_COLLISION_NOT_RUN;
    uint64_t current;
    if(!set || !SudekiMpLanStoryCollisionLifetimeRevision((HMODULE)base,&current) ||
        current!=set->lifetime_revision) return SUDEKIMP_STORY_COLLISION_NOT_RUN;
    native_thread=GetCurrentThreadId();
    if(!prepare(policy,set,query_area,query_source)) {
        (void)release_scope(); return SUDEKIMP_STORY_COLLISION_NOT_RUN;
    }
    incomplete=FALSE; normal_return=FALSE; active=TRUE;
    uint32_t result=native_query(scope.grid,position,radius,query_source,hit_buffer);
    active=FALSE;
    BOOL complete=!incomplete && normal_return &&
        SudekiMpLanStoryCollisionLifetimeRevision((HMODULE)base,&current) && current==scope.lifetime_revision &&
        hook_exact() && grid_exact() &&
        *(uint32_t *)((uint8_t *)scope.grid+0x10)<MAX_NATIVE_CANDIDATES;
    for(unsigned i=0;i<SUDEKIMP_STORY_AREAS;++i) if(scope.pins[i] && !area_ready(scope.areas[i])) complete=FALSE;
    if(!release_scope()) complete=FALSE;
    if(!complete) return SUDEKIMP_STORY_COLLISION_INCOMPLETE;
    *hits=result; return SUDEKIMP_STORY_COLLISION_COMPLETE;
}
