#include "hooks/lan_story_client_camera.h"
#include "hooks/lan_story_client_camera_height.h"
#include "hooks/lan_story_spawn_trace.h"
#include "engine/log.h"
#include <limits.h>
#include <math.h>
#include <string.h>

enum {
    RVA_CAMERA_MANAGER=0x409d7cu, RVA_SPEED=0x408da0u, RVA_UI=0x408d8cu,
    RVA_NODE_UPDATE=0xe7660u, RVA_INPUT_EVENT=0xe85f0u, RVA_SET_STATE=0x37530u, RVA_CAN_SET_STATE=0x37c30u,
    RVA_NODE_VTABLE_UPDATE_SLOT=0x2cce70u,
    VT_MANAGER=0x2c7b80u, VT_CAMERA=0x2cce5cu, VT_NODE=0x2cce6cu, VT_RENDER=0x2dd638u,
    VT_EXPLORATION_STATE=0x2d9854u, VT_EXPLORATION_DATA=0x2ca694u,
    VT_INTERPOLATION_STATE=0x2db9a4u, VT_INTERPOLATION_DATA=0x2db998u,
    VT_OFFSET_TARGET=0x2d436cu, VT_GAME_OBJECT_TARGET=0x2d42ccu,
    RVA_OFFSET_TARGET_REFRESH=0x19a0d0u, RVA_GAME_OBJECT_TARGET_REFRESH=0x19a290u,
    RVA_GAME_OBJECT_TARGET_POSITION=0x19a300u, VT_POSITION=0x2cdefcu,
    /* H1b house camera: CCameraManager::SetStaticCameraPosition (thiscall,
     * camera name, ResourceName&, transition, collide, max interpolation;
     * RET 0x14) and the Aimed state singleton it ends in (vtable 0x2D6DFC,
     * id getter slot 0x14D730). CONFIRMED_STATIC + live host poll. */
    RVA_SET_STATIC=0x38500u, RVA_AIMED_STATE=0x3d4b10u, VT_AIMED_STATE=0x2d6dfcu,
    RVA_AIMED_ID=0x14d730u, RESOURCE_KIND_STATIC_CAMERA=0x3au
};
typedef void (__attribute__((fastcall)) *TargetRefresh)(void *target);
typedef void (__attribute__((thiscall)) *NodeUpdate)(void *node,const float *timing);
typedef void (__attribute__((thiscall)) *InputEvent)(void *camera,const void *event);
typedef void (__attribute__((thiscall)) *SetState)(void *manager,const char *camera,const char *state,float a,BOOL flag,float b);
typedef BOOL (__attribute__((thiscall)) *CanSetState)(void *manager,const char *camera,const char *state);
typedef void (__attribute__((thiscall)) *SetStatic)(void *manager,const char *camera,void *position,float transition,BOOL collide,float max_interp);
/* While TRUE (this client settled in a house whose cluster script sets a
 * static camera), the native Aimed state is the camera's ordinary state. */
static BOOL house_static;
typedef struct CameraInputEvent { uint32_t action,reserved_04,reserved_08; float magnitude; uint32_t reserved_10; uint16_t owner,reserved_16; } CameraInputEvent;

static uint8_t *base;
static DWORD native_thread;
static BOOL lease_exact(const SudekiMpLanArenaOwnerViewLease *o,uint8_t **manager_out);
static BOOL height_scope_exact(const SudekiMpLanStoryClientCameraHeightScope *s);
static SudekiMpLanArenaOwnerViewLease height_owner;
static void *height_actor,*height_position;
static BOOL tick_active;
static uint32_t state_requested_at;
static unsigned logs,ticks,state_requests;

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && a<=UINTPTR_MAX-n && VirtualQuery(p,&m,sizeof(m))==sizeof(m) &&
        m.State==MEM_COMMIT && !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
        a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL bytes(const uint8_t *b,unsigned rva,const char *expected,unsigned n) {
    return readable(b+rva,n) && !memcmp(b+rva,expected,n);
}
/* Exact-image guards for every native entry this module calls. The camera
 * update entry carries one relocated operand (the speed global), compared
 * by value. The node vtable slot must still point at that update. */
