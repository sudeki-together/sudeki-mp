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
        switch(clip) {
        case 0:return 0; case 1:return idle; case 2:return move; case 3:return secondary;
        default:return -1;
        }
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
            type==SUDEKIMP_LAN_ARENA_TAL_TYPE?4u:10u;
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
BOOL SudekiMpLanPartyRangedActionObserve(uint8_t type,int selector,
    uint8_t state,uint8_t *action) {
    int fire;
    if(!action || (type!=SUDEKIMP_LAN_ARENA_ELCO_TYPE &&
        type!=SUDEKIMP_LAN_ARENA_AILISH_TYPE)) return FALSE;
    fire=SudekiMpLanArenaRangedCombatSelector(type,0x85u);
    if(fire<0) return FALSE;
    *action=selector==fire && state!=192u ?
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
            SudekiMpLanArenaTalActionToNativePresentation(
                variant,&expected,&expected_state);
        idle=SudekiMpLanPartyCombatMotionSelector(type,1u);
        return mapped && (state==128u &&
            (selector==expected || selector==idle));
    }
    if(type==SUDEKIMP_LAN_ARENA_ELCO_TYPE ||
        type==SUDEKIMP_LAN_ARENA_AILISH_TYPE) {
        if(variant!=SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE || state!=192u)
            return FALSE;
        expected=SudekiMpLanArenaRangedCombatSelector(type,0x85u);
        return expected>=0 && (selector==expected || selector==0);
    }
    return FALSE;
}
BOOL SudekiMpLanPartyRangedActionChannelDrained(uint8_t type,int selector,
    uint8_t state) {
    int fire;
    if((type!=SUDEKIMP_LAN_ARENA_ELCO_TYPE &&
        type!=SUDEKIMP_LAN_ARENA_AILISH_TYPE) || state!=192u) return FALSE;
    fire=SudekiMpLanArenaRangedCombatSelector(type,0x85u);
    return fire>=0 && (selector==0 || selector==fire);
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
    default:return -1;
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
        for(clip=0;clip<6;++clip)
            if(SudekiMpLanPartyMotionSelector(type,clip)==selectors[i]) break;
        if(clip==6) return FALSE;
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
    return FALSE; /* Unknown is not idle. */
}
BOOL SudekiMpLanPartyMovementFrameValid(const SudekiMpLanPartyFrame *frame) {
    if(!SudekiMpLanPartyFrameValid(frame)) return FALSE;
    for(unsigned c=0;c<2;++c) {
        const SudekiMpLanArenaSnapshot *s=&frame->chunk[c];
        if(s->combat_enabled || s->enemy_count || s->spirit_vfx_count ||
            s->spirit_audio_history_count) return FALSE;
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
        if(s->spirit_vfx_count || s->spirit_audio_history_count) return FALSE;
        for(unsigned a=0;a<2;++a) {
            const SudekiMpLanArenaActorSnapshot *actor=&s->seat[a];
            if(actor->skill_active || actor->skill_target_phase ||
                s->cast[a].spirit_view.kind || s->cast[a].skill_fade.kind ||
                (actor->actor_type==SUDEKIMP_LAN_ARENA_ELCO_TYPE &&
                    !actor->weapon.valid) ||
                !SudekiMpLanPartyCombatMotionValid(actor->actor_type,
                    &actor->locomotion)) return FALSE;
        }
    }
    return TRUE;
}
