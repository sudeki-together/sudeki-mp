#include "hooks/lan_story_cinematic.h"
#include "cleanroom/engine.h"
#include "engine/log.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Story speech audio requires the supported x86 ABI"
#endif

typedef struct AudioCode { unsigned rva,size; uint32_t hash; } AudioCode;
typedef struct AudioRelocation { unsigned rva,target; } AudioRelocation;
static const AudioCode codes[]={
    {0x28aeb0u,274u,0x7ed92b2du},
    {0x28ade0u,111u,0xcf71e287u},
    {0x287ad0u,640u,0xcaffac9au},
    {0x287aa0u,33u,0xadb85453u},
    {0x2848a0u,231u,0xea4e3e69u},
    {0x284ae0u,122u,0x579db7ccu},
    {0x28e970u,70u,0xcda6b54fu},
    {0x280b90u,108u,0xf561ea5fu},
    {0x281610u,49u,0x8aa80e9cu},
    {0x280f40u,1447u,0x17975b02u},
    {0x280c30u,102u,0xc2c94d96u},
    {0x2871b0u,127u,0x4ce313c5u},
    {0x287d90u,9u,0x2105a5c9u},
};
static const AudioRelocation relocations[]={
    {0x28aedbu,0x409e58u},
    {0x28af36u,0x409e8cu},
    {0x28af93u,0x409e8cu},
    {0x28ae0au,0x409e8cu},
    {0x28ae36u,0x409e8cu},
    {0x287b0au,0x3c37d4u},
    {0x287b29u,0x3c2370u},
    {0x287b2fu,0x3c2370u},
    {0x287b42u,0x3c2374u},
    {0x287c99u,0x3c37d4u},
    {0x284ae1u,0x409e8cu},
    {0x28e972u,0x409e8cu},
    {0x28102fu,0x2e3608u},
    {0x28104cu,0x2c05d4u},
    {0x2810e3u,0x2c0514u},
    {0x281121u,0x2c05e4u},
    {0x28118bu,0x3c37d8u},
    {0x2812c3u,0x3c37d8u},
    {0x281370u,0x2e3658u},
    {0x281392u,0x2c05ecu},
    {0x2813c1u,0x2e3690u},
    {0x2813cfu,0x2e3908u},
    {0x2813d9u,0x2e3730u},
    {0x28140eu,0x2e3618u},
    {0x281426u,0x3c37e0u},
    {0x281477u,0x2e3658u},
    {0x281499u,0x2c05f8u},
    {0x280c64u,0x3c37d8u},
    {0x280c81u,0x3c37d8u},
};

