#include "hooks/lan_story_name_tags.h"
#include "hooks/lan_story_area_fade.h"
#include "hooks/lan_story_runtime.h"
#include "hooks/call_hook.h"
#include "cleanroom/engine.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include "ui/title_menu_view.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#if !defined(__GNUC__) || !defined(__i386__)
#error "Name tags require the supported x86 ABI"
#endif
enum { NAME_CALL=0xa9eb5, NAME_LOOKUP=0x12a9b0, GROUP=0x408d94, GIZMO_SLOT=0x32c,
    POSITION_VTABLE=0x2cdefc, NAME_CHARS=32 };
static uint8_t *base;
static SudekiMpRelativeCallHook name_hook;
void *SudekiMpLanStoryNameTagsOriginal __attribute__((used));
static wchar_t names[4][NAME_CHARS];
static unsigned hud_logs,tag_logs;
/* Character index (0 Buki,1 Elco,2 Tal,3 Ailish) to cleanroom actor. */
static const SudekiMpCleanroomActor actors[4]={SUDEKIMP_CLEANROOM_BUKI,
    SUDEKIMP_CLEANROOM_ELCO,SUDEKIMP_CLEANROOM_TAL,SUDEKIMP_CLEANROOM_AILISH};
static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || VirtualQuery(p,&m,sizeof(m))!=sizeof(m) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
        a+n>(uintptr_t)m.BaseAddress+m.RegionSize) return FALSE;
    DWORD access=m.Protect&0xffu;
    return access==PAGE_READONLY || access==PAGE_READWRITE || access==PAGE_WRITECOPY ||
        access==PAGE_EXECUTE_READ || access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
}
static unsigned character_of(const void *actor) {
    for(unsigned c=0;c<4u;++c) if(actor && SudekiMpCleanroomEngineActorEntity(actors[c])==actor) return c;
    return 4u;
}
/* Lobby name for a character, as a retained UTF-16 copy; NULL when the
 * character is not held by a named player. */
static const wchar_t *player_name_wide(unsigned c) {
    char name[NAME_CHARS];
    if(c>=4u || !SudekiMpLanStoryRuntimePlayerName(c,name) || !name[0]) return NULL;
    for(unsigned i=0;i<NAME_CHARS;++i) { names[c][i]=(wchar_t)(unsigned char)name[i]; if(!name[i]) break; }
    names[c][NAME_CHARS-1]=0;
    return names[c];
}
static const wchar_t * __attribute__((used,noinline,cdecl)) name_for_gizmo(const uint8_t *gizmo) {
    if(!base || !readable(gizmo,GIZMO_SLOT+4u)) return NULL;
    unsigned slot=*(const uint32_t *)(gizmo+GIZMO_SLOT);
    const uint8_t *group=*(const uint8_t *const *)(base+GROUP);
    if(slot>=4u || !readable(group,0xd4u)) return NULL;
    const void *actor=*(const void *const *)(group+0x90u+slot*0xcu);
    unsigned c=character_of(actor);
    const wchar_t *name=c<4u?player_name_wide(c):NULL;
    if(name && hud_logs<16u) {
        ++hud_logs;
        SudekiMpLogFormat("lan_story_name_tags event=hud_name slot=%u character=%u\r\n",slot,c);
    }
    return name;
}
/* Replaces CALL 0x52A9B0: EAX=table, ECX=enum, EBP=gizmo. Returns the player
 * name in EAX, or tail-calls the native lookup with EAX/ECX intact. pushal
 * saves EAX at [ESP+28]. */
