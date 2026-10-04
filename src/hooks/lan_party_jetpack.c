#include "hooks/lan_party_jetpack.h"
#include "hooks/call_hook.h"
#include "cleanroom/engine.h"
#include "engine/log.h"
#include <math.h>
#include <limits.h>
#include <string.h>

/* Exact retail crystal/ability adapters. Native distance, combat/phase gates,
 * fill/drain integration and authored rates stay in the original functions.
 * Only the two lead-identity decisions become owned-Elco decisions. */
static uint8_t *base;
static SudekiMpLanPartySession *session;
static unsigned seat;
static DWORD game_thread,last_log;
static SudekiMpLanPartyRosterObservation roster;
static volatile LONG stopping,depth;
static SudekiMpPointerHook crystal_update,ability_update;
static SudekiMpInlineHook crystal_lead,ability_lead;
static SudekiMpRelativeCallHook fuel_tick;
static void *crystal_yes __attribute__((used)),*crystal_no __attribute__((used));
static void *ability_resume __attribute__((used)),*native_fuel_tick __attribute__((used));
static void *native_crystal_animation __attribute__((used));
static BOOL fixture_wanted,spawn_pending,remove_pending,fixture_failed;
static DWORD spawn_at;
static void *fixture_entity,*fixture_component;
static float fixture_position[3];
static BOOL fixture_position_valid;
static SudekiMpLanPartyJetpackState confirmed_state;
static BOOL have_confirmed;
static DWORD confirmed_at;

typedef void (__attribute__((thiscall)) *Update)(void *,void *);
typedef void (__attribute__((thiscall)) *SetFuel)(void *,float);
typedef void (__attribute__((thiscall)) *SetMaximum)(void *,float,BOOL);
typedef void (__attribute__((thiscall)) *Rate)(void *);
static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && VirtualQuery(p,&m,sizeof(m)) && m.State==MEM_COMMIT &&
        !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) && a+n>=a &&
        a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL bytes(unsigned rva,const void *p,size_t n) {
    return base && readable(base+rva,n) && !memcmp(base+rva,p,n);
}
static BOOL call(unsigned rva,unsigned target) {
    return readable(base+rva,5) && base[rva]==0xe8 &&
        (uintptr_t)(base+rva+5+*(int32_t *)(base+rva+1))==(uintptr_t)(base+target);
}
static BOOL thread_exact(void) { return game_thread && GetCurrentThreadId()==game_thread; }
static uint8_t *ability_exact(void *actor) {
    if(!base || !readable(actor,0x108u)) return NULL;
    uint8_t *a=*(uint8_t **)((uint8_t *)actor+0x104u);
    if(!readable(a,0x8cu) || *(void **)a!=base+0x2d64acu ||
        *(void **)(a+0x18u)!=base+0x2d64f4u || *(void **)(a+0x10u)!=actor) return NULL;
    for(unsigned i=0x68u;i<=0x80u;i+=4u)
        if(!isfinite(*(float *)(a+i))) return NULL;
    if(*(float *)(a+0x68u)<0 || *(float *)(a+0x68u)>1000000.0f ||
        *(float *)(a+0x6cu)<0 || *(float *)(a+0x6cu)>1000000.0f ||
        fabsf(*(float *)(a+0x7cu))>1000.0f) return NULL;
    return a;
}
static BOOL elco_admission(SudekiMpLanPartyLease *native,SudekiMpLanPartyLease *connection) {
    SudekiMpLanPartyPeerStatus peer;
    unsigned player=seat?seat:SudekiMpLanPartyCharacterPlayer(session,1u);
    /* The native local host already owns Elco's full flight state machine.
     * Remote orchestration must not replace its input/camera with a peer. */
    if(!native || !connection || !player || player>=4u ||
        !SudekiMpLanPartyPeerStatusGet(session,player,&peer) ||
        peer.phase!=SUDEKIMP_LAN_PARTY_ACTIVE || !peer.lease.token || !peer.lease.generation) return FALSE;
    if(seat && SudekiMpLanPartyControlLocalCharacter()==1u) {
        *native=peer.lease; native->seat=1u;
    } else if(!SudekiMpLanPartyControlActorLeaseOnNativeThread(1u,native) ||
        native->token!=peer.lease.token) return FALSE;
    *connection=peer.lease;
    return TRUE;
}
static BOOL elco_owned(void *actor) {
    SudekiMpLanPartyLease native,connection;
    if(!thread_exact() || !actor || actor!=roster.actors[1] ||
        !SudekiMpLanPartyControlNativeActorExact(&roster,1u) ||
        !ability_exact(actor)) return FALSE;
    /* Charging is native host world state, including an unclaimed AI Elco.
     * Replica writes still require this client's admitted presentation lease. */
    return !seat || elco_admission(&native,&connection);
}
static uint8_t *crystal_exact(void *iface) {
    if(!base || (uintptr_t)iface<0x18u) return NULL;
    uint8_t *c=(uint8_t *)iface-0x18u;
    if(!readable(c,0x88u) || *(void **)c!=base+0x2d629cu ||
        *(void **)iface!=base+0x2d62e4u || !readable(*(void **)(c+0x10u),0x48u)) return NULL;
    float fuel=*(float *)(c+0x3cu),radius=*(float *)(c+0x40u);
    if(!isfinite(fuel) || fuel<=0 || fuel>1000000.0f ||
        !isfinite(radius) || radius<=0 || radius>1000 || c[0x84u]>1u) return NULL;
    return c;
}
__attribute__((used,noinline)) static BOOL crystal_gate(void *actor,void *ability) {
    return !seat && !InterlockedCompareExchange(&stopping,0,0) &&
        elco_owned(actor) && ability_exact(actor)==ability;
}
/* Displaced conditional branch is never executed from the generic trampoline.
 * Both destinations are pinned in the supported image. Preserve all live
 * registers. This seam precedes the original distance calculation; no live
 * x87 stack values cross the helper. */
