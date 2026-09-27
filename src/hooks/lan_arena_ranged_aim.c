#include "hooks/lan_arena_ranged_aim.h"
#include "hooks/call_hook.h"
#include "engine/log.h"
#include <math.h>
#include <string.h>

enum { AIM_CALL = 0xc74e3, AIM_DIRECTION = 0xc7aa0,
    MISSILE_VT = 0x2d4c8c, POSE_CALL=0x222434, POSE_BUILD=0x21e810,
    POSE_SAMPLE=0x21df00, BASE_SAMPLE_CALL=0x2221df,
    RENDERER_VT=0x2df8ec, ELCO_VT=0x2d66fc,
    MODEL_VT=0x2d5464, BONES=109 };
static uint8_t *aim_image;
static SudekiMpRelativeCallHook direction_hook;
static SudekiMpRelativeCallHook pose_hook;
static SudekiMpRelativeCallHook sample_hook;
static void *direction_original __attribute__((used));
static void *pose_original __attribute__((used));
static void *sample_original __attribute__((used));
static SudekiMpLanAimWitness aim_witness;
static SudekiMpLanAimTargetWitness aim_target_witness;
static SudekiMpLanAimFireWitness aim_fire_witness;
static SudekiMpLanIdleWitness idle_witness;
static DWORD aim_thread;
static void *aim_actors[2];
static uint32_t corrected_shots;
static SudekiMpLanWeaponShotObserver weapon_shot_observer;

void SudekiMpLanAimSetShotObserver(SudekiMpLanWeaponShotObserver observer) {
    weapon_shot_observer = observer;
}
void SudekiMpLanAimSetIdleWitness(SudekiMpLanIdleWitness witness) {
    idle_witness = witness;
}
static void *world_renderers[2];
static void *first_person_renderers[2];
enum { FP_BONES=22, FP_IDLE_SELECTOR=5 };
static float fp_idle_start[FP_BONES*12] __attribute__((aligned(16)));
static float fp_idle_result[FP_BONES*12] __attribute__((aligned(16)));
static uint32_t smoothed_fp_idle_poses;
/* Retail sampler uses MOVAPS stores for local quaternions. */
static float aimed_pose[BONES*12] __attribute__((aligned(16)));
static float center_pose[BONES*12] __attribute__((aligned(16)));
static float result_pose[BONES*12] __attribute__((aligned(16)));
static unsigned pose_depth;
static uint32_t corrected_poses;
static uint32_t calibrated_poses;
static float reference_pose[BONES*12] __attribute__((aligned(16)));
static float uncalibrated_reference[BONES*12] __attribute__((aligned(16)));
static float hold_straight[BONES*12] __attribute__((aligned(16)));
typedef struct HoldState { DWORD at; float weight; BOOL valid; } HoldState;
static HoldState hold_states[2];
static uint32_t held_base_samples;
static uint32_t idle_pose_passthrough;
/* Bounded diagnostics for admitted renderer calls, readable by the external
 * observer. A missed frame must not be confused with successful calibration. */
static volatile LONG pose_attempts;
static volatile LONG pose_rejected[5]; /* thread/reentry, witness, owner, layout, math */

enum { SAMPLE_CHANNELS=2, SAMPLE_STATE_BYTES=24 };
static void sample_states(uint8_t states[SAMPLE_CHANNELS][SAMPLE_STATE_BYTES],
    uint16_t selector,float phase) {
    memset(states,0,SAMPLE_CHANNELS*SAMPLE_STATE_BYTES);
    for(unsigned channel=0;channel<SAMPLE_CHANNELS;++channel) {
        memcpy(states[channel],&selector,sizeof(selector));
        memcpy(states[channel]+12,&phase,sizeof(phase));
    }
}

