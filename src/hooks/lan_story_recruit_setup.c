#include "hooks/lan_story_recruit_setup.h"
#include "engine/build_identity.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

#if !defined(__i386__)
#error "Lighthouse recruitment setup requires the supported x86 image"
#endif

typedef struct CodeIdentity { unsigned rva,size; uint32_t hash; } CodeIdentity;
typedef struct Relocation { unsigned rva,target; } Relocation;
static const CodeIdentity codes[]={
    {0x0d9bf0u,76u,0x39b9917du},
    {0x0d9d00u,10u,0xab205b63u},
    {0x0da420u,396u,0xdfaaaa6fu},
};
static const Relocation relocations[]={
    {0x0d9bffu,0x408d94u}, {0x0da433u,0x33a3d0u},
    {0x0da46eu,0x3d27c4u}, {0x0da489u,0x3d27c8u},
    {0x0da4a4u,0x0da59cu}, {0x0da4abu,0x0da58cu},
    {0x0da58cu,0x0da4afu}, {0x0da590u,0x0da4b6u},
    {0x0da594u,0x0da4bdu}, {0x0da598u,0x0da4c1u},
};
enum { STEP_XP=1u, STEP_SKILL=2u, STEP_SP=4u, ALL_STEPS=7u };
static const char skill_name[]="AbilityBonusSkill2Alice";
static const char bonus_name[]="AbilityBonusSPAlice";
static uint8_t *verified_image;
static DWORD native_thread;
static LONG active;

