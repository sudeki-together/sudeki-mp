#include "hooks/lan_story_door_camera.h"
#include "network/lan_story_door_view.h"
#include "engine/log.h"
#include <math.h>
#include <string.h>

enum {
    RVA_REGISTRY=0x409d8cu, RVA_CAMERA_MANAGER=0x3c2f30u, RVA_LOOKUP=0x3e540u,
    RVA_LOOKUP_CALLSITE=0xe7dd9u, RVA_RESOURCE_MANAGER=0x408d10u,
    VT_DOOR=0x2d6430u, VT_DOOR_NODE=0x2d6454u, VT_CDOOR=0x2d637cu, VT_POSITION=0x2cdefcu,
    VT_CAMERA_RECORD=0x2cdd70u, VT_RENDER=0x2dd638u,
    CDOOR=0x3b0u, DOOR_STATE=0x74u, DOOR_FRONT=0x78u, DOOR_BACK=0x84u, DOOR_TRAVERSE=0x9cu,
    RELEASE_MARGIN_MS=1500u, DOOR_SEQUENCE_BASE=0x8000u
};
static uint8_t *base;
typedef struct Engaged { void *door; unsigned camera; uint16_t sequence; uint32_t engaged_at,last_state_at; } Engaged;
static Engaged engaged[4];
static uint16_t next_sequence;
static unsigned logs;

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && a<=UINTPTR_MAX-n && VirtualQuery(p,&m,sizeof(m))==sizeof(m) &&
        m.State==MEM_COMMIT && !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
        a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
/* Exact-image guards. The lookup entry carries one relocated operand (the
 * resource manager global) which is compared by value, never patched. */
static BOOL lookup_exact(uint8_t *b) {
    static const uint8_t head[4]={0x83,0xec,0x08,0x53};
    if(!readable(b+RVA_LOOKUP,12u) || memcmp(b+RVA_LOOKUP,head,4u) || b[RVA_LOOKUP+4u]!=0x8bu || b[RVA_LOOKUP+5u]!=0x1du ||
        *(uint32_t *)(b+RVA_LOOKUP+6u)!=(uint32_t)(uintptr_t)(b+RVA_RESOURCE_MANAGER) ||
        b[RVA_LOOKUP+10u]!=0x55u || b[RVA_LOOKUP+11u]!=0x33u) return FALSE;
    /* SetStaticCameraPosition's own call: INC [ECX]; CALL lookup. */
    if(!readable(b+RVA_LOOKUP_CALLSITE,7u) || b[RVA_LOOKUP_CALLSITE]!=0xffu || b[RVA_LOOKUP_CALLSITE+1u]!=0x01u ||
        b[RVA_LOOKUP_CALLSITE+2u]!=0xe8u) return FALSE;
    int32_t rel; memcpy(&rel,b+RVA_LOOKUP_CALLSITE+3u,4u);
    return b+RVA_LOOKUP_CALLSITE+7u+rel==b+RVA_LOOKUP;
}
BOOL SudekiMpLanStoryDoorCameraInstall(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    if(!b || base) { SetLastError(ERROR_INVALID_STATE); return FALSE; }
    if(!lookup_exact(b) || !readable(b+RVA_REGISTRY,4u) || !readable(b+RVA_CAMERA_MANAGER,4u)) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    base=b; memset(engaged,0,sizeof(engaged)); next_sequence=0;
    SudekiMpLogWrite("story_door_camera event=installed lookup=3e540 kind=3\r\n");
    return TRUE;
}
void SudekiMpLanStoryDoorCameraUninstall(void) { base=NULL; memset(engaged,0,sizeof(engaged)); }

/* The game's static-camera lookup, exactly as RVA 0x4E7D80 calls it: EAX=-1
 * (the current zone's resource table), ResourceName by value with a NULL
 * reference pointer (no refcount is taken or released), a zero flag word.
 * ESP is saved and restored around the call so the callee's own stack
 * convention cannot unbalance us. Returns the record (handle-4) or NULL. */
