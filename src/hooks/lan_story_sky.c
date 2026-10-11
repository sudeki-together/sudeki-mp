#include "hooks/lan_story_sky.h"
#include "hooks/call_hook.h"
#include "hooks/lan_story_spawn_trace.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Story sky requires the supported x86 native ABI"
#endif

/* CSceneManager (global 808D58) thiscall exports, callee-cleaned:
 *   SetSkyDomeColour(uint r,uint g,uint b,float)      1BC50, RET 0x10
 *   BlendSkyDomeColour(uint,uint,uint,float,float)    1BD60, RET 0x14
 *   SetSkyDomeBrightness(float,float)                 1BBB0, RET 0x8
 * cdecl exports: SetFogColour(float,float,float) 103610, SetFogRange(float,
 * float) 1036D0, SetVerticalFog(const char *,bool,float) 1C360. Each hook
 * copies only whole leading instructions with no relative operand. */
enum { SCENE_GLOBAL=0x408d58u };
static const unsigned rvas[SUDEKIMP_STORY_SKY_KINDS]={0x1bc50u,0x1bd60u,0x1bbb0u,0x103610u,0x1036d0u,0x1c360u};
static const uint8_t entries[SUDEKIMP_STORY_SKY_KINDS][7]={
    {0xd9,0xee,0xd9,0x44,0x24,0x10},        /* FLDZ; FLD [ESP+10] */
    {0xd9,0xee,0x56,0xd8,0x54,0x24,0x18},   /* FLDZ; PUSH ESI; FCOM [ESP+18] */
    {0xd9,0xee,0xd9,0x44,0x24,0x04},        /* FLDZ; FLD [ESP+4] */
    {0xd9,0x44,0x24,0x04,0x53},             /* FLD [ESP+4]; PUSH EBX */
    {0xd9,0x44,0x24,0x08,0x83,0xec,0x08},   /* FLD [ESP+8]; SUB ESP,8 */
    {0x8b,0x44,0x24,0x04,0x56}};            /* MOV EAX,[ESP+4]; PUSH ESI */
static const size_t lengths[SUDEKIMP_STORY_SKY_KINDS]={6,7,6,5,7,5};
static SudekiMpInlineHook hooks[SUDEKIMP_STORY_SKY_KINDS];
static uint8_t *base;
static DWORD native_thread;
static BOOL replaying;
static uint32_t next_serial;
static SudekiMpStorySkyCall latest[SUDEKIMP_STORY_SKY_KINDS];
static char latest_fog[SUDEKIMP_STORY_AREA_NAME];
static uint32_t applied[SUDEKIMP_STORY_SKY_KINDS];
static unsigned logs;

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && VirtualQuery(p,&m,sizeof(m))==sizeof(m) && m.State==MEM_COMMIT &&
        !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) && a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static void *scene(void) {
    return base && readable(base+SCENE_GLOBAL,4u)?*(void **)(base+SCENE_GLOBAL):NULL;
}
static uint32_t bits(float f) { uint32_t u; memcpy(&u,&f,4); return u; }
static float flt(uint32_t u) { float f; memcpy(&f,&u,4); return f; }
static void record(unsigned kind,const uint32_t *args,unsigned n) {
    if(replaying || GetCurrentThreadId()!=native_thread) return;
    if(++next_serial==0u) next_serial=1u;
    memset(&latest[kind],0,sizeof(latest[kind]));
    latest[kind].serial=next_serial; memcpy(latest[kind].args,args,n*4u);
    if(logs<5000u) { ++logs;
        SudekiMpLogFormat("story_sky event=call kind=%u serial=%lu args=%08lx,%08lx,%08lx,%08lx,%08lx\r\n",kind,
            (unsigned long)next_serial,(unsigned long)latest[kind].args[0],(unsigned long)latest[kind].args[1],
            (unsigned long)latest[kind].args[2],(unsigned long)latest[kind].args[3],(unsigned long)latest[kind].args[4]); }
}

typedef void (__attribute__((thiscall)) *Colour)(void *,unsigned,unsigned,unsigned,float);
typedef void (__attribute__((thiscall)) *Blend)(void *,unsigned,unsigned,unsigned,float,float);
typedef void (__attribute__((thiscall)) *Brightness)(void *,float,float);
typedef void (__cdecl *FogColour)(float,float,float);
typedef void (__cdecl *FogRange)(float,float);
typedef void (__cdecl *VerticalFog)(const char *,int,float);

