/* Synthetic execution on a mapped, identity-checked supported image. Keep
 * the guarded native prologues and calling conventions, but replace the
 * unguarded UI bodies with spies: no game objects, renderer, or gameplay run.
 * This proves the adapter algorithm/ABI, not visible native menu behavior. */
#include "engine/build_identity.h"
#include "engine/weapon_activation_abi.h"
#include "hooks/armour_menu.h"
#include "hooks/lan_story_main_menu.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    FILL=0x16fc60u, DETAILS=0x16f5b0u, SELECT=0x17d3c0u, CLEAR=0x17d720u,
    LIST=0x7d0u, SELECTED=0x858u, COUNT=0x884u, ROWS=0x88cu,
    REORDER, REPLACE_LAYER, REPLACE_WIDGETS, REPLACE_ACTOR, REMOVE_SELECTED
};
typedef void (__attribute__((stdcall)) *LayerCall)(void *);
extern void *volatile SudekiMpArmourMenuLayer;
static uint8_t *image;
static uint8_t ui[0x220],group[0xd0],front[0x174],layer[0x15c],other_layer[0x15c];
static uint8_t widgets[0x900],other_widgets[0x900],row_storage[3][0x88];
static uint8_t actor[4],other_actor[4];
static uint8_t *old_rows[2],*new_rows[3];
static BOOL describe_ok;
static unsigned describe_calls,fill_calls,select_calls,details_calls,mode;
static unsigned detail_items[4];

void SudekiMpLogFormat(const char *fmt,...) {(void)fmt;}
void SudekiMpLogWrite(const char *text) {(void)text;}
BOOL SudekiMpDescribeCharacterWeapons(void *character,SudekiMpWeaponQuickList *out) {
    ++describe_calls;
    assert(character==actor && out);
    memset(out,0,sizeof(*out));
    out->inventory_category=4;
    out->row_count=3;
    return describe_ok;
}
static BOOL allow_actor(void *character) {return character==actor;}
static unsigned selected_item(void) {
    int selected=*(int *)(widgets+SELECTED);
    unsigned count=*(unsigned *)(widgets+COUNT);
    uint8_t **rows=*(uint8_t ***)(widgets+ROWS);
    assert(selected>=0 && (unsigned)selected<count);
    return *(unsigned *)(rows[selected]+0x84);
}
static void __attribute__((used,noinline,force_align_arg_pointer)) details_spy(void *page) {
    assert(page==layer && details_calls<4);
    detail_items[details_calls++]=selected_item();
}
static void __attribute__((used,noinline,force_align_arg_pointer)) select_spy(void *list,int row) {
    assert(list==widgets+LIST && row>=0 && (unsigned)row<*(unsigned *)(widgets+COUNT));
    ++select_calls;
    *(int *)(widgets+SELECTED)=row;
}
static void __attribute__((used,noinline,force_align_arg_pointer)) fill_spy(void *page) {
    assert(page==layer);
    ++fill_calls;
    *(uint8_t ***)(widgets+ROWS)=new_rows;
    *(unsigned *)(widgets+COUNT)=mode==REMOVE_SELECTED?2u:3u;
    /* Model native fill's equipped-row default and its details call. The
     * old highlight is item 3; item 1 is equipped, now at row 2. */
    *(int *)(widgets+SELECTED)=mode==REMOVE_SELECTED?1:2;
    ((LayerCall)(image+DETAILS))(page);
    if(mode==REPLACE_LAYER) *(void **)(front+0x170)=other_layer;
    if(mode==REPLACE_WIDGETS) *(void **)(layer+0x8c)=other_widgets;
    if(mode==REPLACE_ACTOR) *(void **)(group+0x90)=other_actor;
}
/* The real fill prologue has saved EBX/ESI/EDI; the details prologue only
 * saved EBP. Each C spy realigns its own stack and preserves nonvolatiles. */
