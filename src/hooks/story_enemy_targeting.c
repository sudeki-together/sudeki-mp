#include "hooks/story_enemy_targeting.h"
#include "hooks/call_hook.h"
#include "hooks/lan_arena_hit_feedback.h"
#include "hooks/lan_story_spawn_trace.h"
#include "hooks/lan_story_shots.h"
#include "hooks/lan_story_cast.h"
#include "hooks/lan_party_control.h"
#include "hooks/lan_story_runtime.h"
#include "hooks/lan_party_projectile_lifetime.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Enemy observation requires the exact retail x86 ABI"
#endif
enum { SELECTOR=0x1b76c0, FILTER=0x1b7120, CAP_MELEE=0x1b7169, CAP_RANGED=0x1b71b7 };
static uint8_t *base;
static DWORD thread;
static volatile LONG calls;
static BOOL installed,subscribed,hit_owned,admitted,cooperative;
static wchar_t ini[MAX_PATH];
static uint32_t sampled_at,config_at,report_at;
static SudekiMpStoryThreatConfig config;
static SudekiMpLanStoryNativeRoster roster;
static unsigned kinds[4],firing_signal[4];
static SudekiMpStoryThreatSeat seat_inputs[4];
static SudekiMpLanPartyLease away_connections[4];
static SudekiMpInlineHook seams[3];
void *SudekiMpStoryEnemyCapNext[2] __attribute__((used));
void *SudekiMpStoryEnemyCapAuthored[2] __attribute__((used));
static const uint8_t prefix[]={0x55,0x8b,0xec,0x83,0xe4,0xf8};
static const uint8_t compare[]={0x66,0x83,0xb9,0x6a,1,0,0,0};
static const unsigned sites[]={SELECTOR,CAP_MELEE,CAP_RANGED};
static const unsigned callers[]={0x1a8c67,0x1a9d79,0x1aa0ac,0x1aa2bc,0x1aa5d9,0x1aa819,
    0x1aa9e9,0x1aac89,0x1aae28,0x1ab0eb,0x1ab4a8,0x1ab77e,0x1abcdb,0x1abf28,
    0x1ac01e,0x1ac36e,0x1ac789,0x1ac96b,0x1acc89,0x1acffe,0x1adfe8,0x1ae05d,0x1ae8a8,0x1af04a};
static const unsigned relocations[]={0x83,0x91,0x9f,0xe9,0x108,0x115,0x13b,0x158};
static const unsigned companion_relocations[]={0x1adf90,0x1ae029,0x1ae850};
static const struct {unsigned rva,size;uint32_t hash;} exact_ranges[]={
    {SELECTOR,0x32f,0xf0895a2e},{FILTER,0xeb,0xbdbd504c},
    {0x1adf60,0xa0,0x532523e9},{0x1ae000,0x70,0x0cd7c6c3},
    {0x1ae820,0xa0,0x62aa4d69},{0x1ae940,0xb0,0xfc5c1466},
    {0x1ae280,0x90,0xc548324b},{0xebfb0,0x55,0x60079519},
    {0xebf70,0x20,0xa869c941},{0xb2c40,0x70,0xecd35039},{0xb2cb0,0x90,0x68b90090}};
/* CONFIRMED_STATIC: all three selector exits are C2 10 00. The draft's
 * cdecl annotation was incorrect. Preserve both EAX and the raw *out. */
typedef void *(__stdcall *SelectFn)(void *,void **,void *,float);
typedef struct Request {
    struct Request *parent;
    uint8_t *request,*self;
    unsigned cap_only,parity;
} Request;
static Request *current;
typedef struct Counts {
    uint32_t resource,mode,count,empty,cap_only,native_absent;
    uint32_t picks[4],cap_refusals[4],cap_parity[4],would[STORY_RULE_COUNT];
    char name[48];
} Counts;
static Counts counters[128];
typedef struct EnemyIdentity {
    uint32_t epoch,serial,seen;
    SudekiMpStoryThreatRecord memory;
} EnemyIdentity;
static EnemyIdentity identities[128];
static uint32_t walk_sequence,list_seq;
static struct {uint32_t epoch,tick;float x,z;} scale[4];
/* Current-frame observations contain no native pointers. Ranged capability is
 * only +46 bit 0; no state-vtable scan or unleased zone clustering. */
typedef struct EnemyFrame {
    uint32_t serial,stamp,group_id;
    float x,z;
    unsigned type,excluded,ranged_capable,target_seat,aware;
} EnemyFrame;
static EnemyFrame unit_table[128];
static unsigned unit_count;
static BOOL table_complete;
typedef struct SeatObservation {
    float x,z,nearest,melee_distance;
    unsigned kind,firing,engaged,melee,ranged,attacking,in_melee,unleased,position_unknown;
} SeatObservation;
static SeatObservation seat_frames[4];
static SudekiMpStoryThreatPushState push_states[4][STORY_SET_COUNT];
static BOOL push_tracked,level_logged,level_clamped;
static uint32_t push_tracked_at;
static unsigned effective_level;
enum { EVENT_ALERT,EVENT_ALERT_END,EVENT_ALERT_RETIRED,EVENT_RESPONDER_RELEASED,EVENT_PUSH,EVENT_PUSH_END,EVENT_COUNT };
static unsigned event_logs[EVENT_COUNT],threat_logs;
static uint32_t threat_log_at,threat_substitutions,threat_changes,alerts,alert_empty,pushes,lock_blocked,hit_unslotted;

static uint32_t hit_count,hit_unknown,lead_substitutions,lead_changes,companion_substitutions;
static unsigned lead_logs;
static uint32_t lead_log_at;
/* Namespace-bounded type observations survive the per-second count flush.
 * A full set suppresses additional type records rather than evicting entries. */
static struct {uint32_t resource,mode;BOOL used;} seen_types[128];
static BOOL first_type(uint32_t resource,unsigned mode) {
    unsigned vacant=128;
    for(unsigned i=0;i<128;++i) {
        if(seen_types[i].used && seen_types[i].resource==resource && seen_types[i].mode==mode) return FALSE;
        if(!seen_types[i].used && vacant==128) vacant=i;
    }
    if(vacant==128) return FALSE;
    seen_types[vacant].used=TRUE;seen_types[vacant].resource=resource;seen_types[vacant].mode=mode;
    return TRUE;
}
static void reset_selection_logs(void) {
    lead_substitutions=lead_changes=lead_logs=lead_log_at=0;
    threat_logs=threat_log_at=threat_substitutions=threat_changes=alerts=alert_empty=pushes=lock_blocked=hit_unslotted=0;
    memset(event_logs,0,sizeof(event_logs));
    memset(seen_types,0,sizeof(seen_types));
}
typedef struct CompanionMemory {
    unsigned anchor_plus_one,selection_logs;
    uint32_t sampled_at,selection_at;
} CompanionMemory;
static CompanionMemory companions[4];
static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m;uintptr_t a=(uintptr_t)p;
    return p && n && a<=UINTPTR_MAX-n && VirtualQuery(p,&m,sizeof(m))==sizeof(m) &&
        m.State==MEM_COMMIT && !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
        a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static uint8_t canonical_byte(uint8_t *b,unsigned rva,BOOL owned) {
    for(unsigned i=0;owned && i<3;++i) {
        size_t n=i?sizeof(compare):sizeof(prefix);
        if(rva>=sites[i] && rva<sites[i]+n) return (i?compare:prefix)[rva-sites[i]];
    }
    for(unsigned i=0;i<sizeof(relocations)/sizeof(relocations[0]);++i) {
        unsigned start=SELECTOR+relocations[i];
        if(rva>=start && rva<start+4) {
            uint32_t value;memcpy(&value,b+start,4);
            value-=(uint32_t)(uintptr_t)b-0x400000u;
            return (uint8_t)(value>>((rva-start)*8));
        }
    }
    for(unsigned i=0;i<sizeof(companion_relocations)/sizeof(companion_relocations[0]);++i) {
        unsigned start=companion_relocations[i];
        if(rva>=start && rva<start+4) {
            uint32_t value;memcpy(&value,b+start,4);value-=(uint32_t)(uintptr_t)b-0x400000u;
            return (uint8_t)(value>>((rva-start)*8));
        }
    }
    return b[rva];
}
static BOOL image_exact(uint8_t *b,BOOL owned) {
    for(unsigned i=0;i<sizeof(exact_ranges)/sizeof(exact_ranges[0]);++i) {
        if(!b || !readable(b+exact_ranges[i].rva,exact_ranges[i].size)) return FALSE;
        uint32_t hash=2166136261u;
        for(unsigned j=0;j<exact_ranges[i].size;++j)
            hash=(hash^canonical_byte(b,exact_ranges[i].rva+j,owned))*16777619u;
        if(hash!=exact_ranges[i].hash) return FALSE;
    }
    static const unsigned melee[]={0x2dc6a8,0x2dc880,0x2dc7b0};
    static const unsigned ranged[]={0x2dc748,0x2dc814,0x2dc710};
    for(unsigned i=0;i<3;++i)
        if(!readable(b+melee[i],0x30) || *(void **)(b+melee[i]+0x2c)!=b+0x1ae820 ||
            !readable(b+ranged[i],0x34) || *(void **)(b+ranged[i]+0x2c)!=b+0x1adf60 ||
            *(void **)(b+ranged[i]+0x30)!=b+0x1ae000) return FALSE;
    for(unsigned i=0;i<24;++i) {
        unsigned r=callers[i];int32_t d;
        if(!readable(b+r,5) || b[r]!=0xe8) return FALSE;
        memcpy(&d,b+r+1,4);if((int32_t)(r+5)+d!=SELECTOR) return FALSE;
    }
    /* GetAiDamageMultiplier: read-only FLD [global], RET. */
    return readable(b+0xd17c0,7) && b[0xd17c0]==0xd9 && b[0xd17c1]==5 &&
        *(void **)(b+0xd17c2)==b+0x33aff0 && b[0xd17c6]==0xc3;
}
BOOL SudekiMpStoryEnemyTargetingImageExact(uint8_t *b) {return image_exact(b,FALSE);}
static BOOL fresh(void) {
    return installed && admitted && cooperative && thread==GetCurrentThreadId() &&
        config.level!=STORY_TARGET_OFF && (uint32_t)(GetTickCount()-sampled_at)<=250 &&
        SudekiMpLanStoryObserverNativeRosterExact(&roster);
}
static unsigned seat_of(const void *ai) {
    for(unsigned c=0;c<4;++c) if(ai && (roster.available_mask&(1u<<c)) && roster.ai[c]==ai) return c;
    return 4;
}
static unsigned actor_seat(const void *actor) {
    for(unsigned c=0;c<4;++c) if(actor && (roster.available_mask&(1u<<c)) && roster.actors[c]==actor) return c;
    return 4;
}
static BOOL actor_unit(uint8_t *ai,uint8_t **actor) {
    if(!readable(ai,0x16c) || *(void **)ai!=base+0x2d4924) return FALSE;
    uint8_t *a=*(uint8_t **)(ai+0x10);
    if(!readable(a,0xb4) || *(void **)(a+0x94)!=ai) return FALSE;
    unsigned c=actor_seat(a),type=*(uint32_t *)(ai+0x148);
    if(c<4) {if(type!=5 || roster.ai[c]!=ai) return FALSE;}
    else if(!((type==1 || type==2) && *(void **)a==base+0x2d5fa0) &&
        !(type==3 && *(void **)a==base+0x2d69ec)) return FALSE;
    *actor=a;return TRUE;
}
static BOOL position(uint8_t *actor,float *x,float *z) {
    if(!readable(actor,0x78)) return FALSE;
    uint8_t *p=*(uint8_t **)(actor+0x74);
    if(!readable(p,0x50) || *(void **)(p+0x10)!=actor) return FALSE;
    *x=*(float *)(p+0x44);*z=*(float *)(p+0x4c);return isfinite(*x)&&isfinite(*z);
}
static void entity_name(uint8_t *a,char out[48]) {
    strcpy(out,"?");
    const uint32_t *r=*(const uint32_t **)(a+0x38);
    if(!readable(r,8)) return;
    const char *p=(const char *)(uintptr_t)r[1];unsigned n=0;
    while(n<47 && readable(p+n,1) && p[n]>=0x20 && p[n]<0x7f) {out[n]=p[n];++n;}
    out[n]=0;
}
static BOOL manager_units(uint8_t **entries,unsigned *count) {
    uint8_t *m=*(uint8_t **)(base+0x409de4);
    if(!readable(m,0xf4)) return FALSE;
    unsigned n=*(unsigned *)(m+0xe8);uint8_t *list=*(uint8_t **)(m+0xf0);
    if(n>4096 || (n && !readable(list,n*12))) return FALSE;
    *entries=list;*count=n;return TRUE;
}
static BOOL manager_has(const void *ai) {
    uint8_t *list;unsigned n;
    if(!manager_units(&list,&n)) return FALSE;
    for(unsigned i=0;i<n;++i) if(*(uintptr_t *)(list+i*12)==(uintptr_t)ai+4) return TRUE;
    return FALSE;
}
/* The runtime's per-frame classification is necessary but not sufficient:
 * re-prove the retained native lease and current session at the decision. */
