#include "hooks/lan_story_inventory.h"
#include "hooks/call_hook.h"
#include "engine/log.h"
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Story inventory requires 32-bit GCC thiscall support"
#endif

enum { RVA_ADD_ITEM=0x217e0u, RVA_ITEM_TABLE=0x408d80u,
    RVA_INVENTORY=0x408d84u, SITES=6, ITEM_LIMIT=999, CATEGORY_LIMIT=64 };
/* Exact CALL rel32 references to AddItem, including excluded paths. */
static const uint32_t site_rva[SITES]={0x22169u,0x22186u,0x221a3u,0x89da4u,0x1338dbu,0x1850bdu};
static const char *const site_name[SITES]={"export_hhh","export_hh","export_h","shop","pickup","chest"};
static const uint32_t item_vtables[]={0x2c6954u,0x2c69c0u,0x2c6a2cu,0x2c6a98u,
    0x2c6b04u,0x2c6b70u,0x2c6bdcu,0x2d39b8u,0x2d3a28u,0x2d3ab8u,
    0x2d3b20u,0x2d3b8cu,0x2d3c04u,0x2dfa50u};
typedef void (__attribute__((thiscall)) *AddItem)(void *,int,int,int);
static uint8_t *base;
static SudekiMpRelativeCallHook hooks[SITES];
static SudekiMpStoryInventoryEvent last;
static SudekiMpStoryInventoryObserver observer;
static void *observer_context;
static DWORD game_thread;
static uint32_t generation;
static unsigned logs;
static BOOL admission;

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && a<=UINTPTR_MAX-n && VirtualQuery(p,&m,sizeof(m))==sizeof(m) &&
        m.State==MEM_COMMIT && !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
        a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL bytes(uint8_t *b,unsigned rva,const void *expected,size_t size) {
    return readable(b+rva,size) && !memcmp(b+rva,expected,size);
}
static BOOL pointer(uint8_t *b,unsigned rva,unsigned target) {
    return readable(b+rva,4u) && *(void **)(b+rva)==b+target;
}
/* Named NativeLootGrantIdentity: relocated globals, exported entry, native
 * item class getter, inventory/category/item-manager ownership, money cap. */
static BOOL grant_code(uint8_t *b) {
    if(!b || !bytes(b,0x217e0u,"\xa1",1u) || !pointer(b,0x217e1u,RVA_ITEM_TABLE) ||
        !bytes(b,0x217e5u,"\x83\xec\x08\x53\x55\x56\x57\x8b\xf1",9u) ||
        !bytes(b,0x21842u,"\xc2\x0c\0",3u) ||
        !bytes(b,0x21be80u,"\x8b\x81\x88\0\0\0\xc3",7u) ||
        !pointer(b,0x2ca378u,0x78c00u) || !pointer(b,0x2c693cu,0x1fa30u) ||
        !pointer(b,0x2d4184u,0x133c20u) ||
        !bytes(b,0x2157eu,"\xbf\x63\0\0\0\x8d\x47\xc5\xe8\x15\x26\x11\0",13u) ||
        !readable(b+0x30c064u,4u) || *(uint32_t *)(b+0x30c064u)!=RVA_ADD_ITEM ||
        !bytes(b,0x2180fu,"\x83\xf8\x27\x75\x31\x8b\xbf\xdc\0\0\0\x0f\xaf\x7c\x24\x20\x57\x8b\xce\xe8\x29\x04\0\0",24u) ||
        !bytes(b,0x21c89u,"\x8b\x54\x24\x0c\x01\x96\x34\x01\0\0\x81\xbe\x34\x01\0\0\x9f\x86\x01\0\x76\x0a\xc7\x86\x34\x01\0\0\x9f\x86\x01\0",32u)) return FALSE;
    for(unsigned i=0;i<sizeof(item_vtables)/sizeof(*item_vtables);++i)
        if(!pointer(b,item_vtables[i]+8u,0x21be80u)) return FALSE;
    return TRUE;
}
static BOOL call_exact(uint8_t *b,unsigned site,BOOL allow_owned) {
    int32_t rel;
    if(!readable(b+site_rva[site],5u) || b[site_rva[site]]!=0xe8u) return FALSE;
    memcpy(&rel,b+site_rva[site]+1u,4u);
    if(b+site_rva[site]+5u+rel==b+RVA_ADD_ITEM) return TRUE;
    return allow_owned && b==base && hooks[site].installed &&
        hooks[site].instruction==b+site_rva[site] && rel==hooks[site].replacement_displacement;
}
BOOL SudekiMpLanStoryInventoryPickupCallExact(HMODULE image) {
    return image && call_exact((uint8_t *)image,4u,TRUE);
}
/* NativeLootPickupCall / NativeLootChestCall: both preflight CanAddItem,
 * then pass quantity=1 and show=1 to the shared AddItem owner. */
