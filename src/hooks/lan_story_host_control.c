#include "hooks/lan_story_host_control.h"
#include "hooks/lan_story_task_trace.h"
#include "hooks/lan_story_control.h"
#include "cleanroom/engine.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include "engine/skill_activation_abi.h"
#include "engine/spirit_instance_abi.h"
#include "engine/weapon_activation_abi.h"
#include <stdint.h>
#include <string.h>
#include <wincrypt.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Story host control requires the supported x86 native ABI"
#endif

enum { FILTER_NONE=0x8ac0u, FILTER_ALL=0x8ae0u, NEXT=0x24060u,
    SWITCH_SIZE=0xfcu, CONTROLLER_VT=0x2c9f5cu, INPUT_VT=0x2c9f84u };
typedef void (__attribute__((thiscall)) *Filter)(void *);
static uint8_t *base;
static uint8_t verified_next[SWITCH_SIZE];
static unsigned locked,requested=4u;
static DWORD native_thread,next_attempt;
static BOOL active,stopping;
static SudekiMpLanStoryHostLeaderAiExact leader_ai_exact;
static struct {
    void *world,*descriptor,*group,*controller;
    uint32_t epoch;
    BOOL owned,releasing;
} input_owner;
static struct {
    SudekiMpLanStoryNativeRoster before;
    uint32_t load,task;
    uint64_t dispatch;
    BOOL entered;
} rotation;
static uint32_t completed_load,completed_task;
static const uint8_t none_code[]={0x56,0x8b,0xf1,0xc7,0x81,0x84,0,0,0,
    0,0,0,0,0xe8,0xfe,0x05,0x02,0,0x5e,0xc3};