static BOOL image_exact(uint8_t *b) {
    return bytes(b,RVA_NODE_UPDATE,"\x55\x8b\xec\x83\xe4\xc0\xa1",7u) &&
        readable(b+RVA_NODE_UPDATE+7u,5u) && *(uint32_t *)(b+RVA_NODE_UPDATE+7u)==(uint32_t)(uintptr_t)(b+RVA_SPEED) &&
        b[RVA_NODE_UPDATE+11u]==0x81u &&
        bytes(b,RVA_INPUT_EVENT,"\x51\x56\x8b\xf1\x8b\x4e\x38\x33\xc0\x85\xc9\x74",12u) &&
        bytes(b,RVA_SET_STATE,"\xd9\x44\x24\x14\x8b\x44\x24\x10\x56\x51\xd9\x1c",12u) &&
        bytes(b,RVA_CAN_SET_STATE,"\x53\x55\x8b\x6c\x24\x0c\x56\x57\x8b\xd9\x85\xed",12u) &&
        readable(b+RVA_NODE_VTABLE_UPDATE_SLOT,4u) && *(void **)(b+RVA_NODE_VTABLE_UPDATE_SLOT)==b+RVA_NODE_UPDATE &&
        readable(b+RVA_CAMERA_MANAGER,4u) && readable(b+RVA_SPEED,4u) && readable(b+RVA_UI,4u);
}
static BOOL house_image_exact(uint8_t *b) {
    return bytes(b,RVA_SET_STATIC,"\x83\xec\x08\x53\x55\x8b\x6c\x24\x14\x56\x8b\x74\x24\x1c\x8b\x06\x83\xe0\xba\x83\xc8\x3a",22u) &&
        readable(b+VT_AIMED_STATE,0x18u) && *(void **)(b+VT_AIMED_STATE+0x10u)==b+RVA_AIMED_ID &&
        readable(b+RVA_AIMED_STATE,4u);
}
/* H1b Kamo talk (owner 2026-10-17): the native speech camera. CSpeech's
 * per-line SetCamera 0x415AC0 (gated by CSpeech+0x475) aims the stateless
 * camera named "SpeechCamera" (RVA 0x2C5690) and selects it with
 * CCameraManager::SetRenderCamera 0x436FB0 (thiscall(manager, name), RET 4;
 * writes manager+0x20 and the renderer slot *(*(0x408D58)+0x40)+0x7C at
 * 0x4370D8). Vanilla returns with SwitchActiveCamera("default") when the
 * shopkeeper's script ends. CONFIRMED_STATIC; the client is unhooked there. */
enum { RVA_SET_RENDER_CAMERA=0x36fb0u, RVA_SPEECH_SET_CAMERA=0x15ac0u, RVA_SPEECH_CAMERA_NAME=0x2c5690u,
    RVA_SCENE_MANAGER=0x408d58u, CAMERA_SLOTS=10u };
