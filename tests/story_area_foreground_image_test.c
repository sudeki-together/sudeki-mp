/* Exact-image control-flow experiment, NOT a runtime adapter.
 * Executes the supported area-PVS callback and graphics-rebinding helper in
 * an isolated mapped image. All their outbound calls use synthetic stubs.
 * No native loading, scripts, renderer registration or travel executes.
 * The selective inline gate below is fixture-only: it has no native lifetime,
 * asynchronous callback, graphics-reload or production teardown contract. */
#include "engine/build_identity.h"
#include "hooks/call_hook.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(__GNUC__) || !defined(__i386__)
#error "This supported-callback experiment requires x86 GCC"
#endif
enum { PVS_CALLBACK=0x1097a0, PROMOTION=0x1097b7, CONTINUE=0x1097c6, WORLD=0x408d10 };
typedef void (__stdcall *Callback)(void *);
static uint8_t *mapped,world[0x3a0],old_area[0x54],target[0x54],other[0x54];
static uint32_t resident,pending;
static void *protected_descriptor __attribute__((used));
static void *continuation __attribute__((used)),*trampoline __attribute__((used));
static void **world_slot __attribute__((used));
static void *init_data __attribute__((used)),*init_foreground __attribute__((used));
static void *notify_descriptor __attribute__((used)),*notify_foreground __attribute__((used));
static unsigned init_count __attribute__((used)),notify_count __attribute__((used));
static unsigned notify_value __attribute__((used)),events __attribute__((used));
static unsigned init_order __attribute__((used)),notify_order __attribute__((used));
static void __attribute__((naked,noinline)) init_stub(void) {
    __asm__ volatile("mov %eax,_init_data; incl _init_count; incl _events;"
        "mov _events,%eax; mov %eax,_init_order; mov _world_slot,%eax; mov (%eax),%eax;"
        "mov 0x10(%eax),%eax; mov %eax,_init_foreground; ret");
}
static void __attribute__((naked,noinline)) notify_stub(void) {
    __asm__ volatile("mov %edi,_notify_descriptor; mov 4(%esp),%eax; mov %eax,_notify_value;"
        "incl _notify_count; incl _events; mov _events,%eax; mov %eax,_notify_order;"
        "mov _world_slot,%eax; mov (%eax),%eax; mov 0x10(%eax),%eax;"
        "mov %eax,_notify_foreground; ret $4");
}
static void __attribute__((naked,noinline)) selective_gate(void) {
    __asm__ volatile("pushfl; cmp _protected_descriptor,%edi; jne 1f;"
        "popfl; jmp *_continuation; 1: popfl; jmp *_trampoline");
}
/* Graphics rebind can reselect the active PVS BEFORE the reload callback's
 * explicit foreground switch. Exercise that real branch, not an assumed
 * inert binding helper. Registration/deselection effects remain synthetic. */
