#include "hooks/lan_story_remote_use.h"
#include "hooks/call_hook.h"
#include "engine/log.h"
#include <math.h>
#include <stdint.h>
#include <string.h>
#if !defined(__GNUC__) || !defined(__i386__)
#error "Remote use requires the supported x86 ABI"
#endif
/* Supported-image RVAs (CONFIRMED_STATIC 2026-10-16, see the header). */
enum {
    RVA_HANDLE_COPY=0x15b0u, RVA_CONTROLLER_SET=0x295b0u, RVA_GROUP=0x408d94u, RVA_REGISTRY=0x409d8cu,
    RVA_LISTENER=0xb5c30u, RVA_TREASURE_USE=0x184ec0u, RVA_TREASURE_UPDATE=0x1849d0u,
    RVA_TREASURE_VT=0x2d514cu, RVA_TREASURE_USE_SLOT=0x2d51a0u, RVA_NODE_VT=0x2d51c4u,
    RVA_NODE_UPDATE_SLOT=0x2d51c8u, RVA_CHEST_VT=0x2d5200u, RVA_POSITION_VT=0x2cdefcu,
    CHEST_KIND=0x0f9cu, TREASURE_OFFSET=0x688u, NODE_OFFSET=0x18u, LISTENER_OFFSET=0x3cu,
    STATE_OFFSET=0x74u, STATE_DONE=8u, GROUP_FRONT=0x90u, GROUP_SLOT=0xcu, SLOTS=4u,
    OPENS=4u, OPEN_LIMIT_MS=20000u, RVA_PLAY_USE=0xb5ec0u, RVA_USABLE_RELEASE=0xb5e00u,
    RETRY_MS=1500u, ABORT_MS=5000u, RVA_FINISH_SLOT=0x2d51acu, RVA_TREASURE_FINISH=0x185000u
};
static const uint32_t front_sites[]={
    0xb574eu,0xb591fu,0xb5c54u,0xb5d21u,0xb5e74u,0xb5ed9u,0xb5f44u,0xb5feau,
    0xb60d8u,0xb6321u,0xb64afu,0xb67c4u,0xb68beu,0x184b3eu,0x184c75u,0x184f53u };
static const uint32_t controller_sites[]={0x184f7eu,0x184b69u,0x184ca0u};
enum { FRONT_SITES=sizeof(front_sites)/sizeof(*front_sites),
    CONTROLLER_SITES=sizeof(controller_sites)/sizeof(*controller_sites) };

static uint8_t *base;
static DWORD game_thread;
static SudekiMpRelativeCallHook front_hooks[FRONT_SITES],controller_hooks[CONTROLLER_SITES];
static SudekiMpPointerHook update_hook,finish_hook;
void *remote_use_finish_original __attribute__((used));
static BOOL installed;
/* Scope read by the naked stubs: all three are zero outside a scope. */
volatile DWORD remote_use_scope_thread __attribute__((used));
void *volatile remote_use_scope_front __attribute__((used));
void *volatile remote_use_scope_slot __attribute__((used));
void *remote_use_copy_target __attribute__((used));
void *remote_use_controller_target __attribute__((used));
typedef void (__attribute__((thiscall)) *NodeUpdate)(void *,void *);
static NodeUpdate update_original;
/* The chest writes its use animation id (treasure+0x64, e.g. 0x52) into the
 * opener's arbiter action fields (+0x58 bits 8..16, +0x5C bits 0..8) through
 * 0xB5E60. Nothing returns a non-front character's fields to idle, so the
 * open animation loops (CONFIRMED_LIVE 2026-10-16: clearing them returned
 * Tal to idle). The fields are recorded at start and restored at the end
 * when they still hold the chest's value. */
