#include "hooks/lan_story_main_menu.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include "network/lan_story_menu_policy.h"
#include <string.h>
#if !defined(__GNUC__) || !defined(__i386__)
#error "Story main menu scope requires the supported native x86 ABI"
#endif

enum {
    RVA_UI_GLOBAL=0x3c2f78u, RVA_GROUP_GLOBAL=0x408d94u,
    RVA_MENU_VTABLE=0x2cae0cu, RVA_MENU_OPEN_SLOT=0x2cae4cu, RVA_MENU_OPEN=0x956d0u,
    RVA_CYCLE=0x96300u, RVA_CYCLE_NEXT_CALL=0x93b24u, RVA_CYCLE_PREVIOUS_CALL=0x93b47u,
    RVA_EQUIP=0x1706d0u, RVA_EQUIP_CALL=0x16f1a4u,
    RVA_USE=0x179a50u, RVA_USE_CALL=0x17761du, RVA_ITEM_LOOKUP=0x21ce0u, RVA_INVENTORY=0x408d84u,
    ITEMS_SELECTED=0x124u, ITEMS_ROWS=0x158u, ITEMS_DEF=0x200u,
    RVA_MODE_SWITCH=0x177850u, RVA_LIST_CONFIRM_CALL=0x177166u, RVA_CONSUMABLE_VTABLE=0x2d3b20u,
    ITEMS_TARGET_MODE=5u, CONFIRM_SUPPRESS_MS=300u,
    RVA_ITEMS_VTABLE=0x2d8bbcu, RVA_ITEMS_REBUILD=0x178820u, RVA_PANEL_REFRESH=0x15b570u,
    UI_PARTY_PANEL=0x218u, PANEL_CURSOR=0x30c4u,
    RVA_ENTITY_PAUSE=0xfd610u, RVA_OPEN_PAUSE_CALL=0x957f5u, RVA_CLOSE_PAUSE_CALL=0x95a14u,
    RVA_DISABLE_MOVE=0xafd0u, RVA_OPEN_MOVE_CALL=0x956f9u, RVA_CLOSE_MOVE_CALL=0x959ffu,
    UI_SELECTED=0x170u, GROUP_SLOTS=0x90u, GROUP_SLOT_STRIDE=0x0cu, GROUP_COUNT=0xccu,
    RVA_HUD_GLOBAL=0x3c2f88u, RVA_HUD_VTABLE=0x2caf9cu, HUD_STATE=0xb8u, HUD_OPEN_MAIN_MENU=0xfu, MENU_SHOWN=0x29u,
    LAYER_WIDGETS=0x8cu, W_LIST_SELECTED=0x858u, W_LIST_ROWS=0x88cu, ROW_ITEM=0x84u, MAX_ROWS=64u
};
typedef void (__attribute__((thiscall)) *MenuOpen)(void *);
typedef uint8_t (__attribute__((thiscall)) *HudInput)(void *,unsigned,unsigned,unsigned);
typedef unsigned (__attribute__((thiscall)) *MenuInput)(void *,unsigned,unsigned,unsigned);
static uint8_t *base;
static SudekiMpRelativeCallHook cycle_hooks[2],equip_hook,pause_hooks[2],move_hooks[2],use_hook,confirm_hook;
/* Client Items page confirmation (owner, 2026-10-15): instead of the native
 * party-target view the player gets a yes/no prompt for their own character. */
static struct {
    BOOL active; uint8_t *layer; void *actor; unsigned item, choice; int row;
    DWORD suppress_until; BOOL render_logged;
} prompt;
static SudekiMpPointerHook open_hook;
static MenuOpen native_open;
static SudekiMpStoryMenuSlotAllowed slot_allowed;
static SudekiMpStoryMenuEquipRequest equip_request,use_request;
static BOOL installed;
static unsigned redirected,requests,trace_count;
/* Tail-call targets for the naked stubs below (MinGW underscore symbols). */
void *SudekiMpStoryMenuCycleTarget __attribute__((used));
void *SudekiMpStoryMenuEquipTarget __attribute__((used));
void *SudekiMpStoryMenuUseTarget __attribute__((used));
void *SudekiMpStoryMenuConfirmTarget __attribute__((used));
void *SudekiMpStoryMenuPauseTarget __attribute__((used));
/* Client only: the menu's entity-pause pass (RVA 0xFD610, cdecl, +1/-1 on
 * every registry entity and on the world-speed pause count 0x408DA0+0x2A) is
 * skipped. The client world is already held paused by the mod; running the
 * native pass on top left the speed count at 1 after the menu closed and the
 * presentation waited for a world that never ticked (owner, 2026-10-14). */
