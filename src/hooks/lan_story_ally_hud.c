#include "hooks/lan_story_ally_hud.h"
#include "hooks/lan_story_ally_seat.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <stdint.h>
#include <string.h>

/* Native seams (supported image RVAs).
 * 0x4d14d0 FUN_004d14d0 (EAX = CComboManager): an input was accepted into the
 *   chain. Reads manager+0x64 (index), +0x68[index] (1 weak / 2 strong) and
 *   +0xa0[index] (timing byte) into a slot kind 0..3, then, only when the
 *   manager's owner (+0x10) is the controlled hero, calls 0x4af0d0 (index 0,
 *   re-arm) and 0x4af060 (push into the first empty HUD slot).
 * 0x4d0440 FUN_004d0440 (EAX = manager): chain reset; for the controlled hero
 *   it empties the slots and clears +0x200..+0x203 without re-arming.
 * 0x4d0a10 FUN_004d0a10 (ECX = manager+0x3c, stack byte): combo complete; it
 *   stores the byte at HUD +0x202 (the flash) and refreshes the slots.
 * 0x4ae430 FUN_004ae430 (stdcall, HUD controller): recomputes the HUD mode
 *   from the controlled hero's arbiter and transitions 0x4aed20 (exit, EAX =
 *   old mode, ECX = HUD) -> 0x4ae9d0 (enter, EAX = new mode, ESI = HUD), then
 *   0x4ae7c0 (EAX = HUD, per-mode elements). Mode 3 = melee combat: shows
 *   COMBO_GIZMO/COMBO_SLOTS. 0x4af110 (EAX = HUD) maps the slot words
 *   +0x1f4/+0x1f8/+0x1fc (7 = empty) and +0x201/+0x202 onto COMBO_BUTTON1..3. */
enum { RVA_ACCEPT=0xd14d0u, RVA_RESET=0xd0440u, RVA_COMPLETE=0xd0a10u, RVA_HIT=0xd0a80u, SEAM_LENGTH=7u,
    RVA_HUD_UPDATE=0xae430u, UPDATE_LENGTH=12u, RVA_HUD_GLOBAL=0x3c2fc4u, RVA_ICON_GLOBAL=0x3c3038u,
    RVA_MODE_EXIT=0xaed20u, RVA_MODE_ENTER=0xae9d0u, RVA_ELEMENTS=0xae7c0u, RVA_SLOTS=0xaf110u,
    HUD_MODE_MELEE=3u, SLOT_EMPTY=SUDEKIMP_STORY_ALLY_HUD_SLOT_EMPTY };
static const uint8_t accept_bytes[SEAM_LENGTH]={0x83,0xec,0x10,0x53,0x56,0x8b,0xf0};
static const uint8_t reset_bytes[SEAM_LENGTH]={0x83,0xec,0x14,0x53,0x56,0x8b,0xf0};
static const uint8_t complete_bytes[SEAM_LENGTH]={0x56,0x8b,0xf1,0x80,0x4e,0x75,0x04};
/* 0x4d0a80 FUN_004d0a80 (ECX = manager+0x3c, listener vtable slot +0x10): sets
 * manager state bit 8 ("hit/window seen"); without it the next accepted input
 * is marked as a miss (0x4d0640). Research observation only: owner and caller. */
static const uint8_t hit_bytes[SEAM_LENGTH]={0x51,0x56,0x8b,0xf1,0x8b,0x4e,0xd4};
/* SUB ESP,0x10; PUSH EBX; PUSH EBP; PUSH ESI; MOV ESI,[abs]; the operand is relocated. */
static const uint8_t update_head[8]={0x83,0xec,0x10,0x53,0x55,0x56,0x8b,0x35};
enum { UPDATE_OPERAND_RVA=0x408d94u };

static uint8_t *base; static BOOL host_mode,client_mode,avatar_mode;
static DWORD avatar_thread;
static BOOL avatar_validating;
static SudekiMpInlineHook accept_hook,reset_hook,complete_hook,hit_hook,update_hook;
void *SudekiMpLanStoryAllyHudAcceptTrampoline __attribute__((used));
void *SudekiMpLanStoryAllyHudResetTrampoline __attribute__((used));
void *SudekiMpLanStoryAllyHudCompleteTrampoline __attribute__((used));
void *SudekiMpLanStoryAllyHudHitTrampoline __attribute__((used));
void *SudekiMpLanStoryAllyHudUpdateTrampoline __attribute__((used));
static unsigned logs;

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && VirtualQuery(p,&m,sizeof(m))==sizeof(m) && m.State==MEM_COMMIT &&
        !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) && a+n>=a && a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL take_log(void) { return SudekiMpLogResearchEnabled() && logs<400u && (++logs,TRUE); }

