#include "hooks/lan_story_avatar_party.h"
#include "hooks/lan_story_avatar_spawn.h"
#include "hooks/lan_story_input.h"
#include "hooks/lan_story_task_trace.h"
#include "engine/build_identity.h"
#include "engine/spirit_instance_abi.h"
#include "engine/log.h"
#include <math.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Native avatar party requires the supported x86 ABI"
#endif

/* DevPlayHeroExclusionReport: use the public actual-object operations. The
 * name-based RemovePC path also serializes the character; it is not used.
 * Never delete a leader or permit an empty native party. Native SetAsLead
 * queues a switch which the original group update, not this adapter, runs. */
typedef void *(__attribute__((stdcall)) *MakePointer)(void *);
typedef void *(__attribute__((thiscall)) *DeletePointer)(void *,unsigned);
typedef void (__attribute__((thiscall)) *GroupCall)(void *,void *);
typedef unsigned char (__attribute__((thiscall)) *LeadCall)(void *,void *);
typedef void (__attribute__((thiscall)) *FilterCall)(void *);
enum { COMPONENTS=10, MAX_ENTITIES=8192, MAX_LISTENERS=32 };
static const unsigned component_offsets[COMPONENTS]={0x44,0x4c,0x60,0x74,0x80,0x90,0x94,0x98,0xac,0xb0};
static const unsigned component_vtables[COMPONENTS]={0x2cdefc,0x2cc064,0x2c85fc,0x2d47bc,0x2c8644,0x2cc9ac,0x2d4924,0x2d4994,0x2d4b24,0x2d21e4};
static const unsigned component_sizes[COMPONENTS]={0x104,0x13c,0x60,0x44,0xc0,0x64,0x174,0x14,0x88,0x170};
typedef struct Actor {
    uint8_t *entity,*components[COMPONENTS],*mode,*buffers[2];
    void *primary,*resource;
    uint32_t resource_words[3];
} Actor;
static uint8_t *base;
static DWORD native_thread;
static BOOL busy;
static uint32_t begin_reported;
static MakePointer make_pointer;
static DeletePointer delete_pointer;
static GroupCall add_player,remove_delete;
static LeadCall set_leader;
static FilterCall filter_none;
static struct {
    SudekiMpLanStoryAvatarPartyObservation out;
    Actor heroes[4],avatar;
    void *registry,*descriptor,*wrapper,*ui_controller,*ui_layer,*ui_scene;
    uint32_t load;
    uint64_t filter_dispatch;
    BOOL filter_owned,rotation_open,changed;
} party;
#include "hooks/lan_story_avatar_party_signatures.inc"