typedef uint8_t (__attribute__((thiscall)) *SetRenderCamera)(void *manager,const char *name);
static BOOL speech_ready;
static BOOL speech_image_exact(uint8_t *b) {
    return bytes(b,RVA_SET_RENDER_CAMERA,"\x55\x8b\xec\x83\xe4\xf8\x83\xec\x14\x53\x8b\xd9\x8b\x43\x20\x56\x57"
            "\xc6\x44\x24\x13\x00\x85\xc0\x74\x0a\x8a\x88\x05\x01\x00\x00",32u) &&
        bytes(b,0x370d8u,"\x8b\x4c\x24\x1c\x8b\x41\x40\x8b\x4a\x34\x89\x48\x7c\x8a\x4c\x24\x13\x5f\x89\x53\x20",21u) &&
        bytes(b,0x370fau,"\xc2\x04\x00",3u) &&
        bytes(b,RVA_SPEECH_SET_CAMERA,"\x55\x8b\xec\x83\xe4\xf8\x83\xec\x6c\x53\x8b\xd9\x80\xbb\x75\x04\x00\x00\x00"
            "\x56\x57\x0f\x84\x42\x05\x00\x00",27u) &&
        bytes(b,0x15ffau,"\x68",1u) && readable(b+0x15ffbu,4u) &&
        *(uint32_t *)(b+0x15ffbu)==(uint32_t)(uintptr_t)(b+RVA_SPEECH_CAMERA_NAME) &&
        bytes(b,0x16007u,"\xe8\xa4\x0f\x02\x00",5u) &&
        bytes(b,RVA_SPEECH_CAMERA_NAME,"SpeechCamera",13u) &&
        bytes(b,0x36e83u,"\x8d\x7d\x24",3u) && readable(b+RVA_SCENE_MANAGER,4u);
}
/* The one camera slot (of the manager's ten at +0x24) with this name. */
static uint8_t *camera_named(const uint8_t *manager,const char *name) {
    uint8_t *found=NULL; unsigned matches=0; size_t n=strlen(name)+1u;
    for(unsigned i=0;i<CAMERA_SLOTS;++i) {
        uint8_t *c=*(uint8_t *const *)(manager+0x24u+i*4u);
        if(!c) continue;
        if(!readable(c,0x10cu)) return NULL;
        if(!memcmp(c+0x4cu,name,n)) { found=c; ++matches; }
    }
    return matches==1u && *(void **)found==base+VT_CAMERA && readable(*(void **)(found+0x34u),0xe0u) &&
        **(void ***)(found+0x34u)==base+VT_RENDER?found:NULL;
}
int SudekiMpLanStoryClientCameraSpeechObserve(void) {
    if(!base || !speech_ready) return SUDEKIMP_SPEECH_CAMERA_UNKNOWN;
    uint8_t *manager=readable(base+RVA_CAMERA_MANAGER,4u)?*(uint8_t **)(base+RVA_CAMERA_MANAGER):NULL;
    if(!readable(manager,0x60u) || *(void **)manager!=base+VT_MANAGER) return SUDEKIMP_SPEECH_CAMERA_UNKNOWN;
    uint8_t *def=camera_named(manager,"default"),*speech=camera_named(manager,"SpeechCamera");
    uint8_t *scene=*(uint8_t **)(base+RVA_SCENE_MANAGER),*render=readable(scene,0x44u)?*(uint8_t **)(scene+0x40u):NULL;
    if(!def || !speech || !readable(render,0x80u)) return SUDEKIMP_SPEECH_CAMERA_UNKNOWN;
    void *current=*(void **)(manager+0x20u),*slot=*(void **)(render+0x7cu);
    if(current==def && slot==*(void **)(def+0x34u)) return SUDEKIMP_SPEECH_CAMERA_DEFAULT;
    if(current==speech && slot==*(void **)(speech+0x34u) && !*(void **)(speech+0x3cu)) return SUDEKIMP_SPEECH_CAMERA_SPEECH;
    return SUDEKIMP_SPEECH_CAMERA_UNKNOWN;
}
BOOL SudekiMpLanStoryClientCameraSpeechReady(void) {
    return base && speech_ready && !tick_active && SudekiMpLanStoryClientCameraSpeechObserve()==SUDEKIMP_SPEECH_CAMERA_DEFAULT;
}
BOOL SudekiMpLanStoryClientCameraSpeechReturn(void) {
    static unsigned return_logs;
    if(!base || !speech_ready || tick_active || (native_thread && native_thread!=GetCurrentThreadId())) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    int seen=SudekiMpLanStoryClientCameraSpeechObserve();
    if(seen==SUDEKIMP_SPEECH_CAMERA_DEFAULT) return TRUE;
    if(seen!=SUDEKIMP_SPEECH_CAMERA_SPEECH) { SetLastError(ERROR_BUSY); return FALSE; }
    ((SetRenderCamera)(base+RVA_SET_RENDER_CAMERA))(*(uint8_t **)(base+RVA_CAMERA_MANAGER),"default");
    BOOL ok=SudekiMpLanStoryClientCameraSpeechObserve()==SUDEKIMP_SPEECH_CAMERA_DEFAULT;
    if(return_logs<16u) { ++return_logs; SudekiMpLogFormat("story_client_camera event=speech_return result=%u\r\n",(unsigned)ok); }
    if(!ok) SetLastError(ERROR_BUSY);
    return ok;
}
static BOOL aimed_state(const uint8_t *camera) {
    const uint8_t *state=*(const uint8_t *const *)(camera+0x3cu);
    return state && state==*(const uint8_t *const *)(base+RVA_AIMED_STATE) && readable(state,4u) &&
        *(void *const *)state==base+VT_AIMED_STATE && !*(const void *const *)(camera+0x40u);
}
BOOL SudekiMpLanStoryClientCameraInstall(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    if(!b || base) { SetLastError(ERROR_INVALID_STATE); return FALSE; }
    if(!image_exact(b)) { SetLastError(ERROR_INVALID_DATA); return FALSE; }
    if(!SudekiMpLanStoryClientCameraHeightInstall(image,height_scope_exact)) return FALSE;
    base=b; native_thread=0; state_requested_at=0; logs=ticks=state_requests=0; house_static=FALSE;
    speech_ready=speech_image_exact(b);
    if(!speech_ready) SudekiMpLogWrite("story_client_camera event=speech_camera_unavailable policy=house_view_kept\r\n");
    SudekiMpLogWrite("story_client_camera event=installed update=e7660 input=e85f0 state=37530 policy=native_exploration_tick_under_pause small_height_steps=owned_exploration_only\r\n");
    return TRUE;
}
BOOL SudekiMpLanStoryClientCameraUninstall(void) {
    if(tick_active || (native_thread && native_thread!=GetCurrentThreadId()) ||
        SudekiMpLanStoryClientCameraSpeechObserve()==SUDEKIMP_SPEECH_CAMERA_SPEECH) {
        SetLastError(ERROR_BUSY);return FALSE;
    }
    /* A failed native restoration still owns this callback and all of its
     * leases. The runtime must retain the module and retry on the game thread. */
    if(!SudekiMpLanStoryClientCameraHeightUninstall()) return FALSE;
    base=NULL;native_thread=0;memset(&height_owner,0,sizeof(height_owner));
    height_actor=height_position=NULL;return TRUE;
}
BOOL SudekiMpLanStoryClientCameraReseat(const SudekiMpLanArenaOwnerViewLease *owner) {
    uint8_t *manager;
    if(!base || !lease_exact(owner,&manager)) return FALSE;
    const char *name=(const char *)((uint8_t *)owner->camera+0x4cu);
    if(!readable(name,8u) || _stricmp(name,"default")) return FALSE;
    void *state=*(void **)((uint8_t *)owner->camera+0x3cu);
    BOOL can=((CanSetState)(base+RVA_CAN_SET_STATE))(manager,name,"Exploration");
    static unsigned reseat_logs;
    if(reseat_logs<16u) { ++reseat_logs;
        SudekiMpLogFormat("story_client_camera event=reseat can=%u state_vt=%06lx\r\n",can,
            (unsigned long)(readable(state,4u)?(uintptr_t)*(void **)state-(uintptr_t)base:0u)); }
    if(!can) return FALSE;
    ((SetState)(base+RVA_SET_STATE))(manager,name,"Exploration",0.0f,FALSE,99999.9f);
    return TRUE;
}

