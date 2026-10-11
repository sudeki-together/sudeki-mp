#include "hooks/lan_story_research54.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#if !defined(__GNUC__) || !defined(__i386__)
#error "Research54 requires the supported x86 ABI"
#endif

/* Hook sites on the supported image. Every stolen range is whole
 * instructions with no relative branch and no branch target inside it
 * (Ghidra reference scan), so the trampolines replay them verbatim. */
enum { H_MOVE, H_CONTROLLER, H_FORWARD, H_TARGETER, H_COMBAT_CONTROLLER, H_COMBAT_AI,
       H_TURN_PLAYER, H_TURN_AI_MOVING, H_TURN_AI_STANDING, H_AI_SET_SPEED,
       H_LOCK_ON, H_LOCK_OFF, H_LOCK_AUTO, H_AI_MOVE_GATE, H_AI_SNAP_GATE, H_COUNT };
/* Call sites from the S1 controller research (all E8 rel32, target verified
 * by the installer): 0x4B73D0 turn-step branches (EDI = turn component
 * actor+0x8C), AI movement node SetSpeed (ECX = arbiter), controller lock
 * calls 0x4DC610 (ESI = arbiter) / 0x4DC720 (EDI = arbiter). */
static const struct { uint32_t site,target; } call_sites[9]={
    {0x2891fu,0xdb0e0u},{0xda816u,0xdb0e0u},
    {0xb75acu,0xb7b90u},{0xb749fu,0xb7c50u},{0xb74ddu,0xb7cc0u},{0xf4de3u,0xdb070u},
    {0x288b1u,0xdc610u},{0x288c1u,0xdc720u},{0x28510u,0xdc610u}};
enum {
    RVA_MOVE=0xdae80u, RVA_CONTROLLER=0x286c0u, RVA_FORWARD_MID=0x1114dfu, RVA_TARGETER_MID=0xb6e5eu,
    RVA_COMBAT=0xdb0e0u, RVA_COMBAT_CALL_CONTROLLER=0x2891fu, RVA_COMBAT_CALL_AI=0xda816u,
    RVA_GROUP=0x408d94u, RVA_CONTROLLER_GLOBAL=0x408da4u, RVA_CLUSTER=0x409d78u,
    RVA_CLUSTER_VTABLE=0x2c7b30u, RVA_ARBITER_VTABLE=0x2cc9acu, RVA_FRAME_STAMP=0x3c3150u,
    LINE_BUDGET=40000u, WINDOW_MS=2000u, CALLERS=48u, COMPONENTS=0x34u
};
/* 0x4DAE80 entry is checked (14 bytes) by control_separation's
 * party_native_entries_exact before every story lease, so the movement
 * request is observed at its three native call sites instead: controller
 * lock-shape 0x428E3F, controller normal 0x428E5E, AI movement node 0x4F4D7F.
 * The mod's own story call reports through SudekiMpLanStoryResearch54Move. */
static const uint32_t move_sites[3]={0x28e3fu,0x28e5eu,0xf4d7fu};
static SudekiMpRelativeCallHook move_hooks[3];
/* PUSH EBP; MOV EBP,[ESP+8] */
static const uint8_t controller_bytes[5]={0x55,0x8b,0x6c,0x24,0x08};
/* FST [ESP+0x18]; FLD1 (after the 12 bytes the world/replica adapters check) */
static const uint8_t forward_bytes[6]={0xd9,0x54,0x24,0x18,0xd9,0xe8};
/* FLD [EAX]; FSTP [ESP+8] (after the 8 bytes control_separation checks) */
static const uint8_t targeter_bytes[6]={0xd9,0x00,0xd9,0x5c,0x24,0x08};
/* Party actor vtables by character index (lan_story_control.c). */
static const uint32_t actor_vt[4]={0x2d5a88u,0x2d66fcu,0x2d5010u,0x2d555cu};
static const char *const actor_name[4]={"Buki","Elco","Tal","Ailish"};

static SudekiMpInlineHook inline_hooks[3];
static SudekiMpRelativeCallHook call_hooks[9];
void *SudekiMpR54Next[H_COUNT] __attribute__((used));
static uint8_t *base;
static uintptr_t dll_base,dll_end;
static volatile LONG installed,lock,lines;
static BOOL side_client;

typedef struct Tracked {
    uintptr_t actor,arbiter,position,targeter,ai;
    uintptr_t component[COMPONENTS]; /* actor+0x40 .. actor+0x10C */
    unsigned character;
} Tracked;
static Tracked tracked[4];
static volatile LONG tracked_count;
typedef struct ActorState {
    float cmd_x,cmd_z,cmd_speed,cmd_turn; uint32_t cmd_mode; uintptr_t cmd_caller; DWORD cmd_at;
    float move_x,move_z,move_speed; uint32_t move_mode; uintptr_t move_caller; DWORD move_logged;
    float face_x,face_z; uintptr_t face_caller; uint32_t face_flag; DWORD face_logged;
    int32_t combat[2][6]; BOOL combat_seen[2]; unsigned turn_path; DWORD turn_path_logged;
    DWORD target_logged;
    uintptr_t target,ai_target; uint32_t arb50,arb58; int ovr; float hp; BOOL state_seen;
} ActorState;
static ActorState states[4];
typedef struct Caller { uint8_t hook,character; uintptr_t caller; uint32_t calls; } Caller;
static Caller callers[CALLERS]; static unsigned caller_count; static LONG caller_overflow;
static DWORD window_at,last_party_at,last_cluster_at;
static int32_t controller_last[7]; static BOOL controller_seen;
static uintptr_t lead_slot0,lead_controller; static uint32_t lead_pending; static BOOL lead_seen;

static BOOL budget(void) { return InterlockedIncrement(&lines)<=(LONG)LINE_BUDGET; }
static const char *side(void) { return side_client?"client":"host"; }
static BOOL rd(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && a<=UINTPTR_MAX-n && VirtualQuery(p,&m,sizeof(m))==sizeof(m) && m.State==MEM_COMMIT &&
        !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) && a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static float fbits(uint32_t v) { float f; memcpy(&f,&v,sizeof(f)); return f; }
