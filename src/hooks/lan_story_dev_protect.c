#include "hooks/lan_story_dev_protect.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <stdint.h>
#include <string.h>

/* CCharacterArbiter::SetInvulnerable (thiscall: ECX = arbiter, stack BOOL,
 * RET 4): increments/decrements the int8 refcount at +0x54 and sets/clears
 * flag 0x800 in +0x50 when the count crosses zero (same contract the cleanroom
 * engine's party lease uses). Entry bytes verified before any call. */
enum { RVA_SET_INVULNERABLE=0xdca10u, RVA_ARBITER_VTABLE=0x2cc9acu, ARBITER_FLAGS=0x50u, ARBITER_REF=0x54u,
    INVULNERABLE_FLAG=0x800u, LEASES=5u, LOG_LIMIT=200u };
static const uint8_t setter_entry[]={
    0x80,0x7c,0x24,0x04,0x00, 0x74,0x05, 0xfe,0x41,0x54, 0xeb,0x03, 0xfe,0x49,0x54,
    0x80,0x79,0x54,0x00, 0x7e,0x0a, 0x81,0x49,0x50,0x00,0x08,0x00,0x00, 0xc2,0x04,0x00,
    0x81,0x61,0x50,0xff,0xf7,0xff,0xff, 0xc2,0x04,0x00 };
static uint8_t *base; static BOOL enabled; static unsigned logs;
static struct { const uint8_t *entity,*arbiter; } leases[LEASES]; static unsigned lease_count;

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && VirtualQuery(p,&m,sizeof(m))==sizeof(m) && m.State==MEM_COMMIT &&
        !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) && a+n>=a && a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL take_log(void) { return logs<LOG_LIMIT && (++logs,TRUE); }
/* The entity still owns this arbiter and the arbiter is the hero class. */
static BOOL arbiter_of(const uint8_t *entity,const uint8_t **out) {
    if(!readable(entity,0x94u)) return FALSE;
    const uint8_t *a=*(const uint8_t *const *)(entity+0x90u);
    if(!readable(a,0x64u) || *(const void *const *)a!=base+RVA_ARBITER_VTABLE || *(const void *const *)(a+0x10u)!=entity) return FALSE;
    *out=a; return TRUE;
}
static void set_invulnerable(const uint8_t *arbiter,BOOL on) {
    void *fn=base+RVA_SET_INVULNERABLE; uint32_t value=on?1u:0u;
    __asm__ volatile("push %[v]; call *%[fn]" : : "c"(arbiter), [v]"r"(value), [fn]"r"(fn) : "eax","edx","memory","cc");
}
static int lease_index(const uint8_t *entity) {
    for(unsigned i=0;i<lease_count;++i) if(leases[i].entity==entity) return (int)i;
    return -1;
}
static void drop_lease(unsigned i) { leases[i]=leases[--lease_count]; }
static BOOL acquire(const uint8_t *entity,const char *label) {
    const uint8_t *a; if(lease_count>=LEASES || !arbiter_of(entity,&a)) return FALSE;
    int before=*(const int8_t *)(a+ARBITER_REF); uint32_t flags=*(const uint32_t *)(a+ARBITER_FLAGS);
    if(before<0 || before==INT8_MAX || ((before>0)!=((flags&INVULNERABLE_FLAG)!=0u))) {
        if(take_log()) SudekiMpLogFormat("dev_protect event=acquire status=rejected who=%s entity=%p ref=%d flags=%08lx reason=invalid_native_state\r\n",
            label,(const void *)entity,before,(unsigned long)flags);
        return FALSE;
    }
    set_invulnerable(a,TRUE);
    int after=*(const int8_t *)(a+ARBITER_REF); flags=*(const uint32_t *)(a+ARBITER_FLAGS);
    if(after!=before+1 || !(flags&INVULNERABLE_FLAG)) {
        if(after==before+1) set_invulnerable(a,FALSE); /* count moved without the flag: undo */
        if(take_log()) SudekiMpLogFormat("dev_protect event=acquire status=unproven who=%s entity=%p ref=%d->%d flags=%08lx\r\n",
            label,(const void *)entity,before,after,(unsigned long)flags);
        return FALSE;
    }
    leases[lease_count].entity=entity; leases[lease_count].arbiter=a; ++lease_count;
    if(take_log()) SudekiMpLogFormat("dev_protect event=acquire status=confirmed who=%s entity=%p arbiter=%p ref=%d->%d flags=%08lx policy=native_refcount_lease\r\n",
        label,(const void *)entity,(const void *)a,before,after,(unsigned long)flags);
    return TRUE;
}
static void release(unsigned i,const char *why) {
    const uint8_t *entity=leases[i].entity,*a;
    if(!arbiter_of(entity,&a) || a!=leases[i].arbiter) {
        if(take_log()) SudekiMpLogFormat("dev_protect event=release status=dropped entity=%p reason=object_identity_unavailable why=%s\r\n",(const void *)entity,why);
        drop_lease(i); return;
    }
    int before=*(const int8_t *)(a+ARBITER_REF);
    if(before<=0) {
        if(take_log()) SudekiMpLogFormat("dev_protect event=release status=dropped entity=%p ref=%d reason=count_already_zero why=%s\r\n",(const void *)entity,before,why);
        drop_lease(i); return;
    }
    set_invulnerable(a,FALSE);
    int after=*(const int8_t *)(a+ARBITER_REF);
    if(take_log()) SudekiMpLogFormat("dev_protect event=release status=%s entity=%p ref=%d->%d flags=%08lx why=%s\r\n",
        after==before-1?"confirmed":"unproven",(const void *)entity,before,after,(unsigned long)*(const uint32_t *)(a+ARBITER_FLAGS),why);
    drop_lease(i);
}

