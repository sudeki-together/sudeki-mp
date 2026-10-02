#define COBJMACROS
#include <d3d9.h>
#include "hooks/lan_party_dummy_overlay.h"
#include "hooks/lan_party_control.h"
#include "cleanroom/engine.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include "engine/weapon_activation_abi.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

enum { SEGMENTS=32, MAX_VERTICES=1024 };
typedef struct Vertex { float x,y,z,rhw; DWORD color; } Vertex;
typedef struct Camera { float matrix[16],fx,fy,near_z,far_z,width,height; } Camera;
static BOOL enabled=TRUE,reported,reported_boundary;
typedef struct Target {
    float body[3],body_radius,body_height;
    float candidate[3],candidate_radius;
    float damage[3],damage_radius;
} Target;
static volatile LONG stopping,busy;
/* Retained on failed Apply; restoration is retried on the render thread even
 * after the gameplay lease expires. Teardown cannot release this dependency. */
static IDirect3DStateBlock9 *pending_state;

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && a+n>=a && VirtualQuery(p,&m,sizeof(m)) &&
        m.State==MEM_COMMIT && !(m.Protect&(PAGE_NOACCESS|PAGE_GUARD)) &&
        a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL vector(const float *v,unsigned n,float limit) {
    for(unsigned i=0;i<n;++i) if(!isfinite(v[i]) || fabsf(v[i])>limit) return FALSE;
    return TRUE;
}
static BOOL supported_image(uint8_t *base) {
    if(!readable(base,sizeof(IMAGE_DOS_HEADER))) return FALSE;
    const IMAGE_DOS_HEADER *dos=(const IMAGE_DOS_HEADER *)base;
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>0x1000) return FALSE;
    const IMAGE_NT_HEADERS32 *nt=(const IMAGE_NT_HEADERS32 *)(base+dos->e_lfanew);
    return readable(nt,sizeof(*nt)) && nt->Signature==IMAGE_NT_SIGNATURE &&
        nt->FileHeader.Machine==IMAGE_FILE_MACHINE_I386 &&
        nt->OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR32_MAGIC &&
        nt->FileHeader.TimeDateStamp==SUDEKIMP_EXPECTED_TIMESTAMP &&
        nt->OptionalHeader.SizeOfImage==SUDEKIMP_EXPECTED_IMAGE_SIZE;
}
BOOL SudekiMpLanPartyDummyOverlayReset(void) {
    if(!SudekiMpLanPartyDummyOverlayDrained()) return FALSE;
    enabled=TRUE; reported=FALSE; reported_boundary=FALSE; InterlockedExchange(&stopping,0); return TRUE;
}
void SudekiMpLanPartyDummyOverlayToggle(void) {
    if(InterlockedCompareExchange(&stopping,0,0)) return;
    enabled=!enabled;
    SudekiMpLogFormat("lan_party_dummy_overlay event=toggle enabled=%u scope=local_display\r\n",enabled);
}
BOOL SudekiMpLanPartyDummyOverlayEnabled(void) { return enabled; }
void SudekiMpLanPartyDummyOverlayRequestStop(void) { InterlockedExchange(&stopping,1); }
BOOL SudekiMpLanPartyDummyOverlayDrained(void) {
    return InterlockedCompareExchange(&busy,0,0)==0;
}
BOOL SudekiMpLanPartyDummyOverlayRestore(void) {
    if(!SudekiMpLanPartyPresentationBoundary()) return FALSE;
    if(pending_state) {
        if(FAILED(IDirect3DStateBlock9_Apply(pending_state))) return FALSE;
        IDirect3DStateBlock9_Release(pending_state); pending_state=NULL;
        InterlockedExchange(&busy,0);
    }
    return TRUE;
}

/* These are three different native geometries, observed without mutation:
 * physical capsule (CMove), candidate sphere, and the final damage sphere.
 * In particular the unscaled damage radius is NOT the capsule radius. */