static void tag(uintptr_t caller,char out[20]) {
    if(base && caller>=(uintptr_t)base && caller<(uintptr_t)base+SUDEKIMP_EXPECTED_IMAGE_SIZE)
        snprintf(out,20,"g:%06lx",(unsigned long)(caller-(uintptr_t)base));
    else if(caller>=dll_base && caller<dll_end) snprintf(out,20,"dll:%06lx",(unsigned long)(caller-dll_base));
    else snprintf(out,20,"%08lx",(unsigned long)caller);
}
static float angle_deg(float ax,float az,float bx,float bz) {
    float la=sqrtf(ax*ax+az*az),lb=sqrtf(bx*bx+bz*bz);
    if(la<0.0001f || lb<0.0001f) return 0.0f;
    float c=(ax*bx+az*bz)/(la*lb); if(c>1.0f) c=1.0f; if(c<-1.0f) c=-1.0f;
    return acosf(c)*57.29578f;
}
static int find_by(unsigned field,uintptr_t value) {
    LONG count=tracked_count;
    if(!value) return -1;
    for(LONG i=0;i<count && i<4;++i) {
        uintptr_t v=field==0?tracked[i].actor:field==1?tracked[i].arbiter:field==2?tracked[i].position:tracked[i].targeter;
        if(v==value) return (int)i;
    }
    return -1;
}
static void count_call(unsigned hook,unsigned character,uintptr_t caller) {
    unsigned i;
    for(i=0;i<caller_count;++i)
        if(callers[i].hook==hook && callers[i].character==character && callers[i].caller==caller) break;
    if(i==caller_count) {
        if(caller_count>=CALLERS) { ++caller_overflow; return; }
        callers[i].hook=(uint8_t)hook; callers[i].character=(uint8_t)character; callers[i].caller=caller;
        callers[i].calls=0; ++caller_count;
    }
    ++callers[i].calls;
}
void SudekiMpLanStoryResearch54Name(const void *pointer,char out[16]) {
    int i=find_by(0,(uintptr_t)pointer);
    if(!pointer) snprintf(out,16,"none");
    else if(i>=0) snprintf(out,16,"%s",actor_name[tracked[i].character]);
    else snprintf(out,16,"%08lx",(unsigned long)(uintptr_t)pointer);
}

/* Frame f (uint32_t*): f[0]=EDI f[1]=ESI f[2]=EBP f[3]=ESP f[4]=EBX f[5]=EDX
 * f[6]=ECX f[7]=EAX f[8]=EFLAGS f[9..]=stack at the hook site. Runs on the
 * native caller's thread; reads only memory the native code itself is about
 * to read at that site; never calls native code. */