uint32_t SudekiMpStoryMenuSkipPause __attribute__((used));
/* Client only: the menu's "disable front movement + pause the controller's
 * listener list" call (RVA 0xAFD0, one stack arg, RET 4) is skipped too. On the
 * host it is the vanilla menu pause; on the client it silenced the controller
 * dispatch the mod presents host frames from, so the world behind the menu
 * froze and control never came back (owner, 2026-10-14). The mod itself keeps
 * the client's character from moving while its menu is shown. */
void *SudekiMpStoryMenuMoveTarget __attribute__((used));

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && a<=UINTPTR_MAX-n && VirtualQuery(p,&m,sizeof(m))==sizeof(m) &&
        m.State==MEM_COMMIT && !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
        a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static void trace(const char *event,unsigned a,unsigned b) {
    if(trace_count++<2000u) SudekiMpLogFormat("story_main_menu event=%s a=%u b=%u\r\n",event,a,b);
}
/* The live Main Menu object and the party group it indexes. */
static BOOL menu_exact(uint8_t *ui,uint8_t **group_out,unsigned *count_out) {
    uint8_t *group;
    if(!base || !readable(base+RVA_UI_GLOBAL,4u) || *(uint8_t **)(base+RVA_UI_GLOBAL)!=ui ||
        !readable(ui,UI_SELECTED+4u) || *(void **)ui!=base+RVA_MENU_VTABLE ||
        !readable(base+RVA_GROUP_GLOBAL,4u) ||
        !readable(group=*(uint8_t **)(base+RVA_GROUP_GLOBAL),GROUP_COUNT+4u)) return FALSE;
    unsigned count=*(unsigned *)(group+GROUP_COUNT);
    if(!count || count>4u) return FALSE;
    *group_out=group; *count_out=count; return TRUE;
}
static unsigned allowed_mask(uint8_t *group,unsigned count) {
    unsigned mask=0;
    for(unsigned i=0;i<count;++i) {
        void *actor=*(void **)(group+GROUP_SLOTS+i*GROUP_SLOT_STRIDE);
        if(actor && (!slot_allowed || slot_allowed(actor))) mask|=1u<<i;
    }
    return mask;
}
/* Before the native cycle: pre-position the selection so its +1/-1 lands on
 * the next slot this player may edit. Nothing else of the native routine
 * (preview rebuild, page refresh) is touched. */
/* Returns TRUE when the native cycle must be skipped entirely: the player owns
 * no other slot, so there is nothing to switch to and rebuilding the preview
 * for the same character would only make the model jump (owner, 2026-10-14). */
