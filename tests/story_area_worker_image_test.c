/* Supported-image loader-worker experiment, NOT a production stop adapter.
 * Executes the native creator, two worker loops, enqueue/flush and I/O wait
 * pump with real test threads/events/critical sections. Tasks and I/O completion
 * are synthetic; no game resource, renderer, world, save or live process runs.
 * Stop writes and explicit wakeups below belong ONLY to this isolated fixture.
 * A signaled thread handle is not proof that every descendant task has drained. */
#include "engine/build_identity.h"
#include "hooks/call_hook.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(__GNUC__) || !defined(__i386__)
#error "This supported-image experiment requires x86 GCC"
#endif
enum { IO=0, REQUEST=1, RUN=0x3c30b2, CREATE=0x1bb2c0,
    ENQUEUE=0x1bb330, FLUSH=0x1bb400, PUMP_LOCK=0x36c6fc,
    IO_READY=0x36c718, IO_IDLE=0x36c714, IO_INITIALIZED=0x40f724 };
static const unsigned queue_rva[2]={0x3c30d8,0x3c30e0};
static const unsigned active_rva[2]={0x3c30d4,0x3c30dc};
static const unsigned lock_rva[2]={0x34e734,0x34e750};
static const unsigned wake_rva[2]={0x3d56cc,0x3d56d0};
static const unsigned handle_rva[2]={0x324004,0x324008};
static const unsigned entry_rva[2]={0x1bb100,0x1bb1e0};
static uint8_t *mapped;
static void *enqueue_entry __attribute__((used));
static HANDLE wake[2],native_handle[2],owned_handle[2],wait_entered[2];
static HANDLE run_entered[2],run_release[2],delete_entered[2],delete_release[2];
static HANDLE io_entered,io_release;
static DWORD native_id[2],main_thread;
static unsigned creates,priorities;
static volatile LONG io_calls,run_count[4],delete_count[4],cancel_count[4];
static void *tasks[4]; /* Comparison values only after VirtualFree. */
typedef struct Task {
    void **vtable;
    struct Task *next;
    uint16_t flags,pad;
    int priority;
    unsigned id;
} Task;
static void *task_vtable[2];
_Static_assert(offsetof(Task,next)==4 && offsetof(Task,flags)==8 && offsetof(Task,priority)==12,
    "native queue node offsets");