__attribute__((naked,used,noinline)) static void crystal_gate_entry(void) {
    __asm__ volatile("pushfl\n\tpushal\n\tpushl %edi\n\tpushl %ebp\n\tcall _crystal_gate\n\taddl $8,%esp\n\ttestl %eax,%eax\n\tjz 1f\n\tpopal\n\tpopfl\n\tjmp *_crystal_yes\n1:\n\tpopal\n\tpopfl\n\tjmp *_crystal_no\n\t");
}
__attribute__((used,noinline)) static void *own_ability(void *lead_ability,void *ability) {
    uint8_t *a=(uint8_t *)ability;
    if(thread_exact() && readable(a,0x8cu) && elco_owned(*(void **)(a+0x10u)) &&
        ability_exact(*(void **)(a+0x10u))==a && (a[0x88u]&15u)==0u) return a;
    return lead_ability;
}
__attribute__((naked,used,noinline)) static void ability_lead_entry(void) {
    __asm__ volatile("movl 0x104(%eax),%ebp\n\tpushfl\n\tpushal\n\tpushl %edi\n\tpushl %ebp\n\tcall _own_ability\n\taddl $8,%esp\n\tmovl %eax,8(%esp)\n\tpopal\n\tpopfl\n\tjmp *_ability_resume\n\t");
}
/* Fuel integration ABI: EAX=ability, ECX=&native_delta; no stack arguments. */
__attribute__((naked,noinline)) static void call_fuel(void *a __attribute__((unused)),
    const float *dt __attribute__((unused))) {
    __asm__ volatile("movl 4(%esp),%eax\n\tmovl 8(%esp),%ecx\n\tjmp *_native_fuel_tick\n\t");
}
__attribute__((used,noinline)) static void fuel_route(void *a,const float *dt) {
    if(!seat) { call_fuel(a,dt); return; }
    /* Replica resources never integrate local simulation time. Retail still
     * renders the meter and retires/refires its actor-owned refuel effects. */
    if(thread_exact() && SudekiMpLanPartyControlNativeActorExact(&roster,1u) &&
        ability_exact(roster.actors[1])==a) {
        /* This original ability update is also our retirement seam after
         * transport admission closes. Its exact native owner remains alive;
         * no fresh refuel effect is admitted here. */
        if((InterlockedCompareExchange(&stopping,0,0) || !have_confirmed ||
            !confirmed_state.valid || !elco_owned(roster.actors[1])) &&
            (((uint8_t *)a)[0x88u]&15u)==0u && *(float *)((uint8_t *)a+0x7cu)>0)
            ((Rate)(base+0xce0c0u))(a);
        if(*(float *)((uint8_t *)a+0x68u)>0) { const float zero=0; call_fuel(a,&zero); }
    }
}
__attribute__((naked,used,noinline)) static void fuel_route_entry(void) {
    __asm__ volatile("pushl %ecx\n\tpushl %eax\n\tcall _fuel_route\n\taddl $8,%esp\n\tret\n\t");
}
/* Flight is an actor lease, separate from the local view controller. Retail
 * animation conditions still consume ability+5c and call the original actor's
 * animation callbacks. Only CElcoAbility's single-player orchestration is
 * adapted: collision/movement/phase operations follow CD6A0/CDA60/CCFB0;
 * camera, input and music globals remain with each existing local view. */
