#define COBJMACROS
#include "hooks/title_portraits.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <d3d9.h>
#include <stdint.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Native title portraits require the supported x86 ABI"
#endif

enum { CREATE=0x1d92e0, DESTROY=0x1d6b30, WAIT=0x1d6c70,
    RESIDENT_VT=0x2dd80c, TOKEN_VT=0x2dd79c,
    TITLE_VT=0x2cb1fc, SCENE=0x408d1c, DEVICE=0x3c31dc,
    TEXTURES_ACTIVE=0x3c31cd, COUNT=5 };
typedef struct TextureName { uint32_t identifier; const char *name; } TextureName;
typedef void *(__attribute__((cdecl)) *Create)(const TextureName *,unsigned,
    unsigned,void **,void *,unsigned);
typedef void *(__attribute__((thiscall)) *Destroy)(void *,unsigned);
typedef void (__attribute__((thiscall)) *Wait)(void *);
typedef struct Portrait {
    void *resident,*backend,*pending;
    IDirect3DDevice9 *unreleased_device;
    BOOL attempted,unknown,ready;
    uint32_t reported_gpu;
} Portrait;
static const char *const names[COUNT]={
    "SUI_PORTRAIT_BUKI","SUI_PORTRAIT_ELCO",
    "SUI_PORTRAIT_TAL","SUI_PORTRAIT_AILISH",
    "SUI_PORTRAIT_BOSS_TALOSMERGED"
};
static uint8_t *base;
static void *title_owner,*title_scene,*title_device;
static DWORD native_thread;
static BOOL busy;
static Portrait portraits[COUNT];
static Create native_create;
static Destroy native_destroy;
static Wait native_wait;
static unsigned reported_rejections;

