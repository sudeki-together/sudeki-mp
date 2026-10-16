/* Supported-image height threshold and actual x87-hook execution fixture.
 * Native threshold comparisons/branch/pop/add execute; fixture continuations
 * stop before collision or a full-world camera solve. This is not live proof. */
#include "engine/build_identity.h"
#include "hooks/lan_story_client_camera_height.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(file!=INVALID_HANDLE_VALUE);DWORD size=GetFileSize(file,NULL),got=0;
    uint8_t *raw=malloc(size);assert(raw && ReadFile(file,raw,size,&got,NULL) && got==size);CloseHandle(file);
    IMAGE_DOS_HEADER *dos=(void *)raw;IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *b=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
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

static uint8_t *height_image;
static void *height_entry __attribute__((used));
typedef struct HeightResult {
    uint32_t branch;
    float value,sentinel;
    uint16_t control,status;
    uint32_t esp_before,esp_after,ebx_after,esi_after,edi_after,flags;
} HeightResult;

/* The wrapper enters RVA158015 with exactly the native pre-comparison x87
 * shape: ST0=delta, ST1=current height; a sentinel is below both. The actual
 * native FLD adds abs(delta). Retail continuation158033 pops two values on
 * skip;15805C/15805E pop abs and add delta+height on apply. */
__attribute__((naked,noinline,noclone)) static void height_invoke(
    void *context __attribute__((unused)),float delta __attribute__((unused)),
    float current __attribute__((unused)),float sentinel __attribute__((unused)),
    HeightResult *out __attribute__((unused)),unsigned control __attribute__((unused)),
    void *geometry __attribute__((unused)),void *return_site __attribute__((unused)),
    void *geometry_argument __attribute__((unused)),float desired __attribute__((unused))) {
    __asm__ volatile(
        "push %ebp; mov %esp,%ebp; push %ebx; push %esi; push %edi; sub $192,%esp;"
        "fnsave 64(%esp); fldcw 28(%ebp);"
        "mov 24(%ebp),%eax; mov %esp,16(%eax); mov %eax,32(%esp); mov %ebp,60(%esp);"
        "mov 32(%ebp),%ebx; mov $0x55667788,%esi; mov 8(%ebp),%edi;"
        "mov 36(%ebp),%eax; mov %eax,40(%esp);"
        "mov 40(%ebp),%eax; mov %eax,44(%esp);"
        "mov 44(%ebp),%eax; mov %eax,28(%esp);"
        "mov 12(%ebp),%eax; mov %eax,24(%esp);"
        "flds 20(%ebp); flds 16(%ebp); flds 12(%ebp);"
        "flds 12(%ebp); fabs; fstps 20(%esp);"
        "lea 36(%esp),%ebp; std;"
        "jmp *_height_entry;");
}
__attribute__((naked,noinline)) static void height_skipped(void) {
    __asm__ volatile(
        "mov 32(%esp),%eax; movl $0,0(%eax); movl $0,4(%eax);"
        "jmp _height_finish;");
}
__attribute__((naked,noinline)) static void height_applied(void) {
    __asm__ volatile(
        "mov 32(%esp),%eax; movl $1,0(%eax); fstps 4(%eax);"
        "jmp _height_finish;");
}
__attribute__((naked,noinline,used)) static void height_finish(void) {
    __asm__ volatile(
        "fstps 8(%eax); fnstcw 12(%eax); fnstsw 14(%eax);"
        "mov %esp,20(%eax); mov %ebx,24(%eax); mov %esi,28(%eax); mov %edi,32(%eax);"
        "pushfl; pop %edx; mov %edx,36(%eax); cld;"
        "frstor 64(%esp); mov 60(%esp),%ebp; add $192,%esp; pop %edi; pop %esi; pop %ebx; pop %ebp; ret;");
}
static void jump_fixture(unsigned rva,const void *destination,uint8_t original[5]) {
    memcpy(original,height_image+rva,5u);height_image[rva]=0xe9u;
    int32_t displacement=(int32_t)((uintptr_t)destination-(uintptr_t)(height_image+rva+5u));
    memcpy(height_image+rva+1,&displacement,4u);
    assert(FlushInstructionCache(GetCurrentProcess(),height_image+rva,5u));
}
static void restore_fixture(unsigned rva,const uint8_t original[5]) {
    memcpy(height_image+rva,original,5u);
    assert(FlushInstructionCache(GetCurrentProcess(),height_image+rva,5u));
}
static HeightResult invoke_detail(void *context,void *geometry,void *return_site,
    void *geometry_argument,float delta,float current,float desired) {
    HeightResult out={0};
    /* Double precision and downward rounding deliberately differ from FNINIT;
     * the hook must restore this state after any C policy callback. */
    const unsigned control=0x067fu;
    SetLastError(0x12345678u);
    height_invoke(context,delta,current,13.25f,&out,control,geometry,return_site,geometry_argument,desired);
    assert(GetLastError()==0x12345678u);
    assert(out.sentinel==13.25f && out.control==control && !(out.status&0x3800u));
    assert(out.esp_before==out.esp_after && out.ebx_after==(uintptr_t)geometry &&
        out.esi_after==0x55667788u && out.edi_after==(uintptr_t)context && (out.flags&0x400u));
    return out;
}

