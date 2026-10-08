#define COBJMACROS
#include "hooks/lan_story_avatar_portrait.h"
#include "hooks/lan_story_avatar_spawn.h"
#include "hooks/title_portraits.h"
#include "engine/log.h"
#include <d3d9.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Native avatar portraits require the supported x86 ABI"
#endif
enum { CREATE=0x1d92e0,DESTROY=0x1d6b30,WAIT=0x1d6c70,
    RESIDENT_VT=0x2dd80c,TOKEN_VT=0x2dd79c,DEVICE=0x3c31dc,
    TEXTURES_ACTIVE=0x3c31cd,WORLD=0x408d10 };
typedef struct TextureName { uint32_t identifier; const char *name; } TextureName;
typedef void *(__attribute__((cdecl)) *Create)(const TextureName *,unsigned,unsigned,void **,void *,unsigned);
typedef void *(__attribute__((thiscall)) *Destroy)(void *,unsigned);
typedef void (__attribute__((thiscall)) *Wait)(void *);
static uint8_t *base;
static DWORD native_thread;
static Create native_create;
static Destroy native_destroy;
static Wait native_wait;
static struct {
    void *world,*device,*resident,*backend,*pending;
    IDirect3DDevice9 *unreleased_device;
    uint32_t epoch,reported;
    BOOL attempted,unknown,ready,busy;
} portrait;
static BOOL readable(const void *p,size_t n) {
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
    if(!p || !VirtualQuery(p,&m,sizeof(m)) || m.State!=MEM_COMMIT ||
        (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
    DWORD access=m.Protect&0xffu;
    return access==PAGE_EXECUTE || access==PAGE_EXECUTE_READ ||
        access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
}
static void *pointer(const void *p,unsigned o) {
    return *(void *const *)((const uint8_t *)p+o);
}
static void *reject(unsigned reason,const char *stage) {
    if(reason<32u && !(portrait.reported&(1u<<reason))) {
        portrait.reported|=1u<<reason;
        SudekiMpLogFormat("avatar_portrait event=unavailable stage=%s epoch=%lu retained=%u\r\n",
            stage,(unsigned long)portrait.epoch,SudekiMpLanStoryAvatarPortraitRetains());
    }
    return NULL;
}
static BOOL initialize(HMODULE image) {
    if(!image || (base && (base!=(uint8_t *)image || native_thread!=GetCurrentThreadId())) ||
        !SudekiMpTitlePortraitsImageMatches(image)) return FALSE;
    if(!base) {
        base=(uint8_t *)image; native_thread=GetCurrentThreadId();
        native_create=(Create)(base+CREATE); native_destroy=(Destroy)(base+DESTROY);
        native_wait=(Wait)(base+WAIT);
    }
    return TRUE;
}
static BOOL device_current(void *device) {
    return base && readable(base+TEXTURES_ACTIVE,1) && base[TEXTURES_ACTIVE]==1 &&
        readable(base+DEVICE,4) && device && pointer(base,DEVICE)==device;
}
static BOOL spawn_exact(void *world,uint32_t epoch,unsigned player,uint32_t generation,void *actor) {
    SudekiMpLanStoryAvatarSpawnObservation o;
    return world && epoch && player<4u && generation && actor &&
        SudekiMpLanStoryAvatarSpawnObserve(player,epoch,generation,&o) && o.ready && !o.unknown &&
        o.seat==player && o.epoch==epoch && o.generation==generation && o.world==world && o.actor==actor;
}
static BOOL avatar_exact(void *world,uint32_t epoch,unsigned player,uint32_t generation,
    void *actor,void *device) {
    return device_current(device) && readable(base+WORLD,4) && pointer(base,WORLD)==world &&
        spawn_exact(world,epoch,player,generation,actor);
}
static BOOL resident_exact(void) {
    const uint8_t *backend=portrait.backend;
    if(!readable(portrait.resident,8) || pointer(portrait.resident,0)!=base+RESIDENT_VT ||
        !backend || pointer(portrait.resident,4)!=backend || !readable(backend,0x3c)) return FALSE;
    void *group=pointer(backend,0xc);
    if(!group) return *(const uint16_t *)(backend+0x24)>0;
    if(!readable(group,0x10) || *(const uint32_t *)group==0) return FALSE;
    unsigned count=*(const uint32_t *)((uint8_t *)group+4);
    void *const *entries=pointer(group,0xc);
    if(!count || count>256 || !readable(entries,count*4u)) return FALSE;
    unsigned found=0;
    for(unsigned i=0;i<count;++i) if(entries[i]==backend) ++found;
    return found==1;
}
static BOOL pending_exact(void) {
    const uint8_t *token=portrait.pending;
    if(!readable(token,0x18) || pointer(token,0)!=base+TOKEN_VT ||
        pointer(token,0xc)!=&portrait.pending || pointer(token,8) || pointer(token,4)) return FALSE;
    const uint8_t *record=pointer(token,0x14),*job=pointer(token,0x10);
    return readable(record,0x1c) && pointer(record,0)==token &&
        pointer(record,4)==&portrait.pending && pointer(record,8)==portrait.resident &&
        !pointer(record,0xc) && (*(const uint32_t *)(record+0x14)&0x60000000u)==0x20000000u &&
        readable(job,0x14) && pointer(job,0x10)==portrait.backend;
}
static BOOL drain(void) {
    if(!portrait.pending) return TRUE;
    if(!resident_exact() || !pending_exact()) { portrait.unknown=TRUE; return FALSE; }
    native_wait(portrait.pending); /* Deleted token is never read again. */
    if(portrait.pending) { portrait.unknown=TRUE; return FALSE; }
    return TRUE;
}
static void *gpu(void *device) {
    if(!portrait.ready || portrait.unknown || portrait.pending || !resident_exact())
        return reject(0,"resident_owner");
    IDirect3DTexture9 *texture=pointer(portrait.backend,4);
    if(!readable(texture,4)) return reject(1,"backend_texture");
    void *const *vt=pointer(texture,0);
    if(!readable(vt,18*4u) || !executable(vt[3]) || !executable(vt[10]) || !executable(vt[17]))
        return reject(2,"texture_methods");
    if(IDirect3DTexture9_GetType(texture)!=D3DRTYPE_TEXTURE) return reject(3,"texture_type");
    void *const *expected_vt=readable(device,4)?pointer(device,0):NULL;
    if(!readable(expected_vt,3*4u) || !executable(expected_vt[2])) return reject(4,"device_release_method");
    IDirect3DDevice9 *actual=NULL; D3DSURFACE_DESC desc;
    if(FAILED(IDirect3DTexture9_GetDevice(texture,&actual)) || !actual) return reject(5,"device_query");
    BOOL same=actual==device;
    void *const *dv=readable(actual,4)?pointer(actual,0):NULL;
    if(!readable(dv,3*4u) || !executable(dv[2])) {
        portrait.unreleased_device=actual; portrait.unknown=TRUE; return reject(6,"queried_device_release_method");
    }
    IDirect3DDevice9_Release(actual);
    if(!same) return reject(7,"device_mismatch");
    if(FAILED(IDirect3DTexture9_GetLevelDesc(texture,0,&desc)) ||
        !desc.Width || !desc.Height || desc.Width>4096 || desc.Height>4096) return reject(8,"dimensions");
    if(!(portrait.reported&(1u<<31))) {
        portrait.reported|=1u<<31;
        SudekiMpLogFormat("avatar_portrait event=gpu_ready epoch=%lu width=%u height=%u\r\n",
            (unsigned long)portrait.epoch,desc.Width,desc.Height);
    }
    return texture;
}
BOOL SudekiMpLanStoryAvatarPortraitRetains(void) {
    return portrait.busy || portrait.resident || portrait.pending || portrait.unknown || portrait.unreleased_device;
}
BOOL SudekiMpLanStoryAvatarPortraitRelease(void) {
    if(!base) return TRUE;
    if(portrait.busy || native_thread!=GetCurrentThreadId()) return FALSE;
    if(SudekiMpLanStoryAvatarPortraitRetains() && (!device_current(portrait.device) ||
        !SudekiMpTitlePortraitsImageMatches((HMODULE)base))) return FALSE;
    portrait.busy=TRUE;
    if(portrait.unreleased_device) {
        void *const *vt=readable(portrait.unreleased_device,4)?pointer(portrait.unreleased_device,0):NULL;
        if(!readable(vt,3*4u) || !executable(vt[2])) { portrait.busy=FALSE; return FALSE; }
        IDirect3DDevice9_Release(portrait.unreleased_device);
        portrait.unreleased_device=NULL; portrait.unknown=FALSE;
    }
    if(!drain() || portrait.unknown || (portrait.resident && !resident_exact())) {
        portrait.busy=FALSE; return FALSE;
    }
    if(portrait.resident) native_destroy(portrait.resident,1);
    memset(&portrait,0,sizeof(portrait)); return TRUE;
}
BOOL SudekiMpLanStoryAvatarPortraitService(HMODULE image,void *world,uint32_t epoch,
    unsigned player,uint32_t generation,void *actor,void *device) {
    /* Observe proves the native game thread before first initialization may
     * latch it. A foreign first call cannot claim this instance's owner. */
    if(portrait.busy || (!base && !spawn_exact(world,epoch,player,generation,actor)) ||
        !initialize(image) || !avatar_exact(world,epoch,player,generation,actor,device)) return FALSE;
    if(portrait.world && (portrait.world!=world || portrait.epoch!=epoch || portrait.device!=device)) return FALSE;
    if(portrait.attempted) return portrait.ready && !portrait.unknown;
    portrait.world=world; portrait.epoch=epoch; portrait.device=device;
    portrait.attempted=TRUE; portrait.busy=TRUE;
    const TextureName name={UINT32_MAX,"SUI_PORTRAIT_BOSS_TALOSMERGED"};
    /* Native string-name loader appends .sqx. No world/title callback retained. */
    portrait.resident=native_create(&name,0x20,0x80,&portrait.pending,NULL,0);
    if(readable(portrait.resident,8) && pointer(portrait.resident,0)==base+RESIDENT_VT)
        portrait.backend=pointer(portrait.resident,4);
    if((portrait.resident && !resident_exact()) || !drain()) portrait.unknown=TRUE;
    portrait.ready=portrait.resident && !portrait.unknown && !portrait.pending;
    BOOL current=avatar_exact(world,epoch,player,generation,actor,device);
    if(current) (void)gpu(device);
    portrait.busy=FALSE;
    return current && portrait.ready && !portrait.unknown;
}
void *SudekiMpLanStoryAvatarPortraitResolve(HMODULE image,void *world,uint32_t epoch,
    unsigned player,uint32_t generation,void *actor,void *device) {
    if(portrait.busy || !base || base!=(uint8_t *)image || native_thread!=GetCurrentThreadId() ||
        portrait.world!=world || portrait.epoch!=epoch || portrait.device!=device ||
        !avatar_exact(world,epoch,player,generation,actor,device)) return NULL;
    return gpu(device);
}
