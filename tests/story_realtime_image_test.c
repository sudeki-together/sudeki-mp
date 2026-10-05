/* Exact image plus synthetic CGameSpeed storage. Only three verified leaf
 * float setters execute; no world, UI, task, skill or pause function executes. */
#include "hooks/call_hook.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned step,fail_step;
static BOOL test_byte(SudekiMpBytePatch *,uint8_t *,uint8_t,uint8_t);
static BOOL test_inline(SudekiMpInlineHook *,uint8_t *,const uint8_t *,size_t,const void *);
#define SudekiMpInstallBytePatch test_byte
#define SudekiMpInstallInlineHook test_inline
#include "../src/hooks/lan_story_realtime.c"
#undef SudekiMpInstallBytePatch
#undef SudekiMpInstallInlineHook
static BOOL fail(void) {
    if(++step==fail_step) { SetLastError(ERROR_WRITE_FAULT); return TRUE; } return FALSE;
}
static BOOL test_byte(SudekiMpBytePatch *p,uint8_t *at,uint8_t a,uint8_t b) {
    return !fail() && SudekiMpInstallBytePatch(p,at,a,b);
}
static BOOL test_inline(SudekiMpInlineHook *p,uint8_t *at,const uint8_t *a,size_t n,const void *b) {
    return !fail() && SudekiMpInstallInlineHook(p,at,a,n,b);
}
void SudekiMpLogWrite(const char *text) { (void)text; }
static uint8_t *mapped;
static size_t image_size;
static unsigned source_references;
/* Independent relocation census of the pooled alternate-speed constant.
 * All but camera47B668 are CGameSpeed selectors or its two init/reset reads.
 * A changed reference set invalidates this audit instead of silently changing
 * another use of a compiler-pooled float. Values are executable RVAs. */