static void wait_for(HANDLE event) { assert(WaitForSingleObject(event,5000)==WAIT_OBJECT_0); }
static LONG count(volatile LONG *p) { return InterlockedCompareExchange(p,0,0); }
static CRITICAL_SECTION *queue_lock(unsigned which) { return (void *)(mapped+lock_rva[which]); }
static void stopped_request(void) {
    *(volatile uint8_t *)(mapped+RUN)=0;MemoryBarrier();
}
static void still_live(unsigned which) { assert(WaitForSingleObject(owned_handle[which],0)==WAIT_TIMEOUT); }
static void empty(unsigned which) {
    EnterCriticalSection(queue_lock(which));
    assert(!*(void **)(mapped+queue_rva[which]) && !*(void **)(mapped+active_rva[which]));
    LeaveCriticalSection(queue_lock(which));
}
static DWORD WINAPI single_wait(HANDLE handle,DWORD timeout) {
    if(timeout==INFINITE) {
        assert(handle==wake[REQUEST] && GetCurrentThreadId()==native_id[REQUEST]);
        assert(SetEvent(wait_entered[REQUEST]));
    } else assert(!timeout && handle==wake[IO]);
    return WaitForSingleObject(handle,timeout);
}
static DWORD WINAPI multiple_wait(DWORD number,const HANDLE *handles,BOOL all,DWORD timeout) {
    assert(number==2 && !all && timeout==INFINITE && GetCurrentThreadId()==native_id[IO]);
    assert(handles[0]==*(HANDLE *)(mapped+IO_READY) && handles[1]==wake[IO]);
    assert(SetEvent(wait_entered[IO]));
    return WaitForMultipleObjects(number,handles,all,timeout);
}
static HANDLE WINAPI create_thread(LPSECURITY_ATTRIBUTES security,SIZE_T stack,
    LPTHREAD_START_ROUTINE entry,void *argument,DWORD flags,LPDWORD id) {
    assert(creates<2 && !security && stack==0x3400 && !argument && !flags && !id);
    unsigned which=creates++;
    assert((uintptr_t)entry==(uintptr_t)(mapped+entry_rva[which]) && mapped[RUN]==1);
    /* Obtain the ID before letting the fixture's native entry execute. This
     * test-only startup barrier is not a runtime suspend/drain strategy. */
    HANDLE h=CreateThread(security,stack,entry,argument,CREATE_SUSPENDED,&native_id[which]);
    assert(h && native_id[which]);native_handle[which]=h;
    assert(ResumeThread(h)==1);return h;
}
static BOOL WINAPI priority(HANDLE handle,int value) {
    assert(priorities<2 && handle==native_handle[priorities++] && value==THREAD_PRIORITY_NORMAL);
    return SetThreadPriority(handle,value);
}
static void __cdecl thread_name(void) { /* Native debug naming only; deliberately omitted. */ }
static void __cdecl unexpected(void) { assert(!"Unexpected real resource dependency");abort(); }
static unsigned char __cdecl io_completion(void) {
    assert(GetCurrentThreadId()==native_id[IO]);
    InterlockedIncrement(&io_calls);assert(SetEvent(io_entered));wait_for(io_release);
    return 0; /* No synthetic completions remain; no real I/O is executed. */
}
static void __attribute__((thiscall)) task_run(Task *task) {
    unsigned id=task->id,which=id/2;
    assert(id<4 && tasks[id]==task && GetCurrentThreadId()==native_id[which]);
    assert(*(void **)(mapped+active_rva[which])==task);
    assert(InterlockedIncrement(&run_count[id])==1);
    if(!(id&1)) {assert(SetEvent(run_entered[which]));wait_for(run_release[which]);}
}
static void * __attribute__((thiscall)) task_delete(Task *task,unsigned flags) {
    unsigned id=task->id,which=id/2;void *address=task;
    assert(id<4 && tasks[id]==task && flags==1 && count(&delete_count[id])==0);
    BOOL cancelled=GetCurrentThreadId()==main_thread;
    if(cancelled) {assert(id&1);assert(!count(&run_count[id]));InterlockedIncrement(&cancel_count[id]);}
    else {
        assert(GetCurrentThreadId()==native_id[which] && count(&run_count[id])==1);
        assert(*(void **)(mapped+active_rva[which])==task);
    }
    /* Free before the destructor returns, while the worker's ACTIVE still
     * contains this address. Never inspect the task again after this line. */
    assert(VirtualFree(task,0,MEM_RELEASE));InterlockedIncrement(&delete_count[id]);
    if(!cancelled && !(id&1)) {assert(SetEvent(delete_entered[which]));wait_for(delete_release[which]);}
    return address;
}
static Task *new_task(unsigned id) {
    assert(id<4 && !tasks[id]);Task *task=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(task);*task=(Task){.vtable=task_vtable,.flags=id/2?0x401:1,.priority=0x41,.id=id};
    tasks[id]=task;return task;
}
static void __attribute__((naked,noinline)) enqueue(Task *task __attribute__((unused))) {
    __asm__ volatile("mov 4(%esp),%eax; jmp *_enqueue_entry");
}
static void flush_io(void) { ((void (__cdecl *)(void))(uintptr_t)(mapped+FLUSH))(); }
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(file!=INVALID_HANDLE_VALUE);DWORD size=GetFileSize(file,NULL),got=0;
    uint8_t *raw=malloc(size);assert(raw && ReadFile(file,raw,size,&got,NULL) && got==size);CloseHandle(file);
    IMAGE_DOS_HEADER *dos=(void *)raw;IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *b=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(b);memcpy(b,raw,nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *sections=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        IMAGE_SECTION_HEADER *s=&sections[i];
        assert(s->PointerToRawData<=size && s->SizeOfRawData<=size-s->PointerToRawData);
        assert(s->VirtualAddress<=nt->OptionalHeader.SizeOfImage &&
            s->SizeOfRawData<=nt->OptionalHeader.SizeOfImage-s->VirtualAddress);
        memcpy(b+s->VirtualAddress,raw+s->PointerToRawData,s->SizeOfRawData);
    }
    IMAGE_DATA_DIRECTORY reloc=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    uintptr_t delta=(uintptr_t)b-nt->OptionalHeader.ImageBase;
    for(unsigned offset=0;offset<reloc.Size;) {
        IMAGE_BASE_RELOCATION *block=(void *)(b+reloc.VirtualAddress+offset);
        assert(block->SizeOfBlock>=sizeof(*block) && block->SizeOfBlock<=reloc.Size-offset);
        uint16_t *items=(void *)(block+1);unsigned number=(block->SizeOfBlock-sizeof(*block))/2;
        for(unsigned i=0;i<number;++i) {
            unsigned type=items[i]>>12,rva=block->VirtualAddress+(items[i]&0xfff);
            assert(type==IMAGE_REL_BASED_ABSOLUTE || type==IMAGE_REL_BASED_HIGHLOW);
            if(type==IMAGE_REL_BASED_HIGHLOW) {
                assert(rva<=nt->OptionalHeader.SizeOfImage-4);*(uint32_t *)(b+rva)+=(uint32_t)delta;
            }
        }
        offset+=block->SizeOfBlock;
    }
    free(raw);return b;
}
static HANDLE event(BOOL manual) { HANDLE h=CreateEventA(NULL,manual,FALSE,NULL);assert(h);return h; }
static void start(void) {
    ((void (__cdecl *)(void))(uintptr_t)(mapped+CREATE))();
    assert(creates==2 && priorities==2 && mapped[RUN]==1);
    for(unsigned which=0;which<2;++which) {
        assert(*(HANDLE *)(mapped+handle_rva[which])==native_handle[which]);
        assert(DuplicateHandle(GetCurrentProcess(),native_handle[which],GetCurrentProcess(),&owned_handle[which],
            SYNCHRONIZE|THREAD_QUERY_LIMITED_INFORMATION,FALSE,0));
        assert(GetThreadId(owned_handle[which])==native_id[which]);wait_for(wait_entered[which]);
        still_live(which);empty(which);
    }
    assert(*(DWORD *)(mapped+0x36191c)==native_id[IO]);
    assert(mapped[IO_INITIALIZED] && *(HANDLE *)(mapped+IO_READY) && *(HANDLE *)(mapped+IO_IDLE));
}
static void join(unsigned which) {
    wait_for(owned_handle[which]);DWORD code=0xffffffff;
    assert(GetExitCodeThread(owned_handle[which],&code) && code==0);empty(which);
}
static void idle_case(void) {
    start();
    /* The queue locks can be acquired while I/O still owns its separate pump
     * lock. Neither an empty queue nor these locks establish worker drain. */
    assert(!TryEnterCriticalSection((void *)(mapped+PUMP_LOCK)));
    stopped_request();still_live(IO);still_live(REQUEST);
    assert(WaitForSingleObject(wake[IO],0)==WAIT_TIMEOUT && WaitForSingleObject(wake[REQUEST],0)==WAIT_TIMEOUT);
    assert(SetEvent(*(HANDLE *)(mapped+IO_READY)));wait_for(io_entered);
    assert(count(&io_calls)==1);empty(IO);still_live(IO);still_live(REQUEST);
    assert(!TryEnterCriticalSection((void *)(mapped+PUMP_LOCK)));
    assert(SetEvent(io_release));wait_for(wait_entered[IO]);
    still_live(IO);still_live(REQUEST);assert(SetEvent(wake[IO]));join(IO);
    still_live(REQUEST);assert(SetEvent(wake[REQUEST]));join(REQUEST);
    /* Native enqueue has no stop admission fence: work submitted AFTER both
     * threads exit is still linked/woken, but no worker can execute it. */
    for(unsigned which=0;which<2;++which) {
        unsigned id=which*2+1;enqueue(new_task(id));
        assert(*(void **)(mapped+queue_rva[which])==tasks[id]);
        assert(WaitForSingleObject(wake[which],0)==WAIT_OBJECT_0);
        assert(WaitForSingleObject(owned_handle[which],0)==WAIT_OBJECT_0);
        assert(!count(&run_count[id]) && !count(&delete_count[id]));
    }
    flush_io();assert(count(&cancel_count[1])==1 && !count(&delete_count[3]));
    /* Fixture-only cleanup of stranded request; NOT a native cancellation API. */
    Task *stranded=tasks[3];*(void **)(mapped+queue_rva[REQUEST])=NULL;task_delete(stranded,1);
    empty(IO);empty(REQUEST);
}
static void active_case(BOOL flush) {
    start();enqueue(new_task(0));enqueue(new_task(2));
    for(unsigned which=0;which<2;++which) {
        wait_for(run_entered[which]);assert(TryEnterCriticalSection(queue_lock(which)));
        assert(!*(void **)(mapped+queue_rva[which]));
        assert(*(void **)(mapped+active_rva[which])==tasks[which*2]);
        LeaveCriticalSection(queue_lock(which));still_live(which);
    }
    stopped_request();
    /* Even a stopped-requested worker drains newly queued nodes on this pass.
     * This does not prove arbitrary cross-worker child dependencies safe. */
    enqueue(new_task(1));enqueue(new_task(3));
    if(flush) {
        flush_io();assert(count(&cancel_count[1])==1 && !count(&run_count[1]));
        assert(!*(void **)(mapped+queue_rva[IO]) && *(void **)(mapped+queue_rva[REQUEST])==tasks[3]);
        for(unsigned which=0;which<2;++which) {
            assert(*(void **)(mapped+active_rva[which])==tasks[which*2]);still_live(which);
        }
    }
    for(unsigned which=0;which<2;++which) assert(SetEvent(run_release[which]));
    for(unsigned which=0;which<2;++which) {
        wait_for(delete_entered[which]);assert(count(&delete_count[which*2])==1);
        /* The task is already freed, but its destructor and worker remain
         * active under the queue lock. No native memory dereference here. */
        assert(!TryEnterCriticalSection(queue_lock(which)));still_live(which);
    }
    for(unsigned which=0;which<2;++which) assert(SetEvent(delete_release[which]));
    for(unsigned which=0;which<2;++which) join(which);
    for(unsigned id=0;id<4;++id) {
        assert(count(&delete_count[id])==1);
        assert(count(&run_count[id])==(flush && id==1?0:1));
        assert(count(&cancel_count[id])==(flush && id==1?1:0));
    }
    assert(!count(&io_calls));
}
int main(int argc,char **argv) {
    assert(argc==3);wchar_t path[1024];
    assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024));
    BOOL idle=!strcmp(argv[2],"idle"),flush=!strcmp(argv[2],"flush");
    assert(idle || flush || !strcmp(argv[2],"active"));
    mapped=map_image(path);main_thread=GetCurrentThreadId();enqueue_entry=mapped+ENQUEUE;
    task_vtable[0]=(void *)(uintptr_t)task_delete;task_vtable[1]=(void *)(uintptr_t)task_run;
    /* Native CRC-table setup is also executed; event/queue-lock initialization
     * normally performed by static constructors is supplied by this fixture. */
    static const unsigned ranges[][2]={{0x1bb100,0x33e},{0x1bb6e0,0x75},{0x1bcfa0,0x8e},{0x1ead20,0xf3}};
    uint8_t *before[4];DWORD protections[4],ignored;
    for(unsigned i=0;i<4;++i) {
        before[i]=malloc(ranges[i][1]);assert(before[i]);memcpy(before[i],mapped+ranges[i][0],ranges[i][1]);
        assert(VirtualProtect(mapped+ranges[i][0],ranges[i][1],PAGE_EXECUTE_READWRITE,&protections[i]));
    }
    static const unsigned slots[]={0x29a030,0x29a060,0x29a064,0x29a068,0x29a074,0x29a078,
        0x29a07c,0x29a080,0x29a0a4,0x29a140,0x29a180};
    void *replacements[]={(void *)(uintptr_t)multiple_wait,(void *)(uintptr_t)InitializeCriticalSection,
        (void *)(uintptr_t)LeaveCriticalSection,(void *)(uintptr_t)EnterCriticalSection,
        (void *)(uintptr_t)single_wait,(void *)(uintptr_t)SetEvent,(void *)(uintptr_t)CreateEventA,
        (void *)(uintptr_t)ResetEvent,(void *)(uintptr_t)priority,(void *)(uintptr_t)create_thread,
        (void *)(uintptr_t)GetCurrentThreadId};
    SudekiMpPointerHook imports[11]={{0}};void *slot_before[11];
    for(unsigned i=0;i<11;++i) {
        slot_before[i]=*(void **)(mapped+slots[i]);
        assert(SudekiMpInstallPointerHook(&imports[i],(void **)(mapped+slots[i]),slot_before[i],replacements[i]));
    }
    SudekiMpRelativeCallHook calls[4]={{0}};
    assert(SudekiMpInstallRelativeCallHook(&calls[0],mapped+0x1bb110,mapped+0x22ece0,(void *)(uintptr_t)thread_name));
    assert(SudekiMpInstallRelativeCallHook(&calls[1],mapped+0x1bb1ed,mapped+0x22ece0,(void *)(uintptr_t)thread_name));
    assert(SudekiMpInstallRelativeCallHook(&calls[2],mapped+0x1eadc0,mapped+0x1df8c0,(void *)(uintptr_t)io_completion));
    assert(SudekiMpInstallRelativeCallHook(&calls[3],mapped+0x1eadde,mapped+0x1d8be0,(void *)(uintptr_t)unexpected));
    for(unsigned which=0;which<2;++which) {
        assert(*(HANDLE *)(mapped+handle_rva[which])==INVALID_HANDLE_VALUE);
        assert(!*(void **)(mapped+queue_rva[which]) && !*(void **)(mapped+active_rva[which]));
        InitializeCriticalSection(queue_lock(which));wake[which]=event(TRUE);
        *(HANDLE *)(mapped+wake_rva[which])=wake[which];wait_entered[which]=event(FALSE);
        run_entered[which]=event(FALSE);run_release[which]=event(FALSE);
        delete_entered[which]=event(FALSE);delete_release[which]=event(FALSE);
    }
    assert(!mapped[IO_INITIALIZED] && !*(unsigned *)(mapped+0x404140));
    io_entered=event(FALSE);io_release=event(FALSE);
    assert(FlushInstructionCache(GetCurrentProcess(),mapped,SUDEKIMP_EXPECTED_IMAGE_SIZE));
    if(idle) idle_case();else active_case(flush);
    for(unsigned which=0;which<2;++which) {
        assert(WaitForSingleObject(owned_handle[which],0)==WAIT_OBJECT_0);
        assert(CloseHandle(native_handle[which]));native_handle[which]=NULL;
        /* Duplicate remains an exact kernel-object lifetime after original
         * handle closure; a numeric native handle alone is not that lease. */
        assert(GetThreadId(owned_handle[which])==native_id[which]);
        assert(WaitForSingleObject(owned_handle[which],0)==WAIT_OBJECT_0);
        assert(CloseHandle(owned_handle[which]));owned_handle[which]=NULL;
        assert(CloseHandle(wake[which]) && CloseHandle(wait_entered[which]));
        assert(CloseHandle(run_entered[which]) && CloseHandle(run_release[which]));
        assert(CloseHandle(delete_entered[which]) && CloseHandle(delete_release[which]));
        DeleteCriticalSection(queue_lock(which));
    }
    assert(CloseHandle(*(HANDLE *)(mapped+IO_READY)) && CloseHandle(*(HANDLE *)(mapped+IO_IDLE)));
    DeleteCriticalSection((void *)(mapped+PUMP_LOCK));assert(CloseHandle(io_entered) && CloseHandle(io_release));
    for(unsigned i=4;i>0;--i) assert(SudekiMpRestoreRelativeCallHook(&calls[i-1]));
    for(unsigned i=11;i>0;--i) assert(SudekiMpRestorePointerHook(&imports[i-1]));
    for(unsigned i=0;i<11;++i) assert(*(void **)(mapped+slots[i])==slot_before[i]);
    /* Reverse order matters: two ranges share a page. */
    for(unsigned i=4;i>0;--i) {
        unsigned j=i-1;assert(!memcmp(before[j],mapped+ranges[j][0],ranges[j][1]));free(before[j]);
        assert(VirtualProtect(mapped+ranges[j][0],ranges[j][1],protections[j],&ignored));
    }
    for(unsigned i=0;i<4;++i) {MEMORY_BASIC_INFORMATION m;
        assert(VirtualQuery(mapped+ranges[i][0],&m,sizeof(m))==sizeof(m) && m.Protect==PAGE_READWRITE);}
    assert(VirtualFree(mapped,0,MEM_RELEASE));
    printf("StoryAreaWorkerImageTest: PASS (%s; native threads/queues/pump, synthetic tasks; no live stop authority)\n",argv[2]);
    return 0;
}
