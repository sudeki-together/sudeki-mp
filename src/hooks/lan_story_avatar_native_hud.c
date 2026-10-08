#include "hooks/lan_story_avatar_native_hud.h"
#include "hooks/call_hook.h"
#include "hooks/title_portraits.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "The native player HUD requires the supported x86 ABI"
#endif
enum { WORLD=0x408d10,UI_SCENE=0x408d1c,HUD=0x3c2f9c,UI_CONTROLLER=0x3c2f88,
    HUD_VT=0x2cb3e4,GROUP_VT=0x2d9004,GIZMO_VT=0x2cb590,GIZMO_UI_VT=0x2cb59c,
    ICON_VT=0x2d8524,ICON_CALLBACK_VT=0x2d8540,BAR_VT=0x2d9024,
    ANIMATED_VT=0x2d1da8,NODE_VT=0x2d1df0,MATERIAL_VT=0x2deb7c,
    UI_CONTROLLER_VT=0x2caf9c,MODEL_RENDER_VT=0x2dd700,
    GROUP_DRAW=0x1814e0,GIZMO_UPDATE=0xa95e0,RATIOS=0xa9d40,
    STATUS=0xaac90,PORTRAIT=0xaab00,TEXT=0x9930,BAR_SET=0x182230,
    ICON_SET=0x15c0e0,ICON_SELECT=0x15c070,STATE=0x15c2d0,
    COUNT=4,CHILD_COUNT=10 };
static const unsigned child_offsets[CHILD_COUNT]={4,0x2c,0x6c,0xac,0xec,0x12c,0x188,0x1e4,0x224,0x264};
typedef void (__attribute__((thiscall)) *Update)(void *,float);
typedef void (__attribute__((thiscall)) *Draw)(void *);
typedef void (__attribute__((thiscall)) *State)(void *);
typedef struct RowLease {
    uint8_t *gizmo,*animation,*node,*render_object;
    void *model,*scene_renderer;
    uint32_t original_request[CHILD_COUNT],owned_request[CHILD_COUNT];
    uint32_t original_status,original_portrait;
    unsigned character;
    BOOL captured,changed,portrait_changed,restored;
} RowLease;
static uint8_t *base,*layer;
static SudekiMpLanStoryAvatarNativeHudIdentity identity;
static SudekiMpLanStoryAvatarNativeHudObserve observe;
static void *observe_context;
static DWORD native_thread;
static BOOL bound,busy,active,restoring;
static volatile LONG callbacks;
static RowLease rows[COUNT];
static SudekiMpPointerHook draw_hook,update_hook;
static const unsigned call_rvas[]={0xa97cb,0xa9608,0xa97b7,0xa5ffe,0xa5973,0xaa965,0x15b92c,0x181e43};
static SudekiMpRelativeCallHook call_hooks[8];
static Draw original_draw;
static Update original_update;
static void *ratio_original __attribute__((used));
static void *status_original __attribute__((used));
static void *portrait_original __attribute__((used));
static unsigned reports;