typedef struct FlightWeak { void *object,*previous,*next; } FlightWeak;
typedef struct FlightOwner {
    SudekiMpLanPartyLease lease,connection;
    uint8_t *actor,*ability;
    BOOL live,held,release,local_actor;
    DWORD input_at;
    unsigned visual_kind,retired_mask;
    FlightWeak effects[8];
} FlightOwner;
static FlightOwner flight;
static SudekiMpRelativeCallHook flight_enter_hook,flight_exit_hook,flight_edge_hook;
static void *native_enter __attribute__((used)),*native_exit __attribute__((used));
static void *native_edge __attribute__((used)),*native_available __attribute__((used));
static void *native_locator __attribute__((used));
static uint8_t *component(void *actor,unsigned offset,unsigned vt,size_t size) {
    if(!readable(actor,offset+4u)) return NULL;
    uint8_t *c=*(uint8_t **)((uint8_t *)actor+offset);
    return readable(c,size) && *(void **)c==base+vt && *(void **)(c+0x10u)==actor?c:NULL;
}
static BOOL flight_parts(uint8_t *a) {
    void *actor=readable(a,0x8cu)?*(void **)(a+0x10u):NULL;
    return actor && ability_exact(actor)==a &&
        component(actor,0x44u,0x2cdefcu,0xbcu) &&
        component(actor,0x60u,0x2c85fcu,0x84u) &&
        component(actor,0x80u,0x2c8644u,0xc0u) &&
        component(actor,0x90u,0x2cc9acu,0x64u) &&
        component(actor,0x130u,0x2d5464u,0x168u);
}
static BOOL world_allows_flight(void) {
    /* Retail DB950/CDBF0/CD1A0 world blockers; the local player's HUD and
     * camera are deliberately not used as a remote actor's identity. */
    uint8_t *world=*(uint8_t **)(base+0x408d4cu);
    uint8_t *script=*(uint8_t **)(base+0x3c2f84u);
    uint8_t *mode=*(uint8_t **)(base+0x409d78u);
    return SudekiMpCleanroomEngineWorldReady() && readable(world,0x75u) &&
        readable(script,0x2au) && readable(mode,0x35u) &&
        !world[0x74u] && !script[0x29u] && !mode[0x34u];
}
static BOOL flight_owner_exact(BOOL cleanup) {
    return thread_exact() && flight.actor && flight.actor==roster.actors[1] &&
        SudekiMpLanPartyControlNativeActorExact(&roster,1u) &&
        ability_exact(flight.actor)==flight.ability && flight_parts(flight.ability) &&
        ((flight.local_actor && seat && SudekiMpLanPartyControlLocalCharacter()==1u) ||
            (cleanup ? SudekiMpLanPartyControlRetainedCleanupNativeThreadExact(&flight.lease,flight.actor):
                SudekiMpLanPartyControlRetainedNativeThreadExact(&flight.lease,flight.actor)));
}
static BOOL flight_exact(void) {
    return flight_owner_exact(FALSE);
}
static BOOL flight_lifetime_exact(void) {
    /* A latched release may finish its existing native phase/effects after
     * Quiesce closes active control. New binding and input still require
     * flight_exact; this proof alone cannot start a flight. */
    return flight_owner_exact(flight.release);
}
static BOOL bind_flight(void) {
    SudekiMpLanPartyLease native,connection;
    if(flight.actor) return flight_exact();
    if(!elco_owned(roster.actors[1]) || !flight_parts(ability_exact(roster.actors[1])) ||
        !elco_admission(&native,&connection)) return FALSE;
    flight.lease=native; flight.connection=connection;
    flight.local_actor=seat && SudekiMpLanPartyControlLocalCharacter()==1u;
    flight.actor=roster.actors[1]; flight.ability=ability_exact(flight.actor);
    return flight_exact();
}
static BOOL weak_exact(FlightWeak *w) {
    if(!w->object) return !w->previous && !w->next;
    if(!readable(w->object,8u)) return FALSE;
    if(w->previous) {
        FlightWeak *p=w->previous;
        if(!readable(p,sizeof(*p)) || p->object!=w->object || p->next!=w) return FALSE;
    } else if(*(void **)((uint8_t *)w->object+4u)!=w) return FALSE;
    if(w->next) {
        FlightWeak *n=w->next;
        if(!readable(n,sizeof(*n)) || n->object!=w->object || n->previous!=w) return FALSE;
    }
    return TRUE;
}
static void weak_assign(FlightWeak *w,void *object) {
    uintptr_t accumulator=(uintptr_t)w,data=(uintptr_t)object;
    void *entry=base+0x1750u;
    __asm__ volatile("call *%[entry]" : "+a"(accumulator),"+d"(data) :
        [entry] "r"(entry) : "ecx","memory","cc");
}
static BOOL effect_exact(void *p) {
    uint8_t *e=p;
    return readable(e,0x3e4u) && *(void **)e==base+0x2d3c7cu &&
        *(void **)(e+0x44u)==e+0x160u &&
        *(void **)(e+0x160u)==base+0x2cdefcu && *(void **)(e+0x170u)==e;
}
static BOOL retire_effect(void *p) {
    if(!p) return TRUE;
    if(!effect_exact(p)) return FALSE;
    ((void (__attribute__((stdcall)) *)(void *))(base+0x131df0u))(p);
    /* Original ability retire contract: drop looping after native Retire;
     * intrusive weak observers remain until the native destructor nulls them. */
    if(!effect_exact(p)) return FALSE;
    ((uint8_t *)p)[0x3e0u]&=(uint8_t)~1u;
    return TRUE;
}
static BOOL retire_flight_visuals(void) {
    for(unsigned i=0;i<8u;++i) {
        FlightWeak *w=&flight.effects[i];
        if(!weak_exact(w)) return FALSE;
        if(w->object && !(flight.retired_mask&(1u<<i))) {
            if(!retire_effect(w->object)) return FALSE;
            flight.retired_mask|=1u<<i;
        }
    }
    flight.visual_kind=0;
    return TRUE;
}
static BOOL visuals_drained(void) {
    for(unsigned i=0;i<8u;++i)
        if(!weak_exact(&flight.effects[i]) || flight.effects[i].object) return FALSE;
    return TRUE;
}
__attribute__((naked,noinline)) static unsigned char available(void *a __attribute__((unused))) {
    __asm__ volatile("pushl %esi\n\tmovl 8(%esp),%esi\n\tcall *_native_available\n\tpopl %esi\n\tret\n\t");
}
__attribute__((naked,noinline)) static int locator(void *lookup __attribute__((unused)),
    const char *name __attribute__((unused))) {
    __asm__ volatile("movl 4(%esp),%eax\n\tpushl 8(%esp)\n\tcall *_native_locator\n\tret\n\t");
}
static BOOL flight_visual(unsigned kind) {
    if(!flight_lifetime_exact() || kind>2u) return FALSE;
    if(kind==flight.visual_kind) return TRUE;
    if(!retire_flight_visuals()) return FALSE;
    uint8_t *a=flight.ability;
    if(!seat && *(void **)(a+0x60u)) {
        void *old=*(void **)(a+0x60u); BOOL tracked=FALSE;
        for(unsigned i=0;i<8u;++i) if(flight.effects[i].object==old) tracked=TRUE;
        if(!tracked && !retire_effect(old)) return FALSE;
        *(void **)(a+0x60u)=NULL;
    }
    if(!kind) return TRUE;
    FlightWeak *observer=NULL;
    for(unsigned i=0;i<8u;++i)
        if(!flight.effects[i].object && weak_exact(&flight.effects[i])) { observer=&flight.effects[i]; break; }
    if(!observer) return FALSE;
    flight.retired_mask&=~(1u<<(unsigned)(observer-flight.effects));
    uint8_t *position=component(flight.actor,0x44u,0x2cdefcu,0xbcu);
    uint8_t *link=position?*(uint8_t **)(position+0xb4u):NULL;
    uint8_t *lookup=readable(link,0x14u)?*(uint8_t **)(link+8u):NULL;
    if(!readable(lookup,0x18u) || !readable(*(void **)(lookup+0x14u),0x2cu)) return FALSE;
    int bone=locator(lookup,"Jetpack_exhaust");
    if(bone<0 || bone>=1024) return FALSE;
    SudekiMpResourceName resource;
    if(!SudekiMpCleanroomEngineResourceNameFromText(&resource,
        kind==1u?"SFXEJ100_JETPACK_FLAME.HOM":"SFXEJ101_JETPACK_SMOKE.HOM")) return FALSE;
    resource.encoded_kind=(resource.encoded_kind&~0x7fu)|0x29u;
    void *manager=*(void **)(base+0x408d48u);
    if(!readable(manager,4u) || !resource.text_reference ||
        !readable(resource.text_reference,4u) || !*resource.text_reference ||
        *resource.text_reference>=(uint32_t)LONG_MAX) {
        SudekiMpCleanroomEngineReleaseResourceName(&resource); return FALSE;
    }
    /* Retail attachment factory BCE90: EDI=locator, AL=force-ready0,
     * CL=loop1, six callee-cleaned stack words. Transfer one ResourceName
     * reference exactly as the original ability does. No cache-ready bypass. */
    void *args[5]={base+0xbce90u,manager,position,&resource,(void *)(uintptr_t)bone};
    uintptr_t result;
    ++*resource.text_reference;
    __asm__ volatile(
        "pushl %%edi\n\tpushl $0x3f800000\n\t"
        "movl 12(%%esi),%%edx\n\tpushl 8(%%edx)\n\tpushl 4(%%edx)\n\tpushl (%%edx)\n\t"
        "pushl 8(%%esi)\n\tpushl 4(%%esi)\n\tmovl 16(%%esi),%%edi\n\t"
        "xorl %%eax,%%eax\n\tmovl $1,%%ecx\n\tcall *(%%esi)\n\tpopl %%edi"
        : "=a"(result) : "S"(args) : "ecx","edx","memory","cc");
    if(result && effect_exact((void *)result)) {
        weak_assign(observer,(void *)result);
        if(weak_exact(observer) && observer->object==(void *)result) {
            ((uint8_t *)result)[0x3e0u]&=(uint8_t)~4u;
            if(!seat) *(void **)(a+0x60u)=(void *)result;
        }
    }
    SudekiMpCleanroomEngineReleaseResourceName(&resource);
    flight.visual_kind=kind; /* One native factory entry per observed transition. */
    SudekiMpLogFormat("lan_party_jetpack event=flight_visual seat=%u kind=%u created=%u\r\n",
        seat,kind,observer->object!=NULL);
    return result && observer->object==(void *)result && weak_exact(observer);
}
static void phase_set(uint8_t *a,unsigned phase,unsigned request) {
    unsigned before=a[0x88u]&15u;
    a[0x88u]=(a[0x88u]&0xf0u)|(uint8_t)phase;
    *(uint32_t *)(a+0x5cu)=request;
    if(phase!=before) SudekiMpLogFormat("lan_party_jetpack event=flight_phase seat=%u from=%u to=%u request=%u\r\n",
        seat,before,phase,request);
}
static BOOL actor_combat(uint8_t *arbiter) {
    return (arbiter[0x60u]&2u)!=0u;
}
static BOOL ground_near(uint8_t *a) {
    /* Native CD380 uses this actor's CPosition, world collision mask20004,
     * and its authored vertical ray. stdcall one pointer, AL result. */
    return ((unsigned char (__attribute__((stdcall)) *)(void *))(base+0xcd380u))(a)!=0;
}
static void flight_landing(uint8_t *a) {
    uint8_t *movement=*(uint8_t **)(flight.actor+0x80u);
    movement[0xbeu]|=2u;
    phase_set(a,8u,0x23u);
}
static BOOL flight_update(void *iface,void *update) {
    if(seat || !flight_lifetime_exact() || iface!=flight.ability+0x18u ||
        !readable(update,0x10u)) return FALSE;
    uint8_t *a=flight.ability;
    unsigned phase=a[0x88u]&15u;
    if(!phase) return FALSE;
    float dt=*(float *)((uint8_t *)update+0xcu);
    if(!isfinite(dt) || dt<0.0f || dt>0.25f) return TRUE;
    flight.live=TRUE;
    call_fuel(a,&dt);
    uint8_t *arbiter=*(uint8_t **)(flight.actor+0x90u);
    uint8_t *movement=*(uint8_t **)(flight.actor+0x80u);
    uint8_t *collision=*(uint8_t **)(flight.actor+0x60u);
    BOOL held=flight.held && !flight.release && !InterlockedCompareExchange(&stopping,0,0) &&
        GetTickCount()-flight.input_at<=SUDEKIMP_LAN_PARTY_INPUT_MAX_AGE_MS &&
        SudekiMpLanPartyLeaseActive(session,&flight.connection) && !actor_combat(arbiter);
    /* A pending takeoff drains through its native enter/end callbacks even
     * if input was released before the authored clip began. */
    if(phase>=2u && phase<=4u && (!held || *(float *)(a+0x6cu)<=0.0f)) a[0x88u]|=0x10u;
    if((a[0x88u]&0x10u) && phase>=2u && phase<=4u) {
        a[0x88u]&=(uint8_t)~0x10u;
        if(ground_near(a)) flight_landing(a);
        else phase_set(a,5u,0x22u);
    } else if(phase==5u && ground_near(a)) flight_landing(a);
    else if(phase==6u || phase==7u) {
        BOOL eligible=(*(uint32_t *)(collision+0x2cu)&3u)==3u && (movement[0xbfu]&2u) &&
            !actor_combat(arbiter) && !flight.release && world_allows_flight();
        if((phase==7u || eligible) && !ground_near(a)) {
            *(uint32_t *)(arbiter+0x50u)|=0x80u;
            movement[0xbfu]&=(uint8_t)~2u; *(float *)(movement+0x38u)=0;
            *(uint32_t *)(collision+0x2cu)&=~4u;
            phase_set(a,7u,*(uint32_t *)(a+0x5cu));
            if(available(a)) phase_set(a,5u,0x22u);
        } else if(phase==6u) {
            float timer=eligible?*(float *)(a+0x64u)-dt:0;
            *(float *)(a+0x64u)=timer>0?timer:0;
            if(timer<=0) { phase_set(a,0u,0u); *(uint32_t *)(collision+0x2cu)|=0x10u; }
        }
    }
    return TRUE;
}
__attribute__((naked,noinline)) static void call_enter(void *a __attribute__((unused)),unsigned id __attribute__((unused))) {
    __asm__ volatile("movl 8(%esp),%eax\n\tpushl 4(%esp)\n\tcall *_native_enter\n\tret\n\t");
}
__attribute__((naked,noinline)) static void call_exit(void *a __attribute__((unused)),unsigned id __attribute__((unused))) {
    __asm__ volatile("movl 4(%esp),%ecx\n\tmovl 8(%esp),%eax\n\tjmp *_native_exit\n\t");
}
__attribute__((used,noinline)) static void flight_enter(unsigned id,void *ability) {
    InterlockedIncrement(&depth);
    uint8_t *a=ability;
    if(a!=flight.ability) { call_enter(a,id); goto done; }
    if(!flight_lifetime_exact()) goto done; /* Unknown retained owner cannot use globals. */
    if(seat) goto done; /* host semantic callbacks are never client gameplay */
    unsigned phase=a[0x88u]&15u;
    uint8_t *movement=*(uint8_t **)(flight.actor+0x80u);
    uint8_t *collision=*(uint8_t **)(flight.actor+0x60u);
    if(id==0x1eu && phase==1u) {
        phase_set(a,2u,0x1eu);
        *(uint32_t *)(collision+0x2cu)&=~0x14u;
        *(float *)(movement+0x38u)=0; movement[0xbfu]&=(uint8_t)~2u;
        (void)flight_visual(1u);
    } else if(id==0x22u && phase==5u) {
        *(uint16_t *)(movement+0x7au)=1u;
        *(uint32_t *)(collision+0x2cu)&=~2u;
        (void)flight_visual(2u);
    } else if(id==0x23u && phase==8u) {
        phase_set(a,9u,0u);
        *(uint32_t *)(collision+0x2cu)|=0x16u;
        *(uint16_t *)(movement+0x7au)=0;
        *(float *)(movement+0x38u)=0; movement[0xbfu]|=2u;
        (void)flight_visual(0u);
    }
done: InterlockedDecrement(&depth);
}
__attribute__((used,noinline)) static void flight_exit(unsigned id,void *ability) {
    InterlockedIncrement(&depth);
    uint8_t *a=ability;
    if(a!=flight.ability) { call_exit(a,id); goto done; }
    if(!flight_lifetime_exact()) goto done;
    if(seat) goto done;
    unsigned phase=a[0x88u]&15u;
    uint8_t *movement=*(uint8_t **)(flight.actor+0x80u);
    uint8_t *arbiter=*(uint8_t **)(flight.actor+0x90u);
    if(id==0x1eu && phase==2u) {
        phase_set(a,3u,0x1fu); movement[0x7bu]=1u;
    } else if(id==0x23u && phase==9u) {
        phase_set(a,0u,0u); movement[0xbeu]&=(uint8_t)~2u;
        *(uint32_t *)(arbiter+0x50u)&=~0x80u;
        ((Rate)(base+0xce0c0u))(a);
        SudekiMpLogWrite("lan_party_jetpack event=flight_landed authority=host_native_animation_end\r\n");
    }
done: InterlockedDecrement(&depth);
}
__attribute__((naked,used,noinline)) static void flight_enter_entry(void) {
    __asm__ volatile("pushl 4(%esp)\n\tpushl %eax\n\tcall _flight_enter\n\taddl $8,%esp\n\tret $4\n\t");
}
__attribute__((naked,used,noinline)) static void flight_exit_entry(void) {
    __asm__ volatile("pushl %ecx\n\tpushl %eax\n\tcall _flight_exit\n\taddl $8,%esp\n\tret\n\t");
}
__attribute__((naked,noinline)) static unsigned call_edge(void *a __attribute__((unused))) {
    __asm__ volatile("movl 4(%esp),%eax\n\tjmp *_native_edge\n\t");
}
__attribute__((used,noinline)) static unsigned flight_edge(void *ability) {
    if(!seat && SudekiMpLanPartyControlLocalCharacter()==1u &&
        !flight.actor && elco_owned(roster.actors[1]) && ability_exact(roster.actors[1])==ability)
        return call_edge(ability);
    if(!bind_flight() || ability!=flight.ability) {
        if(ability==flight.ability || (roster.actors[1] &&
            SudekiMpLanPartyControlNativeActorExact(&roster,1u) &&
            ability_exact(roster.actors[1])==ability)) return 1u;
        return call_edge(ability);
    }
    if(seat) return 1u; /* Replica collision never starts a local flight task. */
    uint8_t *a=flight.ability,*actor=flight.actor;
    uint8_t *arbiter=*(uint8_t **)(actor+0x90u);
    uint8_t *movement=*(uint8_t **)(actor+0x80u);
    uint8_t *collision=*(uint8_t **)(actor+0x60u);
    unsigned phase=a[0x88u]&15u;
    if(!world_allows_flight() || actor_combat(arbiter) || flight.release || InterlockedCompareExchange(&stopping,0,0) ||
        !SudekiMpLanPartyLeaseActive(session,&flight.connection)) return 1u;
    if((phase==0u || phase==6u) &&
        (*(uint32_t *)(collision+0x2cu)&3u)==3u && (movement[0xbfu]&2u)) {
        *(float *)(a+0x64u)=*(float *)(base+0x2d8a34u);
        phase_set(a,6u,0u); *(uint32_t *)(collision+0x2cu)&=~0x10u;
        /* SetMaxFuel(false) preserves the resource and uses the same native
         * action-load/update-list wake path; no shared player-mode wake. */
        ((SetMaximum)(base+0xcdf80u))(a,*(float *)(a+0x68u),FALSE);
        flight.live=TRUE;
    }
    return 0u;
}
__attribute__((naked,used,noinline)) static void flight_edge_entry(void) {
    __asm__ volatile("pushl %eax\n\tcall _flight_edge\n\taddl $4,%esp\n\tret\n\t");
}
BOOL SudekiMpLanPartyJetpackInput(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *key,void *actor,BOOL held) {
    if(!base || seat || !key || key->seat!=1u ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) ||
        !SudekiMpLanPartyControlExact(w,key,actor) || !bind_flight() ||
        actor!=flight.actor || key->token!=flight.lease.token ||
        key->generation!=flight.lease.generation) return FALSE;
    uint8_t *a=flight.ability,*arbiter=*(uint8_t **)(flight.actor+0x90u);
    uint8_t *movement=*(uint8_t **)(flight.actor+0x80u);
    uint8_t *collision=*(uint8_t **)(flight.actor+0x60u);
    unsigned phase=a[0x88u]&15u;
    BOOL edge=held && !flight.held;
    flight.held=held; flight.input_at=GetTickCount();
    if(!held || !edge || flight.release || InterlockedCompareExchange(&stopping,0,0)) return TRUE;
    /* Retail DB950/CDDB0 admission, with local HUD availability replaced by
     * this exact actor's native action6 availability. All actor blockers,
     * fuel, collision, gravity and phase constraints remain mandatory. */
    if(!world_allows_flight() || actor_combat(arbiter) || (arbiter[0x60u]&1u) ||
        (*(uint32_t *)(arbiter+0x50u)&0x02dbf76cu) ||
        *(float *)(a+0x6cu)<=0.0f ||
        (phase!=0u && phase!=5u && phase!=6u) ||
        (phase!=5u && ((*(uint32_t *)(collision+0x2cu)&3u)!=3u || !(movement[0xbfu]&2u))) ||
        !available(a)) return FALSE;
    flight.live=TRUE;
    if(phase==5u) {
        phase_set(a,3u,0x1fu); movement[0xbfu]&=(uint8_t)~2u;
        *(float *)(movement+0x38u)=0; *(uint16_t *)(movement+0x7au)=0x100u;
        *(uint32_t *)(collision+0x2cu)|=2u; (void)flight_visual(1u);
    } else {
        *(uint32_t *)(arbiter+0x50u)|=0x80u;
        phase_set(a,1u,0x1eu);
    }
    ((SetMaximum)(base+0xcdf80u))(a,*(float *)(a+0x68u),FALSE);
    return TRUE;
}
BOOL SudekiMpLanPartyJetpackIdle(void) {
    if(!base || !thread_exact() || !SudekiMpLanPartyControlNativeActorExact(&roster,1u)) return FALSE;
    uint8_t *a=ability_exact(roster.actors[1]);
    return a && !(a[0x88u]&15u) && !*(uint32_t *)(a+0x5cu) &&
        !flight.live && !flight.release && visuals_drained();
}
BOOL SudekiMpLanPartyJetpackDrained(void *actor) {
    if(!base || !actor || actor!=roster.actors[1]) return TRUE;
    uint8_t *a=ability_exact(actor);
    if(!a || !thread_exact()) return FALSE;
    if(!flight.actor) return (a[0x88u]&15u)==0u && !*(uint32_t *)(a+0x5cu);
    if(!flight_owner_exact(TRUE)) return FALSE;
    flight.release=TRUE; flight.held=FALSE;
    if(seat && !retire_flight_visuals()) return FALSE;
    if((a[0x88u]&15u) || *(uint32_t *)(a+0x5cu) || !visuals_drained()) return FALSE;
    ZeroMemory(&flight,sizeof(flight));
    return TRUE;
}
/* Testroom-only native fixture. It is never adopted from a foreign world.
 * Its native SPH provides collision; the mod does not synthesize a floor. */
