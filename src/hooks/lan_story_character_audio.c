#include "hooks/lan_story_character_audio.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <math.h>
#include <string.h>
#if !defined(__GNUC__) || !defined(__i386__)
#error "Character audio requires the supported native x86 ABI"
#endif
static uint8_t *base;
static DWORD native_thread;
static BOOL host_mode,busy,bound;
static BOOL position_pending[4];
static SudekiMpInlineHook play_hook;
_Static_assert(offsetof(SudekiMpInlineHook,trampoline)==40,"native bridge trampoline offset");
static SudekiMpLanStoryNativeRoster host_roster;
static uint32_t stream;
static unsigned traces;
static const struct {const char *name;unsigned rva,size;uint32_t hash;} codes[]={
    {"AttachmentCreate",0x289df0u,158u,0x6d270c72u},
    {"AttachmentConstruct",0x281a00u,106u,0xeea4d590u},
    {"AttachmentPosition",0x284520u,97u,0x5211f40eu},
    {"AttachmentSetPosition",0x281860u,111u,0x635cc222u},
    {"AttachmentSetVelocity",0x2817f0u,111u,0x5a9d9e86u},
    {"ActorSoundTransform",0xfc560u,178u,0x629d0c6cu},
    {"SoundLocator",0x21bd20u,27u,0xe4665c75u},
    {"AttachedSoundBuffer",0x22e940u,7u,0x16c69a23u},
    {"ActorPlayCue",0xfc200u,323u,0x7b4dab60u},
    {"ActorSoundConstruct",0xfbea0u,238u,0xb13ef130u},
    {"EmitterPlay",0x28aeb0u,274u,0x7ed92b2du},
    {"BankPlay",0x287ad0u,640u,0xcaffac9au},
    {"BankLookup",0x280b90u,108u,0xf561ea5fu},
    {"CueRegister",0x2848a0u,231u,0xea4e3e69u},
    {"CueRetire",0x284ae0u,122u,0x579db7ccu},
    {"CueStop",0x280c30u,102u,0xc2c94d96u},
    {"CuePlay",0x280f40u,1447u,0x17975b02u},
    {"CueCreate",0x281610u,49u,0x8aa80e9cu},
    {"BankGet",0x287d90u,9u,0x2105a5c9u},
    {"BankPrepare",0x2871b0u,127u,0x4ce313c5u},
    {"BankParameter",0x287aa0u,33u,0xadb85453u},
    {"AttachmentGet",0x2846f0u,30u,0xf5f56708u},
};
static const struct {unsigned rva,target;} relocations[]={
    {0x289e3bu,0x3c37d8u},
    {0x281a0au,0x2e1ed8u},
    {0x281a18u,0x2e1f34u},
    {0x281a44u,0x3c2ed8u},
    {0x281a61u,0x2c017cu},
    {0xfc215u,0x31f297u},
    {0xfc21bu,0x409d7cu},
    {0xfc2e3u,0x2e3600u},
    {0xfc31du,0x31e5f0u},
    {0xfc322u,0x31e5e0u},
    {0xfbea5u,0x2c4c34u},
    {0xfbebcu,0x2c553cu},
    {0xfbeccu,0x2c54f4u},
    {0xfbed2u,0x2cd0acu},
    {0xfbed9u,0x2cd0ecu},
    {0xfbee0u,0x2cd0f4u},
    {0xfbf02u,0x2cd024u},
    {0xfbf0eu,0x2e1db4u},
    {0xfbf2eu,0x2c3bf4u},
    {0x28aedbu,0x409e58u},
    {0x28af36u,0x409e8cu},
    {0x28af93u,0x409e8cu},
    {0x287b0au,0x3c37d4u},
    {0x287b29u,0x3c2370u},
    {0x287b2fu,0x3c2370u},
    {0x287b42u,0x3c2374u},
    {0x287c99u,0x3c37d4u},
    {0x284ae1u,0x409e8cu},
    {0x280c64u,0x3c37d8u},
    {0x280c81u,0x3c37d8u},
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
    {0x2846f8u,0x409e58u},
};
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
static BOOL code_exact(uint8_t *b,unsigned k) {
    const __typeof__(codes[0]) *code=&codes[k];
    uint32_t hash=2166136261u;
    if(!memory(b+code->rva,code->size,FALSE))return FALSE;
    for(unsigned i=0;i<code->size;++i) {
        unsigned rva=code->rva+i;uint8_t byte=b[rva];
        if(play_hook.installed && code->rva==0xfc200u && i<play_hook.length) byte=play_hook.original[i];
        for(unsigned j=0;j<sizeof(relocations)/sizeof(relocations[0]);++j) {
            unsigned offset=rva-relocations[j].rva;if(offset>=4u)continue;
            if(u32(b+relocations[j].rva)!=(uint32_t)(uintptr_t)(b+relocations[j].target))return FALSE;
            byte=(uint8_t)((0x400000u+relocations[j].target)>>(8u*offset));break;
        }
        hash=(hash^byte)*16777619u;
    }
    if(hash!=code->hash) {
        if(traces++<16)SudekiMpLogFormat("story_character_audio event=guard_failed name=%s\r\n",code->name);
        return FALSE;
    }
    return TRUE;
}
static BOOL methods(uint8_t *b) {
    if(!b) return FALSE;
    if(play_hook.installed && (play_hook.target!=b+0xfc200u ||
        memcmp(play_hook.target,play_hook.replacement,play_hook.length))) return FALSE;
    for(unsigned k=0;k<sizeof(codes)/sizeof(codes[0]);++k)
        if(!code_exact(b,k)) return FALSE;
    return *(void **)(b+0x2e1f6cu)==b+0x280b90u &&
        *(void **)(b+0x2e1f7cu)==b+0x281610u && *(void **)(b+0x2e1f80u)==b+0x280c30u;
}
static uint32_t digest(const char *s) {
    uint32_t hash=2166136261u;for(unsigned i=0;s[i];++i)hash=(hash^(uint8_t)s[i])*16777619u;return hash;
}
typedef struct AudioOwner {
    uint8_t *base,*component,*emitter,*bank,*soundbank,*manager,*pool,*entries,*backend;
    unsigned count;uint32_t bank_hash;
} AudioOwner;
static BOOL owner(uint8_t *bank,AudioOwner *out) {
    AudioOwner a={.base=base,.bank=bank};if(!base || !out)return FALSE;
    a.manager=*(uint8_t **)(base+0x409e58u);a.pool=*(uint8_t **)(base+0x409e8cu);
    if(!typed(base,a.manager,0x15cu,0x2e1dc0u) || !typed(base,a.pool,0x14u,0x2e1ee0u) ||
        !typed(base,bank,0x34u,0x2e1e40u) || !u32(bank+0x18u) || u32(bank+0x18u)>=0x7fffffffu || bank[0x30u])return FALSE;
    const char *name=*(const char **)(bank+0x10u);
    if(!memory(name,96,FALSE) || !memchr(name,0,96) || !*name)return FALSE;
    a.bank_hash=digest(name);
    unsigned banks=u32(a.manager+0x30u),matches=0;void **list=*(void ***)(a.manager+0x2cu);
    if(!banks || banks>4096 || !memory(list,banks*4u,FALSE))return FALSE;
    for(unsigned i=0;i<banks;++i)if(list[i]==bank)++matches;
    if(matches!=1)return FALSE;
    a.soundbank=*(uint8_t **)(bank+0x1cu);
    a.backend=*(uint8_t **)(a.manager+0x10u);
    if(!typed(base,a.soundbank,0x3cu,0x2e1f68u) || !u32(a.soundbank+4u) ||
        memcmp(a.soundbank+8u,"SSBN",4) || a.backend!=*(void **)(base+0x3c37d8u) ||
        !typed(base,a.backend,0xafbcu,0x2e1e90u))return FALSE;
    a.count=u32(a.pool+8u);a.entries=*(uint8_t **)(a.pool+0x10u);
    if(!a.count || a.count>256 || !memory(a.entries,a.count*24u,TRUE) ||
        !u32(a.pool+4u) || u32(a.pool+4u)>0xffffffu)return FALSE;
    *out=a;return TRUE;
}
static BOOL actor_owner(const SudekiMpLanStoryNativeRoster *roster,unsigned character,AudioOwner *out) {
    if(!roster || character>=4 || !(roster->available_mask&(1u<<character)))return FALSE;
    uint8_t *actor=roster->actors[character];if(!memory(actor,0x60u,FALSE))return FALSE;
    uint8_t *s=*(uint8_t **)(actor+0x5cu);AudioOwner a;
    if(!typed(base,s,0xe0u,0x2cd0acu) || *(void **)(s+0x10u)!=actor ||
        !typed(base,s+0x4cu,0x14u,0x2e1db4u) || s[0x5cu]!=1 ||
        *(void **)(s+0x54u)!=s+0x30u || !owner(*(uint8_t **)(s+0x50u),&a))return FALSE;
    for(unsigned i=0x80;i<0x90;i+=4) {float v;memcpy(&v,s+i,4);if(!isfinite(v))return FALSE;}
    a.component=s;a.emitter=s+0x4cu;*out=a;return TRUE;
}
static BOOL same_owner(const AudioOwner *a,const AudioOwner *b) {
    return a->bank==b->bank && a->soundbank==b->soundbank && a->pool==b->pool &&
        a->entries==b->entries && a->count==b->count && a->manager==b->manager && a->backend==b->backend;
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
static BOOL attachment_ready(const AudioOwner *a) {
    uint8_t *s=a->component,*attachment=*(uint8_t **)(s+0x30u);
    if(attachment) {
        if(!typed(base,attachment,0x54u,0x2e1f34u) || !u32(attachment+4u))return FALSE;
        unsigned matches=0;
        for(unsigned i=0;i<60;++i)if(*(void **)(a->manager+0x38u+4*i)==s+0x30u)++matches;
        return matches==1;
    }
    /* Native lazy creation can insert a 3D attachment, but may not evict a
     * different actor's attachment. Require a free manager slot and exact
     * writable linked-list insertion neighbors before native allocation. */
    if(u32(a->manager+0x130u)>=60)return FALSE;
    BOOL free_slot=FALSE;
    for(unsigned i=0;i<60;++i)if(!u32(a->manager+0x38u+4*i))free_slot=TRUE;
    uint8_t *head=*(uint8_t **)(a->backend+0xafacu);
    if(!free_slot || !memory(head,12,TRUE))return FALSE;
    uint8_t *next=*(uint8_t **)head,*previous=*(uint8_t **)(head+4);
    return memory(next,12,TRUE) && memory(previous,12,TRUE) &&
        *(void **)(next+4)==head && *(void **)previous==head;
}
static BOOL finite_vector(const void *p,unsigned n) {
    if(!memory(p,n*sizeof(float),FALSE)) return FALSE;
    for(unsigned i=0;i<n;++i) {
        float value;memcpy(&value,(const uint8_t *)p+i*4u,4u);
        if(!isfinite(value)) return FALSE;
    }
    return TRUE;
}
static BOOL affine_matrix(const float *m) {
    return finite_vector(m,16) && fabsf(m[3])<.0001f && fabsf(m[7])<.0001f &&
        fabsf(m[11])<.0001f && fabsf(m[15]-1)<.0001f;
}
/* Mirror ActorSoundTransform's origin selection from the presented actor.
 * Its native simulation callback is paused on a saved-story client. Resolve
 * an authored locator from already presented matrices, without advancing a
 * model, sound script, actor task or gameplay controller. */
static BOOL presented_origin(const AudioOwner *a,float out[3]) {
    uint8_t *actor=*(uint8_t **)(a->component+0x10u);
    if(!memory(actor,0x60,FALSE)) return FALSE;
    uint8_t *p=*(uint8_t **)(actor+0x44u);
    if(!typed(base,p,0xb8,0x2cdefcu) || *(void **)(p+0x10)!=actor ||
        (u32(p+0x94) && u32(p+0x94)!=4u) || !finite_vector(p+0x18,3)) return FALSE;
    unsigned index=u32(a->component+0xd4);
    if(!index) {memcpy(out,p+0x18,12);return TRUE;}
    uint8_t *w=*(uint8_t **)(p+0xb4);
    if(!memory(w,0x14,FALSE)) return FALSE;
    uint8_t *o=*(uint8_t **)(w+8),*r=*(uint8_t **)(w+0x10);
    if(!typed(base,o,0xd0,0x2dd700u) || !typed(base,r,0x10,0x2df8ecu) ||
        *(void **)(w+0xc)!=r || *(void **)(o+0x14)!=r || *(void **)(o+0x18) ||
        !affine_matrix((float *)(o+0x90))) return FALSE;
    uint8_t *model=*(uint8_t **)(r+8);
    if(!memory(model,0x54,FALSE)) return FALSE;
    uint8_t *header=*(uint8_t **)(model+0x1c),*rows=*(uint8_t **)(model+0x50);
    if(!memory(header,0x18,FALSE)) return FALSE;
    unsigned count=u32(header+0x14),nodes=u32(header+4);
    if(!count || count>256 || index>=count || !nodes || nodes>4096 ||
        !memory(rows,count*0x50u,FALSE)) return FALSE;
    unsigned bone=u32(rows+index*0x50u);
    float *poses=*(float **)(r+0xc),*world=(float *)(o+0x90);
    if(bone>=nodes || !memory(poses,nodes*64u,FALSE) || !affine_matrix(poses+bone*16u)) return FALSE;
    const float *v=poses+bone*16u+12u;
    for(unsigned i=0;i<3;++i) out[i]=v[0]*world[i]+v[1]*world[4+i]+v[2]*world[8+i]+world[12+i];
    return finite_vector(out,3);
}
static BOOL attachment_position_exact(const AudioOwner *a) {
    uint8_t *p=*(uint8_t **)(a->component+0x30);
    if(!p) return TRUE;
    if(!attachment_ready(a) || !memory(p,0x54,TRUE) ||
        *(void **)(base+0x2e1f38u)!=base+0x281860u ||
        *(void **)(base+0x2e1f3cu)!=base+0x2817f0u ||
        *(void **)(base+0x2e1eb0u)!=base+0x22e940u) return FALSE;
    uint8_t *head=*(uint8_t **)(p+0x48);
    unsigned count=u32(p+0x4c);
    if(count>256 || !memory(head,12,FALSE)) return FALSE;
    uint8_t *node=*(uint8_t **)head,*previous=head;
    for(unsigned i=0;i<count;++i) {
        if(node==head || !memory(node,12,FALSE) || *(void **)(node+4)!=previous) return FALSE;
        uint8_t *sound=*(uint8_t **)(node+8);
        if(!typed(base,sound,0x180,0x2e1ea8u)) return FALSE;
        void *buffer=*(void **)(sound+0x17c);
        if(buffer) {
            /* DirectSound owns this external COM object; its identity comes
             * from the exact native sound in this attachment's linked list. */
            if(!memory(buffer,4,FALSE) || !memory(*(void **)buffer,0x50,FALSE)) return FALSE;
        }
        previous=node;node=*(uint8_t **)node;
    }
    return node==head && *(void **)(head+4)==previous;
}
__attribute__((naked,noinline,used))
static unsigned position_attachment(void *entry __attribute__((unused)),void *tuple __attribute__((unused))) {
    __asm__ volatile("pushl %esi\n\tmovl 12(%esp),%esi\n\tcall *8(%esp)\n\tpopl %esi\n\tret\n\t");
}
static BOOL refresh_position(unsigned character,const AudioOwner *a,SudekiMpStoryCharacterAudioExact exact,void *context) {
    float xyz[3];uint8_t *s=a->component;
    if(!exact || !exact(context) || !presented_origin(a,xyz) ||
        !memory(s,0xe0,TRUE) || !finite_vector(s+0x40,3)) return FALSE;
    if(!position_pending[character] && !memcmp(s+0x84,xyz,12) && !memcmp(s+0x34,xyz,12)) return TRUE;
    /* Only these small leaves are needed while moving an existing emitter.
     * Full play validation remains at cue creation, not every idle frame. */
    for(unsigned i=0;i<sizeof(codes)/sizeof(codes[0]);++i) {
        unsigned r=codes[i].rva;
        if((r==0xfc560u || r==0x21bd20u || r==0x284520u || r==0x281860u ||
                r==0x2817f0u || r==0x22e940u) && !code_exact(base,i)) return FALSE;
    }
    if(!attachment_position_exact(a) || !exact(context)) return FALSE;
    position_pending[character]=TRUE;
    memcpy(s+0x84,xyz,12);memcpy(s+0x34,xyz,12);
    if(*(void **)(s+0x30) && position_attachment(base+0x284520u,s+0x30)) return FALSE;
    position_pending[character]=FALSE;
    return exact(context);
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

static const char *cue_name(const AudioOwner *a,unsigned index) {
    uint8_t *sb=a->soundbank;unsigned count=u32(sb+0x1cu),size=u32(sb+0x2cu);
    uint8_t *rows=*(uint8_t **)(sb+0x38u),*names=*(uint8_t **)(sb+0x30u);
    if(!count || count>8192 || index>=count || !size || size>1048576 ||
        !memory(rows,count*24u,FALSE) || !memory(names,size,FALSE))return NULL;
    unsigned offset=u32(rows+index*24u);if(offset>=size)return NULL;
    const char *name=(const char *)names+offset;unsigned n=size-offset;if(n>96)n=96;
    if(!memchr(name,0,n) || !*name)return NULL;
    unsigned verified=0;if(!cue_index(a,name,&verified) || verified!=index)return NULL;
    return name;
}
/* 0: positively retired; 1: current; -1: unknown. Never dereference an old
 * cue after its native generation changes, including after synchronous Stop. */
static int handle_state(const AudioOwner *a,uint32_t handle,uint8_t *cue) {
    unsigned slot=handle&255u;if(!handle || slot>=a->count)return -1;
    uint8_t *e=a->entries+slot*24u;unsigned state=e[0]&63u;
    if(u32(e+8u)!=(handle>>8) || !state || state==2)return 0;
    if(state!=1 || *(void **)(e+0x10u)!=a->bank || (cue && *(void **)(e+0xcu)!=cue))return -1;
    return 1;
}
typedef struct HostCue {SudekiMpStoryAudioCue wire;uint8_t *bank,*cue;} HostCue;
static HostCue host_cues[4][SUDEKIMP_STORY_AUDIO_CAPACITY];
static BOOL host_overflow[4];
static uint32_t host_bank[4];
static void __attribute__((cdecl,noinline,used)) observe_play(void *component,const char *name,uint32_t *output,
    uint32_t mode,uint32_t gain,uint32_t spatial) {
    if(!host_mode || !bound || busy || !native_thread || native_thread!=GetCurrentThreadId() ||
        !SudekiMpLanStoryObserverNativeRosterExact(&host_roster) || !memory(output,4,FALSE) || !*output ||
        mode>1 || spatial>1 || !memory(name,96,FALSE) || !memchr(name,0,96))return;
    for(unsigned c=0;c<4;++c) {
        AudioOwner a;if(!actor_owner(&host_roster,c,&a) || a.component!=component)continue;
        unsigned index=0;if(!cue_index(&a,name,&index) || handle_state(&a,*output,NULL)!=1)return;
        uint8_t *cue=*(uint8_t **)(a.entries+(*output&255u)*24u+0xcu);
        if(!memory(cue,0x30,FALSE) || *(void **)(cue+8)!=a.soundbank || u32(cue+4)!=index)return;
        SudekiMpStoryAudioCue row={*output,GetTickCount(),digest(name),gain,(uint16_t)index,(uint8_t)mode,(uint8_t)spatial};
        float volume;memcpy(&volume,&gain,4);if(!isfinite(volume) || volume<0 || volume>1)return;
        for(unsigned i=0;i<SUDEKIMP_STORY_AUDIO_CAPACITY;++i) {
            HostCue *h=&host_cues[c][i];if(h->wire.id)continue;
            *h=(HostCue){row,a.bank,cue};
            if(traces++<300)SudekiMpLogFormat("story_character_audio event=observed character=%u cue=%s id=%lu\r\n",c,name,(unsigned long)row.id);
            return;
        }
        host_overflow[c]=TRUE;return;
    }
}
/* Native ECX=component, ESI=out-handle, four stack arguments, RET16.
 * Forward first, then observe the returned handle; never synthesize host audio.
 * The physical detour/trampoline stays process-lifetime pinned. */
__attribute__((naked,noinline,used)) static void play_detour(void) {
    __asm__ volatile("pushl %ebp\n\tmovl %esp,%ebp\n\tpushl %ebx\n\tpushl %esi\n\tpushl %edi\n\t"
        "movl %ecx,%ebx\n\tmovl %esi,%edi\n\t"
        "pushl 20(%ebp)\n\tpushl 16(%ebp)\n\tpushl 12(%ebp)\n\tpushl 8(%ebp)\n\t"
        "call *_play_hook+40\n\tpushl %eax\n\t"
        "pushl 20(%ebp)\n\tpushl 16(%ebp)\n\tpushl 12(%ebp)\n\tpushl %edi\n\tpushl 8(%ebp)\n\tpushl %ebx\n\t"
        "call _observe_play\n\taddl $24,%esp\n\tpopl %eax\n\tpopl %edi\n\tpopl %esi\n\tpopl %ebx\n\tleave\n\tret $16\n\t");
}
__attribute__((naked,noinline,used)) static void invoke_play(void *entry __attribute__((unused)),
    void *component __attribute__((unused)),const char *name __attribute__((unused)),
    uint32_t *output __attribute__((unused)),uint32_t mode __attribute__((unused)),
    uint32_t gain __attribute__((unused)),uint32_t spatial __attribute__((unused))) {
    __asm__ volatile("pushl %ebp\n\tmovl %esp,%ebp\n\tpushl %esi\n\t"
        "movl 12(%ebp),%ecx\n\tmovl 20(%ebp),%esi\n\t"
        "pushl 32(%ebp)\n\tpushl 28(%ebp)\n\tpushl 24(%ebp)\n\tpushl 16(%ebp)\n\t"
        "call *8(%ebp)\n\tpopl %esi\n\tleave\n\tret\n\t");
}
BOOL SudekiMpStoryCharacterAudioInitialize(HMODULE image,BOOL host) {
    if(busy || SudekiMpStoryCharacterAudioRetains() || !image || !SudekiMpCheckLoadedExecutable(image) ||
        !methods((uint8_t *)image))return FALSE;
    base=(uint8_t *)image;host_mode=host;bound=FALSE;native_thread=0;
    if(host && !play_hook.installed) {
        HMODULE pinned;
        if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            (LPCSTR)&play_detour,&pinned))return FALSE;
        uint8_t expected[7];memcpy(expected,base+0xfc200u,7);
        if(!SudekiMpInstallInlineHook(&play_hook,base+0xfc200u,expected,7,play_detour))return FALSE;
    }
    SudekiMpLogFormat("story_character_audio event=initialized host=%u guards=exact\r\n",host);return TRUE;
}
void SudekiMpStoryCharacterAudioClose(void) {bound=FALSE;}
void SudekiMpStoryCharacterAudioBind(const SudekiMpLanStoryNativeRoster *roster) {
    if(!host_mode || !roster || (native_thread && native_thread!=GetCurrentThreadId()) ||
        !SudekiMpLanStoryObserverNativeRosterExact(roster)) {bound=FALSE;return;}
    native_thread=GetCurrentThreadId();
    if(!stream || host_roster.epoch!=roster->epoch || memcmp(host_roster.actors,roster->actors,sizeof(roster->actors))) {
        if(stream==UINT32_MAX) {bound=FALSE;return;}
        ++stream;memset(host_bank,0,sizeof(host_bank));memset(host_cues,0,sizeof(host_cues));memset(host_overflow,0,sizeof(host_overflow));
    }
    host_roster=*roster;bound=TRUE;
}
BOOL SudekiMpStoryCharacterAudioCapture(unsigned character,SudekiMpStoryAudio *out) {
    AudioOwner a;
    if(!out || character>=4 || !bound || !host_mode || native_thread!=GetCurrentThreadId() ||
        !SudekiMpLanStoryObserverNativeRosterExact(&host_roster) || !actor_owner(&host_roster,character,&a))return FALSE;
    if(host_bank[character] && host_bank[character]!=a.bank_hash)return FALSE;
    host_bank[character]=a.bank_hash;
    SudekiMpStoryAudio s={.epoch=host_roster.epoch,.revision=host_roster.revision,.tick=GetTickCount(),
        .stream=stream,.bank=a.bank_hash,.character=(uint8_t)character};
    for(unsigned i=0;i<SUDEKIMP_STORY_AUDIO_CAPACITY;++i) {
        HostCue *h=&host_cues[character][i];if(!h->wire.id)continue;
        if(h->bank!=a.bank)return FALSE;
        int status=handle_state(&a,h->wire.id,h->cue);if(status<0)return FALSE;
        if(!status) {memset(h,0,sizeof(*h));continue;}
        s.cues[s.count++]=h->wire;
    }
    if(host_overflow[character]) {if(!s.count)host_overflow[character]=FALSE;return FALSE;}
    if(!SudekiMpStoryAudioValid(&s))return FALSE;
    *out=s;return TRUE;
}
typedef struct LocalCue {
    uint32_t remote,handle;uint8_t *bank,*soundbank,*pool,*cue;BOOL stopped;
} LocalCue;
static LocalCue local[4][SUDEKIMP_STORY_AUDIO_CAPACITY];
static uint32_t local_stream[4],local_epoch[4];
static const char *last_refusal[4];
static BOOL playback_refused(unsigned character,const char *reason,unsigned count) {
    if(character<4 && last_refusal[character]!=reason) {
        last_refusal[character]=reason;
        if(traces++<300) SudekiMpLogFormat(
            "story_character_audio event=playback_wait character=%u cues=%u reason=%s\r\n",
            character,count,reason);
    }
    return FALSE;
}
BOOL SudekiMpStoryCharacterAudioRetains(void) {
    if(busy)return TRUE;
    for(unsigned c=0;c<4;++c)for(unsigned i=0;i<SUDEKIMP_STORY_AUDIO_CAPACITY;++i)
        if(local[c][i].handle || local[c][i].cue)return TRUE;
    return FALSE;
}
static BOOL positively_retired(const LocalCue *v) {
    if(!v->handle || !base || *(void **)(base+0x409e8cu)!=v->pool ||
        !typed(base,v->pool,0x14,0x2e1ee0u))return FALSE;
    unsigned count=u32(v->pool+8);uint8_t *entries=*(uint8_t **)(v->pool+0x10);
    unsigned slot=v->handle&255u;
    if(!count || count>256 || slot>=count || !memory(entries,count*24u,FALSE))return FALSE;
    uint8_t *e=entries+slot*24u;unsigned state=e[0]&63u;
    return u32(e+8)!=(v->handle>>8) || !state || state==2;
}
static BOOL stop_cue(LocalCue *v,SudekiMpStoryCharacterAudioExact exact,void *context) {
    if(!v->handle && !v->cue)return TRUE;
    /* A zone can retire a bank before its next presentation callback. The
     * retained pool generation proves retirement without touching freed bank
     * or cue storage. Unknown/replaced pool still retains cleanup debt. */
    if(exact && exact(context) && positively_retired(v)) {
        v->handle=0;v->cue=NULL;v->stopped=FALSE;return TRUE;
    }
    AudioOwner a,b;
    if(!exact || !exact(context) || !owner(v->bank,&a) || !methods(base) ||
        a.soundbank!=v->soundbank || a.pool!=v->pool)return FALSE;
    int state=handle_state(&a,v->handle,v->cue);if(state<0)return FALSE;
    if(!state) {v->handle=0;v->cue=NULL;v->stopped=FALSE;return TRUE;}
    if(!v->stopped) {
        if(!v->cue)v->cue=*(uint8_t **)(a.entries+(v->handle&255u)*24u+0xcu);
        if(!memory(v->cue,0x30,FALSE) || *(void **)(v->cue+8)!=a.soundbank || !exact(context))return FALSE;
        typedef unsigned (__attribute__((stdcall)) *Stop)(void *,unsigned,unsigned,void *);
        if(((Stop)(base+0x280c30u))(a.soundbank,UINT32_MAX,0,v->cue))return FALSE;
        v->stopped=TRUE; /* Stop may free cue immediately: no further cue reads. */
    }
    if(!exact(context) || !owner(v->bank,&b) || !same_owner(&a,&b))return FALSE;
    typedef void (__attribute__((stdcall)) *Retire)(void *,uint32_t);
    ((Retire)(base+0x284ae0u))(a.pool,v->handle);
    if(handle_state(&b,v->handle,v->cue)!=0)return FALSE;
    v->handle=0;v->cue=NULL;v->stopped=FALSE;return TRUE;
}
BOOL SudekiMpStoryCharacterAudioStop(unsigned character,SudekiMpStoryCharacterAudioExact exact,void *context) {
    if(character>=4 || busy || (native_thread && native_thread!=GetCurrentThreadId()))return FALSE;
    BOOL ok=TRUE;busy=TRUE;
    for(unsigned i=0;i<SUDEKIMP_STORY_AUDIO_CAPACITY;++i) {
        LocalCue *v=&local[character][i];if(!stop_cue(v,exact,context))ok=FALSE;
        else memset(v,0,sizeof(*v));
    }
    busy=FALSE;return ok;
}
static BOOL listener_exact(void) {
    uint8_t *manager=*(uint8_t **)(base+0x409d7cu);
    if(!typed(base,manager,0x60,0x2c7b80u))return FALSE;
    uint8_t *camera=*(uint8_t **)(manager+0x20u);
    if(!typed(base,camera,0xbc,0x2cce5cu))return FALSE;
    unsigned found=0;for(unsigned i=0;i<10;++i)if(*(void **)(manager+0x24+4*i)==camera)++found;
    uint8_t *state=*(uint8_t **)(camera+0x34);
    if(found!=1 || !typed(base,state,0xdc,0x2dd638u))return FALSE;
    for(unsigned i=0xc0;i<0xcc;i+=4){float f;memcpy(&f,state+i,4);if(!isfinite(f))return FALSE;}
    return TRUE;
}
BOOL SudekiMpStoryCharacterAudioPresent(const SudekiMpLanStoryNativeRoster *roster,const SudekiMpStoryAudio *s,
    SudekiMpStoryCharacterAudioExact exact,void *context) {
    if(!roster || !SudekiMpStoryAudioValid(s) || !exact || !exact(context) || host_mode || busy ||
        (native_thread && native_thread!=GetCurrentThreadId()))return FALSE;
    native_thread=GetCurrentThreadId();unsigned c=s->character;
    if(local_stream[c]!=s->stream || local_epoch[c]!=s->epoch) {
        if(!SudekiMpStoryCharacterAudioStop(c,exact,context))return FALSE;
        local_stream[c]=s->stream;local_epoch[c]=s->epoch;
    }
    BOOL ok=TRUE;busy=TRUE;
    /* Retire missing remote owners first. A retransmit retains consumed IDs
     * even after their local cue naturally completes: no replay or overlap. */
    for(unsigned i=0;i<SUDEKIMP_STORY_AUDIO_CAPACITY;++i) {
        LocalCue *v=&local[c][i];if(!v->remote)continue;
        BOOL present=FALSE;for(unsigned j=0;j<s->count;++j)if(s->cues[j].id==v->remote)present=TRUE;
        if(!present) {if(!stop_cue(v,exact,context))ok=FALSE;else memset(v,0,sizeof(*v));}
    }
    if(!ok) {busy=FALSE;return FALSE;}
    AudioOwner a,b;
    if(!actor_owner(roster,c,&a) || a.bank_hash!=s->bank) {
        busy=FALSE;return playback_refused(c,"actor_bank_owner",s->count);
    }
    if(!refresh_position(c,&a,exact,context)) {
        busy=FALSE;return playback_refused(c,"presented_sound_position",s->count);
    }
    BOOL native_ready=FALSE;
    for(unsigned j=0;j<s->count;++j) {
        const SudekiMpStoryAudioCue *cue=&s->cues[j];LocalCue *slot=NULL;BOOL seen=FALSE;
        for(unsigned i=0;i<SUDEKIMP_STORY_AUDIO_CAPACITY;++i) {
            if(local[c][i].remote==cue->id)seen=TRUE;
            if(!slot && !local[c][i].remote)slot=&local[c][i];
        }
        if(seen)continue;
        const char *name=cue_name(&a,cue->index);
        if(!slot || !name || digest(name)!=cue->hash) {
            ok=playback_refused(c,"cue_identity_or_capacity",s->count);break;
        }
        /* Late join drops old one-shots; a live native shield loop may join. */
        if((uint32_t)(s->tick-cue->started)>250u && strcmp(name,"BlockStart")) {slot->remote=cue->id;continue;}
        if(!native_ready) {
            native_ready=methods(base) && listener_exact();
            if(!native_ready) {ok=playback_refused(c,"native_methods_or_listener",s->count);break;}
        }
        if(!play_available(&a)) {ok=playback_refused(c,"native_play_capacity",s->count);break;}
        if(!attachment_ready(&a)) {ok=playback_refused(c,"native_attachment",s->count);break;}
        if(!exact(context) || !actor_owner(roster,c,&b) ||
            !same_owner(&a,&b) || a.component!=b.component) {
            ok=playback_refused(c,"presentation_owner_changed",s->count);break;
        }
        *slot=(LocalCue){.remote=cue->id,.bank=a.bank,.soundbank=a.soundbank,.pool=a.pool};
        invoke_play(base+0xfc200u,a.component,name,&slot->handle,cue->mode,cue->gain,cue->spatial);
        /* Output is published before subsequent fallible checks, preserving
         * the obligation to retire the exact returned handle on any failure. */
        if(!slot->handle) {ok=playback_refused(c,"native_returned_no_handle",s->count);continue;}
        if(!exact(context) || !owner(slot->bank,&b) || !same_owner(&a,&b) ||
            handle_state(&b,slot->handle,NULL)!=1) {
            ok=playback_refused(c,"returned_handle_owner",s->count);break;
        }
        slot->cue=*(uint8_t **)(b.entries+(slot->handle&255u)*24u+0xcu);
        if(!memory(slot->cue,0x30,FALSE) || *(void **)(slot->cue+8)!=a.soundbank || u32(slot->cue+4)!=cue->index) {ok=FALSE;break;}
        if(traces++<300)SudekiMpLogFormat("story_character_audio event=played character=%u cue=%s remote=%lu local=%lu\r\n",
            c,name,(unsigned long)cue->id,(unsigned long)slot->handle);
        last_refusal[c]=NULL;
    }
    busy=FALSE;return ok;
}
