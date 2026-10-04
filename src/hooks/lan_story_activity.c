#include "hooks/lan_story_activity.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <stdint.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Story activity requires the supported native x86 ABI"
#endif
enum { MAX_LEASES=128, MAX_ZONES=206, MAX_SPAWNS=4096, MAX_CLUSTERS=512 };
typedef struct Zone {
    uint8_t *owner,*data,*descriptor,*spawns,*clusters;
    unsigned index,spawn_count,cluster_count;
} Zone;
typedef struct Activity {
    Zone zone;
    uint8_t *spawn,*entity,*cluster;
    uint32_t identity,cluster_id;
    BOOL releasing;
    unsigned release_refs;
} Activity;
static uint8_t *base,*world;
static Activity leases[MAX_LEASES];
static unsigned lease_count,trace_count;
static DWORD native_thread;
static BOOL busy;
static uint32_t serviced_at;
static unsigned serviced_mask;
static BOOL serviced;
static const char *reason="not_initialized";
static void *activity_pause_native __attribute__((used));
static void *activity_resume_native __attribute__((used));
static const struct {unsigned rva,size;uint32_t hash;unsigned reloc_count,reloc[8];} code[]={
    {0x13d540u,98u,0x1e23f927u,0u,{0}},
    {0x13d5b0u,97u,0x50fe3b77u,0u,{0}},
    {0x1060c0u,57u,0xe1cf64b1u,1u,{0xcu}},
    {0x106100u,28u,0x1f9c06e6u,1u,{0x7u}},
    {0x13ded0u,194u,0x1a3f206bu,1u,{0x2cu}},
    {0x13dfa0u,200u,0x0273300cu,2u,{0x45u,0x56u}},
    {0xe6340u,61u,0x7d1a08a5u,1u,{0x13u}},
    {0xe6380u,108u,0x9a76c276u,3u,{0x1cu,0x45u,0x59u}},
    {0x110d00u,18u,0x57f1bceeu,0u,{0}},
    {0x110d20u,23u,0x5b459dcfu,0u,{0}},
    {0xfc650u,94u,0x9a411f82u,1u,{0x5u}},
    {0xfc790u,81u,0xfc4541bdu,0u,{0}},
    {0x185490u,43u,0x3a87b2a3u,0u,{0}},
    {0x1854c0u,68u,0xefa8ad30u,0u,{0}},
    {0xc3b40u,28u,0xb6040a05u,0u,{0}},
    {0xc3b60u,8u,0x676f1eedu,0u,{0}},
    {0xfd030u,12u,0x3d05a7b5u,0u,{0}},
    {0xfd040u,12u,0x4cd40fc5u,0u,{0}},
    {0x13bdd0u,8u,0x02f067d0u,0u,{0}},
    {0x13bf10u,8u,0xe5920961u,0u,{0}},
    {0xfd070u,178u,0x4d4847bcu,0u,{0}},
    {0xfd130u,201u,0x91b2349cu,0u,{0}},
    {0x3b860u,3u,0x94777cffu,0u,{0}},
    {0x3a5a0u,11u,0x330e5518u,0u,{0}},
    {0xa2900u,1u,0x460b3072u,0u,{0}}
};
static uint8_t verified_code[sizeof(code)/sizeof(code[0])][512];

