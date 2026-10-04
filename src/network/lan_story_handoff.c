#include "network/lan_story_handoff.h"
#include <math.h>
#include <string.h>

static void put32(uint8_t *p,uint32_t n) {
    for(unsigned i=0;i<4u;++i) p[i]=(uint8_t)(n>>(8u*i));
}
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static void putfloat(uint8_t *p,float f) { uint32_t n; memcpy(&n,&f,4); put32(p,n); }
static float getfloat(const uint8_t *p) { uint32_t n=get32(p); float f; memcpy(&f,&n,4); return f; }

static BOOL action_slot_valid(unsigned kind,unsigned slot) {
    return (kind==SUDEKIMP_STORY_ACTION_SKILL && slot<6u) ||
        (kind==SUDEKIMP_STORY_ACTION_SPIRIT && slot>=1u && slot<=2u);
}
BOOL SudekiMpLanStoryActionRequestValid(const SudekiMpLanStoryActionRequest *r) {
    return r && SudekiMpLanStoryControlFenceValid(&r->fence) && r->request &&
        r->acknowledged_frame && action_slot_valid(r->kind,r->slot);
}
BOOL SudekiMpLanStoryActionRequestSame(const SudekiMpLanStoryActionRequest *a,
    const SudekiMpLanStoryActionRequest *b) {
    return SudekiMpLanStoryActionRequestValid(a) && SudekiMpLanStoryActionRequestValid(b) &&
        SudekiMpLanStoryControlFenceSame(&a->fence,&b->fence) && a->request==b->request &&
        a->acknowledged_frame==b->acknowledged_frame && a->kind==b->kind && a->slot==b->slot;
}
BOOL SudekiMpLanStoryActionResultValid(const SudekiMpLanStoryActionResult *r) {
    return r && SudekiMpLanStoryControlFenceValid(&r->fence) && r->request &&
        action_slot_valid(r->kind,r->slot) && r->outcome>=SUDEKIMP_STORY_ACTION_STARTED &&
        r->outcome<=SUDEKIMP_STORY_ACTION_RETAINED;
}
BOOL SudekiMpLanStoryActionResultMatches(const SudekiMpLanStoryActionResult *r,
    const SudekiMpLanStoryActionRequest *q) {
    return SudekiMpLanStoryActionResultValid(r) && SudekiMpLanStoryActionRequestValid(q) &&
        SudekiMpLanStoryControlFenceSame(&r->fence,&q->fence) && r->request==q->request &&
        r->kind==q->kind && r->slot==q->slot;
}
BOOL SudekiMpLanStoryActionRequestEncode(const SudekiMpLanStoryActionRequest *r,uint8_t *p,size_t n) {
    if(!p || n!=SUDEKIMP_STORY_ACTION_REQUEST_WIRE_SIZE || !SudekiMpLanStoryActionRequestValid(r) ||
        !SudekiMpLanStoryControlFenceEncode(&r->fence,p,SUDEKIMP_STORY_CONTROL_FENCE_SIZE)) return FALSE;
    put32(p+20,r->request); put32(p+24,r->acknowledged_frame);
    p[28]=r->kind; p[29]=r->slot; p[30]=p[31]=0; return TRUE;
}
BOOL SudekiMpLanStoryActionRequestDecode(const uint8_t *p,size_t n,SudekiMpLanStoryActionRequest *r) {
    SudekiMpLanStoryActionRequest next={0};
    if(!p || !r || n!=SUDEKIMP_STORY_ACTION_REQUEST_WIRE_SIZE || p[30] || p[31] ||
        !SudekiMpLanStoryControlFenceDecode(p,SUDEKIMP_STORY_CONTROL_FENCE_SIZE,&next.fence)) return FALSE;
    next.request=get32(p+20); next.acknowledged_frame=get32(p+24); next.kind=p[28]; next.slot=p[29];
    if(!SudekiMpLanStoryActionRequestValid(&next)) return FALSE;
    *r=next; return TRUE;
}
BOOL SudekiMpLanStoryActionResultEncode(const SudekiMpLanStoryActionResult *r,uint8_t *p,size_t n) {
    if(!p || n!=SUDEKIMP_STORY_ACTION_RESULT_WIRE_SIZE || !SudekiMpLanStoryActionResultValid(r) ||
        !SudekiMpLanStoryControlFenceEncode(&r->fence,p,SUDEKIMP_STORY_CONTROL_FENCE_SIZE)) return FALSE;
    put32(p+20,r->request); put32(p+24,r->observed_tick);
    p[28]=r->kind; p[29]=r->slot; p[30]=r->outcome; p[31]=0; return TRUE;
}
BOOL SudekiMpLanStoryActionResultDecode(const uint8_t *p,size_t n,SudekiMpLanStoryActionResult *r) {
    SudekiMpLanStoryActionResult next={0};
    if(!p || !r || n!=SUDEKIMP_STORY_ACTION_RESULT_WIRE_SIZE || p[31] ||
        !SudekiMpLanStoryControlFenceDecode(p,SUDEKIMP_STORY_CONTROL_FENCE_SIZE,&next.fence)) return FALSE;
    next.request=get32(p+20); next.observed_tick=get32(p+24);
    next.kind=p[28]; next.slot=p[29]; next.outcome=p[30];
    if(!SudekiMpLanStoryActionResultValid(&next)) return FALSE;
    *r=next; return TRUE;
}

