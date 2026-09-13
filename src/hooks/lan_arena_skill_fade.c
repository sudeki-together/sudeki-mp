#include "hooks/lan_arena_skill_fade.h"
#include "hooks/call_hook.h"
#include "engine/log.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

/* Exact supported image: CLightManager eases +34 RGB towards +40, then
 * publishes light group 7. Its effect stack and update are NEVER changed.
 * Only the primary world draw borrows a view-local light group; restore
 * before native post-render/UI. Scene draw takes its renderer in EAX. */
enum { LIGHT_GLOBAL=0x408d7c, LIGHT_VTABLE=0x2ca030,
    SCENE_GLOBAL=0x408d58, SCENE_VTABLE=0x2c66b8,
    DEVICE_GLOBAL=0x3c31dc, GROUP_RGB=0x404cac,
    SET_GROUP=0x103890, DRAW=0x1d4750, DRAW_CALL=0x28d473,
    IMAGE_SIZE=0x45f000, EXPORT_FUNCTION=0x30cff8,
    EXPORT_NAME=0x30e580, EXPORT_ORDINAL=0x30f328 };
static const uint8_t setter_prefix[] = {
    0x55,0x8b,0xec,0x83,0xe4,0xf8,0x83,0xec,0x1c,0xd9,0x45,0x0c,0x56,0x8b,0x75,0x08};
static const uint8_t draw_prefix[] = {0x53,0x56,0x8b,0xf0,0x80,0xbe,0x88,0,0,0,0,0x57};
typedef void (__attribute__((regparm(1))) *DrawFunction)(void *renderer);
typedef void (*SetGroupFunction)(int group, float r, float g, float b);
static uint8_t *base;
static DrawFunction original_draw;
static SetGroupFunction set_group;
static SudekiMpLanArenaSkillFadeWitness view_witness;
static SudekiMpRelativeCallHook draw_hook;
static unsigned int draw_depth;
static struct { BOOL valid; void *light, *scene, *renderer, *device, *device_vtable;
    float saved[3], applied[3]; } lease;