static BOOL remote_cap_lease(uint8_t *candidate,unsigned c) {
    uint8_t *actor=NULL;SudekiMpLanPartyLease key;
    if(c>=4 || kinds[c]!=STORY_SEAT_REMOTE ||
        SudekiMpStoryThreatSeatKind(&seat_inputs[c],GetTickCount())!=STORY_SEAT_REMOTE ||
        !actor_unit(candidate,&actor) || *(int16_t *)(candidate+0x16a)!=1 ||
        (*(uint32_t *)(candidate+0x44)&0x58)) return FALSE;
    uint8_t *mode=*(uint8_t **)(candidate+0x3c),*arb=*(uint8_t **)(actor+0x90);
    return readable(mode,12) && !mode[0xb] && readable(arb,0x5c) &&
        *(void **)arb==base+0x2cc9ac && *(void **)(arb+0x10)==actor &&
        !(*(uint32_t *)(arb+0x50)&0x10000) &&
        SudekiMpLanPartyControlStoryTurnOwner(actor,&key) && key.seat==c &&
        SudekiMpLanStoryRuntimeTurnLeaseExact(&key);
}
static BOOL away_cap_lease(uint8_t *candidate,unsigned c) {
    uint8_t *actor=NULL;
    if(c>=4 || kinds[c]!=STORY_SEAT_AWAY || !seat_inputs[c].connected ||
        SudekiMpStoryThreatSeatKind(&seat_inputs[c],GetTickCount())!=STORY_SEAT_AWAY ||
        !actor_unit(candidate,&actor) || *(int16_t *)(candidate+0x16a)!=0 ||
        (*(uint32_t *)(candidate+0x44)&0x58)) return FALSE;
    uint8_t *mode=*(uint8_t **)(candidate+0x3c),*arb=*(uint8_t **)(actor+0x90);
    return readable(mode,12) && mode[0xb]==1 && readable(arb,0x5c) &&
        *(void **)arb==base+0x2cc9ac && *(void **)(arb+0x10)==actor &&
        !(*(uint32_t *)(arb+0x50)&0x10000) &&
        SudekiMpLanStoryRuntimeAwaySeatExact(c,&away_connections[c]);
}
/* At S2 the native non-cap filter already succeeded. An approved seat takes
 * the authored-cap branch, bypassing both override and AI-mode reductions.
 * Never write the override count, mode or either attacker nibble. */
static BOOL __attribute__((used,noinline,noclone,regparm(0))) cap_decision(const uint32_t *f,unsigned id) {
    DWORD error=GetLastError();BOOL allow=FALSE;
    if(current && fresh()) {
        uint8_t *candidate=(uint8_t *)(uintptr_t)f[6],*self=(uint8_t *)(uintptr_t)f[7];
        uint8_t *request=(uint8_t *)(uintptr_t)f[2],*actor=NULL;unsigned c=seat_of(candidate);
        if(c<4 && (kinds[c]==STORY_SEAT_REMOTE || kinds[c]==STORY_SEAT_AWAY) &&
            self==current->self && request==current->request &&
            readable(candidate,0x16c) && readable(request,0x28) && actor_unit(self,&actor) &&
            manager_has(self)) {
            char name[48];entity_name(actor,name);
            BOOL excluded=SudekiMpStoryThreatExcluded(&config,name) ||
                SudekiMpStoryThreatResourceExcluded(&config,*(uint32_t *)(actor+0x34));
            allow=SudekiMpStoryThreatCapParity(&config,cooperative,kinds[c],
                *(uint32_t *)(self+0x148),excluded) &&
                (remote_cap_lease(candidate,c) || away_cap_lease(candidate,c));
            if(allow) current->parity|=1u<<c;
            else if(!id) { /* REMOTE and AWAY refusals use the same cap-only math. */
                BOOL same=*(uintptr_t *)(self+0xc0)==(uintptr_t)candidate+4;
                if(SudekiMpStoryThreatCapOnly((int8_t)request[0x26],(int8_t)request[0x27],
                    candidate[0x47]&15,(*(uint16_t *)(candidate+0x46)>>12)&15,same,self[0x46]&1))
                    current->cap_only|=1u<<c;
            }
        }
    }
    SetLastError(error);return allow;
}
/* PUSHAD index: EDI,ESI,EBP,ESP,EBX,EDX,ECX,EAX. The ignored saved ESP slot
 * carries our decision. Preserve every register, FP/SSE state and non-CMP flag.
 * Replay the real ECX comparison on every unapproved or unproven path. The
 * approved continuation executes native CMP EDI,-1 and all native cap math.
 * cap_decision is pinned to regparm(0): these stubs pass both arguments on the stack. */
