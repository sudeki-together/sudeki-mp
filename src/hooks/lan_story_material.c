#include "hooks/lan_story_material.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

#if !defined(__i386__)
#error "Saved-story material observations require the supported x86 layout"
#endif

enum { MAX_MATERIALS=128u, MAX_TEXTURES=8u, MAX_BACKINGS=1024u, MAX_JOB_NODES=256u };
typedef struct CodeIdentity { unsigned rva,size; uint32_t hash; } CodeIdentity;
typedef struct Relocation { unsigned rva,target; } Relocation;
static const CodeIdentity codes[]={
    /* ShaderConstantEnsure */
    {0x2032c0u,457u,0x551d4f50u},
    /* ShaderDefinitionLookup */
    {0x1e76d0u,86u,0x83d40160u},
    /* SolDefinitionFactory */
    {0x202a30u,501u,0xa3c8b529u},
    /* SolDefinitionDelete */
    {0x2014b0u,90u,0xf6ae8a78u},
    /* DefinitionUnlink */
    {0x1e4080u,75u,0xc92f74eau},
    /* BaseDefinitionFactory */
    {0x1fb7c0u,147u,0x0f6e1acbu},
    /* WaterDefinitionFactory */
    {0x216630u,42u,0x7b180985u},
    /* TextureCount */
    {0x201670u,83u,0xe717bf1eu},
    /* ConstantCount */
    {0x2016d0u,86u,0x401493b2u},
    /* TextureMapping */
    {0x201730u,148u,0xfb53df6fu},
    /* ConstantMapping */
    {0x2017d0u,83u,0x27627461u},
    {0x203910u,54u,0xadcad836u},
    {0x1e79d0u,253u,0x071e809eu},
    {0x202cb0u,81u,0xebb3098fu},
    {0x202c80u,36u,0x1b0db763u},
    {0x1fb750u,64u,0xc579f9f7u},
    {0x1fb790u,36u,0xffec1deau},
    {0x1e43f0u,1022u,0x24ed04c4u},
    {0x1e47f0u,173u,0xcc9834c0u},
    {0x1e6db0u,275u,0x8e163397u},
    {0x1d6a90u,225u,0xa533d513u},
    {0x1d9630u,464u,0xfc75536eu},
    {0x1d9800u,311u,0xc06f1f9fu},
    {0x1d9af0u,140u,0xe9d0000bu},
    {0x1d6b80u,48u,0xfd11326bu},
    {0x1d6bb0u,42u,0x6cb1539au},
    {0x1d6c80u,112u,0xc3d4e618u},
    {0x1d6d10u,191u,0x456ab716u},
    {0x1d6dd0u,151u,0x06fc6e17u},
    {0x1d6e70u,93u,0xafd376c4u},
    {0x1d6ed0u,296u,0xd353e1a4u},
    {0x1d9c20u,95u,0xf236be23u},
    {0x206ce0u,52u,0x1172e151u},
    {0x215e70u,24u,0xa68bb247u},
    {0x215ec0u,31u,0x37ddc37au},
    {0x2038c0u,36u,0x4ee71e45u},
    {0x202e40u,240u,0x72ae6836u},
    /* ShaderConstantAbsent: member dispatch, pure early return, jump table.
     * Replacement also requires the closed SOL proof below. */
    {0x2032c0u,69u,0x38ff63ddu},
    {0x20347fu,10u,0x3d201f33u},
    {0x20348cu,16u,0xe40a00f2u},
    {0x1e7380u,448u,0xd8a0258bu},
    {0x1e6ed0u,220u,0xd5cf2b24u},
    {0x206ad0u,74u,0x5b126e41u},
    {0x1fbd90u,81u,0x2043f2a1u},
    {0x21f3a0u,104u,0xc63976d6u},
    {0x21f8c0u,488u,0x29664ac1u},
    {0x3b870u,3u,0xcb8bb955u},
    {0x2249e0u,138u,0x37e15460u},
    {0x224130u,9u,0x51693035u},
    {0x1fc3c0u,9u,0x8f57ce69u},
    {0x1fa0b0u,9u,0xc84f4dfdu},
    {0x1fa0c0u,55u,0x3c7cddaau},
    {0x1fa100u,75u,0x3528ae39u},
};
static const Relocation relocations[]={
    {0x1e409eu,0x36b8b0u},
    {0x1e40c6u,0x36b8b0u},
    {0x1e76eeu,0x36b8b0u},
    {0x1e7705u,0x3c3654u},
    {0x1fb7eau,0x34fbd8u},
    {0x1fb805u,0x34fbb0u},
    {0x1fb820u,0x34fbc4u},
    {0x1fb842u,0x34fbecu},
    {0x2014b9u,0x2deb70u},
    {0x2014d0u,0x2ddb64u},
    {0x202b62u,0x323b40u},
    {0x202b72u,0x323ad0u},
    {0x202bcdu,0x2ddb64u},
    {0x202be4u,0x36b8b0u},
    {0x202beeu,0x36b8b0u},
    {0x202bf4u,0x2deb70u},
    {0x216649u,0x350058u},
    {0x2032d3u,0x20348cu},
    {0x20348cu,0x2032d7u},
    {0x203490u,0x2032dbu},
    {0x203494u,0x2032e2u},
    {0x203498u,0x2032e9u},
    {0x202ce1u,0x2deb7cu},
    {0x202c85u,0x2deb7cu},
    {0x1fb770u,0x2de864u},
    {0x1fb795u,0x2de864u},
    {0x1e4400u,0x2ddafcu},
    {0x1e442fu,0x3c3650u},
    {0x1e4444u,0x3c3650u},
    {0x1e4690u,0x3c37a4u},
    {0x1e47fbu,0x2ddafcu},
    {0x1e4897u,0x2dd6d8u},
    {0x1d6a92u,0x2dd7bcu},
    {0x1d6ab1u,0x2dd80cu},
    {0x1d6b3au,0x2dd80cu},
    {0x1d6b48u,0x2dd7bcu},
    {0x1d6b5cu,0x2dd75cu},
    {0x1d968bu,0x3c3164u},
    {0x1d96b3u,0x3c3164u},
    {0x1d96d3u,0x364000u},
    {0x1d96dbu,0x29a068u},
    {0x1d9702u,0x3c3164u},
    {0x1d972eu,0x3c3164u},
    {0x1d9754u,0x364000u},
    {0x1d975au,0x29a064u},
    {0x1d9767u,0x364000u},
    {0x1d976du,0x29a064u},
    {0x1d9788u,0x364000u},
    {0x1d978eu,0x29a068u},
    {0x1d97ebu,0x364000u},
    {0x1d97f1u,0x29a064u},
    {0x1d982du,0x404160u},
    {0x1d9854u,0x404140u},
    {0x1d985eu,0x404180u},
    {0x1d98acu,0x29a09cu},
    {0x1d98bdu,0x29a064u},
    {0x1d98c3u,0x3c31afu},
    {0x1d98eau,0x404140u},
    {0x1d98f1u,0x404180u},
    {0x1d98f9u,0x4041c0u},
    {0x1d9900u,0x404120u},
    {0x1d991du,0x29a064u},
    {0x1d9afau,0x29a068u},
    {0x1d9affu,0x364148u},
    {0x1d9b07u,0x29a064u},
    {0x1d9b16u,0x363ee8u},
    {0x1d9b2fu,0x364148u},
    {0x1d9b39u,0x364148u},
    {0x1d9b40u,0x364148u},
    {0x1d6b82u,0x364148u},
    {0x1d6b8au,0x29a068u},
    {0x1d6b92u,0x364148u},
    {0x1d6b98u,0x29a064u},
    {0x1d6c8au,0x2dd7acu},
    {0x1d6cc3u,0x364148u},
    {0x1d6cccu,0x29a068u},
    {0x1d6cd3u,0x363ee8u},
    {0x1d6cdbu,0x364148u},
    {0x1d6ce2u,0x363ee8u},
    {0x1d6ce8u,0x29a064u},
    {0x1d6d1au,0x2dd7acu},
    {0x1d6d32u,0x3c3168u},
    {0x1d6d4bu,0x3c316cu},
    {0x1d6dbcu,0x2de234u},
    {0x1d6de2u,0x364148u},
    {0x1d6de8u,0x29a068u},
    {0x1d6e1au,0x29a064u},
    {0x1d6e28u,0x364148u},
    {0x1d6e2eu,0x29a064u},
    {0x1d6e38u,0x3c31afu},
    {0x1d6e46u,0x29a09cu},
    {0x1d6e78u,0x364148u},
    {0x1d6e81u,0x29a068u},
    {0x1d6e88u,0x363ee8u},
    {0x1d6ea7u,0x364148u},
    {0x1d6eb0u,0x29a064u},
    {0x1d6ebau,0x364148u},
    {0x1d6ec1u,0x363ee8u},
    {0x1d6ec7u,0x29a064u},
    {0x1d6ed7u,0x29a068u},
    {0x1d6eedu,0x364148u},
    {0x1d6efeu,0x363ee8u},
    {0x1d6f15u,0x364148u},
    {0x1d6f1fu,0x364148u},
    {0x1d6f25u,0x29a064u},
    {0x1d6f60u,0x364148u},
    {0x1d6f6au,0x364148u},
    {0x1d6f70u,0x29a064u},
    {0x1d6f8bu,0x404140u},
    {0x1d6fa0u,0x3c3168u},
    {0x1d6fa7u,0x404180u},
    {0x1d6fbcu,0x3c316cu},
    {0x1d6fc7u,0x29a064u},
    {0x1d6fccu,0x364148u},
    {0x1d9c22u,0x3c3137u},
    {0x1d9c2au,0x29a060u},
    {0x1d9c31u,0x364148u},
    {0x1d9c39u,0x3c3137u},
    {0x1d9c3fu,0x363fe8u},
    {0x1d9c46u,0x364018u},
    {0x1d9c4du,0x364000u},
    {0x1d9c59u,0x364160u},
    {0x1d9c68u,0x3c31d8u},
    {0x1d9c71u,0x1d9e90u},
    {0x1d9c79u,0x3c31d8u},
    {0x206cffu,0x2dee3cu},
    {0x215e80u,0x2df67cu},
    {0x215ecau,0x2dee1cu},
    {0x1e74f9u,0x34eeb0u},
    {0x1e74ffu,0x2dee3cu},
    {0x1e6ee8u,0x1e6f9cu},
    {0x1e6f9cu,0x1e6eecu},
    {0x1e6fa0u,0x1e6f0au},
    {0x1e6fa4u,0x1e6f1fu},
    {0x1e6fa8u,0x1e6f5du},
    {0x21f91eu,0x3c377cu},
    {0x21f925u,0x32367cu},
    {0x21f93eu,0x32367cu},
    {0x21f94au,0x3c3780u},
    {0x21f955u,0x32367cu},
    {0x21f970u,0x3c377cu},
    {0x21f975u,0x32367cu},
    {0x21f983u,0x3c3780u},
    {0x21f989u,0x3c3780u},
    {0x21fa22u,0x3c3780u},
    {0x21fa2eu,0x3c377cu},
    {0x21fa3du,0x3c3780u},
    {0x21fa7eu,0x3c377cu},
};
static uint8_t *verified_image;
static DWORD verified_thread;
static const char *failure_stage="none",*last_failure_stage;
static unsigned failure_detail,last_failure_detail,failure_logs;
static const void *last_failure_child;
/* Reuse only OS range/protection answers inside one read-only owner walk.
 * Never cache graph validity, identities, values or a successful inspection.
 * The game-owned graph cannot allocate/free/reprotect on this thread during
 * this walk. Resource-worker observations start a fresh scope under its lock. */