/* ---- host: shadow of the slots the native HUD would show for the ally ---- */
typedef struct ComboShadow { uint8_t slots[3],full,alt,flash,armed; uint32_t sequence; } ComboShadow;
static ComboShadow shadow;
typedef struct AvatarShadow {
    void *entity; uint32_t generation;
    SudekiMpLanStoryAllyHudAvatarExact exact; void *context;
    ComboShadow shadow;
} AvatarShadow;
static AvatarShadow avatars[4];
static void *tracked;

static void clear_shadow(ComboShadow *s) {
    memset(s,0,sizeof(*s)); s->slots[0]=s->slots[1]=s->slots[2]=SLOT_EMPTY; s->sequence=1;
}
static void shadow_clear(void) { clear_shadow(&shadow); }
static void clear_avatar(unsigned player) {
    memset(&avatars[player],0,sizeof(avatars[player])); clear_shadow(&avatars[player].shadow);
}
BOOL SudekiMpLanStoryAllyHudConfigureAvatars(BOOL enabled) {
    if(base || avatar_validating) { SetLastError(ERROR_BUSY); return FALSE; }
    avatar_mode=enabled; return TRUE;
}
static BOOL avatar_exact(unsigned player) {
    AvatarShadow *a=&avatars[player];
    if(!avatar_mode || !a->entity || !a->generation || !a->exact || avatar_validating ||
        avatar_thread!=GetCurrentThreadId()) return FALSE;
    avatar_validating=TRUE;
    BOOL exact=a->exact(player,a->entity,a->generation,a->context);
    avatar_validating=FALSE;
    if(!exact) clear_avatar(player);
    return exact;
}
BOOL SudekiMpLanStoryAllyHudTrackAvatar(unsigned player,void *entity,uint32_t generation,
    SudekiMpLanStoryAllyHudAvatarExact exact,void *context) {
    if(!avatar_mode || !host_mode || !base || player>=4u || avatar_validating ||
        (avatar_thread && avatar_thread!=GetCurrentThreadId())) return FALSE;
    if(!entity) { clear_avatar(player); return TRUE; }
    if(!generation || !exact) return FALSE;
    for(unsigned n=0;n<4u;++n) if(n!=player && avatars[n].entity==entity) return FALSE;
    AvatarShadow *a=&avatars[player];
    if(a->entity!=entity || a->generation!=generation || a->exact!=exact || a->context!=context) {
        clear_avatar(player); a->entity=entity; a->generation=generation; a->exact=exact; a->context=context;
    }
    avatar_thread=GetCurrentThreadId();
    return avatar_exact(player);
}
static ComboShadow *event_shadow(const uint8_t *manager,unsigned source) {
    if(!source) return tracked && readable(manager,0xb4u) &&
        *(void *const *)(manager+0x10u)==tracked?&shadow:NULL;
    unsigned player=source-1u;
    if(player>=4u || !readable(manager,0xb4u) ||
        *(void *const *)(manager+0x10u)!=avatars[player].entity || !avatar_exact(player)) return NULL;
    const uint8_t *entity=avatars[player].entity;
    if(!readable(entity,0xbcu) || *(void *const *)(entity+0xb8u)!=manager ||
        *(void *const *)manager!=base+0x2d4bd4u) {
        clear_avatar(player); return NULL;
    }
    return &avatars[player].shadow;
}
void SudekiMpLanStoryAllyHudTrack(void *entity) {
    if(entity==tracked) return;
    tracked=entity; shadow_clear();
    if(take_log()) SudekiMpLogFormat("ally_hud event=track entity=%p\r\n",entity);
}
static BOOL owned_by_ally(const uint8_t *manager) {
    return tracked && readable(manager,0xb4u) && *(void *const *)(manager+0x10u)==tracked;
}
static void __attribute__((used,noinline)) observe_accept(uintptr_t manager) {
    DWORD saved=GetLastError();
    const uint8_t *m=(const uint8_t *)manager;
    for(unsigned source=0;source<5u;++source) {
        ComboShadow *current=event_shadow(m,source);
        if(!current) continue;
        int32_t index=*(const int32_t *)(m+0x64u);
        if(index>=0 && index<6) {
            int32_t kind=*(const int32_t *)(m+0x68u+(unsigned)index*4u);
            uint8_t timed=m[0xa0u+(unsigned)index];
            unsigned slot=SLOT_EMPTY;
            if(kind==1) slot=timed?0u:2u; else if(kind==2) slot=timed?1u:3u;
            if(slot!=SLOT_EMPTY) {
                if(index==0) { /* 0x4af0d0: empty, clear, re-arm */
                    current->full=current->alt=0; current->slots[0]=current->slots[1]=current->slots[2]=SLOT_EMPTY; current->armed=1;
                }
                if(current->armed) { /* 0x4af060 */
                    unsigned i=0; while(i<3u && current->slots[i]!=SLOT_EMPTY) ++i;
                    if(i<3u) {
                        current->slots[i]=(uint8_t)slot;
                        if(i==2u && current->slots[2]<1u) { current->full=1; current->alt=1; }
                        current->flash=0;
                    }
                }
                if(!++current->sequence) current->sequence=1;
                if(take_log()) SudekiMpLogFormat("ally_hud event=accept index=%ld kind=%ld timed=%u slot=%u slots=%u,%u,%u seq=%lu\r\n",
                    (long)index,(long)kind,timed,slot,current->slots[0],current->slots[1],current->slots[2],(unsigned long)current->sequence);
            }
        }
    }
    SetLastError(saved);
}
static void __attribute__((used,noinline)) observe_reset(uintptr_t manager) {
    DWORD saved=GetLastError();
    for(unsigned source=0;source<5u;++source) {
        ComboShadow *current=event_shadow((const uint8_t *)manager,source);
        if(!current) continue;
        current->full=current->alt=0; current->slots[0]=current->slots[1]=current->slots[2]=SLOT_EMPTY; current->armed=0;
        if(!++current->sequence) current->sequence=1;
        if(take_log()) SudekiMpLogFormat("ally_hud event=reset flash=%u seq=%lu\r\n",current->flash,(unsigned long)current->sequence);
    }
    SetLastError(saved);
}
static void __attribute__((used,noinline)) observe_complete(uintptr_t self,uint32_t flash) {
    DWORD saved=GetLastError();
    for(unsigned source=0;self>=0x3cu && source<5u;++source) {
        ComboShadow *current=event_shadow((const uint8_t *)(self-0x3cu),source);
        if(!current) continue;
        current->flash=(uint8_t)(flash&0xffu); if(!++current->sequence) current->sequence=1;
        if(take_log()) SudekiMpLogFormat("ally_hud event=complete flash=%lu slots=%u,%u,%u seq=%lu\r\n",
            (unsigned long)(flash&0xffu),current->slots[0],current->slots[1],current->slots[2],(unsigned long)current->sequence);
    }
    SetLastError(saved);
}
static unsigned hit_logs;
static void __attribute__((used,noinline)) observe_hit(uintptr_t self,uintptr_t ret) {
    DWORD saved=GetLastError();
    if(self>=0x3cu && SudekiMpLogResearchEnabled() && hit_logs<120u) {
        const uint8_t *m=(const uint8_t *)(self-0x3cu);
        BOOL ally=owned_by_ally(m);
        if(ally || hit_logs<40u) { ++hit_logs;
            SudekiMpLogFormat("ally_hud event=hit ally=%u owner=%p state=%02x index=%ld caller=0x%06lx\r\n",
                ally,readable(m,0xb4u)?*(void *const *)(m+0x10u):NULL,readable(m,0xb4u)?m[0xb1u]:0u,
                readable(m,0xb4u)?(long)*(const int32_t *)(m+0x64u):-1L,
                (unsigned long)(ret>=(uintptr_t)base?ret-(uintptr_t)base:ret)); }
    }
    SetLastError(saved);
}
static BOOL snapshot_shadow(const void *entity,const SudekiMpLanStoryControlFence *fence,
    const ComboShadow *s,SudekiMpLanStoryAllyHud *hud) {
    const uint8_t *e=(const uint8_t *)entity;
    if(!readable(e,0x94u)) return FALSE;
    const uint8_t *arbiter=*(const uint8_t *const *)(e+0x90u);
    if(!readable(arbiter,0x64u)) return FALSE;
    uint32_t f50=*(const uint32_t *)(arbiter+0x50u),f58=*(const uint32_t *)(arbiter+0x58u),
        f60=*(const uint32_t *)(arbiter+0x60u);
    /* 0x4ae430: armed, sub-state not 3, combat engaged -> HUD flag bit 0 -> melee mode. */
    BOOL combat=(f50&2u) && (f58&0xfu)!=3u && (f60&2u);
    memset(hud,0,sizeof(*hud));
    hud->fence=*fence; hud->sequence=s->sequence; hud->observed_tick=GetTickCount();
    memcpy(hud->slots,s->slots,3u);
    hud->flags=(uint8_t)((s->full?SUDEKIMP_STORY_ALLY_HUD_FULL:0u)|(s->alt?SUDEKIMP_STORY_ALLY_HUD_ALT_ICONS:0u)|
        (s->flash?SUDEKIMP_STORY_ALLY_HUD_FLASH:0u)|(s->armed?SUDEKIMP_STORY_ALLY_HUD_ARMED:0u)|
        (combat?SUDEKIMP_STORY_ALLY_HUD_COMBAT:0u));
    return TRUE;
}