static BOOL __attribute__((used,noinline)) cycle_prepare(uint8_t *ui,int forward) {
    DWORD saved=GetLastError(); uint8_t *group; unsigned count; BOOL skip=FALSE;
    if(installed && menu_exact(ui,&group,&count)) {
        unsigned current=*(unsigned *)(ui+UI_SELECTED);
        if(current<count) {
            unsigned mask=allowed_mask(group,count);
            unsigned target=SudekiMpStoryMenuNextSlot(count,current,forward!=0,mask);
            unsigned natural=forward?(current+1u)%count:(current+count-1u)%count;
            if(target==current) { skip=TRUE; ++redirected; trace("cycle_skipped",natural,current); }
            else if(target!=natural) {
                *(unsigned *)(ui+UI_SELECTED)=forward?(target+count-1u)%count:(target+1u)%count;
                ++redirected; trace("cycle_redirected",natural,target);
            }
        }
    }
    SetLastError(saved); return skip;
}
static void __attribute__((naked,noinline)) cycle_stub(void) {
    __asm__ volatile(
        "pushal; mov %esp,%ebp; and $-16,%esp; sub $16,%esp;"
        "mov %eax,(%esp); mov 0x24(%ebp),%eax; mov %eax,4(%esp); call _cycle_prepare; test %al,%al;"
        "mov %ebp,%esp; popal; jnz 1f; jmp *_SudekiMpStoryMenuCycleTarget;"
        "1: ret $4");
}
/* Native cycle: EAX = ui, one pushed direction, callee cleans (RET 4). */
static void cycle_native(uint8_t *ui,int forward) {
    void *f=base+RVA_CYCLE;
    __asm__ volatile("push %1; call *%2" : "+a"(ui) : "r"(forward), "r"(f) : "ecx","edx","memory","cc");
}
static void __attribute__((thiscall,force_align_arg_pointer)) open_menu(void *menu) {
    native_open(menu);
    uint8_t *ui=menu,*group; unsigned count;
    if(!installed || !menu_exact(ui,&group,&count)) return;
    unsigned current=*(unsigned *)(ui+UI_SELECTED);
    if(current>=count) return;
    unsigned mask=allowed_mask(group,count);
    unsigned initial=SudekiMpStoryMenuInitialSlot(count,mask,current);
    if(initial!=current) {
        /* Let the native routine rebuild the preview for the new slot. */
        *(unsigned *)(ui+UI_SELECTED)=(initial+count-1u)%count;
        cycle_native(ui,1);
        trace("open_moved",current,*(unsigned *)(ui+UI_SELECTED));
    }
}
/* Weapons layer confirm: a client never equips its paused replica locally. */
static BOOL __attribute__((used,noinline)) equip_intercept(uint8_t *layer) {
    DWORD saved=GetLastError(); BOOL skip=FALSE;
    if(installed && equip_request) {
        uint8_t *ui=base && readable(base+RVA_UI_GLOBAL,4u)?*(uint8_t **)(base+RVA_UI_GLOBAL):NULL,*group;
        unsigned count,outcome=SUDEKIMP_STORY_MENU_EQUIP_REFUSED,item=~0u,stage=0; void *actor=NULL;
        if(menu_exact(ui,&group,&count) && readable(layer,LAYER_WIDGETS+4u)) {
            uint8_t *widgets=*(uint8_t **)(layer+LAYER_WIDGETS);
            unsigned selected=*(unsigned *)(ui+UI_SELECTED); stage=1;
            if(selected<count) actor=*(void **)(group+GROUP_SLOTS+selected*GROUP_SLOT_STRIDE);
            if(actor && readable(widgets,W_LIST_ROWS+4u)) {
                int index=*(int *)(widgets+W_LIST_SELECTED); uint8_t **rows=*(uint8_t ***)(widgets+W_LIST_ROWS); stage=2;
                if(index>=0 && (unsigned)index<MAX_ROWS && readable(rows,(index+1u)*4u) &&
                    readable(rows[index],ROW_ITEM+4u)) {
                    int id=*(int *)(rows[index]+ROW_ITEM); stage=3;
                    if(id>=0 && id<48) { item=(unsigned)id; stage=4; outcome=equip_request(actor,item); }
                    else trace("equip_row_item",(unsigned)id,(unsigned)index);
                } else trace("equip_row_unreadable",(unsigned)index,0);
            } else trace("equip_no_actor",selected,count);
        } else trace("equip_menu_inexact",ui?1u:0u,layer?1u:0u);
        skip=outcome!=SUDEKIMP_STORY_MENU_EQUIP_NATIVE;
        if(outcome==SUDEKIMP_STORY_MENU_EQUIP_QUEUED) ++requests;
        trace(outcome==SUDEKIMP_STORY_MENU_EQUIP_QUEUED?"equip_requested":
            outcome==SUDEKIMP_STORY_MENU_EQUIP_REFUSED?"equip_refused":"equip_native",item,stage);
    }
    SetLastError(saved); return skip;
}
static void __attribute__((naked,noinline)) equip_stub(void) {
    __asm__ volatile(
        "pushal; mov %esp,%ebp; and $-16,%esp; sub $16,%esp;"
        "mov 0x24(%ebp),%eax; mov %eax,(%esp); call _equip_intercept; test %al,%al;"
        "mov %ebp,%esp; popal; jnz 1f; jmp *_SudekiMpStoryMenuEquipTarget;"
        "1: ret $4");
}
static void __attribute__((naked,noinline)) pause_stub(void) {
    __asm__ volatile(
        "cmpl $0,_SudekiMpStoryMenuSkipPause; jne 1f; jmp *_SudekiMpStoryMenuPauseTarget;"
        "1: ret");
}
static void __attribute__((naked,noinline)) move_stub(void) {
    __asm__ volatile(
        "cmpl $0,_SudekiMpStoryMenuSkipPause; jne 1f; jmp *_SudekiMpStoryMenuMoveTarget;"
        "1: ret $4");
}
/* Items page confirm on the client: look the selected row up through the
 * inventory's own category lookup (RVA 0x21CE0, thiscall(inventory, category
 * 2, row index)) to the item definition, and request USE_ITEM for the owned
 * character. Never applies locally. */
