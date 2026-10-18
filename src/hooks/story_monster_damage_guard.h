#ifndef SUDEKIMP_STORY_MONSTER_DAMAGE_GUARD_H
#define SUDEKIMP_STORY_MONSTER_DAMAGE_GUARD_H

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "hooks/story_spawned_actor.h"

/* Temporary client damage replay, until host combat events replace HP deltas.
 * The caller owns the game-thread and replica-generation proofs. Memory checks
 * must be fresh: DoDirectDamage can remove objects or replace registry storage. */
typedef BOOL (*SudekiMpStoryDamageMemory)(const void *,size_t,BOOL);
typedef struct SudekiMpStoryDamageIdentity {
    uint8_t *entity;
    uint32_t identifier,native_identifier,epoch,generation;
} SudekiMpStoryDamageIdentity;
typedef struct SudekiMpStoryDamageGuard {
    SudekiMpStoryDamageIdentity identity;
    uint8_t *registry,*arbiter,*combat;
    uint32_t state;
    BOOL lifted;
} SudekiMpStoryDamageGuard;
enum { SUDEKIMP_DAMAGE_UNKNOWN=-1, SUDEKIMP_DAMAGE_RETIRED=0, SUDEKIMP_DAMAGE_EXACT=1 };
/* Host HitPoints published for a replicated monster or ally (#54 D1, E2).
 * A failed or non-finite read is unknown, never 0: clients replay only HP
 * drops, so a false 0 would become a near-full hit no later frame undoes.
 * Unknown keeps the HP last published for the same exact actor (no new
 * information, no replay); with none, the caller leaves the actor out of
 * the frame. A real negative reading (overkill) is 0. Pure. */
enum { SUDEKIMP_COMBATANT_HP_PROVEN=0, SUDEKIMP_COMBATANT_HP_HELD=1, SUDEKIMP_COMBATANT_HP_UNKNOWN=2 };
static inline unsigned SudekiMpStoryCombatantHp(BOOL read_ok,float read,BOOL previous_ok,float previous,float *out) {
    if(read_ok && isfinite(read) && read<=100000000.0f) { *out=read>0.0f?read:0.0f; return SUDEKIMP_COMBATANT_HP_PROVEN; }
    if(previous_ok && isfinite(previous) && previous>=0.0f && previous<=100000000.0f) {
        *out=previous; return SUDEKIMP_COMBATANT_HP_HELD;
    }
    *out=0.0f; return SUDEKIMP_COMBATANT_HP_UNKNOWN;
}
/* Test aid (E2, [StoryAreas] TestAllyHealth): the one native hit that leaves
 * hp at `fraction` of its current value, at least 1 HP; 0 when nothing
 * applies. Pure. */
static inline float SudekiMpStoryNonlethalDamage(float hp,float amount);
static inline float SudekiMpStoryTestAllyDamage(float hp,float fraction) {
    if(!isfinite(hp) || !isfinite(fraction) || hp<=1.0f || fraction<=0.0f || fraction>=1.0f) return 0.0f;
    float target=floorf(hp*fraction); if(target<1.0f) target=1.0f;
    /* The binary16 amount may round up: keep the nonlethal reserve. */
    return target<hp?SudekiMpStoryNonlethalDamage(hp,hp-target):0.0f;
}
/* DoDirectDamage's own admission for a positive amount (0x4D3B24..0x4D3B4F,
 * CONFIRMED_STATIC): arbiter+0x50 bit 2, (+0x58 & 0xF) != 3, +0x60 bit 2,
 * and none of 0x211CE50 in +0x50 (idle guards carry 0x800: a native no-op).
 * The bytes are checked so the predicate cannot drift from the image. */
static const uint8_t story_direct_damage_gate[]={0x8b,0x4f,0x50,0xf6,0xc1,0x02,0x76,0x3a,0x8b,0x57,0x58,
    0x83,0xe2,0x0f,0x80,0xfa,0x03,0x74,0x2f,0xf6,0x47,0x60,0x02,0x74,0x29,0xba,0x40,0xc6,0x11,0x02,
    0x84,0xc0,0x75,0x05,0xba,0x50,0xce,0x11,0x02,0x85,0xca,0x77,0x17};
static inline BOOL SudekiMpStoryDirectDamageAdmits(uint8_t *image,const uint8_t *arbiter,SudekiMpStoryDamageMemory memory) {
    if(!image || !memory(image+0xd3b24u,sizeof(story_direct_damage_gate),FALSE) ||
        memcmp(image+0xd3b24u,story_direct_damage_gate,sizeof(story_direct_damage_gate)) ||
        !memory(arbiter,0x64u,FALSE)) return FALSE;
    uint32_t f50=*(const uint32_t *)(arbiter+0x50u),f58=*(const uint32_t *)(arbiter+0x58u);
    return (f50&2u) && (f58&0xfu)!=3u && (arbiter[0x60u]&2u) && !(f50&0x211ce50u);
}
/* The native DamageStructure stores the amount as binary16. Leave more
 * than its rounding error below current HP, including near a killing hit. */