BOOL SudekiMpLanStoryRecruitmentValid(const SudekiMpLanStoryRecruitment *r) {
    return r && r->route==1u && r->transaction && r->before_epoch && r->before_revision &&
        r->after_epoch>r->before_epoch && r->after_revision>r->before_revision && r->actor_generation;
}
BOOL SudekiMpLanStoryRecruitmentSame(const SudekiMpLanStoryRecruitment *a,
    const SudekiMpLanStoryRecruitment *b) {
    return SudekiMpLanStoryRecruitmentValid(a) && SudekiMpLanStoryRecruitmentValid(b) &&
        a->route==b->route && a->transaction==b->transaction &&
        a->before_epoch==b->before_epoch && a->before_revision==b->before_revision &&
        a->after_epoch==b->after_epoch && a->after_revision==b->after_revision &&
        a->actor_generation==b->actor_generation;
}
BOOL SudekiMpLanStoryRecruitmentMatchesScene(const SudekiMpLanStoryRecruitment *r,
    const SudekiMpLanStoryScene *s) {
    return SudekiMpLanStoryRecruitmentValid(r) && SudekiMpLanStorySceneValid(s) &&
        s->phase==SUDEKIMP_LAN_STORY_READY && s->available_mask==12u && (s->leader_seat==2u || s->leader_seat==3u) &&
        s->epoch==r->after_epoch && s->revision>=r->after_revision;
}
BOOL SudekiMpLanStoryRecruitmentEncode(const SudekiMpLanStoryRecruitment *r,uint8_t *p,size_t n) {
    if(!p || n!=SUDEKIMP_STORY_RECRUITMENT_WIRE_SIZE || !SudekiMpLanStoryRecruitmentValid(r)) return FALSE;
    put32(p,r->route); put32(p+4,r->transaction); put32(p+8,r->before_epoch); put32(p+12,r->before_revision);
    put32(p+16,r->after_epoch); put32(p+20,r->after_revision); put32(p+24,r->actor_generation);
    put32(p+28,r->observed_tick); return TRUE;
}
BOOL SudekiMpLanStoryRecruitmentDecode(const uint8_t *p,size_t n,SudekiMpLanStoryRecruitment *r) {
    if(!p || !r || n!=SUDEKIMP_STORY_RECRUITMENT_WIRE_SIZE) return FALSE;
    SudekiMpLanStoryRecruitment v={get32(p),get32(p+4),get32(p+8),get32(p+12),
        get32(p+16),get32(p+20),get32(p+24),get32(p+28)};
    if(!SudekiMpLanStoryRecruitmentValid(&v)) return FALSE;
    *r=v; return TRUE;
}