typedef struct AudioOwner {
    uint8_t *base,*speech,*emitter,*bank,*soundbank,*manager,*pool,*entries,*backend;
    unsigned count;
} AudioOwner;
static struct {
    DWORD thread;
    uint32_t epoch,line,handle;
    uint8_t *bank,*soundbank,*pool,*cue;
    BOOL busy,attempted,line_ok,cue_stopped;
} voice;
static BOOL memory(const void *pointer,size_t n,BOOL write) {
    MEMORY_BASIC_INFORMATION m; uintptr_t p=(uintptr_t)pointer;
    if(!pointer || !n || p>UINTPTR_MAX-n || VirtualQuery(pointer,&m,sizeof(m))!=sizeof(m) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
    DWORD a=m.Protect&0xffu;
    if(write) {
        if(a!=PAGE_READWRITE && a!=PAGE_WRITECOPY && a!=PAGE_EXECUTE_READWRITE && a!=PAGE_EXECUTE_WRITECOPY)
            return FALSE;
    } else if(a!=PAGE_READONLY && a!=PAGE_READWRITE && a!=PAGE_WRITECOPY &&
        a!=PAGE_EXECUTE_READ && a!=PAGE_EXECUTE_READWRITE && a!=PAGE_EXECUTE_WRITECOPY) return FALSE;
    return p+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static uint32_t u32(const void *p) { uint32_t v; memcpy(&v,p,4u); return v; }
static BOOL typed(uint8_t *base,void *p,unsigned size,unsigned table) {
    return memory(p,size,FALSE) && *(void **)p==base+table;
}
static BOOL methods(uint8_t *base) {
    if(!SudekiMpCleanroomEngineImageExact((HMODULE)base)) return FALSE;
    for(unsigned c=0;c<sizeof(codes)/sizeof(codes[0]);++c) {
        const AudioCode *code=codes+c; uint32_t h=2166136261u;
        if(!memory(base+code->rva,code->size,FALSE)) return FALSE;
        for(unsigned i=0;i<code->size;++i) {
            uint8_t byte=base[code->rva+i];
            for(unsigned j=0;j<sizeof(relocations)/sizeof(relocations[0]);++j) {
                unsigned offset=code->rva+i-relocations[j].rva;
                if(offset>=4u) continue;
                if(u32(base+relocations[j].rva)!=(uint32_t)(uintptr_t)(base+relocations[j].target)) return FALSE;
                byte=(uint8_t)((0x400000u+relocations[j].target)>>(offset*8u)); break;
            }
            h=(h^byte)*16777619u;
        }
        if(h!=code->hash) return FALSE;
    }
    return *(void **)(base+0x2e1e40u+8u)==base+0x287d90u &&
        *(void **)(base+0x2e1f68u+4u)==base+0x280b90u &&
        *(void **)(base+0x2e1f68u+0x14u)==base+0x281610u &&
        *(void **)(base+0x2e1f68u+0x18u)==base+0x280c30u;
}
static BOOL owner(AudioOwner *out) {
    AudioOwner a={0}; a.base=(uint8_t *)GetModuleHandleW(NULL);
    if(!out || !a.base) return FALSE;
    a.speech=*(uint8_t **)(a.base+0x408d3cu);
    a.manager=*(uint8_t **)(a.base+0x409e58u);
    a.pool=*(uint8_t **)(a.base+0x409e8cu);
    if(!typed(a.base,a.speech,0x3e4u,0x2c6190u) || u32(a.speech+0xc8u)!=2u ||
        !typed(a.base,a.manager,0x15cu,0x2e1dc0u) ||
        !typed(a.base,a.pool,0x14u,0x2e1ee0u)) return FALSE;
    a.emitter=*(uint8_t **)(a.speech+0x3e0u);
    if(!typed(a.base,a.emitter,0x14u,0x2e1db4u) || a.emitter[0x10u]!=1u ||
        u32(a.emitter+8u)) return FALSE;
    a.bank=*(uint8_t **)(a.emitter+4u);
    if(!typed(a.base,a.bank,0x34u,0x2e1e40u) || !u32(a.bank+0x18u) ||
        u32(a.bank+0x18u)>=0x7fffffffu || a.bank[0x30u] ||
        u32(a.bank+0x14u)!=0x14197b06u) return FALSE;
    const char *bank_name=*(const char **)(a.bank+0x10u);
    if(!memory(bank_name,11u,FALSE) || memcmp(bank_name,"speech.xsb",11u)) return FALSE;
    unsigned banks=u32(a.manager+0x30u),matches=0;
    void **list=*(void ***)(a.manager+0x2cu);
    if(!banks || banks>4096u || !memory(list,banks*4u,FALSE)) return FALSE;
    for(unsigned i=0;i<banks;++i) if(list[i]==a.bank) ++matches;
    if(matches!=1u) return FALSE;
    a.soundbank=*(uint8_t **)(a.bank+0x1cu);
    if(!typed(a.base,a.soundbank,0x3cu,0x2e1f68u) || !u32(a.soundbank+4u) ||
        memcmp(a.soundbank+8u,"SSBN",4u)) return FALSE;
    a.backend=*(uint8_t **)(a.manager+0x10u);
    if(a.backend!=*(void **)(a.base+0x3c37d8u) ||
        !typed(a.base,a.backend,0xafbcu,0x2e1e90u)) return FALSE;
    a.count=u32(a.pool+8u); a.entries=*(uint8_t **)(a.pool+0x10u);
    if(!a.count || a.count>256u || !memory(a.entries,a.count*24u,TRUE) ||
        !u32(a.pool+4u) || u32(a.pool+4u)>0xffffffu) return FALSE;
    *out=a; return TRUE;
}
static BOOL same_owner(const AudioOwner *a,const AudioOwner *b) {
    return a->base==b->base && a->speech==b->speech && a->emitter==b->emitter &&
        a->bank==b->bank && a->soundbank==b->soundbank && a->manager==b->manager &&
        a->pool==b->pool && a->entries==b->entries && a->count==b->count && a->backend==b->backend;
}
static BOOL cue_index(const AudioOwner *a,const char *name,unsigned *index) {
    uint8_t *bank=a->soundbank;
    unsigned count=u32(bank+0x1cu),names_size=u32(bank+0x2cu);
    uint8_t *rows=*(uint8_t **)(bank+0x38u),*names=*(uint8_t **)(bank+0x30u);
    if(!name || !index || !count || count>8192u || !names_size || names_size>1048576u ||
        !memory(rows,count*24u,FALSE) || !memory(names,names_size,FALSE)) return FALSE;
    size_t wanted=strlen(name);
    for(unsigned i=0;i<count;++i) {
        unsigned offset=u32(rows+i*24u);
        if(offset>=names_size) return FALSE;
        const char *text=(const char *)names+offset;
        const char *end=memchr(text,0,names_size-offset);
        if(!end) return FALSE;
        if((size_t)(end-text)==wanted && !memcmp(text,name,wanted)) {
            unsigned variations=u32(rows+i*24u+4u);
            uint8_t *variants=*(uint8_t **)(rows+i*24u+0x10u);
            unsigned sounds=u32(bank+0x18u);
            uint8_t *sound_rows=*(uint8_t **)(bank+0x34u);
            if(!variations || variations>256u || !memory(variants,variations*16u,FALSE) ||
                !sounds || sounds>8192u || !memory(sound_rows,sounds*80u,FALSE)) return FALSE;
            for(unsigned j=0;j<variations;++j) {
                uint32_t key=u32(variants+j*16u); BOOL found=FALSE;
                for(unsigned k=0;k<sounds;++k) if(u32(sound_rows+k*80u)==key) { found=TRUE; break; }
                if(!found) return FALSE;
            }
            *index=i; return TRUE;
        }
    }
    return FALSE;
}
static BOOL play_available(const AudioOwner *a) {
    unsigned categories=*(uint16_t *)(a->manager+0x12cu);
    unsigned current=*(uint16_t *)(a->manager+0x12eu);
    uint8_t *categories_data=*(uint8_t **)(a->manager+0x128u);
    unsigned free_slots=0;
    if(!categories || categories>256u || current>=categories ||
        !memory(categories_data,categories*28u,FALSE) || categories_data[current*28u+0x18u] ||
        !u32(a->pool+0xcu) || u32(a->pool+0xcu)>a->count ||
        u32(a->pool+0xcu)<=u32(a->emitter+0xcu)) return FALSE;
    for(unsigned i=0;i<a->count;++i) if(!(a->entries[i*24u]&63u)) ++free_slots;
    if(!free_slots) return FALSE;
    unsigned params=u32(a->base+0x3c37d4u);
    uint8_t *rows=*(uint8_t **)(a->base+0x3c2374u);
    if(!params || params>64u || !memory(rows,params*24u,TRUE) ||
        !memory(a->base+0x3c2370u,4u,TRUE)) return FALSE;
    for(unsigned i=0;i<params;++i) {
        const char *name=*(const char **)(rows+i*24u+20u); float value;
        memcpy(&value,rows+i*24u+12u,4u);
        if(!memory(name,64u,FALSE) || !memchr(name,0,64u) || !isfinite(value)) return FALSE;
    }
    return TRUE;
}
typedef unsigned (__attribute__((stdcall)) *StopCue)(void *,unsigned,unsigned,void *);
__attribute__((naked,noinline,used))
static unsigned play_unattached_cue(void *entry __attribute__((unused)),
    void *bank __attribute__((unused)),unsigned index __attribute__((unused)),
    void **cue __attribute__((unused))) {
    /* Native687AD0 additionally consumes EAX as its optional cue attachment.
     * It is NOT a plain five-argument stdcall. Its retail caller68AF48 zeros
     * EAX when emitter+8 is null (required by owner above), then supplies the
     * five stack arguments and receives RET20. Preserve that register input;
     * an ordinary C indirect call can leave the function address in EAX. */
    __asm__ volatile("pushl %ebp\n\tmovl %esp,%ebp\n\tpushl %ebx\n\t"
        "movl 8(%ebp),%ebx\n\tpushl $0\n\tpushl $0\n\t"
        "pushl 20(%ebp)\n\tpushl 16(%ebp)\n\tpushl 12(%ebp)\n\t"
        "xorl %eax,%eax\n\tcall *%ebx\n\tpopl %ebx\n\tleave\n\tret\n\t");
}
__attribute__((naked,noinline,used))
static uint32_t register_cue(void *pool __attribute__((unused)),
    void *cue __attribute__((unused)),void *bank __attribute__((unused)),
    void *entry __attribute__((unused))) {
    /* cdecl bridge; native receives ESI=tracker and four ret16 stack args.
     * The output handle lives in this bridge's stack only during the call. */
    __asm__ volatile("pushl %esi\n\tsubl $4,%esp\n\t"
        "movl 12(%esp),%esi\n\tmovl $0,(%esp)\n\tleal (%esp),%eax\n\t"
        "pushl $1\n\tpushl 24(%esp)\n\tpushl 24(%esp)\n\tpushl %eax\n\t"
        "call *40(%esp)\n\tmovl (%esp),%eax\n\taddl $4,%esp\n\tpopl %esi\n\tret\n\t");
}
static BOOL exact_voice(const AudioOwner *a,uint8_t **entry) {
    if(a->bank!=voice.bank || a->soundbank!=voice.soundbank || a->pool!=voice.pool) return FALSE;
    if(!voice.handle) { *entry=NULL; return voice.cue!=NULL; }
    unsigned slot=voice.handle&255u;
    if(slot>=a->count) return FALSE;
    uint8_t *e=a->entries+slot*24u;
    if(u32(e+8u)!=(voice.handle>>8) || !(e[0]&63u)) {
        /* Positive native retirement/generation change: this handle no
         * longer grants access to the old cue. Never dereference it again. */
        voice.handle=0; voice.cue=NULL; voice.cue_stopped=FALSE; *entry=NULL; return TRUE;
    }
    if((e[0]&63u)!=1u && (e[0]&63u)!=2u) return FALSE;
    if(*(void **)(e+0x10u)!=voice.bank || *(void **)(e+0xcu)!=voice.cue) return FALSE;
    *entry=e; return TRUE;
}
static BOOL stop_owned(SudekiMpLanStoryCinematicExact exact,void *context) {
    AudioOwner a,b; uint8_t *entry=NULL;
    if(!voice.cue && !voice.handle) return TRUE;
    if(!exact || !exact(context) || !owner(&a) || !methods(a.base) ||
        !exact_voice(&a,&entry)) return FALSE;
    if(!voice.cue) return TRUE;
    if(entry && (entry[0]&63u)==2u) {
        voice.handle=0; voice.cue=NULL; voice.cue_stopped=FALSE; return TRUE;
    }
    if(!voice.cue_stopped) {
        if(!memory(voice.cue,0x30u,FALSE) || *(void **)(voice.cue+8u)!=a.soundbank ||
            !exact(context) || !owner(&b) || !same_owner(&a,&b)) return FALSE;
        /* A failed creation may have produced a cue before normal tracker
         * registration. Without its completion bit, no positive disposal
         * contract is known: retain it instead of declaring cleanup done. */
        if(!voice.handle && !(voice.cue[0x2cu]&1u)) return FALSE;
        /* Stop's exact callback can free the cue immediately. Record that
         * return before any fallible observation, so a later retry never
         * dereferences or stops the old cue twice. */
        unsigned result=((StopCue)(a.base+0x280c30u))(a.soundbank,UINT32_MAX,0,voice.cue);
        if(result) return FALSE;
        voice.cue_stopped=TRUE;
    }
    if(voice.handle) {
        typedef void (__attribute__((stdcall)) *RetireHandle)(void *,uint32_t);
        if(!exact(context) || !owner(&b) || !same_owner(&a,&b)) return FALSE;
        ((RetireHandle)(a.base+0x284ae0u))(a.pool,voice.handle);
        /* Native Stop may already have freed the cue. Only the retained
         * tracker slot is read after this point; no post-stop cue access. */
        if(u32(entry+8u)!=(voice.handle>>8) || (entry[0]&63u)!=2u) return FALSE;
    }
    voice.handle=0; voice.cue=NULL; voice.cue_stopped=FALSE; return exact(context);
}
BOOL SudekiMpLanStoryCinematicAudioRetains(void) { return voice.handle || voice.cue || voice.busy; }
BOOL SudekiMpLanStoryCinematicAudioStop(SudekiMpLanStoryCinematicExact exact,void *context) {
    if(voice.busy || (voice.thread && voice.thread!=GetCurrentThreadId())) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    voice.busy=TRUE; BOOL ok=stop_owned(exact,context); voice.busy=FALSE;
    SetLastError(ok?ERROR_SUCCESS:ERROR_RETRY); return ok;
}
BOOL SudekiMpLanStoryCinematicAudioPresent(const SudekiMpLanStoryPresentation *f,
    SudekiMpLanStoryCinematicExact exact,void *context) {
    AudioOwner a,b; unsigned index=0;
    if(!exact || !SudekiMpLanStoryPresentationValid(f) || voice.busy ||
        (voice.thread && voice.thread!=GetCurrentThreadId()) || !exact(context)) {
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    if(!(f->flags&SUDEKIMP_STORY_CUE_PRESENT)) return SudekiMpLanStoryCinematicAudioStop(exact,context);
    if(voice.attempted && voice.epoch==f->epoch && voice.line==f->line_serial) {
        SetLastError(voice.line_ok?ERROR_SUCCESS:ERROR_NOT_SUPPORTED); return voice.line_ok;
    }
    if(!SudekiMpLanStoryCinematicAudioStop(exact,context)) return FALSE;
    voice.thread=GetCurrentThreadId(); voice.epoch=f->epoch; voice.line=f->line_serial;
    voice.attempted=FALSE; voice.line_ok=FALSE;
    if(f->elapsed_ms>250u) { SetLastError(ERROR_NOT_SUPPORTED); return FALSE; }
    if(!owner(&a) || !methods(a.base) || !play_available(&a) || !cue_index(&a,f->cue,&index) ||
        !exact(context) || !owner(&b) || !same_owner(&a,&b)) {
        SetLastError(ERROR_RETRY); return FALSE;
    }
    voice.attempted=TRUE; voice.busy=TRUE; voice.bank=a.bank; voice.soundbank=a.soundbank; voice.pool=a.pool;
    /* Same native bank play and handle-registration stages used by PlayWav.
     * Retain the intermediate cue explicitly so failed native creation cannot
     * lose the object before it reaches the normal cue tracker. */
    unsigned result=play_unattached_cue(a.base+0x287ad0u,a.bank,index,(void **)&voice.cue);
    BOOL ok=!result && voice.cue && exact(context) && owner(&b) && same_owner(&a,&b) &&
        memory(voice.cue,0x30u,FALSE) && *(void **)(voice.cue+8u)==a.soundbank &&
        u32(voice.cue+4u)==index && play_available(&a);
    if(ok) {
        voice.handle=register_cue(a.pool,voice.cue,a.bank,a.base+0x2848a0u);
        uint8_t *entry=NULL;
        ok=voice.handle && exact(context) && owner(&b) && same_owner(&a,&b) &&
            exact_voice(&b,&entry) && entry && (entry[0]&63u)==1u;
    }
    voice.line_ok=ok; voice.busy=FALSE;
    if(!ok) { SetLastError(ERROR_RETRY); return FALSE; }
    SudekiMpLogFormat("story_cinematic event=voice_started epoch=%lu line=%lu elapsed_ms=%lu owned_handle=1\r\n",
        (unsigned long)f->epoch,(unsigned long)f->line_serial,(unsigned long)f->elapsed_ms);
    SetLastError(ERROR_SUCCESS); return TRUE;
}
BOOL SudekiMpLanStoryCinematicAudioReset(void) {
    if(SudekiMpLanStoryCinematicAudioRetains() ||
        (voice.thread && voice.thread!=GetCurrentThreadId())) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    memset(&voice,0,sizeof(voice)); SetLastError(ERROR_SUCCESS); return TRUE;
}