static const uint8_t all_code[]={0x56,0x8b,0xf1,0xc7,0x81,0x84,0,0,0,
    1,0,0,0,0xe8,0xde,0x05,0x02,0,0x5e,0xc3};

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || !VirtualQuery(p,&m,sizeof(m)) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
    DWORD access=m.Protect&0xffu;
    return (access==PAGE_READONLY || access==PAGE_READWRITE || access==PAGE_WRITECOPY ||
        access==PAGE_EXECUTE_READ || access==PAGE_EXECUTE_READWRITE ||
        access==PAGE_EXECUTE_WRITECOPY) && a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL retain(DWORD error) {
    HMODULE self;
    (void)GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        (LPCSTR)&input_owner,&self);
    SetLastError(error?error:ERROR_BUSY); return FALSE;
}
static BOOL next_supported(uint8_t *image) {
    /* Same normalized whole-function proof as the established SMP4 native
     * rotation adapter. Story requires the pristine entry, never its RET
     * campaign patch or a trampoline borrowed from another owner. */
    static const uint8_t expected[32]={
        0x6a,0x72,0x83,0xc4,0x8a,0x49,0xd5,0xde,0x30,0x23,0x7c,0xf9,0xad,0x34,0x58,0x8d,
        0xb0,0x1f,0x1f,0x4c,0x22,0x13,0x98,0x46,0xcf,0xd7,0x27,0xc9,0x15,0x4d,0x15,0x66};
    uint8_t normalized[SWITCH_SIZE],digest[32]; uint32_t operand=0x408d1cu;
    HCRYPTPROV provider=0; HCRYPTHASH hash=0; DWORD n=sizeof(digest); BOOL exact=FALSE;
    if(!readable(image+NEXT,sizeof(normalized)) || image[NEXT]!=0x55 ||
        *(void **)(image+NEXT+0x5e)!=image+operand) return FALSE;
    memcpy(normalized,image+NEXT,sizeof(normalized)); memcpy(normalized+0x5e,&operand,4);
    if(CryptAcquireContextW(&provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT|CRYPT_SILENT) &&
        CryptCreateHash(provider,CALG_SHA_256,0,0,&hash) &&
        CryptHashData(hash,normalized,sizeof(normalized),0) &&
        CryptGetHashParam(hash,HP_HASHVAL,digest,&n,0) && n==sizeof(digest))
        exact=!memcmp(digest,expected,sizeof(digest));
    if(hash) CryptDestroyHash(hash);
    if(provider) CryptReleaseContext(provider,0);
    if(exact) memcpy(verified_next,image+NEXT,sizeof(verified_next));
    return exact;
}
static BOOL entries_exact(void) {
    /* Private caster namespaces own these two native filter entries. Accept
     * only that adapter's complete live-hook/trampoline/neutral-scope proof;
     * arbitrary changed bytes still invalidate the host binding. */
    return base && (!memcmp(base+FILTER_NONE,none_code,sizeof(none_code)) ||
            SudekiMpSpiritInstanceFilterNoneEntryExact((HMODULE)base)) &&
        (!memcmp(base+FILTER_ALL,all_code,sizeof(all_code)) ||
            SudekiMpSpiritInstanceFilterAllEntryExact((HMODULE)base)) &&
        !memcmp(base+NEXT,verified_next,sizeof(verified_next));
}
static BOOL observe(void *controller,const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryScene *scene,SudekiMpLanStoryNativeRoster *r) {
    static const char *trace; static unsigned traces;
    const char *why=!base?"base":active?"active":!w?"witness":!w->service_only?"service_only":
        !w->service_post_original_exact?"post_original":!w->dispatch_serial?"serial":
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w)?"witness_stale":
        (native_thread && native_thread!=GetCurrentThreadId())?"thread":
        !entries_exact()?(memcmp(base+NEXT,verified_next,sizeof(verified_next))?"filter_next":
            (memcmp(base+FILTER_NONE,none_code,sizeof(none_code)) &&
             !SudekiMpSpiritInstanceFilterNoneEntryExact((HMODULE)base))?"filter_none":"filter_all"):
        !SudekiMpLanStoryObserverRoster(controller,w,scene,r)?"roster":
        !readable(controller,0x24cu) || *(void **)controller!=base+CONTROLLER_VT ||
        *(void **)((uint8_t *)controller+0x2cu)!=base+INPUT_VT?"controller":NULL;
    if(why!=trace && traces<48u) {
        ++traces; trace=why;
        SudekiMpLogFormat("lan_story_host_control event=observe result=%s phase=%u temporary=%s spirit_none=%u spirit_all=%u fault_site=%u enter_failure=%u named_fail=%u\r\n",
            why?why:"ok",scene?scene->phase:9u,scene?scene->temporary:"-",
            base?SudekiMpSpiritInstanceFilterEntryDiag((HMODULE)base,0u):0u,
            base?SudekiMpSpiritInstanceFilterEntryDiag((HMODULE)base,1u):0u,
            SudekiMpSpiritInstanceFaultSite(),SudekiMpSpiritInstanceEnterFailure(),SudekiMpSpiritInstanceNamedFail());
    }
    if(why) return FALSE;
    native_thread=GetCurrentThreadId(); return TRUE;
}
static BOOL owner_matches(const SudekiMpLanStoryNativeRoster *r) {
    /* The observer epoch is the exterior lifetime: a split-area TEMP changes
     * the current descriptor without replacing the world or party. */
    return !input_owner.owned || (input_owner.world==r->world &&
        (input_owner.descriptor==r->descriptor || input_owner.epoch==r->epoch) && input_owner.group==r->group &&
        input_owner.controller==r->controller);
}
static BOOL binding_exact(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,BOOL allow_remote) {
    /* The sparse observer proves each canonical identity and native party
     * membership. Native control is additionally proved by AI ownership:
     * exactly the front/controller target has AI disabled; no override is
     * borrowed from a script or another multiplayer controller. */
    for(unsigned c=0;c<4u;++c) if(r->available_mask&(1u<<c)) {
        uint8_t *a=r->actors[c],*ai=r->ai[c],*mode;
        if(!readable(a,0x98u) || *(void **)(a+0x94u)!=ai || !readable(ai,0x174u) ||
            *(void **)ai!=base+0x2d4924u || *(void **)(ai+0x10u)!=a ||
            !readable(mode=*(uint8_t **)(ai+0x3cu),0xcu)) return FALSE;
        if(allow_remote && c!=r->leader_character &&
            SudekiMpLanStoryControlActorOwned(w,r,c)) continue;
        if(allow_remote && c==r->leader_character && leader_ai_exact &&
            *(int16_t *)(ai+0x16au)==0 && mode[0xbu]==1u && leader_ai_exact(w,r)) continue;
        if(*(int16_t *)(ai+0x16au)!=0 ||
            mode[0xbu]!=(c==r->leader_character?0u:1u)) return FALSE;
    }
    return TRUE;
}
static BOOL same_native_party(const SudekiMpLanStoryNativeRoster *a,
    const SudekiMpLanStoryNativeRoster *b) {
    return a->world==b->world && (a->descriptor==b->descriptor || a->epoch==b->epoch) && a->group==b->group &&
        a->controller==b->controller && a->epoch==b->epoch &&
        a->available_mask==b->available_mask && !memcmp(a->actors,b->actors,sizeof(a->actors)) &&
        !memcmp(a->ai,b->ai,sizeof(a->ai));
}
static BOOL menus_clear(void);
static BOOL input_filter(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,BOOL blocked) {
    uint8_t *c=r->controller;
    if(!owner_matches(r) || !entries_exact() ||
        !SudekiMpLanStoryObserverRosterStillExact(w,r)) return FALSE;
    int current=*(int *)(c+0x80u),pending=*(int *)(c+0x84u);
    /* Native UI retirement (including the ranged combat prime) can restore
     * All while our custom menu is still open. The exact original pair on
     * the SAME owner ends this lease; there is nothing left to restore.
     * Never write a mode to repair drift, and never treat a pending-only All
     * or a foreign filter as completion. Reopening below still needs a fresh
     * native-menu boundary before acquiring a new None lease. */
    if(input_owner.owned && current==1 && pending==1)
        memset(&input_owner,0,sizeof(input_owner));
    if(blocked) {
        if(!input_owner.owned) {
            if(current!=1 || pending!=1 || !menus_clear()) return FALSE;
            input_owner.world=r->world; input_owner.descriptor=r->descriptor; input_owner.epoch=r->epoch;
            input_owner.group=r->group; input_owner.controller=c;
            input_owner.owned=TRUE; input_owner.releasing=FALSE;
        } else if(input_owner.releasing) {
            /* Cancel only this adapter's own pending All request. Unknown
             * skill/menu filter drift is never repaired by writing modes. */
            if((current!=0 && current!=1) || pending!=1) return FALSE;
            input_owner.releasing=FALSE;
        } else return current==0 && pending==0;
        active=TRUE; ((Filter)(base+FILTER_NONE))(c); active=FALSE;
        return entries_exact() && SudekiMpLanStoryObserverRosterStillExact(w,r) &&
            *(int *)(c+0x80u)==0 && *(int *)(c+0x84u)==0;
    }
    if(!input_owner.owned) return current==1 && pending==1;
    if(!input_owner.releasing) {
        if(current!=0 || pending!=0) return FALSE;
        input_owner.releasing=TRUE;
        active=TRUE; ((Filter)(base+FILTER_ALL))(c); active=FALSE;
    }
    if(!entries_exact() || !SudekiMpLanStoryObserverRosterStillExact(w,r) ||
        *(int *)(c+0x80u)!=1 || *(int *)(c+0x84u)!=1) return FALSE;
    memset(&input_owner,0,sizeof(input_owner)); return TRUE;
}
static BOOL menus_clear(void) {
    uint8_t *ui=*(uint8_t **)(base+0x408d3cu),*quit=*(uint8_t **)(base+0x408d68u),
        *quick=*(uint8_t **)(base+0x3c2f84u),*speed=*(uint8_t **)(base+0x408da0u);
    return !*(void **)(base+0x408db8u) &&
        (!ui || (readable(ui,0x47eu) && !ui[0x47du])) &&
        readable(quit,0x1c3u) && !quit[0x1c2u] && readable(quick,0x2au) && !quick[0x29u] &&
        readable(speed,0x2cu) && !speed[0x28u] && !*(uint16_t *)(speed+0x2au) &&
        !*(int *)(speed+0x20u) && !*(int *)(speed+0x24u);
}
static BOOL actor_idle(void *actor) {
    uint8_t *a=actor,*arbiter,*interaction,*mode; SudekiMpCharacterSkillState skill;
    BOOL weapon_pending=TRUE;
    if(!readable(a,0xdcu) || !readable(arbiter=*(uint8_t **)(a+0x90u),0x64u) ||
        *(void **)arbiter!=base+0x2cc9acu || *(void **)(arbiter+0x10u)!=actor ||
        (*(uint32_t *)(arbiter+0x50u)&0x081802c8u) || (arbiter[0x60u]&5u) ||
        !SudekiMpWeaponActivationPending(actor,&weapon_pending) || weapon_pending ||
        !SudekiMpObserveCharacterSkill(actor,&skill) || skill.active ||
        !readable(skill.skill,0x78u) || *(void **)(a+0xd8u)!=skill.skill ||
        *(void **)((uint8_t *)skill.skill+0x10u)!=actor || ((uint8_t *)skill.skill)[0x6cu]) return FALSE;
    void *task=*(void **)((uint8_t *)skill.skill+0x74u);
    if(task && (!readable(task,8u) || *(void **)task || !*((uint32_t *)task+1))) return FALSE;
    interaction=*(uint8_t **)(a+0xa8u);
    if(interaction && (!readable(interaction,0x64u) ||
        !readable(mode=*(uint8_t **)(interaction+0x60u),0x4du) || mode[0x4cu])) return FALSE;
    return TRUE;
}
static BOOL switch_ready(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,BOOL allow_remote) {
    uint8_t *g=r->group,*c=r->controller; BOOL combat=TRUE; int spirit=-1;
    if(!readable(g,0xd8u) || *(unsigned *)(g+0xd0u) || !g[0xd6u] || g[0xd7u] ||
        *(void **)(g+0xc0u) || *(unsigned *)(c+0xf4u) || *(unsigned *)(c+0xfcu) ||
        !menus_clear() || !binding_exact(w,r,allow_remote) || !SudekiMpCleanroomEngineCombatMode(&combat) || combat ||
        SudekiMpCleanroomEngineRangedCombatPrimePending() ||
        !SudekiMpCleanroomEngineSpiritPresentationState(&spirit) || spirit!=0) return FALSE;
    for(unsigned i=0;i<4u;++i) if(r->actors[i] && !actor_idle(r->actors[i])) return FALSE;
    return SudekiMpLanStoryObserverRosterStillExact(w,r);
}
BOOL SudekiMpLanStoryHostControlSetLeaderAiWitness(SudekiMpLanStoryHostLeaderAiExact exact) {
    if(!base || native_thread || active || input_owner.owned || rotation.entered) return FALSE;
    leader_ai_exact=exact; return TRUE;
}
BOOL SudekiMpLanStoryHostControlLeaderActionsDrained(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r) {
    if(!base || !r || r->leader_character>=4u || !SudekiMpLanStoryObserverRosterStillExact(w,r) ||
        !actor_idle(r->actors[r->leader_character])) return FALSE;
    const uint8_t *actor=r->actors[r->leader_character];
    const uint8_t *arbiter=*(const uint8_t *const *)(actor+0x90u);
    return readable(arbiter,0x64u) && *(const void *const *)(arbiter+0x10u)==actor &&
        !(*(const uint32_t *)(arbiter+0x50u)&0x1000u) && SudekiMpLanStoryObserverRosterStillExact(w,r);
}
/* Retail entry ABI: ESI=group, no stack arguments, ordinary RET. This calls
 * the entire pristine function including native veto, intrusive rotation,
 * AI transition and camera/HUD/listener notification. */
