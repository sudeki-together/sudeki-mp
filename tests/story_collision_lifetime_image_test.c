/* Non-executable retail PE mapping + synthetic native-event/ABI fixtures. */
#include "hooks/call_hook.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned installs,fail_install,restores,fail_restore;
static BOOL test_install(SudekiMpInlineHook *,uint8_t *,const uint8_t *,size_t,const void *);
static BOOL test_restore(SudekiMpInlineHook *);
static DWORD test_thread(void);
#define SudekiMpInstallInlineHook test_install
#define SudekiMpRestoreInlineHook test_restore
#define GetCurrentThreadId test_thread
#include "../src/hooks/lan_story_collision_lifetime.c"
#undef GetCurrentThreadId
#undef SudekiMpInstallInlineHook
#undef SudekiMpRestoreInlineHook
static BOOL test_install(SudekiMpInlineHook *h,uint8_t *p,const uint8_t *e,size_t n,const void *r) {
    if(++installs==fail_install) {SetLastError(ERROR_WRITE_FAULT);return FALSE;}
    return SudekiMpInstallInlineHook(h,p,e,n,r);
}
static BOOL test_restore(SudekiMpInlineHook *h) {
    if(++restores==fail_restore) {SetLastError(ERROR_WRITE_FAULT);return FALSE;}
    return SudekiMpRestoreInlineHook(h);
}
static BOOL disturb;
static DWORD test_thread(void) {
    DWORD thread=GetCurrentThreadId();
    if(disturb) {
        SetLastError(0x9999);
        __asm__ volatile("fninit; fld1; pxor %%xmm0,%%xmm0; pxor %%xmm7,%%xmm7" : : : "memory");
    }
    return thread;
}
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(file!=INVALID_HANDLE_VALUE); DWORD size=GetFileSize(file,NULL),got=0;
    uint8_t *raw=malloc(size); assert(raw && ReadFile(file,raw,size,&got,NULL) && got==size); CloseHandle(file);
    IMAGE_DOS_HEADER *dos=(void *)raw;
    IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *base=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(base); memcpy(base,raw,nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *sections=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        IMAGE_SECTION_HEADER *s=&sections[i];
        assert(s->PointerToRawData<=size && s->SizeOfRawData<=size-s->PointerToRawData);
        assert(s->VirtualAddress<=nt->OptionalHeader.SizeOfImage &&
            s->SizeOfRawData<=nt->OptionalHeader.SizeOfImage-s->VirtualAddress);
        memcpy(base+s->VirtualAddress,raw+s->PointerToRawData,s->SizeOfRawData);
    }
    uintptr_t delta=(uintptr_t)base-nt->OptionalHeader.ImageBase;
    IMAGE_DATA_DIRECTORY reloc=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    unsigned offset=0;
    while(offset<reloc.Size) {
        IMAGE_BASE_RELOCATION *block=(void *)(base+reloc.VirtualAddress+offset);
        assert(block->SizeOfBlock>=sizeof(*block) && block->SizeOfBlock<=reloc.Size-offset);
        uint16_t *items=(uint16_t *)(block+1);
        unsigned count=(block->SizeOfBlock-sizeof(*block))/2;
        for(unsigned i=0;i<count;++i) {
            unsigned type=items[i]>>12,rva=block->VirtualAddress+(items[i]&0xfffu);
            assert(type==IMAGE_REL_BASED_ABSOLUTE || type==IMAGE_REL_BASED_HIGHLOW);
            if(type==IMAGE_REL_BASED_HIGHLOW) {
                assert(rva<=nt->OptionalHeader.SizeOfImage-4);
                *(uint32_t *)(base+rva)+=(uint32_t)delta;
            }
        }
        offset+=block->SizeOfBlock;
    }
    free(raw); return base;
}

