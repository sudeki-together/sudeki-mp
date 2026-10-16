/* Supported-image proof for the existing armour adapter's Weapons refresh
 * entries. No game UI or live gameplay is executed by this fixture. */
#include "engine/build_identity.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/hooks/armour_menu.c"

void SudekiMpLogFormat(const char *fmt,...) {(void)fmt;}
void SudekiMpLogWrite(const char *text) {(void)text;}
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

static void mutate_byte(void *p) {
    DWORD old,ignored;
    assert(VirtualProtect(p,1,PAGE_EXECUTE_READWRITE,&old));
    *(uint8_t *)p ^= 1;
    assert(VirtualProtect(p,1,old,&ignored));
    assert(FlushInstructionCache(GetCurrentProcess(),p,1));
}
static void byte_guards(uint8_t *b,const char *mode) {
    static const struct {const char *name;unsigned rva,length;} guards[] = {
        {"WeaponsRefreshFill",LIST_FILL,32}, {"WeaponsRefreshDetails",DETAILS,12}
    };
    for(unsigned i=0;i<sizeof(guards)/sizeof(*guards);++i) {
        for(unsigned j=0;j<guards[i].length;++j) {
            mutate_byte(b+guards[i].rva+j);
            assert(!SudekiMpArmourMenuWeaponsRefreshExact((HMODULE)b,NULL));
            mutate_byte(b+guards[i].rva+j);
            assert(SudekiMpArmourMenuWeaponsRefreshExact((HMODULE)b,NULL));
        }
        printf("PASS named %s %s every byte refused when changed\n",guards[i].name,mode);
    }
}
static void ownership_guards(uint8_t *b,SudekiMpInlineHook *hook,void **published,const char *name) {
    uint8_t *trampoline=hook->trampoline;
    SudekiMpInlineHook saved=*hook;
    void *published_saved=*published;
    for(unsigned j=0;j<11;++j) {
        mutate_byte(trampoline+j);
        assert(!SudekiMpArmourMenuWeaponsRefreshExact((HMODULE)b,NULL));
        mutate_byte(trampoline+j);
    }
    for(unsigned j=0;j<6;++j) {
        hook->original[j]^=1;
        assert(!SudekiMpArmourMenuWeaponsRefreshExact((HMODULE)b,NULL));
        *hook=saved;
        hook->replacement[j]^=1;
        assert(!SudekiMpArmourMenuWeaponsRefreshExact((HMODULE)b,NULL));
        *hook=saved;
    }
    hook->length=7;
    assert(!SudekiMpArmourMenuWeaponsRefreshExact((HMODULE)b,NULL));*hook=saved;
    hook->target++;
    assert(!SudekiMpArmourMenuWeaponsRefreshExact((HMODULE)b,NULL));*hook=saved;
    hook->installed=FALSE;
    assert(!SudekiMpArmourMenuWeaponsRefreshExact((HMODULE)b,NULL));*hook=saved;
    hook->trampoline=NULL;
    assert(!SudekiMpArmourMenuWeaponsRefreshExact((HMODULE)b,NULL));*hook=saved;
    *published=trampoline+1;
    assert(!SudekiMpArmourMenuWeaponsRefreshExact((HMODULE)b,NULL));*published=published_saved;
    DWORD old,ignored;
    assert(VirtualProtect(trampoline,11,PAGE_READWRITE,&old));
    assert(!SudekiMpArmourMenuWeaponsRefreshExact((HMODULE)b,NULL));
    assert(VirtualProtect(trampoline,11,old,&ignored));
    /* Original bytes with an installed lease are not this owner's detour. */
    memcpy(b+(hook->target-b),hook->original,6);
    assert(!SudekiMpArmourMenuWeaponsRefreshExact((HMODULE)b,NULL));
    memcpy(b+(hook->target-b),hook->replacement,6);
    assert(SudekiMpArmourMenuWeaponsRefreshExact((HMODULE)b,NULL));
    printf("PASS named %s retained trampoline, owner metadata and executable protection\n",name);
}
static void no_new_patch(uint8_t *b) {
    uint8_t *snapshot=malloc(SUDEKIMP_EXPECTED_IMAGE_SIZE);assert(snapshot);
    memcpy(snapshot,b,SUDEKIMP_EXPECTED_IMAGE_SIZE);
    SudekiMpInlineHook old_fill=fill_hook,old_details=details_hook;
    SetLastError(0x12345678);
    assert(SudekiMpArmourMenuWeaponsRefreshExact((HMODULE)b,NULL));
    assert(GetLastError()==0x12345678);
    assert(!memcmp(snapshot,b,SUDEKIMP_EXPECTED_IMAGE_SIZE));
    assert(!memcmp(&old_fill,&fill_hook,sizeof(fill_hook)));
    assert(!memcmp(&old_details,&details_hook,sizeof(details_hook)));
    free(snapshot);
}
int main(int argc,char **argv) {
    wchar_t path[4096],directory[MAX_PATH],config[MAX_PATH];
    assert(argc==2 && MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,4096));
    uint8_t *b=map_image(path),*other=map_image(path);
    assert(!SudekiMpArmourMenuWeaponsRefreshExact(NULL,NULL));
    assert(SudekiMpArmourMenuWeaponsRefreshExact((HMODULE)b,NULL));
    no_new_patch(b);byte_guards(b,"original");
    /* A foreign JMP is rejected even when it lands on a valid native entry. */
    uint8_t saved[6];memcpy(saved,b+LIST_FILL,6);
    b[LIST_FILL]=0xe9;
    int32_t rel=(int32_t)((b+DETAILS)-(b+LIST_FILL+5));
    memcpy(b+LIST_FILL+1,&rel,4);b[LIST_FILL+5]=0x90;
    assert(!SudekiMpArmourMenuWeaponsRefreshExact((HMODULE)b,NULL));
    memcpy(b+LIST_FILL,saved,6);
    puts("PASS named WeaponsRefreshForeignDetour rejected without owner state");
    assert(GetTempPathW(MAX_PATH,directory) && GetTempFileNameW(directory,L"arm",0,config));
    assert(SudekiMpArmourMenuInstall((HMODULE)b,config));
    assert(SudekiMpArmourMenuActive());
    assert(SudekiMpArmourMenuWeaponsRefreshExact((HMODULE)b,NULL));
    no_new_patch(b);byte_guards(b,"owned");
    ownership_guards(b,&fill_hook,&SudekiMpArmourMenuFillTrampoline,"WeaponsRefreshFillOwner");
    ownership_guards(b,&details_hook,&SudekiMpArmourMenuDetailsTrampoline,"WeaponsRefreshDetailsOwner");
    assert(!SudekiMpArmourMenuWeaponsRefreshExact((HMODULE)other,NULL));
    uint8_t *saved_base=base;base=NULL;
    assert(!SudekiMpArmourMenuWeaponsRefreshExact((HMODULE)b,NULL));base=saved_base;
    puts("PASS named WeaponsRefreshImageOwner rejects different image and lost ownership");
    uint8_t weapons_layer[4],armour_layer[4];
    *(void **)weapons_layer=b+0x2d8904;*(void **)armour_layer=b+0x2d8904;
    SudekiMpArmourMenuLayer=armour_layer;
    assert(!SudekiMpArmourMenuWeaponsRefreshExact((HMODULE)b,armour_layer));
    assert(SudekiMpArmourMenuWeaponsRefreshExact((HMODULE)b,weapons_layer));
    assert(SudekiMpArmourMenuWeaponsRefreshExact((HMODULE)b,NULL));
    SudekiMpArmourMenuLayer=NULL;
    puts("PASS named WeaponsRefreshArmourExclusion rejects only owned armour instance");
    assert(SudekiMpArmourMenuUninstall());
    assert(SudekiMpArmourMenuWeaponsRefreshExact((HMODULE)b,NULL));
    no_new_patch(b);assert(DeleteFileW(config));
    assert(VirtualFree(other,0,MEM_RELEASE) && VirtualFree(b,0,MEM_RELEASE));
    puts("PASS named WeaponsRefreshNoNewPatch original/owned/restored proof changes no code or hooks");
    return 0;
}