BOOL SudekiMpLanStoryAllyHudSnapshot(const void *entity,const SudekiMpLanStoryControlFence *fence,
    SudekiMpLanStoryAllyHud *hud) {
    if(!host_mode || !entity || entity!=tracked || !fence || !hud) return FALSE;
    return snapshot_shadow(entity,fence,&shadow,hud);
}
BOOL SudekiMpLanStoryAllyHudSnapshotAvatar(unsigned player,const void *entity,uint32_t generation,
    const SudekiMpLanStoryControlFence *fence,SudekiMpLanStoryAllyHud *hud) {
    if(hud) memset(hud,0,sizeof(*hud));
    if(!host_mode || player>=4u || !hud || !fence || !avatar_exact(player)) return FALSE;
    const AvatarShadow *a=&avatars[player];
    if(entity!=a->entity || generation!=a->generation || !fence->epoch || !fence->revision ||
        !fence->transaction || fence->actor_generation!=generation || fence->player!=player ||
        fence->character!=SUDEKIMP_STORY_CHARACTER_ALLY) return FALSE;
    const uint8_t *e=entity;
    if(!readable(e,0xbcu)) { clear_avatar(player); return FALSE; }
    const uint8_t *arbiter=*(const uint8_t *const *)(e+0x90u),*combo=*(const uint8_t *const *)(e+0xb8u);
    if(!readable(arbiter,0x64u) || *(const void *const *)arbiter!=base+0x2cc9acu ||
        *(const void *const *)(arbiter+0x10u)!=entity ||
        !readable(combo,0xb4u) || *(const void *const *)combo!=base+0x2d4bd4u ||
        *(const void *const *)(combo+0x10u)!=entity) {
        clear_avatar(player); return FALSE;
    }
    return snapshot_shadow(entity,fence,&a->shadow,hud);
}