static void copy_firing_pose(float *pose,const float *straight,const uint8_t *mask,unsigned count) {
    for(unsigned i=0;i<count;++i) if(mask[i]) {
        /* The sampler writes XYZ/XYZ/quaternion, not translation.w or scale.w.
         * Preserve native homogeneous lanes: replacing translation.w with the
         * zero in our sampling buffer discards the parent's translation in
         * the retail matrix builder and detaches the weapon from its actor. */
        memcpy(pose+i*12,straight+i*12,3*sizeof(float));
        memcpy(pose+i*12+4,straight+i*12+4,3*sizeof(float));
        memcpy(pose+i*12+8,straight+i*12+8,4*sizeof(float));
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
static BOOL point(const float p[3]) {
    if(!p) return FALSE;
    for(unsigned i=0;i<3;++i)
        if(!isfinite(p[i]) || fabsf(p[i])>=1000000.0f) return FALSE;
    return TRUE;
}
BOOL SudekiMpLanAimConverge(const float muzzle[3],const float target[3],float direction[3]) {
    float delta[3],norm=0;
    if(!direction || !point(muzzle) || !point(target)) return FALSE;
    for(unsigned i=0;i<3;++i) { delta[i]=target[i]-muzzle[i]; norm+=delta[i]*delta[i]; }
    if(!isfinite(norm) || norm<0.000001f) return FALSE;
    norm=sqrtf(norm);
    for(unsigned i=0;i<3;++i) direction[i]=delta[i]/norm;
    return TRUE;
}
BOOL SudekiMpLanAimTargetNearActor(const float actor[3],const float direction[3],const float target[3]) {
    float unit[3],distance=0;
    if(!point(actor) || !point(target) || !SudekiMpLanAimNormalize(direction,unit)) return FALSE;
    /* Retail target = camera position + normalized forward * 100. A remote
     * request cannot relocate that camera far from its authoritative actor.
     * Five units accommodates the local eye, recoil and network movement. */
    for(unsigned i=0;i<3;++i) {
        float offset=target[i]-unit[i]*100.0f-actor[i]; distance+=offset*offset;
    }
    return isfinite(distance) && distance<=25.0f;
}
BOOL SudekiMpLanAimCameraTarget(float direction[3],float target[3]) {
    uint8_t *manager,*camera,*mode,*render;
    float unit[3],result[3];
    if(!aim_image || !direction || !target || GetCurrentThreadId()!=aim_thread ||
        !memory(aim_image+0x409d7c,4,FALSE) || !memory(aim_image+0x408da8,4,FALSE)) return FALSE;
    manager=*(uint8_t **)(aim_image+0x409d7c);
    mode=*(uint8_t **)(aim_image+0x408da8);
    if(!memory(manager,0x24,FALSE) || *(void **)manager!=aim_image+0x2c7b80 ||
        !memory(mode,0x10,FALSE)) return FALSE;
    camera=*(uint8_t **)(manager+0x20);
    if(!memory(camera,0x108,FALSE) || *(void **)camera!=aim_image+0x2cce5c ||
        *(void **)(mode+0xc)!=camera+0x2c) return FALSE;
    render=*(uint8_t **)(camera+0x34);
    if(!memory(render,0xd0,FALSE) || !point((float *)(render+0xc0)) ||
        !SudekiMpLanAimNormalize((float *)(render+0xb0),unit)) return FALSE;
    for(unsigned i=0;i<3;++i) result[i]=((float *)(render+0xc0))[i]+unit[i]*100.0f;
    if(!point(result)) return FALSE;
    memcpy(direction,unit,12); memcpy(target,result,12); return TRUE;
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
/* Original FP_Elco_Idle01 has a small rotational seam at frame40. Close only
 * its final four frames, in transient pose space, without retiming the native
 * clock or touching the firing/reload state machine. Homogeneous lanes and
 * translation/scale are preserved exactly; the original idle has constant
 * translation/scale tracks. Validate the entire operation before writing. */
static BOOL close_fp_idle_loop(float *pose,const float *start,unsigned count,float phase) {
    if(!pose || !start || !count || count>FP_BONES || !isfinite(phase) ||
        phase<0 || phase>40) return FALSE;
    if(phase<=36) return TRUE;
    for(unsigned i=0;i<count;++i)
        if(!quaternion(pose+i*12+8) || !quaternion(start+i*12+8)) return FALSE;
    float t=(phase-36)*.25f,weight=t*t*t*(t*(t*6-15)+10);
    for(unsigned i=0;i<count;++i) {
        float *q=pose+i*12+8;const float *target=start+i*12+8;
        if(!memcmp(q,target,16)) continue;
        float dot=0,n=0,out[4];
        for(unsigned j=0;j<4;++j) dot+=q[j]*target[j];
        for(unsigned j=0;j<4;++j) {
            out[j]=q[j]*(1-weight)+target[j]*(dot<0 ? -weight:weight);
            n+=out[j]*out[j];
        }
        n=sqrtf(n);
        for(unsigned j=0;j<4;++j) q[j]=out[j]/n;
    }
    return TRUE;
}
/* The renderer cache is only a rejection filter. Resolve the local owner and
 * its attached FP wrapper afresh before using the loaded bank or any pose. */
static void *first_person_renderer(void *actor) {
    uint8_t *a=actor,*model,*position,*arbiter,*controller,*wrapper,*renderer,*bank,*header;
    if(!aim_image || !memory(a,0x138,FALSE) || *(void **)a!=aim_image+ELCO_VT ||
        !memory(aim_image+0x408da4,4,FALSE)) return NULL;
    controller=*(uint8_t **)(aim_image+0x408da4);
    if(!memory(controller,0x24c,FALSE) || *(void **)(controller+0x248)!=actor) return NULL;
    model=*(uint8_t **)(a+0x134);position=*(uint8_t **)(a+0x44);arbiter=*(uint8_t **)(a+0x90);
    if(!memory(model,0x168,FALSE) || *(void **)model!=aim_image+MODEL_VT ||
        *(void **)(model+0x10)!=actor || !memory(position,0xb8,FALSE) ||
        *(void **)(position+0x10)!=actor || !memory(arbiter,0x54,FALSE) ||
        *(void **)(arbiter+0x10)!=actor || !(*(uint32_t *)(arbiter+0x50)&0x400000u)) return NULL;
    wrapper=*(uint8_t **)(model+0x160);
    if(wrapper!=*(void **)(position+0xb4) || !memory(wrapper,0x14,FALSE)) return NULL;
    renderer=*(uint8_t **)(wrapper+0x10);
    if(!memory(renderer,0xa8,FALSE) || *(void **)renderer!=aim_image+RENDERER_VT) return NULL;
    bank=*(uint8_t **)(renderer+8);
    if(!memory(bank,0x28,FALSE)) return NULL;
    header=*(uint8_t **)(bank+0x1c);
    if(!memory(header,16,FALSE) || *(uint32_t *)header!=11 ||
        *(uint32_t *)(header+4)!=FP_BONES || *(uint32_t *)(header+8)!=1 ||
        *(uint32_t *)(header+12)!=1) return NULL;
    return renderer;
}
static BOOL fp_idle_storage(void *renderer,void *bank,float *phase) {
    uint8_t *r=renderer,*channels,*blends,*state;
    if(!phase || !memory(r,0xa8,FALSE) || *(void **)(r+8)!=bank ||
        *(uint32_t *)(r+0xa0)!=5 || *(uint32_t *)(r+0xa4)!=4) return FALSE;
    channels=*(uint8_t **)(r+0x98);blends=*(uint8_t **)(r+0x9c);
    if(!memory(channels,5*36,FALSE) || !memory(blends,4*20,FALSE)) return FALSE;
    const uint16_t links[8]={0,1,2,3,0x8000,0x8001,0x8002,4};
    for(unsigned i=0;i<4;++i) {
        /* Accept the measured unblended FP base only. Fire, recovery and
         * switching blends must finish natively before smoothing resumes. */
        if(memcmp(blends+i*20,links+i*2,4) ||
            *(uint32_t *)(blends+i*20+4)!=(i==3 ? 0x80000002u:0xffffffffu) ||
            *(uint32_t *)(blends+i*20+8)!=0 || *(float *)(blends+i*20+12)!=0) return FALSE;
    }
    state=*(uint8_t **)channels;
    if(!memory(state,24,FALSE) || *(uint16_t *)state!=FP_IDLE_SELECTOR ||
        (*(uint16_t *)(state+2)&~0x80u) || *(float *)(state+4)!=24.f ||
        !isfinite(*(float *)(state+8)) || !isfinite(*(float *)(state+12)) ||
        *(float *)(state+12)<0 || *(float *)(state+12)>=40) return FALSE;
    *phase=*(float *)(state+12);return TRUE;
}
static BOOL fp_idle_layout(uint8_t *bank) {
    uint8_t *header,*entries,*clip,*groups,*hierarchy;
    uint32_t *palette;void **tracks;
    if(!memory(bank,0x28,FALSE)) return FALSE;
    header=*(uint8_t **)(bank+0x1c);entries=*(uint8_t **)(bank+0x20);
    groups=*(uint8_t **)(bank+0x24);
    if(!memory(header,16,FALSE) || *(uint32_t *)header!=11 ||
        *(uint32_t *)(header+4)!=FP_BONES || *(uint32_t *)(header+8)!=1 ||
        *(uint32_t *)(header+12)!=1 || !memory(entries,11*28,FALSE) ||
        !memory(groups,8,FALSE)) return FALSE;
    clip=*(uint8_t **)(entries+FP_IDLE_SELECTOR*28);
    tracks=*(void ***)(entries+FP_IDLE_SELECTOR*28+4);
    if(!memory(clip,24,FALSE) || *(uint32_t *)clip!=1050148086u ||
        *(float *)(clip+4)!=40.f || *(uint32_t *)(clip+8)!=0 ||
        !memory(tracks,FP_BONES*4,FALSE)) return FALSE;
    hierarchy=*(uint8_t **)groups;palette=*(uint32_t **)(groups+4);
    if(!memory(hierarchy,FP_BONES*8,FALSE) || !memory(palette,FP_BONES*4,FALSE)) return FALSE;
    uint8_t seen[FP_BONES]={0};
    for(unsigned i=0;i<FP_BONES;++i) {
        int parent=*(int16_t *)(hierarchy+i*8+4);
        if(palette[i]>=FP_BONES || seen[palette[i]] || parent < -1 || parent>=(int)i ||
            *(int16_t *)(hierarchy+i*8+6)!=0 || !memory(tracks[i],4,FALSE)) return FALSE;
        seen[palette[i]]=1;
    }
    return TRUE;
}
static float *first_person_idle_pose(void *actor,void *renderer,void *bank,float *original) {
    float phase;
    if(GetCurrentThreadId()!=aim_thread || pose_depth || !idle_witness ||
        !idle_witness(actor) || first_person_renderer(actor)!=renderer ||
        !fp_idle_storage(renderer,bank,&phase) || phase<=36 ||
        !memory(original,sizeof(fp_idle_result),FALSE) || !fp_idle_layout(bank)) return original;
    typedef void (__stdcall *Sample)(void *,void *,void *);
    uint8_t states[SAMPLE_CHANNELS][SAMPLE_STATE_BYTES];
    sample_states(states,FP_IDLE_SELECTOR,0);
    ++pose_depth;
    memcpy(fp_idle_start,original,sizeof(fp_idle_start));
    ((Sample)(aim_image+POSE_SAMPLE))(bank,states,fp_idle_start);
    memcpy(fp_idle_result,original,sizeof(fp_idle_result));
    BOOL okay=close_fp_idle_loop(fp_idle_result,fp_idle_start,FP_BONES,phase);
    --pose_depth;
    if(!okay) return original;
    if(++smoothed_fp_idle_poses<=3)
        SudekiMpLogFormat("lan_ranged_aim event=fp_idle_loop actor=Elco phase=%.3f policy=transient_rotation_seam_no_clock_change\r\n",phase);
    return fp_idle_result;
}
static void rotate(const float q[4],const float v[3],float out[3]) {
    float t[3]={2*(q[1]*v[2]-q[2]*v[1]),2*(q[2]*v[0]-q[0]*v[2]),2*(q[0]*v[1]-q[1]*v[0])};
    float r[3]={v[0]+q[3]*t[0]+q[1]*t[2]-q[2]*t[1],
        v[1]+q[3]*t[1]+q[2]*t[0]-q[0]*t[2],v[2]+q[3]*t[2]+q[0]*t[1]-q[1]*t[0]};
    memcpy(out,r,sizeof(r));
}
/* Model-space forward kinematics. Parents name pose indices and must be an
 * acyclic parent-first permutation. Only the pivot quaternion is changed.
 * Weapon model/locator admission is the caller's responsibility. */
static BOOL calibrate_pose(float *pose,const int *parent,const unsigned *order,
    unsigned count,unsigned pivot,unsigned hand,const float muzzle_local[3],const float target[3]) {
    float rotations[BONES][4],positions[BONES][3],scales[BONES],saved[4];
    uint8_t seen[BONES]={0},chain[BONES]={0};
    if(!pose || !parent || !order || !count || count>BONES || pivot>=count || hand>=count ||
        !point(target) || !point(muzzle_local)) return FALSE;
    for(unsigned n=0;n<count;++n) {
        unsigned i=order[n];
        if(i>=count || seen[i] || parent[i]<-1 || parent[i]>=(int)count ||
            (parent[i]>=0 && !seen[parent[i]]) || !quaternion(pose+i*12+8) ||
            !point(pose+i*12) || !isfinite(pose[i*12+4]) || pose[i*12+4]<=0 ||
            !isfinite(pose[i*12+5]) || pose[i*12+5]<=0 ||
            !isfinite(pose[i*12+6]) || pose[i*12+6]<=0) return FALSE;
        seen[i]=1;
    }
    for(int ancestor=(int)hand;ancestor>=0;ancestor=parent[ancestor]) {
        chain[ancestor]=1;
        /* Only the hand's ancestor chain contributes to barrel geometry.
         * Authored clothing/finger branches have non-uniform scales; those
         * transforms are preserved and must not be flattened or rejected. */
        float *v=pose+ancestor*12;
        if(fabsf(v[4]-v[5])>.0001f || fabsf(v[4]-v[6])>.0001f) return FALSE;
    }
    if(!chain[pivot]) return FALSE;
    memcpy(saved,pose+pivot*12+8,16);
    for(unsigned iteration=0;iteration<=5;++iteration) {
        for(unsigned n=0;n<count;++n) {
            unsigned i=order[n];int p=parent[i];float *v=pose+i*12;
            if(!chain[i]) continue;
            if(p<0) { memcpy(rotations[i],v+8,16);memcpy(positions[i],v,12);scales[i]=v[4]; }
            else {
                float t[3]={v[0]*scales[p],v[1]*scales[p],v[2]*scales[p]};
                multiply(rotations[p],v+8,rotations[i]);rotate(rotations[p],t,positions[i]);
                for(unsigned axis=0;axis<3;++axis) positions[i][axis]+=positions[p][axis];
                scales[i]=scales[p]*v[4];
            }
        }
        float offset[3],muzzle[3],axis[3],desired[3],delta[4],local[4];
        for(unsigned j=0;j<3;++j) offset[j]=muzzle_local[j]*scales[hand];
        rotate(rotations[hand],offset,muzzle);
        for(unsigned j=0;j<3;++j) muzzle[j]+=positions[hand][j];
        rotate(rotations[hand],(float[]){-1,0,0},axis);
        /* Compressed/interpolated native quaternions are only approximately
         * unit length. Compare directions, not their lengths: otherwise an
         * aligned axis of length0.99998 never satisfies the angular tolerance
         * and intermittently drops the complete aim layer during idle. Keep
         * native key data untouched and normalize only the derived axis. */
        if(!SudekiMpLanAimConverge((float[]){0,0,0},axis,axis) ||
           !SudekiMpLanAimConverge(muzzle,target,desired)) break;
        float dot=axis[0]*desired[0]+axis[1]*desired[1]+axis[2]*desired[2];
        if(dot>.999999f) return TRUE; /* below 0.082 degrees; float FK tolerance */
        if(!isfinite(dot) || dot<-.99f || iteration==5) break;
        delta[0]=axis[1]*desired[2]-axis[2]*desired[1];
        delta[1]=axis[2]*desired[0]-axis[0]*desired[2];
        delta[2]=axis[0]*desired[1]-axis[1]*desired[0];delta[3]=1+dot;
        float norm=sqrtf(delta[0]*delta[0]+delta[1]*delta[1]+delta[2]*delta[2]+delta[3]*delta[3]);
        if(!isfinite(norm) || norm<.0001f) break;
        for(unsigned j=0;j<4;++j) delta[j]/=norm;
        int p=parent[pivot];
        if(p>=0) {
            float inverse[4]={-rotations[p][0],-rotations[p][1],-rotations[p][2],rotations[p][3]};
            multiply(inverse,delta,local);multiply(local,rotations[p],delta);
        }
        multiply(delta,pose+pivot*12+8,pose+pivot*12+8);
        if(!quaternion(pose+pivot*12+8)) break;
    }
    memcpy(pose+pivot*12+8,saved,16); return FALSE;
}
/* A support hand must follow the gun-arm correction, not remain in a different
 * pose. Solve the existing two-bone arm without moving the chest, shoulder
 * anchors, or stretching the skeleton. These helpers operate only on scratch
 * reference poses; native animation clocks and recoil layers stay untouched. */
static BOOL joint_world(const float *pose,const int *parent,unsigned count,unsigned joint,
    float rotation[4],float position[3]) {
    unsigned chain[BONES],length=0;uint8_t seen[BONES]={0};
    if(!pose || !parent || !count || count>BONES || joint>=count) return FALSE;
    for(int at=(int)joint;at>=0;at=parent[at]) {
        if((unsigned)at>=count || seen[at] || parent[at]<-1) return FALSE;
        seen[at]=1;chain[length++]=(unsigned)at;
    }
    float q[4]={0,0,0,1},p[3]={0},scale=1;
    while(length) {
        const float *v=pose+chain[--length]*12;float local[4],offset[3],norm=0;
        if(!point(v) || !quaternion(v+8) || !isfinite(v[4]) || v[4]<=0 ||
            !isfinite(v[5]) || !isfinite(v[6]) ||
            fabsf(v[4]-v[5])>.0001f || fabsf(v[4]-v[6])>.0001f) return FALSE;
        for(unsigned i=0;i<4;++i) norm+=v[8+i]*v[8+i];
        for(unsigned i=0;i<4;++i) local[i]=v[8+i]/sqrtf(norm);
        for(unsigned i=0;i<3;++i) offset[i]=v[i]*scale;
        rotate(q,offset,offset);
        for(unsigned i=0;i<3;++i) p[i]+=offset[i];
        multiply(q,local,q);scale*=v[4];
        if(!isfinite(scale) || !point(p)) return FALSE;
    }
    memcpy(rotation,q,16);memcpy(position,p,12);return TRUE;
}
static BOOL from_to(const float from[3],const float to[3],float out[4]) {
    float a[3],b[3];
    if(!SudekiMpLanAimConverge((float[]){0,0,0},from,a) ||
       !SudekiMpLanAimConverge((float[]){0,0,0},to,b)) return FALSE;
    float dot=a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
    if(dot<-.9999f) return FALSE; /* no arbitrary elbow flip */
    float q[4]={a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],
        a[0]*b[1]-a[1]*b[0],1+dot};
    float n=sqrtf(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);
    if(!isfinite(n) || n<.0001f) return FALSE;
    for(unsigned i=0;i<4;++i) out[i]=q[i]/n;
    return TRUE;
}
static BOOL set_world_rotation(float *pose,const int *parent,unsigned count,unsigned joint,
    const float rotation[4]) {
    float q[4]={0,0,0,1},p[3],local[4];
    if(joint>=count || !quaternion(rotation) || parent[joint]<-1 ||
       (parent[joint]>=0 && !joint_world(pose,parent,count,(unsigned)parent[joint],q,p))) return FALSE;
    float inverse[4]={-q[0],-q[1],-q[2],q[3]};multiply(inverse,rotation,local);
    if(!quaternion(local)) return FALSE;
    memcpy(pose+joint*12+8,local,16);return TRUE;
}
static BOOL follow_support(float *pose,const int *parent,unsigned count,
    unsigned shoulder,unsigned elbow,unsigned wrist,const float target[3],const float wrist_rotation[4]) {
    float aq[4],bq[4],cq[4],a[3],b[3],c[3],direction[3],pole[3],goal[3];
    if(shoulder>=count || elbow>=count || wrist>=count || parent[elbow]!=(int)shoulder ||
       parent[wrist]!=(int)elbow || !point(target) || !quaternion(wrist_rotation) ||
       !joint_world(pose,parent,count,shoulder,aq,a) ||
       !joint_world(pose,parent,count,elbow,bq,b) ||
       !joint_world(pose,parent,count,wrist,cq,c) || !SudekiMpLanAimConverge(a,target,direction)) return FALSE;
    float l1=0,l2=0,d=0,projection=0;
    for(unsigned i=0;i<3;++i) {
        l1+=(b[i]-a[i])*(b[i]-a[i]);l2+=(c[i]-b[i])*(c[i]-b[i]);
        d+=(target[i]-a[i])*(target[i]-a[i]);projection+=(b[i]-a[i])*direction[i];
    }
    l1=sqrtf(l1);l2=sqrtf(l2);d=sqrtf(d);
    if(l1<.0001f || l2<.0001f || !isfinite(d)) return FALSE;
    /* Keep the authored elbow side. Unreachable aim extremes approach the
     * nearest reachable point, never stretching or rotating the whole torso. */
    for(unsigned i=0;i<3;++i) pole[i]=b[i]-a[i]-projection*direction[i];
    if(!SudekiMpLanAimConverge((float[]){0,0,0},pole,pole)) return FALSE;
    float epsilon=fminf(l1,l2)*.001f;
    d=fminf(l1+l2-epsilon,fmaxf(fabsf(l1-l2)+epsilon,d));
    float along=(l1*l1-l2*l2+d*d)/(2*d),height=sqrtf(fmaxf(0,l1*l1-along*along));
    for(unsigned i=0;i<3;++i) goal[i]=a[i]+direction[i]*along+pole[i]*height;
    float from[3],to[3],delta[4],turned[4];
    for(unsigned i=0;i<3;++i) { from[i]=b[i]-a[i];to[i]=goal[i]-a[i]; }
    if(!from_to(from,to,delta)) return FALSE;
    multiply(delta,aq,turned);
    if(!set_world_rotation(pose,parent,count,shoulder,turned) ||
       !joint_world(pose,parent,count,elbow,bq,b) || !joint_world(pose,parent,count,wrist,cq,c)) return FALSE;
    for(unsigned i=0;i<3;++i) { from[i]=c[i]-b[i];to[i]=a[i]+direction[i]*d-b[i]; }
    if(!from_to(from,to,delta)) return FALSE;
    multiply(delta,bq,turned);
    return set_world_rotation(pose,parent,count,elbow,turned) &&
        set_world_rotation(pose,parent,count,wrist,wrist_rotation);
}
static BOOL calibrate_arms(float *pose,const int *parent,const unsigned *order,unsigned count,
    unsigned gun_arm,unsigned gun_hand,unsigned left_arm,unsigned left_elbow,unsigned left_wrist,
    const float muzzle[3],const float target[3]) {
    unsigned joints[4]={gun_arm,left_arm,left_elbow,left_wrist};float saved[4][4];
    float before_q[4],after_q[4],pivot[3],after_p[3],wrist_q[4],wrist_p[3];
    if(!pose || !parent || !order || !count || count>BONES) return FALSE;
    for(unsigned i=0;i<4;++i) {
        if(joints[i]>=count) return FALSE;
        for(unsigned j=0;j<i;++j) if(joints[j]==joints[i]) return FALSE;
        memcpy(saved[i],pose+joints[i]*12+8,16);
    }
    if(!joint_world(pose,parent,count,gun_arm,before_q,pivot) ||
       !joint_world(pose,parent,count,left_wrist,wrist_q,wrist_p)) return FALSE;
    /* The two chains must be separate: adjusting a support ancestor must
     * never undo the gun solution or rotate the actor's shared spine. */
    for(int at=(int)gun_arm;at>=0;at=parent[at])
        if(at==(int)left_arm) return FALSE;
    for(int at=(int)left_arm;at>=0;at=parent[at])
        if(at==(int)gun_arm) return FALSE;
    BOOL okay=calibrate_pose(pose,parent,order,count,gun_arm,gun_hand,muzzle,target) &&
        joint_world(pose,parent,count,gun_arm,after_q,after_p);
    if(okay) {
        float inverse[4]={-before_q[0],-before_q[1],-before_q[2],before_q[3]},delta[4],offset[3],goal[3],goal_q[4];
        multiply(after_q,inverse,delta);
        for(unsigned i=0;i<3;++i) offset[i]=wrist_p[i]-pivot[i];
        rotate(delta,offset,goal);
        for(unsigned i=0;i<3;++i) goal[i]+=pivot[i];
        multiply(delta,wrist_q,goal_q);
        okay=follow_support(pose,parent,count,left_arm,left_elbow,left_wrist,goal,goal_q);
    }
    if(!okay) for(unsigned i=0;i<4;++i) memcpy(pose+joints[i]*12+8,saved[i],16);
    return okay;
}
/* Apply the calibrated reference as a rotation delta to the already blended
 * native pose. The reference is NOT the output: replacing the upper body
 * would erase idle motion, shot recoil and the native fire/idle blend. */
static BOOL apply_reference(float *pose,const float *reference,const float *straight,
    const uint8_t *mask,unsigned count,float stance) {
    /* FirePose-relative aim belongs to the firing stance, not combat idle.
     * At zero this must be an exact no-op, including compressed native keys. */
    if(!isfinite(stance) || stance<0 || stance>1) return FALSE;
    return SudekiMpLanAimOverlay(pose,reference,straight,mask,count,stance);
}
/* Presentation envelope only. Never runs native input, changes a selector or
 * guesses recoil timing. The native firing layer owns the final shot tail. */
static float advance_hold(HoldState *s,DWORD now,BOOL admitted,BOOL held,BOOL recoil) {
    if(!admitted) { memset(s,0,sizeof(*s));return 0; }
    DWORD elapsed=s->valid ? now-s->at:0;
    s->valid=TRUE;s->at=now;
    if(elapsed>100u) elapsed=100u;
    float target=held || recoil ? 1.f:0.f;
    float step=(float)elapsed/(target>0 ? 100.f:200.f);
    if(s->weight<target) s->weight=fminf(target,s->weight+step);
    else s->weight=fmaxf(target,s->weight-step);
    return s->weight;
}
static float current_stance(const HoldState *s,DWORD now) {
    /* A skipped/rejected base sampler is not evidence of a full firing pose.
     * Do not apply last frame's fire reference to a different native state. */
    return s && s->valid && (DWORD)(now-s->at)<=100u &&
        isfinite(s->weight) && s->weight>=0 && s->weight<=1 ? s->weight:0;
}
/* Rebase the coordinated upper-body rotations in the BASE pose, before retail
 * blend tree adds its actual per-shot firing layer. This avoids both resetting
 * the native shot phase and double-applying a manually reconstructed recoil.
 * Legs and every translation/scale remain native. The authored upper-body
 * stance includes both clavicles/arms; recoil still animates above this base. */
static BOOL blend_hold_base(float *pose,const float *straight,const uint8_t *mask,
    unsigned count,float amount) {
    return SudekiMpLanAimOverlay(pose,straight,pose,mask,count,amount);
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
        bytes(b,0x21e203,"\xc2\x0c\x00",3) &&
        relative_call(b,BASE_SAMPLE_CALL,POSE_SAMPLE) &&
        bytes(b,BASE_SAMPLE_CALL-6,"\x52\x8b\x14\x19\x52\x50",6) &&
        bytes(b,BASE_SAMPLE_CALL+5,"\x47\x83\xc3\x24\x3b\xbe\xa0\x00\x00\x00",10) &&
        bytes(b,0x2234e0,"\x8b\x44\x24\x04\x8b\x89\x9c\x00\x00\x00"
            "\x8d\x04\x80\xd9\x44\x81\x0c\xc2\x04\x00",20);
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
/* Narrow measured weapon adapter: Proton Phaser's original bank and SFX
 * locator. Other weapons retain the prior authored overlay, never borrow
 * this barrel basis. No native methods or cross-frame pointer caches. */
static BOOL proton_geometry(void *actor,void *renderer,const float target[3],
    float model_target[3],float muzzle_local[3]) {
    uint8_t *a=actor,*weapon,*position,*wrapper,*body,*gun,*gr,*bank,*header,*entries,*clip;
    float *world,*gun_local,*matrices; unsigned sfx=UINT32_MAX;
    weapon=*(uint8_t **)(a+0xc0); position=*(uint8_t **)(a+0x44);
    if(!memory(weapon,0x270,FALSE) || *(void **)weapon!=aim_image+0x2d4d3c ||
        *(void **)(weapon+0x10)!=actor || !memory(position,0xb8,FALSE) ||
        *(void **)(weapon+0xd4)!=position+4 ||
        !memory(*(void **)(weapon+0x268),0x18,FALSE) ||
        *(uint32_t *)(*(uint8_t **)(weapon+0x268)+0x14)!=24u) return FALSE;
    wrapper=*(uint8_t **)(position+0xb4);
    if(!memory(wrapper,0x14,FALSE) || *(void **)(wrapper+0x10)!=renderer) return FALSE;
    body=*(uint8_t **)(wrapper+8);
    wrapper=*(uint8_t **)(weapon+0xf4);
    if(!memory(body,0xd0,FALSE) || !memory(wrapper,0x14,FALSE)) return FALSE;
    gun=*(uint8_t **)(wrapper+8); gr=*(uint8_t **)(wrapper+0x10);
    if(!memory(gun,0xd0,FALSE) || !memory(gr,0x10,FALSE) ||
        *(void **)gr!=aim_image+RENDERER_VT) return FALSE;
    bank=*(uint8_t **)(gr+8);
    if(!memory(bank,0x28,FALSE)) return FALSE;
    header=*(uint8_t **)(bank+0x1c);entries=*(uint8_t **)(bank+0x20);
    if(!memory(header,16,FALSE) || *(uint32_t *)header!=1 ||
        *(uint32_t *)(header+4)!=11 || !memory(entries,28,FALSE)) return FALSE;
    clip=*(uint8_t **)entries;
    if(!memory(clip,24,FALSE) || *(uint32_t *)clip!=1235880602u || *(uint32_t *)(clip+8)!=0) return FALSE;
    uint8_t *groups=*(uint8_t **)(bank+0x24);
    if(!memory(groups,8,FALSE)) return FALSE;
    uint8_t *hier=*(uint8_t **)groups;uint32_t *palette=*(uint32_t **)(groups+4);
    if(!memory(hier,11*8,FALSE) || !memory(palette,11*4,FALSE)) return FALSE;
    for(unsigned i=0;i<11;++i) if(*(uint32_t *)(hier+i*8)==124205u) sfx=palette[i];
    if(sfx>=11) return FALSE;
    matrices=*(float **)(gr+0xc);gun_local=(float *)(gun+0x90);
    world=*(void **)(body+0x18) ? *(float **)(body+0x38):(float *)(body+0x90);
    if(!memory(matrices,11*64,FALSE) || !memory(world,64,FALSE) || !point(world+12) ||
        !point(matrices+sfx*16+12)) return FALSE;
    /* Primary weapon attachment is identity plus uniform model scale. */
    float scale=gun_local[0];
    if(!isfinite(scale) || scale<.01f || scale>100 ||
        fabsf(gun_local[5]-scale)>.001f || fabsf(gun_local[10]-scale)>.001f) return FALSE;
    for(unsigned i=0;i<16;++i) {
        if(!isfinite(gun_local[i])) return FALSE;
        if(i!=0 && i!=5 && i!=10 && i!=15 && fabsf(gun_local[i])>.0001f) return FALSE;
    }
    if(fabsf(gun_local[15]-1)>.0001f) return FALSE;
    for(unsigned i=0;i<3;++i) {
        float length=0,dot=0;
        for(unsigned j=0;j<3;++j) { length+=world[i*4+j]*world[i*4+j];dot+=(target[j]-world[12+j])*world[i*4+j]; }
        if(!isfinite(length) || fabsf(length-1)>.002f) return FALSE;
        for(unsigned k=0;k<i;++k) {
            float perpendicular=0;
            for(unsigned j=0;j<3;++j) perpendicular+=world[i*4+j]*world[k*4+j];
            if(!isfinite(perpendicular) || fabsf(perpendicular)>.002f) return FALSE;
        }
        model_target[i]=dot/length;
        muzzle_local[i]=matrices[sfx*16+12+i]*scale;
    }
    return point(model_target) && point(muzzle_local);
}
static BOOL pose_layout(uint8_t *bank,uint8_t mask[BONES],int parents[BONES],
    unsigned order[BONES],int *hand_out,int *arm_out,int support[3]) {
    uint8_t *entries,*circle,*straight,*groups,*hierarchy;
    uint32_t *palette;
    /* Native hierarchy groups sort the bones; palette maps sorted entries to
     * pose indices. Use that mapping, never assume exporter order at runtime. */
    entries=*(uint8_t **)(bank+0x20); groups=*(uint8_t **)(bank+0x24);
    if(!memory(entries,141*28,FALSE)) return FALSE;
    circle=*(uint8_t **)(entries+50*28); straight=*(uint8_t **)(entries+54*28);
    if(!memory(circle,24,FALSE) || !memory(straight,24,FALSE) ||
        *(uint32_t *)(circle+8)!=*(uint32_t *)(straight+8) ||
        *(uint32_t *)(circle+8)>8 ||
        !memory(groups,(*(uint32_t *)(circle+8)+1)*8,FALSE)) return FALSE;
    groups+=*(uint32_t *)(circle+8)*8;
    hierarchy=*(uint8_t **)groups; palette=*(uint32_t **)(groups+4);
    if(!memory(hierarchy,BONES*8,FALSE) || !memory(palette,BONES*4,FALSE)) return FALSE;
    int upper=-1,hand=-1,arm=-1;
    const uint32_t support_hashes[3]={1075686250u,3256625099u,4139196770u};
    for(unsigned i=0;i<3;++i) support[i]=-1;
    uint8_t seen[BONES]={0};
    for(unsigned i=0;i<BONES;++i) {
        int parent=*(int16_t *)(hierarchy+i*8+4);
        int channel=*(int16_t *)(hierarchy+i*8+6);
        /* The retail sampler indexes state[bone.channel], not just state[0].
         * Elco's loaded hierarchy uses channels0/1 for lower/upper body.
         * Reject unsupported channels before making any native sampler call. */
        if(palette[i]>=BONES || seen[palette[i]] || parent>=(int)i || parent < -1 ||
            channel<0 || channel>=SAMPLE_CHANNELS) return FALSE;
        seen[palette[i]]=1;
        order[i]=palette[i]; parents[palette[i]]=parent<0 ? -1:(int)palette[parent];
        if(*(uint32_t *)(hierarchy+i*8)==599974188u) hand=(int)palette[i];
        if(*(uint32_t *)(hierarchy+i*8)==580010193u) {
            if(arm>=0) return FALSE;
            arm=(int)palette[i]; /* original Bound_Right_Upper_Arm */
        }
        if(*(uint32_t *)(hierarchy+i*8)==4257395119u) upper=(int)i;
        for(unsigned j=0;j<3;++j) if(*(uint32_t *)(hierarchy+i*8)==support_hashes[j]) {
            if(support[j]>=0) return FALSE;
            support[j]=(int)palette[i];
        }
        mask[palette[i]]=(int)i==upper || (parent>=0 && mask[palette[parent]]);
    }
    if(upper<0 || arm<0 || !mask[arm] || arm==(int)palette[upper]) return FALSE;
    if(support[0]<0 || support[1]<0 || support[2]<0 || !mask[support[0]] ||
       parents[support[1]]!=support[0] || parents[support[2]]!=support[1]) return FALSE;
    *hand_out=hand;*arm_out=arm;return TRUE;
}
static BOOL hold_storage(void *renderer,void *bank,void *states,unsigned layer,BOOL *recoil) {
    uint8_t *r=renderer,*channels,*blends,*fire;
    if(layer>=4 || *(void **)(r+8)!=bank || *(uint32_t *)(r+0xa0)!=5 ||
        *(uint32_t *)(r+0xa4)!=4) return FALSE;
    channels=*(uint8_t **)(r+0x98);blends=*(uint8_t **)(r+0x9c);
    if(!memory(channels,5*36,FALSE) || !memory(blends,4*20,FALSE) ||
        states!=*(void **)(channels+layer*36) || !memory(states,48,FALSE)) return FALSE;
    /* Exact native locomotion pairs -> combined base -> ranged layer4. */
    const uint16_t links[8]={0,1,2,3,0x8000,0x8001,0x8002,4};
    for(unsigned i=0;i<4;++i) {
        /* Exact retail blend getter at2234e0 reads +0xc; +4 is a bone-channel
         * mask, not a float. Do not confuse mask bits with a blend weight. */
        float w=*(float *)(blends+i*20+12);
        if(memcmp(blends+i*20,links+i*2,4) ||
            *(uint32_t *)(blends+i*20+4)!=(i==3 ? 0xc0000002u:0xffffffffu) ||
            *(uint32_t *)(blends+i*20+8)!=0 || !isfinite(w) || w<0 || w>1) return FALSE;
    }
    for(unsigned ch=0;ch<2;++ch) {
        uint16_t clip=*(uint16_t *)((uint8_t *)states+ch*24);
        if(!(clip>=22 && clip<=25) && !(clip>=58 && clip<=68)) return FALSE;
    }
    fire=*(uint8_t **)(channels+4*36);
    if(!memory(fire,48,FALSE)) return FALSE;
    float w=*(float *)(blends+3*20+12);
    for(unsigned ch=0;ch<2;++ch) {
        uint16_t clip=*(uint16_t *)(fire+ch*24);
        if(clip!=0 && clip!=53) return FALSE;
        if(w>0 && clip!=53) return FALSE;
    }
    *recoil=w>0;return TRUE;
}
static void __attribute__((used,noinline)) hold_base_pose(void *renderer,unsigned layer,
    void *bank,void *states,float *pose) {
    if(layer>=4 || GetCurrentThreadId()!=aim_thread || pose_depth || !aim_fire_witness) return;
    unsigned seat;
    for(seat=0;seat<2;++seat) if(renderer && renderer==world_renderers[seat]) break;
    if(seat==2 || !aim_actors[seat]) return;
    BOOL held=FALSE,recoil=FALSE;float target[3],model_target[3],muzzle[3];
    uint8_t mask[BONES]={0};int parents[BONES],hand,arm,support[3];unsigned order[BONES];
    BOOL admitted=aim_fire_witness(aim_actors[seat],&held) &&
        world_renderer(aim_actors[seat])==renderer &&
        aim_target_witness(aim_actors[seat],target) &&
        proton_geometry(aim_actors[seat],renderer,target,model_target,muzzle) &&
        memory(pose,sizeof(hold_straight),TRUE) &&
        hold_storage(renderer,bank,states,layer,&recoil) &&
        pose_layout(bank,mask,parents,order,&hand,&arm,support);
    float weight=advance_hold(&hold_states[seat],GetTickCount(),admitted,held,recoil);
    if(!admitted || weight<=0) return;
    /* Do not combine a held right arm with an idle left arm/chest. Use one
     * authored upper-body baseline and one release envelope for the pair. */
    if(hand<0 || !mask[hand]) { memset(&hold_states[seat],0,sizeof(HoldState));return; }
    typedef void (__stdcall *Sample)(void *,void *,void *);
    uint8_t straight_states[SAMPLE_CHANNELS][SAMPLE_STATE_BYTES];
    sample_states(straight_states,54,0);
    ++pose_depth;
    ((Sample)(aim_image+POSE_SAMPLE))(bank,straight_states,hold_straight);
    if(blend_hold_base(pose,hold_straight,mask,BONES,weight)) ++held_base_samples;
    --pose_depth;
}
static DWORD __attribute__((naked,noinline,stdcall)) sample_bridge(
    void *bank __attribute__((unused)),void *states __attribute__((unused)),
    void *output __attribute__((unused))) {
    /* Retail loop: ESI=renderer, EDI=layer, stack=(bank,states,output), ret12.
     * Original sampler first; edit its transient base output only. Layer4 and
     * the original blend tree run untouched afterward. Preserve result/flags. */
    __asm__ volatile("pushl %ebp\n\tmovl %esp,%ebp\n\t"
        "pushl 16(%ebp)\n\tpushl 12(%ebp)\n\tpushl 8(%ebp)\n\tcall *_sample_original\n\t"
        "pushfl\n\tpushal\n\tpushl 16(%ebp)\n\tpushl 12(%ebp)\n\tpushl 8(%ebp)\n\t"
        "pushl %edi\n\tpushl %esi\n\tcall _hold_base_pose\n\taddl $20,%esp\n\t"
        "popal\n\tpopfl\n\tleave\n\tret $12\n\t");
}
static float *__attribute__((used,noinline)) select_pose(void **args,void *renderer) {
    float *original=args[2], direction[3], unit[3];
    for(unsigned i=0;i<2;++i) if(renderer && renderer==first_person_renderers[i] && aim_actors[i])
        return first_person_idle_pose(aim_actors[i],renderer,args[0],original);
    uint8_t *bank=args[0],mask[BONES]={0};
    int parents[BONES],hand,arm,support[3];unsigned order[BONES],seat=2;
    void *actor=NULL;
    float phase,amount,yaw,pitch,forward_x,forward_z;
    for(unsigned i=0;i<2;++i) if(renderer && renderer==world_renderers[i]) { actor=aim_actors[i];seat=i; }
    if(!actor) return original;
    InterlockedIncrement(&pose_attempts);
    if(GetCurrentThreadId()!=aim_thread || pose_depth || !aim_witness) {
        InterlockedIncrement(&pose_rejected[0]);return original;
    }
    if(!aim_witness(actor,FALSE,direction) || !SudekiMpLanAimNormalize(direction,unit)) {
        InterlockedIncrement(&pose_rejected[1]);return original;
    }
    if(world_renderer(actor)!=renderer || *(void **)((uint8_t *)renderer+8)!=bank ||
        !memory(original,sizeof(result_pose),FALSE)) {
        InterlockedIncrement(&pose_rejected[2]);return original;
    }
    InterlockedIncrement(&pose_rejected[3]);
    if(!pose_layout(bank,mask,parents,order,&hand,&arm,support)) return original;
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
    amount=fminf(amount,2.0f);
    phase=atan2f(yaw,pitch)*(48.0f/(2*3.14159265358979323846f));
    if(phase<0) phase+=48.0f;
    InterlockedDecrement(&pose_rejected[3]);
    float target[3],model_target[3],muzzle_local[3];
    BOOL precise=hand>=0 && aim_target_witness && aim_target_witness(actor,target) &&
        proton_geometry(actor,renderer,target,model_target,muzzle_local);
    float stance=precise ? current_stance(&hold_states[seat],GetTickCount()):1.f;
    if(precise && stance==0) {
        /* Return the ORIGINAL pose pointer. Even a valid off-centre aim must
         * not rotate the native gun-up idle towards the actor's head. */
        ++idle_pose_passthrough;
        return original;
    }
    typedef void (__stdcall *Sample)(void *,void *,void *);
    uint8_t states[SAMPLE_CHANNELS][SAMPLE_STATE_BYTES];
    ++pose_depth;
    sample_states(states,50,phase);
    ((Sample)(aim_image+POSE_SAMPLE))(bank,states,aimed_pose);
    sample_states(states,54,0);
    ((Sample)(aim_image+POSE_SAMPLE))(bank,states,center_pose);
    memcpy(result_pose,original,sizeof(result_pose));
    BOOL okay;
    if(precise) {
        /* Keep native final recoil; align a separate reference. The support
         * wrist follows the gun-arm correction using its own elbow chain,
         * not by rotating the waist or stretching the arm. The correction
         * AND directional aim fade with the shared stance. Previously only
         * calibration faded; the full FirePose-relative aim was still added
         * to combat idle after release, bringing the gun towards the head. */
        memcpy(reference_pose,original,sizeof(reference_pose));
        copy_firing_pose(reference_pose,center_pose,mask,BONES);
        okay=SudekiMpLanAimOverlay(reference_pose,aimed_pose,center_pose,mask,BONES,amount) &&
            apply_reference(result_pose,reference_pose,center_pose,mask,BONES,stance);
        if(okay) {
            memcpy(uncalibrated_reference,reference_pose,sizeof(reference_pose));
            okay=calibrate_arms(reference_pose,parents,order,BONES,(unsigned)arm,(unsigned)hand,
                (unsigned)support[0],(unsigned)support[1],(unsigned)support[2],muzzle_local,model_target) &&
                SudekiMpLanAimOverlay(result_pose,reference_pose,uncalibrated_reference,mask,BONES,stance);
            if(okay) ++calibrated_poses;
        }
    } else {
        okay=SudekiMpLanAimOverlay(result_pose,aimed_pose,center_pose,mask,BONES,amount);
    }
    --pose_depth;
    if(!okay) { InterlockedIncrement(&pose_rejected[4]);return original; }
    if(++corrected_poses<=3)
        SudekiMpLogFormat("lan_ranged_aim event=pose actor=Elco phase=%.3f amount=%.3f precise=%u calibrated=%lu policy=authored_upper_body_no_channel_restart\r\n",phase,amount,precise,(unsigned long)calibrated_poses);
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
static void __attribute__((used,noinline)) apply_direction(void *manager, const float *muzzle,float *out) {
    uint8_t *m=manager;
    void *actor;
    float direction[3], normalized[3],target[3];
    if (GetCurrentThreadId()!=aim_thread || !aim_witness ||
        !memory(m,0x14,FALSE) || *(void **)m!=aim_image+MISSILE_VT ||
        !memory(out,12,TRUE) || !memory(muzzle,12,FALSE)) return;
    actor=*(void **)(m+0x10);
    if (!actor || (actor!=aim_actors[0] && actor!=aim_actors[1]) ||
        !memory(actor,0xc0,FALSE) || *(void **)((uint8_t *)actor+0xbc)!=m) return;
    if (weapon_shot_observer) weapon_shot_observer(actor);
    if (!aim_witness(actor,TRUE,direction) ||
        !aim_target_witness || !aim_target_witness(actor,target) ||
        !SudekiMpLanAimConverge(muzzle,target,normalized)) return;
    memcpy(out,normalized,12);
    ++corrected_shots;
    if (corrected_shots<=4 || !(corrected_shots%32))
        SudekiMpLogFormat("lan_ranged_aim event=shot_direction count=%lu xyz=%.5f,%.5f,%.5f muzzle=%.5f,%.5f,%.5f target=%.5f,%.5f,%.5f calibrated=%lu policy=actor_scoped_before_native_spread\r\n",
            (unsigned long)corrected_shots,out[0],out[1],out[2],muzzle[0],muzzle[1],muzzle[2],target[0],target[1],target[2],(unsigned long)calibrated_poses);
}
static void __attribute__((naked,noinline)) direction_bridge(void) {
    /* EAX=CMissileManager, stack=(muzzle*,direction*), native ret8.
     * Preserve the original result registers/flags around the observer. */
    __asm__ volatile(
        "pushl %ebp\n\tmovl %esp,%ebp\n\tpushl %ebx\n\tmovl %eax,%ebx\n\t"
        "pushl 12(%ebp)\n\tpushl 8(%ebp)\n\tcall *_direction_original\n\t"
        "pushfl\n\tpushal\n\tpushl 12(%ebp)\n\tpushl 8(%ebp)\n\tpushl %ebx\n\t"
        "call _apply_direction\n\taddl $12,%esp\n\tpopal\n\tpopfl\n\t"
        "popl %ebx\n\tleave\n\tret $8\n\t");
}
BOOL SudekiMpLanAimInstall(HMODULE image, SudekiMpLanAimWitness witness,
    SudekiMpLanAimTargetWitness target_witness, SudekiMpLanAimFireWitness fire_witness) {
    if (!witness || !target_witness || !fire_witness || aim_image || !SudekiMpLanAimImageMatches(image)) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    aim_image=(uint8_t *)image;
    direction_original=aim_image+AIM_DIRECTION;
    aim_witness=witness;
    aim_target_witness=target_witness;
    aim_fire_witness=fire_witness;
    if (!SudekiMpInstallRelativeCallHook(&direction_hook,aim_image+AIM_CALL,
            direction_original,direction_bridge)) {
        aim_image=NULL; aim_witness=NULL; aim_target_witness=NULL; aim_fire_witness=NULL; direction_original=NULL; return FALSE;
    }
    pose_original=aim_image+POSE_BUILD;
    if(!SudekiMpInstallRelativeCallHook(&pose_hook,aim_image+POSE_CALL,
        pose_original,pose_bridge)) {
        DWORD error=GetLastError(); (void)SudekiMpLanAimUninstall();
        SetLastError(error); return FALSE;
    }
    sample_original=aim_image+POSE_SAMPLE;
    if(!SudekiMpInstallRelativeCallHook(&sample_hook,aim_image+BASE_SAMPLE_CALL,
        sample_original,sample_bridge)) {
        DWORD error=GetLastError();(void)SudekiMpLanAimUninstall();
        SetLastError(error);return FALSE;
    }
    return TRUE;
}
BOOL SudekiMpLanAimUninstall(void) {
    if (!SudekiMpRestoreRelativeCallHook(&sample_hook)) return FALSE;
    if (!SudekiMpRestoreRelativeCallHook(&pose_hook)) return FALSE;
    if (!SudekiMpRestoreRelativeCallHook(&direction_hook)) return FALSE;
    aim_image=NULL; direction_original=NULL; aim_witness=NULL; aim_target_witness=NULL;
    idle_witness=NULL;
    aim_fire_witness=NULL;sample_original=NULL;aim_thread=0;
    weapon_shot_observer=NULL;
    memset(aim_actors,0,sizeof(aim_actors)); corrected_shots=0;
    memset(world_renderers,0,sizeof(world_renderers)); pose_original=NULL;
    memset(first_person_renderers,0,sizeof(first_person_renderers));smoothed_fp_idle_poses=0;
    corrected_poses=calibrated_poses=0;
    held_base_samples=idle_pose_passthrough=0;memset(hold_states,0,sizeof(hold_states));
    pose_attempts=0;memset((void *)pose_rejected,0,sizeof(pose_rejected));
    return TRUE;
}
void SudekiMpLanAimActors(void *first, void *second) {
    if (!aim_image) return;
    aim_thread=GetCurrentThreadId();
    void *actors[2]={first,second};
    for(unsigned i=0;i<2;++i) {
        void *renderer=world_renderer(actors[i]);
        if(actors[i]!=aim_actors[i] || renderer!=world_renderers[i])
            memset(&hold_states[i],0,sizeof(hold_states[i]));
        aim_actors[i]=actors[i];world_renderers[i]=renderer;
        first_person_renderers[i]=first_person_renderer(actors[i]);
    }
}
