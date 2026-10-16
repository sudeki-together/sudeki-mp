#include "hooks/story_melee_snapshot.h"
#include "hooks/lan_story_control.h"
#include "hooks/lan_story_inventory.h"
#include "hooks/lan_story_cast.h"
#include "hooks/lan_party_control.h"
#include "hooks/story_interaction_guard.h"
#include "hooks/story_block_input.h"
#include "hooks/story_player_turn.h"
#include "hooks/story_ai_movement.h"
#include "hooks/story_idle_contact.h"
#include "hooks/story_player_movement.h"
#include "hooks/lan_story_research54.h"
#include "hooks/lan_story_runtime.h"
#include "hooks/call_hook.h"
#include "cleanroom/engine.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include "engine/skill_activation_abi.h"
#include "engine/weapon_activation_abi.h"
#include "network/lan_story_handoff.h"
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
static SudekiMpLanStoryNativeRoster idle_roster;
static BOOL idle_contact_reported;
/* Retail IsAttacking (RVA88D0) reads bit1000. Transfers/drains must wait for
 * it. Ordinary movement instead reaches native Move/CanMove every frame;
 * the arbiter owns which phases of the combo permit locomotion. */
/* 0x20 = native item-use action in progress (quick-menu path, RVA 0xDC110 →
 * 0x4DA5B0 sets 0x800|0x20; the completion handler 0x58A3C0 applies the item
 * only while 0x20 is still set). The mod must not drive the body meanwhile,
 * exactly as the native player controller's admission mask 0x29BE6F0 does
 * (owner gate 2026-10-15: Tal's action animated but never applied). */
enum { STORY_BODY_BUSY_FLAGS=0x081812c8u|0x20u };

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
static BOOL body_idle_common(const SudekiMpLanPartyLease *key,void *actor,
    const SudekiMpControlUpdateDispatchWitness *w,BOOL include_lighting,BOOL movement_request) {
    uint8_t *a=actor,*arbiter,*interaction,*mode; SudekiMpCharacterSkillState skill;
    static const uint32_t actor_vt[4]={0x2d5a88u,0x2d66fcu,0x2d5010u,0x2d555cu};
    BOOL pending=TRUE; int spirit=-1;
    BOOL aiming=movement_request && SudekiMpLanStoryCastAiming(actor);
    uint32_t busy=movement_request ? STORY_BODY_BUSY_FLAGS&~0x1000u : STORY_BODY_BUSY_FLAGS;
    if(!base || !key || key->seat>=4u || !w || !w->service_post_original_exact ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) ||
        !readable(a,0xdcu) || *(void **)a!=base+actor_vt[key->seat] ||
        !readable(arbiter=*(uint8_t **)(a+0x90u),0x64u) ||
        *(void **)arbiter!=base+0x2cc9acu || *(void **)(arbiter+0x10u)!=actor ||
        (*(uint32_t *)(arbiter+0x50u)&busy) || (arbiter[0x60u]&5u) ||
        SudekiMpCleanroomEngineRangedCombatPrimePending() ||
        !SudekiMpCleanroomEngineSpiritPresentationState(&spirit) || spirit!=0 ||
        !SudekiMpWeaponActivationPending(actor,&pending) || pending ||
        !SudekiMpObserveCharacterSkill(actor,&skill) || (!aiming && skill.active) ||
        !readable(skill.skill,0x78u) || *(void **)(a+0xd8u)!=skill.skill ||
        *(void **)((uint8_t *)skill.skill+0x10u)!=actor ||
        (!aiming && ((uint8_t *)skill.skill)[0x6cu])) return FALSE;
    void *task=*(void **)((uint8_t *)skill.skill+0x74u);
    if(!aiming && task && (!readable(task,8u) || *(void **)task || !*((uint32_t *)task+1))) return FALSE;
    interaction=*(uint8_t **)(a+0xa8u);
    if(interaction && (!readable(interaction,0x64u) ||
        !readable(mode=*(uint8_t **)(interaction+0x60u),0x4du) || mode[0x4cu])) return FALSE;
    return (aiming || (include_lighting?SudekiMpLanStoryCastDrained(actor):SudekiMpLanStoryCastBodyDrained(actor))) &&
        SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w);
}
static BOOL body_idle(const SudekiMpLanPartyLease *key,void *actor,
    const SudekiMpControlUpdateDispatchWitness *w) {
    return body_idle_common(key,actor,w,TRUE,FALSE);
}
static BOOL ordinary_world(BOOL allow_combat) {
    BOOL combat=TRUE; uint8_t *speed;
    return base && readable(base+0x408da0u,4u) &&
        readable(speed=*(uint8_t **)(base+0x408da0u),0x2cu) &&
        !speed[0x28u] && !*(uint16_t *)(speed+0x2au) &&
        !*(int *)(speed+0x20u) && !*(int *)(speed+0x24u) &&
        SudekiMpCleanroomEngineCombatMode(&combat) && (allow_combat || !combat);
}
static BOOL player_turn_lease(void *actor) {
    SudekiMpLanPartyLease key;
    return base && installed && native_thread && native_thread==GetCurrentThreadId() &&
        hooks_exact() && ordinary_world(TRUE) &&
        SudekiMpLanPartyControlStoryTurnOwner(actor,&key) &&
        SudekiMpLanStoryRuntimeTurnLeaseExact(&key);
}
static BOOL ai_movement_lease(void *actor,unsigned gate) {
    SudekiMpLanPartyLease key;
    /* An acquired PREPARE lease already owns this body's AI suppression.
     * Client input and player turning still wait for the separate READY ACK. */
    if(!base || !installed || !native_thread || native_thread!=GetCurrentThreadId() ||
        !hooks_exact() || !ordinary_world(TRUE) ||
        !SudekiMpLanPartyControlStoryTurnOwner(actor,&key) ||
        !SudekiMpLanStoryRuntimeAiLeaseExact(&key)) return FALSE;
    SudekiMpLanStoryResearch54AiGate(actor,gate);
    return TRUE;
}
/* The isolated StoryIdleContactImageTest covers the exact native seam, all
 * named byte contracts, ABI preservation and native contact responses.
 * Runtime admission additionally requires current host roster authority. */
