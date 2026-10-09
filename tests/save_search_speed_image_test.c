/* Maps the supported SUDEKI.exe as data (not executed) and checks the
 * [SudekiMP] FastSaveSearch seams: exact bytes at the wait load and the
 * catalog step call, the bounded one-tick scan loop (with a fake step), a
 * byte-identical restore, and rollback when one seam's bytes differ.
 * Usage: SudekiMP.SaveSearchSpeedImageTest <SUDEKI.exe> */
#include "../src/hooks/save_search_speed.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(f!=INVALID_HANDLE_VALUE);DWORD size=GetFileSize(f,NULL),got=0;
    uint8_t *raw=malloc(size);assert(raw && ReadFile(f,raw,size,&got,NULL) && got==size);CloseHandle(f);
    IMAGE_DOS_HEADER *dos=(void *)raw;IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *b=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(b);memcpy(b,raw,nt->OptionalHeader.SizeOfHeaders);IMAGE_SECTION_HEADER *s=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        assert(s[i].PointerToRawData<=size && s[i].SizeOfRawData<=size-s[i].PointerToRawData);
        memcpy(b+s[i].VirtualAddress,raw+s[i].PointerToRawData,s[i].SizeOfRawData);
    }
    IMAGE_DATA_DIRECTORY reloc=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    uintptr_t delta=(uintptr_t)b-nt->OptionalHeader.ImageBase;
    for(unsigned offset=0;offset<reloc.Size;) {
        IMAGE_BASE_RELOCATION *block=(void *)(b+reloc.VirtualAddress+offset);
        uint16_t *items=(void *)(block+1);unsigned count=(block->SizeOfBlock-sizeof(*block))/2;
        for(unsigned i=0;i<count;++i) {
            unsigned type=items[i]>>12,rva=block->VirtualAddress+(items[i]&0xfff);
            if(type==IMAGE_REL_BASED_HIGHLOW) *(uint32_t *)(b+rva)+=(uint32_t)delta;
        }
        offset+=block->SizeOfBlock;
    }
    free(raw);return b;
}

static uint32_t fake_catalog; static unsigned fake_calls,fake_finish_at;
static uint8_t __cdecl fake_step(void) {
    ++fake_calls;
    if(fake_catalog==1u) fake_catalog=2u;
    else if(fake_catalog==2u && fake_calls>=7u) fake_catalog=3u;
    else if(fake_catalog==3u && fake_calls>=fake_finish_at) { fake_catalog=5u; return 1; }
    return fake_catalog==5u;
}

int wmain(int argc,wchar_t **argv) {
    assert(argc==2);
    uint8_t *b=map_image(argv[1]);
    uint8_t *call=b+RVA_STEP_CALL, *load=b+RVA_WAIT_LOAD;
    uint8_t call_before[5],load_before[6];
    memcpy(call_before,call,5); memcpy(load_before,load,6);
    assert(call[0]==0xe8 && call+5+*(int32_t *)(call+1)==b+RVA_CATALOG_STEP);
    assert(!memcmp(load,wait_load_bytes,6));
    assert(!memcmp(b+RVA_WAIT_STORE,"\xd9\x9b\x94\x03\x00\x00",6));   /* FSTP [EBX+0x394] */

    assert(!SudekiMpSaveSearchSpeedInstall((HMODULE)b,-1.0f) && !memcmp(call,call_before,5));
    assert(SudekiMpSaveSearchSpeedInstall((HMODULE)b,0.25f));
    assert(call+5+*(int32_t *)(call+1)==(uint8_t *)step_until_done);
    assert(load[0]==0xe9 && load+5+*(int32_t *)(load+1)==(uint8_t *)wait_load && load[5]==0x90);
    assert(save_search_wait_seconds==0.25f && save_search_wait_resume==b+RVA_WAIT_STORE);

    /* The whole native scan in one call; result and order are the native step's. */
    catalog_step=fake_step; catalog_state=&fake_catalog;
    fake_catalog=1u; fake_calls=0; fake_finish_at=20u;
    assert(step_until_done()==1 && fake_calls==20u && fake_catalog==5u);
    /* Idle or unknown catalog phases are not spun: one native call, as before. */
    fake_catalog=0u; fake_calls=0;
    assert(step_until_done()==0 && fake_calls==1u);
    fake_catalog=4u; fake_calls=0;
    assert(step_until_done()==0 && fake_calls==1u);
    /* A scan that never finishes is bounded. */
    fake_catalog=1u; fake_calls=0; fake_finish_at=UINT_MAX;
    assert(step_until_done()==0 && fake_calls==MAX_STEPS_PER_TICK);

    assert(SudekiMpSaveSearchSpeedUninstall());
    assert(!memcmp(call,call_before,5) && !memcmp(load,load_before,6));

    /* Changed bytes at the wait seam: refused and the step seam rolled back. */
    load[2]^=0xff;
    assert(!SudekiMpSaveSearchSpeedInstall((HMODULE)b,0.0f) && !memcmp(call,call_before,5));
    puts("save search speed image test passed");
    return 0;
}