BOOL SudekiMpLanStoryDevProtectInstall(HMODULE game_module,const wchar_t *config_path) {
    uint8_t *b=(uint8_t *)game_module;
    enabled=FALSE; base=NULL;
    if(!b || !config_path) return TRUE;
    wchar_t v[16]={0}; GetPrivateProfileStringW(L"DevPlay",L"PartyInvulnerable",L"",v,15,config_path);
    if(!(v[0] && (!_wcsicmp(v,L"true") || !_wcsicmp(v,L"1") || !_wcsicmp(v,L"yes")))) return TRUE;
    if(!SudekiMpCheckLoadedExecutable(game_module) || memcmp(b+RVA_SET_INVULNERABLE,setter_entry,sizeof(setter_entry))) {
        SudekiMpLogWrite("dev_protect event=install_failed reason=setter_entry_mismatch\r\n");
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    base=b; enabled=TRUE;
    SudekiMpLogWrite("dev_protect event=installed targets=party_heroes+dev_play_ally seam=arbiter_set_invulnerable:0xdca10 policy=native_refcount_lease_no_damage_patch\r\n");
    return TRUE;
}
BOOL SudekiMpLanStoryDevProtectEnabled(void) { return enabled; }
void SudekiMpLanStoryDevProtectService(const SudekiMpLanStoryNativeRoster *roster,void *ally_entity) {
    if(!enabled || !base) return;
    const uint8_t *want[LEASES]; unsigned want_count=0; const char *labels[LEASES];
    if(roster) for(unsigned c=0;c<4u;++c) if(roster->actors[c]) { want[want_count]=roster->actors[c]; labels[want_count++]="hero"; }
    if(ally_entity) { want[want_count]=ally_entity; labels[want_count++]="ally"; }
    /* Leases whose entity left the wanted set (zone change, rebuild, ally lost). */
    for(unsigned i=0;i<lease_count;) {
        BOOL keep=FALSE; for(unsigned k=0;k<want_count;++k) if(want[k]==leases[i].entity) keep=TRUE;
        const uint8_t *a;
        if(keep && arbiter_of(leases[i].entity,&a) && a==leases[i].arbiter &&
            (*(const uint32_t *)(a+ARBITER_FLAGS)&INVULNERABLE_FLAG)) { ++i; continue; }
        release(i,keep?"native_state_lost":"entity_left_roster");
    }
    for(unsigned k=0;k<want_count;++k) if(lease_index(want[k])<0) (void)acquire(want[k],labels[k]);
}
void SudekiMpLanStoryDevProtectRelease(const char *why) {
    while(lease_count) release(lease_count-1u,why?why:"release");
}
