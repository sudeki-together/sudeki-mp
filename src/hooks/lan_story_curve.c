#include "hooks/lan_story_curve.h"
#include "engine/build_identity.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

#if !defined(__i386__)
#error "Saved-story curve observations require the supported x86 layout"
#endif

enum { MAX_KEYS=65535u, MAX_COMPONENTS=4096u, MAX_KEY_BYTES=4u*1024u*1024u };
static uint8_t *verified_image;
static DWORD verified_thread;

static BOOL readable(const void *pointer,size_t size) {
    MEMORY_BASIC_INFORMATION m;
    uintptr_t p=(uintptr_t)pointer;
    if(!pointer || !size || p>UINTPTR_MAX-size ||
        VirtualQuery(pointer,&m,sizeof(m))!=sizeof(m) || m.State!=MEM_COMMIT ||
        (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
    DWORD access=m.Protect&0xffu;
    if(access!=PAGE_READONLY && access!=PAGE_READWRITE && access!=PAGE_WRITECOPY &&
        access!=PAGE_EXECUTE_READ && access!=PAGE_EXECUTE_READWRITE &&
        access!=PAGE_EXECUTE_WRITECOPY) return FALSE;
    return p+size<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static uint16_t u16(const void *p) { uint16_t v; memcpy(&v,p,2u); return v; }
static uint32_t u32(const void *p) { uint32_t v; memcpy(&v,p,4u); return v; }
static float f32(const void *p) { float v; memcpy(&v,p,4u); return v; }
static BOOL code_exact(uint8_t *image,unsigned rva,unsigned size,uint32_t expected,
    unsigned relocation) {
    if(!readable(image+rva,size)) return FALSE;
    uint32_t hash=2166136261u;
    for(unsigned i=0;i<size;++i) {
        uint8_t byte=image[rva+i];
        if(relocation && i>=relocation && i<relocation+4u) {
            uint32_t relocated=u32(image+rva+relocation);
            if(relocated!=(uintptr_t)image+0x2e3620u) return FALSE;
            byte=(uint8_t)(0x006e3620u>>((i-relocation)*8u));
        }
        hash=(hash^byte)*16777619u;
    }
    return hash==expected;
}
BOOL SudekiMpLanStoryCurveInitialize(HMODULE module) {
    uint8_t *image=(uint8_t *)module;
    if(!image || (verified_image && (verified_image!=image ||
        verified_thread!=GetCurrentThreadId())) || !SudekiMpCheckLoadedExecutable(module) ||
        !code_exact(image,0x1fb590u,333u,0x648335c2u,0u) ||
        !code_exact(image,0x1fae70u,575u,0x1a38399au,0x19au) ||
        !code_exact(image,0x1fb1c0u,257u,0x39cecdb4u,0u) ||
        !code_exact(image,0x214830u,189u,0xdcf9400au,0u) ||
        !code_exact(image,0x22b660u,177u,0xf5dbd541u,0u) ||
        !code_exact(image,0x22b720u,384u,0x70d3f8c4u,0u) ||
        !code_exact(image,0x22b8a0u,153u,0xc6ea6e7cu,0u)) {
        SetLastError(ERROR_BAD_EXE_FORMAT); return FALSE;
    }
    verified_image=image; verified_thread=GetCurrentThreadId();
    SetLastError(ERROR_SUCCESS); return TRUE;
}
static BOOL key_values(const uint8_t *key,unsigned components,BOOL indirect) {
    if(indirect) {
        /* key_rows already proved the whole row array, including this pointer
         * slot. Its pointee still needs its own fresh range proof. */
        key=(const uint8_t *)(uintptr_t)u32(key);
        if(!readable(key,components*4u)) return FALSE;
    }
    /* Inline values are inside key_rows' checked span. No native callback or
     * allocation occurs during this read-only traversal. Check every value,
     * without repeating VirtualQuery for every inline animation key. */
    for(unsigned i=0;i<components;++i) if(!isfinite(f32(key+i*4u))) return FALSE;
    return TRUE;
}
static BOOL key_rows(const uint8_t *data,unsigned count,unsigned stride,
    unsigned components,BOOL indirect,BOOL explicit_time) {
    size_t bytes=(size_t)count*stride;
    size_t row_bytes=(explicit_time?4u:0u)+(indirect?4u:(size_t)components*4u);
    if(!count || !components || components>MAX_COMPONENTS || row_bytes>stride ||
        bytes>MAX_KEY_BYTES || !readable(data,bytes)) return FALSE;
    float previous=0.0f;
    for(unsigned i=0;i<count;++i) {
        const uint8_t *key=data+(size_t)i*stride;
        if(explicit_time) {
            float time=f32(key);
            if(!isfinite(time) || fabsf(time)>1000000.0f ||
                (i && time<=previous)) return FALSE;
            previous=time; key+=4u;
        }
        if(!key_values(key,components,indirect)) return FALSE;
    }
    return TRUE;
}
static BOOL segmented(const uint8_t *header,const uint8_t *data,unsigned keys,
    unsigned stride,unsigned components,BOOL indirect) {
    if(!readable(data,4u)) return FALSE;
    unsigned segments=u32(data);
    if(!segments || segments>keys || segments>MAX_KEYS) return FALSE;
    size_t prefix=4u+(size_t)segments*8u,bytes=(size_t)keys*stride;
    if(prefix>MAX_KEY_BYTES || bytes>MAX_KEY_BYTES-prefix ||
        !readable(data,prefix+bytes)) return FALSE;
    uintptr_t lower=(uintptr_t)data+prefix,upper=lower+bytes;
    float previous_time=0.0f,previous_end=0.0f;
    for(unsigned i=0;i<segments;++i) {
        const uint8_t *segment=data+4u+i*8u;
        unsigned descriptor=u16(segment),count=descriptor&0x1ffu;
        uintptr_t start=(uintptr_t)segment+u16(segment+2u);
        size_t span=(size_t)count*stride;
        float time=f32(segment+4u),scale=1.0f;
        if(!count || count>keys || !isfinite(time) || fabsf(time)>1000000.0f ||
            start<lower || start>upper || span>upper-start) return FALSE;
        if(!(descriptor&0x8000u)) {
            unsigned back=4u+4u*((descriptor>>10u)&31u);
            uintptr_t h=(uintptr_t)header;
            if(h<back || !readable((const void *)(h-back),4u)) return FALSE;
            scale=f32((const void *)(h-back));
            if(!isfinite(scale) || scale<=0.0f || scale>1000000.0f) return FALSE;
        }
        float end=time+(float)(count-1u)*scale;
        if(!isfinite(end) || (i && (time<=previous_time || time<previous_end)) ||
            !key_rows((const uint8_t *)start,count,stride,components,indirect,FALSE)) return FALSE;
        previous_time=time; previous_end=end;
    }
    return TRUE;
}
BOOL SudekiMpLanStoryCurveExact(HMODULE image,const void *pointer,
    SudekiMpLanStoryCurveKind kind,unsigned output_floats) {
    const uint8_t *curve=pointer,*header,*data;
    unsigned table,sampler,components,header_size;
    if(!verified_image || image!=(HMODULE)verified_image ||
        verified_thread!=GetCurrentThreadId() || !readable(curve,8u) ||
        !output_floats || output_floats>MAX_COMPONENTS) return FALSE;
    if(kind==SUDEKIMP_STORY_CURVE_WEIGHTS) {
        table=0x2de748u; sampler=0x1fb590u; components=output_floats; header_size=12u;
    } else if(kind==SUDEKIMP_STORY_CURVE_UV_MATRIX && output_floats==16u) {
        table=0x2de6ecu; sampler=0x1fae70u; components=5u; header_size=8u;
    } else if(kind==SUDEKIMP_STORY_CURVE_SCALAR && output_floats==1u) {
        table=0x2df2a8u; sampler=0x214830u; components=1u; header_size=8u;
    } else if(kind==SUDEKIMP_STORY_CURVE_COLOR && output_floats==4u) {
        table=0x2de70cu; sampler=0x1fb1c0u; components=4u; header_size=8u;
    } else return FALSE;
    if(*(void *const *)curve!=verified_image+table ||
        !readable(verified_image+table,8u) ||
        *(void **)(verified_image+table+4u)!=verified_image+sampler) return FALSE;
    header=*(const uint8_t *const *)(curve+4u);
    if(!readable(header,header_size)) return FALSE;
    unsigned keys=u16(header),flags=u16(header+2u);
    if(!keys || keys>MAX_KEYS ||
        (kind==SUDEKIMP_STORY_CURVE_WEIGHTS && u32(header+8u)!=components)) return FALSE;
    BOOL indirect=(flags&0x4000u)!=0;
    unsigned stride=indirect?4u:components*4u;
    data=header+header_size;
    BOOL result;
    if(flags&0x2000u) {
        if(flags&0x8000u) result=segmented(header,data,keys,stride,components,indirect);
        else result=key_rows(data,keys,stride+4u,components,indirect,TRUE);
    } else if(flags&0x8000u) {
        result=key_rows(data,keys,stride,components,indirect,FALSE);
    } else {
        if(!readable(data,4u) || !isfinite(f32(data)) || fabsf(f32(data))>1000000.0f) return FALSE;
        result=key_rows(data+4u,keys,stride,components,indirect,FALSE);
    }
    /* No authorization is retained across calls; reject a changed native
     * class or header after the bounded source observation. */
    return result && *(void *const *)curve==verified_image+table &&
        *(const uint8_t *const *)(curve+4u)==header;
}