__attribute__((naked,noinline,used)) static void call_next(void *group __attribute__((unused)),
    void *function __attribute__((unused))) {
    __asm__ volatile("pushl %esi\n\tmovl 8(%esp),%esi\n\tcall *12(%esp)\n\tpopl %esi\n\tret\n\t");
}

BOOL SudekiMpLanStoryHostControlInstall(HMODULE image,unsigned locked_character) {
    uint8_t *b=(uint8_t *)image;
    if(!image || locked_character>=4u) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    if(base || !SudekiMpCheckLoadedExecutable(image) ||
        !readable(b+FILTER_NONE,sizeof(none_code)) || !readable(b+FILTER_ALL,sizeof(all_code)) ||
        memcmp(b+FILTER_NONE,none_code,sizeof(none_code)) ||
        memcmp(b+FILTER_ALL,all_code,sizeof(all_code)) || !next_supported(b)) {
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    base=b; locked=locked_character; requested=4u; native_thread=next_attempt=0; leader_ai_exact=NULL;
    active=stopping=FALSE; completed_load=completed_task=0;
    memset(&input_owner,0,sizeof(input_owner)); memset(&rotation,0,sizeof(rotation));
    return TRUE;
}

/* Recruitment proof is deliberately narrower than "party count is two".
 * The native Lighthouse root must have added this actual Ailish and reached
 * both normal terminal return and native retirement in the same load. */
static BOOL recruitment_ready(const SudekiMpLanStoryNativeRoster *r,
    SudekiMpLanStoryRecruitmentStatus *out) {
    SudekiMpLanStoryTaskTraceStatus load;
    return locked==2u && r->available_mask==12u && r->leader_character==3u &&
        SudekiMpLanStoryTaskTraceGetStatus(&load) && !load.unknown && load.load_generation &&
        load.start_terminal && load.start_retired && load.on_load_terminal && load.on_load_retired &&
        SudekiMpLanStoryTaskTraceGetRecruitmentStatus(out) && !out->unknown &&
        out->load_generation==load.load_generation && out->task && out->hash==0x4fb91e97u &&
        out->terminal && out->retired && out->party_add_exact && out->party_add_count==1u &&
        /* Native spawn has already inserted Ailish before this script's
         * explicit AddPlayer. Both witnessed opening runs recorded 2 -> 2;
         * inventing a 1 -> 2 requirement would never admit that handoff. */
        out->party_add_before==2u && out->party_add_after==2u && out->added_actor==r->actors[3] &&
        !(out->load_generation==completed_load && out->task==completed_task);
}

BOOL SudekiMpLanStoryHostControlService(void *controller,
    const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryScene *scene,BOOL menu_open) {
    SudekiMpLanStoryNativeRoster r;
    if(!observe(controller,w,scene,&r) || !owner_matches(&r)) {
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    if(rotation.entered) {
        if(w->dispatch_serial==rotation.dispatch || !same_native_party(&rotation.before,&r) ||
            !binding_exact(w,&r,FALSE) || !input_owner.owned || input_owner.releasing ||
            *(int *)((uint8_t *)controller+0x80u)!=0 ||
            *(int *)((uint8_t *)controller+0x84u)!=0) {
            SetLastError(ERROR_BUSY); return FALSE;
        }
        unsigned target=requested<4u?requested:locked;
        BOOL restored=r.leader_character==target;
        SudekiMpLogFormat("story_host_control event=rotation_result load=%lu task=%lu requested=%u observed=%u restored=%u native_binding_exact=1\r\n",
            (unsigned long)rotation.load,(unsigned long)rotation.task,target,r.leader_character,restored);
        if(restored) {
            if(requested<4u) { locked=requested; requested=4u; }
            else { completed_load=rotation.load; completed_task=rotation.task; }
        }
        else if(r.leader_character==rotation.before.leader_character) next_attempt=GetTickCount()+500u; /* Retail veto, coherent unchanged binding. */
        memset(&rotation,0,sizeof(rotation));
    }
    if(stopping) { requested=4u; return input_filter(w,&r,FALSE); }
    if(menu_open) {
        /* The custom menu never rotates a character or borrows a foreign
         * native modal's filter. Once acquired, its exact ownership suffices
         * to keep input closed while ordinary host world updates continue. */
        if(!input_owner.owned && !menus_clear()) { SetLastError(ERROR_BUSY); return FALSE; }
        return input_filter(w,&r,TRUE);
    }
    SudekiMpLanStoryRecruitmentStatus recruitment={0};
    BOOL explicit_request=requested<4u;
    if(explicit_request && r.leader_character==requested) { locked=requested; requested=4u; explicit_request=FALSE; }
    if((!explicit_request && !recruitment_ready(&r,&recruitment)) ||
        (next_attempt && (int32_t)(GetTickCount()-next_attempt)<0) || !switch_ready(w,&r,FALSE))
        return input_filter(w,&r,FALSE);
    if(!input_filter(w,&r,TRUE)) { SetLastError(ERROR_BUSY); return FALSE; }
    if(!switch_ready(w,&r,FALSE) || (!explicit_request && !recruitment_ready(&r,&recruitment)) || !entries_exact()) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    rotation.before=r; rotation.load=recruitment.load_generation; rotation.task=recruitment.task;
    rotation.dispatch=w->dispatch_serial; rotation.entered=TRUE;
    SudekiMpLogFormat("story_host_control event=rotation_enter load=%lu task=%lu from=%u requested=%u available=%u native_next=1\r\n",
        (unsigned long)rotation.load,(unsigned long)rotation.task,r.leader_character,explicit_request?requested:locked,r.available_mask);
    active=TRUE; call_next(r.group,base+NEXT); active=FALSE;
    /* Never use the old roster after a native rotation. Even a veto is
     * acknowledged from a fresh exact sparse observation next dispatch. */
    SetLastError(ERROR_IO_PENDING); return FALSE;
}
BOOL SudekiMpLanStoryHostControlCanSwap(void *controller,
    const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryScene *scene) {
    SudekiMpLanStoryNativeRoster r;
    return !stopping && requested>=4u && !rotation.entered && !input_owner.owned &&
        observe(controller,w,scene,&r) && r.leader_character==locked &&
        *(int *)((uint8_t *)controller+0x80u)==1 && *(int *)((uint8_t *)controller+0x84u)==1 &&
        switch_ready(w,&r,TRUE);
}
BOOL SudekiMpLanStoryHostControlSelect(void *controller,
    const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryScene *scene,unsigned target) {
    SudekiMpLanStoryNativeRoster r;
    if(target>=4u || stopping || !observe(controller,w,scene,&r) || !owner_matches(&r) ||
        !(r.available_mask&(1u<<target))) return FALSE;
    if(requested<4u) return requested==target;
    if(rotation.entered || SudekiMpLanStoryControlRetains() || !switch_ready(w,&r,FALSE)) return FALSE;
    requested=target; return TRUE;
}
BOOL SudekiMpLanStoryHostControlDrain(void *controller,
    const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryScene *scene) {
    stopping=TRUE;
    if(!rotation.entered) requested=4u;
    if(!input_owner.owned && !rotation.entered && !active) return TRUE;
    return SudekiMpLanStoryHostControlService(controller,w,scene,FALSE) &&
        !input_owner.owned && !rotation.entered;
}
BOOL SudekiMpLanStoryHostControlBound(void *controller,
    const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryScene *scene) {
    SudekiMpLanStoryNativeRoster r;
    static const char *trace; static unsigned traces;
    const char *why=stopping?"stopping":requested<4u?"requested":rotation.entered?"rotation":
        !observe(controller,w,scene,&r)?"observe":!owner_matches(&r)?"owner":
        r.leader_character!=locked?"leader":!binding_exact(w,&r,TRUE)?"binding":
        !SudekiMpLanStoryObserverRosterStillExact(w,&r)?"roster":NULL;
    if(why!=trace && traces<48u) {
        ++traces; trace=why;
        SudekiMpLogFormat("lan_story_host_control event=bound result=%s\r\n",why?why:"bound");
    }
    return why==NULL;
}
BOOL SudekiMpLanStoryHostControlReady(void *controller,
    const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryScene *scene) {
    if(!SudekiMpLanStoryHostControlBound(controller,w,scene) || input_owner.releasing) return FALSE;
    uint8_t *c=controller;
    int mode=input_owner.owned?0:1;
    return *(int *)(c+0x80u)==mode && *(int *)(c+0x84u)==mode;
}
BOOL SudekiMpLanStoryHostControlRetains(void) {
    return input_owner.owned || rotation.entered || active || requested<4u;
}
BOOL SudekiMpLanStoryHostControlUninstall(void) {
    if(!base) return TRUE;
    if(SudekiMpLanStoryHostControlRetains() || SudekiMpLanStoryControlRetains() ||
        (native_thread && native_thread!=GetCurrentThreadId())) return retain(ERROR_BUSY);
    base=NULL; native_thread=next_attempt=0; active=stopping=FALSE; locked=4u; leader_ai_exact=NULL;
    memset(verified_next,0,sizeof(verified_next)); return TRUE;
}
