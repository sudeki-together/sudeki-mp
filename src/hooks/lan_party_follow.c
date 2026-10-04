#include "hooks/lan_party_follow.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/party_follow.h"
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <wincrypt.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Native party follow requires the supported x86 ABI"
#endif
/* Only Formation / IdleFormation consumers are patched. Combat selection,
 * path finding, collision, task arbitration and intrusive formation refs stay
 * native. The retained pointers below are comparison identities; every entry
 * revalidates them against the live canonical roster on its native thread. */
static uint8_t *image;
static DWORD native_thread;
static volatile LONG callbacks,ready;
static SudekiMpLanPartyRosterObservation roster;
static uint8_t humans,managed;
static unsigned targets[4];
static void *components[4],*transforms[4],*manager,*formation;
static SudekiMpInlineHook leaders[4];
static SudekiMpRelativeCallHook calls[6];
static void *leader_yes[4] __attribute__((used));
static void *leader_no[4] __attribute__((used));
static void *native_goal __attribute__((used));
static void *native_distance __attribute__((used));
static void *native_facing __attribute__((used));

typedef struct CodeIdentity {
    uint32_t rva,size;
    uint8_t digest[32];
    unsigned reloc_count;
    struct {uint32_t offset,target;} reloc[9];
} CodeIdentity;
/* SHA256 of complete native routines after verified ASLR operands are
 * normalized to RVAs. No executable function bodies are distributed. */