typedef struct Open { uint8_t *treasure; void *actor; DWORD started; uint32_t action58,action5c,chest_action; unsigned retries; DWORD last_update; } Open;
static Open opens[OPENS];
static unsigned logs;
static BOOL readable(const void *p,size_t n);
static void log_event(const char *format,const char *what,unsigned a,unsigned b);
static uint8_t *arbiter_of(const void *actor) {
    uint8_t *a=readable(actor,0x94u)?*(uint8_t **)((const uint8_t *)actor+0x90u):NULL;
    return readable(a,0x60u) && *(const void *const *)(a+0x10u)==actor?a:NULL;
}
static void restore_action(const Open *o) {
    uint8_t *a=arbiter_of(o->actor);
    if(!a) return;
    /* treasure+0x64 is cleared before the open finishes: use the id seen
     * while it ran (CONFIRMED_LIVE 2026-10-16: restore read 0 otherwise). */
    uint32_t chest_action=o->chest_action;
    if(!chest_action) { log_event("story_remote_use event=%s action=%u restored=%u\r\n","action_end",0u,0u); return; }
    uint32_t *w58=(uint32_t *)(a+0x58u),*w5c=(uint32_t *)(a+0x5cu);
    BOOL changed=FALSE;
    if(((*w58>>8)&0x1ffu)==chest_action && chest_action!=o->action58) { *w58=(*w58&0xfffe00ffu)|(o->action58<<8); changed=TRUE; }
    if((*w5c&0x1ffu)==chest_action && chest_action!=o->action5c) { *w5c=(*w5c&0xfffffe00u)|o->action5c; changed=TRUE; }
    log_event("story_remote_use event=%s action=%u restored=%u\r\n","action_end",chest_action,(unsigned)changed);
}

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m;
    if(!p || VirtualQuery(p,&m,sizeof(m))!=sizeof(m) || m.State!=MEM_COMMIT ||
        (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
    DWORD access=m.Protect&0xffu;
    if(access!=PAGE_READONLY && access!=PAGE_READWRITE && access!=PAGE_WRITECOPY &&
        access!=PAGE_EXECUTE_READ && access!=PAGE_EXECUTE_READWRITE && access!=PAGE_EXECUTE_WRITECOPY) return FALSE;
    return (uintptr_t)p+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static void log_event(const char *format,const char *what,unsigned a,unsigned b) {
    if(logs>=256u) return;
    ++logs; SudekiMpLogFormat(format,what,a,b);
}

/* CALL 0x15B0 replacement: EAX destination, ECX source, no stack arguments;
 * EDX and flags are scratch for the callee. Only the source is changed. */
__attribute__((naked)) static void front_copy_stub(void) {
    __asm__ volatile(
        "mov _remote_use_scope_thread,%edx\n\t"
        "test %edx,%edx\n\t"
        "jz 1f\n\t"
        "cmp %fs:0x24,%edx\n\t"
        "jne 1f\n\t"
        "cmp _remote_use_scope_front,%ecx\n\t"
        "jne 1f\n\t"
        "mov _remote_use_scope_slot,%ecx\n"
        "1:\n\t"
        "jmp *_remote_use_copy_target");
}
/* CALL 0x295B0 replacement: thiscall ECX=controller, one stack argument,
 * RET 4. Inside a scope the remote opener's use must not switch the host
 * controller, so the call is skipped. */
__attribute__((naked)) static void controller_stub(void) {
    __asm__ volatile(
        "mov _remote_use_scope_thread,%eax\n\t"
        "test %eax,%eax\n\t"
        "jz 1f\n\t"
        "cmp %fs:0x24,%eax\n\t"
        "jne 1f\n\t"
        "ret $4\n"
        "1:\n\t"
        "jmp *_remote_use_controller_target");
}

static uint8_t *group(void) {
    uint8_t *g=readable(base+RVA_GROUP,4u)?*(uint8_t **)(base+RVA_GROUP):NULL;
    return readable(g,GROUP_FRONT+SLOTS*GROUP_SLOT)?g:NULL;
}
static uint8_t *slot_of(uint8_t *g,const void *actor) {
    for(unsigned s=1;s<SLOTS;++s) if(*(void **)(g+GROUP_FRONT+s*GROUP_SLOT)==actor) return g+GROUP_FRONT+s*GROUP_SLOT;
    return NULL; /* slot 0 is the front itself: nothing to scope */
}
static BOOL scope_begin(const void *actor) {
    uint8_t *g=group(),*slot=g?slot_of(g,actor):NULL;
    if(!slot || remote_use_scope_thread) return FALSE;
    remote_use_scope_front=g+GROUP_FRONT; remote_use_scope_slot=slot;
    remote_use_scope_thread=GetCurrentThreadId();
    return TRUE;
}
static void scope_end(void) {
    remote_use_scope_thread=0; remote_use_scope_front=NULL; remote_use_scope_slot=NULL;
}
static BOOL in_registry(const void *e) {
    const uint8_t *owner=readable(base+RVA_REGISTRY,4u)?*(const uint8_t *const *)(base+RVA_REGISTRY):NULL;
    if(!readable(owner,0x40u)) return FALSE;
    unsigned n=*(const uint32_t *)(owner+0x34u);
    const void *const *entries=*(const void *const *const *)(owner+0x3cu);
    if(!n || n>8192u || !readable(entries,n*sizeof(void *))) return FALSE;
    for(unsigned i=0;i<n;++i) if(entries[i]==e) return TRUE;
    return FALSE;
}
static uint8_t *treasure_of(const uint8_t *chest) {
    if(!readable(chest,TREASURE_OFFSET+0x80u) || *(void *const *)chest!=base+RVA_CHEST_VT ||
        (*(const uint32_t *)(chest+0x30u)&0x1fffu)!=CHEST_KIND) return NULL;
    uint8_t *t=(uint8_t *)chest+TREASURE_OFFSET;
    return *(void **)t==base+RVA_TREASURE_VT && *(void **)(t+NODE_OFFSET)==base+RVA_NODE_VT &&
        *(const uint8_t *const *)(t+0x10u)==chest?t:NULL;
}
static Open *open_of(const uint8_t *treasure) {
    for(unsigned i=0;i<OPENS;++i) if(opens[i].treasure && opens[i].treasure==treasure) return &opens[i];
    return NULL;
}
/* State 1 waits for the opener's use animation to finish. Live 2026-10-16
 * one open never saw the animation start (UNKNOWN cause; the opener had just
 * stopped walking), which held the opener forever with bit 0x200. Inside the
 * scope, repeat the export's own request (anim id = treasure vtable +0x48,
 * then 0xB5EC0: EDI = id, stack = treasure, RET 4); past ABORT_MS put the
 * chest back to idle and release the opener. */
static void play_use(uint8_t *treasure) {
    typedef uint32_t (__attribute__((thiscall)) *AnimId)(void *);
    uint32_t id=((AnimId)(*(void ***)treasure)[0x48u/4u])(treasure);
    void *play=base+RVA_PLAY_USE; uintptr_t edi=id;
    __asm__ volatile("push %[t]\n\tcall *%[f]" : "+D"(edi) : [t] "r"(treasure),[f] "r"(play) : "eax","ecx","edx","memory","cc");
}
static void abort_open(Open *o) {
    uint8_t *treasure=o->treasure,*a=arbiter_of(o->actor);
    *(uint32_t *)(treasure+STATE_OFFSET)=0u;
    if(a) *(uint32_t *)(a+0x50u)&=~0x200u;
    restore_action(o);
    void *release=base+RVA_USABLE_RELEASE; uintptr_t eax=(uintptr_t)treasure;
    __asm__ volatile("call *%[f]" : "+a"(eax) : [f] "r"(release) : "ecx","edx","memory","cc");
    log_event("story_remote_use event=%s treasure=%u retries=%u\r\n","aborted",(unsigned)(uintptr_t)treasure,o->retries);
    memset(o,0,sizeof(*o));
}
static void __attribute__((thiscall)) update_wrapper(void *node,void *arg) {
    uint8_t *treasure=(uint8_t *)node-NODE_OFFSET; Open *o=GetCurrentThreadId()==game_thread?open_of(treasure):NULL;
    if(o) o->last_update=GetTickCount();
    if(o && (!in_registry(*(void **)(treasure+0x10u)) || !in_registry(o->actor) ||
        GetTickCount()-o->started>OPEN_LIMIT_MS)) {
        log_event("story_remote_use event=%s treasure=%u reason=%u\r\n","released",(unsigned)(uintptr_t)treasure,1u);
        if(in_registry(o->actor) && in_registry(*(void **)(treasure+0x10u))) restore_action(o);
        memset(o,0,sizeof(*o)); o=NULL;
    }
    if(o && *(uint32_t *)(treasure+STATE_OFFSET)==1u) {
        DWORD age=GetTickCount()-o->started;
        if(age>ABORT_MS) { abort_open(o); o=NULL; }
        else if(age>RETRY_MS*(o->retries+1u) && o->retries<2u && scope_begin(o->actor)) {
            ++o->retries; play_use(treasure); scope_end();
            log_event("story_remote_use event=%s treasure=%u retry=%u\r\n","replay_use",(unsigned)(uintptr_t)treasure,o->retries);
        }
    }
    BOOL scoped=o && scope_begin(o->actor);
    update_original(node,arg);
    if(scoped) scope_end();
    if(o) { uint32_t act=*(uint32_t *)(treasure+0x64u)&0x1ffu; if(act) o->chest_action=act; }
    if(o) {
        uint32_t state=*(uint32_t *)(treasure+STATE_OFFSET);
        if(state==0u || state==STATE_DONE) {
            log_event("story_remote_use event=%s treasure=%u state=%u\r\n","finished",(unsigned)(uintptr_t)treasure,state);
            restore_action(o);
            memset(o,0,sizeof(*o));
        }
    }
}

/* Treasure vtable +0x60 (0x185000): the opener's action-finish callback,
 * reached from the opener's own actor update (0xDBDC0 -> current usable),
 * outside any update scope. It advances the state and calls 0xB5E60, which
 * writes the next action into the FRONT's arbiter; without the scope that
 * landed on the host's character and the opener replayed its use animation
 * (live 2026-10-16: 2, 82, 2). thiscall, one stack argument, RET 4, and
 * 0xB5E60 takes its action id in the caller's ESI: the wrapper keeps ESI. */
int __cdecl remote_use_finish_enter(uint8_t *treasure) __attribute__((used));
void __cdecl remote_use_finish_leave(void) __attribute__((used));
int __cdecl remote_use_finish_enter(uint8_t *treasure) {
    Open *o=GetCurrentThreadId()==game_thread?open_of(treasure):NULL;
    return o && scope_begin(o->actor);
}
void __cdecl remote_use_finish_leave(void) { scope_end(); }
__attribute__((naked)) static void finish_wrapper(void) {
    __asm__ volatile(
        "push %ecx\n\t"
        "push %edx\n\t"
        "push %ecx\n\t"
        "call _remote_use_finish_enter\n\t"
        "add $4,%esp\n\t"
        "pop %edx\n\t"
        "pop %ecx\n\t"
        "test %eax,%eax\n\t"
        "jz 1f\n\t"
        "push 4(%esp)\n\t"
        "call *_remote_use_finish_original\n\t"
        "push %eax\n\t"
        "call _remote_use_finish_leave\n\t"
        "pop %eax\n\t"
        "ret $4\n"
        "1:\n\t"
        "jmp *_remote_use_finish_original");
}
static BOOL bytes(uint32_t rva,const void *expected,size_t n) {
    return readable(base+rva,n) && !memcmp(base+rva,expected,n);
}
/* Each front site loads the party group pointer within the preceding 40
 * bytes (absolute, relocated; 0xB6321 is 27 bytes after its load) and calls
 * the handle copy. */
enum { SITE_WINDOW=40u };
static BOOL front_site_exact(uint32_t rva) {
    uint32_t group_va=(uint32_t)(uintptr_t)(base+RVA_GROUP);
    if(!readable(base+rva-SITE_WINDOW,SITE_WINDOW+5u) || base[rva]!=0xe8u) return FALSE;
    for(unsigned k=0;k+4u<=SITE_WINDOW;++k) if(!memcmp(base+rva-SITE_WINDOW+k,&group_va,4u)) return TRUE;
    return FALSE;
}
BOOL SudekiMpLanStoryRemoteUseInstall(HMODULE image) {
    static const uint8_t copy_entry[]={0x8b,0x11,0x8b,0xca,0x89,0x10};
    static const uint8_t listener_entry[]={0x83,0xec,0x0c,0x56,0x8b,0xf1,0x8b,0x46,0xd4,0x3b,0x44,0x24,0x14};
    static const uint8_t use_entry[]={0x83,0xec,0x18,0x56,0x8b,0xf1,0x83,0x7e,0x74,0x00};
    static const uint8_t update_entry[]={0x55,0x8b,0xec,0x83,0xe4,0xf8,0x81,0xec,0x44,0x01,0,0};
    static const uint8_t play_entry[]={0x83,0xec,0x0c,0x8b,0x0d};
    static const uint8_t finish_entry[]={0x8b,0x41,0x74,0x83,0xf8,0x04,0x7c,0x41,0x83,0xf8,0x07};
    static const uint8_t release_entry[]={0x85,0xc0,0x74,0x05,0x8d,0x48,0x04,0xeb,0x02,0x33,0xc9,0xa1};
    if(installed) return TRUE;
    base=(uint8_t *)image; game_thread=GetCurrentThreadId();
    if(!bytes(RVA_HANDLE_COPY,copy_entry,sizeof(copy_entry)) ||
        !bytes(RVA_LISTENER,listener_entry,sizeof(listener_entry)) ||
        !bytes(RVA_TREASURE_USE,use_entry,sizeof(use_entry)) ||
        !bytes(RVA_TREASURE_UPDATE,update_entry,sizeof(update_entry)) ||
        !bytes(RVA_PLAY_USE,play_entry,sizeof(play_entry)) ||
        !bytes(RVA_TREASURE_FINISH,finish_entry,sizeof(finish_entry)) ||
        !readable(base+RVA_FINISH_SLOT,4u) || *(void **)(base+RVA_FINISH_SLOT)!=base+RVA_TREASURE_FINISH ||
        !bytes(RVA_USABLE_RELEASE,release_entry,sizeof(release_entry)) ||
        !readable(base+RVA_TREASURE_USE_SLOT,4u) || *(void **)(base+RVA_TREASURE_USE_SLOT)!=base+RVA_TREASURE_USE ||
        !readable(base+RVA_NODE_UPDATE_SLOT,4u) || *(void **)(base+RVA_NODE_UPDATE_SLOT)!=base+RVA_TREASURE_UPDATE) {
        SudekiMpLogWrite("story_remote_use event=install_refused reason=image\r\n");
        base=NULL; SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    for(unsigned i=0;i<FRONT_SITES;++i) if(!front_site_exact(front_sites[i])) {
        SudekiMpLogFormat("story_remote_use event=install_refused reason=front_site rva=%06lx\r\n",(unsigned long)front_sites[i]);
        base=NULL; SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    remote_use_copy_target=base+RVA_HANDLE_COPY; remote_use_controller_target=base+RVA_CONTROLLER_SET;
    update_original=(NodeUpdate)(base+RVA_TREASURE_UPDATE);
    BOOL ok=TRUE;
    for(unsigned i=0;ok && i<FRONT_SITES;++i)
        ok=SudekiMpInstallRelativeCallHook(&front_hooks[i],base+front_sites[i],base+RVA_HANDLE_COPY,(const void *)front_copy_stub);
    for(unsigned i=0;ok && i<CONTROLLER_SITES;++i)
        ok=SudekiMpInstallRelativeCallHook(&controller_hooks[i],base+controller_sites[i],base+RVA_CONTROLLER_SET,(const void *)controller_stub);
    remote_use_finish_original=base+RVA_TREASURE_FINISH;
    if(ok) ok=SudekiMpInstallPointerHook(&update_hook,(void **)(base+RVA_NODE_UPDATE_SLOT),base+RVA_TREASURE_UPDATE,(const void *)update_wrapper);
    if(ok) ok=SudekiMpInstallPointerHook(&finish_hook,(void **)(base+RVA_FINISH_SLOT),base+RVA_TREASURE_FINISH,(const void *)finish_wrapper);
    if(!ok) {
        DWORD error=GetLastError(); installed=TRUE; (void)SudekiMpLanStoryRemoteUseUninstall();
        SudekiMpLogFormat("story_remote_use event=install_refused reason=hook error=%lu\r\n",(unsigned long)error);
        SetLastError(error); return FALSE;
    }
    installed=TRUE;
    SudekiMpLogFormat("story_remote_use event=installed front_sites=%u controller_sites=%u\r\n",(unsigned)FRONT_SITES,(unsigned)CONTROLLER_SITES);
    return TRUE;
}
BOOL SudekiMpLanStoryRemoteUseUninstall(void) {
    if(!installed) return TRUE;
    if(remote_use_scope_thread) { SetLastError(ERROR_BUSY); return FALSE; }
    BOOL ok=TRUE;
    if(finish_hook.installed && !SudekiMpRestorePointerHook(&finish_hook)) ok=FALSE;
    if(update_hook.installed && !SudekiMpRestorePointerHook(&update_hook)) ok=FALSE;
    for(unsigned i=CONTROLLER_SITES;i-- > 0;) if(controller_hooks[i].installed && !SudekiMpRestoreRelativeCallHook(&controller_hooks[i])) ok=FALSE;
    for(unsigned i=FRONT_SITES;i-- > 0;) if(front_hooks[i].installed && !SudekiMpRestoreRelativeCallHook(&front_hooks[i])) ok=FALSE;
    if(!ok) { SetLastError(ERROR_BUSY); return FALSE; } /* stubs stay; module retained */
    memset(opens,0,sizeof(opens)); installed=FALSE; base=NULL; return TRUE;
}

/* Client: the native interact icon is CUIScene context-menu flag 0x200 on
 * the UI scene *(0x408D1C) (set RVA 0xB210, clear RVA 0xB230: thiscall,
 * one flag argument). The paused client world never runs the treasure idle
 * update that sets it, so the host's use hint drives it. */
enum { RVA_UI_SCENE=0x408d1cu, RVA_FLAGS_SET=0xb210u, RVA_FLAGS_CLEAR=0xb230u };
static uint8_t *icon_base; static uint32_t icon_shown;
BOOL SudekiMpLanStoryRemoteUseIconInitialize(HMODULE image) {
    static const uint8_t set_entry[]={0x8b,0x81,0x74,0x01,0,0,0x8b,0x40,0x60,0x8b,0x4c,0x24,0x04,0x09,0x48,0x58};
    static const uint8_t clear_entry[]={0x8b,0x81,0x74,0x01,0,0,0x85,0xc0,0x74,0x15,0x8b,0x4c,0x24,0x04};
    uint8_t *b=(uint8_t *)image;
    if(!readable(b+RVA_FLAGS_SET,sizeof(set_entry)) || memcmp(b+RVA_FLAGS_SET,set_entry,sizeof(set_entry)) ||
        !readable(b+RVA_FLAGS_CLEAR,sizeof(clear_entry)) || memcmp(b+RVA_FLAGS_CLEAR,clear_entry,sizeof(clear_entry))) return FALSE;
    icon_base=b; icon_shown=0u; return TRUE;
}
/* The client's UI clears these flags; natively the usable in front sets its
 * flag every frame (CONFIRMED_LIVE 2026-10-16: our one-time set read back as
 * cleared, the prompt "shows up then vanishes"). Re-assert while wanted. */
static uint32_t icon_host,icon_local;
static void icon_refresh(void);
void SudekiMpLanStoryRemoteUseIconApply(unsigned hint) {
    icon_host=hint==1u?4u:hint==2u?0x200u:0u;
    icon_refresh();
}
/* H1b: the client's own shopkeeper talk prompt (flag 2) shares this one
 * owner of the flag word with the host's chest hint. */
BOOL SudekiMpLanStoryRemoteUseIconSetLocal(uint32_t bits) {
    icon_local=bits&(2u|0x40u); /* talk, examine */
    icon_refresh();
    return icon_base!=NULL;
}
static void icon_refresh(void) {
    uint32_t want=icon_host|icon_local;
    if(!icon_base || (!want && !icon_shown)) return;
    uint8_t *scene=readable(icon_base+RVA_UI_SCENE,4u)?*(uint8_t **)(icon_base+RVA_UI_SCENE):NULL;
    uint8_t *hud=readable(scene,0x178u)?*(uint8_t **)(scene+0x174u):NULL;
    uint8_t *ctx=readable(hud,0x64u)?*(uint8_t **)(hud+0x60u):NULL;
    if(!readable(ctx,0x5cu)) return;
    typedef void (__attribute__((thiscall)) *Flags)(void *,uint32_t);
    uint32_t flags=*(uint32_t *)(ctx+0x58u);
    uint32_t drop=icon_shown&~want&flags,need=want&~flags;
    if(drop) ((Flags)(icon_base+RVA_FLAGS_CLEAR))(scene,drop);
    if(need) ((Flags)(icon_base+RVA_FLAGS_SET))(scene,need);
    if(want!=icon_shown) log_event("story_remote_use event=%s shown=%u flag=%u\r\n","icon",want?1u:0u,want);
    icon_shown=want;
}
/* Entities outside the host leader's active area are paused (entity+0x2B)
 * and their treasure node is not updated (CONFIRMED_LIVE 2026-10-16: zero
 * updates in 4 s while the host was in another area; the open stayed in
 * state 1 and held the opener). A remote opener's chest is stepped here when
 * the native update skipped it, through the same wrapper (scope, watchdog,
 * finish). The node update ignores its stack argument (RET 4). */
void SudekiMpLanStoryRemoteUseService(void) {
    if(!installed || GetCurrentThreadId()!=game_thread || remote_use_scope_thread) return;
    DWORD now=GetTickCount();
    for(unsigned i=0;i<OPENS;++i) {
        Open *o=&opens[i];
        if(!o->treasure) continue;
        uint8_t *chest=*(uint8_t **)(o->treasure+0x10u);
        if(!in_registry(chest) || treasure_of(chest)!=o->treasure) {
            log_event("story_remote_use event=%s treasure=%u reason=%u\r\n","released",(unsigned)(uintptr_t)o->treasure,2u);
            memset(o,0,sizeof(*o)); continue;
        }
        /* An active chest may update less often than every frame; extra
         * steps between native updates replayed the use animation once more
         * (live 2026-10-16). Step only a paused chest, or one the native
         * update has left alone for half a second. */
        if(!chest[0x2bu] && now-o->last_update<500u) continue;
        static unsigned stepped_logs;
        if(stepped_logs<4u) { ++stepped_logs;
            log_event("story_remote_use event=%s treasure=%u paused=%u\r\n","stepped",(unsigned)(uintptr_t)o->treasure,(unsigned)chest[0x2bu]); }
        update_wrapper(o->treasure+NODE_OFFSET,NULL);
    }
}
BOOL SudekiMpLanStoryRemoteUseActive(const void *actor) {
    for(unsigned i=0;i<OPENS;++i) if(opens[i].treasure && opens[i].actor==actor) return TRUE;
    return FALSE;
}
static BOOL position_of(const uint8_t *e,float out[3]) {
    const uint8_t *p=readable(e,0x48u)?*(const uint8_t *const *)(e+0x44u):NULL;
    if(!readable(p,0xb8u) || *(void *const *)p!=base+RVA_POSITION_VT || *(const void *const *)(p+0x10u)!=e) return FALSE;
    memcpy(out,p+0x18u,12u);
    return isfinite(out[0]) && isfinite(out[1]) && isfinite(out[2]);
}
/* Invisible chests stay unusable until revealed: require full render alpha
 * (wrapper +8 object, colour +0x10, alpha byte +0x13), which ordinary chests
 * always have. */
static BOOL chest_visible(const uint8_t *chest) {
    const uint8_t *p=*(const uint8_t *const *)(chest+0x44u);
    const uint8_t *w=readable(p,0xb8u)?*(const uint8_t *const *)(p+0xb4u):NULL;
    const uint8_t *o=readable(w,0x0cu)?*(const uint8_t *const *)(w+8u):NULL;
    return readable(o,0x14u) && o[0x13u]==0xffu;
}
/* Nearest unopened, visible chest within 3 units (planar) and 2 units of
 * height of `at`; optionally also within about 70 degrees of `forward`.
 * Natively the range is the chest's trigger volume (front only) and 0xB60A0
 * tests facing against the chest's front; this approximates it for the
 * prompt, while the press still runs the native facing test. */
static uint8_t *nearest_chest(const float at[3],const float *forward,uint8_t **treasure_out) {
    const uint8_t *owner=readable(base+RVA_REGISTRY,4u)?*(const uint8_t *const *)(base+RVA_REGISTRY):NULL;
    if(!readable(owner,0x40u)) return NULL;
    unsigned n=*(const uint32_t *)(owner+0x34u);
    uint8_t *const *entries=*(uint8_t *const *const *)(owner+0x3cu);
    if(!n || n>8192u || !readable(entries,n*sizeof(void *))) return NULL;
    uint8_t *best=NULL; float best_d=9.0f;
    for(unsigned i=0;i<n;++i) {
        uint8_t *chest=entries[i],*t=treasure_of(chest); float c[3];
        if(!t || *(uint32_t *)(t+STATE_OFFSET)!=0u || open_of(t) || !position_of(chest,c) || !chest_visible(chest)) continue;
        float dx=c[0]-at[0],dy=c[1]-at[1],dz=c[2]-at[2],d=dx*dx+dz*dz;
        if(fabsf(dy)>2.0f || d>=best_d) continue;
        if(forward) {
            float len=sqrtf(d),fl=sqrtf(forward[0]*forward[0]+forward[2]*forward[2]);
            if(len>0.3f && fl>0.001f && (dx*forward[0]+dz*forward[2])/(len*fl)<0.34f) continue;
        }
        best_d=d; best=chest; if(treasure_out) *treasure_out=t;
    }
    return best;
}
/* The native idle update (0x4B6850 from treasure case 0) shows context-menu
 * flag 4 when treasure+0x73 bit 1 is set, otherwise 0x200 (CONFIRMED_LIVE
 * 2026-10-16: the host showed 0x84 with Ailish at E3_NB_ShdCamp; the hand
 * "open" prompt). The hint names the native choice: 1 = flag 4, 2 = 0x200. */
unsigned SudekiMpLanStoryRemoteUseHint(const void *actor) {
    if(!installed || GetCurrentThreadId()!=game_thread || !actor || SudekiMpLanStoryRemoteUseActive(actor)) return 0u;
    float at[3]; uint8_t *g=group(); const uint8_t *a=arbiter_of(actor);
    if(!g || !slot_of(g,actor) || !a || (*(const uint32_t *)(a+0x50u)&0x280u) || !position_of(actor,at)) return 0u;
    const uint8_t *p=*(const uint8_t *const *)((const uint8_t *)actor+0x44u);
    float forward[3]; memcpy(forward,p+0x50u,12u);
    if(!isfinite(forward[0]) || !isfinite(forward[2])) return 0u;
    uint8_t *treasure=NULL;
    if(!nearest_chest(at,forward,&treasure) || !treasure) return 0u;
    return (treasure[0x73u]&2u)?1u:2u;
}
typedef void (__attribute__((thiscall)) *Listener)(void *,void *,void *,int);
BOOL SudekiMpLanStoryRemoteUseTry(void *actor) {
    if(!installed || GetCurrentThreadId()!=game_thread || !actor || remote_use_scope_thread) return FALSE;
    float at[3]; if(!in_registry(actor) || !position_of(actor,at)) return FALSE;
    uint8_t *g=group(); if(!g || !slot_of(g,actor)) return FALSE;
    for(unsigned i=0;i<OPENS;++i) if(opens[i].treasure && opens[i].actor==actor) return FALSE;
    uint8_t *best_treasure=NULL,*best=nearest_chest(at,NULL,&best_treasure);
    if(!best) return FALSE;
    Open *slot=NULL;
    for(unsigned i=0;i<OPENS && !slot;++i) if(!opens[i].treasure) slot=&opens[i];
    uint8_t *arbiter=arbiter_of(actor);
    if(!slot || !arbiter || !scope_begin(actor)) return FALSE;
    uint32_t action58=(*(uint32_t *)(arbiter+0x58u)>>8)&0x1ffu,action5c=*(uint32_t *)(arbiter+0x5cu)&0x1ffu;
    /* Native OnAction handling for this chest's usable listener: owner check,
     * actor == front (now the opener), kind 2, arbiter checks, can-use, use. */
    ((Listener)(base+RVA_LISTENER))(best_treasure+LISTENER_OFFSET,best,actor,2);
    scope_end();
    uint32_t state=*(uint32_t *)(best_treasure+STATE_OFFSET);
    log_event("story_remote_use event=%s treasure=%u state=%u\r\n",state?"started":"refused",
        (unsigned)(uintptr_t)best_treasure,state);
    if(!state) return FALSE;
    slot->treasure=best_treasure; slot->actor=actor; slot->started=GetTickCount();
    slot->action58=action58; slot->action5c=action5c; slot->last_update=GetTickCount();
    return TRUE;
}
