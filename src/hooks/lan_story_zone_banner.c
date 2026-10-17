#include "hooks/lan_story_zone_banner.h"
#include "hooks/lan_story_client.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Zone banner requires the supported x86 ABI"
#endif
/* DisplayMessage(int,int) 0xAC6A0 (cdecl): string lookup 0x127510 on the
 * string table *(0x409E04)+4, then UIMessageBox::Show 0xABB80 on the box
 * *(0x3C2FB8) with (text, mode, 0.0, mode==2 ? 0xE : 6). Show refuses (and
 * rewrites box+0x4C) unless: box+0x48 bit 0x20 clear; not already showing the
 * same text; help box *(0x3C2FC0)+0x38 == 2; not visible with +0x48 bit 1;
 * the in-game layer *(0x3C2F88) is the UI scene's +0x170 with +0xB8 == 1.
 * HideMessageBox 0xAC730 jumps to box hide 0xAC220 (uses the context gizmo
 * *(0x3C2FC4) unchecked). All CONFIRMED_STATIC on the supported image. */
static const struct { const char *name; unsigned rva,size; uint32_t hash; } codes[]={
    {"DISPLAY_MESSAGE_II",0xac6a0u,0x45u,0x749fb7b7u},
    {"HIDE_MESSAGE_BOX",0xac730u,0x0fu,0x03d6a739u},
    {"MESSAGE_BOX_IS_VISIBLE",0xac740u,0x13u,0xf0cf0447u},
    {"MESSAGE_BOX_SHOW_GATE",0xabb80u,0x8au,0x26d0800cu},
    {"MESSAGE_BOX_HIDE",0xac220u,0xb2u,0x86e69096u},
    {"MESSAGE_BOX_CLEAR",0xac1c0u,0x5eu,0x5c1e3d01u},
    {"STRING_LOOKUP_HEAD",0x127510u,0x1au,0x67f84087u},
    /* Examinables (H1b): MessageBoxDisplay(int text,int mode,float time,int
     * flags) cdecl, the box input handler (vtable slot +0x2C, thiscall,
     * RET 0xC) and CWorld::EnableAudioEmitter(const char*) (thiscall, RET 4,
     * then 0x50D090 on the current zone). CONFIRMED_STATIC. */
    {"MESSAGE_BOX_DISPLAY",0xac540u,0x3bu,0x5ba2c68cu},
    {"MESSAGE_BOX_INPUT",0xac090u,0x124u,0xd4a62f43u},
    {"WORLD_ENABLE_AUDIO_EMITTER",0x5830u,0x4au,0x0c3743d2u},
    {"ZONE_EMITTER_ENABLE",0x10d090u,0x75u,0xfb0cf6e8u},
};
static const struct { unsigned rva,target; } relocations[]={
    {0xac6a6u,0x409e04u},{0xac6b8u,0x3c2fb8u},{0xac731u,0x3c2fb8u},{0xac741u,0x3c2fb8u},
    {0xabb9du,0x408d1cu},{0xabbc7u,0x3c2fc0u},{0xabbe9u,0x3c2f88u},{0xac2b2u,0x3c2fc4u},
    {0xac1dau,0x3c2f88u},{0x127522u,0x2d2170u},
    {0xac543u,0x3c2fb8u},{0xac559u,0x409e04u},{0xac091u,0x408d94u},{0xac09cu,0x408d1cu},{0xac0a3u,0x3c2f88u},
};
enum { RVA_DISPLAY_MESSAGE=0xac6a0u, RVA_HIDE_MESSAGE_BOX=0xac730u,
    RVA_STRINGS=0x409e04u, RVA_BOX=0x3c2fb8u, RVA_HELP=0x3c2fc0u, RVA_LAYER=0x3c2f88u,
    RVA_GIZMO=0x3c2fc4u, RVA_SCENE=0x408d1cu,
    SHOW_WINDOW_MS=1000u, BANNER_MS=3000u };
