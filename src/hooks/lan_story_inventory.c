#include "hooks/lan_story_inventory.h"
#include "hooks/call_hook.h"
#include "engine/log.h"
#include <string.h>

enum { RVA_ADD_ITEM=0x217e0u, RVA_ITEM_TABLE=0x408d80u, SITES=6 };
/* CALL rel32 sites whose target is CInventory::AddItem (byte scan of the exact image, 2026-10-14). */
static const uint32_t site_rva[SITES]={0x22169u,0x22186u,0x221a3u,0x89da4u,0x1338dbu,0x1850bdu};
static const char *const site_name[SITES]={"export_hhh","export_hh","export_h","script_or_shop","pickup","chest"};
typedef void (__attribute__((thiscall)) *AddItem)(void *inventory,int item,int quantity,int show);
static uint8_t *base;
static SudekiMpRelativeCallHook hooks[SITES];
static SudekiMpStoryInventoryEvent last;
static unsigned logs;

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && a<=UINTPTR_MAX-n && VirtualQuery(p,&m,sizeof(m))==sizeof(m) &&
        m.State==MEM_COMMIT && !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
        a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static void record(unsigned site,void *inventory,int item,int quantity,int show) {
    if(!++last.serial) ++last.serial;
    last.tick=GetTickCount(); last.site=site; last.item=item; last.quantity=quantity; last.show=show;
    if(logs<256u) { ++logs;
        SudekiMpLogFormat("story_inventory event=add_item site=%s inventory=%p item=%d quantity=%d show=%d\r\n",
            site<SITES?site_name[site]:"?",inventory,item,quantity,show); }
}
#define STUB(n) static void __attribute__((thiscall)) stub_##n(void *inventory,int item,int quantity,int show) { \
    record(n,inventory,item,quantity,show); ((AddItem)(base+RVA_ADD_ITEM))(inventory,item,quantity,show); }
STUB(0) STUB(1) STUB(2) STUB(3) STUB(4) STUB(5)
static void *const stubs[SITES]={(void *)stub_0,(void *)stub_1,(void *)stub_2,(void *)stub_3,(void *)stub_4,(void *)stub_5};

static BOOL image_exact(uint8_t *b) {
    /* AddItem entry: MOV EAX,[item table]; SUB ESP,8; PUSH EBX/EBP/ESI/EDI. */
    if(!readable(b+RVA_ADD_ITEM,12u) || b[RVA_ADD_ITEM]!=0xa1u ||
        *(uint32_t *)(b+RVA_ADD_ITEM+1u)!=(uint32_t)(uintptr_t)(b+RVA_ITEM_TABLE) ||
        memcmp(b+RVA_ADD_ITEM+5u,"\x83\xec\x08\x53\x55\x56\x57",7u)) return FALSE;
    for(unsigned i=0;i<SITES;++i) {
        int32_t rel;
        if(!readable(b+site_rva[i],5u) || b[site_rva[i]]!=0xe8u) return FALSE;
        memcpy(&rel,b+site_rva[i]+1u,4u);
        if(b+site_rva[i]+5u+rel!=b+RVA_ADD_ITEM) return FALSE;
    }
    return TRUE;
}
BOOL SudekiMpLanStoryInventoryInstall(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    if(!b || base) { SetLastError(ERROR_INVALID_STATE); return FALSE; }
    if(!image_exact(b)) { SetLastError(ERROR_INVALID_DATA); return FALSE; }
    base=b; memset(&last,0,sizeof(last)); logs=0;
    for(unsigned i=0;i<SITES;++i)
        if(!SudekiMpInstallRelativeCallHook(&hooks[i],b+site_rva[i],b+RVA_ADD_ITEM,stubs[i])) {
            DWORD error=GetLastError();
            while(i--) (void)SudekiMpRestoreRelativeCallHook(&hooks[i]);
            base=NULL; SetLastError(error); return FALSE;
        }
    SudekiMpLogWrite("story_inventory event=installed sites=6 target=217e0 policy=observe_only\r\n");
    return TRUE;
}
BOOL SudekiMpLanStoryInventoryUninstall(void) {
    if(!base) return TRUE;
    BOOL ok=TRUE;
    for(unsigned i=SITES;i--;) if(hooks[i].installed && !SudekiMpRestoreRelativeCallHook(&hooks[i])) ok=FALSE;
    if(ok) base=NULL;
    return ok;
}
BOOL SudekiMpLanStoryInventoryLast(SudekiMpStoryInventoryEvent *out) {
    if(!out || !last.serial) return FALSE;
    *out=last; return TRUE;
}