static void observe_move(const uint32_t *f) {
    int i=find_by(1,f[10]); if(i<0) return;
    const float *dir=(const float *)(uintptr_t)f[11];
    if(!dir) return;
    ActorState *s=&states[i]; DWORD now=GetTickCount();
    float x=dir[0],z=dir[2],speed=fbits(f[12]),turn=fbits(f[13]); uint32_t mode=f[14]; uintptr_t ra=f[9];
    count_call(H_MOVE,tracked[i].character,ra);
    s->cmd_x=x; s->cmd_z=z; s->cmd_speed=speed; s->cmd_turn=turn; s->cmd_mode=mode; s->cmd_caller=ra; s->cmd_at=now;
    if(angle_deg(x,z,s->move_x,s->move_z)>15.0f || fabsf(speed-s->move_speed)>0.2f || mode!=s->move_mode ||
        ra!=s->move_caller || now-s->move_logged>=1000u) {
        s->move_x=x; s->move_z=z; s->move_speed=speed; s->move_mode=mode; s->move_caller=ra; s->move_logged=now;
        if(budget()) { char c[20]; tag(ra,c);
            SudekiMpLogFormat("r54 event=move side=%s char=%s caller=%s dir=%.2f,%.2f,%.2f speed=%.2f turn=%.2f mode=%lu ms=%lu\r\n",
                side(),actor_name[tracked[i].character],c,(double)x,(double)dir[1],(double)z,(double)speed,(double)turn,
                (unsigned long)mode,(unsigned long)now); }
    }
}
static void observe_controller(const uint32_t *f) {
    const uint8_t *ctrl=(const uint8_t *)(uintptr_t)f[10];
    if(!ctrl) return;
    int32_t v[7]; static const unsigned off[6]={0x8cu,0x94u,0x9cu,0xa4u,0xacu,0xb4u};
    v[0]=*(const int32_t *)(ctrl+0x248u);
    for(unsigned k=0;k<6u;++k) v[k+1]=*(const int32_t *)(ctrl+off[k]);
    if(controller_seen && !memcmp(v,controller_last,sizeof(v))) return;
    controller_seen=TRUE; memcpy(controller_last,v,sizeof(v));
    if(budget()) { char lead[16]; SudekiMpLanStoryResearch54Name((const void *)(uintptr_t)(uint32_t)v[0],lead);
        SudekiMpLogFormat("r54 event=controller side=%s lead=%s weak=%ld strong=%ld sweep=%ld block=%ld next=%ld prev=%ld ms=%lu\r\n",
            side(),lead,(long)v[1],(long)v[2],(long)v[3],(long)v[4],(long)v[5],(long)v[6],(unsigned long)GetTickCount()); }
}
static void observe_forward(const uint32_t *f) {
    int i=find_by(2,f[1]); if(i<0) return;
    const float *dir=(const float *)(uintptr_t)f[6];
    const uint8_t *pos=(const uint8_t *)(uintptr_t)f[1];
    const uint32_t *frame=(const uint32_t *)(uintptr_t)f[2];
    if(!dir || !frame) return;
    uintptr_t ra=frame[1];
    count_call(H_FORWARD,tracked[i].character,ra);
    /* Current facing: basis row +0x50 (the vector the native controller hands
     * to 0x4B6E50 at 0x428016). */
    const float *cur=(const float *)(pos+0x50u);
    float deg=angle_deg(cur[0],cur[2],dir[0],dir[2]);
    if(deg<25.0f || !budget()) return;
    ActorState *s=&states[i]; DWORD now=GetTickCount(); char c[20],cc[20]; tag(ra,c); tag(s->cmd_caller,cc);
    SudekiMpLogFormat("r54 event=turn side=%s char=%s caller=%s from=%.2f,%.2f to=%.2f,%.2f deg=%.0f cmd=%.2f,%.2f cmd_speed=%.2f cmd_caller=%s cmd_age=%ld ms=%lu\r\n",
        side(),actor_name[tracked[i].character],c,(double)cur[0],(double)cur[2],(double)dir[0],(double)dir[2],(double)deg,
        (double)s->cmd_x,(double)s->cmd_z,(double)s->cmd_speed,cc,s->cmd_at?(long)(now-s->cmd_at):-1l,(unsigned long)now);
}
static void observe_targeter(const uint32_t *f) {
    uintptr_t edi=f[0]; LONG count=tracked_count; int i=-1; unsigned k=COMPONENTS;
    if(!edi) return;
    for(LONG a=0;a<count && a<4 && i<0;++a)
        for(unsigned c=0;c<COMPONENTS;++c) if(tracked[a].component[c]==edi) { i=(int)a; k=c; break; }
    if(i<0) return;
    const float *dir=(const float *)(uintptr_t)f[7];
    uintptr_t ra=f[17]; uint32_t flag=f[18];
    if(!dir) return;
    count_call(H_TARGETER,tracked[i].character,ra);
    ActorState *s=&states[i]; DWORD now=GetTickCount();
    if(angle_deg(dir[0],dir[2],s->face_x,s->face_z)<15.0f && ra==s->face_caller && flag==s->face_flag &&
        now-s->face_logged<1000u) return;
    s->face_x=dir[0]; s->face_z=dir[2]; s->face_caller=ra; s->face_flag=flag; s->face_logged=now;
    if(budget()) { char c[20]; tag(ra,c);
        SudekiMpLogFormat("r54 event=face side=%s char=%s caller=%s component=+%02x dir=%.2f,%.2f flag=%lu b4a=%u ms=%lu\r\n",
            side(),actor_name[tracked[i].character],c,0x40u+k*4u,(double)dir[0],(double)dir[2],(unsigned long)flag,
            (unsigned)((const uint8_t *)edi)[0x4au],(unsigned long)now); }
}
static void observe_combat(const uint32_t *f,unsigned site) {
    int i=find_by(1,f[6]); if(i<0) return;
    int32_t v[6]={(int32_t)f[10],(int32_t)f[11],(int32_t)f[12],(int32_t)f[7],(int32_t)f[13],(int32_t)f[14]};
    ActorState *s=&states[i];
    count_call(site==0?H_COMBAT_CONTROLLER:H_COMBAT_AI,tracked[i].character,f[9]);
    if(s->combat_seen[site] && !memcmp(v,s->combat[site],sizeof(v))) return;
    s->combat_seen[site]=TRUE; memcpy(s->combat[site],v,sizeof(v));
    if(budget()) SudekiMpLogFormat("r54 event=combat_input side=%s char=%s site=%s weak=%ld strong=%ld sweep=%ld block=%ld next=%ld prev=%ld ms=%lu\r\n",
        side(),actor_name[tracked[i].character],site==0?"controller_0x42891f":"ai_0x4da816",
        (long)v[0],(long)v[1],(long)v[2],(long)v[3],(long)v[4],(long)v[5],(unsigned long)GetTickCount());
}
static void observe_turn_path(const uint32_t *f,unsigned id) {
    LONG count=tracked_count; int i=-1; const unsigned k=(0x8cu-0x40u)/4u;
    for(LONG a=0;a<count && a<4;++a) if(f[0] && tracked[a].component[k]==f[0]) { i=(int)a; break; }
    if(i<0) return;
    count_call(id,tracked[i].character,f[9]);
    ActorState *s=&states[i]; DWORD now=GetTickCount();
    if(s->turn_path==id && now-s->turn_path_logged<1000u) return;
    s->turn_path=id; s->turn_path_logged=now;
    const uint8_t *turn=(const uint8_t *)(uintptr_t)f[0],*arb=(const uint8_t *)tracked[i].arbiter;
    const float *want=(const float *)(turn+0x3cu);
    const uint8_t *ai=(const uint8_t *)tracked[i].ai;
    const uint8_t *mode=rd(ai,0x16cu)?*(const uint8_t *const *)(ai+0x3cu):NULL;
    static const char *const branch[3]={"player","ai_moving","ai_standing"};
    if(budget()) SudekiMpLogFormat("r54 event=turn_path side=%s char=%s branch=%s desired=%.2f,%.2f t38=%08lx t48=%08lx mode4a=%u arb50=%08lx latch20000=%u ai_override=%d ai_mode=%d ms=%lu\r\n",
        side(),actor_name[tracked[i].character],branch[id-H_TURN_PLAYER],(double)want[0],(double)want[2],
        (unsigned long)*(const uint32_t *)(turn+0x38u),(unsigned long)*(const uint32_t *)(turn+0x48u),(unsigned)turn[0x4au],
        (unsigned long)*(const uint32_t *)(arb+0x50u),(*(const uint32_t *)(arb+0x50u)&0x20000u)?1u:0u,
        rd(ai,0x16cu)?(int)*(const int16_t *)(ai+0x16au):-1,
        rd(mode,0xcu)?(int)mode[0xbu]:-1,(unsigned long)now);
}
static void observe_ai_set_speed(const uint32_t *f) {
    int i=find_by(1,f[6]); if(i<0) return;
    count_call(H_AI_SET_SPEED,tracked[i].character,f[9]);
}
static void observe_lock(const uint32_t *f,unsigned id) {
    uintptr_t arb=id==H_LOCK_OFF?f[0]:f[1]; int i=find_by(1,arb);
    static DWORD last[3]; DWORD now=GetTickCount(); unsigned k=id-H_LOCK_ON;
    if(id==H_LOCK_AUTO && now-last[k]<1000u) return;
    last[k]=now;
    if(budget()) SudekiMpLogFormat("r54 event=lock side=%s site=%s char=%s arb50=%08lx ms=%lu\r\n",side(),
        id==H_LOCK_ON?"on_0x4288b1":id==H_LOCK_OFF?"off_0x4288c1":"auto_0x428510",
        i>=0?actor_name[tracked[i].character]:"untracked",
        i>=0?(unsigned long)*(const uint32_t *)(tracked[i].arbiter+0x50u):0ul,(unsigned long)now);
}
static void __attribute__((used,noinline,noclone)) r54_observe(const uint32_t *f,unsigned id) {
    DWORD saved=GetLastError();
    if(installed && InterlockedCompareExchange(&lock,1,0)==0) {
        switch(id) {
        case H_MOVE: observe_move(f); break;
        case H_CONTROLLER: observe_controller(f); break;
        case H_FORWARD: observe_forward(f); break;
        case H_TARGETER: observe_targeter(f); break;
        case H_COMBAT_CONTROLLER: observe_combat(f,0u); break;
        case H_COMBAT_AI: observe_combat(f,1u); break;
        case H_TURN_PLAYER: case H_TURN_AI_MOVING: case H_TURN_AI_STANDING: observe_turn_path(f,id); break;
        case H_AI_SET_SPEED: observe_ai_set_speed(f); break;
        case H_LOCK_ON: case H_LOCK_OFF: case H_LOCK_AUTO: observe_lock(f,id); break;
        default: break;
        }
        InterlockedExchange(&lock,0);
    }
    SetLastError(saved);
}
/* Preserves every register, EFLAGS and the FP/SSE state, then continues at
 * the trampoline (inline) or the original callee (call site). */
