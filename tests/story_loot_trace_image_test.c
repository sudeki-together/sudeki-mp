/* Inert retail-image validation plus synthetic callbacks. No retail gameplay,
 * inventory grant, actor animation, native constructor or destructor executes. */
#include "hooks/call_hook.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
static unsigned install_step,fail_step;
static void **sabotage_slot;
static BOOL test_pointer(SudekiMpPointerHook *,void **,const void *,const void *);
static BOOL test_call(SudekiMpRelativeCallHook *,uint8_t *,const void *,const void *);
static BOOL test_inline(SudekiMpInlineHook *,uint8_t *,const uint8_t *,size_t,const void *);
#define SudekiMpInstallPointerHook test_pointer
#define SudekiMpInstallRelativeCallHook test_call
#define SudekiMpInstallInlineHook test_inline
#include "../src/hooks/lan_story_loot_trace.c"
#undef SudekiMpInstallPointerHook
#undef SudekiMpInstallRelativeCallHook
#undef SudekiMpInstallInlineHook
static BOOL fail_install(void) {
    if(++install_step==fail_step) {
        if(sabotage_slot) *sabotage_slot=sabotage_slot;
        SetLastError(ERROR_WRITE_FAULT);return TRUE;
    }
    return FALSE;
}
static BOOL test_pointer(SudekiMpPointerHook *h,void **p,const void *a,const void *b) {
    return !fail_install() && SudekiMpInstallPointerHook(h,p,a,b);
}
static BOOL test_call(SudekiMpRelativeCallHook *h,uint8_t *p,const void *a,const void *b) {
    return !fail_install() && SudekiMpInstallRelativeCallHook(h,p,a,b);
}
static BOOL test_inline(SudekiMpInlineHook *h,uint8_t *p,const uint8_t *a,size_t n,const void *b) {
    return !fail_install() && SudekiMpInstallInlineHook(h,p,a,n,b);
}
static uint8_t *mapped;
static uint8_t world[0x390],actors[2][0x134],arbiters[2][0x64],entities[2][0x800];
static uint8_t *tasks[2];
static unsigned actor_calls,break_calls,submit_calls,ready_calls,destroy_calls,collect_calls;
static unsigned synchronous,poison_task;
static uint32_t submit_args[4] __attribute__((used));
static void *submit_this __attribute__((used)),*submit_esi __attribute__((used));
static uint32_t submit_edx __attribute__((used));
static void *finish_edi __attribute__((used));
static uint32_t finish_esi __attribute__((used)),finish_flags __attribute__((used));
static BOOL witness_exact=TRUE;
static unsigned notices;
typedef struct Notice {
    const char *event;
    uint32_t id,parent,a,b;
    void *object,*actor;
} Notice;
static Notice observed[LOG_LIMIT];
static SudekiMpControlUpdateDispatchWitness witness={.service_post_original_exact=1};
static SudekiMpLanStoryNativeRoster fixture_roster;
BOOL SudekiMpLanStoryObserverRosterStillExact(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r) {(void)w;(void)r;return witness_exact;}
void SudekiMpLogFormat(const char *format,...) {
    assert(notices<LOG_LIMIT);
    va_list args; va_start(args,format);
    Notice *n=&observed[notices++];
    n->event=va_arg(args,const char *); n->id=va_arg(args,unsigned long);
    n->parent=va_arg(args,unsigned long); n->object=va_arg(args,void *);
    n->actor=va_arg(args,void *); n->a=va_arg(args,unsigned long); n->b=va_arg(args,unsigned long);
    va_end(args);
    /* Deliberately hostile observer dependencies must not leak to native code. */
    SetLastError(0xdeadbeefu);
    __asm__ volatile("fninit; fld1; pxor %%xmm0,%%xmm0" : : : "memory");
}
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(file!=INVALID_HANDLE_VALUE); DWORD size=GetFileSize(file,NULL),got=0;
    uint8_t *raw=malloc(size); assert(raw && ReadFile(file,raw,size,&got,NULL) && got==size); CloseHandle(file);
    IMAGE_DOS_HEADER *dos=(void *)raw;
    IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *b=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(b); memcpy(b,raw,nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *sections=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        IMAGE_SECTION_HEADER *s=&sections[i];
        assert(s->PointerToRawData<=size && s->SizeOfRawData<=size-s->PointerToRawData);
        assert(s->VirtualAddress<=nt->OptionalHeader.SizeOfImage &&
            s->SizeOfRawData<=nt->OptionalHeader.SizeOfImage-s->VirtualAddress);
        memcpy(b+s->VirtualAddress,raw+s->PointerToRawData,s->SizeOfRawData);
    }
    uintptr_t delta=(uintptr_t)b-nt->OptionalHeader.ImageBase;
    IMAGE_DATA_DIRECTORY reloc=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    unsigned offset=0;
    while(offset<reloc.Size) {
        IMAGE_BASE_RELOCATION *block=(void *)(b+reloc.VirtualAddress+offset);
        assert(block->SizeOfBlock>=sizeof(*block) && block->SizeOfBlock<=reloc.Size-offset);
        uint16_t *items=(uint16_t *)(block+1);
        unsigned count=(block->SizeOfBlock-sizeof(*block))/2;
        for(unsigned i=0;i<count;++i) {
            unsigned type=items[i]>>12,rva=block->VirtualAddress+(items[i]&0xfffu);
            assert(type==IMAGE_REL_BASED_ABSOLUTE || type==IMAGE_REL_BASED_HIGHLOW);
            if(type==IMAGE_REL_BASED_HIGHLOW) {
                assert(rva<=nt->OptionalHeader.SizeOfImage-4);
                *(uint32_t *)(b+rva)+=(uint32_t)delta;
            }
        }
        offset+=block->SizeOfBlock;
    }
    free(raw); return b;
}
static uint32_t invoke_event(void (*entry)(void),void *self,uint32_t event) {
    uint32_t result;
    __asm__ volatile("push %3; call *%2" : "=&a"(result),"+c"(self)
        : "r"(entry),"g"(event) : "edx","memory","cc");
    return result;
}
static uint32_t invoke_actor(void *self,uint32_t event,uint32_t detail) {
    uint32_t result;
    __asm__ volatile("push %3; push %2; call _actor_entry" : "=&a"(result),"+c"(self)
        : "r"(event),"r"(detail) : "edx","memory","cc");
    return result;
}
/* An assembly-level contract independent of the production forwarding code:
 * capture both sides, including ESP and registers a C fixture may not expose. */