typedef void (__cdecl *DisplayMessageCall)(int,int);
typedef void (__cdecl *HideMessageBoxCall)(void);
typedef void (__cdecl *MessageBoxDisplayCall)(int,int,float,int);
typedef uint8_t (__attribute__((thiscall)) *BoxInputCall)(void *,unsigned,unsigned,unsigned);
typedef void (__attribute__((thiscall)) *EnableEmitterCall)(void *,const char *);
enum { RVA_MESSAGE_BOX_DISPLAY=0xac540u, RVA_BOX_INPUT=0xac090u, RVA_ENABLE_EMITTER=0x5830u, RVA_WORLD=0x408d10u,
    BOX_VTABLE_INPUT_SLOT=0x2cb5bcu+0x2cu };
/* One pending or shown examinable box (H1b), separate from the banner. */
static struct { BOOL pending,shown; uint16_t text; unsigned mode; uint32_t flags; const char *emitter; const char *zone;
    DWORD at; uint32_t token; } examine;

static uint8_t *base;
/* The exact native routines (set at Initialize; guarded by code_exact). */
static DisplayMessageCall display_call;
static HideMessageBoxCall hide_call;
static DWORD native_thread;
static BOOL pending,shown;
static uint16_t pending_id;
static DWORD pending_at,shown_at;
static uint32_t token;
static unsigned logs;