static const unsigned source_reads[]={
    0x5e16,0x7a7b,0xca84,0xceba,0x102a0,0x2606b,0x26106,0x27063,
    0x2f40b,0x2f4f9,0x35202,0x35464,0x3554c,0x73353,0x74675,
    0x777ac,0x77973,0x7b668,0x8401a,0x846c5,0x95752,0x95a70,
    0x99096,0x9927d,0xa139f,0xb53ba,0xb54da,0xbd58c,0xbe467,
    0xbe574,0xbe8ae,0xc1e28,0xfcd09,0xfced7,0xfdc93,0x1008d2,
    0x10097c,0x1053ba,0x10a2de,0x10a42d,0x10b085,0x10b231,
    0x10d708,0x12ea4c,0x164bbd,0x164d59,0x17dd08,0x17de4f,
    0x17dfc5,0x17e0e9,0x17e209,0x17e329,0x17e449,0x17e569,
    0x17e687,0x17e7cd,0x17e8e9,0x17e99f,0x17ead4,0x17ec15,
    0x17ed17,0x1843be,0x184509,0x18466c,0x184bc8,0x184d12,
    0x19c2a0,0x1a8262,0x1ad72c,0x1ad8ed,0x28dd30};
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(f!=INVALID_HANDLE_VALUE); DWORD size=GetFileSize(f,NULL),got=0;
    uint8_t *raw=malloc(size); assert(raw && ReadFile(f,raw,size,&got,NULL) && got==size); CloseHandle(f);
    IMAGE_DOS_HEADER *dos=(void *)raw; IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    image_size=nt->OptionalHeader.SizeOfImage;
    uint8_t *b=VirtualAlloc(NULL,image_size,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    assert(b); memcpy(b,raw,nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *sections=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        IMAGE_SECTION_HEADER *s=&sections[i];
        assert(s->PointerToRawData<=size && s->SizeOfRawData<=size-s->PointerToRawData);
        assert(s->VirtualAddress<=image_size && s->SizeOfRawData<=image_size-s->VirtualAddress);
        memcpy(b+s->VirtualAddress,raw+s->PointerToRawData,s->SizeOfRawData);
    }
    uintptr_t delta=(uintptr_t)b-nt->OptionalHeader.ImageBase;
    IMAGE_DATA_DIRECTORY reloc=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    unsigned offset=0;
    while(offset<reloc.Size) {
        IMAGE_BASE_RELOCATION *block=(void *)(b+reloc.VirtualAddress+offset);
        assert(block->SizeOfBlock>=sizeof(*block) && block->SizeOfBlock<=reloc.Size-offset);
        uint16_t *items=(uint16_t *)(block+1);
        for(unsigned i=0;i<(block->SizeOfBlock-sizeof(*block))/2;++i) {
            unsigned type=items[i]>>12,rva=block->VirtualAddress+(items[i]&0xfffu);
            assert(type==IMAGE_REL_BASED_ABSOLUTE || type==IMAGE_REL_BASED_HIGHLOW);
            if(type==IMAGE_REL_BASED_HIGHLOW) {
                assert(rva<=image_size-4);
                if(*(uint32_t *)(b+rva)==0x6c4018u) {
                    unsigned matches=0;
                    for(unsigned j=0;j<sizeof(source_reads)/sizeof(*source_reads);++j)
                        if(rva==source_reads[j]+2u) ++matches;
                    assert(matches==1u && b[rva-2]==0xd9u && b[rva-1]==0x05u);
                    ++source_references;
                }
                *(uint32_t *)(b+rva)+=(uint32_t)delta;
            }
        }
        offset+=block->SizeOfBlock;
    }
    assert(source_references==sizeof(source_reads)/sizeof(*source_reads));
    free(raw); return b;
}
static void install(void) {
    step=fail_step=0; assert(SudekiMpLanStoryRealtimeInstall((HMODULE)mapped));
    assert(step==12 && SudekiMpLanStoryRealtimeExact((HMODULE)mapped));
}
static DWORD WINAPI foreign_thread(void *unused) {
    (void)unused; assert(!SudekiMpLanStoryRealtimeUninstall() && retains()); return 0;
}
static void lifecycle_tests(void) {
    uint8_t *original=malloc(image_size); assert(original); memcpy(original,mapped,image_size);
    for(unsigned i=1;i<=12;++i) {
        step=0; fail_step=i;
        assert(!SudekiMpLanStoryRealtimeInstall((HMODULE)mapped) && GetLastError()==ERROR_WRITE_FAULT);
        assert(!retains() && !installed && !memcmp(original,mapped,image_size));
    }
    static const unsigned changed[]={0x27040,0x27046,0x2704a,0x28be96,0x2751f,
        0x2752f,0x98ee6,0x98ef3,0x7b668,0x7b66e,0x777b2,0x7797b,0x2c4018,0x345f70};
    for(unsigned i=0;i<sizeof(changed)/sizeof(*changed);++i) {
        mapped[changed[i]]^=1; step=fail_step=0;
        assert(!SudekiMpLanStoryRealtimeInstall((HMODULE)mapped) && !step && !retains());
        mapped[changed[i]]^=1;
    }
    install(); assert(!SudekiMpLanStoryRealtimeInstall((HMODULE)mapped));
    HANDLE worker=CreateThread(NULL,0,foreign_thread,NULL,0,NULL); assert(worker);
    assert(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0); CloseHandle(worker);
    uint8_t owned=mapped[0x7b66b]; mapped[0x7b66b]^=1;
    assert(!SudekiMpLanStoryRealtimeUninstall() && retains() && normal_hook.installed);
    assert(camera_patches[1].installed && !SudekiMpLanStoryRealtimeExact((HMODULE)mapped));
    mapped[0x7b66b]=owned;
    assert(SudekiMpLanStoryRealtimeUninstall() && !retains() && !memcmp(original,mapped,image_size));
    free(original);
}
static void setter_tests(void) {
    typedef void (__cdecl *SetSpeed)(float);
    typedef void (__attribute__((thiscall)) *SetVariable)(void *,float);
    SetSpeed normal=(SetSpeed)(uintptr_t)(mapped+0x27040),master=(SetSpeed)(uintptr_t)(mapped+0x28be90);
    SetVariable variable=(SetVariable)(uintptr_t)(mapped+0x27510);
    const float requests[]={0.07f,0.5f,1.0f,0.0f,-1.0f,2.0f,NAN,INFINITY};
    uint8_t speed[0x30],saved[sizeof(speed)];
    uint8_t pause_code[0x40],mode_code[0x100];
    memcpy(pause_code,mapped+0x27480,sizeof(pause_code)); memcpy(mode_code,mapped+0x27190,sizeof(mode_code));
    install();
    assert(mapped[0x98ef3]==0 && *(float *)(mapped+0x2c4018)==1.0f);
    assert(*(const float **)(mapped+0x7b66a)==&camera_threshold &&
        !memcmp(*(const void **)(mapped+0x7b66a),alternate,4));
    for(unsigned i=0;i<sizeof(requests)/sizeof(*requests);++i) {
        normal(requests[i]); master(requests[i]);
        assert(*(float *)(mapped+0x345f70)==1.0f && *(float *)(mapped+0x325810)==1.0f);
        memset(speed,0xa5,sizeof(speed)); *(float *)(speed+0x2c)=0.25f; memcpy(saved,speed,sizeof(speed));
        /* Preserving a live x87 caller value also checks accepted/rejected
         * paths have equal stack depth. The native thiscall owns RET4. */
        double preserved;
        __asm__ volatile("fldpi" : : : "memory");
        variable(speed,requests[i]);
        __asm__ volatile("fstpl %0" : "=m"(preserved) : : "memory");
        assert(preserved>3.14159 && preserved<3.14160);
        assert(*(float *)(speed+0x2c)==(i<3u?1.0f:0.25f));
        assert(!memcmp(speed,saved,0x2c)); /* includes modes and full-pause/reference bytes */
    }
    assert(!memcmp(pause_code,mapped+0x27480,sizeof(pause_code)) &&
        !memcmp(mode_code,mapped+0x27190,sizeof(mode_code)));
    assert(SudekiMpLanStoryRealtimeUninstall());
    normal(0.5f); master(0.25f); variable(speed,0.125f);
    assert(*(float *)(mapped+0x345f70)==0.5f && *(float *)(mapped+0x325810)==0.25f &&
        *(float *)(speed+0x2c)==0.125f && !memcmp(mapped+0x2c4018,alternate,4));
    assert(*(void **)(mapped+0x7b66a)==mapped+0x2c4018);
}
int main(int argc,char **argv) {
    assert(argc==2); wchar_t path[1024];
    assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024)); mapped=map_image(path);
    lifecycle_tests(); setter_tests(); VirtualFree(mapped,0,MEM_RELEASE);
    printf("story realtime exact-image policy, %u-source audit, native leaf ABI and rollback tests passed (no gameplay)\n",source_references);
    return 0;
}