static void __attribute__((thiscall)) colour_hook(void *self,unsigned r,unsigned g,unsigned b,float a) {
    if(self==scene()) { uint32_t w[4]={r,g,b,bits(a)}; record(SUDEKIMP_STORY_SKY_COLOUR,w,4u); }
    ((Colour)hooks[0].trampoline)(self,r,g,b,a);
}
static void __attribute__((thiscall)) blend_hook(void *self,unsigned r,unsigned g,unsigned b,float a,float t) {
    if(self==scene()) { uint32_t w[5]={r,g,b,bits(a),bits(t)}; record(SUDEKIMP_STORY_SKY_BLEND,w,5u); }
    ((Blend)hooks[1].trampoline)(self,r,g,b,a,t);
}
static void __attribute__((thiscall)) brightness_hook(void *self,float a,float b) {
    if(self==scene()) { uint32_t w[2]={bits(a),bits(b)}; record(SUDEKIMP_STORY_SKY_BRIGHTNESS,w,2u); }
    ((Brightness)hooks[2].trampoline)(self,a,b);
}
static void __cdecl fog_colour_hook(float r,float g,float b) {
    uint32_t w[3]={bits(r),bits(g),bits(b)}; record(SUDEKIMP_STORY_SKY_FOG_COLOUR,w,3u);
    ((FogColour)hooks[3].trampoline)(r,g,b);
}
static void __cdecl fog_range_hook(float a,float b) {
    uint32_t w[2]={bits(a),bits(b)}; record(SUDEKIMP_STORY_SKY_FOG_RANGE,w,2u);
    ((FogRange)hooks[4].trampoline)(a,b);
}
static void __cdecl vertical_fog_hook(const char *name,int on,float value) {
    if(!replaying && GetCurrentThreadId()==native_thread) {
        char copy[SUDEKIMP_STORY_AREA_NAME]={0}; unsigned k=0;
        while(name && k<SUDEKIMP_STORY_AREA_NAME-1u && readable(name+k,1u) && name[k]>=0x20 && name[k]<0x7f) { copy[k]=name[k]; ++k; }
        if(name && readable(name+k,1u) && !name[k]) {
            uint32_t w[2]={(uint32_t)(on&0xff),bits(value)};
            record(SUDEKIMP_STORY_SKY_VERTICAL_FOG,w,2u); memcpy(latest_fog,copy,sizeof(copy));
        }
    }
    ((VerticalFog)hooks[5].trampoline)(name,on,value);
}
static const void *replacements[SUDEKIMP_STORY_SKY_KINDS]={(const void *)colour_hook,(const void *)blend_hook,
    (const void *)brightness_hook,(const void *)fog_colour_hook,(const void *)fog_range_hook,(const void *)vertical_fog_hook};

/* Lighting script calls (the ambush darkening is stronger than the sky
 * dome alone): CSceneManager::SetAmbientLight(f,f,f) 1B280 thiscall RET 0xC;
 * cdecl SetLightGroupMultiplier(int,f,f,f) 103890, IncLightGroupMultiplier
 * 103930, SetBrightness(f) 103D70, SetColourAdjust(f,f,f) 103DF0,
 * SetBackgroundColour(f,f,f) 103530. Recorded in call order.
 * CLightManager::SetLightingChangeRate (1F740) stays native: Capture sends
 * manager+30 with its target every frame and Follow leases both together.
 * Detouring that redundant setter also invalidates the cast-light ABI. */
enum { LIGHT_HOOKS=6, LIGHT_MANAGER_GLOBAL=0x408d7cu };
static const unsigned light_rvas[LIGHT_HOOKS]={0x1b280u,0x103890u,0x103930u,0x103d70u,0x103df0u,0x103530u};
static const uint8_t light_entries[LIGHT_HOOKS][9]={
    {0xd9,0x44,0x24,0x04,0x53},
    {0x55,0x8b,0xec,0x83,0xe4,0xf8,0x83,0xec,0x1c},
    {0x55,0x8b,0xec,0x83,0xe4,0xf8,0x83,0xec,0x1c},
    {0x55,0x8b,0xec,0x83,0xe4,0xf8},
    {0x55,0x8b,0xec,0x83,0xe4,0xf8},
    {0xd9,0x44,0x24,0x04,0x53}};