static BOOL target(uint8_t *base,Target *out) {
    float *center=out->body,*radius=&out->body_radius,*half_height=&out->body_height;
    uint8_t *entity=SudekiMpCleanroomEngineGenericEntity("MON_TrainingDummy");
    uint8_t *position,*collision,*proxy,*registry;
    unsigned matches=0;
    if(!readable(entity,0x84) || *(void **)entity!=base+0x2d5fa0) return FALSE;
    position=*(uint8_t **)(entity+0x44); collision=*(uint8_t **)(entity+0x60);
    if(!readable(position,0x110) || *(void **)position!=base+0x2cdefc ||
        *(void **)(position+0x10)!=entity ||
        !readable(collision,0x84) || *(void **)collision!=base+0x2c85fc ||
        *(void **)(collision+0x10)!=entity || !(*(uint32_t *)(collision+0x2c)&4u)) return FALSE;
    proxy=*(uint8_t **)(position+0x8c);
    if(!readable(proxy,0x110) || *(void **)(proxy+0x100)!=position+4 ||
        !proxy[0xf4] || (*(uint32_t *)(proxy+0x44)&0x50000u)!=0x10000u) return FALSE;
    registry=*(uint8_t **)(base+0x408dd4);
    if(!readable(registry,0x8c)) return FALSE;
    for(unsigned group=0;group<2;++group) {
        unsigned count=*(unsigned *)(registry+0x70+group*0x10);
        void **data=*(void ***)(registry+0x78+group*0x10);
        if(count>8192 || (count && !readable(data,count*sizeof(*data)))) return FALSE;
        for(unsigned i=0;i<count;++i) if(data[i]==proxy) ++matches;
    }
    if(matches!=1) return FALSE;
    const float *offset=(float *)(proxy+0x20),*world=(float *)(proxy+0xa0);
    const float *scale=(float *)(position+0x70),*radii=(float *)(collision+0x78);
    if(!vector(offset,3,1000) || !vector(world,16,1000000) ||
        !vector(scale,3,100) || !vector(radii,3,1000) ||
        scale[0]<=0 || scale[1]<=0 || scale[2]<=0 ||
        fabsf(world[3])+fabsf(world[7])+fabsf(world[11])+fabsf(world[15]-1)>0.001f)
        return FALSE;
    for(unsigned i=0;i<3;++i)
        center[i]=offset[0]*world[i]+offset[1]*world[4+i]+offset[2]*world[8+i]+world[12+i];
    *radius=radii[0]*scale[0]; *half_height=radii[1]*scale[1];
    if(!vector(center,3,1000000) || *radius<=0.001f || *radius>100 ||
        *half_height<*radius || *half_height>100) return FALSE;
    /* Ordinary spherical attacks: 0x136d50 copies unscaled CStats height
     * and radius; 0x1376e0 uses their MAX as a sphere at Position+0x18.
     * A sphere-tree target takes a different path and must not be guessed. */
    uint8_t *stats=*(uint8_t **)(entity+0x4c);
    if(!readable(stats,0x64) || *(void **)stats!=base+0x2cc064 ||
        *(void **)(stats+0x10)!=entity ||
        (*(uint32_t *)(collision+0x2c)&0x10000000u)) return FALSE;
    float h=*(float *)(stats+0x48),r=*(float *)(stats+0x60);
    if(!isfinite(h) || !isfinite(r) || h<=0 || h>100 || r<=0 || r>100) return FALSE;
    memcpy(out->damage,position+0x18,12); out->damage_radius=fmaxf(h,r);
    /* 0x33700 performs a separate candidate test around proxy+0x10. */
    memcpy(out->candidate,proxy+0x10,12); out->candidate_radius=*(float *)(proxy+0xc);
    return vector(out->damage,3,1000000) && vector(out->candidate,3,1000000) &&
        isfinite(out->candidate_radius) && out->candidate_radius>0 &&
        out->candidate_radius<=100 && !(*(uint32_t *)(proxy+0x44)&0x100000u);
}
/* Default pistol/Royal Sceptre only. Other weapons can use boxes, changing
 * phase scales or secondary explosions; never present this as their envelope.
 * 0x1375b0: area bound = radius*phase_scale + length(owner movement).
 * 0x33700: candidate distance < proxy.radius + sqrt(1.5)*area_bound.
 * 0x1376e0: final swept sphere uses max(stats.height,stats.radius)+shot_radius.
 * When the smaller sphere fits wholly inside the final sphere, its surface is
 * the limiting boundary. This is local native geometry, not a host hit claim. */
