#include "hooks/lan_story_control.h"
#include "hooks/lan_party_control.h"
#include "hooks/story_interaction_guard.h"
#include "hooks/call_hook.h"
#include "cleanroom/engine.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include "engine/skill_activation_abi.h"
#include "engine/weapon_activation_abi.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Story actor control requires the supported x86 native ABI"
#endif

enum { PREVIOUS_CALL=0x28464u,NEXT_CALL=0x28480u,
    PREVIOUS=0x23f60u,NEXT=0x24060u };
static uint8_t *base;
static SudekiMpRelativeCallHook switches[2];
static DWORD native_thread;
static BOOL installed;

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || VirtualQuery(p,&m,sizeof(m))!=sizeof(m) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
    DWORD access=m.Protect&0xffu;
    return (access==PAGE_READONLY || access==PAGE_READWRITE || access==PAGE_WRITECOPY ||
        access==PAGE_EXECUTE_READ || access==PAGE_EXECUTE_READWRITE ||
        access==PAGE_EXECUTE_WRITECOPY) && a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL retain(DWORD error) {
    HMODULE self;
    (void)GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        (LPCSTR)&switches,&self);
    SetLastError(error?error:ERROR_BUSY); return FALSE;
}
/* Exact controller update supplies ESI=group, no arguments, and ignores the
 * callee's result. Preserve every register and flag. Only these two input
 * callsites are replaced; scripted/native party selection still has its
 * original entrypoints and must obey the coordinator's lifecycle. */