static inline float SudekiMpStoryNonlethalDamage(float hp,float amount) {
    if(!isfinite(hp) || !isfinite(amount) || hp<=0.0f || amount<=0.0f) return 0.0f;
    float reserve=fmaxf(1.0f,hp/512.0f);
    float limit=hp>reserve?hp-reserve:hp*0.5f;
    return fminf(fminf(amount,limit),65504.0f);
}
static inline BOOL SudekiMpStoryDamageArmDisplay(uint8_t *image,uint8_t *entity,
    BOOL host_visible,SudekiMpStoryDamageMemory memory) {
    if(!host_visible || !image || !memory(entity,0xb4u,FALSE) ||
        !SudekiMpStorySpawnedClass(image,*(void **)entity)) return FALSE;
    uint8_t *stat=*(uint8_t **)(entity+0xb0u);
    if(!memory(stat,0x1b4u,TRUE) || *(void **)stat!=image+0x2d21e4u ||
        *(void **)(stat+0x10u)!=entity || !stat[0x24u] || !stat[0x30u]) return FALSE;
    *(float *)(stat+0x18u)=1.0f;
    return TRUE;
}
/* Retail 529360's health/name branch uses state 2 and each widget's
 * native visibility propagation. Its later branch also destroys numbers;
 * keep that branch out of nonlethal client feedback. No arbiter/HP change. */