static void __attribute__((naked,noinline)) fill_tail(void) {
    __asm__ volatile("pushl 8(%ebp); call _fill_spy; addl $4,%esp;"
        "popl %edi; popl %esi; popl %ebx; movl %ebp,%esp; popl %ebp; ret $4");
}
static void __attribute__((naked,noinline)) details_tail(void) {
    __asm__ volatile("pushl 8(%ebp); call _details_spy; addl $4,%esp;"
        "movl %ebp,%esp; popl %ebp; ret $4");
}
static void __attribute__((naked,noinline)) select_tail(void) {
    __asm__ volatile("pushl 16(%esp); pushl %esi; call _select_spy; addl $8,%esp;"
        "popl %esi; popl %ebp; popl %ebx; ret $4");
}
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(file!=INVALID_HANDLE_VALUE);
    DWORD size=GetFileSize(file,NULL),got=0;
    uint8_t *raw=malloc(size);
    assert(raw && ReadFile(file,raw,size,&got,NULL) && got==size);
    CloseHandle(file);
    IMAGE_DOS_HEADER *dos=(void *)raw;
    IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *b=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    assert(b);
    memcpy(b,raw,nt->OptionalHeader.SizeOfHeaders);
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
        uint16_t *items=(void *)(block+1);
        unsigned count=(block->SizeOfBlock-sizeof(*block))/2;
        for(unsigned i=0;i<count;++i) {
            unsigned type=items[i]>>12,rva=block->VirtualAddress+(items[i]&0xfff);
            assert(type==IMAGE_REL_BASED_ABSOLUTE || type==IMAGE_REL_BASED_HIGHLOW);
            if(type==IMAGE_REL_BASED_HIGHLOW) {
                assert(rva<=nt->OptionalHeader.SizeOfImage-4);
                *(uint32_t *)(b+rva)+=(uint32_t)delta;
            }
        }
        offset+=block->SizeOfBlock;
    }
    free(raw);
    return b;
}
static void jump_to(uint8_t *site,const void *target) {
    int32_t displacement=(int32_t)((uintptr_t)target-(uintptr_t)(site+5));
    site[0]=0xe9;
    memcpy(site+1,&displacement,4);
}
/* PollKeys uses physical key state only when this process owns the foreground
 * window. This fixture creates no windows and requires that unfocused path. */