static BOOL memory(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || VirtualQuery(p,&m,sizeof(m))!=sizeof(m) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
    DWORD access=m.Protect&0xffu;
    return (access==PAGE_READONLY || access==PAGE_READWRITE || access==PAGE_WRITECOPY ||
        access==PAGE_EXECUTE_READ || access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY) &&
        a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL executable(const void *p) {
    MEMORY_BASIC_INFORMATION m;
    if(!p || VirtualQuery(p,&m,sizeof(m))!=sizeof(m) || m.State!=MEM_COMMIT ||
        (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
    DWORD access=m.Protect&0xffu;
    return access==PAGE_EXECUTE || access==PAGE_EXECUTE_READ ||
        access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
}
static void *ptr(const void *p,unsigned offset) { return *(void *const *)((const uint8_t *)p+offset); }
static uint32_t word(const void *p,unsigned offset) { return *(const uint32_t *)((const uint8_t *)p+offset); }
static BOOL object(const void *p,unsigned size,unsigned vt) {
    return memory(p,size) && ptr(p,0)==base+vt;
}
static BOOL filter_entry(uint8_t *b) {
    static const uint8_t code[]={0x56,0x8b,0xf1,0xc7,0x81,0x84,0,0,0,0,0,0,0,0xe8,0xfe,0x05,0x02,0,0x5e,0xc3};
    return memory(b+0x8ac0,sizeof(code)) && (!memcmp(b+0x8ac0,code,sizeof(code)) ||
        SudekiMpSpiritInstanceFilterNoneEntryExact((HMODULE)b));
}
BOOL SudekiMpLanStoryAvatarPartyImageMatches(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    if(!memory(b,0x1000) || !SudekiMpCheckLoadedExecutable(image) || !filter_entry(b) ||
        !SudekiMpLanStoryTaskTraceAddCallImageExact(image)) return FALSE;
    uint32_t delta=(uint32_t)(uintptr_t)b-0x400000u;
    for(unsigned s=0;s<sizeof(signatures)/sizeof(signatures[0]);++s) {
        const unsigned r=signatures[s].rva,n=signatures[s].length;
        if(!memory(b+r,n) || !executable(b+r)) return FALSE;
        uint32_t h=2166136261u;
        for(unsigned i=0;i<n;++i) {
            uint8_t v=b[r+i];
            for(unsigned j=0;j<signatures[s].count;++j) {
                unsigned at=signatures[s].relocs[j];
                if(i>=at && i<at+4u) { v=(uint8_t)((word(b+r,at)-delta)>>((i-at)*8u)); break; }
            }
            /* TaskTrace owns AddPlayer's inner call, including before a load
             * establishes its native thread. Only its complete immutable
             * ownership proof above permits normalizing the displacement.
             * Keep the opcode and every other byte in the whole entry hash. */
            if(r+i>=0x23261u && r+i<0x23265u)
                v=(uint8_t)((0x23280u-0x23265u)>>((r+i-0x23261u)*8u));
            h=(h^v)*16777619u;
        }
        if(h!=signatures[s].hash) return FALSE;
    }
    return memory(b+0x2c0098,4) && ptr(b,0x2c0098)==b+0x1b30;
}
static BOOL fail(const char *reason) {
    if(party.out.phase!=SUDEKIMP_AVATAR_PARTY_UNKNOWN) {
        SudekiMpLogFormat("avatar_party event=unknown reason=%s phase=%u revision=%lu\r\n",
            reason,(unsigned)party.out.phase,(unsigned long)party.out.membership_revision);
        party.out.phase=SUDEKIMP_AVATAR_PARTY_UNKNOWN;
    }
    SetLastError(ERROR_INVALID_DATA); return FALSE;
}
static BOOL refuse(unsigned bit,const char *reason) {
    if(bit<32u && !(begin_reported&(1u<<bit))) {
        begin_reported|=1u<<bit;
        SudekiMpLogFormat("avatar_party event=begin_refused reason=%s\r\n",reason);
    }
    return FALSE;
}
static BOOL catalogue(void *actor,unsigned *found) {
    uint8_t *r=ptr(base,0x409d8c); void **entries;
    if(!memory(r,0x40) || (party.registry && r!=party.registry)) return FALSE;
    unsigned count=word(r,0x34);
    if(!count || count>MAX_ENTITIES || !memory(entries=ptr(r,0x3c),count*sizeof(void *))) return FALSE;
    unsigned n=0; for(unsigned i=0;i<count;++i) if(entries[i]==actor) ++n;
    if(ptr(base,0x409d8c)!=r || word(r,0x34)!=count || ptr(r,0x3c)!=entries) return FALSE;
    *found=n; return TRUE;
}
static BOOL formation(void *entity,uint8_t *ai) {
    uint8_t *f=ptr(ai,0x40),*g=party.out.group;
    if(!f) return TRUE;
    unsigned n=word(g,0x38),owners=0; void **entries=ptr(g,0x40);
    if(n>MAX_LISTENERS || !memory(entries,n*sizeof(void *))) return FALSE;
    for(unsigned i=0;i<n;++i) if(object(entries[i],0x110,0x2ca244) &&
        f==(uint8_t *)entries[i]+0xb0) ++owners;
    if(owners!=1 || !memory(f,0x60) || word(f,0x30)!=word(g,0xcc) || word(f,0x30)>4u) return FALSE;
    unsigned found=0;
    for(unsigned i=0;i<word(f,0x30);++i) if(ptr(f,i*12u)==entity) ++found;
    return found==1;
}
static BOOL actor(Actor *a,void *entity,BOOL capture) {
    unsigned found=0; uint8_t *e=entity;
    if(!catalogue(e,&found) || found!=1 || !memory(e,0x138)) return FALSE;
    if(capture) {
        a->entity=e; a->primary=ptr(e,0); a->resource=ptr(e,0x2c);
        memcpy(a->resource_words,e+0x30,12);
    } else if(e!=a->entity || ptr(e,0)!=a->primary || ptr(e,0x2c)!=a->resource ||
        memcmp(a->resource_words,e+0x30,12)) return FALSE;
    for(unsigned i=0;i<COMPONENTS;++i) {
        uint8_t *c=ptr(e,component_offsets[i]);
        if(!object(c,component_sizes[i],component_vtables[i]) || ptr(c,0x10)!=e ||
            (!capture && c!=a->components[i])) return FALSE;
        if(capture) a->components[i]=c;
    }
    uint8_t *ai=a->components[6],*mode=ptr(ai,0x3c);
    unsigned mode_class=e==party.avatar.entity?0x2da360:0x2da340;
    if(!object(mode,0x10,mode_class) || (!capture && mode!=a->mode)) return FALSE;
    if(capture) a->mode=mode;
    for(unsigned i=0;i<2;++i) {
        uint8_t *b=ptr(ai,0x16c+i*4u);
        /* These are constructor-owned small buffers, not polymorphic objects.
         * The native mode setter resets +8/+C; no synthetic buffer is used. */
        if(!memory(b,16) || !memory(ptr(b,0),4) || word(b,4)!=8u || word(b,8)>8u ||
            (!capture && b!=a->buffers[i])) return FALSE;
        if(capture) a->buffers[i]=b;
    }
    return formation(e,ai);
}
static BOOL idle(Actor *a,BOOL leader,BOOL avatar) {
    uint8_t *ai=a->components[6],*stats=a->components[1],*arbiter=a->components[5];
    float hp=*(float *)(stats+0x30),sp=*(float *)(stats+0x38);
    return *(int16_t *)(ai+0x16a)==0 && a->mode[0xb]==(leader?0u:1u) &&
        (!avatar || (a->mode[8]==0xffu && !ptr(ai,0x40))) &&
        (!leader || a->mode[8]==0xffu) && !ptr(ai,0xc0) &&
        !(word(arbiter,0x50)&0x0289e568u) &&
        isfinite(hp) && hp>0 && isfinite(sp) && sp>0;
}
static BOOL input_neutral(void) {
    uint8_t *c=party.out.controller;
    if(!SudekiMpLanStoryInputControllerExact(c) || !memory(c,0x24c)) return FALSE;
    for(unsigned i=0x50;i<0x80;++i) if(c[i]) return FALSE;
    for(unsigned i=0;i<17;++i) if(word(c,0x8c+i*8u) || *(float *)(c+0x90+i*8u)!=0) return FALSE;
    for(unsigned i=0x114;i<0x1a0;++i) if(c[i]) return FALSE;
    for(unsigned i=0x1a0;i<0x1b8;i+=4) if(*(float *)(c+i)!=0) return FALSE;
    return !c[0x1c9] && *(float *)(c+0x1b8)==2.0f;
}
static BOOL filter_state(void) {
    uint8_t *c=party.out.controller;
    return party.filter_owned && !word(c,0x84) &&
        (word(c,0x80)==0 || (party.out.phase==SUDEKIMP_AVATAR_PARTY_FILTER_PENDING && word(c,0x80)==1));
}
static BOOL ui_ready(void) {
    uint8_t *u=party.ui_controller,*h=party.ui_layer,*scene=party.ui_scene;
    if(ptr(base,0x3c2f88)!=u || ptr(base,0x3c2f9c)!=h || ptr(base,0x408d1c)!=scene ||
        !object(u,0x110,0x2caf9c) || ptr(u,0x6c)!=h ||
        !memory(scene,0x174) || ptr(scene,0x170)!=u ||
        !object(h,0x1a4,0x2cb3e4) || !object(h+0x10c,0x58,0x2d9004) || !h[0x134] ||
        !object(h+0x40,0xcc,0x2d8fb4) || ptr(h,0x68)!=base+0x2d8fd8 ||
        ptr(base+0x2d8fb4,0x14)!=base+0x15c2d0 || word(h,0x158)>4u) return FALSE;
    for(unsigned i=0;i<4;++i) {
        uint8_t *g=ptr(h,0x138+i*4u),*animation,*node,*render;
        if(!object(g,0xc00,0x2cb590) || ptr(g,4)!=base+0x2cb59c || word(g,0x32c)>=4u ||
            !object(animation=ptr(g,0x320),0x13c,0x2d1da8) || ptr(h,0x148+i*4u)!=animation ||
            !object(node=ptr(animation,0xbc),0x1c,0x2d1df0) ||
            !object(render=ptr(node,8),0x38,0x2dd700) || !ptr(node,0xc) ||
            ptr(render,0x14)!=ptr(node,0xc) || ptr(node,0x14)!=ptr(scene,0x70)) return FALSE;
        for(unsigned j=0;j<i;++j) if(g==ptr(h,0x138+j*4u)) return FALSE;
    }
    return TRUE;
}
static BOOL listeners(void) {
    uint8_t *g=party.out.group; unsigned n=word(g,0x38); void **entries=ptr(g,0x40);
    if(!n || n>MAX_LISTENERS || !memory(entries,n*sizeof(void *))) return FALSE;
    for(unsigned i=0;i<n;++i) {
        uint8_t *p=entries[i]; if(!p) continue;
        if(!memory(p,4)) return FALSE;
        uint8_t *vt=ptr(p,0); unsigned primary,add,remove;
        if(vt==base+0x2ca244) { primary=0x8760; add=0xf2b00; remove=0xf2b30; }
        else if(vt==base+0x2c5248) {
            primary=0x8760; add=0xb360; remove=0xb3d0;
            if(!memory(p,0x140) || ptr(p,0x13c)!=party.ui_controller || !ui_ready() ||
                ptr(base+0x2caf9c,0x2c)!=base+0x9c930) return FALSE;
        } else if(vt==base+0x2d4d14) { primary=0xc8d20; add=remove=0x8780; }
        else if(vt==base+0x2d4fa4) { primary=0xbcc30; add=0x8780; remove=0xbcc60; }
        else return FALSE;
        if(!memory(vt,0x20) || ptr(vt,0x14)!=base+primary || ptr(vt,0x18)!=base+add ||
            ptr(vt,0x1c)!=base+remove) return FALSE;
    }
    return word(g,0x38)==n && ptr(g,0x40)==entries;
}
static BOOL tuple(void) {
    SudekiMpLanStoryTaskTraceStatus trace;
    return base && native_thread==GetCurrentThreadId() &&
        SudekiMpLanStoryTaskTraceGetStatus(&trace) && !trace.unknown && trace.load_generation==party.load &&
        SudekiMpLanStoryTaskTraceEntitySetupExact((HMODULE)base) &&
        ptr(base,0x408d10)==party.out.world && object(party.out.world,0x39b,0x2c4c3c) &&
        ptr(party.out.world,0xc)==party.descriptor &&
        ptr(base,0x408d94)==party.out.group && object(party.out.group,0xd8,0x2c6d30) &&
        ptr(base,0x408da4)==party.out.controller && object(party.out.controller,0x24c,0x2c9f5c) &&
        ptr(party.out.controller,0x2c)==base+0x2c9f84 &&
        !word(party.out.group,0x58) && !word(party.out.group,0x5c) && listeners();
}
static BOOL members(BOOL rotating) {
    uint8_t *g=party.out.group; unsigned n=word(g,0xcc);
    if(n!=party.out.member_count || !n || n>4) return FALSE;
    BOOL same=TRUE,swapped=rotating && n==2;
    for(unsigned i=0;i<4;++i) {
        void *a=ptr(g,0x90+i*12u);
        if(a!=party.out.members[i]) same=FALSE;
        if(a!=(i<2?party.out.members[1u-i]:NULL)) swapped=FALSE;
    }
    void *target=ptr(party.out.controller,0x248),*lead=ptr(g,0x90),*pending=ptr(g,0xc0);
    if(target!=lead || (!same && !swapped) ||
        (pending && (!rotating || pending!=party.avatar.entity))) return FALSE;
    return TRUE;
}
static BOOL avatars_exact(void) {
    if(party.rotation_open) return actor(&party.avatar,party.avatar.entity,FALSE);
    SudekiMpLanStoryAvatarSpawnObservation o;
    return SudekiMpLanStoryAvatarSpawnObserve(party.out.local_player,party.out.epoch,
        party.out.spawn_generation,&o) && o.ready && !o.unknown && o.actor==party.avatar.entity &&
        o.world==party.out.world && actor(&party.avatar,o.actor,FALSE);
}
static void publish_members(void) {
    uint8_t *g=party.out.group;
    party.out.member_count=word(g,0xcc); party.out.hero_mask=0;
    memset(party.out.heroes,0,sizeof(party.out.heroes));
    for(unsigned i=0;i<4;++i) {
        party.out.members[i]=i<party.out.member_count?ptr(g,0x90+i*12u):NULL;
        for(unsigned c=0;c<4;++c) if(party.out.members[i] && party.out.members[i]==party.heroes[c].entity) {
            party.out.heroes[c]=party.heroes[c].entity; party.out.hero_mask|=(uint8_t)(1u<<c);
        }
    }
    party.out.native_leader=party.out.members[0]; party.out.leader_character=SUDEKIMP_LAN_STORY_NO_SEAT;
    for(unsigned c=0;c<4;++c) if(party.out.native_leader==party.out.heroes[c]) party.out.leader_character=(uint8_t)c;
}
BOOL SudekiMpLanStoryAvatarPartyInstall(HMODULE image) {
    if(base) return base==(uint8_t *)image && SudekiMpLanStoryAvatarPartyImageMatches(image);
    if(!SudekiMpLanStoryAvatarPartyImageMatches(image)) return FALSE;
    base=(uint8_t *)image;
    make_pointer=(MakePointer)(base+0x1c20); delete_pointer=(DeletePointer)(base+0x1b30);
    add_player=(GroupCall)(base+0x23230); remove_delete=(GroupCall)(base+0x235e0);
    set_leader=(LeadCall)(base+0x24e40); filter_none=(FilterCall)(base+0x8ac0);
    return TRUE;
}
static BOOL dispatch(const SudekiMpControlUpdateDispatchWitness *w) {
    return w && w->service_only && w->service_post_original_exact && w->dispatch_serial &&
        SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w);
}
BOOL SudekiMpLanStoryAvatarPartyBegin(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,unsigned local,uint32_t generation,uint8_t selected) {
    SudekiMpLanStoryAvatarSpawnObservation spawn; SudekiMpLanStoryTaskTraceStatus trace;
    if(!base || busy || party.out.phase!=SUDEKIMP_AVATAR_PARTY_OFF || selected || local>=4 || !generation ||
        !r || !r->epoch || !r->available_mask || r->leader_character>=4 || !dispatch(w) ||
        !SudekiMpLanStoryObserverRosterStillExact(w,r) || !SudekiMpLanStoryAvatarPartyImageMatches((HMODULE)base) ||
        !SudekiMpLanStoryAvatarSpawnObserve(local,r->epoch,generation,&spawn) || !spawn.ready || spawn.unknown ||
        spawn.world!=r->world || !SudekiMpLanStoryTaskTraceGetStatus(&trace) || trace.unknown)
        return refuse(16,"scope_or_spawn");
    memset(&party,0,sizeof(party)); native_thread=GetCurrentThreadId();
    unsigned stage=0; const char *reason="tuple";
    party.out=(SudekiMpLanStoryAvatarPartyObservation){.phase=SUDEKIMP_AVATAR_PARTY_PREPARING,
        .epoch=r->epoch,.spawn_generation=generation,.membership_revision=1,.local_player=(uint8_t)local,
        .world=r->world,.group=r->group,.controller=r->controller};
    party.load=trace.load_generation; party.descriptor=r->descriptor; party.registry=ptr(base,0x409d8c);
    party.ui_controller=ptr(base,0x3c2f88); party.ui_layer=ptr(base,0x3c2f9c); party.ui_scene=ptr(base,0x408d1c);
    party.avatar.entity=spawn.actor;
    if(!tuple() || word(r->group,0xd0)) goto refusal;
    stage=1; reason="input_cache"; if(!input_neutral()) goto refusal;
    stage=2; reason="original_group_target";
    if(ptr(r->group,0xc0) || ptr(r->controller,0x248)!=r->actors[r->leader_character]) goto refusal;
    stage=3; reason="original_filter";
    if(word(r->controller,0x80)!=1 || word(r->controller,0x84)!=1) goto refusal;
    stage=4; reason="avatar_components"; if(!actor(&party.avatar,spawn.actor,TRUE)) goto refusal;
    stage=5; reason="avatar_ai_idle"; if(!idle(&party.avatar,FALSE,TRUE)) goto refusal;
    for(unsigned c=0;c<4;++c) if(r->actors[c]) {
        stage=6+c; reason="hero_components";
        if(!actor(&party.heroes[c],r->actors[c],TRUE)) goto refusal;
        stage=10+c; reason="hero_ai_idle";
        if(!idle(&party.heroes[c],c==r->leader_character,FALSE)) goto refusal;
    }
    publish_members();
    stage=14; reason="canonical_membership";
    if(party.out.hero_mask!=r->available_mask || party.out.native_leader!=r->actors[r->leader_character]) goto refusal;
    party.filter_dispatch=w->dispatch_serial;
    party.out.phase=SUDEKIMP_AVATAR_PARTY_FILTER_PENDING;
    busy=TRUE; filter_none(r->controller); busy=FALSE;
    party.filter_owned=TRUE;
    if(!tuple() || !members(FALSE) || !input_neutral() || !filter_state()) return fail("filter_transition");
    SudekiMpLogFormat("avatar_party event=begin epoch=%lu player=%u heroes=%u filter=pending current=%lu requested=%lu\r\n",
        (unsigned long)r->epoch,local,(unsigned)party.out.hero_mask,
        (unsigned long)word(r->controller,0x80),(unsigned long)word(r->controller,0x84));
    return TRUE;
refusal:
    memset(&party,0,sizeof(party)); return refuse(stage,reason);
}
static BOOL wrapper_open(void *entity) {
    if(party.wrapper) return FALSE;
    party.wrapper=make_pointer(entity);
    return object(party.wrapper,0x18,0x2c0098) && ptr(party.wrapper,0xc)==entity;
}
static BOOL wrapper_close(void) {
    if(!object(party.wrapper,0x18,0x2c0098)) return FALSE;
    void *p=party.wrapper; delete_pointer(p,1); party.wrapper=NULL; return TRUE;
}
static BOOL mutate(SudekiMpLanStoryAvatarSpawnGroupOperation operation,void *entity) {
    if(!SudekiMpLanStoryAvatarSpawnGroupBegin(&party,party.out.epoch,operation,entity)) return fail("group_begin");
    busy=TRUE;
    if(!wrapper_open(entity)) { busy=FALSE; return fail("native_pointer"); }
    if(operation==SUDEKIMP_AVATAR_SPAWN_GROUP_ADD) add_player(party.out.group,party.wrapper);
    else remove_delete(party.out.group,party.wrapper);
    BOOL deleted=operation!=SUDEKIMP_AVATAR_SPAWN_GROUP_REMOVE ||
        (object(party.wrapper,0x18,0x2c0098) && !ptr(party.wrapper,0xc));
    BOOL closed=wrapper_close(); busy=FALSE;
    BOOL changed=FALSE;
    if(!closed || !SudekiMpLanStoryAvatarSpawnGroupEnd(&party,&changed) || !changed) return fail("group_result");
    party.changed=TRUE; ++party.out.membership_revision;
    publish_members();
    unsigned remaining=0;
    if(!deleted || (operation==SUDEKIMP_AVATAR_SPAWN_GROUP_REMOVE &&
        (!catalogue(entity,&remaining) || remaining))) return fail("delete_not_retired");
    if(!tuple() || !members(FALSE) || !avatars_exact()) return fail("post_membership");
    return TRUE;
}
BOOL SudekiMpLanStoryAvatarPartyService(const SudekiMpControlUpdateDispatchWitness *w,void *controller) {
    if(!base || busy || !dispatch(w) || !party.out.phase || party.out.phase==SUDEKIMP_AVATAR_PARTY_UNKNOWN ||
        controller!=party.out.controller || native_thread!=GetCurrentThreadId()) return FALSE;
    BOOL rotating=party.out.phase==SUDEKIMP_AVATAR_PARTY_LEAD_PENDING;
    if(!SudekiMpLanStoryAvatarPartyImageMatches((HMODULE)base) || !tuple() || !members(rotating) || !input_neutral() ||
        !filter_entry(base)) return fail("service_tuple");
    if(!filter_state()) return fail("filter_state");
    if(party.out.phase==SUDEKIMP_AVATAR_PARTY_FILTER_PENDING) {
        if(!avatars_exact()) return fail("avatar_identity");
        /* FilterNone8AC0 writes pending+84 only. The original27CF0 tail
         * commits current+80 at285D7. Do not manufacture this write, repeat
         * the request, or mutate membership inside its dispatch. */
        if(w->dispatch_serial<=party.filter_dispatch || word(controller,0x80)) return TRUE;
        party.out.phase=SUDEKIMP_AVATAR_PARTY_PREPARING;
        SudekiMpLogFormat("avatar_party event=filter_committed epoch=%lu player=%u current=0 requested=0\r\n",
            (unsigned long)party.out.epoch,(unsigned)party.out.local_player);
    }
    if(rotating) {
        if(ptr(party.out.group,0x90)!=party.avatar.entity) return TRUE;
        if(party.rotation_open) {
            BOOL changed=FALSE;
            if(!SudekiMpLanStoryAvatarSpawnGroupEnd(&party,&changed) || !changed) return fail("lead_receipt");
            party.rotation_open=FALSE; party.changed=TRUE; ++party.out.membership_revision;
            publish_members();
        }
        if(ptr(party.out.group,0xc0)) return TRUE;
        if(!actor(&party.avatar,party.avatar.entity,FALSE) ||
            *(int16_t *)(party.avatar.components[6]+0x16a) || party.avatar.mode[0xb]!=0) return fail("lead_ai");
        party.out.phase=SUDEKIMP_AVATAR_PARTY_DELETE_PENDING;
    }
    if(!avatars_exact()) return fail("avatar_identity");
    if(party.out.phase==SUDEKIMP_AVATAR_PARTY_READY) return TRUE;
    /* +D0 is the native leader-switch prohibition, not a lifetime identity.
     * It gates membership work; a later native lock does not replace an
     * already completed party or invalidate its observer epoch. */
    if(word(party.out.group,0xd0)) return TRUE;
    /* Delete companions first: the switch then has exactly two members and
     * cannot rotate through an unselected hero or overflow native capacity. */
    if(party.out.member_count>1) {
        void *e=party.out.members[party.out.member_count-1u];
        if(e!=party.avatar.entity) {
            Actor *old=NULL; for(unsigned c=0;c<4;++c) if(party.heroes[c].entity==e) old=&party.heroes[c];
            if(!old || !actor(old,e,FALSE) || !idle(old,FALSE,FALSE)) return fail("delete_preflight");
            if(!mutate(SUDEKIMP_AVATAR_SPAWN_GROUP_REMOVE,e)) return FALSE;
            memset(old,0,sizeof(*old)); return TRUE;
        }
    }
    if(party.out.native_leader!=party.avatar.entity) {
        if(party.out.member_count==1) {
            if(!idle(&party.avatar,FALSE,TRUE)) return fail("add_preflight");
            return mutate(SUDEKIMP_AVATAR_SPAWN_GROUP_ADD,party.avatar.entity);
        }
        if(party.out.member_count!=2 || party.out.members[1]!=party.avatar.entity ||
            !SudekiMpLanStoryAvatarSpawnGroupBegin(&party,party.out.epoch,
                SUDEKIMP_AVATAR_SPAWN_GROUP_ROTATE,party.avatar.entity)) return fail("lead_begin");
        party.rotation_open=TRUE; busy=TRUE;
        if(!wrapper_open(party.avatar.entity)) { busy=FALSE; return fail("lead_pointer"); }
        BOOL queued=set_leader(party.out.group,party.wrapper)!=0;
        BOOL closed=wrapper_close(); busy=FALSE;
        if(!closed || !queued || ptr(party.out.group,0xc0)!=party.avatar.entity) return fail("lead_queue");
        party.out.phase=SUDEKIMP_AVATAR_PARTY_LEAD_PENDING; return TRUE;
    }
    if(party.out.member_count!=1 || party.out.hero_mask) return fail("final_group");
    party.out.phase=SUDEKIMP_AVATAR_PARTY_READY;
    SudekiMpLogFormat("avatar_party event=ready epoch=%lu player=%u heroes=0 members=1 revision=%lu\r\n",
        (unsigned long)party.out.epoch,(unsigned)party.out.local_player,(unsigned long)party.out.membership_revision);
    return TRUE;
}
BOOL SudekiMpLanStoryAvatarPartyObserve(SudekiMpLanStoryAvatarPartyObservation *out) {
    if(!out || !base || busy || !party.out.phase || party.out.phase==SUDEKIMP_AVATAR_PARTY_UNKNOWN ||
        !tuple() || !members(party.out.phase==SUDEKIMP_AVATAR_PARTY_LEAD_PENDING) || !filter_state() || !avatars_exact()) return FALSE;
    for(unsigned c=0;c<4;++c) if(party.out.heroes[c] && !actor(&party.heroes[c],party.out.heroes[c],FALSE)) return FALSE;
    *out=party.out; return TRUE;
}
BOOL SudekiMpLanStoryAvatarPartyRetains(void) { return party.out.phase!=SUDEKIMP_AVATAR_PARTY_OFF || party.wrapper || busy; }
BOOL SudekiMpLanStoryAvatarPartyUninstall(void) {
    if(!base) return TRUE;
    /* Spawn remains the sole shared EntitySetup observer. Consume its exact
     * original-world receipt after TaskTraceForgetExitedWorld and before
     * SpawnShutdown. No retired native object is dereferenced here. */
    if(party.out.phase && !party.wrapper && !busy &&
        SudekiMpLanStoryAvatarSpawnWorldExited(party.out.epoch,party.load,party.out.world))
        memset(&party,0,sizeof(party));
    if(SudekiMpLanStoryAvatarPartyRetains() || (native_thread && native_thread!=GetCurrentThreadId())) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    base=NULL; native_thread=0; begin_reported=0;
    make_pointer=NULL; delete_pointer=NULL; add_player=remove_delete=NULL; set_leader=NULL; filter_none=NULL;
    return TRUE;
}