typedef void *(__attribute__((thiscall)) *ItemLookup)(void *,int,int);
static BOOL __attribute__((used,noinline)) use_intercept(uint8_t *layer) {
    static const uint8_t lookup_entry[]={0x53,0x8b,0x5c,0x24,0x08,0x55,0x56,0x57,0x8b,0x3d};
    DWORD saved=GetLastError(); BOOL skip=FALSE;
    if(installed && !use_request && readable(layer,ITEMS_DEF+4u)) {
        /* Host: native use, logged for A/B against client requests. */
        uint8_t *def=*(uint8_t **)(layer+ITEMS_DEF),*ui=readable(base+RVA_UI_GLOBAL,4u)?*(uint8_t **)(base+RVA_UI_GLOBAL):NULL;
        trace("use_native",def && readable(def,0x18u)?*(uint32_t *)(def+0x14u):~0u,
            readable(ui,UI_PARTY_PANEL+4u) && readable(*(uint8_t **)(ui+UI_PARTY_PANEL),PANEL_CURSOR+4u)?
            *(unsigned *)(*(uint8_t **)(ui+UI_PARTY_PANEL)+PANEL_CURSOR):~0u);
    }
    if(installed && use_request) {
        uint8_t *ui=base && readable(base+RVA_UI_GLOBAL,4u)?*(uint8_t **)(base+RVA_UI_GLOBAL):NULL,*group;
        unsigned count,outcome=SUDEKIMP_STORY_MENU_EQUIP_REFUSED,item=~0u; void *actor=NULL;
        if(menu_exact(ui,&group,&count) && readable(layer,ITEMS_ROWS+4u) &&
            readable(base+RVA_ITEM_LOOKUP,14u) && !memcmp(base+RVA_ITEM_LOOKUP,lookup_entry,sizeof(lookup_entry)) &&
            *(void **)(base+RVA_ITEM_LOOKUP+10u)==base+0x408d80u && readable(base+RVA_INVENTORY,4u)) {
            unsigned selected=*(unsigned *)(ui+UI_SELECTED);
            if(selected<count) actor=*(void **)(group+GROUP_SLOTS+selected*GROUP_SLOT_STRIDE);
            int index=*(int *)(layer+ITEMS_SELECTED); uint8_t **rows=*(uint8_t ***)(layer+ITEMS_ROWS);
            void *inventory=*(void **)(base+RVA_INVENTORY);
            if(actor && index>=0 && (unsigned)index<MAX_ROWS && readable(rows,(index+1u)*4u) &&
                readable(rows[index],ROW_ITEM+4u) && readable(inventory,0x130u)) {
                int row=*(int *)(rows[index]+ROW_ITEM);
                uint8_t *def=row>=0?((ItemLookup)(base+RVA_ITEM_LOOKUP))(inventory,2,row):NULL;
                if(def && readable(def,0x18u)) { item=*(uint32_t *)(def+0x14u); if(item<999u) outcome=use_request(actor,item); }
            }
        }
        skip=outcome!=SUDEKIMP_STORY_MENU_EQUIP_NATIVE;
        if(outcome==SUDEKIMP_STORY_MENU_EQUIP_QUEUED) ++requests;
        trace(outcome==SUDEKIMP_STORY_MENU_EQUIP_QUEUED?"use_requested":
            outcome==SUDEKIMP_STORY_MENU_EQUIP_REFUSED?"use_refused":"use_native",item,outcome);
    }
    SetLastError(saved); return skip;
}
static void __attribute__((naked,noinline)) use_stub(void) {
    __asm__ volatile(
        "pushal; mov %esp,%ebp; and $-16,%esp; sub $16,%esp;"
        "mov 0x24(%ebp),%eax; mov %eax,(%esp); call _use_intercept; test %al,%al;"
        "mov %ebp,%esp; popal; jnz 1f; jmp *_SudekiMpStoryMenuUseTarget;"
        "1: ret $4");
}
/* The player's own party slot as the Main Menu currently selects it. */
static void *own_actor(void) {
    uint8_t *ui=base && readable(base+RVA_UI_GLOBAL,4u)?*(uint8_t **)(base+RVA_UI_GLOBAL):NULL,*group; unsigned count;
    if(!menu_exact(ui,&group,&count)) return NULL;
    unsigned selected=*(unsigned *)(ui+UI_SELECTED);
    if(selected>=count) return NULL;
    void *actor=*(void **)(group+GROUP_SLOTS+selected*GROUP_SLOT_STRIDE);
    return actor && (!slot_allowed || slot_allowed(actor))?actor:NULL;
}
static void prompt_close(const char *event) {
    if(prompt.active) trace(event,prompt.item,prompt.choice);
    prompt.active=FALSE; prompt.layer=NULL; prompt.actor=NULL; prompt.render_logged=FALSE;
}
/* Enter on the prompt: Yes = USE_ITEM request for the player's own character
 * (D10), No = nothing. Either way the native list stays where it was. */
static void prompt_decide(void) {
    DWORD now=GetTickCount(); prompt.suppress_until=now+CONFIRM_SUPPRESS_MS;
    if(prompt.choice==0u && use_request && prompt.actor) {
        unsigned outcome=use_request(prompt.actor,prompt.item);
        if(outcome==SUDEKIMP_STORY_MENU_EQUIP_QUEUED) ++requests;
        trace(outcome==SUDEKIMP_STORY_MENU_EQUIP_QUEUED?"use_requested":"use_refused",prompt.item,outcome);
        prompt_close("confirm_yes");
    } else prompt_close("confirm_no");
}
/* Items list confirm on the client (RVA 0x177166: EBX = layer, EDI = 5, the
 * mode switch that would show the party-target view). The selected consumable
 * (layer+0x200, set by the list refresh) becomes a prompt instead; the layer
 * stays in list mode. A confirm arriving while the prompt is shown decides
 * it, so the native key and the polled key agree whichever runs first. */