static BOOL memory(const void *p,size_t n,BOOL write) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || VirtualQuery(p,&m,sizeof(m))!=sizeof(m) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
    DWORD access=m.Protect&0xffu;
    if(access!=PAGE_READONLY && access!=PAGE_READWRITE && access!=PAGE_WRITECOPY &&
        access!=PAGE_EXECUTE_READ && access!=PAGE_EXECUTE_READWRITE &&
        access!=PAGE_EXECUTE_WRITECOPY) return FALSE;
    if(write && access!=PAGE_READWRITE && access!=PAGE_WRITECOPY &&
        access!=PAGE_EXECUTE_READWRITE && access!=PAGE_EXECUTE_WRITECOPY) return FALSE;
    return a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL fail(void) {
    if(trace_count<32u) {
        ++trace_count;
        SudekiMpLogFormat("lan_story_activity event=refused retained=%u reason=%s\r\n",lease_count,reason);
    }
    SetLastError(ERROR_RETRY); return FALSE;
}
static BOOL zone(uint8_t *w,unsigned index,Zone *out) {
    Zone z={0}; z.index=index;
    if(index>=MAX_ZONES || !memory(w,0x390u,FALSE)) return FALSE;
    z.owner=*(uint8_t **)(w+0x58u+index*4u);
    if(!memory(z.owner,0x54u,FALSE) || *(void **)z.owner!=base+0x2c82a4u) return FALSE;
    unsigned state=*(unsigned *)(z.owner+0x34u);
    z.data=*(uint8_t **)(z.owner+(state==1u?0x18u:0x14u));
    if(!memory(z.data,0x12au,FALSE) || *(void **)z.data!=base+0x2cdcdcu ||
        *(unsigned *)(z.data+0x24u)!=index) return FALSE;
    z.descriptor=*(uint8_t **)(z.data+0x118u);
    if(!memory(z.descriptor,0x1cu,FALSE)) return FALSE;
    z.cluster_count=*(unsigned *)(z.descriptor+4u);
    z.clusters=*(uint8_t **)(z.descriptor+0xcu);
    z.spawn_count=*(unsigned *)(z.descriptor+0x10u);
    z.spawns=*(uint8_t **)(z.descriptor+0x18u);
    if(!z.cluster_count || z.cluster_count>MAX_CLUSTERS || z.spawn_count>MAX_SPAWNS ||
        !memory(z.clusters,z.cluster_count*0x48u,FALSE) ||
        (z.spawn_count && !memory(z.spawns,z.spawn_count*0x90u,TRUE))) return FALSE;
    *out=z; return TRUE;
}
static BOOL same_zone(const Zone *a,const Zone *b) {
    return a->index==b->index && a->owner==b->owner && a->data==b->data &&
        a->descriptor==b->descriptor && a->spawns==b->spawns && a->clusters==b->clusters &&
        a->spawn_count==b->spawn_count && a->cluster_count==b->cluster_count;
}
static BOOL cluster_exact(uint8_t *c) {
    return memory(c,0x48u,FALSE) && *(void **)c==base+0x2d4780u &&
        *(void **)(c+4u)==base+0x2d4788u && (*(unsigned *)(c+8u)&0x1fffu)==0xfb4u;
}
static BOOL cluster_for_sector(const Zone *z,uint16_t sector,uint8_t **out) {
    unsigned matches=0; uint8_t *result=NULL;
    /* Native 509440 searches each cluster's counted list of packed sector IDs. */
    for(unsigned i=0;i<z->cluster_count;++i) {
        uint8_t *c=z->clusters+i*0x48u;
        if(!cluster_exact(c)) return FALSE;
        unsigned count=*(unsigned *)(c+0x34u); uint16_t *ids=*(uint16_t **)(c+0x3cu);
        if(count>4096u || (count && !memory(ids,count*2u,FALSE))) return FALSE;
        for(unsigned j=0;j<count;++j) if(ids[j] && ids[j]==sector) {result=c;++matches;}
    }
    if(matches!=1u) return FALSE;
    *out=result; return TRUE;
}
static BOOL entity_exact(const Activity *a) {
    uint8_t *e=a->entity,*s=a->spawn;
    uint8_t *registry=*(uint8_t **)(base+0x409d8cu);
    if(!memory(registry,0x40u,FALSE)) return FALSE;
    unsigned n=*(unsigned *)(registry+0x34u),matches=0;
    void **items=*(void ***)(registry+0x3cu);
    if(!n || n>8192u || !memory(items,n*4u,FALSE)) return FALSE;
    for(unsigned i=0;i<n;++i) if(items[i]==e) ++matches;
    return matches==1u && memory(e,0x148u,TRUE) && *(void **)e==base+0x2d65a0u &&
        *(void **)(e+8u)==base+0x2d65c4u && *(void **)(e+0x2cu)==base+0x2d65e4u &&
        (*(unsigned *)(e+0x30u)&0x1fffu)==0xf9bu &&
        *(uint32_t *)(e+0x34u)==a->identity && !*(void **)(e+0xc0u) &&
        *(void **)(s+0x78u)==e+0x2cu && *(uint32_t *)(s+0x70u)==a->identity;
}
static BOOL methods(const uint8_t *vt,unsigned pause,unsigned resume,unsigned getter) {
    return memory(vt,0x38u,FALSE) && *(void **)(vt+0x1cu)==base+pause &&
        *(void **)(vt+0x20u)==base+resume && *(void **)(vt+0x34u)==base+getter;
}
static BOOL transition_exact(const Activity *a,BOOL resume) {
    /* Native zero-crossing dispatches this NPC's owned component list. Check
     * its complete known closure before invoking the stock pause/resume path;
     * nonzero reference-only changes do not enter component callbacks. */
    if(!entity_exact(a)) return FALSE;
    unsigned refs=a->entity[0x2bu];
    if(resume?refs==0u:refs==255u) return FALSE;
    if(resume?refs>1u:refs>0u) return TRUE;
    if(*(void **)(base+0x2d65ccu)!=base+0x13ded0u ||
        *(void **)(base+0x2d65d0u)!=base+0x13dfa0u) return FALSE;
    uint8_t *seen[64],*c=*(uint8_t **)(a->entity+0x3cu); unsigned count=0;
    while(c) {
        if(count>=64u || !memory(c,0x18u,TRUE) || *(void **)(c+0x10u)!=a->entity) return FALSE;
        for(unsigned i=0;i<count;++i) if(seen[i]==c) return FALSE;
        seen[count++]=c;
        uint8_t *vt=*(uint8_t **)c; unsigned pause=0xa2900u,unpause=0xa2900u,getter=0x3b860u,sub=0;
        if(vt==base+0x2c85fcu) {pause=0xe6340u;unpause=0xe6380u;}
        else if(vt==base+0x2cdefcu) {pause=0x110d00u;unpause=0x110d20u;}
        else if(vt==base+0x2cd0acu) {pause=0xfc650u;unpause=0xfc790u;}
        else if(vt==base+0x2c8644u) {pause=0xc3b40u;unpause=0xc3b60u;}
        else if(vt==base+0x2cd15cu) {pause=0xfd030u;unpause=0xfd040u;}
        else if(vt==base+0x2d486cu) {getter=0x3a5a0u;sub=0x2d48b4u;}
        else if(vt==base+0x2d47bcu) {getter=0x3a5a0u;sub=0x2d4804u;pause=0x185490u;unpause=0x1854c0u;}
        else if(vt==base+0x2cc9acu) {getter=0x3a5a0u;sub=0x2cc9f4u;}
        else if(vt==base+0x2d4924u) {getter=0x3a5a0u;sub=0x2d4970u;}
        else if(vt!=base+0x2cc064u && vt!=base+0x2d1fccu && vt!=base+0x2c8504u &&
            vt!=base+0x2c83acu && vt!=base+0x2d48d4u && vt!=base+0x2d4994u &&
            vt!=base+0x2d49e4u && vt!=base+0x2d4a74u) return FALSE;
        if(!methods(vt,pause,unpause,getter)) return FALSE;
        if(sub) {
            if(!memory(c,0x3cu,TRUE) || *(void **)(c+0x18u)!=base+sub ||
                (resume?c[0x3bu]==0u:c[0x3bu]==255u) ||
                *(void **)(base+sub+8u)!=base+(sub==0x2d4804u?0x13bdd0u:0xa2900u) ||
                *(void **)(base+sub+12u)!=base+(sub==0x2d4804u?0x13bf10u:0xa2900u)) return FALSE;
        }
        c=*(uint8_t **)(c+0xcu);
    }
    return count!=0u;
}
static BOOL activity_exact(const Activity *a,BOOL leased) {
    Zone z;
    if(!zone(world,a->zone.index,&z) || !same_zone(&z,&a->zone) ||
        a->spawn<z.spawns || a->spawn>=z.spawns+z.spawn_count*0x90u ||
        (size_t)(a->spawn-z.spawns)%0x90u ||
        a->cluster<z.clusters || a->cluster>=z.clusters+z.cluster_count*0x48u ||
        (size_t)(a->cluster-z.clusters)%0x48u || !cluster_exact(a->cluster)) return FALSE;
    uint8_t *s=a->spawn;
    return *(void **)s==base+0x2cddf4u && *(void **)(s+4u)==base+0x2cde04u &&
        (*(unsigned *)(s+0x5cu)&0x1fffu)==0xfb4u &&
        *(uint32_t *)(s+0x60u)==a->cluster_id &&
        *(uint32_t *)(a->cluster+0xcu)==a->cluster_id &&
        !!(s[0x82u]&2u)==!!leased && entity_exact(a);
}
__attribute__((naked,noinline,used)) static void native_activity(
    void *spawn __attribute__((unused)),unsigned resume __attribute__((unused))) {
    __asm__ volatile("pushfl\n\tpushal\n\tmovl 40(%esp),%eax\n\t"
        "cmpl $0,44(%esp)\n\tjne 1f\n\tcall *_activity_pause_native\n\tjmp 2f\n\t"
        "1: call *_activity_resume_native\n\t2: popal\n\tpopfl\n\tret\n\t");
}
static BOOL boundary(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r) {
    if(!base) return FALSE;
    for(unsigned i=0;i<sizeof(code)/sizeof(code[0]);++i)
        if(!memory(base+code[i].rva,code[i].size,FALSE) ||
            memcmp(base+code[i].rva,verified_code[i],code[i].size)) return FALSE;
    return w && r && w->service_post_original_exact && w->dispatch_serial &&
        (!native_thread || native_thread==GetCurrentThreadId()) &&
        (!lease_count || r->world==world) &&
        *(void **)(base+0x408d10u)==r->world &&
        SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) &&
        SudekiMpLanStoryObserverRosterStillExact(w,r);
}
BOOL SudekiMpLanStoryActivityService(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,unsigned mask) {
    reason="boundary";
    if(busy || !boundary(w,r) || (mask&~r->available_mask)) return fail();
    /* NPC activity changes at region boundaries, not the render rate. Drains
     * and ownership changes bypass this cap. Never catch up stale activity. */
    uint32_t now=GetTickCount();
    if(mask && serviced && serviced_mask==mask && world==r->world && now-serviced_at<100u) return TRUE;
    native_thread=GetCurrentThreadId(); world=r->world;
    Activity wanted[MAX_LEASES]; unsigned count=0;
    /* Build a union of occupied native clusters without executing their story
     * entry/exit callbacks. Only the authoritative host owns these NPCs. */
    for(unsigned c=0;c<4u;++c) if(mask&(1u<<c)) {
        reason="actor_region_membership";
        uint8_t *actor=r->actors[c],*membership;
        if(!memory(actor,0x78u,FALSE) ||
            !memory(membership=*(uint8_t **)(actor+0x74u),0x42u,FALSE) ||
            *(void **)membership!=base+0x2d47bcu || *(void **)(membership+0x10u)!=actor) return fail();
        uint16_t sector=*(uint16_t *)(membership+0x40u);
        Zone z; uint8_t *cluster;
        reason="resident_zone_cluster";
        if((sector&0xffu)==0xffu || !zone(world,sector>>8,&z) ||
            !cluster_for_sector(&z,sector,&cluster)) return fail();
        uint32_t id=*(uint32_t *)(cluster+0xcu);
        for(unsigned i=0;i<z.spawn_count;++i) {
            reason="spawn_identity";
            uint8_t *s=z.spawns+i*0x90u;
            if(*(void **)s!=base+0x2cddf4u || *(void **)(s+4u)!=base+0x2cde04u) return fail();
            if((*(unsigned *)(s+0x5cu)&0x1fffu)!=0xfb4u || *(uint32_t *)(s+0x60u)!=id) continue;
            uintptr_t resource=*(uintptr_t *)(s+0x78u);
            if(resource<=0x2cu) continue;
            uint8_t *entity=(uint8_t *)(resource-0x2cu);
            if(!memory(entity,4u,FALSE)) return fail();
            if(*(void **)entity!=base+0x2d65a0u) continue;
            BOOL owned=FALSE,duplicate=FALSE;
            for(unsigned j=0;j<lease_count;++j)
                if(leases[j].spawn==s && !leases[j].releasing) owned=TRUE;
            for(unsigned j=0;j<count;++j) if(wanted[j].spawn==s) duplicate=TRUE;
            if(duplicate || ((s[0x82u]&2u) && !owned)) continue;
            Activity a={z,s,entity,cluster,*(uint32_t *)(s+0x70u),id,FALSE,0u};
            reason="npc_identity";
            if(count==MAX_LEASES || !activity_exact(&a,owned)) return fail();
            wanted[count++]=a;
        }
    }
    /* Prove every outstanding lease before changing any flag/reference. */
    reason="retained_lease";
    for(unsigned i=0;i<lease_count;++i) if(!activity_exact(&leases[i],!leases[i].releasing) ||
        (leases[i].releasing && leases[i].entity[0x2bu]!=leases[i].release_refs)) return fail();
    busy=TRUE;
    for(unsigned i=0;i<lease_count;) {
        Activity *a=&leases[i]; BOOL keep=FALSE;
        if(a->releasing) {leases[i]=leases[--lease_count];continue;}
        for(unsigned j=0;j<count;++j) if(wanted[j].spawn==a->spawn) keep=TRUE;
        if(keep) {++i;continue;}
        BOOL host_active=!!(a->cluster[0x44u]&4u);
        reason="release_preflight";
        if(!boundary(w,r) || !activity_exact(a,TRUE) || (!host_active && !transition_exact(a,FALSE))) goto refused;
        a->release_refs=a->entity[0x2bu]+(host_active?0u:1u); a->releasing=TRUE;
        a->spawn[0x82u]&=(uint8_t)~2u;
        unsigned before=a->entity[0x2bu];
        if(!host_active) native_activity(a->spawn,0u);
        reason="release_confirmation";
        if(!boundary(w,r) || !activity_exact(a,FALSE) ||
            a->entity[0x2bu]!=before+(host_active?0u:1u)) {
            /* Preserve the lease if the native operation cannot be proved. */
            goto refused;
        }
        if(trace_count<32u) {++trace_count;SudekiMpLogFormat(
            "lan_story_activity event=release npc=%08lx host_active=%u refs=%u\r\n",
            (unsigned long)a->identity,host_active,a->entity[0x2bu]);}
        leases[i]=leases[--lease_count];
    }
    for(unsigned i=0;i<count;++i) {
        Activity *a=&wanted[i]; BOOL owned=FALSE;
        for(unsigned j=0;j<lease_count;++j) if(leases[j].spawn==a->spawn) owned=TRUE;
        if(owned) continue;
        BOOL host_active=!!(a->cluster[0x44u]&4u);
        reason="acquire_preflight";
        if(lease_count==MAX_LEASES || !boundary(w,r) || !activity_exact(a,FALSE) ||
            (!host_active && !transition_exact(a,TRUE))) goto refused;
        unsigned before=a->entity[0x2bu];
        /* Remove only the native cluster's suspension, then retain its native
         * Don'tEverPause flag so Tal's later entry/exit cannot double-adjust
         * that reference. Other pause reasons retain their own references. */
        leases[lease_count++]=*a;
        if(!host_active) native_activity(a->spawn,1u);
        a->spawn[0x82u]|=2u;
        reason="acquire_confirmation";
        if(!boundary(w,r) || !activity_exact(a,TRUE) ||
            a->entity[0x2bu]!=before-(host_active?0u:1u)) goto refused;
        if(trace_count<32u) {++trace_count;SudekiMpLogFormat(
            "lan_story_activity event=acquire npc=%08lx cluster=%08lx host_active=%u refs=%u\r\n",
            (unsigned long)a->identity,(unsigned long)a->cluster_id,host_active,a->entity[0x2bu]);}
    }
    serviced=TRUE; serviced_mask=mask; serviced_at=now;
    reason="ready";
    busy=FALSE; SetLastError(ERROR_SUCCESS); return TRUE;
refused:
    busy=FALSE; return fail();
}
BOOL SudekiMpLanStoryActivityInitialize(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    if(base || lease_count || !image || !SudekiMpCheckLoadedExecutable(image)) return FALSE;
    for(unsigned i=0;i<sizeof(code)/sizeof(code[0]);++i) {
        uint32_t hash=2166136261u;
        uint8_t normalized[512];
        if(!memory(b+code[i].rva,code[i].size,FALSE)) return FALSE;
        memcpy(normalized,b+code[i].rva,code[i].size);
        for(unsigned j=0;j<code[i].reloc_count;++j) {
            uint32_t address; memcpy(&address,normalized+code[i].reloc[j],4u);
            if(address<(uintptr_t)b || address-(uintptr_t)b>=0x420000u) return FALSE;
            address=address-(uintptr_t)b+0x400000u;
            memcpy(normalized+code[i].reloc[j],&address,4u);
        }
        for(unsigned j=0;j<code[i].size;++j) hash=(hash^normalized[j])*16777619u;
        if(hash!=code[i].hash) return FALSE;
        memcpy(verified_code[i],b+code[i].rva,code[i].size);
    }
    activity_pause_native=b+0x13d540u;activity_resume_native=b+0x13d5b0u;
    base=b;world=NULL;native_thread=0;busy=serviced=FALSE;trace_count=0;
    return TRUE;
}
BOOL SudekiMpLanStoryActivityRetains(void) {return busy || lease_count!=0u;}
BOOL SudekiMpLanStoryActivityNativeExitReturned(void) {
    if(busy || (native_thread && native_thread!=GetCurrentThreadId())) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    /* Caller owns the existing synchronous Quit return witness. Forget only
     * plain metadata after destruction; no retired world pointer is read. */
    memset(leases,0,sizeof(leases)); lease_count=0; world=NULL; serviced=FALSE;
    return TRUE;
}
BOOL SudekiMpLanStoryActivityUninstall(void) {
    if(SudekiMpLanStoryActivityRetains()) {SetLastError(ERROR_BUSY);return FALSE;}
    base=world=NULL;native_thread=0;return TRUE;
}
