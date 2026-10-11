#include "hooks/lan_story_combo_hud.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Story combo HUD requires the supported x86 ABI"
#endif

enum { HUD_GLOBAL=0x3c2fc4, HUD_VTABLE=0x2cb784, TAL_VTABLE=0x2d5010, BUKI_VTABLE=0x2d5a88,
    COMBO_VTABLE=0x2d4bd4, ACCEPT_CALL=0xd06b0, ACCEPT=0xd14d0,
    RESET=0xd0440, WINDOW=0xd0a3b, HIDE=0xaed20, SHOW=0xae9d0,
    LABEL=0xae7c0, ICONS=0xaf110, EMPTY=0x81ff };
/* Named exact-image guards. WINDOW is after the native queued-input branch:
 * ESI=combo+3c, [ESP+8]=marker. Its seven displaced bytes have no relative
 * operands. The BukiReplicaNativeImageMatches combo_window prefix at d0a10,
 * timing_window at d0a80, and interrupt's call to d0440 remain untouched. */
static const struct {const char *name; unsigned rva,n; uint8_t bytes[15];} codes[]={
    {"ComboAcceptedDisplay",ACCEPT,7,{0x83,0xec,0x10,0x53,0x56,0x8b,0xf0}},
    {"ComboReset",RESET,7,{0x83,0xec,0x14,0x53,0x56,0x8b,0xf0}},
    {"ComboWindowDisplay",WINDOW,7,{0x83,0x7e,0x6c,0,0x8b,0x46,0xd4}},
    {"HudHideMode",HIDE,10,{0x53,0x56,0x57,0x8b,0xf1,0x83,0xf8,6,0x77,0x6e}},
    {"HudShowMode",SHOW,8,{0x53,0x55,0x57,0x83,0xf8,6,0x77,0x5b}},
    {"HudAttackLabel",LABEL,6,{0x83,0xec,0x10,0x56,0x8b,0xf0}},
    {"HudComboIcons",ICONS,15,{0x83,0xec,8,0x56,0x57,0x8b,0xf8,0xb8,3,0,0,0,0x39,0x47,0x48}}
};
static uint8_t *base;
static DWORD native_thread;
static BOOL observing,stopping;
static SudekiMpLanStoryNativeRoster owners;
static struct { void *actor,*component,*group; uint32_t epoch; uint16_t state; } observed[4];
static SudekiMpRelativeCallHook accept_hook;
static SudekiMpInlineHook reset_hook,window_hook;
static void *accept_original __attribute__((used));
static void *reset_original __attribute__((used));
static void *window_original __attribute__((used));
static void *hide_fn,*show_fn,*label_fn,*icons_fn;
static struct {void *hud,*actor; uint32_t epoch,generation; uint16_t state; unsigned character; BOOL active;} shown;