/* Stubs preserve every register and the FP/SSE state. After pushfl/pushal with
 * EBP=ESP: [EBP+28]=EAX, [EBP+24]=ECX, [EBP+36]=return address, [EBP+40]=arg1. */
static void __attribute__((naked,noinline)) accept_stub(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;"
        "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld; sub $16,%esp;"
        "mov 28(%ebp),%eax; mov %eax,(%esp); call _observe_accept;"
        "add $16,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl;"
        "jmp *_SudekiMpLanStoryAllyHudAcceptTrampoline");
}
static void __attribute__((naked,noinline)) reset_stub(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;"
        "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld; sub $16,%esp;"
        "mov 28(%ebp),%eax; mov %eax,(%esp); call _observe_reset;"
        "add $16,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl;"
        "jmp *_SudekiMpLanStoryAllyHudResetTrampoline");
}
static void __attribute__((naked,noinline)) complete_stub(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;"
        "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld; sub $16,%esp;"
        "mov 24(%ebp),%eax; mov %eax,(%esp); mov 40(%ebp),%eax; mov %eax,4(%esp); call _observe_complete;"
        "add $16,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl;"
        "jmp *_SudekiMpLanStoryAllyHudCompleteTrampoline");
}

static void __attribute__((naked,noinline)) hit_stub(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;"
        "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld; sub $16,%esp;"
        "mov 24(%ebp),%eax; mov %eax,(%esp); mov 36(%ebp),%eax; mov %eax,4(%esp); call _observe_hit;"
        "add $16,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl;"
        "jmp *_SudekiMpLanStoryAllyHudHitTrampoline");
}

