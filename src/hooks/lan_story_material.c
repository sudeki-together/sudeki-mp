#include "hooks/lan_story_material.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

#if !defined(__i386__)
#error "Saved-story material observations require the supported x86 layout"
#endif

enum { MAX_MATERIALS=128u, MAX_TEXTURES=8u, MAX_BACKINGS=1024u };
typedef struct CodeIdentity { unsigned rva,size; uint32_t hash; } CodeIdentity;
typedef struct Relocation { unsigned rva,target; } Relocation;
static const CodeIdentity codes[]={
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
    {0x206ce0u,52u,0x1172e151u},
    {0x215e70u,24u,0xa68bb247u},
    {0x215ec0u,31u,0x37ddc37au},
    {0x2038c0u,36u,0x4ee71e45u},
    {0x202e40u,240u,0x72ae6836u},
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
static void observe(const char *stage,unsigned detail) {
    failure_stage=stage; failure_detail=detail;
}
static BOOL memory(const void *pointer,size_t size,BOOL write) {
    MEMORY_BASIC_INFORMATION m; uintptr_t p=(uintptr_t)pointer;
    if(!pointer || !size || p>UINTPTR_MAX-size ||
        VirtualQuery(pointer,&m,sizeof(m))!=sizeof(m) || m.State!=MEM_COMMIT ||
        (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
    DWORD access=m.Protect&0xffu;
    if(write) {
        if(access!=PAGE_READWRITE && access!=PAGE_WRITECOPY &&
            access!=PAGE_EXECUTE_READWRITE && access!=PAGE_EXECUTE_WRITECOPY) return FALSE;
    } else if(access!=PAGE_READONLY && access!=PAGE_READWRITE && access!=PAGE_WRITECOPY &&
        access!=PAGE_EXECUTE_READ && access!=PAGE_EXECUTE_READWRITE &&
        access!=PAGE_EXECUTE_WRITECOPY) return FALSE;
    return p+size<=(uintptr_t)m.BaseAddress+m.RegionSize;
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
static BOOL jobs_empty(void) {
    /* The native hash is (resource>>6)&31. Never traverse worker-owned
     * pending job/callback nodes without their lock. This same-area scope
     * requires the complete job table empty before admitting clone/delete. */
    observe("resource_jobs",0);
    void *const *heads=(void *const *)(verified_image+0x363ee8u);
    if(!readable(heads,32u*4u)) return FALSE;
    for(unsigned i=0;i<32u;++i) if(heads[i]) {
        observe("resource_jobs",i); SetLastError(ERROR_BUSY); return FALSE;
    }
    return TRUE;
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
    if(!writable(pointer,0x3cu) || !*(void **)(pointer+4u) ||
        *(void **)(pointer+0xcu)) return FALSE;
    uint32_t flags=*(uint32_t *)(pointer+0x20u);
    if((flags&0x2000u) || !(flags&0x7fff0000u) || !word(pointer+0x24u)) return FALSE;
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
        (original && *(void **)(original+4u)!=description)) return FALSE;
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
    if(!jobs_empty()) return FALSE;
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
    for(unsigned i=0;i<proof.backing_count;++i) {
        observe("backing_counts",i);
        const Backing *b=&proof.backing[i];
        unsigned references=word(b->pointer+0x24u);
        unsigned requests=(*(unsigned *)(b->pointer+0x20u)>>16u)&0x7fffu;
        if(references<=b->retirements || requests<=b->retirements ||
            references>0xffffu-b->additions || requests>0x7fffu-b->additions) return FALSE;
    }
    for(unsigned i=0;i<proof.definition_count;++i) {
        observe("definition_counts",i);
        const Definition *d=&proof.definitions[i];
        uint32_t references=*(uint32_t *)(d->pointer+4u);
        if(references<=d->retirements || references>UINT32_MAX-d->additions) return FALSE;
    }
    observe("owner_recheck",count);
    return *(void **)(resource+manager_offset)==manager && *(unsigned *)manager==count &&
        *(void **)(manager+4u)==originals && (!override_pair ||
            (*(void **)override_pair==overrides && *(void **)(override_pair+4u)==flags)) && jobs_empty();
}
BOOL SudekiMpLanStoryMaterialOwnerExact(HMODULE image,const void *child,const void *resource) {
    SetLastError(ERROR_INVALID_DATA);
    BOOL result=owner_exact(image,child,resource);
    DWORD error=result?ERROR_SUCCESS:GetLastError();
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