static BOOL shot_boundary(uint8_t *base,unsigned seat,const Target *t,float *radius,
    float *shot_radius,unsigned *item_id) {
    if(seat!=1u && seat!=3u) return FALSE;
    uint8_t *actor=SudekiMpCleanroomEngineActorEntity(seat==1?
        SUDEKIMP_CLEANROOM_ELCO:SUDEKIMP_CLEANROOM_AILISH);
    SudekiMpElcoWeaponObservation weapon;
    if(!SudekiMpObserveRangedWeapon(actor,seat==1?0x0e:0x01,&weapon) ||
        weapon.item!=(seat==1?24:12) || !readable(actor,0xc8)) return FALSE;
    uint8_t *attacks=*(uint8_t **)(actor+0xc4),*movement=*(uint8_t **)(actor+0x80);
    if(!readable(attacks,0x38) || *(void **)attacks!=base+0x2d4dac ||
        *(void **)(attacks+0x10)!=actor || !readable(movement,0xc0) ||
        *(void **)movement!=base+0x2c8644 || *(void **)(movement+0x10)!=actor ||
        (movement[0xbe]&1)) return FALSE;
    uint8_t *areas=*(uint8_t **)(attacks+0x34);
    if(!readable(areas,0x24) || *(void **)areas!=base+0x2c8a4c) return FALSE;
    unsigned n=*(unsigned *)(areas+0x18); uint8_t **rows=*(uint8_t ***)(areas+0x20);
    if(!n || n>128 || !readable(rows,n*sizeof(*rows))) return FALSE;
    const char *name="XrpdShot";
    /* The default weapons both select XrpdShot; validate the live record
     * rather than assuming every default-looking weapon has this mapping. */
    uint8_t *manager=*(uint8_t **)(actor+0xbc);
    uint8_t *record=*(uint8_t **)(manager+0x60);
    if(!readable(record,0xc4) || *(unsigned *)(record+8)!=weapon.item) return FALSE;
    const char *attack_name=(*(uint32_t *)(record+0x60)&0x80000000u)?
        (char *)(record+0x64):*(char **)(record+0x64);
    if(!readable(attack_name,9) || memcmp(attack_name,name,9)) return FALSE;
    uint8_t *area=NULL; unsigned matches=0;
    for(unsigned i=0;i<n;++i) {
        uint8_t *p=rows[i];
        if(!readable(p,0x38)) return FALSE;
        const char *text=(*(uint32_t *)(p+4)&0x80000000u)?(char *)(p+8):*(char **)(p+8);
        if(!readable(text,9)) return FALSE;
        if(!memcmp(text,name,9)) { area=p; ++matches; }
    }
    if(matches!=1 || *(unsigned *)(area+0x2c)!=1) return FALSE;
    uint8_t *shape=*(uint8_t **)(area+0x24),**phases=*(uint8_t ***)(area+0x34);
    if(!readable(shape,0x24) || *(void **)shape!=base+0x2d4548 ||
        (shape[0x1c]&7)!=1 || !readable(phases,sizeof(*phases)) ||
        !readable(phases[0],0x42) || *(float *)(phases[0]+0x38)!=1.0f) return FALSE;
    float r=*(float *)(shape+0x20),delta[3]; memcpy(delta,movement+0x34,12);
    if(!isfinite(r) || r<=0 || r>10 || !vector(delta,3,10) ||
        *(double *)(base+0x2e3838)!=1.5) return FALSE;
    float candidate=t->candidate_radius+sqrtf(1.5f)*(r+
        sqrtf(delta[0]*delta[0]+delta[1]*delta[1]+delta[2]*delta[2]));
    float distance=0;
    for(unsigned i=0;i<3;++i) { float d=t->candidate[i]-t->damage[i]; distance+=d*d; }
    if(sqrtf(distance)+candidate>t->damage_radius+r) return FALSE;
    *radius=candidate; *shot_radius=r; *item_id=weapon.item;
    return TRUE;
}
static BOOL camera(uint8_t *base,Camera *out) {
    uint8_t *manager=*(uint8_t **)(base+0x409d7c),*mode=*(uint8_t **)(base+0x408da8);
    uint8_t *view,*render;
    if(!readable(manager,0x24) || *(void **)manager!=base+0x2c7b80 || !readable(mode,0x10)) return FALSE;
    view=*(uint8_t **)(manager+0x20);
    if(!readable(view,0x38) || *(void **)view!=base+0x2cce5c || *(void **)(mode+0xc)!=view+0x2c)
        return FALSE;
    render=*(uint8_t **)(view+0x34);
    if(!readable(render,0xdc) || *(void **)render!=base+0x2dd638) return FALSE;
    memcpy(out->matrix,render+0x90,sizeof(out->matrix));
    if(!vector(out->matrix,16,1000000)) return FALSE;
    for(unsigned i=0;i<3;++i) for(unsigned j=i;j<3;++j) {
        float dot=0;
        for(unsigned k=0;k<3;++k) dot+=out->matrix[4*i+k]*out->matrix[4*j+k];
        if(fabsf(dot-(i==j?1.0f:0.0f))>0.01f) return FALSE;
    }
    float fov=*(float *)(render+0xd0),pixel_aspect=*(float *)(base+0x34e9e4);
    out->near_z=*(float *)(render+0xd4); out->far_z=*(float *)(render+0xd8);
    out->width=(float)*(unsigned *)(base+0x34ec9c);
    out->height=(float)*(unsigned *)(base+0x34eca0);
    if(!isfinite(fov) || fov<0.1f || fov>3 || !isfinite(pixel_aspect) ||
        pixel_aspect<0.1f || pixel_aspect>10 || !isfinite(out->near_z) ||
        !isfinite(out->far_z) || out->near_z<=0 || out->far_z<=out->near_z ||
        out->width<64 || out->width>16384 || out->height<64 || out->height>16384) return FALSE;
    /* Retail 0x1d1590 uses a horizontal FOV and this platform aspect branch. */
    float aspect=(*(uintptr_t *)(base+0x323e74)<=(uintptr_t)(base+0x404838) || base[0x3c31c5]) ?
        out->width/out->height:1.0f;
    out->fx=out->width*0.5f/tanf(fov*0.5f)/pixel_aspect;
    out->fy=out->height*0.5f/tanf(fov*0.5f)*aspect;
    return TRUE;
}
static BOOL clip(float p,float q,float *lo,float *hi) {
    if(p==0) return q>=0;
    float t=q/p;
    if(p<0) { if(t>*hi) return FALSE; if(t>*lo) *lo=t; }
    else { if(t<*lo) return FALSE; if(t<*hi) *hi=t; }
    return TRUE;
}
static void line(const Camera *c,const float a[3],const float b[3],Vertex *vertices,unsigned *count,DWORD color) {
    float v[2][3]={{0}},lo=0,hi=1;
    if(*count+2>MAX_VERTICES) return;
    for(unsigned n=0;n<2;++n) for(unsigned axis=0;axis<3;++axis)
        for(unsigned k=0;k<3;++k)
            v[n][axis]+=((n?b:a)[k]-c->matrix[12+k])*c->matrix[4*axis+k];
    float dz=v[1][2]-v[0][2];
    if(!clip(-dz,v[0][2]-c->near_z,&lo,&hi) || !clip(dz,c->far_z-v[0][2],&lo,&hi)) return;
    float x[2],y[2];
    for(unsigned n=0;n<2;++n) {
        float t=n?hi:lo,z=v[0][2]+t*dz;
        x[n]=c->width*0.5f+c->fx*(v[0][0]+t*(v[1][0]-v[0][0]))/z;
        y[n]=c->height*0.5f-c->fy*(v[0][1]+t*(v[1][1]-v[0][1]))/z;
        if(!isfinite(x[n]) || !isfinite(y[n])) return;
    }
    float dx=x[1]-x[0],dy=y[1]-y[0]; lo=0; hi=1;
    if(!clip(-dx,x[0],&lo,&hi) || !clip(dx,c->width-x[0],&lo,&hi) ||
        !clip(-dy,y[0],&lo,&hi) || !clip(dy,c->height-y[0],&lo,&hi)) return;
    vertices[(*count)++]=(Vertex){x[0]+lo*dx-0.5f,y[0]+lo*dy-0.5f,0,1,color};
    vertices[(*count)++]=(Vertex){x[0]+hi*dx-0.5f,y[0]+hi*dy-0.5f,0,1,color};
}
static unsigned wire(const Camera *view,const float center[3],float radius,float height,Vertex *v) {
    const float pi=3.14159265358979323846f,stem=height-radius;
    unsigned count=0;
    for(unsigned plane=0;plane<3;++plane) {
        float direction=pi*(float)plane/3,dx=cosf(direction),dz=sinf(direction);
        for(unsigned cap=0;cap<2;++cap) {
            float last[3];
            for(unsigned i=0;i<=SEGMENTS/2;++i) {
                float a=pi*((float)cap+(float)i/(SEGMENTS/2)),r=radius*cosf(a);
                float p[3]={center[0]+r*dx,center[1]+(cap?-stem:stem)+radius*sinf(a),center[2]+r*dz};
                if(i) line(view,last,p,v,&count,0xff50f5dcu);
                memcpy(last,p,sizeof(last));
            }
            float end[3]={last[0],last[1]+(cap?2*stem:-2*stem),last[2]};
            line(view,last,end,v,&count,0xff50f5dcu);
        }
    }
    for(unsigned ring=0;ring<2;++ring) for(unsigned i=0;i<SEGMENTS;++i) {
        float a=2*pi*(float)i/SEGMENTS,b=2*pi*(float)(i+1)/SEGMENTS;
        float p[3]={center[0]+radius*cosf(a),center[1]+(ring?stem:-stem),center[2]+radius*sinf(a)};
        float q[3]={center[0]+radius*cosf(b),p[1],center[2]+radius*sinf(b)};
        line(view,p,q,v,&count,0xff50f5dcu);
    }
    return count;
}
static void sphere(const Camera *view,const float center[3],float radius,DWORD color,
    Vertex *v,unsigned *count) {
    const float tau=6.2831853071795864769f;
    for(unsigned axis=0;axis<3;++axis) for(unsigned i=0;i<SEGMENTS;++i) {
        float a=tau*i/SEGMENTS,b=tau*(i+1)/SEGMENTS,p[3],q[3];
        memcpy(p,center,sizeof(p)); memcpy(q,center,sizeof(q));
        p[(axis+1)%3]+=radius*cosf(a); p[(axis+2)%3]+=radius*sinf(a);
        q[(axis+1)%3]+=radius*cosf(b); q[(axis+2)%3]+=radius*sinf(b);
        line(view,p,q,v,count,color);
    }
}
void SudekiMpLanPartyDummyOverlayRender(HMODULE module,unsigned seat) {
    uint8_t *base=(uint8_t *)module; Camera view; Target t; float hit_radius=0,shot_radius=0; unsigned item=0;
    Vertex vertices[MAX_VERTICES]; unsigned count;
    IDirect3DDevice9 *device; D3DVIEWPORT9 viewport; HRESULT hr=S_OK;
    if(seat>=4u || !enabled || InterlockedCompareExchange(&stopping,0,0) ||
        !SudekiMpLanPartyPresentationBoundary() || pending_state || !base ||
        !supported_image(base) || !SudekiMpCleanroomEngineWorldReady() ||
        !target(base,&t) || !camera(base,&view)) return;
    count=wire(&view,t.body,t.body_radius,t.body_height,vertices);
    sphere(&view,t.damage,t.damage_radius,0xffff70e0u,vertices,&count);
    BOOL has_boundary=shot_boundary(base,seat,&t,&hit_radius,&shot_radius,&item);
    if(has_boundary) sphere(&view,t.candidate,hit_radius,0xffffd34du,vertices,&count);
    device=*(IDirect3DDevice9 **)(base+0x3c31dc);
    if(!count || !readable(device,sizeof(void *)) ||
        FAILED(IDirect3DDevice9_GetViewport(device,&viewport)) || viewport.X || viewport.Y ||
        viewport.Width!=(DWORD)view.width || viewport.Height!=(DWORD)view.height) return;
    InterlockedExchange(&busy,1);
    if(FAILED(IDirect3DDevice9_CreateStateBlock(device,D3DSBT_ALL,&pending_state)) || !pending_state) {
        InterlockedExchange(&busy,0); return;
    }
#define DRAW_CHECK(call) do { hr=(call); if(FAILED(hr)) goto restore; } while(0)
    DRAW_CHECK(IDirect3DDevice9_SetVertexShader(device,NULL));
    DRAW_CHECK(IDirect3DDevice9_SetPixelShader(device,NULL));
    DRAW_CHECK(IDirect3DDevice9_SetFVF(device,D3DFVF_XYZRHW|D3DFVF_DIFFUSE));
    DRAW_CHECK(IDirect3DDevice9_SetTexture(device,0,NULL));
    DRAW_CHECK(IDirect3DDevice9_SetRenderState(device,D3DRS_ZENABLE,FALSE));
    DRAW_CHECK(IDirect3DDevice9_SetRenderState(device,D3DRS_ZWRITEENABLE,FALSE));
    DRAW_CHECK(IDirect3DDevice9_SetRenderState(device,D3DRS_LIGHTING,FALSE));
    DRAW_CHECK(IDirect3DDevice9_SetRenderState(device,D3DRS_FOGENABLE,FALSE));
    DRAW_CHECK(IDirect3DDevice9_SetRenderState(device,D3DRS_SCISSORTESTENABLE,FALSE));
    DRAW_CHECK(IDirect3DDevice9_SetRenderState(device,D3DRS_STENCILENABLE,FALSE));
    DRAW_CHECK(IDirect3DDevice9_SetRenderState(device,D3DRS_ALPHATESTENABLE,FALSE));
    DRAW_CHECK(IDirect3DDevice9_SetRenderState(device,D3DRS_ALPHABLENDENABLE,FALSE));
    DRAW_CHECK(IDirect3DDevice9_SetRenderState(device,D3DRS_CULLMODE,D3DCULL_NONE));
    DRAW_CHECK(IDirect3DDevice9_SetRenderState(device,D3DRS_COLORWRITEENABLE,15));
    DRAW_CHECK(IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_COLOROP,D3DTOP_SELECTARG1));
    DRAW_CHECK(IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_COLORARG1,D3DTA_DIFFUSE));
    DRAW_CHECK(IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1));
    DRAW_CHECK(IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE));
    DRAW_CHECK(IDirect3DDevice9_SetTextureStageState(device,1,D3DTSS_COLOROP,D3DTOP_DISABLE));
    DRAW_CHECK(IDirect3DDevice9_DrawPrimitiveUP(device,D3DPT_LINELIST,count/2,vertices,sizeof(Vertex)));