static uint32_t binding_holder[3] __attribute__((used));
static uint8_t binding_object, binding_resource, binding_gpu[0x44];
static void *binding_registry[2], *binding_vtable[2];
static void *binding_entry __attribute__((used));
static void *binding_data __attribute__((used)), *binding_receiver __attribute__((used));
static void *binding_selected __attribute__((used));
static void *binding_registered;
static unsigned binding_events __attribute__((used));
static unsigned binding_count __attribute__((used)), binding_remove_count __attribute__((used));
static unsigned binding_active_count __attribute__((used)), binding_active_order __attribute__((used));
static unsigned binding_register_count, binding_register_order;
static unsigned binding_order __attribute__((used)), binding_remove_order __attribute__((used));
static unsigned binding_unexpected __attribute__((used));
static void __attribute__((naked,noinline)) binding_bind_stub(void) {
    __asm__ volatile("mov %edi,_binding_data; mov %esi,_binding_receiver;"
        "incl _binding_count; incl _binding_events; mov _binding_events,%eax;"
        "mov %eax,_binding_order; ret");
}
static void __attribute__((naked,noinline)) binding_remove_stub(void) {
    __asm__ volatile("incl _binding_remove_count; incl _binding_events;"
        "mov _binding_events,%eax; mov %eax,_binding_remove_order;"
        "movl $0,8(%esi); ret");
}
static void __attribute__((naked,noinline)) binding_active_stub(void) {
    __asm__ volatile("incl _binding_active_count; incl _binding_events;"
        "mov _binding_events,%eax; mov %eax,_binding_active_order;"
        "mov 4(%esp),%eax; mov %eax,_binding_selected; movl $2,8(%eax); ret $4");
}
static void __attribute__((naked,noinline)) binding_create_unexpected(void) {
    __asm__ volatile("incl _binding_unexpected; ret $4");
}
static void __attribute__((naked,noinline)) binding_deselect_unexpected(void) {
    __asm__ volatile("incl _binding_unexpected; ret");
}
static void __attribute__((thiscall)) binding_register_stub(void *self,void *object,
    unsigned first,unsigned second) {
    assert(self==&binding_registry[1] && !first && !second);
    binding_registered=object;++binding_register_count;
    binding_register_order=++binding_events;
}
static void __attribute__((naked,noinline)) call_binding(void *holder __attribute__((unused)),
    void *resource __attribute__((unused))) {
    __asm__ volatile("mov 8(%esp),%eax; push 4(%esp); call *_binding_entry; ret");
}
static void test_reload_binding(void) {
    enum { BIND=0x110490, BODY=0x7d, GPU=0x408d58 };
    static const unsigned sites[]={0x11049f,0x1104a9,0x1104b9,0x1104bf,0x1104cc,0x1104e6};
    static const unsigned targets[]={0x1103e0,0x1d6460,0x110310,0x110250,0x110310,0x110510};
    void *stubs[]={(void *)binding_create_unexpected,(void *)binding_bind_stub,
        (void *)binding_remove_stub,(void *)binding_active_stub,
        (void *)binding_remove_stub,(void *)binding_deselect_unexpected};
    uint8_t original[BODY];memcpy(original,mapped+BIND,sizeof(original));
    SudekiMpRelativeCallHook calls[6]={{0}};
    for(unsigned i=0;i<6;++i)
        assert(SudekiMpInstallRelativeCallHook(&calls[i],mapped+sites[i],mapped+targets[i],stubs[i]));
    DWORD old_protection;
    assert(VirtualProtect(mapped+BIND,BODY,PAGE_EXECUTE_READWRITE,&old_protection));
    assert(FlushInstructionCache(GetCurrentProcess(),mapped+BIND,BODY));
    binding_entry=mapped+BIND;
    binding_vtable[1]=(void *)binding_register_stub;
    binding_registry[1]=binding_vtable;
    *(void **)(binding_gpu+0x40)=binding_registry;
    *(void **)(mapped+GPU)=binding_gpu;
    for(unsigned state=0;state<=2;++state) {
        binding_holder[0]=(uint32_t)(uintptr_t)&binding_object;
        binding_holder[1]=0;binding_holder[2]=state;
        binding_data=binding_receiver=binding_registered=NULL;
        binding_selected=old_area;
        binding_events=binding_count=binding_remove_count=binding_active_count=0;
        binding_register_count=binding_register_order=binding_order=binding_remove_order=0;
        binding_active_order=binding_unexpected=0;
        call_binding(binding_holder,&binding_resource);
        assert(binding_data==&binding_resource && binding_receiver==&binding_object);
        assert(binding_holder[0]==(uint32_t)(uintptr_t)&binding_object);
        assert(binding_holder[1]==(uint32_t)(uintptr_t)&binding_resource && binding_holder[2]==state);
        assert(binding_count==1 && binding_order==1 && !binding_unexpected);
        assert(binding_remove_count==(state!=0) && binding_remove_order==(state?2u:0u));
        assert(binding_active_count==(state==2) && binding_active_order==(state==2?3u:0u));
        assert(binding_register_count==(state==1) && binding_register_order==(state==1?3u:0u));
        assert(binding_events==(state?3u:1u));
        assert(binding_selected==(state==2?(void *)binding_holder:(void *)old_area));
        assert(binding_registered==(state==1?(void *)&binding_object:NULL));
    }
    for(unsigned i=6;i>0;--i) assert(SudekiMpRestoreRelativeCallHook(&calls[i-1]));
    assert(!memcmp(original,mapped+BIND,sizeof(original)));
    *(void **)(mapped+GPU)=NULL;binding_entry=NULL;
}
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(f!=INVALID_HANDLE_VALUE);DWORD size=GetFileSize(f,NULL),got=0;
    uint8_t *raw=malloc(size);assert(raw && ReadFile(f,raw,size,&got,NULL) && got==size);CloseHandle(f);
    IMAGE_DOS_HEADER *dos=(void *)raw;IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *b=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(b);memcpy(b,raw,nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *s=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        assert(s[i].PointerToRawData<=size && s[i].SizeOfRawData<=size-s[i].PointerToRawData);
        assert(s[i].VirtualAddress<=nt->OptionalHeader.SizeOfImage &&
            s[i].SizeOfRawData<=nt->OptionalHeader.SizeOfImage-s[i].VirtualAddress);
        memcpy(b+s[i].VirtualAddress,raw+s[i].PointerToRawData,s[i].SizeOfRawData);
    }
    IMAGE_DATA_DIRECTORY reloc=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    uintptr_t delta=(uintptr_t)b-nt->OptionalHeader.ImageBase;
    for(unsigned offset=0;offset<reloc.Size;) {
        IMAGE_BASE_RELOCATION *block=(void *)(b+reloc.VirtualAddress+offset);
        assert(block->SizeOfBlock>=sizeof(*block) && block->SizeOfBlock<=reloc.Size-offset);
        uint16_t *items=(void *)(block+1);unsigned count=(block->SizeOfBlock-sizeof(*block))/2;
        for(unsigned i=0;i<count;++i) {
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
static void run_case(uint8_t *descriptor,unsigned state,unsigned flags,BOOL promotes) {
    uint8_t before[sizeof(world)],descriptor_before[0x54];
    memset(world,0x5a,sizeof(world));memset(descriptor,0,sizeof(target));
    *(void **)(world+0x10)=old_area;world[0x399]=0;
    *(void **)(descriptor+0x14)=&resident;*(void **)(descriptor+0x18)=&pending;
    *(unsigned *)(descriptor+0x34)=state;descriptor[0x50]=(uint8_t)flags;
    memcpy(before,world,sizeof(world));memcpy(descriptor_before,descriptor,sizeof(descriptor_before));
    events=init_count=notify_count=init_order=notify_order=notify_value=0;
    init_data=init_foreground=notify_descriptor=notify_foreground=NULL;
    ((Callback)(uintptr_t)(mapped+PVS_CALLBACK))(descriptor);
    assert(!memcmp(descriptor_before,descriptor,sizeof(descriptor_before)));
    if(!state) {
        assert(!events && !init_count && !notify_count && !memcmp(before,world,sizeof(world)));
        return;
    }
    assert(events==2 && init_count==1 && notify_count==1 && init_order==1 && notify_order==2);
    assert(init_data==(state==1?(void *)&pending:(void *)&resident));
    assert(notify_descriptor==descriptor && notify_value==1);
    void *expected=promotes?(void *)descriptor:(void *)old_area;
    assert(init_foreground==expected && notify_foreground==expected);
    if(promotes) {*(void **)(before+0x10)=descriptor;before[0x399]=1;}
    assert(!memcmp(before,world,sizeof(world)));
}
int main(int argc,char **argv) {
    assert(argc==2);wchar_t path[1024];assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024));
    mapped=map_image(path);world_slot=(void **)(mapped+WORLD);*world_slot=world;
    uint8_t original_body[0x58];memcpy(original_body,mapped+PVS_CALLBACK,sizeof(original_body));
    static const unsigned sites[4]={0x1097cf,0x1097d6,0x1097e5,0x1097ec};
    SudekiMpRelativeCallHook calls[4]={{0}};
    for(unsigned i=0;i<4;++i) {
        unsigned target_rva=(i&1)?0x10d620:0x10a070;
        void *replacement=(void *)(uintptr_t)((i&1)?notify_stub:init_stub);
        assert(SudekiMpInstallRelativeCallHook(&calls[i],mapped+sites[i],mapped+target_rva,replacement));
    }
    /* Only the selected routine is invoked; page protection also covers
     * neighboring bytes. All its outbound CALLs point to inert stubs. */
    DWORD protection;
    assert(VirtualProtect(mapped+PVS_CALLBACK,sizeof(original_body),PAGE_EXECUTE_READWRITE,&protection));
    assert(FlushInstructionCache(GetCurrentProcess(),mapped+PVS_CALLBACK,sizeof(original_body)));
    for(unsigned state=0;state<=4;++state) {
        run_case(target,state,2,state!=0);
        run_case(target,state,0,FALSE);
        run_case(target,state,4,FALSE);
    }
    SudekiMpInlineHook gate={0};
    uint8_t expected[5]={0xa1,0,0,0,0};
    uint32_t global=(uint32_t)(uintptr_t)(mapped+WORLD);memcpy(expected+1,&global,4);
    protected_descriptor=target;continuation=mapped+CONTINUE;
    assert(SudekiMpInstallInlineHook(&gate,mapped+PROMOTION,expected,sizeof(expected),
        (void *)(uintptr_t)selective_gate));
    trampoline=gate.trampoline;
    for(unsigned state=0;state<=4;++state) {
        run_case(target,state,2,FALSE);
        run_case(target,state,6,FALSE);
        run_case(other,state,2,state!=0);
    }
    /* A new owned target can differ from the old one; fixture is synchronous,
     * so this does NOT prove safe asynchronous reservation replacement. */
    protected_descriptor=other;
    run_case(target,2,2,TRUE);run_case(other,2,2,FALSE);
    assert(SudekiMpRestoreInlineHook(&gate));
    trampoline=continuation=protected_descriptor=NULL;
    run_case(target,2,2,TRUE);
    for(unsigned i=4;i>0;--i) assert(SudekiMpRestoreRelativeCallHook(&calls[i-1]));
    assert(!memcmp(original_body,mapped+PVS_CALLBACK,sizeof(original_body)));
    test_reload_binding();
    *world_slot=NULL;
    assert(VirtualFree(mapped,0,MEM_RELEASE));
    puts("StoryAreaForegroundImageTest: PASS (native callback and reload-binding branches, synthetic dependencies; no live area playability)");
    return 0;
}