static const uint8_t story_health_hide_bar_head[]={0x57,0x8b,0xf9,0x8b,0x47,0x1c};
static const uint8_t story_health_hide_label_head[]={0x57,0x8b,0xf9,0x8b,0x47,0x1c};
static inline BOOL SudekiMpStoryHealthHideCode(uint8_t *b,SudekiMpStoryDamageMemory m) {
    return b && m(b+0x2d9038u,4,FALSE) && m(b+0x2d8538u,4,FALSE) &&
        *(void **)(b+0x2d9038u)==b+0x15c2d0u && *(void **)(b+0x2d8538u)==b+0x15c230u &&
        m(b+0x15c2d0u,0x28u,FALSE) &&
        !memcmp(b+0x15c2d0u,story_health_hide_bar_head,sizeof(story_health_hide_bar_head)) && b[0x15c2f7u]==0xc3 &&
        m(b+0x15c230u,0x3eu,FALSE) &&
        !memcmp(b+0x15c230u,story_health_hide_label_head,sizeof(story_health_hide_label_head)) && b[0x15c26du]==0xc3;
}
static inline BOOL SudekiMpStoryDamageHideHealth(uint8_t *b,uint8_t *e,SudekiMpStoryDamageMemory m) {
    if(!SudekiMpStoryHealthHideCode(b,m) || !m(e,0xb4u,FALSE) || !SudekiMpStorySpawnedClass(b,*(void **)e)) return FALSE;
    uint8_t *s=*(uint8_t **)(e+0xb0u);
    if(!m(s,0x1b4u,TRUE) || *(void **)s!=b+0x2d21e4u || *(void **)(s+0x10u)!=e ||
        *(void **)(s+0xd0u)!=b+0x2d9024u || *(void **)(s+0x12cu)!=b+0x2d8524u) return FALSE;
    typedef void (__attribute__((thiscall)) *Visibility)(void *);
    s[0x1cu]=s[0x25u]=0;
    *(uint32_t *)(s+0xd0u+0x1cu)=2;
    ((Visibility)(b+0x15c2d0u))(s+0xd0u);
    *(uint32_t *)(s+0x12cu+0x1cu)=2;
    ((Visibility)(b+0x15c230u))(s+0x12cu);
    return TRUE;
}
enum { SUDEKIMP_RVA_DIRECT_DAMAGE=0xd3ae0u };
typedef void (__attribute__((thiscall)) *SudekiMpStoryDirectDamage)(void *,float);
static const uint8_t story_direct_damage_head[]={0x55,0x8b,0xec,0x83,0xe4,0xf8,0x83,0xec,0x68};
static const uint8_t story_direct_damage_hit_call[]={0xe8,0x6a,0xe6,0xff,0xff};
static const uint8_t story_direct_damage_return[]={0xc2,0x04,0x00};
static inline BOOL SudekiMpStoryDamageCodeExact(uint8_t *image,SudekiMpStoryDamageMemory memory) {
    return image && memory(image+SUDEKIMP_RVA_DIRECT_DAMAGE,0xbcu,FALSE) &&
        !memcmp(image+SUDEKIMP_RVA_DIRECT_DAMAGE,story_direct_damage_head,sizeof(story_direct_damage_head)) &&
        !memcmp(image+0xd3b61u,story_direct_damage_hit_call,sizeof(story_direct_damage_hit_call)) &&
        !memcmp(image+0xd3b99u,story_direct_damage_return,sizeof(story_direct_damage_return));
}
static inline int SudekiMpStoryDamageReprove(uint8_t *image,const SudekiMpStoryDamageGuard *g,
    BOOL generation_matches,SudekiMpStoryDamageMemory memory) {
    if(!generation_matches) return SUDEKIMP_DAMAGE_RETIRED;
    if(!image || !memory(image+0x409d8cu,4u,FALSE)) return SUDEKIMP_DAMAGE_UNKNOWN;
    if(*(void **)(image+0x409d8cu)!=g->registry) return SUDEKIMP_DAMAGE_RETIRED;
    if(!memory(g->registry,0x40u,FALSE)) return SUDEKIMP_DAMAGE_UNKNOWN;
    unsigned count=*(uint32_t *)(g->registry+0x34u);
    uint8_t **entries=*(uint8_t ***)(g->registry+0x3cu);
    if(count>8192u || (count && !memory(entries,count*sizeof(*entries),FALSE))) return SUDEKIMP_DAMAGE_UNKNOWN;
    BOOL present=FALSE;
    for(unsigned i=0;i<count;++i) if(entries[i]==g->identity.entity) { present=TRUE; break; }
    if(!present) return SUDEKIMP_DAMAGE_RETIRED;
    uint8_t *e=g->identity.entity;
    if(!memory(e,0xa8u,FALSE)) return SUDEKIMP_DAMAGE_UNKNOWN;
    if(!SudekiMpStorySpawnedClass(image,*(void **)e) || *(uint32_t *)(e+0x34u)!=g->identity.native_identifier ||
        *(void **)(e+0x90u)!=g->arbiter || *(void **)(e+0xa4u)!=g->combat) return SUDEKIMP_DAMAGE_RETIRED;
    if(!memory(g->arbiter,0x64u,FALSE) || !memory(g->combat,0x14u,FALSE)) return SUDEKIMP_DAMAGE_UNKNOWN;
    if(*(void **)g->arbiter!=image+0x2cc9acu || *(void **)(g->arbiter+0x10u)!=e ||
        *(void **)g->combat!=image+0x2c8754u || *(void **)(g->combat+0x10u)!=e) return SUDEKIMP_DAMAGE_RETIRED;
    return SUDEKIMP_DAMAGE_EXACT;
}
static inline BOOL SudekiMpStoryDamageCapture(uint8_t *image,const SudekiMpStoryDamageIdentity *identity,
    SudekiMpStoryDamageGuard *g,SudekiMpStoryDamageMemory memory) {
    memset(g,0,sizeof(*g));
    if(!SudekiMpStoryDamageCodeExact(image,memory) || !memory(image+0x409d8cu,4u,FALSE) ||
        !memory(identity->entity,0xa8u,FALSE)) return FALSE;
    g->identity=*identity; g->registry=*(uint8_t **)(image+0x409d8cu);
    g->arbiter=*(uint8_t **)(identity->entity+0x90u);
    g->combat=*(uint8_t **)(identity->entity+0xa4u);
    if(SudekiMpStoryDamageReprove(image,g,TRUE,memory)!=SUDEKIMP_DAMAGE_EXACT ||
        !memory(g->arbiter+0x50u,4u,TRUE)) return FALSE;
    g->state=*(uint32_t *)(g->arbiter+0x58u);
    return TRUE;
}
/* Capture and lift are adjacent on the native thread, with no native call in
 * between. Never restore the whole flags word: other bits belong to the hit. */
static inline void SudekiMpStoryDamageLift(SudekiMpStoryDamageGuard *g) {
    g->lifted=(*(uint32_t *)(g->arbiter+0x50u)&0x400u)!=0;
    if(g->lifted) *(uint32_t *)(g->arbiter+0x50u)&=~0x400u;
}
static inline int SudekiMpStoryDamageRestore(uint8_t *image,SudekiMpStoryDamageGuard *g,
    BOOL generation_matches,SudekiMpStoryDamageMemory memory) {
    int proof=SudekiMpStoryDamageReprove(image,g,generation_matches,memory);
    if(proof==SUDEKIMP_DAMAGE_RETIRED) g->lifted=FALSE;
    if(proof!=SUDEKIMP_DAMAGE_EXACT || !g->lifted) return proof;
    if(*(uint32_t *)(g->arbiter+0x58u)==g->state && !(*(uint32_t *)(g->arbiter+0x50u)&0x400u)) {
        if(!memory(g->arbiter+0x50u,4u,TRUE)) return SUDEKIMP_DAMAGE_UNKNOWN;
        *(uint32_t *)(g->arbiter+0x50u)|=0x400u;
    }
    g->lifted=FALSE;
    return SUDEKIMP_DAMAGE_EXACT;
}
#endif