static BOOL idle_contact_lease(void *actor) {
    BOOL pending=TRUE; int spirit=-1; SudekiMpCharacterSkillState skill;
    if(!base || !installed || !native_thread || native_thread!=GetCurrentThreadId() ||
        !ordinary_world(TRUE) || !hooks_exact() || !idle_roster.dispatch_serial ||
        SudekiMpLanStoryObserverWorldLoading() ||
        !SudekiMpLanStoryObserverNativeRosterExact(&idle_roster)) return FALSE;
    unsigned c=0;
    for(;c<4 && idle_roster.actors[c]!=actor;++c) {}
    if(c>=4 || !(idle_roster.available_mask&(1u<<c)) ||
        (c!=idle_roster.leader_character && !player_turn_lease(actor)) ||
        SudekiMpCleanroomEngineRangedCombatPrimePending() ||
        !SudekiMpCleanroomEngineSpiritPresentationState(&spirit) || spirit!=0 ||
        !SudekiMpWeaponActivationPending(actor,&pending) || pending ||
        !SudekiMpObserveCharacterSkill(actor,&skill) || skill.active ||
        !SudekiMpLanStoryCastBodyDrained(actor)) return FALSE;
    return TRUE;
}
void SudekiMpLanStoryControlObserveIdleContact(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *roster) {
    memset(&idle_roster,0,sizeof(idle_roster));
    if(boundary(w,roster) && SudekiMpStoryIdleContactBindThread()) idle_roster=*roster;
    if(!idle_contact_reported && SudekiMpStoryIdleContactCount()) {
        idle_contact_reported=TRUE;
        SudekiMpLogWrite("story_control event=idle_contact scope=stationary_party_terrain status=observed\r\n");
    }
}
BOOL SudekiMpLanStoryControlInstall(HMODULE image,unsigned locked_character_id) {
    uint8_t *b=(uint8_t *)image;
    static const uint8_t previous_context[]={0x83,0xbb,0xfc,0,0,0,1,0x75,0x0f,0x8b,0xf7};
    static const uint8_t next_context[]={0x83,0xbb,0xf4,0,0,0,1,0x75,9,0x8b,0xf7};
    static const uint8_t switch_entry[]={0x55,0x8b,0xec,0x83,0xe4,0xf8,0x83,0xec,0x18};
    if(!image || locked_character_id>=4u) { SetLastError(ERROR_NOT_SUPPORTED); return FALSE; }
    if(base || !SudekiMpCheckLoadedExecutable(image) ||
        !SudekiMpStoryMovementImageExact(b,readable) ||
        !SudekiMpStoryInteractionInputImageExact(b,readable) ||
        !SudekiMpStoryBlockImageExact(b,readable) ||
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
        !SudekiMpInstallRelativeCallHook(&switches[1],b+NEXT_CALL,b+NEXT,locked_character) ||
        !SudekiMpStoryPlayerTurnInstall(image,player_turn_lease) ||
        !SudekiMpStoryAiMovementInstall(image,ai_movement_lease) ||
        !SudekiMpStoryIdleContactInstall(image,idle_contact_lease)) {
        DWORD error=GetLastError();
        (void)SudekiMpLanStoryControlUninstall(); SetLastError(error); return FALSE;
    }
    installed=TRUE;
    SudekiMpLogWrite("story_control event=install scope=validated_saved_party native_input_rotation=locked turn_path=leased_player ai_movement=leased_gate\r\n");
    return TRUE;
}
BOOL SudekiMpLanStoryControlBegin(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *roster) {
    return boundary(w,roster) && SudekiMpLanPartyControlStoryBegin(w,roster) &&
        SudekiMpStoryPlayerTurnBindThread() && SudekiMpStoryAiMovementBindThread();
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
static BOOL block_world_ready(void *actor) {
    BOOL combat=FALSE,pending=TRUE; int spirit=-1;
    return ordinary_world(TRUE) && SudekiMpCleanroomEngineCombatMode(&combat) && combat &&
        !SudekiMpCleanroomEngineRangedCombatPrimePending() &&
        SudekiMpCleanroomEngineSpiritPresentationState(&spirit) && spirit==0 &&
        SudekiMpWeaponActivationPending(actor,&pending) && !pending &&
        SudekiMpLanStoryCastBodyDrained(actor) &&
        SudekiMpStoryMeleeInteractionClear(base,actor,2u,readable);
}
/* Bounded research diagnostic (item action, 2026-10-15): the remote body's
 * arbiter state word on every change, sampled at dispatch rate. */
static void trace_arbiter_flags(const SudekiMpLanStoryNativeRoster *roster,const SudekiMpLanPartyLease *key,const char *where) {
    static uint32_t last[4]; static unsigned logs;
    if(!roster || !key || key->seat>=4u || logs>=400u) return;
    uint8_t *a=roster->actors[key->seat],*arbiter=readable(a,0x94u)?*(uint8_t **)(a+0x90u):NULL;
    if(!readable(arbiter,0x64u)) return;
    uint32_t now=*(uint32_t *)(arbiter+0x50u);
    uint8_t *action=readable(a,0xa0u)?*(uint8_t **)(a+0x9cu):NULL; BOOL ok=readable(action,0x58u);
    uint8_t *block=readable(a,0xb8u)?*(uint8_t **)(a+0xb4u):NULL; BOOL bok=readable(block,0x20u);
    static uint32_t last_cancel[4],last_block[4];
    uint32_t cancel=ok?action[0x54u]:0u,bstate=bok?*(uint32_t *)(block+0x18u):0xffu;
    uint8_t *stats=readable(a,0x50u)?*(uint8_t **)(a+0x4cu):NULL; float hp=readable(stats,0x30u)?*(float *)(stats+0x2cu):-1.0f;
    static float last_hp[4];
    if(now!=last[key->seat] || cancel!=last_cancel[key->seat] || bstate!=last_block[key->seat] || hp!=last_hp[key->seat]) { ++logs;
        last_cancel[key->seat]=cancel; last_block[key->seat]=bstate; last_hp[key->seat]=hp;
        SudekiMpLogFormat("story_arbiter event=flags seat=%u where=%s arb50=%08lx->%08lx arb54=%d action=%p kind=%lu def=%08lx handle=%08lx applied=%lu block_state=%lu hp=%.0f ms=%lu\r\n",
            key->seat,where,(unsigned long)last[key->seat],(unsigned long)now,(int)(int8_t)arbiter[0x54u],(void *)action,
            ok?(unsigned long)*(uint32_t *)(action+0x40u):0ul,ok?(unsigned long)*(uint32_t *)(action+0x44u):0ul,
            ok?(unsigned long)*(uint32_t *)(action+0x48u):0ul,(unsigned long)cancel,(unsigned long)bstate,(double)hp,(unsigned long)GetTickCount());
        last[key->seat]=now; }
}
BOOL SudekiMpLanStoryControlMove(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *roster,const SudekiMpLanPartyLease *key,float x,float z,
    BOOL *temporarily_held) {
    trace_arbiter_flags(roster,key,"move");
    if(temporarily_held) *temporarily_held=FALSE;
    if(!boundary(w,roster) || !key || key->seat>=4u || !isfinite(x) || !isfinite(z) ||
        !SudekiMpLanPartyControlStoryExact(w,roster,key)) return FALSE;
    {   /* The native node at actor+8 keeps its pause references at +0x23.
         * TEMP travel may retain a genuine pause after the split owner removes
         * only its own added references. Preserve that pause without target,
         * dodge or even neutral movement submissions; an exact lease above is
         * mandatory before treating this as a temporary hold. */
        const uint8_t *a=roster->actors[key->seat];
        if(!readable(a,0x2cu)) return FALSE;
        if(a[0x2bu]) {
            if(temporarily_held) *temporarily_held=TRUE;
            return TRUE;
        }
    }
    {   /* Native item-use action in progress (arbiter 0x20): the native player
         * controller submits nothing while it runs, and even a zero-speed
         * submission interrupted it (owner gate 2026-10-15: animation played,
         * item never applied). Hold without touching the body. */
        uint8_t *a=roster->actors[key->seat],*arbiter=readable(a,0x94u)?*(uint8_t **)(a+0x90u):NULL;
        if(readable(arbiter,0x64u) && *(void **)(arbiter+0x10u)==a && (*(uint32_t *)(arbiter+0x50u)&0x20u)) {
            if(temporarily_held) *temporarily_held=TRUE;
            return TRUE;
        }
    }
    /* A neutral packet may quiesce our speed while native UI or a menu is
     * active. New locomotion still requires the ordinary unpaused world. */
    BOOL ordinary=ordinary_world(TRUE);
    if(ordinary && !SudekiMpLanPartyControlStoryRefreshTarget(w,roster,key,x,z)) return FALSE;
    if((key->seat==0u || key->seat==2u) && block_world_ready(roster->actors[key->seat])) {
        BOOL handled=FALSE;
        if(!SudekiMpLanPartyControlStoryDodge(w,roster,key,x,z,&handled)) return FALSE;
        if(handled) return TRUE;
    }
    if(!ordinary || !body_idle_common(key,roster->actors[key->seat],w,FALSE,TRUE)) {
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
BOOL SudekiMpLanStoryControlBlock(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *roster,const SudekiMpLanPartyLease *key,BOOL held) {
    if(!key || (key->seat!=0u && key->seat!=2u) || !boundary(w,roster) ||
        !SudekiMpLanPartyControlStoryExact(w,roster,key)) return FALSE;
    if(held && !block_world_ready(roster->actors[key->seat])) held=FALSE;
    return SudekiMpLanPartyControlStoryBlock(w,roster,key,held);
}
unsigned SudekiMpLanStoryControlMelee(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *roster,const SudekiMpLanPartyLease *key,unsigned kind) {
    if(!key || (key->seat!=0u && key->seat!=2u) || kind<1u || kind>3u || !boundary(w,roster) ||
        !SudekiMpLanPartyControlStoryExact(w,roster,key)) return SUDEKIMP_STORY_ACTION_UNAVAILABLE;
    BOOL pending=TRUE; int spirit=-1;
    void *actor=roster->actors[key->seat];
    /* Do not require body_idle: a native combo accepts another press while
     * its previous swing owns animation/movement. Native arbiter validation
     * remains responsible for combat state and combo gates. Its early
     * interaction branches are NOT actor-isolated: deny those before entry. */
    if(!ordinary_world(TRUE) || SudekiMpCleanroomEngineRangedCombatPrimePending() ||
        !SudekiMpCleanroomEngineSpiritPresentationState(&spirit) || spirit!=0 ||
        !SudekiMpWeaponActivationPending(actor,&pending) || pending ||
        !SudekiMpLanStoryCastBodyDrained(actor) ||
        !SudekiMpStoryMeleeInteractionRestorable(base,actor,kind,readable,&(uint8_t *){NULL}))
        return SUDEKIMP_STORY_ACTION_BUSY;
    BOOL submitted=FALSE;
    if(SudekiMpLanPartyControlStoryMelee(w,roster,key,kind,&submitted))
        return SUDEKIMP_STORY_ACTION_SUBMITTED;
    return submitted?SUDEKIMP_STORY_ACTION_RETAINED:SUDEKIMP_STORY_ACTION_UNAVAILABLE;
}
unsigned SudekiMpLanStoryControlMeleeSnapshot(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *roster,const SudekiMpLanPartyLease *key,unsigned buttons) {
    if(!key || (key->seat!=0u && key->seat!=2u) || buttons>63u || !boundary(w,roster) ||
        !SudekiMpLanPartyControlStoryExact(w,roster,key)) return SUDEKIMP_STORY_ACTION_UNAVAILABLE;
    BOOL pending=TRUE; int spirit=-1;
    void *actor=roster->actors[key->seat];
    /* Do not require body_idle: a native combo accepts another press while
     * its previous swing owns animation/movement. Native arbiter validation
     * remains responsible for combat state and combo gates. Its early
     * interaction branches are NOT actor-isolated: deny those before entry. */
    if(!ordinary_world(TRUE) || SudekiMpCleanroomEngineRangedCombatPrimePending() ||
        !SudekiMpCleanroomEngineSpiritPresentationState(&spirit) || spirit!=0 ||
        !SudekiMpWeaponActivationPending(actor,&pending) || pending ||
        !SudekiMpLanStoryCastBodyDrained(actor) ||
        !SudekiMpStoryMeleeSnapshotInteractionClear(base,actor,readable))
        return SUDEKIMP_STORY_ACTION_BUSY;
    BOOL submitted=FALSE;
    if(SudekiMpLanPartyControlStoryMeleeSnapshot(w,roster,key,buttons,&submitted))
        return SUDEKIMP_STORY_ACTION_SUBMITTED;
    return submitted?SUDEKIMP_STORY_ACTION_RETAINED:SUDEKIMP_STORY_ACTION_UNAVAILABLE;
}
unsigned SudekiMpLanStoryControlRanged(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *roster,const SudekiMpLanPartyLease *key,unsigned state) {
    if(!key || (key->seat!=1u && key->seat!=3u) || state<1u || state>2u || !boundary(w,roster) ||
        !SudekiMpLanPartyControlStoryExact(w,roster,key)) return SUDEKIMP_STORY_ACTION_UNAVAILABLE;
    BOOL pending=TRUE; int spirit=-1;
    void *actor=roster->actors[key->seat];
    /* Native CanFire owns reload and hold cadence. Do not require body_idle
     * during a weapon's firing cycle. Deny the arbiter's nonisolated early
     * interaction branches before submitting either press or held input. */
    if(!ordinary_world(TRUE) || SudekiMpCleanroomEngineRangedCombatPrimePending() ||
        !SudekiMpCleanroomEngineSpiritPresentationState(&spirit) || spirit!=0 ||
        !SudekiMpWeaponActivationPending(actor,&pending) || pending ||
        !SudekiMpLanStoryCastBodyDrained(actor) ||
        !SudekiMpStoryRangedInteractionRestorable(base,actor,state,readable,&(uint8_t *){NULL}))
        return SUDEKIMP_STORY_ACTION_BUSY;
    BOOL submitted=FALSE;
    if(SudekiMpLanPartyControlStoryRanged(w,roster,key,state,&submitted))
        return SUDEKIMP_STORY_ACTION_SUBMITTED;
    return submitted?SUDEKIMP_STORY_ACTION_RETAINED:SUDEKIMP_STORY_ACTION_UNAVAILABLE;
}
BOOL SudekiMpLanStoryControlDrain(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *roster,const SudekiMpLanPartyLease *key) {
    /* Do not require a valid current roster before the lower layer closes
     * retained admission. Its native writes still require the full proof. */
    return base && installed &&
        SudekiMpLanPartyControlStoryDrain(w,roster,key,body_idle);
}
/* World-load variant (#42): the mod's spirit/cast/weapon observers cannot
 * answer while the native world load is pending, so prove only the native
 * body itself is idle (arbiter action flags, interaction mode, skill byte). */
static BOOL body_idle_load(const SudekiMpLanPartyLease *key,void *actor,
    const SudekiMpControlUpdateDispatchWitness *w) {
    uint8_t *a=actor,*arbiter,*interaction,*mode,*skill;
    static const uint32_t actor_vt[4]={0x2d5a88u,0x2d66fcu,0x2d5010u,0x2d555cu};
    /* w==NULL: called inside a native zone call (no dispatch witness). */
    if(!base || !key || key->seat>=4u ||
        (w && (!w->service_post_original_exact || !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w))) ||
        !readable(a,0xdcu) || *(void **)a!=base+actor_vt[key->seat] ||
        !readable(arbiter=*(uint8_t **)(a+0x90u),0x64u) ||
        *(void **)arbiter!=base+0x2cc9acu || *(void **)(arbiter+0x10u)!=actor ||
        (*(uint32_t *)(arbiter+0x50u)&STORY_BODY_BUSY_FLAGS) || (arbiter[0x60u]&5u)) return FALSE;
    skill=*(uint8_t **)(a+0xd8u);
    if(skill && (!readable(skill,0x78u) || *(void **)(skill+0x10u)!=actor || skill[0x6cu])) return FALSE;
    interaction=*(uint8_t **)(a+0xa8u);
    if(interaction && (!readable(interaction,0x64u) ||
        !readable(mode=*(uint8_t **)(interaction+0x60u),0x4du) || mode[0x4cu])) return FALSE;
    return !w || SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w);
}
BOOL SudekiMpLanStoryControlLoadDrain(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *key) {
    return base && installed && SudekiMpLanPartyControlStoryLoadDrain(w,key,body_idle_load);
}
BOOL SudekiMpLanStoryControlReleaseBeforeWorldChange(const SudekiMpLanPartyLease *key) {
    return base && installed && SudekiMpLanPartyControlStoryReleaseBeforeWorldChange(key,body_idle_load);
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
    memset(&idle_roster,0,sizeof(idle_roster));
    if(!SudekiMpStoryIdleContactUninstall()) return retain(GetLastError());
    if(!SudekiMpStoryAiMovementUninstall()) return retain(GetLastError());
    if(!SudekiMpStoryPlayerTurnUninstall()) return retain(GetLastError());
    BOOL restored=TRUE; DWORD error=ERROR_SUCCESS;
    for(unsigned i=2u;i-->0u;) if(!SudekiMpRestoreRelativeCallHook(&switches[i])) {
        if(restored) error=GetLastError();
        restored=FALSE;
    }
    if(!restored) return retain(error);
    base=NULL; installed=FALSE; native_thread=0; idle_contact_reported=FALSE;
    return TRUE;
}

unsigned SudekiMpLanStoryControlEquipWeapon(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *roster,const SudekiMpLanPartyLease *key,unsigned item) {
    if(!boundary(w,roster) || !key || key->seat>=4u || !(roster->available_mask&(1u<<key->seat)) ||
        !SudekiMpStoryWeaponItemForCharacter(key->seat,item) ||
        !SudekiMpLanPartyControlStoryExact(w,roster,key)) return SUDEKIMP_STORY_ACTION_UNAVAILABLE;
    void *actor=roster->actors[key->seat];
    /* Native rule: no weapon change while fighting, drawing or casting. */
    if(!ordinary_world(FALSE) || !body_idle(key,actor,w)) return SUDEKIMP_STORY_ACTION_BUSY;
    SudekiMpWeaponQuickList list;
    if(!SudekiMpGrantCharacterWeaponItem(actor,item) || !SudekiMpDescribeCharacterWeapons(actor,&list) ||
        !SudekiMpLanPartyControlStoryExact(w,roster,key)) return SUDEKIMP_STORY_ACTION_UNAVAILABLE;
    unsigned slot=SUDEKIMP_WEAPON_ACTIVATION_MAX_ROWS; BOOL equipped=FALSE;
    for(unsigned i=0;i<list.row_count;++i) {
        const uint8_t *row_item=list.rows[i].native_item;
        if(!readable(row_item,0x18u)) return SUDEKIMP_STORY_ACTION_UNAVAILABLE;
        if(*(const uint32_t *)(row_item+0x14u)==item) { slot=list.rows[i].slot; equipped=list.rows[i].equipped; }
    }
    if(slot>=SUDEKIMP_WEAPON_ACTIVATION_MAX_ROWS) return SUDEKIMP_STORY_ACTION_UNAVAILABLE;
    if(equipped) return SUDEKIMP_STORY_ACTION_STARTED;
    /* The Main Menu's own equip core, not the combat quick-activation: that
     * one left an idle AI-driven Tal on its old weapon (live, 2026-10-15). */
    BOOL applied=SudekiMpEquipCharacterWeaponItem(actor,item);
    unsigned outcome=applied?SUDEKIMP_STORY_ACTION_STARTED:SUDEKIMP_STORY_ACTION_UNAVAILABLE;
    SudekiMpLogFormat("story_main_menu event=host_equip character=%u item=%u slot=%u applied=%u outcome=%u\r\n",
        key->seat,item,slot,(unsigned)applied,outcome);
    return outcome;
}

unsigned SudekiMpLanStoryControlEquipArmour(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *roster,const SudekiMpLanPartyLease *key,unsigned item) {
    if(!boundary(w,roster) || !key || key->seat>=4u || !(roster->available_mask&(1u<<key->seat)) ||
        !SudekiMpArmourItemWearable(key->seat,item) ||
        !SudekiMpLanPartyControlStoryExact(w,roster,key)) return SUDEKIMP_STORY_ACTION_UNAVAILABLE;
    void *actor=roster->actors[key->seat];
    /* Same native rule as a weapon change: never while fighting or acting. */
    if(!ordinary_world(FALSE) || !body_idle(key,actor,w)) return SUDEKIMP_STORY_ACTION_BUSY;
    unsigned worn=0;
    if(!SudekiMpCharacterWornArmour(actor,&worn)) return SUDEKIMP_STORY_ACTION_UNAVAILABLE;
    if(worn==item) return SUDEKIMP_STORY_ACTION_STARTED;
    /* D9: per-game inventory; equipment follows the character, so the host
     * receives the client's chosen armour once if it does not own it. */
    SudekiMpStoryInventoryGrant grant; SudekiMpStoryInventoryGrantResult granted={0};
    BOOL owned=FALSE,grant_ran=FALSE;
    if(SudekiMpLanStoryInventoryPrepare(item,1u,&grant)) {
        owned=grant.before>0u;
        if(!owned) grant_ran=SudekiMpLanStoryInventoryApply(&grant,&granted);
    }
    BOOL applied=SudekiMpLanPartyControlStoryExact(w,roster,key) && SudekiMpEquipCharacterArmourItem(actor,key->seat,item);
    unsigned outcome=applied?SUDEKIMP_STORY_ACTION_STARTED:SUDEKIMP_STORY_ACTION_UNAVAILABLE;
    SudekiMpLogFormat("story_main_menu event=host_equip_armour character=%u item=%u from=%u owned=%u granted=%u applied=%u outcome=%u\r\n",
        key->seat,item,worn,(unsigned)owned,grant_ran?(unsigned)granted.status:0u,(unsigned)applied,outcome);
    return outcome;
}

unsigned SudekiMpLanStoryControlUseItem(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *roster,const SudekiMpLanPartyLease *key,unsigned item) {
    if(!boundary(w,roster) || !key || key->seat>=4u || !(roster->available_mask&(1u<<key->seat)) ||
        item>=999u || !SudekiMpLanPartyControlStoryExact(w,roster,key)) return SUDEKIMP_STORY_ACTION_UNAVAILABLE;
    void *actor=roster->actors[key->seat];
    if(!ordinary_world(FALSE) || !body_idle(key,actor,w)) return SUDEKIMP_STORY_ACTION_BUSY;
    /* Prefer the native "use item" action (quick-menu path: animation,
     * keyframe apply, item effect through the character's arbiter); the
     * direct Items-page apply remains the fallback (owner, 2026-10-15). */
    unsigned stage=0,action_stage=0,slot=0; BOOL applied=FALSE,action=FALSE;
    {   /* The character's party slot (group+0x90+slot*0xC), as the quick menu indexes it. */
        const uint8_t *group=roster->group; unsigned count=group?*(const unsigned *)(group+0xccu):0u;
        for(unsigned i=0;i<count && i<4u;++i) if(*(void *const *)(group+0x90u+i*0xcu)==actor) slot=i;
    }
    /* D9: the host owns the actor's action, while the requesting client's
     * game owns the consumed item. Its accepted result debits that inventory
     * once; neither native route below may also debit the host's copy. */
    action=SudekiMpUseItemActionOnCharacter(actor,item,slot,&action_stage,FALSE);
    /* Native quick-menu path (animation, keyframe apply, item effect through
     * the character's own arbiter; CONFIRMED_LIVE for the remote Tal on
     * 2026-10-15 once the native can-use ran first). The direct Items-page
     * apply remains the fallback when the action cannot start. */
    if(!action && action_stage<50u) applied=SudekiMpUseItemOnCharacter(actor,item,&stage,FALSE,FALSE);
    {   const uint8_t *group=roster->group; const uint8_t *s0=group?group+0x90u+slot*0xcu:NULL;
        SudekiMpLogFormat("story_main_menu event=host_use_target actor=%p slot=%u slot_words=%08lx,%08lx,%08lx\r\n",actor,slot,
            s0?(unsigned long)*(const uint32_t *)s0:0ul,s0?(unsigned long)*(const uint32_t *)(s0+4u):0ul,s0?(unsigned long)*(const uint32_t *)(s0+8u):0ul); }
    unsigned outcome=(action || applied)?SUDEKIMP_STORY_ACTION_STARTED:
        (action_stage>=50u || stage>=100u)?SUDEKIMP_STORY_ACTION_BUSY:SUDEKIMP_STORY_ACTION_UNAVAILABLE;
    /* can-use refused (stage 100+code, e.g. full HP): not a body-busy case. */
    if(!action && action_stage>=100u && !applied) outcome=SUDEKIMP_STORY_ACTION_BUSY;
    trace_arbiter_flags(roster,key,"use");
    SudekiMpLogFormat("story_main_menu event=host_use character=%u item=%u action=%u action_stage=%u applied=%u stage=%u outcome=%u\r\n",
        key->seat,item,(unsigned)action,action_stage,(unsigned)applied,stage,outcome);
    return outcome;
}