static const size_t light_lengths[LIGHT_HOOKS]={5,9,9,6,6,5};
static SudekiMpInlineHook light_hooks[LIGHT_HOOKS];
static SudekiMpStoryLightCall light_ring[SUDEKIMP_STORY_LIGHT_MAX];
static uint32_t light_serial,light_applied;
static void light_record(uint32_t kind,const uint32_t *args,unsigned n) {
    if(replaying || GetCurrentThreadId()!=native_thread) return;
    memmove(light_ring,light_ring+1,(SUDEKIMP_STORY_LIGHT_MAX-1u)*sizeof(*light_ring));
    SudekiMpStoryLightCall *c=&light_ring[SUDEKIMP_STORY_LIGHT_MAX-1u];
    memset(c,0,sizeof(*c));
    if(++light_serial==0u) light_serial=1u;
    c->serial=light_serial; c->kind=kind; memcpy(c->args,args,n*4u);
    if(logs<5000u) { ++logs;
        SudekiMpLogFormat("story_sky event=light kind=%lu serial=%lu args=%08lx,%08lx,%08lx,%08lx\r\n",(unsigned long)kind,
            (unsigned long)light_serial,(unsigned long)c->args[0],(unsigned long)c->args[1],(unsigned long)c->args[2],(unsigned long)c->args[3]); }
}
typedef void (__attribute__((thiscall)) *Ambient)(void *,float,float,float);
typedef void (__cdecl *Group)(int,float,float,float);
typedef void (__cdecl *One)(float);
typedef void (__cdecl *Three)(float,float,float);
static void __attribute__((thiscall)) ambient_hook(void *self,float r,float g,float b) {
    if(self==scene()) { uint32_t w[3]={bits(r),bits(g),bits(b)}; light_record(SUDEKIMP_STORY_LIGHT_AMBIENT,w,3u); }
    ((Ambient)light_hooks[0].trampoline)(self,r,g,b);
}
static void __cdecl group_set_hook(int group,float r,float g,float b) {
    uint32_t w[4]={(uint32_t)group,bits(r),bits(g),bits(b)}; light_record(SUDEKIMP_STORY_LIGHT_GROUP_SET,w,4u);
    ((Group)light_hooks[1].trampoline)(group,r,g,b);
}
static void __cdecl group_inc_hook(int group,float r,float g,float b) {
    uint32_t w[4]={(uint32_t)group,bits(r),bits(g),bits(b)}; light_record(SUDEKIMP_STORY_LIGHT_GROUP_INC,w,4u);
    ((Group)light_hooks[2].trampoline)(group,r,g,b);
}
static void __cdecl brightness_global_hook(float v) {
    uint32_t w[1]={bits(v)}; light_record(SUDEKIMP_STORY_LIGHT_BRIGHTNESS,w,1u);
    ((One)light_hooks[3].trampoline)(v);
}
static void __cdecl colour_adjust_hook(float r,float g,float b) {
    uint32_t w[3]={bits(r),bits(g),bits(b)}; light_record(SUDEKIMP_STORY_LIGHT_COLOUR_ADJUST,w,3u);
    ((Three)light_hooks[4].trampoline)(r,g,b);
}
static void __cdecl background_hook(float r,float g,float b) {
    uint32_t w[3]={bits(r),bits(g),bits(b)}; light_record(SUDEKIMP_STORY_LIGHT_BACKGROUND,w,3u);
    ((Three)light_hooks[5].trampoline)(r,g,b);
}
static const void *light_replacements[LIGHT_HOOKS]={(const void *)ambient_hook,(const void *)group_set_hook,
    (const void *)group_inc_hook,(const void *)brightness_global_hook,(const void *)colour_adjust_hook,
    (const void *)background_hook};
static void light_follow(const SudekiMpStoryEffectState *host) {
    void *s=scene();
    for(unsigned i=0;i<SUDEKIMP_STORY_LIGHT_MAX;++i) {
        const SudekiMpStoryLightCall *c=&host->light[i];
        if(!c->serial || (int32_t)(c->serial-light_applied)<=0) continue;
        light_applied=c->serial;
        BOOL ok=TRUE; float f[4];
        for(unsigned a=0;a<4u;++a) { f[a]=flt(c->args[a]); }
        for(unsigned a=c->kind==SUDEKIMP_STORY_LIGHT_GROUP_SET||c->kind==SUDEKIMP_STORY_LIGHT_GROUP_INC?1u:0u;a<4u;++a)
            if(!isfinite(f[a]) || fabsf(f[a])>1.0e6f) ok=FALSE;
        unsigned k=c->kind-1u;
        replaying=TRUE;
        if(ok) switch(c->kind) {
        case SUDEKIMP_STORY_LIGHT_AMBIENT: if((ok=s!=NULL)) ((Ambient)(base+light_rvas[k]))(s,f[0],f[1],f[2]); break;
        case SUDEKIMP_STORY_LIGHT_GROUP_SET: case SUDEKIMP_STORY_LIGHT_GROUP_INC:
            if((ok=c->args[0]<64u)) ((Group)(base+light_rvas[k]))((int)c->args[0],f[1],f[2],f[3]);
            break;
        case SUDEKIMP_STORY_LIGHT_BRIGHTNESS: ((One)(base+light_rvas[k]))(f[0]); break;
        case SUDEKIMP_STORY_LIGHT_COLOUR_ADJUST: case SUDEKIMP_STORY_LIGHT_BACKGROUND:
            ((Three)(base+light_rvas[k]))(f[0],f[1],f[2]); break;
        /* The authoritative state applies rate and target together under one
         * restorable lease. Replaying this setter first would lose the local rate. */
        case SUDEKIMP_STORY_LIGHT_CHANGE_RATE: break;
        default: ok=FALSE; break;
        }
        replaying=FALSE;
        if(logs<5000u) { ++logs;
            SudekiMpLogFormat("story_sky event=light_replayed kind=%lu serial=%lu ok=%u\r\n",(unsigned long)c->kind,(unsigned long)c->serial,(unsigned)ok); }
    }
}

