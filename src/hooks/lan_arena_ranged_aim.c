#include "hooks/lan_arena_ranged_aim.h"
#include "hooks/call_hook.h"
#include "engine/log.h"
#include <math.h>
#include <string.h>

enum { AIM_CALL = 0xc74e3, AIM_DIRECTION = 0xc7aa0,
    MISSILE_VT = 0x2d4c8c, POSE_CALL=0x222434, POSE_BUILD=0x21e810,
    POSE_SAMPLE=0x21df00, RENDERER_VT=0x2df8ec, ELCO_VT=0x2d66fc,
    MODEL_VT=0x2d5464, BONES=109 };
static uint8_t *aim_image;
static SudekiMpRelativeCallHook direction_hook;
static SudekiMpRelativeCallHook pose_hook;
static void *direction_original __attribute__((used));
static void *pose_original __attribute__((used));
static SudekiMpLanAimWitness aim_witness;
static DWORD aim_thread;
static void *aim_actors[2];
static uint32_t corrected_shots;
static void *world_renderers[2];
/* Retail sampler uses MOVAPS stores for local quaternions. */
static float aimed_pose[BONES*12] __attribute__((aligned(16)));
static float center_pose[BONES*12] __attribute__((aligned(16)));
static float result_pose[BONES*12] __attribute__((aligned(16)));
static unsigned pose_depth;
static uint32_t corrected_poses;

enum { SAMPLE_CHANNELS=2, SAMPLE_STATE_BYTES=24 };
static void sample_states(uint8_t states[SAMPLE_CHANNELS][SAMPLE_STATE_BYTES],
    uint16_t selector,float phase) {
    memset(states,0,SAMPLE_CHANNELS*SAMPLE_STATE_BYTES);
    for(unsigned channel=0;channel<SAMPLE_CHANNELS;++channel) {
        memcpy(states[channel],&selector,sizeof(selector));
        memcpy(states[channel]+12,&phase,sizeof(phase));
    }
}

static BOOL memory(const void *p, size_t n, BOOL write) {
    MEMORY_BASIC_INFORMATION m;
    uintptr_t a = (uintptr_t)p;
    return p && n && a+n >= a && VirtualQuery(p,&m,sizeof(m)) &&
        m.State == MEM_COMMIT && !(m.Protect & (PAGE_NOACCESS|PAGE_GUARD)) &&
        a+n <= (uintptr_t)m.BaseAddress+m.RegionSize &&
        (!write || (m.Protect & (PAGE_READWRITE|PAGE_WRITECOPY|
            PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)));
}
BOOL SudekiMpLanAimNormalize(const float input[3], float output[3]) {
    float norm;
    if (!input || !output) return FALSE;
    for (unsigned i=0;i<3;++i)
        if (!isfinite(input[i]) || fabsf(input[i]) > 1.001f) return FALSE;
    norm = sqrtf(input[0]*input[0]+input[1]*input[1]+input[2]*input[2]);
    if (!isfinite(norm) || norm < 0.5f || norm > 1.5f) return FALSE;
    for (unsigned i=0;i<3;++i) output[i]=input[i]/norm;
    return TRUE;
}
static BOOL bytes(uint8_t *b, unsigned rva, const void *data, size_t n) {
    return memory(b+rva,n,FALSE) && !memcmp(b+rva,data,n);
}
static void multiply(const float a[4],const float b[4],float out[4]) {
    float t[4]={a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],
        a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0],
        a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3],
        a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2]};
    memcpy(out,t,sizeof(t));
}
static BOOL quaternion(const float q[4]) {
    float n=0;
    for(unsigned j=0;j<4;++j) { if(!isfinite(q[j])) return FALSE; n+=q[j]*q[j]; }
    return n>0.98f && n<1.02f;
}
BOOL SudekiMpLanAimOverlay(float *pose,const float *aim,const float *center,
    const uint8_t *mask,unsigned count,float amount) {
    if(!pose || !aim || !center || !mask || !count || count>BONES ||
        !isfinite(amount) || amount<0 || amount>2.5f) return FALSE;
    for(unsigned i=0;i<count;++i) if(mask[i] &&
        (!quaternion(pose+i*12+8) || !quaternion(aim+i*12+8) ||
         !quaternion(center+i*12+8))) return FALSE;
    if(amount==0) return TRUE;
    for(unsigned i=0;i<count;++i) if(mask[i]) {
        float *q=pose+i*12+8;
        const float *c=center+i*12+8;
        float inverse[4]={-c[0],-c[1],-c[2],c[3]},delta[4];
        float length,angle,scale;
        multiply(aim+i*12+8,inverse,delta);
        length=sqrtf(delta[0]*delta[0]+delta[1]*delta[1]+delta[2]*delta[2]+delta[3]*delta[3]);
        for(unsigned j=0;j<4;++j) delta[j]/=length;
        if(delta[3]<0) for(unsigned j=0;j<4;++j) delta[j]=-delta[j];
        angle=acosf(fminf(1.0f,delta[3]));
        scale=angle<0.00001f ? amount : sinf(angle*amount)/sinf(angle);
        for(unsigned j=0;j<3;++j) delta[j]*=scale;
        delta[3]=cosf(angle*amount);
        multiply(delta,q,q);
        length=sqrtf(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);
        for(unsigned j=0;j<4;++j) q[j]/=length;
    }
    return TRUE;
}
static BOOL relative_call(uint8_t *b,unsigned call,unsigned target) {
    int32_t d;
    if(!bytes(b,call,"\xe8",1) || !memory(b+call+1,4,FALSE)) return FALSE;
    memcpy(&d,b+call+1,4); return (int64_t)call+5+d==target;
}
BOOL SudekiMpLanAimImageMatches(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    int32_t displacement;
    if (!b || !bytes(b,AIM_CALL-0xau,
            "\x8d\x44\x24\x58\x50\x8b\xcf\x51\x8b\xc3",10) ||
        !bytes(b,AIM_CALL,"\xe8",1) ||
        !bytes(b,AIM_CALL+5,"\x8b\x43\x58\x8b\x50\x30",6) ||
        !bytes(b,AIM_DIRECTION,"\x55\x8b\xec\x83\xe4\xf0",6) ||
        !bytes(b,0xc7d0d,"\x5f\x5e\x5b\x8b\xe5\x5d\xc2\x08\x00",9))
        return FALSE;
    memcpy(&displacement,b+AIM_CALL+1,4);
    return (int64_t)AIM_CALL+5+displacement==AIM_DIRECTION &&
        relative_call(b,POSE_CALL,POSE_BUILD) &&
        bytes(b,POSE_CALL-6,"\x50\x0f\xb7\x02\x53\x51",6) &&
        bytes(b,POSE_CALL+5,"\x8b\x57\x08\x8b\x42\x1c",6) &&
        bytes(b,0x21e951,"\x5f\x5e\x5b\x83\xc4\x14\xc2\x14\x00",9) &&
        bytes(b,0x21e203,"\xc2\x0c\x00",3);
}
/* Resolve only Elco's original WORLD bank. Never his separate first-person
 * weapon bank. Every use resolves again; cached renderer identity is only a
 * fast rejection filter for other world objects. */