static BOOL __attribute__((used,noinline)) confirm_intercept(uint8_t *layer,unsigned mode) {
    DWORD saved=GetLastError(); BOOL skip=FALSE;
    if(installed && use_request && mode==ITEMS_TARGET_MODE) {
        DWORD now=GetTickCount();
        if(prompt.active && prompt.layer==layer) { prompt_decide(); skip=TRUE; }
        else if(prompt.suppress_until && (int)(prompt.suppress_until-now)>0) { trace("confirm_suppressed",0,0); skip=TRUE; }
        else {
            uint8_t *def=readable(layer,ITEMS_DEF+4u)?*(uint8_t **)(layer+ITEMS_DEF):NULL; void *actor=own_actor();
            unsigned item=def && readable(def,0x8cu) && *(void **)def==base+RVA_CONSUMABLE_VTABLE &&
                *(uint32_t *)(def+0x88u)==2u?*(uint32_t *)(def+0x14u):~0u;
            if(item<999u && actor) {
                prompt.active=TRUE; prompt.layer=layer; prompt.actor=actor; prompt.item=item; prompt.choice=0;
                prompt.row=*(int *)(layer+ITEMS_SELECTED); prompt.render_logged=FALSE;
                trace("confirm_prompt",item,(unsigned)prompt.row); skip=TRUE;
            } else trace("confirm_native",item,actor!=NULL);
        }
    }
    SetLastError(saved); return skip;
}
static void __attribute__((naked,noinline)) confirm_stub(void) {
    __asm__ volatile(
        "pushal; mov %esp,%ebp; and $-16,%esp; sub $16,%esp;"
        "mov 0x10(%ebp),%eax; mov %eax,(%esp); mov (%ebp),%eax; mov %eax,4(%esp); call _confirm_intercept; test %al,%al;"
        "mov %ebp,%esp; popal; jnz 1f; jmp *_SudekiMpStoryMenuConfirmTarget;"
        "1: ret");
}
static BOOL image_exact(uint8_t *b) {
    /* Items list confirm context: sound call, ESI = layer, the mode switch,
     * then `mov al,1` and the epilogue; EDI = 5 is loaded at RVA 0x177104. */
    static const uint8_t confirm_tail[]={0xb0,0x01,0x5f,0x5e,0x5b,0x8b,0xe5};
    if(!readable(b+0x177104u,5u) || b[0x177104u]!=0xbfu || *(uint32_t *)(b+0x177105u)!=5u ||
        !readable(b+0x17714fu,0x24u) ||
        b[0x17714fu]!=0x8bu || b[0x177150u]!=0x15u || *(uint32_t *)(b+0x177151u)!=(uint32_t)(uintptr_t)(b+0x408d1cu) ||
        b[0x177155u]!=0x52u || b[0x177156u]!=0xb8u || *(uint32_t *)(b+0x177157u)!=(uint32_t)(uintptr_t)(b+0x2ca7f4u) ||
        memcmp(b+0x17715bu,"\x8d\x74\x24\x10",4u) ||
        b[0x177164u]!=0x8bu || b[0x177165u]!=0xf3u || memcmp(b+0x17716bu,confirm_tail,sizeof(confirm_tail))) return FALSE;
    static const uint8_t cycle_tail[]={0x83,0xec,0x1c,0x53,0x56,0x8b,0xf0};
    static const uint8_t open_entry[]={0x55,0x8b,0xec,0x83,0xe4,0xf8,0x81,0xec,0xb0,0x00,0x00,0x00};
    return readable(b+RVA_CYCLE,13u) && b[RVA_CYCLE]==0x8bu && b[RVA_CYCLE+1u]==0x0du &&
        *(uint32_t *)(b+RVA_CYCLE+2u)==(uint32_t)(uintptr_t)(b+RVA_GROUP_GLOBAL) &&
        !memcmp(b+RVA_CYCLE+6u,cycle_tail,sizeof(cycle_tail)) &&
        readable(b+RVA_MENU_OPEN,sizeof(open_entry)) && !memcmp(b+RVA_MENU_OPEN,open_entry,sizeof(open_entry)) &&
        readable(b+RVA_MENU_OPEN_SLOT,4u) && *(void **)(b+RVA_MENU_OPEN_SLOT)==b+RVA_MENU_OPEN;
}
BOOL SudekiMpLanStoryMainMenuInstall(HMODULE image,SudekiMpStoryMenuSlotAllowed allowed,
    SudekiMpStoryMenuEquipRequest request,SudekiMpStoryMenuEquipRequest use) {
    uint8_t *b=(uint8_t *)image;
    if(!image || installed || !SudekiMpCheckLoadedExecutable(image) || !image_exact(b)) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    base=b; slot_allowed=allowed; equip_request=request; use_request=use;
    SudekiMpStoryMenuUseTarget=b+RVA_USE; SudekiMpStoryMenuConfirmTarget=b+RVA_MODE_SWITCH;
    memset(&prompt,0,sizeof(prompt));
    SudekiMpStoryMenuCycleTarget=b+RVA_CYCLE; SudekiMpStoryMenuEquipTarget=b+RVA_EQUIP;
    SudekiMpStoryMenuPauseTarget=b+RVA_ENTITY_PAUSE; SudekiMpStoryMenuSkipPause=request?1u:0u;
    SudekiMpStoryMenuMoveTarget=b+RVA_DISABLE_MOVE;
    native_open=(MenuOpen)(b+RVA_MENU_OPEN);
    if(!SudekiMpInstallRelativeCallHook(&cycle_hooks[0],b+RVA_CYCLE_NEXT_CALL,b+RVA_CYCLE,(const void *)(uintptr_t)cycle_stub) ||
        !SudekiMpInstallRelativeCallHook(&cycle_hooks[1],b+RVA_CYCLE_PREVIOUS_CALL,b+RVA_CYCLE,(const void *)(uintptr_t)cycle_stub) ||
        !SudekiMpInstallRelativeCallHook(&equip_hook,b+RVA_EQUIP_CALL,b+RVA_EQUIP,(const void *)(uintptr_t)equip_stub) ||
        !SudekiMpInstallRelativeCallHook(&pause_hooks[0],b+RVA_OPEN_PAUSE_CALL,b+RVA_ENTITY_PAUSE,(const void *)(uintptr_t)pause_stub) ||
        !SudekiMpInstallRelativeCallHook(&pause_hooks[1],b+RVA_CLOSE_PAUSE_CALL,b+RVA_ENTITY_PAUSE,(const void *)(uintptr_t)pause_stub) ||
        !SudekiMpInstallRelativeCallHook(&move_hooks[0],b+RVA_OPEN_MOVE_CALL,b+RVA_DISABLE_MOVE,(const void *)(uintptr_t)move_stub) ||
        !SudekiMpInstallRelativeCallHook(&move_hooks[1],b+RVA_CLOSE_MOVE_CALL,b+RVA_DISABLE_MOVE,(const void *)(uintptr_t)move_stub) ||
        !SudekiMpInstallRelativeCallHook(&use_hook,b+RVA_USE_CALL,b+RVA_USE,(const void *)(uintptr_t)use_stub) ||
        !SudekiMpInstallRelativeCallHook(&confirm_hook,b+RVA_LIST_CONFIRM_CALL,b+RVA_MODE_SWITCH,(const void *)(uintptr_t)confirm_stub) ||
        !SudekiMpInstallPointerHook(&open_hook,(void **)(b+RVA_MENU_OPEN_SLOT),native_open,(const void *)(uintptr_t)open_menu)) {
        DWORD error=GetLastError();
        (void)SudekiMpLanStoryMainMenuUninstall(); SetLastError(error); return FALSE;
    }
    installed=TRUE; redirected=requests=0;
    SudekiMpLogFormat("story_main_menu event=install scope=%s equip=%s entity_pause=%s menu_pause=%s\r\n",
        allowed?"owned_slots":"all",request?"request":"native",request?"skipped":"native",request?"skipped":"native");
    return TRUE;
}
BOOL SudekiMpLanStoryMainMenuUninstall(void) {
    BOOL ok=TRUE; DWORD error=ERROR_SUCCESS;
    if(open_hook.installed && !SudekiMpRestorePointerHook(&open_hook)) { ok=FALSE; error=GetLastError(); }
    if(confirm_hook.installed && !SudekiMpRestoreRelativeCallHook(&confirm_hook)) { ok=FALSE; if(!error) error=GetLastError(); }
    if(use_hook.installed && !SudekiMpRestoreRelativeCallHook(&use_hook)) { ok=FALSE; if(!error) error=GetLastError(); }
    if(equip_hook.installed && !SudekiMpRestoreRelativeCallHook(&equip_hook)) { ok=FALSE; if(!error) error=GetLastError(); }
    for(unsigned i=2u;i>0;--i)
        if(move_hooks[i-1u].installed && !SudekiMpRestoreRelativeCallHook(&move_hooks[i-1u])) { ok=FALSE; if(!error) error=GetLastError(); }
    for(unsigned i=2u;i>0;--i)
        if(pause_hooks[i-1u].installed && !SudekiMpRestoreRelativeCallHook(&pause_hooks[i-1u])) { ok=FALSE; if(!error) error=GetLastError(); }
    for(unsigned i=2u;i>0;--i)
        if(cycle_hooks[i-1u].installed && !SudekiMpRestoreRelativeCallHook(&cycle_hooks[i-1u])) { ok=FALSE; if(!error) error=GetLastError(); }
    if(!ok) { SetLastError(error); return FALSE; } /* A failed restoration retains the adapter. */
    installed=FALSE; slot_allowed=NULL; equip_request=NULL; use_request=NULL; base=NULL; native_open=NULL; SudekiMpStoryMenuSkipPause=0;
    memset(&prompt,0,sizeof(prompt));
    return TRUE;
}
BOOL SudekiMpLanStoryMainMenuVisible(void) {
    if(!installed || !readable(base+RVA_UI_GLOBAL,4u)) return FALSE;
    uint8_t *ui=*(uint8_t **)(base+RVA_UI_GLOBAL);
    return readable(ui,UI_SELECTED+4u) && *(void **)ui==base+RVA_MENU_VTABLE && ui[MENU_SHOWN]==1u;
}
BOOL SudekiMpLanStoryMainMenuRequestOpen(void) {
    if(!installed || !readable(base+RVA_HUD_GLOBAL,4u)) return FALSE;
    uint8_t *hud=*(uint8_t **)(base+RVA_HUD_GLOBAL);
    if(!readable(hud,HUD_STATE+4u) || *(void **)hud!=base+RVA_HUD_VTABLE ||
        *(unsigned *)(hud+HUD_STATE)!=1u || SudekiMpLanStoryMainMenuVisible() ||
        !readable(*(void **)hud,0x30u)) return FALSE;
    HudInput input=(*(HudInput **)hud)[0x2cu/4u];
    uint8_t result=input(hud,HUD_OPEN_MAIN_MENU,0u,0u);
    trace("open_requested",result,*(unsigned *)(hud+HUD_STATE));
    return TRUE;
}
BOOL SudekiMpLanStoryMainMenuRequestClose(void) {
    /* "Back" goes to whichever UI object the front scene currently routes
     * input to (front+0x170): a sub-page layer (e.g. Weapons, handler RVA
     * 0x16F110, back = return to the page list) or the Main Menu itself
     * (handler RVA 0x93980, back = leave through state 6). Same thiscall
     * (this, kind, command, value) RET 0xC contract the native router uses. */
    if(!SudekiMpLanStoryMainMenuVisible() || !readable(base+0x408d1cu,4u)) return FALSE;
    uint8_t *front=*(uint8_t **)(base+0x408d1cu),*ui=*(uint8_t **)(base+RVA_UI_GLOBAL);
    if(!readable(front,0x174u)) return FALSE;
    uint8_t *active=*(uint8_t **)(front+0x170u);
    if(!readable(active,0x30u) || !readable(*(void **)active,0x30u)) return FALSE;
    uint8_t *vt=*(uint8_t **)active,*handler=*(uint8_t **)(vt+0x2cu);
    if(vt<base+0x2c0000u || vt>=base+0x300000u || handler<base+0x1000u || handler>=base+0x2c0000u) {
        trace("close_unknown_ui",(unsigned)(vt-base),(unsigned)(handler-base)); return FALSE;
    }
    MenuInput input=(MenuInput)handler;
    unsigned result=input(active,5u,1u,0u);
    trace(active==ui?"close_requested":"back_requested",(unsigned)(handler-base),*(unsigned *)(ui+0x4cu));
    (void)result; return TRUE;
}
static BOOL deliver_active(unsigned command,const char *event) {
    if(!readable(base+0x408d1cu,4u)) return FALSE;
    uint8_t *front=*(uint8_t **)(base+0x408d1cu),*ui=*(uint8_t **)(base+RVA_UI_GLOBAL);
    if(!readable(front,0x174u)) return FALSE;
    uint8_t *active=*(uint8_t **)(front+0x170u);
    if(!readable(active,0x4cu) || !readable(*(void **)active,0x30u)) return FALSE;
    uint8_t *vt=*(uint8_t **)active,*handler=*(uint8_t **)(vt+0x2cu);
    if(vt<base+0x2c0000u || vt>=base+0x300000u || handler<base+0x1000u || handler>=base+0x2c0000u) return FALSE;
    /* Info (command 10) exists on the Weapons layer only, while browsing. */
    if(command==10u && (handler!=base+0x16f110u || *(unsigned *)(active+0x48u)!=2u)) return FALSE;
    unsigned result=((MenuInput)handler)(active,5u,command,0u);
    trace(event,(unsigned)(handler-base),active==ui?*(unsigned *)(ui+0x4cu):*(unsigned *)(active+0x48u));
    (void)result; return TRUE;
}
static uint8_t *active_ui(void) {
    if(!readable(base+0x408d1cu,4u)) return NULL;
    uint8_t *front=*(uint8_t **)(base+0x408d1cu);
    return readable(front,0x174u)?*(uint8_t **)(front+0x170u):NULL;
}
void SudekiMpLanStoryMainMenuPollKeys(void) {
    enum { K_M, K_ESCAPE, K_F3, K_RETURN, K_SPACE, K_LEFT, K_RIGHT, K_TAB, K_COUNT };
    static BOOL down[K_COUNT]; static const int keys[K_COUNT]={'M',VK_ESCAPE,VK_F3,VK_RETURN,VK_SPACE,VK_LEFT,VK_RIGHT,VK_TAB};
    if(!installed || !SudekiMpLanStoryMainMenuVisible()) { memset(down,0,sizeof(down)); prompt_close("confirm_dropped"); return; }
    DWORD owner=0; HWND fg=GetForegroundWindow();
    BOOL focused=fg && GetWindowThreadProcessId(fg,&owner) && owner==GetCurrentProcessId();
    /* The prompt belongs to one Items layer and one highlighted row. */
    if(prompt.active && (active_ui()!=prompt.layer || !readable(prompt.layer,ITEMS_DEF+4u) ||
        *(int *)(prompt.layer+ITEMS_SELECTED)!=prompt.row)) prompt_close("confirm_dropped");
    BOOL edge[K_COUNT];
    for(unsigned i=0;i<K_COUNT;++i) {
        BOOL now=(GetAsyncKeyState(keys[i])&0x8000)!=0;
        edge[i]=now && !down[i] && focused; down[i]=now;
    }
    if(prompt.active) {
        if(edge[K_RETURN] || edge[K_SPACE]) prompt_decide();
        else if(edge[K_ESCAPE] || edge[K_M]) { prompt.suppress_until=GetTickCount()+CONFIRM_SUPPRESS_MS; prompt_close("confirm_cancel"); }
        else if(edge[K_LEFT] || edge[K_RIGHT] || edge[K_TAB]) prompt.choice^=1u;
        return;
    }
    if(prompt.suppress_until && (int)(prompt.suppress_until-GetTickCount())>0 && (edge[K_ESCAPE] || edge[K_M])) return;
    if(edge[K_M]) (void)deliver_active(1u,"m_back");
    else if(edge[K_ESCAPE]) (void)deliver_active(1u,"escape_back");
    else if(edge[K_F3]) (void)deliver_active(10u,"f3_info");
}
typedef void (__attribute__((stdcall)) *ItemsRebuild)(void *);
BOOL SudekiMpLanStoryMainMenuRefreshItems(void) {
    static const uint8_t rebuild_entry[]={0x55,0x8b,0xec,0x83,0xe4,0xf8,0x83,0xec,0x4c,0x53,0x56,0x8b,0x75,0x08};
    static const uint8_t panel_entry[]={0x53,0x55,0x8b,0x6c,0x24,0x0c,0x56,0x57,0x8d,0xb5,0x70,0x01,0x00,0x00,0xbf,0x04,0x00,0x00,0x00};
    if(!installed || !SudekiMpLanStoryMainMenuVisible()) return FALSE;
    uint8_t *ui=*(uint8_t **)(base+RVA_UI_GLOBAL),*layer=active_ui();
    if(!readable(ui,UI_PARTY_PANEL+4u) || !readable(layer,ITEMS_DEF+4u) || *(void **)layer!=base+RVA_ITEMS_VTABLE ||
        !readable(base+RVA_ITEMS_REBUILD,sizeof(rebuild_entry)) || memcmp(base+RVA_ITEMS_REBUILD,rebuild_entry,sizeof(rebuild_entry)) ||
        !readable(base+RVA_PANEL_REFRESH,sizeof(panel_entry)) || memcmp(base+RVA_PANEL_REFRESH,panel_entry,sizeof(panel_entry))) {
        trace("items_refresh_refused",layer?(unsigned)((uint8_t *)*(void **)layer-base):0u,0); return FALSE;
    }
    uint8_t *panel=*(uint8_t **)(ui+UI_PARTY_PANEL);
    if(!readable(panel,PANEL_CURSOR+4u)) return FALSE;
    ((ItemsRebuild)(base+RVA_ITEMS_REBUILD))(layer);
    unsigned cursor=*(unsigned *)(panel+PANEL_CURSOR); void *target=base+RVA_PANEL_REFRESH;
    __asm__ volatile("push %%edx\n\tcall *%%ecx" : "+a"(cursor), "+c"(target), "+d"(panel) : : "memory", "cc");
    trace("items_refreshed",*(unsigned *)(layer+ITEMS_SELECTED),cursor); return TRUE;
}
BOOL SudekiMpLanStoryMainMenuPrompt(unsigned *choice) {
    if(!prompt.active) return FALSE;
    if(choice) *choice=prompt.choice;
    return TRUE;
}
void SudekiMpLanStoryMainMenuPromptRendered(BOOL ok,DWORD error) {
    if(prompt.active && !prompt.render_logged) { prompt.render_logged=TRUE; trace("confirm_render",(unsigned)ok,(unsigned)error); }
}
void SudekiMpLanStoryMainMenuCounters(unsigned *out_redirected,unsigned *out_requests) {
    if(out_redirected) *out_redirected=redirected;
    if(out_requests) *out_requests=requests;
}