/* CSceneManager::SetSkyDome(ResourceName &) 1B440, thiscall RET 4: swaps
 * the sky dome entity (live: the ambush leaves the host on a different dome,
 * scene+0x4C hash). The host records the name; a client replays it. */
enum { RVA_SKY_DOME=0x1b440u };
static const uint8_t dome_entry[9]={0x55,0x8b,0xec,0x83,0xe4,0xf8,0x83,0xec,0x14};
static SudekiMpInlineHook dome_hook;
static uint32_t dome_serial,dome_applied;
static char dome_name[SUDEKIMP_STORY_AREA_NAME];
typedef void (__attribute__((thiscall)) *Dome)(void *,void *);
static void __attribute__((thiscall)) dome_hook_fn(void *self,void *resource) {
    if(self==scene() && !replaying && GetCurrentThreadId()==native_thread) {
        const uint32_t *r=resource; char name[SUDEKIMP_STORY_AREA_NAME]={0};
        const uint32_t *rec=readable(r,12u)?(const uint32_t *)(uintptr_t)r[2]:NULL;
        const char *t=readable(rec,8u)?(const char *)(uintptr_t)rec[1]:NULL; unsigned k=0;
        while(t && k<SUDEKIMP_STORY_AREA_NAME-1u && readable(t+k,1u) && t[k]>=0x20 && t[k]<0x7f) { name[k]=t[k]; ++k; }
        if(k && t && readable(t+k,1u) && !t[k]) {
            memcpy(dome_name,name,sizeof(name)); if(++dome_serial==0u) dome_serial=1u;
            if(logs<5000u) { ++logs; SudekiMpLogFormat("story_sky event=sky_dome name=%s serial=%lu\r\n",name,(unsigned long)dome_serial); }
        }
    }
    ((Dome)dome_hook.trampoline)(self,resource);
}
static void dome_use(void *resource,void *context) {
    ((Dome)(base+RVA_SKY_DOME))(context,resource);
}

