#include "network/lan_story_frame.h"
#include "network/lan_party_motion.h"
#include <math.h>
#include <string.h>

_Static_assert(sizeof(float)==4u,"Story frame requires binary32 native floats");
_Static_assert(SUDEKIMP_LAN_STORY_FRAME_MAX_SIZE+28u<=
    SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE,"Sparse story frame exceeds SMP4 datagram");

static const uint8_t actor_types[4]={SUDEKIMP_LAN_ARENA_BUKI_TYPE,
    SUDEKIMP_LAN_ARENA_ELCO_TYPE,SUDEKIMP_LAN_ARENA_TAL_TYPE,
    SUDEKIMP_LAN_ARENA_AILISH_TYPE};
static const uint8_t weapon_first[4]={36u,24u,0u,12u};

static unsigned count_actors(unsigned mask) {
    unsigned count=0;
    for(unsigned c=0;c<4u;++c) count+=(mask>>c)&1u;
    return count;
}
static BOOL coordinate(float f) {
    return isfinite(f) && f>-1000000.0f && f<1000000.0f;
}
static BOOL empty_shot(const SudekiMpLanStoryShot *s) {
    if(s->sequence || s->host_tick || s->item || s->pre_charge_q8) return FALSE;
    for(unsigned i=0;i<3u;++i) if(s->origin[i]!=0 || s->direction[i]!=0) return FALSE;
    return TRUE;
}
BOOL SudekiMpLanStoryShotsValid(const SudekiMpLanStoryShots *s,unsigned character) {
    if(!s || character>=4u || s->count>SUDEKIMP_LAN_STORY_SHOT_HISTORY ||
        (!s->count && s->latest) || (s->count && character!=1u && character!=3u)) return FALSE;
    for(unsigned i=0;i<SUDEKIMP_LAN_STORY_SHOT_HISTORY;++i) {
        const SudekiMpLanStoryShot *e=&s->events[i];
        if(i>=s->count) { if(!empty_shot(e)) return FALSE; continue; }
        if(!e->sequence || !SudekiMpLanPartyWeaponClip(actor_types[character],e->item) ||
            (i && (e->sequence!=s->events[i-1u].sequence+1u ||
                (int32_t)(e->host_tick-s->events[i-1u].host_tick)<0))) return FALSE;
        float norm=0;
        for(unsigned k=0;k<3u;++k) {
            if(!coordinate(e->origin[k]) || !isfinite(e->direction[k]) ||
                fabsf(e->direction[k])>1.001f) return FALSE;
            norm+=e->direction[k]*e->direction[k];
        }
        if(fabsf(norm-1.0f)>.01f) return FALSE;
    }
    return !s->count || s->latest==s->events[s->count-1u].sequence;
}
BOOL SudekiMpLanStoryShotsAppend(SudekiMpLanStoryShots *s,unsigned character,
    const SudekiMpLanStoryShot *emission) {
    if(!emission || !SudekiMpLanStoryShotsValid(s,character) || s->latest==UINT32_MAX ||
        emission->sequence!=s->latest+1u) return FALSE;
    SudekiMpLanStoryShots next=*s;
    if(next.count==SUDEKIMP_LAN_STORY_SHOT_HISTORY) {
        memmove(next.events,next.events+1,(SUDEKIMP_LAN_STORY_SHOT_HISTORY-1u)*sizeof(*emission));
        --next.count;
    }
    next.events[next.count++]=*emission; next.latest=emission->sequence;
    if(!SudekiMpLanStoryShotsValid(&next,character)) return FALSE;
    *s=next; return TRUE;
}
const SudekiMpLanStoryShot *SudekiMpLanStoryShotNext(const SudekiMpLanStoryShots *s,
    unsigned character,uint32_t tick,uint32_t cursor) {
    if(!SudekiMpLanStoryShotsValid(s,character) || cursor>=s->latest) return NULL;
    for(unsigned i=0;i<s->count;++i) {
        const SudekiMpLanStoryShot *e=&s->events[i];
        int32_t age=(int32_t)(tick-e->host_tick);
        if(e->sequence>cursor && age>=0 && age<=250) return e;
    }
    return NULL;
}
BOOL SudekiMpLanStoryViewGeometryValid(const SudekiMpLanStoryView *v) {
    if(!v || v->valid>1u) return FALSE;
    if(!v->valid) {
        for(unsigned i=0;i<16u;++i) if(v->matrix[i]!=0) return FALSE;
        for(unsigned i=0;i<3u;++i) if(v->projection[i]!=0) return FALSE;
        return TRUE;
    }
    /* Same native matrix/projection bounds as the established render-only
     * Spirit view, without borrowing that feature's sequence namespace. */
    for(unsigned i=0;i<16u;++i) if(!coordinate(v->matrix[i])) return FALSE;
    for(unsigned i=0;i<3u;++i)
        if(!isfinite(v->projection[i]) || v->projection[i]<=0 ||
            v->projection[i]>100000.0f) return FALSE;
    if(v->projection[0]<.1f || v->projection[0]>3.0f ||
        v->projection[2]<=v->projection[1]) return FALSE;
    if(fabsf(v->matrix[3])>.001f || fabsf(v->matrix[7])>.001f ||
        fabsf(v->matrix[11])>.001f || fabsf(v->matrix[15]-1.0f)>.001f) return FALSE;
    for(unsigned i=0;i<3u;++i) for(unsigned j=i;j<3u;++j) {
        float dot=0;
        for(unsigned k=0;k<3u;++k) dot+=v->matrix[4*i+k]*v->matrix[4*j+k];
        if(fabsf(dot-(i==j?1.0f:0.0f))>.02f) return FALSE;
    }
    return TRUE;
}
BOOL SudekiMpLanStoryViewValid(const SudekiMpLanStoryView *v) {
    return SudekiMpLanStoryViewGeometryValid(v) &&
        (v->valid ? v->camera_serial!=0u : v->camera_serial==0u);
}
static BOOL absent_actor(const SudekiMpLanStoryActor *a) {
    if(a->shots.count || a->shots.latest || !SudekiMpLanStoryShotsValid(&a->shots,0u)) return FALSE;
    if(a->generation || a->character || a->animation_state ||
        a->weapon_item_plus_one || a->weapon_visible || a->weapon_attachment[0] ||
        a->weapon_attachment[1] || a->native_pose || a->hp || a->sp ||
        a->x!=0 || a->y!=0 || a->z!=0 || a->facing_x!=0 || a->facing_z!=0 ||
        a->locomotion.valid || a->locomotion.sequence) return FALSE;
    for(unsigned i=0;i<4u;++i)
        if(a->locomotion.clip[i] || a->locomotion.state[i] ||
            a->locomotion.rate[i]!=0 || a->locomotion.time[i]!=0) return FALSE;
    for(unsigned i=0;i<3u;++i) if(a->locomotion.blend[i]!=0) return FALSE;
    return TRUE;
}
BOOL SudekiMpLanStoryActorValid(const SudekiMpLanStoryActor *a,unsigned character) {
    SudekiMpLanPartyMotion description;
    if(!a || character>=4u || a->character!=character || !a->generation ||
        !SudekiMpLanStoryShotsValid(&a->shots,character) ||
        a->hp>SUDEKIMP_LAN_ARENA_MAX_RESOURCE_VALUE ||
        a->sp>SUDEKIMP_LAN_ARENA_MAX_RESOURCE_VALUE ||
        !coordinate(a->x) || !coordinate(a->y) || !coordinate(a->z) ||
        !isfinite(a->facing_x) || !isfinite(a->facing_z) || a->native_pose>1u ||
        a->weapon_visible>1u || (a->weapon_visible && !a->weapon_item_plus_one)) return FALSE;
    if(a->weapon_attachment[0]>2u || a->weapon_attachment[1]>2u ||
        (character!=0u && a->weapon_attachment[1]) ||
        (!a->weapon_item_plus_one && (a->weapon_attachment[0] || a->weapon_attachment[1])) ||
        (a->weapon_visible && (!a->weapon_attachment[0] ||
            (character==0u && !a->weapon_attachment[1])))) return FALSE;
    if(a->weapon_item_plus_one) {
        unsigned item=a->weapon_item_plus_one-1u;
        if(item<weapon_first[character] || item>=weapon_first[character]+12u) return FALSE;
    }
    if(a->native_pose) {
        const SudekiMpLanArenaLocomotion *m=&a->locomotion;
        if(a->animation_state || m->valid || m->sequence) return FALSE;
        for(unsigned i=0;i<4u;++i)
            if(m->clip[i] || m->state[i] || m->rate[i]!=0 || m->time[i]!=0) return FALSE;
        for(unsigned i=0;i<3u;++i) if(m->blend[i]!=0) return FALSE;
    } else {
        if(!SudekiMpLanPartyMotionDescribe(actor_types[character],a->animation_state,&description) ||
            !SudekiMpLanPartyMotionValid(actor_types[character],&a->locomotion)) return FALSE;
        /* Ground movement only in this version. Flight requires its own native
         * ability/presentation lease and cannot be inferred from a body clip. */
        for(unsigned i=0;i<4u;++i) if(a->locomotion.clip[i]>5u) return FALSE;
        if(SudekiMpLanPartyMotionSelector(actor_types[character],a->locomotion.clip[0])!=
            description.primary) return FALSE;
    }
    float norm=sqrtf(a->facing_x*a->facing_x+a->facing_z*a->facing_z);
    return isfinite(norm) && norm>=0.5f && norm<=1.5f;
}
BOOL SudekiMpLanStoryFrameValid(const SudekiMpLanStoryFrame *f) {
    if(!f || !f->epoch || !f->revision || !f->sequence || !SudekiMpLanStoryViewValid(&f->view) ||
        !f->available_mask || (f->available_mask&~15u) || f->leader_character>=4u ||
        !(f->available_mask&(1u<<f->leader_character)) || f->combat_mode>1u) return FALSE;
    for(unsigned c=0;c<4u;++c) {
        if(f->available_mask&(1u<<c)) {
            if(!SudekiMpLanStoryActorValid(&f->actors[c],c) ||
                (f->combat_mode && !f->actors[c].native_pose)) return FALSE;
        } else if(!absent_actor(&f->actors[c])) return FALSE;
        for(unsigned i=0;i<f->actors[c].shots.count;++i)
            if((int32_t)(f->host_tick-f->actors[c].shots.events[i].host_tick)<0) return FALSE;
    }
    return TRUE;
}
BOOL SudekiMpLanStoryFrameMatchesScene(const SudekiMpLanStoryFrame *f,
    const SudekiMpLanStoryScene *s) {
    return SudekiMpLanStoryFrameValid(f) && SudekiMpLanStorySceneValid(s) &&
        s->phase==SUDEKIMP_LAN_STORY_READY && f->epoch==s->epoch &&
        f->revision==s->revision && f->available_mask==s->available_mask &&
        f->leader_character==s->leader_seat;
}

