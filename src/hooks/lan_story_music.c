#include "hooks/lan_story_music.h"
#include "hooks/lan_story_client.h"
#include "hooks/lan_story_area_follow.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Battle-score presentation requires the supported x86 ABI"
#endif
static uint8_t *base;
static DWORD native_thread;
static BOOL busy;
static uint32_t applied_epoch,applied_revision,applied_sequence;
static unsigned refusals;
static const unsigned curve_offsets[]={0x7cu,0x80u,0x84u,0xa0u,0xa4u,0xa8u};
static SudekiMpStoryEffectState last_capture;
static const struct { const char *name; unsigned rva,size; uint32_t hash; } codes[]={
    {"BATTLE_SCORE_LOAD",0x39560u,0x140u,0xfb9c8a3du},
    {"BATTLE_SCORE_RESET",0x39a00u,0x70u,0x7a4acf14u},
    {"BATTLE_SCORE_ENCOUNTER",0x39ad0u,0x22u,0xca19d71au},
    {"BATTLE_SCORE_UPDATE",0x391c0u,0x396u,0xdde984d4u},
    {"BATTLE_SCORE_EMITTER_LOAD",0x28ae50u,0x60u,0x59fe6168u},
};
static const struct { unsigned rva,target; } relocations[]={
    {0x395b4u,0x2e1db4u},
    {0x395e5u,0x2c7bb0u},
    {0x39614u,0x2c7bb8u},
    {0x39a16u,0x409e58u},
    {0x39282u,0x3c2f84u},
    {0x3946bu,0x409e58u},
    {0x394d0u,0x2e3608u},
    {0x394ebu,0x31e5e0u},
    {0x394f5u,0x31e5f0u},
    {0x39510u,0x409e8cu},
    {0x28ae6au,0x409e58u},
};
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
        a!=PAGE_EXECUTE_READ && a!=PAGE_EXECUTE_READWRITE && a!=PAGE_EXECUTE_WRITECOPY) return FALSE;
    return p+size<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static uint32_t word(const void *p) {uint32_t v; memcpy(&v,p,4); return v;}
