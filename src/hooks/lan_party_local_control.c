#include "hooks/lan_party_local_control.h"
#include "hooks/lan_arena_campaign_guard.h"
#include "engine/build_identity.h"
#include <stdint.h>
#include <string.h>
#include <wincrypt.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "SMP4 local control requires the supported x86 ABI"
#endif

enum { AI_MODE=0xec2d0,AI_MODE_SIZE=0x78,DIRECTORY=0x409de4,
    SWITCH_SIZE=0xfcu,
    LOCAL_EMPTY=0,LOCAL_ENABLING=1,LOCAL_AI=2,LOCAL_RESTORING=3 };
typedef struct LocalLease {
    SudekiMpLanPartyLease key;
    void *actor,*group,*controller,*ai,*mode,*directory;
    unsigned phase;
} LocalLease;
static uint8_t *game_base;
static uint8_t verified_mode_code[AI_MODE_SIZE];
static uint8_t verified_switch_code[2][SWITCH_SIZE];
static const uint32_t switch_rva[2]={0x23f60u,0x24060u};
static SudekiMpLanPartyControlDrainProbe fence_probe,drain_probe;
static LocalLease lease;
static DWORD native_thread;
static struct {
    SudekiMpLanPartyRosterObservation roster;
    SudekiMpLanPartyLease key;
    unsigned target;
    BOOL pending,invoked;
} switch_owner;