/* ---- client: hold the native HUD in melee mode and write the host's slots ---- */
static SudekiMpLanStoryAllyHud want; static BOOL want_active;
static uint32_t applied_sequence; static uint8_t applied_flags; static BOOL applied_any;

static BOOL hud_chain_readable(const uint8_t *hud) {
    /* 0x4ae9d0 mode 3: scene = hud+0x2c -> +0xbc -> +0x8 (element lookup by name).
     * 0x4af110: icons = [0x7c3038] -> +0x10 -> +0x8 -> +0x10 -> table[0..0x18]. */
    if(!readable(hud,0x210u)) return FALSE;
    const uint8_t *a=*(const uint8_t *const *)(hud+0x2cu); if(!readable(a,0xc0u)) return FALSE;
    const uint8_t *b=*(const uint8_t *const *)(a+0xbcu); if(!readable(b,0xcu)) return FALSE;
    if(!*(const uint32_t *)(b+0x8u)) return FALSE;
    const uint8_t *g=*(const uint8_t *const *)(base+RVA_ICON_GLOBAL); if(!readable(g,0x14u)) return FALSE;
    const uint8_t *c=*(const uint8_t *const *)(g+0x10u); if(!readable(c,0xcu)) return FALSE;
    const uint8_t *d=*(const uint8_t *const *)(c+0x8u); if(!readable(d,0x14u)) return FALSE;
    const uint8_t *t=*(const uint8_t *const *)(d+0x10u); return readable(t,0x1cu);
}
static void call_eax(uint8_t *hud,unsigned rva) {
    void *fn=base+rva;
    __asm__ volatile("call *%[fn]" : "+a"(hud) : [fn]"r"(fn) : "ecx","edx","memory","cc");
}
static void mode_exit(uint8_t *hud,uint32_t old_mode) {
    void *fn=base+RVA_MODE_EXIT;
    __asm__ volatile("call *%%edx" : "+a"(old_mode), "+c"(hud), "+d"(fn) : : "memory","cc");
}
static void mode_enter(uint8_t *hud,uint32_t new_mode) {
    void *fn=base+RVA_MODE_ENTER;
    __asm__ volatile("push %%esi; mov %%ecx,%%esi; call *%%edx; pop %%esi"
        : "+a"(new_mode), "+c"(hud), "+d"(fn) : : "memory","cc");
}
/* Returns 1 when this call replaced the native update (ally melee HUD held). */
static uint32_t __attribute__((used,noinline)) before_update(uintptr_t hud_address) {
    DWORD saved=GetLastError(); uint32_t handled=0;
    uint8_t *hud=(uint8_t *)hud_address;
    if(client_mode && want_active && (want.flags&SUDEKIMP_STORY_ALLY_HUD_COMBAT) && hud_chain_readable(hud)) {
        uint32_t mode=*(uint32_t *)(hud+0x48u); BOOL forced=FALSE;
        if(mode!=HUD_MODE_MELEE) {
            mode_exit(hud,mode); mode_enter(hud,HUD_MODE_MELEE);
            *(uint32_t *)(hud+0x48u)=HUD_MODE_MELEE; forced=TRUE;
        }
        if(forced || !applied_any || want.sequence!=applied_sequence || want.flags!=applied_flags) {
            BOOL rising_full=(want.flags&SUDEKIMP_STORY_ALLY_HUD_FULL) && !hud[0x200u];
            *(uint32_t *)(hud+0x1f4u)=want.slots[0]; *(uint32_t *)(hud+0x1f8u)=want.slots[1]; *(uint32_t *)(hud+0x1fcu)=want.slots[2];
            hud[0x200u]=(want.flags&SUDEKIMP_STORY_ALLY_HUD_FULL)?1u:0u;
            hud[0x201u]=(want.flags&SUDEKIMP_STORY_ALLY_HUD_ALT_ICONS)?1u:0u;
            hud[0x202u]=(want.flags&SUDEKIMP_STORY_ALLY_HUD_FLASH)?1u:0u;
            hud[0x203u]=(want.flags&SUDEKIMP_STORY_ALLY_HUD_ARMED)?1u:0u;
            if(rising_full) *(uint32_t *)(hud+0x1ecu)=*(uint32_t *)(hud+0x1f0u); /* as 0x4af060 */
            call_eax(hud,RVA_ELEMENTS); call_eax(hud,RVA_SLOTS);
            applied_sequence=want.sequence; applied_flags=want.flags; applied_any=TRUE;
            if(take_log()) SudekiMpLogFormat("ally_hud event=applied mode=%lu->3 forced=%u slots=%u,%u,%u flags=%02x seq=%lu\r\n",
                (unsigned long)mode,forced,want.slots[0],want.slots[1],want.slots[2],want.flags,(unsigned long)want.sequence);
        } else call_eax(hud,RVA_ELEMENTS);
        handled=1;
    } else applied_any=FALSE;
    SetLastError(saved); return handled;
}
/* stdcall(HUD): decide first; either replace the update (RET 4) or run the original. */
static void __attribute__((naked,noinline)) update_stub(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;"
        "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld; sub $16,%esp;"
        "mov 40(%ebp),%eax; mov %eax,(%esp); call _before_update; mov %eax,28(%ebp);"
        "add $16,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl;"
        "test %eax,%eax; jnz 1f; jmp *_SudekiMpLanStoryAllyHudUpdateTrampoline; 1: ret $4");
}
void SudekiMpLanStoryAllyHudClientPresent(const SudekiMpLanStoryAllyHud *hud) {
    if(!client_mode || !base) return;
    BOOL changed=FALSE;
    if(hud) {
        BOOL same_fence=want.fence.epoch==hud->fence.epoch && want.fence.revision==hud->fence.revision &&
            want.fence.transaction==hud->fence.transaction && want.fence.actor_generation==hud->fence.actor_generation &&
            want.fence.player==hud->fence.player && want.fence.character==hud->fence.character;
        changed=!want_active || want.sequence!=hud->sequence || want.flags!=hud->flags ||
            memcmp(want.slots,hud->slots,3u)!=0 || (avatar_mode && !same_fence);
        if(avatar_mode && !same_fence) applied_any=FALSE;
        want=*hud; want_active=TRUE;
    } else { changed=want_active; want_active=FALSE; applied_any=FALSE; }
    if(!changed) return;
    uint8_t *controller=*(uint8_t **)(base+RVA_HUD_GLOBAL);
    if(!readable(controller,0x210u)) return;
    /* Through the hooked entry (stdcall, callee cleans): either our
     * replacement holds mode 3, or the native recompute releases it. */
    void *fn=base+RVA_HUD_UPDATE;
    __asm__ volatile("push %[hud]; call *%[fn]" : : [hud]"r"(controller), [fn]"r"(fn) : "eax","ecx","edx","memory","cc");
}