__attribute__((naked,noinline,used)) static void locked_character(void) {
    __asm__ volatile("ret\n\t");
}
static BOOL hooks_exact(void) {
    static const unsigned calls[2]={PREVIOUS_CALL,NEXT_CALL};
    for(unsigned i=0;i<2u;++i) {
        SudekiMpRelativeCallHook *h=&switches[i];
        if(!h->installed || h->instruction!=base+calls[i] ||
            !readable(h->instruction,5u) || h->instruction[0]!=0xe8 ||
            memcmp(h->instruction+1,&h->replacement_displacement,4u)) return FALSE;
    }
    return TRUE;
}
static BOOL boundary(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *roster) {
    if(!base || !installed || !hooks_exact() || !w || !roster ||
        !w->service_post_original_exact || !w->dispatch_serial ||
        !roster->available_mask || roster->leader_character>=4u ||
        !(roster->available_mask&(1u<<roster->leader_character)) ||
        !SudekiMpLanStoryObserverRosterStillExact(w,roster) ||
        (native_thread && native_thread!=GetCurrentThreadId())) return FALSE;
    native_thread=GetCurrentThreadId(); return TRUE;
}
static BOOL body_idle(const SudekiMpLanPartyLease *key,void *actor,
    const SudekiMpControlUpdateDispatchWitness *w) {
    uint8_t *a=actor,*arbiter,*interaction,*mode; SudekiMpCharacterSkillState skill;
    static const uint32_t actor_vt[4]={0x2d5a88u,0x2d66fcu,0x2d5010u,0x2d555cu};
    BOOL pending=TRUE; int spirit=-1;
    if(!base || !key || key->seat>=4u || !w || !w->service_post_original_exact ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) ||
        !readable(a,0xdcu) || *(void **)a!=base+actor_vt[key->seat] ||
        !readable(arbiter=*(uint8_t **)(a+0x90u),0x64u) ||
        *(void **)arbiter!=base+0x2cc9acu || *(void **)(arbiter+0x10u)!=actor ||
        (*(uint32_t *)(arbiter+0x50u)&0x081802c8u) || (arbiter[0x60u]&5u) ||
        SudekiMpCleanroomEngineRangedCombatPrimePending() ||
        !SudekiMpCleanroomEngineSpiritPresentationState(&spirit) || spirit!=0 ||
        !SudekiMpWeaponActivationPending(actor,&pending) || pending ||
        !SudekiMpObserveCharacterSkill(actor,&skill) || skill.active ||
        !readable(skill.skill,0x78u) || *(void **)(a+0xd8u)!=skill.skill ||
        *(void **)((uint8_t *)skill.skill+0x10u)!=actor ||
        ((uint8_t *)skill.skill)[0x6cu]) return FALSE;
    void *task=*(void **)((uint8_t *)skill.skill+0x74u);
    if(task && (!readable(task,8u) || *(void **)task || !*((uint32_t *)task+1))) return FALSE;
    interaction=*(uint8_t **)(a+0xa8u);
    if(interaction && (!readable(interaction,0x64u) ||
        !readable(mode=*(uint8_t **)(interaction+0x60u),0x4du) || mode[0x4cu])) return FALSE;
    return SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w);
}
static BOOL ordinary_world(BOOL allow_combat) {
    BOOL combat=TRUE; uint8_t *speed;
    return base && readable(base+0x408da0u,4u) &&
        readable(speed=*(uint8_t **)(base+0x408da0u),0x2cu) &&
        !speed[0x28u] && !*(uint16_t *)(speed+0x2au) &&
        !*(int *)(speed+0x20u) && !*(int *)(speed+0x24u) &&
        SudekiMpCleanroomEngineCombatMode(&combat) && (allow_combat || !combat);
}
BOOL SudekiMpLanStoryControlInstall(HMODULE image,unsigned locked_character_id) {
    uint8_t *b=(uint8_t *)image;
    static const uint8_t previous_context[]={0x83,0xbb,0xfc,0,0,0,1,0x75,0x0f,0x8b,0xf7};
    static const uint8_t next_context[]={0x83,0xbb,0xf4,0,0,0,1,0x75,9,0x8b,0xf7};
    static const uint8_t switch_entry[]={0x55,0x8b,0xec,0x83,0xe4,0xf8,0x83,0xec,0x18};
    if(!image || locked_character_id>=4u) { SetLastError(ERROR_NOT_SUPPORTED); return FALSE; }
    if(base || !SudekiMpCheckLoadedExecutable(image) ||
        !SudekiMpStoryInteractionInputImageExact(b,readable) ||
        !readable(b+PREVIOUS_CALL-11u,16u) || !readable(b+NEXT_CALL-11u,16u) ||
        memcmp(b+PREVIOUS_CALL-11u,previous_context,sizeof(previous_context)) ||
        memcmp(b+NEXT_CALL-11u,next_context,sizeof(next_context)) ||
        !readable(b+PREVIOUS,sizeof(switch_entry)) || !readable(b+NEXT,sizeof(switch_entry)) ||
        memcmp(b+PREVIOUS,switch_entry,sizeof(switch_entry)) ||
        memcmp(b+NEXT,switch_entry,sizeof(switch_entry))) {
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    base=b; native_thread=0;
    if(!SudekiMpInstallRelativeCallHook(&switches[0],b+PREVIOUS_CALL,b+PREVIOUS,locked_character) ||
        !SudekiMpInstallRelativeCallHook(&switches[1],b+NEXT_CALL,b+NEXT,locked_character)) {
        DWORD error=GetLastError();
        (void)SudekiMpLanStoryControlUninstall(); SetLastError(error); return FALSE;
    }
    installed=TRUE;
    SudekiMpLogWrite("story_control event=install scope=validated_saved_party locomotion=noncombat native_input_rotation=locked\r\n");
    return TRUE;
}
BOOL SudekiMpLanStoryControlBegin(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *roster) {
    return boundary(w,roster) && SudekiMpLanPartyControlStoryBegin(w,roster);
}
BOOL SudekiMpLanStoryControlNextLease(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *connection,unsigned character,SudekiMpLanPartyLease *key) {
    return base && installed && hooks_exact() && character<4u && native_thread &&
        native_thread==GetCurrentThreadId() &&
        SudekiMpLanPartyControlStoryNextLease(w,connection,character,key);
}
BOOL SudekiMpLanStoryControlAcquire(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *roster,const SudekiMpLanPartyLease *key) {
    /* Acquiring the same sparse AI lease is valid at a positively idle
     * combat boundary too. A menu may legitimately retire an old lease;
     * requiring exploration here stranded its replacement until combat ended.
     * body_idle and the lower adapter's fresh ownership/AI-mode checks remain
     * mandatory, exactly as for the existing combat movement path. */
    return boundary(w,roster) && ordinary_world(TRUE) &&
        SudekiMpLanPartyControlStoryAcquire(w,roster,key,body_idle);
}
BOOL SudekiMpLanStoryControlExact(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *roster,const SudekiMpLanPartyLease *key) {
    return boundary(w,roster) && SudekiMpLanPartyControlStoryExact(w,roster,key);
}
BOOL SudekiMpLanStoryControlMove(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *roster,const SudekiMpLanPartyLease *key,float x,float z,
    BOOL *temporarily_held) {
    if(temporarily_held) *temporarily_held=FALSE;
    if(!boundary(w,roster) || !key || key->seat>=4u || !isfinite(x) || !isfinite(z)) return FALSE;
    /* A neutral packet may quiesce our speed while native UI or a menu is
     * active. New locomotion still requires the ordinary unpaused world. */
    if(!ordinary_world(TRUE) || !body_idle(key,roster->actors[key->seat],w)) {
        /* A temporary native interaction is not a disconnect. The lower
         * adapter still proves this exact retained AI/movement lease before
         * stopping it; failure there remains an ownership/drain fault. */
        x=z=0.0f;
        if(temporarily_held) *temporarily_held=TRUE;
    }
    if(SudekiMpLanPartyControlStoryMove(w,roster,key,x,z)) return TRUE;
    /* A draw/attack task can temporarily own movement. Preserve the exact
     * AI lease just as the Test Room driver does; a rejected speed write is
     * not evidence of a disconnect. Never turn an identity failure into this
     * transient result. */
    if(GetLastError()==ERROR_BUSY && SudekiMpLanPartyControlStoryExact(w,roster,key)) {
        if(temporarily_held) *temporarily_held=TRUE;
        return TRUE;
    }
    return FALSE;
}
BOOL SudekiMpLanStoryControlDrain(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *roster,const SudekiMpLanPartyLease *key) {
    /* Do not require a valid current roster before the lower layer closes
     * retained admission. Its native writes still require the full proof. */
    return base && installed &&
        SudekiMpLanPartyControlStoryDrain(w,roster,key,body_idle);
}
BOOL SudekiMpLanStoryControlActorOwned(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *roster,unsigned character) {
    return boundary(w,roster) &&
        SudekiMpLanPartyControlStoryActorOwned(w,roster,character);
}
BOOL SudekiMpLanStoryControlRetainsKey(const SudekiMpLanPartyLease *key) {
    return SudekiMpLanPartyControlStoryRetainsKey(key);
}
BOOL SudekiMpLanStoryControlRetains(void) {
    return SudekiMpLanPartyControlStoryRetains();
}
BOOL SudekiMpLanStoryControlUninstall(void) {
    if(!base) return TRUE;
    if((native_thread && native_thread!=GetCurrentThreadId()) ||
        SudekiMpLanStoryControlRetains() || !SudekiMpLanPartyControlStoryEnd())
        return retain(ERROR_BUSY);
    BOOL restored=TRUE; DWORD error=ERROR_SUCCESS;
    for(unsigned i=2u;i-->0u;) if(!SudekiMpRestoreRelativeCallHook(&switches[i])) {
        if(restored) error=GetLastError();
        restored=FALSE;
    }
    if(!restored) return retain(error);
    base=NULL; installed=FALSE; native_thread=0;
    return TRUE;
}