static void __attribute__((naked,noinline)) name_entry(void) {
    __asm__ volatile("pushal; push %ebp; call _name_for_gizmo; add $4,%esp; test %eax,%eax; jz 1f;"
        "mov %eax,28(%esp); popal; ret;"
        "1: popal; jmp *_SudekiMpLanStoryNameTagsOriginal");
}
/* ---- floating tags --------------------------------------------------- */
typedef struct Camera { float matrix[16],near_z,far_z,width,height,fx,fy; } Camera;
static BOOL finite_vector(const float *v,unsigned n,float limit) {
    for(unsigned i=0;i<n;++i) if(!isfinite(v[i]) || fabsf(v[i])>limit) return FALSE;
    return TRUE;
}
/* Same owners and checks as the established dummy overlay projection. */
static BOOL camera(Camera *out) {
    uint8_t *manager=*(uint8_t **)(base+0x409d7c),*mode=*(uint8_t **)(base+0x408da8);
    if(!readable(manager,0x24) || *(void **)manager!=base+0x2c7b80 || !readable(mode,0x10)) return FALSE;
    uint8_t *view=*(uint8_t **)(manager+0x20);
    if(!readable(view,0x38) || *(void **)view!=base+0x2cce5c || *(void **)(mode+0xc)!=view+0x2c) return FALSE;
    uint8_t *render=*(uint8_t **)(view+0x34);
    if(!readable(render,0xdc) || *(void **)render!=base+0x2dd638) return FALSE;
    memcpy(out->matrix,render+0x90,sizeof(out->matrix));
    if(!finite_vector(out->matrix,16,1000000)) return FALSE;
    float fov=*(float *)(render+0xd0),pixel_aspect=*(float *)(base+0x34e9e4);
    out->near_z=*(float *)(render+0xd4); out->far_z=*(float *)(render+0xd8);
    out->width=(float)*(unsigned *)(base+0x34ec9c); out->height=(float)*(unsigned *)(base+0x34eca0);
    if(!isfinite(fov) || fov<0.1f || fov>3 || !isfinite(pixel_aspect) || pixel_aspect<0.1f || pixel_aspect>10 ||
        !isfinite(out->near_z) || !isfinite(out->far_z) || out->near_z<=0 || out->far_z<=out->near_z ||
        out->width<64 || out->width>16384 || out->height<64 || out->height>16384) return FALSE;
    float aspect=(*(uintptr_t *)(base+0x323e74)<=(uintptr_t)(base+0x404838) || base[0x3c31c5])?out->width/out->height:1.0f;
    out->fx=out->width*0.5f/tanf(fov*0.5f)/pixel_aspect;
    out->fy=out->height*0.5f/tanf(fov*0.5f)*aspect;
    return TRUE;
}
static BOOL project(const Camera *c,const float p[3],float *sx,float *sy,float *depth) {
    float v[3]={0,0,0};
    for(unsigned axis=0;axis<3;++axis) for(unsigned k=0;k<3;++k) v[axis]+=(p[k]-c->matrix[12+k])*c->matrix[4*axis+k];
    if(v[2]<c->near_z || v[2]>c->far_z) return FALSE;
    *sx=c->width*0.5f+c->fx*v[0]/v[2]; *sy=c->height*0.5f-c->fy*v[1]/v[2]; *depth=v[2];
    return isfinite(*sx) && isfinite(*sy) && *sx>=0 && *sx<=c->width && *sy>=0 && *sy<=c->height;
}
static float clamp01(float v) { return v<0?0:v>1?1:v; }
/* Fade with camera distance: progressively hidden when a teammate is close
 * to the camera (the name would cover the character) and again beyond a
 * comfortable reading range. Distances are world units (Position+0x18 space). */