static BOOL pickup_code(uint8_t *b) {
    return bytes(b,0x1338c0u,"\x8b\x3d",2u) && pointer(b,0x1338c2u,RVA_INVENTORY) &&
        bytes(b,0x1338c6u,"\x6a\x01\x56\x8b\xcf\xe8\0\xe1\xee\xff\x84\xc0\x74\x1a\x6a\x01\x6a\x01\x56\x8b\xcf",21u) &&
        call_exact(b,4u,FALSE) &&
        bytes(b,0x185090u,"\x8b\x87\xb8\0\0\0\x56\x8b\x35",9u) &&
        pointer(b,0x185099u,RVA_INVENTORY) &&
        bytes(b,0x18509du,"\x83\xf8\xff\x74\x2f\x6a\x01\x50\x8b\xce\xe8\x24\xc9\xe9\xff\x84\xc0\x74\x21\x8b\x87\xb8\0\0\0\x6a\x01\x6a\x01\x50\x8b\xce",32u) &&
        call_exact(b,5u,FALSE);
}
static BOOL context(uint32_t item,uint32_t quantity,SudekiMpStoryInventoryGrant *g) {
    uint8_t *inventory,*database,*definition,*table;
    unsigned type_index;
    if(!base || !admission || !g || !quantity || quantity>999u || item>=ITEM_LIMIT ||
        !game_thread || GetCurrentThreadId()!=game_thread || !grant_code(base) ||
        !readable(base+RVA_INVENTORY,4u) || !readable(base+RVA_ITEM_TABLE,4u)) return FALSE;
    inventory=*(void **)(base+RVA_INVENTORY);database=*(void **)(base+RVA_ITEM_TABLE);
    if(!readable(inventory,0x138u) || *(void **)inventory!=base+0x2ca378u ||
        !readable(database,12u+(item+1u)*4u) || *(void **)database!=base+0x2c693cu) return FALSE;
    definition=*(void **)(database+12u+item*4u);
    if(!readable(definition,0x8cu) || *(uint32_t *)(definition+0x14u)!=item) return FALSE;
    table=*(void **)definition;
    for(type_index=0;type_index<sizeof(item_vtables)/sizeof(*item_vtables);++type_index)
        if(table==base+item_vtables[type_index]) break;
    if(type_index==sizeof(item_vtables)/sizeof(*item_vtables)) return FALSE;
    uint32_t kind=*(uint32_t *)(definition+0x88u);
    if(kind>40u) return FALSE;
    uint32_t before=0,unit=1;
    if(kind==39u) {
        if(!readable(definition,0xe0u)) return FALSE;
        unit=*(uint32_t *)(definition+0xdcu);
        before=*(uint32_t *)(inventory+0x134u);
        if(!unit || unit>99999u || before>99999u) return FALSE;
    } else {
        unsigned count=*(unsigned *)(inventory+0x12cu);void **categories=*(void ***)(inventory+12u);
        if(!count || count>CATEGORY_LIMIT || !readable(categories,count*4u)) return FALSE;
        BOOL found=FALSE;
        for(unsigned i=0;i<count;++i) {
            uint8_t *category=categories[i];
            if(!readable(category,0x14u) || *(void **)category!=base+0x2d4184u) return FALSE;
            if(*(unsigned *)(category+8u)!=kind) continue;
            if(found) return FALSE;
            found=TRUE;
            int slots=*(int16_t *)(category+0x10u);int16_t *entries=*(void **)(category+4u);
            if(slots<1 || slots>ITEM_LIMIT || !readable(entries,(unsigned)slots*4u)) return FALSE;
            BOOL matched=FALSE;
            for(int j=0;j<slots;++j) if(entries[2*j]==(int)item) {
                if(matched || entries[2*j+1]<0) return FALSE;
                before=(uint32_t)entries[2*j+1];matched=TRUE;
            }
        }
        if(!found) return FALSE;
    }
    memset(g,0,sizeof(*g));g->inventory=inventory;g->database=database;g->definition=definition;
    g->generation=generation;g->item=item;g->quantity=quantity;g->kind=kind;g->before=before;g->unit_value=unit;
    return *(void **)(base+RVA_INVENTORY)==inventory && *(void **)(base+RVA_ITEM_TABLE)==database &&
        *(void **)(database+12u+item*4u)==definition;
}
BOOL SudekiMpLanStoryInventoryPrepare(uint32_t item,uint32_t quantity,SudekiMpStoryInventoryGrant *g) {
    if(!g) return FALSE;
    memset(g,0,sizeof(*g));
    if(context(item,quantity,g)) return TRUE;
    SetLastError(ERROR_INVALID_STATE);return FALSE;
}
static BOOL same_context(const SudekiMpStoryInventoryGrant *a,const SudekiMpStoryInventoryGrant *b) {
    return a->generation==b->generation && a->inventory==b->inventory &&
        a->database==b->database && a->definition==b->definition && a->kind==b->kind &&
        a->item==b->item && a->quantity==b->quantity && a->unit_value==b->unit_value;
}
BOOL SudekiMpLanStoryInventoryApply(SudekiMpStoryInventoryGrant *g,SudekiMpStoryInventoryGrantResult *out) {
    SudekiMpStoryInventoryGrant fresh;DWORD error;
    if(!out) return FALSE;
    memset(out,0,sizeof(*out));
    if(!g || g->used || !context(g->item,g->quantity,&fresh) || !same_context(g,&fresh) ||
        g->before!=fresh.before) { SetLastError(ERROR_INVALID_STATE);return FALSE; }
    g->used=1u;out->status=SUDEKIMP_STORY_INVENTORY_UNCERTAIN;
    out->before=g->before;out->requested=g->quantity*g->unit_value;out->money=g->kind==39u;
    ((AddItem)(base+RVA_ADD_ITEM))(g->inventory,(int)g->item,(int)g->quantity,1);
    error=GetLastError();
    if(context(g->item,g->quantity,&fresh) && same_context(g,&fresh) && fresh.before>=g->before &&
        fresh.before-g->before<=out->requested) {
        out->after=fresh.before;
        out->status=fresh.before-g->before<out->requested?
            SUDEKIMP_STORY_INVENTORY_CAPPED:SUDEKIMP_STORY_INVENTORY_GRANTED;
    }
    SudekiMpLogFormat("story_inventory event=grant_result item=%lu quantity=%lu before=%lu after=%lu money=%lu status=%u\r\n",
        (unsigned long)g->item,(unsigned long)g->quantity,(unsigned long)out->before,
        (unsigned long)out->after,(unsigned long)out->money,(unsigned)out->status);
    SetLastError(error);return TRUE;
}
static void invoke(unsigned site,void *inventory,int item,int quantity,int show) {
    SudekiMpStoryInventoryGrant before,after;DWORD incoming=GetLastError();
    unsigned source=site==4u?1u:site==5u?2u:0u;
    BOOL captured=admission && source && observer && context((uint32_t)item,(uint32_t)quantity,&before) && before.inventory==inventory;
    SetLastError(incoming);
    ((AddItem)(base+RVA_ADD_ITEM))(inventory,item,quantity,show);
    DWORD native_error=GetLastError();
    if(!++last.serial) ++last.serial;
    last.tick=GetTickCount();last.site=site;last.item=item;last.quantity=quantity;last.show=show;
    last.source=source;last.before=last.after=last.money=last.capture_valid=0;
    if(captured && context((uint32_t)item,(uint32_t)quantity,&after) && same_context(&before,&after) &&
        after.before>=before.before && after.before-before.before<=before.quantity*before.unit_value) {
        last.before=before.before;last.after=after.before;last.money=before.kind==39u;last.capture_valid=1;
        SudekiMpStoryInventoryEvent event=last;observer(&event,observer_context);
        if(after.before==before.before) SudekiMpLogFormat("story_inventory event=loot_cap site=%s item=%d quantity=%d\r\n",site_name[site],item,quantity);
    } else if(admission && source && observer) {
        SudekiMpLogFormat("story_inventory event=loot_capture_refused site=%s item=%d quantity=%d\r\n",site_name[site],item,quantity);
        if(game_thread && GetCurrentThreadId()==game_thread) {
            SudekiMpStoryInventoryEvent event=last;observer(&event,observer_context);
        }
    }
    if(logs<256u) {++logs;SudekiMpLogFormat("story_inventory event=add_item site=%s inventory=%p item=%d quantity=%d show=%d\r\n",
        site_name[site],inventory,item,quantity,show);}
    SetLastError(native_error);
}
#define STUB(n) static void __attribute__((thiscall)) stub_##n(void *p,int i,int q,int s) {invoke(n,p,i,q,s);}
STUB(0) STUB(1) STUB(2) STUB(3) STUB(4) STUB(5)
static void *const stubs[SITES]={(void *)stub_0,(void *)stub_1,(void *)stub_2,(void *)stub_3,(void *)stub_4,(void *)stub_5};
BOOL SudekiMpLanStoryInventoryInitialize(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    if(base) {SetLastError(ERROR_ALREADY_INITIALIZED);return FALSE;}
    if(!grant_code(b)) {SetLastError(ERROR_INVALID_DATA);return FALSE;}
    base=b;admission=TRUE;logs=0;memset(&last,0,sizeof(last));if(!++generation) ++generation;
    return TRUE;
}
BOOL SudekiMpLanStoryInventoryBind(DWORD thread,SudekiMpStoryInventoryObserver callback,void *ctx) {
    if(!base || !admission || !thread || thread!=GetCurrentThreadId() || (game_thread && game_thread!=thread)) {
        SetLastError(ERROR_INVALID_STATE);return FALSE;
    }
    if(game_thread!=thread || observer!=callback || observer_context!=ctx) {if(!++generation) ++generation;}
    game_thread=thread;observer=callback;observer_context=ctx;return TRUE;
}
BOOL SudekiMpLanStoryInventoryInstall(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    if(base || !b) {SetLastError(ERROR_INVALID_STATE);return FALSE;}
    if(!grant_code(b) || !pickup_code(b)) {SetLastError(ERROR_INVALID_DATA);return FALSE;}
    for(unsigned i=0;i<SITES;++i) if(!call_exact(b,i,FALSE)) {SetLastError(ERROR_INVALID_DATA);return FALSE;}
    if(!SudekiMpLanStoryInventoryInitialize(image)) return FALSE;
    for(unsigned i=0;i<SITES;++i)
        if(!SudekiMpInstallRelativeCallHook(&hooks[i],b+site_rva[i],b+RVA_ADD_ITEM,stubs[i])) {
            DWORD error=GetLastError();
            (void)SudekiMpLanStoryInventoryUninstall(); /* Includes attempted seam; retains dependencies on failure. */
            SetLastError(error);return FALSE;
        }
    SudekiMpLogWrite("story_inventory event=installed sites=6 target=217e0 policy=loot_sources_only\r\n");
    return TRUE;
}
BOOL SudekiMpLanStoryInventoryUninstall(void) {
    if(!base) return TRUE;
    if(game_thread && GetCurrentThreadId()!=game_thread) {SetLastError(ERROR_BUSY);return FALSE;}
    admission=FALSE; /* Retained hooks only pass through while teardown retries. */
    BOOL ok=TRUE;DWORD first=ERROR_SUCCESS;
    for(unsigned i=SITES;i--;) if(hooks[i].installed && !SudekiMpRestoreRelativeCallHook(&hooks[i])) {
        if(ok) first=GetLastError();
        ok=FALSE;
    }
    if(!ok) {SetLastError(first);return FALSE;}
    base=NULL;game_thread=0;observer=NULL;observer_context=NULL;
    memset(&last,0,sizeof(last));if(!++generation) ++generation;
    return TRUE;
}
BOOL SudekiMpLanStoryInventoryLast(SudekiMpStoryInventoryEvent *out) {
    if(!out || !last.serial || (game_thread && GetCurrentThreadId()!=game_thread)) return FALSE;
    *out=last;return TRUE;
}