static uint8_t *mapped;
static uint8_t object[0x110];
static void *call_target __attribute__((used)),*source_target __attribute__((used));
static uint32_t output[9] __attribute__((used)),stack_before __attribute__((used));
static uint32_t deleted_system __attribute__((used)),delete_calls __attribute__((used));
static void __attribute__((naked,noinline,used)) factory(void) {
    __asm__ volatile("push %ebp; mov %esp,%ebp; and $-8,%esp; sub $16,%esp; push %ebx; push %edi;"
        "mov 12(%ebp),%edi; std; stc; jmp _birth_entry");
}
static void __attribute__((naked,noinline,used)) birth_end(void) { __asm__ volatile("ret $8"); }
static void __attribute__((naked,noinline,used)) delete_end(void) {
    __asm__ volatile("mov %ebp,_deleted_system; incl _delete_calls; pop %ebp; add $0x18,%esp; ret $8");
}
static void __attribute__((naked,noinline,used)) invoke(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,_stack_before;"
        "mov $0x11111111,%eax; mov $0x22222222,%ecx; mov $0x33333333,%edx;"
        "mov $0x44444444,%ebx; mov $0x55555555,%ebp; mov $0x66666666,%esi;"
        "mov $0x77777777,%edi; push _source_target; push $0x44556677; std; stc; call *_call_target;"
        "mov %eax,_output; mov %ecx,_output+4; mov %edx,_output+8; mov %ebx,_output+12;"
        "mov %ebp,_output+16; mov %esi,_output+20; mov %edi,_output+24;"
        "pushfl; pop _output+28; mov %esp,_output+32; popal; popfl; ret");
}
static void abi_call(BOOL birth) {
    uint8_t before[512] __attribute__((aligned(16)))={0};
    uint8_t after[512] __attribute__((aligned(16)))={0};
    uint16_t control=0x077f; uint32_t mxcsr=0x3f80;
    call_target=(void *)(uintptr_t)(birth?factory:delete_entry); source_target=object;
    birth_resume=(void *)(uintptr_t)birth_end; delete_resume=(void *)(uintptr_t)delete_end;
    disturb=TRUE; SetLastError(0x7788);
    __asm__ volatile("fninit; fldcw %1; ldmxcsr %2; fldpi; fld1; fldl2t;"
        "pcmpeqd %%xmm0,%%xmm0; pcmpeqd %%xmm7,%%xmm7; fxsave %0"
        : "=m"(before) : "m"(control),"m"(mxcsr) : "memory");
    invoke();
    __asm__ volatile("fxsave %0; fninit" : "=m"(after) : : "memory");
    disturb=FALSE;
    assert(GetLastError()==0x7788 && !memcmp(before,after,160) && !memcmp(before+160,after+160,128));
    assert(output[0]==(birth?(uintptr_t)object:0x11111111));
    for(unsigned i=1;i<7;++i) assert(output[i]==0x11111111u*(i+1));
    assert((output[7]&0x400) && output[8]==stack_before);
    if(birth) assert(output[7]&1);
    else assert(deleted_system==0x44556677 && delete_calls);
}
static void install(void) {
    installs=restores=fail_install=fail_restore=0;
    assert(SudekiMpLanStoryCollisionLifetimeInstall((HMODULE)mapped));
}
static void refused(void) {
    SudekiMpLanStoryCollisionStamp out={123,456},before=out; uint64_t v=888;
    assert(!SudekiMpLanStoryCollisionLifetimeObserve((HMODULE)mapped,object,&out));
    assert(!memcmp(&before,&out,sizeof(out)));
    assert(!SudekiMpLanStoryCollisionLifetimeRevision((HMODULE)mapped,&v) && v==888);
}
/* Independent synthetic fault cases share one executable. Production has no
 * reset API: each fault is first proved sticky/pinned; only this test fixture
 * clears it after all fake callbacks/threads have positively returned. */
