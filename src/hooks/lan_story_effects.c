#include "hooks/lan_story_effects.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include "hooks/call_hook.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Saved-story effects require the supported x86 ABI"
#endif

enum {
    PARTICLE_CALL=0x1d4915u, PARTICLE_UPDATE=0x22faa0u,
    MODEL_CLOCK_CALL=0x1f9faeu, MODEL_CLOCK_SET=0x231860u,
    SCENE_MANAGER=0x408d58u, WORLD_GLOBAL=0x408d10u,
    PARTICLE_ANIMATION_VT=0x2e22c8u, EMITTER_VT=0x2e26ccu,
    MAX_INSTANCES=128u, MAX_EMITTERS=64u, MAX_EFFECTS=128u,
    MAX_MODIFIERS=64u, MAX_NODES=2048u
};
typedef void (__stdcall *ParticleUpdate)(void *manager,float delta);
static uint8_t *base;
static SudekiMpRelativeCallHook update_hook;
static SudekiMpRelativeCallHook model_clock_hook;
static void *original_model_clock __attribute__((used));
static volatile LONG clock_callbacks;
static unsigned clock_trace_logs;
typedef struct ModelClockObservation {
    void *renderer,*instance,*resource;
    uint32_t source,last_tick;
    unsigned observations,logs;
} ModelClockObservation;
static ModelClockObservation model_observations[MAX_INSTANCES];
static ParticleUpdate original_update;
static SudekiMpLanStoryEffectsDispatch dispatcher;
static void *dispatch_context;
static volatile LONG callbacks,stopping;
static DWORD native_thread;
static BOOL installed,active,original_entered,advanced;
static uint8_t *current_scene,*current_manager;
static float current_delta;
static struct {
    BOOL valid;
    uint32_t epoch,revision,sequence,host_tick,local_tick;
    void *world,*descriptor,*controller;
} clock_state;
static uint32_t successful,refused;
static unsigned topology_stage,topology_index,topology_table;
static unsigned refusal_logs,last_refusal_stage,last_refusal_table;
static uint32_t last_refusal_tick;
static LARGE_INTEGER timing_frequency;
static uint64_t dispatch_us;
static uint32_t dispatch_count,dispatch_max_us;
/* Plain observations only: each pointer is compared after this frame's full
 * manager/resource/node proof. Never dereference a cached node. */
typedef struct CustomClock {
    void *instance,*node,*data;
    float phase;
} CustomClock;
static CustomClock custom_clocks[MAX_NODES];
static unsigned custom_clock_count,custom_advanced;
static uint8_t verified_clock_code[4][153];
static const struct { unsigned rva,size; uint32_t hash; } clock_code[]={
    {0x235520u,105u,0x3818f16bu}, {0x235590u,153u,0xa2c1e045u},
    {0x233bd0u,10u,0x2021e574u}, {0x237990u,10u,0xa572a92du}
};
typedef void (__attribute__((thiscall)) *AdvanceNodeClock)(void *,float);