BOOL SudekiMpLanStorySkyInstall(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    if(!b || !SudekiMpCheckLoadedExecutable(image)) { SetLastError(ERROR_INVALID_DATA); return FALSE; }
    for(unsigned i=0;i<SUDEKIMP_STORY_SKY_KINDS;++i)
        if(!SudekiMpInstallInlineHook(&hooks[i],b+rvas[i],entries[i],lengths[i],replacements[i])) {
            DWORD error=GetLastError();
            while(i--) (void)SudekiMpRestoreInlineHook(&hooks[i]);
            SetLastError(error); return FALSE;
        }
    for(unsigned i=0;i<LIGHT_HOOKS;++i)
        if(!SudekiMpInstallInlineHook(&light_hooks[i],b+light_rvas[i],light_entries[i],light_lengths[i],light_replacements[i])) {
            DWORD error=GetLastError();
            while(i--) (void)SudekiMpRestoreInlineHook(&light_hooks[i]);
            for(unsigned k=SUDEKIMP_STORY_SKY_KINDS;k--;) (void)SudekiMpRestoreInlineHook(&hooks[k]);
            SetLastError(error); return FALSE;
        }
    if(!SudekiMpInstallInlineHook(&dome_hook,b+RVA_SKY_DOME,dome_entry,sizeof(dome_entry),(const void *)dome_hook_fn)) {
        DWORD error=GetLastError();
        for(unsigned i=LIGHT_HOOKS;i--;) (void)SudekiMpRestoreInlineHook(&light_hooks[i]);
        for(unsigned k=SUDEKIMP_STORY_SKY_KINDS;k--;) (void)SudekiMpRestoreInlineHook(&hooks[k]);
        SetLastError(error); return FALSE;
    }
    base=b; native_thread=GetCurrentThreadId();
    SudekiMpLogWrite("story_sky event=installed\r\n");
    return TRUE;
}
BOOL SudekiMpLanStorySkyUninstall(void) {
    BOOL ok=TRUE;
    if(!SudekiMpLanStorySkyReleaseLighting()) return FALSE;
    if(dome_hook.installed && !SudekiMpRestoreInlineHook(&dome_hook)) ok=FALSE;
    for(unsigned i=LIGHT_HOOKS;i--;) if(light_hooks[i].installed && !SudekiMpRestoreInlineHook(&light_hooks[i])) ok=FALSE;
    for(unsigned i=SUDEKIMP_STORY_SKY_KINDS;i--;) if(hooks[i].installed && !SudekiMpRestoreInlineHook(&hooks[i])) ok=FALSE;
    return ok;
}
static void fog_state_log(const char *side);
/* CLightManager (global 808D7C, primary vtable 2CA030): +0x30 change rate,
 * +0x34..+0x3C current colour, +0x40..+0x48 target colour. AddLightEffect
 * stores the target; the native scheduler already advances current and
 * publishes it on contained clients. Copy the host target because the client
 * does not execute the ambush's AddLightEffect. Never invoke Update here. */