static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0]|((uint16_t)p[1]<<8)); }
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static void put16(uint8_t *p,uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static void put32(uint8_t *p,uint32_t v) {
    for(unsigned i=0;i<4u;++i) p[i]=(uint8_t)(v>>(8u*i));
}
static float get_float(const uint8_t *p) {
    uint32_t bits=get32(p); float f; memcpy(&f,&bits,sizeof(f)); return f;
}
static void put_float(uint8_t *p,float f) {
    uint32_t bits; memcpy(&bits,&f,sizeof(bits)); put32(p,bits);
}
static void write_actor(uint8_t *p,const SudekiMpLanStoryActor *a) {
    const SudekiMpLanArenaLocomotion *m=&a->locomotion;
    p[0]=a->character; p[1]=a->animation_state; p[2]=a->weapon_item_plus_one; p[3]=a->native_pose;
    put32(p+4,a->generation); put32(p+8,a->hp); put32(p+12,a->sp);
    put_float(p+16,a->x); put_float(p+20,a->y); put_float(p+24,a->z);
    put_float(p+28,a->facing_x); put_float(p+32,a->facing_z);
    p[36]=m->valid; put16(p+37,m->sequence);
    memcpy(p+39,m->clip,4); memcpy(p+43,m->state,4);
    for(unsigned i=0;i<4u;++i) {
        put_float(p+47+i*4,m->rate[i]); put_float(p+63+i*4,m->time[i]);
    }
    for(unsigned i=0;i<3u;++i) put_float(p+79+i*4,m->blend[i]);
    p[91]=a->weapon_visible;
    p[92]=a->weapon_attachment[0]; p[93]=a->weapon_attachment[1]; p[94]=p[95]=0;
    p[96]=a->shots.count; p[97]=p[98]=p[99]=0; put32(p+100,a->shots.latest);
    for(unsigned i=0;i<SUDEKIMP_LAN_STORY_SHOT_HISTORY;++i) {
        const SudekiMpLanStoryShot *e=&a->shots.events[i]; uint8_t *q=p+104+i*36u;
        put32(q,e->sequence); put32(q+4,e->host_tick); q[8]=e->item; q[9]=0;
        put16(q+10,e->pre_charge_q8);
        for(unsigned k=0;k<3u;++k) { put_float(q+12+k*4,e->origin[k]); put_float(q+24+k*4,e->direction[k]); }
    }
}
static BOOL read_actor(const uint8_t *p,SudekiMpLanStoryActor *a,unsigned character) {
    SudekiMpLanArenaLocomotion *m=&a->locomotion;
    if(p[0]!=character || p[3]>1u || p[91]>1u || p[94] || p[95] || p[97] || p[98] || p[99]) return FALSE;
    a->character=p[0]; a->animation_state=p[1]; a->weapon_item_plus_one=p[2]; a->native_pose=p[3];
    a->weapon_visible=p[91];
    a->weapon_attachment[0]=p[92]; a->weapon_attachment[1]=p[93];
    a->generation=get32(p+4); a->hp=get32(p+8); a->sp=get32(p+12);
    a->x=get_float(p+16); a->y=get_float(p+20); a->z=get_float(p+24);
    a->facing_x=get_float(p+28); a->facing_z=get_float(p+32);
    m->valid=p[36]; m->sequence=get16(p+37);
    memcpy(m->clip,p+39,4); memcpy(m->state,p+43,4);
    for(unsigned i=0;i<4u;++i) {
        m->rate[i]=get_float(p+47+i*4); m->time[i]=get_float(p+63+i*4);
    }
    for(unsigned i=0;i<3u;++i) m->blend[i]=get_float(p+79+i*4);
    a->shots.count=p[96]; a->shots.latest=get32(p+100);
    for(unsigned i=0;i<SUDEKIMP_LAN_STORY_SHOT_HISTORY;++i) {
        SudekiMpLanStoryShot *e=&a->shots.events[i]; const uint8_t *q=p+104+i*36u;
        if(q[9]) return FALSE;
        e->sequence=get32(q); e->host_tick=get32(q+4); e->item=q[8]; e->pre_charge_q8=get16(q+10);
        for(unsigned k=0;k<3u;++k) { e->origin[k]=get_float(q+12+k*4); e->direction[k]=get_float(q+24+k*4); }
    }
    return SudekiMpLanStoryActorValid(a,character);
}
BOOL SudekiMpLanStoryFrameEncode(const SudekiMpLanStoryFrame *f,
    uint8_t *bytes,size_t capacity,size_t *written) {
    if(written) *written=0;
    if(!bytes || !written || !SudekiMpLanStoryFrameValid(f)) return FALSE;
    unsigned count=count_actors(f->available_mask);
    size_t size=SUDEKIMP_LAN_STORY_FRAME_HEADER_SIZE+count*SUDEKIMP_LAN_STORY_ACTOR_WIRE_SIZE;
    if(capacity<size) return FALSE;
    memset(bytes,0,size);
    bytes[0]='S'; bytes[1]='T'; bytes[2]='F'; bytes[3]=SUDEKIMP_LAN_STORY_FRAME_VERSION;
    put32(bytes+4,f->epoch); put32(bytes+8,f->revision); put32(bytes+12,f->host_tick);
    put32(bytes+16,f->sequence); bytes[20]=f->available_mask; bytes[21]=f->leader_character;
    bytes[22]=(uint8_t)count;
    bytes[23]=f->combat_mode;
    bytes[24]=f->view.valid;
    for(unsigned i=0;i<16u;++i) put_float(bytes+28+4*i,f->view.matrix[i]);
    for(unsigned i=0;i<3u;++i) put_float(bytes+92+4*i,f->view.projection[i]);
    put32(bytes+104,f->view.camera_serial);
    unsigned n=0;
    for(unsigned c=0;c<4u;++c) if(f->available_mask&(1u<<c))
        write_actor(bytes+SUDEKIMP_LAN_STORY_FRAME_HEADER_SIZE+
            n++*SUDEKIMP_LAN_STORY_ACTOR_WIRE_SIZE,&f->actors[c]);
    *written=size; return TRUE;
}
BOOL SudekiMpLanStoryFrameDecode(const uint8_t *bytes,size_t size,
    SudekiMpLanStoryFrame *f) {
    SudekiMpLanStoryFrame next={0};
    if(!bytes || !f || size<SUDEKIMP_LAN_STORY_FRAME_HEADER_SIZE ||
        size>SUDEKIMP_LAN_STORY_FRAME_MAX_SIZE || bytes[0]!='S' || bytes[1]!='T' ||
        bytes[2]!='F' || bytes[3]!=SUDEKIMP_LAN_STORY_FRAME_VERSION || bytes[23]>1u ||
        bytes[25] || bytes[26] || bytes[27] ||
        !bytes[20] || (bytes[20]&~15u) || bytes[22]!=count_actors(bytes[20]) ||
        size!=SUDEKIMP_LAN_STORY_FRAME_HEADER_SIZE+
            bytes[22]*SUDEKIMP_LAN_STORY_ACTOR_WIRE_SIZE) return FALSE;
    next.epoch=get32(bytes+4); next.revision=get32(bytes+8); next.host_tick=get32(bytes+12);
    next.sequence=get32(bytes+16); next.available_mask=bytes[20]; next.leader_character=bytes[21];
    next.combat_mode=bytes[23];
    next.view.valid=bytes[24];
    for(unsigned i=0;i<16u;++i) next.view.matrix[i]=get_float(bytes+28+4*i);
    for(unsigned i=0;i<3u;++i) next.view.projection[i]=get_float(bytes+92+4*i);
    next.view.camera_serial=get32(bytes+104);
    unsigned n=0;
    for(unsigned c=0;c<4u;++c) if(next.available_mask&(1u<<c)) {
        if(!read_actor(bytes+SUDEKIMP_LAN_STORY_FRAME_HEADER_SIZE+
            n++*SUDEKIMP_LAN_STORY_ACTOR_WIRE_SIZE,&next.actors[c],c)) return FALSE;
    }
    if(!SudekiMpLanStoryFrameValid(&next)) return FALSE;
    *f=next; return TRUE;
}

static float lerp(float a,float b,float alpha) { return a+(b-a)*alpha; }
static float basis_dot(const float *a,const float *b) {
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}
static void basis_cross(const float *a,const float *b,float *out) {
    out[0]=a[1]*b[2]-a[2]*b[1];
    out[1]=a[2]*b[0]-a[0]*b[2];
    out[2]=a[0]*b[1]-a[1]*b[0];
}
static BOOL basis_normalize(float *row) {
    float length=sqrtf(basis_dot(row,row));
    if(!isfinite(length) || length<.001f) return FALSE;
    for(unsigned i=0;i<3u;++i) row[i]/=length;
    return TRUE;
}
static void view_interpolate(const SudekiMpLanStoryView *a,
    const SudekiMpLanStoryView *b,uint32_t interval,float alpha,SudekiMpLanStoryView *out) {
    *out=*a;
    /* The host camera serial fences observed camera changes. These bounded
     * continuity guards do not identify every cut inside one camera animation.
     * Missing views, projection changes and discontinuities arrive only at
     * the confirmed endpoint, as do all discrete frame fields. */
    if(!a->valid || !b->valid || a->camera_serial!=b->camera_serial || interval>100u ||
        memcmp(a->projection,b->projection,sizeof(a->projection))) return;
    float distance_squared=0;
    for(unsigned i=0;i<3u;++i) {
        float step=b->matrix[12u+i]-a->matrix[12u+i];
        distance_squared+=step*step;
        if(basis_dot(a->matrix+i*4u,b->matrix+i*4u)<.8660254f) return;
    }
    if(!isfinite(distance_squared) || distance_squared>16.0f) return;
    float right_a[3],right_b[3];
    basis_cross(a->matrix+8u,a->matrix+4u,right_a);
    basis_cross(b->matrix+8u,b->matrix+4u,right_b);
    float hand_a=basis_dot(a->matrix,right_a),hand_b=basis_dot(b->matrix,right_b);
    if(fabsf(hand_a)<.95f || fabsf(hand_b)<.95f || hand_a*hand_b<=0) return;
    SudekiMpLanStoryView value=*a;
    for(unsigned i=0;i<3u;++i) {
        value.matrix[4u+i]=lerp(a->matrix[4u+i],b->matrix[4u+i],alpha);
        value.matrix[8u+i]=lerp(a->matrix[8u+i],b->matrix[8u+i],alpha);
        value.matrix[12u+i]=lerp(a->matrix[12u+i],b->matrix[12u+i],alpha);
    }
    /* Reuse the established Spirit view's Gram-Schmidt construction. Native
     * cameras have a reflected right row (forward cross up); keep the observed
     * handedness instead of introducing a reflection, scale or shear. */
    if(!basis_normalize(value.matrix+8u)) return;
    float dot=basis_dot(value.matrix+4u,value.matrix+8u);
    for(unsigned i=0;i<3u;++i) value.matrix[4u+i]-=dot*value.matrix[8u+i];
    if(!basis_normalize(value.matrix+4u)) return;
    basis_cross(value.matrix+8u,value.matrix+4u,value.matrix);
    if(hand_a<0) for(unsigned i=0;i<3u;++i) value.matrix[i]=-value.matrix[i];
    if(SudekiMpLanStoryViewValid(&value)) *out=value;
}
__attribute__((noinline))
void SudekiMpLanStoryInterpolatePosition(const float before[3],const float after[3],
    uint32_t elapsed,uint32_t span,float position[3]) {
    /* Materialize the fraction and outputs as binary32 on the x86 target.
     * Both record types call this body with the identical integer timeline. */
    volatile float fraction=(float)elapsed/(float)span;
    for(unsigned i=0;i<3u;++i) {
        volatile float value=before[i]+(after[i]-before[i])*fraction;
        position[i]=value;
    }
}
static void actor_interpolate(const SudekiMpLanStoryActor *a,
    const SudekiMpLanStoryActor *b,float alpha,uint32_t elapsed,uint32_t span,
    SudekiMpLanStoryActor *out) {
    *out=*a; /* Discrete resources/weapon/semantic pose never arrive early. */
    const float before[3]={a->x,a->y,a->z},after[3]={b->x,b->y,b->z}; float position[3];
    SudekiMpLanStoryInterpolatePosition(before,after,elapsed,span,position);
    out->x=position[0]; out->y=position[1]; out->z=position[2];
    float x=lerp(a->facing_x,b->facing_x,alpha),z=lerp(a->facing_z,b->facing_z,alpha);
    float norm=sqrtf(x*x+z*z);
    if(norm>0.0001f) { out->facing_x=x/norm; out->facing_z=z/norm; }
    if(a->native_pose) return;
    /* Same rule as the established actor interpolation: loop/clip/state edges
     * retain the preceding channel until the endpoint, never seek backwards. */
    if(a->locomotion.sequence!=b->locomotion.sequence ||
        memcmp(a->locomotion.clip,b->locomotion.clip,sizeof(a->locomotion.clip)) ||
        memcmp(a->locomotion.state,b->locomotion.state,sizeof(a->locomotion.state))) return;
    for(unsigned i=0;i<4u;++i) if(b->locomotion.time[i]<a->locomotion.time[i]) return;
    for(unsigned i=0;i<4u;++i) {
        out->locomotion.rate[i]=lerp(a->locomotion.rate[i],b->locomotion.rate[i],alpha);
        out->locomotion.time[i]=lerp(a->locomotion.time[i],b->locomotion.time[i],alpha);
    }
    for(unsigned i=0;i<3u;++i)
        out->locomotion.blend[i]=lerp(a->locomotion.blend[i],b->locomotion.blend[i],alpha);
}
BOOL SudekiMpLanStoryFrameInterpolate(const SudekiMpLanStoryFrame *a,
    const SudekiMpLanStoryFrame *b,uint32_t tick,SudekiMpLanStoryFrame *out) {
    if(!out || !SudekiMpLanStoryFrameValid(a) || !SudekiMpLanStoryFrameValid(b) ||
        a->epoch!=b->epoch || a->revision!=b->revision ||
        a->available_mask!=b->available_mask || a->leader_character!=b->leader_character ||
        (int32_t)(b->sequence-a->sequence)<=0 || (int32_t)(b->host_tick-a->host_tick)<=0 ||
        (int32_t)(tick-a->host_tick)<0 || (int32_t)(b->host_tick-tick)<0) return FALSE;
    for(unsigned c=0;c<4u;++c) if((a->available_mask&(1u<<c)) &&
        (a->actors[c].generation!=b->actors[c].generation ||
         a->actors[c].native_pose!=b->actors[c].native_pose)) return FALSE;
    if(tick==b->host_tick) { *out=*b; return TRUE; }
    if(tick==a->host_tick) { *out=*a; return TRUE; }
    float alpha=(float)(tick-a->host_tick)/(float)(b->host_tick-a->host_tick);
    SudekiMpLanStoryFrame next=*a; next.host_tick=tick;
    view_interpolate(&a->view,&b->view,b->host_tick-a->host_tick,alpha,&next.view);
    for(unsigned c=0;c<4u;++c) if(a->available_mask&(1u<<c))
        actor_interpolate(&a->actors[c],&b->actors[c],alpha,
            tick-a->host_tick,b->host_tick-a->host_tick,&next.actors[c]);
    if(!SudekiMpLanStoryFrameValid(&next)) return FALSE;
    *out=next; return TRUE;
}
