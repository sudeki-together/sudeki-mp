#include "network/lan_party_motion.h"
#include "network/lan_arena_tal_combo_graph.h"
#include <string.h>

typedef struct Bank { uint8_t type; int idle,move,secondary,variant[2];
    float rate[2]; BOOL ranged; } Bank;
static const Bank banks[4]={
    {SUDEKIMP_LAN_ARENA_BUKI_TYPE,1,6,7,{4,3},{37.17093f,30.97577f},FALSE},
    {SUDEKIMP_LAN_ARENA_ELCO_TYPE,1,5,6,{2,3},{34.33f,28.608f},TRUE},
    {SUDEKIMP_LAN_ARENA_TAL_TYPE,4,8,9,{10,11},{37.17093f,30.97577f},FALSE},
    {SUDEKIMP_LAN_ARENA_AILISH_TYPE,1,7,8,{4,5},{41.22882f,30.92161f},TRUE}
};
/* SMP4 v4: retain Tal's incoming AND outgoing native combat channels. The
 * action selectors come from the established Tal combo graph; 21 is its
 * native block hold and 3 is the already validated weapon-draw transition.
 * These are Tal's bank, never Buki's same-numbered clip identities. */
static const int tal_combat_selectors[]={
    0,17,36,32,50,52,51,53,62,54,60,61,63,65,68,69,70,71,20,21,3,22,30,31,29,38
};
_Static_assert(sizeof(tal_combat_selectors)/sizeof(tal_combat_selectors[0])==
    SUDEKIMP_LAN_ARENA_PARTY_TAL_MOTION_MAX+1u,"SMP4 Tal codec/motion namespace mismatch");
/* SMP4 only. Exact loaded Tal semantics 6b..6f map to these own-bank
 * selectors, including the outgoing release and all three native dodges.
 * v7 adds semantic82 -> selector38 (resource68d6a3a3, 20 frames), including
 * every outgoing blend channel. Buki's running attack is selector69. */