static BOOL reject(unsigned reason,const char *name) {
    if(reason<32u && !(reported_rejections&(1u<<reason))) {
        reported_rejections|=1u<<reason;
        SudekiMpLogFormat("title_portraits event=unavailable reason=%s retained=%u\r\n",
            name,SudekiMpTitlePortraitsRetains());
    }
    return FALSE;
}

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n ||
       VirtualQuery(p,&m,sizeof(m))!=sizeof(m) || m.State!=MEM_COMMIT ||
       (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
    DWORD access=m.Protect&0xffu;
    return (access==PAGE_READONLY || access==PAGE_READWRITE ||
        access==PAGE_WRITECOPY || access==PAGE_EXECUTE_READ ||
        access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY) &&
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
/* Whole native entry hashes, normalizing only verified HIGHLOW relocations.
 * DevPlayTitlePortraitReport proves the six-argument cdecl creation, native
 * request-list wait, NULL completion callback branch, and deleting destructor.
 * Hashes are signatures, not copies of the proprietary executable. */
static BOOL signature(const uint8_t *image,unsigned rva,unsigned n,uint32_t hash,
    const unsigned *relocations,unsigned count) {
    if(!readable(image+rva,n) || !executable(image+rva)) return FALSE;
    uint32_t actual=2166136261u,delta=(uint32_t)(uintptr_t)image-0x400000u;
    for(unsigned i=0;i<n;++i) {
        uint8_t b=image[rva+i];
        for(unsigned j=0;j<count;++j) if(i>=relocations[j] && i<relocations[j]+4u) {
            uint32_t value; memcpy(&value,image+rva+relocations[j],4);
            b=(uint8_t)((value-delta)>>((i-relocations[j])*8u)); break;
        }
        actual=(actual^b)*16777619u;
    }
    return actual==hash;
}
BOOL SudekiMpTitlePortraitsImageMatches(HMODULE image) {
    const uint8_t *b=(const uint8_t *)image;
    static const unsigned create_reloc[]={8,0x6c},ctor_reloc[]={2,0x21};
    static const unsigned destroy_reloc[]={0xa,0x18,0x2c};
    static const unsigned request_reloc[]={0x5d,0x7b,0x8d,0x92,0x99,0xa1,0xa9,0xb6};
    return b && readable(b,0x1000) && SudekiMpCheckLoadedExecutable(image) &&
        readable(b+RESIDENT_VT,4) && pointer(b,RESIDENT_VT)==b+DESTROY &&
        readable(b+TOKEN_VT,8) && pointer(b,TOKEN_VT+4)==b+WAIT &&
        /* The native string-name SQX loader appends this suffix itself. */
        readable(b+0x2d5708,7) && !memcmp(b+0x2d5708,"%s.sqx",7) &&
        signature(b,CREATE,0x1b5,0xd63207f7u,create_reloc,2) &&
        signature(b,0x1d6a90,0x66,0x9fd2b246u,ctor_reloc,2) &&
        signature(b,DESTROY,0x41,0x582588e8u,destroy_reloc,3) &&
        signature(b,WAIT,13,0x3d40fbb6u,NULL,0) &&
        signature(b,0x1d7000,0xc5,0x3571134fu,request_reloc,8) &&
        signature(b,0x1d7320,0x77,0xd05e4c45u,NULL,0) &&
        signature(b,0x15c0e0,0xc4,0xff32845au,NULL,0);
}
static BOOL initialize(HMODULE image) {
    if(base) return base==(uint8_t *)image && native_thread==GetCurrentThreadId() &&
        SudekiMpTitlePortraitsImageMatches(image);
    if(!SudekiMpTitlePortraitsImageMatches(image)) return FALSE;
    base=(uint8_t *)image; native_thread=GetCurrentThreadId();
    native_create=(Create)(base+CREATE); native_destroy=(Destroy)(base+DESTROY);
    native_wait=(Wait)(base+WAIT); return TRUE;
}
static BOOL device_current(void *device) {
    return base && readable(base+TEXTURES_ACTIVE,1) && base[TEXTURES_ACTIVE]==1 &&
        readable(base+DEVICE,4) && device && pointer(base,DEVICE)==device;
}
static BOOL tuple(void *owner,void *scene,void *device) {
    return device_current(device) && readable(owner,0x48) &&
        pointer(owner,0)==base+TITLE_VT && *(const uint32_t *)((uint8_t *)owner+0x44)==5 &&
        readable(base+SCENE,4) && pointer(base,SCENE)==scene &&
        readable(scene,0x174) && pointer(scene,0x170)==owner;
}
static BOOL resident_exact(const Portrait *p) {
    if(!readable(p->resident,8) || pointer(p->resident,0)!=base+RESIDENT_VT ||
        !p->backend || pointer(p->resident,4)!=p->backend || !readable(p->backend,0x3c)) return FALSE;
    const uint8_t *backend=p->backend;
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
static BOOL pending_exact(Portrait *p) {
    const uint8_t *token=p->pending;
    if(!readable(token,0x18) || pointer(token,0)!=base+TOKEN_VT ||
       pointer(token,0xc)!=&p->pending || pointer(token,8) || pointer(token,4)) return FALSE;
    const uint8_t *record=pointer(token,0x14),*job=pointer(token,0x10);
    return readable(record,0x1c) && pointer(record,0)==token &&
        pointer(record,4)==&p->pending && pointer(record,8)==p->resident &&
        pointer(record,0xc)==NULL &&
        (*(const uint32_t *)(record+0x14)&0x60000000u)==0x20000000u &&
        readable(job,0x14) && pointer(job,0x10)==p->backend;
}
static BOOL drain(Portrait *p) {
    if(!p->pending) return TRUE;
    if(!resident_exact(p) || !pending_exact(p)) { p->unknown=TRUE; return FALSE; }
    /* Native wait completes and deletes this token. Never read token again. */
    native_wait(p->pending);
    if(p->pending) { p->unknown=TRUE; return FALSE; }
    return TRUE;
}
static void *gpu_unavailable(Portrait *p,unsigned reason,const char *stage,uint32_t detail) {
    if(reason<32u && !(p->reported_gpu&(1u<<reason))) {
        p->reported_gpu|=1u<<reason;
        unsigned index=(unsigned)(p-portraits);
        SudekiMpLogFormat("title_portraits event=gpu_unavailable character=%u stage=%s detail=%08lx "
            "resident=%u pending=%u unknown=%u backend_flags=%08lx backend_file=%08lx\r\n",
            index==4?5:index,stage,(unsigned long)detail,p->ready,p->pending!=NULL,p->unknown,
            (unsigned long)(readable(p->backend,0x24)?*(const uint32_t *)((uint8_t *)p->backend+0x20):0),
            (unsigned long)(readable(p->backend,0x14)?*(const uint32_t *)((uint8_t *)p->backend+0x10):0));
    }
    return NULL;
}
static void *gpu(Portrait *p,void *device) {
    if(!p->ready || p->unknown || p->pending || !resident_exact(p))
        return gpu_unavailable(p,0,"resident_owner",0);
    IDirect3DTexture9 *texture=pointer(p->backend,4);
    if(!readable(texture,4)) return gpu_unavailable(p,1,"backend_texture",texture!=NULL);
    void *const *vt=pointer(texture,0);
    if(!readable(vt,18*4u) || !executable(vt[3]) || !executable(vt[10]) ||
        !executable(vt[17])) return gpu_unavailable(p,2,"texture_methods",0);
    D3DRESOURCETYPE type=IDirect3DTexture9_GetType(texture);
    if(type!=D3DRTYPE_TEXTURE) return gpu_unavailable(p,3,"texture_type",type);
    void *const *expected_vt=readable(device,4)?pointer(device,0):NULL;
    if(!readable(expected_vt,3*4u) || !executable(expected_vt[2]))
        return gpu_unavailable(p,4,"device_release_method",0);
    IDirect3DDevice9 *actual=NULL; D3DSURFACE_DESC desc;
    HRESULT result=IDirect3DTexture9_GetDevice(texture,&actual);
    if(FAILED(result) || !actual) return gpu_unavailable(p,5,"texture_device_query",(uint32_t)result);
    BOOL same=actual==device;
    /* GetDevice adds one COM reference. Validate and release precisely it. */
    void *const *dv=readable(actual,4)?pointer(actual,0):NULL;
    if(!readable(dv,3*4u) || !executable(dv[2])) {
        p->unreleased_device=actual; p->unknown=TRUE;
        return gpu_unavailable(p,6,"queried_device_release_method",0);
    }
    IDirect3DDevice9_Release(actual);
    if(!same) return gpu_unavailable(p,7,"texture_device_mismatch",0);
    result=IDirect3DTexture9_GetLevelDesc(texture,0,&desc);
    if(FAILED(result)) return gpu_unavailable(p,8,"level_description",(uint32_t)result);
    if(!desc.Width || !desc.Height || desc.Width>4096 || desc.Height>4096)
        return gpu_unavailable(p,9,"texture_dimensions",(desc.Width<<16)|(desc.Height&0xffffu));
    if(!(p->reported_gpu&(1u<<31))) {
        p->reported_gpu|=1u<<31;
        unsigned index=(unsigned)(p-portraits);
        SudekiMpLogFormat("title_portraits event=gpu_ready character=%u width=%u height=%u\r\n",
            index==4?5:index,desc.Width,desc.Height);
    }
    return texture;
}
BOOL SudekiMpTitlePortraitsRetains(void) {
    if(busy) return TRUE;
    for(unsigned i=0;i<COUNT;++i)
        if(portraits[i].resident || portraits[i].pending || portraits[i].unknown ||
            portraits[i].unreleased_device) return TRUE;
    return FALSE;
}
BOOL SudekiMpTitlePortraitsRelease(void) {
    if(!base) return TRUE;
    if(busy || native_thread!=GetCurrentThreadId()) return FALSE;
    if(SudekiMpTitlePortraitsRetains() && (!device_current(title_device) ||
        !SudekiMpTitlePortraitsImageMatches((HMODULE)base))) return FALSE;
    busy=TRUE;
    for(unsigned i=0;i<COUNT;++i) {
        Portrait *p=&portraits[i];
        if(p->unreleased_device) {
            void *const *vt=readable(p->unreleased_device,4)?pointer(p->unreleased_device,0):NULL;
            if(!readable(vt,3*4u) || !executable(vt[2])) { busy=FALSE; return FALSE; }
            IDirect3DDevice9_Release(p->unreleased_device); p->unreleased_device=NULL;
            p->unknown=FALSE;
        }
        if(!drain(p) || p->unknown || (p->resident && !resident_exact(p))) {
            busy=FALSE; return FALSE;
        }
        if(p->resident) { native_destroy(p->resident,1); p->resident=NULL; }
        memset(p,0,sizeof(*p));
    }
    title_owner=title_scene=title_device=NULL; busy=FALSE; return TRUE;
}
BOOL SudekiMpTitlePortraitsService(HMODULE image,void *owner,void *scene,void *device) {
    if(busy) return reject(0,"reentrant_service");
    if(!initialize(image)) return reject(1,"supported_image_or_native_thread");
    if(!device_current(device)) return reject(2,"native_texture_system_or_device");
    if(!tuple(owner,scene,device)) return reject(3,"title_scene_owner_tuple");
    if(title_owner && (title_owner!=owner || title_scene!=scene || title_device!=device))
        if(!SudekiMpTitlePortraitsRelease()) return FALSE;
    title_owner=owner; title_scene=scene; title_device=device;
    busy=TRUE;
    for(unsigned i=0;i<COUNT;++i) {
        Portrait *p=&portraits[i];
        if(p->unknown) { busy=FALSE; return FALSE; }
        if(p->attempted) continue;
        p->attempted=TRUE;
        const TextureName descriptor={UINT32_MAX,names[i]};
        /* Same flags as stock CycleIcon. A NULL native completion callback is
         * explicitly allowed; the returned resident wrapper is ours to retain.
         * 1F54D0 appends .sqx to this extensionless native resource name.
         * No mod callback or replacement vtable is passed into the engine. */
        p->resident=native_create(&descriptor,0x20,0x80,&p->pending,NULL,0);
        if(p->resident && readable(p->resident,8) && pointer(p->resident,0)==base+RESIDENT_VT)
            p->backend=pointer(p->resident,4);
        if((p->resident && !resident_exact(p)) || !drain(p)) p->unknown=TRUE;
        p->ready=p->resident && !p->unknown && !p->pending;
        void *texture=gpu(p,device);
        SudekiMpLogFormat("title_portraits event=request character=%u resource=%s resident=%u "
            "gpu=%u unknown=%u ownership=native_resident_wrapper\r\n",
            i==4?5:i,names[i],p->ready,texture!=NULL,p->unknown);
        if(p->unknown) { busy=FALSE; return FALSE; }
    }
    busy=FALSE; return TRUE;
}
void *SudekiMpTitlePortraitsResolve(HMODULE image,void *owner,void *scene,void *device,
    unsigned character) {
    unsigned index=character==5?4:character;
    if(busy || !base || base!=(uint8_t *)image || native_thread!=GetCurrentThreadId() ||
        index>=COUNT || character==4 || owner!=title_owner || scene!=title_scene ||
        device!=title_device || !tuple(owner,scene,device)) return NULL;
    return gpu(&portraits[index],device);
}