enum { RVA_LIGHT_VTABLE=0x2ca030u, RVA_LIGHT_UPDATE=0x1f350u };
static const uint8_t light_update_head[15]={0x55,0x8b,0xec,0x83,0xe4,0xf8,0x83,0xec,0x24,0x8b,0x45,0x08,0xd9,0x40,0x0c};
static struct {
    uint8_t *manager;
    void *scene;
    float saved_target[3],saved_rate,last_target[3],last_rate;
} light_lease;
static uint8_t *light_manager(void) {
    uint8_t *m=base && readable(base+LIGHT_MANAGER_GLOBAL,4u)?*(uint8_t **)(base+LIGHT_MANAGER_GLOBAL):NULL;
    return readable(m,0x6cu) && *(void **)m==base+RVA_LIGHT_VTABLE?m:NULL;
}
static BOOL light_values_valid(const float target[3],float rate) {
    for(unsigned i=0;i<3u;++i)
        if(!isfinite(target[i]) || target[i]<0.0f || target[i]>4.0f) return FALSE;
    return isfinite(rate) && rate>=0.0f && rate<=100.0f;
}
static BOOL light_target_owner(uint8_t *m) {
    MEMORY_BASIC_INFORMATION info;
    /* This follower is for the contained client, which has no private cast
     * lighting. Reject a hooked update slot (including the cast-light owner)
     * instead of treating a temporarily selected private manager as the world. */
    return base && native_thread==GetCurrentThreadId() && m && m==light_manager() && scene() &&
        readable(base+RVA_LIGHT_VTABLE+4u,4u) &&
        *(void **)(base+RVA_LIGHT_VTABLE+4u)==base+RVA_LIGHT_UPDATE &&
        readable(base+RVA_LIGHT_UPDATE,sizeof(light_update_head)) &&
        !memcmp(base+RVA_LIGHT_UPDATE,light_update_head,sizeof(light_update_head)) &&
        VirtualQuery(m,&info,sizeof(info))==sizeof(info) &&
        (info.Protect&(PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)) &&
        (!light_lease.manager || (light_lease.manager==m && light_lease.scene==scene()));
}
BOOL SudekiMpLanStorySkyReleaseLighting(void) {
    uint8_t *m=light_lease.manager;
    if(!m) return TRUE;
    if(!light_target_owner(m)) return FALSE;
    /* A native script can supersede either field independently. Restore only
     * values still owned by this lease; never overwrite that newer writer. */
    BOOL target=!memcmp(m+0x40u,light_lease.last_target,12u);
    BOOL rate=!memcmp(m+0x30u,&light_lease.last_rate,4u);
    if(target) memcpy(m+0x40u,light_lease.saved_target,12u);
    if(rate) memcpy(m+0x30u,&light_lease.saved_rate,4u);
    memset(&light_lease,0,sizeof(light_lease));
    SudekiMpLogFormat("story_sky event=light_released target=%u rate=%u\r\n",(unsigned)target,(unsigned)rate);
    return TRUE;
}
static void light_state_follow(const SudekiMpStoryEffectState *host) {
    uint8_t *m=light_manager();
    if(!host->light_valid || !light_target_owner(m)) return;
    float target[3],rate=flt(host->light_rate);
    for(unsigned i=0;i<3u;++i) target[i]=flt(host->light_target[i]);
    if(!light_values_valid(target,rate) ||
        !light_values_valid((float *)(m+0x40u),*(float *)(m+0x30u))) return;
    BOOL changed=memcmp(m+0x40u,target,12u) || memcmp(m+0x30u,&rate,4u);
    if(!light_lease.manager && !changed) return;
    /* Keep a newer local script value as the exit baseline if it changed a
     * field since our last write. Current RGB and the effect stack are untouched. */
    if(!light_lease.manager || memcmp(m+0x40u,light_lease.last_target,12u))
        memcpy(light_lease.saved_target,m+0x40u,12u);
    if(!light_lease.manager || memcmp(m+0x30u,&light_lease.last_rate,4u))
        memcpy(&light_lease.saved_rate,m+0x30u,4u);
    light_lease.manager=m; light_lease.scene=scene();
    memcpy(light_lease.last_target,target,12u); light_lease.last_rate=rate;
    if(changed) {
        memcpy(m+0x40u,target,12u); memcpy(m+0x30u,&rate,4u);
        if(logs<5000u) { ++logs; SudekiMpLogFormat("story_sky event=light_target target=%.3f,%.3f,%.3f rate=%.3f\r\n",
            (double)target[0],(double)target[1],(double)target[2],(double)rate); }
    }
}
void SudekiMpLanStorySkyCapture(SudekiMpStoryEffectState *out) {
    fog_state_log("host");
    if(!out) return;
    memcpy(out->sky,latest,sizeof(latest)); memcpy(out->vertical_fog,latest_fog,sizeof(latest_fog));
    memcpy(out->light,light_ring,sizeof(light_ring));
    /* State, not only calls: the host's current dome is scene+0x48 ResourceName
     * {kind bits, hash +0x4C, record +0x50 -> C string at record+4}. The hash
     * serves as the serial (0x7FFFF = none). */
    {   const uint8_t *sc=scene(); uint32_t hash=readable(sc+0x48u,12u)?*(const uint32_t *)(sc+0x4cu):0u;
        const uint32_t *rec=readable(sc+0x48u,12u)?*(const uint32_t *const *)(sc+0x50u):NULL;
        const char *t=readable(rec,8u)?(const char *)(uintptr_t)rec[1]:NULL; char name[SUDEKIMP_STORY_AREA_NAME]={0}; unsigned k=0;
        while(t && k<SUDEKIMP_STORY_AREA_NAME-1u && readable(t+k,1u) && t[k]>=0x20 && t[k]<0x7f) { name[k]=t[k]; ++k; }
        if(hash && hash!=0x7ffffu && k && readable(t+k,1u) && !t[k]) {
            out->sky_dome_serial=hash; memcpy(out->sky_dome,name,sizeof(name)); }
        if(readable(sc+0x68u,4u)) memcpy(&out->sky_dome_speed,sc+0x68u,4u); }
    {   uint8_t *m=light_manager();
        if(m) { out->light_valid=1u; memcpy(out->light_target,m+0x40u,12u); memcpy(&out->light_rate,m+0x30u,4u); } }
}
/* The scene manager advances a blend (scene+0x54 duration, +0x58 elapsed,
 * +0x60 current, +0x64 target) in its own update, which the contained
 * client world does not run. On the client a blend (and brightness, which
 * is a blend of the current colour) is stepped here instead, by calling the
 * immediate native SetSkyDomeColour with the interpolated colour each
 * service. Colour word (+0x5C/+0x60): b, g, r, (1-alpha)*255. */
