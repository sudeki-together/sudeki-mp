#include "hooks/lan_story_residency.h"
#include "hooks/lan_story_resource_file.h"
#include "engine/build_identity.h"
#include <stdint.h>
#include <string.h>

#if !defined(__i386__)
#error "Saved-story residency requires the supported x86 renderer"
#endif

enum { MAX_ANIMATIONS=4096u, MAX_DEPENDENCIES=4096u };
typedef struct CodeIdentity { unsigned rva,size; uint32_t hash; } CodeIdentity;
typedef struct Relocation { unsigned rva,target; } Relocation;
static const CodeIdentity codes[]={
    {0x223b70u,66u,0x11f56333u},
    {0x220930u,99u,0xd2ab18dau},
    {0x2208b0u,121u,0x381f4f69u},
    {0x226800u,225u,0x645b8ebdu},
    {0x226590u,47u,0x5a625013u},
    {0x226600u,289u,0x48bb520du},
    {0x21d2d0u,969u,0xfcc0210eu},
    {0x21d6a0u,147u,0x2df8d8e6u},
    {0x21cb30u,165u,0x11547f7du},
    {0x22baa0u,175u,0x4f7f85c6u},
    {0x22bb50u,86u,0xe1dc7c3fu},
    {0x1fadb0u,74u,0x592022a0u},
    {0x1fb0b0u,261u,0x254bce3fu},
    {0x2147e0u,74u,0x6d636a64u},
    {0x216100u,74u,0x53cf95c4u},
    {0x220e40u,30u,0x9df487a8u},
    {0x220e60u,609u,0x75272bf6u},
};
static const Relocation relocations[]={
    {0x1faddau,0x2de6ecu},
    {0x1fb0c6u,0x1fb198u},
    {0x1fb0cdu,0x1fb184u},
    {0x1fb0edu,0x2de70cu},
    {0x1fb10fu,0x2de720u},
    {0x1fb130u,0x2de734u},
    {0x1fb14du,0x2de748u},
    {0x1fb184u,0x1fb116u},
    {0x1fb188u,0x1fb0f5u},
    {0x1fb18cu,0x1fb0d1u},
    {0x1fb190u,0x1fb137u},
    {0x1fb194u,0x1fb17cu},
    {0x21480au,0x2df2a8u},
    {0x21612au,0x2df6a8u},
    {0x21d4a5u,0x3c37c0u},
    {0x220e6du,0x2df8ecu},
    {0x220e74u,0x2dfa88u},
    {0x220fc3u,0x36c6b8u},
    {0x220fceu,0x29a068u},
    {0x220fe9u,0x36c6b8u},
    {0x220fefu,0x29a064u},
    {0x2210b8u,0x2ddb8cu},
};

typedef unsigned char (__attribute__((thiscall)) *NativeAcquire)(void *,uint32_t);
typedef struct Observation {
    uint8_t *renderer,*bank,*description,*entries,*resource,*remap;
    uint8_t *package,*header,*descriptors,*rows,*row,*renderer_refs,*bank_refs,*chunks;
    unsigned selector,animations,dependencies,package_count,index;
    uint32_t dependency,descriptor,flags,package_handle;
    uint8_t renderer_count,bank_count;
} Observation;
static uint8_t *image;
static DWORD native_thread;
static BOOL ready,active;
/* A failed native attempt is not assumed reversible. Suppress retries of the
   same observed identity until a new runtime initialization; these are opaque
   comparison values, never retained borrowed objects or release obligations. */
static void *failed_renderer,*failed_bank;
static uint32_t failed_dependency;