typedef struct HeightFixture {
    uint8_t camera[0xc0],state[4],data[4],outer[4],inner[4],context[0x20],geometry[0x48];
    SudekiMpLanStoryClientCameraHeightScope scope;
} HeightFixture;
static HeightFixture fixture;
static BOOL proof_allowed=TRUE;
static unsigned proof_calls;
static BOOL owner_proof(const SudekiMpLanStoryClientCameraHeightScope *s) {
    ++proof_calls;
    /* The hook must restore foreign callback LastError and FP control state. */
    SetLastError(0xabcdef12u);
    const uint16_t control=0x0b7fu;
    __asm__ volatile("fldcw %0" : : "m"(control));
    return proof_allowed && !memcmp(s,&fixture.scope,sizeof(*s));
}
static void reset_fixture(void) {
    memset(&fixture,0,sizeof(fixture));
    fixture.scope=(SudekiMpLanStoryClientCameraHeightScope){fixture.camera,fixture.state,
        fixture.data,fixture.outer,fixture.inner};
    *(void **)(fixture.camera+0x3c)=fixture.state;
    *(void **)(fixture.camera+0x40)=fixture.data;
    *(void **)(fixture.camera+0xb4)=fixture.outer;
    *(void **)(fixture.camera+0xb8)=fixture.inner;
    *(void **)fixture.state=height_image+0x2d9854u;
    *(void **)fixture.data=height_image+0x2ca694u;
    *(void **)(fixture.context+0)=fixture.camera;
    *(void **)(fixture.context+4)=fixture.data;
    *(void **)(fixture.context+0x18)=fixture.outer;
    *(void **)(fixture.context+0x1c)=fixture.inner;
    *(float *)(fixture.geometry+0x34)=1.0f;
    *(float *)(fixture.geometry+0x44)=2.0f;
    proof_allowed=TRUE;proof_calls=0;
}
static HeightResult invoke(float delta,float current) {
    return invoke_detail(fixture.context,fixture.geometry,height_image+0x1572bfu,
        fixture.geometry,delta,current,3.0f);
}
static void expect_skip(float delta) { assert(invoke(delta,2.0f).branch==0u); }
static void expect_apply(float delta) {
    HeightResult result=invoke(delta,2.0f);
    assert(result.branch==1u && fabsf(result.value-(2.0f+delta))<0.000001f);
}
static void named_image_guards(void) {
    const struct {const char *name;unsigned rva,count;} guards[]={
        {"HeightCaller",0x1572b5u,0xau},
        {"HeightEntryAndErrorClamp",0x157e60u,0x1b5u},
        {"HeightCutoff",0x158015u,0x22u},
        {"HeightApplyCollision",0x15805cu,0xa6u},
        {"HeightSpring",0x158110u,0xbeu},
        {"HeightConstants",0x2e3cd8u,16u}
    };
    assert(SudekiMpLanStoryClientCameraHeightImageExact((HMODULE)height_image));
    assert(!SudekiMpLanStoryClientCameraHeightImageExact(NULL));
    for(unsigned i=0;i<sizeof(guards)/sizeof(*guards);++i) {
        for(unsigned j=0;j<guards[i].count;++j) {
            height_image[guards[i].rva+j]^=1u;
            assert(!SudekiMpLanStoryClientCameraHeightImageExact((HMODULE)height_image));
            if(!j) assert(!SudekiMpLanStoryClientCameraHeightInstall((HMODULE)height_image,owner_proof));
            height_image[guards[i].rva+j]^=1u;
        }
        assert(SudekiMpLanStoryClientCameraHeightImageExact((HMODULE)height_image));
        printf("PASS %s: supported image; every checked byte mutation refused\n",guards[i].name);
    }
    assert(*(void **)(height_image+0x15801bu)==height_image+0x2e3ce0u);
    assert(*(void **)(height_image+0x158028u)==height_image+0x2e3cd8u);
    puts("PASS HeightCutoffRelocations: both absolute threshold operands resolve to checked constants");
}
static void relocate_copy(uint8_t *copy,const uint8_t *original) {
    IMAGE_DOS_HEADER *dos=(void *)copy;
    IMAGE_NT_HEADERS32 *nt=(void *)(copy+dos->e_lfanew);
    IMAGE_DATA_DIRECTORY reloc=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    uint32_t delta=(uint32_t)((uintptr_t)copy-(uintptr_t)original);
    for(unsigned offset=0;offset<reloc.Size;) {
        IMAGE_BASE_RELOCATION *block=(void *)(copy+reloc.VirtualAddress+offset);
        uint16_t *items=(void *)(block+1);
        for(unsigned j=0;j<(block->SizeOfBlock-sizeof(*block))/2u;++j)
            if((items[j]>>12)==IMAGE_REL_BASED_HIGHLOW)
                *(uint32_t *)(copy+block->VirtualAddress+(items[j]&0xfffu))+=delta;
        offset+=block->SizeOfBlock;
    }
}
/* A supported, executable, read-only section passes image validation but its
 * section maximum protection forbids the final writable patch. No hook helper
 * is mocked; successful retry on the normal image proves ownership was reset. */