static struct { BOOL active; float from[4],to[4],duration; DWORD started; } blend;
static void current_colour(const uint8_t *scene_object,float out[4]) {
    uint32_t w=readable(scene_object+0x60u,4u)?*(const uint32_t *)(scene_object+0x60u):0u;
    out[0]=(float)((w>>16)&0xffu); out[1]=(float)((w>>8)&0xffu); out[2]=(float)(w&0xffu);
    out[3]=1.0f-(float)((w>>24)&0xffu)/255.0f;
}
static void blend_start(void *s,float r,float g,float b,float a,float duration) {
    current_colour(s,blend.from);
    blend.to[0]=r; blend.to[1]=g; blend.to[2]=b; blend.to[3]=a<0.0f?0.0f:a>1.0f?1.0f:a;
    blend.duration=duration; blend.started=GetTickCount(); blend.active=TRUE;
}
static void blend_step(void *s) {
    if(!blend.active) return;
    float t=blend.duration>0.0f?(float)(GetTickCount()-blend.started)/(blend.duration*1000.0f):1.0f;
    if(t>=1.0f) { t=1.0f; blend.active=FALSE; }
    float c[4];
    for(unsigned i=0;i<4u;++i) c[i]=blend.from[i]+(blend.to[i]-blend.from[i])*t;
    replaying=TRUE;
    ((Colour)(base+rvas[SUDEKIMP_STORY_SKY_COLOUR]))(s,(unsigned)(c[0]+0.5f),(unsigned)(c[1]+0.5f),(unsigned)(c[2]+0.5f),c[3]);
    replaying=FALSE;
}
/* Research: native fog globals and their render-state caches (SetFogColour
 * 76C7A8/76CBA8, SetFogRange 76C7B0,76C7B4/76CBB0,76CBB4, fog enable
 * 76C790/76CB90) and the sky colour word (scene+0x60), every 2 s per side. */
