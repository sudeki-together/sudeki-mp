#include "hooks/lan_story_cinematic.h"
#include "cleanroom/engine.h"
#include "engine/log.h"
#include <stdint.h>
#include <string.h>
#include <math.h>

enum {
    CAMERA_MANAGER_GLOBAL=0x409d7cu, CAMERA_MANAGER_VT=0x2c7b80u,
    SCENE_MANAGER_GLOBAL=0x408d58u, SCENE_MANAGER_VT=0x2c66b8u,
    CAMERA_VT=0x2cce5cu, VIEW_VT=0x2dd638u, CAMERA_SLOTS=10u
};
typedef struct CameraObservation {
    uint8_t *manager,*scene_manager,*scene,*camera,*render_state;
    void *cameras[CAMERA_SLOTS];
    char names[CAMERA_SLOTS][SUDEKIMP_STORY_CAMERA_NAME_SIZE];
    unsigned slot;
    uint16_t revision;
} CameraObservation;

static BOOL readable(const void *pointer,size_t size) {
    MEMORY_BASIC_INFORMATION m;
    uintptr_t p=(uintptr_t)pointer;
    if(!pointer || !size || p>UINTPTR_MAX-size ||
        VirtualQuery(pointer,&m,sizeof(m))!=sizeof(m) || m.State!=MEM_COMMIT ||
        (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
    DWORD protection=m.Protect&0xffu;
    if(protection!=PAGE_READONLY && protection!=PAGE_READWRITE &&
        protection!=PAGE_WRITECOPY && protection!=PAGE_EXECUTE_READ &&
        protection!=PAGE_EXECUTE_READWRITE && protection!=PAGE_EXECUTE_WRITECOPY) return FALSE;
    return p+size<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL typed(uint8_t *base,const void *pointer,size_t size,unsigned table) {
    return readable(pointer,size) && *(void *const *)pointer==base+table;
}
static BOOL camera_layout_exact(uint8_t *base) {
    /* Existing exact named-camera evidence: lookup scans ten registration
     * slots by bounded name; SetRenderCamera publishes selected+34 into the
     * primary render-state slot and records manager+20. No native call here. */
    static const uint8_t lookup[]={0x8d,0x7d,0x24,0x8b,0x07,0x85,0xc0,0x74,0x11,0x83,0xc0,0x4c};
    static const uint8_t loop[]={0x46,0x83,0xc7,0x04,0x83,0xfe,0x0a,0x72,0xe0};
    static const uint8_t publish[]={0x8b,0x4c,0x24,0x1c,0x8b,0x41,0x40,0x8b,0x4a,0x34,0x89,0x48,0x7c};
    return SudekiMpCleanroomEngineImageExact((HMODULE)base) &&
        !memcmp(base+0x36ee5u,lookup,sizeof(lookup)) &&
        !memcmp(base+0x36effu,loop,sizeof(loop)) &&
        !memcmp(base+0x370d8u,publish,sizeof(publish)) &&
        !memcmp(base+0x370eau,"\x89\x53\x20",3u);
}
static BOOL observe_camera(uint8_t *base,CameraObservation *out) {
    CameraObservation next={0};
    if(!base || !out || !camera_layout_exact(base)) return FALSE;
    next.manager=*(uint8_t **)(base+CAMERA_MANAGER_GLOBAL);
    next.scene_manager=*(uint8_t **)(base+SCENE_MANAGER_GLOBAL);
    if(!typed(base,next.manager,0x60u,CAMERA_MANAGER_VT) ||
        !typed(base,next.scene_manager,0x44u,SCENE_MANAGER_VT)) return FALSE;
    next.scene=*(uint8_t **)(next.scene_manager+0x40u);
    next.camera=*(uint8_t **)(next.manager+0x20u);
    if(!readable(next.scene,0x8cu) || !next.scene[0x88u] ||
        !typed(base,next.camera,0x108u,CAMERA_VT)) return FALSE;
    next.render_state=*(uint8_t **)(next.camera+0x34u);
    if(!typed(base,next.render_state,0xdcu,VIEW_VT) ||
        *(void **)(next.scene+0x7cu)!=next.render_state) return FALSE;
    unsigned selected=0;
    for(unsigned i=0;i<CAMERA_SLOTS;++i) {
        uint8_t *camera=*(uint8_t **)(next.manager+0x24u+i*4u);
        next.cameras[i]=camera;
        if(!camera) continue;
        if(!typed(base,camera,0x108u,CAMERA_VT)) return FALSE;
        const char *source=(const char *)camera+0x4cu;
        const char *end=memchr(source,0,SUDEKIMP_STORY_CAMERA_NAME_SIZE);
        if(!end || end==source) return FALSE;
        memcpy(next.names[i],source,(size_t)(end-source));
        for(unsigned j=0;j<i;++j) if(next.cameras[j] &&
            (next.cameras[j]==camera || !_stricmp(next.names[i],next.names[j]))) return FALSE;
        if(camera==next.camera) { ++selected; next.slot=i; }
    }
    if(selected!=1u) return FALSE;
    next.revision=*(uint16_t *)(next.render_state+0x2cu);
    *out=next; return TRUE;
}
static BOOL same_camera(const CameraObservation *a,const CameraObservation *b) {
    return a->manager==b->manager && a->scene_manager==b->scene_manager &&
        a->scene==b->scene && a->camera==b->camera && a->render_state==b->render_state &&
        a->slot==b->slot && a->revision==b->revision &&
        !memcmp(a->cameras,b->cameras,sizeof(a->cameras)) &&
        !memcmp(a->names,b->names,sizeof(a->names));
}
BOOL SudekiMpLanStoryCinematicCaptureView(SudekiMpLanStoryCinematicCapture *capture,
    uint32_t epoch,SudekiMpLanStoryView *out,uint32_t *serial,
    char name[SUDEKIMP_STORY_CAMERA_NAME_SIZE],
    SudekiMpLanStoryCinematicExact exact,void *context) {
    CameraObservation before,after;
    SudekiMpLanStoryView view={.valid=1};
    if(!capture || !out || !serial || !name || !exact || !epoch) {
        SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
    }
    DWORD thread=GetCurrentThreadId();
    uint8_t *base=(uint8_t *)GetModuleHandleW(NULL);
    if((capture->native_thread && capture->native_thread!=thread) || !exact(context) ||
        !observe_camera(base,&before)) goto unknown;
    memcpy(view.matrix,before.render_state+0x90u,sizeof(view.matrix));
    memcpy(view.projection,before.render_state+0xd0u,sizeof(view.projection));
    if(!SudekiMpLanStoryViewGeometryValid(&view) || !exact(context) ||
        !observe_camera(base,&after) || !same_camera(&before,&after) ||
        memcmp(view.matrix,after.render_state+0x90u,sizeof(view.matrix)) ||
        memcmp(view.projection,after.render_state+0xd0u,sizeof(view.projection))) goto unknown;
    BOOL changed=!capture->continuous || capture->epoch!=epoch ||
        capture->manager!=before.manager || capture->camera!=before.camera ||
        capture->render_state!=before.render_state ||
        memcmp(capture->camera_name,before.names[before.slot],sizeof(capture->camera_name));
    if(changed && capture->camera_serial==UINT32_MAX) {
        capture->continuous=FALSE; SetLastError(ERROR_ARITHMETIC_OVERFLOW); return FALSE;
    }
    view.camera_serial=capture->camera_serial+(changed?1u:0u);
    if(!SudekiMpLanStoryViewValid(&view)) goto unknown;
    if(changed) ++capture->camera_serial;
    capture->epoch=epoch; capture->native_thread=thread;
    capture->manager=before.manager; capture->camera=before.camera;
    capture->render_state=before.render_state; capture->continuous=TRUE;
    memcpy(capture->camera_name,before.names[before.slot],sizeof(capture->camera_name));
    *out=view; *serial=capture->camera_serial;
    memcpy(name,capture->camera_name,SUDEKIMP_STORY_CAMERA_NAME_SIZE);
    SetLastError(ERROR_SUCCESS); return TRUE;
unknown:
    capture->continuous=FALSE; SetLastError(ERROR_RETRY); return FALSE;
}

typedef struct SpeechObservation {
    uint8_t *speech,*dialogue;
    const uint16_t *text;
    const char *filename;
    uint32_t mode,dialogue_state;
    float elapsed,duration;
    void *speaker;
    uint8_t hide_speaker;
} SpeechObservation;
typedef struct BorderObservation {
    uint8_t *owner;
    uint8_t state[0x34];
    uint32_t width,height;
} BorderObservation;
static BOOL border_draw_exact(uint8_t *base) {
    /* ScreenEffects::Render (ECX=singleton+8), exact border branch. Normalize
     * its two relocated canvas globals before comparing the image digest. */
    const unsigned sites[]={0x89190u,0x89196u},targets[]={0x34eca4u,0x34eca8u};
    uint32_t hash=2166136261u;
    if(!SudekiMpCleanroomEngineImageExact((HMODULE)base) ||
        *(void **)(base+0x2ca118u)!=base+0x89100u) return FALSE;
    for(unsigned j=0;j<2u;++j)
        if(*(void **)(base+sites[j])!=base+targets[j]) return FALSE;
    for(unsigned i=0;i<0xabu;++i) {
        uint8_t byte=base[0x89182u+i];
        for(unsigned j=0;j<2u;++j) {
            unsigned offset=0x89182u+i-sites[j];
            if(offset<4u) byte=(uint8_t)((0x400000u+targets[j])>>(offset*8u));
        }
        hash=(hash^byte)*16777619u;
    }
    return hash==0x171431fbu;
}
static BOOL border_owner(uint8_t *base,BorderObservation *out) {
    BorderObservation b={0};
    if(!border_draw_exact(base)) return FALSE;
    b.owner=*(uint8_t **)(base+0x409db0u);
    if(!typed(base,b.owner,sizeof(b.state),0x2ca110u) ||
        !typed(base,b.owner+8u,4u,0x2ca118u) ||
        !typed(base,b.owner+12u,4u,0x2ca124u)) return FALSE;
    memcpy(b.state,b.owner,sizeof(b.state));
    b.width=*(uint32_t *)(base+0x34eca4u); b.height=*(uint32_t *)(base+0x34eca8u);
    /* Native rendering uses the fixed 640x480 UI coordinate space. These
     * are current animated extents, not the requested destination heights. */
    int32_t top,bottom; uint32_t color;
    memcpy(&top,b.state+0x14u,4u); memcpy(&bottom,b.state+0x20u,4u);
    memcpy(&color,b.state+0x2cu,4u);
    if(b.width!=640u || b.height!=480u || b.state[0x30u]>1u || b.state[0x31u]>1u ||
        color!=0xff000000u || top<0 || bottom<0 || top>192*256 || bottom>192*256)
        return FALSE;
    *out=b; return TRUE;
}
static void border_contents(const BorderObservation *b,SudekiMpLanStoryPresentation *f) {
    int32_t top,bottom;
    memcpy(&top,b->state+0x14u,4u); memcpy(&bottom,b->state+0x20u,4u);
    if(!b->state[0x31u]) return;
    f->flags|=SUDEKIMP_STORY_LETTERBOX_PRESENT;
    f->letterbox=(float)(top/256)/(float)b->height;
    f->letterbox_bottom=(float)(bottom/256)/(float)b->height;
}
static BOOL speech_owner(uint8_t *base,SpeechObservation *out) {
    SpeechObservation s={0};
    static const uint8_t mode[]={0xbf,0x02,0,0,0,0x39,0xbe,0xc8,0,0,0};
    static const uint8_t time[]={0xd9,0x86,0x44,0x04,0,0,0xd8,0x4c,0x24,0x0c,
        0xd8,0x86,0xcc,0,0,0,0xd9,0x9e,0xcc,0,0,0};
    if(!base || !out || !SudekiMpCleanroomEngineImageExact((HMODULE)base) ||
        memcmp(base+0x13f51u,mode,sizeof(mode)) ||
        memcmp(base+0x13fe8u,time,sizeof(time))) return FALSE;
    s.speech=*(uint8_t **)(base+0x408d3cu);
    s.dialogue=*(uint8_t **)(base+0x3c2f94u);
    if(!typed(base,s.speech,0x47cu,0x2c6190u) ||
        !typed(base,s.dialogue,0x6bcu,0x2cb2b4u)) return FALSE;
    s.mode=*(uint32_t *)(s.speech+0xc8u);
    s.dialogue_state=*(uint32_t *)(s.dialogue+0x4cu);
    s.elapsed=*(float *)(s.speech+0xccu); s.duration=*(float *)(s.speech+0xd0u);
    s.text=*(const uint16_t **)(s.speech+0xd8u);
    s.filename=*(const char **)(s.speech+0xacu);
    s.speaker=*(void **)(s.speech+0x3bcu); s.hide_speaker=s.speech[0x478u];
    if(s.mode>2u || s.dialogue_state>9u || *(uint32_t *)(s.dialogue+0x48u)>1u)
        return FALSE;
    if(s.mode!=2u && (!isfinite(s.elapsed) || !isfinite(s.duration) ||
        s.elapsed<0 || s.elapsed>3600 || s.duration<0 || s.duration>3600)) return FALSE;
    *out=s; return TRUE;
}
static BOOL copy_u16(const uint16_t *source,uint16_t *out,unsigned capacity) {
    if(!source || !out) return FALSE;
    uintptr_t boundary=0;
    for(unsigned i=0;i<=capacity;++i) {
        uintptr_t address=(uintptr_t)(source+i);
        if(address>UINTPTR_MAX-2u) return FALSE;
        if(address+2u>boundary) {
            MEMORY_BASIC_INFORMATION m;
            if(!readable(source+i,2u) || VirtualQuery(source+i,&m,sizeof(m))!=sizeof(m)) return FALSE;
            boundary=(uintptr_t)m.BaseAddress+m.RegionSize;
        }
        out[i]=source[i];
        if(!out[i]) return TRUE;
    }
    return FALSE;
}
static BOOL cue_stem(const char *source,char out[SUDEKIMP_STORY_CUE_BYTES+1u]) {
    char name[SUDEKIMP_STORY_CUE_BYTES+1u]={0}; unsigned n=0;
    if(!source) return TRUE;
    uintptr_t boundary=0;
    for(;n<=SUDEKIMP_STORY_CUE_BYTES;++n) {
        uintptr_t address=(uintptr_t)(source+n);
        if(address==UINTPTR_MAX) return FALSE;
        if(address+1u>boundary) {
            MEMORY_BASIC_INFORMATION m;
            if(!readable(source+n,1u) || VirtualQuery(source+n,&m,sizeof(m))!=sizeof(m)) return FALSE;
            boundary=(uintptr_t)m.BaseAddress+m.RegionSize;
        }
        name[n]=source[n]; if(!name[n]) break;
    }
    if(n>SUDEKIMP_STORY_CUE_BYTES) return FALSE;
    if(!n) return TRUE;
    /* Mirror PlayWav's _splitpath basename/extension stripping locally. The
     * original path never leaves this process, and no filesystem call occurs. */
    unsigned first=0,last=n;
    for(unsigned i=0;i<n;++i) if(name[i]=='/' || name[i]=='\\' || name[i]==':') first=i+1u;
    for(unsigned i=first;i<n;++i) if(name[i]=='.') last=i;
    if(last<=first || last-first>SUDEKIMP_STORY_CUE_BYTES) return FALSE;
    memcpy(out,name+first,last-first); out[last-first]=0;
    return SudekiMpLanStoryPresentationCueValid(out);
}
static BOOL speaker_name(const SpeechObservation *s,uint16_t *out) {
    if(s->hide_speaker || !s->speaker) return TRUE;
    uint8_t *base=(uint8_t *)GetModuleHandleW(NULL);
    uint8_t *registry=*(uint8_t **)(base+0x409d8cu),*entity=s->speaker;
    if(!readable(registry,0x40u)) return FALSE;
    uint32_t count=*(uint32_t *)(registry+0x34u);
    void **entries=*(void ***)(registry+0x3cu);
    if(!count || count>8192u || !readable(entries,count*sizeof(void *))) return FALSE;
    unsigned slot=0,matches=0;
    for(unsigned i=0;i<count;++i) if(entries[i]==entity) { slot=i; ++matches; }
    if(matches!=1u || !readable(entity,0x50u)) return FALSE;
    uint8_t *component=*(uint8_t **)(entity+0x4cu);
    /* Native GetSpeakersName follows the entity's CCombat component to its
     * owned UnicodeString. The exact class and actor back-reference apply to
     * both PCs and NPCs; never invoke the weak-pointer resolver here. */
    if(!typed(base,component,0xc8u,0x2cc064u) || *(void **)(component+0x10u)!=entity) return FALSE;
    uint32_t flags=*(uint32_t *)(component+0xc0u);
    const uint16_t *text=(flags&0x80000000u)?(const uint16_t *)(component+0xc4u):
        *(const uint16_t **)(component+0xc4u);
    if(text && !copy_u16(text,out,SUDEKIMP_STORY_SPEAKER_UNITS)) return FALSE;
    return *(void **)(base+0x409d8cu)==registry && *(uint32_t *)(registry+0x34u)==count &&
        *(void **)(registry+0x3cu)==entries && entries[slot]==entity &&
        *(void **)(entity+0x4cu)==component && *(void **)(component+0x10u)==entity &&
        *(uint32_t *)(component+0xc0u)==flags &&
        ((flags&0x80000000u) || *(const uint16_t **)(component+0xc4u)==text);
}
static BOOL speech_contents(const SpeechObservation *s,SudekiMpLanStoryPresentation *out) {
    if(s->dialogue_state || s->mode!=2u) out->flags|=SUDEKIMP_STORY_DIALOGUE_ACTIVE;
    if(s->mode==2u) return TRUE;
    if(!copy_u16(s->text,out->subtitle,SUDEKIMP_STORY_SUBTITLE_UNITS) ||
        !out->subtitle[0] || !cue_stem(s->filename,out->cue) || !speaker_name(s,out->speaker)) return FALSE;
    out->flags|=SUDEKIMP_STORY_LINE_ACTIVE;
    if(out->cue[0]) out->flags|=SUDEKIMP_STORY_CUE_PRESENT;
    if(out->speaker[0]) out->flags|=SUDEKIMP_STORY_SPEAKER_PRESENT;
    out->elapsed_ms=(uint32_t)(s->elapsed*1000.0f);
    out->duration_ms=(uint32_t)(s->duration*1000.0f);
    /* Native null/empty speaker names are not replaced by an invented label. */
    return TRUE;
}
BOOL SudekiMpLanStoryCinematicCaptureSpeech(SudekiMpLanStorySpeechCapture *capture,
    const SudekiMpLanStoryFrame *party,SudekiMpLanStoryPresentation *out,
    SudekiMpLanStoryCinematicExact exact,void *context) {
    SpeechObservation before,after;
    BorderObservation border_before,border_after;
    SudekiMpLanStoryPresentation f={0},check={0};
    if(!capture || !out || !exact || !SudekiMpLanStoryFrameValid(party)) {
        SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
    }
    uint8_t *base=(uint8_t *)GetModuleHandleW(NULL); DWORD thread=GetCurrentThreadId();
    if((capture->native_thread && capture->native_thread!=thread) || !exact(context) ||
        !speech_owner(base,&before) || !speech_contents(&before,&f) ||
        !border_owner(base,&border_before) || !exact(context) ||
        !speech_owner(base,&after) || memcmp(&before,&after,sizeof(before)) ||
        !speech_contents(&after,&check) || memcmp(&f,&check,sizeof(f)) ||
        !border_owner(base,&border_after) || memcmp(&border_before,&border_after,sizeof(border_before))) goto unknown;
    border_contents(&border_before,&f);
    BOOL active=!!(f.flags&SUDEKIMP_STORY_DIALOGUE_ACTIVE);
    BOOL line=!!(f.flags&SUDEKIMP_STORY_LINE_ACTIVE);
    BOOL continuity=capture->continuous && capture->epoch==party->epoch && capture->speech==before.speech;
    BOOL conversation_changed=active && (!continuity || !capture->dialogue_active);
    BOOL line_changed=line && (!continuity || !capture->line_active ||
        capture->text!=before.text || f.elapsed_ms<capture->previous.elapsed_ms ||
        memcmp(f.subtitle,capture->previous.subtitle,sizeof(f.subtitle)) ||
        memcmp(f.speaker,capture->previous.speaker,sizeof(f.speaker)) ||
        memcmp(f.cue,capture->previous.cue,sizeof(f.cue)));
    if((conversation_changed && capture->conversation_serial==UINT32_MAX) ||
        (line_changed && capture->line_serial==UINT32_MAX)) {
        capture->continuous=FALSE; SetLastError(ERROR_ARITHMETIC_OVERFLOW); return FALSE;
    }
    f.epoch=party->epoch; f.revision=party->revision; f.host_tick=party->host_tick; f.sequence=party->sequence;
    f.conversation_serial=capture->conversation_serial+(conversation_changed?1u:0u);
    f.line_serial=capture->line_serial+(line_changed?1u:0u);
    if(!SudekiMpLanStoryPresentationMatches(&f,party)) goto unknown;
    capture->native_thread=thread; capture->epoch=party->epoch; capture->continuous=TRUE;
    capture->conversation_serial=f.conversation_serial; capture->line_serial=f.line_serial;
    capture->speech=before.speech; capture->text=before.text;
    capture->dialogue_active=active; capture->line_active=line; capture->previous=f;
    /* Bounded per-capture diagnostics contain lengths/timing, never dialogue
     * text. A later live run can distinguish capture, audio and draw failures. */
    if(capture->trace_count<64u && (!capture->trace_known || line_changed ||
        capture->trace_flags!=f.flags)) {
        ++capture->trace_count; capture->trace_known=TRUE; capture->trace_flags=f.flags;
        SudekiMpLogFormat("story_cinematic event=captured epoch=%lu line=%lu flags=%u elapsed_ms=%lu bars=%.4f,%.4f\r\n",
            (unsigned long)f.epoch,(unsigned long)f.line_serial,(unsigned)f.flags,
            (unsigned long)f.elapsed_ms,(double)f.letterbox,(double)f.letterbox_bottom);
    }
    *out=f; SetLastError(ERROR_SUCCESS); return TRUE;
unknown:
    capture->continuous=FALSE; SetLastError(ERROR_RETRY); return FALSE;
}