static void poll(void) {
    DWORD owner=0;
    HWND foreground=GetForegroundWindow();
    if(foreground) assert(GetWindowThreadProcessId(foreground,&owner));
    assert(owner!=GetCurrentProcessId());
    SudekiMpLanStoryMainMenuPollKeys();
}
static void reset(void) {
    ui[0x29]=0;
    poll(); /* Drop pending refresh from the previous synthetic page. */
    memset(ui,0,sizeof(ui));memset(group,0,sizeof(group));memset(front,0,sizeof(front));
    memset(layer,0,sizeof(layer));memset(other_layer,0,sizeof(other_layer));
    memset(widgets,0,sizeof(widgets));memset(other_widgets,0,sizeof(other_widgets));
    memset(row_storage,0,sizeof(row_storage));memset(detail_items,0,sizeof(detail_items));
    *(void **)(image+0x3c2f78)=ui;
    *(void **)(image+0x408d94)=group;
    *(void **)(image+0x408d1c)=front;
    *(void **)ui=image+0x2cae0c;ui[0x29]=1;
    *(unsigned *)(group+0xcc)=1;*(void **)(group+0x90)=actor;
    *(void **)(front+0x170)=layer;
    *(void **)layer=image+0x2d8904;*(unsigned *)(layer+0x48)=2;
    *(void **)(layer+0x8c)=widgets;
    *(void **)other_layer=image+0x2d8904;
    *(void **)(widgets+LIST)=image+0x2d8cf4;
    *(unsigned *)(row_storage[0]+0x84)=1;
    *(unsigned *)(row_storage[1]+0x84)=3;
    *(unsigned *)(row_storage[2]+0x84)=7;
    old_rows[0]=row_storage[0];old_rows[1]=row_storage[1];
    new_rows[0]=row_storage[1];new_rows[1]=row_storage[2];new_rows[2]=row_storage[0];
    *(uint8_t ***)(widgets+ROWS)=old_rows;
    *(unsigned *)(widgets+COUNT)=2;*(int *)(widgets+SELECTED)=1;
    describe_calls=fill_calls=select_calls=details_calls=0;
    describe_ok=TRUE;mode=REORDER;SudekiMpArmourMenuLayer=NULL;
}
static void no_native_calls(void) {assert(!fill_calls && !select_calls && !details_calls);}
static void preserved(void) {
    assert(describe_calls==1 && fill_calls==1 && select_calls==1 && details_calls==2);
    assert(*(int *)(widgets+SELECTED)==0 && selected_item()==3);
    assert(detail_items[0]==1 && detail_items[1]==3);
}
static void behavior_cases(const char *entries) {
    reset();assert(SudekiMpLanStoryMainMenuRefreshInventory());preserved();
    printf("PASS %s highlight follows item ID after row insertion/reorder\n",entries);

    reset();*(unsigned *)(layer+0x48)=5;
    assert(!SudekiMpLanStoryMainMenuRefreshInventory());
    poll();poll();assert(!describe_calls);no_native_calls();
    *(unsigned *)(layer+0x48)=2;poll();preserved();poll();preserved();
    printf("PASS %s MoreInfo defers until browse and refreshes once while unfocused\n",entries);

    reset();*(void **)(group+0x90)=other_actor;
    assert(!SudekiMpLanStoryMainMenuRefreshInventory());assert(!describe_calls);no_native_calls();
    reset();describe_ok=FALSE;
    assert(!SudekiMpLanStoryMainMenuRefreshInventory());assert(describe_calls==1);no_native_calls();
    printf("PASS %s foreign actor and failed inventory description refuse native calls\n",entries);

    for(unsigned replacement=REPLACE_LAYER;replacement<=REPLACE_ACTOR;++replacement) {
        reset();mode=replacement;
        assert(!SudekiMpLanStoryMainMenuRefreshInventory());
        assert(fill_calls==1 && !select_calls && details_calls==1);
    }
    printf("PASS %s layer/widgets/actor replacement after rebuild prevents restoration\n",entries);

    reset();SudekiMpArmourMenuLayer=layer;
    assert(!SudekiMpLanStoryMainMenuRefreshInventory());assert(!describe_calls);no_native_calls();
    SudekiMpArmourMenuLayer=NULL;
    printf("PASS %s public inventory refresh excludes the armour-owned layer\n",entries);

    reset();mode=REMOVE_SELECTED;new_rows[0]=row_storage[2];new_rows[1]=row_storage[0];
    assert(SudekiMpLanStoryMainMenuRefreshInventory());
    assert(fill_calls==1 && !select_calls && details_calls==1 && selected_item()==1);
    printf("PASS %s removed highlighted item keeps native equipped-row default\n",entries);

    reset();*(unsigned *)(layer+0x48)=5;
    assert(!SudekiMpLanStoryMainMenuRefreshInventory());ui[0x29]=0;poll();
    ui[0x29]=1;*(unsigned *)(layer+0x48)=2;poll();no_native_calls();
    printf("PASS %s closing menu drops deferred refresh\n",entries);
}
int main(int argc,char **argv) {
    wchar_t path[4096],temp[MAX_PATH],config[MAX_PATH];
    assert(argc==2 && MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,argv[1],-1,path,4096));
    image=map_image(path);
    const unsigned sites[]={FILL+32,DETAILS+12,SELECT+11,CLEAR};
    uint8_t originals[4][5];
    for(unsigned i=0;i<4;++i) memcpy(originals[i],image+sites[i],5);
    assert(GetTempPathW(MAX_PATH,temp) && GetTempFileNameW(temp,L"smf",0,config));
    assert(WritePrivateProfileStringW(L"Menus",L"ArmourChoice",L"true",config));
    for(unsigned owned=0;owned<2;++owned) {
        if(owned) assert(SudekiMpArmourMenuInstall((HMODULE)image,config));
        assert(SudekiMpLanStoryMainMenuInstall((HMODULE)image,allow_actor,NULL,NULL));
        /* Keep all guarded bytes. The fill's retained call to ListClear
         * returns immediately before entering our replacement fill body. */
        jump_to(image+FILL+32,(const void *)(uintptr_t)fill_tail);
        jump_to(image+DETAILS+12,(const void *)(uintptr_t)details_tail);
        jump_to(image+SELECT+11,(const void *)(uintptr_t)select_tail);
        image[CLEAR]=0xc3;
        assert(FlushInstructionCache(GetCurrentProcess(),image,SUDEKIMP_EXPECTED_IMAGE_SIZE));
        assert(SudekiMpArmourMenuWeaponsRefreshExact((HMODULE)image,NULL));
        behavior_cases(owned?"owned entries":"original entries");
        assert(SudekiMpLanStoryMainMenuUninstall());
        for(unsigned i=0;i<4;++i) memcpy(image+sites[i],originals[i],5);
        assert(FlushInstructionCache(GetCurrentProcess(),image,SUDEKIMP_EXPECTED_IMAGE_SIZE));
        if(owned) assert(SudekiMpArmourMenuUninstall());
    }
    assert(DeleteFileW(config));
    assert(VirtualFree(image,0,MEM_RELEASE));
    puts("PASS synthetic Weapons refresh execution and retained native prologue ABIs");
    return 0;
}