restore:
    if(!SudekiMpLanPartyDummyOverlayRestore()) {
        enabled=FALSE;
        SudekiMpLogWrite("lan_party_dummy_overlay event=restore_failed policy=retain_state_retry_render_thread\r\n");
    } else if(SUCCEEDED(hr) && !reported) {
        reported=TRUE;
        SudekiMpLogFormat("lan_party_dummy_overlay event=draw seat=%u center=%.3f,%.3f,%.3f radius=%.3f half_height=%.3f lines=%u key=F9\r\n",
            seat,t.body[0],t.body[1],t.body[2],t.body_radius,t.body_height,count/2);
    }
    if(SUCCEEDED(hr) && !pending_state && has_boundary && !reported_boundary) {
        reported_boundary=TRUE;
        SudekiMpLogFormat("lan_party_dummy_overlay event=damage_geometry seat=%u damage_center=%.3f,%.3f,%.3f damage_radius=%.3f candidate_center=%.3f,%.3f,%.3f candidate_radius=%.3f boundary_valid=%u item=%u shot_radius=%.3f hit_radius=%.3f colors=cyan_body_magenta_damage_gold_default_shot local_geometry=1\r\n",
            seat,t.damage[0],t.damage[1],t.damage[2],t.damage_radius,
            t.candidate[0],t.candidate[1],t.candidate[2],t.candidate_radius,
            has_boundary,item,shot_radius,hit_radius);
    }
#undef DRAW_CHECK
}