static void *platform_entity;
/* Authored JUMP_PILLAR_SMLB.SPH walking surface, measured in model space.
 * Its upper rim has native traversal kind6. The unused SM_TestPlatform
 * asset has kind0 walls around that rim and traps even native flight. */
static const float platform_top=10.453256f;
static BOOL platform_spawn,platform_remove,platform_failed,platform_position_valid;
static DWORD platform_at;
static float platform_position[3];
static BOOL platform_exact(void) {
    if(!platform_entity || SudekiMpCleanroomEngineGenericEntity("DB_PM_Jump_Pillar_S_B")!=platform_entity)
        return FALSE;
    uint8_t *p=component(platform_entity,0x44u,0x2cdefcu,0xbcu);
    if(!p) return FALSE;
    for(unsigned i=0;i<3u;++i)
        if(!isfinite(*(float *)(p+0x18u+i*4u)) ||
            fabsf(*(float *)(p+0x18u+i*4u)-platform_position[i])>0.01f) return FALSE;
    return TRUE;
}
static void platform_service(void) {
    void *current=SudekiMpCleanroomEngineGenericEntity("DB_PM_Jump_Pillar_S_B");
    if(platform_entity && current!=platform_entity) {
        if(current) { platform_failed=TRUE; return; }
        platform_entity=NULL; platform_remove=FALSE;
    }
    if(platform_spawn && current) {
        platform_entity=current; platform_spawn=FALSE;
        /* Retail SpawnEntity grounds generic objects. Pin this transaction's
         * exact native generic entity before applying its requested elevation
         * with CPosition's normal dirty-transform/collision notifications. */
        uint8_t *p=component(current,0x44u,0x2cdefcu,0xbcu);
        if(!readable(current,0x88u) || *(void **)current!=base+0x2d5b00u || !p ||
            !isfinite(*(float *)(p+0x1cu)) ||
            fabsf(*(float *)(p+0x18u)-platform_position[0])>0.01f ||
            fabsf(*(float *)(p+0x20u)-platform_position[2])>0.01f ||
            fabsf(*(float *)(p+0x1cu)-platform_position[1])>16.0f) {
            platform_failed=TRUE; return;
        }
        ((void (__attribute__((fastcall)) *)(void *,const float *))(base+0x3050u))(p,platform_position);
        if(!platform_exact()) { platform_failed=TRUE; return; }
        SudekiMpLogFormat("lan_party_jetpack event=platform_present seat=%u entity=%p\r\n",seat,current);
    }
    BOOL want=!InterlockedCompareExchange(&stopping,0,0) &&
        (!seat || (have_confirmed && confirmed_state.platform_present));
    if(!want) {
        if(platform_entity && !platform_remove && platform_exact())
            platform_remove=SudekiMpCleanroomEngineRemoveFlightPlatform(platform_entity);
        return;
    }
    if(platform_entity || platform_failed || !platform_position_valid) return;
    if(platform_spawn) {
        if(GetTickCount()-platform_at>10000u) platform_failed=TRUE;
        return;
    }
    if(current) { platform_failed=TRUE; return; }
    /* SpawnEntity first resolves a walkable world position (RVA FBD50).
     * The authored pillar origin lies below its top, beneath the room floor.
     * Request on the room's walkable side; after positive identity discovery,
     * the exact CPosition setter above establishes the final origin. */
    float spawn_point[3]={platform_position[0],
        platform_position[1]+platform_top,platform_position[2]};
    platform_spawn=SudekiMpCleanroomEngineSpawnFlightPlatform(spawn_point);
    platform_at=GetTickCount();
    if(!platform_spawn) platform_failed=TRUE;
    SudekiMpLogFormat("lan_party_jetpack event=platform_spawn_request seat=%u accepted=%u pos=%.3f,%.3f,%.3f\r\n",
        seat,platform_spawn,platform_position[0],platform_position[1],platform_position[2]);
}
BOOL SudekiMpLanPartyJetpackLedgeReady(BOOL *enabled) {
    if(!base || seat || !enabled) return FALSE;
    *enabled=thread_exact() && platform_exact() && !platform_remove &&
        !platform_failed && SudekiMpLanPartyJetpackIdle();
    return TRUE;
}
BOOL SudekiMpLanPartyJetpackToLedge(const SudekiMpControlUpdateDispatchWitness *w) {
    BOOL ready=FALSE,combat=TRUE;
    BOOL local_elco=!seat && SudekiMpLanPartyControlLocalCharacter()==1u;
    void *actor=roster.actors[1];
    if(!SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) ||
        !SudekiMpLanPartyJetpackLedgeReady(&ready) || !ready ||
        SudekiMpLanPartyControlObserveActor(w,1u)!=actor ||
        !SudekiMpLanPartyControlNativeActorExact(&roster,1u) ||
        (!local_elco && (!bind_flight() || !SudekiMpLanPartyControlExact(w,&flight.lease,actor))) ||
        !SudekiMpCleanroomEngineCombatMode(&combat) || combat || !world_allows_flight() ||
        InterlockedCompareExchange(&stopping,0,0)) return FALSE;
    uint8_t *m=component(actor,0x80u,0x2c8644u,0xc0u);
    uint8_t *p=component(actor,0x44u,0x2cdefcu,0xbcu);
    uint8_t *arbiter=component(actor,0x90u,0x2cc9acu,0x64u);
    if(!m || !p || !arbiter || !(m[0xbfu]&2u) ||
        (*(uint32_t *)(arbiter+0x50u)&0x02dbf7ecu)) return FALSE;
    /* Place above the authored column, then let native gravity/collision
     * settle Elco. No guessed hit surface or position integration bypass. */
    float point[3]={platform_position[0],platform_position[1]+platform_top+3.0f,platform_position[2]};
    ((void (__attribute__((fastcall)) *)(void *,const float *))(base+0x3050u))(p,point);
    SudekiMpLogFormat("lan_party_jetpack event=to_ledge seat=0 pos=%.3f,%.3f,%.3f policy=native_gravity_settle\r\n",
        point[0],point[1],point[2]);
    return TRUE;
}

