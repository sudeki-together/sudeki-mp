#include "hooks/lan_story_talos_damage_probe.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Talos damage observation requires the supported x86 ABI"
#endif

/* DevPlayAvatarContractReport + exact-image disassembly:
 * D1B00: stdcall(source CCombat*, target Entity*, damage packet*), RET 0x0c.
 * D21D0: ECX=target CCombat*, [ESP+4]=packet (thiscall), RET 4.
 * Each stub observes then restores ALL registers, flags and FP/SSE state.
 * Original native entries, including any later owning adapter, receive the
 * original stack/return address. We neither interpret nor change the result.
 * The D1B00 snapshot precedes damage calculation; D21D0 sees the resulting
 * packet. A packet address plus source/target/caller correlates those stages.
 */
enum { PREPARE_RVA=0xd1b00u, APPLY_RVA=0xd21d0u, CALL_COUNT=6u,
    LOG_LIMIT=12000u };
static const uint32_t call_rvas[CALL_COUNT]={
    0xd3a78u,0xdabbcu,0x1046e8u,0xd3b61u,0xd3edau,0xdab91u
};
static const uint8_t prepare_prefix[]={0x8b,0x44,0x24,0x08,0x53};
static const uint8_t apply_prefix[]={0x55,0x8b,0xec,0x83,0xe4,0xf8};
static SudekiMpRelativeCallHook hooks[CALL_COUNT];
static uint8_t *base;
static BOOL installed;
static LONG lock,lines,active;
void *SudekiMpTalosDamagePrepareEntry __attribute__((used));
void *SudekiMpTalosDamageApplyEntry __attribute__((used));

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && a<=UINTPTR_MAX-n &&
        VirtualQuery(p,&m,sizeof(m))==sizeof(m) && m.State==MEM_COMMIT &&
        !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
        a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static unsigned rva(uintptr_t p) {
    return base && p>=(uintptr_t)base &&
        p<(uintptr_t)base+SUDEKIMP_EXPECTED_IMAGE_SIZE?
        (unsigned)(p-(uintptr_t)base):0u;
}
/* Only these exact known character classes expose the common fields read
 * below. A readable unknown entity produces no speculative component reads. */
static const char *entity_class(const uint8_t *e) {
    static const struct {uint32_t primary,resource; const char *name;} classes[]={
        {0x2d5010u,0x2d5054u,"Tal"},{0x2d555cu,0x2d55a0u,"Ailish"},
        {0x2d5a88u,0x2d5accu,"Buki"},{0x2d66fcu,0x2d6740u,"Elco"},
        {0x2d55d4u,0x2d5618u,"Ally"},{0x2d69ecu,0x2d6a30u,"Monster"}
    };
    if(!readable(e,0xc4u)) return NULL;
    for(unsigned i=0;i<sizeof(classes)/sizeof(classes[0]);++i)
        if(*(const void *const *)e==base+classes[i].primary &&
            *(const void *const *)(e+0x2cu)==base+classes[i].resource)
            return classes[i].name;
    return NULL;
}
static void name_of(const uint8_t *e,const char *kind,char out[40]) {
    lstrcpynA(out,kind?kind:"unknown",40);
    if(!kind) return;
    const uint32_t *resource=*(const uint32_t *const *)(e+0x38u);
    if(!readable(resource,8u) || !resource[0]) return;
    const char *name=(const char *)(uintptr_t)resource[1]; unsigned i=0;
    while(i<39u && readable(name+i,1u) && name[i]>=0x20 && name[i]<0x7f) {
        out[i]=name[i]; ++i;
    }
    if(i) out[i]=0;
}
static void __attribute__((used,noinline)) probe_observe(unsigned stage,
    const uint8_t *combat,const uint8_t *target,const uint8_t *packet,
    uintptr_t return_address) {
    DWORD saved=GetLastError(); InterlockedIncrement(&active);
    if(InterlockedCompareExchange(&lock,1,0)!=0) goto done;
    if(!base || !SudekiMpLogResearchEnabled() || lines>=LOG_LIMIT) goto unlock;
    const uint8_t *source=NULL,*owner=NULL,*weapon=NULL,*item=NULL,*stats=NULL;
    uint16_t p14=0; uint32_t p28=0,p3c=0,w334=0,w338=0; float hp=0.0f;
    BOOL packet_valid=readable(packet,0x68u),hp_valid=FALSE;
    if(readable(combat,0x14u)) owner=*(const uint8_t *const *)(combat+0x10u);
    if(stage==0u) source=owner;
    else {
        target=owner;
        if(packet_valid) source=*(const uint8_t *const *)(packet+0x30u);
    }
    const char *source_kind=entity_class(source),*target_kind=entity_class(target);
    char source_name[40],target_name[40];
    name_of(source,source_kind,source_name); name_of(target,target_kind,target_name);
    if(packet_valid) {
        memcpy(&p14,packet+0x14u,2u); memcpy(&p28,packet+0x28u,4u);
        memcpy(&p3c,packet+0x3cu,4u);
    }
    if(source_kind) {
        weapon=*(const uint8_t *const *)(source+0xc0u);
        if(readable(weapon,0x33cu) && *(const void *const *)weapon==base+0x2d4d3cu &&
            *(const void *const *)(weapon+0x10u)==source) {
            item=*(const uint8_t *const *)(weapon+0x268u);
            memcpy(&w334,weapon+0x334u,4u); memcpy(&w338,weapon+0x338u,4u);
        }
        else weapon=NULL;
    }
    if(target_kind) {
        stats=*(const uint8_t *const *)(target+0x4cu);
        if(readable(stats,0x3cu) && *(const void *const *)stats==base+0x2cc064u &&
            *(const void *const *)(stats+0x10u)==target) {
            memcpy(&hp,stats+0x2cu,4u); hp_valid=isfinite(hp);
        }
    }
    ++lines;
    SudekiMpLogFormat("talos_damage_probe event=%s ms=%lu caller=0x%06x "
        "combat=%p source=%p source_name=%s target=%p target_name=%s "
        "packet=%p packet_valid=%u p14=%04x p28=%08lx p3c=%08lx "
        "p64=%02x p67=%02x source_weapon_valid=%u source_weapon=%p source_item=%p "
        "weapon334=%08lx weapon338=%08lx target_hp_valid=%u target_hp=%.6f "
        "policy=observe_only\r\n",stage?"accepted_entry":"prepare_entry",
        (unsigned long)GetTickCount(),rva(return_address-5u),(const void *)combat,
        (const void *)source,source_name,(const void *)target,target_name,
        (const void *)packet,packet_valid,(unsigned)p14,(unsigned long)p28,
        (unsigned long)p3c,packet_valid?packet[0x64u]:0u,packet_valid?packet[0x67u]:0u,
        weapon!=NULL,(const void *)weapon,(const void *)item,(unsigned long)w334,
        (unsigned long)w338,hp_valid,(double)hp);
unlock:
    InterlockedExchange(&lock,0);
done:
    InterlockedDecrement(&active); SetLastError(saved);
}

