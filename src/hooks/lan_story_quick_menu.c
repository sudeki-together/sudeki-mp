#include "hooks/lan_story_quick_menu.h"
#include "hooks/lan_story_input.h"
#include "hooks/lan_story_realtime.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <stdint.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Story Q menu requires the supported native x86 ABI"
#endif
enum { MENU_GLOBAL=0x3c2f84, MENU_VT=0x2caf1c, FRONT_GLOBAL=0x408d1c };
typedef uint8_t (__attribute__((thiscall)) *MenuInput)(void *,unsigned,unsigned,unsigned);
typedef void (__attribute__((thiscall)) *MenuTransition)(void *);
static uint8_t *base;
static MenuInput native_input;
static MenuTransition native_open,native_close;
static void *native_submit;
static SudekiMpPointerHook input_hook,open_hook,close_hook;
static SudekiMpRelativeCallHook script_hooks[2];
static SudekiMpStoryQuickSkillRequest request_skill;
static DWORD native_thread;
static BOOL installed,retained,fault,admission;
static unsigned callbacks,scope,transition;
static void *menu_owner,*actor_owner,*controller_owner,*group_owner;
static uint32_t owner_transaction;
static unsigned trace_count;

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && a<=UINTPTR_MAX-n && VirtualQuery(p,&m,sizeof(m))==sizeof(m) &&
        m.State==MEM_COMMIT && !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
        a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL thread_exact(void) { return native_thread && native_thread==GetCurrentThreadId(); }
static BOOL call_exact(const SudekiMpRelativeCallHook *h) {
    return h->installed && readable(h->instruction,5u) && h->instruction[0]==0xe8u &&
        !memcmp(h->instruction+1u,&h->replacement_displacement,4u);
}
static BOOL pointer_exact(const SudekiMpPointerHook *h) {
    return h->installed && readable(h->slot,4u) && *h->slot==h->replacement_value;
}
static BOOL hooks_exact(void) {
    return installed && pointer_exact(&input_hook) && pointer_exact(&open_hook) &&
        pointer_exact(&close_hook) && call_exact(&script_hooks[0]) && call_exact(&script_hooks[1]) &&
        SudekiMpLanStoryRealtimeExact((HMODULE)base);
}
static BOOL menu_exact(void *menu) {
    return base && readable(base+MENU_GLOBAL,4u) && *(void **)(base+MENU_GLOBAL)==menu &&
        readable(menu,0x224u) && *(void **)menu==base+MENU_VT;
}
static BOOL binding_exact(void) {
    return thread_exact() && owner_transaction && actor_owner && controller_owner &&
        readable(base+0x408d94u,4u) && *(void **)(base+0x408d94u)==group_owner &&
        readable(group_owner,0x94u) && *(void **)((uint8_t *)group_owner+0x90u)==actor_owner &&
        SudekiMpLanStoryInputExact(controller_owner,actor_owner);
}
static BOOL roster_exact(const SudekiMpLanStoryNativeRoster *r) {
    return r && r->leader_character<4u && (r->available_mask&(1u<<r->leader_character)) &&
        r->controller==controller_owner && r->group==group_owner && r->actors[r->leader_character]==actor_owner &&
        SudekiMpLanStoryClientRosterExact(r) && binding_exact();
}
static BOOL visible(void) {
    return menu_owner && menu_exact(menu_owner) && ((uint8_t *)menu_owner)[0x29u];
}
BOOL SudekiMpLanStoryQuickMenuCapturesInput(void) {
    /* An unknown retained owner still closes movement admission. */
    return retained && (menu_owner || fault);
}
static BOOL pin(DWORD error) {
    HMODULE self;
    (void)GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        (LPCWSTR)(uintptr_t)&SudekiMpLanStoryQuickMenuUninstall,&self);
    SetLastError(error?error:ERROR_BUSY); return FALSE;
}
static void trace_menu(const char *event,const char *reason) {
    if(trace_count++<32u) SudekiMpLogFormat(
        "story_quick_menu event=%s reason=%s transaction=%lu actor=%p admitted=%u\r\n",
        event,reason,(unsigned long)owner_transaction,actor_owner,admission?1u:0u);
}
static void close_owned(const char *reason) {
    if(!scope || transition || !menu_owner || !menu_exact(menu_owner) || !binding_exact()) {
        fault=TRUE; return;
    }
    if(visible()) {
        /* Retail successful selection/toggle performs this exact pair. Keep
         * the active UI listener and balanced actor UI lock native. */
        native_input(menu_owner,4u,0u,0u);
        transition=2u; native_close(menu_owner); transition=0;
    }
    if(!menu_exact(menu_owner) || visible()) { fault=TRUE; return; }
    menu_owner=NULL;
    trace_menu("closed",reason);
}
static void __attribute__((thiscall,force_align_arg_pointer)) open_menu(void *menu) {
    ++callbacks;
    if(!retained) { if(native_open) native_open(menu); }
    else if(scope && !transition && admission && !fault && hooks_exact() &&
        binding_exact() && menu_exact(menu) && !menu_owner) {
        /* Retain BEFORE native UI lock acquisition. If a postcondition fails
         * only the same native owner may close it; never zero actor flags. */
        menu_owner=menu; transition=1u;
        native_open(menu); transition=0;
        if(!visible()) fault=TRUE;
        else trace_menu("opened","local_q");
    }
    --callbacks;
}
static void __attribute__((thiscall,force_align_arg_pointer)) close_menu(void *menu) {
    ++callbacks;
    if(!retained) { if(native_close) native_close(menu); }
    else if(scope && !transition && menu==menu_owner) close_owned("native_cancel");
    /* No owned activation means no native UI lock or script to release. */
    --callbacks;
}
/* Both exact OnQuickmenuActivate/Deactivate callsites use EAX=descriptor,
 * seven stack arguments, RET28. Suppression happens BEFORE task creation:
 * return the native empty task-reference result (arg2), not a fabricated
 * terminal task. Leave descriptor destruction/refcounts with retail. */