static uint32_t actor_native_in[11] __attribute__((used));
static uint32_t actor_native_out[11] __attribute__((used));
static uint32_t actor_stack_before __attribute__((used));
static uint32_t actor_request[3] __attribute__((used));
static uint32_t actor_tail_args[2] __attribute__((used));
#define CAPTURE_ACTOR(name) \
    "mov %eax,_" name "; mov %ecx,_" name "+4; mov %edx,_" name "+8;" \
    "mov %ebx,_" name "+12; mov %esi,_" name "+16; mov %edi,_" name "+20;" \
    "mov %ebp,_" name "+24; pushfl; pop _" name "+28; mov %esp,_" name "+32;" \
    "push 4(%esp); pop _" name "+36; push 8(%esp); pop _" name "+40;"
#define ACTOR_PROBE_RETURN \
    "mov $0x2468ace0,%eax; mov $0x13579bdf,%ecx; mov $0xabcdef01,%edx;" \
    "cmp %eax,%eax; ret $8"
__attribute__((naked,used)) static void fake_actor_tail(void) {
    __asm__ volatile("push 4(%esp); pop _actor_tail_args; push 8(%esp); pop _actor_tail_args+4;"
        ACTOR_PROBE_RETURN);
}
__attribute__((naked,used)) static void fake_actor_probe(void) {
    __asm__ volatile(CAPTURE_ACTOR("actor_native_in")
        /* Native event 0x19 can replace both stack words and tail-jump to a
         * different RET8 function. Its return must still reach actor_after. */
        "cmpl $0x19,4(%esp); jne 1f; movl $0x1234abcd,4(%esp); movl $1,8(%esp);"
        "jmp _fake_actor_tail; 1:" ACTOR_PROBE_RETURN);
}
__attribute__((naked)) static void invoke_actor_probe(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,_actor_stack_before;"
        "mov $0x11223344,%ebx; mov $0x22334455,%esi; mov $0x33445566,%edi; mov $0x44556677,%ebp;"
        "mov _actor_request,%ecx; mov $0x55667788,%edx; mov $0x66778899,%eax;"
        "push _actor_request+8; push _actor_request+4; cmp %eax,%eax; call _actor_entry;"
        CAPTURE_ACTOR("actor_native_out") "popal; popfl; ret");
}
#undef ACTOR_PROBE_RETURN
#undef CAPTURE_ACTOR
static uint32_t invoke_ready(void *task) {
    uint32_t result;
    __asm__ volatile("call _ready_entry" : "=a"(result),"+c"(task) : : "edx","memory","cc");
    return result;
}
static void invoke_submit(void *task) {
    uint32_t result,flags=1;
    __asm__ volatile("push $0x44; push $0x33; push $0x22; push $0x11; call _submit_entry"
        : "=a"(result),"+c"(task),"+d"(flags) : "S"(entities[0]) : "memory","cc");
    assert(result==0xabcdef01u);
}
/* Fastcall with an unused EDX models the exact thiscall register/stack ABI
 * without relying on GCC's non-class thiscall extension. */