/* After PUSHFD/PUSHAD, EBP+24=ECX, +36=return, +40..48=arguments. */
static void __attribute__((naked,noinline)) prepare_stub(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;"
        "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld; sub $32,%esp;"
        "movl $0,(%esp); mov 40(%ebp),%eax; mov %eax,4(%esp);"
        "mov 44(%ebp),%eax; mov %eax,8(%esp); mov 48(%ebp),%eax; mov %eax,12(%esp);"
        "mov 36(%ebp),%eax; mov %eax,16(%esp); call _probe_observe;"
        "add $32,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl;"
        "jmp *_SudekiMpTalosDamagePrepareEntry");
}
static void __attribute__((naked,noinline)) apply_stub(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;"
        "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld; sub $32,%esp;"
        "movl $1,(%esp); mov 24(%ebp),%eax; mov %eax,4(%esp); movl $0,8(%esp);"
        "mov 40(%ebp),%eax; mov %eax,12(%esp); mov 36(%ebp),%eax; mov %eax,16(%esp);"
        "call _probe_observe; add $32,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl;"
        "jmp *_SudekiMpTalosDamageApplyEntry");
}

BOOL SudekiMpLanStoryTalosDamageProbeImageMatches(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    if(!b || !SudekiMpCheckLoadedExecutable(image) ||
        memcmp(b+PREPARE_RVA,prepare_prefix,sizeof(prepare_prefix)) ||
        memcmp(b+APPLY_RVA,apply_prefix,sizeof(apply_prefix)) ||
        memcmp(b+0xd1c1fu,(const uint8_t[]){0xc2,0x0c,0x00},3u)) return FALSE;
    for(unsigned i=0;i<CALL_COUNT;++i) {
        int32_t d; const uint8_t *call=b+call_rvas[i];
        if(call[0]!=0xe8u) return FALSE;
        memcpy(&d,call+1u,4u);
        if(call+5u+d!=b+(i<3u?PREPARE_RVA:APPLY_RVA)) return FALSE;
    }
    return TRUE;
}
BOOL SudekiMpLanStoryTalosDamageProbeUninstall(void) {
    DWORD error=ERROR_SUCCESS; BOOL ok=TRUE;
    if(InterlockedCompareExchange(&active,0,0)) {SetLastError(ERROR_BUSY);return FALSE;}
    for(unsigned i=CALL_COUNT;i>0u;--i)
        if(!SudekiMpRestoreRelativeCallHook(&hooks[i-1u])) {
            if(ok) error=GetLastError();
            ok=FALSE;
        }
    if(ok) installed=FALSE;
    /* Even after successful restoration, retain native entry dependencies:
     * a call already in a stub may still tail-jump to them. No allocations,
     * callback trampolines, or gameplay object leases are owned here. */
    if(!ok) SetLastError(error?error:ERROR_BUSY);
    return ok;
}
BOOL SudekiMpLanStoryTalosDamageProbeInstall(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    if(installed || active || (base && base!=b)) {SetLastError(ERROR_BUSY);return FALSE;}
    for(unsigned i=0;i<CALL_COUNT;++i) if(hooks[i].installed) {SetLastError(ERROR_BUSY);return FALSE;}
    if(!SudekiMpLanStoryTalosDamageProbeImageMatches(image)) {SetLastError(ERROR_INVALID_DATA);return FALSE;}
    base=b; lines=0;
    /* Publish both forwarding targets before any callsite can enter a stub. */
    SudekiMpTalosDamagePrepareEntry=b+PREPARE_RVA;
    SudekiMpTalosDamageApplyEntry=b+APPLY_RVA;
    for(unsigned i=0;i<CALL_COUNT;++i)
        if(!SudekiMpInstallRelativeCallHook(&hooks[i],b+call_rvas[i],
            b+(i<3u?PREPARE_RVA:APPLY_RVA),
            (const void *)(uintptr_t)(i<3u?prepare_stub:apply_stub))) {
            DWORD error=GetLastError();
            (void)SudekiMpLanStoryTalosDamageProbeUninstall();
            SetLastError(error?error:ERROR_INVALID_DATA);return FALSE;
        }
    installed=TRUE;
    SudekiMpLogWrite("talos_damage_probe event=installed calls=6 policy=observe_only "
        "damage_entry_ownership=unchanged equip_writes=none\r\n");
    return TRUE;
}