static void fog_state_log(const char *side) {
    static DWORD last;
    if(!base || !SudekiMpLogResearchEnabled() || GetTickCount()-last<2000u) return;
    last=GetTickCount();
    const uint32_t *g=(const uint32_t *)(base+0x36c790u);
    if(!readable(g,0x430u)) return;
    const uint8_t *s=scene();
    SudekiMpLogFormat("story_sky event=fog_state side=%s colour=%08lx/%08lx start=%08lx/%08lx end=%08lx/%08lx enable=%lu/%lu sky=%08lx blend=%u\r\n",side,
        (unsigned long)g[(0x7a8-0x790)/4],(unsigned long)g[(0xba8-0x790)/4],
        (unsigned long)g[(0x7b0-0x790)/4],(unsigned long)g[(0xbb0-0x790)/4],
        (unsigned long)g[(0x7b4-0x790)/4],(unsigned long)g[(0xbb4-0x790)/4],
        (unsigned long)g[0],(unsigned long)g[(0xb90-0x790)/4],
        (unsigned long)(readable(s+0x60u,4u)?*(const uint32_t *)(s+0x60u):0u),(unsigned)blend.active);
    /* Lighting held in the scene manager (first 0x100 bytes) and the light
     * manager (808D7C, first 0x80): diff host against client. */
    static char hex[0x100u/4u*9u+2u];
    for(unsigned part=0;part<2u;++part) {
        const uint8_t *o=part?(readable(base+LIGHT_MANAGER_GLOBAL,4u)?*(const uint8_t *const *)(base+LIGHT_MANAGER_GLOBAL):NULL):s;
        unsigned n=part?0x80u:0x100u,k=0; hex[0]=0;
        if(!readable(o,n)) continue;
        for(unsigned i=0;i<n;i+=4u) k+=(unsigned)wsprintfA(hex+k,"%08lx ",(unsigned long)*(const uint32_t *)(o+i));
        SudekiMpLogFormat("story_sky event=%s side=%s %s\r\n",part?"light_manager":"scene_manager",side,hex);
    }
}
static BOOL finite_args(const SudekiMpStorySkyCall *c,unsigned first,unsigned n) {
    for(unsigned i=first;i<first+n;++i) { float f=flt(c->args[i]); if(!isfinite(f) || fabsf(f)>1.0e7f) return FALSE; }
    return TRUE;
}
void SudekiMpLanStorySkyFollow(const SudekiMpStoryEffectState *host) {
    void *s=scene();
    if(!host || !base || !s || GetCurrentThreadId()!=native_thread) return;
    /* Host order: brightness, then blend; replay in serial order so the
     * newest blend wins like native. */
    unsigned order[SUDEKIMP_STORY_SKY_KINDS],n=0;
    for(unsigned k=0;k<SUDEKIMP_STORY_SKY_KINDS;++k) order[n++]=k;
    for(unsigned i=1;i<n;++i) for(unsigned j=i;j>0 && host->sky[order[j]].serial<host->sky[order[j-1u]].serial;--j)
        { unsigned x=order[j]; order[j]=order[j-1u]; order[j-1u]=x; }
    for(unsigned o=0;o<n;++o) {
        unsigned k=order[o];
        const SudekiMpStorySkyCall *c=&host->sky[k];
        if(!c->serial || c->serial==applied[k]) continue;
        BOOL ok=TRUE;
        replaying=TRUE;
        switch(k) {
        case SUDEKIMP_STORY_SKY_COLOUR:
            if((ok=c->args[0]<=255u && c->args[1]<=255u && c->args[2]<=255u && finite_args(c,3u,1u)))
                ((Colour)(base+rvas[k]))(s,c->args[0],c->args[1],c->args[2],flt(c->args[3]));
            break;
        case SUDEKIMP_STORY_SKY_BLEND:
            if((ok=c->args[0]<=255u && c->args[1]<=255u && c->args[2]<=255u && finite_args(c,3u,2u) &&
                flt(c->args[4])>0.0f))
                blend_start(s,(float)c->args[0],(float)c->args[1],(float)c->args[2],flt(c->args[3]),flt(c->args[4]));
            break;
        case SUDEKIMP_STORY_SKY_BRIGHTNESS:
            if((ok=finite_args(c,0u,2u))) {
                float cur[4],brightness=flt(c->args[0]);
                current_colour(s,cur);
                brightness=brightness<0.0f?0.0f:brightness>1.0f?1.0f:brightness;
                if(flt(c->args[1])>0.0f) blend_start(s,cur[0],cur[1],cur[2],1.0f-brightness,flt(c->args[1]));
                else { blend.active=FALSE; replaying=TRUE;
                    ((Colour)(base+rvas[SUDEKIMP_STORY_SKY_COLOUR]))(s,(unsigned)cur[0],(unsigned)cur[1],(unsigned)cur[2],1.0f-brightness); }
            }
            break;
        case SUDEKIMP_STORY_SKY_FOG_COLOUR:
            if((ok=finite_args(c,0u,3u))) ((FogColour)(base+rvas[k]))(flt(c->args[0]),flt(c->args[1]),flt(c->args[2]));
            break;
        case SUDEKIMP_STORY_SKY_FOG_RANGE:
            if((ok=finite_args(c,0u,2u))) ((FogRange)(base+rvas[k]))(flt(c->args[0]),flt(c->args[1]));
            break;
        default:
            if((ok=host->vertical_fog[0] && c->args[0]<=1u && finite_args(c,1u,1u)))
                ((VerticalFog)(base+rvas[k]))(host->vertical_fog,(int)c->args[0],flt(c->args[1]));
            break;
        }
        replaying=FALSE;
        if(k==SUDEKIMP_STORY_SKY_COLOUR && ok) blend.active=FALSE;
        applied[k]=c->serial;
        if(logs<5000u) { ++logs;
            SudekiMpLogFormat("story_sky event=replayed kind=%u serial=%lu ok=%u\r\n",k,(unsigned long)c->serial,(unsigned)ok); }
    }
    blend_step(s);
    light_follow(host);
    light_state_follow(host);
    /* Dome animation speed (live: host 24, client 1 -> the storm dome's clouds
     * and lightning barely moved). SetSkyDomePlaySpeed 1B550, thiscall, RET 4. */
    if(host->sky_dome_speed && readable((const uint8_t *)s+0x68u,4u) &&
        *(const uint32_t *)((const uint8_t *)s+0x68u)!=host->sky_dome_speed) {
        float speed=flt(host->sky_dome_speed);
        if(isfinite(speed) && speed>=0.0f && speed<=1000.0f &&
            !memcmp(base+0x1b550u,(const uint8_t *)"\x57\x8b\xf9\x8b\x47\x40\x85\xc0",8)) {
            typedef void (__attribute__((thiscall)) *DomeSpeed)(void *,float);
            replaying=TRUE; ((DomeSpeed)(base+0x1b550u))(s,speed); replaying=FALSE;
            if(logs<5000u) { ++logs; SudekiMpLogFormat("story_sky event=sky_dome_speed speed=%.3f\r\n",(double)speed); }
        }
    }
    if(host->sky_dome_serial && host->sky_dome_serial!=dome_applied &&
        !(readable((const uint8_t *)s+0x4cu,4u) && *(const uint32_t *)((const uint8_t *)s+0x4cu)==host->sky_dome_serial)) {
        dome_applied=host->sky_dome_serial;
        replaying=TRUE;
        BOOL ok=SudekiMpLanStoryWithResourceName(host->sky_dome,0x29u,dome_use,s);
        replaying=FALSE;
        if(logs<5000u) { ++logs; SudekiMpLogFormat("story_sky event=sky_dome_replayed name=%s serial=%lu ok=%u\r\n",
            host->sky_dome,(unsigned long)host->sky_dome_serial,(unsigned)ok); }
    }
    fog_state_log("client");
}