static BOOL readable(const void *p, size_t n) {
    MEMORY_BASIC_INFORMATION m;
    uintptr_t a=(uintptr_t)p;
    return p && n && VirtualQuery(p,&m,sizeof(m)) && m.State==MEM_COMMIT &&
        !(m.Protect&(PAGE_NOACCESS|PAGE_GUARD)) && a+n>=a &&
        a+n <= (uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL rgb_valid(const float rgb[3]) {
    unsigned int i;
    for(i=0;i<3;++i) if(!isfinite(rgb[i]) || rgb[i]<0 || rgb[i]>1) return FALSE;
    return TRUE;
}
static BOOL exact_setter_export(uint8_t *candidate) {
    static const char name[]="?SetLightGroupMultiplier@@YAXHMMM@Z";
    uint32_t name_rva;
    /* The supported image's named export, ordinal 1009 (zero-based 1008).
     * Read the mapped PE tables, not the OS module registry: inert exact-image
     * tests deliberately do not load or execute proprietary startup code. */
    if(!readable(candidate+EXPORT_FUNCTION,4) ||
        !readable(candidate+EXPORT_NAME,4) || !readable(candidate+EXPORT_ORDINAL,2) ||
        *(uint32_t *)(candidate+EXPORT_FUNCTION)!=SET_GROUP ||
        *(uint16_t *)(candidate+EXPORT_ORDINAL)!=1008u) return FALSE;
    name_rva=*(uint32_t *)(candidate+EXPORT_NAME);
    return name_rva<=IMAGE_SIZE-sizeof(name) &&
        readable(candidate+name_rva,sizeof(name)) &&
        !memcmp(candidate+name_rva,name,sizeof(name));
}
static uint8_t *light_owner(void) {
    uint8_t *light;
    if(!base || !readable(base+LIGHT_GLOBAL,4)) return NULL;
    light=*(uint8_t **)(base+LIGHT_GLOBAL);
    if(!readable(light,0x6c) || *(void **)light!=base+LIGHT_VTABLE) return NULL;
    return light;
}
BOOL SudekiMpLanArenaReadSkillLight(float current[3], float baseline[3]) {
    uint8_t *light=light_owner();
    if(!light || !current || !baseline) return FALSE;
    memcpy(current,light+0x34,12); memcpy(baseline,light+0x50,12);
    return rgb_valid(current) && rgb_valid(baseline);
}
static BOOL exact_lease(void) {
    return lease.valid && base && light_owner()==lease.light &&
        readable(base+SCENE_GLOBAL,4) && *(void **)(base+SCENE_GLOBAL)==lease.scene &&
        readable(lease.scene,0x44) && *(void **)lease.scene==base+SCENE_VTABLE &&
        *(void **)((uint8_t *)lease.scene+0x40)==lease.renderer &&
        readable(base+DEVICE_GLOBAL,4) && *(void **)(base+DEVICE_GLOBAL)==lease.device &&
        readable(lease.device,4) && *(void **)lease.device==lease.device_vtable &&
        readable(base+GROUP_RGB,12) && !memcmp(base+GROUP_RGB,lease.applied,12);
}
static BOOL restore_light(void) {
    if(!lease.valid) return TRUE;
    if(!exact_lease()) { SetLastError(ERROR_BUSY); return FALSE; }
    set_group(7,lease.saved[0],lease.saved[1],lease.saved[2]);
    if(memcmp(base+GROUP_RGB,lease.saved,12)) { SetLastError(ERROR_WRITE_FAULT); return FALSE; }
    memset(&lease,0,sizeof(lease));
    return TRUE;
}
static BOOL stage_light(void *renderer, const float rgb[3]) {
    uint8_t *scene, *light;
    void *device;
    if(lease.valid || !rgb_valid(rgb) || !(light=light_owner()) ||
        !readable(base+SCENE_GLOBAL,4) || !readable(base+DEVICE_GLOBAL,4) ||
        !readable(base+GROUP_RGB,12)) return FALSE;
    scene=*(uint8_t **)(base+SCENE_GLOBAL); device=*(void **)(base+DEVICE_GLOBAL);
    if(!readable(scene,0x44) || *(void **)scene!=base+SCENE_VTABLE ||
        *(void **)(scene+0x40)!=renderer || !readable(device,4) ||
        !readable(*(void **)device,0x17c) || !rgb_valid((float *)(base+GROUP_RGB))) return FALSE;
    if(!memcmp(base+GROUP_RGB,rgb,12)) return FALSE; /* Proven no-op. */
    lease.light=light; lease.scene=scene; lease.renderer=renderer;
    lease.device=device; lease.device_vtable=*(void **)device;
    memcpy(lease.saved,base+GROUP_RGB,12); memcpy(lease.applied,lease.saved,12);
    lease.valid=TRUE;
    if(!exact_lease()) return FALSE;
    set_group(7,rgb[0],rgb[1],rgb[2]);
    memcpy(lease.applied,rgb,12);
    return TRUE;
}
static void __attribute__((regparm(1))) draw_local_view(void *renderer) {
    float rgb[3];
    BOOL staged=FALSE;
    ++draw_depth;
    if(draw_depth==1 && restore_light() && view_witness && view_witness(rgb))
        staged=stage_light(renderer,rgb);
    original_draw(renderer);
    if(staged && !restore_light())
        SudekiMpLogWrite("lan_arena_skill_fade event=restore_deferred policy=retain_exact_render_lease\r\n");
    --draw_depth;
}
BOOL SudekiMpInstallLanArenaSkillFade(HMODULE image, SudekiMpLanArenaSkillFadeWitness witness) {
    uint8_t *candidate=(uint8_t *)image;
    if(base || !image || !witness ||
        !readable(candidate+SET_GROUP,sizeof(setter_prefix)) ||
        memcmp(candidate+SET_GROUP,setter_prefix,sizeof(setter_prefix)) ||
        !readable(candidate+DRAW,sizeof(draw_prefix)) ||
        memcmp(candidate+DRAW,draw_prefix,sizeof(draw_prefix)) ||
        !exact_setter_export(candidate)) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    base=candidate; original_draw=(DrawFunction)(base+DRAW);
    set_group=(SetGroupFunction)(base+SET_GROUP); view_witness=witness;
    if(!SudekiMpInstallRelativeCallHook(&draw_hook,base+DRAW_CALL,original_draw,draw_local_view)) {
        base=NULL; original_draw=NULL; set_group=NULL; view_witness=NULL; return FALSE;
    }
    return TRUE;
}
BOOL SudekiMpUninstallLanArenaSkillFade(void) {
    if(draw_depth || !restore_light()) { SetLastError(ERROR_BUSY); return FALSE; }
    if(!SudekiMpRestoreRelativeCallHook(&draw_hook)) return FALSE;
    base=NULL; original_draw=NULL; set_group=NULL; view_witness=NULL;
    return TRUE;
}