static BOOL __attribute__((cdecl,used,force_align_arg_pointer)) suppress_script(void) {
    if(!retained) return FALSE;
    DWORD error=GetLastError();
    if(!thread_exact() || !scope || !transition || !menu_owner || !menu_exact(menu_owner)) fault=TRUE;
    SetLastError(error);
    return TRUE;
}
__attribute__((naked,noinline,used)) static void submit_script(void) {
    __asm__ volatile(
        "pushfl\n\tpushal\n\tmovl %esp,%ebp\n\tsubl $528,%esp\n\tandl $-16,%esp\n\t"
        "fxsave (%esp)\n\tcall _suppress_script\n\tmovl %eax,12(%ebp)\n\tfxrstor (%esp)\n\t"
        "movl %ebp,%esp\n\tcmpl $0,12(%esp)\n\tje 1f\n\tpopal\n\tpopfl\n\t"
        "movl 8(%esp),%eax\n\tmovl $0,(%eax)\n\tret $28\n\t"
        "1: popal\n\tpopfl\n\tjmp *_native_submit\n\t");
}
typedef struct UiEvent { void *menu; unsigned kind,command,value; uint8_t result; } UiEvent;
static BOOL ui_witness(void *context) {
    UiEvent *e=context;
    return callbacks==1u && thread_exact() && hooks_exact() && scope<=1u &&
        e && menu_exact(e->menu);
}
static BOOL input_present(const SudekiMpLanStoryNativeRoster *r,
    const SudekiMpLanStoryScene *scene,void *context) {
    (void)scene; UiEvent *e=context;
    if(!roster_exact(r) || !menu_exact(e->menu) || e->menu!=menu_owner || fault) return FALSE;
    ++scope;
    /* Never enter the retail action dispatcher: it can consume items, alter
     * party AI/equipment or execute CSkill::Use on this contained replica.
     * Reading a displayed row only reserves an ordinary host skill request. */
    if((e->kind==5u || e->kind==6u) && (e->command==0u || e->command==2u)) {
        e->result=1;
        uint8_t *m=e->menu,*list=*(uint8_t **)(m+0x214u),*category=*(uint8_t **)(m+0x208u);
        if(e->kind==5u && e->value && admission && *(unsigned *)(m+0x204u)==0u &&
            !m[0x188u] && !m[0x189u] && !m[0xfdu] && !m[0xfeu] &&
            !*(unsigned *)(m+0x108u) && readable(category,0xbcu) &&
            *(unsigned *)(category+0xb4u)==*(unsigned *)(category+0xb8u) &&
            readable(list,0x480u)) {
            int row=*(int *)(list+0x448u); unsigned count=*(unsigned *)(list+0x474u);
            uint8_t **rows=*(uint8_t ***)(list+0x47cu);
            if(row>=0 && row<6 && count<=6u && (unsigned)row<count &&
                readable(rows,count*4u) && readable(rows[row],0x88u)) {
                int slot=*(int *)(rows[row]+0x84u);
                if(slot>=0 && slot<6 && request_skill(actor_owner,(unsigned)slot)) close_owned("skill_queued");
            }
        }
    } else if(e->kind!=0x19u) {
        /* Pointer forwarding can invoke a child widget directly. Keep it
         * closed until the complete child command closure is validated. */
        e->result=native_input(e->menu,e->kind,e->command,e->value);
    }
    --scope;
    return !fault && SudekiMpLanStoryClientRosterExact(r);
}
static uint8_t __attribute__((thiscall,force_align_arg_pointer)) input_menu(
    void *menu,unsigned kind,unsigned command,unsigned value) {
    ++callbacks; UiEvent e={menu,kind,command,value,0};
    if(!retained) e.result=native_input?native_input(menu,kind,command,value):0;
    else if(scope && menu==menu_owner && menu_exact(menu) &&
        kind!=5u && kind!=6u && kind!=0x19u) {
        /* Native activation/deactivation selects listeners synchronously. */
        e.result=native_input(menu,kind,command,value);
    } else if(menu==menu_owner && visible() && binding_exact()) {
        (void)SudekiMpLanStoryClientUiPresent(input_present,&e,ui_witness,&e);
    }
    --callbacks; return e.result;
}
__attribute__((naked,noinline)) static void toggle_native(void *front __attribute__((unused)),
    void *function __attribute__((unused))) {
    __asm__ volatile("movl 4(%esp),%eax\n\tcall *8(%esp)\n\tret\n\t");
}
BOOL SudekiMpLanStoryQuickMenuService(const SudekiMpLanStoryNativeRoster *r,
    uint32_t transaction,BOOL owned,BOOL admitted,BOOL toggle) {
    if(!installed || !r || callbacks || scope || !SudekiMpLanStoryClientRosterExact(r)) return FALSE;
    if(!native_thread) native_thread=GetCurrentThreadId();
    if(!thread_exact() || !hooks_exact()) return FALSE;
    admission=FALSE;
    if(menu_owner && (!transaction || owner_transaction!=transaction || !owned)) {
        if(!roster_exact(r)) return FALSE;
        ++scope; close_owned("ownership_changed"); --scope;
        if(menu_owner) return FALSE;
    }
    if(!owned || !transaction || r->leader_character>=4u || fault) return !menu_owner && !fault;
    if(menu_owner && !roster_exact(r)) return FALSE;
    controller_owner=r->controller; group_owner=r->group;
    actor_owner=r->actors[r->leader_character]; owner_transaction=transaction;
    if(!roster_exact(r)) return FALSE;
    admission=admitted;
    if(toggle && (menu_owner || admitted)) {
        uint8_t *front=*(uint8_t **)(base+FRONT_GLOBAL);
        if(!readable(front,0x178u) || !front[0x8cu]) return FALSE;
        ++scope;
        if(menu_owner) close_owned("local_q");
        else {
            uint8_t *ui=*(uint8_t **)(front+0x170u);
            uint8_t *quick=*(uint8_t **)(base+MENU_GLOBAL);
            /* Only the native gameplay HUD may open Q. Never turn a save,
             * speech, shop or frontend screen into an assumed gameplay UI. */
            if(menu_exact(quick) && !quick[0x29u] && ui &&
                ui==*(void **)(front+0x174u) && readable(ui,0xd0u) &&
                *(void **)(ui+0x70u)==quick &&
                *(void **)ui==base+0x2caf9cu && *(void **)(base+0x2cafc8u)==base+0x9c930u)
                toggle_native(front,base+0xa080u);
        }
        --scope;
    }
    if(menu_owner) SudekiMpLanStoryInputMuteMovement();
    return !fault && SudekiMpLanStoryClientRosterExact(r);
}
BOOL SudekiMpLanStoryQuickMenuUninstall(void) {
    admission=FALSE;
    if(menu_owner || callbacks || scope || transition || (native_thread && !thread_exact())) return pin(ERROR_BUSY);
    /* Close entry first, retain script interception until all UI callers
     * restore. Immutable native callbacks survive fetched-slot retirement. */
    if(!SudekiMpRestorePointerHook(&input_hook) || !SudekiMpRestorePointerHook(&close_hook) ||
        !SudekiMpRestorePointerHook(&open_hook)) return pin(GetLastError());
    for(unsigned i=2u;i>0;--i)
        if(!SudekiMpRestoreRelativeCallHook(&script_hooks[i-1u])) return pin(GetLastError());
    installed=retained=fault=FALSE; native_thread=0;
    controller_owner=actor_owner=group_owner=NULL; owner_transaction=0; request_skill=NULL;
    return TRUE;
}
BOOL SudekiMpLanStoryQuickMenuInstall(HMODULE image,SudekiMpStoryQuickSkillRequest request) {
    uint8_t *b=(uint8_t *)image;
    if(!image || !request || retained || installed || (base && base!=b) ||
        !SudekiMpCheckLoadedExecutable(image) || !SudekiMpLanStoryRealtimeExact(image)) return FALSE;
    if(*(void **)(b+MENU_VT+0x2cu)!=b+0x98b40u ||
        *(void **)(b+MENU_VT+0x40u)!=b+0x98ec0u ||
        *(void **)(b+MENU_VT+0x44u)!=b+0x99180u) { SetLastError(ERROR_INVALID_DATA); return FALSE; }
    base=b; native_input=(MenuInput)(b+0x98b40u);
    native_open=(MenuTransition)(b+0x98ec0u); native_close=(MenuTransition)(b+0x99180u);
    native_submit=b+0x1c37b0u; request_skill=request; retained=TRUE;
    static const unsigned sites[]={0x990fau,0x992e1u};
    for(unsigned i=0;i<2u;++i)
        if(!SudekiMpInstallRelativeCallHook(&script_hooks[i],b+sites[i],native_submit,submit_script)) goto fail;
    if(!SudekiMpInstallPointerHook(&open_hook,(void **)(b+MENU_VT+0x40u),native_open,open_menu) ||
        !SudekiMpInstallPointerHook(&close_hook,(void **)(b+MENU_VT+0x44u),native_close,close_menu) ||
        !SudekiMpInstallPointerHook(&input_hook,(void **)(b+MENU_VT+0x2cu),native_input,input_menu)) goto fail;
    installed=TRUE; return TRUE;
fail: {
        DWORD error=GetLastError();
        if(!SudekiMpLanStoryQuickMenuUninstall()) return FALSE;
        SetLastError(error); return FALSE;
    }
}