BOOL SudekiMpLanPartyTalActionToPresentation(uint8_t variant,int *selector,int *state) {
    if(!selector || !state) return FALSE;
    switch(variant) {
    case SUDEKIMP_LAN_ARENA_ACTION_RUNNING_ATTACK:*selector=38; break;
    case SUDEKIMP_LAN_ARENA_ACTION_BLOCK_HOLD:*selector=21; *state=0; return TRUE;
    case SUDEKIMP_LAN_ARENA_ACTION_BLOCK_RELEASE:*selector=22; break;
    case SUDEKIMP_LAN_ARENA_ACTION_ROLL_LEFT:*selector=30; break;
    case SUDEKIMP_LAN_ARENA_ACTION_ROLL_RIGHT:*selector=31; break;
    case SUDEKIMP_LAN_ARENA_ACTION_BACKFLIP:*selector=29; break;
    default:return SudekiMpLanArenaTalActionToNativePresentation(variant,selector,state);
    }
    *state=1; return TRUE;
}
BOOL SudekiMpLanPartyTalActionObserve(int selector,uint8_t state,uint8_t *variant) {
    if(!variant) return FALSE;
    if(state==0u || state==1u || state==65u || state==128u) {
        switch(selector) {
        case 38:*variant=SUDEKIMP_LAN_ARENA_ACTION_RUNNING_ATTACK; return TRUE;
        case 21:*variant=SUDEKIMP_LAN_ARENA_ACTION_BLOCK_HOLD; return TRUE;
        case 22:*variant=SUDEKIMP_LAN_ARENA_ACTION_BLOCK_RELEASE; return TRUE;
        case 30:*variant=SUDEKIMP_LAN_ARENA_ACTION_ROLL_LEFT; return TRUE;
        case 31:*variant=SUDEKIMP_LAN_ARENA_ACTION_ROLL_RIGHT; return TRUE;
        case 29:*variant=SUDEKIMP_LAN_ARENA_ACTION_BACKFLIP; return TRUE;
        }
    }
    return SudekiMpLanArenaTalActionFromNativePresentation(selector,state,variant);
}
static const Bank *bank_for(uint8_t type) {
    for(unsigned i=0;i<4;++i) if(banks[i].type==type) return &banks[i];
    return NULL;
}
static BOOL combat_bank(uint8_t type,int *idle,int *move,int *secondary) {
    if(!idle || !move || !secondary) return FALSE;
    switch(type) {
    case SUDEKIMP_LAN_ARENA_BUKI_TYPE:
        *idle=20; *move=23; *secondary=24; return TRUE;
    case SUDEKIMP_LAN_ARENA_ELCO_TYPE:
        *idle=22; *move=24; *secondary=25; return TRUE;
    case SUDEKIMP_LAN_ARENA_TAL_TYPE:
        *idle=17; *move=36; *secondary=32; return TRUE;
    case SUDEKIMP_LAN_ARENA_AILISH_TYPE:
        *idle=20; *move=22; *secondary=23; return TRUE;
    default:return FALSE;
    }
}
int SudekiMpLanPartyCombatMotionSelector(uint8_t type,unsigned clip) {
    int idle,move,secondary;
    if(!combat_bank(type,&idle,&move,&secondary)) return -1;
    if(type==SUDEKIMP_LAN_ARENA_TAL_TYPE) {
        return clip<sizeof(tal_combat_selectors)/sizeof(tal_combat_selectors[0])?
            tal_combat_selectors[clip]:-1;
    }
    return SudekiMpLanArenaLocomotionSelector(clip,
        type==SUDEKIMP_LAN_ARENA_BUKI_TYPE?2u:
        type==SUDEKIMP_LAN_ARENA_ELCO_TYPE?1u:0u);
}
BOOL SudekiMpLanPartyCombatMotionValid(uint8_t type,
    const SudekiMpLanArenaLocomotion *m) {
    if(!m || !m->valid || !SudekiMpLanArenaLocomotionValid(m)) return FALSE;
    for(unsigned i=0;i<4;++i)
        if(SudekiMpLanPartyCombatMotionSelector(type,m->clip[i])<0) return FALSE;
    return TRUE;
}
BOOL SudekiMpLanPartyCombatMotionCapture(uint8_t type,const int selectors[4],
    const uint8_t states[4],const float rates[4],const float times[4],
    const float blends[3],const SudekiMpLanArenaLocomotion *previous,
    SudekiMpLanArenaLocomotion *out) {
    SudekiMpLanArenaLocomotion m; BOOL edge;
    if(!selectors || !states || !rates || !times || !blends || !out) return FALSE;
    memset(&m,0,sizeof(m)); m.valid=1; m.sequence=1;
    for(unsigned i=0;i<4;++i) {
        unsigned clip;
        unsigned limit=type==SUDEKIMP_LAN_ARENA_BUKI_TYPE?29u:
            type==SUDEKIMP_LAN_ARENA_TAL_TYPE?
                sizeof(tal_combat_selectors)/sizeof(tal_combat_selectors[0]):10u;
        for(clip=0;clip<limit;++clip)
            if(SudekiMpLanPartyCombatMotionSelector(type,clip)==selectors[i]) break;
        if(clip==limit) return FALSE;
        m.clip[i]=(uint8_t)clip; m.state[i]=states[i];
        if(clip) { m.rate[i]=rates[i]; m.time[i]=times[i]; }
    }
    memcpy(m.blend,blends,sizeof(m.blend));
    if(!SudekiMpLanPartyCombatMotionValid(type,&m)) return FALSE;
    edge=!previous || !SudekiMpLanPartyCombatMotionValid(type,previous);
    if(!edge) {
        edge=memcmp(m.clip,previous->clip,sizeof(m.clip)) ||
            memcmp(m.state,previous->state,sizeof(m.state));
        for(unsigned i=0;i<4;++i) if(m.time[i]<previous->time[i]) edge=TRUE;
        m.sequence=previous->sequence;
        if(edge && ++m.sequence==0) ++m.sequence;
    }
    *out=m; return TRUE;
}
int SudekiMpLanPartyRangedClipSelector(uint8_t type,unsigned clip) {
    if(type!=SUDEKIMP_LAN_ARENA_ELCO_TYPE && type!=SUDEKIMP_LAN_ARENA_AILISH_TYPE)
        return -1;
    if(!clip) return 0;
    if(clip==1u) return SudekiMpLanArenaRangedCombatSelector(type,0x85u);
    /* Exact loaded Elco definitions: 85/86/87 -> 53/52/55 (4/12/24
     * frames). Selector54 is the straight aim pose, NOT the medium shot. */
    if(type==SUDEKIMP_LAN_ARENA_ELCO_TYPE) {
        if(clip==2u) return 52;
        if(clip==3u) return 55;
    } else {
        /* Exact loaded Ailish definitions: 85/86/87 -> 59/58/55,
         * with four/twelve/twenty-six authored frames. SMP4 v5 only. */
        if(clip==2u) return 58;
        if(clip==3u) return 55;
    }
    return -1;
}
int SudekiMpLanPartyRangedClip(uint8_t type,int selector) {
    for(unsigned i=0;i<4u;++i) {
        int expected=SudekiMpLanPartyRangedClipSelector(type,i);
        if(expected>=0 && selector==expected) return (int)i;
    }
    return -1;
}
unsigned SudekiMpLanPartyElcoWeaponClip(unsigned item) {
    switch(item) {
    case 24u:case 27u:return 1u;
    case 26u:case 31u:return 2u;
    case 30u:case 34u:case 35u:return 3u;
    default:return 0u;
    }
}
unsigned SudekiMpLanPartyWeaponClip(uint8_t type,unsigned item) {
    if(type==SUDEKIMP_LAN_ARENA_ELCO_TYPE) return SudekiMpLanPartyElcoWeaponClip(item);
    if(type!=SUDEKIMP_LAN_ARENA_AILISH_TYPE) return 0u;
    switch(item) {
    case 12u:return 1u;
    case 14u:case 18u:return 2u;
    case 16u:case 17u:case 20u:case 23u:return 3u;
    default:return 0u;
    }
}
BOOL SudekiMpLanPartyRangedActionObserve(uint8_t type,int selector,
    uint8_t state,uint8_t *action) {
    int clip;
    if(!action || (type!=SUDEKIMP_LAN_ARENA_ELCO_TYPE &&
        type!=SUDEKIMP_LAN_ARENA_AILISH_TYPE)) return FALSE;
    clip=SudekiMpLanPartyRangedClip(type,selector);
    if(clip<0) return FALSE;
    *action=clip && state!=192u ?
        SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE :
        SUDEKIMP_LAN_ARENA_ACTION_NONE;
    return TRUE;
}
BOOL SudekiMpLanPartyActionTerminalObserved(uint8_t type,uint8_t variant,
    int selector,uint8_t state) {
    int expected,expected_state,idle;
    if(type==SUDEKIMP_LAN_ARENA_BUKI_TYPE ||
        type==SUDEKIMP_LAN_ARENA_TAL_TYPE) {
        BOOL mapped=type==SUDEKIMP_LAN_ARENA_BUKI_TYPE ?
            SudekiMpLanArenaBukiActionToNativePresentation(
                variant,&expected,&expected_state) :
            SudekiMpLanPartyTalActionToPresentation(
                variant,&expected,&expected_state);
        idle=SudekiMpLanPartyCombatMotionSelector(type,1u);
        return mapped && (state==128u &&
            (selector==expected || selector==idle));
    }
    if(type==SUDEKIMP_LAN_ARENA_ELCO_TYPE ||
        type==SUDEKIMP_LAN_ARENA_AILISH_TYPE) {
        if(variant!=SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE || state!=192u)
            return FALSE;
        return SudekiMpLanPartyRangedClip(type,selector)>=0;
    }
    return FALSE;
}
BOOL SudekiMpLanPartyRangedActionChannelDrained(uint8_t type,int selector,
    uint8_t state) {
    if((type!=SUDEKIMP_LAN_ARENA_ELCO_TYPE &&
        type!=SUDEKIMP_LAN_ARENA_AILISH_TYPE) || state!=192u) return FALSE;
    return SudekiMpLanPartyRangedClip(type,selector)>=0;
}
unsigned SudekiMpLanPartyMotionChannels(uint8_t type) {
    /* All four world models have two base pairs. Tal/Elco's formerly
     * restricted two-channel observation omitted the outgoing crossfade. */
    return bank_for(type)?4:0;
}
int SudekiMpLanPartyMotionSelector(uint8_t type,unsigned clip) {
    const Bank *b=bank_for(type);
    if(!b) return -1;
    switch(clip) {
    case 0:return 0; case 1:return b->idle; case 2:return b->move;
    case 3:return b->secondary; case 4:return b->variant[0]; case 5:return b->variant[1];
    default: {
        static const int flight[]={124,125,123,119,122,128};
        return type==SUDEKIMP_LAN_ARENA_ELCO_TYPE && clip>=6u && clip<12u ?
            flight[clip-6u] : -1;
    }
    }
}
BOOL SudekiMpLanPartyMotionValid(uint8_t type,const SudekiMpLanArenaLocomotion *m) {
    unsigned count=SudekiMpLanPartyMotionChannels(type);
    if(!count || !m || !m->valid || !SudekiMpLanArenaLocomotionValid(m)) return FALSE;
    for(unsigned i=0;i<4;++i) {
        if(SudekiMpLanPartyMotionSelector(type,m->clip[i])<0 ||
            (i>=count && (m->clip[i] || m->state[i] || m->rate[i] || m->time[i]))) return FALSE;
    }
    return count==4 || (m->blend[1]==0 && m->blend[2]==0);
}
BOOL SudekiMpLanPartyMotionElcoIdleEnded(uint8_t type,
    const SudekiMpLanArenaLocomotion *m) {
    if(type!=SUDEKIMP_LAN_ARENA_ELCO_TYPE || !SudekiMpLanPartyMotionValid(type,m))
        return FALSE;
    for(unsigned i=0;i<4;++i) if(m->clip[i]==4) return FALSE;
    return TRUE;
}
BOOL SudekiMpLanPartyMotionCapture(uint8_t type,const int selectors[4],
    const uint8_t states[4],const float rates[4],const float times[4],
    const float blends[3],const SudekiMpLanArenaLocomotion *previous,
    SudekiMpLanArenaLocomotion *out) {
    unsigned count=SudekiMpLanPartyMotionChannels(type);
    SudekiMpLanArenaLocomotion m; BOOL edge;
    if(!count || !selectors || !states || !rates || !times || !blends || !out) return FALSE;
    memset(&m,0,sizeof(m)); m.valid=1; m.sequence=1;
    for(unsigned i=0;i<count;++i) {
        unsigned clip;
        unsigned limit=type==SUDEKIMP_LAN_ARENA_ELCO_TYPE?12u:6u;
        for(clip=0;clip<limit;++clip)
            if(SudekiMpLanPartyMotionSelector(type,clip)==selectors[i]) break;
        if(clip==limit) return FALSE;
        m.clip[i]=(uint8_t)clip; m.state[i]=states[i];
        if(clip) { m.rate[i]=rates[i]; m.time[i]=times[i]; }
    }
    for(unsigned i=0;i<(count==4?3u:1u);++i) m.blend[i]=blends[i];
    if(!SudekiMpLanPartyMotionValid(type,&m)) return FALSE;
    edge=!previous || !SudekiMpLanPartyMotionValid(type,previous);
    if(!edge) {
        edge=memcmp(m.clip,previous->clip,sizeof(m.clip)) ||
            memcmp(m.state,previous->state,sizeof(m.state));
        for(unsigned i=0;i<count;++i) if(m.time[i]<previous->time[i]) edge=TRUE;
        m.sequence=previous->sequence;
        if(edge && ++m.sequence==0) ++m.sequence;
    }
    *out=m; return TRUE;
}
BOOL SudekiMpLanPartyMotionDescribe(uint8_t type,uint8_t animation,
    SudekiMpLanPartyMotion *out) {
    const Bank *bank=NULL; SudekiMpLanPartyMotion m;
    if (!out) return FALSE;
    for(unsigned i=0;i<4;++i) if(banks[i].type==type) bank=&banks[i];
    if (!bank) return FALSE;
    memset(&m,0,sizeof(m)); m.primary=bank->idle; m.state=128;
    m.primary_rate=12; m.ranged=bank->ranged;
    switch(animation) {
    case SUDEKIMP_LAN_ARENA_ANIMATION_IDLE: break;
    case SUDEKIMP_LAN_ARENA_ANIMATION_MOVING:
        m.moving=TRUE; m.primary=bank->move; m.secondary=bank->secondary;
        m.state=0; m.primary_rate=bank->rate[0]; m.secondary_rate=bank->rate[1]; break;
    case SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_ONE:
        m.primary=bank->variant[0]; m.state=1; m.primary_rate=24; break;
    case SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_TWO:
        m.primary=bank->variant[1]; m.state=1; m.primary_rate=24; break;
    default: return FALSE;
    }
    *out=m; return TRUE;
}
BOOL SudekiMpLanPartyMotionObserve(uint8_t type,int primary,uint8_t *out) {
    static const uint8_t animations[]={SUDEKIMP_LAN_ARENA_ANIMATION_IDLE,
        SUDEKIMP_LAN_ARENA_ANIMATION_MOVING,SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_ONE,
        SUDEKIMP_LAN_ARENA_ANIMATION_IDLE_VARIANT_TWO};
    if(!out) return FALSE;
    for(unsigned i=0;i<4;++i) {
        SudekiMpLanPartyMotion m;
        if(SudekiMpLanPartyMotionDescribe(type,animations[i],&m) && primary==m.primary) {
            *out=animations[i]; return TRUE;
        }
    }
    if(type==SUDEKIMP_LAN_ARENA_ELCO_TYPE) {
        for(unsigned clip=6u;clip<12u;++clip)
            if(primary==SudekiMpLanPartyMotionSelector(type,clip)) {
                /* Broad locomotion category; the four closed native channels
                 * below retain the exact takeoff/hover/fall/landing result. */
                *out=SUDEKIMP_LAN_ARENA_ANIMATION_MOVING; return TRUE;
            }
    }
    return FALSE; /* Unknown is not idle. */
}
BOOL SudekiMpLanPartyMovementFrameValid(const SudekiMpLanPartyFrame *frame) {
    if(!SudekiMpLanPartyFrameValid(frame)) return FALSE;
    for(unsigned c=0;c<2;++c) {
        const SudekiMpLanArenaSnapshot *s=&frame->chunk[c];
        if(s->combat_enabled || s->enemy_count>1u ||
            s->spirit_audio_history_count) return FALSE;
        /* The host-owned room fixture exists before anyone draws a weapon.
         * Noncombat frames may carry only this dummy, without damage events. */
        if(s->enemy_count && (c!=0u ||
            s->enemies[0].native_entity_id!=SUDEKIMP_LAN_ARENA_TRAINING_DUMMY_ID ||
            s->enemies[0].hit_count || s->enemies[0].feedback_generation)) return FALSE;
        for(unsigned a=0;a<2;++a) {
            const SudekiMpLanArenaActorSnapshot *actor=&s->seat[a]; SudekiMpLanPartyMotion m;
            if(actor->skill_active || actor->skill_sequence || actor->action_sequence ||
                actor->combat_state || actor->ranged_aim_valid || actor->ranged_target_valid ||
                s->cast[a].spirit_view.kind || s->cast[a].skill_fade.kind ||
                actor->action_history_count || !SudekiMpLanPartyMotionValid(actor->actor_type,&actor->locomotion) || actor->weapon.valid ||
                !SudekiMpLanPartyMotionDescribe(actor->actor_type,actor->animation_state,&m)) return FALSE;
        }
    }
    return TRUE;
}
BOOL SudekiMpLanPartyBasicCombatFrameValid(const SudekiMpLanPartyFrame *frame) {
    if(!SudekiMpLanPartyFrameValid(frame) ||
        !frame->chunk[0].combat_enabled || !frame->chunk[1].combat_enabled ||
        frame->chunk[0].match_state!=SUDEKIMP_LAN_ARENA_MATCH_ACTIVE ||
        frame->chunk[0].enemy_count!=1u ||
        frame->chunk[0].enemies[0].native_entity_id!=
            SUDEKIMP_LAN_ARENA_TRAINING_DUMMY_ID) return FALSE;
    for(unsigned c=0;c<2;++c) {
        const SudekiMpLanArenaSnapshot *s=&frame->chunk[c];
        if(s->spirit_audio_history_count) return FALSE;
        for(unsigned a=0;a<2;++a) {
            const SudekiMpLanArenaActorSnapshot *actor=&s->seat[a];
            BOOL cast=actor->skill_kind==SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_CHARACTER ||
                actor->skill_kind==SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT;
            if((actor->skill_kind && !cast) ||
                s->cast[a].spirit_view.kind || s->cast[a].skill_fade.kind ||
                (actor->actor_type==SUDEKIMP_LAN_ARENA_ELCO_TYPE &&
                    !actor->weapon.valid) ||
                (actor->skill_active ?
                    (!cast || !actor->skill_sequence ||
                     !actor->skill_presentation_valid || actor->action_variant ||
                     actor->locomotion.valid ||
                     !SudekiMpLanArenaSkillPresentationValid(actor,actor->actor_type)) :
                    !SudekiMpLanPartyCombatMotionValid(actor->actor_type,
                        &actor->locomotion))) return FALSE;
        }
    }
    return TRUE;
}