#define CAP_STUB(name,id,offset) \
static void __attribute__((naked,noinline)) name(void) { \
    __asm__ volatile("pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;" \
        "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld; sub $16,%esp;" \
        "mov %ebp,(%esp); movl $" #id ",4(%esp); call _cap_decision;" \
        "mov %eax,12(%ebp); add $16,%esp; fxrstor (%esp); mov %ebp,%esp;" \
        "cmpl $0,12(%esp); jnz 1f; popal; popfl; cmpw $0,0x16a(%ecx);" \
        "jmp *(_SudekiMpStoryEnemyCapNext+" #offset ");" \
        "1: popal; popfl; jmp *(_SudekiMpStoryEnemyCapAuthored+" #offset ")"); }
CAP_STUB(cap_melee,0,0)
CAP_STUB(cap_ranged,1,4)
typedef struct CompanionView {
    uint8_t *mode,*states[4];
    unsigned row,active,active_type,formation,request_state;
} CompanionView;
/* The array is owned through this exact roster AI's group. A request may
 * belong to an evaluated state before group[8] names it as active. */
static BOOL companion_view(uint8_t *self,uint8_t *actor,const uint8_t *request,CompanionView *v) {
    uint8_t *stats=*(uint8_t **)(actor+0x4c),*mode=*(uint8_t **)(self+0x3c);
    if(!readable(stats,0x44) || *(void **)stats!=base+0x2cc064 || *(void **)(stats+0x10)!=actor ||
        !readable(mode,12) || mode[9]!=4 || (mode[8]>=4 && mode[8]!=0xff) || mode[0xb]>1) return FALSE;
    float row=*(float *)(stats+0x40);
    if(!isfinite(row) || row<0 || row>2 || floorf(row)!=row) return FALSE;
    uint8_t **states=*(uint8_t ***)(mode+4);
    if(!readable(states,16)) return FALSE;
    memset(v,0,sizeof(*v));v->mode=mode;v->row=(unsigned)row;v->active=mode[8];v->request_state=4;
    for(unsigned i=0;i<4;++i) {
        uint8_t *s=states[i];if(!readable(s,0x1a)) return FALSE;
        for(unsigned j=0;j<i;++j) if(v->states[j]==s) return FALSE;
        unsigned vt=(unsigned)((uintptr_t)*(void **)s-(uintptr_t)base),type=s[0x19],offset=0,size=0;
        if(i==0 && ((vt==0x2dc7b0 && type==7) || (vt==0x2dc710 && type==4))) {
            offset=type==7?0x38:0x44;size=type==7?0x68:0x70;
        } else if(i==1 && ((vt==0x2dc6a8 && type==2) || (vt==0x2dc880 && type==11) ||
            (vt==0x2dc748 && type==5) || (vt==0x2dc814 && type==9))) {
            offset=(type==2 || type==11)?0x38:0x44;size=offset==0x38?0x68:0x70;
        } else if(i==2 && vt==0x2dc7e4 && type==8) size=0x3c;
        else if(i==3 && vt==0x2dc5d4 && type==12) size=0x1c;
        else return FALSE;
        if(!readable(s,size)) return FALSE;
        v->states[i]=s;
        if(i==v->active) v->active_type=type;
        if(offset && request==s+offset) v->request_state=i;
    }
    uint8_t *manager=*(uint8_t **)(base+0x409de4);
    if(!readable(manager,0x128) || *(void **)(self+0x40)!=manager+0xf4) return FALSE;
    unsigned n=*(unsigned *)(manager+0x124),members=0;
    if(!n || n>4) return FALSE;
    v->formation=4;
    for(unsigned i=0;i<n;++i) {
        void *member=*(void **)(manager+0xf4+i*12);unsigned c=actor_seat(member);uint8_t *a=NULL;
        if(c>=4 || (members&(1u<<c)) || !actor_unit(roster.ai[c],&a) ||
            *(void **)((uint8_t *)roster.ai[c]+0x40)!=manager+0xf4) return FALSE;
        members|=1u<<c;if(!i) v->formation=c;
    }
    return (members&(1u<<actor_seat(actor)))!=0;
}
static BOOL party_position(unsigned c,float *x,float *z,unsigned *kind) {
    uint8_t *a=NULL,*ai=roster.ai[c];
    if(!actor_unit(ai,&a) || !position(a,x,z)) return FALSE;
    uint8_t *arb=*(uint8_t **)(a+0x90);
    if(!readable(arb,0x5c) || *(void **)arb!=base+0x2cc9ac || *(void **)(arb+0x10)!=a) return FALSE;
    *kind=(*(uint32_t *)(ai+0x44)&0x58) || (*(uint32_t *)(arb+0x50)&0x10000)?STORY_SEAT_DOWN:
        SudekiMpStoryThreatSeatKind(&seat_inputs[c],GetTickCount());
    return TRUE;
}
static void companion_sample(uint32_t now) {
    for(unsigned c=0;c<4;++c) {
        CompanionMemory *m=&companions[c];
        if(!(roster.available_mask&(1u<<c))) {m->anchor_plus_one=0;continue;}
        if(m->sampled_at && (uint32_t)(now-m->sampled_at)<1000) continue;
        uint8_t *a=NULL;CompanionView v;
        BOOL known=actor_unit(roster.ai[c],&a) && companion_view(roster.ai[c],a,NULL,&v);
        if(!known || v.row!=1 || v.mode[0xb]!=1 || config.level<STORY_TARGET_TIER0 ||
            !SudekiMpLanStoryRuntimeCompanionSeatExact(c)) m->anchor_plus_one=0;
        m->sampled_at=now;
        if(!known) {
            SudekiMpLogFormat("story_enemy_targeting event=companion_state char=%u known=0\r\n",c);continue;
        }
        SudekiMpLogFormat("story_enemy_targeting event=companion_state char=%u known=1 row=%u ai_mode=%u active=%u active_type=%u formation=%u kind=%s\r\n",
            c,v.row,v.mode[0xb],v.active,v.active_type,v.formation,SudekiMpStoryThreatSeatName(kinds[c]));
    }
}
static void companion_pick(uint8_t *request,void **out,uint8_t *self,uint8_t *actor,float range) {
    unsigned c=actor_seat(actor);CompanionView v;
    if(c>=4 || !companion_view(self,actor,request,&v) || v.request_state>=4) return;
    unsigned mode=request[0x24]&15,anchor=4;
    void *native=*out;
    /* Builders preserve bit 7 (the constructors set it) and the unused high
     * nibble of +25. Match only the fields consumed by these native requests. */
    if(v.row==1 && v.request_state==1 && mode==9 && (request[0x24]&0x7f)==0x79 &&
        (request[0x25]&15)==4 && request[0x26]==0xff && request[0x27]==0xff && range==-1 &&
        *(float *)(request+0xc)==-1 && (v.states[1][0x1c]&15)==9 &&
        *(int16_t *)(self+0x16a)==0 && v.mode[0xb]==1 &&
        SudekiMpLanStoryRuntimeCompanionSeatExact(c)) {
        float px[4]={0},pz[4]={0},distances[4]={0};unsigned live[4]={0},eligible=0;
        for(unsigned i=0;i<4;++i) if(roster.available_mask&(1u<<i)) {
            if(!party_position(i,&px[i],&pz[i],&live[i])) return;
            if(live[i]!=STORY_SEAT_DOWN && live[i]!=STORY_SEAT_ABSENT) eligible|=1u<<i;
        }
        for(unsigned i=0;i<4;++i) distances[i]=hypotf(px[i]-px[c],pz[i]-pz[c]);
        anchor=SudekiMpStoryThreatDefendAnchor(&config,cooperative,c,
            companions[c].anchor_plus_one?companions[c].anchor_plus_one-1:4,eligible,live,distances);
        if(anchor<4) {
            unsigned n=*(unsigned *)(base+0x3c30ac);uint8_t *best=NULL;float best_distance=INFINITY;
            if(n>32) return;
            for(unsigned i=0;i<n;++i) {
                uintptr_t p=*(uintptr_t *)(base+0x3d5540+i*12);
                uint8_t *candidate=p>=4?(uint8_t *)(p-4):NULL,*a=NULL;float x,z;
                if(!actor_unit(candidate,&a) || actor_seat(a)<4 || !manager_has(candidate) ||
                    !position(a,&x,&z)) return;
                /* The native list remains authoritative for side/range/filter;
                 * supported companion requests exclude type-3 bosses. */
                if(*(unsigned *)(candidate+0x148)==3 || (*(unsigned *)(candidate+0x44)&0x1058)) continue;
                float distance=hypotf(x-px[anchor],z-pz[anchor]);
                if(isfinite(distance) && distance<best_distance) {best=candidate;best_distance=distance;}
            }
            typedef unsigned (__stdcall *FilterFn)(void *,void *,float,void *);
            if(best && fresh() && (((FilterFn)(uintptr_t)(base+FILTER))(request,self,range,best)&0xff)) {
                *out=best;companions[c].anchor_plus_one=anchor+1;
                if(best!=native) ++companion_substitutions;
            }
        }
    } else if(v.row!=1 || v.mode[0xb]!=1 || *(int16_t *)(self+0x16a)!=0 ||
        !SudekiMpLanStoryRuntimeCompanionSeatExact(c)) companions[c].anchor_plus_one=0;
    /* Bound repeated weight/tick evaluations without caching their list or
     * selected enemy. Self-defense (mode 0) still returns its native answer. */
    CompanionMemory *m=&companions[c];uint32_t now=GetTickCount();
    if(m->selection_logs>=128 || (m->selection_at && (uint32_t)(now-m->selection_at)<250)) return;
    m->selection_at=now;++m->selection_logs;
    uint8_t *na=NULL;float nx=0,nz=0,sx=0,sz=0,fx=0,fz=0;unsigned ignored;
    BOOL native_known=!native || (actor_unit(native,&na) && manager_has(native) && position(na,&nx,&nz));
    BOOL distances_known=native && native_known && position(actor,&sx,&sz) &&
        party_position(v.formation,&fx,&fz,&ignored);
    SudekiMpLogFormat("story_enemy_targeting event=companion_selection char=%u row=%u active=%u evaluated=%u mode=%u formation=%u anchor=%u native_known=%u native_empty=%u native_resource=%08lx distances_known=%u self_distance=%.2f formation_distance=%.2f substituted=%u\r\n",
        c,v.row,v.active,v.request_state,mode,v.formation,anchor,native_known,!native,
        (unsigned long)(na?*(uint32_t *)(na+0x34):0),distances_known,
        distances_known?(double)hypotf(nx-sx,nz-sz):0,distances_known?(double)hypotf(nx-fx,nz-fz):0,*out!=native);
}
static BOOL lease_of(void *actor,SudekiMpStorySpawnLeaseObservation *lease);
static SudekiMpStoryThreatRecord *record_of(uint32_t serial) {
    for(unsigned i=0;serial && i<128;++i)
        if(identities[i].epoch==roster.epoch && identities[i].serial==serial) return &identities[i].memory;
    return NULL;
}
static EnemyFrame *frame_of(uint32_t serial) {
    for(unsigned i=0;serial && i<unit_count;++i)
        if(unit_table[i].serial==serial && unit_table[i].stamp==walk_sequence) return &unit_table[i];
    return NULL;
}
static BOOL excluded_unit(uint8_t *actor,uint32_t resource) {
    char name[48];entity_name(actor,name);
    return SudekiMpStoryThreatExcluded(&config,name) || SudekiMpStoryThreatResourceExcluded(&config,resource) ||
        SudekiMpStoryThreatResourceExcluded(&config,*(uint32_t *)(actor+0x34));
}
static const SudekiMpStoryThreatPushState (*push_view(void))[STORY_SET_COUNT] {
    /* C11 needs an explicit qualification conversion for nested arrays. */
    return (const SudekiMpStoryThreatPushState (*)[STORY_SET_COUNT])push_states;
}
static unsigned shadow_level(void) {
    SudekiMpStoryThreatConfig hypothetical=config;hypothetical.level=STORY_TARGET_TIER2;
    return SudekiMpStoryThreatEffectiveLevel(&hypothetical);
}
static unsigned set_level(unsigned set) {return set==STORY_SET_SHADOW?shadow_level():effective_level;}
static const char *end_name(unsigned reason) {
    static const char *const names[]={"none","level","down","stop","close","left","expired","invalid","engaged"};
    return reason<sizeof(names)/sizeof(names[0])?names[reason]:"unknown";
}
static BOOL event_budget(unsigned event) {
    if(event>=EVENT_COUNT || event_logs[event]>=64) return FALSE;
    ++event_logs[event];return TRUE;
}
static void alert_end_log(unsigned set,uint32_t serial,const char *reason) {
    if(event_budget(EVENT_ALERT_END)) SudekiMpLogFormat("story_enemy_targeting event=alert_end reason=%s serial=%lu watch=%u\r\n",reason,(unsigned long)serial,set==STORY_SET_SHADOW);
}
static void push_end_log(unsigned set,unsigned seat,uint32_t serial,const char *reason) {
    if(event_budget(EVENT_PUSH_END)) SudekiMpLogFormat("story_enemy_targeting event=push_end reason=%s seat=%u serial=%lu watch=%u\r\n",reason,seat,(unsigned long)serial,set==STORY_SET_SHADOW);
}
static BOOL copies_remain(unsigned set,uint32_t serial,uint32_t seq,const SudekiMpStoryThreatRecord *skip) {
    for(unsigned i=0;i<128;++i) {
        SudekiMpStoryThreatRecord *r=&identities[i].memory;
        if(identities[i].serial && r!=skip && r->responding[set].active &&
            r->responding[set].alerting_serial==serial && r->responding[set].raise_seq==seq) return TRUE;
    }
    return FALSE;
}
static BOOL alert_owner_live(unsigned set,const SudekiMpStoryThreatResponding *a) {
    SudekiMpStoryThreatRecord *r=record_of(a->alerting_serial);
    return r && r->alerting[set].active && r->alerting[set].raise_seq==a->raise_seq;
}
static BOOL shooter_alert_active(unsigned set,unsigned shooter) {
    for(unsigned i=0;i<128;++i) if(identities[i].serial) {
        SudekiMpStoryThreatRecord *r=&identities[i].memory;
        if((r->alerting[set].active && r->alerting[set].shooter==shooter) ||
            (r->responding[set].active && r->responding[set].shooter==shooter)) return TRUE;
    }
    return FALSE;
}
static void clear_designations(const char *reason) {
    for(unsigned set=0;set<STORY_SET_COUNT;++set) {
        for(unsigned i=0;i<128;++i) if(identities[i].serial) {
            SudekiMpStoryThreatRecord *r=&identities[i].memory;
            if(r->alerting[set].active) alert_end_log(set,r->serial,reason);
            SudekiMpStoryThreatResponding *a=&r->responding[set];
            if(a->active && !alert_owner_live(set,a)) {
                BOOL first=TRUE;
                for(unsigned j=0;j<i;++j) if(identities[j].serial && identities[j].memory.responding[set].active &&
                    identities[j].memory.responding[set].alerting_serial==a->alerting_serial &&
                    identities[j].memory.responding[set].raise_seq==a->raise_seq) {first=FALSE;break;}
                if(first) alert_end_log(set,a->alerting_serial,reason);
            }
        }
        for(unsigned seat=0;seat<4;++seat) if(push_states[seat][set].active)
            push_end_log(set,seat,push_states[seat][set].serial,reason);
    }
    for(unsigned i=0;i<128;++i) {
        memset(identities[i].memory.alerting,0,sizeof(identities[i].memory.alerting));
        memset(identities[i].memory.responding,0,sizeof(identities[i].memory.responding));
    }
    memset(push_states,0,sizeof(push_states));
}
static void retire_memory(SudekiMpStoryThreatRecord *r) {
    for(unsigned set=0;set<STORY_SET_COUNT;++set) {
        SudekiMpStoryThreatAlerting *a=&r->alerting[set];
        if(a->active) {
            if(copies_remain(set,r->serial,a->raise_seq,r)) {
                if(event_budget(EVENT_ALERT_RETIRED)) SudekiMpLogFormat("story_enemy_targeting event=alert_retired serial=%lu watch=%u\r\n",(unsigned long)r->serial,set==STORY_SET_SHADOW);
            } else alert_end_log(set,r->serial,"retired");
        }
        SudekiMpStoryThreatResponding *b=&r->responding[set];
        if(b->active && !(a->active && b->alerting_serial==r->serial && b->raise_seq==a->raise_seq) &&
            !alert_owner_live(set,b) && !copies_remain(set,b->alerting_serial,b->raise_seq,r))
            alert_end_log(set,b->alerting_serial,"retired");
    }
}
static void touch_shooter(unsigned shooter,uint32_t now) {
    for(unsigned i=0;i<128;++i) if(identities[i].serial)
        for(unsigned set=0;set<STORY_SET_COUNT;++set) SudekiMpStoryThreatTouch(&identities[i].memory,set,shooter,now);
}
static void raise_alert(SudekiMpStoryThreatRecord *r,unsigned shooter,unsigned kind,int added,uint32_t now) {
    EnemyFrame *origin=frame_of(r->serial);
    if(!table_complete || !origin) return;
    for(unsigned set=0;set<STORY_SET_COUNT;++set) {
        if(!SudekiMpStoryThreatAlertRaise(&config,set_level(set),cooperative,origin->type,origin->excluded,set,r,
            shooter,kind,added,shooter_alert_active(set,shooter),now)) continue;
        SudekiMpStoryThreatCandidate candidates[128];unsigned count=0;
        for(unsigned i=0;i<unit_count;++i) {
            EnemyFrame *u=&unit_table[i];SudekiMpStoryThreatRecord *candidate=record_of(u->serial);
            if(!candidate) continue;
            candidates[count++]=(SudekiMpStoryThreatCandidate){.serial=u->serial,.distance=hypotf(u->x-origin->x,u->z-origin->z),
                .ranged_capable=u->ranged_capable,.same_group=u->group_id && origin->group_id && u->group_id==origin->group_id,
                .type=u->type,.excluded=u->excluded,.substitutable=SudekiMpStoryThreatSubstitutable(candidate),
                .shooter_listed=SudekiMpStoryThreatShooterListed(&config,candidate,shooter,now),
                .designated=SudekiMpStoryThreatDesignated(candidate,set,push_view()),.current_target_seat=u->target_seat,
                .current_target_threat=u->target_seat<4 && u->target_seat!=shooter?
                    SudekiMpStoryThreatValue(&config,candidate,u->target_seat,0,now):0};
        }
        uint32_t selected[STORY_ALERT_RESPONDERS_MAX];SudekiMpStoryThreatRecord *records[STORY_ALERT_RESPONDERS_MAX];
        unsigned n=SudekiMpStoryThreatAlertResponders(&config,SudekiMpStoryThreatValue(&config,r,shooter,0,now),candidates,count,selected,STORY_ALERT_RESPONDERS_MAX);
        if(!n) {if(set==STORY_SET_LIVE) ++alert_empty;continue;}
        unsigned unknown=0;char serials[64]={0};size_t used=0;
        for(unsigned i=0;i<n;++i) {
            records[i]=record_of(selected[i]);unknown+=SudekiMpStoryThreatShooterListed(&config,records[i],shooter,now)==STORY_LIST_UNKNOWN;
            used+=(size_t)snprintf(serials+used,sizeof(serials)-used,"%s%lu",i?",":"",(unsigned long)selected[i]);
        }
        if(SudekiMpStoryThreatAlertCommit(&config,set,r,shooter,selected,n,records,now,list_seq)) {
            if(set==STORY_SET_LIVE) ++alerts;
            if(event_budget(EVENT_ALERT)) SudekiMpLogFormat("story_enemy_targeting event=alert serial=%lu shooter=%u responders=%u serials=%s unlisted=%u watch=%u\r\n",
                (unsigned long)r->serial,shooter,n,serials,unknown,set==STORY_SET_SHADOW);
        }
    }
}
static BOOL responder_close(unsigned shooter,uint32_t serial) {
    EnemyFrame *u=frame_of(serial);
    return table_complete && shooter<4 && !seat_frames[shooter].position_unknown && u &&
        hypotf(seat_frames[shooter].x-u->x,seat_frames[shooter].z-u->z)<=config.value[STORY_CLOSE_DISTANCE];
}
static uint32_t aware_pick(unsigned seat,unsigned set) {
    if(!table_complete || seat>=4 || seat_frames[seat].position_unknown) return 0;
    uint32_t best=0;float distance=INFINITY;
    for(unsigned i=0;i<unit_count;++i) {
        EnemyFrame *u=&unit_table[i];SudekiMpStoryThreatRecord *r=record_of(u->serial);
        if(!u->aware || u->type<1 || u->type>3 || u->excluded || !r ||
            SudekiMpStoryThreatDesignated(r,set,push_view()) || !SudekiMpStoryThreatSubstitutable(r)) continue;
        float d=hypotf(u->x-seat_frames[seat].x,u->z-seat_frames[seat].z);
        if(isfinite(d) && d<=config.value[STORY_IDLE_RADIUS] && d<distance) {best=u->serial;distance=d;}
    }
    return best;
}
static void maintain_memory(uint32_t now) {
    BOOL gap=push_tracked && (int32_t)(now-push_tracked_at)>250;
    push_tracked=TRUE;push_tracked_at=now;
    for(unsigned set=0;set<STORY_SET_COUNT;++set) for(unsigned c=0;c<4;++c) {
        if(!admitted || gap) SudekiMpStoryThreatPushTrack(set,&push_states[c][set],0,0,now);
        if(admitted) SudekiMpStoryThreatPushTrack(set,&push_states[c][set],table_complete,seat_frames[c].engaged,now);
    }
    if(!admitted) return;
    for(unsigned set=0;set<STORY_SET_COUNT;++set) {
        unsigned level=set_level(set);
        for(unsigned i=0;i<128;++i) if(identities[i].serial) {
            SudekiMpStoryThreatRecord *r=&identities[i].memory;SudekiMpStoryThreatAlerting *a=&r->alerting[set];
            if(!a->active || a->shooter>=4) continue;
            SudekiMpStoryThreatResponderView views[STORY_ALERT_RESPONDERS_MAX];unsigned count=0;
            for(unsigned j=0;j<a->count && j<STORY_ALERT_RESPONDERS_MAX;++j) {
                SudekiMpStoryThreatRecord *v=record_of(a->responders[j]);if(!v) continue;
                views[count++]=(SudekiMpStoryThreatResponderView){v,responder_close(a->shooter,v->serial),
                    table_complete?SudekiMpStoryThreatResponderListed(v,set):STORY_LIST_UNKNOWN};
            }
            uint32_t released[STORY_ALERT_RESPONDERS_MAX];unsigned n=0;
            unsigned reason=SudekiMpStoryThreatAlertFade(&config,level,cooperative,set,r,views,count,kinds[a->shooter],firing_signal[a->shooter],now,released,&n);
            for(unsigned j=0;j<n;++j) if(event_budget(EVENT_RESPONDER_RELEASED))
                SudekiMpLogFormat("story_enemy_targeting event=responder_released serial=%lu watch=%u\r\n",(unsigned long)released[j],set==STORY_SET_SHADOW);
            if(reason) {
                alert_end_log(set,r->serial,end_name(reason));
                for(unsigned j=0;j<count;++j) SudekiMpStoryThreatAlertClear(set,views[j].record,r->serial,a->raise_seq);
            }
        }
        for(unsigned i=0;i<128;++i) if(identities[i].serial) {
            SudekiMpStoryThreatRecord *r=&identities[i].memory;SudekiMpStoryThreatResponding *a=&r->responding[set];
            if(!a->active || a->shooter>=4 || alert_owner_live(set,a)) continue;
            unsigned reason=SudekiMpStoryThreatResponderFade(&config,level,cooperative,set,r,responder_close(a->shooter,r->serial),
                table_complete?SudekiMpStoryThreatResponderListed(r,set):STORY_LIST_UNKNOWN,kinds[a->shooter],firing_signal[a->shooter],now);
            if(reason) {
                uint32_t serial=a->alerting_serial,seq=a->raise_seq;
                SudekiMpStoryThreatAlertClear(set,r,serial,seq);
                if(event_budget(EVENT_RESPONDER_RELEASED)) SudekiMpLogFormat("story_enemy_targeting event=responder_released serial=%lu watch=%u\r\n",(unsigned long)r->serial,set==STORY_SET_SHADOW);
                if(!copies_remain(set,serial,seq,NULL)) alert_end_log(set,serial,"retired");
            }
        }
        for(unsigned c=0;c<4;++c) {
            SudekiMpStoryThreatPushState *p=&push_states[c][set];EnemyFrame *u=frame_of(p->serial);
            SudekiMpStoryThreatRecord *r=record_of(p->serial);unsigned others=seat_frames[c].engaged;
            if(others && u && u->target_seat==c) --others;
            unsigned reason=SudekiMpStoryThreatPushEnd(&config,level,cooperative,set,p,kinds[c],table_complete,others,r!=NULL,
                table_complete?SudekiMpStoryThreatPushListed(r,p,c):STORY_LIST_UNKNOWN,now);
            if(reason) push_end_log(set,c,p->serial,end_name(reason));
        }
    }
    for(unsigned c=0;c<4;++c) if(firing_signal[c]) touch_shooter(c,now);
    for(unsigned set=0;set<STORY_SET_COUNT;++set) for(unsigned c=0;c<4;++c)
        if(table_complete && SudekiMpStoryThreatPushEvaluate(&config,set_level(set),cooperative,set,&push_states[c][set],kinds[c],firing_signal[c],now)) {
            uint32_t serial=aware_pick(c,set);if(!serial) continue;
            SudekiMpStoryThreatPushStart(&config,set,&push_states[c][set],serial,now,list_seq);
            if(set==STORY_SET_LIVE) ++pushes;
            if(event_budget(EVENT_PUSH)) SudekiMpLogFormat("story_enemy_targeting event=push seat=%u serial=%lu watch=%u\r\n",c,(unsigned long)serial,set==STORY_SET_SHADOW);
        }
}

typedef struct SelectorView {
    unsigned eligible,kinds[4],current,host_standing;
    float distances[4];
} SelectorView;
static BOOL selector_view(uint8_t *self,uint8_t *actor,unsigned mode,SelectorView *v,uint32_t now) {
    if(mode!=0 && mode!=11) return FALSE;
    unsigned n=*(unsigned *)(base+0x3c30ac);if(n>32) return FALSE;
    float x,z;if(!position(actor,&x,&z)) return FALSE;
    memset(v,0,sizeof(*v));memcpy(v->kinds,kinds,sizeof(v->kinds));
    for(unsigned c=0;c<4;++c) v->distances[c]=INFINITY;
    for(unsigned i=0;i<n;++i) {
        uintptr_t p=*(uintptr_t *)(base+0x3d5540+i*12);
        uint8_t *candidate=p>=4?(uint8_t *)(p-4):NULL,*a=NULL;
        unsigned c=seat_of(candidate);float cx,cz;
        if(c>=4) continue;
        if(!actor_unit(candidate,&a) || !position(a,&cx,&cz)) return FALSE;
        uint8_t *arb=*(uint8_t **)(a+0x90);
        if(!readable(arb,0x5c) || *(void **)arb!=base+0x2cc9ac || *(void **)(arb+0x10)!=a) return FALSE;
        v->kinds[c]=((*(uint32_t *)(candidate+0x44)&0x58) || (*(uint32_t *)(arb+0x50)&0x10000))?
            STORY_SEAT_DOWN:SudekiMpStoryThreatSeatKind(&seat_inputs[c],now);
        v->distances[c]=hypotf(x-cx,z-cz);v->eligible|=1u<<c;
    }
    uintptr_t prior=*(uintptr_t *)(self+0xc0);v->current=prior>=4?seat_of((void *)(prior-4)):4;
    unsigned host=roster.leader_character;
    v->host_standing=host<4 && v->kinds[host]!=STORY_SEAT_DOWN && v->kinds[host]!=STORY_SEAT_ABSENT;
    if(!config.lead_always && v->host_standing) {
        uint8_t *ha=NULL,*hai=roster.ai[host];if(!actor_unit(hai,&ha)) return FALSE;
        uint8_t *arb=*(uint8_t **)(ha+0x90);
        if(!readable(arb,0x5c) || *(void **)arb!=base+0x2cc9ac || *(void **)(arb+0x10)!=ha) return FALSE;
        v->host_standing=!(*(uint32_t *)(hai+0x44)&0x58) && !(*(uint32_t *)(arb+0x50)&0x10000);
    }
    return TRUE;
}
static BOOL recheck_pick(uint8_t *request,uint8_t *self,float range,unsigned seat) {
    typedef unsigned (__stdcall *FilterFn)(void *,void *,float,void *);
    return seat<4 && fresh() && (((FilterFn)(uintptr_t)(base+FILTER))(request,self,range,roster.ai[seat])&0xff);
}
static void substitution_log(unsigned rule,unsigned native,unsigned pick,const SelectorView *v,uint32_t now) {
    static const char *const rules[]={"none","lead","threat","alert","push"};
    if(rule==STORY_RULE_LEAD) {
        ++lead_substitutions;
        if(pick!=v->current) {
            ++lead_changes;
            if(lead_logs<128 && (!lead_logs || (uint32_t)(now-lead_log_at)>=1000)) {
                ++lead_logs;lead_log_at=now;
                SudekiMpLogFormat("story_enemy_targeting event=lead_substitution native=%u current=%u chosen=%u kind=%s fallback=%u substitutions=%lu changes=%lu\r\n",
                    native,v->current,pick,SudekiMpStoryThreatSeatName(v->kinds[pick]),
                    v->kinds[pick]==STORY_SEAT_AI || v->kinds[pick]==STORY_SEAT_AWAY,(unsigned long)lead_substitutions,(unsigned long)lead_changes);
            }
        }
    } else {
        ++threat_substitutions;
        if(pick!=v->current) {
            ++threat_changes;
            if(threat_logs<128 && (!threat_logs || (uint32_t)(now-threat_log_at)>=1000)) {
                ++threat_logs;threat_log_at=now;
                SudekiMpLogFormat("story_enemy_targeting event=threat_substitution rule=%s native=%u current=%u chosen=%u substitutions=%lu changes=%lu\r\n",
                    rules[rule],native,v->current,pick,(unsigned long)threat_substitutions,(unsigned long)threat_changes);
            }
        }
    }
}
static SudekiMpStoryThreatDecision enemy_pick(uint8_t *request,void **out,uint8_t *self,uint8_t *actor,float range) {
    SudekiMpStoryThreatDecision shadow={4,STORY_RULE_NONE};unsigned mode=request[0x24]&15;uint32_t now=GetTickCount();
    SudekiMpStorySpawnLeaseObservation lease={0};BOOL leased=lease_of(actor,&lease);
    if(excluded_unit(actor,leased?lease.resource:0) || !fresh()) return shadow;
    SudekiMpStoryThreatRecord *r=leased?record_of(lease.serial):NULL;
    if(r) {r->mode_seen=1;r->last_mode=mode;}
    if(mode!=0 && mode!=11) {
        if(mode==14 && r && SudekiMpStoryThreatDesignated(r,STORY_SET_LIVE,push_view())) {
            unsigned designated=4;
            if(r->responding[0].active) designated=r->responding[0].shooter;
            else for(unsigned c=0;c<4;++c) if(push_states[c][0].active && push_states[c][0].serial==r->serial) {designated=c;break;}
            if(designated<4 && seat_of(*out)!=designated) {++r->lock_blocked;++lock_blocked;}
        }
        return shadow; /* Mode E never reads the global list or substitutes. */
    }
    SelectorView v;if(!selector_view(self,actor,mode,&v,now)) return shadow;
    ++list_seq;
    if(r) {r->list_seen=1;r->last_list_mask=v.eligible;r->last_list_at=now;r->last_list_seq=list_seq;}
    unsigned type=*(uint32_t *)(self+0x148);
    unsigned lead=SudekiMpStoryThreatLeadPick(&config,cooperative,type,FALSE,mode,v.host_standing,v.current,v.eligible,v.kinds,v.distances);
    SudekiMpStoryThreatDecision live=SudekiMpStoryThreatDecide(&config,effective_level,cooperative,type,FALSE,mode,STORY_SET_LIVE,
        r,push_view(),table_complete,now,v.current,v.eligible,v.kinds,v.distances,lead);
    SudekiMpStoryThreatConfig hypothetical=config;hypothetical.level=STORY_TARGET_TIER2;
    unsigned shadow_lead=SudekiMpStoryThreatLeadPick(&hypothetical,cooperative,type,FALSE,mode,v.host_standing,v.current,v.eligible,v.kinds,v.distances);
    shadow=SudekiMpStoryThreatDecide(&hypothetical,shadow_level(),cooperative,type,FALSE,mode,STORY_SET_SHADOW,
        r,push_view(),table_complete,now,v.current,v.eligible,v.kinds,v.distances,shadow_lead);
    if(live.seat<4 && *out!=roster.ai[live.seat]) {
        unsigned native=seat_of(*out);
        if(recheck_pick(request,self,range,live.seat)) {
            *out=roster.ai[live.seat];substitution_log(live.rule,native,live.seat,&v,now);
        } else {
            if(r && live.rule==STORY_RULE_ALERT) r->responding[0].recheck_failed=1;
            if(r && live.rule==STORY_RULE_PUSH) push_states[live.seat][0].recheck_failed=1;
            if(live.rule!=STORY_RULE_LEAD && mode==11 && lead<4 && *out!=roster.ai[lead] && recheck_pick(request,self,range,lead)) {
                *out=roster.ai[lead];substitution_log(STORY_RULE_LEAD,native,lead,&v,now);
            }
        }
    }
    return shadow;
}

static void * __stdcall select_target(void *request,void **out,void *self,float range) {
    InterlockedIncrement(&calls);
    Request scope={NULL,request,self,0,0};BOOL observe=FALSE;uint8_t *actor=NULL;
    if(fresh() && readable(request,0x28) && readable(out,4) && actor_unit(self,&actor) &&
        actor_seat(actor)==4 && manager_has(self)) {
        scope.parent=current;current=&scope;observe=TRUE;
    }
    void *result=((SelectFn)(uintptr_t)seams[0].trampoline)(request,out,self,range);
    DWORD error=GetLastError();
    if(observe) {
        unsigned native_seat=seat_of(*out);SudekiMpStoryThreatDecision shadow={4,STORY_RULE_NONE};
        if(fresh() && actor_unit(self,&actor) && manager_has(self)) shadow=enemy_pick(request,out,self,actor,range);
        current=scope.parent;
        if(fresh() && actor_unit(self,&actor) && manager_has(self)) {
            unsigned mode=((uint8_t *)request)[0x24]&15,c=seat_of(*out);
            uint32_t resource=*(uint32_t *)(actor+0x34);Counts *entry=NULL;
            for(unsigned i=0;i<128;++i) {
                if(counters[i].count && counters[i].resource==resource && counters[i].mode==mode) {entry=&counters[i];break;}
                if(!entry && !counters[i].count) entry=&counters[i];
            }
            if(entry) {
                if(!entry->count) {entry->resource=resource;entry->mode=mode;entity_name(actor,entry->name);}
                ++entry->count;entry->empty+=!*out;entry->cap_only+=scope.cap_only!=0;
                entry->native_absent+=native_seat<4 && (kinds[native_seat]==STORY_SEAT_ABSENT || kinds[native_seat]==STORY_SEAT_DOWN);
                if(c<4) ++entry->picks[c];
                if(shadow.seat<4 && shadow.seat!=c && shadow.rule<STORY_RULE_COUNT) ++entry->would[shadow.rule];
                for(unsigned seat=0;seat<4;++seat) {
                    entry->cap_refusals[seat]+=(scope.cap_only>>seat)&1;
                    entry->cap_parity[seat]+=(scope.parity>>seat)&1;
                }
            }
            /* Mode E never rebuilt the globals. Do not even read them. */
            if(mode==0 || mode==11) {
                unsigned n=*(unsigned *)(base+0x3c30ac);
                if(n<=32) {
                    unsigned mask=0;
                    for(unsigned i=0;i<n;++i) {
                        uintptr_t p=*(uintptr_t *)(base+0x3d5540+i*12);
                        unsigned seat=p>=4?seat_of((void *)(p-4)):4;
                        if(seat<4) mask|=1u<<seat;
                    }
                    if(entry && first_type(resource,mode))
                        SudekiMpLogFormat("story_enemy_targeting event=type type=%s resource=%08lx mode=%u list_mask=%u excluded=%u substitutions_total=%lu\r\n",
                            entry->name,(unsigned long)resource,mode,mask,SudekiMpStoryThreatExcluded(&config,entry->name),(unsigned long)lead_substitutions);
                }
            }
        }
    } else if(fresh() && readable(request,0x28) && readable(out,4) && actor_unit(self,&actor) &&
        actor_seat(actor)<4 && manager_has(self)) companion_pick(request,out,self,actor,range);
    InterlockedDecrement(&calls);SetLastError(error);return result;
}
void SudekiMpStoryEnemyTargetingConfigure(const wchar_t *path) {
    if(base) return;
    ini[0]=0;if(path && wcslen(path)<MAX_PATH) wcscpy(ini,path);
}
static void read_config(uint32_t now) {
    if(config_at && (uint32_t)(now-config_at)<1000) return;
    config_at=now;SudekiMpStoryThreatConfig next;SudekiMpStoryThreatDefaults(&next);
    const char *extra[]={"EnemyTargeting","EnemyTargetingCapParity","EnemyTargetingLeadRule","EnemyTargetingExcludeTypes"};
    for(unsigned i=0;ini[0] && i<STORY_THRESHOLD_COUNT+4;++i) {
        const char *key=i<4?extra[i]:SudekiMpStoryThreatKeys[i-4];wchar_t wide[80],value[512];char text[512];
        MultiByteToWideChar(CP_UTF8,0,key,-1,wide,80);
        if(!GetPrivateProfileStringW(L"StoryAreas",wide,L"",value,512,ini)) continue;
        if(!WideCharToMultiByte(CP_UTF8,0,value,-1,text,512,NULL,NULL) || !SudekiMpStoryThreatParse(&next,key,text))
            SudekiMpLogFormat("story_enemy_targeting event=invalid_config key=%s\r\n",key);
    }
    unsigned next_level=SudekiMpStoryThreatEffectiveLevel(&next);
    if(next_level!=next.level && !level_clamped) {
        level_clamped=TRUE;SudekiMpLogWrite("story_enemy_targeting event=level_clamped effective=tier0\r\n");
    }
    if(!level_logged || effective_level!=next_level) {
        static const char *const levels[]={"off","watch","tier0","tier1","tier15","tier2"};
        if(level_logged) clear_designations("level");
        effective_level=next_level;level_logged=TRUE;
        SudekiMpLogFormat("story_enemy_targeting event=level effective=%s\r\n",levels[effective_level]);
    }
    config=next;
}
static BOOL lease_of(void *actor,SudekiMpStorySpawnLeaseObservation *lease) {
    SudekiMpStorySpawnLeaseObservation entries[128];unsigned n;
    if(!SudekiMpLanStorySpawnLeasesObserve(entries,128,&n) || n>128) return FALSE;
    for(unsigned i=0;i<n;++i) if(entries[i].actor==actor && !entries[i].departed) {*lease=entries[i];return TRUE;}
    return FALSE;
}
typedef struct Hit {
    void *actor,*stats;
    uint32_t epoch,serial,resource;
    float hp,max_hp,distance;
    unsigned source,kind,is_far,ranged_mode,missile_known,missile,spirit_known,spirit,firing;
} Hit;
_Static_assert(sizeof(Hit)<=192,"shared hit capture capacity");
static BOOL hit_identity(void *combat,Hit *h,BOOL after) {
    uint8_t *co=combat;
    if(!fresh() || !readable(co,0x74) || *(void **)co!=base+0x2c8754) return FALSE;
    uint8_t *a=*(uint8_t **)(co+0x10);
    if(!readable(a,0xb4) || *(void **)(a+0xa4)!=combat) return FALSE;
    unsigned c=actor_seat(a);SudekiMpStorySpawnLeaseObservation lease={0};
    if(c==4) {
        void *vt=*(void **)a;
        if(vt!=base+0x2d5fa0 && vt!=base+0x2d55d4 && vt!=base+0x2d69ec) return FALSE;
        if(!lease_of(a,&lease)) return FALSE; /* unreadable/unleased is unknown */
    }
    uint8_t *stats=*(uint8_t **)(a+0x4c);
    if(!readable(stats,0x34) || *(void **)stats!=base+0x2cc064 || *(void **)(stats+0x10)!=a ||
        !isfinite(*(float *)(stats+0x2c)) || !isfinite(*(float *)(stats+0x30))) return FALSE;
    if(after) return h->actor==a && h->stats==stats && h->epoch==roster.epoch && h->serial==lease.serial;
    h->actor=a;h->stats=stats;h->epoch=roster.epoch;h->serial=lease.serial;h->resource=lease.resource;
    h->hp=*(float *)(stats+0x2c);h->max_hp=*(float *)(stats+0x30);return TRUE;
}
static void memory_hit(const Hit *h,float amount,uint32_t now) {
    if(h->source>=4) return;
    uint8_t *actor=h->actor,*ai=*(uint8_t **)(actor+0x94),*proven=NULL;
    SudekiMpStoryThreatRecord *r=NULL;SudekiMpStoryThreatHitAdded added={0};
    if(actor_seat(actor)==4 && actor_unit(ai,&proven) && proven==actor) {
        r=record_of(h->serial);
        if(!r) ++hit_unslotted;
        else {
            SeatObservation *s=&seat_frames[h->source];
            added=SudekiMpStoryThreatAddHit(&config,cooperative,*(uint32_t *)(ai+0x148),
                excluded_unit(actor,h->resource),r,h->source,kinds[h->source],amount,h->max_hp,
                !h->spirit && (h->missile || h->ranged_mode),h->spirit_known,h->spirit,h->firing,
                table_complete && s->melee==0 && !s->position_unknown && s->nearest>config.value[STORY_MELEE_REACH],
                h->distance>config.value[STORY_FAR_DISTANCE],now);
        }
    }
    /* An attributed hit anywhere refreshes every alert on this shooter,
     * including a hit on an ally or a victim without a memory slot. */
    touch_shooter(h->source,now);
    if(added.added_sniper) raise_alert(r,h->source,kinds[h->source],TRUE,now);
}
static BOOL hit_observe(void *combat,void *packet,BOOL after,void *storage) {
    DWORD error=GetLastError();Hit *h=storage;BOOL result=FALSE;
    if(!after) {
        if(!fresh()) goto done;
        if(!readable(packet,0x68) || *(void **)packet!=base+0x2d9148 || !hit_identity(combat,h,FALSE)) {
            ++hit_unknown;goto done;
        }
        uint8_t *p=packet,*source=*(uint8_t **)(p+0x30);
        h->source=source==h->actor?4:actor_seat(source);
        if(h->source<4) {
            h->kind=kinds[h->source];h->firing=firing_signal[h->source];
            uint32_t seq,tick;
            if(SudekiMpLanStoryShotsLatest(h->source,&seq,&tick) && seq &&
                (uint32_t)(GetTickCount()-tick)<=config.value[STORY_FIRE_RECENT]*1000)
                h->firing|=h->kind==STORY_SEAT_HOST || h->kind==STORY_SEAT_REMOTE;
            BOOL spirit=FALSE,match=FALSE;
            h->spirit_known=SudekiMpLanStoryCastSpiritInFlight(h->source,&spirit);h->spirit=spirit;
            h->missile_known=SudekiMpLanPartyProjectileDamageMatch(source,*(void **)(p+0x3c),&match);h->missile=match;
            float x,z,sx,sz;
            if(position(h->actor,&x,&z) && position(source,&sx,&sz)) {
                h->distance=hypotf(x-sx,z-sz);h->is_far=h->distance>config.value[STORY_MELEE_REACH];
            } else h->distance=-1;
            uint8_t *arb=*(uint8_t **)(source+0x90);
            if(readable(arb,0x5c) && *(void **)arb==base+0x2cc9ac && *(void **)(arb+0x10)==source)
                h->ranged_mode=((*(uint32_t *)(arb+0x58)>>4)&15)==2;
        }
        result=TRUE;
    } else {
        ++hit_count;
        if(!hit_identity(combat,h,TRUE) || !readable(packet,0x68) || *(void **)packet!=base+0x2d9148) {
            ++hit_unknown;goto done;
        }
        float hp=*(float *)((uint8_t *)h->stats+0x2c),amount=fmaxf(0,h->hp-hp);
        uint8_t *p=packet;
        SudekiMpLogFormat("story_enemy_targeting event=hit epoch=%lu serial=%lu resource=%08lx source=%u seat_kind=%s amount=%.3f max_hp=%.3f lethal=%u distance=%.3f far=%u missile_known=%u missile=%u arbiter_ranged=%u spirit_known=%u hit_all=%u ranged=%u far_not_ranged=%u firing=%u result=%u class=%u flags=%u\r\n",
            (unsigned long)h->epoch,(unsigned long)h->serial,(unsigned long)h->resource,h->source,
            h->source<4?SudekiMpStoryThreatSeatName(h->kind):"unattributable",(double)amount,(double)h->max_hp,
            hp<=0 && h->hp>0,(double)h->distance,h->is_far&&!h->spirit,h->missile_known,h->missile,h->ranged_mode,
            h->spirit_known,h->spirit,!h->spirit&&(h->missile||h->ranged_mode),
            !h->spirit&&h->is_far&&!h->missile&&!h->ranged_mode,h->firing,p[0x61],p[0x64],p[0x67]);
        memory_hit(h,amount,GetTickCount());result=TRUE;
    }
done:SetLastError(error);return result;
}
static void report(uint32_t now) {
    if(report_at && now-report_at<1000) return;
    report_at=now;
    SudekiMpLogFormat("story_enemy_targeting event=status installed=yes active=%s known=%u hits=%lu hit_unknown=%lu substitutions=%lu changes=%lu companion_substitutions=%lu lock_substitutions=0 cap_parity=%s threat_substitutions=%lu alerts=%lu alert_empty=%lu pushes=%lu lock_blocked=%lu hit_unslotted=%lu\r\n",
        admitted&&cooperative&&config.level!=STORY_TARGET_OFF?"yes":"no",admitted,
        (unsigned long)hit_count,(unsigned long)hit_unknown,(unsigned long)lead_substitutions,
        (unsigned long)lead_changes,(unsigned long)companion_substitutions,
        SudekiMpStoryThreatCapParity(&config,admitted&&cooperative,STORY_SEAT_REMOTE,1,FALSE)?"on":"off",
        (unsigned long)threat_substitutions,(unsigned long)alerts,(unsigned long)alert_empty,(unsigned long)pushes,
        (unsigned long)lock_blocked,(unsigned long)hit_unslotted);
    for(unsigned i=0;i<128;++i) {
        Counts *e=&counters[i];if(!e->count) continue;
        SudekiMpLogFormat("story_enemy_targeting event=selection type=%s resource=%08lx mode=%lu requests=%lu empty=%lu cap_only_requests=%lu native_absent=%lu picks=%lu,%lu,%lu,%lu cap_refusals=%lu,%lu,%lu,%lu cap_parity_requests=%lu,%lu,%lu,%lu would_lead=%lu would_threat=%lu would_alert=%lu would_push=%lu\r\n",
            e->name,(unsigned long)e->resource,(unsigned long)e->mode,(unsigned long)e->count,
            (unsigned long)e->empty,(unsigned long)e->cap_only,(unsigned long)e->native_absent,
            (unsigned long)e->picks[0],(unsigned long)e->picks[1],(unsigned long)e->picks[2],(unsigned long)e->picks[3],
            (unsigned long)e->cap_refusals[0],(unsigned long)e->cap_refusals[1],
            (unsigned long)e->cap_refusals[2],(unsigned long)e->cap_refusals[3],
            (unsigned long)e->cap_parity[0],(unsigned long)e->cap_parity[1],
            (unsigned long)e->cap_parity[2],(unsigned long)e->cap_parity[3],
            (unsigned long)e->would[STORY_RULE_LEAD],(unsigned long)e->would[STORY_RULE_THREAT],
            (unsigned long)e->would[STORY_RULE_ALERT],(unsigned long)e->would[STORY_RULE_PUSH]);
    }
    memset(counters,0,sizeof(counters));
}
static void walk(uint32_t now) {
    uint8_t *list;unsigned count;unit_count=0;table_complete=FALSE;
    memset(seat_frames,0,sizeof(seat_frames));
    for(unsigned c=0;c<4;++c) {
        SeatObservation *s=&seat_frames[c];s->kind=kinds[c];s->firing=firing_signal[c];
        s->nearest=s->melee_distance=INFINITY;
        s->position_unknown=!position(roster.actors[c],&s->x,&s->z);
    }
    if(!manager_units(&list,&count)) return; /* unknown does not retire */
    SudekiMpStorySpawnLeaseObservation leases[128];unsigned lease_count=0;
    BOOL leases_known=SudekiMpLanStorySpawnLeasesObserve(leases,128,&lease_count) && lease_count<=128;
    table_complete=leases_known;++walk_sequence;
    for(unsigned i=0;i<count;++i) {
        uintptr_t p=*(uintptr_t *)(list+i*12);if(p<4) continue;
        uint8_t *ai=(uint8_t *)(p-4),*a;
        if(!actor_unit(ai,&a) || actor_seat(a)<4) continue;
        const SudekiMpStorySpawnLeaseObservation *lease=NULL;
        if(leases_known) for(unsigned j=0;j<lease_count;++j)
            if(leases[j].serial && leases[j].actor==a && !leases[j].departed) {lease=&leases[j];break;}
        if(lease) {
            unsigned slot=128;
            for(unsigned k=0;k<128;++k) {
                if(identities[k].serial==lease->serial && identities[k].epoch==roster.epoch) {slot=k;break;}
                if(slot==128 && !identities[k].serial) slot=k;
            }
            if(slot<128) {
                if(!identities[slot].serial) {
                    SudekiMpLogFormat("story_enemy_targeting event=enemy epoch=%lu serial=%lu resource=%08lx group_id=%08lx group=%s native_ranged=%u\r\n",
                        (unsigned long)roster.epoch,(unsigned long)lease->serial,(unsigned long)lease->resource,
                        (unsigned long)lease->group_id,lease->group,(unsigned)(ai[0x46]&1));
                    identities[slot].memory.serial=lease->serial;
                }
                identities[slot].serial=lease->serial;identities[slot].epoch=roster.epoch;identities[slot].seen=walk_sequence;
            } else table_complete=FALSE;
        }
        if(ai[0x3b] || (*(uint32_t *)(ai+0x44)&(0x58|0x10|0x1000))) continue;
        uintptr_t target=*(uintptr_t *)(ai+0xc0);unsigned c=target>=4?seat_of((void *)(target-4)):4;
        float x=0,z=0;BOOL positioned=position(a,&x,&z);
        if(c<4) {
            SeatObservation *s=&seat_frames[c];++s->engaged;s->unleased+=!lease;
            s->melee+=(ai[0x46]&1)==0;s->ranged+=(ai[0x46]&1)!=0;
            s->in_melee+=(ai[0xf2]&1)!=0;s->attacking+=(*(uint32_t *)(ai+0x44)&0x480)!=0;
            if(!positioned) s->position_unknown=TRUE;
            else if(!s->position_unknown) {
                float distance=hypotf(x-s->x,z-s->z);
                if(distance<s->nearest) s->nearest=distance;
                if((ai[0xf2]&1) && distance<s->melee_distance) s->melee_distance=distance;
            }
        }
        if(lease && positioned) {
            if(unit_count>=128) {table_complete=FALSE;continue;}
            unit_table[unit_count++]=(EnemyFrame){.serial=lease->serial,.stamp=walk_sequence,.group_id=lease->group_id,
                .x=x,.z=z,.type=*(uint32_t *)(ai+0x148),.excluded=excluded_unit(a,lease->resource),
                .ranged_capable=(ai[0x46]&1)!=0,.target_seat=c,.aware=!target && *(uintptr_t *)(ai+0xf4)!=0};
        }
    }
    if(leases_known) for(unsigned i=0;i<128;++i) if(identities[i].serial && identities[i].seen!=walk_sequence) {
        retire_memory(&identities[i].memory);
        SudekiMpLogFormat("story_enemy_targeting event=retired epoch=%lu serial=%lu reason=manager_or_lease_absent\r\n",
            (unsigned long)identities[i].epoch,(unsigned long)identities[i].serial);memset(&identities[i],0,sizeof(identities[i]));
    }
    static uint32_t last;if(last && now-last<1000) return;last=now;
    for(unsigned c=0;c<4;++c) if(roster.available_mask&(1u<<c)) {
        uint8_t *ai=roster.ai[c];if(!readable(ai,0x16c)) continue;
        SeatObservation *s=&seat_frames[c];unsigned mc=ai[0x47]&15,rc=(*(uint16_t *)(ai+0x46)>>12)&15;
        if(mc>s->melee || rc>s->ranged)
            SudekiMpLogFormat("story_enemy_targeting event=stale_count character=%u native_melee=%u native_ranged=%u walk_melee=%u walk_ranged=%u engaged=%u\r\n",c,mc,rc,s->melee,s->ranged,s->engaged);
        uint32_t sequence=0,tick=0;BOOL shots=SudekiMpLanStoryShotsLatest(c,&sequence,&tick);
        float x,z,speed=-1;
        if(position(roster.actors[c],&x,&z)) {
            if(scale[c].epoch==roster.epoch && scale[c].tick && now!=scale[c].tick && now-scale[c].tick<=2000)
                speed=hypotf(x-scale[c].x,z-scale[c].z)*1000.0f/(now-scale[c].tick);
            scale[c].epoch=roster.epoch;scale[c].tick=now;scale[c].x=x;scale[c].z=z;
        }
        SudekiMpLogFormat("story_enemy_targeting event=seat character=%u kind=%s firing=%u engaged=%u attacking=%u in_melee=%u shot_known=%u shot=%lu shot_tick=%lu speed_units_per_s=%.3f nearest_targeting_units=%.3f native_melee_units=%.3f melee_reach=%.3f unleased=%u position_unknown=%u table_complete=%u\r\n",
            c,SudekiMpStoryThreatSeatName(kinds[c]),firing_signal[c],s->engaged,s->attacking,s->in_melee,shots,
            (unsigned long)sequence,(unsigned long)tick,(double)speed,isfinite(s->nearest)?(double)s->nearest:-1.0,
            isfinite(s->melee_distance)?(double)s->melee_distance:-1.0,(double)config.value[STORY_MELEE_REACH],s->unleased,s->position_unknown,table_complete);
    }
}

void SudekiMpStoryEnemyTargetingSuspend(void) {
    /* The runtime suspends before every frame's admission proof. Retain only
     * plain character indices across this gap; clearing here defeats stickiness. */
    admitted=FALSE;
}
void SudekiMpStoryEnemyTargetingService(const SudekiMpLanStoryNativeRoster *r,
    const SudekiMpLanStoryScene *scene,const SudekiMpStoryThreatSeat seats[4],
    const unsigned firing[4],const SudekiMpLanPartyLease away_keys[4],BOOL coop,uint32_t now) {
    if(!installed || thread!=GetCurrentThreadId()) return;
    admitted=FALSE;cooperative=coop;table_complete=FALSE;unit_count=0;read_config(now);
    memset(away_connections,0,sizeof(away_connections));
    if(r && scene && seats && firing && SudekiMpLanStoryObserverNativeRosterExact(r)) {
        if(sampled_at && (uint32_t)(now-sampled_at)>250)
            for(unsigned c=0;c<4;++c) {companions[c].anchor_plus_one=0;companions[c].sampled_at=0;}
        if(roster.epoch!=r->epoch || roster.revision!=r->revision) {
            clear_designations("namespace");push_tracked=FALSE;list_seq=0;
            for(unsigned i=0;i<128;++i) if(identities[i].serial)
                SudekiMpLogFormat("story_enemy_targeting event=retired epoch=%lu serial=%lu reason=namespace_changed\r\n",
                    (unsigned long)identities[i].epoch,(unsigned long)identities[i].serial);
            memset(identities,0,sizeof(identities));memset(scale,0,sizeof(scale));memset(counters,0,sizeof(counters));
            memset(companions,0,sizeof(companions));reset_selection_logs();
        }
        roster=*r;sampled_at=now;admitted=TRUE;
        if(away_keys) memcpy(away_connections,away_keys,sizeof(away_connections));
        for(unsigned c=0;c<4;++c) {
            SudekiMpStoryThreatSeat seat=seats[c];seat.host=c==r->leader_character;
            seat.absent|=!(r->available_mask&(1u<<c)) || (scene->inside_mask&(1u<<c)) ||
                !SudekiMpLanStorySceneSameArea(scene,r->leader_character,c);
            uint8_t *a=r->actors[c],*ai=r->ai[c];
            if(!readable(a,0x94) || !readable(ai,0x16c)) {kinds[c]=STORY_SEAT_ABSENT;continue;}
            uint8_t *arb=*(uint8_t **)(a+0x90),*mode=*(uint8_t **)(ai+0x3c);
            if(!readable(arb,0x5c) || *(void **)arb!=base+0x2cc9ac || *(void **)(arb+0x10)!=a || !readable(mode,12)) {
                admitted=FALSE;break;
            }
            seat.down=(*(uint32_t *)(arb+0x50)&0x10000) || (*(uint32_t *)(ai+0x44)&0x58);
            seat.native_ai=*(int16_t *)(ai+0x16a)==0 && mode[0xb]==1;
            seat_inputs[c]=seat;kinds[c]=SudekiMpStoryThreatSeatKind(&seat,now);
            firing_signal[c]=kinds[c]==STORY_SEAT_HOST?!!(*(uint32_t *)(arb+0x50)&0x400000):
                kinds[c]==STORY_SEAT_REMOTE?firing[c]:0;
            uint32_t seq,tick;if(SudekiMpLanStoryShotsLatest(c,&seq,&tick) && seq &&
                (uint32_t)(now-tick)<=config.value[STORY_FIRE_RECENT]*1000)
                firing_signal[c]|=kinds[c]==STORY_SEAT_HOST || kinds[c]==STORY_SEAT_REMOTE;
        }
        if(admitted && coop && config.level!=STORY_TARGET_OFF) {companion_sample(now);walk(now);}
    }
    if(!admitted || !coop || config.level<STORY_TARGET_TIER0)
        for(unsigned c=0;c<4;++c) companions[c].anchor_plus_one=0;
    maintain_memory(now);report(now);
}
BOOL SudekiMpStoryEnemyTargetingBindThread(DWORD verified) {
    if(!installed || !verified || verified!=GetCurrentThreadId() || (thread && thread!=verified)) return FALSE;
    if(!SudekiMpLanHitObserverBindThread(verified)) return FALSE;
    thread=verified;
    if(!subscribed) {if(!SudekiMpLanHitObserverSubscribe(hit_observe)) return FALSE;subscribed=TRUE;}
    return TRUE;
}
BOOL SudekiMpStoryEnemyTargetingInstall(HMODULE image) {
    if(base || !image || !SudekiMpLanHitObserverVacant() || !SudekiMpCheckLoadedExecutable(image) || !SudekiMpStoryEnemyTargetingImageExact((uint8_t *)image)) {
        SetLastError(ERROR_INVALID_DATA);return FALSE;
    }
    base=(uint8_t *)image;level_logged=level_clamped=FALSE;config_at=0;
    SudekiMpStoryThreatDefaults(&config);read_config(GetTickCount());
    SudekiMpStoryEnemyCapNext[0]=base+CAP_MELEE+8;SudekiMpStoryEnemyCapNext[1]=base+CAP_RANGED+8;
    /* Both branch targets are covered by the exact full FILTER body hash. */
    SudekiMpStoryEnemyCapAuthored[0]=base+0x1b7183;SudekiMpStoryEnemyCapAuthored[1]=base+0x1b71d1;
    if(!SudekiMpInstallInlineHook(&seams[0],base+SELECTOR,prefix,sizeof(prefix),(void *)(uintptr_t)select_target) ||
        !SudekiMpInstallInlineHook(&seams[1],base+CAP_MELEE,compare,sizeof(compare),(void *)(uintptr_t)cap_melee) ||
        !SudekiMpInstallInlineHook(&seams[2],base+CAP_RANGED,compare,sizeof(compare),(void *)(uintptr_t)cap_ranged)) goto fail;
    hit_owned=TRUE;
    if(!SudekiMpLanHitObserverInstall(image)) goto fail;
    installed=TRUE;
    SudekiMpLogFormat("story_enemy_targeting event=installed active=no configured_level=%u ai_damage_multiplier=%.6f substitutions=0\r\n",
        config.level,(double)*(float *)(base+0x33aff0));return TRUE;
fail: {DWORD error=GetLastError();(void)SudekiMpStoryEnemyTargetingUninstall();SetLastError(error);return FALSE;}
}
BOOL SudekiMpStoryEnemyTargetingUninstall(void) {
    if(InterlockedCompareExchange(&calls,0,0) || current || (thread && thread!=GetCurrentThreadId())) {
        SetLastError(ERROR_BUSY);return FALSE;
    }
    admitted=FALSE;
    if(subscribed) {
        if(!SudekiMpLanHitObserverUnsubscribe(hit_observe)) return FALSE;
        subscribed=FALSE;
    }
    BOOL ok=TRUE;DWORD error=0;
    if(hit_owned) {
        if(!SudekiMpLanHitObserverUninstall()) {ok=FALSE;error=GetLastError();}
        else hit_owned=FALSE;
    }
    for(unsigned i=3;i-->0;) if(!SudekiMpRestoreInlineHook(&seams[i])) {if(!error) error=GetLastError();ok=FALSE;}
    if(!ok) {SetLastError(error);return FALSE;}
    base=NULL;thread=0;installed=cooperative=FALSE;sampled_at=config_at=report_at=0;
    level_logged=level_clamped=push_tracked=table_complete=FALSE;effective_level=unit_count=0;
    push_tracked_at=walk_sequence=list_seq=0;memset(push_states,0,sizeof(push_states));
    memset(unit_table,0,sizeof(unit_table));memset(seat_frames,0,sizeof(seat_frames));
    memset(&roster,0,sizeof(roster));memset(counters,0,sizeof(counters));memset(identities,0,sizeof(identities));
    memset(scale,0,sizeof(scale));memset(SudekiMpStoryEnemyCapNext,0,sizeof(SudekiMpStoryEnemyCapNext));
    memset(SudekiMpStoryEnemyCapAuthored,0,sizeof(SudekiMpStoryEnemyCapAuthored));
    memset(away_connections,0,sizeof(away_connections));
    memset(companions,0,sizeof(companions));
    memset(seat_inputs,0,sizeof(seat_inputs));hit_count=hit_unknown=companion_substitutions=0;reset_selection_logs();return TRUE;
}