static BOOL memory(const void *p,size_t n,BOOL write) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || VirtualQuery(p,&m,sizeof(m))!=sizeof(m) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
        a+n>(uintptr_t)m.BaseAddress+m.RegionSize) return FALSE;
    DWORD access=m.Protect&0xffu;
    if(write) return access==PAGE_READWRITE || access==PAGE_WRITECOPY ||
        access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
    return access==PAGE_READONLY || access==PAGE_READWRITE || access==PAGE_WRITECOPY ||
        access==PAGE_EXECUTE_READ || access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
}
static void *ptr(const void *p,unsigned o) { return *(void *const *)((const uint8_t *)p+o); }
static uint32_t word(const void *p,unsigned o) { return *(const uint32_t *)((const uint8_t *)p+o); }
static BOOL object(const void *p,unsigned n,unsigned vt,BOOL write) {
    return memory(p,n,write) && ptr(p,0)==base+vt;
}
static BOOL fail(unsigned reason,const char *name) {
    active=FALSE;
    if(reason<32u && !(reports&(1u<<reason))) {
        reports|=1u<<reason;
        SudekiMpLogFormat("avatar_native_hud event=pending reason=%s epoch=%lu retained=%u\r\n",
            name,(unsigned long)identity.epoch,bound);
    }
    SetLastError(ERROR_NOT_READY); return FALSE;
}
static BOOL signature(unsigned rva,unsigned n,uint32_t expected,const unsigned *relocs,unsigned count) {
    if(!memory(base+rva,n,FALSE)) return FALSE;
    uint32_t hash=2166136261u,delta=(uint32_t)(uintptr_t)base-0x400000u;
    for(unsigned i=0;i<n;++i) {
        uint8_t value=base[rva+i];
        for(unsigned j=0;j<count;++j) if(i>=relocs[j] && i<relocs[j]+4u) {
            uint32_t v; memcpy(&v,base+rva+relocs[j],4);
            value=(uint8_t)((v-delta)>>((i-relocs[j])*8u)); break;
        }
        hash=(hash^value)*16777619u;
    }
    return hash==expected;
}
static BOOL code_exact(void) {
    static const unsigned bar_relocs[]={0x22,0x30,0x51};
    return base && signature(TEXT,0x51,0xd80d5763u,NULL,0) &&
        signature(0x9810,0x8f,0x1dc08f85u,NULL,0) &&
        signature(0x1b9fc0,0xf3,0xd4dbada9u,NULL,0) &&
        signature(BAR_SET,0x117,0x63256411u,bar_relocs,3) &&
        signature(STATE,0x28,0x9549b46bu,NULL,0) &&
        signature(0x15c230,0x3e,0x0f7ac078u,NULL,0) &&
        signature(ICON_SET,0xc4,0xff32845au,NULL,0);
}
static BOOL hooks_exact(void) {
    if(!(draw_hook.installed && update_hook.installed &&
        ptr(draw_hook.slot,0)==draw_hook.replacement_value &&
        ptr(update_hook.slot,0)==update_hook.replacement_value)) return FALSE;
    for(unsigned n=0;n<8;++n) {
        const SudekiMpRelativeCallHook *h=&call_hooks[n]; int32_t displacement;
        if(!h->installed || !memory(h->instruction,5,FALSE) || h->instruction[0]!=0xe8) return FALSE;
        memcpy(&displacement,h->instruction+1,4);
        if(displacement!=h->replacement_displacement) return FALSE;
    }
    return TRUE;
}
static BOOL current_globals(void) {
    return bound && native_thread==GetCurrentThreadId() && code_exact() && hooks_exact() &&
        memory(base+WORLD,4,FALSE) && ptr(base,WORLD)==identity.world &&
        memory(base+UI_SCENE,4,FALSE) && ptr(base,UI_SCENE)==identity.scene_manager &&
        memory(identity.scene_manager,0x174,TRUE) && memory(base+HUD,4,FALSE) &&
        memory(base+UI_CONTROLLER,4,FALSE);
}
static BOOL layer_exact(void) {
    if(!current_globals() || !layer || ptr(base,HUD)!=layer ||
        !object(layer,0x1a4,HUD_VT,TRUE) || !object(layer+0x10c,0x58,GROUP_VT,TRUE)) return FALSE;
    uint8_t *controller=ptr(base,UI_CONTROLLER);
    return object(controller,0x70,UI_CONTROLLER_VT,FALSE) && ptr(controller,0x6c)==layer &&
        ptr(identity.scene_manager,0x170)==controller;
}
static BOOL icon_exact(const uint8_t *icon,const RowLease *row,BOOL material) {
    if(!object(icon,0x40,ICON_VT,TRUE) || ptr(icon,0x28)!=base+ICON_CALLBACK_VT ||
        ptr(icon,0x30)!=row->node || *(const uint16_t *)(icon+0x2c)==UINT16_MAX) return FALSE;
    /* AA170/15BE70 bind geometric overlays even when their material name is
     * empty. 15C020 toggles the node anchor without dereferencing a material.
     * Texture-bearing portrait/chrome still require their exact material. */
    return (!material && !ptr(icon,0x34)) || object(ptr(icon,0x34),0x10,MATERIAL_VT,TRUE);
}
static BOOL unused_icon_exact(const uint8_t *icon) {
    /* A9060 constructs gizmo+EC but AA170 neither binds nor registers it in
     * the native child list. Preserve this exact dormant state; never call
     * its native state setter or invent an owner/anchor for it. */
    return object(icon,0x40,ICON_VT,TRUE) && ptr(icon,0x28)==base+ICON_CALLBACK_VT &&
        *(const uint16_t *)(icon+0x2c)==UINT16_MAX && !icon[0x2e] &&
        !ptr(icon,0x30) && !ptr(icon,0x34) && !ptr(icon,0x3c) &&
        word(icon,0x1c)==3u && word(icon,0x20)<=3u && word(icon,0x24)==3u;
}
static BOOL bar_exact(const uint8_t *bar,const RowLease *row) {
    if(!object(bar,0x5c,BAR_VT,TRUE) || ptr(bar,0x2c)!=row->node ||
        word(bar,0x50)!=2 || !memory(ptr(bar,0x34),8,FALSE) ||
        !memory(ptr(bar,0x4c),8,FALSE) || !memory(ptr(bar,0x58),8,TRUE)) return FALSE;
    for(unsigned k=0;k<2;++k) {
        if(((const int *)ptr(bar,0x4c))[k]<0 ||
            !object(((void *const *)ptr(bar,0x34))[k],0x10,MATERIAL_VT,TRUE)) return FALSE;
    }
    return TRUE;
}
static BOOL row_exact(unsigned slot) {
    if(slot>=COUNT || !layer_exact()) return FALSE;
    const RowLease *r=&rows[slot]; const uint8_t *g=r->gizmo;
    if(!r->captured || ptr(layer,0x138+4*slot)!=g || !object(g,0xc00,GIZMO_VT,TRUE) ||
        ptr(g,4)!=base+GIZMO_UI_VT || word(g,0x32c)!=slot || ptr(g,0x320)!=r->animation ||
        ptr(layer,0x148+4*slot)!=r->animation ||
        !object(r->animation,0x13c,ANIMATED_VT,TRUE) || ptr(r->animation,0xbc)!=r->node ||
        !object(r->node,0x1c,NODE_VT,FALSE) || ptr(r->node,8)!=r->render_object ||
        ptr(r->node,0xc)!=r->model || ptr(r->node,0x14)!=r->scene_renderer ||
        ptr(identity.scene_manager,0x70)!=r->scene_renderer ||
        !object(r->render_object,0x38,MODEL_RENDER_VT,TRUE) || ptr(r->render_object,0x14)!=r->model)
        return FALSE;
    for(unsigned c=1;c<CHILD_COUNT;++c) {
        unsigned o=child_offsets[c];
        if(c==4) { if(!unused_icon_exact(g+o)) return FALSE; }
        else if(o==0x12c || o==0x188) { if(!bar_exact(g+o,r)) return FALSE; }
        else if(!icon_exact(g+o,r,c==1 || c==2)) return FALSE;
    }
    return TRUE;
}
static BOOL capture(void) {
    if(layer) return layer_exact();
    if(!current_globals()) return fail(0,"world_or_hook");
    uint8_t *candidate=ptr(base,HUD);
    if(!object(candidate,0x1a4,HUD_VT,TRUE) ||
        !object(candidate+0x10c,0x58,GROUP_VT,TRUE) || !candidate[0x134])
        return fail(1,"four_widgets_pending");
    layer=candidate;
    if(!layer_exact()) { layer=NULL; return fail(2,"layer_owner"); }
    for(unsigned s=0;s<COUNT;++s) {
        RowLease *r=&rows[s]; r->gizmo=ptr(layer,0x138+4*s);
        if(!object(r->gizmo,0xc00,GIZMO_VT,TRUE)) goto reject;
        r->animation=ptr(r->gizmo,0x320);
        if(!object(r->animation,0x13c,ANIMATED_VT,TRUE)) goto reject;
        r->node=ptr(r->animation,0xbc);
        if(!object(r->node,0x1c,NODE_VT,FALSE)) goto reject;
        r->render_object=ptr(r->node,8); r->model=ptr(r->node,0xc);
        r->scene_renderer=ptr(r->node,0x14); r->captured=TRUE; r->character=UINT32_MAX;
        if(!row_exact(s)) goto reject;
        for(unsigned c=0;c<CHILD_COUNT;++c) {
            uint32_t request=word(r->gizmo,child_offsets[c]+0x1c);
            if(request>3) goto reject;
            r->original_request[c]=r->owned_request[c]=request;
        }
        r->original_status=word(r->gizmo,0x2ac);
        r->original_portrait=word(r->gizmo,0x2a8);
        if(r->original_portrait>=16u) goto reject;
    }
    return TRUE;
reject:
    memset(rows,0,sizeof(rows)); layer=NULL; return fail(3,"widget_anchor_owner");
}
static BOOL read_model(SudekiMpLanStoryAvatarNativeHudSnapshot *out) {
    memset(out,0,sizeof(*out));
    if(!current_globals() || !observe ||
        !observe(&identity,SUDEKIMP_AVATAR_NATIVE_HUD_PRESENT,out,observe_context) ||
        !current_globals() || out->stats.epoch!=identity.epoch ||
        out->stats.revision!=identity.revision || out->stats.local_player>=COUNT) return FALSE;
    return TRUE;
}
static BOOL fresh_row(const SudekiMpLanStoryAvatarNativeHudSnapshot *s,unsigned player,DWORD now) {
    const SudekiMpStoryAvatarStatsRow *r=&s->stats.rows[player]; unsigned c=s->character[player];
    return r->present && r->epoch==s->stats.epoch && r->revision==s->stats.revision &&
        r->spawn_generation && r->sequence && (DWORD)(now-r->received_tick)<=250u && (c<4u || c==5u) &&
        isfinite(r->hp) && isfinite(r->max_hp) && isfinite(r->sp) && isfinite(r->max_sp) &&
        r->max_hp>0 && r->hp>=0 && r->hp<=r->max_hp && r->sp>=0 &&
        r->max_sp>=0 && r->sp<=r->max_sp;
}
static unsigned player_for_slot(const SudekiMpLanStoryAvatarNativeHudSnapshot *s,unsigned slot) {
    if(!slot) return s->stats.local_player;
    unsigned n=1;
    for(unsigned p=0;p<COUNT;++p) if(p!=s->stats.local_player && n++==slot) return p;
    return COUNT;
}
/* Native bar setter: EDI=UIBar,EAX=submesh,one float stack arg,ret4. */
static void native_bar(void *bar,unsigned index,float value) {
    __asm__ volatile("push %[value]; call *%[fn]" : "+a"(index) : "D"(bar),
        [value]"m"(value),[fn]"r"(base+BAR_SET) : "ecx","edx","memory","cc");
}
static void native_select(void *icon,unsigned resource) {
    unsigned synchronous=1;
    __asm__ volatile("push %[icon]; call *%[fn]" : "+a"(synchronous),"+c"(resource) : [icon]"r"(icon),
        [fn]"r"(base+ICON_SELECT) : "edx","memory","cc");
}
static void (*select_resource)(void *,unsigned)=native_select;
/* ResourceName.kind=SQX(42),numeric ID already verified against the native
 * typed constructor and shipped archive index; no retained text reference.
 * This uses the same native resident callback and synchronous request drain
 * as15C070. The native widget owns its resulting material/texture lifetime. */