static BOOL typed(const void *p,size_t size,unsigned table,BOOL write) {
    return memory(p,size,write) && *(void *const *)p==base+table;
}
static BOOL code_exact(uint8_t *b) {
    if(!b) return FALSE;
    for(unsigned k=0;k<sizeof(codes)/sizeof(codes[0]);++k) {
        uint32_t h=2166136261u;
        if(!memory(b+codes[k].rva,codes[k].size,FALSE)) return FALSE;
        for(unsigned i=0;i<codes[k].size;++i) {
            unsigned rva=codes[k].rva+i; uint8_t byte=b[rva];
            for(unsigned j=0;j<sizeof(relocations)/sizeof(relocations[0]);++j) {
                unsigned offset=rva-relocations[j].rva;
                if(offset>=4u) continue;
                if(word(b+relocations[j].rva)!=(uint32_t)(uintptr_t)(b+relocations[j].target)) return FALSE;
                byte=(uint8_t)((0x400000u+relocations[j].target)>>(8u*offset)); break;
            }
            h=(h^byte)*16777619u;
        }
        if(h!=codes[k].hash) {
            if(refusals++<8u) SudekiMpLogFormat("story_music event=guard_failed name=%s\r\n",codes[k].name);
            return FALSE;
        }
    }
    return *(void **)(b+0x2c7c00u)==b+0x391c0u &&
        *(void **)(b+0x2c7c30u)==b+0x39ad0u;
}
static BOOL witnessed(const SudekiMpControlUpdateDispatchWitness *w) {
    if(!base || !w || w->native_thread_id!=GetCurrentThreadId() ||
        (native_thread && native_thread!=GetCurrentThreadId()) ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w)) return FALSE;
    native_thread=GetCurrentThreadId(); return TRUE;
}
static BOOL text_at(const char *text,char *out,unsigned capacity) {
    if(!out || !capacity || !memory(text,capacity,FALSE)) return FALSE;
    memset(out,0,capacity);
    for(unsigned i=0;i<capacity;++i) {
        unsigned char c=(unsigned char)text[i];
        if(!c) return i>0u;
        if(c<0x20u || c>0x7eu || i+1u==capacity) return FALSE;
        out[i]=(char)c;
    }
    return FALSE;
}
typedef struct MusicOwner {uint8_t *score,*manager,*pool,*emitter,*bank;} MusicOwner;
static uint8_t *registered_bank(uint8_t *manager,const char *score) {
    char wanted[SUDEKIMP_STORY_AREA_NAME+4],name[96];
    int n=snprintf(wanted,sizeof(wanted),"%s.xsb",score);
    if(n<=0 || (unsigned)n>=sizeof(wanted)) return NULL;
    unsigned count=word(manager+0x30u),matches=0;
    uint8_t **list=*(uint8_t ***)(manager+0x2cu),*result=NULL;
    if(!count || count>4096u || !memory(list,count*4u,FALSE)) return NULL;
    for(unsigned i=0;i<count;++i) {
        uint8_t *bank=list[i];
        if(!typed(bank,0x34u,0x2e1e40u,FALSE) ||
            !text_at(*(const char **)(bank+0x10u),name,sizeof(name))) return NULL;
        if(!_stricmp(name,wanted)) {result=bank; ++matches;}
    }
    return matches==1u?result:NULL;
}
static BOOL observe(MusicOwner *out,SudekiMpStoryEffectState *state) {
    if(!base || !out || !state || !memory(base+0x409d80u,4,FALSE) ||
        !memory(base+0x409e58u,4,FALSE) || !memory(base+0x409e8cu,4,FALSE)) return FALSE;
    MusicOwner a={.score=*(uint8_t **)(base+0x409d80u),
        .manager=*(uint8_t **)(base+0x409e58u),.pool=*(uint8_t **)(base+0x409e8cu)};
    uint8_t *s=a.score;
    if(!typed(s,0x100u,0x2c7bf4u,TRUE) ||
        *(void **)(s+8u)!=base+0x2c7bfcu || *(void **)(s+0x2cu)!=base+0x2c7c1cu ||
        *(void **)(s+0x30u)!=base+0x2c7c24u || s[0xacu]>1u ||
        !typed(a.manager,0x15cu,0x2e1dc0u,FALSE) || !typed(a.pool,0x14u,0x2e1ee0u,FALSE)) return FALSE;
    const char *name=(word(s+0x4cu)&0x80000000u)?(const char *)(s+0x50u):*(const char **)(s+0x50u);
    if(!text_at(name,state->music_score,sizeof(state->music_score))) return FALSE;
    state->music_flags=1u|(s[0xacu]?2u:0u); state->music_intensity=word(s+0x78u);
    state->music_variation=s[0xadu];
    for(unsigned i=0;i<6u;++i) state->music_curve[i]=word(s+curve_offsets[i]);
    /* Reuse the portable policy for canonical name and finite normalized mix. */
    memcpy(state->current,"music",6);
    if(!SudekiMpStoryEffectStateValid(state)) return FALSE;
    a.emitter=*(uint8_t **)(s+0x44u);
    if(!typed(a.emitter,0x14u,0x2e1db4u,FALSE) || a.emitter[0x10u]!=1u || word(a.emitter+8u)) return FALSE;
    a.bank=*(uint8_t **)(a.emitter+4u);
    if(!a.bank || registered_bank(a.manager,state->music_score)!=a.bank ||
        !word(a.bank+0x18u) || word(a.bank+0x18u)>=0x7fffffffu || a.bank[0x30u]) return FALSE;
    uint8_t *soundbank=*(uint8_t **)(a.bank+0x1cu);
    if(!typed(soundbank,0x3cu,0x2e1f68u,FALSE) || memcmp(soundbank+8u,"SSBN",4)) return FALSE;
    /* The native reset may stop the current cue. Prove its tracker generation
     * and bank before calling it; an already-retired handle is inert natively. */
    unsigned count=word(a.pool+8u); uint8_t *entries=*(uint8_t **)(a.pool+0x10u);
    uint32_t handle=word(s+0x48u);
    if(!count || count>256u || !memory(entries,count*24u,FALSE)) return FALSE;
    if(handle) {
        unsigned slot=handle&255u;
        if(slot>=count) return FALSE;
        uint8_t *e=entries+slot*24u;
        if(word(e+8u)==(handle>>8) && (e[0]&63u)==1u) {
            uint8_t *cue=*(uint8_t **)(e+0xcu);
            if(*(void **)(e+0x10u)!=a.bank || !memory(cue,0x30u,FALSE) ||
                *(void **)(cue+8u)!=soundbank) return FALSE;
        }
    }
    *out=a; return TRUE;
}
BOOL SudekiMpLanStoryMusicInitialize(HMODULE image) {
    if(busy || !image || !SudekiMpCheckLoadedExecutable(image) || !code_exact((uint8_t *)image)) return FALSE;
    base=(uint8_t *)image; memset(&last_capture,0,sizeof(last_capture)); native_thread=0; applied_epoch=applied_revision=applied_sequence=0; return TRUE;
}
void SudekiMpLanStoryMusicCapture(SudekiMpStoryEffectState *out,
    const SudekiMpControlUpdateDispatchWitness *w) {
    MusicOwner a; SudekiMpStoryEffectState music={0};
    if(!out || !witnessed(w) || !code_exact(base) || !observe(&a,&music)) return;
    out->music_flags=music.music_flags; out->music_intensity=music.music_intensity;
    memcpy(out->music_score,music.music_score,sizeof(out->music_score));
    out->music_variation=music.music_variation;
    memcpy(out->music_curve,music.music_curve,sizeof(out->music_curve));
    if(last_capture.music_flags!=music.music_flags || strcmp(last_capture.music_score,music.music_score))
        SudekiMpLogFormat("story_music event=captured score=%s danger=%u intensity_bits=%08lx\r\n",
            music.music_score,(music.music_flags>>1)&1u,(unsigned long)music.music_intensity);
    last_capture=music;
}
typedef uint8_t (__attribute__((thiscall)) *LoadScore)(void *,const char *);
typedef void (__attribute__((thiscall)) *EncounterScore)(void *,uint32_t,uint32_t,uint32_t);
typedef struct MusicCall {
    const SudekiMpStoryEffectState *host;
    const SudekiMpControlUpdateDispatchWitness *w;
    BOOL applied;
} MusicCall;
static BOOL present(void *context) {
    MusicCall *call=context; const SudekiMpStoryEffectState *h=call->host;
    MusicOwner a,b; SudekiMpStoryEffectState local={0},after={0}; BOOL entered=FALSE;
    if(!witnessed(call->w) || !SudekiMpLanStoryEffectsInCurrentArea(h) ||
        !code_exact(base) || !observe(&a,&local) || !registered_bank(a.manager,h->music_score)) return FALSE;
    if(_stricmp(local.music_score,h->music_score)) {
        entered=TRUE;
        BOOL loaded=((LoadScore)(base+0x39560u))(a.score,h->music_score);
        /* Load owns retirement of its old emitter/cue. Never access those
         * pointers after the call; reacquire all native owners from globals. */
        if(!loaded || !witnessed(call->w) || !SudekiMpLanStoryEffectsInCurrentArea(h) ||
            !observe(&b,&after) || b.score!=a.score || _stricmp(after.music_score,h->music_score)) return TRUE;
        a=b; local=after;
    }
    if((local.music_flags&2u)!=(h->music_flags&2u)) {
        /* Exact ClusterListener subobject and RET12 ABI. The two cluster
         * identity arguments are unused in this native audio-only listener. */
        ((EncounterScore)(base+0x39ad0u))(a.score+0x30u,0,0,(h->music_flags>>1)&1u);
    }
    /* Project the host's requested mix, including native scripted/variation
     * changes. The native ramp also needs the host cluster's curve (the
     * contained client has no cluster-enter notification). Without its ramp
     * duration Update would divide by zero after entering battle. Keep local
     * user volume, fade clocks and cue cursor; native Update renders the mix. */
    for(unsigned i=0;i<6u;++i) memcpy(a.score+curve_offsets[i],&h->music_curve[i],4u);
    a.score[0xadu]=(uint8_t)h->music_variation;
    memcpy(a.score+0x78u,&h->music_intensity,4u);
    call->applied=TRUE;
    if(_stricmp(local.music_score,h->music_score) || local.music_flags!=h->music_flags ||
        local.music_intensity!=h->music_intensity || entered)
        SudekiMpLogFormat("story_music event=followed score=%s danger=%u intensity_bits=%08lx native_load=%u\r\n",
            h->music_score,(h->music_flags>>1)&1u,(unsigned long)h->music_intensity,entered);
    return entered; /* Only Load can change native residency; listener is scalar-only. */
}
BOOL SudekiMpLanStoryMusicFollow(const SudekiMpStoryEffectState *h,
    const SudekiMpControlUpdateDispatchWitness *w) {
    if(busy || !witnessed(w) || !SudekiMpStoryEffectStateValid(h) || !(h->music_flags&1u) ||
        !h->epoch || !h->revision || !h->sequence || !SudekiMpLanStoryEffectsInCurrentArea(h)) return FALSE;
    if(applied_epoch==h->epoch && applied_revision==h->revision && applied_sequence==h->sequence) return TRUE;
    MusicCall call={h,w,FALSE}; busy=TRUE;
    BOOL contained=SudekiMpLanStoryClientResidencyCall(w,present,&call); busy=FALSE;
    if(!contained || !call.applied) return FALSE;
    applied_epoch=h->epoch; applied_revision=h->revision; applied_sequence=h->sequence; return TRUE;
}