static BOOL memory(const void *pointer,size_t size,BOOL write) {
    MEMORY_BASIC_INFORMATION m; uintptr_t p=(uintptr_t)pointer;
    if(!pointer || !size || p>UINTPTR_MAX-size ||
        VirtualQuery(pointer,&m,sizeof(m))!=sizeof(m) || m.State!=MEM_COMMIT ||
        (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
    DWORD access=m.Protect&0xffu;
    if(write) {
        if(access!=PAGE_READWRITE && access!=PAGE_WRITECOPY &&
            access!=PAGE_EXECUTE_READWRITE && access!=PAGE_EXECUTE_WRITECOPY) return FALSE;
    } else if(access!=PAGE_READONLY && access!=PAGE_READWRITE && access!=PAGE_WRITECOPY &&
        access!=PAGE_EXECUTE_READ && access!=PAGE_EXECUTE_READWRITE &&
        access!=PAGE_EXECUTE_WRITECOPY) return FALSE;
    return p+size<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL readable(const void *p,size_t n) { return memory(p,n,FALSE); }
static BOOL writable(const void *p,size_t n) { return memory(p,n,TRUE); }
static float number(const void *p) { float v; memcpy(&v,p,4u); return v; }
static int16_t small(const void *p) { int16_t v; memcpy(&v,p,2u); return v; }
static BOOL code_exact(const CodeIdentity *code) {
    if(!readable(verified_image+code->rva,code->size)) return FALSE;
    uint32_t hash=2166136261u;
    for(unsigned i=0;i<code->size;++i) {
        uint8_t byte=verified_image[code->rva+i];
        for(unsigned r=0;r<sizeof(relocations)/sizeof(relocations[0]);++r) {
            unsigned offset=code->rva+i-relocations[r].rva;
            if(offset>=4u) continue;
            uint32_t value; memcpy(&value,verified_image+relocations[r].rva,4u);
            if(value!=(uintptr_t)verified_image+relocations[r].target) return FALSE;
            byte=(uint8_t)((0x400000u+relocations[r].target)>>(offset*8u)); break;
        }
        hash=(hash^byte)*16777619u;
    }
    return hash==code->hash;
}
static BOOL methods_exact(void) {
    if(!verified_image) return FALSE;
    for(unsigned i=0;i<sizeof(codes)/sizeof(codes[0]);++i)
        if(!code_exact(&codes[i])) return FALSE;
    return TRUE;
}
static BOOL ability_definitions(void) {
    const char *const *names=(const char *const *)(verified_image+0x33a3d0u);
    if(!readable(names,41u*4u)) return FALSE;
    /* Native GiveAbility scans every preceding name. Prove all names are
     * terminated and that neither selected name resolves to another slot. */
    for(unsigned i=0;i<41u;++i) {
        if(!readable(names[i],64u) || !memchr(names[i],0,64u) ||
            ((_stricmp(names[i],skill_name)==0)!=(i==22u)) ||
            ((_stricmp(names[i],bonus_name)==0)!=(i==6u))) return FALSE;
    }
    const uint8_t *skill=verified_image+0x3d27c4u+22u*0xe0u;
    const uint8_t *bonus=verified_image+0x3d27c4u+6u*0xe0u;
    return readable(skill,8u) && readable(bonus,8u) &&
        *(const int32_t *)skill==1 && number(skill+4u)==0.0f &&
        *(const int32_t *)bonus==10 && number(bonus+4u)==40.0f;
}
BOOL SudekiMpLanStoryRecruitSetupInitialize(HMODULE image) {
    if(!image || (verified_image && verified_image!=(uint8_t *)image) ||
        InterlockedCompareExchange(&active,0,0) || !SudekiMpCheckLoadedExecutable(image)) {
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    verified_image=(uint8_t *)image;
    /* Ability definitions are populated by native loading. Installation at
     * the title proves code only; components() proves those dynamic values
     * before any eventual setup mutation. */
    if(!methods_exact()) {
        verified_image=NULL; SetLastError(ERROR_BAD_EXE_FORMAT); return FALSE;
    }
    SetLastError(ERROR_SUCCESS); return TRUE;
}

/* The callback establishes creation identity, exact paused registry and
 * native-thread authority. These local checks prove the individual methods'
 * concrete component/read/write paths; readability alone grants no lease. */
static BOOL components(SudekiMpLanStoryRecruitSetup *t,void *actor,
    SudekiMpLanStoryRecruitSetupOwnerExact exact,void *context,
    uint8_t **experience,uint8_t **stats) {
    t->reason="actor_owner";
    if(!exact(actor,context)) { SetLastError(ERROR_RETRY); return FALSE; }
    t->reason="native_components";
    if(!readable(actor,0xd8u) || *(void **)actor!=verified_image+0x2d555cu ||
        !readable(verified_image+0x408d94u,4u)) goto bad;
    uint8_t *group=*(uint8_t **)(verified_image+0x408d94u);
    uint8_t *e=*(uint8_t **)((uint8_t *)actor+0xd4u);
    uint8_t *s=*(uint8_t **)((uint8_t *)actor+0x4cu);
    if(!readable(group,0xd8u) || !readable(e,0x166u) || !readable(s,0x7cu) ||
        *(void **)e!=verified_image+0x2d4eacu || *(void **)(e+0x10u)!=actor ||
        *(void **)s!=verified_image+0x2cc064u || *(void **)(s+0x10u)!=actor ||
        !writable(e+0x114u,41u*2u) || !writable(s+0x18u,0x64u) ||
        number(s+0x44u)!=3.0f || number(e+0x20u+3u*4u)!=300.0f) goto bad;
    t->reason="native_methods";
    if(!methods_exact()) goto bad;
    t->reason="ability_definitions";
    if(!ability_definitions()) goto bad;
    *experience=e; *stats=s; return TRUE;
bad:
    SetLastError(ERROR_INVALID_DATA); return FALSE;
}
static BOOL state_exact(const SudekiMpLanStoryRecruitSetup *t,unsigned steps,
    const uint8_t *e,const uint8_t *s) {
    uint8_t expected[sizeof(t->stats_before)];
    int16_t abilities[41];
    memcpy(expected,t->stats_before,sizeof(expected));
    memcpy(abilities,t->abilities_before,sizeof(abilities));
    if(steps&STEP_XP) memcpy(expected+0x74u-0x18u,&t->expected_xp,4u);
    if(steps&STEP_SKILL) abilities[22]=1;
    if(steps&STEP_SP) {
        memcpy(expected+0x34u-0x18u,&t->expected_sp,4u);
        memcpy(expected+0x38u-0x18u,&t->expected_max_sp,4u);
        abilities[6]=1;
    }
    return memcmp(s+0x18u,expected,sizeof(expected))==0 &&
        memcmp(e+0x114u,abilities,sizeof(abilities))==0;
}
static BOOL same_owner(SudekiMpLanStoryRecruitSetup *t,void *actor,
    SudekiMpLanStoryRecruitSetupOwnerExact exact,void *context,uint8_t **e,uint8_t **s) {
    if(!components(t,actor,exact,context,e,s)) return FALSE;
    t->reason="transaction_owner";
    if(t->actor!=actor || t->experience!=*e || t->stats!=*s ||
        t->thread!=GetCurrentThreadId()) {
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    return TRUE;
}
static BOOL begin(SudekiMpLanStoryRecruitSetup *t,void *actor,
    SudekiMpLanStoryRecruitSetupOwnerExact exact,void *context) {
    uint8_t *e,*s;
    if(!components(t,actor,exact,context,&e,&s)) return FALSE;
    float xp=number(s+0x74u),sp=number(s+0x34u),maximum=number(s+0x38u);
    float ap=number(s+0x78u);
    int16_t skill=small(e+0x114u+22u*2u),bonus=small(e+0x114u+6u*2u);
    /* The freshly created Lighthouse PC_AILISH has SP80/max40 before
     * InitialSetup grants SP ability6. Native GiveAbility(4DA420) first
     * raises maxSP by40, then adds40 to currentSP and clamps it to maxSP.
     * Permit only this observed transient over-cap state, with its bonus
     * still unapplied and the exact level3 actor/components proved above.
     * Ordinary over-cap stats remain rejected; no raw repair is performed. */
    BOOL initial_sp_bonus=bonus==0 && sp==80.0f && maximum==40.0f;
    t->reason="initial_stats";
    if(!isfinite(xp) || xp<0.0f || xp>300.0f || !isfinite(sp) ||
        !isfinite(maximum) || sp<0.0f || (maximum<sp && !initial_sp_bonus) || maximum>1000000.0f ||
        !isfinite(ap) || ap<0.0f || (skill!=0 && skill!=1) || (bonus!=0 && bonus!=1)) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    t->actor=actor; t->experience=e; t->stats=s; t->thread=GetCurrentThreadId();
    t->before_xp=xp; t->expected_xp=xp<201.0f?201.0f:xp;
    t->level=3.0f; t->ability_points=ap; t->sp=sp; t->max_sp=maximum;
    t->expected_sp=sp; t->expected_max_sp=maximum;
    if(bonus==0) {
        t->expected_max_sp=(float)(maximum+40.0f);
        t->expected_sp=(float)(sp+40.0f);
        if(t->expected_sp>t->expected_max_sp) t->expected_sp=t->expected_max_sp;
    }
    t->before_skill=skill; t->before_bonus=bonus;
    memcpy(t->stats_before,s+0x18u,sizeof(t->stats_before));
    memcpy(t->abilities_before,e+0x114u,sizeof(t->abilities_before));
    t->initialized=TRUE; return TRUE;
}
static BOOL apply_steps(SudekiMpLanStoryRecruitSetup *t,void *actor,
    SudekiMpLanStoryRecruitSetupOwnerExact exact,void *context) {
    uint8_t *e,*s;
    for(unsigned step=STEP_XP;step<=STEP_SP;step<<=1u) {
        if(t->completed&step) continue;
        if(!same_owner(t,actor,exact,context,&e,&s)) return FALSE;
        if(t->attempted&step) {
            /* An unavailable post-call witness is retried without repeating
             * a possibly successful native mutation. */
            t->reason="native_return_state";
            if(!state_exact(t,t->completed|step,e,s)) goto mismatch;
            t->completed|=step; continue;
        }
        t->reason="before_native_state";
        if(!state_exact(t,t->completed,e,s)) goto mismatch;
        t->attempted|=step;
        t->reason="native_call";
        if(step==STEP_XP) {
            if(t->before_xp<201.0f) {
                typedef void (__attribute__((thiscall)) *AddExperience)(void *,float);
                /* Actual level3 and next threshold300 were re-proved above.
                 * Target201 cannot call DoLevelUp, even with group+D4 clear. */
                ((AddExperience)(verified_image+0x0d9bf0u))(e,(float)(201.0f-t->before_xp));
            }
        } else {
            typedef unsigned char (__attribute__((thiscall)) *GiveAbility)(void *,const char *,int);
            if(!((GiveAbility)(verified_image+0x0da420u))(e,
                step==STEP_SKILL?skill_name:bonus_name,1)) goto mismatch;
        }
        if(!same_owner(t,actor,exact,context,&e,&s)) return FALSE;
        t->reason="native_return_state";
        if(!state_exact(t,t->completed|step,e,s)) goto mismatch;
        t->completed|=step;
    }
    if(!same_owner(t,actor,exact,context,&e,&s)) return FALSE;
    t->reason="final_state";
    if(!state_exact(t,ALL_STEPS,e,s)) goto mismatch;
    return TRUE;
mismatch:
    t->failed=TRUE; SetLastError(ERROR_INVALID_DATA); return FALSE;
}
BOOL SudekiMpLanStoryRecruitSetupApply(SudekiMpLanStoryRecruitSetup *t,void *actor,
    SudekiMpLanStoryRecruitSetupOwnerExact exact,void *context) {
    if(!verified_image || !t || !actor || !exact || t->failed || t->active ||
        (native_thread && native_thread!=GetCurrentThreadId()) ||
        (t->attempted&~ALL_STEPS) || (t->completed&~t->attempted) ||
        (!t->initialized && (t->attempted || t->completed))) {
        if(t && !t->active) t->reason="transaction_state";
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    if(InterlockedCompareExchange(&active,1,0)) { SetLastError(ERROR_BUSY); return FALSE; }
    t->active=TRUE;
    BOOL result=FALSE;
    if(!t->initialized && !begin(t,actor,exact,context)) goto done;
    if(!native_thread) native_thread=GetCurrentThreadId();
    result=apply_steps(t,actor,exact,context);
done:
    t->active=FALSE; InterlockedExchange(&active,0);
    if(result) { t->reason="complete"; SetLastError(ERROR_SUCCESS); }
    return result;
}