static void native_portrait(void *icon,unsigned character) {
    static const uint32_t ids[6]={0,0,0,0,0,0xa9bbcf45u};
    if(character==5) {
        __asm__ volatile("push $1; push $0; push %[id]; push $0xfaa; call *%[fn]"
            : : "D"(icon),[id]"r"(ids[5]),[fn]"r"(base+ICON_SET) : "eax","ecx","edx","memory","cc");
    } else {
        static const unsigned enums[4]={3,2,0,1};
        unsigned resource=word(base,0x2c2a94+4*enums[character]);
        select_resource(icon,resource);
    }
}
static void native_state(void *element) {
    State update=(State)ptr(ptr(element,0),0x14); update(element);
}
/* Test fixtures substitute only these call boundaries; production always
 * uses the exact native ABI wrappers above. */
static void (*set_bar)(void *,unsigned,float)=native_bar;
static void (*set_portrait)(void *,unsigned)=native_portrait;
static void (*set_state)(void *)=native_state;

static BOOL request(unsigned slot,unsigned child,uint32_t state) {
    if(!row_exact(slot)) return FALSE;
    RowLease *r=&rows[slot]; uint8_t *element=r->gizmo+child_offsets[child];
    if(word(element,0x1c)==state) { r->owned_request[child]=state; return TRUE; }
    r->changed=TRUE; r->owned_request[child]=state;
    *(uint32_t *)(element+0x1c)=state; set_state(element);
    return row_exact(slot) && word(element,0x1c)==state;
}
static BOOL same_player(const SudekiMpLanStoryAvatarNativeHudSnapshot *before,unsigned p) {
    SudekiMpLanStoryAvatarNativeHudSnapshot after;
    return read_model(&after) && fresh_row(&after,p,GetTickCount()) &&
        after.stats.local_player==before->stats.local_player &&
        after.character[p]==before->character[p] &&
        after.stats.rows[p].spawn_generation==before->stats.rows[p].spawn_generation;
}
static BOOL row_present(unsigned slot,const SudekiMpLanStoryAvatarNativeHudSnapshot *s,BOOL model) {
    if(!row_exact(slot)) return FALSE;
    RowLease *r=&rows[slot]; unsigned p=model?player_for_slot(s,slot):COUNT;
    BOOL show=p<COUNT && fresh_row(s,p,GetTickCount());
    /* Entire-row parent and all native children are explicit. Unreplicated
     * status/tactic/highlight content is hidden, never read from a hero. */
    if(show && r->character!=s->character[p]) {
        uint8_t *icon=r->gizmo+0x2c;
        if(ptr(icon,0x3c)) return fail(4,"portrait_request_in_flight");
        r->portrait_changed=TRUE;
        set_portrait(icon,s->character[p]);
        if(!row_exact(slot) || !same_player(s,p) || ptr(icon,0x3c) || icon[0x2e]!=1)
            return fail(5,"portrait_not_resident");
        r->character=s->character[p];
    }
    r->changed=TRUE; *(uint32_t *)(r->gizmo+0x2ac)=0;
    for(unsigned c=1;c<CHILD_COUNT;++c) {
        if(c==4) continue; /* Unregistered constructor-only icon stays at3. */
        BOOL essential=c==1 || c==2 || c==5 || c==6 || c==9;
        if(!request(slot,c,show && essential?0u:2u)) return FALSE;
    }
    if(!request(slot,0,show?0u:2u)) return FALSE;
    if(show) {
        const SudekiMpStoryAvatarStatsRow *v=&s->stats.rows[p];
        float hp=v->hp/v->max_hp,sp=v->max_sp>0?v->sp/v->max_sp:0;
        for(unsigned k=0;k<2;++k) {
            if(!row_exact(slot) || !same_player(s,p)) return FALSE;
            set_bar(r->gizmo+0x12c,k,hp);
            if(!row_exact(slot) || !same_player(s,p)) return FALSE;
            set_bar(r->gizmo+0x188,k,sp);
        }
    }
    return row_exact(slot);
}
static int owned_slot(const void *gizmo) {
    if(!bound || !layer) return -1;
    for(unsigned s=0;s<COUNT;++s) if(rows[s].gizmo==gizmo) return (int)s;
    return -1;
}
static BOOL service(void) {
    if(!capture()) return FALSE;
    SudekiMpLanStoryAvatarNativeHudSnapshot s; BOOL model=read_model(&s),okay=TRUE;
    for(unsigned n=0;n<COUNT;++n) if(!row_present(n,&s,model)) okay=FALSE;
    if(!okay) for(unsigned n=0;n<COUNT;++n) (void)request(n,0,2u);
    active=okay && model;
    if(active && !(reports&0x80000000u)) {
        reports|=0x80000000u;
        SudekiMpLogWrite("avatar_native_hud event=active widgets=4 values=authoritative presentation=native\r\n");
    }
    return active;
}
static BOOL __attribute__((noinline,used,force_align_arg_pointer)) ratio_handle(void *gizmo) {
    int slot=owned_slot(gizmo); if(slot<0 || restoring) return FALSE;
    InterlockedIncrement(&callbacks);
    if(!busy) { busy=TRUE; SudekiMpLanStoryAvatarNativeHudSnapshot s;
        BOOL model=read_model(&s); if(!row_present((unsigned)slot,&s,model)) active=FALSE; busy=FALSE; }
    InterlockedDecrement(&callbacks); return TRUE;
}
static BOOL __attribute__((noinline,used,force_align_arg_pointer)) status_handle(void *gizmo) {
    int slot=owned_slot(gizmo); if(slot<0 || restoring) return FALSE;
    InterlockedIncrement(&callbacks);
    if(row_exact((unsigned)slot)) { rows[slot].changed=TRUE; *(uint32_t *)((uint8_t *)gizmo+0x2ac)=0; }
    else active=FALSE;
    InterlockedDecrement(&callbacks); return TRUE;
}
static BOOL __attribute__((noinline,used,force_align_arg_pointer)) portrait_handle(void *gizmo) {
    return ratio_handle(gizmo);
}
static void __attribute__((naked,noinline)) ratio_entry(void) {
    __asm__ volatile("pushfl; pushal; push %esi; call _ratio_handle; add $4,%esp; test %eax,%eax; jz 1f;"
        "popal; popfl; ret; 1: popal; popfl; jmp *_ratio_original");
}
static void __attribute__((naked,noinline)) status_entry(void) {
    __asm__ volatile("pushfl; pushal; push %esi; call _status_handle; add $4,%esp; test %eax,%eax; jz 1f;"
        "popal; popfl; ret; 1: popal; popfl; jmp *_status_original");
}
static void __attribute__((naked,noinline)) portrait_entry(void) {
    __asm__ volatile("pushfl; pushal; push 40(%esp); call _portrait_handle; add $4,%esp; test %eax,%eax; jz 1f;"
        "popal; popfl; ret $4; 1: popal; popfl; jmp *_portrait_original");
}
/* Native409930 borrows this view and copies its text to the native scene's
 * owned command string. It never retains or destroys the supplied view. */