static BOOL memory(const void *p,size_t n,BOOL write) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    if(!p || !n || a+n<a || !VirtualQuery(p,&m,sizeof(m)) || m.State!=MEM_COMMIT ||
        (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) || a+n>(uintptr_t)m.BaseAddress+m.RegionSize) return FALSE;
    return !write || (m.Protect&(PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));
}
static BOOL code_exact(uint8_t *b) {
    if(!b) return FALSE;
    for(unsigned i=0;i<sizeof(codes)/sizeof(codes[0]);++i)
        if(!memory(b+codes[i].rva,codes[i].n,FALSE) ||
            memcmp(b+codes[i].rva,codes[i].bytes,codes[i].n)) {
            SudekiMpLogFormat("story_combo_hud event=guard_failed name=%s rva=%lx\r\n",
                codes[i].name,(unsigned long)codes[i].rva); return FALSE;
        }
    if(!memory(b+ACCEPT_CALL,5,FALSE) || b[ACCEPT_CALL]!=0xe8) return FALSE;
    int32_t rel; memcpy(&rel,b+ACCEPT_CALL+1,4);
    return b+ACCEPT_CALL+5+rel==b+ACCEPT;
}
static BOOL melee_character(unsigned c) { return c==0u || c==2u; }
static uint8_t *combo(const SudekiMpLanStoryNativeRoster *r,unsigned character) {
    uint8_t *a,*c;
    if(!r || !melee_character(character) || !(r->available_mask&(1u<<character))) return NULL;
    a=r->actors[character];
    if(!memory(a,0xbcu,FALSE) ||
        *(void **)a!=base+(character==0u?BUKI_VTABLE:TAL_VTABLE)) return NULL;
    c=*(uint8_t **)(a+0xb8);
    return memory(c,0xb2u,FALSE) && *(void **)c==base+COMBO_VTABLE &&
        *(void **)(c+0x10)==a?c:NULL;
}
static unsigned observation_character(uint8_t *c) {
    if(!observing || stopping || GetCurrentThreadId()!=native_thread || !c ||
        !SudekiMpLanStoryObserverNativeRosterExact(&owners)) return 4u;
    for(unsigned i=0;i<4;i+=2) if(observed[i].actor==owners.actors[i] &&
        observed[i].epoch==owners.epoch && observed[i].group==owners.group &&
        observed[i].component==c && combo(&owners,i)==c) return i;
    return 4u;
}
static void __attribute__((used,noinline)) accept_observe(uint8_t *c) {
    unsigned character=observation_character(c); if(character>=4u) return;
    uint16_t *state=&observed[character].state;
    unsigned stage=*(uint32_t *)(c+0x64);
    if(stage>=6u) {*state=0; return;}
    unsigned kind=*(uint32_t *)(c+0x68+4u*stage);
    if(kind<1u || kind>2u) {*state=0; return;}
    if(!stage) *state=EMPTY;
    if(!*state) return; /* Unknown partial history waits for a native reset. */
    unsigned cue=kind-1u+(c[0xa0+stage]?0u:2u);
    for(unsigned i=0;i<3u;++i) if(((*state>>(i*3u))&7u)==7u) {
        *state=(uint16_t)((*state&~(7u<<(i*3u)))|(cue<<(i*3u)));
        *state&=(uint16_t)~0x200u; break;
    }
}
static void __attribute__((used,noinline)) reset_observe(uint8_t *c) {
    unsigned character=observation_character(c);
    if(character<4u) observed[character].state=EMPTY;
}
static void __attribute__((used,noinline)) window_observe(uint8_t *sub,unsigned marker) {
    unsigned character=observation_character(sub-0x3c); if(character>=4u) return;
    uint16_t *state=&observed[character].state; if(!*state) return;
    if((*state&7u)!=7u && (marker&0xffu)) *state|=0x200u;
    else *state&=(uint16_t)~0x200u;
}
static void __attribute__((naked,noinline)) accepted(void) {
    __asm__ volatile("pushfl\n\tpushal\n\tpushl %eax\n\tcall _accept_observe\n\t"
        "addl $4,%esp\n\tpopal\n\tpopfl\n\tjmp *_accept_original\n\t");
}
static void __attribute__((naked,noinline)) reset(void) {
    __asm__ volatile("pushfl\n\tpushal\n\tpushl %eax\n\tcall _reset_observe\n\t"
        "addl $4,%esp\n\tpopal\n\tpopfl\n\tjmp *_reset_original\n\t");
}
static void __attribute__((naked,noinline)) window(void) {
    __asm__ volatile("pushfl\n\tpushal\n\tpushl 44(%esp)\n\tpushl %esi\n\t"
        "call _window_observe\n\taddl $8,%esp\n\tpopal\n\tpopfl\n\tjmp *_window_original\n\t");
}

BOOL SudekiMpLanStoryComboHudInitialize(HMODULE image,BOOL host) {
    if(base || accept_hook.installed || reset_hook.installed || window_hook.installed) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    if(!SudekiMpCheckLoadedExecutable(image) || !code_exact((uint8_t *)image)) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    base=(uint8_t *)image; stopping=FALSE;
    hide_fn=base+HIDE; show_fn=base+SHOW; label_fn=base+LABEL; icons_fn=base+ICONS;
    if(host) {
        accept_original=base+ACCEPT;
        if(!SudekiMpInstallRelativeCallHook(&accept_hook,base+ACCEPT_CALL,accept_original,(void *)accepted)) goto fail;
        if(!SudekiMpInstallInlineHook(&reset_hook,base+RESET,codes[1].bytes,7,(void *)reset)) goto fail;
        reset_original=reset_hook.trampoline;
        if(!SudekiMpInstallInlineHook(&window_hook,base+WINDOW,codes[2].bytes,7,(void *)window)) goto fail;
        window_original=window_hook.trampoline;
        observing=TRUE;
    }
    SudekiMpLogFormat("story_combo_hud event=initialized host=%u\r\n",(unsigned)host);
    return TRUE;
fail: {
    DWORD error=GetLastError(); (void)SudekiMpLanStoryComboHudUninstall(); SetLastError(error); return FALSE;
    }
}
uint16_t SudekiMpLanStoryComboHudCapture(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,unsigned character) {
    if(!observing || stopping || !r || !w || !melee_character(character) ||
        !SudekiMpLanStoryObserverRosterStillExact(w,r)) return 0;
    if(native_thread && native_thread!=GetCurrentThreadId()) return 0;
    native_thread=GetCurrentThreadId();
    uint8_t *c=combo(r,character); if(!c) return 0;
    /* Separate histories: capturing Buki must not carry Tal's old epoch or
     * partially accepted row into the same new roster (or vice versa). */
    if(observed[character].epoch!=r->epoch || observed[character].group!=r->group ||
        observed[character].actor!=r->actors[character] || observed[character].component!=c) {
        observed[character].state=0;
        BOOL idle=*(uint32_t *)(c+0x64)==0u;
        for(unsigned i=0;i<6u;++i) if(*(uint32_t *)(c+0x68+i*4u)) idle=FALSE;
        if(idle) observed[character].state=EMPTY;
        observed[character].epoch=r->epoch; observed[character].group=r->group;
        observed[character].actor=r->actors[character]; observed[character].component=c;
    }
    owners=*r;
    uint16_t state=observed[character].state;
    if(!SudekiMpLanStoryComboHudValid(state,character)) state=0;
    static uint16_t last[4]; static uint32_t epoch[4]; static unsigned logs;
    if(logs<160u && (last[character]!=state || epoch[character]!=r->epoch)) {
        ++logs; last[character]=state; epoch[character]=r->epoch;
        SudekiMpLogFormat("story_combo_hud event=captured character=%u state=%04x epoch=%lu\r\n",
            character,state,(unsigned long)r->epoch);
    }
    return state;
}