enum { FADE_NEAR_START=2, FADE_NEAR_FULL=5, FADE_FAR_FULL=28, FADE_FAR_END=42 };
static float tag_opacity(float depth) {
    float close_fade=clamp01((depth-FADE_NEAR_START)/(float)(FADE_NEAR_FULL-FADE_NEAR_START));
    float distant_fade=clamp01((FADE_FAR_END-depth)/(float)(FADE_FAR_END-FADE_FAR_FULL));
    return close_fade*distant_fade;
}
enum { MARKERS=4, MARKER_CHARS=48, MARKER_HITS=8, REGISTRY=0x409d8c };
static char markers[MARKERS][MARKER_CHARS];
static unsigned marker_count,marker_logs;
void SudekiMpLanStoryNameTagsSetMarkers(const char *names) {
    memset(markers,0,sizeof(markers)); marker_count=0;
    for(const char *s=names;s && *s && marker_count<MARKERS;) {
        while(*s==',' || *s==' ') ++s;
        unsigned k=0;
        while(*s && *s!=',' && k<MARKER_CHARS-1u) markers[marker_count][k++]=*s++;
        while(k && markers[marker_count][k-1]==' ') --k;
        markers[marker_count][k]=0;
        if(k) ++marker_count;
        while(*s && *s!=',') ++s;
    }
}
static BOOL marker_name(const uint8_t *entity,char out[MARKER_CHARS]) {
    const uint32_t *r=readable(entity+0x38u,4u)?*(const uint32_t *const *)(entity+0x38u):NULL;
    if(!readable(r,8u)) return FALSE;
    const char *s=(const char *)(uintptr_t)r[1]; unsigned k=0;
    while(k<MARKER_CHARS-1u && readable(s+k,1u) && s[k]>=0x20 && s[k]<0x7f) { out[k]=s[k]; ++k; }
    out[k]=0; return k!=0;
}
/* Screen point for any world point: inside the view it is the projection;
 * off-screen or behind the camera it is pinned to the edge in its direction. */