static void native_text(const wchar_t *text,int x,int y,unsigned style,uint32_t color) {
    struct { uint32_t length; const wchar_t *data; } view={0,text};
    while(text[view.length]) ++view.length;
    void *scene=identity.scene_manager; void *v=&view;
    uint32_t args[5]={0,style,(uint32_t)x,(uint32_t)y,color};
    __asm__ volatile("push 16(%[args]); push 12(%[args]); push 8(%[args]); push 4(%[args]); push (%[args]); call *%[fn]"
        : "+a"(v),"+c"(scene) : [args]"r"(args),[fn]"r"(base+TEXT) : "edx","memory","cc");
}
static void (*queue_text)(const wchar_t *,int,int,unsigned,uint32_t)=native_text;
static void wide_name(const char in[32],wchar_t out[32],unsigned player) {
    unsigned n=0;
    while(n<31u && in[n]) { unsigned char c=(unsigned char)in[n]; out[n]=(wchar_t)(c>=32u?c:'?'); ++n; }
    if(!n || !memchr(in,0,32)) { swprintf(out,32,L"Player %u",player+1u); return; }
    out[n]=0;
}
static BOOL text_owner(void) {
    uint8_t *scene=identity.scene_manager;
    if(!layer_exact() || !memory(scene,0x174,TRUE)) return FALSE;
    uint8_t *array=ptr(scene,0x12c); unsigned count=word(scene,0x11c),used=word(scene,0x128);
    return memory(array,0x10,FALSE) && count<=4096u && used<=count && word(array,4)<=4096u &&
        (!word(array,4) || memory(ptr(array,0xc),word(array,4)*4u,FALSE));
}
static void __attribute__((thiscall,force_align_arg_pointer)) draw_entry(void *group) {
    BOOL selected=bound && memory(base+HUD,4,FALSE) && ptr(base,HUD) &&
        group==(uint8_t *)ptr(base,HUD)+0x10c;
    if(!selected || restoring) {
        original_draw(group); return;
    }
    InterlockedIncrement(&callbacks);
    if(!busy) {
        busy=TRUE;
        SudekiMpLanStoryAvatarNativeHudSnapshot s;
        if(active && text_owner() && read_model(&s)) {
            for(unsigned slot=0;slot<COUNT;++slot) {
                unsigned p=player_for_slot(&s,slot);
                if(!row_exact(slot) || !fresh_row(&s,p,GetTickCount()) || !same_player(&s,p) ||
                    word(rows[slot].gizmo,0x28)!=0) continue;
                wchar_t name[32],value[48]; wide_name(s.stats.rows[p].name,name,p);
                int x=(int)word(rows[slot].gizmo,0x2d0),y=(int)word(rows[slot].gizmo,0x2d4);
                if(x<0 || x>4096 || y<0 || y>4096) continue;
                uint32_t color=word(base,0x33542c)|0xffu;
                queue_text(name,x,y,2,color);
                if(!slot && text_owner()) {
                    swprintf(value,48,L"%.0f",(double)s.stats.rows[p].hp); queue_text(value,507,345,2,color);
                    queue_text(L"HP",510,413,1,color);
                    swprintf(value,48,L"%.0f",(double)s.stats.rows[p].sp); queue_text(value,507,393,2,color);
                    queue_text(L"SP",510,436,1,color);
                }
            }
        } else active=FALSE;
        busy=FALSE;
    }
    InterlockedDecrement(&callbacks);
}
static void __attribute__((thiscall,force_align_arg_pointer)) update_entry(void *element,float delta) {
    InterlockedIncrement(&callbacks);
    original_update(element,delta);
    if(bound && !restoring && !busy && owned_slot((uint8_t *)element-4)>=0) {
        busy=TRUE; (void)service(); busy=FALSE;
    }
    InterlockedDecrement(&callbacks);
}