BOOL SudekiMpLanStoryControlFenceValid(const SudekiMpLanStoryControlFence *f) {
    return f && f->epoch && f->revision && f->transaction && f->actor_generation &&
        f->player>0u && f->player<4u && f->character<4u;
}
BOOL SudekiMpLanStoryControlFenceSame(const SudekiMpLanStoryControlFence *a,
    const SudekiMpLanStoryControlFence *b) {
    return SudekiMpLanStoryControlFenceValid(a) && SudekiMpLanStoryControlFenceValid(b) &&
        a->epoch==b->epoch && a->revision==b->revision && a->transaction==b->transaction &&
        a->actor_generation==b->actor_generation && a->player==b->player && a->character==b->character;
}
BOOL SudekiMpLanStoryControlMatchesScene(const SudekiMpLanStoryControlFence *f,
    const SudekiMpLanStoryScene *s) {
    return SudekiMpLanStoryControlFenceValid(f) && SudekiMpLanStorySceneValid(s) &&
        s->phase==SUDEKIMP_LAN_STORY_READY && f->epoch==s->epoch && f->revision==s->revision &&
        (s->available_mask&(1u<<f->character));
}
BOOL SudekiMpLanStoryControlStateValid(const SudekiMpLanStoryControlState *s) {
    return s && SudekiMpLanStoryControlFenceValid(&s->fence) &&
        s->phase>=SUDEKIMP_STORY_CONTROL_PREPARE && s->phase<=SUDEKIMP_STORY_CONTROL_REVOKED;
}
BOOL SudekiMpLanStoryMovementValid(const SudekiMpLanStoryMovement *i) {
    return i && SudekiMpLanStoryControlFenceValid(&i->fence) && i->sequence &&
        i->acknowledged_frame && isfinite(i->world_x) && isfinite(i->world_z) &&
        fabsf(i->world_x)<=1.0f && fabsf(i->world_z)<=1.0f &&
        i->world_x*i->world_x+i->world_z*i->world_z<=1.001f;
}
BOOL SudekiMpLanStoryControlFenceEncode(const SudekiMpLanStoryControlFence *f,uint8_t *p,size_t n) {
    if(!p || n!=SUDEKIMP_STORY_CONTROL_FENCE_SIZE || !SudekiMpLanStoryControlFenceValid(f)) return FALSE;
    memset(p,0,n); put32(p,f->epoch); put32(p+4,f->revision);
    put32(p+8,f->transaction); put32(p+12,f->actor_generation);
    p[16]=f->player; p[17]=f->character; return TRUE;
}
BOOL SudekiMpLanStoryControlFenceDecode(const uint8_t *p,size_t n,SudekiMpLanStoryControlFence *f) {
    if(!p || !f || n!=SUDEKIMP_STORY_CONTROL_FENCE_SIZE || p[18] || p[19]) return FALSE;
    SudekiMpLanStoryControlFence v={get32(p),get32(p+4),get32(p+8),get32(p+12),p[16],p[17]};
    if(!SudekiMpLanStoryControlFenceValid(&v)) return FALSE;
    *f=v; return TRUE;
}
BOOL SudekiMpLanStoryControlEncode(const SudekiMpLanStoryControlState *s,uint8_t *p,size_t n) {
    if(!p || n!=SUDEKIMP_STORY_CONTROL_WIRE_SIZE || !SudekiMpLanStoryControlStateValid(s)) return FALSE;
    if(!SudekiMpLanStoryControlFenceEncode(&s->fence,p,SUDEKIMP_STORY_CONTROL_FENCE_SIZE)) return FALSE;
    p[18]=s->phase; put32(p+20,s->observed_tick); return TRUE;
}
BOOL SudekiMpLanStoryControlDecode(const uint8_t *p,size_t n,SudekiMpLanStoryControlState *s) {
    if(!p || !s || n!=SUDEKIMP_STORY_CONTROL_WIRE_SIZE || p[19]) return FALSE;
    uint8_t fence[SUDEKIMP_STORY_CONTROL_FENCE_SIZE]; memcpy(fence,p,sizeof(fence)); fence[18]=0;
    SudekiMpLanStoryControlState v={0};
    if(!SudekiMpLanStoryControlFenceDecode(fence,sizeof(fence),&v.fence)) return FALSE;
    v.phase=p[18]; v.observed_tick=get32(p+20);
    if(!SudekiMpLanStoryControlStateValid(&v)) return FALSE;
    *s=v; return TRUE;
}
BOOL SudekiMpLanStoryMovementEncode(const SudekiMpLanStoryMovement *i,uint8_t *p,size_t n) {
    if(!p || n!=SUDEKIMP_STORY_MOVEMENT_WIRE_SIZE || !SudekiMpLanStoryMovementValid(i)) return FALSE;
    if(!SudekiMpLanStoryControlFenceEncode(&i->fence,p,SUDEKIMP_STORY_CONTROL_FENCE_SIZE)) return FALSE;
    put32(p+20,i->sequence); put32(p+24,i->acknowledged_frame);
    putfloat(p+28,i->world_x); putfloat(p+32,i->world_z); return TRUE;
}
BOOL SudekiMpLanStoryMovementDecode(const uint8_t *p,size_t n,SudekiMpLanStoryMovement *i) {
    if(!p || !i || n!=SUDEKIMP_STORY_MOVEMENT_WIRE_SIZE) return FALSE;
    SudekiMpLanStoryMovement v={0};
    if(!SudekiMpLanStoryControlFenceDecode(p,SUDEKIMP_STORY_CONTROL_FENCE_SIZE,&v.fence)) return FALSE;
    v.sequence=get32(p+20); v.acknowledged_frame=get32(p+24);
    v.world_x=getfloat(p+28); v.world_z=getfloat(p+32);
    if(!SudekiMpLanStoryMovementValid(&v)) return FALSE;
    *i=v; return TRUE;
}