static uint32_t __attribute__((fastcall)) fake_ready(void *self,void *unused) {
    (void)unused;
    assert(find_job(self) && find_job(self)->entered); ++ready_calls;
    SetLastError(0x1234); return 0xabcd0001u;
}
static uint32_t __attribute__((fastcall)) fake_destroy(void *self,void *unused,uint32_t flags) {
    (void)unused;
    assert(flags==1 && find_job(self) && find_job(self)->destroying); ++destroy_calls;
    if(poison_task) {DWORD old; assert(VirtualProtect(self,0x1000,PAGE_NOACCESS,&old));}
    SetLastError(0x5678); return (uint32_t)(uintptr_t)self;
}
static void __attribute__((used,noinline)) submit_body(void *self) {
    ++submit_calls;
    assert(find_job(self)); /* recorded before a synchronous native callback */
    if(synchronous) {
        assert(invoke_ready(self)==0xabcd0001u);
        assert(invoke_event(destroy_entry,self,1)==(uint32_t)(uintptr_t)self);
    }
}
__attribute__((naked,used)) static void fake_submit(void) {
    __asm__ volatile("mov %ecx,_submit_this; mov %edx,_submit_edx; mov %esi,_submit_esi;"
        "mov 4(%esp),%eax; mov %eax,_submit_args; mov 8(%esp),%eax; mov %eax,_submit_args+4;"
        "mov 12(%esp),%eax; mov %eax,_submit_args+8; mov 16(%esp),%eax; mov %eax,_submit_args+12;"
        "pushal; push %ecx; call _submit_body; add $4,%esp; popal; mov $0xabcdef01,%eax; ret $16");
}
static uint32_t __attribute__((fastcall)) fake_break(void *self,void *unused,uint32_t event) {
    (void)unused;
    assert(event==1 && break_depth && component_exact(self)); ++break_calls;
    unsigned i=self==entities[0]+0x70c?0:1;
    invoke_submit(tasks[i]);
    *(uint32_t *)((uint8_t *)self+0x7c)=4;
    *(uint32_t *)((uint8_t *)self+0x60)=25;
    SetLastError(0x2345); return 0x13579bdfu;
}
static uint32_t __attribute__((fastcall)) fake_actor(void *self,void *unused,uint32_t event,uint32_t detail) {
    (void)unused;
    /* Independent native contract: actor event returns RET8, unlike the
     * breakable event's RET4. The caller supplies two distinct stack words. */
    assert(detail==0x5a71c0deu);
    assert(event==1 && actor_depth); ++actor_calls;
    unsigned i=self==arbiters[0]+0x3c?0:1;
    assert(invoke_event(break_entry,entities[i]+0x70c,event)==0x13579bdfu);
    SetLastError(0x3456); return 0x2468ace0u;
}
__attribute__((naked,used)) static void fake_finish(void) {
    __asm__ volatile("mov %edi,_finish_edi; mov %esi,_finish_esi; pushfl; pop %eax;"
        "mov %eax,_finish_flags; mov $0x98765432,%eax; ret");
}
static uint32_t __attribute__((stdcall)) fake_collect(void *self) {
    ++collect_calls; ((uint8_t *)self)[0x4c]=1;
    SetLastError(0x4567); return 0x12345601u;
}
#define ADDRESS(fn) ((void *)(uintptr_t)(fn))
static void setup(void) {
    assert(!base);
    fail_step=install_step=0; witness_exact=TRUE;
    assert(SudekiMpLanStoryLootTraceInstall((HMODULE)mapped));
    memset(actors,0,sizeof(actors)); memset(arbiters,0,sizeof(arbiters));
    memset(entities,0,sizeof(entities)); memset(&fixture_roster,0,sizeof(fixture_roster));
    *(void **)(mapped+0x408d10)=world;
    fixture_roster.world=world; fixture_roster.available_mask=3;
    for(unsigned i=0;i<2;++i) {
        fixture_roster.actors[i]=actors[i];
        *(void **)actors[i]=mapped+(i?0x2d66fc:0x2d5a88);
        *(void **)(actors[i]+0x90)=arbiters[i];
        *(void **)arbiters[i]=mapped+0x2cc9ac;
        *(void **)(arbiters[i]+0x10)=actors[i];
        *(uint32_t *)(arbiters[i]+0x50)=0x200;
        *(uint32_t *)(arbiters[i]+0x58)=0x4d00;
        uint8_t *c=entities[i]+0x70c;
        *(void **)entities[i]=mapped+0x2c8a8c;
        *(void **)(entities[i]+0x48)=c; *(void **)(entities[i]+0x128)=c;
        *(void **)c=mapped+0x2c869c; *(void **)(c+0x10)=entities[i];
        DWORD old; assert(VirtualProtect(tasks[i],0x1000,PAGE_READWRITE,&old));
        memset(tasks[i],0,0x28); *(void **)tasks[i]=mapped+0x2c6c48;
        *(void **)(tasks[i]+0x14)=entities[i]+0x2c;
        *(uint32_t *)(tasks[i]+0x18)=10+i;
    }
    assert(SudekiMpLanStoryLootTraceBind(&witness,&fixture_roster));
    actor_original=ADDRESS(fake_actor); break_original=ADDRESS(fake_break);
    submit_original=ADDRESS(fake_submit); ready_original=ADDRESS(fake_ready);
    destroy_original=ADDRESS(fake_destroy); finish_original=ADDRESS(fake_finish);
    collect_original=ADDRESS(fake_collect);
    actor_calls=break_calls=submit_calls=ready_calls=destroy_calls=collect_calls=0;
    notices=synchronous=poison_task=0;
}
static void drained(void) {
    assert(!callbacks && !actor_depth && !break_depth);
    assert(SudekiMpLanStoryLootTraceUninstall() && !base && image_exact(mapped));
}
static void actor_abi_tests(void) {
    static const uint32_t events[]={2,0,1,0x19,0x1a};
    for(unsigned mode=0;mode<5;++mode) {
        setup(); actor_original=ADDRESS(fake_actor_probe);
        if(mode==1) { /* Ordinary animation, not a breakable interaction. */
            *(uint32_t *)(arbiters[0]+0x50)=0;
            *(uint32_t *)(arbiters[0]+0x58)=0;
        }
        if(mode==2) {bound=FALSE;thread=0;}
        if(mode==4) thread=GetCurrentThreadId()+1;
        actor_request[0]=mode==3?0:(uint32_t)(uintptr_t)(arbiters[0]+0x3c);
        for(unsigned i=0;i<sizeof(events)/sizeof(events[0]);++i) {
            actor_request[1]=events[i]; actor_request[2]=0x5a71c0deu+i;
            memset(actor_tail_args,0,sizeof(actor_tail_args));
            SetLastError(0x9988); invoke_actor_probe();
            assert(GetLastError()==0x9988 && !callbacks && !actor_depth && !break_depth && !unknown);
            assert(actor_native_in[0]==0x66778899u && actor_native_in[1]==actor_request[0]);
            assert(actor_native_in[2]==0x55667788u && actor_native_in[9]==events[i]);
            assert(actor_native_in[10]==actor_request[2]);
            assert(actor_native_out[0]==0x2468ace0u && actor_native_out[1]==0x13579bdfu);
            assert(actor_native_out[2]==0xabcdef01u && actor_native_out[8]==actor_stack_before);
            for(unsigned j=3;j<=6;++j) {
                assert(actor_native_in[j]==0x11223344u+(j-3)*0x11111111u);
                assert(actor_native_out[j]==actor_native_in[j]);
            }
            assert((actor_native_in[7]&0x8d5u)==0x44u && (actor_native_out[7]&0x8d5u)==0x44u);
            assert(notices==(mode==0?i+1:0));
            if(events[i]==0x19) assert(actor_tail_args[0]==0x1234abcdu && actor_tail_args[1]==1);
            else assert(!actor_tail_args[0] && !actor_tail_args[1]);
        }
        thread=GetCurrentThreadId(); drained();
    }
}
static void callback_tests(void) {
    setup();
    for(unsigned i=0;i<2;++i) {
        assert(invoke_actor(arbiters[i]+0x3c,1,0x5a71c0deu)==0x2468ace0u);
        assert(GetLastError()==0x3456);
        assert(submit_this==tasks[i] && submit_edx==1 && submit_esi==entities[0]);
        for(unsigned j=0;j<4;++j) assert(submit_args[j]==0x11u*(j+1));
        assert(find_job(tasks[i]) && !find_job(tasks[i])->completed);
    }
    assert(actor_calls==2 && break_calls==2 && submit_calls==2 && !unknown);
    assert(!strcmp(observed[0].event,"actor_event") && observed[0].actor==actors[0]);
    assert(!strcmp(observed[1].event,"break_enter") && observed[1].parent==observed[0].id);
    assert(!strcmp(observed[2].event,"drop_submit") && observed[2].parent==observed[1].id);
    assert(observed[2].actor==actors[0] && observed[2].object==entities[0]+0x2c);
    assert(find_job(tasks[0])->source!=find_job(tasks[1])->source);
    assert(!SudekiMpLanStoryLootTraceUninstall() && GetLastError()==ERROR_BUSY && installed);
    /* Reverse-order completion, then a destructor which makes memory unreadable. */
    for(unsigned i=2;i-->0;) {
        assert(invoke_ready(tasks[i])==0xabcd0001u && GetLastError()==0x1234);
        assert(find_job(tasks[i])->completed);
        poison_task=1;
        assert(invoke_event(destroy_entry,tasks[i],1)==(uint32_t)(uintptr_t)tasks[i]);
        assert(GetLastError()==0x5678 && !find_job(tasks[i]));
    }
    assert(ready_calls==2 && destroy_calls==2 && !unknown); drained();
    setup(); synchronous=1;
    assert(invoke_actor(arbiters[0]+0x3c,1,0x5a71c0deu)==0x2468ace0u);
    assert(ready_calls==1 && destroy_calls==1 && !find_job(tasks[0]) && !unknown); drained();
    setup(); actor_before(arbiters[0]+0x3c,1); break_before(entities[0]+0x70c,1);
    submitted(tasks[0]); break_after(); actor_after();
    /* Destruction is terminal even without setup success; never label a reward. */
    assert(!find_job(tasks[0])->completed);
    invoke_event(destroy_entry,tasks[0],1); assert(!find_job(tasks[0])); drained();
    setup();
    for(unsigned i=0;i<DEPTH+2;++i) actor_before(arbiters[0]+0x3c,1);
    assert(unknown && actor_depth==DEPTH+2);
    for(unsigned i=0;i<DEPTH+2;++i) actor_after();
    assert(!actor_depth && !callbacks); drained();
    setup();
    actor_before(arbiters[0]+0x3c,1); break_before(entities[0]+0x70c,1); submitted(tasks[0]);
    submitted(tasks[0]); assert(unknown); break_after(); actor_after();
    invoke_event(destroy_entry,tasks[0],1); drained();
    setup();
    actor_before(arbiters[0]+0x3c,1);
    actor_before(NULL,1); /* unknown nested caller must mask the outer actor */
    break_before(entities[0]+0x70c,1); submitted(tasks[0]);
    assert(!observed[1].actor && !observed[1].parent && !observed[2].actor);
    break_after(); actor_after(); actor_after();
    invoke_event(destroy_entry,tasks[0],1); drained();
    setup();
    break_before(entities[0]+0x70c,1); submitted(tasks[0]); break_after();
    *(uint32_t *)(tasks[0]+0x18)^=1;
    ready_before(tasks[0]); assert(unknown && !find_job(tasks[0])->entered);
    ready_after(tasks[0],1); assert(!find_job(tasks[0])->completed);
    invoke_event(destroy_entry,tasks[0],1); drained();
}
static void preservation_tests(void) {
    setup();
    uint8_t before[512] __attribute__((aligned(16)))={0};
    uint8_t after[512] __attribute__((aligned(16)))={0};
    SetLastError(0x9988);
    __asm__ volatile("fninit; fldpi; pcmpeqd %%xmm0,%%xmm0; fxsave %0" : "=m"(before) : : "memory");
    actor_before(arbiters[0]+0x3c,1); actor_after();
    __asm__ volatile("fxsave %0; fninit" : "=m"(after) : : "memory");
    assert(GetLastError()==0x9988 && notices);
    assert(!memcmp(before,after,160)); /* x87 environment and all live register bytes */
    assert(!memcmp(before+160,after+160,128)); /* all eight x86 XMM registers */
    for(unsigned active=0;active<2;++active) {
        *(uint32_t *)(arbiters[0]+0x50)=active?0x200:0;
        uint32_t result;
        __asm__ volatile("call _finish_entry" : "=a"(result)
            : "D"(arbiters[0]),"S"(0x4du) : "ecx","edx","memory","cc");
        assert(result==0x98765432u && finish_edi==arbiters[0] && finish_esi==0x4d);
        assert((finish_flags&0x40u)==(active?0u:0x40u));
    }
    uint8_t pickup[0x50]={0}; *(void **)(pickup+0x10)=entities[0];
    *(void **)(entities[0]+0x100)=pickup;
    uint32_t result;
    __asm__ volatile("push %1; call _collect_entry" : "=a"(result)
        : "r"(pickup) : "ecx","edx","memory","cc");
    assert(result==0x12345601u);
    assert(GetLastError()==0x4567 && pickup[0x4c]==1 && collect_calls==1);
    drained();
}
static DWORD WINAPI foreign_thread(void *unused) {
    (void)unused;
    assert(!SudekiMpLanStoryLootTraceBind(&witness,&fixture_roster));
    assert(!SudekiMpLanStoryLootTraceUninstall()); return 0;
}
static void lifecycle_tests(void) {
    for(unsigned i=1;i<=7;++i) {
        fail_step=i; install_step=0;
        assert(!SudekiMpLanStoryLootTraceInstall((HMODULE)mapped));
        assert(GetLastError()==ERROR_WRITE_FAULT && !base && image_exact(mapped));
    }
    fail_step=7; install_step=0; sabotage_slot=(void **)(mapped+0x2c8700);
    assert(!SudekiMpLanStoryLootTraceInstall((HMODULE)mapped));
    assert(GetLastError()==ERROR_WRITE_FAULT && base && pointers[1].installed && !installed);
    assert(*sabotage_slot==sabotage_slot && actor_original && break_original);
    *sabotage_slot=(void *)entry_address(break_entry); sabotage_slot=NULL;
    assert(SudekiMpLanStoryLootTraceUninstall() && !base && image_exact(mapped));
    setup();
    assert(!SudekiMpLanStoryLootTraceInstall((HMODULE)mapped));
    witness_exact=FALSE; assert(!SudekiMpLanStoryLootTraceBind(&witness,&fixture_roster)); witness_exact=TRUE;
    SudekiMpLanStoryNativeRoster other=fixture_roster; other.world=entities[0];
    assert(!SudekiMpLanStoryLootTraceBind(&witness,&other));
    HANDLE worker=CreateThread(NULL,0,foreign_thread,NULL,0,NULL); assert(worker);
    assert(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0); CloseHandle(worker);
    actor_before(arbiters[0]+0x3c,1); assert(!SudekiMpLanStoryLootTraceUninstall()); actor_after();
    void **owned=(void **)(mapped+0x2c8700); void *replacement=*owned;
    *owned=mapped;
    assert(!SudekiMpLanStoryLootTraceUninstall() && base && pointers[1].installed);
    assert(*owned==mapped && !finish_hook.installed && !submit_hook.installed && !collect_hook.installed);
    assert(actor_original && break_original && ready_original && destroy_original);
    *owned=replacement; drained();
    /* Contract guards must reject changed actor caller arguments, each normal
     * return, the tail rewrite/target and both tail exits before patching. */
    static const unsigned changed[]={0x1338a0,0x18ac6d,0x18ac75,0x18ac76,
        0xdbec1,0xdbecd,0xdbece,0xdbef9,0xdbf05,0xdbf57,0x188c30,0x188c51,0xdbe35};
    for(unsigned i=0;i<sizeof(changed)/sizeof(changed[0]);++i) {
        mapped[changed[i]]^=1; install_step=0;
        assert(!SudekiMpLanStoryLootTraceInstall((HMODULE)mapped) && !base && !install_step);
        assert(GetLastError()==ERROR_INVALID_DATA);
        mapped[changed[i]]^=1;
        assert(image_exact(mapped));
    }
    assert(image_exact(mapped));
}
int main(int argc,char **argv) {
    assert(argc==2); wchar_t path[1024];
    assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024)); mapped=map_image(path);
    assert(image_exact(mapped));
    for(unsigned i=0;i<2;++i) {tasks[i]=VirtualAlloc(NULL,0x1000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(tasks[i]);}
    lifecycle_tests(); actor_abi_tests(); callback_tests(); preservation_tests();
    for(unsigned i=0;i<2;++i) VirtualFree(tasks[i],0,MEM_RELEASE);
    VirtualFree(mapped,0,MEM_RELEASE);
    puts("story loot trace exact-image/synthetic ABI, concurrency and rollback tests passed (no native gameplay)");
    return 0;
}