static void finish_fault(void) {
    assert(unknown && !callbacks);
    refused();
    assert(!SudekiMpLanStoryCollisionLifetimeUninstall());
    assert(!SudekiMpLanStoryCollisionLifetimeInstall((HMODULE)mapped));
    InterlockedExchange(&unknown,0);
    assert(SudekiMpLanStoryCollisionLifetimeUninstall());
}
static DWORD WINAPI foreign_event(void *p) { observed(p,TRUE); return 0; }
static void failures(void) {
    for(unsigned n=1;n<=2;++n) {
        installs=restores=0;fail_install=n;
        assert(!SudekiMpLanStoryCollisionLifetimeInstall((HMODULE)mapped));
        assert(!base && !installed && !birth_hook.installed && !delete_hook.installed);
    }
    fail_install=0;
    unsigned changed[]={0x314e0,0x314f0,0x314fe,BIRTH,BIRTH+7,SOURCE_DELETE,SOURCE_DELETE+4,0x319cc,0x319db};
    for(unsigned i=0;i<sizeof(changed)/sizeof(*changed);++i) {
        mapped[changed[i]]^=1; installs=0;
        assert(!SudekiMpLanStoryCollisionLifetimeInstall((HMODULE)mapped) && !installs);
        mapped[changed[i]]^=1;
    }
    unsigned globals[]={0x408d10,0x408dd4};
    for(unsigned i=0;i<2;++i) {
        *(void **)(mapped+globals[i])=object; installs=0;
        assert(!SudekiMpLanStoryCollisionLifetimeInstall((HMODULE)mapped) && !installs);
        *(void **)(mapped+globals[i])=NULL;
    }
    IMAGE_DOS_HEADER *dos=(void *)mapped; LONG offset=dos->e_lfanew; dos->e_lfanew=0x7fffffff;
    assert(!SudekiMpLanStoryCollisionLifetimeInstall((HMODULE)mapped)); dos->e_lfanew=offset;
    for(unsigned n=1;n<=2;++n) {
        install(); restores=0;fail_restore=n;
        assert(!SudekiMpLanStoryCollisionLifetimeUninstall() && base && birth_resume && delete_resume);
        assert(restores==2 && GetLastError()==ERROR_WRITE_FAULT);
        fail_restore=0;assert(SudekiMpLanStoryCollisionLifetimeUninstall());
    }
    installs=restores=0;fail_install=2;fail_restore=2;
    assert(!SudekiMpLanStoryCollisionLifetimeInstall((HMODULE)mapped) && base && delete_hook.installed);
    fail_install=fail_restore=0;assert(SudekiMpLanStoryCollisionLifetimeUninstall());
    install(); uint8_t saved[7];memcpy(saved,mapped+BIRTH,7);mapped[BIRTH]=0xcc;
    assert(!SudekiMpLanStoryCollisionLifetimeUninstall() && birth_hook.installed && !delete_hook.installed);
    memcpy(mapped+BIRTH,saved,7);assert(SudekiMpLanStoryCollisionLifetimeUninstall());
}
static void identity(void) {
    install(); refused(); /* no native thread yet */
    assert(SudekiMpLanStoryCollisionLifetimeAttach((HMODULE)mapped,object));
    assert(!SudekiMpLanStoryCollisionLifetimeUninstall());
    assert(!SudekiMpLanStoryCollisionLifetimeDetach((HMODULE)mapped,object+1));
    assert(!SudekiMpLanStoryCollisionLifetimeAttach((HMODULE)mapped,object+1));
    abi_call(TRUE); SudekiMpLanStoryCollisionStamp first,second;
    assert(SudekiMpLanStoryCollisionLifetimeObserve((HMODULE)mapped,object,&first));
    assert(SudekiMpLanStoryCollisionLifetimeMatches(object,first));
    callbacks=1;refused();assert(!SudekiMpLanStoryCollisionLifetimeUninstall());callbacks=0;
    abi_call(FALSE);assert(!SudekiMpLanStoryCollisionLifetimeMatches(object,first));
    second=first;assert(!SudekiMpLanStoryCollisionLifetimeObserve((HMODULE)mapped,object,&second));
    assert(!memcmp(&first,&second,sizeof(first)));
    abi_call(TRUE);assert(SudekiMpLanStoryCollisionLifetimeObserve((HMODULE)mapped,object,&second));
    assert(second.incarnation>first.incarnation && second.revision>first.revision);
    assert(!SudekiMpLanStoryCollisionLifetimeMatches(object,first));
    assert(SudekiMpLanStoryCollisionLifetimeMatches(object,second));
    assert(SudekiMpLanStoryCollisionLifetimeDetach((HMODULE)mapped,object));
    assert(SudekiMpLanStoryCollisionLifetimeUninstall());
    assert(!SudekiMpLanStoryCollisionLifetimeMatches(object,second));
    install();observed(object,TRUE);
    assert(SudekiMpLanStoryCollisionLifetimeObserve((HMODULE)mapped,object,&first));
    assert(first.incarnation>second.incarnation); /* never reuse identities across reinstall */
    assert(SudekiMpLanStoryCollisionLifetimeUninstall());
}
static void unknowns(void) {
    install();observed(object,FALSE);finish_fault(); /* unseen delete */
    install();observed(object,TRUE);observed(object,TRUE);finish_fault(); /* duplicate live birth */
    install();observed(NULL,TRUE);finish_fault();
    install();observed(object,TRUE);
    HANDLE h=CreateThread(NULL,0,foreign_event,object+16,0,NULL);assert(h);
    assert(WaitForSingleObject(h,5000)==WAIT_OBJECT_0);CloseHandle(h);finish_fault();
    install();
    for(unsigned i=0;i<CELLS;++i) observed((void *)(uintptr_t)(0x10000u+16*i),TRUE);
    assert(!unknown);
    observed((void *)(uintptr_t)0x900000,TRUE);finish_fault();
    install();uint64_t saved=revision;revision=UINT64_MAX;observed(object,TRUE);
    finish_fault();revision=saved; /* fixture-only fresh counter domain */
    install();
    for(unsigned i=0;i<CELLS+32;++i) {
        void *p=(void *)(uintptr_t)(0x10000u+16*i);observed(p,TRUE);observed(p,FALSE);
    }
    assert(!unknown);assert(SudekiMpLanStoryCollisionLifetimeUninstall()); /* tombstones reusable */
}
int main(int argc,char **argv) {
    assert(argc==2);wchar_t path[1024];
    assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024));mapped=map_image(path);
    assert(!SudekiMpLanStoryCollisionLifetimeInstall(NULL));
    failures();identity();unknowns();
    assert(!base && !installed && !callbacks && !consumer);
    assert(!memcmp(mapped+BIRTH,birth_bytes,sizeof(birth_bytes)));
    assert(!memcmp(mapped+SOURCE_DELETE,delete_bytes,sizeof(delete_bytes)));
    VirtualFree(mapped,0,MEM_RELEASE);
    puts("story collision lifetime exact-image/synthetic incarnation, ABI, foreign-event and teardown tests passed (no native gameplay)");
    return 0;
}