#define R54_STUB(name,id,offset) \
static void __attribute__((naked,noinline)) name(void) { \
    __asm__ volatile("pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;" \
        "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld; sub $16,%esp;" \
        "mov %ebp,(%esp); movl $" #id ",4(%esp); call _r54_observe;" \
        "add $16,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl;" \
        "jmp *(_SudekiMpR54Next+" #offset ")"); }
R54_STUB(stub_move,0,0)
R54_STUB(stub_controller,1,4)
R54_STUB(stub_forward,2,8)
R54_STUB(stub_targeter,3,12)
R54_STUB(stub_combat_controller,4,16)
R54_STUB(stub_combat_ai,5,20)
R54_STUB(stub_turn_player,6,24)
R54_STUB(stub_turn_ai_moving,7,28)
R54_STUB(stub_turn_ai_standing,8,32)
R54_STUB(stub_ai_set_speed,9,36)
R54_STUB(stub_lock_on,10,40)
R54_STUB(stub_lock_off,11,44)
R54_STUB(stub_lock_auto,12,48)
static void (*const call_stubs[9])(void)={stub_combat_controller,stub_combat_ai,stub_turn_player,
    stub_turn_ai_moving,stub_turn_ai_standing,stub_ai_set_speed,stub_lock_on,stub_lock_off,stub_lock_auto};

static void refresh_party(void) {
    uint8_t *group=rd(base+RVA_GROUP,4u)?*(uint8_t **)(base+RVA_GROUP):NULL;
    Tracked next[4]; LONG n=0;
    if(rd(group,0xd0u)) {
        unsigned count=*(uint32_t *)(group+0xccu);
        for(unsigned s=0;s<count && s<4u;++s) {
            uint8_t *actor=*(uint8_t **)(group+0x90u+s*0xcu); unsigned c;
            if(!rd(actor,0x110u)) continue;
            for(c=0;c<4u && *(void **)actor!=base+actor_vt[c];++c);
            if(c==4u) continue;
            uint8_t *arbiter=*(uint8_t **)(actor+0x90u);
            if(!rd(arbiter,0x64u) || *(void **)arbiter!=base+RVA_ARBITER_VTABLE) continue;
            Tracked *t=&next[n++]; memset(t,0,sizeof(*t));
            t->actor=(uintptr_t)actor; t->arbiter=(uintptr_t)arbiter; t->character=c;
            /* CTargeter (vtable 0x6D4B24) is the +0xAC component; its +0x54 is the
             * current-target TPtr (S3). +0x8C (vtable 0x6D48D4) is what the
             * controller hands to 0x4B6E50; the face hook matches any component. */
            t->position=*(uintptr_t *)(actor+0x44u); t->targeter=*(uintptr_t *)(actor+0xacu);
            t->ai=*(uintptr_t *)(actor+0x94u);
            for(unsigned k=0;k<COMPONENTS;++k) t->component[k]=*(uintptr_t *)(actor+0x40u+k*4u);
        }
    }
    /* Hooks read the table without blocking; take their lock for the copy. */
    if(InterlockedCompareExchange(&lock,1,0)==0) {
        for(LONG i=0;i<n;++i) if(tracked[i].actor!=next[i].actor) memset(&states[i],0,sizeof(states[i]));
        memcpy(tracked,next,sizeof(next)); tracked_count=n;
        InterlockedExchange(&lock,0);
    }
    if(rd(group,0xd0u)) {
        uint8_t *controller=rd(base+RVA_CONTROLLER_GLOBAL,4u)?*(uint8_t **)(base+RVA_CONTROLLER_GLOBAL):NULL;
        uintptr_t slot0=*(uintptr_t *)(group+0x90u),ctrl=rd(controller,0x24cu)?*(uintptr_t *)(controller+0x248u):0;
        uint32_t pending=*(uint32_t *)(group+0xc0u);
        if(!lead_seen || slot0!=lead_slot0 || ctrl!=lead_controller || pending!=lead_pending) {
            lead_seen=TRUE; lead_slot0=slot0; lead_controller=ctrl; lead_pending=pending;
            char a[16],b[16]; SudekiMpLanStoryResearch54Name((void *)slot0,a); SudekiMpLanStoryResearch54Name((void *)ctrl,b);
            if(budget()) SudekiMpLogFormat("r54 event=lead side=%s slot0=%s controller=%s pending_c0=%08lx count=%lu ms=%lu\r\n",
                side(),a,b,(unsigned long)pending,(unsigned long)*(uint32_t *)(group+0xccu),(unsigned long)GetTickCount());
        }
    }
}
static float actor_hp(const uint8_t *actor) {
    const uint8_t *stats=*(const uint8_t *const *)(actor+0x4cu);
    if(!rd(stats,0x30u) || *(const void *const *)(stats+0x10u)!=actor) return -1.0f;
    float hp=*(const float *)(stats+0x2cu);
    return isfinite(hp)?hp:-1.0f;
}
static void sample_party(DWORD now) {
    BOOL periodic=now-last_party_at>=250u;
    if(periodic) last_party_at=now;
    for(LONG i=0;i<tracked_count && i<4;++i) {
        const uint8_t *actor=(const uint8_t *)tracked[i].actor,*arb=(const uint8_t *)tracked[i].arbiter;
        const uint8_t *tg=(const uint8_t *)tracked[i].targeter,*ai=(const uint8_t *)tracked[i].ai;
        const uint8_t *pos=(const uint8_t *)tracked[i].position;
        if(!rd(actor,0x110u) || !rd(arb,0x64u)) continue;
        uintptr_t target=rd(tg,0x58u)?*(const uintptr_t *)(tg+0x54u):0;
        uint32_t a50=*(const uint32_t *)(arb+0x50u),a58=*(const uint32_t *)(arb+0x58u);
        int ovr=rd(ai,0x16cu)?(int)*(const int16_t *)(ai+0x16au):-1;
        uintptr_t ai_target=rd(ai,0xc4u)?*(const uintptr_t *)(ai+0xc0u):0;
        float hp=actor_hp(actor);
        ActorState *s=&states[i]; const char *name=actor_name[tracked[i].character];
        if(!s->state_seen || target!=s->target || ai_target!=s->ai_target || (a50&0x400002u)!=(s->arb50&0x400002u) ||
            (a58&0xfu)!=(s->arb58&0xfu) || ovr!=s->ovr || (hp<=0.0f)!=(s->hp<=0.0f)) {
            char t[16],at[16]; SudekiMpLanStoryResearch54Name((void *)target,t); SudekiMpLanStoryResearch54Name((void *)ai_target,at);
            if(budget()) SudekiMpLogFormat("r54 event=party_state side=%s char=%s target=%s ai_target=%s arb50=%08lx arb58=%08lx ai_override=%d hp=%.1f ms=%lu\r\n",
                side(),name,t,at,(unsigned long)a50,(unsigned long)a58,ovr,(double)hp,(unsigned long)now);
        }
        s->state_seen=TRUE; s->target=target; s->ai_target=ai_target; s->arb50=a50; s->arb58=a58; s->ovr=ovr; s->hp=hp;
        if(periodic && rd(pos,0x88u) && budget()) {
            const float *p=(const float *)(pos+0x18u),*fw=(const float *)(pos+0x50u);
            char t[16]; SudekiMpLanStoryResearch54Name((void *)target,t);
            SudekiMpLogFormat("r54 event=party side=%s char=%s pos=%.2f,%.2f,%.2f fwd=%.2f,%.2f arb50=%08lx arb58=%08lx target=%s pause=%u ms=%lu\r\n",
                side(),name,(double)p[0],(double)p[1],(double)p[2],(double)fw[0],(double)fw[2],(unsigned long)a50,
                (unsigned long)a58,t,(unsigned)actor[0x2bu],(unsigned long)now);
        }
    }
}
static void sample_cluster(DWORD now) {
    if(now-last_cluster_at<1000u) return;
    last_cluster_at=now;
    uint8_t *cm=rd(base+RVA_CLUSTER,4u)?*(uint8_t **)(base+RVA_CLUSTER):NULL;
    if(!rd(cm,0x40u) || *(void **)cm!=base+RVA_CLUSTER_VTABLE) {
        if(budget()) SudekiMpLogFormat("r54 event=cluster side=%s manager=unknown ms=%lu\r\n",side(),(unsigned long)now);
        return;
    }
    const uint8_t *cluster=*(const uint8_t *const *)(cm+0x24u);
    uint32_t hash=rd(cluster,0x10u)?*(const uint32_t *)(cluster+0xcu):0;
    /* Leader region membership sector: *(u16*)(*(slot0+0x74)+0x40) (0x4352D6). */
    uint8_t *group=rd(base+RVA_GROUP,4u)?*(uint8_t **)(base+RVA_GROUP):NULL;
    const uint8_t *leader=rd(group,0x94u)?*(const uint8_t *const *)(group+0x90u):NULL;
    const uint8_t *member=rd(leader,0x78u)?*(const uint8_t *const *)(leader+0x74u):NULL;
    long sector=rd(member,0x42u)?(long)*(const uint16_t *)(member+0x40u):-1l;
    char raw[3*0x14+1]; for(unsigned k=0;k<0x14u;++k) snprintf(raw+3*k,4,"%02x%s",cm[0x24u+k],k+1<0x14u?".":"");
    long stamp=rd(base+RVA_FRAME_STAMP,2u)?(long)*(const uint16_t *)(base+RVA_FRAME_STAMP):-1l;
    if(budget()) SudekiMpLogFormat("r54 event=cluster side=%s hash=%08lx cm2c=%04x cm2e=%04x cm30=%04x in_combat=%u cm36=%u leader_sector=%04lx raw24=%s frame_stamp=%ld ms=%lu\r\n",
        side(),(unsigned long)hash,(unsigned)*(uint16_t *)(cm+0x2cu),(unsigned)*(uint16_t *)(cm+0x2eu),
        (unsigned)*(uint16_t *)(cm+0x30u),(unsigned)cm[0x34u],(unsigned)cm[0x36u],sector,raw,stamp,(unsigned long)now);
}
static void summarise(DWORD now) {
    if(now-window_at<WINDOW_MS) return;
    Caller copy[CALLERS]; unsigned n=0; LONG overflow=0;
    if(InterlockedCompareExchange(&lock,1,0)!=0) return;
    n=caller_count; memcpy(copy,callers,n*sizeof(copy[0])); overflow=caller_overflow;
    caller_count=0; caller_overflow=0;
    InterlockedExchange(&lock,0);
    static const char *const hook_name[H_COUNT]={"move","controller","forward","face","combat_controller","combat_ai",
        "turn_player","turn_ai_moving","turn_ai_standing","ai_set_speed","lock_on","lock_off","lock_auto",
        "ai_move_bypassed","ai_snap_bypassed"};
    for(unsigned i=0;i<n;++i) if(budget()) { char c[20]; tag(copy[i].caller,c);
        SudekiMpLogFormat("r54 event=calls side=%s window_ms=%lu hook=%s char=%s caller=%s calls=%lu\r\n",
            side(),(unsigned long)(now-window_at),hook_name[copy[i].hook],actor_name[copy[i].character&3u],c,
            (unsigned long)copy[i].calls); }
    if(overflow && budget()) SudekiMpLogFormat("r54 event=calls_overflow side=%s dropped=%ld\r\n",side(),(long)overflow);
    window_at=now;
}
/* Read-only effect snapshots on the existing game-thread sampling seam.
 * Keep asset identity, position and animation time together so an orb or
 * lingering impact is not classified as a spawn sigil merely by its class.
 * No cached native pointers, getters, new hooks or native writes. */
static void sample_effects(DWORD now) {
    static DWORD last; static unsigned effect_lines;
    if(now-last<500u || effect_lines>=6400u) return;
    last=now;
    uint8_t *registry=*(uint8_t **)(base+0x409d8cu);
    if(!rd(registry,0x40u)) return;
    unsigned count=*(unsigned *)(registry+0x34u);
    uint8_t *const *entries=*(uint8_t *const **)(registry+0x3cu);
    if(!count || count>8192u || !rd(entries,count*sizeof(*entries))) return;
    for(unsigned i=0;i<count && effect_lines<6400u;++i) {
        uint8_t *e=entries[i];
        if(!rd(e,0x3e4u) || *(void **)e!=base+0x2d3c7cu ||
            *(void **)(e+8u)!=base+0x2d3ca4u ||
            *(void **)(e+0x44u)!=e+0x160u || *(void **)(e+0x58u)!=e+0x270u ||
            *(void **)(e+0x160u)!=base+0x2cdefcu || *(void **)(e+0x170u)!=e ||
            *(void **)(e+0x270u)!=base+0x2c83f4u || *(void **)(e+0x280u)!=e) continue;
        uint8_t *p=e+0x160u,*wrapper=*(uint8_t **)(p+0xb4u);
        if(!rd(wrapper,0x14u)) continue;
        uint8_t *object=*(uint8_t **)(wrapper+8u),*renderer=*(uint8_t **)(wrapper+0x10u);
        if(!rd(object,0xd0u) || !rd(renderer,0xb0u) ||
            *(void **)(object+0x14u)!=renderer || *(void **)renderer!=base+0x2df8ecu) continue;
        unsigned channels=*(unsigned *)(renderer+0xa0u);
        uint8_t *rows=*(uint8_t **)(renderer+0x98u),*row=NULL;
        if(channels && channels<=8u && rd(rows,channels*36u)) row=*(uint8_t **)rows;
        BOOL clock_known=rd(row,0x18u);
        uint32_t resource_kind=*(uint32_t *)(e+0x29cu),resource_id=*(uint32_t *)(e+0x2a0u);
        char name[64]="unknown";
        if((resource_kind&0x7fu)==0x29u) {
            uint8_t *reference=*(uint8_t **)(e+0x2a4u);
            if(rd(reference,8u)) {
                const char *text=*(const char **)(reference+4u);
                if(rd(text,sizeof(name)) && memchr(text,0,sizeof(name))) {
                    memcpy(name,text,sizeof(name));
                    for(unsigned c=0;name[c];++c)
                        if((unsigned char)name[c]<32u || (unsigned char)name[c]>126u) name[c]='?';
                }
            }
        }
        if(!budget()) return;
        ++effect_lines;
        SudekiMpLogFormat("r54 event=effect side=%s ms=%lu entity=%p asset=%08lx kind=%04lx name=%s pause=%u active=%02x flags=%08lx stamp=%u frame=%u clock_known=%u clip_state=%08lx time=%.3f rate=%.3f local=(%.3f,%.3f,%.3f) world=(%.3f,%.3f,%.3f) parent=%p\r\n",
            side(),(unsigned long)now,(void *)e,(unsigned long)resource_id,(unsigned long)resource_kind,name,
            (unsigned)e[0x2bu],(unsigned)e[0x3e0u],(unsigned long)*(uint32_t *)(object+0x34u),
            (unsigned)*(uint16_t *)(object+0x32u),(unsigned)*(uint16_t *)(base+RVA_FRAME_STAMP),
            clock_known?1u:0u,clock_known?(unsigned long)*(uint32_t *)row:0ul,
            clock_known?(double)*(float *)(row+8u):0.0,clock_known?(double)*(float *)(row+4u):0.0,
            (double)*(float *)(p+0x18u),(double)*(float *)(p+0x1cu),(double)*(float *)(p+0x20u),
            (double)*(float *)(object+0xc0u),(double)*(float *)(object+0xc4u),(double)*(float *)(object+0xc8u),
            *(void **)(p+0x94u));
    }
}
void SudekiMpLanStoryResearch54Sample(BOOL client) {
    if(!installed || !base) return;
    DWORD now=GetTickCount();
    side_client=client;
    if(!window_at) window_at=now;
    refresh_party();
    sample_party(now);
    sample_cluster(now);
    sample_effects(now);
    summarise(now);
}

/* Host input receipt gaps per remote player, summarised every 5 s. */
static struct { BOOL seen; uint32_t last_receipt; DWORD window_at; unsigned packets,hist[6],stale,held,max_gap;
    unsigned frames,moving,held_moving;
    BOOL last_moving; } input[4];
void SudekiMpLanStoryResearch54Input(unsigned player,BOOL took,uint32_t receipt,DWORD now,
    BOOL fresh,BOOL packet_moving,BOOL applied_moving,BOOL held) {
    if(!installed || player>=4u) return;
    __typeof__(input[0]) *s=&input[player];
    if(!s->window_at) s->window_at=now;
    if(took) {
        if(s->seen) {
            uint32_t gap=receipt-s->last_receipt;
            unsigned b=gap<=20u?0:gap<=40u?1:gap<=60u?2:gap<=100u?3:gap<=200u?4:5;
            ++s->hist[b]; if(gap>s->max_gap) s->max_gap=gap;
            if(gap>100u && s->last_moving && budget())
                SudekiMpLogFormat("r54 event=input_gap player=%u gap_ms=%lu moving_before=1 moving_after=%u ms=%lu\r\n",
                    player,(unsigned long)gap,packet_moving?1u:0u,(unsigned long)now);
        }
        s->seen=TRUE; s->last_receipt=receipt; ++s->packets; s->last_moving=packet_moving;
    }
    if(!fresh && s->last_moving && !applied_moving) ++s->stale;
    if(held) ++s->held;
    ++s->frames;
    if(fresh && packet_moving) { ++s->moving; if(held) ++s->held_moving; }
    if(now-s->window_at>=5000u) {
        if(budget()) SudekiMpLogFormat("r54 event=input player=%u window_ms=%lu packets=%u gaps_le20=%u le40=%u le60=%u le100=%u le200=%u gt200=%u max_gap=%u stale_frames=%u held_frames=%u frames=%u moving_frames=%u held_moving_frames=%u ms=%lu\r\n",
            player,(unsigned long)(now-s->window_at),s->packets,s->hist[0],s->hist[1],s->hist[2],s->hist[3],s->hist[4],
            s->hist[5],s->max_gap,s->stale,s->held,s->frames,s->moving,s->held_moving,(unsigned long)now);
        s->window_at=now; s->packets=s->stale=s->held=s->max_gap=0; memset(s->hist,0,sizeof(s->hist));
        s->frames=s->moving=s->held_moving=0;
    }
}
void SudekiMpLanStoryResearch54Target(const void *actor,float x,float z,const void *before,BOOL refreshed) {
    if(!installed) return;
    int i=find_by(0,(uintptr_t)actor);
    if(i<0) return;
    const uint8_t *tg=(const uint8_t *)tracked[i].targeter;
    if(!rd(tg,0x85u) || *(const void *const *)(tg+0x10u)!=actor) return;
    void *after=*(void *const *)(tg+0x54u); DWORD now=GetTickCount();
    if(before==after && now-states[i].target_logged<250u) return;
    states[i].target_logged=now;
    if(budget()) SudekiMpLogFormat("r54 event=auto_target side=%s char=%s input=%.2f,%.2f heading=%.2f,%.2f before=%p after=%p flags=%02x refreshed=%u ms=%lu\r\n",
        side(),actor_name[tracked[i].character],(double)x,(double)z,
        (double)*(const float *)(tg+0x48u),(double)*(const float *)(tg+0x50u),
        before,after,(unsigned)tg[0x84u],refreshed?1u:0u,(unsigned long)now);
}
void SudekiMpLanStoryResearch54Melee(const void *actor,unsigned kind) {
    if(!installed) return;
    int i=find_by(0,(uintptr_t)actor);
    const uint8_t *arb=i>=0?(const uint8_t *)tracked[i].arbiter:NULL;
    char t[16]; uintptr_t target=0;
    if(i>=0 && rd((const void *)tracked[i].targeter,0x58u)) target=*(const uintptr_t *)(tracked[i].targeter+0x54u);
    SudekiMpLanStoryResearch54Name((void *)target,t);
    if(budget()) SudekiMpLogFormat("r54 event=combat_input side=%s char=%s site=mod_story_melee weak=%u strong=%u sweep=%u block=0 next=0 prev=0 arb50=%08lx arb58=%08lx target=%s ms=%lu\r\n",
        side(),i>=0?actor_name[tracked[i].character]:"?",kind==1u,kind==2u,kind==3u,
        arb&&rd(arb,0x5cu)?(unsigned long)*(const uint32_t *)(arb+0x50u):0ul,
        arb&&rd(arb,0x5cu)?(unsigned long)*(const uint32_t *)(arb+0x58u):0ul,t,(unsigned long)GetTickCount());
}
/* Monster presentation gaps: a copy not captured (host) or not applied
 * (client) for >= 300 ms, then seen again; logs the gap and the jump. */
static struct { uint32_t identity; DWORD at; float pos[3]; } monster_seen[2][48];
void SudekiMpLanStoryResearch54Monster(unsigned side_index,uint32_t identity,const float position[3]) {
    if(!installed || side_index>1u || !position) return;
    DWORD now=GetTickCount(); unsigned slot=48u,oldest=0u;
    for(unsigned k=0;k<48u;++k) {
        if(monster_seen[side_index][k].identity==identity) { slot=k; break; }
        if(monster_seen[side_index][k].at<monster_seen[side_index][oldest].at) oldest=k;
    }
    if(slot==48u) { slot=oldest; monster_seen[side_index][slot].identity=identity; monster_seen[side_index][slot].at=0u; }
    __typeof__(monster_seen[0][0]) *m=&monster_seen[side_index][slot];
    if(m->at && now-m->at>=300u && budget()) {
        float dx=position[0]-m->pos[0],dz=position[2]-m->pos[2];
        SudekiMpLogFormat("r54 event=monster_gap side=%s identity=%08lx gap_ms=%lu jump=%.2f ms=%lu\r\n",
            side_index?"client":"host",(unsigned long)identity,(unsigned long)(now-m->at),(double)sqrtf(dx*dx+dz*dz),
            (unsigned long)now);
    }
    m->at=now?now:1u; memcpy(m->pos,position,sizeof(m->pos));
}
void SudekiMpLanStoryResearch54Move(const void *arbiter,const float direction[3],float speed,float rate,uint32_t mode) {
    uint32_t f[15]={0};
    if(!installed || !direction) return;
    f[9]=(uint32_t)(uintptr_t)__builtin_return_address(0); f[10]=(uint32_t)(uintptr_t)arbiter;
    f[11]=(uint32_t)(uintptr_t)direction; memcpy(&f[12],&speed,4); memcpy(&f[13],&rate,4); f[14]=mode;
    r54_observe(f,H_MOVE);
}
void SudekiMpLanStoryResearch54AiGate(const void *actor,unsigned gate) {
    DWORD saved=GetLastError();
    if(installed && gate<2u && InterlockedCompareExchange(&lock,1,0)==0) {
        int i=find_by(0,(uintptr_t)actor);
        if(i>=0) count_call(gate?H_AI_SNAP_GATE:H_AI_MOVE_GATE,tracked[i].character,
            (uintptr_t)(base+(gate?0xf50ffu:0xf4cb3u)));
        InterlockedExchange(&lock,0);
    }
    SetLastError(saved);
}
BOOL SudekiMpLanStoryResearch54Enabled(void) { return installed!=0; }

BOOL SudekiMpLanStoryResearch54Uninstall(void) {
    BOOL ok=TRUE;
    InterlockedExchange(&installed,0);
    for(int i=8;i>=0;--i) if(!SudekiMpRestoreRelativeCallHook(&call_hooks[i])) ok=FALSE;
    for(int i=2;i>=0;--i) if(!SudekiMpRestoreInlineHook(&inline_hooks[i])) ok=FALSE;
    for(int i=2;i>=0;--i) if(!SudekiMpRestoreRelativeCallHook(&move_hooks[i])) ok=FALSE;
    /* Teardown only (exit to title / process end): restore frees the trampolines, as for every inline hook here. */
    if(!ok) SudekiMpLogWrite("r54 event=uninstall result=restore_failed policy=retain\r\n");
    return ok;
}
BOOL SudekiMpLanStoryResearch54Install(HMODULE image) {
    uint8_t *b=(uint8_t *)image; HMODULE self=NULL;
    if(installed || base || !b || !SudekiMpCheckLoadedExecutable(image) || !SudekiMpLogResearchEnabled()) {
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCWSTR)(uintptr_t)&SudekiMpLanStoryResearch54Install,&self) && self) {
        const IMAGE_DOS_HEADER *dos=(const IMAGE_DOS_HEADER *)self;
        const IMAGE_NT_HEADERS32 *nt=(const IMAGE_NT_HEADERS32 *)((const uint8_t *)self+dos->e_lfanew);
        dll_base=(uintptr_t)self; dll_end=dll_base+nt->OptionalHeader.SizeOfImage;
    }
    base=b;
    SudekiMpR54Next[H_MOVE]=b+RVA_MOVE;
    for(unsigned i=0;i<3u;++i)
        if(!SudekiMpInstallRelativeCallHook(&move_hooks[i],b+move_sites[i],b+RVA_MOVE,
                (const void *)(uintptr_t)stub_move)) goto fail;
    struct { uint32_t rva; const uint8_t *bytes; size_t length; void (*stub)(void); unsigned id; } inl[3]={
        {RVA_CONTROLLER,controller_bytes,sizeof(controller_bytes),stub_controller,H_CONTROLLER},
        {RVA_FORWARD_MID,forward_bytes,sizeof(forward_bytes),stub_forward,H_FORWARD},
        {RVA_TARGETER_MID,targeter_bytes,sizeof(targeter_bytes),stub_targeter,H_TARGETER}};
    for(unsigned i=0;i<3u;++i) {
        if(!SudekiMpInstallInlineHook(&inline_hooks[i],b+inl[i].rva,inl[i].bytes,inl[i].length,
                (const void *)(uintptr_t)inl[i].stub)) goto fail;
        SudekiMpR54Next[inl[i].id]=inline_hooks[i].trampoline;
    }
    for(unsigned i=0;i<9u;++i) {
        SudekiMpR54Next[H_COMBAT_CONTROLLER+i]=b+call_sites[i].target;
        if(!SudekiMpInstallRelativeCallHook(&call_hooks[i],b+call_sites[i].site,b+call_sites[i].target,
                (const void *)(uintptr_t)call_stubs[i])) goto fail;
    }
    InterlockedExchange(&installed,1);
    SudekiMpLogWrite("r54 event=installed hooks=move_calls_0x428e3f_0x428e5e_0x4f4d7f,controller_0x4286c0,forward_mid_0x5114df,face_mid_0x4b6e5e,combat_calls_0x42891f_0x4da816,turn_steps_0x4b75ac_0x4b749f_0x4b74dd,ai_set_speed_0x4f4de3,lock_0x4288b1_0x4288c1_0x428510 policy=log_only\r\n");
    return TRUE;
fail:;
    DWORD error=GetLastError();
    if(!SudekiMpLanStoryResearch54Uninstall()) { SetLastError(error?error:ERROR_INVALID_DATA); return FALSE; }
    memset(inline_hooks,0,sizeof(inline_hooks)); memset(call_hooks,0,sizeof(call_hooks)); memset(move_hooks,0,sizeof(move_hooks));
    base=NULL;
    SetLastError(error?error:ERROR_INVALID_DATA);
    return FALSE;
}