static void *world_renderer(void *actor) {
    uint8_t *a=actor,*model,*position,*table,*details,*wrapper,*renderer,*bank,*header;
    void *wrappers[3];
    typedef int (__attribute__((thiscall)) *Lookup)(void *,uint32_t);
    if(!memory(a,0x138,FALSE) || *(void **)a!=aim_image+ELCO_VT) return NULL;
    model=*(uint8_t **)(a+0x134); position=*(uint8_t **)(a+0x44);
    if(!memory(model,0x168,FALSE) || *(void **)model!=aim_image+MODEL_VT ||
        *(void **)(model+0x10)!=actor || !memory(position,0xb8,FALSE) ||
        *(void **)(position+0x10)!=actor) return NULL;
    table=*(uint8_t **)(model+0xdc);
    if(!memory(table,0x14+0x99*4,FALSE)) return NULL;
    wrappers[0]=*(void **)(position+0xb4); wrappers[1]=*(void **)(model+0x160);
    wrappers[2]=*(void **)(model+0x164);
    for(unsigned i=0;i<3;++i) {
        wrapper=wrappers[i];
        if(!memory(wrapper,0x14,FALSE)) continue;
        renderer=*(uint8_t **)(wrapper+0x10);
        if(!memory(renderer,0xa8,FALSE) || *(void **)renderer!=aim_image+RENDERER_VT) continue;
        bank=*(uint8_t **)(renderer+8);
        if(!memory(bank,0x28,FALSE)) continue;
        header=*(uint8_t **)(bank+0x1c);
        if(!memory(header,16,FALSE) || *(uint32_t *)header!=141 ||
            *(uint32_t *)(header+4)!=BONES) continue;
        if(*(void **)(aim_image+RENDERER_VT+0x40)!=aim_image+0x21bac0) return NULL;
        BOOL matches=TRUE;
        for(unsigned id=0x97;id<=0x98;++id) {
            details=*(uint8_t **)(table+0x14+id*4);
            if(!memory(details,0x28,FALSE)) { matches=FALSE; break; }
            int expected=id==0x97 ? 50:54;
            Lookup lookup=(Lookup)(aim_image+0x21bac0);
            if(lookup(renderer,*(uint32_t *)(details+0x14))!=expected &&
                lookup(renderer,*(uint32_t *)(details+0x20))!=expected) { matches=FALSE; break; }
        }
        if(matches) return renderer;
    }
    return NULL;
}
static float *__attribute__((used,noinline)) select_pose(void **args,void *renderer) {
    float *original=args[2], direction[3], unit[3];
    uint8_t *bank=args[0],*entries,*circle,*straight,*groups,*hierarchy;
    uint32_t *palette;
    uint8_t mask[BONES]={0};
    void *actor=NULL;
    float phase,amount,yaw,pitch,forward_x,forward_z;
    if(GetCurrentThreadId()!=aim_thread || pose_depth || !aim_witness) return original;
    for(unsigned i=0;i<2;++i) if(renderer && renderer==world_renderers[i]) actor=aim_actors[i];
    if(!actor || !aim_witness(actor,FALSE,direction) ||
        !SudekiMpLanAimNormalize(direction,unit) || world_renderer(actor)!=renderer ||
        *(void **)((uint8_t *)renderer+8)!=bank || !memory(original,sizeof(result_pose),FALSE)) return original;
    /* Native hierarchy groups sort the bones; palette maps sorted entries to
     * pose indices. Use that mapping, never assume exporter order at runtime. */
    entries=*(uint8_t **)(bank+0x20); groups=*(uint8_t **)(bank+0x24);
    if(!memory(entries,141*28,FALSE)) return original;
    circle=*(uint8_t **)(entries+50*28); straight=*(uint8_t **)(entries+54*28);
    if(!memory(circle,24,FALSE) || !memory(straight,24,FALSE) ||
        *(uint32_t *)(circle+8)!=*(uint32_t *)(straight+8) ||
        *(uint32_t *)(circle+8)>8 ||
        !memory(groups,(*(uint32_t *)(circle+8)+1)*8,FALSE)) return original;
    groups+=*(uint32_t *)(circle+8)*8;
    hierarchy=*(uint8_t **)groups; palette=*(uint32_t **)(groups+4);
    if(!memory(hierarchy,BONES*8,FALSE) || !memory(palette,BONES*4,FALSE)) return original;
    int upper=-1;
    uint8_t seen[BONES]={0};
    for(unsigned i=0;i<BONES;++i) {
        int parent=*(int16_t *)(hierarchy+i*8+4);
        int channel=*(int16_t *)(hierarchy+i*8+6);
        /* The retail sampler indexes state[bone.channel], not just state[0].
         * Elco's loaded hierarchy uses channels0/1 for lower/upper body.
         * Reject unsupported channels before making any native sampler call. */
        if(palette[i]>=BONES || seen[palette[i]] || parent>=(int)i || parent < -1 ||
            channel<0 || channel>=SAMPLE_CHANNELS) return original;
        seen[palette[i]]=1;
        if(*(uint32_t *)(hierarchy+i*8)==4257395119u) upper=(int)i;
        mask[palette[i]]=(int)i==upper || (parent>=0 && mask[palette[parent]]);
    }
    if(upper<0) return original;
    uint8_t *position=*(uint8_t **)((uint8_t *)actor+0x44);
    if(!memory(position,0x5c,FALSE)) return original;
    forward_x=*(float *)(position+0x50); forward_z=*(float *)(position+0x58);
    if(!isfinite(forward_x) || !isfinite(forward_z) ||
        forward_x*forward_x+forward_z*forward_z<0.5f) return original;
    yaw=atan2f(unit[0]*forward_z-unit[2]*forward_x,unit[0]*forward_x+unit[2]*forward_z);
    pitch=atan2f(unit[1],sqrtf(unit[0]*unit[0]+unit[2]*unit[2]));
    /* Original circle is up/right/down/left at 0/12/24/36, around
     * the authored straight FirePose. Root/legs and current recoil survive. */
    amount=sqrtf(yaw*yaw+pitch*pitch)/(3.14159265358979323846f/4);
    if(amount<0.0001f) return original;
    amount=fminf(amount,2.0f);
    phase=atan2f(yaw,pitch)*(48.0f/(2*3.14159265358979323846f));
    if(phase<0) phase+=48.0f;
    typedef void (__stdcall *Sample)(void *,void *,void *);
    uint8_t states[SAMPLE_CHANNELS][SAMPLE_STATE_BYTES];
    ++pose_depth;
    sample_states(states,50,phase);
    ((Sample)(aim_image+POSE_SAMPLE))(bank,states,aimed_pose);
    sample_states(states,54,0);
    ((Sample)(aim_image+POSE_SAMPLE))(bank,states,center_pose);
    memcpy(result_pose,original,sizeof(result_pose));
    BOOL okay=SudekiMpLanAimOverlay(result_pose,aimed_pose,center_pose,mask,BONES,amount);
    --pose_depth;
    if(!okay) return original;
    if(++corrected_poses<=3)
        SudekiMpLogFormat("lan_ranged_aim event=pose actor=Elco phase=%.3f amount=%.3f policy=authored_additive_upper_body_no_channel_restart\r\n",phase,amount);
    return result_pose;
}
static void __attribute__((naked,noinline)) pose_bridge(void) {
    /* Final local-pose -> model-matrix call: EDI=renderer, EAX=root clip,
     * five stack args, ret20. Replace only the transient input pose pointer.
     * Native matrix construction/weapon attachment still run unmodified. */
    __asm__ volatile("pushfl\n\tpushal\n\tlea 40(%esp),%eax\n\t"
        "pushl %edi\n\tpushl %eax\n\tcall _select_pose\n\taddl $8,%esp\n\t"
        "movl %eax,48(%esp)\n\tpopal\n\tpopfl\n\tjmp *_pose_original\n\t");
}
/* Original direction runs first. This override is before native pellet
 * spread, velocity, collision, damage and reload; none are reimplemented. */