static void flight_service(void) {
    if(seat && flight.actor) {
        SudekiMpLanPartyPeerStatus p;
        BOOL active=SudekiMpLanPartyPeerStatusGet(session,seat,&p) &&
            p.phase==SUDEKIMP_LAN_PARTY_ACTIVE &&
            p.lease.token==flight.connection.token && p.lease.generation==flight.connection.generation;
        if(!active || InterlockedCompareExchange(&stopping,0,0)) {
            flight.release=TRUE; flight.held=FALSE;
        }
    }
    if(flight.actor && flight.release) {
        if(!flight_lifetime_exact() || (seat && !retire_flight_visuals())) return;
        if(!(flight.ability[0x88u]&15u) && !*(uint32_t *)(flight.ability+0x5cu) &&
            visuals_drained()) ZeroMemory(&flight,sizeof(flight));
        return;
    }
    if(!bind_flight()) return;
    if(seat) {
        unsigned phase=have_confirmed && confirmed_state.valid &&
            !InterlockedCompareExchange(&stopping,0,0)?confirmed_state.flight_phase:0u;
        unsigned kind=phase>=2u && phase<=4u?1u:phase==5u?2u:0u;
        if(flight.release) kind=0;
        if(kind || flight.visual_kind) (void)flight_visual(kind);
    }
    if(!seat && flight.live && !(flight.ability[0x88u]&15u) && visuals_drained()) {
        flight.live=FALSE;
        /* Keep a held action latched: a completed landing must not start
         * another flight until this owner releases and presses again. */
    }
}