static BOOL memory(const void *pointer,size_t size) {
    MEMORY_BASIC_INFORMATION m; uintptr_t p=(uintptr_t)pointer;
    if(!pointer || !size || p>UINTPTR_MAX-size || VirtualQuery(pointer,&m,sizeof(m))!=sizeof(m) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
    return p+size<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static uint32_t word(const void *p) { uint32_t v; memcpy(&v,p,4); return v; }
static uint8_t *global(unsigned rva) { return memory(base+rva,4u)?*(uint8_t **)(base+rva):NULL; }
static BOOL typed(const void *p,size_t size,unsigned table) {
    return memory(p,size) && *(void *const *)p==base+table;
}
static BOOL code_exact(uint8_t *b) {
    if(!b) return FALSE;
    for(unsigned k=0;k<sizeof(codes)/sizeof(codes[0]);++k) {
        uint32_t h=2166136261u;
        if(!memory(b+codes[k].rva,codes[k].size)) return FALSE;
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
            if(logs++<16u) SudekiMpLogFormat("story_zone_banner event=guard_failed name=%s\r\n",codes[k].name);
            return FALSE;
        }
    }
    return TRUE;
}
static uint8_t *box_exact(void) {
    uint8_t *box=global(RVA_BOX);
    return typed(box,0x1a8u,0x2cb5bcu) && *(void **)(box+0x40u)==base+0x2cb610u?box:NULL;
}
/* Every gate Show checks, plus the owners its lookup and the later hide use. */
static const char *show_refusal(void) {
    if(!code_exact(base)) return "guards";
    /* The constructed table is the derived class (vtable 0x2CA1B0, written
     * at 0x47690A after the base constructor's 0x2CA1B8 at 0x4768EE). */
    uint8_t *text=global(RVA_STRINGS);
    if(!typed(text,0x10u,0x2ca1b0u) || !word(text+4u) || !word(text+8u) || *(uint16_t *)(text+0xcu)<1u) return "strings";
    uint8_t *box=box_exact();
    if(!box) return "box";
    uint8_t *help=global(RVA_HELP),*layer=global(RVA_LAYER),*scene=global(RVA_SCENE),*gizmo=global(RVA_GIZMO);
    if(!typed(help,0x3cu,0x2cb63cu) || word(help+0x38u)!=2u) return "help_box";
    if(!typed(layer,0xc0u,0x2caf9cu) || word(layer+0xb8u)!=1u) return "layer";
    if(!typed(scene,0x174u,0x2c5210u) || scene[0x60u] || *(uint8_t **)(scene+0x170u)!=layer) return "scene";
    if(!typed(gizmo,0x60u,0x2cb784u)) return "gizmo";
    /* Never replace a visible message, ours or the game's. */
    if((box[0x48u]&0x20u) || box[0x1a7u] || (box[0x19du] && (box[0x48u]&1u))) return "box_busy";
    return NULL;
}
typedef struct ShowCall { uint16_t id; const char *refusal; uint32_t now; } ShowCall;
static BOOL show_op(void *context) {
    ShowCall *call=context;
    if((call->refusal=show_refusal())) return FALSE;
    display_call(call->id,0);
    uint8_t *box=box_exact();
    if(box && box[0x1a7u]==1u && box[0x19du]==1u) {
        shown=TRUE; shown_at=call->now?call->now:1u; token=word(box+0x1a0u);
        SudekiMpLogFormat("story_zone_banner event=shown id=%u\r\n",call->id);
    } else {
        call->refusal="native_dropped";
        SudekiMpLogFormat("story_zone_banner event=native_dropped id=%u\r\n",call->id);
    }
    pending=FALSE;
    return FALSE; /* message-box fields only: no entity or task */
}
/* H1b examinable: what the examinable's OnAction does natively on the game
 * that runs it (EnableEnvironmentSound, HideMessageBox, ExaminableUse ->
 * MessageBoxDisplay(text, mode, 0.0, flags)); the gamepad filter it also
 * sets is not needed here (the runtime holds this client's body while the
 * box is up). */
static BOOL examine_op(void *context) {
    ShowCall *call=context;
    if((call->refusal=show_refusal())) return FALSE;
    if(!memory(base+BOX_VTABLE_INPUT_SLOT,4u) || *(void **)(base+BOX_VTABLE_INPUT_SLOT)!=base+RVA_BOX_INPUT) {
        call->refusal="box_input"; return FALSE;
    }
    BOOL sound=FALSE;
    if(examine.emitter && examine.zone) {
        uint8_t *w=global(RVA_WORLD),*d=memory(w,0x10u)?*(uint8_t **)(w+0x0cu):NULL;
        const char *n=memory(d,0x38u)?*(const char **)(d+0x24u):NULL;
        /* Only the settled shop's own emitter (the native call reads the
         * current zone, its +0x14 data when its state is not 1). */
        if(memory(n,1u) && !_stricmp(n,examine.zone) && *(uint32_t *)(d+0x34u)!=1u) {
            ((EnableEmitterCall)(base+RVA_ENABLE_EMITTER))(w,examine.emitter); sound=TRUE;
        }
    }
    ((MessageBoxDisplayCall)(base+RVA_MESSAGE_BOX_DISPLAY))(examine.text,(int)examine.mode,0.0f,(int)examine.flags);
    uint8_t *box=box_exact();
    if(box && box[0x1a7u]==1u && box[0x19du]==1u && box[0x48u]==(uint8_t)examine.flags) {
        examine.shown=TRUE; examine.token=word(box+0x1a0u);
        SudekiMpLogFormat("story_zone_banner event=examine_shown text=%u sound=%u\r\n",examine.text,(unsigned)sound);
    } else {
        call->refusal="native_dropped";
        SudekiMpLogFormat("story_zone_banner event=examine_dropped text=%u sound=%u\r\n",examine.text,(unsigned)sound);
    }
    examine.pending=FALSE;
    return FALSE;
}
/* Hide only the box we showed: still visible, same text token. */
static void hide_ours(const char *why) {
    uint8_t *box=box_exact(),*gizmo=global(RVA_GIZMO);
    const char *policy;
    if(!code_exact(base) || !box || !typed(gizmo,0x60u,0x2cb784u)) policy="unproved_native_timer";
    else if(!box[0x1a7u]) policy="closed";
    else if(word(box+0x1a0u)!=token) policy="not_ours";
    else { hide_call(); policy="token_match"; }
    SudekiMpLogFormat("story_zone_banner event=hidden why=%s policy=%s\r\n",why,policy);
    shown=FALSE; token=0;
}
static BOOL hide_op(void *context) { hide_ours((const char *)context); return FALSE; }

BOOL SudekiMpLanStoryZoneBannerInitialize(HMODULE image) {
    if(!image || !SudekiMpCheckLoadedExecutable(image) || !code_exact((uint8_t *)image)) return FALSE;
    base=(uint8_t *)image; native_thread=0; pending=shown=FALSE; token=0;
    display_call=(DisplayMessageCall)(base+RVA_DISPLAY_MESSAGE);
    hide_call=(HideMessageBoxCall)(base+RVA_HIDE_MESSAGE_BOX);
    return TRUE;
}
void SudekiMpLanStoryZoneBannerRequest(uint16_t text_id,uint32_t now) {
    if(!base || !text_id) return;
    pending=TRUE; pending_id=text_id; pending_at=now?now:1u;
}
void SudekiMpLanStoryZoneBannerCancel(void) { pending=FALSE; }
void SudekiMpLanStoryZoneBannerService(uint32_t now,const SudekiMpControlUpdateDispatchWitness *w) {
    if(!base || !w || w->native_thread_id!=GetCurrentThreadId() ||
        (native_thread && native_thread!=GetCurrentThreadId())) return;
    native_thread=GetCurrentThreadId();
    /* Our examinable box closed natively (confirm) or was replaced. */
    if(examine.shown) {
        uint8_t *box=box_exact();
        if(!box || !box[0x1a7u] || word(box+0x1a0u)!=examine.token) {
            examine.shown=FALSE; SudekiMpLogWrite("story_zone_banner event=examine_closed\r\n");
        }
    }
    if(examine.pending) {
        if(shown) hide_ours("examine"); /* the examinable's own HideMessageBox */
        ShowCall call={examine.text,"residency",now};
        (void)SudekiMpLanStoryClientResidencyCall(w,examine_op,&call);
        if(examine.pending && now-examine.at>SHOW_WINDOW_MS) {
            examine.pending=FALSE;
            SudekiMpLogFormat("story_zone_banner event=examine_refused text=%u reason=%s\r\n",examine.text,call.refusal?call.refusal:"-");
        }
        return;
    }
    if(examine.shown) { pending=FALSE; return; } /* no banner over an examinable box */
    if(shown && now-shown_at>=BANNER_MS)
        (void)SudekiMpLanStoryClientResidencyCall(w,hide_op,(void *)"timer");
    if(!pending) return;
    if(shown) { pending=FALSE; return; } /* ours is still up */
    ShowCall call={pending_id,"residency",now};
    (void)SudekiMpLanStoryClientResidencyCall(w,show_op,&call);
    if(pending && now-pending_at>SHOW_WINDOW_MS) {
        pending=FALSE;
        SudekiMpLogFormat("story_zone_banner event=refused id=%u reason=%s\r\n",pending_id,call.refusal?call.refusal:"-");
    }
}
BOOL SudekiMpLanStoryZoneBannerExamine(const char *zone,uint16_t text,unsigned mode,uint32_t flags,const char *emitter,uint32_t now) {
    if(!base || !text || examine.pending || examine.shown) return FALSE;
    examine.pending=TRUE; examine.text=text; examine.mode=mode; examine.flags=flags; examine.emitter=emitter;
    examine.zone=zone; examine.at=now?now:1u;
    return TRUE;
}
BOOL SudekiMpLanStoryZoneBannerExamineActive(void) { return base && (examine.pending || examine.shown); }
/* The press/release the native HUD forwards to its box (kind 5/6, cmd 0):
 * in its typing state a press finishes the text, then closes the box. */
void SudekiMpLanStoryZoneBannerExamineInput(BOOL press) {
    uint8_t *box;
    if(!base || !examine.shown || !(box=box_exact()) || !box[0x1a7u] || word(box+0x1a0u)!=examine.token ||
        (native_thread && native_thread!=GetCurrentThreadId()) || !code_exact(base)) return;
    uint8_t result=((BoxInputCall)(base+RVA_BOX_INPUT))(box,press?5u:6u,0u,0u);
    SudekiMpLogFormat("story_zone_banner event=examine_input press=%u result=%u\r\n",(unsigned)press,(unsigned)result);
}
void SudekiMpLanStoryZoneBannerExamineClose(const char *why) {
    examine.pending=FALSE;
    if(!examine.shown) return;
    BOOL banner_shown=shown; uint32_t banner_token=token;
    shown=TRUE; token=examine.token; hide_ours(why);   /* the same token-checked hide */
    shown=banner_shown && banner_token!=examine.token; token=banner_token;
    examine.shown=FALSE;
}
void SudekiMpLanStoryZoneBannerReset(void) {
    pending=FALSE;
    if(examine.shown && base && (!native_thread || native_thread==GetCurrentThreadId())) SudekiMpLanStoryZoneBannerExamineClose("teardown");
    memset(&examine,0,sizeof(examine));
    if(shown) {
        if(base && (!native_thread || native_thread==GetCurrentThreadId())) hide_ours("teardown");
        else { SudekiMpLogFormat("story_zone_banner event=hidden why=teardown policy=native_timer\r\n"); shown=FALSE; }
    }
    native_thread=0;
}