static BOOL pin(DWORD error) {
    HMODULE self;
    (void)GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_PIN,
        (LPCWSTR)(uintptr_t)&SudekiMpLanStoryEffectsUninstall,&self);
    SetLastError(error); return FALSE;
}
static BOOL readable(const void *pointer,size_t size) {
    MEMORY_BASIC_INFORMATION m;
    uintptr_t p=(uintptr_t)pointer;
    if(!pointer || !size || p>UINTPTR_MAX-size ||
        VirtualQuery(pointer,&m,sizeof(m))!=sizeof(m) || m.State!=MEM_COMMIT ||
        (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
    DWORD protection=m.Protect&0xffu;
    if(protection!=PAGE_READONLY && protection!=PAGE_READWRITE &&
        protection!=PAGE_WRITECOPY && protection!=PAGE_EXECUTE_READ &&
        protection!=PAGE_EXECUTE_READWRITE && protection!=PAGE_EXECUTE_WRITECOPY) return FALSE;
    return p+size<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL call_targets(const uint8_t *call,const void *target) {
    int32_t displacement;
    if(!readable(call,5u) || call[0]!=0xe8u) return FALSE;
    memcpy(&displacement,call+1u,4u); return call+5u+displacement==target;
}
/* Diagnostic only. The previous cosmetic updater can advance a custom node,
 * then native model playback can set it back to the contained world time.
 * Observe the actual setter arguments before changing clock ownership. This
 * seam never suppresses the original, edits its arguments, or retains a node.
 * The native call takes ESI=instance, one stack float, and owns RET4. */
static void model_clock_entry(void);
typedef struct ClockObserveState {
    uint8_t fp[512] __attribute__((aligned(16)));
    DWORD error;
} ClockObserveState;
static void clock_observe_leave(ClockObserveState *s) {
    SetLastError(s->error);
    __asm__ volatile("fxrstor %0" : : "m"(s->fp) : "memory");
}
__attribute__((noinline,used,force_align_arg_pointer))
static void observe_model_clock(uint8_t *sub,uint8_t *instance,uint32_t source) {
    ClockObserveState saved __attribute__((cleanup(clock_observe_leave)));
    const uint32_t mxcsr=0x1f80u;
    __asm__ volatile("fxsave %0" : "=m"(saved.fp) : : "memory");
    __asm__ volatile("fninit; ldmxcsr %0" : : "m"(mxcsr) : "memory");
    saved.error=GetLastError();
    LONG depth=InterlockedIncrement(&clock_callbacks);
    if(depth!=1 || !installed || !base || native_thread!=GetCurrentThreadId() ||
        InterlockedCompareExchange(&stopping,0,0) || clock_trace_logs>=256u ||
        !model_clock_hook.installed || !call_targets(base+MODEL_CLOCK_CALL,model_clock_entry) ||
        (uintptr_t)sub<4u) goto done;
    uint8_t *renderer=sub-4u;
    if(!readable(renderer,0x28u) || *(void **)renderer!=base+0x2de564u ||
        *(void **)sub!=base+0x2de658u || *(void **)(sub+0x10u)!=instance ||
        !readable(instance,0x8cu) || *(void **)instance!=base+PARTICLE_ANIMATION_VT ||
        !readable(base+SCENE_MANAGER,4u)) goto done;
    uint8_t *owner=*(uint8_t **)(base+SCENE_MANAGER);
    if(!readable(owner,0x44u)) goto done;
    uint8_t *scene=*(uint8_t **)(owner+0x40u);
    if(!readable(scene,0x8cu) || !scene[0x88u]) goto done;
    uint8_t *manager=*(uint8_t **)(scene+0x54u);
    if(!readable(manager,0x10u) || *(void **)(instance+0x1cu)!=manager) goto done;
    unsigned count=*(unsigned *)(manager+4u),capacity=*(unsigned *)(manager+8u),matches=0;
    void **items=*(void ***)(manager+0xcu);
    if(!count || count>MAX_INSTANCES || count>capacity || capacity>4096u ||
        !readable(items,count*4u)) goto done;
    for(unsigned i=0;i<count;++i) if(items[i]==instance) ++matches;
    if(matches!=1u) goto done;
    uint8_t *node=NULL;
    for(unsigned kind=0;kind<2u && !node;++kind) {
        unsigned n=*(unsigned *)(instance+(kind?0x14u:8u));
        uint8_t **nodes=*(uint8_t ***)(instance+(kind?0x10u:4u));
        if(n>(kind?MAX_EFFECTS:MAX_EMITTERS) || (n && !readable(nodes,n*4u))) goto done;
        for(unsigned i=0;i<n;++i) {
            uint8_t *p=nodes[i]; unsigned offset=kind?0x120u:0x190u;
            if(!readable(p,offset+4u)) goto done;
            uint8_t *vt=*(uint8_t **)p,*data=*(uint8_t **)(p+offset);
            /* The exact native getter and sampler, not arbitrary readable
             * objects. No callback is invoked by this observer. */
            if(!readable(vt,0x38u) || *(void **)(vt+4u)!=base+0x235590u ||
                *(void **)(vt+0x34u)!=base+(kind?0x237990u:0x233bd0u) ||
                !readable(data,kind?9u:10u)) goto done;
            if(data[kind?8u:9u]==1u) { node=p; break; }
        }
    }
    if(!node) goto done;
    void *resource=*(void **)(renderer+0x10u);
    ModelClockObservation *observation=NULL,*empty=NULL;
    for(unsigned i=0;i<MAX_INSTANCES;++i) {
        ModelClockObservation *o=&model_observations[i];
        if(!o->renderer && !empty) empty=o;
        if(o->renderer==renderer && o->instance==instance && o->resource==resource) {
            observation=o; break;
        }
    }
    if(!observation) {
        if(!empty) goto done;
        observation=empty;
        *observation=(ModelClockObservation){.renderer=renderer,.instance=instance,.resource=resource};
    }
    uint32_t now=GetTickCount(),previous=observation->source;
    ++observation->observations; observation->source=source;
    if(observation->logs<4u || (observation->logs<12u && now-observation->last_tick>=1000u)) {
        ++clock_trace_logs; ++observation->logs; observation->last_tick=now;
        uint32_t times[5]; memcpy(times,node+0xd8u,sizeof(times));
        SudekiMpLogFormat("story_effect_clock event=model_set renderer=%p resource=%p instance=%p node=%p source_bits=%08lx previous_source_bits=%08lx phase_bits=%08lx delta_bits=%08lx previous_phase_bits=%08lx duration_bits=%08lx observations=%u policy=observe_only_original_unchanged\r\n",
            renderer,resource,instance,node,(unsigned long)source,(unsigned long)previous,
            (unsigned long)times[0],(unsigned long)times[1],(unsigned long)times[2],
            (unsigned long)times[4],observation->observations);
    }
done:
    InterlockedDecrement(&clock_callbacks);
}
__attribute__((naked,noinline,used))
static void model_clock_entry(void) {
    __asm__ volatile("pushfl\n\tpushal\n\tpushl 40(%esp)\n\tpushl %esi\n\tpushl %edi\n\t"
        "call _observe_model_clock\n\taddl $12,%esp\n\tpopal\n\tpopfl\n\t"
        "jmp *_original_model_clock\n\t");
}
static void update_entry(void);
static BOOL hook_exact(void) {
    return update_hook.installed && call_targets(update_hook.instruction,update_entry);
}
static BOOL main_manager(uint8_t *scene,uint8_t *manager) {
    if(!base || !readable(base+SCENE_MANAGER,4u)) return FALSE;
    uint8_t *owner=*(uint8_t **)(base+SCENE_MANAGER);
    return readable(owner,0x44u) && *(void **)(owner+0x40u)==scene &&
        readable(scene,0x8cu) && scene[0x88u] &&
        *(void **)(scene+0x54u)==manager && readable(manager,0x24u);
}
BOOL SudekiMpLanStoryEffectsWitness(void *unused) {
    (void)unused;
    return installed && active && native_thread==GetCurrentThreadId() &&
        InterlockedCompareExchange(&callbacks,0,0)==1 &&
        !InterlockedCompareExchange(&stopping,0,0) && hook_exact() &&
        current_delta==0.0f && main_manager(current_scene,current_manager);
}
void SudekiMpLanStoryEffectsResetClock(void) {
    if(!native_thread || native_thread==GetCurrentThreadId()) {
        memset(&clock_state,0,sizeof(clock_state));
        custom_clock_count=0u;
    }
}
static BOOL method(const uint8_t *table,unsigned slot,unsigned target) {
    return readable(table+slot,4u) && *(void **)(table+slot)==base+target;
}
static BOOL member(void *target,void **items,unsigned count) {
    for(unsigned i=0;i<count;++i) if(items[i]==target) return TRUE;
    return FALSE;
}
static BOOL reject(unsigned stage) {
    uint32_t now=GetTickCount();
    ++refused;
    if(stage) topology_stage=stage;
    if(refusal_logs<64u && (!refusal_logs || topology_stage!=last_refusal_stage ||
        topology_table!=last_refusal_table || now-last_refusal_tick>=5000u)) {
        SudekiMpLogFormat("lan_story event=effects_rejected stage=%u instance=%u table_rva=%08x count=%lu\r\n",
            topology_stage,topology_index,topology_table,(unsigned long)refused);
        ++refusal_logs; last_refusal_stage=topology_stage;
        last_refusal_table=topology_table; last_refusal_tick=now;
    }
    SudekiMpLanStoryEffectsResetClock(); return FALSE;
}
static BOOL node_times(const uint8_t *node) {
    for(unsigned offset=0xd8u;offset<=0xe8u;offset+=4u)
        if(!isfinite(*(const float *)(node+offset))) return FALSE;
    return *(const float *)(node+0xe8u)>0.0f &&
        *(const float *)(node+0xe8u)<=1000000.0f;
}
/* Exact native callback closure, observed on the loaded main-world manager.
 * These types operate on particle arrays, render transforms, authored curves,
 * native visual PRNG and renderer allocations. They contain no entity/GEL/AI
 * dispatch. Any other type rejects the complete manager BEFORE native entry. */
static BOOL effect(uint8_t *node) {
    topology_stage=40u; topology_table=0u;
    if(!readable(node,0x160u) || !node_times(node)) return FALSE;
    uint8_t *table=*(uint8_t **)node;
    topology_table=(unsigned)((uintptr_t)table-(uintptr_t)base);
    unsigned update=0x0a2900u,modifier=0,apply=0;
    if(table==base+0x2e2b14u) modifier=0x244510u;
    else if(table==base+0x2e2b5cu) modifier=0x237e40u;
    else if(table==base+0x2e2c94u) modifier=0x23d5b0u;
    else if(table==base+0x2e2fecu) modifier=0x2450a0u;
    else if(table==base+0x2e2d7cu) {update=0x240450u;modifier=0x248010u;apply=0x240550u;}
    else if(table==base+0x2e2eacu) {modifier=0x248010u;apply=0x2432d0u;}
    else if(table==base+0x2e2f3cu) {modifier=0x248010u;apply=0x244000u;}
    else return FALSE;
    if(!method(table,4u,0x235590u) || !method(table,8u,update) ||
        !method(table,0x34u,0x237990u) || !method(table,0x38u,modifier) ||
        (apply && !method(table,0x3cu,apply))) return FALSE;
    return readable(*(void **)(node+0x120u),0x10u);
}
static BOOL emitter(uint8_t *node,void **effects,unsigned count) {
    topology_stage=50u; topology_table=EMITTER_VT;
    if(!readable(node,0x1e8u) || *(void **)node!=base+EMITTER_VT ||
        !node_times(node)) return FALSE;
    uint8_t *table=*(uint8_t **)node;
    if(!method(table,4u,0x235590u) || !method(table,8u,0x2371a0u) ||
        !method(table,0x34u,0x233bd0u) || !method(table,0x38u,0x2373b0u) ||
        !method(table,0x3cu,0x237450u) ||
        !readable(*(void **)(node+0x190u),0x60u)) return FALSE;
    /* All effect nodes were fully checked above in this same synchronous
     * topology pass. Membership reuses that proof without any intervening
     * native call, yield or mutable cross-frame cache. */
    unsigned n=*(unsigned *)(node+0x134u);
    void **modifiers=*(void ***)(node+0x13cu);
    if(n>MAX_MODIFIERS || (n && !readable(modifiers,n*4u))) return FALSE;
    for(unsigned i=0;i<n;++i) {
        uint8_t *modifier=modifiers[i];
        if(!modifier) continue; /* exact native loop permits null entries */
        if(!readable(modifier,8u)) return FALSE;
        uint8_t *mt=*(uint8_t **)modifier,*target;
        topology_stage=60u; topology_table=(unsigned)((uintptr_t)mt-(uintptr_t)base);
        if(mt==base+0x2e2f80u) {
            if(!readable(modifier,0x74u) || !method(mt,8u,0x2444d0u) ||
                !method(mt,0x14u,0x2444c0u)) return FALSE;
            target=*(uint8_t **)(modifier+0x70u);
            if(!member(target,effects,count)) return FALSE;
            uint8_t *target_table=*(uint8_t **)target;
            if(target_table!=base+0x2e2b14u && target_table!=base+0x2e2b5cu &&
                target_table!=base+0x2e2c94u && target_table!=base+0x2e2fecu) return FALSE;
        } else if(mt==base+0x2e2d60u) {
            if(!method(mt,8u,0x2402e0u) || !method(mt,0x14u,0x2402c0u)) return FALSE;
            target=*(uint8_t **)(modifier+4u);
            if(!member(target,effects,count) ||
                !method(*(uint8_t **)target,0x38u,0x248010u)) return FALSE;
        } else return FALSE;
    }
    topology_stage=70u; topology_table=EMITTER_VT;
    n=*(unsigned *)(node+0x174u);
    void **outputs=*(void ***)(node+0x17cu);
    if(n>MAX_EFFECTS || (n && !readable(outputs,n*4u))) return FALSE;
    for(unsigned i=0;i<n;++i)
        if(!outputs[i] || !member(outputs[i],effects,count)) return FALSE;
    return TRUE;
}
typedef struct ParticleResource {
    void *data;
    uint8_t *node_table;
} ParticleResource;
/* 631300/6313B0 copy the authored descriptor vectors into instance+40/+4C.
 * 631E10 increments each descriptor reference; 630F60 constructs the node
 * arrays from those exact descriptors. Factories bind emitter+190 to desc+18
 * and effect+120 to desc+14. Validate this native ownership chain rather than
 * admitting a resource pointer merely because its memory is readable. */
static BOOL resources(void **descriptors,unsigned count,BOOL emitters,
    ParticleResource *out) {
    if(count && !readable(descriptors,count*4u)) return FALSE;
    for(unsigned i=0;i<count;++i) {
        uint8_t *descriptor=descriptors[i];
        if(!readable(descriptor,0x1cu) || !*(unsigned *)(descriptor+4u) ||
            *(unsigned *)(descriptor+4u)==UINT32_MAX) return FALSE;
        uint8_t *table=*(uint8_t **)descriptor;
        unsigned node=0,factory=0;
        if(emitters) {
            if(table!=base+0x2e233cu) return FALSE;
            node=EMITTER_VT; factory=0x232000u;
        } else if(table==base+0x2e23fcu) {node=0x2e2b14u;factory=0x232660u;}
        else if(table==base+0x2e246cu) {node=0x2e2b5cu;factory=0x232980u;}
        else if(table==base+0x2e2434u) {node=0x2e2c94u;factory=0x2327d0u;}
        else if(table==base+0x2e24dcu) {node=0x2e2fecu;factory=0x232ce0u;}
        else if(table==base+0x2e23c4u) {node=0x2e2d7cu;factory=0x2324c0u;}
        else if(table==base+0x2e262cu) {node=0x2e2eacu;factory=0x233640u;}
        else if(table==base+0x2e25bcu) {node=0x2e2f3cu;factory=0x2333b0u;}
        else return FALSE;
        if(!method(table,0x14u,factory)) return FALSE;
        out[i].data=*(void **)(descriptor+(emitters?0x18u:0x14u));
        out[i].node_table=base+node;
        if(!out[i].data) return FALSE;
    }
    return TRUE;
}
static BOOL resource_matches(uint8_t *node,unsigned offset,
    const ParticleResource *resources,unsigned count) {
    unsigned matches=0;
    for(unsigned i=0;i<count;++i)
        if(*(void **)(node+offset)==resources[i].data &&
            *(uint8_t **)node==resources[i].node_table) ++matches;
    return matches==1u;
}
static BOOL topology(uint8_t *manager) {
    topology_stage=10u; topology_index=topology_table=0u;
    if(!readable(manager,0x24u)) return FALSE;
    unsigned count=*(unsigned *)(manager+4u),capacity=*(unsigned *)(manager+8u);
    void **instances=*(void ***)(manager+0xcu);
    float remainder=*(float *)(manager+0x10u),accumulator=*(float *)(manager+0x14u);
    float step=*(float *)(manager+0x1cu);
    if(!count || count>MAX_INSTANCES || capacity<count || capacity>4096u ||
        !readable(instances,count*4u) || !isfinite(remainder) || !isfinite(accumulator) ||
        !isfinite(step) || step<=0.0f || step>24.0f ||
        remainder<0.0f || remainder>step || accumulator<0.0f || accumulator>0.1f) return FALSE;
    unsigned total=0;
    for(unsigned i=0;i<count;++i) {
        uint8_t *instance=instances[i];
        topology_stage=20u; topology_index=i; topology_table=0u;
        if(!instance) continue;
        if(!readable(instance,0x8cu) || *(void **)instance!=base+PARTICLE_ANIMATION_VT ||
            *(void **)(instance+0x1cu)!=manager || instance[0x58u] ||
            instance[0x88u] || !isfinite(*(float *)(instance+0x84u)) ||
            *(float *)(instance+0x84u)!=1.0f) return FALSE;
        unsigned emitters=*(unsigned *)(instance+8u),effects=*(unsigned *)(instance+0x14u);
        void **emitters_list=*(void ***)(instance+4u),**effects_list=*(void ***)(instance+0x10u);
        if(emitters>MAX_EMITTERS || effects>MAX_EFFECTS ||
            (total+=emitters+effects)>MAX_NODES ||
            (emitters && !readable(emitters_list,emitters*4u)) ||
            (effects && !readable(effects_list,effects*4u))) return FALSE;
        ParticleResource emitter_resources[MAX_EMITTERS],effect_resources[MAX_EFFECTS];
        topology_stage=30u;
        if(*(unsigned *)(instance+0x44u)!=emitters || *(unsigned *)(instance+0x50u)!=effects ||
            !resources(*(void ***)(instance+0x40u),emitters,TRUE,emitter_resources) ||
            !resources(*(void ***)(instance+0x4cu),effects,FALSE,effect_resources)) return FALSE;
        for(unsigned e=0;e<effects;++e)
            if(!effect(effects_list[e]) ||
                !resource_matches(effects_list[e],0x120u,effect_resources,effects)) return FALSE;
        for(unsigned e=0;e<emitters;++e)
            if(!emitter(emitters_list[e],effects_list,effects) ||
                !resource_matches(emitters_list[e],0x190u,emitter_resources,emitters)) return FALSE;
    }
    return *(unsigned *)(manager+4u)==count && *(void ***)(manager+0xcu)==instances;
}
static BOOL clock_code_exact(void) {
    for(unsigned i=0;i<4u;++i)
        if(!readable(base+clock_code[i].rva,clock_code[i].size) ||
            memcmp(base+clock_code[i].rva,verified_clock_code[i],clock_code[i].size)) return FALSE;
    return TRUE;
}
/* 6314C0 deliberately skips authored custom-time nodes when instance+88 is
 * zero. Native sector/model animation normally supplies their time through
 * 631860; the contained client's world clock cannot do that. Complete the
 * cosmetic clock here, using the same 635520 -> 635590 modulo/delta math as
 * native particle updates. Do not change +88 (which also controls lifetime),
 * unpause entities, or invoke a model callback that could replace an instance.
 * A node advanced by native model playback since our previous observation is
 * left alone. Newly seen/reordered nodes seed once; gaps reset all observations.
 * Caller just proved the ENTIRE topology, with no intervening native callback. */
static BOOL advance_custom_clocks(uint8_t *manager,uint32_t elapsed_ms) {
    CustomClock next[MAX_NODES]; unsigned n=0u;
    unsigned count=*(unsigned *)(manager+4u);
    uint8_t **instances=*(uint8_t ***)(manager+0xcu);
    /* Bound the integer time before converting to authored frames. On x87,
     * a 50 ms float argument rounds above the extended-precision literal
     * 1.2f used by the previous comparison and incorrectly rejects the cap. */
    topology_stage=70u;
    if(!clock_code_exact()) return FALSE;
    topology_stage=71u;
    if(!elapsed_ms || elapsed_ms>50u) return FALSE;
    float frames=(float)elapsed_ms*0.024f;
    /* Preflight every custom flag/phase before the first clock mutation. */
    for(unsigned i=0;i<count;++i) {
        uint8_t *instance=instances[i]; if(!instance) continue;
        topology_index=i;
        for(unsigned kind=0;kind<2u;++kind) {
            unsigned nodes=*(unsigned *)(instance+(kind?0x14u:8u));
            uint8_t **items=*(uint8_t ***)(instance+(kind?0x10u:4u));
            for(unsigned j=0;j<nodes;++j) {
                uint8_t *node=items[j],*data=*(uint8_t **)(node+(kind?0x120u:0x190u));
                unsigned custom=data[kind?8u:9u];
                float phase=*(float *)(node+0xd8u),duration=*(float *)(node+0xe8u);
                topology_table=(unsigned)((uintptr_t)*(void **)node-(uintptr_t)base);
                topology_stage=72u;
                if(custom>1u) return FALSE;
                if(!custom) continue;
                topology_stage=73u;
                if(n>=MAX_NODES) return FALSE;
                topology_stage=74u;
                if(phase<0.0f || phase>=duration) return FALSE;
                next[n++]=(CustomClock){instance,node,data,phase};
            }
        }
    }
    AdvanceNodeClock advance=(AdvanceNodeClock)(base+0x235520u);
    for(unsigned i=0;i<n;++i) {
        CustomClock *c=&next[i];
        if(i<custom_clock_count && c->instance==custom_clocks[i].instance &&
            c->node==custom_clocks[i].node && c->data==custom_clocks[i].data &&
            c->phase==custom_clocks[i].phase) {
            advance(c->node,frames);
            ((uint8_t *)c->instance)[0x80u]=1u;
            c->phase=*(float *)((uint8_t *)c->node+0xd8u);
            ++custom_advanced;
        }
    }
    memcpy(custom_clocks,next,n*sizeof(next[0])); custom_clock_count=n;
    return TRUE;
}
BOOL SudekiMpLanStoryEffectsAdvance(const SudekiMpLanStoryNativeRoster *roster,
    uint32_t epoch,uint32_t revision,uint32_t sequence,uint32_t host_tick,
    uint32_t receipt_tick,SudekiMpLanStoryReplicaExact exact,void *context) {
    uint32_t now=GetTickCount();
    int32_t age=(int32_t)(now-receipt_tick);
    topology_stage=1u; topology_index=topology_table=0u;
    if(!roster || !exact || !epoch || !revision || !sequence ||
        !SudekiMpLanStoryEffectsWitness(NULL) || original_entered ||
        age < -16 || age>250 || !exact(roster,context) ||
        roster->world!=*(void **)(base+WORLD_GLOBAL) || !topology(current_manager)) {
        return reject(0u);
    }
    if(!clock_state.valid || clock_state.epoch!=epoch || clock_state.revision!=revision ||
        clock_state.world!=roster->world || clock_state.descriptor!=roster->descriptor ||
        clock_state.controller!=roster->controller ||
        (int32_t)(sequence-clock_state.sequence)<0 ||
        (int32_t)(host_tick-clock_state.host_tick)<0) {
        clock_state.valid=TRUE; clock_state.local_tick=now;
        clock_state.epoch=epoch; clock_state.revision=revision;
        clock_state.world=roster->world; clock_state.descriptor=roster->descriptor;
        clock_state.controller=roster->controller;
        clock_state.sequence=sequence; clock_state.host_tick=host_tick;
        custom_clock_count=0u;
        return TRUE; /* one unchanged native zero-delta call, no catch-up */
    }
    uint32_t elapsed=now-clock_state.local_tick;
    clock_state.local_tick=now; clock_state.sequence=sequence; clock_state.host_tick=host_tick;
    if(!elapsed || elapsed>100u) { custom_clock_count=0u; return TRUE; }
    if(elapsed>50u) elapsed=50u;
    if(!SudekiMpLanStoryEffectsWitness(NULL) || !exact(roster,context)) {
        return reject(2u);
    }
    /* Native manager 62FAA0 converts seconds to authored frames at 24 Hz. */
    if(!advance_custom_clocks(current_manager,elapsed)) return reject(0u);
    original_entered=TRUE;
    original_update(current_manager,(float)elapsed*0.001f);
    advanced=TRUE; ++successful;
    if(!SudekiMpLanStoryEffectsWitness(NULL) || !exact(roster,context)) {
        return reject(3u);
    }
    return TRUE;
}
__attribute__((noinline,used,force_align_arg_pointer))
static void handle_update(uint8_t *scene,uint8_t *manager,float delta) {
    LONG depth=InterlockedIncrement(&callbacks);
    DWORD thread=GetCurrentThreadId();
    if(installed && depth==1 && !InterlockedCompareExchange(&stopping,0,0) &&
        (!native_thread || native_thread==thread) && hook_exact() &&
        main_manager(scene,manager)) {
        if(!native_thread) native_thread=thread;
        LARGE_INTEGER started={0},finished={0};
        if(timing_frequency.QuadPart>0) (void)QueryPerformanceCounter(&started);
        active=TRUE; original_entered=FALSE; advanced=FALSE;
        current_scene=scene; current_manager=manager; current_delta=delta;
        if(delta==0.0f && dispatcher) dispatcher(dispatch_context);
        else SudekiMpLanStoryEffectsResetClock();
        if(!original_entered) {
            original_entered=TRUE; original_update(manager,delta);
        }
        active=FALSE; current_scene=current_manager=NULL; current_delta=0.0f;
        if(advanced && started.QuadPart && QueryPerformanceCounter(&finished) &&
            finished.QuadPart>=started.QuadPart) {
            uint64_t us=(uint64_t)(finished.QuadPart-started.QuadPart)*UINT64_C(1000000)/
                (uint64_t)timing_frequency.QuadPart;
            if(us<=UINT32_MAX && dispatch_count<UINT32_MAX) {
                dispatch_us+=us; ++dispatch_count;
                if(us>dispatch_max_us) dispatch_max_us=(uint32_t)us;
            }
        }
        if(successful && successful<=6000u && (successful==1u || successful%600u==0u) && advanced)
            SudekiMpLogFormat("lan_story event=effects_cosmetic applied=%lu refused=%lu mean_us=%lu max_us=%lu custom_nodes=%u custom_advanced=%u gameplay_pause=retained\r\n",
                (unsigned long)successful,(unsigned long)refused,
                dispatch_count?(unsigned long)(dispatch_us/dispatch_count):0ul,
                (unsigned long)dispatch_max_us,custom_clock_count,custom_advanced);
    } else original_update(manager,delta);
    InterlockedDecrement(&callbacks);
}
__attribute__((naked,noinline,used))
static void update_entry(void) {
    __asm__ volatile("pushfl\n\tpushal\n\tpushl 44(%esp)\n\tpushl 44(%esp)\n\t"
        "pushl %esi\n\tcall _handle_update\n\taddl $12,%esp\n\tpopal\n\tpopfl\n\tret $8\n\t");
}
BOOL SudekiMpLanStoryEffectsInstall(HMODULE image,
    SudekiMpLanStoryEffectsDispatch dispatch,void *context) {
    static const uint8_t update_bytes[]={0x83,0xec,0x24,0x53,0x55,0x8b,0x6c,0x24,0x30,0x56,0x57};
    static const uint8_t call_prefix[]={0xd9,0x44,0x24,0x0c,0x51,0x8b,0x4e,0x54,0xd9,0x1c,0x24,0x51};
    static const uint8_t model_prefix[]={0x8b,0x77,0x10,0x85,0xf6,0x74,0x0d,
        0xd9,0x44,0x24,0x10,0x51,0xd9,0x1c,0x24};
    static const uint8_t model_entry[]={0x53,0x55,0x8b,0x6e,0x08,0x57,0x33,0xff};
    static const uint8_t model_return[]={0x5f,0x5d,0x5b,0xc2,0x04,0x00};
    uint8_t *b=(uint8_t *)image;
    if(installed || update_hook.installed || model_clock_hook.installed || !image || !dispatch ||
        InterlockedCompareExchange(&callbacks,0,0) || InterlockedCompareExchange(&clock_callbacks,0,0) ||
        !SudekiMpCheckLoadedExecutable(image) ||
        memcmp(b+PARTICLE_UPDATE,update_bytes,sizeof(update_bytes)) ||
        memcmp(b+PARTICLE_CALL-sizeof(call_prefix),call_prefix,sizeof(call_prefix)) ||
        !call_targets(b+PARTICLE_CALL,b+PARTICLE_UPDATE) ||
        memcmp(b+MODEL_CLOCK_CALL-sizeof(model_prefix),model_prefix,sizeof(model_prefix)) ||
        memcmp(b+MODEL_CLOCK_SET,model_entry,sizeof(model_entry)) ||
        memcmp(b+0x23190du,model_return,sizeof(model_return)) ||
        *(void **)(b+0x2de660u)!=b+0x1f9f70u ||
        !call_targets(b+MODEL_CLOCK_CALL,b+MODEL_CLOCK_SET)) {
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    /* All four bodies have no image relocations. Their callback closure is
     * only native float math and the already-proved node+4 sampler. */
    for(unsigned i=0;i<4u;++i) {
        uint32_t hash=2166136261u;
        if(!readable(b+clock_code[i].rva,clock_code[i].size)) return FALSE;
        for(unsigned j=0;j<clock_code[i].size;++j)
            hash=(hash^b[clock_code[i].rva+j])*16777619u;
        if(hash!=clock_code[i].hash) { SetLastError(ERROR_INVALID_DATA); return FALSE; }
        memcpy(verified_clock_code[i],b+clock_code[i].rva,clock_code[i].size);
    }
    if(!readable(b+0x2e35d0u,8u) || *(double *)(b+0x2e35d0u)!=24.0) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    base=b; dispatcher=dispatch; dispatch_context=context;
    original_update=(ParticleUpdate)(b+PARTICLE_UPDATE);
    original_model_clock=b+MODEL_CLOCK_SET;
    clock_trace_logs=0u; memset(model_observations,0,sizeof(model_observations));
    native_thread=0; successful=refused=0; active=original_entered=advanced=FALSE;
    topology_stage=topology_index=topology_table=refusal_logs=0u;
    last_refusal_stage=last_refusal_table=last_refusal_tick=0u;
    timing_frequency.QuadPart=0; (void)QueryPerformanceFrequency(&timing_frequency);
    dispatch_us=0; dispatch_count=dispatch_max_us=0u;
    custom_advanced=0u;
    current_scene=current_manager=NULL; SudekiMpLanStoryEffectsResetClock();
    InterlockedExchange(&stopping,0);
    if(!SudekiMpInstallRelativeCallHook(&update_hook,b+PARTICLE_CALL,
        b+PARTICLE_UPDATE,update_entry)) return FALSE;
    if(!SudekiMpInstallRelativeCallHook(&model_clock_hook,b+MODEL_CLOCK_CALL,
        b+MODEL_CLOCK_SET,model_clock_entry)) {
        DWORD error=GetLastError();
        if(!SudekiMpLanStoryEffectsUninstall()) return FALSE;
        SetLastError(error); return FALSE;
    }
    installed=TRUE; SetLastError(ERROR_SUCCESS); return TRUE;
}
BOOL SudekiMpLanStoryEffectsUninstall(void) {
    if(!installed && !update_hook.installed && !model_clock_hook.installed) return TRUE;
    InterlockedExchange(&stopping,1);
    if(active || InterlockedCompareExchange(&callbacks,0,0) || InterlockedCompareExchange(&clock_callbacks,0,0) ||
        (native_thread && native_thread!=GetCurrentThreadId())) return pin(ERROR_BUSY);
    if(!SudekiMpRestoreRelativeCallHook(&model_clock_hook) ||
        !SudekiMpRestoreRelativeCallHook(&update_hook) ||
        InterlockedCompareExchange(&callbacks,0,0) || InterlockedCompareExchange(&clock_callbacks,0,0)) return pin(ERROR_BUSY);
    installed=FALSE; dispatcher=NULL; dispatch_context=NULL;
    current_scene=current_manager=NULL; SudekiMpLanStoryEffectsResetClock();
    base=NULL; native_thread=0;
    /* Original image target remains immutable for any late native return.
     * Runtime owns thread quiescence before publishing/removing this CALL. */
    SetLastError(ERROR_SUCCESS); return TRUE;
}
