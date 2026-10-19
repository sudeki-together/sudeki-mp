#include "hooks/lan_story_character_audio.h"
#include "hooks/call_hook.h"
#include "hooks/lan_arena_spirit_audio.h"
#include "hooks/lan_story_shot_cues.h"
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
typedef struct NativeCode {const char *name;unsigned rva,size;uint32_t hash;} NativeCode;
typedef struct NativeRelocation {unsigned rva,target;} NativeRelocation;
static const NativeCode codes[]={
    {"SoundBankNameId",0x16550u,53u,0x565d9efcu},
    {"SoundBankSet",0xfc3b0u,93u,0x03b9dde6u},
    {"EmitterBankSet",0x28ae50u,95u,0xf2449934u},
    {"ManagerBankFind",0x289a10u,63u,0xf5f92246u},
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
static const NativeRelocation relocations[]={
    {0x28ae6au,0x409e58u},
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
static BOOL code_match(uint8_t *b,const NativeCode *code,const NativeRelocation *table,unsigned count) {
    uint32_t hash=2166136261u;
    if(!memory(b+code->rva,code->size,FALSE))return FALSE;
    for(unsigned i=0;i<code->size;++i) {
        unsigned rva=code->rva+i;uint8_t byte=b[rva];
        if(play_hook.installed && code->rva==0xfc200u && i<play_hook.length) byte=play_hook.original[i];
        for(unsigned j=0;j<count;++j) {
            unsigned offset=rva-table[j].rva;if(offset>=4u)continue;
            if(u32(b+table[j].rva)!=(uint32_t)(uintptr_t)(b+table[j].target))return FALSE;
            byte=(uint8_t)((0x400000u+table[j].target)>>(8u*offset));break;
        }
        hash=(hash^byte)*16777619u;
    }
    if(hash!=code->hash) {
        if(traces++<16)SudekiMpLogFormat("story_character_audio event=guard_failed name=%s\r\n",code->name);
        return FALSE;
    }
    return TRUE;
}
static BOOL code_exact(uint8_t *b,unsigned k) {
    return code_match(b,&codes[k],relocations,sizeof(relocations)/sizeof(relocations[0]));
}
/* Shot audio only (never gates character audio or Initialize): the record
 * teardown routines it calls, the native routines that write through or
 * reclaim what a record registers, and the reclaim contract handle_state
 * relies on. CONFIRMED_STATIC FNV under the codes[] normalisation. */
static const NativeCode shot_codes[]={
    {"AttachmentRelease",0x2893c0u,86u,0xeba6676fu},
    {"AttachmentDetachAll",0x281a70u,89u,0x97b6f9b1u},
    {"AttachmentIdleSweep",0x2897b0u,91u,0xa890ccd8u},
    {"AttachmentStatus",0x2816c0u,92u,0x14c71fd7u},
    {"AttachmentDelete",0x22ead0u,30u,0x900caf40u},
    {"AttachmentDestruct",0x281ce0u,119u,0x5f4f975du},
    {"HandleState",0x2847a0u,48u,0xc48e2947u},
    {"CueRecycle",0x284840u,94u,0x78fdd401u},
    {"CueNotify",0x284990u,116u,0x32d044deu},
};
static const NativeRelocation shot_relocations[]={
    {0x2893f5u,0x29a0d8u},{0x2897ebu,0x409e58u},
    {0x281ce4u,0x3c37d8u},{0x281cedu,0x2e1f34u},{0x281d51u,0x2e1ed8u},
};
/* Vtable entries and image literals the shot executor depends on. */
static const struct {unsigned rva,target;} shot_slots[]={
    {0x2e1db4u,0x22e060u},                       /* emitter [0] deleting destructor */
    {0x2e1e58u,0x288140u},{0x2e1e5cu,0x287e10u}, /* bank +0x18 load, +0x1C unload */
    {0x2e1f34u,0x22ead0u},{0x2e1f38u,0x281860u},{0x2e1f3cu,0x2817f0u}, /* attachment [0], +4, +8 */
    {0x2e1f54u,0x2816c0u},{0x2e1f5cu,0x281a70u}, /* attachment +0x20 status, +0x28 detach all */
};
static BOOL shot_methods(uint8_t *b) {
    if(!b) return FALSE;
    for(unsigned k=0;k<sizeof(shot_codes)/sizeof(shot_codes[0]);++k)
        if(!code_match(b,&shot_codes[k],shot_relocations,sizeof(shot_relocations)/sizeof(shot_relocations[0])))
            return FALSE;
    for(unsigned k=0;k<sizeof(shot_slots)/sizeof(shot_slots[0]);++k)
        if(*(void **)(b+shot_slots[k].rva)!=b+shot_slots[k].target) return FALSE;
    /* AttachmentRelease's import (KERNEL32 InterlockedDecrement, IAT RVA
     * 0x29A0D8) and the literals native CMissile passes. */
    HMODULE kernel=GetModuleHandleA("kernel32.dll");
    FARPROC decrement=kernel?GetProcAddress(kernel,"InterlockedDecrement"):NULL,bound;
    memcpy(&bound,b+0x29a0d8u,sizeof(bound));
    return decrement && bound==decrement &&
        !memcmp(b+0x2c621cu,"missiles.xsb",13u) && !memcmp(b+0x2d9154u,"_2D",4u);
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
static void bind_effect_bank(void *entry,void *sound,const char *name) {
    uintptr_t edx=(uintptr_t)name;
    __asm__ volatile("pushl %[sound]\n\tcall *%[entry]"
        : "+d"(edx) : [sound] "r"(sound), [entry] "r"(entry)
        : "eax","ecx","memory","cc");
}
BOOL SudekiMpStoryCharacterAudioBindEffect(const SudekiMpLanStoryNativeRoster *roster,
    unsigned character,void *component) {
    uint8_t *s=component;AudioOwner source,old;
    if(host_mode || busy || !base || (native_thread && native_thread!=GetCurrentThreadId()) ||
        !SudekiMpLanStoryObserverNativeRosterExact(roster) || !actor_owner(roster,character,&source) ||
        !typed(base,s,0xe0u,0x2cd0acu) || !memory(s,0xe0u,TRUE) ||
        !typed(base,s+0x4cu,0x14u,0x2e1db4u) || *(void **)(s+0x54u)!=s+0x30u || s[0x5cu]>1)
        return FALSE;
    if(*(void **)(s+0x50u)==source.bank && s[0x5cu]==1) return TRUE;
    /* Never release a foreign bank or its last reference. The initial clone
     * has no bank; a delayed setup may replace the still-shared generic bank. */
    uint8_t *previous=*(uint8_t **)(s+0x50u);
    if(previous && (!owner(previous,&old) || strcmp(*(char **)(previous+0x10u),"sfx_generic.xsb") ||
        (s[0x5cu] && u32(previous+0x18u)<=1u))) return FALSE;
    if(!previous && s[0x5cu])return FALSE;
    if(!methods(base) || u32(base+0x2c3d80u)!=0x42480000u)return FALSE;
    const char *name=*(const char **)(source.bank+0x10u);uint32_t id=0;
    for(unsigned i=0;name[i];++i) {
        uint8_t ch=(uint8_t)name[i];if(ch>='a' && ch<='z')ch-=32;
        id=(i&1u)?id*ch:id+ch;
    }
    if(id!=u32(source.bank+0x14u))return FALSE;
    /* The bank already belongs to this local actor. Native lookup must find
     * its unique resource ID; no arbitrary packet text or bank loading. */
    void **banks=*(void ***)(source.manager+0x2cu);unsigned count=u32(source.manager+0x30u),matches=0;
    for(unsigned i=0;i<count;++i) {
        uint8_t *b=banks[i];
        if(!typed(base,b,0x34u,0x2e1e40u))return FALSE;
        if(u32(b+0x14u)==u32(source.bank+0x14u))++matches;
    }
    if(matches!=1 || !SudekiMpLanStoryObserverNativeRosterExact(roster))return FALSE;
    bind_effect_bank(base+0xfc3b0u,s,*(const char **)(source.bank+0x10u));
    BOOL ok=*(void **)(s+0x50u)==source.bank && s[0x5cu]==1;
    if(ok)memcpy(s+0x80u,base+0x2c3d80u,4u); /* Native PlaySFXWithAll hearing radius. */
    if(traces++<300)SudekiMpLogFormat("story_character_audio event=effect_bank character=%u ready=%u\r\n",character,ok);
    return ok;
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
static unsigned slot_cells(const uint8_t *manager,const void *slot) {
    unsigned matches=0;
    for(unsigned i=0;i<60;++i)if(*(void *const *)(manager+0x38u+4*i)==slot)++matches;
    return matches;
}
/* `slot` is an emitter's attachment slot (attachment pointer, position,
 * cone orientation) registered in the manager's 60-cell table. */
static BOOL slot_ready(const uint8_t *manager,uint8_t *backend,uint8_t *slot) {
    uint8_t *attachment=*(uint8_t **)slot;
    if(attachment) {
        if(!typed(base,attachment,0x54u,0x2e1f34u) || !u32(attachment+4u))return FALSE;
        return slot_cells(manager,slot)==1;
    }
    /* Native lazy creation can insert a 3D attachment, but may not evict a
     * different actor's attachment. Require a free manager slot and exact
     * writable linked-list insertion neighbors before native allocation. */
    if(u32(manager+0x130u)>=60)return FALSE;
    BOOL free_slot=FALSE;
    for(unsigned i=0;i<60;++i)if(!u32(manager+0x38u+4*i))free_slot=TRUE;
    uint8_t *head=*(uint8_t **)(backend+0xafacu); /* backend proven by owner() */
    if(!free_slot || !memory(head,12,TRUE))return FALSE;
    uint8_t *next=*(uint8_t **)head,*previous=*(uint8_t **)(head+4);
    return memory(next,12,TRUE) && memory(previous,12,TRUE) &&
        *(void **)(next+4)==head && *(void **)previous==head;
}
static BOOL attachment_ready(const AudioOwner *a) {
    return slot_ready(a->manager,a->backend,a->component+0x30u);
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
/* An existing attachment and its whole sound list, before a native call
 * walks it (positioning, or detaching every sound). */
static BOOL slot_position_exact(const uint8_t *manager,uint8_t *backend,uint8_t *slot) {
    uint8_t *p=*(uint8_t **)slot;
    if(!p) return TRUE;
    if(!slot_ready(manager,backend,slot) || !memory(p,0x54,TRUE) ||
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
static BOOL attachment_position_exact(const AudioOwner *a) {
    return slot_position_exact(a->manager,a->backend,a->component+0x30u);
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
static void shot_initialize(void);
BOOL SudekiMpStoryCharacterAudioInitialize(HMODULE image,BOOL host) {
    /* A quarantined shot record (pinned, never reused) does not block a new
     * saved-story start; a retryable one does. */
    if(busy || SudekiMpStoryCharacterAudioRetains() || SudekiMpStoryShotAudioRetains() || !image ||
        !SudekiMpCheckLoadedExecutable(image) || !methods((uint8_t *)image))return FALSE;
    base=(uint8_t *)image;host_mode=host;bound=FALSE;native_thread=0;shot_initialize();
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
static unsigned listener_character=4u;
void SudekiMpStoryCharacterAudioSetLocalCharacter(unsigned character) {
    listener_character=character<4u?character:4u;
}
static uint16_t local_start[4];
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
/* The guarded native CueStop/CueRetire (a test may substitute a stub). */
static void *cue_stop_entry,*cue_retire_entry;
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
        if(((Stop)(cue_stop_entry?cue_stop_entry:base+0x280c30u))(a.soundbank,UINT32_MAX,0,v->cue))return FALSE;
        v->stopped=TRUE; /* Stop may free cue immediately: no further cue reads. */
    }
    if(!exact(context) || !owner(v->bank,&b) || !same_owner(&a,&b))return FALSE;
    typedef void (__attribute__((stdcall)) *Retire)(void *,uint32_t);
    ((Retire)(cue_retire_entry?cue_retire_entry:base+0x284ae0u))(a.pool,v->handle);
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
        local_stream[c]=s->stream;local_epoch[c]=s->epoch;local_start[c]=0;
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
    /* A native global one-shot: accept only the latest witnessed cast and
     * consume it once, even when the packet repeats after its natural end. */
    if(s->start_sequence>local_start[c]) {
        if((uint32_t)(s->tick-s->start_tick)<=250u) {
            if(!exact(context) || !SudekiMpLanArenaSpiritAudioReplayLocalCue((HMODULE)base,
                    SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_START)) {busy=FALSE;return FALSE;}
            SudekiMpLogFormat("story_character_audio event=spirit_start character=%u cast=%u\r\n",c,s->start_sequence);
        }
        local_start[c]=s->start_sequence;
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
        /* Late join drops old one-shots; a live native shield loop may join.
         * Another player's reload is consumed the same way: never played,
         * never replayed while the host still lists it. */
        if(((uint32_t)(s->tick-cue->started)>250u && strcmp(name,"BlockStart")) ||
            SudekiMpStoryAudioSilentForListener(c,listener_character,name)) {slot->remote=cue->id;continue;}
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

/* H1b: an interior door's spot cue. EnterInterior|SRSBS and ExitInterior|BSN
 * play GetSound()->PlayCue(cue) right before their fade (CONFIRMED_STATIC,
 * SOLWORLDM.gex); a client entering a house locally runs no door script.
 * CSound::PlayCue (0x17090) only does EmitterPlay(EAX = CSound+0x18 spot
 * emitter; cue, NULL, 1, 0, 0) and may be detoured by another owner, so the
 * guarded EmitterPlay is called with those same arguments: an unattached 2D
 * emitter, fire and forget (no handle is requested or kept). Client only. */
__attribute__((naked,noinline,used)) static void invoke_spot(void *entry __attribute__((unused)),
    void *emitter __attribute__((unused)),const char *name __attribute__((unused))) {
    __asm__ volatile("pushl %ebp\n\tmovl %esp,%ebp\n\t"
        "pushl $0\n\tpushl $0\n\tpushl $1\n\tpushl $0\n\tpushl 16(%ebp)\n\t"
        "movl 12(%ebp),%eax\n\tcall *8(%ebp)\n\tleave\n\tret\n\t");
}
/* The guarded native EmitterPlay (a test may substitute a recording stub). */
static void *spot_entry;
static BOOL spot_refused(const char *cue,const char *reason) {
    static unsigned spot_logs;
    if(spot_logs++<16u) SudekiMpLogFormat("story_character_audio event=spot_refused cue=%s reason=%s\r\n",cue?cue:"-",reason);
    return FALSE;
}
BOOL SudekiMpStoryCharacterAudioPlaySpot(const char *cue) {
    AudioOwner a; unsigned index=0;
    if(host_mode || busy || !base || !cue || !*cue || (native_thread && native_thread!=GetCurrentThreadId()))
        return spot_refused(cue,"owner_or_thread");
    uint8_t *sound=memory(base+0x408d40u,4,FALSE)?*(uint8_t **)(base+0x408d40u):NULL;
    if(!typed(base,sound,0x2cu,0x2c6238u) || *(void **)(sound+8u)!=base+0x2c6240u) return spot_refused(cue,"sound");
    uint8_t *emitter=*(uint8_t **)(sound+0x18u);
    if(!typed(base,emitter,0x14u,0x2e1db4u) || emitter[0x10u]!=1u || u32(emitter+8u)) return spot_refused(cue,"emitter");
    if(!methods(base) || !owner(*(uint8_t **)(emitter+4u),&a)) return spot_refused(cue,"bank");
    a.emitter=emitter;
    if(!cue_index(&a,cue,&index)) return spot_refused(cue,"cue");
    if(!play_available(&a)) return spot_refused(cue,"capacity");
    uint32_t generation=u32(a.pool+4u);
    busy=TRUE; invoke_spot(spot_entry?spot_entry:base+0x28aeb0u,emitter,cue); busy=FALSE;
    BOOL played=*(uint8_t **)(base+0x409e8cu)==a.pool && u32(a.pool+4u)!=generation;
    if(traces++<300) SudekiMpLogFormat("story_character_audio event=spot_played cue=%s played=%u\r\n",cue,(unsigned)played);
    return played;
}

/* Client shot audio (#54): ranged party weapons and Kurian Shooter missiles,
 * played by this game itself, positionally (the 'Final Fantasy way'). One
 * record copies one CMissile emitter/slot pair (travel +0x70/+0xA0, fire
 * +0x84/+0xBC; constructor 0x1861C0 inlines 0x28ACA0 and slot init 0x284620)
 * and drives it only with the routines CMissile uses (CONFIRMED_STATIC):
 * EmitterBankSet 0x28AE50 (inactive, image literal 'missiles.xsb'), EmitterPlay
 * 0x28AEB0 (name, &handle, 3D, 0, 0), AttachmentPosition 0x284520, CueStop
 * 0x280C30 + CueRetire 0x284AE0 (stop_cue), AttachmentDetachAll 0x281A70 and
 * AttachmentRelease 0x2893C0, plus the SetActive mirror (bank+0x18 +-1 and
 * emitter+0x10, 0x586C90), in the destructor's order (0x586311-0x5863C3).
 * The manager's attachment table stores &slot and native code writes NULL
 * through it (idle sweep 0x2897B0, eviction), so records are static and
 * *slot is always re-read. Travel and world fire stop on RETIRE, as the
 * destructor stops +0x98/+0x9C. Party fire drains to its natural end with a
 * 4 s cap: the client sees the host shot's termination, never its missile's
 * destruction (INFERENCE deviation). Client only; failure means silence. */
enum {SHOT_FREE,SHOT_BOUND,SHOT_PLAYING,SHOT_STOPPING,SHOT_RELEASING,SHOT_QUARANTINED};
enum {SHOT_FIRE=1u,SHOT_TRAVEL=2u};
enum {SHOT_SERVICE,SHOT_DRAIN,SHOT_STOP_ALL};
#define SHOT_RECORDS 16u          /* 0..7 travel, 8..15 fire */
#define SHOT_MISSILES_ID 0x17c6f516u /* SoundBankNameId("missiles.xsb"), 0x416550 */
#define SHOT_CELL_LIMIT 47u       /* keep 12 of the 60 attachment cells for native owners */
#define SHOT_RESERVE 5u           /* cue-pool entries and units left to everyone else */
#define SHOT_PARTY_FIRE_MS 4000u
#define SHOT_CUE_MS 10000u
typedef struct ShotRecord {
    uint8_t emitter[0x14],slot[0x1c];
    LocalCue cue;
    uint64_t space;
    uint32_t key,emitted,started,limit,index;
    float target[3];
    uint8_t role,world,character,item,kind,state,spatial,active,move_pending;
} ShotRecord;
_Static_assert(offsetof(ShotRecord,slot)==0x14 && offsetof(ShotRecord,cue)%4==0,"shot record layout");
static ShotRecord shot_records[SHOT_RECORDS];
typedef struct ShotWorldNames {
    uint64_t space;uint32_t key,emitted;uint8_t used,result;SudekiMpStoryShotCueNames names;
} ShotWorldNames;
static ShotWorldNames shot_world[8];
static unsigned shot_world_next,shot_logs,shot_lifecycle_logs;
static SudekiMpStoryShotAudioPlan shot_plan;
static uint64_t shot_space,shot_items_logged;
static uint32_t shot_last_present;
static BOOL shot_space_known,shot_presented,shot_pinned,shot_claim_probed,shot_world_probed;
static int shot_native_state=-1;
static const char *shot_last_reason;
/* Native entries a test may substitute (NULL: the guarded retail routine). */
static struct {void *bank_set,*play,*position,*detach,*release;} shot_entry;
static DWORD (WINAPI *shot_tick)(void);
static void *shot_native(void *test,unsigned rva) {return test?test:base+rva;}
static uint32_t shot_now(void) {return shot_tick?shot_tick():GetTickCount();}
static BOOL shot_log(void) {if(shot_logs>=400u)return FALSE;++shot_logs;return TRUE;}
static void shot_initialize(void) {
    SudekiMpStoryShotAudioPlanReset(&shot_plan);
    memset(shot_world,0,sizeof(shot_world));shot_world_next=0;
    shot_space=0;shot_space_known=shot_presented=FALSE;shot_last_present=0;
}
static BOOL shot_native_ready(void) {
    if(shot_native_state<0)shot_native_state=methods(base) && shot_methods(base);
    return shot_native_state==1;
}
/* Check #7: only a quarantined record can leave a native cell or pointer
 * into this module, so the module is pinned then, and only then. */
static void shot_pin(void) {
    HMODULE module;
    if(!shot_pinned && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        (LPCSTR)(const void *)shot_records,&module))shot_pinned=TRUE;
}
static BOOL shot_retryable(const ShotRecord *r) {
    return (r->state>=SHOT_BOUND && r->state<=SHOT_RELEASING) || (r->state==SHOT_QUARANTINED && !shot_pinned);
}
static const char *shot_role(const ShotRecord *r) {return r->role==SHOT_FIRE?"fire":"travel";}
static BOOL shot_quarantine(ShotRecord *r,const char *reason) {
    r->state=SHOT_QUARANTINED;shot_pin();
    if(shot_log())SudekiMpLogFormat("story_shot_audio event=quarantined role=%s source=%s character=%u item=%u "
        "key=%lu reason=%s pinned=%u\r\n",shot_role(r),r->world?"world":"party",r->character,r->item,
        (unsigned long)r->key,reason,(unsigned)shot_pinned);
    return FALSE;
}
static unsigned shot_refused(const SudekiMpStoryShotAudioAction *a,const char *reason,unsigned result) {
    if(shot_last_reason!=reason) {
        shot_last_reason=reason;
        if(shot_log())SudekiMpLogFormat("story_shot_audio event=%s type=%u character=%u item=%u kind=%u key=%lu reason=%s\r\n",
            result==SUDEKIMP_SHOT_AUDIO_DEFER?"deferred":"silent",a->type,a->character,a->item,a->vfx_kind,
            (unsigned long)a->key,reason);
    }
    return result;
}
static BOOL shot_finite(const float v[3]) {return isfinite(v[0]) && isfinite(v[1]) && isfinite(v[2]);}
static uint32_t bank_name_id(const char *name) {
    uint32_t id=0;
    for(unsigned i=0;name[i];++i) {uint8_t ch=(uint8_t)name[i];if(ch>='a' && ch<='z')ch-=32;id=(i&1u)?id*ch:id+ch;}
    return id;
}
static void **bank_list(const uint8_t *manager,unsigned *count) {
    void **list=*(void ***)(manager+0x2cu);*count=u32(manager+0x30u);
    return *count && *count<=4096u && memory(list,*count*4u,FALSE)?list:NULL;
}
/* The bank native EmitterBankSet resolves for the image literal (ManagerBankFind
 * 0x289A10 takes the first SoundBankNameId match): the only listed bank with
 * that id, with exactly that name, resident (owner(): refcount 1..0x7FFFFFFE,
 * +0x30 clear, SSBN soundbank). Nothing here can load a bank. */
static uint8_t *missiles_bank(AudioOwner *out) {
    const char *literal=(const char *)base+0x2c621cu;
    uint8_t *manager=*(uint8_t **)(base+0x409e58u),*found=NULL;unsigned count=0,matches=0;
    if(bank_name_id(literal)!=SHOT_MISSILES_ID || !typed(base,manager,0x15cu,0x2e1dc0u))return NULL;
    void **list=bank_list(manager,&count);if(!list)return NULL;
    for(unsigned i=0;i<count;++i) {
        uint8_t *b=list[i];if(!typed(base,b,0x34u,0x2e1e40u))return NULL;
        if(u32(b+0x14u)==SHOT_MISSILES_ID) {++matches;found=b;}
    }
    const char *name=found?*(const char **)(found+0x10u):NULL;
    if(matches!=1 || !memory(name,13u,FALSE) || memcmp(name,literal,13u) || !owner(found,out))return NULL;
    return found;
}
/* Check #8: before dropping the record's bank reference, re-prove the very
 * bank it took: listed once, typed, missiles.xsb by id and name, the same
 * soundbank, held by the emitter, and still referenced beyond this record.
 * 1 proven; 0 not provable now (retry); -1 definitively not ours (no write). */
static int shot_bank_proof(const ShotRecord *r) {
    uint8_t *bank=r->cue.bank,*manager=*(uint8_t **)(base+0x409e58u);unsigned count=0,listed=0;AudioOwner a;
    if(!typed(base,manager,0x15cu,0x2e1dc0u))return 0;
    void **list=bank_list(manager,&count);if(!list)return 0;
    for(unsigned i=0;i<count;++i)if(list[i]==bank)++listed;
    if(listed!=1 || !typed(base,bank,0x34u,0x2e1e40u) || u32(bank+0x14u)!=SHOT_MISSILES_ID ||
        *(uint8_t **)(bank+0x1cu)!=r->cue.soundbank || *(uint8_t **)(r->emitter+4u)!=bank)return -1;
    const char *name=*(const char **)(bank+0x10u);
    if(!memory(name,13u,FALSE) || memcmp(name,"missiles.xsb",13u) || u32(bank+0x18u)<2u)return -1;
    return missiles_bank(&a)==bank && a.soundbank==r->cue.soundbank;
}
static BOOL shot_capacity(const AudioOwner *a) {
    if(!play_available(a) || u32(a->pool+0xcu)<SHOT_RESERVE)return FALSE;
    unsigned free_entries=0;
    for(unsigned i=0;i<a->count;++i)if(!(a->entries[i*24u]&63u))++free_entries;
    return free_entries>=SHOT_RESERVE;
}
/* EmitterBankSet: ESI = emitter, push name, RET 4, AL = bank found. */
__attribute__((naked,noinline,used)) static unsigned invoke_bank_set(void *entry __attribute__((unused)),
    void *emitter __attribute__((unused)),const char *name __attribute__((unused))) {
    __asm__ volatile("pushl %ebp\n\tmovl %esp,%ebp\n\tpushl %ebx\n\tpushl %esi\n\tpushl %edi\n\t"
        "movl 12(%ebp),%esi\n\tpushl 16(%ebp)\n\tcall *8(%ebp)\n\tmovzbl %al,%eax\n\t"
        "popl %edi\n\tpopl %esi\n\tpopl %ebx\n\tleave\n\tret\n\t");
}
/* EmitterPlay: EAX = emitter, (name, &handle, 3D, rows=0, count=0), RET 0x14,
 * EAX 0 = ok. The handle is zeroed on entry and written only on success. */
__attribute__((naked,noinline,used)) static unsigned invoke_emitter_play(void *entry __attribute__((unused)),
    void *emitter __attribute__((unused)),const char *name __attribute__((unused)),
    uint32_t *handle __attribute__((unused)),uint32_t spatial __attribute__((unused))) {
    __asm__ volatile("pushl %ebp\n\tmovl %esp,%ebp\n\tpushl %ebx\n\tpushl %esi\n\tpushl %edi\n\t"
        "pushl $0\n\tpushl $0\n\tpushl 24(%ebp)\n\tpushl 20(%ebp)\n\tpushl 16(%ebp)\n\t"
        "movl 12(%ebp),%eax\n\tcall *8(%ebp)\n\t"
        "popl %edi\n\tpopl %esi\n\tpopl %ebx\n\tleave\n\tret\n\t");
}
/* AttachmentRelease: EDI = manager, EDX = &slot, push attachment, RET 4.
 * 0 after clearing the matching cell; 0x80070057 when no cell matches. */
__attribute__((naked,noinline,used)) static unsigned invoke_release(void *entry __attribute__((unused)),
    void *manager __attribute__((unused)),void *slot __attribute__((unused)),void *attachment __attribute__((unused))) {
    __asm__ volatile("pushl %ebp\n\tmovl %esp,%ebp\n\tpushl %ebx\n\tpushl %esi\n\tpushl %edi\n\t"
        "movl 12(%ebp),%edi\n\tmovl 16(%ebp),%edx\n\tpushl 20(%ebp)\n\tcall *8(%ebp)\n\t"
        "popl %edi\n\tpopl %esi\n\tpopl %ebx\n\tleave\n\tret\n\t");
}
/* AttachmentDetachAll: stdcall(attachment), RET 4. */
__attribute__((naked,noinline,used)) static unsigned invoke_detach(void *entry __attribute__((unused)),
    void *attachment __attribute__((unused))) {
    __asm__ volatile("pushl %ebp\n\tmovl %esp,%ebp\n\tpushl %ebx\n\tpushl %esi\n\tpushl %edi\n\t"
        "pushl 12(%ebp)\n\tcall *8(%ebp)\n\tpopl %edi\n\tpopl %esi\n\tpopl %ebx\n\tleave\n\tret\n\t");
}
/* RELEASING -> FREE only with the cue positively retired. A failure that
 * leaves native state referring to the record quarantines it. */
static BOOL shot_release(ShotRecord *r,SudekiMpStoryCharacterAudioExact exact,void *context) {
    if(r->cue.handle || r->cue.cue || !exact(context) || !shot_native_ready())return FALSE;
    uint8_t *manager=*(uint8_t **)(base+0x409e58u),*slot=r->slot,*attachment=*(uint8_t **)slot;
    if(!typed(base,manager,0x15cu,0x2e1dc0u))return FALSE;
    if(attachment) {
        if(!slot_position_exact(manager,*(uint8_t **)(manager+0x10u),slot))
            return shot_quarantine(r,"attachment_unproven");
        (void)invoke_detach(shot_native(shot_entry.detach,0x281a70u),attachment);
        if(*(uint8_t **)slot!=attachment)return shot_quarantine(r,"attachment_changed");
        if(invoke_release(shot_native(shot_entry.release,0x2893c0u),manager,slot,attachment))
            return shot_quarantine(r,"attachment_release");
        memset(slot,0,4u);
    }
    if(slot_cells(manager,slot))return shot_quarantine(r,"cell_retained");
    if(r->active) {
        int proof=shot_bank_proof(r);
        if(proof<0)return shot_quarantine(r,"bank_reference");
        if(!proof)return FALSE;
        /* SetActive(0) mirror; bank+0x18 >= 2, so never the unload at +0x1C. */
        uint32_t refs=u32(r->cue.bank+0x18u)-1u;memcpy(r->cue.bank+0x18u,&refs,4u);
        r->emitter[0x10u]=0;r->active=0;
    }
    if(shot_lifecycle_logs<32u && shot_log()) {
        ++shot_lifecycle_logs;
        SudekiMpLogFormat("story_shot_audio event=released role=%s source=%s character=%u key=%lu\r\n",
            shot_role(r),r->world?"world":"party",r->character,(unsigned long)r->key);
    }
    memset(r,0,sizeof(*r));return TRUE;
}
static BOOL shot_move(ShotRecord *r,SudekiMpStoryCharacterAudioExact exact,void *context) {
    uint8_t *slot=r->slot,*manager=*(uint8_t **)(base+0x409e58u);
    if(!r->move_pending)return TRUE;
    /* Check #13: the idle sweep released a finished cue's attachment and
     * cleared the slot; nothing native remains to move. */
    if(!*(void **)slot) {r->move_pending=0;return TRUE;}
    for(unsigned i=0;i<sizeof(codes)/sizeof(codes[0]);++i) {
        unsigned rva=codes[i].rva;
        if((rva==0x284520u || rva==0x281860u || rva==0x2817f0u || rva==0x22e940u) && !code_exact(base,i))return FALSE;
    }
    if(!exact(context) || !typed(base,manager,0x15cu,0x2e1dc0u) ||
        !slot_position_exact(manager,*(uint8_t **)(manager+0x10u),slot))return FALSE;
    memcpy(slot+4u,r->target,12u);
    if(position_attachment(shot_native(shot_entry.position,0x284520u),slot))return FALSE;
    r->move_pending=0;return TRUE;
}
static void shot_advance(ShotRecord *r,uint32_t now,unsigned mode,SudekiMpStoryCharacterAudioExact exact,void *context) {
    if(r->state==SHOT_FREE)return;
    if(r->state==SHOT_QUARANTINED) {shot_pin();return;}
    if(r->state==SHOT_BOUND)r->state=SHOT_RELEASING;
    if(r->state==SHOT_PLAYING) {
        /* Check #13: a finished cue (travel included) releases at once. */
        if(positively_retired(&r->cue)) {
            r->cue.handle=0;r->cue.cue=NULL;r->cue.stopped=FALSE;r->state=SHOT_RELEASING;
        } else if(mode==SHOT_STOP_ALL || (mode==SHOT_DRAIN && (r->role==SHOT_TRAVEL || r->world)) ||
            (int32_t)(now-r->started)>(int32_t)r->limit) r->state=SHOT_STOPPING;
        else if(r->move_pending)(void)shot_move(r,exact,context);
    }
    if(r->state==SHOT_STOPPING && stop_cue(&r->cue,exact,context)) {
        r->state=SHOT_RELEASING;
        if(shot_lifecycle_logs<32u && shot_log()) {
            ++shot_lifecycle_logs;
            SudekiMpLogFormat("story_shot_audio event=stopped role=%s source=%s character=%u key=%lu\r\n",
                shot_role(r),r->world?"world":"party",r->character,(unsigned long)r->key);
        }
    }
    if(r->state==SHOT_RELEASING)(void)shot_release(r,exact,context);
}
/* Cue names from this game's own data (resolver; read-only, no native call).
 * World pairings are cached per instance so both cues use one answer. */
static unsigned shot_names(const SudekiMpStoryShotAudioAction *act,BOOL world,SudekiMpStoryShotCueNames *out) {
    unsigned result=SUDEKIMP_SHOT_CUES_SILENT;
    memset(out,0,sizeof(*out));
    if(world)for(unsigned i=0;i<sizeof(shot_world)/sizeof(shot_world[0]);++i) {
        const ShotWorldNames *w=&shot_world[i];
        if(w->used && w->space==shot_space && w->key==act->key && w->emitted==act->emitted_tick) {
            *out=w->names;return w->result;
        }
    }
    if(SudekiMpStoryShotCuesImageMatches((HMODULE)base))
        result=world?SudekiMpStoryShotCuesForWorldMissile((HMODULE)base,act->vfx_kind,act->position,out):
            SudekiMpStoryShotCuesForItem((HMODULE)base,act->character,act->item,out);
    if(result==SUDEKIMP_SHOT_CUES_DEFER)return result;
    if(result!=SUDEKIMP_SHOT_CUES_OK || !memchr(out->travel,0,sizeof(out->travel)) ||
        !memchr(out->fire,0,sizeof(out->fire))) {result=SUDEKIMP_SHOT_CUES_SILENT;memset(out,0,sizeof(*out));}
    if(world) {
        ShotWorldNames *w=&shot_world[shot_world_next++%(sizeof(shot_world)/sizeof(shot_world[0]))];
        *w=(ShotWorldNames){.space=shot_space,.key=act->key,.emitted=act->emitted_tick,.used=1,
            .result=(uint8_t)result,.names=*out};
        if(!shot_world_probed && shot_log()) {
            shot_world_probed=TRUE;
            SudekiMpLogFormat("story_shot_audio event=pairing kind=%u key=%lu result=%u travel=%s fire=%s\r\n",
                act->vfx_kind,(unsigned long)act->key,result,out->travel,out->fire);
        }
    } else if(result==SUDEKIMP_SHOT_CUES_OK && act->item<64u && !(shot_items_logged&(1ull<<act->item)) && shot_log()) {
        shot_items_logged|=1ull<<act->item;
        SudekiMpLogFormat("story_shot_audio event=probe character=%u item=%u travel=%s fire=%s\r\n",
            act->character,act->item,out->travel,out->fire);
    }
    return result;
}
static unsigned shot_start(const SudekiMpStoryShotAudioAction *act,const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpStoryShotAudioInput *in,SudekiMpStoryCharacterAudioExact exact,void *context) {
    BOOL world=act->type==SUDEKIMP_SHOT_AUDIO_WORLD_FIRE || act->type==SUDEKIMP_SHOT_AUDIO_WORLD_TRAVEL;
    unsigned role=act->type==SUDEKIMP_SHOT_AUDIO_PARTY_FIRE || act->type==SUDEKIMP_SHOT_AUDIO_WORLD_FIRE?
        SHOT_FIRE:SHOT_TRAVEL;
    if((!world && act->character>=4u) || !shot_finite(act->position))
        return shot_refused(act,"action",SUDEKIMP_SHOT_AUDIO_SILENT);
    for(unsigned i=0;i<SHOT_RECORDS;++i) {
        const ShotRecord *r=&shot_records[i];
        if(r->state!=SHOT_FREE && r->state!=SHOT_QUARANTINED && r->space==shot_space && r->key==act->key &&
            r->emitted==act->emitted_tick && r->role==role && r->world==world && r->character==act->character)
            return shot_refused(act,"duplicate",SUDEKIMP_SHOT_AUDIO_SILENT);
    }
    if(!shot_native_ready())return shot_refused(act,"native_guards",SUDEKIMP_SHOT_AUDIO_SILENT);
    SudekiMpStoryShotCueNames names;unsigned resolved=shot_names(act,world,&names);
    if(resolved==SUDEKIMP_SHOT_CUES_DEFER)return shot_refused(act,"cue_source",SUDEKIMP_SHOT_AUDIO_DEFER);
    if(resolved!=SUDEKIMP_SHOT_CUES_OK)return shot_refused(act,"cue_source",SUDEKIMP_SHOT_AUDIO_SILENT);
    const char *wanted=role==SHOT_FIRE?names.fire:names.travel;
    if(!*wanted)return SUDEKIMP_SHOT_AUDIO_SILENT; /* native skips an empty name (0x587107, 0x58714D) */
    if(!listener_exact())return shot_refused(act,"listener",SUDEKIMP_SHOT_AUDIO_DEFER);
    AudioOwner a,b;uint8_t *bank=missiles_bank(&a);
    if(!bank)return shot_refused(act,"missiles_bank",SUDEKIMP_SHOT_AUDIO_DEFER);
    /* Native _2D rule (0x58714F): only this game's own controlled shooter
     * hears its fire cue unpositioned. Check #3: the planner's flag, the
     * proven local character and the presented native leader must agree. */
    BOOL local=role==SHOT_FIRE && !world && act->local && in->local_character==act->character &&
        roster && roster->leader_character==act->character;
    unsigned index=0;BOOL found=FALSE;
    if(local) {
        char variant[sizeof(names.fire)+4u];size_t n=strlen(wanted);
        memcpy(variant,wanted,n);memcpy(variant+n,"_2D",4u);found=cue_index(&a,variant,&index);
    }
    if(!found && !cue_index(&a,wanted,&index))return shot_refused(act,"cue_missing",SUDEKIMP_SHOT_AUDIO_SILENT);
    const char *name=cue_name(&a,index);
    if(!name)return shot_refused(act,"cue_missing",SUDEKIMP_SHOT_AUDIO_SILENT);
    unsigned spatial=local?0u:1u;
    ShotRecord *r=NULL;
    for(unsigned i=role==SHOT_FIRE?8u:0u,end=i+8u;i<end && !r;++i)if(shot_records[i].state==SHOT_FREE)r=&shot_records[i];
    if(!r)return shot_refused(act,"records",SUDEKIMP_SHOT_AUDIO_DEFER);
    if(*(void **)r->slot || slot_cells(a.manager,r->slot)) {
        r->role=(uint8_t)role;r->world=(uint8_t)world; /* names the pool in the log */
        (void)shot_quarantine(r,"slot_registered");
        return shot_refused(act,"slot_registered",SUDEKIMP_SHOT_AUDIO_DEFER);
    }
    /* CMissile constructor stores (0x58622D-0x586257): emitter vtable, no
     * bank, no slot, no reserve, inactive; slot: no attachment, origin,
     * cone orientation (0,0,1). Only this module can see the record yet. */
    memset(r,0,sizeof(*r));
    void *vtable=base+0x2e1db4u;float one=1.0f;
    memcpy(r->emitter,&vtable,4u);memcpy(r->slot+0x18u,&one,4u);
    a.emitter=r->emitter;
    if(!shot_capacity(&a))return shot_refused(act,"native_play_capacity",SUDEKIMP_SHOT_AUDIO_DEFER);
    if(spatial && (u32(a.manager+0x130u)>SHOT_CELL_LIMIT || !slot_ready(a.manager,a.backend,r->slot)))
        return shot_refused(act,"native_attachment",SUDEKIMP_SHOT_AUDIO_DEFER);
    if(!exact(context))return shot_refused(act,"presentation",SUDEKIMP_SHOT_AUDIO_DEFER);
    if(!shot_claim_probed) {
        unsigned pool_free=0,strings=0;uint8_t *table=*(uint8_t **)(base+0x409e10u);
        for(unsigned i=0;i<a.count;++i)if(!(a.entries[i*24u]&63u))++pool_free;
        if(typed(base,table,0x14u,0x2dcc8cu))strings=u32(table+8u);
        shot_claim_probed=TRUE;
        if(shot_log())SudekiMpLogFormat("story_shot_audio event=probe bank_refs=%u cells=%u pool_free=%u "
            "pool_units=%u strings=%u\r\n",u32(bank+0x18u),u32(a.manager+0x130u),pool_free,u32(a.pool+0xcu),strings);
    }
    /* Claim (FREE -> BOUND). Inactive, so the bank is stored without a
     * reference and nothing is released or loaded (0x68AE50-0x68AEAC). */
    uint32_t refs=u32(bank+0x18u);
    unsigned bank_found=invoke_bank_set(shot_native(shot_entry.bank_set,0x28ae50u),r->emitter,(const char *)base+0x2c621cu);
    if(!bank_found || *(uint8_t **)(r->emitter+4u)!=bank || u32(bank+0x18u)!=refs || r->emitter[0x10u]) {
        memset(r,0,sizeof(*r));return shot_refused(act,"emitter_bank",SUDEKIMP_SHOT_AUDIO_SILENT);
    }
    void *slot=r->slot;memcpy(r->emitter+8u,&slot,4u);
    if(!owner(bank,&b) || !same_owner(&a,&b) || !exact(context)) {
        memset(r,0,sizeof(*r));return shot_refused(act,"bank_owner",SUDEKIMP_SHOT_AUDIO_DEFER);
    }
    /* SetActive(1) mirror (0x586C90): resident bank, so never the 0->1 load. */
    refs=u32(bank+0x18u)+1u;memcpy(bank+0x18u,&refs,4u);r->emitter[0x10u]=1;r->active=1;
    r->state=SHOT_BOUND;r->cue=(LocalCue){.bank=bank,.soundbank=a.soundbank,.pool=a.pool};
    r->space=shot_space;r->key=act->key;r->emitted=act->emitted_tick;r->role=(uint8_t)role;r->world=(uint8_t)world;
    r->character=act->character;r->item=act->item;r->kind=act->vfx_kind;r->spatial=(uint8_t)spatial;r->index=index;
    r->limit=role==SHOT_FIRE && !world?SHOT_PARTY_FIRE_MS:SHOT_CUE_MS;r->started=in->now;
    /* Both CMissile slots start at the launch point (0x586EEF-0x586F56). */
    memcpy(r->slot+4u,act->position,12u);memcpy(r->target,act->position,12u);
    unsigned status=invoke_emitter_play(shot_native(shot_entry.play,0x28aeb0u),r->emitter,name,&r->cue.handle,spatial);
    if(!r->cue.handle) {
        shot_advance(r,in->now,SHOT_SERVICE,exact,context);
        return shot_refused(act,"native_returned_no_handle",SUDEKIMP_SHOT_AUDIO_SILENT);
    }
    /* The returned handle is published first: any failure below still owes
     * a stop of exactly that handle. */
    uint8_t *cue=NULL;
    if(!status && exact(context) && owner(bank,&b) && same_owner(&a,&b) && handle_state(&b,r->cue.handle,NULL)==1) {
        cue=*(uint8_t **)(b.entries+(r->cue.handle&255u)*24u+0xcu);
        if(!memory(cue,0x30u,FALSE) || *(void **)(cue+8u)!=a.soundbank || u32(cue+4u)!=index)cue=NULL;
    }
    if(!cue) {
        r->state=SHOT_STOPPING;shot_advance(r,in->now,SHOT_SERVICE,exact,context);
        return shot_refused(act,"returned_handle_owner",SUDEKIMP_SHOT_AUDIO_SILENT);
    }
    r->cue.cue=cue;r->state=SHOT_PLAYING;shot_last_reason=NULL;
    if(shot_log())SudekiMpLogFormat("story_shot_audio event=played role=%s source=%s character=%u item=%u kind=%u "
        "key=%lu cue=%s spatial=%u handle=%lu\r\n",shot_role(r),world?"world":"party",act->character,act->item,
        act->vfx_kind,(unsigned long)act->key,name,spatial,(unsigned long)r->cue.handle);
    return SUDEKIMP_SHOT_AUDIO_PLAYED;
}
static unsigned shot_execute(const SudekiMpStoryShotAudioAction *act,const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpStoryShotAudioInput *in,SudekiMpStoryCharacterAudioExact exact,void *context) {
    switch(act->type) {
    case SUDEKIMP_SHOT_AUDIO_PARTY_FIRE: case SUDEKIMP_SHOT_AUDIO_PARTY_TRAVEL:
    case SUDEKIMP_SHOT_AUDIO_WORLD_FIRE: case SUDEKIMP_SHOT_AUDIO_WORLD_TRAVEL:
        return shot_start(act,roster,in,exact,context);
    case SUDEKIMP_SHOT_AUDIO_MOVE:
        for(unsigned i=0;i<SHOT_RECORDS;++i) {
            ShotRecord *r=&shot_records[i];
            if(r->state!=SHOT_PLAYING || r->role!=SHOT_TRAVEL || r->space!=shot_space || r->key!=act->key)continue;
            if(!shot_finite(act->position))return SUDEKIMP_SHOT_AUDIO_SILENT;
            if(memcmp(r->target,act->position,12u)) {memcpy(r->target,act->position,12u);r->move_pending=1;}
            return shot_move(r,exact,context)?SUDEKIMP_SHOT_AUDIO_PLAYED:SUDEKIMP_SHOT_AUDIO_DEFER;
        }
        return SUDEKIMP_SHOT_AUDIO_SILENT;
    case SUDEKIMP_SHOT_AUDIO_RETIRE: {
        unsigned matched=0;
        for(unsigned i=0;i<SHOT_RECORDS;++i) {
            ShotRecord *r=&shot_records[i];
            if(r->state!=SHOT_PLAYING || r->space!=shot_space || r->key!=act->key ||
                (r->role!=SHOT_TRAVEL && !r->world))continue;
            r->state=SHOT_STOPPING;shot_advance(r,in->now,SHOT_SERVICE,exact,context);++matched;
        }
        return matched?SUDEKIMP_SHOT_AUDIO_PLAYED:SUDEKIMP_SHOT_AUDIO_SILENT;
    }
    default: return SUDEKIMP_SHOT_AUDIO_SILENT;
    }
}
BOOL SudekiMpStoryShotAudioPresent(const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpStoryShotAudioInput *input,SudekiMpStoryCharacterAudioExact exact,void *context) {
    if(host_mode || busy || !base || !input || !exact || (native_thread && native_thread!=GetCurrentThreadId()) ||
        !exact(context))return FALSE;
    native_thread=GetCurrentThreadId();busy=TRUE;shot_native_state=-1;
    if(shot_space_known && shot_space!=input->namespace_token) {
        /* A new session/lease/stream: its keys are a new namespace. Stop the
         * old cues that follow host visuals; party fire drains as usual. */
        for(unsigned i=0;i<SHOT_RECORDS;++i) {
            ShotRecord *r=&shot_records[i];
            if(r->state==SHOT_PLAYING && (r->role==SHOT_TRAVEL || r->world))r->state=SHOT_STOPPING;
        }
        memset(shot_world,0,sizeof(shot_world));
    }
    shot_space=input->namespace_token;shot_space_known=TRUE;
    for(unsigned i=0;i<SHOT_RECORDS;++i)shot_advance(&shot_records[i],input->now,SHOT_SERVICE,exact,context);
    SudekiMpStoryShotAudioAction actions[SUDEKIMP_SHOT_AUDIO_MAX_ACTIONS];
    unsigned count=SudekiMpStoryShotAudioPlanStep(&shot_plan,input,actions,SUDEKIMP_SHOT_AUDIO_MAX_ACTIONS);
    if(count>SUDEKIMP_SHOT_AUDIO_MAX_ACTIONS)count=SUDEKIMP_SHOT_AUDIO_MAX_ACTIONS;
    for(unsigned i=0;i<count;++i)
        SudekiMpStoryShotAudioPlanCommit(&shot_plan,&actions[i],shot_execute(&actions[i],roster,input,exact,context));
    shot_last_present=input->now;shot_presented=TRUE;busy=FALSE;return TRUE;
}
BOOL SudekiMpStoryShotAudioRetains(void) {
    for(unsigned i=0;i<SHOT_RECORDS;++i)if(shot_retryable(&shot_records[i]))return TRUE;
    return FALSE;
}
BOOL SudekiMpStoryShotAudioNeedsService(uint32_t now) {
    return SudekiMpStoryShotAudioRetains() && (!shot_presented || (int32_t)(now-shot_last_present)>250);
}
static BOOL shot_teardown(unsigned mode,SudekiMpStoryCharacterAudioExact exact,void *context) {
    if(!SudekiMpStoryShotAudioRetains())return TRUE;
    if(host_mode || busy || !base || !exact || (native_thread && native_thread!=GetCurrentThreadId()) ||
        !exact(context))return FALSE;
    busy=TRUE;shot_native_state=-1;uint32_t now=shot_now();
    for(unsigned i=0;i<SHOT_RECORDS;++i)shot_advance(&shot_records[i],now,mode,exact,context);
    busy=FALSE;return !SudekiMpStoryShotAudioRetains();
}
BOOL SudekiMpStoryShotAudioDrain(SudekiMpStoryCharacterAudioExact exact,void *context) {
    return shot_teardown(SHOT_DRAIN,exact,context);
}
BOOL SudekiMpStoryShotAudioStop(SudekiMpStoryCharacterAudioExact exact,void *context) {
    return shot_teardown(SHOT_STOP_ALL,exact,context);
}