static BOOL marker_point(const Camera *c,const float p[3],float *sx,float *sy,float *depth,BOOL *pinned) {
    float v[3]={0,0,0};
    for(unsigned axis=0;axis<3;++axis) for(unsigned k=0;k<3;++k) v[axis]+=(p[k]-c->matrix[12+k])*c->matrix[4*axis+k];
    if(!finite_vector(v,3,10000000)) return FALSE;
    *depth=sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    float cx=c->width*0.5f,cy=c->height*0.5f,mx=c->width*0.06f,my=c->height*0.08f;
    if(v[2]>c->near_z) {
        *sx=cx+c->fx*v[0]/v[2]; *sy=cy-c->fy*v[1]/v[2];
        if(isfinite(*sx) && isfinite(*sy) && *sx>=mx && *sx<=c->width-mx && *sy>=my && *sy<=c->height-my) {
            *pinned=FALSE; return TRUE;
        }
    }
    float dx=v[0],dy=-v[1];
    if(v[2]<=c->near_z && fabsf(dx)<0.001f && fabsf(dy)<0.001f) dy=1.0f; /* straight behind: bottom */
    if(v[2]<=c->near_z && fabsf(dy)<fabsf(dx)*0.25f) dy=fabsf(dx)*0.25f;
    float tx=fabsf(dx)>0.0001f?(cx-mx)/fabsf(dx):1e9f,ty=fabsf(dy)>0.0001f?(cy-my)/fabsf(dy):1e9f;
    float t=tx<ty?tx:ty;
    *sx=cx+dx*t; *sy=cy+dy*t; *pinned=TRUE;
    return isfinite(*sx) && isfinite(*sy);
}
static void render_markers(void *device) {
    if(!marker_count) return;
    const uint8_t *owner=readable(base+REGISTRY,4u)?*(const uint8_t *const *)(base+REGISTRY):NULL;
    if(!readable(owner,0x40u)) return;
    unsigned count=*(const uint32_t *)(owner+0x34u);
    const uint8_t *const *entries=*(const uint8_t *const *const *)(owner+0x3cu);
    if(!count || count>8192u || !readable(entries,count*sizeof(*entries))) return;
    Camera cam; if(!camera(&cam)) return;
    SudekiMpTitleExtras extras; HWND window=NULL; unsigned hits=0;
    memset(&extras,0,sizeof(extras)); extras.overlay=TRUE;
    for(unsigned i=0;i<count && hits<MARKER_HITS;++i) {
        const uint8_t *e=entries[i]; char name[MARKER_CHARS];
        if(!readable(e,0x48u) || !marker_name(e,name)) continue;
        BOOL wanted=FALSE;
        for(unsigned m=0;m<marker_count && !wanted;++m) wanted=!_stricmp(name,markers[m]);
        if(!wanted) continue;
        const uint8_t *position=*(const uint8_t *const *)(e+0x44u);
        if(!readable(position,0x24u) || *(void *const *)position!=base+POSITION_VTABLE ||
            *(const void *const *)(position+0x10u)!=e) continue;
        float at[3]; memcpy(at,position+0x18u,sizeof(at));
        if(!finite_vector(at,3,1000000)) continue;
        at[1]+=1.2f;
        float sx,sy,depth; BOOL pinned;
        if(!marker_point(&cam,at,&sx,&sy,&depth,&pinned)) continue;
        const float size=18.f; float scale=fminf(cam.width/960.f,cam.height/720.f);
        SudekiMpPanelText *t=&extras.texts[extras.text_count++];
        snprintf(t->text,sizeof(t->text),"%s%s %.0fm%s",pinned?"<< ":"[ ",name,depth,pinned?" >>":" ]");
        float pixels=SudekiMpTitleViewTextWidth(t->text,sizeof(t->text),size*scale);
        if(pixels<0) pixels=0;
        float left=sx-pixels*.5f;
        if(left<4.f) left=4.f;
        if(left+pixels>cam.width-4.f) left=cam.width-4.f-pixels;
        t->x=left*960.f/cam.width; t->y=sy*720.f/cam.height; t->size=size; t->width=600.f; t->colour=0xffd21f;
        if(marker_logs<16u) { ++marker_logs;
            SudekiMpLogFormat("lan_story_name_tags event=marker name=%s world=%.1f,%.1f,%.1f screen=%.0f,%.0f distance=%.1f pinned=%u\r\n",
                name,(double)at[0],(double)at[1]-1.2,(double)at[2],(double)sx,(double)sy,(double)depth,(unsigned)pinned); }
        ++hits;
        if(extras.text_count>=SUDEKIMP_PANEL_TEXTS) break;
    }
    if(!extras.text_count) return;
    if(!SudekiMpTitleViewPrepare(device) || !SudekiMpTitleViewDraw(device,1,0,0,NULL,
        SUDEKIMP_TITLE_BUTTON_REST,0,1.0f,&window,&extras)) return;
}
void SudekiMpLanStoryNameTagsRender(void *device) {
    if(!base || !device) return;
    render_markers(device);
    SudekiMpTitleExtras extras; HWND window=NULL; Camera cam;
    BOOL have_camera=FALSE;
    memset(&extras,0,sizeof(extras)); extras.overlay=TRUE;
    for(unsigned c=0;c<4u;++c) {
        char player_name[NAME_CHARS],name[NAME_CHARS+5];
        if(!SudekiMpLanStoryRuntimePlayerName(c,player_name) || !player_name[0]) continue;
        snprintf(name,sizeof(name),"%s%s",player_name,SudekiMpLanStoryRuntimeCharacterAway(c)?" (AI)":"");
        const uint8_t *entity=SudekiMpCleanroomEngineActorEntity(actors[c]);
        if(!readable(entity,0x48u)) continue;
        const uint8_t *position=*(const uint8_t *const *)(entity+0x44u);
        if(!readable(position,0x104u) || *(void *const *)position!=base+POSITION_VTABLE ||
            *(const void *const *)(position+0x10u)!=entity) continue;
        float head[3]; memcpy(head,position+0x18u,sizeof(head));
        if(!finite_vector(head,3,1000000)) continue;
        /* Native CStats (entity+0x4C) carries the unscaled body height used by
         * the engine's own sphere tests at Position+0x18; place the tag just
         * above it. Fall back to a fixed offset when the stats are unavailable. */
        float height=2.0f;
        const uint8_t *stats=*(const uint8_t *const *)(entity+0x4cu);
        if(readable(stats,0x64u) && *(void *const *)stats==base+0x2cc064u && *(const void *const *)(stats+0x10u)==entity) {
            float h=*(const float *)(stats+0x48u);
            if(isfinite(h) && h>0 && h<100) height=h;
        }
        head[1]+=height+0.6f;
        if(!have_camera) { if(!camera(&cam)) return; have_camera=TRUE; }
        float sx,sy,depth;
        if(!project(&cam,head,&sx,&sy,&depth)) continue;
        float opacity=tag_opacity(depth)*SudekiMpLanStoryAreaFadeAlpha(c);
        if(opacity<0.03f) continue;
        /* Overlay text is placed on a 960x720 grid stretched over the whole
         * viewport (x scales by W/960, y by H/720) and drawn left-aligned with
         * its size scaled by min(W/960,H/720); center it on the projection. */
        const float size=16.f;
        float scale=fminf(cam.width/960.f,cam.height/720.f);
        float pixels=SudekiMpTitleViewTextWidth(name,sizeof(name),size*scale);
        if(pixels<0) continue;
        /* One overlay submission per tag: the overlay renderer takes a single
         * opacity, and each tag fades on its own distance. */
        memset(&extras,0,sizeof(extras)); extras.overlay=TRUE;
        SudekiMpPanelText *t=&extras.texts[extras.text_count++];
        t->x=(sx-pixels*.5f)*960.f/cam.width; t->y=sy*720.f/cam.height; t->size=size; t->width=400.f;
        snprintf(t->text,sizeof(t->text),"%s",name);
        if(tag_logs<8u) {
            ++tag_logs;
            SudekiMpLogFormat("lan_story_name_tags event=tag character=%u screen=%.0f,%.0f depth=%.1f opacity=%.2f\r\n",c,sx,sy,depth,opacity);
        }
        if(!SudekiMpTitleViewPrepare(device) || !SudekiMpTitleViewDraw(device,1,0,0,NULL,
            SUDEKIMP_TITLE_BUTTON_REST,0,opacity,&window,&extras)) {
            static BOOL failed_logged;
            if(!failed_logged) { failed_logged=TRUE; SudekiMpLogFormat("lan_story_name_tags event=draw_unavailable error=%lu\r\n",(unsigned long)GetLastError()); }
            return;
        }
    }
}
BOOL SudekiMpLanStoryNameTagsUninstall(void) {
    if(!base) return TRUE;
    if(!SudekiMpRestoreRelativeCallHook(&name_hook)) return FALSE;
    base=NULL; return TRUE;
}
BOOL SudekiMpLanStoryNameTagsInstall(HMODULE image) {
    static const uint8_t site[9]={0x8b,0xc8,0x8b,0xc7,0xe8};        /* MOV ECX,EAX; MOV EAX,EDI; CALL */
    static const uint8_t lookup[6]={0xf7,0xc1,0x00,0x10,0x00,0x00}; /* TEST ECX,0x1000 */
    uint8_t *b=(uint8_t *)image;
    if(base || !b || !SudekiMpCheckLoadedExecutable(image)) {SetLastError(ERROR_INVALID_STATE); return FALSE;}
    if(!readable(b+NAME_CALL-4u,9u) || memcmp(b+NAME_CALL-4u,site,5u) ||
        !readable(b+NAME_LOOKUP,6u) || memcmp(b+NAME_LOOKUP,lookup,6u)) {SetLastError(ERROR_INVALID_DATA); return FALSE;}
    SudekiMpLanStoryNameTagsOriginal=b+NAME_LOOKUP;
    if(!SudekiMpInstallRelativeCallHook(&name_hook,b+NAME_CALL,b+NAME_LOOKUP,(const void *)(uintptr_t)name_entry)) return FALSE;
    base=b;
    SudekiMpLogWrite("lan_story_name_tags event=installed seam=hud_gizmo_name tags=overlay\r\n");
    return TRUE;
}