static BOOL lease_exact(const SudekiMpLanArenaOwnerViewLease *o,uint8_t **manager_out) {
    if(!o || !o->valid || !readable(o->camera,0x10cu) || *(void **)o->camera!=base+VT_CAMERA ||
        *(void **)((uint8_t *)o->camera+8u)!=base+VT_NODE ||
        !readable(o->render_state,0xe0u) || *(void **)o->render_state!=base+VT_RENDER ||
        *(void **)((uint8_t *)o->camera+0x34u)!=o->render_state) return FALSE;
    uint8_t *manager=*(uint8_t **)(base+RVA_CAMERA_MANAGER);
    if(!readable(manager,0x60u) || *(void **)manager!=base+VT_MANAGER || *(void **)(manager+0x20u)!=o->camera) return FALSE;
    if(manager_out) *manager_out=manager;
    return TRUE;
}
static BOOL ordinary_state(uint8_t *camera,BOOL *exploration_out) {
    const uint8_t *state=*(const uint8_t **)(camera+0x3cu),*data=*(const uint8_t **)(camera+0x40u);
    if(exploration_out) *exploration_out=FALSE;
    /* A house's static camera ends in Aimed with no state data. */
    if(house_static && aimed_state(camera)) return TRUE;
    if(!readable(state,4u) || !readable(data,0xcu)) return FALSE;
    BOOL exploration=*(void **)state==base+VT_EXPLORATION_STATE &&
        *(void **)data==base+VT_EXPLORATION_DATA;
    BOOL interpolating=*(void **)state==base+VT_INTERPOLATION_STATE &&
        *(void **)data==base+VT_INTERPOLATION_DATA;
    if(exploration_out) *exploration_out=exploration;
    return exploration || interpolating;
}
typedef struct CameraTargets { uint8_t *inner,*outer; } CameraTargets;
static BOOL targets_exact(uint8_t *camera,CameraTargets *out) {
    uint8_t *inner=*(uint8_t **)(camera+0xb8u),*outer=*(uint8_t **)(camera+0xb4u);
    if(!readable(inner,0x34u) || *(void **)inner!=base+VT_GAME_OBJECT_TARGET ||
        !*(uint32_t *)(inner+4u) ||
        !readable(base+VT_GAME_OBJECT_TARGET,0x14u) ||
        *(void **)(base+VT_GAME_OBJECT_TARGET+4u)!=base+RVA_GAME_OBJECT_TARGET_REFRESH ||
        *(void **)(base+VT_GAME_OBJECT_TARGET+0x10u)!=base+RVA_GAME_OBJECT_TARGET_POSITION) return FALSE;
    if(outer!=inner && (!readable(outer,0xd0u) || *(void **)outer!=base+VT_OFFSET_TARGET ||
        !*(uint32_t *)(outer+4u) || !readable(base+VT_OFFSET_TARGET,8u) ||
        *(void **)(base+VT_OFFSET_TARGET+4u)!=base+RVA_OFFSET_TARGET_REFRESH ||
        *(void **)(outer+0x20u)!=inner)) return FALSE;
    /* GameObjectTarget's native position getter dereferences its actor+44.
     * Match the same party/CPosition ownership accepted by local control;
     * readability alone cannot authorize that virtual call. The caller's
     * local-view proof additionally binds this actor to its selected seat. */
    uint8_t *actor=*(uint8_t **)(inner+0x20u),*position;
    if(!readable(actor,0x48u)) return FALSE;
    static const unsigned actor_vt[4]={0x2d5a88u,0x2d66fcu,0x2d5010u,0x2d555cu};
    unsigned c=0;while(c<4u && *(void **)actor!=base+actor_vt[c]) ++c;
    if(c==4u || !readable(position=*(uint8_t **)(actor+0x44u),0x98u) ||
        *(void **)position!=base+VT_POSITION || *(void **)(position+0x10u)!=actor ||
        (*(uintptr_t *)(position+0x94u) && *(uintptr_t *)(position+0x94u)!=4u)) return FALSE;
    for(unsigned i=0;i<3u;++i) {
        float p=*(float *)(position+0x18u+i*4u);
        if(!isfinite(p) || fabsf(p)>1000000.0f) return FALSE;
    }
    if(out) *out=(CameraTargets){inner,outer};
    return TRUE;
}
static BOOL height_scope_exact(const SudekiMpLanStoryClientCameraHeightScope *s) {
    CameraTargets targets;BOOL exploration=FALSE;
    if(!base || !tick_active || !native_thread || native_thread!=GetCurrentThreadId() ||
        !s || s->camera!=height_owner.camera || !lease_exact(&height_owner,NULL) ||
        !ordinary_state(s->camera,&exploration) || !exploration ||
        !targets_exact(s->camera,&targets) || targets.inner!=s->inner || targets.outer!=s->outer)
        return FALSE;
    uint8_t *camera=s->camera;
    return *(void **)(camera+0x3cu)==s->state && *(void **)(camera+0x40u)==s->data &&
        *(void **)(targets.inner+0x20u)==height_actor &&
        *(void **)((uint8_t *)height_actor+0x44u)==height_position;
}
BOOL SudekiMpLanStoryClientCameraContinuityReady(const SudekiMpLanArenaOwnerViewLease *owner) {
    BOOL exploration=FALSE;
    return base && native_thread && native_thread==GetCurrentThreadId() &&
        lease_exact(owner,NULL) && ordinary_state(owner->camera,&exploration) &&
        (exploration || (house_static && aimed_state(owner->camera))) &&
        targets_exact(owner->camera,NULL);
}
/* H1b: this client settled in a house whose cluster entry script calls
 * SetCameraStaticPosition('default', <camera>, 0, false, 0) (the client holds
 * cluster scripts, so it never runs). Make that same native call with the
 * house's own authored camera; the manager's transition then ends in Aimed
 * (fixed eye, aimed at the character every update), as on the host. */