BOOL SudekiMpLanStoryAvatarNativeHudInstall(HMODULE image) {
    static const uint8_t ratio_bytes[]={0x83,0xec,0x14,0x8b,0x86,0x2c,0x03,0,0};
    static const uint8_t status_bytes[]={0x83,0xec,0x0c,0x8b,0x86,0x2c,0x03,0,0};
    static const uint8_t portrait_bytes[]={0x55,0x8b,0xec,0x83,0xe4,0xf8};
    if(base || !image || !SudekiMpCheckLoadedExecutable(image) || !SudekiMpTitlePortraitsImageMatches(image)) return FALSE;
    base=(uint8_t *)image;
    if(!code_exact() || !memory(base+GROUP_VT,0x1c,FALSE) || !memory(base+GIZMO_UI_VT,0x18,FALSE) ||
        ptr(base,GROUP_VT+0x18)!=base+GROUP_DRAW || ptr(base,GIZMO_UI_VT+4)!=base+GIZMO_UPDATE ||
        !memory(base+RATIOS,sizeof(ratio_bytes),FALSE) || memcmp(base+RATIOS,ratio_bytes,sizeof(ratio_bytes)) ||
        !memory(base+STATUS,sizeof(status_bytes),FALSE) || memcmp(base+STATUS,status_bytes,sizeof(status_bytes)) ||
        !memory(base+PORTRAIT,sizeof(portrait_bytes),FALSE) || memcmp(base+PORTRAIT,portrait_bytes,sizeof(portrait_bytes))) goto reject;
    /* Only these eight native calls consume actor data. Keep the native
     * entries intact for other adapters' ABI checks and immutable fallback
     * calls; there is no executable trampoline to retire. */
    ratio_original=base+RATIOS; status_original=base+STATUS; portrait_original=base+PORTRAIT;
    original_draw=(Draw)(base+GROUP_DRAW); original_update=(Update)(base+GIZMO_UPDATE);
    for(unsigned n=0;n<8;++n) {
        void *target=n==0?ratio_original:n<4?status_original:portrait_original;
        void *entry=n==0?(void *)ratio_entry:n<4?(void *)status_entry:(void *)portrait_entry;
        if(!memory(base+call_rvas[n],5,FALSE) ||
            !SudekiMpInstallRelativeCallHook(&call_hooks[n],base+call_rvas[n],target,entry)) goto reject;
    }
    if(!SudekiMpInstallPointerHook(&update_hook,(void **)(base+GIZMO_UI_VT+4),base+GIZMO_UPDATE,update_entry) ||
       !SudekiMpInstallPointerHook(&draw_hook,(void **)(base+GROUP_VT+0x18),base+GROUP_DRAW,draw_entry)) goto reject;
    return TRUE;
reject:
    (void)SudekiMpLanStoryAvatarNativeHudUninstall(); SetLastError(ERROR_INVALID_DATA); return FALSE;
}
BOOL SudekiMpLanStoryAvatarNativeHudBind(const SudekiMpLanStoryAvatarNativeHudIdentity *id,
    SudekiMpLanStoryAvatarNativeHudObserve callback,void *context) {
    if(!base || bound || busy || !id || !callback || !id->world || !id->scene_manager ||
        !id->session_generation || !id->epoch || !id->revision || !code_exact() || !hooks_exact()) return FALSE;
    identity=*id; observe=callback; observe_context=context; native_thread=GetCurrentThreadId(); bound=TRUE;
    SudekiMpLanStoryAvatarNativeHudSnapshot s;
    if(!read_model(&s)) { bound=FALSE; memset(&identity,0,sizeof(identity)); observe=NULL; observe_context=NULL; native_thread=0; return FALSE; }
    return TRUE;
}
BOOL SudekiMpLanStoryAvatarNativeHudService(void) {
    if(!bound || busy || native_thread!=GetCurrentThreadId()) return FALSE;
    busy=TRUE; BOOL result=service(); busy=FALSE; return result;
}
BOOL SudekiMpLanStoryAvatarNativeHudActive(void) { return active && bound; }
BOOL SudekiMpLanStoryAvatarNativeHudRetains(void) { return bound || busy || callbacks; }
BOOL SudekiMpLanStoryAvatarNativeHudUnbind(void) {
    if(!bound) return TRUE;
    if(busy || callbacks || native_thread!=GetCurrentThreadId() || !observe ||
        !observe(&identity,SUDEKIMP_AVATAR_NATIVE_HUD_RESTORE,NULL,observe_context) || !current_globals()) return FALSE;
    restoring=TRUE; BOOL okay=TRUE;
    for(unsigned s=0;s<COUNT;++s) {
        RowLease *r=&rows[s]; if(!r->captured || r->restored) continue;
        if(!row_exact(s)) { okay=FALSE; continue; }
        for(unsigned c=0;c<CHILD_COUNT;++c) if(word(r->gizmo,child_offsets[c]+0x1c)!=r->owned_request[c]) okay=FALSE;
        if(!okay || ptr(r->gizmo+0x2c,0x3c)) { okay=FALSE; continue; }
        if(r->portrait_changed) {
            unsigned original=r->original_portrait;
            unsigned resource=word(base,0x2c2a94+4*original);
            select_resource(r->gizmo+0x2c,resource);
            if(!row_exact(s) || ptr(r->gizmo+0x2c,0x3c)) { okay=FALSE; continue; }
            r->portrait_changed=FALSE;
        }
        *(uint32_t *)(r->gizmo+0x2ac)=r->original_status;
        for(unsigned c=0;c<CHILD_COUNT;++c) if(!request(s,c,r->original_request[c])) { okay=FALSE; break; }
        if(okay) r->restored=TRUE;
    }
    restoring=FALSE;
    if(!okay) return fail(6,"restore_owner_or_state");
    memset(rows,0,sizeof(rows)); memset(&identity,0,sizeof(identity)); layer=NULL;
    bound=active=FALSE; observe=NULL; observe_context=NULL; native_thread=0; reports=0; return TRUE;
}
BOOL SudekiMpLanStoryAvatarNativeHudNativeExitReturned(void) {
    if(!bound) return TRUE;
    if(busy || callbacks || native_thread!=GetCurrentThreadId() || !observe ||
        !observe(&identity,SUDEKIMP_AVATAR_NATIVE_HUD_DESTROYED,NULL,observe_context) ||
        !memory(base+HUD,4,FALSE) || (layer && ptr(base,HUD)!=NULL)) return FALSE;
    /* A57F0 is the supported image's sole NULL writer for this singleton;
     * its synchronous teardown deletes the four gizmos. The caller must
     * additionally witness native Quit returning, so the NULL store inside
     * an in-progress destructor cannot release our callback dependencies. */
    memset(rows,0,sizeof(rows)); memset(&identity,0,sizeof(identity)); layer=NULL;
    bound=active=FALSE; observe=NULL; observe_context=NULL; native_thread=0; reports=0; return TRUE;
}
BOOL SudekiMpLanStoryAvatarNativeHudUninstall(void) {
    if(SudekiMpLanStoryAvatarNativeHudRetains()) return FALSE;
    BOOL okay=SudekiMpRestorePointerHook(&draw_hook);
    if(!SudekiMpRestorePointerHook(&update_hook)) okay=FALSE;
    for(unsigned n=8;n>0;--n) if(!SudekiMpRestoreRelativeCallHook(&call_hooks[n-1])) okay=FALSE;
    if(!okay) return FALSE;
    /* Native entry addresses remain valid for a dispatch that fetched our
     * replacement immediately before restoration. Retain them unchanged. */
    base=NULL; return TRUE;
}
