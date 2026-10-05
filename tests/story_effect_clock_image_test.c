/* Inert exact-image seam validation and synthetic native callbacks only.
 * No retail renderer, script, world update or game constructor executes. */
#include "hooks/call_hook.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
static unsigned install_step,fail_step;
static BOOL test_install(SudekiMpRelativeCallHook *,uint8_t *,const void *,const void *);
#define SudekiMpInstallRelativeCallHook test_install
#include "../src/hooks/lan_story_effects.c"
#undef SudekiMpInstallRelativeCallHook
static BOOL test_install(SudekiMpRelativeCallHook *h,uint8_t *p,const void *a,const void *b) {
    if(++install_step==fail_step) { SetLastError(ERROR_WRITE_FAULT); return FALSE; }
    return SudekiMpInstallRelativeCallHook(h,p,a,b);
}
static unsigned notices;
void SudekiMpLogFormat(const char *format,...) {
    (void)format; ++notices;
    SetLastError(0xdeadbeefu);
    __asm__ volatile("fninit; fld1; pxor %%xmm0,%%xmm0" : : : "memory");
}
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(f!=INVALID_HANDLE_VALUE); DWORD size=GetFileSize(f,NULL),got=0;
    uint8_t *raw=malloc(size); assert(raw && ReadFile(f,raw,size,&got,NULL) && got==size); CloseHandle(f);
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
static uint8_t *mapped;
static struct {
    uint8_t scene_owner[0x44],scene[0x8c],manager[0x24],renderer[0x28];
    uint8_t instance[0x8c],node[0x1e8],data[0x60];
    void *instances[2],*nodes[1];
} fixture,snapshot;
static void dispatch(void *unused) { (void)unused; assert(0); }
static void setup(void) {
    memset(&fixture,0,sizeof(fixture)); notices=0;
    install_step=fail_step=0;
    assert(SudekiMpLanStoryEffectsInstall((HMODULE)mapped,dispatch,NULL));
    native_thread=GetCurrentThreadId();
    *(void **)(mapped+SCENE_MANAGER)=fixture.scene_owner;
    *(void **)(fixture.scene_owner+0x40)=fixture.scene;
    fixture.scene[0x88]=1; *(void **)(fixture.scene+0x54)=fixture.manager;
    *(unsigned *)(fixture.manager+4)=1; *(unsigned *)(fixture.manager+8)=2;
    *(void **)(fixture.manager+0xc)=fixture.instances; fixture.instances[0]=fixture.instance;
    *(void **)fixture.renderer=mapped+0x2de564;
    *(void **)(fixture.renderer+4)=mapped+0x2de658;
    *(void **)(fixture.renderer+0x10)=fixture.data; *(void **)(fixture.renderer+0x14)=fixture.instance;
    *(void **)fixture.instance=mapped+PARTICLE_ANIMATION_VT;
    *(void **)(fixture.instance+0x1c)=fixture.manager;
    *(unsigned *)(fixture.instance+8)=1; *(void **)(fixture.instance+4)=fixture.nodes;
    fixture.nodes[0]=fixture.node; *(void **)fixture.node=mapped+EMITTER_VT;
    *(void **)(fixture.node+0x190)=fixture.data; fixture.data[9]=1;
    *(float *)(fixture.node+0xd8)=3.336f; *(float *)(fixture.node+0xe8)=400.0f;
}
static uint32_t native_in[10] __attribute__((used));
static uint32_t native_out[8] __attribute__((used));
static uint32_t stack_before __attribute__((used));
static void *request_sub __attribute__((used)),*request_instance __attribute__((used));
static unsigned native_calls __attribute__((used));
__attribute__((naked,noinline,used)) static void fake_clock(void) {
    __asm__ volatile("mov %eax,_native_in; mov %ecx,_native_in+4; mov %edx,_native_in+8;"
        "mov %ebx,_native_in+12; mov %esi,_native_in+16; mov %edi,_native_in+20;"
        "mov %ebp,_native_in+24; pushfl; pop _native_in+28; mov %esp,_native_in+32;"
        "push 4(%esp); pop _native_in+36; incl _native_calls;"
        "mov $0x2468ace0,%eax; mov $0x13579bdf,%ecx; mov $0xabcdef01,%edx;"
        "cmp %eax,%eax; ret $4");
}
__attribute__((naked,noinline,used)) static void invoke_clock(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,_stack_before;"
        "mov $0x11223344,%ebx; mov _request_instance,%esi; mov _request_sub,%edi;"
        "mov $0x44556677,%ebp; mov $0x55667788,%edx; mov $0x66778899,%eax;"
        "mov $0x778899aa,%ecx; push $0x4016872c; cmp %eax,%eax; call _model_clock_entry;"
        "mov %eax,_native_out; mov %ecx,_native_out+4; mov %edx,_native_out+8;"
        "mov %ebx,_native_out+12; mov %esi,_native_out+16; mov %edi,_native_out+20;"
        "pushfl; pop _native_out+24; mov %esp,_native_out+28; popal; popfl; ret");
}
static void __attribute__((force_align_arg_pointer)) invoke(void) {
    original_model_clock=(void *)(uintptr_t)fake_clock;
    request_sub=fixture.renderer+4; request_instance=fixture.instance;
    uint8_t before[512] __attribute__((aligned(16)))={0};
    uint8_t after[512] __attribute__((aligned(16)))={0};
    snapshot=fixture; native_calls=0; SetLastError(0x9988);
    __asm__ volatile("fninit; fldpi; pcmpeqd %%xmm0,%%xmm0; fxsave %0" : "=m"(before) : : "memory");
    invoke_clock();
    __asm__ volatile("fxsave %0; fninit" : "=m"(after) : : "memory");
    assert(native_calls==1 && !clock_callbacks && !memcmp(&snapshot,&fixture,sizeof(fixture)));
    assert(GetLastError()==0x9988 && !memcmp(before,after,160) && !memcmp(before+160,after+160,128));
    assert(native_in[0]==0x66778899 && native_in[1]==0x778899aa && native_in[2]==0x55667788);
    assert(native_in[3]==0x11223344 && native_in[4]==(uintptr_t)request_instance);
    assert(native_in[5]==(uintptr_t)request_sub && native_in[6]==0x44556677);
    assert((native_in[7]&0x40) && native_in[8]==stack_before-8 && native_in[9]==0x4016872c);
    assert(native_out[0]==0x2468ace0 && native_out[1]==0x13579bdf && native_out[2]==0xabcdef01);
    assert(native_out[3]==0x11223344 && native_out[4]==(uintptr_t)request_instance);
    assert(native_out[5]==(uintptr_t)request_sub && (native_out[6]&0x40) && native_out[7]==stack_before);
}
static DWORD WINAPI foreign_thread(void *unused) {
    (void)unused;
    unsigned n=notices; invoke(); assert(notices==n);
    assert(!SudekiMpLanStoryEffectsUninstall()); return 0;
}
static void lifecycle_tests(void) {
    for(unsigned i=1;i<=2;++i) {
        install_step=0; fail_step=i;
        assert(!SudekiMpLanStoryEffectsInstall((HMODULE)mapped,dispatch,NULL));
        assert(!installed && !update_hook.installed && !model_clock_hook.installed);
        assert(call_targets(mapped+PARTICLE_CALL,mapped+PARTICLE_UPDATE));
        assert(call_targets(mapped+MODEL_CLOCK_CALL,mapped+MODEL_CLOCK_SET));
    }
    static const unsigned changed[]={MODEL_CLOCK_CALL,MODEL_CLOCK_CALL-1,MODEL_CLOCK_CALL-15,
        MODEL_CLOCK_SET,0x23190d,0x231911,0x2de660};
    for(unsigned i=0;i<sizeof(changed)/sizeof(*changed);++i) {
        mapped[changed[i]]^=1; install_step=fail_step=0;
        assert(!SudekiMpLanStoryEffectsInstall((HMODULE)mapped,dispatch,NULL) && !install_step);
        mapped[changed[i]]^=1;
    }
    setup(); active=TRUE; assert(!SudekiMpLanStoryEffectsUninstall()); active=FALSE;
    clock_callbacks=1; assert(!SudekiMpLanStoryEffectsUninstall()); clock_callbacks=0;
    uint8_t owned[5]; memcpy(owned,mapped+MODEL_CLOCK_CALL,5); mapped[MODEL_CLOCK_CALL]=0xcc;
    assert(!SudekiMpLanStoryEffectsUninstall() && model_clock_hook.installed && update_hook.installed);
    assert(base==mapped && original_model_clock && original_update);
    memcpy(mapped+MODEL_CLOCK_CALL,owned,5); assert(SudekiMpLanStoryEffectsUninstall());
    assert(!base && call_targets(mapped+MODEL_CLOCK_CALL,mapped+MODEL_CLOCK_SET));
}
int main(int argc,char **argv) {
    assert(argc==2); wchar_t path[1024];
    assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024)); mapped=map_image(path);
    lifecycle_tests();
    setup(); invoke(); assert(notices==1);
    fixture.data[9]=0; invoke(); assert(notices==1); fixture.data[9]=1;
    fixture.instances[1]=fixture.instance; *(unsigned *)(fixture.manager+4)=2;
    invoke(); assert(notices==1); *(unsigned *)(fixture.manager+4)=1;
    *(void **)(fixture.renderer+4)=mapped; invoke(); assert(notices==1);
    *(void **)(fixture.renderer+4)=mapped+0x2de658;
    HANDLE worker=CreateThread(NULL,0,foreign_thread,NULL,0,NULL); assert(worker);
    assert(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0); CloseHandle(worker);
    stopping=0; invoke(); assert(notices==2);
    clock_trace_logs=256; invoke(); assert(notices==2);
    assert(SudekiMpLanStoryEffectsUninstall()); invoke(); assert(notices==2);
    VirtualFree(mapped,0,MEM_RELEASE);
    puts("story effect clock exact-image/synthetic observer ABI, isolation and rollback tests passed (no native gameplay)");
    return 0;
}
