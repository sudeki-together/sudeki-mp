/* Exact supported-image native loot contracts plus synthetic owned inventory.
 * Executes retail AddItem; native popup UI is replaced by an inert fixture.
 * No live gameplay or transport proof is claimed here. */
#include "engine/build_identity.h"
#include "hooks/call_hook.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned install_attempt,install_fail,restore_count;
static void *restore_fail;
static uintptr_t restore_order[6];
static BOOL fixture_install(SudekiMpRelativeCallHook *h,uint8_t *p,const void *t,const void *r) {
    if(++install_attempt==install_fail) {SetLastError(ERROR_ACCESS_DENIED);return FALSE;}
    return SudekiMpInstallRelativeCallHook(h,p,t,r);
}
static BOOL fixture_restore(SudekiMpRelativeCallHook *h) {
    assert(restore_count<6);restore_order[restore_count++]=(uintptr_t)h->instruction;
    if(h->instruction==restore_fail) {SetLastError(ERROR_ACCESS_DENIED);return FALSE;}
    return SudekiMpRestoreRelativeCallHook(h);
}
#define SudekiMpInstallRelativeCallHook fixture_install
#define SudekiMpRestoreRelativeCallHook fixture_restore
#include "../src/hooks/lan_story_inventory.c"
#undef SudekiMpInstallRelativeCallHook
#undef SudekiMpRestoreRelativeCallHook
void SudekiMpLogFormat(const char *fmt,...) {(void)fmt;}
void SudekiMpLogWrite(const char *s) {(void)s;}
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
static void reset_faults(void) {
    install_attempt=install_fail=restore_count=0;restore_fail=NULL;
    memset(restore_order,0,sizeof(restore_order));
}
static void named_checks(uint8_t *b) {
    static const struct {const char *name;unsigned rva,size;} guards[]={
        {"NativeLootGrantEntry",0x217e0,14},{"NativeLootGrantReturn",0x21842,3},
        {"NativeLootGrantExport",0x30c064,4},{"NativeLootItemClass",0x21be80,7},
        {"NativeLootInventoryOwner",0x2ca378,4},{"NativeLootDatabaseOwner",0x2c693c,4},
        {"NativeLootCategoryOwner",0x2d4184,4},{"NativeLootCategory40",0x2157e,13},{"NativeLootMoneyDispatch",0x2180f,24},
        {"NativeLootMoneyCap",0x21c89,32},{"NativeLootPickupContext",0x1338c0,32},
        {"NativeLootChestContext",0x185090,50},
        {"NativeLootExcludedExportHHH",0x22169,5},{"NativeLootExcludedExportHH",0x22186,5},
        {"NativeLootExcludedExportH",0x221a3,5},{"NativeLootExcludedShop",0x89da4,5}
    };
    assert(grant_code(b) && pickup_code(b));
    for(unsigned i=0;i<sizeof(guards)/sizeof(*guards);++i) {
        for(unsigned j=0;j<guards[i].size;++j) {
            b[guards[i].rva+j]^=1;
            assert(!SudekiMpLanStoryInventoryInstall((HMODULE)b) && !base);
            b[guards[i].rva+j]^=1;
        }
        printf("PASS named %s all byte mutations refused\n",guards[i].name);
    }
    for(unsigned i=0;i<sizeof(item_vtables)/sizeof(*item_vtables);++i) for(unsigned j=0;j<4;++j) {
        b[item_vtables[i]+8+j]^=1;
        assert(!SudekiMpLanStoryInventoryInitialize((HMODULE)b));
        b[item_vtables[i]+8+j]^=1;
    }
    puts("PASS named NativeLootItemVtables all 14 definition-class slots");
}
static void lifecycle(uint8_t *b) {
    uint8_t saved[SITES][5];for(unsigned i=0;i<SITES;++i) memcpy(saved[i],b+site_rva[i],5);
    reset_faults();assert(SudekiMpLanStoryInventoryPickupCallExact((HMODULE)b));
    assert(SudekiMpLanStoryInventoryInstall((HMODULE)b));
    assert(!SudekiMpLanStoryInventoryInstall((HMODULE)b));
    assert(SudekiMpLanStoryInventoryPickupCallExact((HMODULE)b));
    b[site_rva[4]+1]^=1;assert(!SudekiMpLanStoryInventoryPickupCallExact((HMODULE)b));b[site_rva[4]+1]^=1;
    b[site_rva[4]]^=1;assert(!SudekiMpLanStoryInventoryPickupCallExact((HMODULE)b));b[site_rva[4]]^=1;
    assert(SudekiMpLanStoryInventoryUninstall());
    assert(restore_count==SITES);
    for(unsigned i=0;i<SITES;++i) assert(restore_order[i]==(uintptr_t)(b+site_rva[SITES-1-i]) && !memcmp(saved[i],b+site_rva[i],5));
    reset_faults();install_fail=4;
    assert(!SudekiMpLanStoryInventoryInstall((HMODULE)b) && !base && restore_count==3);
    for(unsigned i=0;i<3;++i) assert(restore_order[i]==(uintptr_t)(b+site_rva[2-i]));
    for(unsigned i=0;i<SITES;++i) assert(!memcmp(saved[i],b+site_rva[i],5));
    reset_faults();install_fail=4;restore_fail=b+site_rva[1];
    assert(!SudekiMpLanStoryInventoryInstall((HMODULE)b) && base==b && restore_count==3);
    assert(hooks[1].installed && !hooks[0].installed && !hooks[2].installed);
    assert(!SudekiMpLanStoryInventoryInitialize((HMODULE)b));
    reset_faults();assert(SudekiMpLanStoryInventoryUninstall());
    for(unsigned i=0;i<SITES;++i) assert(!memcmp(saved[i],b+site_rva[i],5));
    reset_faults();assert(SudekiMpLanStoryInventoryInstall((HMODULE)b));
    assert(SudekiMpLanStoryInventoryBind(GetCurrentThreadId(),NULL,NULL));
    restore_fail=b+site_rva[4];assert(!SudekiMpLanStoryInventoryUninstall() && base==b);
    assert(!SudekiMpLanStoryInventoryBind(GetCurrentThreadId(),NULL,NULL));
    assert(restore_count==SITES && hooks[4].installed);
    reset_faults();assert(SudekiMpLanStoryInventoryUninstall());
    puts("PASS shared pickup seam original/owned/foreign; reverse rollback and retained dependency retry");
}
static uint8_t inv[0x200],database[0xfb0],definition[0x100],category[0x14],ui[0x80];
static void *categories[1];static int16_t entries[32];
static uint8_t *test_image;static unsigned notices,observations,capture_faults;static BOOL invalidate_after,inflate_after;
static SudekiMpStoryInventoryEvent observed;
static void __attribute__((stdcall)) notice(int show) {
    assert(show==1 || show==2);++notices;
    if(inflate_after) ++entries[1];
    if(invalidate_after) *(void **)(test_image+RVA_INVENTORY)=NULL;
    SetLastError(0x12345678u);
}
static void observer_fixture(const SudekiMpStoryInventoryEvent *e,void *ctx) {
    assert(ctx==&observations);++observations;observed=*e;
    if(!e->capture_valid) ++capture_faults;
    else assert(entries[1]>0 || e->money); /* Called after native mutation. */
    SetLastError(0xdeadbeefu);
}
static void jump_to(uint8_t *b,unsigned rva,void *target) {
    b[rva]=0xe9;int32_t rel=(int32_t)((uintptr_t)target-(uintptr_t)(b+rva+5));
    memcpy(b+rva+1,&rel,4);assert(FlushInstructionCache(GetCurrentProcess(),b+rva,5));
}
static void make_inventory(uint8_t *b,unsigned kind,unsigned id) {
    memset(inv,0,sizeof(inv));memset(database,0,sizeof(database));memset(definition,0,sizeof(definition));
    memset(category,0,sizeof(category));memset(entries,0xff,sizeof(entries));memset(ui,0,sizeof(ui));
    *(void **)inv=b+0x2ca378;*(void **)database=b+0x2c693c;*(void **)definition=b+0x2d3a28;
    *(void **)(database+12+id*4)=definition;*(unsigned *)(definition+0x14)=id;
    *(unsigned *)(definition+0x18)=3;*(unsigned *)(definition+0x88)=kind;*(unsigned *)(definition+0xdc)=10;
    *(void **)(b+RVA_INVENTORY)=inv;*(void **)(b+RVA_ITEM_TABLE)=database;
    *(void **)category=b+0x2d4184;*(void **)(category+4)=entries;*(unsigned *)(category+8)=kind;
    *(int16_t *)(category+0x10)=16;categories[0]=category;
    *(void **)(inv+12)=categories;*(unsigned *)(inv+0x12c)=1;
    *(void **)(b+0x3c2f78)=ui;*(void **)(b+0x408d1c)=NULL;
}
static DWORD WINAPI wrong_thread(void *unused) {
    SudekiMpStoryInventoryGrant g;(void)unused;
    assert(!SudekiMpLanStoryInventoryPrepare(204,1,&g));
    assert(!SudekiMpLanStoryInventoryUninstall() && GetLastError()==ERROR_BUSY);
    return 0;
}
static void native_grants(uint8_t *b) {
    SudekiMpStoryInventoryGrant g;SudekiMpStoryInventoryGrantResult r;
    test_image=b;jump_to(b,0x215e0,notice);make_inventory(b,2,204);
    assert(SudekiMpLanStoryInventoryInitialize((HMODULE)b));
    assert(!SudekiMpLanStoryInventoryPrepare(204,1,&g)); /* Unbound. */
    assert(!SudekiMpLanStoryInventoryBind(GetCurrentThreadId()+1,NULL,NULL));
    assert(SudekiMpLanStoryInventoryBind(GetCurrentThreadId(),NULL,NULL));
    HANDLE worker=CreateThread(NULL,0,wrong_thread,NULL,0,NULL);assert(worker);
    assert(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0);CloseHandle(worker);
    assert(base==b && admission);
    assert(!SudekiMpLanStoryInventoryPrepare(999,1,&g));
    assert(!SudekiMpLanStoryInventoryPrepare(204,0,&g));
    *(void **)inv=NULL;assert(!SudekiMpLanStoryInventoryPrepare(204,1,&g));*(void **)inv=b+0x2ca378;
    *(void **)database=NULL;assert(!SudekiMpLanStoryInventoryPrepare(204,1,&g));*(void **)database=b+0x2c693c;
    *(void **)category=NULL;assert(!SudekiMpLanStoryInventoryPrepare(204,1,&g));*(void **)category=b+0x2d4184;
    *(void **)definition=NULL;assert(!SudekiMpLanStoryInventoryPrepare(204,1,&g));*(void **)definition=b+0x2d3a28;
    assert(SudekiMpLanStoryInventoryPrepare(204,1,&g) && g.before==0);
    assert(SudekiMpLanStoryInventoryApply(&g,&r) && r.status==SUDEKIMP_STORY_INVENTORY_GRANTED && r.after==1 && notices==1);
    assert(GetLastError()==0x12345678u && !SudekiMpLanStoryInventoryApply(&g,&r) && notices==1);
    assert(SudekiMpLanStoryInventoryPrepare(204,1,&g));
    inflate_after=TRUE;
    assert(SudekiMpLanStoryInventoryApply(&g,&r) && r.status==SUDEKIMP_STORY_INVENTORY_UNCERTAIN);
    inflate_after=FALSE;assert(entries[1]==3 && !SudekiMpLanStoryInventoryApply(&g,&r));
    entries[1]=1;
    assert(SudekiMpLanStoryInventoryPrepare(204,4,&g));
    assert(SudekiMpLanStoryInventoryApply(&g,&r) && r.status==SUDEKIMP_STORY_INVENTORY_CAPPED && r.before==1 && r.after==3);
    assert(SudekiMpLanStoryInventoryPrepare(204,1,&g));
    assert(SudekiMpLanStoryInventoryApply(&g,&r) && r.status==SUDEKIMP_STORY_INVENTORY_CAPPED && r.after==3);
    assert(SudekiMpLanStoryInventoryPrepare(204,1,&g));entries[1]=2;
    assert(!SudekiMpLanStoryInventoryApply(&g,&r)); /* Count changed between phases. */
    assert(SudekiMpLanStoryInventoryPrepare(204,1,&g));
    invalidate_after=TRUE;assert(SudekiMpLanStoryInventoryApply(&g,&r) && r.status==SUDEKIMP_STORY_INVENTORY_UNCERTAIN);
    invalidate_after=FALSE;*(void **)(b+RVA_INVENTORY)=inv;
    assert(!SudekiMpLanStoryInventoryApply(&g,&r)); /* No retry after native entry. */
    make_inventory(b,40,800);
    assert(SudekiMpLanStoryInventoryPrepare(800,1,&g));
    assert(SudekiMpLanStoryInventoryApply(&g,&r) && r.status==SUDEKIMP_STORY_INVENTORY_GRANTED && r.after==1);
    make_inventory(b,39,315);*(unsigned *)(inv+0x134)=99985;
    assert(SudekiMpLanStoryInventoryPrepare(315,2,&g));
    assert(SudekiMpLanStoryInventoryApply(&g,&r) && r.money && r.requested==20 && r.status==SUDEKIMP_STORY_INVENTORY_CAPPED && r.before==99985 && r.after==99999);
    assert(SudekiMpLanStoryInventoryPrepare(315,1,&g));
    assert(SudekiMpLanStoryInventoryApply(&g,&r) && r.status==SUDEKIMP_STORY_INVENTORY_CAPPED && r.after==99999);
    assert(SudekiMpLanStoryInventoryUninstall());
    puts("PASS native thiscall grant, ownership, stack caps, money cap, lease fences, single use and uncertain completion");
    make_inventory(b,2,204);reset_faults();assert(SudekiMpLanStoryInventoryInstall((HMODULE)b));
    assert(SudekiMpLanStoryInventoryBind(GetCurrentThreadId(),observer_fixture,&observations));
    for(unsigned i=0;i<SITES;++i) {
        entries[0]=-1;entries[1]=-1;
        ((AddItem)stubs[i])(inv,204,1,1);
        assert(GetLastError()==0x12345678u);
        assert(observations==(i<4?0:i-3));
        if(i>=4) assert(observed.source==i-3 && observed.item==204 && observed.quantity==1 && observed.before==0 && observed.after==1);
    }
    entries[0]=204;entries[1]=3;
    ((AddItem)stubs[4])(inv,204,1,1);
    assert(observations==3 && observed.source==1 && observed.before==3 && observed.after==3);
    make_inventory(b,39,315);*(unsigned *)(inv+0x134)=99999;
    ((AddItem)stubs[5])(inv,315,1,1);
    assert(observations==4 && observed.money && observed.quantity==1 && observed.before==99999 && observed.after==99999);
    invalidate_after=TRUE;((AddItem)stubs[4])(inv,315,1,1);invalidate_after=FALSE;
    assert(observations==5 && capture_faults==1 && !observed.capture_valid && observed.item==315 && observed.source==1);
    *(void **)(b+RVA_INVENTORY)=inv;
    make_inventory(b,2,204);inflate_after=TRUE;((AddItem)stubs[4])(inv,204,1,1);inflate_after=FALSE;
    assert(entries[1]==2 && observations==6 && capture_faults==2 && !observed.capture_valid);
    assert(SudekiMpLanStoryInventoryUninstall());
    puts("PASS native quantity overshoot yields uncertain client completion and unknown host capture");
    puts("PASS unknown capture reports a fault on the bound game thread");
    puts("PASS host cap still publishes full loot request for peers with independent room");
    puts("PASS six native stubs preserve ABI/LastError; only pickup and chest publish after grant, shop/exports excluded");
}
int main(int argc,char **argv) {
    assert(argc==2);wchar_t path[1024];assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024));
    uint8_t *b=map_image(path);named_checks(b);lifecycle(b);native_grants(b);
    VirtualFree(b,0,MEM_RELEASE);puts("story inventory image checks passed");return 0;
}
