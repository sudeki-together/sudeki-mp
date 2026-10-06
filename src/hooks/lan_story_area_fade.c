#include "hooks/lan_story_area_fade.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <math.h>
#include <string.h>
#if !defined(__GNUC__) || !defined(__i386__)
#error "Area fade requires the supported x86 ABI"
#endif
enum { WORLD_GLOBAL=0x408d10, CPOSITION_VTABLE=0x2cdefc, RENDERER_VTABLE=0x2df8ec,
       FLAG_HIDDEN=0x4u, FLAG_VISIBILITY_CALLBACK=0x4000000u };
static const float FADE_SECONDS=0.6f;
typedef void (__attribute__((thiscall)) *SetVisible)(void *renderer,int visible);
typedef struct Fade {
    uint8_t *object; const void *world; uint32_t original_colour;
    float alpha; BOOL touched,hidden_set;
} Fade;
static Fade fades[4]; static uint8_t foreign_mask; static BOOL enabled; static DWORD last_tick;
static unsigned logs;
static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || VirtualQuery(p,&m,sizeof(m))!=sizeof(m) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
        a+n>(uintptr_t)m.BaseAddress+m.RegionSize) return FALSE;
    DWORD access=m.Protect&0xffu;
    return access==PAGE_READWRITE || access==PAGE_EXECUTE_READWRITE;
}
static uint8_t *base(void) {
    HMODULE m=GetModuleHandleW(NULL); return m && SudekiMpCheckLoadedExecutable(m)?(uint8_t *)m:NULL;
}
/* The character's world scene object, exactly as the native hide/fade paths
 * reach it; NULL unless every link and both vtables are intact. */
static uint8_t *world_object(const uint8_t *b,const uint8_t *entity) {
    if(!readable(entity,0x48u)) return NULL;
    const uint8_t *position=*(const uint8_t *const *)(entity+0x44u);
    if(!readable(position,0xb8u) || *(const void *const *)position!=b+CPOSITION_VTABLE ||
        *(const void *const *)(position+0x10u)!=entity) return NULL;
    const uint8_t *wrapper=*(const uint8_t *const *)(position+0xb4u);
    if(!readable(wrapper,0x14u)) return NULL;
    uint8_t *object=*(uint8_t *const *)(wrapper+8u);
    const uint8_t *renderer=*(const uint8_t *const *)(wrapper+0x10u);
    if(!readable(object,0xd0u) || *(const void *const *)(object+0x14u)!=renderer ||
        !readable(renderer,0x18u) || *(const void *const *)renderer!=b+RENDERER_VTABLE) return NULL;
    return object;
}
static void set_hidden(uint8_t *object,BOOL hidden) {
    uint32_t flags=*(uint32_t *)(object+0x34u),next=hidden?(flags|FLAG_HIDDEN):(flags&~FLAG_HIDDEN);
    if(next==flags) return;
    /* Native 411680/4E0AA0 order: callback with the new visibility, then store. */
    if(flags&FLAG_VISIBILITY_CALLBACK) {
        const uint8_t *renderer=*(const uint8_t *const *)(object+0x14u);
        SetVisible callback=*(SetVisible *)(*(const uint8_t *const *)renderer+0x14u);
        callback((void *)renderer,hidden?0:1);
    }
    *(uint32_t *)(object+0x34u)=next;
}
static void write_alpha(Fade *f) {
    uint32_t original=f->original_colour; unsigned top=original>>24;
    unsigned a=(unsigned)lroundf(f->alpha*(float)top); if(a>top) a=top;
    *(uint32_t *)(f->object+0x10u)=(original&0x00ffffffu)|((uint32_t)a<<24);
}
static void restore(Fade *f,const uint8_t *b) {
    if(f->touched && f->object && b && *(const void *const *)(b+WORLD_GLOBAL)==f->world &&
        readable(f->object,0xd0u) && readable(*(const uint8_t *const *)(f->object+0x14u),4u) &&
        **(const void *const *const *)(f->object+0x14u)==b+RENDERER_VTABLE) {
        *(uint32_t *)(f->object+0x10u)=f->original_colour;
        if(f->hidden_set) set_hidden(f->object,FALSE);
    }
    memset(f,0,sizeof(*f)); f->alpha=1.0f;
}
void SudekiMpLanStoryAreaFadeEnable(BOOL on) { enabled=on; for(unsigned c=0;c<4u;++c) fades[c].alpha=1.0f; }
void SudekiMpLanStoryAreaFadeSetForeign(uint8_t mask) { foreign_mask=(uint8_t)(mask&15u); }
float SudekiMpLanStoryAreaFadeAlpha(unsigned c) { return c<4u && enabled?fades[c].alpha:1.0f; }
void SudekiMpLanStoryAreaFadeReset(void) {
    const uint8_t *b=base();
    for(unsigned c=0;c<4u;++c) restore(&fades[c],b);
    foreign_mask=0; last_tick=0;
}
void SudekiMpLanStoryAreaFadeApply(const SudekiMpLanStoryNativeRoster *roster) {
    const uint8_t *b=base();
    if(!enabled || !roster || !b) return;
    DWORD now=GetTickCount(); float dt=last_tick?(float)(now-last_tick)*0.001f:0.0f;
    if(dt>0.1f) dt=0.1f; last_tick=now;
    const void *world=*(const void *const *)(b+WORLD_GLOBAL);
    for(unsigned c=0;c<4u;++c) {
        Fade *f=&fades[c];
        BOOL available=(roster->available_mask&(1u<<c))!=0;
        uint8_t *object=available?world_object(b,roster->actors[c]):NULL;
        float target=(foreign_mask&(1u<<c))?0.0f:1.0f;
        if(!object || (f->touched && (f->object!=object || f->world!=world))) { restore(f,b); if(!object) continue; }
        if(!f->touched) {
            if(target>=1.0f) continue; /* nothing to do for a present, visible character */
            f->object=object; f->world=world; f->original_colour=*(uint32_t *)(object+0x10u);
            f->alpha=1.0f; f->touched=TRUE; f->hidden_set=FALSE;
            if(logs<64u) { ++logs; SudekiMpLogFormat("lan_story_area_fade event=begin character=%u object=%p colour=%08lx\r\n",c,(void *)object,(unsigned long)f->original_colour); }
        }
        float step=dt/FADE_SECONDS;
        if(f->alpha<target) { if(f->hidden_set) { set_hidden(object,FALSE); f->hidden_set=FALSE; } f->alpha=fminf(target,f->alpha+step); }
        else if(f->alpha>target) f->alpha=fmaxf(target,f->alpha-step);
        write_alpha(f);
        if(f->alpha<=0.0f && !f->hidden_set) {
            set_hidden(object,TRUE); f->hidden_set=TRUE;
            if(logs<64u) { ++logs; SudekiMpLogFormat("lan_story_area_fade event=hidden character=%u\r\n",c); }
        }
        if(f->alpha>=1.0f && target>=1.0f) {
            if(logs<64u) { ++logs; SudekiMpLogFormat("lan_story_area_fade event=restored character=%u\r\n",c); }
            restore(f,b);
        }
    }
}