static uint8_t *lookup_record(uint32_t kind,uint32_t id) {
    if(!base || id==0x7ffffu || !id) return NULL;
    uint32_t encoded=(kind&0xffffffbau)|0x3au,result=0;
    void *function=base+RVA_LOOKUP;
    static unsigned lookup_logs;
    if(lookup_logs<8u) { ++lookup_logs; SudekiMpLogFormat("story_door_camera event=lookup kind=%08lx id=%08lx\r\n",(unsigned long)encoded,(unsigned long)id); }
    /* EAX is the native type selector (-1 = current zone table) and is set
     * last, so no input may live in EAX: the output is early-clobber and the
     * call target is held in ECX (a pushed-stack memory operand would be
     * mis-addressed). ESP is kept in EBX across the call. */
    __asm__ volatile(
        "movl %%esp, %%ebx\n\t"
        "pushl $0\n\t"
        "pushl $0\n\t"
        "pushl %[id]\n\t"
        "pushl %[kind]\n\t"
        "movl $-1, %%eax\n\t"
        "call *%%ecx\n\t"
        "movl %%ebx, %%esp"
        : "=&a"(result)
        : [id]"S"(id),[kind]"D"(encoded),[fn]"c"(function)
        : "ebx","edx","memory","cc");
    if(!result || result<8u) return NULL;
    uint8_t *record=(uint8_t *)(uintptr_t)(result-4u);
    if(!readable(record,0x30u) || *(void **)record!=base+VT_CAMERA_RECORD || *(uint32_t *)(record+0x24u)!=id) return NULL;
    return record;
}
static BOOL camera_from_name(const uint8_t *cdoor,unsigned offset,SudekiMpStoryDoorCamera *out) {
    memset(out,0,sizeof(*out));
    uint32_t kind=*(const uint32_t *)(cdoor+offset),id=*(const uint32_t *)(cdoor+offset+4u);
    uint8_t *record=lookup_record(kind,id);
    if(!record) return FALSE;
    memcpy(out->position,record+0x08u,sizeof(out->position));
    memcpy(out->direction,record+0x14u,sizeof(out->direction));
    out->fixed_direction=record[0x2cu]!=0;
    out->valid=isfinite(out->position[0]) && isfinite(out->position[1]) && isfinite(out->position[2]) &&
        isfinite(out->direction[0]) && isfinite(out->direction[1]) && isfinite(out->direction[2]);
    return out->valid;
}
static BOOL projection_now(float out[3]) {
    uint8_t *manager=*(uint8_t **)(base+RVA_CAMERA_MANAGER),*camera,*render;
    if(!readable(manager,0x60u) || !readable(camera=*(uint8_t **)(manager+0x20u),0x40u) ||
        !readable(render=*(uint8_t **)(camera+0x34u),0xe0u) || *(void **)render!=base+VT_RENDER) return FALSE;
    memcpy(out,render+0xd0u,12u);
    return isfinite(out[0]) && isfinite(out[1]) && isfinite(out[2]) && out[0]>0.1f && out[0]<3.0f && out[1]>0 && out[2]>out[1];
}
static BOOL actor_position(const void *actor,float out[3]) {
    const uint8_t *a=actor,*position;
    if(!readable(a,0x48u) || !readable(position=*(const uint8_t *const *)(a+0x44u),0x60u) ||
        *(void *const *)position!=base+VT_POSITION || *(const void *const *)(position+0x10u)!=actor) return FALSE;
    memcpy(out,position+0x18u,12u);
    return isfinite(out[0]) && isfinite(out[1]) && isfinite(out[2]);
}
static BOOL door_geometry(const uint8_t *door,float position[3],float forward[3],float *traverse,uint32_t *state) {
    const uint8_t *cpos;
    if(!readable(door,CDOOR+0x140u) || *(void *const *)door!=base+VT_DOOR || *(void *const *)(door+8u)!=base+VT_DOOR_NODE ||
        *(void *const *)(door+CDOOR)!=base+VT_CDOOR || !readable(cpos=*(const uint8_t *const *)(door+0x44u),0x60u) ||
        cpos!=door+0x150u || *(void *const *)cpos!=base+VT_POSITION || *(const void *const *)(cpos+0x10u)!=door) return FALSE;
    memcpy(position,cpos+0x18u,12u); memcpy(forward,cpos+0x50u,12u);
    *traverse=*(const float *)(door+CDOOR+DOOR_TRAVERSE); *state=*(const uint32_t *)(door+CDOOR+DOOR_STATE);
    return isfinite(*traverse) && *traverse>0 && *traverse<=50.0f && *state<=0x10u;
}
BOOL SudekiMpLanStoryDoorCameraView(const SudekiMpLanStoryNativeRoster *roster,unsigned character,
    uint32_t now,SudekiMpStoryCastView *view) {
    if(!base || !roster || !view || character>=4u || view->kind || !(roster->available_mask&(1u<<character)) ||
        roster->leader_character==character) return FALSE;
    Engaged *e=&engaged[character];
    float actor[3];
    if(!actor_position(roster->actors[character],actor)) { memset(e,0,sizeof(*e)); return FALSE; }
    uint8_t *manager=*(uint8_t **)(base+RVA_REGISTRY);
    if(!readable(manager,0x40u)) return FALSE;
    unsigned count=*(unsigned *)(manager+0x34u); void **entries=*(void ***)(manager+0x3cu);
    if(!count || count>8192u || !readable(entries,count*sizeof(void *))) return FALSE;
    /* Prefer the door this character is already engaged with; otherwise the
     * nearest open door whose traverse zone contains the character. */
    uint8_t *best=NULL; float best_distance=1e9f,best_position[3],best_forward[3],best_traverse=0; uint32_t best_state=0;
    for(unsigned i=0;i<count;++i) {
        uint8_t *door=entries[i]; float position[3],forward[3],traverse; uint32_t state;
        if(!door || !door_geometry(door,position,forward,&traverse,&state)) continue;
        BOOL current=e->door==door;
        if(!state && !current) continue;
        float dx=actor[0]-position[0],dz=actor[2]-position[2],distance=sqrtf(dx*dx+dz*dz);
        if(!isfinite(distance)) continue;
        if(current) { best=door; best_distance=distance; memcpy(best_position,position,12u); memcpy(best_forward,forward,12u); best_traverse=traverse; best_state=state; break; }
        if(distance<=traverse+0.5f && distance<best_distance) {
            best=door; best_distance=distance; memcpy(best_position,position,12u); memcpy(best_forward,forward,12u); best_traverse=traverse; best_state=state;
        }
    }
    if(!best) { if(e->door) { if(logs<64u) { ++logs; SudekiMpLogFormat("story_door_camera event=released character=%u reason=door_gone\r\n",character); } memset(e,0,sizeof(*e)); } return FALSE; }
    if(e->door==best) {
        if(best_state) e->last_state_at=now;
        if(best_distance>best_traverse+2.0f || (!best_state && now-e->last_state_at>RELEASE_MARGIN_MS)) {
            if(logs<64u) { ++logs; SudekiMpLogFormat("story_door_camera event=released character=%u distance=%.2f state=%lu\r\n",character,(double)best_distance,(unsigned long)best_state); }
            memset(e,0,sizeof(*e)); return FALSE;
        }
    }
    SudekiMpStoryDoorCamera cameras[2];
    BOOL front=camera_from_name(best+CDOOR,DOOR_FRONT,&cameras[0]),back=camera_from_name(best+CDOOR,DOOR_BACK,&cameras[1]);
    if(!front && !back) return FALSE;
    unsigned chosen;
    if(e->door!=best) {
        if(!SudekiMpStoryDoorViewSelect(best_position,best_forward,actor,best_traverse,0.5f,cameras,&chosen)) return FALSE;
        if(!++next_sequence) ++next_sequence;
        e->door=best; e->camera=chosen; e->sequence=(uint16_t)(DOOR_SEQUENCE_BASE+(next_sequence&0x7fffu)); e->engaged_at=e->last_state_at=now;
        if(logs<64u) { ++logs; SudekiMpLogFormat("story_door_camera event=engaged character=%u door=%p camera=%u state=%lu distance=%.2f\r\n",
            character,(void *)best,chosen,(unsigned long)best_state,(double)best_distance); }
    } else chosen=e->camera;
    if(!cameras[chosen].valid) return FALSE;
    float projection[3];
    if(!projection_now(projection)) return FALSE;
    SudekiMpLanStoryView built;
    if(!SudekiMpStoryDoorViewBuild(&cameras[chosen],actor,projection,&built)) return FALSE;
    view->kind=3u; view->cast_sequence=e->sequence; view->view=built;
    view->view.camera_serial=(uint32_t)view->cast_sequence*4u+3u;
    return TRUE;
}