static const CodeIdentity identities[]={
    {0x1ada90,0x8a, {0x8b,0x09,0xbd,0x91,0x7c,0x11,0x77,0x79,0xa6,0xc5,0x1a,0x47,0xea,0x17,0xcd,0x20,0xec,0xdd,0xc6,0x54,0xb2,0x2c,0xd2,0xc1,0x80,0x74,0x53,0x13,0x16,0xfd,0x99,0x7c}, 0, {{0,0}}},
    {0x1a8e00,0x8a, {0x32,0x07,0xbe,0x24,0x5a,0xc1,0xa2,0xec,0xef,0x38,0xc5,0xdb,0xbc,0x51,0x85,0xa7,0x22,0x4a,0x5c,0x29,0x30,0x1d,0xe9,0x5a,0x4d,0x8d,0xa7,0x6d,0x2c,0x29,0x8d,0xaa}, 0, {{0,0}}},
    {0x1adb40,0x420, {0xdf,0x6d,0xff,0xed,0x22,0xce,0xe0,0xb7,0x92,0x49,0x34,0x19,0x8f,0xb9,0x58,0x62,0xe5,0x2e,0x8f,0xc0,0xbb,0x31,0xbb,0x5b,0xb1,0xc1,0x07,0xc0,0xb9,0x42,0x2f,0x83}, 9, {{0x20d,0x1adf44},{0x270,0x2e3c50},{0x29c,0x2c3b84},{0x35c,0x2c02e0},{0x391,0x2e35cc},{0x404,0x1ade52},{0x408,0x1adda7},{0x40c,0x1add51},{0x410,0x1adeca}}},
    {0x1a8ed0,0x410, {0xa8,0xe1,0xbc,0x7a,0x2c,0x2a,0x0c,0x7a,0x59,0x3a,0x16,0x0e,0xb3,0xaa,0x3d,0xda,0x32,0x31,0x40,0x50,0x8b,0x5b,0xca,0xfd,0x49,0x5f,0xde,0x06,0x3d,0x4a,0xdb,0x65}, 9, {{0x206,0x1a92cc},{0x269,0x2e3c50},{0x295,0x2c3b84},{0x355,0x2c02e0},{0x38a,0x2e35cc},{0x3fc,0x1a91db},{0x400,0x1a9130},{0x404,0x1a90da},{0x408,0x1a9253}}},
    {0xb2b10,0xc0, {0x91,0xaf,0xfb,0xb0,0x1c,0x87,0x5f,0x93,0x81,0x23,0x46,0x2c,0xd2,0xb7,0xec,0xcb,0x91,0xc9,0xc6,0x35,0x15,0x87,0xe9,0x0f,0x6c,0x5e,0xbd,0x40,0xb0,0x22,0xbc,0xe2}, 1, {{0x8e,0x2e3640}}},
    {0xb2bd0,0x60, {0x10,0x45,0x8c,0xde,0x6a,0x4e,0xcf,0x04,0xc3,0x98,0x7f,0x45,0xdf,0xbb,0x36,0xd7,0xce,0xf1,0x4b,0x33,0x96,0x7a,0xce,0xfd,0xc8,0x28,0xdc,0xa3,0x4d,0x34,0xfa,0xbc}, 1, {{0x37,0x33e044}}},
    {0xb32c0,0xb0, {0xdf,0x90,0xad,0xd3,0x3e,0x70,0x28,0x7c,0x88,0x56,0xe7,0x5b,0xbb,0x16,0x99,0x6c,0x28,0x0c,0x6f,0x93,0x46,0x2e,0x9d,0x73,0xaf,0xb9,0xb2,0x86,0xba,0x6b,0xe9,0x49}, 0, {{0,0}}},
    {0xb3370,0x320, {0x1c,0x06,0x39,0x30,0x6c,0x08,0x34,0x87,0x87,0x8e,0xb9,0xd2,0x5d,0xa9,0x0a,0x75,0x6f,0xc8,0xc9,0x9c,0x2a,0xf2,0x93,0x15,0xe1,0x7b,0xc1,0xfa,0xae,0x90,0x78,0xa0}, 9, {{0xab,0x33e044},{0xf0,0x2e3ae0},{0x14b,0x2e3ad8},{0x179,0x2e3958},{0x19f,0x2e39f8},{0x1ee,0x33e044},{0x22d,0x2e3ad0},{0x2ac,0x33e044},{0x2e9,0x2e3788}}},
    {0xf5280,0xd0, {0x95,0x27,0x44,0xf8,0x4a,0x83,0xa6,0x70,0x1e,0xa2,0xb8,0x42,0xb0,0x7a,0x0b,0xb5,0x7a,0xb2,0x89,0xff,0x84,0x6f,0x03,0x7c,0xaa,0x86,0x2a,0x47,0xa0,0x71,0x31,0x72}, 8, {{0x8,0x2e3780},{0x26,0x338bb8},{0x38,0x2e36cc},{0x47,0x2e36c8},{0x5c,0x2e36c0},{0x6b,0x2e36b8},{0x73,0x2e36b0},{0x80,0x2e36a8}}},
};
static uint8_t verified[9][0x420];
static BOOL readable(const void *p,size_t size) {
    MEMORY_BASIC_INFORMATION m; uintptr_t start=(uintptr_t)p;
    if(!p || !size || start>UINTPTR_MAX-size ||
        VirtualQuery(p,&m,sizeof(m))!=sizeof(m) || m.State!=MEM_COMMIT ||
        (m.Protect&(PAGE_NOACCESS|PAGE_GUARD))) return FALSE;
    DWORD a=m.Protect&0xffu;
    if(a!=PAGE_READONLY && a!=PAGE_READWRITE && a!=PAGE_WRITECOPY &&
        a!=PAGE_EXECUTE_READ && a!=PAGE_EXECUTE_READWRITE && a!=PAGE_EXECUTE_WRITECOPY) return FALSE;
    return start+size<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL signatures(uint8_t *base) {
    HCRYPTPROV provider=0; BOOL ok=FALSE;
    if(!CryptAcquireContextW(&provider,NULL,NULL,PROV_RSA_AES,
        CRYPT_VERIFYCONTEXT|CRYPT_SILENT)) return FALSE;
    for(unsigned i=0;i<9;++i) {
        const CodeIdentity *id=&identities[i]; uint8_t normalized[0x420],hash[32];
        HCRYPTHASH h=0; DWORD length=sizeof(hash);
        if(!readable(base+id->rva,id->size)) goto done;
        memcpy(normalized,base+id->rva,id->size);
        for(unsigned r=0;r<id->reloc_count;++r) {
            uint32_t value; memcpy(&value,normalized+id->reloc[r].offset,4);
            if(value!=(uint32_t)(uintptr_t)(base+id->reloc[r].target)) goto done;
            memcpy(normalized+id->reloc[r].offset,&id->reloc[r].target,4);
        }
        BOOL exact=CryptCreateHash(provider,CALG_SHA_256,0,0,&h) &&
            CryptHashData(h,normalized,id->size,0) &&
            CryptGetHashParam(h,HP_HASHVAL,hash,&length,0) &&
            length==sizeof(hash) && !memcmp(hash,id->digest,sizeof(hash));
        if(h) CryptDestroyHash(h);
        if(!exact) goto done;
        memcpy(verified[i],base+id->rva,id->size);
    }
    ok=TRUE;
done:
    CryptReleaseContext(provider,0); return ok;
}
static BOOL code_exact(void) {
    if(!image) return FALSE;
    for(unsigned i=0;i<9;++i)
        if(memcmp(image+identities[i].rva,verified[i],identities[i].size)) return FALSE;
    return TRUE;
}
static BOOL position(unsigned c,float out[3]) {
    uint8_t *actor=roster.actors[c],*p;
    if(!readable(actor,0x98) || (p=*(uint8_t **)(actor+0x44))!=transforms[c] ||
        !readable(p,0x5c) || *(void **)p!=image+0x2cdefc ||
        *(void **)(p+0x10)!=actor) return FALSE;
    for(unsigned a=0;a<3;++a) {
        out[a]=*(float *)(p+0x18+4*a);
        if(!isfinite(out[a]) || fabsf(out[a])>=1000000.f) return FALSE;
    }
    return TRUE;
}
static BOOL actor_exact(unsigned c,BOOL human) {
    uint8_t *actor=roster.actors[c],*ai,*mode;
    if(!readable(actor,0x98) || (ai=*(uint8_t **)(actor+0x94))!=components[c] ||
        !readable(ai,0x16c) || *(void **)ai!=image+0x2d4924 ||
        *(void **)(ai+0x10)!=actor || *(void **)(ai+0x40)!=formation ||
        !readable(mode=*(uint8_t **)(ai+0x3c),0xc) ||
        mode[0xb]!=(human?0u:1u) || *(int16_t *)(ai+0x16a)<0 ||
        (!human && *(int16_t *)(ai+0x16a)!=0)) return FALSE;
    return TRUE;
}
static BOOL formation_exact(void) {
    if(!image || *(void **)(image+0x409de4)!=manager ||
        !readable(manager,0x148) || formation!=(uint8_t *)manager+0xf4 ||
        !readable(formation,0x54) || *(unsigned *)((uint8_t *)formation+0x30)!=4 ||
        ((uint8_t *)formation)[0x50]) return FALSE;
    unsigned seen=0;
    for(unsigned i=0;i<4;++i) {
        void *actor=*(void **)((uint8_t *)formation+12*i); unsigned c;
        for(c=0;c<4;++c) if(actor==roster.actors[c]) break;
        if(c==4 || (seen&(1u<<c))) return FALSE;
        seen|=1u<<c;
    }
    return seen==15;
}
static BOOL selection(void *ai,unsigned *source,unsigned *target) {
    if(!InterlockedCompareExchange(&ready,0,0) ||
        GetCurrentThreadId()!=native_thread ||
        !SudekiMpLanPartyControlNativeRosterExact(&roster) || !formation_exact()) return FALSE;
    unsigned c;
    for(c=0;c<4;++c) if(components[c]==ai) break;
    if(c==4 || !(managed&(1u<<c)) || !actor_exact(c,FALSE)) return FALSE;
    unsigned t=targets[c];
    if(t<4 && (!(humans&(1u<<t)) || !actor_exact(t,TRUE))) return FALSE;
    *source=c; *target=t; return TRUE;
}
static void *__attribute__((noinline,used)) choose_leader(void *ai,void *original) {
    unsigned source,target; InterlockedIncrement(&callbacks);
    void *chosen=selection(ai,&source,&target)?
        (target<4?roster.actors[target]:NULL):original;
    InterlockedDecrement(&callbacks); return chosen;
}
/* Score has AI in ESI; Update has AI in ECX. Save the native x87 stack as
 * well as all registers: updates can enter this seam with live FP temporaries.
 * The displaced conditional branch is replayed explicitly, never through the
 * inline hook's unrelocated short-branch trampoline. */
#define LEADER_ENTRY(name,index,source_offset) \
__attribute__((naked,noinline,used)) static void name(void) { \
 __asm__ volatile("pushfl\n\tpushal\n\tsubl $108,%esp\n\tfnsave (%esp)\n\t" \
 "movl 136(%esp),%eax\n\tpushl (%eax)\n\t" \
 "pushl " #source_offset "(%esp)\n\tcall _choose_leader\n\taddl $8,%esp\n\t" \
 "movl %eax,136(%esp)\n\tfrstor (%esp)\n\taddl $108,%esp\n\tpopal\n\tpopfl\n\t" \
 "testl %eax,%eax\n\tjz 1f\n\tjmp *_leader_yes+" #index "\n\t" \
 "1: jmp *_leader_no+" #index "\n\t"); }
LEADER_ENTRY(score_formation,0,116)
LEADER_ENTRY(score_idle,4,116)
LEADER_ENTRY(update_formation,8,136)
LEADER_ENTRY(update_idle,12,136)
#undef LEADER_ENTRY
/* Exact native helpers keep their original register/stack calling contracts. */
__attribute__((naked,noinline,used)) static float *call_goal(
    void *f __attribute__((unused)),float *out __attribute__((unused)),
    void *actor __attribute__((unused))) {
    __asm__ volatile("pushl %edi\n\tmovl 8(%esp),%edi\n\t"
        "pushl 16(%esp)\n\tpushl 16(%esp)\n\tcall *_native_goal\n\tpopl %edi\n\tret\n\t");
}
__attribute__((naked,noinline,used)) static float call_distance(
    void *f __attribute__((unused)),void *actor __attribute__((unused))) {
    __asm__ volatile("movl 4(%esp),%eax\n\tpushl 8(%esp)\n\t"
        "call *_native_distance\n\tret\n\t");
}
static BOOL forward(unsigned c,float out[2]) {
    float xyz[3]; if(!position(c,xyz)) return FALSE;
    uint8_t *p=transforms[c];
    float x=*(float *)(p+0x50),z=*(float *)(p+0x58),length=x*x+z*z;
    if(!isfinite(length) || length<0.0001f || length>100.f) return FALSE;
    length=sqrtf(length); out[0]=x/length; out[1]=z/length; return TRUE;
}
static BOOL remapped_goal(void *f,void *actor,float out[3]) {
    unsigned c,t;
    for(c=0;c<4;++c) if(roster.actors[c]==actor) break;
    if(c==4 || !selection(components[c],&c,&t) || f!=formation) return FALSE;
    float self[3]; if(!position(c,self)) return FALSE;
    if(t==4) { memcpy(out,self,sizeof(self)); return TRUE; }
    unsigned old;
    for(old=0;old<4;++old) if(roster.actors[old]==*(void **)f) break;
    /* Preserve native spacing and smoothed formation heading bit-for-bit
     * whenever native already follows the selected human. */
    if(old==t) return FALSE;
    float before[3],after[3],facing[2];
    if(old==4 || !position(old,before) || !position(t,after) || !forward(t,facing)) return FALSE;
    /* A former native leader now following a human borrows the human's
     * vacated formation slot. Other AI retain their native distinct slot. */
    if(!call_goal(f,out,old==c?roster.actors[t]:actor)) return FALSE;
    for(unsigned a=0;a<3;++a) if(!isfinite(out[a])) return FALSE;
    float x=out[0]-before[0],z=out[2]-before[2];
    float fx=*(float *)((uint8_t *)f+0x3c),fz=*(float *)((uint8_t *)f+0x40);
    float norm=fx*fx+fz*fz;
    if(!isfinite(norm) || norm<0.0001f || norm>100.f) return FALSE;
    norm=sqrtf(norm); fx/=norm; fz/=norm;
    float longitudinal=x*fx+z*fz,side=x*fz-z*fx;
    out[0]=after[0]+longitudinal*facing[0]+side*facing[1];
    out[1]+=after[1]-before[1];
    out[2]=after[2]+longitudinal*facing[1]-side*facing[0];
    return isfinite(out[0]) && isfinite(out[1]) && isfinite(out[2]);
}
static float *__attribute__((stdcall,noinline,used)) goal(void *f,float *out,void *actor) {
    InterlockedIncrement(&callbacks);
    if(!remapped_goal(f,actor,out)) call_goal(f,out,actor);
    InterlockedDecrement(&callbacks); return out;
}
static float __attribute__((stdcall,noinline,used)) distance(void *f,void *actor) {
    float result,out[3],self[3]; unsigned c;
    InterlockedIncrement(&callbacks);
    for(c=0;c<4;++c) if(roster.actors[c]==actor) break;
    float radius=*(float *)((uint8_t *)f+0x38)+*(float *)(image+0x33e044);
    if(c<4 && isfinite(radius) && radius>0.0001f &&
        remapped_goal(f,actor,out) && position(c,self)) {
        float x=out[0]-self[0],z=out[2]-self[2]; result=(x*x+z*z)/(radius*radius);
    } else result=call_distance(f,actor);
    InterlockedDecrement(&callbacks); return result;
}
static void __attribute__((noinline,used)) facing(void *ai,float *out) {
    unsigned c,t; float dir[2]; InterlockedIncrement(&callbacks);
    if(selection(ai,&c,&t) && t<4 && roster.actors[t]!=*(void **)formation && forward(t,dir)) {
        out[0]=dir[0];out[1]=0;out[2]=dir[1];
    }
    InterlockedDecrement(&callbacks);
}
__attribute__((naked,noinline,used)) static void goal_entry(void) {
    __asm__ volatile("pushl 8(%esp)\n\tpushl 8(%esp)\n\tpushl %edi\n\t"
        "call _goal@12\n\tret $8\n\t");
}
__attribute__((naked,noinline,used)) static void distance_entry(void) {
    __asm__ volatile("pushl 4(%esp)\n\tpushl %eax\n\tcall _distance@8\n\tret $4\n\t");
}
__attribute__((naked,noinline,used)) static void facing_entry(void) {
    __asm__ volatile("pushfl\n\tpushal\n\tsubl $108,%esp\n\tfnsave (%esp)\n\t"
        "pushl 136(%esp)\n\tpushl 12(%ebp)\n\tcall _facing\n\taddl $8,%esp\n\t"
        "frstor (%esp)\n\taddl $108,%esp\n\tpopal\n\tpopfl\n\tjmp *_native_facing\n\t");
}
void SudekiMpLanPartyFollowClear(void) { InterlockedExchange(&ready,0); }
BOOL SudekiMpLanPartyFollowPublish(const SudekiMpControlUpdateDispatchWitness *w,
    uint8_t active,unsigned host) {
    SudekiMpLanPartyRosterObservation current; float positions[4][3];
    SudekiMpLanPartyFollowClear();
    if(!image || active>15 || host>4 || !w || !w->service_post_original_exact ||
        !w->service_only || !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) ||
        (native_thread && native_thread!=GetCurrentThreadId()) || !code_exact() ||
        !SudekiMpLanPartyControlObserveRoster(w,&current) || current.present_mask!=15) return FALSE;
    roster=current; manager=*(void **)(image+0x409de4);
    formation=manager?(uint8_t *)manager+0xf4:NULL; humans=active; managed=0;
    if(!formation_exact()) return FALSE;
    for(unsigned c=0;c<4;++c) {
        uint8_t *actor=current.actors[c];
        if(!readable(actor,0x98)) return FALSE;
        components[c]=*(void **)(actor+0x94); transforms[c]=*(void **)(actor+0x44);
        if(!position(c,positions[c])) return FALSE;
        if(active&(1u<<c)) { if(!actor_exact(c,TRUE)) return FALSE; }
        else if(actor_exact(c,FALSE)) managed|=(uint8_t)(1u<<c);
    }
    for(unsigned c=0;c<4;++c)
        targets[c]=SudekiMpPartyFollowTarget(c,host,15,humans,(const float (*)[3])positions);
    native_thread=GetCurrentThreadId(); InterlockedExchange(&ready,1); return TRUE;
}
static BOOL retain(DWORD error) {
    HMODULE module;
    (void)GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        (LPCSTR)(uintptr_t)&SudekiMpLanPartyFollowUninstall,&module);
    SetLastError(error?error:ERROR_BUSY); return FALSE;
}
BOOL SudekiMpLanPartyFollowUninstall(void) {
    SudekiMpLanPartyFollowClear();
    if(InterlockedCompareExchange(&callbacks,0,0)) return retain(ERROR_BUSY);
    BOOL restored=TRUE; DWORD error=0;
    for(unsigned i=6;i--;) if(!SudekiMpRestoreRelativeCallHook(&calls[i])) {
        restored=FALSE; if(!error) error=GetLastError();
    }
    for(unsigned i=4;i--;) if(!SudekiMpRestoreInlineHook(&leaders[i])) {
        restored=FALSE; if(!error) error=GetLastError();
    }
    if(!restored || InterlockedCompareExchange(&callbacks,0,0)) return retain(error);
    /* Immutable native continuations remain valid for a fetched old entry. */
    native_thread=0; SetLastError(ERROR_SUCCESS); return TRUE;
}
BOOL SudekiMpLanPartyFollowInstall(HMODULE base) {
    static const uint32_t sites[4]={0x1ada9c,0x1a8e0c,0x1adb7d,0x1a8f06};
    static const uint32_t yes[4]={0x1adaa2,0x1a8e12,0x1adb83,0x1a8f0c};
    static const uint32_t no[4]={0x1adb14,0x1a8e84,0x1adb8f,0x1a8f18};
    static const uint32_t callsites[6]={0x1adc23,0x1a8fac,0x1adc60,0x1a8fe9,0x1add81,0x1a910a};
    const void *entries[4]={(void *)(uintptr_t)score_formation,(void *)(uintptr_t)score_idle,
        (void *)(uintptr_t)update_formation,(void *)(uintptr_t)update_idle};
    const void *replacements[6]={(void *)(uintptr_t)goal_entry,(void *)(uintptr_t)goal_entry,
        (void *)(uintptr_t)distance_entry,(void *)(uintptr_t)distance_entry,
        (void *)(uintptr_t)facing_entry,(void *)(uintptr_t)facing_entry};
    for(unsigned i=0;i<4;++i) if(leaders[i].installed) return retain(ERROR_ALREADY_EXISTS);
    for(unsigned i=0;i<6;++i) if(calls[i].installed) return retain(ERROR_ALREADY_EXISTS);
    if(!base || !SudekiMpCheckLoadedExecutable(base) || !signatures((uint8_t *)base)) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    image=(uint8_t *)base; native_thread=0; SudekiMpLanPartyFollowClear();
    native_goal=image+0xb2b10; native_distance=image+0xb2bd0; native_facing=image+0xf5280;
    for(unsigned i=0;i<4;++i) { leader_yes[i]=image+yes[i]; leader_no[i]=image+no[i]; }
    for(unsigned i=0;i<4;++i) {
        uint8_t expected[6]; memcpy(expected,image+sites[i],6);
        if(!SudekiMpInstallInlineHook(&leaders[i],image+sites[i],expected,6,entries[i])) goto fail;
    }
    for(unsigned i=0;i<6;++i) if(!SudekiMpInstallRelativeCallHook(&calls[i],image+callsites[i],
        i<2?native_goal:i<4?native_distance:native_facing,replacements[i])) goto fail;
    for(unsigned i=0;i<9;++i) memcpy(verified[i],image+identities[i].rva,identities[i].size);
    return TRUE;
fail: {
    DWORD error=GetLastError(); if(!SudekiMpLanPartyFollowUninstall()) return FALSE;
    SetLastError(error); return FALSE;
}}
#ifdef SUDEKIMP_LAN_PARTY_FOLLOW_TESTING
void *SudekiMpLanPartyFollowTestLeader(void *ai,void *native) {return choose_leader(ai,native);}
float *SudekiMpLanPartyFollowTestGoal(void *f,float *out,void *actor) {return goal(f,out,actor);}
float SudekiMpLanPartyFollowTestDistance(void *f,void *actor) {return distance(f,actor);}
void SudekiMpLanPartyFollowTestFacing(void *ai,float out[3]) {facing(ai,out);}
#endif
