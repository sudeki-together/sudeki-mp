/* Exact-image sky hooks plus inert client light ownership/restore fixtures.
 * No campaign, native lighting tick, or graphics device is executed here. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "engine/cast_light_abi.h"
#include "../src/hooks/lan_story_sky.c"
void SudekiMpLogWrite(const char *message) { (void)message; }
void SudekiMpLogFormat(const char *format,...) { (void)format; }
BOOL SudekiMpLogResearchEnabled(void) { return FALSE; }
BOOL SudekiMpLanStoryWithResourceName(const char *name,unsigned kind,
    void (*use)(void *,void *),void *context) {
    (void)name; (void)kind; (void)use; (void)context; return FALSE;
}
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(file!=INVALID_HANDLE_VALUE); DWORD size=GetFileSize(file,NULL),got=0;
    uint8_t *raw=malloc(size); assert(raw && ReadFile(file,raw,size,&got,NULL) && got==size); CloseHandle(file);
    IMAGE_DOS_HEADER *dos=(void *)raw;
    IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *base=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
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
    assert(FlushInstructionCache(GetCurrentProcess(),base,nt->OptionalHeader.SizeOfImage));
    free(raw); return base;
}

int main(int argc,char **argv) {
    wchar_t path[32768];
    assert(argc==2 && MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,32768));
    uint8_t *image=map_image(path),scene_object[0x80]={0};
    uint8_t *manager=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    assert(manager);
    *(void **)manager=image+RVA_LIGHT_VTABLE;
    *(void **)(image+LIGHT_MANAGER_GLOBAL)=manager;
    *(void **)(image+SCENE_GLOBAL)=scene_object;
    const float original[3]={.2f,.3f,.3f};
    memcpy(manager+0x34,original,12); memcpy(manager+0x40,original,12);
    *(float *)(manager+0x30)=.4f;
    assert(SudekiMpLanStorySkyInstall((HMODULE)image));
    /* The retail vtable and named Update prefix both gate target admission. */
    assert(light_target_owner(manager));
    SudekiMpStoryEffectState host={0}; host.light_valid=1;
    host.light_target[0]=bits(.6f); host.light_target[1]=bits(.75f);
    host.light_target[2]=bits(1.1f); host.light_rate=bits(.3f);
    light_state_follow(&host);
    assert(light_lease.manager==manager);
    assert(!memcmp(manager+0x40,host.light_target,12));
    assert(!memcmp(manager+0x34,original,12)); /* No manual interpolation. */
    host.light_rate=bits(.7f); light_state_follow(&host);
    assert(bits(*(float *)(manager+0x30))==bits(.7f)); /* A rate-only change is not lost. */
    /* Invalid/missing observations retain the last lease, not a false exit. */
    host.light_valid=0; light_state_follow(&host);
    assert(light_lease.manager==manager && bits(*(float *)(manager+0x30))==bits(.7f));
    host.light_valid=1; host.light_rate=bits(NAN); light_state_follow(&host);
    assert(bits(*(float *)(manager+0x30))==bits(.7f));
    host.light_rate=bits(.7f);
    /* Foreign/native owner and thread changes retain for retry, with no writes. */
    void *update_slot=*(void **)(image+RVA_LIGHT_VTABLE+4);
    *(void **)(image+RVA_LIGHT_VTABLE+4)=scene_object;
    assert(!SudekiMpLanStorySkyReleaseLighting());
    assert(!SudekiMpLanStorySkyUninstall() && dome_hook.installed);
    *(void **)(image+RVA_LIGHT_VTABLE+4)=update_slot;
    native_thread++; assert(!SudekiMpLanStorySkyReleaseLighting()); native_thread--;
    *(void **)(image+SCENE_GLOBAL)=manager;
    assert(!SudekiMpLanStorySkyReleaseLighting());
    *(void **)(image+SCENE_GLOBAL)=scene_object;
    *(void **)(image+LIGHT_MANAGER_GLOBAL)=NULL;
    assert(!SudekiMpLanStorySkyReleaseLighting());
    *(void **)(image+LIGHT_MANAGER_GLOBAL)=manager;
    *(void **)manager=image; assert(!SudekiMpLanStorySkyReleaseLighting());
    *(void **)manager=image+RVA_LIGHT_VTABLE;
    DWORD old;
    assert(VirtualProtect(manager,4096,PAGE_READONLY,&old));
    assert(!SudekiMpLanStorySkyReleaseLighting());
    assert(VirtualProtect(manager,4096,old,&old));
    image[RVA_LIGHT_UPDATE]^=1;
    assert(!SudekiMpLanStorySkyReleaseLighting()); image[RVA_LIGHT_UPDATE]^=1;
    assert(SudekiMpLanStorySkyReleaseLighting() && !light_lease.manager);
    assert(!memcmp(manager+0x40,original,12) && bits(*(float *)(manager+0x30))==bits(.4f));
    /* A later native writer wins independently for each field. */
    light_state_follow(&host); *(float *)(manager+0x30)=.9f;
    assert(SudekiMpLanStorySkyReleaseLighting());
    assert(bits(*(float *)(manager+0x30))==bits(.9f) && !memcmp(manager+0x40,original,12));
    light_state_follow(&host); memcpy(manager+0x40,original,12);
    assert(SudekiMpLanStorySkyReleaseLighting());
    assert(bits(*(float *)(manager+0x30))==bits(.9f) && !memcmp(manager+0x40,original,12));
    /* Continued following remembers a newer local baseline for eventual exit. */
    light_state_follow(&host); *(float *)(manager+0x30)=.8f;
    light_state_follow(&host); assert(bits(*(float *)(manager+0x30))==bits(.7f));
    assert(SudekiMpLanStorySkyUninstall());
    assert(!light_lease.manager && bits(*(float *)(manager+0x30))==bits(.8f));
    assert(!memcmp(manager+0x34,original,12) && !memcmp(manager+0x40,original,12));
    /* Host story sky capture and private casting must coexist. No native
     * constructor, task or lighting tick runs: this proves exact ABI guards,
     * both hook owners and restoration using an inert scheduler owner. */
    assert(SudekiMpLanStorySkyInstall((HMODULE)image));
    uint32_t scheduler[2]={0};
    *(void **)(image+0x409e14u)=scheduler;
    assert(SudekiMpInitializeCastLightAbi((HMODULE)image));
    assert(*(void **)(image+RVA_LIGHT_VTABLE+4)!=update_slot);
    assert(SudekiMpResetCastLightAbi());
    assert(*(void **)(image+RVA_LIGHT_VTABLE+4)==update_slot);
    assert(light_target_owner(manager));
    /* Named native contract: CLightManager::SetLightingChangeRate, 1F740.
     * A corrupt setter still rejects casting; sky never grants an exception. */
    image[0x1f740u]^=1;
    assert(!SudekiMpInitializeCastLightAbi((HMODULE)image));
    image[0x1f740u]^=1;
    assert(SudekiMpInitializeCastLightAbi((HMODULE)image));
    assert(SudekiMpResetCastLightAbi());
    /* Capture the rate itself, including a rate-only change, without a setter
     * call event. The existing client lease applies this state above. */
    memset(&host,0,sizeof(host));
    *(float *)(manager+0x30)=.65f;
    SudekiMpLanStorySkyCapture(&host);
    assert(host.light_valid && host.light_rate==bits(.65f));
    assert(!memcmp(host.light_target,manager+0x40,12));
    assert(SudekiMpLanStorySkyUninstall());
    for(unsigned i=0;i<SUDEKIMP_STORY_SKY_KINDS;++i)
        assert(!memcmp(image+rvas[i],entries[i],lengths[i]));
    for(unsigned i=0;i<LIGHT_HOOKS;++i)
        assert(!memcmp(image+light_rvas[i],light_entries[i],light_lengths[i]));
    assert(!memcmp(image+RVA_SKY_DOME,dome_entry,sizeof(dome_entry)));
    assert(*(void **)(image+RVA_LIGHT_VTABLE+4)==update_slot);
    VirtualFree(manager,0,MEM_RELEASE); VirtualFree(image,0,MEM_RELEASE);
    puts("Story sky/light target exact-image tests: PASS"); return 0;
}