static void install_failure(void) {
    wchar_t directory[MAX_PATH],path[MAX_PATH];
    assert(GetTempPathW(MAX_PATH,directory) && GetTempFileNameW(directory,L"sch",0,path));
    HANDLE file=CreateFileW(path,GENERIC_READ|GENERIC_WRITE,0,NULL,CREATE_ALWAYS,
        FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,NULL);
    assert(file!=INVALID_HANDLE_VALUE);
    IMAGE_DOS_HEADER *dos=(void *)height_image;
    IMAGE_NT_HEADERS32 *nt=(void *)(height_image+dos->e_lfanew);
    DWORD size=nt->OptionalHeader.SizeOfImage;
    HANDLE mapping=CreateFileMappingW(file,NULL,PAGE_EXECUTE_READWRITE,0,size,NULL);
    assert(mapping);
    uint8_t *copy=MapViewOfFile(mapping,FILE_MAP_READ|FILE_MAP_WRITE|FILE_MAP_EXECUTE,0,0,size);
    assert(copy);memcpy(copy,height_image,size);relocate_copy(copy,height_image);
    assert(FlushViewOfFile(copy,size));
    void *address=copy;assert(UnmapViewOfFile(copy));CloseHandle(mapping);
    mapping=CreateFileMappingW(file,NULL,PAGE_EXECUTE_READ,0,size,NULL);assert(mapping);
    copy=MapViewOfFileEx(mapping,FILE_MAP_READ|FILE_MAP_EXECUTE,0,0,size,address);assert(copy==address);
    assert(SudekiMpLanStoryClientCameraHeightImageExact((HMODULE)copy));
    uint8_t original[6];memcpy(original,copy+0x158026u,sizeof(original));
    assert(!SudekiMpLanStoryClientCameraHeightInstall((HMODULE)copy,owner_proof));
    assert(!memcmp(original,copy+0x158026u,sizeof(original)));
    assert(SudekiMpLanStoryClientCameraHeightUninstall());
    assert(UnmapViewOfFile(copy));CloseHandle(mapping);CloseHandle(file);
    assert(SudekiMpLanStoryClientCameraHeightInstall((HMODULE)height_image,owner_proof));
    assert(SudekiMpLanStoryClientCameraHeightUninstall());
    puts("PASS HeightInstallPatchFailure: validated read-only section refuses final patch; bytes intact and retry succeeds");
}
static DWORD WINAPI foreign_thread(void *unused) {
    (void)unused;
    assert(!SudekiMpLanStoryClientCameraHeightBegin(&fixture.scope));
    expect_skip(0.004f);
    SudekiMpLanStoryClientCameraHeightEnd();
    assert(!SudekiMpLanStoryClientCameraHeightUninstall());
    return 0;
}
static void run_foreign_thread(void) {
    HANDLE thread=CreateThread(NULL,0,foreign_thread,NULL,0,NULL);assert(thread);
    assert(WaitForSingleObject(thread,5000)==WAIT_OBJECT_0);
    DWORD result=1;assert(GetExitCodeThread(thread,&result) && !result);CloseHandle(thread);
}
static void execution_tests(void) {
    uint8_t original_skip[5],original_apply[5],original_hook[6],installed_hook[6];
    reset_fixture();height_entry=height_image+0x158015u;
    memcpy(original_hook,height_image+0x158026u,6);
    /* Establish the unchanged original branch behavior before installing. */
    jump_fixture(0x158037u,height_skipped,original_skip);
    jump_fixture(0x158060u,height_applied,original_apply);
    expect_skip(0.004f);expect_skip(-0.004f);expect_skip(0.0f);
    expect_apply(0.01f);expect_apply(-0.02f);
    const float nonfinite[]={NAN,INFINITY,-INFINITY};
    HeightResult nonfinite_native[3];
    for(unsigned i=0;i<3;++i) nonfinite_native[i]=invoke(nonfinite[i],2.0f);
    restore_fixture(0x158037u,original_skip);restore_fixture(0x158060u,original_apply);
    assert(!SudekiMpLanStoryClientCameraHeightInstall((HMODULE)height_image,NULL));
    assert(SudekiMpLanStoryClientCameraHeightInstall((HMODULE)height_image,owner_proof));
    assert(!SudekiMpLanStoryClientCameraHeightInstall((HMODULE)height_image,owner_proof));
    memcpy(installed_hook,height_image+0x158026u,6);
    jump_fixture(0x158037u,height_skipped,original_skip);
    jump_fixture(0x158060u,height_applied,original_apply);
    expect_skip(0.004f);expect_skip(-0.004f);expect_apply(0.01f);expect_apply(-0.02f);
    assert(!proof_calls);
    puts("PASS HeightOutsideScope: actual native comparisons retain small-step discard and ordinary correction branches");
    assert(!SudekiMpLanStoryClientCameraHeightBegin(NULL));
    proof_allowed=FALSE;assert(!SudekiMpLanStoryClientCameraHeightBegin(&fixture.scope));
    proof_allowed=TRUE;
    assert(SudekiMpLanStoryClientCameraHeightBegin(&fixture.scope));
    assert(!SudekiMpLanStoryClientCameraHeightBegin(&fixture.scope));
    assert(!SudekiMpLanStoryClientCameraHeightUninstall());
    unsigned before=proof_calls;
    expect_apply(0.004f);expect_apply(-0.004f);
    assert(proof_calls==before+2u);
    expect_skip(0.0f);expect_skip(-0.0f);expect_apply(0.01f);expect_apply(-0.02f);
    for(unsigned i=0;i<3;++i) {
        HeightResult result=invoke(nonfinite[i],2.0f);
        assert(result.branch==nonfinite_native[i].branch);
        assert(result.value==nonfinite_native[i].value ||
            (isnan(result.value) && isnan(nonfinite_native[i].value)));
    }
    puts("PASS HeightScopedX87: tiny signed finite corrections use native pop/add; zero and ordinary steps keep retail policy; stack, registers, FP control and LastError preserved");
    /* A newly failed owner proof refuses the same borrowed scope immediately. */
    proof_allowed=FALSE;expect_skip(0.004f);proof_allowed=TRUE;
    const unsigned camera_offsets[]={0x3cu,0x40u,0xb4u,0xb8u};
    for(unsigned i=0;i<sizeof(camera_offsets)/sizeof(*camera_offsets);++i) {
        void **slot=(void **)(fixture.camera+camera_offsets[i]),*saved=*slot;
        *slot=NULL;expect_skip(0.004f);*slot=saved;
    }
    for(unsigned i=0;i<2;++i) {
        void **slot=i?(void **)fixture.data:(void **)fixture.state,*saved=*slot;
        *slot=NULL;expect_skip(0.004f);*slot=saved;
    }
    const unsigned context_offsets[]={0u,4u,0x18u,0x1cu};
    for(unsigned i=0;i<sizeof(context_offsets)/sizeof(*context_offsets);++i) {
        void **slot=(void **)(fixture.context+context_offsets[i]),*saved=*slot;
        *slot=NULL;expect_skip(0.004f);*slot=saved;
    }
    assert(!invoke_detail(fixture.context,fixture.geometry,height_image+0x1572c0u,
        fixture.geometry,0.004f,2,3).branch);
    assert(!invoke_detail(fixture.context,fixture.geometry,height_image+0x1572bfu,
        fixture.geometry+4,0.004f,2,3).branch);
    assert(!invoke_detail(NULL,fixture.geometry,height_image+0x1572bfu,
        fixture.geometry,0.004f,2,3).branch);
    assert(!invoke_detail(fixture.context,NULL,height_image+0x1572bfu,NULL,0.004f,2,3).branch);
    const float invalid[]={NAN,INFINITY,-INFINITY,1000001.0f,-1000001.0f};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);++i) {
        assert(!invoke_detail(fixture.context,fixture.geometry,height_image+0x1572bfu,
            fixture.geometry,0.004f,2,invalid[i]).branch);
        *(float *)(fixture.geometry+0x34)=invalid[i];expect_skip(0.004f);
        *(float *)(fixture.geometry+0x34)=1;
        *(float *)(fixture.geometry+0x44)=invalid[i];expect_skip(0.004f);
        *(float *)(fixture.geometry+0x44)=2;
    }
    run_foreign_thread();expect_apply(0.004f);
    puts("PASS HeightScopeRefusal: fresh owner, camera/state/data/targets, native caller/context/geometry, finite bounds and thread fences");
    SudekiMpLanStoryClientCameraHeightEnd();expect_skip(0.004f);
    run_foreign_thread();
    /* Foreign patch mutation must preserve complete hook ownership for retry. */
    height_image[0x15802bu]^=1u;
    assert(!SudekiMpLanStoryClientCameraHeightUninstall());
    assert(!SudekiMpLanStoryClientCameraHeightInstall((HMODULE)height_image,owner_proof));
    assert(!SudekiMpLanStoryClientCameraHeightBegin(&fixture.scope));
    assert(height_image[0x15802bu]==(uint8_t)(installed_hook[5]^1u));
    memcpy(height_image+0x158026u,installed_hook,6);
    assert(FlushInstructionCache(GetCurrentProcess(),height_image+0x158026u,6));
    assert(SudekiMpLanStoryClientCameraHeightBegin(&fixture.scope));expect_apply(0.004f);
    SudekiMpLanStoryClientCameraHeightEnd();
    assert(SudekiMpLanStoryClientCameraHeightUninstall());
    assert(!memcmp(original_hook,height_image+0x158026u,6));
    expect_skip(0.004f);expect_apply(0.01f);
    restore_fixture(0x158037u,original_skip);restore_fixture(0x158060u,original_apply);
    assert(SudekiMpLanStoryClientCameraHeightImageExact((HMODULE)height_image));
    assert(SudekiMpLanStoryClientCameraHeightInstall((HMODULE)height_image,owner_proof));
    assert(SudekiMpLanStoryClientCameraHeightUninstall());
    puts("PASS HeightRestoreRetry: active/foreign thread/changed patch refuse; owned retry executes and restores exact original image");
}
int main(int argc,char **argv) {
    assert(argc==2);wchar_t path[1024];
    assert(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,argv[1],-1,path,1024));
    height_image=map_image(path);
    named_image_guards();install_failure();execution_tests();
    assert(VirtualFree(height_image,0,MEM_RELEASE));
    puts("PASS story client camera height exact-image/synthetic execution (no native collision or full-world solve)");
    return 0;
}