static void __attribute__((used,noinline)) apply_direction(void *manager, float *out) {
    uint8_t *m=manager;
    void *actor;
    float direction[3], normalized[3];
    if (GetCurrentThreadId()!=aim_thread || !aim_witness ||
        !memory(m,0x14,FALSE) || *(void **)m!=aim_image+MISSILE_VT ||
        !memory(out,12,TRUE)) return;
    actor=*(void **)(m+0x10);
    if (!actor || (actor!=aim_actors[0] && actor!=aim_actors[1]) ||
        !memory(actor,0xc0,FALSE) || *(void **)((uint8_t *)actor+0xbc)!=m ||
        !aim_witness(actor,TRUE,direction) ||
        !SudekiMpLanAimNormalize(direction,normalized)) return;
    memcpy(out,normalized,12);
    ++corrected_shots;
    if (corrected_shots<=4 || !(corrected_shots%32))
        SudekiMpLogFormat("lan_ranged_aim event=shot_direction count=%lu xyz=%.5f,%.5f,%.5f policy=actor_scoped_before_native_spread\r\n",
            (unsigned long)corrected_shots,out[0],out[1],out[2]);
}
static void __attribute__((naked,noinline)) direction_bridge(void) {
    /* EAX=CMissileManager, stack=(muzzle*,direction*), native ret8.
     * Preserve the original result registers/flags around the observer. */
    __asm__ volatile(
        "pushl %ebp\n\tmovl %esp,%ebp\n\tpushl %ebx\n\tmovl %eax,%ebx\n\t"
        "pushl 12(%ebp)\n\tpushl 8(%ebp)\n\tcall *_direction_original\n\t"
        "pushfl\n\tpushal\n\tpushl 12(%ebp)\n\tpushl %ebx\n\t"
        "call _apply_direction\n\taddl $8,%esp\n\tpopal\n\tpopfl\n\t"
        "popl %ebx\n\tleave\n\tret $8\n\t");
}
BOOL SudekiMpLanAimInstall(HMODULE image, SudekiMpLanAimWitness witness) {
    if (!witness || aim_image || !SudekiMpLanAimImageMatches(image)) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    aim_image=(uint8_t *)image;
    direction_original=aim_image+AIM_DIRECTION;
    aim_witness=witness;
    if (!SudekiMpInstallRelativeCallHook(&direction_hook,aim_image+AIM_CALL,
            direction_original,direction_bridge)) {
        aim_image=NULL; aim_witness=NULL; direction_original=NULL; return FALSE;
    }
    pose_original=aim_image+POSE_BUILD;
    if(!SudekiMpInstallRelativeCallHook(&pose_hook,aim_image+POSE_CALL,
        pose_original,pose_bridge)) {
        DWORD error=GetLastError(); (void)SudekiMpLanAimUninstall();
        SetLastError(error); return FALSE;
    }
    return TRUE;
}
BOOL SudekiMpLanAimUninstall(void) {
    if (!SudekiMpRestoreRelativeCallHook(&pose_hook)) return FALSE;
    if (!SudekiMpRestoreRelativeCallHook(&direction_hook)) return FALSE;
    aim_image=NULL; direction_original=NULL; aim_witness=NULL; aim_thread=0;
    memset(aim_actors,0,sizeof(aim_actors)); corrected_shots=0;
    memset(world_renderers,0,sizeof(world_renderers)); pose_original=NULL;
    corrected_poses=0;
    return TRUE;
}
void SudekiMpLanAimActors(void *first, void *second) {
    if (!aim_image) return;
    aim_thread=GetCurrentThreadId();
    aim_actors[0]=first; aim_actors[1]=second;
    world_renderers[0]=world_renderer(first); world_renderers[1]=world_renderer(second);
}