/* Native HUD adapters preserve callee-saved registers. Mode: EAX=mode,
 * ESI/ECX=HUD; label/icons: EAX=HUD. All four native routines use plain RET. */
static void __attribute__((naked,noinline)) mode_call(void *fn __attribute__((unused)),
    void *hud __attribute__((unused)),unsigned mode __attribute__((unused))) {
    __asm__ volatile("pushl %esi\n\tmovl 12(%esp),%esi\n\tmovl %esi,%ecx\n\t"
        "movl 16(%esp),%eax\n\tcall *8(%esp)\n\tpopl %esi\n\tret\n\t");
}
static void __attribute__((naked,noinline)) refresh_call(void *fn __attribute__((unused)),
    void *hud __attribute__((unused))) {
    __asm__ volatile("movl 8(%esp),%eax\n\tjmp *4(%esp)\n\t");
}
static BOOL hud_exact(const SudekiMpLanStoryNativeRoster *r,uint8_t *h,
    SudekiMpLanStoryReplicaExact exact,void *context) {
    if(!base || !r || !exact || !exact(r,context) || !combo(r,r->leader_character) ||
        !memory(r->controller,0x24cu,FALSE) || *(void **)((uint8_t *)r->controller+0x248)!=r->actors[r->leader_character] ||
        !memory(base+HUD_GLOBAL,4,FALSE) || *(void **)(base+HUD_GLOBAL)!=h ||
        !memory(h,0x440,TRUE) || *(void **)h!=base+HUD_VTABLE || !h[0x44] ||
        *(uint32_t *)(h+0x48)>6u || !isfinite(*(float *)(h+0x1f0)) ||
        *(float *)(h+0x1f0)<=0 || *(float *)(h+0x1f0)>1) return FALSE;
    uint8_t *assets,*entries,*bank,*textures;
    if(!memory(base+0x3c3038,4,FALSE) ||
        !memory((assets=*(uint8_t **)(base+0x3c3038)),0x14,FALSE) ||
        !memory((entries=*(uint8_t **)(assets+0x10)),0xc,FALSE) ||
        !memory((bank=*(uint8_t **)(entries+8)),0x14,FALSE) ||
        !memory((textures=*(uint8_t **)(bank+0x10)),32,FALSE)) return FALSE;
    for(unsigned i=0;i<4u;++i) {
        uint8_t *g=h+0xec+i*0x40; void **v=*(void ***)g;
        if(!memory(v,0x18,FALSE) || !v[5]) return FALSE;
    }
    return TRUE;
}
static BOOL change_mode(const SudekiMpLanStoryNativeRoster *r,uint8_t *h,unsigned mode,
    SudekiMpLanStoryReplicaExact exact,void *ctx) {
    if(!hud_exact(r,h,exact,ctx)) return FALSE;
    unsigned old=*(uint32_t *)(h+0x48);
    if(old!=mode) {
        mode_call(hide_fn,h,old);
        if(!hud_exact(r,h,exact,ctx)) return FALSE;
        mode_call(show_fn,h,mode);
        if(!hud_exact(r,h,exact,ctx)) return FALSE;
        *(uint32_t *)(h+0x48)=mode;
    }
    refresh_call(label_fn,h);
    return hud_exact(r,h,exact,ctx);
}
BOOL SudekiMpLanStoryComboHudRetains(void) {return shown.active;}
BOOL SudekiMpLanStoryComboHudRelease(const SudekiMpLanStoryNativeRoster *r,
    SudekiMpLanStoryReplicaExact exact,void *ctx) {
    if(!shown.active) return TRUE;
    if(!base || !exact || !exact(r,ctx) || !memory(base+HUD_GLOBAL,4,FALSE)) return FALSE;
    uint8_t *h=*(uint8_t **)(base+HUD_GLOBAL);
    /* A positively replaced widget belongs to the native UI, not this lease. */
    if(h==shown.hud && r->leader_character==shown.character &&
        r->actors[shown.character]==shown.actor) {
        if(!hud_exact(r,h,exact,ctx)) return FALSE;
        if(*(uint32_t *)(h+0x48)==3u && !change_mode(r,h,0,exact,ctx)) return FALSE;
    }
    memset(&shown,0,sizeof(shown)); return TRUE;
}
BOOL SudekiMpLanStoryComboHudPresent(const SudekiMpLanStoryNativeRoster *r,
    const SudekiMpLanStoryFrame *f,BOOL visible,SudekiMpLanStoryReplicaExact exact,void *ctx) {
    if(!base || observing || stopping || !r || !f || !exact || !exact(r,ctx) ||
        !SudekiMpLanStoryFrameValid(f) || !memory(base+HUD_GLOBAL,4,FALSE)) return FALSE;
    uint8_t *h=*(uint8_t **)(base+HUD_GLOBAL);
    unsigned character=r->leader_character;
    if(!visible || !melee_character(character) || !(f->available_mask&(1u<<character)) ||
        !f->combat_mode || !f->actors[character].hp || !f->actors[character].combo_hud ||
        !f->actors[character].weapon_visible || f->actors[character].weapon_attachment[0]!=1u ||
        (character==0u && f->actors[character].weapon_attachment[1]!=1u))
        return SudekiMpLanStoryComboHudRelease(r,exact,ctx);
    if(!hud_exact(r,h,exact,ctx)) return FALSE;
    /* Native contextual/pickup/cinematic conditions outrank melee UI. The
     * replica deliberately has no native combat bit; only UI is projected. */
    uint32_t flags=*(uint32_t *)(h+0x58);
    if(!(flags&0x80u) || (flags&(0x2000000u|0x8000000u|0x80000u|0x400000u|2u)))
        return SudekiMpLanStoryComboHudRelease(r,exact,ctx);
    BOOL fresh=!shown.active || shown.hud!=h || shown.actor!=r->actors[r->leader_character] ||
        shown.epoch!=f->epoch || shown.generation!=f->actors[character].generation || *(uint32_t *)(h+0x48)!=3u;
    if(fresh) {
        shown.active=TRUE; shown.hud=h; shown.actor=r->actors[character]; shown.character=character;
        shown.epoch=f->epoch; shown.generation=f->actors[character].generation; shown.state=0;
        if(!change_mode(r,h,3u,exact,ctx)) return FALSE;
    }
    uint16_t state=f->actors[character].combo_hud;
    BOOL changed=fresh || shown.state!=state;
    for(unsigned i=0;i<3u;++i) {
        unsigned cue=(state>>(i*3u))&7u;
        if(*(uint32_t *)(h+0x1f4+4u*i)!=cue) changed=TRUE;
        *(uint32_t *)(h+0x1f4+4u*i)=cue;
    }
    /* Preserve the native blink clock across repeated frames and marker-only
     * changes. A changed row ending in weak starts its authored flash. */
    if(fresh || (shown.state&0x1ffu)!=(state&0x1ffu)) {
        h[0x200]=h[0x201]=((state>>6u)&7u)==0u;
        if(h[0x200]) *(float *)(h+0x1ec)=*(float *)(h+0x1f0);
    }
    if(h[0x202]!=!!(state&0x200u)) changed=TRUE;
    h[0x202]=!!(state&0x200u); h[0x203]=0; /* Display never accepts local inputs. */
    shown.state=state;
    if(changed) {
        refresh_call(icons_fn,h);
        if(!hud_exact(r,h,exact,ctx)) return FALSE;
        static unsigned logs;
        if(logs<160u) {++logs; SudekiMpLogFormat("story_combo_hud event=presented state=%04x tick=%lu\r\n",
            state,(unsigned long)f->host_tick);}
    }
    return TRUE;
}
BOOL SudekiMpLanStoryComboHudUninstall(void) {
    stopping=TRUE;
    if(shown.active || (native_thread && native_thread!=GetCurrentThreadId())) return FALSE;
    BOOL ok=TRUE;
    if(window_hook.installed && !SudekiMpRestoreInlineHook(&window_hook)) ok=FALSE;
    if(reset_hook.installed && !SudekiMpRestoreInlineHook(&reset_hook)) ok=FALSE;
    if(accept_hook.installed && !SudekiMpRestoreRelativeCallHook(&accept_hook)) ok=FALSE;
    if(!ok) return FALSE;
    base=NULL; observing=FALSE; native_thread=0; memset(observed,0,sizeof(observed));
    memset(&owners,0,sizeof(owners));
    accept_original=reset_original=window_original=NULL;
    hide_fn=show_fn=label_fn=icons_fn=NULL;
    return TRUE;
}