static void __attribute__((thiscall)) crystal_route(void *iface,void *update) {
    InterlockedIncrement(&depth);
    uint8_t *c=crystal_exact(iface);
    if(c && thread_exact()) {
        if(fixture_entity && *(void **)(c+0x10u)==fixture_entity) fixture_component=c;
        /* Only authority runs native proximity and resource mutations. */
        if(!seat) ((Update)crystal_update.original_value)(iface,update);
    }
    InterlockedDecrement(&depth);
}
static void __attribute__((thiscall)) ability_route(void *iface,void *update) {
    InterlockedIncrement(&depth);
    if(!flight_update(iface,update)) ((Update)ability_update.original_value)(iface,update);
    InterlockedDecrement(&depth);
}
/* Animation helper ABI: EAX=crystal component, EDX=authored string,
 * stack bool + float, callee pops eight bytes. */
__attribute__((naked,noinline)) static unsigned char animate(void *c __attribute__((unused)),
    const char *name __attribute__((unused)),BOOL active __attribute__((unused))) {
    __asm__ volatile("movl 4(%esp),%eax\n\tmovl 8(%esp),%edx\n\tmovl 12(%esp),%ecx\n\tpushl $0x3f800000\n\tpushl %ecx\n\tcall *_native_crystal_animation\n\tret\n\t");
}
static BOOL crystal_visual(BOOL active) {
    uint8_t *c=fixture_component;
    if(!c || !crystal_exact(c+0x18u) || *(void **)(c+0x10u)!=fixture_entity) return FALSE;
    if(c[0x84u]==(uint8_t)active) return TRUE;
    /* The native update uses +0x64/+0x68 for active and +0x44/+0x48 for idle. */
    unsigned offset=active?0x64u:0x44u;
    const char *name=(*(uint32_t *)(c+offset)&0x80000000u)?
        (const char *)(c+offset+4u):*(const char **)(c+offset+4u);
    if(!readable(name,1u)) return FALSE;
    unsigned n; for(n=0;n<128u && readable(name+n,1u) && name[n];++n) {}
    if(!n || n==128u || !readable(name+n,1u)) return FALSE;
    if(!animate(c,name,active)) return FALSE;
    c[0x84u]=(uint8_t)active; return TRUE;
}
static BOOL fixture_observe(void) {
    void *entity=SudekiMpCleanroomEngineGenericEntity("GEN_FuelPoint");
    if(fixture_entity) {
        if(entity==fixture_entity) return TRUE;
        if(entity) { fixture_failed=TRUE; return FALSE; }
        fixture_entity=fixture_component=NULL; remove_pending=FALSE;
    }
    if(spawn_pending && entity) {
        fixture_entity=entity; spawn_pending=FALSE;
        SudekiMpLogFormat("lan_party_jetpack event=crystal_present seat=%u entity=%p\r\n",seat,entity);
        return TRUE;
    }
    return FALSE;
}
static void fixture_service(void) {
    BOOL present=fixture_observe();
    BOOL want=!InterlockedCompareExchange(&stopping,0,0) &&
        (seat?(have_confirmed && confirmed_state.crystal_present):fixture_wanted);
    if(!want) {
        if(present && !remove_pending && !fixture_failed) {
            if(!seat && fixture_component && crystal_exact((uint8_t *)fixture_component+0x18u)) {
                uint8_t *a=ability_exact(roster.actors[1]);
                if(a && SudekiMpLanPartyControlNativeActorExact(&roster,1u) &&
                    (a[0x88u]&15u)==0u) ((Rate)(base+0xce0c0u))(a);
            }
            remove_pending=SudekiMpCleanroomEngineRemoveFuelCrystal(fixture_entity);
            if(remove_pending) SudekiMpLogFormat("lan_party_jetpack event=crystal_remove_requested seat=%u\r\n",seat);
        }
        return;
    }
    if(present || spawn_pending || fixture_failed || !fixture_position_valid) return;
    /* A foreign named entity cannot be adopted or removed by this fixture. */
    if(SudekiMpCleanroomEngineGenericEntity("GEN_FuelPoint")) { fixture_failed=TRUE; return; }
    spawn_pending=SudekiMpCleanroomEngineSpawnFuelCrystal(fixture_position);
    if(!spawn_pending) fixture_failed=TRUE;
    spawn_at=GetTickCount();
    SudekiMpLogFormat("lan_party_jetpack event=crystal_spawn_request seat=%u accepted=%u pos=%.3f,%.3f,%.3f\r\n",
        seat,spawn_pending,fixture_position[0],fixture_position[1],fixture_position[2]);
}
void SudekiMpLanPartyJetpackService(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyJetpackState *confirmed) {
    if(!base || !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w)) return;
    if(!game_thread) game_thread=GetCurrentThreadId();
    if(!thread_exact() || !SudekiMpLanPartyControlObserveRoster(w,&roster)) return;
    if(seat) {
        have_confirmed=confirmed && SudekiMpLanPartyJetpackStateValid(confirmed);
        if(have_confirmed) { confirmed_state=*confirmed; confirmed_at=GetTickCount(); }
        if(have_confirmed && confirmed->crystal_present) {
            if(fixture_position_valid && (fixture_entity || spawn_pending) &&
                memcmp(fixture_position,confirmed->crystal_position,sizeof(fixture_position))) {
                /* A changed fixture waits for its previous entity to drain. */
                confirmed_state.crystal_present=0; fixture_service(); return;
            }
            memcpy(fixture_position,confirmed->crystal_position,sizeof(fixture_position)); fixture_position_valid=TRUE;
        }
    } else if(!fixture_position_valid && roster.present_mask==15u &&
        SudekiMpCleanroomEngineActorPosition(SUDEKIMP_CLEANROOM_ELCO,fixture_position)) {
        fixture_position[0]+=4.0f; fixture_position_valid=TRUE;
    }
    if(seat && have_confirmed && confirmed_state.platform_present) {
        if(platform_position_valid && (platform_entity || platform_spawn) &&
            memcmp(platform_position,confirmed_state.platform_position,sizeof(platform_position))) {
            platform_failed=TRUE; /* Incompatible fixture identity: retain, never move it. */
        } else {
            memcpy(platform_position,confirmed_state.platform_position,sizeof(platform_position));
            platform_position_valid=TRUE;
        }
    } else if(!seat && !platform_position_valid && fixture_position_valid) {
        memcpy(platform_position,fixture_position,sizeof(platform_position));
        platform_position[1]+=4.0f-platform_top; platform_position[2]+=4.0f;
        platform_position_valid=TRUE;
    }
    flight_service();
    fixture_service();
    platform_service();
    if(spawn_pending && GetTickCount()-spawn_at>10000u && !fixture_failed) {
        fixture_failed=TRUE;
        SudekiMpLogFormat("lan_party_jetpack event=crystal_spawn_timeout seat=%u policy=retain_pending_no_retry\r\n",seat);
    }
    uint8_t *a=ability_exact(roster.actors[1]);
    if(seat && a && elco_owned(roster.actors[1]) && (a[0x88u]&15u)==0u) {
        if(have_confirmed && confirmed->valid && !InterlockedCompareExchange(&stopping,0,0)) {
            if(*(float *)(a+0x6cu)!=confirmed->fuel || *(float *)(a+0x68u)!=confirmed->maximum) {
                ((SetFuel)(base+0xcdf30u))(a,confirmed->fuel);
                ((SetMaximum)(base+0xcdf80u))(a,confirmed->maximum,FALSE);
            }
            BOOL filling=confirmed->rate>0.0f && confirmed->fuel<confirmed->maximum;
            if(filling && *(float *)(a+0x7cu)<=0.0f) ((Rate)(base+0xcdff0u))(a);
            else if(!filling && *(float *)(a+0x7cu)>0.0f) ((Rate)(base+0xce0c0u))(a);
            if(fixture_entity && fixture_component) (void)crystal_visual(confirmed->crystal_active);
        } else if(*(float *)(a+0x7cu)>0.0f) ((Rate)(base+0xce0c0u))(a);
    }
    DWORD now=GetTickCount();
    if(a && (!last_log || now-last_log>=1000u)) {
        last_log=now;
        SudekiMpLogFormat("lan_party_jetpack event=state seat=%u fuel=%.3f max=%.3f rate=%.3f phase=%u crystal=%u active=%u confirmed=%u age=%lu\r\n",
            seat,*(float *)(a+0x6cu),*(float *)(a+0x68u),*(float *)(a+0x7cu),a[0x88u]&15u,
            fixture_entity!=NULL,fixture_component &&
                crystal_exact((uint8_t *)fixture_component+0x18u)?
                ((uint8_t *)fixture_component)[0x84u]:0u,have_confirmed,
            (unsigned long)(have_confirmed?now-confirmed_at:0));
    }
}
void SudekiMpLanPartyJetpackServiceStop(const SudekiMpControlUpdateDispatchWitness *w) {
    if(InterlockedCompareExchange(&stopping,0,0)) SudekiMpLanPartyJetpackService(w,NULL);
}
void SudekiMpLanPartyJetpackCapture(SudekiMpLanPartyJetpackState *v) {
    if(!v) return;
    ZeroMemory(v,sizeof(*v));
    if(!base || seat || !thread_exact()) return;
    uint8_t *a=ability_exact(roster.actors[1]);
    if(a && elco_owned(roster.actors[1])) {
        BOOL infinite=FALSE; (void)SudekiMpCleanroomEngineInfiniteJetpackFuel(&infinite);
        v->valid=1; v->fuel=*(float *)(a+0x6cu); v->maximum=*(float *)(a+0x68u);
        v->rate=*(float *)(a+0x7cu); v->infinite=(uint8_t)infinite;
        v->flight_phase=a[0x88u]&15u;
    }
    if(fixture_entity && fixture_observe() && !remove_pending) {
        v->crystal_present=1; memcpy(v->crystal_position,fixture_position,sizeof(fixture_position));
        if(v->valid && fixture_component && crystal_exact((uint8_t *)fixture_component+0x18u))
            v->crystal_active=((uint8_t *)fixture_component)[0x84u];
    }
    if(platform_exact() && !platform_remove && !platform_failed) {
        v->platform_present=1;
        memcpy(v->platform_position,platform_position,sizeof(platform_position));
    }
    if(!SudekiMpLanPartyJetpackStateValid(v)) ZeroMemory(v,sizeof(*v));
}
BOOL SudekiMpLanPartyJetpackFixture(BOOL *enabled) {
    if(!base || seat || !enabled) return FALSE;
    *enabled=fixture_wanted; return TRUE;
}
BOOL SudekiMpLanPartyJetpackSetFixture(BOOL enabled) {
    uint8_t *a=ability_exact(roster.actors[1]);
    if(!base || seat || !thread_exact() || !a || !elco_owned(roster.actors[1]) ||
        (a[0x88u]&15u) || fixture_failed || InterlockedCompareExchange(&stopping,0,0)) return FALSE;
    fixture_wanted=enabled!=FALSE; return TRUE;
}
void SudekiMpLanPartyJetpackRequestStop(void) { InterlockedExchange(&stopping,1); }
BOOL SudekiMpLanPartyJetpackUninstall(void) {
    if(!base) return TRUE;
    if(InterlockedCompareExchange(&depth,0,0) || fixture_entity || spawn_pending || remove_pending ||
        flight.live || !visuals_drained() || platform_entity || platform_spawn || platform_remove) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    /* Restore callbacks before their dependencies; retain everything if any
     * restoration fails so a later call can safely retry. */
    BOOL restored=TRUE;
    if(!SudekiMpRestoreRelativeCallHook(&flight_edge_hook)) restored=FALSE;
    if(!SudekiMpRestoreRelativeCallHook(&flight_exit_hook)) restored=FALSE;
    if(!SudekiMpRestoreRelativeCallHook(&flight_enter_hook)) restored=FALSE;
    if(!SudekiMpRestorePointerHook(&ability_update)) restored=FALSE;
    if(!SudekiMpRestorePointerHook(&crystal_update)) restored=FALSE;
    if(!SudekiMpRestoreRelativeCallHook(&fuel_tick)) restored=FALSE;
    if(!SudekiMpRestoreInlineHook(&ability_lead)) restored=FALSE;
    if(!SudekiMpRestoreInlineHook(&crystal_lead)) restored=FALSE;
    if(!restored) return FALSE;
    base=NULL; session=NULL; game_thread=0; return TRUE;
}
BOOL SudekiMpLanPartyJetpackInstall(HMODULE module,SudekiMpLanPartySession *s) {
    static const uint8_t gate[]={0x85,0xdb,0x0f,0x85,0x37,1,0,0};
    static const uint8_t lead[]={0x8b,0xa8,4,1,0,0};
    if(base || !module || !s || !SudekiMpCleanroomEngineImageExact(module)) return FALSE;
    base=(uint8_t *)module;
    if(!bytes(0x19c95eu,gate,sizeof(gate)) || !bytes(0xcd021u,lead,sizeof(lead)) ||
        !bytes(0xcdf30u,"\xd9\x44\x24\x04\x56\x8b\xf1\xd9\x56\x68\xd9\x5e\x6c",13) ||
        !bytes(0xcdf80u,"\x80\x7c\x24\x08\x01\xd9\x44\x24\x04",9) ||
        !bytes(0xcdff0u,"\x51\x53\x55\x56\x8b\xf1\x83\x7e\x60\x00\xd9\x46\x70\xd9\x5e\x7c",16) ||
        !bytes(0xce0c0u,"\x56\x8b\xf1",3) ||
        !bytes(0x19c8c0u,"\x83\xec\x30\x56\x57",5) ||
        !bytes(0x19caf0u,"\x85\xd2\x0f\x84\xab\0\0\0",8) ||
        !call(0xccfcdu,0xcd440u) ||
        !call(0x14c6c7u,0xcd6a0u) || !call(0x14c7edu,0xcda60u) ||
        !call(0xc5ac5u,0xcde20u) ||
        !bytes(0x3050u,"\xd9\x41\x18\xd9\x02\xda\xe9\xdf\xe0\xf6\xc4\x44",12) ||
        !bytes(0xcd380u,"\xb8\x01\0\0\0\x83\xec\x18",8) ||
        !bytes(0xbce90u,"\xd9\x44\x24\x18\x56\x50\x51\xd9\x1c\x24",10) ||
        !bytes(0xbce90u+0xa9u,"\xc2\x18\0",3) ||
        !bytes(0xc5ff0u,"\x56\x8b\x70\x14\x85\xf6",6) ||
        !bytes(0xce160u,"\x8b\x46\x10\x8b\x88\x90\0\0\0\x57",10) ||
        !bytes(0x1750u,"\x8b\x08\x85\xc9\x74\x35\x57",7) ||
        !bytes(0x131df0u,"\x55\x8b\xec\x83\xe4\xf8\x8b\x4d\x08",9) ||
        *(void **)(base+0x2d62e8u)!=base+0x19c8c0u ||
        *(void **)(base+0x2d64f8u)!=base+0xccfb0u) { base=NULL; return FALSE; }
    session=s; ZeroMemory(&flight,sizeof(flight)); seat=SudekiMpLanPartyLocalSeat(s); game_thread=0; last_log=0;
    fixture_wanted=TRUE; fixture_position_valid=spawn_pending=remove_pending=fixture_failed=FALSE;
    fixture_entity=fixture_component=NULL; have_confirmed=FALSE; stopping=depth=0;
    platform_entity=NULL; platform_spawn=platform_remove=platform_failed=platform_position_valid=FALSE;
    ZeroMemory(&roster,sizeof(roster));
    crystal_yes=base+0x19c966u; crystal_no=base+0x19ca9du;
    ability_resume=base+0xcd027u; native_fuel_tick=base+0xcd440u;
    native_crystal_animation=base+0x19caf0u;
    native_enter=base+0xcd6a0u; native_exit=base+0xcda60u; native_edge=base+0xcde20u;
    native_available=base+0xce160u; native_locator=base+0xc5ff0u;
    if(!SudekiMpInstallInlineHook(&crystal_lead,base+0x19c95eu,gate,sizeof(gate),crystal_gate_entry) ||
        !SudekiMpInstallInlineHook(&ability_lead,base+0xcd021u,lead,sizeof(lead),ability_lead_entry) ||
        !SudekiMpInstallRelativeCallHook(&fuel_tick,base+0xccfcdu,base+0xcd440u,fuel_route_entry) ||
        !SudekiMpInstallPointerHook(&crystal_update,(void **)(base+0x2d62e8u),base+0x19c8c0u,crystal_route) ||
        !SudekiMpInstallPointerHook(&ability_update,(void **)(base+0x2d64f8u),base+0xccfb0u,ability_route) ||
        !SudekiMpInstallRelativeCallHook(&flight_enter_hook,base+0x14c6c7u,base+0xcd6a0u,flight_enter_entry) ||
        !SudekiMpInstallRelativeCallHook(&flight_exit_hook,base+0x14c7edu,base+0xcda60u,flight_exit_entry) ||
        !SudekiMpInstallRelativeCallHook(&flight_edge_hook,base+0xc5ac5u,base+0xcde20u,flight_edge_entry)) {
        (void)SudekiMpLanPartyJetpackUninstall(); return FALSE;
    }
    SudekiMpLogFormat("lan_party_jetpack event=installed seat=%u policy=host_native_crystal_owned_elco_replica_fuel\r\n",seat);
    return TRUE;
}