static BOOL memory(const void *pointer,size_t size,BOOL write) {
    MEMORY_BASIC_INFORMATION m; uintptr_t p=(uintptr_t)pointer;
    if(!pointer || !size || p>UINTPTR_MAX-size ||
        VirtualQuery(pointer,&m,sizeof(m))!=sizeof(m) || m.State!=MEM_COMMIT ||
        (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
    DWORD a=m.Protect&0xffu;
    if(write) {
        if(a!=PAGE_READWRITE && a!=PAGE_WRITECOPY &&
            a!=PAGE_EXECUTE_READWRITE && a!=PAGE_EXECUTE_WRITECOPY) return FALSE;
    } else if(a!=PAGE_READONLY && a!=PAGE_READWRITE && a!=PAGE_WRITECOPY &&
        a!=PAGE_EXECUTE_READ && a!=PAGE_EXECUTE_READWRITE &&
        a!=PAGE_EXECUTE_WRITECOPY) return FALSE;
    return p+size<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL readable(const void *p,size_t n) { return memory(p,n,FALSE); }
static BOOL writable(const void *p,size_t n) { return memory(p,n,TRUE); }
static uint32_t dword(const void *p) { uint32_t v; memcpy(&v,p,4u); return v; }
static unsigned word(const void *p) { uint16_t v; memcpy(&v,p,2u); return v; }
static BOOL code_exact(const CodeIdentity *code) {
    if(!readable(image+code->rva,code->size)) return FALSE;
    uint32_t hash=2166136261u;
    for(unsigned i=0;i<code->size;++i) {
        uint8_t byte=image[code->rva+i];
        for(unsigned r=0;r<sizeof(relocations)/sizeof(relocations[0]);++r) {
            unsigned offset=code->rva+i-relocations[r].rva;
            if(offset>=4u) continue;
            if(dword(image+relocations[r].rva)!=(uintptr_t)image+relocations[r].target)
                return FALSE;
            byte=(uint8_t)((0x400000u+relocations[r].target)>>(offset*8u)); break;
        }
        hash=(hash^byte)*16777619u;
    }
    return hash==code->hash;
}
static BOOL methods_exact(void) {
    if(!image || !readable(image+0x2df8ecu,0x198u) ||
        *(void **)(image+0x2df8ecu)!=image+0x220e40u ||
        *(void **)(image+0x2df8ecu+0x18cu)!=image+0x223b70u) return FALSE;
    for(unsigned i=0;i<sizeof(codes)/sizeof(codes[0]);++i)
        if(!code_exact(&codes[i])) return FALSE;
    return TRUE;
}
static BOOL factories_exact(void) {
    static const unsigned nodes[]={0x34ffc0u,0x34fe30u,0x34fb20u,0x34fad0u};
    static const unsigned tables[]={0x2df6bcu,0x2df2bcu,0x2de75cu,0x2de700u};
    static const unsigned entries[]={0x216100u,0x2147e0u,0x1fb0b0u,0x1fadb0u};
    if(!readable(image+0x3c37c0u,4u)) return FALSE;
    uint8_t *current=*(uint8_t **)(image+0x3c37c0u);
    unsigned visited=0;
    for(unsigned n=0;n<4u;++n) {
        unsigned match=4u;
        for(unsigned i=0;i<4u;++i) if(current==image+nodes[i]) { match=i; break; }
        if(match==4u || (visited&(1u<<match)) || !readable(current,8u) ||
            *(void **)current!=image+tables[match] ||
            !readable(image+tables[match],4u) ||
            *(void **)(image+tables[match])!=image+entries[match]) return FALSE;
        visited|=1u<<match; current=*(uint8_t **)(current+4u);
    }
    return !current && visited==15u;
}
static BOOL within(const void *p,size_t n,const void *start,size_t size) {
    uintptr_t a=(uintptr_t)p,b=(uintptr_t)start;
    return a>=b && n<=size && a-b<=size-n;
}
static BOOL remap_exact(const Observation *o) {
    /* 61CBE0 creates this dictionary through 5F5910 in the primary resident
       bank payload. 61CB30 only replaces authored string indices with its
       entries; it calls neither scripts nor an event dispatcher. */
    uint8_t *dictionary=o->remap;
    if(!dictionary) return TRUE;
    if(!readable(dictionary,0x14u)) return FALSE;
    unsigned count=dword(dictionary);
    uint8_t *strings=*(uint8_t **)(dictionary+0xcu);
    uint8_t *data=*(uint8_t **)(o->rows+8u);
    unsigned size=dword(o->descriptors)&0xffffffu;
    if(count>65536u || !(dword(o->rows+4u)&1u) ||
        !size || !readable(data,size)) return FALSE;
    if(!count) return !strings;
    if(!within(strings,count*4u,data,size) || !readable(strings,count*4u)) return FALSE;
    for(unsigned i=0;i<count;++i) {
        const char *text=*(const char **)(strings+i*4u);
        if(!within(text,1u,data,size)) return FALSE;
        size_t remaining=size-((uintptr_t)text-(uintptr_t)data);
        if(remaining>4096u) remaining=4096u;
        if(!memchr(text,0,remaining)) return FALSE;
    }
    return TRUE;
}
static SudekiMpLanStoryResidencyState report_state(SudekiMpLanStoryResidencyReport *out,
    const Observation *o,SudekiMpLanStoryResidencyState state,const char *reason,DWORD error) {
    if(out) {
        memset(out,0,sizeof(*out)); out->state=state; out->reason=reason;
        if(o) {
            out->dependency=o->dependency; out->package_flags=o->flags;
            out->renderer_references=o->renderer_count; out->bank_references=o->bank_count;
        }
    }
    SetLastError(error); return state;
}
static SudekiMpLanStoryResidencyState inspect(void *renderer,void *expected_bank,unsigned selector,
    Observation *o,SudekiMpLanStoryResidencyReport *out) {
    memset(o,0,sizeof(*o)); o->renderer=renderer; o->bank=expected_bank; o->selector=selector;
#define INVALID(why) return report_state(out,o,SUDEKIMP_STORY_RESIDENCY_INVALID,why,ERROR_NOT_SUPPORTED)
    if(!ready || !image || (native_thread && native_thread!=GetCurrentThreadId())) INVALID("native_thread");
    if(!writable(renderer,0xb0u) || *(void **)renderer!=image+0x2df8ecu ||
        *(void **)(o->renderer+8u)!=expected_bank || !writable(expected_bank,0x7cu) ||
        *(void **)expected_bank!=image+0x2dfa9cu || !(dword(o->bank+8u)&0x3fffffffu))
        INVALID("renderer_bank");
    o->description=*(uint8_t **)(o->bank+0x1cu); o->entries=*(uint8_t **)(o->bank+0x20u);
    o->remap=*(uint8_t **)(o->bank+0x54u);
    if(!readable(o->description,0x24u)) INVALID("bank_description");
    o->animations=dword(o->description);
    if(!o->animations || o->animations>MAX_ANIMATIONS || selector>=o->animations ||
        !writable(o->entries,o->animations*28u)) INVALID("bank_entries");
    o->resource=*(uint8_t **)(o->entries+selector*28u);
    if(!readable(o->resource,0x20u)) INVALID("clip_resource");
    o->dependency=dword(o->resource+0x1cu);
    if(!o->dependency) return report_state(out,o,SUDEKIMP_STORY_RESIDENCY_READY,"no_dependency",0);
    o->index=o->dependency&0x7fffffffu; o->dependencies=dword(o->description+0x20u);
    o->package=*(uint8_t **)(o->bank+0x10u);
    if(!o->index || o->dependencies<2u || o->dependencies>MAX_DEPENDENCIES ||
        o->index>=o->dependencies || !writable(o->package,0x20u)) INVALID("dependency_bounds");
    o->package_handle=dword(o->package);
    o->header=*(uint8_t **)(o->package+4u); o->descriptors=*(uint8_t **)(o->package+0x10u);
    o->rows=*(uint8_t **)(o->package+0x14u);
    if(!readable(o->header,6u)) INVALID("package_header");
    o->package_count=word(o->header+4u);
    if(!o->package_count || o->package_count>MAX_DEPENDENCIES ||
        o->index>=o->package_count || o->dependencies>o->package_count ||
        !readable(o->descriptors,o->package_count*4u) ||
        !writable(o->rows,o->package_count*12u)) INVALID("package_rows");
    o->row=o->rows+o->index*12u; o->flags=dword(o->row+4u);
    o->descriptor=dword(o->descriptors+o->index*4u);
    o->renderer_refs=*(uint8_t **)(o->renderer+0xacu);
    o->bank_refs=*(uint8_t **)(o->bank+0x74u); o->chunks=*(uint8_t **)(o->bank+0x78u);
    if(!writable(o->renderer_refs,o->dependencies-1u) ||
        !writable(o->bank_refs,o->dependencies-1u) ||
        !writable(o->chunks,o->dependencies*4u)) INVALID("reference_arrays");
    o->renderer_count=o->renderer_refs[o->index-1u];
    o->bank_count=o->bank_refs[o->index-1u];
    if(o->bank_count<o->renderer_count) INVALID("reference_order");
    if((o->flags&1u) && ((int32_t)o->dependency<0 || o->renderer_count)) {
        if(!o->bank_count || !readable(*(void **)(o->chunks+o->index*4u),16u))
            INVALID("resident_chunk");
        return report_state(out,o,SUDEKIMP_STORY_RESIDENCY_READY,"resident",0);
    }
    if((int32_t)o->dependency<0) INVALID("negative_dependency_unowned");
    if(o->renderer_count || o->bank_count==255u) INVALID("reference_capacity");
    if(failed_renderer==renderer && failed_bank==expected_bank && failed_dependency==o->dependency)
        INVALID("previous_native_attempt_unknown");
    for(unsigned i=0;i<o->package_count;++i) if(dword(o->rows+i*12u+4u)&2u)
        return report_state(out,o,SUDEKIMP_STORY_RESIDENCY_WAITING,"package_request_pending",ERROR_IO_PENDING);
    if(o->bank_count) {
        if(!(o->flags&1u) || !readable(*(void **)(o->chunks+o->index*4u),16u))
            INVALID("bank_reference_without_residency");
    } else if(*(void **)(o->chunks+o->index*4u)) INVALID("unowned_parsed_chunk");
    /* 626600's two dictionary branches are outside this animation-only
       adapter. Other chunk kinds take its no-callback default branch. */
    uint32_t kind=o->descriptor&0xf8000000u;
    if(kind==0x08000000u || kind==0x18000000u ||
        (o->descriptor&0xffffffu)<16u) INVALID("package_chunk_kind");
    if(!(o->flags&1u)) {
        if(*(void **)(o->row+8u)) INVALID("unloaded_buffer_present");
        if((dword(o->package+0xcu)&2u) || !o->package_handle ||
            o->package_handle==UINT32_MAX || o->package_handle==0x7ffffu)
            INVALID("package_file_branch");
        if(!SudekiMpLanStoryResourceFileExact(o->package_handle,dword(o->row),
            o->descriptor&0xffffffu)) INVALID("package_file_source");
    } else if(!readable(*(void **)(o->row+8u),o->descriptor&0xffffffu))
        INVALID("resident_buffer");
    if(!factories_exact()) INVALID("curve_factory_chain");
    if(!remap_exact(o)) INVALID("bank_argument_remap");
    return report_state(out,o,SUDEKIMP_STORY_RESIDENCY_NEEDS_LOAD,"native_dependency_required",0);
#undef INVALID
}
BOOL SudekiMpLanStoryResidencyInitialize(HMODULE module) {
    uint8_t *candidate=(uint8_t *)module;
    if(active || !candidate || (image && image!=candidate) ||
        (native_thread && native_thread!=GetCurrentThreadId()) ||
        !SudekiMpCheckLoadedExecutable(module)) {
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    ready=FALSE; image=candidate;
    if(!methods_exact() || !SudekiMpLanStoryResourceFileInitialize(module)) {
        SetLastError(ERROR_BAD_EXE_FORMAT); return FALSE;
    }
    failed_renderer=NULL; failed_bank=NULL; failed_dependency=0; ready=TRUE;
    SetLastError(ERROR_SUCCESS); return TRUE;
}
SudekiMpLanStoryResidencyState SudekiMpLanStoryResidencyInspect(
    void *renderer,void *bank,unsigned selector,SudekiMpLanStoryResidencyReport *out) {
    if(active) return report_state(out,NULL,SUDEKIMP_STORY_RESIDENCY_INVALID,"operation_active",ERROR_BUSY);
    Observation o; return inspect(renderer,bank,selector,&o,out);
}
static BOOL same_observation(const Observation *a,const Observation *b) {
    return a->renderer==b->renderer && a->bank==b->bank && a->description==b->description &&
        a->entries==b->entries && a->resource==b->resource && a->remap==b->remap && a->package==b->package &&
        a->header==b->header && a->descriptors==b->descriptors && a->rows==b->rows &&
        a->row==b->row && a->renderer_refs==b->renderer_refs && a->bank_refs==b->bank_refs &&
        a->chunks==b->chunks && a->selector==b->selector && a->animations==b->animations &&
        a->dependencies==b->dependencies && a->package_count==b->package_count &&
        a->index==b->index && a->dependency==b->dependency && a->descriptor==b->descriptor &&
        a->package_handle==b->package_handle;
}
SudekiMpLanStoryResidencyState SudekiMpLanStoryResidencyAcquire(
    void *renderer,void *bank,unsigned selector,SudekiMpLanStoryResidencyExact exact,void *context,
    SudekiMpLanStoryResidencyReport *out) {
    Observation before,after;
    if(active || !exact || !exact(context))
        return report_state(out,NULL,SUDEKIMP_STORY_RESIDENCY_INVALID,"owner_witness",ERROR_INVALID_STATE);
    SudekiMpLanStoryResidencyState state=inspect(renderer,bank,selector,&before,out);
    if(state!=SUDEKIMP_STORY_RESIDENCY_NEEDS_LOAD) return state;
    if(!methods_exact() || !exact(context))
        return report_state(out,&before,SUDEKIMP_STORY_RESIDENCY_INVALID,"native_contract",ERROR_INVALID_STATE);
    state=inspect(renderer,bank,selector,&after,out);
    if(state!=SUDEKIMP_STORY_RESIDENCY_NEEDS_LOAD || !same_observation(&before,&after) ||
        before.flags!=after.flags || before.bank_count!=after.bank_count ||
        before.renderer_count!=after.renderer_count)
        return report_state(out,&before,SUDEKIMP_STORY_RESIDENCY_INVALID,"request_changed",ERROR_RETRY);
    active=TRUE; native_thread=GetCurrentThreadId();
    NativeAcquire acquire=(NativeAcquire)(image+0x223b70u);
    unsigned char succeeded=acquire(renderer,before.dependency);
    /* The callback must establish current lifetime before any post-call object
       reads. Never compensate by guessing at old references on an unknown owner. */
    BOOL owner_exact=exact(context);
    if(!owner_exact || !succeeded) {
        failed_renderer=renderer; failed_bank=bank; failed_dependency=before.dependency; active=FALSE;
        return report_state(out,&before,SUDEKIMP_STORY_RESIDENCY_INVALID,
            owner_exact?"native_load_failed":"owner_changed_after_load",ERROR_INVALID_STATE);
    }
    state=inspect(renderer,bank,selector,&after,out);
    BOOL complete=state==SUDEKIMP_STORY_RESIDENCY_READY && same_observation(&before,&after) &&
        after.renderer_count==1u && after.bank_count==(unsigned)before.bank_count+1u &&
        !(after.flags&2u) && factories_exact() && remap_exact(&after);
    active=FALSE;
    if(!complete) {
        failed_renderer=renderer; failed_bank=bank; failed_dependency=before.dependency;
        return report_state(out,&after,SUDEKIMP_STORY_RESIDENCY_INVALID,"native_readback_unknown",ERROR_INVALID_STATE);
    }
    return report_state(out,&after,SUDEKIMP_STORY_RESIDENCY_LOADED,"native_renderer_owns_reference",0);
}