typedef struct StaticCall { uint8_t *manager; const char *camera; BOOL called; } StaticCall;
static void static_with_name(void *resource,void *context) {
    StaticCall *c=context;
    ((SetStatic)(base+RVA_SET_STATIC))(c->manager,c->camera,resource,0.0f,FALSE,0.0f);
    c->called=TRUE;
}
BOOL SudekiMpLanStoryClientCameraHouseStatic(const SudekiMpLanArenaOwnerViewLease *owner,const char *position) {
    uint8_t *manager;
    if(!base || tick_active || !position || !position[0] || !lease_exact(owner,&manager) || !house_image_exact(base)) return FALSE;
    const char *name=(const char *)((uint8_t *)owner->camera+0x4cu);
    if(!readable(name,8u) || _stricmp(name,"default")) return FALSE;
    StaticCall call={manager,name,FALSE};
    if(!SudekiMpLanStoryWithResourceName(position,RESOURCE_KIND_STATIC_CAMERA,static_with_name,&call) || !call.called) return FALSE;
    house_static=TRUE; state_requests=0; state_requested_at=0;
    const uint8_t *state=*(const uint8_t **)((uint8_t *)owner->camera+0x3cu);
    SudekiMpLogFormat("story_client_camera event=house_static camera=%s state_vt=%06lx aimed=%u\r\n",position,
        (unsigned long)(readable(state,4u)?(uintptr_t)*(void **)state-(uintptr_t)base:0u),(unsigned)aimed_state(owner->camera));
    return TRUE;
}
/* Leaving the house: the vanilla exit asks for Exploration (0, false, 0). */
BOOL SudekiMpLanStoryClientCameraHouseRelease(const SudekiMpLanArenaOwnerViewLease *owner) {
    uint8_t *manager;
    BOOL was=house_static;
    house_static=FALSE; state_requests=0; state_requested_at=0;
    if(!base || !lease_exact(owner,&manager)) return FALSE;
    const char *name=(const char *)((uint8_t *)owner->camera+0x4cu);
    if(!readable(name,8u) || _stricmp(name,"default") ||
        !((CanSetState)(base+RVA_CAN_SET_STATE))(manager,name,"Exploration")) return FALSE;
    ((SetState)(base+RVA_SET_STATE))(manager,name,"Exploration",0.0f,FALSE,0.0f);
    if(was) SudekiMpLogWrite("story_client_camera event=house_released state=exploration\r\n");
    return TRUE;
}
BOOL SudekiMpLanStoryClientCameraTick(const SudekiMpLanArenaOwnerViewLease *owner,
    float dt_seconds,float look_x,float look_y) {
    if(!base || tick_active) return FALSE;
    DWORD thread=GetCurrentThreadId();
    if(native_thread && native_thread!=thread) return FALSE;
    uint8_t *manager;
    if(!lease_exact(owner,&manager) || !isfinite(dt_seconds) || !isfinite(look_x) || !isfinite(look_y)) return FALSE;
    uint8_t *camera=owner->camera;
    void *state=*(void **)(camera+0x3cu);
    BOOL exploration=FALSE;
    /* A camera interpolation state is the manager's transition toward the
     * requested state; it only completes when the camera update runs, so it
     * is ticked like Exploration rather than re-requested. */
    uint32_t now=GetTickCount();
    if(!ordinary_state(camera,&exploration)) {
        /* Ask the manager for Exploration the way the retail bootstrap does
         * (RVA 0x4CF831: 0.0, false, 99999.9). At most a few attempts, well
         * apart; the state object is the manager's, never ours. */
        if(state_requests<3u && (!state_requested_at || now-state_requested_at>=5000u)) {
            state_requested_at=now;
            const char *name=(const char *)(camera+0x4cu);
            if(!readable(name,8u) || _stricmp(name,"default")) return FALSE;
            if(((CanSetState)(base+RVA_CAN_SET_STATE))(manager,name,"Exploration")) {
                ((SetState)(base+RVA_SET_STATE))(manager,name,"Exploration",0.0f,FALSE,99999.9f);
                ++state_requests;
                SudekiMpLogFormat("story_client_camera event=state_requested attempt=%u previous_vt=%06lx\r\n",
                    state_requests,(unsigned long)(readable(state,4u)?(uintptr_t)*(void **)state-(uintptr_t)base:0u));
            } else if(logs<16u) { ++logs; SudekiMpLogWrite("story_client_camera event=state_refused reason=can_set_false\r\n"); }
        }
        return FALSE;
    }
    /* Preflight the entire native target chain and clock before any input or
     * target refresh. A refused outer target must not leave inner/input writes. */
    CameraTargets targets;
    uint8_t *speed=*(uint8_t **)(base+RVA_SPEED);
    if(!targets_exact(camera,&targets) || !readable(speed,0x2cu)) return FALSE;
    native_thread=thread;
    if(dt_seconds<=0.0f) return FALSE;
    if(dt_seconds>0.1f) dt_seconds=0.1f;
    tick_active=TRUE;
    height_owner=*owner;
    height_actor=*(void **)(targets.inner+0x20u);
    height_position=*(void **)((uint8_t *)height_actor+0x44u);
    /* Right stick as the native camera actions, when the camera accepts
     * input and no native UI owns it (the same gates InputEvent applies). */
    uint8_t *ui=*(uint8_t **)(base+RVA_UI);
    if(camera[0x107u] && readable(ui,0x58u) && !ui[0x56u]) {
        CameraInputEvent event={0};
        event.action=0x69u; event.magnitude=look_x; ((InputEvent)(base+RVA_INPUT_EVENT))(camera,&event);
        event.action=0x6au; event.magnitude=look_y; ((InputEvent)(base+RVA_INPUT_EVENT))(camera,&event);
    }
    /* Camera targets cache the actor position (GameObjectTarget, RVA
     * 0x19A290: copies the live CPosition) and the composed offset matrix
     * (OffsetTarget, RVA 0x19A0D0); their refresh is scheduled native work
     * that the pause also stops, so the Exploration solve would aim at a
     * stale point. Refresh the camera's two target slots, inner first, through
     * their own vtable slot 1, exact classes only. */
    ((TargetRefresh)(base+RVA_GAME_OBJECT_TARGET_REFRESH))(targets.inner);
    if(targets.outer!=targets.inner)
        ((TargetRefresh)(base+RVA_OFFSET_TARGET_REFRESH))(targets.outer);
    /* The scheduled update refuses while the speed global is paused. Lift
     * that byte for this one call only; the pause count is untouched. */
    uint8_t paused=speed[0x28u];
    float timing[4]={0.0f,0.0f,0.0f,dt_seconds};
    uint16_t serial=*(uint16_t *)((uint8_t *)owner->render_state+0x2cu);
    /* This cosmetic continuation changes only the native small-step discard
     * during our owned Exploration call. Re-prove the complete owner/targets
     * at its exact native seam; cast, interpolation and all other calls retain
     * the original cutoff. Spring, error bound and collision remain native. */
    SudekiMpLanStoryClientCameraHeightScope height={camera,
        *(void **)(camera+0x3cu),*(void **)(camera+0x40u),targets.outer,targets.inner};
    BOOL height_started=exploration && SudekiMpLanStoryClientCameraHeightBegin(&height);
    speed[0x28u]=0;
    ((NodeUpdate)(base+RVA_NODE_UPDATE))(camera+8u,timing);
    speed[0x28u]=paused;
    if(height_started) SudekiMpLanStoryClientCameraHeightEnd();
    memset(&height_owner,0,sizeof(height_owner));height_actor=height_position=NULL;
    tick_active=FALSE;
    if(ticks<UINT_MAX) ++ticks;
    static BOOL was_exploration;
    if(exploration!=was_exploration) {
        was_exploration=exploration;
        SudekiMpLogFormat("story_client_camera event=state side=client exploration=%u state_vt=%06lx ticks=%u\r\n",
            exploration,(unsigned long)((uintptr_t)*(void **)state-(uintptr_t)base),ticks);
    }
    if(ticks==1u || ticks==300u) {
        SudekiMpLogFormat("story_client_camera event=ticking count=%u dt=%.4f serial=%u->%u look=%.2f,%.2f\r\n",
            ticks,(double)dt_seconds,serial,*(uint16_t *)((uint8_t *)owner->render_state+0x2cu),(double)look_x,(double)look_y);
    }
    return lease_exact(owner,NULL);
}