/* ---- install / teardown ---- */
BOOL SudekiMpLanStoryAllyHudInstalled(void) { return base!=NULL; }
BOOL SudekiMpLanStoryAllyHudOwnsUpdateEntry(const uint8_t *target,const uint8_t *expected,size_t length) {
    return base && client_mode && update_hook.installed && update_hook.target==target &&
        length==update_hook.length && expected && !memcmp(update_hook.original,expected,length);
}
BOOL SudekiMpLanStoryAllyHudUninstall(void) {
    BOOL ok=TRUE;
    if(update_hook.target && !SudekiMpRestoreInlineHook(&update_hook)) ok=FALSE;
    if(hit_hook.target && !SudekiMpRestoreInlineHook(&hit_hook)) ok=FALSE;
    if(complete_hook.target && !SudekiMpRestoreInlineHook(&complete_hook)) ok=FALSE;
    if(reset_hook.target && !SudekiMpRestoreInlineHook(&reset_hook)) ok=FALSE;
    if(accept_hook.target && !SudekiMpRestoreInlineHook(&accept_hook)) ok=FALSE;
    /* Trampolines stay referenced: a thread may still be inside them. */
    if(ok) {
        base=NULL; host_mode=client_mode=FALSE; want_active=FALSE; tracked=NULL;
        for(unsigned p=0;p<4u;++p) clear_avatar(p);
        avatar_thread=0;
    }
    return ok;
}
BOOL SudekiMpLanStoryAllyHudInstall(HMODULE game_module) {
    uint8_t *b=(uint8_t *)game_module;
    BOOL host=avatar_mode || SudekiMpLanStoryAllySeatPlayer()!=0u,
        client=avatar_mode || SudekiMpLanStoryAllySeatClientEnabled();
    if(base) { SetLastError(ERROR_INVALID_STATE); return FALSE; }
    if(!host && !client) return TRUE; /* no ally seat configured: nothing to install */
    if(!b || !SudekiMpCheckLoadedExecutable(game_module)) { SetLastError(ERROR_INVALID_STATE); return FALSE; }
    if(host) {
        if(memcmp(b+RVA_ACCEPT,accept_bytes,SEAM_LENGTH) || memcmp(b+RVA_RESET,reset_bytes,SEAM_LENGTH) ||
            memcmp(b+RVA_COMPLETE,complete_bytes,SEAM_LENGTH) || memcmp(b+RVA_HIT,hit_bytes,SEAM_LENGTH)) { SetLastError(ERROR_INVALID_DATA); return FALSE; }
        if(!SudekiMpInstallInlineHook(&accept_hook,b+RVA_ACCEPT,accept_bytes,SEAM_LENGTH,(const void *)(uintptr_t)accept_stub)) return FALSE;
        SudekiMpLanStoryAllyHudAcceptTrampoline=accept_hook.trampoline;
        if(!SudekiMpInstallInlineHook(&reset_hook,b+RVA_RESET,reset_bytes,SEAM_LENGTH,(const void *)(uintptr_t)reset_stub) ||
            (SudekiMpLanStoryAllyHudResetTrampoline=reset_hook.trampoline,
             !SudekiMpInstallInlineHook(&complete_hook,b+RVA_COMPLETE,complete_bytes,SEAM_LENGTH,(const void *)(uintptr_t)complete_stub))) {
            DWORD error=GetLastError(); (void)SudekiMpLanStoryAllyHudUninstall(); SetLastError(error?error:ERROR_INVALID_DATA); return FALSE;
        }
        SudekiMpLanStoryAllyHudCompleteTrampoline=complete_hook.trampoline;
        if(SudekiMpLogResearchEnabled()) { /* research probe: who delivers the hit callback */
            if(!SudekiMpInstallInlineHook(&hit_hook,b+RVA_HIT,hit_bytes,SEAM_LENGTH,(const void *)(uintptr_t)hit_stub)) {
                DWORD error=GetLastError(); (void)SudekiMpLanStoryAllyHudUninstall(); SetLastError(error?error:ERROR_INVALID_DATA); return FALSE;
            }
            SudekiMpLanStoryAllyHudHitTrampoline=hit_hook.trampoline;
        }
        shadow_clear();
    }
    if(client) {
        uint8_t expected[UPDATE_LENGTH]; uintptr_t operand=(uintptr_t)(b+UPDATE_OPERAND_RVA);
        memcpy(expected,update_head,8u); memcpy(expected+8u,&operand,4u);
        if(memcmp(b+RVA_HUD_UPDATE,expected,UPDATE_LENGTH) ||
            !SudekiMpInstallInlineHook(&update_hook,b+RVA_HUD_UPDATE,expected,UPDATE_LENGTH,(const void *)(uintptr_t)update_stub)) {
            DWORD error=GetLastError(); (void)SudekiMpLanStoryAllyHudUninstall(); SetLastError(error?error:ERROR_INVALID_DATA); return FALSE;
        }
        SudekiMpLanStoryAllyHudUpdateTrampoline=update_hook.trampoline;
    }
    base=b; host_mode=host; client_mode=client;
    SudekiMpLogFormat("ally_hud event=installed host_seams=%s client_seam=%s policy=host_observe_only_client_mode3_slot_mirror\r\n",
        host?(hit_hook.installed?"accept:0xd14d0,reset:0xd0440,complete:0xd0a10,hit_probe:0xd0a80":"accept:0xd14d0,reset:0xd0440,complete:0xd0a10"):"-",client?"hud_update:0xae430":"-");
    return TRUE;
}