static BOOL readable(const void *p,size_t bytes) {
    MEMORY_BASIC_INFORMATION region; uintptr_t start=(uintptr_t)p;
    if(!p || !bytes || start>UINTPTR_MAX-bytes ||
        VirtualQuery(p,&region,sizeof(region))!=sizeof(region) ||
        region.State!=MEM_COMMIT || (region.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
    DWORD access=region.Protect&0xffu;
    if(access!=PAGE_READONLY && access!=PAGE_READWRITE && access!=PAGE_WRITECOPY &&
        access!=PAGE_EXECUTE_READ && access!=PAGE_EXECUTE_READWRITE &&
        access!=PAGE_EXECUTE_WRITECOPY) return FALSE;
    return start+bytes<=(uintptr_t)region.BaseAddress+region.RegionSize;
}
static BOOL supported_mode_code(uint8_t *image) {
    /* Full function digest after normalizing its one ASLR operand. Cache the
     * verified live bytes for cheap exact revalidation at each native entry.
     * This records no proprietary function body in the distributed sources. */
    static const uint8_t expected[32]={
        0xb4,0x02,0x81,0x36,0xb7,0x29,0x7d,0xeb,0xd9,0x79,0xb5,0x7d,0xd7,0x8f,0x88,0xef,
        0x84,0xc8,0xc1,0x72,0x07,0xcf,0x9d,0x21,0xd0,0xed,0x72,0x72,0xad,0xae,0x7f,0xad};
    uint8_t normalized[AI_MODE_SIZE],digest[32];
    DWORD size=sizeof(digest); HCRYPTPROV provider=0; HCRYPTHASH hash=0;
    BOOL exact=FALSE;
    if(!readable(image+AI_MODE,AI_MODE_SIZE) ||
        *(void **)(image+AI_MODE+31)!=image+DIRECTORY) return FALSE;
    memcpy(normalized,image+AI_MODE,sizeof(normalized));
    uint32_t rva=DIRECTORY; memcpy(normalized+31,&rva,4);
    if(CryptAcquireContextW(&provider,NULL,NULL,PROV_RSA_AES,
            CRYPT_VERIFYCONTEXT|CRYPT_SILENT) &&
        CryptCreateHash(provider,CALG_SHA_256,0,0,&hash) &&
        CryptHashData(hash,normalized,sizeof(normalized),0) &&
        CryptGetHashParam(hash,HP_HASHVAL,digest,&size,0) && size==sizeof(digest))
        exact=memcmp(digest,expected,sizeof(digest))==0;
    if(hash) CryptDestroyHash(hash);
    if(provider) CryptReleaseContext(provider,0);
    if(exact) memcpy(verified_mode_code,image+AI_MODE,AI_MODE_SIZE);
    return exact;
}
static BOOL supported_switch_code(uint8_t *image) {
    static const uint8_t expected[2][32]={
        {0x2f,0x94,0xa4,0x74,0x38,0x48,0x54,0xe6,0xb3,0x62,0x97,0xf0,0xe2,0xe2,0x54,0x89,
         0x00,0xc8,0x8a,0xc2,0xc3,0xb4,0x93,0x1b,0x80,0x02,0x8c,0x47,0xf7,0xab,0xc6,0x9c},
        {0x6a,0x72,0x83,0xc4,0x8a,0x49,0xd5,0xde,0x30,0x23,0x7c,0xf9,0xad,0x34,0x58,0x8d,
         0xb0,0x1f,0x1f,0x4c,0x22,0x13,0x98,0x46,0xcf,0xd7,0x27,0xc9,0x15,0x4d,0x15,0x66}};
    for(unsigned i=0;i<2;++i) {
        uint8_t normalized[SWITCH_SIZE],digest[32],*code=image+switch_rva[i];
        DWORD size=sizeof(digest); HCRYPTPROV provider=0; HCRYPTHASH hash=0;
        BOOL exact=FALSE; uint32_t operand=0x408d1cu;
        if(!readable(code,sizeof(normalized)) || (code[0]!=0x55 && code[0]!=0xc3) ||
            *(void **)(code+0x5e)!=image+operand) return FALSE;
        memcpy(normalized,code,sizeof(normalized));
        normalized[0]=0x55; memcpy(normalized+0x5e,&operand,4);
        if(CryptAcquireContextW(&provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT|CRYPT_SILENT) &&
            CryptCreateHash(provider,CALG_SHA_256,0,0,&hash) &&
            CryptHashData(hash,normalized,sizeof(normalized),0) &&
            CryptGetHashParam(hash,HP_HASHVAL,digest,&size,0) && size==sizeof(digest))
            exact=memcmp(digest,expected[i],sizeof(digest))==0;
        if(hash) CryptDestroyHash(hash);
        if(provider) CryptReleaseContext(provider,0);
        if(!exact) return FALSE;
        memcpy(verified_switch_code[i],code,SWITCH_SIZE);
    }
    return TRUE;
}
static BOOL switch_code_exact(void) {
    if(!game_base) return FALSE;
    for(unsigned i=0;i<2;++i)
        if(game_base[switch_rva[i]]!=0xc3 ||
            memcmp(game_base+switch_rva[i]+1u,verified_switch_code[i]+1u,SWITCH_SIZE-1u)) return FALSE;
    return TRUE;
}
static BOOL key_equal(const SudekiMpLanPartyLease *a,const SudekiMpLanPartyLease *b) {
    return a->seat==b->seat && a->token==b->token && a->generation==b->generation;
}
static BOOL identity_equal(const LocalLease *a,const LocalLease *b) {
    return key_equal(&a->key,&b->key) && a->actor==b->actor &&
        a->group==b->group && a->controller==b->controller && a->ai==b->ai &&
        a->mode==b->mode && a->directory==b->directory;
}
static BOOL observe(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *key,void *actor,LocalLease *out) {
    SudekiMpLanPartyRosterObservation roster;
    uint8_t *ai,*mode,*controller;
    if(!game_base || !key || key->seat>=4u || !key->token || !key->generation ||
        !w || !w->service_post_original_exact || !w->service_only ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) ||
        memcmp(game_base+AI_MODE,verified_mode_code,sizeof(verified_mode_code)) ||
        !SudekiMpLanPartyControlObserveRoster(w,&roster) ||
        roster.present_mask!=15u || roster.actors[key->seat]!=actor ||
        !SudekiMpLanPartyControlNativeActorExact(&roster,key->seat) ||
        !readable(actor,0x98u)) return FALSE;
    controller=roster.controller;
    if(!readable(controller,0x24cu) ||
        *(void **)(controller+0x248u)!=actor ||
        *(void **)((uint8_t *)roster.group+0x90u)!=actor) return FALSE;
    ai=*(uint8_t **)((uint8_t *)actor+0x94u);
    if(!readable(ai,0x174u) || *(void **)(ai+0x10u)!=actor ||
        !readable(*(void **)(ai+0x16cu),0x10u) ||
        !readable(*(void **)(ai+0x170u),0x10u)) return FALSE;
    mode=*(uint8_t **)(ai+0x3cu);
    if(!readable(mode,0x0cu) || mode[0x0bu]>1u ||
        *(int16_t *)(ai+0x16au)<0 ||
        !readable(*(void **)(game_base+DIRECTORY),0x14u)) return FALSE;
    memset(out,0,sizeof(*out)); out->key=*key;
    out->actor=actor; out->ai=ai; out->mode=mode;
    out->group=roster.group; out->controller=controller;
    out->directory=*(void **)(game_base+DIRECTORY);
    return !native_thread || native_thread==GetCurrentThreadId();
}
/* Native mode transition: ECX=exact AI component, AL=enabled, no stack args.
 * It executes native behavior exit/reset; it does not change override refs,
 * controller identities, group ordering, or camera targets. */
__attribute__((naked,noinline,used))
static void call_mode(void *function __attribute__((unused)),
    void *ai __attribute__((unused)),unsigned enabled __attribute__((unused))) {
    __asm__ volatile("movl 8(%esp),%ecx\n\tmovl 12(%esp),%eax\n\t"
        "call *4(%esp)\n\tret\n\t");
}
BOOL SudekiMpLanPartyLocalControlInstall(HMODULE image,
    SudekiMpLanPartyControlDrainProbe input_fenced,
    SudekiMpLanPartyControlDrainProbe actions_drained) {
    if(game_base || !image || !input_fenced || !actions_drained ||
        !SudekiMpCheckLoadedExecutable(image) ||
        !supported_mode_code((uint8_t *)image) || !supported_switch_code((uint8_t *)image)) {
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    game_base=(uint8_t *)image;
    fence_probe=input_fenced; drain_probe=actions_drained;
    native_thread=0;
    return TRUE;
}
BOOL SudekiMpLanPartyLocalControlAiExact(
    const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *key,void *actor) {
    LocalLease observed;
    return lease.phase==LOCAL_AI && observe(w,key,actor,&observed) &&
        identity_equal(&lease,&observed) &&
        *(int16_t *)((uint8_t *)lease.ai+0x16au)==0 &&
        ((uint8_t *)lease.mode)[0x0bu]==1u && fence_probe(key,actor,w) &&
        SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w);
}
BOOL SudekiMpLanPartyLocalControlRetains(void) {
    return lease.phase!=LOCAL_EMPTY;
}
BOOL SudekiMpLanPartyLocalControlSwitchPending(void) {
    return switch_owner.pending;
}
BOOL SudekiMpLanPartyLocalControlSetAi(
    const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *key,void *actor,BOOL enabled) {
    LocalLease observed,fresh;
    if(switch_owner.pending || !observe(w,key,actor,&observed) ||
        (lease.phase!=LOCAL_EMPTY && !identity_equal(&lease,&observed)) ||
        !fence_probe(key,actor,w)) { SetLastError(ERROR_BUSY); return FALSE; }
    if(lease.phase==LOCAL_EMPTY && !enabled) return TRUE;
    int16_t refs=*(int16_t *)((uint8_t *)observed.ai+0x16au);
    uint8_t mode=((uint8_t *)observed.mode)[0x0bu];
    if(lease.phase==LOCAL_ENABLING) {
        if(refs!=0 || mode!=1) { SetLastError(ERROR_BUSY); return FALSE; }
        lease.phase=LOCAL_AI;
    } else if(lease.phase==LOCAL_RESTORING) {
        if(refs!=0 || mode!=0) { SetLastError(ERROR_BUSY); return FALSE; }
        memset(&lease,0,sizeof(lease));
        if(!enabled) return TRUE;
    }
    if(enabled && lease.phase==LOCAL_AI && refs==0 && mode==1) return TRUE;
    if(refs!=0 || !drain_probe(key,actor,w) ||
        !observe(w,key,actor,&fresh) || !identity_equal(&observed,&fresh) ||
        *(int16_t *)((uint8_t *)fresh.ai+0x16au)!=0 ||
        ((uint8_t *)fresh.mode)[0x0bu]!=mode || !fence_probe(key,actor,w)) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    if(enabled) {
        /* Already-active AI without our lease belongs to another owner. */
        if(lease.phase==LOCAL_EMPTY && mode!=0) {
            SetLastError(ERROR_BUSY); return FALSE;
        }
        lease=fresh; lease.phase=LOCAL_ENABLING;
    } else {
        if(lease.phase!=LOCAL_AI) { SetLastError(ERROR_BUSY); return FALSE; }
        lease.phase=LOCAL_RESTORING;
    }
    native_thread=GetCurrentThreadId();
    call_mode(game_base+AI_MODE,lease.ai,enabled!=FALSE);
    if(!observe(w,key,actor,&fresh) || !identity_equal(&lease,&fresh) ||
        *(int16_t *)((uint8_t *)fresh.ai+0x16au)!=0 ||
        ((uint8_t *)fresh.mode)[0x0bu]!=(enabled!=FALSE) ||
        !fence_probe(key,actor,w)) { SetLastError(ERROR_BUSY); return FALSE; }
    if(enabled) lease.phase=LOCAL_AI;
    else memset(&lease,0,sizeof(lease));
    SetLastError(ERROR_SUCCESS); return TRUE;
}
BOOL SudekiMpLanPartyLocalControlUninstall(void) {
    if(lease.phase!=LOCAL_EMPTY || switch_owner.pending) { SetLastError(ERROR_BUSY); return FALSE; }
    game_base=NULL; fence_probe=drain_probe=NULL; native_thread=0;
    memset(verified_mode_code,0,sizeof(verified_mode_code));
    memset(verified_switch_code,0,sizeof(verified_switch_code));
    return TRUE;
}

static BOOL switch_eligible(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *key,const SudekiMpLanPartyRosterObservation *r) {
    uint8_t *g=r->group,*c=r->controller,*ui,*quit,*quick;
    if(!SudekiMpLanPartyControlLocalSwitchReady(w,r) ||
        !SudekiMpLanPartyControlGameplayReady(w) || !readable(g,0xd8u) ||
        *(unsigned *)(g+0xd0u) || !g[0xd6] || g[0xd7] || *(void **)(g+0xc0u) ||
        !readable(c,0x24cu) || *(unsigned *)(c+0xf4u) || *(unsigned *)(c+0xfcu) ||
        !readable(game_base+0x408db8u,4u) || *(void **)(game_base+0x408db8u) ||
        !readable(game_base+0x408d3cu,4u) || !readable(game_base+0x408d68u,4u) ||
        !readable(game_base+0x3c2f84u,4u)) return FALSE;
    ui=*(uint8_t **)(game_base+0x408d3cu);
    quit=*(uint8_t **)(game_base+0x408d68u);
    quick=*(uint8_t **)(game_base+0x3c2f84u);
    if((ui && (!readable(ui,0x47eu) || ui[0x47d])) ||
        !readable(quit,0x1c3u) || quit[0x1c2] ||
        !readable(quick,0x2au) || quick[0x29]) return FALSE;
    for(unsigned i=0;i<4;++i) {
        SudekiMpLanPartyLease actor_key=*key; actor_key.seat=(uint8_t)i;
        uint8_t *a=r->actors[i],*arbiter,*ai,*mode;
        if(!readable(a,0x98u) || !drain_probe(&actor_key,a,w)) return FALSE;
        arbiter=*(uint8_t **)(a+0x90u); ai=*(uint8_t **)(a+0x94u);
        if(!readable(arbiter,0x64u) || *(void **)(arbiter+0x10u)!=a ||
            (*(uint32_t *)(arbiter+0x50u)&0x081802c0u) ||
            (*(uint32_t *)(arbiter+0x60u)&1u) ||
            !readable(ai,0x174u) || *(void **)(ai+0x10u)!=a ||
            *(int16_t *)(ai+0x16au)!=0) return FALSE;
        mode=*(uint8_t **)(ai+0x3cu);
        if(!readable(mode,0xcu) || mode[0xbu]!=(i==key->seat?0u:1u)) return FALSE;
    }
    return SudekiMpLanPartyControlLocalSwitchReady(w,r) &&
        fence_probe(key,r->actors[key->seat],w);
}
BOOL SudekiMpLanPartyLocalControlSwitchStep(
    const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *key,unsigned target,unsigned *observed_character) {
    SudekiMpLanPartyRosterObservation r; LocalLease local;
    if(observed_character) *observed_character=4u;
    if(!game_base || !key || !observed_character || target>=4u || lease.phase!=LOCAL_EMPTY ||
        !w || !w->service_post_original_exact || !w->service_only ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) || !switch_code_exact()) return FALSE;
    if(!switch_owner.pending) {
        if(!SudekiMpLanPartyControlObserveRoster(w,&r) || key->seat>=4u ||
            !observe(w,key,r.actors[key->seat],&local) ||
            !switch_eligible(w,key,&r)) { SetLastError(ERROR_BUSY); return FALSE; }
        if(target==key->seat) { *observed_character=target; return TRUE; }
        unsigned slot=0;
        for(;slot<4u && *(void **)((uint8_t *)r.group+0x90u+12u*slot)!=r.actors[target];++slot) {}
        if(slot==4u) return FALSE;
        switch_owner.roster=r; switch_owner.key=*key; switch_owner.target=target;
        switch_owner.pending=TRUE; switch_owner.invoked=FALSE;
        native_thread=GetCurrentThreadId();
        switch_owner.invoked=SudekiMpLanArenaCampaignGuardSwitchCharacter(r.group,slot>2u);
    } else if(!key_equal(key,&switch_owner.key) || switch_owner.target!=target) return FALSE;
    r=switch_owner.roster;
    if(!switch_owner.invoked || !readable(r.controller,0x24cu) || !readable(r.group,0xd0u)) return FALSE;
    void *actor=*(void **)((uint8_t *)r.controller+0x248u);
    unsigned selected=0;
    for(;selected<4u && r.actors[selected]!=actor;++selected) {}
    if(selected>=4u || *(void **)((uint8_t *)r.group+0x90u)!=actor ||
        !SudekiMpLanPartyControlRebindLocal(w,&r,selected)) return FALSE;
    unsigned previous=switch_owner.key.seat;
    memset(&switch_owner,0,sizeof(switch_owner));
    *observed_character=selected;
    SetLastError(selected==previous?ERROR_BUSY:ERROR_SUCCESS);
    return selected!=previous;
}