enum { MATERIAL_REGIONS=32u };
typedef struct MaterialRegion { uintptr_t lower,upper; DWORD access; } MaterialRegion;
static MaterialRegion memory_regions[MATERIAL_REGIONS];
static unsigned memory_region_count,memory_region_next;
static BOOL memory_scope;
static void memory_reset(void) { memory_region_count=memory_region_next=0u; }
static void observe(const char *stage,unsigned detail) {
    failure_stage=stage; failure_detail=detail;
}
static BOOL memory(const void *pointer,size_t size,BOOL write) {
    uintptr_t p=(uintptr_t)pointer; DWORD access=0; BOOL known=FALSE;
    if(!pointer || !size || p>UINTPTR_MAX-size) return FALSE;
    if(memory_scope) for(unsigned i=0;i<memory_region_count;++i)
        if(p>=memory_regions[i].lower && p+size<=memory_regions[i].upper) {
            access=memory_regions[i].access; known=TRUE; break;
        }
    if(!known) {
        MEMORY_BASIC_INFORMATION m;
        if(VirtualQuery(pointer,&m,sizeof(m))!=sizeof(m) || m.State!=MEM_COMMIT ||
            (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
            p+size>(uintptr_t)m.BaseAddress+m.RegionSize) return FALSE;
        access=m.Protect&0xffu;
        if(memory_scope) {
            unsigned i=memory_region_count<MATERIAL_REGIONS?memory_region_count++:memory_region_next;
            memory_region_next=(memory_region_next+1u)%MATERIAL_REGIONS;
            memory_regions[i]=(MaterialRegion){(uintptr_t)m.BaseAddress,
                (uintptr_t)m.BaseAddress+m.RegionSize,access};
        }
    }
    if(write) {
        if(access!=PAGE_READWRITE && access!=PAGE_WRITECOPY &&
            access!=PAGE_EXECUTE_READWRITE && access!=PAGE_EXECUTE_WRITECOPY) return FALSE;
    } else if(access!=PAGE_READONLY && access!=PAGE_READWRITE && access!=PAGE_WRITECOPY &&
        access!=PAGE_EXECUTE_READ && access!=PAGE_EXECUTE_READWRITE &&
        access!=PAGE_EXECUTE_WRITECOPY) return FALSE;
    return TRUE;
}
static BOOL readable(const void *p,size_t n) { return memory(p,n,FALSE); }
static BOOL writable(const void *p,size_t n) { return memory(p,n,TRUE); }
static unsigned word(const void *p) { uint16_t v; memcpy(&v,p,2u); return v; }
static BOOL code_exact(uint8_t *image,const CodeIdentity *code) {
    if(!readable(image+code->rva,code->size)) return FALSE;
    uint32_t hash=2166136261u;
    for(unsigned i=0;i<code->size;++i) {
        uint8_t byte=image[code->rva+i];
        for(unsigned r=0;r<sizeof(relocations)/sizeof(relocations[0]);++r) {
            unsigned offset=code->rva+i-relocations[r].rva;
            if(offset>=4u) continue;
            uint32_t value; memcpy(&value,image+relocations[r].rva,4u);
            if(value!=(uintptr_t)image+relocations[r].target) return FALSE;
            byte=(uint8_t)((0x400000u+relocations[r].target)>>(offset*8u)); break;
        }
        hash=(hash^byte)*16777619u;
    }
    return hash==code->hash;
}
BOOL SudekiMpLanStoryMaterialInitialize(HMODULE module) {
    uint8_t *image=(uint8_t *)module;
    if(!image || (verified_image && (image!=verified_image ||
        verified_thread!=GetCurrentThreadId())) || !SudekiMpCheckLoadedExecutable(module)) return FALSE;
    for(unsigned i=0;i<sizeof(codes)/sizeof(codes[0]);++i)
        if(!code_exact(image,&codes[i])) { SetLastError(ERROR_BAD_EXE_FORMAT); return FALSE; }
    verified_image=image; verified_thread=GetCurrentThreadId();
    SetLastError(ERROR_SUCCESS); return TRUE;
}
static BOOL slot(unsigned table,unsigned offset,unsigned method) {
    return readable(verified_image+table+offset,4u) &&
        *(void **)(verified_image+table+offset)==verified_image+method;
}
/* Closed SOL color/texture family used by Tal's material animation. Texture0
 * and authored texture fields stay native; zero fields for members3..5 may
 * each add one float4, in any order. Prove the whole
 * reachable definition family before native evaluation, including a later
 * output in the same pose. Other flag families still need their own proof. */
static uint32_t sol_color_flags(uint32_t original,unsigned mask) {
    static const unsigned shifts[3]={2u,6u,8u};
    for(unsigned i=0;i<3u;++i)
        if((mask&(1u<<i)) && !((original>>shifts[i])&3u)) original|=3u<<shifts[i];
    return original;
}
static BOOL sol_colors(uint32_t flags,unsigned *count) {
    /* The non-color fields at bits4/10 are retained authored textures.
     * Bit23 selects the factory's second static layout, never an owned array.
     * Authored bit13 is opaque here: the native factory preserves the key
     * without changing its arrays. Derivation must retain it exactly. */
    if((flags&~0x802ffcu)!=0x80000001u ||
        ((flags>>4u)&3u)>1u || ((flags>>10u)&3u)>1u) return FALSE;
    *count=0;
    static const unsigned shifts[3]={2u,6u,8u};
    for(unsigned i=0;i<3u;++i) {
        unsigned field=(flags>>shifts[i])&3u;
        if(field==2u) return FALSE;
        *count+=field==3u;
    }
    return TRUE;
}
static BOOL sol_cache(uint32_t flags,uint8_t **found) {
    unsigned bucket=(flags+(flags>>8)+(flags>>16)+(flags>>24))&255u;
    uint8_t **head=(uint8_t **)(verified_image+0x36b8b0u+bucket*4u);
    uint8_t *seen[256]; unsigned count=0;
    if(!writable(head,4u)) return FALSE;
    *found=NULL;
    for(uint8_t *p=*head;p;p=*(uint8_t **)(p+0x24u)) {
        if(count==256u || !writable(p,0x28u) ||
            (*(void **)p!=verified_image+0x2ddb64u && *(void **)p!=verified_image+0x2deb70u)) return FALSE;
        for(unsigned i=0;i<count;++i) if(seen[i]==p) return FALSE;
        seen[count++]=p;
        uint32_t key=*(uint32_t *)(p+8u);
        if(((key+(key>>8)+(key>>16)+(key>>24))&255u)!=bucket) return FALSE;
        if(key==flags) { if(*found) return FALSE; *found=p; }
    }
    return TRUE;
}
static BOOL sol_definition(uint8_t *d,BOOL unreferenced) {
    uint8_t *cached=NULL;
    if(!writable(d,0x30u) || *(void **)d!=verified_image+0x2deb70u ||
        !slot(0x2deb70u,0u,0x2014b0u)) return FALSE;
    uint32_t flags=*(uint32_t *)(d+8u),refs=*(uint32_t *)(d+4u); unsigned colors;
    if(!sol_colors(flags,&colors) || (!unreferenced && !refs) ||
        refs>UINT32_MAX-MAX_MATERIALS ||
        *(uint32_t *)(d+0x10u)!=colors ||
        *(uint32_t *)(d+0x14u)!=9u ||
        *(void **)(d+0x18u)!=verified_image+((flags&0x800000u)?0x323b40u:0x323ad0u) ||
        !sol_cache(flags,&cached) || cached!=d) return FALSE;
    uint32_t texture[6]; unsigned textures=0;
    for(unsigned i=0;i<6u;++i) texture[i]=((flags>>(i*2u))&3u)==1u?textures++:UINT32_MAX;
    if(*(uint32_t *)(d+0xcu)!=textures) return FALSE;
    uint32_t constant[4]={UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};
    unsigned slot_index=0; static const unsigned shifts[3]={2u,6u,8u};
    for(unsigned i=0;i<3u;++i) if(((flags>>shifts[i])&3u)==3u) constant[i+1u]=slot_index++;
    unsigned second=(flags>>6u)&3u,third=(flags>>8u)&3u;
    const uint32_t descriptor[7]={0x103u+((flags&0xcu)==4u?0x100u:0u),
        second==1u?0x103u:second==3u?2u:0u,third==1u?0x103u:third==3u?2u:0u,
        (flags&0x400u)?0x103u:1u,(flags&0x10u)?0x103u:0u,
        (flags&0x10u)?0x103u:0u,(flags&0x10u)?0x103u:0u};
    void *t=*(void **)(d+0x28u),*c=*(void **)(d+0x2cu),*v=*(void **)(d+0x1cu);
    void *optional=*(void **)(d+0x20u);
    if(flags&0x10u) {
        uint32_t second_descriptor[7]; memcpy(second_descriptor,descriptor,sizeof(descriptor));
        second_descriptor[2]=second_descriptor[3]=1u;
        second_descriptor[4]=second_descriptor[5]=second_descriptor[6]=0u;
        if(!writable(optional,sizeof(second_descriptor)) || optional==t || optional==c || optional==v ||
            memcmp(optional,second_descriptor,sizeof(second_descriptor))) return FALSE;
    } else if(optional) return FALSE;
    return writable(t,sizeof(texture)) && writable(c,sizeof(constant)) && writable(v,sizeof(descriptor)) &&
        t!=c && t!=v && c!=v && !memcmp(t,texture,sizeof(texture)) &&
        !memcmp(c,constant,sizeof(constant)) && !memcmp(v,descriptor,sizeof(descriptor));
}
static BOOL sol_factories(void) {
    uint8_t *p=*(uint8_t **)(verified_image+0x3c3654u);
    unsigned seen=0;
    /* Lookup stops at SOL's successful factory. Each earlier native factory
     * rejects this flag family without allocation or callback. */
    for(unsigned i=0;p && i<3u;++i) {
        unsigned bit,table,method;
        if(p==verified_image+0x34fc00u) {bit=1;table=0x2de8ccu;method=0x1fb7c0u;}
        else if(p==verified_image+0x34fcb0u) {bit=2;table=0x2debe4u;method=0x202a30u;}
        else if(p==verified_image+0x350040u) {bit=4;table=0x2df76cu;method=0x216630u;}
        else return FALSE;
        if((seen&bit) || !readable(p,8u) || *(void **)p!=verified_image+table || !slot(table,0u,method)) return FALSE;
        seen|=bit;
        if(bit==2) return TRUE;
        p=*(uint8_t **)(p+4u);
    }
    return FALSE;
}
static BOOL sol_arrays_distinct(uint8_t *value,uint8_t *original) {
    /* Counted native definitions separately own these heap arrays. */
    for(unsigned i=0;i<4u;++i) for(unsigned j=0;j<4u;++j) {
        static const unsigned offsets[4]={0x1cu,0x20u,0x28u,0x2cu};
        void *a=*(void **)(value+offsets[i]),*b=*(void **)(original+offsets[j]);
        if(a && a==b) return FALSE;
    }
    return TRUE;
}
static BOOL sol_derived(uint8_t *value,uint8_t *original) {
    if(!sol_definition(original,FALSE) || !sol_definition(value,FALSE) ||
        *(uint32_t *)(original+0x10u)!=0u || !sol_arrays_distinct(value,original)) return FALSE;
    uint32_t old=*(uint32_t *)(original+8u),next=*(uint32_t *)(value+8u);
    if((old&~0x3ccu)!=(next&~0x3ccu)) return FALSE;
    /* Native can add a constant only to an absent field. Authored textures
     * must remain textures in the same slots, not merely keep their count. */
    static const unsigned shifts[3]={2u,6u,8u};
    for(unsigned i=0;i<3u;++i) {
        unsigned a=(old>>shifts[i])&3u,b=(next>>shifts[i])&3u;
        if(a==1u?b!=1u:(b!=0u && b!=3u)) return FALSE;
    }
    return old!=next;
}
static BOOL sol_expansion_closure(uint32_t original) {
    uint8_t *definitions[8]={0}; BOOL needs_factory=FALSE;
    uint32_t flags[8]; unsigned count=0;
    for(unsigned i=0;i<8u;++i) {
        uint32_t next=sol_color_flags(original,i); BOOL seen=FALSE;
        for(unsigned j=0;j<count;++j) if(flags[j]==next) seen=TRUE;
        if(seen) continue;
        unsigned at=count++; flags[at]=next;
        if(!sol_cache(next,&definitions[at])) return FALSE;
        if(!definitions[at]) { needs_factory=TRUE; continue; }
        if(!sol_definition(definitions[at],TRUE)) return FALSE;
        for(unsigned j=0;j<at;++j) if(definitions[j] &&
            !sol_arrays_distinct(definitions[at],definitions[j])) return FALSE;
    }
    return !needs_factory || sol_factories();
}
BOOL SudekiMpLanStoryMaterialShaderOutputExact(HMODULE image,const void *pointer,
    const void *original_pointer,unsigned member) {
    if(!verified_image || image!=(HMODULE)verified_image ||
        verified_thread!=GetCurrentThreadId() || memory_scope || member<3u || member>5u) return FALSE;
    memory_reset(); memory_scope=TRUE;
    uint8_t *value=(uint8_t *)pointer,*original=(uint8_t *)original_pointer;
    BOOL result=FALSE;
    if(!writable(value,0x40u) || !readable(original,0x40u) ||
        *(void **)value!=verified_image+0x2deb7cu || *(void **)original!=verified_image+0x2deb7cu) goto done;
    uint8_t *d=*(uint8_t **)(value+4u),*o=*(uint8_t **)(original+4u);
    if(!sol_definition(o,FALSE) || !sol_definition(d,FALSE) ||
        *(uint32_t *)(o+0x10u)!=0u || (d!=o && !sol_derived(d,o))) goto done;
    if(!*(uint32_t *)(d+0x10u)) {
        if(*(void **)(value+0x38u)) goto done;
    } else if((*(uint32_t *)(value+0xcu)&0x200u) || !writable(*(void **)(value+0x38u),*(uint32_t *)(d+0x10u)*16u)) goto done;
    if(!sol_expansion_closure(*(uint32_t *)(o+8u))) goto done;
    result=TRUE;
done:
    memory_scope=FALSE; memory_reset(); return result;
}

typedef struct Backing {
    uint8_t *pointer;
    unsigned additions,retirements;
} Backing;
typedef struct Definition {
    uint8_t *pointer;
    unsigned additions,retirements;
} Definition;
typedef struct Proof {
    Backing backing[MAX_BACKINGS]; unsigned backing_count;
    Definition definitions[MAX_MATERIALS]; unsigned definition_count;
    void *original_textures[MAX_BACKINGS],*retired_textures[MAX_BACKINGS];
    void *original_uv[MAX_BACKINGS],*retired_uv[MAX_BACKINGS];
    unsigned original_texture_count,retired_texture_count,original_uv_count,retired_uv_count;
} Proof;
static unsigned resource_bucket(const void *resource) {
    return ((uintptr_t)resource>>6u)&31u;
}
static DWORD backings_ready_locked(const Proof *proof) {
    uint32_t buckets=0;
    for(unsigned i=0;i<proof->backing_count;++i)
        buckets|=1u<<resource_bucket(proof->backing[i].pointer);
    uint8_t *const *heads=(uint8_t *const *)(verified_image+0x363ee8u);
    observe("resource_job_table",0);
    if(!readable(heads,32u*4u)) return ERROR_INVALID_DATA;
    unsigned visited=0;
    for(unsigned bucket=0;bucket<32u;++bucket) {
        if(!(buckets&(1u<<bucket))) continue;
        for(uint8_t *node=heads[bucket];node;node=*(uint8_t **)(node+0x18u)) {
            observe("resource_job_node",bucket);
            /* Exact5D6C80/5D6ED0 publish resource+10/hash-next+18 under
             * 764148. Unlink5D6E70 uses that same lock before destruction.
             * Never follow callbacks, load-queue owners or foreign backings.
             * A bound also rejects cycles without retaining borrowed nodes. */
            if(++visited>MAX_JOB_NODES || !readable(node,0x1cu) ||
                *(void **)node!=verified_image+0x2dd7acu ||
                !*(void **)(node+0x10u) || !*(uint32_t *)(node+0xcu) ||
                resource_bucket(*(void **)(node+0x10u))!=bucket) return ERROR_INVALID_DATA;
            for(unsigned i=0;i<proof->backing_count;++i)
                if(*(void **)(node+0x10u)==proof->backing[i].pointer) {
                    observe("resource_job_owned",i); return ERROR_IO_PENDING;
                }
        }
    }
    for(unsigned i=0;i<proof->backing_count;++i) {
        observe("texture_backing",i);
        const Backing *b=&proof->backing[i];
        if(!writable(b->pointer,0x3cu) || *(void **)(b->pointer+0xcu)) return ERROR_INVALID_DATA;
        uint32_t flags=*(uint32_t *)(b->pointer+0x20u);
        /* Queue membership is authoritative even for resident textures:
         * 5D9800 may queue a resident resource's callback. Also exclude the
         * worker/queued bits, including the unlink-to-destructor interval. */
        if(flags&0x5000u) { observe("resource_backing_waiting",i); return ERROR_IO_PENDING; }
        if(!*(void **)(b->pointer+4u) || (flags&0x2000u)) return ERROR_INVALID_DATA;
        observe("backing_counts",i);
        unsigned references=word(b->pointer+0x24u),requests=(flags>>16u)&0x7fffu;
        if(references<=b->retirements || requests<=b->retirements ||
            references>0xffffu-b->additions || requests>0x7fffu-b->additions) return ERROR_INVALID_DATA;
    }
    return ERROR_SUCCESS;
}
static BOOL backings_ready(const Proof *proof) {
    /* No texture graph means no texture-job dependency (including Q icons).
     * Borrow only the already initialized native lock, never initialize it.
     * Try once: no wait, load pumping, native calls, logging or allocation
     * while locked. The caller's game-thread owner lease remains required;
     * this is an observation, not a lease across later renderer mutation. */
    if(!proof->backing_count) return TRUE;
    observe("resource_job_lock",0);
    CRITICAL_SECTION *lock=(CRITICAL_SECTION *)(verified_image+0x364148u);
    if(!readable(verified_image+0x3c3137u,1u) || verified_image[0x3c3137u]!=1u ||
        !writable(lock,sizeof(*lock))) { SetLastError(ERROR_INVALID_STATE); return FALSE; }
    if(!TryEnterCriticalSection(lock)) { SetLastError(ERROR_IO_PENDING); return FALSE; }
    memory_reset();
    DWORD error=backings_ready_locked(proof);
    LeaveCriticalSection(lock);
    memory_reset();
    SetLastError(error); return error==ERROR_SUCCESS;
}
static BOOL distinct_owned(void *value,void **originals,unsigned *original_count,
    void **retired,unsigned *retired_count,BOOL add,BOOL retire) {
    observe("owned_alias",*retired_count);
    if(add) {
        if(*original_count==MAX_BACKINGS) return FALSE;
        originals[(*original_count)++]=value;
    }
    if(retire) {
        for(unsigned i=0;i<*original_count;++i) if(originals[i]==value) return FALSE;
        for(unsigned i=0;i<*retired_count;++i) if(retired[i]==value) return FALSE;
        if(*retired_count==MAX_BACKINGS) return FALSE;
        retired[(*retired_count)++]=value;
    }
    return TRUE;
}
static BOOL backing(Proof *proof,uint8_t *pointer,BOOL add,BOOL retire) {
    observe("texture_backing",proof->backing_count);
    if(!writable(pointer,0x3cu) || *(void **)(pointer+0xcu)) return FALSE;
    /* Readiness and aggregate reference arithmetic are checked after the
     * complete graph is collected, alongside resource-specific queue state. */
    unsigned i;
    for(i=0;i<proof->backing_count;++i) if(proof->backing[i].pointer==pointer) break;
    if(i==proof->backing_count) {
        if(i==MAX_BACKINGS) return FALSE;
        proof->backing[i].pointer=pointer; ++proof->backing_count;
    }
    proof->backing[i].additions+=add!=FALSE;
    proof->backing[i].retirements+=retire!=FALSE;
    return TRUE;
}
static BOOL definition(Proof *proof,uint8_t *pointer,BOOL add,BOOL retire) {
    observe("material_definition",proof->definition_count);
    if(!writable(pointer,0x14u) || !*(uint32_t *)(pointer+4u)) return FALSE;
    unsigned i;
    for(i=0;i<proof->definition_count;++i) if(proof->definitions[i].pointer==pointer) break;
    if(i==proof->definition_count) {
        if(i==MAX_MATERIALS) return FALSE;
        proof->definitions[i].pointer=pointer; ++proof->definition_count;
    }
    proof->definitions[i].additions+=add!=FALSE;
    proof->definitions[i].retirements+=retire!=FALSE;
    return TRUE;
}
static BOOL uv(uint8_t *object) {
    observe("uv_class",0);
    if(!writable(object,4u)) return FALSE;
    if(*(void **)object==verified_image+0x2dee3cu) {
        if(!writable(object,0x50u) || !slot(0x2dee3cu,0u,0x215ec0u) ||
            !slot(0x2dee3cu,0x14u,0x206ce0u) || !slot(0x2dee3cu,0x18u,0x206d10u)) return FALSE;
        /* Exact5FAE70 writes a 3x3 UV transform at indices0,1,2/4,5,6/
         * 8,9,10, then copies all16 words from its stack. The other seven
         * words are not initialized by that sampler. Clone606CE0 preserves
         * them as opaque data; they are not seven more authored floats.
         * Keep the complete allocation/class/clone proof and check every
         * defined UV component without interpreting the native padding. */
        for(unsigned row=0;row<3u;++row) for(unsigned column=0;column<3u;++column)
            if(!isfinite(*(float *)(object+0x10u+(row*4u+column)*4u))) return FALSE;
        return TRUE;
    }
    return *(void **)object==verified_image+0x2df67cu &&
        slot(0x2df67cu,0u,0x215ec0u) && slot(0x2df67cu,0x14u,0x215e70u);
}
static BOOL material(Proof *proof,uint8_t *value,uint8_t *original,
    BOOL add,BOOL retire) {
    observe("material_class",retire?2u:add?0u:1u);
    if(!writable(value,0x40u) || !word(value+0x14u)) return FALSE;
    unsigned table,clone,destroy,start;
    if(*(void **)value==verified_image+0x2deb7cu) {
        table=0x2deb7cu; clone=0x202cb0u; destroy=0x202c80u; start=0x202e40u;
    } else if(*(void **)value==verified_image+0x2de864u) {
        /* The native base material has the same counted graph. Its exact
         * clone/destructor use the already validated constructor/copy/free
         * paths, preserving the retained original resources below. Property
         * output dispatch is validated separately by the world adapter. */
        table=0x2de864u; clone=0x1fb750u; destroy=0x1fb790u; start=0x1e6ed0u;
    } else return FALSE;
    if(!slot(table,0u,destroy) || !slot(table,4u,start) ||
        !slot(table,8u,0x3b870u) || !slot(table,0x50u,clone)) return FALSE;
    uint8_t *description=*(uint8_t **)(value+4u);
    if(!definition(proof,description,add,retire) ||
        (original && *(void **)(original+4u)!=description &&
            !sol_derived(description,*(uint8_t **)(original+4u)))) return FALSE;
    if(original && *(void **)(original+4u)!=description &&
        ((*(uint32_t *)(value+0xcu)&0x200u) || !writable(*(void **)(value+0x38u),*(uint32_t *)(description+0x10u)*16u))) return FALSE;
    unsigned count=*(unsigned *)(description+0xcu),constants=*(unsigned *)(description+0x10u);
    uint8_t **textures=*(uint8_t ***)(value+8u);
    observe("material_arrays",count);
    if(count>MAX_TEXTURES || constants>4096u ||
        (count && (!readable(textures,count*4u) || !readable(*(void **)(value+0x10u),count*4u))) ||
        (!count && (textures || *(void **)(value+0x10u))) ||
        (constants && !readable(*(void **)(value+0x38u),constants*16u)) ||
        (!constants && *(void **)(value+0x38u))) return FALSE;
    uint8_t **retained=original?*(uint8_t ***)(original+8u):NULL;
    if(original && count && !readable(retained,count*4u)) return FALSE;
    for(unsigned i=0;i<count;++i) {
        observe("texture_class",i);
        uint8_t *texture=textures[i];
        if(!texture) { if(original && retained[i]) return FALSE; continue; }
        if(!writable(texture,8u) || *(void **)texture!=verified_image+0x2dd80cu ||
            !slot(0x2dd80cu,0u,0x1d6b30u) || !slot(0x2dd80cu,0x38u,0x1d6b00u)) return FALSE;
        uint8_t *resource=*(uint8_t **)(texture+4u);
        observe("texture_original_owner",i);
        if(original && (!readable(retained[i],8u) || retained[i]==texture ||
            *(void **)retained[i]!=verified_image+0x2dd80cu ||
            *(void **)(retained[i]+4u)!=resource)) return FALSE;
        if(!distinct_owned(texture,proof->original_textures,&proof->original_texture_count,
            proof->retired_textures,&proof->retired_texture_count,add,retire) ||
            !backing(proof,resource,add,retire)) return FALSE;
    }
    unsigned rows=word(value+0x16u),flags=*(unsigned *)(value+0xcu),bits=0u;
    for(unsigned i=0;i<MAX_TEXTURES;++i) if(flags&(1u<<(i+12u))) ++bits;
    uint32_t *entries=*(uint32_t **)(value+0x3cu);
    observe("uv_layout",rows);
    if(rows!=bits || rows>count || (!rows && entries) || (rows &&
        ((uintptr_t)entries<4u || !readable((void *)((uintptr_t)entries-4u),4u+rows*8u) ||
            entries[-1]!=rows))) return FALSE;
    unsigned previous=0;
    for(unsigned i=0;i<rows;++i) {
        observe("uv_row",i);
        unsigned token=entries[i*2u];
        if(token>=count || !(flags&(1u<<(token+12u))) || (i && token<=previous) ||
            !uv((uint8_t *)(uintptr_t)entries[i*2u+1u]) ||
            !distinct_owned((void *)(uintptr_t)entries[i*2u+1u],proof->original_uv,
                &proof->original_uv_count,proof->retired_uv,&proof->retired_uv_count,add,retire)) return FALSE;
        previous=token;
    }
    return TRUE;
}
static BOOL owner_exact(HMODULE image,const void *pointer,const void *resource_pointer) {
    observe("owner_image",0);
    uint8_t *child=(uint8_t *)pointer,*resource=(uint8_t *)resource_pointer;
    if(!verified_image || image!=(HMODULE)verified_image) return FALSE;
    observe("owner_thread",GetCurrentThreadId());
    if(verified_thread!=GetCurrentThreadId()) return FALSE;
    observe("owner_memory",0);
    if(!writable(child,0x2cu) || !readable(resource,0x30u)) return FALSE;
    unsigned manager_offset;
    uint8_t *override_pair;
    if(*(void **)child==verified_image+0x2dec74u) {
        observe("skin_interface",0);
        if(*(void **)(child+8u)!=verified_image+0x2dedbcu ||
            !slot(0x2dedbcu,0x14u,0x206ad0u) ||
            *(void **)(child+0xcu)!=verified_image+0x2dedd8u ||
            *(void **)(child+0x10u)!=resource || !slot(0x2dedd8u,4u,0x1fc3c0u) ||
            !slot(0x2dedd8u,8u,0x2038c0u) || !slot(0x2dedd8u,0x20u,0x203910u)) return FALSE;
        manager_offset=0x2cu; override_pair=child+0x20u;
    } else if(*(void **)child==verified_image+0x2dfb14u) {
        observe("morph_interface",0);
        if(*(void **)(child+0x10u)!=verified_image+0x2dfc54u ||
            *(void **)(child+0x14u)!=resource || !slot(0x2dfc54u,4u,0x224130u) ||
            !slot(0x2dfc54u,8u,0x2249e0u) || !slot(0x2dfc54u,0x20u,0x224a10u)) return FALSE;
        manager_offset=0x20u; override_pair=*(uint8_t **)(child+0x20u);
        if(override_pair && !writable(override_pair,8u)) return FALSE;
    } else if(*(void **)child==verified_image+0x2de564u) {
        observe("particle_interface",0);
        if(*(void **)(child+0xcu)!=verified_image+0x2de67cu ||
            *(void **)(child+0x10u)!=resource ||
            !slot(0x2de67cu,4u,0x1fa0b0u) || !slot(0x2de67cu,8u,0x1fa0c0u) ||
            !slot(0x2de67cu,0x20u,0x1fa100u)) return FALSE;
        /* The particle renderer retains its material override pair through
         * a counted cD3DParticleXResource, rather than in the renderer itself.
         * Exact5FA100 delegates to the same validated prepare/clone helper. */
        uint8_t *wrapper=*(uint8_t **)(child+0x1cu);
        if(!readable(wrapper,0x10u) || *(void **)wrapper!=verified_image+0x2de54cu ||
            !*(uint32_t *)(wrapper+4u) || *(uint32_t *)(wrapper+4u)==UINT32_MAX ||
            *(void **)(wrapper+8u)!=resource) return FALSE;
        manager_offset=0x28u; override_pair=*(uint8_t **)(wrapper+0xcu);
        if(override_pair && !writable(override_pair,8u)) return FALSE;
    } else return FALSE;
    observe("material_manager",0);
    uint8_t *manager=*(uint8_t **)(resource+manager_offset);
    if(!readable(manager,8u)) return FALSE;
    unsigned count=*(unsigned *)manager;
    uint8_t **originals=*(uint8_t ***)(manager+4u);
    uint8_t **overrides=override_pair?*(uint8_t ***)override_pair:NULL;
    uint8_t *flags=override_pair?*(uint8_t **)(override_pair+4u):NULL;
    if(count>MAX_MATERIALS || (count && !readable(originals,count*4u)) ||
        (overrides && count && (!writable(overrides,count*4u) || !writable(flags,count)))) return FALSE;
    Proof proof; memset(&proof,0,sizeof(proof));
    for(unsigned i=0;i<count;++i)
        if(!material(&proof,originals[i],NULL,TRUE,FALSE)) return FALSE;
    if(overrides) for(unsigned i=0;i<count;++i) {
        observe("override_state",i);
        unsigned state=flags[i]&3u;
        if(state==0u) { if(overrides[i]!=originals[i]) return FALSE; continue; }
        if(state!=2u || !overrides[i]) return FALSE;
        observe("override_alias",i);
        for(unsigned j=0;j<count;++j) {
            if(overrides[i]==originals[j]) return FALSE;
            if(j<i && (flags[j]&3u)==2u && overrides[i]==overrides[j]) return FALSE;
        }
        if(!writable(overrides[i],0x40u)) return FALSE;
        BOOL retiring=word(overrides[i]+0x14u)==1u;
        if(!material(&proof,overrides[i],originals[i],FALSE,retiring)) return FALSE;
    }
    /* All old overrides are released before replacement clones are made.
     * Sum every possibly retired wrapper sharing a backing; per-wrapper >1
     * alone does not prove the final release preserves the original holder. */
    for(unsigned i=0;i<proof.definition_count;++i) {
        observe("definition_counts",i);
        const Definition *d=&proof.definitions[i];
        uint32_t references=*(uint32_t *)(d->pointer+4u);
        if(references<d->retirements || references>UINT32_MAX-d->additions ||
            (references==d->retirements && (d->additions || !sol_definition(d->pointer,FALSE)))) return FALSE;
    }
    observe("owner_recheck",count);
    return *(void **)(resource+manager_offset)==manager && *(unsigned *)manager==count &&
        *(void **)(manager+4u)==originals && (!override_pair ||
            (*(void **)override_pair==overrides && *(void **)(override_pair+4u)==flags)) && backings_ready(&proof);
}
BOOL SudekiMpLanStoryMaterialOwnerExact(HMODULE image,const void *child,const void *resource) {
    /* Reject a foreign/reentrant caller before touching the scoped cache. */
    if(!verified_image || image!=(HMODULE)verified_image ||
        verified_thread!=GetCurrentThreadId() || memory_scope) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    SetLastError(ERROR_INVALID_DATA);
    memory_reset(); memory_scope=TRUE;
    BOOL result=owner_exact(image,child,resource);
    DWORD error=result?ERROR_SUCCESS:GetLastError();
    memory_scope=FALSE; memory_reset();
    if(!result && failure_logs<64u &&
        (failure_stage!=last_failure_stage || failure_detail!=last_failure_detail ||
            child!=last_failure_child)) {
        SudekiMpLogFormat("lan_story_material event=refused stage=%s detail=%u child=%p init_thread=%lu current_thread=%lu win32_error=%lu\r\n",
            failure_stage,failure_detail,child,(unsigned long)verified_thread,
            (unsigned long)GetCurrentThreadId(),(unsigned long)error);
        ++failure_logs; last_failure_stage=failure_stage; last_failure_detail=failure_detail;
        last_failure_child=child;
    }
    SetLastError(error); return result;
}
