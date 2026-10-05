#include "network/lan_party_session.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <bcrypt.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

enum {
    HEADER = 28, LEGACY_HEADER = 20, HELLO_SIZE = 47,
    MAX_DATAGRAM = 1468, MAX_POLL = 64, FRAME_QUEUE = 8,
    ASSEMBLIES = 4, HELLO_INTERVAL = 300, KEEPALIVE_INTERVAL = 250,
    MSG_HELLO = 1, MSG_ACK, MSG_REJECT, MSG_INPUT, MSG_FRAME, MSG_END,
    MSG_KEEPALIVE, MSG_EXTENSION_CAPS, MSG_EXTENSION_CAPS_ACK,
    MSG_EXTENSION_READY, MSG_INPUT_EXTENDED, MSG_AILISH_STATE,
    MSG_EXTENSION_READY_ACK,
    MSG_PRESENTATION_CAPS, MSG_PRESENTATION_ACK, MSG_PRESENTATION_STATE,
    MSG_COMBAT_MODE, MSG_JETPACK_STATE, MSG_STORY_SCENE,
    MSG_COMMAND, MSG_PRESENCE, MSG_STORY_FRAME, MSG_STORY_WORLD,
    MSG_STORY_CONTROL, MSG_STORY_CONTROL_ACK, MSG_STORY_MOVEMENT, MSG_STORY_RECRUITMENT,
    MSG_STORY_PRESENTATION, MSG_STORY_CATCHUP, MSG_STORY_CATCHUP_ACK,
    MSG_STORY_ACTION, MSG_STORY_ACTION_RESULT, MSG_STORY_LOOT,
    COMMAND_SIZE = 24, PRESENCE_SIZE = 95, INPUT_FENCE_SIZE = 12, PRESENCE_INTERVAL = 100,
    /* Saved profile 18 adds Tal melee requests and explicit submission/SP
     * outcomes. Native routing is gated separately. Test Room and observation
     * profiles, loot sidecars and native saves remain unchanged. */
    STORY_OBSERVATION_PROFILE = 16, STORY_GAMEPLAY_PROFILE = 18, STORY_SCENE_INTERVAL = 100,
    JETPACK_STATE_SIZE = 42, COMBAT_MODE_SIZE = 13, COMBAT_MODE_INTERVAL = 100,
    INPUT_EXTENSION_SIZE = 5, AILISH_STATE_SIZE = 11 + 9 * SUDEKIMP_LAN_WEAPON_SHOT_HISTORY,
    PRESENTATION_STATE_SIZE = 37,
    ASSEMBLY_CHUNK_MASK = 0x03, ASSEMBLY_AILISH_MASK = 0x04,
    ASSEMBLY_PRESENTATION_MASK = 0x08, ASSEMBLY_JETPACK_MASK = 0x10
};

_Static_assert(SUDEKIMP_LAN_ARENA_PROTOCOL_VERSION == 42u,
    "Version the party envelope when its embedded actor codec changes");
_Static_assert(HEADER + SUDEKIMP_LAN_ARENA_MAX_SNAPSHOT_PACKET_SIZE -
    LEGACY_HEADER <= MAX_DATAGRAM, "Party chunk exceeds path MTU");
_Static_assert(HEADER + SUDEKIMP_STORY_CATCHUP_HEADER_SIZE +
    SUDEKIMP_STORY_CATCHUP_FRAGMENT_SIZE <= MAX_DATAGRAM,
    "Story catch-up fragment exceeds path MTU");

typedef struct Peer {
    SudekiMpLanPartyPeerStatus status;
    struct sockaddr_in address;
    uint64_t nonce;
    uint32_t next_sequence;
    uint32_t received_sequence;
    uint32_t last_received_at;
    uint32_t last_sent_at;
    uint32_t last_sent_frame;
    uint32_t last_sent_input;
    uint32_t last_taken_input;
    uint32_t last_extension_offer_at;
    uint32_t last_presentation_offer_at;
    uint8_t input_pending;
    uint8_t extension_offered;
    uint8_t extension_acked;
    uint8_t extension_ready;
    uint8_t extension_flags;
    uint8_t presentation_offered, presentation_ready;
    uint32_t last_mode_sequence, last_mode_sent_at;
    uint32_t last_story_revision, last_story_sent_at;
    SudekiMpLanArenaInput input;
    SudekiMpLanPartyCombatInputExtension combat;
    uint32_t input_world, input_revision, input_actor_generation;
    SudekiMpLanPartyCommand command;
    uint32_t last_command_taken, last_command_sent_at, last_presence_sent_at;
    uint8_t command_pending, command_outstanding;
    SudekiMpLanStoryControlState story_control;
    SudekiMpLanStoryControlFence story_control_ack;
    SudekiMpLanStoryMovement story_movement;
    uint32_t story_control_floor,story_control_received_at;
    uint32_t story_movement_sequence,story_movement_received_at,story_last_frame;
    uint8_t story_movement_pending;
    SudekiMpLanStoryActionRequest story_action;
    SudekiMpLanStoryActionResult story_action_result;
    uint32_t story_action_received_at;
    uint8_t story_action_pending,story_action_taken;
} Peer;

typedef struct Assembly {
    uint32_t sequence;
    uint8_t mask;
    SudekiMpLanPartyFrame frame;
} Assembly;

typedef struct StoryWorldAssembly {
    uint8_t mask, chunks;
    uint32_t received_at;
    SudekiMpLanStoryWorldFrame frame;
} StoryWorldAssembly;
typedef struct StoryPresentationAssembly {
    uint32_t sequence,received_at;
    unsigned mask;
    SudekiMpLanStoryPresentationChunk chunks[SUDEKIMP_STORY_PRESENTATION_MAX_CHUNKS];
} StoryPresentationAssembly;
typedef struct StoryCatchupSlot {
    uint8_t required,valid,acknowledged;
    uint32_t transaction,size,digest,mask,started_at,last_sent_at,frame_floor;
    uint8_t bytes[SUDEKIMP_STORY_CATCHUP_MAX_SIZE];
    SudekiMpLanStoryCatchup snapshot;
} StoryCatchupSlot;

struct SudekiMpLanPartySession {
    SRWLOCK lock;
    SOCKET socket;
    SudekiMpLanPartyConfig config;
    Peer peer[SUDEKIMP_LAN_PARTY_PLAYERS];
    uint32_t generations[SUDEKIMP_LAN_PARTY_PLAYERS];
    uint32_t rejoin_generation_floor;
    uint32_t last_hello_at;
    uint32_t last_frame_sequence;
    uint32_t last_frame_tick;
    uint32_t frame_floor;
    uint8_t frame_floor_valid;
    uint8_t last_match_state;
    uint32_t now;
    BOOL lobby_poll_started;
    Assembly assembly[ASSEMBLIES];
    SudekiMpLanPartyFrame frames[FRAME_QUEUE];
    unsigned int frame_head, frame_count;
    SudekiMpLanPartyCombatMode combat_mode;
    uint32_t combat_mode_received_at;
    SudekiMpLanStoryScene story_scene;
    uint32_t story_scene_received_at,story_scene_sequence;
    SudekiMpLanStoryFrame story_frames[FRAME_QUEUE];
    uint32_t story_frame_receipts[FRAME_QUEUE];
    unsigned story_frame_head,story_frame_count;
    uint32_t story_frame_sequence,story_frame_tick,story_frame_received_at;
    StoryWorldAssembly story_world_assembly[ASSEMBLIES];
    SudekiMpLanStoryWorldFrame story_world_frames[FRAME_QUEUE];
    uint32_t story_world_receipts[FRAME_QUEUE];
    unsigned story_world_head,story_world_count;
    uint32_t story_world_sequence,story_world_tick;
    SudekiMpLanStoryRecruitment story_recruitment;
    uint32_t story_recruitment_received_at;
    StoryCatchupSlot catchup[4];
    StoryPresentationAssembly story_presentation_assembly[ASSEMBLIES];
    SudekiMpLanStoryPresentation story_presentations[FRAME_QUEUE];
    uint32_t story_presentation_receipts[FRAME_QUEUE],story_presentation_sequence;
    unsigned story_presentation_head,story_presentation_count;
    uint8_t story_loot_save[32],story_loot_bound;
    SudekiMpStoryLootState story_loot;
    SudekiMpLanStoryLootAssembly story_loot_assembly;
    uint32_t story_loot_epoch,story_loot_scene_revision,story_loot_tick,story_loot_received_at;
    SudekiMpPartyAssignment assignment;
    uint8_t assignment_valid, presence_valid;
    SudekiMpLanPartyPresence presence;
    uint32_t presence_received_at;
};

static void service_presence_transport(SudekiMpLanPartySession *s,Peer *p);
static BOOL story_tick_near_scene(uint32_t tick,uint32_t scene_tick) {
    /* A newer heartbeat for the SAME epoch/revision may overtake the frame.
     * Local receipt freshness and independent frame sequences still apply. */
    int32_t distance=(int32_t)(tick-scene_tick);
    return distance>=-(int32_t)SUDEKIMP_LAN_STORY_MAX_AGE_MS &&
        distance<=(int32_t)SUDEKIMP_LAN_STORY_MAX_AGE_MS;
}
static void clear_story_action(Peer *p) {
    memset(&p->story_action,0,sizeof(p->story_action));
    memset(&p->story_action_result,0,sizeof(p->story_action_result));
    p->story_action_received_at=0;
    p->story_action_pending=p->story_action_taken=0;
}
static void clear_story_frames(SudekiMpLanPartySession *s) {
    s->story_frame_head=s->story_frame_count=0;
    s->story_frame_sequence=s->story_frame_tick=s->story_frame_received_at=0;
    memset(s->story_frames,0,sizeof(s->story_frames));
    memset(s->story_frame_receipts,0,sizeof(s->story_frame_receipts));
    memset(s->story_world_assembly,0,sizeof(s->story_world_assembly));
    memset(s->story_world_frames,0,sizeof(s->story_world_frames));
    memset(s->story_world_receipts,0,sizeof(s->story_world_receipts));
    s->story_world_head=s->story_world_count=0;
    s->story_world_sequence=s->story_world_tick=0;
    memset(s->story_presentation_assembly,0,sizeof(s->story_presentation_assembly));
    memset(s->story_presentations,0,sizeof(s->story_presentations));
    memset(s->story_presentation_receipts,0,sizeof(s->story_presentation_receipts));
    s->story_presentation_head=s->story_presentation_count=s->story_presentation_sequence=0;
    memset(&s->story_loot_assembly,0,sizeof(s->story_loot_assembly));
    s->story_loot_epoch=s->story_loot_scene_revision=0;
    /* Keep the account revision floor and trusted save identity. A roster
     * change/reconnect must not reset reward ownership or permit rollback. */
    for(unsigned i=1u;i<4u;++i) {
        Peer *p=&s->peer[i];
        clear_story_action(p);
        memset(&p->story_control,0,sizeof(p->story_control));
        memset(&p->story_control_ack,0,sizeof(p->story_control_ack));
        memset(&p->story_movement,0,sizeof(p->story_movement));
        p->story_control_received_at=p->story_movement_received_at=0;
        p->story_movement_sequence=p->story_last_frame=0;
        p->story_movement_pending=0;
        /* Keep the transaction floor: a roster change cannot revive an offer
         * issued for the same connection before its topology was replaced. */
    }
}

static const uint8_t actors[SUDEKIMP_LAN_PARTY_PLAYERS] = {
    SUDEKIMP_LAN_ARENA_BUKI_TYPE, SUDEKIMP_LAN_ARENA_ELCO_TYPE,
    SUDEKIMP_LAN_ARENA_TAL_TYPE, SUDEKIMP_LAN_ARENA_AILISH_TYPE
};
static const SudekiMpLanArenaCodecRoster rosters[2] = {
    {{SUDEKIMP_LAN_ARENA_BUKI_TYPE, SUDEKIMP_LAN_ARENA_ELCO_TYPE},1},
    {{SUDEKIMP_LAN_ARENA_TAL_TYPE, SUDEKIMP_LAN_ARENA_AILISH_TYPE},1}
};

static void put16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
}
static void put32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (i * 8));
}
static void put64(uint8_t *p, uint64_t v) {
    put32(p, (uint32_t)v); put32(p + 4, (uint32_t)(v >> 32));
}
static uint16_t get16(const uint8_t *p) {
    return (uint16_t)(p[0] | (uint16_t)p[1] << 8);
}
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
        (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static uint64_t get64(const uint8_t *p) {
    return get32(p) | (uint64_t)get32(p + 4) << 32;
}
static uint32_t next_sequence(Peer *p) {
    if (++p->next_sequence == 0u) ++p->next_sequence;
    return p->next_sequence;
}
static BOOL random_token(uint64_t *value) {
    return BCryptGenRandom(NULL, (PUCHAR)value, sizeof(*value),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0 && *value != 0u;
}
static BOOL same_address(const struct sockaddr_in *a, const struct sockaddr_in *b) {
    return a->sin_family == AF_INET && b->sin_family == AF_INET &&
        a->sin_addr.s_addr == b->sin_addr.s_addr && a->sin_port == b->sin_port;
}
static BOOL connected_phase(SudekiMpLanPartyPhase phase) {
    return phase==SUDEKIMP_LAN_PARTY_PENDING || phase==SUDEKIMP_LAN_PARTY_ACTIVE ||
        phase==SUDEKIMP_LAN_PARTY_OBSERVING;
}
static unsigned profile_id(const SudekiMpLanPartySession *s) {
    return s->config.story_observation==2u ? STORY_GAMEPLAY_PROFILE :
        s->config.story_observation ? STORY_OBSERVATION_PROFILE :
        SUDEKIMP_LAN_ARENA_MAP_CLEANROOM;
}
static BOOL exact(const Peer *p, const SudekiMpLanPartyLease *lease) {
    return lease != NULL && lease->seat > 0 &&
        lease->seat < SUDEKIMP_LAN_PARTY_PLAYERS && lease->token != 0 &&
        lease->generation != 0 && p->status.lease.seat == lease->seat &&
        p->status.lease.token == lease->token &&
        p->status.lease.generation == lease->generation;
}
static Peer *leased(SudekiMpLanPartySession *s, const SudekiMpLanPartyLease *l) {
    if (!l || l->seat == 0 || l->seat >= SUDEKIMP_LAN_PARTY_PLAYERS) return NULL;
    return exact(&s->peer[l->seat], l) ? &s->peer[l->seat] : NULL;
}
uint8_t SudekiMpLanPartyActorType(unsigned int seat) {
    return seat < SUDEKIMP_LAN_PARTY_PLAYERS ? actors[seat] : 0u;
}
static unsigned player_character_locked(const SudekiMpLanPartySession *s,unsigned p) {
    if(p>=4) return 4;
    return s->assignment_valid?s->assignment.character[p]:
        (s->config.assignment_enabled?s->config.character[p]:p);
}
static BOOL input_ready_locked(const SudekiMpLanPartySession *s,unsigned p,
    uint32_t world,uint32_t revision,uint32_t generation) {
    unsigned c=player_character_locked(s,p);
    if(c>=4) return FALSE;
    if(!s->presence_valid)
        return !s->config.assignment_enabled && !s->assignment_valid &&
            !world && !revision && !generation;
    if(s->config.local_seat && (uint32_t)(s->now-s->presence_received_at)>
        SUDEKIMP_LAN_PARTY_MODE_MAX_AGE_MS) return FALSE;
    return SudekiMpPartyPlayerInputReady(&s->presence.ownership,p,c,world,revision,generation);
}
BOOL SudekiMpLanPartyCommandValid(const SudekiMpLanPartyCommand *c) {
    if(!c || !c->request || !c->world || !c->revision || c->player>=4 ||
        c->kind<SUDEKIMP_LAN_PARTY_COMMAND_PRESENCE ||
        c->kind>SUDEKIMP_LAN_PARTY_COMMAND_REQUEST_PAUSE) return FALSE;
    switch(c->kind) {
        case SUDEKIMP_LAN_PARTY_COMMAND_PRESENCE:
            return c->target==4 && c->value<=3 && !c->transaction;
        case SUDEKIMP_LAN_PARTY_COMMAND_SWAP:
            return c->target<4 && !c->value && !c->transaction;
        case SUDEKIMP_LAN_PARTY_COMMAND_POLICY:
        case SUDEKIMP_LAN_PARTY_COMMAND_PAUSE:
            return !c->player && c->target==4 && c->value<=1 && !c->transaction;
        case SUDEKIMP_LAN_PARTY_COMMAND_RELEASE:
            return !c->player && c->target<4 && !c->value && !c->transaction;
        case SUDEKIMP_LAN_PARTY_COMMAND_REASSIGN:
            return !c->player && c->target<4 && c->value<4 && !c->transaction;
        case SUDEKIMP_LAN_PARTY_COMMAND_CONTROL_ACK:
            return c->target==4 && !c->value && !c->transaction && c->generation;
        case SUDEKIMP_LAN_PARTY_COMMAND_SWAP_ACK:
            return c->target==4 && !c->value && c->transaction && c->generation;
        case SUDEKIMP_LAN_PARTY_COMMAND_REQUEST_PAUSE:
            return c->target==4 && !c->value && !c->transaction;
        default: return FALSE;
    }
}
BOOL SudekiMpLanPartyPresenceValid(const SudekiMpLanPartyPresence *p) {
    if(!p || !p->sequence || !SudekiMpPartyOwnershipValid(&p->ownership)) return FALSE;
    for(unsigned i=0;i<4;++i)
        if(p->ack_result[i]>SUDEKIMP_PARTY_SWAP_UNAUTHORIZED ||
            (!p->ack_request[i] && p->ack_result[i])) return FALSE;
    return TRUE;
}
static void encode_command(uint8_t *b,const SudekiMpLanPartyCommand *c) {
    put32(b,c->request); put32(b+4,c->world); put32(b+8,c->revision);
    put32(b+12,c->generation); put32(b+16,c->transaction);
    b[20]=c->kind; b[21]=c->player; b[22]=c->target; b[23]=c->value;
}
static void decode_command(const uint8_t *b,SudekiMpLanPartyCommand *c) {
    memset(c,0,sizeof(*c)); c->request=get32(b); c->world=get32(b+4);
    c->revision=get32(b+8); c->generation=get32(b+12); c->transaction=get32(b+16);
    c->kind=b[20]; c->player=b[21]; c->target=b[22]; c->value=b[23];
}
static void encode_presence(uint8_t *b,const SudekiMpLanPartyPresence *p) {
    const SudekiMpPartyOwnership *o=&p->ownership;
    const SudekiMpPartyAssignment *a=&o->assignment;
    put32(b,p->sequence); put32(b+4,p->observed_tick);
    put32(b+8,a->world); put32(b+12,a->revision);
    for(unsigned i=0;i<4;++i) put32(b+16+4*i,a->generation[i]);
    b[32]=a->humans; b[33]=a->available; memcpy(b+34,a->character,4);
    for(unsigned i=0;i<4;++i) put32(b+38+4*i,o->last_request[i]);
    put32(b+54,o->request); b[58]=o->player; b[59]=o->previous;
    b[60]=o->target; b[61]=o->requester; b[62]=o->displaced;
    b[63]=o->handoff_control; b[64]=(uint8_t)o->phase;
    b[65]=o->connected; b[66]=o->controlling; b[67]=o->menu;
    b[68]=o->away; b[69]=o->paused;
    for(unsigned i=0;i<4;++i) b[70+i]=(uint8_t)o->control[i];
    b[74]=(uint8_t)o->absence_policy;
    for(unsigned i=0;i<4;++i) put32(b+75+4*i,p->ack_request[i]);
    memcpy(b+91,p->ack_result,4);
}
static void decode_presence(const uint8_t *b,SudekiMpLanPartyPresence *p) {
    SudekiMpPartyOwnership *o=&p->ownership;
    SudekiMpPartyAssignment *a=&o->assignment;
    memset(p,0,sizeof(*p)); p->sequence=get32(b); p->observed_tick=get32(b+4);
    a->world=get32(b+8); a->revision=get32(b+12);
    for(unsigned i=0;i<4;++i) a->generation[i]=get32(b+16+4*i);
    a->humans=b[32]; a->available=b[33]; memcpy(a->character,b+34,4);
    for(unsigned i=0;i<4;++i) o->last_request[i]=get32(b+38+4*i);
    o->request=get32(b+54); o->player=b[58]; o->previous=b[59];
    o->target=b[60]; o->requester=b[61]; o->displaced=b[62];
    o->handoff_control=b[63]; o->phase=(SudekiMpPartySwapPhase)b[64];
    o->connected=b[65]; o->controlling=b[66]; o->menu=b[67];
    o->away=b[68]; o->paused=b[69];
    for(unsigned i=0;i<4;++i) o->control[i]=(SudekiMpPartyControlPhase)b[70+i];
    o->absence_policy=(SudekiMpPartyAbsencePolicy)b[74];
    for(unsigned i=0;i<4;++i) p->ack_request[i]=get32(b+75+4*i);
    memcpy(p->ack_result,b+91,4);
}
static BOOL assignment_advances(const SudekiMpPartyAssignment *a,
    const SudekiMpPartyAssignment *b) {
    if(a->world!=b->world || b->revision<a->revision) return FALSE;
    for(unsigned i=0;i<4;++i)
        if(b->generation[i]<a->generation[i]) return FALSE;
    if(b->revision==a->revision &&
        (a->humans!=b->humans || a->available!=b->available ||
         memcmp(a->character,b->character,4) ||
         memcmp(a->generation,b->generation,sizeof(a->generation)))) return FALSE;
    return TRUE;
}
static BOOL presence_advances(const SudekiMpLanPartyPresence *a,
    const SudekiMpLanPartyPresence *b) {
    uint8_t prior[PRESENCE_SIZE],next[PRESENCE_SIZE];
    if(!SudekiMpLanPartyPresenceValid(b)) return FALSE;
    if(!a->sequence) return TRUE;
    if(!assignment_advances(&a->ownership.assignment,&b->ownership.assignment) ||
        (int32_t)(b->observed_tick-a->observed_tick)<0) return FALSE;
    if(a->sequence==b->sequence) {
        encode_presence(prior,a); encode_presence(next,b);
        return !memcmp(prior+8,next+8,PRESENCE_SIZE-8);
    }
    return SudekiMpLanArenaSequenceNewer(b->sequence,a->sequence);
}
static void fence_inputs(SudekiMpLanPartySession *s) {
    for(unsigned i=0;i<4;++i) {
        Peer *p=&s->peer[i]; p->input_pending=0;
        memset(&p->input,0,sizeof(p->input)); memset(&p->combat,0,sizeof(p->combat));
        p->input_world=p->input_revision=p->input_actor_generation=0;
        if(s->config.story_observation==2u) {
            p->story_action_pending=0;
            p->story_movement_pending=0; p->story_movement_received_at=0;
            memset(&p->story_movement,0,sizeof(p->story_movement));
        }
    }
}
static BOOL presence_input_changed(const SudekiMpLanPartyPresence *a,
    const SudekiMpLanPartyPresence *b) {
    const SudekiMpPartyOwnership *x=&a->ownership,*y=&b->ownership;
    return x->assignment.revision!=y->assignment.revision ||
        x->connected!=y->connected || x->controlling!=y->controlling ||
        x->menu!=y->menu || x->away!=y->away || x->paused!=y->paused ||
        x->phase!=y->phase || memcmp(x->control,y->control,sizeof(x->control));
}
BOOL SudekiMpLanPartyRangedPresentationValid(const SudekiMpLanPartyRangedPresentation *p) {
    if(!p || p->valid>1u || p->held>1u || p->clip>3u ||
        !isfinite(p->rate) || !isfinite(p->time) || !isfinite(p->blend)) return FALSE;
    if(!p->valid) return !p->held && !p->clip && !p->state && !p->sequence &&
        p->rate==0.0f && p->time==0.0f && p->blend==0.0f;
    return p->sequence && (p->state==0u || p->state==1u || p->state==64u ||
        p->state==65u || p->state==128u || p->state==192u) &&
        p->rate>=0.0f && p->rate<=256.0f && p->time>=0.0f && p->time<=4096.0f &&
        p->blend>=-1.0f && p->blend<=2.0f;
}
BOOL SudekiMpLanPartyAilishWeaponJournal(
    const SudekiMpLanPartyAilishWeaponState *state,SudekiMpLanWeaponState *journal) {
    SudekiMpLanWeaponState value={0};
    if(!state || !journal || (state->valid ? !state->reload_sequence :
            state->reload_sequence!=0u)) return FALSE;
    value.valid=state->valid; value.stage=state->stage; value.item=state->item;
    value.charge_q8=state->charge_q8; value.reload_ms=state->reload_ms;
    value.shot_count=state->shot_count;
    memcpy(value.shots,state->shots,sizeof(value.shots));
    if(!SudekiMpLanRangedWeaponStateValid(&value,SUDEKIMP_LAN_ARENA_AILISH_TYPE))
        return FALSE;
    *journal=value;
    return TRUE;
}
BOOL SudekiMpLanPartyJetpackStateValid(const SudekiMpLanPartyJetpackState *v) {
    if(!v || v->valid>1u || v->crystal_present>1u || v->crystal_active>1u ||
        v->infinite>1u || v->flight_phase>9u || v->platform_present>1u || !isfinite(v->fuel) || !isfinite(v->maximum) ||
        !isfinite(v->rate) || v->fuel<0.0f || v->maximum<0.0f ||
        v->maximum>1000000.0f || v->fuel>1000000.0f || fabsf(v->rate)>1000.0f ||
        (v->crystal_active && (!v->valid || !v->crystal_present))) return FALSE;
    if(!v->valid && (v->fuel!=0 || v->maximum!=0 || v->rate!=0 || v->infinite || v->flight_phase)) return FALSE;
    for(unsigned i=0;i<3u;++i)
        if(!isfinite(v->crystal_position[i]) || fabsf(v->crystal_position[i])>1000.0f ||
            (!v->crystal_present && v->crystal_position[i]!=0)) return FALSE;
    for(unsigned i=0;i<3u;++i)
        if(!isfinite(v->platform_position[i]) || fabsf(v->platform_position[i])>1000.0f ||
            (!v->platform_present && v->platform_position[i]!=0)) return FALSE;
    return TRUE;
}
static void put_float(uint8_t *p,float f) { uint32_t bits; memcpy(&bits,&f,4); put32(p,bits); }
static float get_float(const uint8_t *p) { uint32_t bits=get32(p); float f; memcpy(&f,&bits,4); return f; }

BOOL SudekiMpLanPartyFrameValid(const SudekiMpLanPartyFrame *frame) {
    if (!frame || !SudekiMpLanPartyJetpackStateValid(&frame->jetpack) ||
        !SudekiMpLanArenaSnapshotValidForRoster(&frame->chunk[0], &rosters[0]) ||
        !SudekiMpLanArenaSnapshotValidForRoster(&frame->chunk[1], &rosters[1])) return FALSE;
    for(unsigned i=0;i<2u;++i)
        if(!SudekiMpLanPartyRangedPresentationValid(&frame->ranged[i]) ||
            (frame->ranged[i].valid && !frame->chunk[0].combat_enabled)) return FALSE;
    SudekiMpLanWeaponState ailish;
    /* Replica samples carry the newest resource journal beside delayed body
     * poses. Shot admission compares against the separate confirmed tick. */
    if(!SudekiMpLanPartyAilishWeaponJournal(&frame->ailish_weapon,&ailish)) return FALSE;
    if(frame->chunk[0].spirit_vfx_count+frame->chunk[1].spirit_vfx_count >
        SUDEKIMP_LAN_ARENA_SPIRIT_VFX_CAPACITY) return FALSE;
    for(unsigned a=0; a<frame->chunk[0].spirit_vfx_count; ++a)
        for(unsigned b=0; b<frame->chunk[1].spirit_vfx_count; ++b)
            if(frame->chunk[0].spirit_vfx[a].instance_sequence ==
                frame->chunk[1].spirit_vfx[b].instance_sequence) return FALSE;
    for (unsigned part = 0; part < 2; ++part)
        for (unsigned v = 0; v < frame->chunk[part].spirit_vfx_count; ++v) {
            uint8_t owner = frame->chunk[part].spirit_vfx[v].owner_actor_type;
            if (!SudekiMpLanPartyShieldOwnerValid(&frame->chunk[part].spirit_vfx[v]) ||
                (owner != actors[part * 2] && owner != actors[part * 2 + 1]))
                return FALSE;
        }
    return frame->chunk[0].host_tick == frame->chunk[1].host_tick &&
        frame->chunk[0].match_state == frame->chunk[1].match_state &&
        frame->chunk[0].combat_enabled == frame->chunk[1].combat_enabled &&
        frame->chunk[0].sequence == frame->chunk[1].sequence &&
        frame->chunk[0].acknowledged_input == frame->chunk[1].acknowledged_input &&
        frame->chunk[1].enemy_count == 0u;
}

static BOOL combat_mode_valid(const SudekiMpLanPartyCombatMode *mode) {
    return mode && mode->sequence && mode->enabled<=1u &&
        (int32_t)(mode->observed_tick-mode->host_tick)>=0;
}
BOOL SudekiMpLanPartyCombatModeFrameReady(const SudekiMpLanPartyCombatMode *mode,
    const SudekiMpLanPartyFrame *frame) {
    return combat_mode_valid(mode) && SudekiMpLanPartyFrameValid(frame) &&
        frame->chunk[0].combat_enabled==mode->enabled &&
        (int32_t)(frame->chunk[0].host_tick-mode->host_tick)>=0;
}

static BOOL send_message(SudekiMpLanPartySession *s, const struct sockaddr_in *to,
    unsigned kind, unsigned seat, uint32_t generation, uint64_t token,
    uint32_t sequence, unsigned part, const uint8_t *payload, size_t size) {
    uint8_t bytes[MAX_DATAGRAM];
    if (size > sizeof(bytes) - HEADER) return FALSE;
    memcpy(bytes, "SMP4", 4); put16(bytes + 4, SUDEKIMP_LAN_PARTY_VERSION);
    bytes[6] = (uint8_t)kind; bytes[7] = (uint8_t)seat;
    put32(bytes + 8, generation); put64(bytes + 12, token);
    put32(bytes + 20, sequence); put16(bytes + 24, (uint16_t)size);
    bytes[26] = (uint8_t)part; bytes[27] = 0;
    if (size) memcpy(bytes + HEADER, payload, size);
    return sendto(s->socket, (const char *)bytes, (int)(HEADER + size), 0,
        (const struct sockaddr *)to, sizeof(*to)) == (int)(HEADER + size);
}
static BOOL send_peer(SudekiMpLanPartySession *s, Peer *p, unsigned kind,
    uint32_t sequence, unsigned part, const uint8_t *payload, size_t size) {
    BOOL result = send_message(s, &p->address, kind, p->status.lease.seat,
        p->status.lease.generation, p->status.lease.token, sequence, part, payload, size);
    if (result) p->last_sent_at = s->now;
    return result;
}
static void send_ack(SudekiMpLanPartySession *s, Peer *p) {
    uint8_t body[9]; put64(body, p->nonce); body[8] = (uint8_t)p->status.phase;
    (void)send_peer(s, p, MSG_ACK, next_sequence(p), 0, body, sizeof(body));
}
static void send_extension_caps(SudekiMpLanPartySession *s, Peer *p) {
    uint8_t body[2] = {SUDEKIMP_LAN_PARTY_EXTENSION_VERSION,
        SUDEKIMP_LAN_PARTY_EXTENSION_SUPPORTED};
    (void)send_peer(s, p, MSG_EXTENSION_CAPS, next_sequence(p), 0,
        body, sizeof(body));
    p->last_extension_offer_at = s->now;
    p->extension_offered = 1;
}
static void send_extension_ready(SudekiMpLanPartySession *s, Peer *p) {
    uint8_t body[2] = {SUDEKIMP_LAN_PARTY_EXTENSION_VERSION, p->extension_flags};
    (void)send_peer(s, p, MSG_EXTENSION_READY, next_sequence(p), 0,
        body, sizeof(body));
}
static void send_presentation_caps(SudekiMpLanPartySession *s,Peer *p) {
    const uint8_t version=1;
    (void)send_peer(s,p,MSG_PRESENTATION_CAPS,next_sequence(p),0,&version,1u);
    p->presentation_offered=1; p->last_presentation_offer_at=s->now;
}
static void reject(SudekiMpLanPartySession *s, const struct sockaddr_in *source,
    unsigned seat, uint64_t nonce, SudekiMpLanArenaRejectReason reason) {
    uint8_t body[9]; put64(body, nonce); body[8] = (uint8_t)reason;
    (void)send_message(s, source, MSG_REJECT, seat, 0, 0, 0, 0, body, sizeof(body));
}
static void drain(SudekiMpLanPartySession *s, Peer *p, SudekiMpLanArenaRejectReason why) {
    /* A departed lobby player must make a fresh character choice. Retire the
     * old ticket under this same transport lock, before native cleanup can
     * publish FREE; an old HELLO must never reacquire that slot in between. */
    if(!s->config.local_seat && s->config.lobby_members &&
        p->status.lease.seat>0 && p->status.lease.seat<4) {
        unsigned player=p->status.lease.seat;
        s->config.lobby_members&=(uint8_t)~(1u<<player);
        s->config.lobby_nonce[player]=0;
    }
    p->status.phase = SUDEKIMP_LAN_PARTY_DRAINING;
    clear_story_action(p);
    p->status.failure = why;
    p->input_pending = 0; p->command_pending=p->command_outstanding=0;
    memset(&p->input, 0, sizeof(p->input));
    memset(&p->combat, 0, sizeof(p->combat));
    p->extension_offered = p->extension_acked = p->extension_ready =
        p->extension_flags = 0;
    p->presentation_offered=p->presentation_ready=0;
    memset(&p->story_control,0,sizeof(p->story_control));
    memset(&p->story_control_ack,0,sizeof(p->story_control_ack));
    memset(&p->story_movement,0,sizeof(p->story_movement));
    p->story_movement_pending=0;
    memset(&s->catchup[p->status.lease.seat],0,sizeof(s->catchup[0]));
    if (s->config.local_seat) {
        memset(s->assembly, 0, sizeof(s->assembly));
        s->frame_count = s->frame_head = 0;
        s->frame_floor=0; s->frame_floor_valid=0;
        memset(&s->combat_mode,0,sizeof(s->combat_mode));
        s->combat_mode_received_at=0;
        memset(&s->story_scene,0,sizeof(s->story_scene));
        s->story_scene_received_at=s->story_scene_sequence=0;
        memset(&s->story_recruitment,0,sizeof(s->story_recruitment));
        s->story_recruitment_received_at=0;
        clear_story_frames(s);
        s->presence_valid=0; s->presence_received_at=0;
        memset(&s->presence,0,sizeof(s->presence));
    }
}

static void hello_received(SudekiMpLanPartySession *s, Peer *p,
    unsigned seat, const struct sockaddr_in *source, const uint8_t *body) {
    uint64_t nonce = get64(body);
    SudekiMpLanArenaRejectReason reason = SUDEKIMP_LAN_ARENA_REJECT_NONE;
    if (!nonce) return;
    if (s->config.lobby_members && (!(s->config.lobby_members&(1u<<seat)) ||
        nonce!=s->config.lobby_nonce[seat])) return;
    if (get32(body + 8) != SUDEKIMP_LAN_PARTY_BUILD_ID ||
        get16(body + 45) != SUDEKIMP_LAN_ARENA_PROTOCOL_VERSION)
        reason = SUDEKIMP_LAN_ARENA_REJECT_BUILD;
    else if (memcmp(body + 12, s->config.game_hash, 32)) reason = SUDEKIMP_LAN_ARENA_REJECT_GAME_HASH;
    else if (body[44] != profile_id(s)) reason = SUDEKIMP_LAN_ARENA_REJECT_MAP;
    if (reason) { reject(s, source, seat, nonce, reason); return; }
    if (p->status.phase != SUDEKIMP_LAN_PARTY_FREE) {
        if (same_address(source, &p->address) && nonce == p->nonce &&
            connected_phase(p->status.phase)) {
            p->last_received_at = s->now;
            send_ack(s, p); /* Lost ACK: never allocate another actor/generation. */
            if (!s->config.story_observation && !p->extension_ready) send_extension_caps(s, p);
        } else reject(s, source, seat, nonce, SUDEKIMP_LAN_ARENA_REJECT_BUSY);
        return;
    }
    uint64_t token;
    if (!random_token(&token) || s->generations[seat] == UINT32_MAX) {
        reject(s, source, seat, nonce, SUDEKIMP_LAN_ARENA_REJECT_BUSY);
        return;
    }
    memset(p, 0, sizeof(*p));
    p->status.lease.seat = (uint8_t)seat;
    p->status.lease.generation = ++s->generations[seat];
    p->status.lease.token = token;
    p->status.phase = SUDEKIMP_LAN_PARTY_PENDING;
    p->nonce = nonce; p->address = *source; p->last_received_at = s->now;
    send_ack(s, p);
    if(!s->config.story_observation) send_extension_caps(s, p);
}

/* Preserve a queued action edge while replacing only continuous axes/held
 * state. This is the legacy bounded coalescing rule, now independent per peer. */
static BOOL party_combat_extension_valid(uint8_t actor_type,
    const SudekiMpLanArenaInput *input,
    const SudekiMpLanPartyCombatInputExtension *combat) {
    BOOL strong_or_sweep;
    if (!input || !combat) return FALSE;
    strong_or_sweep = combat->strong_pressed || combat->sweep_pressed;
    return combat->strong_pressed <= 1u && combat->sweep_pressed <= 1u &&
        combat->block_held <= 1u && combat->flight_held <= 1u &&
        (!combat->flight_held || (actor_type==SUDEKIMP_LAN_ARENA_ELCO_TYPE &&
            !strong_or_sweep && !combat->block_held && !input->weak_attack_pressed &&
            !input->weak_attack_held && !input->ranged_first_person_active &&
            !input->skill_pressed && !input->kit_action && !input->cleanroom_combat_test_pressed)) &&
        (!strong_or_sweep || actor_type == SUDEKIMP_LAN_ARENA_TAL_TYPE) &&
        !(combat->strong_pressed && combat->sweep_pressed) &&
        !(combat->block_held &&
          (strong_or_sweep || input->weak_attack_pressed || input->weak_attack_held)) &&
        !(strong_or_sweep && input->weak_attack_held) &&
        (!(strong_or_sweep || combat->block_held) ||
          !(input->skill_pressed || input->kit_action ||
            input->cleanroom_combat_test_pressed));
}
static void latch_input(Peer *p, const SudekiMpLanArenaInput *input,
    const SudekiMpLanPartyCombatInputExtension *combat) {
    SudekiMpLanArenaInput next = *input;
    SudekiMpLanPartyCombatInputExtension next_combat = *combat;
    if (p->input_pending) {
        next.weak_attack_pressed |= p->input.weak_attack_pressed;
        next.cleanroom_combat_test_pressed |= p->input.cleanroom_combat_test_pressed;
        next_combat.strong_pressed |= p->combat.strong_pressed;
        next_combat.sweep_pressed |= p->combat.sweep_pressed;
        if (p->input.skill_pressed) {
            next.skill_pressed = 1; next.skill_slot = p->input.skill_slot;
            next.kit_action = next.kit_slot = 0;
        }
        if (p->input.kit_action) {
            next.kit_action = p->input.kit_action; next.kit_slot = p->input.kit_slot;
        }
    }
    if (next.kit_action) {
        next.skill_pressed = next.skill_slot = 0;
        next.weak_attack_pressed = next.weak_attack_held = 0;
        ZeroMemory(&next_combat, sizeof(next_combat));
    }
    if(next.skill_pressed || next.cleanroom_combat_test_pressed)
        ZeroMemory(&next_combat,sizeof(next_combat));
    else if(next_combat.strong_pressed || next_combat.sweep_pressed) {
        next_combat.block_held=0;
        next.weak_attack_held=0;
    } else if(next_combat.block_held &&
        (next.weak_attack_pressed || next.weak_attack_held))
        next_combat.block_held=0;
    /* A newer flight hold cannot resurrect an older queued combat edge. */
    if(next_combat.flight_held) {
        next.weak_attack_pressed=next.weak_attack_held=0;
        next_combat.strong_pressed=next_combat.sweep_pressed=next_combat.block_held=0;
    }
    p->input = next; p->combat = next_combat; p->input_pending = 1;
    p->status.last_input_sequence = input->sequence;
}
static BOOL nested_decode(unsigned kind, unsigned part, const uint8_t *body,
    size_t size, uint32_t sequence, uint64_t token, SudekiMpLanArenaPacket *packet) {
    uint8_t bytes[SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE];
    if (kind == MSG_INPUT_EXTENDED) {
        if (size < INPUT_EXTENSION_SIZE) return FALSE;
        size -= INPUT_EXTENSION_SIZE;
    }
    if (size > sizeof(bytes) - LEGACY_HEADER || part >= 2) return FALSE;
    memcpy(bytes, "SMPN", 4); put16(bytes + 4, SUDEKIMP_LAN_ARENA_PROTOCOL_VERSION);
    bytes[6] = (kind == MSG_INPUT || kind == MSG_INPUT_EXTENDED) ?
        SUDEKIMP_LAN_ARENA_PACKET_INPUT : SUDEKIMP_LAN_ARENA_PACKET_SNAPSHOT;
    bytes[7] = 0; put32(bytes + 8, sequence); put64(bytes + 12, token);
    memcpy(bytes + LEGACY_HEADER, body, size);
    return SudekiMpLanArenaDecodeForRoster(bytes, size + LEGACY_HEADER, packet, &rosters[part]);
}
static BOOL sequence_ahead(uint32_t candidate, uint32_t ceiling) {
    return candidate && (!ceiling || SudekiMpLanArenaSequenceNewer(candidate, ceiling));
}
static void accept_assembly(SudekiMpLanPartySession *s, Assembly *a,
    unsigned required_mask) {
    if ((a->mask & required_mask) != required_mask ||
        !SudekiMpLanPartyFrameValid(&a->frame)) return;
    uint32_t tick = a->frame.chunk[0].host_tick;
    uint8_t match = a->frame.chunk[0].match_state;
    /* Queue flush alone cannot reject reordered pre-transition datagrams.
     * The authenticated host observation fences completed assemblies too. */
    if(s->frame_floor_valid && (int32_t)(tick-s->frame_floor)<0) {
        a->mask=0; return;
    }
    if ((!s->last_frame_sequence && match == SUDEKIMP_LAN_ARENA_MATCH_ENDED) ||
        (s->last_frame_sequence &&
         (!SudekiMpLanArenaSequenceNewer(tick, s->last_frame_tick) ||
          match < s->last_match_state))) return;
    if (s->frame_count == FRAME_QUEUE) {
        s->frame_head = (s->frame_head + 1) % FRAME_QUEUE; --s->frame_count;
    }
    s->frames[(s->frame_head + s->frame_count++) % FRAME_QUEUE] = a->frame;
    s->last_frame_sequence = a->sequence;
    s->last_frame_tick = tick; s->last_match_state = match; a->mask = 0;
}
static Assembly *assembly_for(SudekiMpLanPartySession *s, uint32_t sequence) {
    Assembly *a;
    if (!sequence || (s->last_frame_sequence &&
        !SudekiMpLanArenaSequenceNewer(sequence, s->last_frame_sequence))) return NULL;
    a = &s->assembly[sequence % ASSEMBLIES];
    if (a->mask && a->sequence != sequence &&
        !SudekiMpLanArenaSequenceNewer(sequence, a->sequence)) return NULL;
    if (a->sequence != sequence) {
        memset(a, 0, sizeof(*a)); a->sequence = sequence;
    }
    return a;
}
static unsigned assembly_required(const Peer *p) {
    return ASSEMBLY_CHUNK_MASK | ASSEMBLY_JETPACK_MASK |
        ((p->extension_ready && (p->extension_flags &
            SUDEKIMP_LAN_PARTY_EXTENSION_AILISH_WEAPON))?ASSEMBLY_AILISH_MASK:0u) |
        (p->presentation_ready?ASSEMBLY_PRESENTATION_MASK:0u);
}
static void accept_frame(SudekiMpLanPartySession *s, unsigned part,
    const SudekiMpLanArenaSnapshot *frame) {
    Assembly *a = assembly_for(s, frame->sequence);
    if (!a) return;
    /* Duplicate halves cannot replace already admitted data. */
    if (a->mask & (1u << part)) return;
    a->frame.chunk[part] = *frame; a->mask |= (uint8_t)(1u << part);
    Peer *p = &s->peer[s->config.local_seat];
    accept_assembly(s, a, assembly_required(p));
}
static void accept_ailish_state(SudekiMpLanPartySession *s, uint32_t sequence,
    const SudekiMpLanPartyAilishWeaponState *state) {
    Assembly *a = assembly_for(s, sequence);
    Peer *p = &s->peer[s->config.local_seat];
    if (!a || (a->mask & ASSEMBLY_AILISH_MASK)) return;
    a->frame.ailish_weapon = *state;
    a->mask |= ASSEMBLY_AILISH_MASK;
    p->extension_ready = 1;
    accept_assembly(s, a, assembly_required(p));
}

/* World chunks have their own bounded frame sequence. Network reordering
 * between these chunks and the smaller party frame must not revoke a fresh
 * frame merely because its envelope arrived second. */
static BOOL accept_story_world(SudekiMpLanPartySession *s,const uint8_t *bytes,size_t size) {
    SudekiMpLanStoryWorldChunk c;
    if(!SudekiMpLanStoryWorldChunkDecode(bytes,size,&c) ||
        s->story_scene.phase!=SUDEKIMP_LAN_STORY_READY ||
        c.epoch!=s->story_scene.epoch || c.revision!=s->story_scene.revision ||
        (uint32_t)(s->now-s->story_scene_received_at)>SUDEKIMP_LAN_STORY_MAX_AGE_MS ||
        !story_tick_near_scene(c.host_tick,s->story_scene.observed_tick) ||
        (s->story_world_sequence &&
            (!SudekiMpLanArenaSequenceNewer(c.sequence,s->story_world_sequence) ||
             (int32_t)(c.host_tick-s->story_world_tick)<=0))) return FALSE;
    StoryWorldAssembly *a=NULL,*available=NULL,*oldest=NULL;
    for(unsigned i=0;i<ASSEMBLIES;++i) {
        StoryWorldAssembly *candidate=&s->story_world_assembly[i];
        if(candidate->mask && (uint32_t)(s->now-candidate->received_at)>SUDEKIMP_LAN_STORY_MAX_AGE_MS)
            memset(candidate,0,sizeof(*candidate));
        if(candidate->mask && candidate->frame.sequence==c.sequence) a=candidate;
        if(!candidate->mask && !available) available=candidate;
        if(candidate->mask && (!oldest ||
            SudekiMpLanArenaSequenceNewer(oldest->frame.sequence,candidate->frame.sequence))) oldest=candidate;
    }
    if(!a) {
        a=available;
        if(!a && oldest && SudekiMpLanArenaSequenceNewer(c.sequence,oldest->frame.sequence)) a=oldest;
        if(!a) return FALSE;
        memset(a,0,sizeof(*a)); a->received_at=s->now; a->chunks=c.chunks;
        a->frame.epoch=c.epoch; a->frame.revision=c.revision; a->frame.host_tick=c.host_tick;
        a->frame.sequence=c.sequence; a->frame.count=c.total;
    }
    if(a->frame.epoch!=c.epoch || a->frame.revision!=c.revision || a->frame.host_tick!=c.host_tick ||
        a->frame.count!=c.total || a->chunks!=c.chunks || (a->mask&(1u<<c.index))) return FALSE;
    memcpy(a->frame.actors+c.index*SUDEKIMP_LAN_STORY_WORLD_CHUNK_ACTORS,
        c.actors,c.count*sizeof(c.actors[0]));
    a->mask|=(uint8_t)(1u<<c.index);
    if(a->mask!=(1u<<a->chunks)-1u) return TRUE;
    if(!SudekiMpLanStoryWorldFrameMatchesScene(&a->frame,&s->story_scene)) {
        memset(a,0,sizeof(*a)); return FALSE;
    }
    if(s->story_world_count==FRAME_QUEUE) {
        s->story_world_head=(s->story_world_head+1u)%FRAME_QUEUE; --s->story_world_count;
    }
    unsigned at=(s->story_world_head+s->story_world_count++)%FRAME_QUEUE;
    s->story_world_frames[at]=a->frame;
    /* Age starts at the FIRST chunk. Completing an old partial batch does
     * not give it another freshness interval. */
    s->story_world_receipts[at]=a->received_at;
    s->story_world_sequence=a->frame.sequence; s->story_world_tick=a->frame.host_tick;
    for(unsigned i=0;i<ASSEMBLIES;++i)
        if(s->story_world_assembly[i].mask &&
            !SudekiMpLanArenaSequenceNewer(s->story_world_assembly[i].frame.sequence,s->story_world_sequence))
            memset(&s->story_world_assembly[i],0,sizeof(s->story_world_assembly[i]));
    return TRUE;
}

static BOOL story_control_fresh(uint32_t now,uint32_t tick) {
    int32_t age=(int32_t)(now-tick);
    return age>=-16 && age<=(int32_t)SUDEKIMP_STORY_CONTROL_MAX_AGE_MS;
}
static BOOL story_control_peer(const SudekiMpLanPartySession *s,const Peer *p,
    const SudekiMpLanStoryControlFence *f) {
    return s->config.story_observation==2u && p &&
        p->status.phase==SUDEKIMP_LAN_PARTY_OBSERVING && p->status.transport_confirmed &&
        f->player==p->status.lease.seat && f->character==player_character_locked(s,f->player) &&
        (s->config.local_seat || !s->catchup[f->player].required || s->catchup[f->player].acknowledged) &&
        SudekiMpLanStoryControlMatchesScene(f,&s->story_scene);
}
static BOOL story_control_advances(const Peer *p,const SudekiMpLanStoryControlState *v) {
    if(!SudekiMpLanStoryControlStateValid(v)) return FALSE;
    if(!p->story_control.fence.transaction)
        return v->fence.transaction>p->story_control_floor;
    if(v->fence.transaction!=p->story_control.fence.transaction)
        return v->fence.transaction>p->story_control_floor && v->phase==SUDEKIMP_STORY_CONTROL_PREPARE;
    if(!SudekiMpLanStoryControlFenceSame(&p->story_control.fence,&v->fence) ||
        v->phase<p->story_control.phase ||
        (int32_t)(v->observed_tick-p->story_control.observed_tick)<0) return FALSE;
    return TRUE;
}
static void story_control_store(Peer *p,const SudekiMpLanStoryControlState *v) {
    if(!SudekiMpLanStoryControlFenceSame(&p->story_control.fence,&v->fence)) {
        clear_story_action(p);
        memset(&p->story_control_ack,0,sizeof(p->story_control_ack));
        memset(&p->story_movement,0,sizeof(p->story_movement));
        p->story_movement_sequence=p->story_movement_received_at=0;
        p->story_movement_pending=0;
    }
    p->story_control=*v; p->story_control_floor=v->fence.transaction;
    if(v->phase!=SUDEKIMP_STORY_CONTROL_READY) p->story_movement_pending=0;
}
static BOOL story_movement_current(const SudekiMpLanPartySession *s,const Peer *p,
    const SudekiMpLanStoryMovement *input,uint32_t now) {
    uint32_t observation=s->config.local_seat?p->story_control_received_at:p->story_control.observed_tick;
    return story_control_peer(s,p,&input->fence) &&
        p->story_control.phase==SUDEKIMP_STORY_CONTROL_READY &&
        SudekiMpLanStoryControlFenceSame(&p->story_control.fence,&input->fence) &&
        SudekiMpLanStoryControlFenceSame(&p->story_control_ack,&input->fence) &&
        story_control_fresh(now,observation) &&
        (s->config.local_seat?story_control_fresh(now,s->story_scene_received_at):
            story_control_fresh(now,s->story_scene.observed_tick));
}

static BOOL story_action_current(const SudekiMpLanPartySession *s,const Peer *p,
    const SudekiMpLanStoryActionRequest *request,uint32_t now) {
    if(!p || !SudekiMpLanStoryActionRequestValid(request)) return FALSE;
    SudekiMpLanStoryMovement fence={0}; fence.fence=request->fence;
    uint32_t last=s->config.local_seat?s->story_frame_sequence:p->story_last_frame;
    return story_movement_current(s,p,&fence,now) && last &&
        !sequence_ahead(request->acknowledged_frame,last) &&
        (uint32_t)(last-request->acknowledged_frame)<=8u;
}
static BOOL send_story_action_result(SudekiMpLanPartySession *s,Peer *p) {
    uint8_t bytes[SUDEKIMP_STORY_ACTION_RESULT_WIRE_SIZE];
    return SudekiMpLanStoryActionResultEncode(&p->story_action_result,bytes,sizeof(bytes)) &&
        send_peer(s,p,MSG_STORY_ACTION_RESULT,next_sequence(p),0,bytes,sizeof(bytes));
}

static BOOL presentation_scene(const SudekiMpLanPartySession *s,
    uint32_t epoch,uint32_t revision,uint32_t tick) {
    return s->story_scene.phase==SUDEKIMP_LAN_STORY_READY &&
        epoch==s->story_scene.epoch && revision==s->story_scene.revision &&
        story_tick_near_scene(tick,s->story_scene.observed_tick);
}
static BOOL accept_story_presentation(SudekiMpLanPartySession *s,const uint8_t *bytes,size_t size) {
    SudekiMpLanStoryPresentationChunk c;
    if(s->config.story_observation!=2u || !s->config.local_seat ||
        !story_control_fresh(s->now,s->story_scene_received_at) ||
        !SudekiMpLanStoryPresentationChunkDecode(bytes,size,&c) ||
        !presentation_scene(s,c.epoch,c.revision,c.host_tick) ||
        (s->story_presentation_sequence &&
         !SudekiMpLanArenaSequenceNewer(c.sequence,s->story_presentation_sequence))) return FALSE;
    StoryPresentationAssembly *a=NULL,*empty=NULL;
    for(unsigned i=0;i<ASSEMBLIES;++i) {
        StoryPresentationAssembly *slot=&s->story_presentation_assembly[i];
        if(slot->mask && !story_control_fresh(s->now,slot->received_at)) memset(slot,0,sizeof(*slot));
        if(slot->sequence==c.sequence && slot->mask) a=slot;
        if(!slot->mask && !empty) empty=slot;
    }
    if(!a) {
        if(!empty) return FALSE;
        a=empty; a->sequence=c.sequence; a->received_at=s->now;
    }
    if(a->mask&(1u<<c.index)) {
        /* A duplicate is harmless only when it is the same authenticated
         * fragment. Its receipt never extends the assembly's original age. */
        const SudekiMpLanStoryPresentationChunk *old=&a->chunks[c.index];
        return old->epoch==c.epoch && old->revision==c.revision &&
            old->host_tick==c.host_tick && old->sequence==c.sequence && old->digest==c.digest &&
            old->total_size==c.total_size && old->offset==c.offset && old->size==c.size &&
            old->index==c.index && old->chunks==c.chunks && !memcmp(old->bytes,c.bytes,c.size);
    }
    for(unsigned i=0;i<SUDEKIMP_STORY_PRESENTATION_MAX_CHUNKS;++i) if(a->mask&(1u<<i)) {
        const SudekiMpLanStoryPresentationChunk *prior=&a->chunks[i];
        if(prior->epoch!=c.epoch || prior->revision!=c.revision || prior->host_tick!=c.host_tick ||
            prior->digest!=c.digest || prior->total_size!=c.total_size || prior->chunks!=c.chunks) {
            memset(a,0,sizeof(*a)); return FALSE;
        }
    }
    a->chunks[c.index]=c; a->mask|=1u<<c.index;
    if(a->mask!=(1u<<c.chunks)-1u) return TRUE;
    SudekiMpLanStoryPresentation frame;
    if(!SudekiMpLanStoryPresentationAssemble(a->chunks,c.chunks,&frame)) {
        memset(a,0,sizeof(*a)); return FALSE;
    }
    if(s->story_presentation_count==FRAME_QUEUE) {
        s->story_presentation_head=(s->story_presentation_head+1u)%FRAME_QUEUE;
        --s->story_presentation_count;
    }
    unsigned at=(s->story_presentation_head+s->story_presentation_count++)%FRAME_QUEUE;
    s->story_presentations[at]=frame; s->story_presentation_receipts[at]=a->received_at;
    s->story_presentation_sequence=frame.sequence;
    for(unsigned i=0;i<ASSEMBLIES;++i)
        if(s->story_presentation_assembly[i].mask &&
            !SudekiMpLanArenaSequenceNewer(s->story_presentation_assembly[i].sequence,frame.sequence))
            memset(&s->story_presentation_assembly[i],0,sizeof(s->story_presentation_assembly[i]));
    return TRUE;
}

static BOOL story_loot_advances(const SudekiMpStoryLootState *old,const SudekiMpStoryLootState *next) {
    if(!old->revision) return TRUE;
    return SudekiMpStoryLootAdvances(old,next);
}
static BOOL accept_story_loot(SudekiMpLanPartySession *s,const uint8_t *bytes,size_t size) {
    SudekiMpLanStoryLootChunk c; SudekiMpStoryLootState completed;
    if(s->config.story_observation!=2u || !s->config.local_seat || !s->story_loot_bound ||
        !story_control_fresh(s->now,s->story_scene_received_at) ||
        !SudekiMpLanStoryLootDecode(bytes,size,&c) ||
        !presentation_scene(s,c.epoch,c.scene_revision,c.host_tick) ||
        c.revision<s->story_loot.revision ||
        (s->story_loot_epoch==c.epoch && s->story_loot_scene_revision==c.scene_revision &&
            (int32_t)(c.host_tick-s->story_loot_tick)<=0)) return FALSE;
    int result=SudekiMpLanStoryLootAccept(&s->story_loot_assembly,&c,s->now,s->story_loot_save,&completed);
    if(result!=2) return result==1;
    if(!story_loot_advances(&s->story_loot,&completed)) return FALSE;
    s->story_loot=completed; s->story_loot_epoch=c.epoch;
    s->story_loot_scene_revision=c.scene_revision; s->story_loot_tick=c.host_tick;
    s->story_loot_received_at=s->story_loot_assembly.started_at;
    return TRUE;
}

static uint32_t catchup_digest(const uint8_t *bytes,size_t size) {
    /* Fragment consistency only; authentication is the existing peer lease. */
    uint32_t hash=2166136261u;
    for(size_t i=0;i<size;++i) hash=(hash^bytes[i])*16777619u;
    return hash;
}
static BOOL accept_story_catchup(SudekiMpLanPartySession *s,Peer *p,
    const uint8_t *bytes,size_t size) {
    if(s->config.story_observation!=2u || !s->config.local_seat ||
        !story_control_fresh(s->now,s->story_scene_received_at) ||
        size<=SUDEKIMP_STORY_CATCHUP_HEADER_SIZE) return FALSE;
    uint32_t transaction=get32(bytes),total=get32(bytes+4),digest=get32(bytes+8),index=get32(bytes+12);
    if(!transaction || !total || total>SUDEKIMP_STORY_CATCHUP_MAX_SIZE ||
        index>=SUDEKIMP_STORY_CATCHUP_MAX_FRAGMENTS) return FALSE;
    unsigned chunks=(total+SUDEKIMP_STORY_CATCHUP_FRAGMENT_SIZE-1u)/SUDEKIMP_STORY_CATCHUP_FRAGMENT_SIZE;
    unsigned offset=index*SUDEKIMP_STORY_CATCHUP_FRAGMENT_SIZE;
    if(index>=chunks) return FALSE;
    unsigned n=total-offset;
    if(n>SUDEKIMP_STORY_CATCHUP_FRAGMENT_SIZE) n=SUDEKIMP_STORY_CATCHUP_FRAGMENT_SIZE;
    if(size!=SUDEKIMP_STORY_CATCHUP_HEADER_SIZE+n) return FALSE;
    StoryCatchupSlot *a=&s->catchup[p->status.lease.seat];
    /* One immutable transaction per connection. Retrying fragments never
     * changes the snapshot or upgrades it into a fresh gameplay observation. */
    if(a->transaction && (a->transaction!=transaction || a->size!=total || a->digest!=digest)) return FALSE;
    if(!a->valid && a->mask && (uint32_t)(s->now-a->started_at)>3000u) a->mask=0;
    if(!a->mask) {
        a->required=1; a->transaction=transaction; a->size=total; a->digest=digest; a->started_at=s->now;
    }
    const uint8_t *body=bytes+SUDEKIMP_STORY_CATCHUP_HEADER_SIZE;
    if(a->mask&(1u<<index)) return !memcmp(a->bytes+offset,body,n);
    memcpy(a->bytes+offset,body,n); a->mask|=1u<<index;
    if(a->mask!=(1u<<chunks)-1u) return TRUE;
    if(catchup_digest(a->bytes,a->size)!=a->digest ||
        !SudekiMpLanStoryCatchupDecode(a->bytes,a->size,&a->snapshot) ||
        a->snapshot.transaction!=transaction ||
        !SudekiMpLanStoryCatchupMatches(&a->snapshot,&s->story_scene)) {
        a->mask=0; return FALSE;
    }
    a->valid=1; return TRUE;
}
static BOOL accept_story_catchup_ack(SudekiMpLanPartySession *s,Peer *p,
    const uint8_t *bytes,size_t size) {
    if(s->config.local_seat || s->config.story_observation!=2u || size!=16u ||
        !story_control_fresh(s->now,s->story_scene.observed_tick)) return FALSE;
    StoryCatchupSlot *a=&s->catchup[p->status.lease.seat];
    uint32_t frame=get32(bytes+12);
    if(!a->required || !a->valid || a->transaction!=get32(bytes) ||
        a->snapshot.target.epoch!=get32(bytes+4) || a->snapshot.target.revision!=get32(bytes+8) ||
        !SudekiMpLanStoryCatchupMatches(&a->snapshot,&s->story_scene) || !frame ||
        !a->frame_floor || sequence_ahead(a->frame_floor,frame) ||
        sequence_ahead(frame,p->story_last_frame) || (uint32_t)(p->story_last_frame-frame)>128u) return FALSE;
    a->acknowledged=1; return TRUE;
}

static void receive_message(SudekiMpLanPartySession *s,
    const struct sockaddr_in *source, const uint8_t *bytes, size_t size) {
    if (size < HEADER || memcmp(bytes, "SMP4", 4)) return;
    unsigned kind = bytes[6], seat = bytes[7], part = bytes[26];
    uint32_t generation = get32(bytes + 8), sequence = get32(bytes + 20);
    uint64_t token = get64(bytes + 12);
    const uint8_t *body = bytes + HEADER;
    size_t body_size = size - HEADER;
    if (seat == 0 || seat >= SUDEKIMP_LAN_PARTY_PLAYERS || bytes[27] ||
        get16(bytes + 24) != body_size || kind < MSG_HELLO ||
        kind > MSG_STORY_LOOT ||
        part >= SUDEKIMP_LAN_PARTY_CHUNKS || (kind != MSG_FRAME && part)) return;
    if (get16(bytes + 4) != SUDEKIMP_LAN_PARTY_VERSION) {
        if (!s->config.local_seat && kind == MSG_HELLO && body_size == HELLO_SIZE)
            reject(s, source, seat, get64(body), SUDEKIMP_LAN_ARENA_REJECT_VERSION);
        return;
    }
    Peer *p = &s->peer[seat];
    if (!s->config.local_seat && kind == MSG_HELLO) {
        if (body_size == HELLO_SIZE && !generation && !token && !sequence)
            hello_received(s, p, seat, source, body);
        return;
    }
    if (s->config.local_seat && (seat != s->config.local_seat ||
        !same_address(source, &p->address))) return;
    if (s->config.local_seat && kind == MSG_REJECT) {
        if ((p->status.phase == SUDEKIMP_LAN_PARTY_JOINING ||
             p->status.phase == SUDEKIMP_LAN_PARTY_PENDING) && !token && !generation &&
            body_size == 9 && get64(body) == p->nonce &&
            body[8] > 0 && body[8] <= SUDEKIMP_LAN_ARENA_REJECT_AUTHORITY) {
            p->status.phase = SUDEKIMP_LAN_PARTY_REJECTED;
            p->status.failure = (SudekiMpLanArenaRejectReason)body[8];
        }
        return;
    }
    if (s->config.local_seat && kind == MSG_ACK) {
        if (body_size != 9 || get64(body) != p->nonce || !token || !generation || !sequence ||
            generation <= s->rejoin_generation_floor ||
            (body[8] != SUDEKIMP_LAN_PARTY_PENDING &&
             body[8] != (s->config.story_observation ? SUDEKIMP_LAN_PARTY_OBSERVING : SUDEKIMP_LAN_PARTY_ACTIVE)) ||
            (p->status.phase != SUDEKIMP_LAN_PARTY_JOINING &&
             !connected_phase(p->status.phase))) return;
        if (p->status.phase != SUDEKIMP_LAN_PARTY_JOINING &&
            (token != p->status.lease.token || generation != p->status.lease.generation ||
             !SudekiMpLanArenaSequenceNewer(sequence, p->received_sequence))) return;
        p->status.lease.token = token; p->status.lease.generation = generation;
        p->status.transport_confirmed = 1;
        if (p->status.phase != SUDEKIMP_LAN_PARTY_ACTIVE &&
            p->status.phase != SUDEKIMP_LAN_PARTY_OBSERVING)
            p->status.phase = (SudekiMpLanPartyPhase)body[8];
        p->last_received_at = s->now; p->received_sequence = sequence;
        return;
    }
    if (!connected_phase(p->status.phase) || !sequence ||
        !same_address(source, &p->address) || !token || !generation ||
        token != p->status.lease.token || generation != p->status.lease.generation) return;
    if(kind==MSG_STORY_LOOT) {
        if(p->status.phase!=SUDEKIMP_LAN_PARTY_OBSERVING || !accept_story_loot(s,body,body_size)) return;
        p->last_received_at=s->now;
        if(!p->received_sequence || SudekiMpLanArenaSequenceNewer(sequence,p->received_sequence))
            p->received_sequence=sequence;
        return;
    }
    if(kind==MSG_STORY_CATCHUP || kind==MSG_STORY_CATCHUP_ACK) {
        if(p->status.phase!=SUDEKIMP_LAN_PARTY_OBSERVING ||
            !(kind==MSG_STORY_CATCHUP ? accept_story_catchup(s,p,body,body_size) :
                ((!p->received_sequence || SudekiMpLanArenaSequenceNewer(sequence,p->received_sequence)) &&
                 accept_story_catchup_ack(s,p,body,body_size)))) return;
        p->last_received_at=s->now;
        if(!p->received_sequence || SudekiMpLanArenaSequenceNewer(sequence,p->received_sequence))
            p->received_sequence=sequence;
        return;
    }
    if(kind==MSG_STORY_SCENE) {
        SudekiMpLanStoryScene next;
        if(!s->config.story_observation || !s->config.local_seat ||
            p->status.phase!=SUDEKIMP_LAN_PARTY_OBSERVING ||
            (s->story_scene_sequence && !SudekiMpLanArenaSequenceNewer(sequence,s->story_scene_sequence)) ||
            !SudekiMpLanStorySceneDecode(body,body_size,&next) ||
            !SudekiMpLanStorySceneAdvances(&s->story_scene,&next)) return;
        if(!SudekiMpLanStorySceneSame(&s->story_scene,&next)) clear_story_frames(s);
        s->story_scene=next; s->story_scene_received_at=s->now;
        s->story_scene_sequence=sequence; p->last_received_at=s->now;
        if(!p->received_sequence || SudekiMpLanArenaSequenceNewer(sequence,p->received_sequence))
            p->received_sequence=sequence;
        return;
    }
    if(kind==MSG_STORY_FRAME) {
        SudekiMpLanStoryFrame frame;
        if(!s->config.story_observation || !s->config.local_seat ||
            p->status.phase!=SUDEKIMP_LAN_PARTY_OBSERVING ||
            (uint32_t)(s->now-s->story_scene_received_at)>SUDEKIMP_LAN_STORY_MAX_AGE_MS ||
            !SudekiMpLanStoryFrameDecode(body,body_size,&frame) ||
            !SudekiMpLanStoryFrameMatchesScene(&frame,&s->story_scene) ||
            !story_tick_near_scene(frame.host_tick,s->story_scene.observed_tick) ||
            (s->story_frame_sequence &&
                (!SudekiMpLanArenaSequenceNewer(frame.sequence,s->story_frame_sequence) ||
                 (int32_t)(frame.host_tick-s->story_frame_tick)<=0))) return;
        if(s->story_frame_count==FRAME_QUEUE) {
            s->story_frame_head=(s->story_frame_head+1u)%FRAME_QUEUE;
            --s->story_frame_count;
        }
        unsigned at=(s->story_frame_head+s->story_frame_count++)%FRAME_QUEUE;
        s->story_frames[at]=frame; s->story_frame_receipts[at]=s->now;
        s->story_frame_sequence=frame.sequence; s->story_frame_tick=frame.host_tick;
        s->story_frame_received_at=s->now;
        p->last_received_at=s->now;
        if(!p->received_sequence || SudekiMpLanArenaSequenceNewer(sequence,p->received_sequence))
            p->received_sequence=sequence;
        return;
    }
    if(kind==MSG_STORY_WORLD) {
        if(s->config.story_observation!=2u || !s->config.local_seat ||
            p->status.phase!=SUDEKIMP_LAN_PARTY_OBSERVING ||
            !accept_story_world(s,body,body_size)) return;
        p->last_received_at=s->now;
        if(!p->received_sequence || SudekiMpLanArenaSequenceNewer(sequence,p->received_sequence))
            p->received_sequence=sequence;
        return;
    }
    if(kind==MSG_STORY_PRESENTATION) {
        if(p->status.phase!=SUDEKIMP_LAN_PARTY_OBSERVING ||
            !accept_story_presentation(s,body,body_size)) return;
        p->last_received_at=s->now;
        if(!p->received_sequence || SudekiMpLanArenaSequenceNewer(sequence,p->received_sequence))
            p->received_sequence=sequence;
        return;
    }
    if(kind==MSG_STORY_RECRUITMENT) {
        SudekiMpLanStoryRecruitment r;
        if(s->config.story_observation!=2u || !s->config.local_seat ||
            p->status.phase!=SUDEKIMP_LAN_PARTY_OBSERVING ||
            (p->received_sequence && !SudekiMpLanArenaSequenceNewer(sequence,p->received_sequence)) ||
            !SudekiMpLanStoryRecruitmentDecode(body,body_size,&r) ||
            !SudekiMpLanStoryRecruitmentMatchesScene(&r,&s->story_scene) ||
            !story_control_fresh(s->now,s->story_scene_received_at) ||
            !story_tick_near_scene(r.observed_tick,s->story_scene.observed_tick) ||
            (s->story_recruitment.transaction &&
             (!SudekiMpLanStoryRecruitmentSame(&r,&s->story_recruitment) ||
              (int32_t)(r.observed_tick-s->story_recruitment.observed_tick)<0))) return;
        BOOL refreshed=!s->story_recruitment.transaction ||
            (int32_t)(r.observed_tick-s->story_recruitment.observed_tick)>0;
        s->story_recruitment=r;
        if(refreshed) s->story_recruitment_received_at=s->now;
        p->received_sequence=sequence; p->last_received_at=s->now; return;
    }
    if(kind==MSG_STORY_CONTROL) {
        SudekiMpLanStoryControlState next;
        if(!s->config.local_seat ||
            (p->received_sequence && !SudekiMpLanArenaSequenceNewer(sequence,p->received_sequence)) ||
            !SudekiMpLanStoryControlDecode(body,body_size,&next) ||
            !story_control_peer(s,p,&next.fence) || !story_control_advances(p,&next) ||
            !story_control_fresh(s->now,s->story_scene_received_at) ||
            !story_tick_near_scene(next.observed_tick,s->story_scene.observed_tick)) return;
        /* Repetition can recover a lost datagram, but cannot renew the age of
         * an old native observation. READY also needs our actual prior ACK. */
        if(next.phase==SUDEKIMP_STORY_CONTROL_READY &&
            !SudekiMpLanStoryControlFenceSame(&p->story_control_ack,&next.fence)) return;
        BOOL refreshed=!p->story_control.fence.transaction ||
            next.fence.transaction!=p->story_control.fence.transaction ||
            (int32_t)(next.observed_tick-p->story_control.observed_tick)>0;
        story_control_store(p,&next);
        if(refreshed) p->story_control_received_at=s->now;
        p->received_sequence=sequence; p->last_received_at=s->now; return;
    }
    if(kind==MSG_STORY_CONTROL_ACK) {
        SudekiMpLanStoryControlFence fence;
        if(s->config.local_seat ||
            (p->received_sequence && !SudekiMpLanArenaSequenceNewer(sequence,p->received_sequence)) ||
            !SudekiMpLanStoryControlFenceDecode(body,body_size,&fence) ||
            !story_control_peer(s,p,&fence) ||
            p->story_control.phase==SUDEKIMP_STORY_CONTROL_REVOKED ||
            !SudekiMpLanStoryControlFenceSame(&p->story_control.fence,&fence) ||
            !story_control_fresh(s->now,p->story_control.observed_tick) ||
            !story_control_fresh(s->now,s->story_scene.observed_tick)) return;
        p->story_control_ack=fence;
        p->received_sequence=sequence; p->last_received_at=s->now; return;
    }
    if(kind==MSG_STORY_MOVEMENT) {
        SudekiMpLanStoryMovement input;
        if(s->config.local_seat ||
            (p->received_sequence && !SudekiMpLanArenaSequenceNewer(sequence,p->received_sequence)) ||
            !SudekiMpLanStoryMovementDecode(body,body_size,&input) ||
            !story_movement_current(s,p,&input,s->now) ||
            !p->story_last_frame || sequence_ahead(input.acknowledged_frame,p->story_last_frame) ||
            (p->story_movement_sequence &&
             !SudekiMpLanArenaSequenceNewer(input.sequence,p->story_movement_sequence))) return;
        p->story_movement=input; p->story_movement_pending=1;
        p->story_movement_sequence=input.sequence; p->story_movement_received_at=s->now;
        p->received_sequence=sequence; p->last_received_at=s->now; return;
    }
    if(kind==MSG_STORY_ACTION) {
        SudekiMpLanStoryActionRequest request;
        if(s->config.local_seat || s->config.story_observation!=2u ||
            (p->received_sequence && !SudekiMpLanArenaSequenceNewer(sequence,p->received_sequence)) ||
            !SudekiMpLanStoryActionRequestDecode(body,body_size,&request) ||
            !story_control_peer(s,p,&request.fence) ||
            !SudekiMpLanStoryControlFenceSame(&p->story_control.fence,&request.fence)) return;
        if(p->story_action.request==request.request) {
            /* Never refresh receipt age or turn a taken request back into
             * pending. A lost result permits retransmission, not another Use. */
            if(!SudekiMpLanStoryActionRequestSame(&request,&p->story_action)) return;
            if(SudekiMpLanStoryActionResultMatches(&p->story_action_result,&request))
                (void)send_story_action_result(s,p);
        } else {
            if(p->story_action.request &&
                 (!SudekiMpLanArenaSequenceNewer(request.request,p->story_action.request) ||
                  !SudekiMpLanStoryActionResultMatches(&p->story_action_result,&p->story_action))) return;
            p->story_action=request; p->story_action_received_at=s->now;
            BOOL fresh=story_action_current(s,p,&request,s->now);
            p->story_action_pending=fresh?1:0; p->story_action_taken=fresh?0:1;
            memset(&p->story_action_result,0,sizeof(p->story_action_result));
            if(!fresh) {
                /* A delayed first datagram still needs a terminal reply.
                 * Authenticated/current-fence but stale requests are NEVER
                 * native admission, nor can retries rejuvenate their frame. */
                p->story_action_result=(SudekiMpLanStoryActionResult){request.fence,
                    request.request,s->now,request.kind,request.slot,SUDEKIMP_STORY_ACTION_EXPIRED};
                (void)send_story_action_result(s,p);
            }
        }
        p->received_sequence=sequence; p->last_received_at=s->now; return;
    }
    if(kind==MSG_STORY_ACTION_RESULT) {
        SudekiMpLanStoryActionResult result;
        if(!s->config.local_seat || s->config.story_observation!=2u ||
            (p->received_sequence && !SudekiMpLanArenaSequenceNewer(sequence,p->received_sequence)) ||
            !SudekiMpLanStoryActionResultDecode(body,body_size,&result) ||
            !story_control_peer(s,p,&result.fence) ||
            !SudekiMpLanStoryControlFenceSame(&p->story_control.fence,&result.fence) ||
            !SudekiMpLanStoryActionResultMatches(&result,&p->story_action) ||
            (p->story_action_result.request &&
             (p->story_action_result.outcome!=result.outcome ||
              p->story_action_result.observed_tick!=result.observed_tick))) return;
        p->story_action_result=result; p->story_action_pending=0;
        p->received_sequence=sequence; p->last_received_at=s->now; return;
    }
    /* Observation endpoints cannot enter any arena frame, mode or input path,
     * even if a peer sends otherwise well-formed nested LA42 traffic. */
    if(s->config.story_observation && kind!=MSG_KEEPALIVE && kind!=MSG_END &&
        !(s->config.story_observation==2u && (kind==MSG_COMMAND || kind==MSG_PRESENCE))) return;
    if(kind==MSG_COMMAND) {
        SudekiMpLanPartyCommand command;
        if(s->config.local_seat || body_size!=COMMAND_SIZE ||
            (p->received_sequence && !SudekiMpLanArenaSequenceNewer(sequence,p->received_sequence))) return;
        decode_command(body,&command);
        if(command.player!=seat || !SudekiMpLanPartyCommandValid(&command) ||
            (s->config.story_observation==2u && command.kind!=SUDEKIMP_LAN_PARTY_COMMAND_SWAP)) return;
        command.lease=p->status.lease;
        if(command.request>p->last_command_taken &&
            (!p->command_pending || command.request==p->command.request)) {
            if(p->command_pending) {
                uint8_t existing[COMMAND_SIZE]; encode_command(existing,&p->command);
                if(memcmp(existing,body,COMMAND_SIZE)) return;
            } else { p->command=command; p->command_pending=1; }
        }
        p->last_received_at=s->now; p->received_sequence=sequence;
        return;
    }
    if(kind==MSG_PRESENCE) {
        SudekiMpLanPartyPresence next;
        if(!s->config.local_seat || body_size!=PRESENCE_SIZE) return;
        decode_presence(body,&next);
        if(!presence_advances(&s->presence,&next) ||
            (s->assignment_valid && !assignment_advances(&s->assignment,&next.ownership.assignment))) return;
        if(!s->presence_valid || presence_input_changed(&s->presence,&next)) {
            fence_inputs(s);
            memset(s->assembly,0,sizeof(s->assembly)); s->frame_head=s->frame_count=0;
            s->frame_floor=next.observed_tick; s->frame_floor_valid=1;
        }
        if(!s->presence_valid || s->presence.observed_tick!=next.observed_tick)
            s->presence_received_at=s->now;
        s->presence=next; s->presence_valid=1;
        s->assignment=next.ownership.assignment; s->assignment_valid=1;
        if(p->command_outstanding && next.ack_request[seat]>=p->command.request)
            p->command_outstanding=0;
        p->last_received_at=s->now;
        return;
    }
    if(kind==MSG_JETPACK_STATE) {
        if(!s->config.local_seat || p->status.phase!=SUDEKIMP_LAN_PARTY_ACTIVE ||
            part || body_size!=JETPACK_STATE_SIZE) return;
        SudekiMpLanPartyJetpackState v={0};
        v.valid=body[0]; v.crystal_present=body[1]; v.crystal_active=body[2]; v.infinite=body[3];
        v.fuel=get_float(body+4); v.maximum=get_float(body+8); v.rate=get_float(body+12);
        for(unsigned i=0;i<3;++i) v.crystal_position[i]=get_float(body+16+4*i);
        v.flight_phase=body[28]; v.platform_present=body[29];
        for(unsigned i=0;i<3;++i) v.platform_position[i]=get_float(body+30+4*i);
        if(!SudekiMpLanPartyJetpackStateValid(&v)) return;
        Assembly *a=assembly_for(s,sequence);
        if(!a || (a->mask & ASSEMBLY_JETPACK_MASK)) return;
        a->frame.jetpack=v; a->mask|=ASSEMBLY_JETPACK_MASK;
        p->last_received_at=s->now; accept_assembly(s,a,assembly_required(p));
        return;
    }
    if(kind==MSG_COMBAT_MODE) {
        SudekiMpLanPartyCombatMode next;
        const SudekiMpLanPartyCombatMode *prior=&s->combat_mode;
        if(!s->config.local_seat || p->status.phase!=SUDEKIMP_LAN_PARTY_ACTIVE ||
            body_size!=COMBAT_MODE_SIZE) return;
        next.sequence=get32(body); next.host_tick=get32(body+4);
        next.observed_tick=get32(body+8); next.enabled=body[12];
        if(!combat_mode_valid(&next)) return;
        if(prior->sequence) {
            if(next.sequence==prior->sequence) {
                if(next.host_tick!=prior->host_tick || next.enabled!=prior->enabled ||
                    !SudekiMpLanArenaSequenceNewer(next.observed_tick,
                        prior->observed_tick)) return;
            } else if(!SudekiMpLanArenaSequenceNewer(next.sequence,prior->sequence) ||
                !SudekiMpLanArenaSequenceNewer(next.host_tick,prior->observed_tick))
                return;
        }
        s->combat_mode=next; s->combat_mode_received_at=s->now;
        p->last_received_at=s->now;
        if(!p->received_sequence || SudekiMpLanArenaSequenceNewer(sequence,p->received_sequence))
            p->received_sequence=sequence;
        return;
    }
    if(kind==MSG_PRESENTATION_CAPS || kind==MSG_PRESENTATION_ACK) {
        if(body_size!=1u || body[0]!=1u || !p->extension_ready ||
            (p->received_sequence &&
             !SudekiMpLanArenaSequenceNewer(sequence,p->received_sequence))) return;
        if(s->config.local_seat && kind==MSG_PRESENTATION_CAPS) {
            /* From this point no partial presentation frame is admitted.
             * The host repeats the offer until this ACK arrives. SMP4 v4
             * version/build checks already exclude incompatible peers. */
            p->presentation_offered=p->presentation_ready=1;
            (void)send_peer(s,p,MSG_PRESENTATION_ACK,next_sequence(p),0,body,1u);
        } else if(!s->config.local_seat && kind==MSG_PRESENTATION_ACK &&
            p->presentation_offered) p->presentation_ready=1;
        else return;
        p->received_sequence=sequence; p->last_received_at=s->now;
        return;
    }
    if(kind==MSG_PRESENTATION_STATE && s->config.local_seat &&
        p->status.phase==SUDEKIMP_LAN_PARTY_ACTIVE) {
        SudekiMpLanPartyRangedPresentation values[2];
        Assembly *a;
        if(!p->presentation_ready || body_size!=PRESENTATION_STATE_SIZE ||
            body[0]!=1u) return;
        memset(values,0,sizeof(values));
        for(unsigned i=0;i<2u;++i) {
            const uint8_t *b=body+1u+i*18u;
            values[i].valid=b[0]; values[i].held=b[1];
            values[i].clip=b[2]; values[i].state=b[3];
            values[i].sequence=get16(b+4u);
            uint32_t rate=get32(b+6u),time=get32(b+10u),blend=get32(b+14u);
            memcpy(&values[i].rate,&rate,4u); memcpy(&values[i].time,&time,4u);
            memcpy(&values[i].blend,&blend,4u);
            if(!SudekiMpLanPartyRangedPresentationValid(&values[i])) return;
        }
        a=assembly_for(s,sequence);
        if(!a || (a->mask&ASSEMBLY_PRESENTATION_MASK)) return;
        memcpy(a->frame.ranged,values,sizeof(values));
        a->mask|=ASSEMBLY_PRESENTATION_MASK; p->last_received_at=s->now;
        accept_assembly(s,a,assembly_required(p));
        return;
    }
    if (s->config.local_seat && kind == MSG_EXTENSION_CAPS) {
        if (body_size != 2u || body[0] != SUDEKIMP_LAN_PARTY_EXTENSION_VERSION ||
            !body[1] || (body[1] & ~SUDEKIMP_LAN_PARTY_EXTENSION_SUPPORTED) ||
            (p->received_sequence &&
             !SudekiMpLanArenaSequenceNewer(sequence, p->received_sequence))) return;
        p->extension_offered = 1;
        p->extension_flags = body[1];
        p->received_sequence = sequence;
        p->last_received_at = s->now;
        uint8_t ack[2] = {SUDEKIMP_LAN_PARTY_EXTENSION_VERSION, p->extension_flags};
        (void)send_peer(s, p, MSG_EXTENSION_CAPS_ACK, next_sequence(p), 0,
            ack, sizeof(ack));
        return;
    }
    if (!s->config.local_seat && kind == MSG_EXTENSION_CAPS_ACK) {
        if (body_size != 2u || body[0] != SUDEKIMP_LAN_PARTY_EXTENSION_VERSION ||
            !p->extension_offered || !body[1] ||
            (body[1] & ~SUDEKIMP_LAN_PARTY_EXTENSION_SUPPORTED) ||
            (p->received_sequence &&
             !SudekiMpLanArenaSequenceNewer(sequence, p->received_sequence))) return;
        p->extension_flags = body[1];
        p->extension_acked = 1;
        p->received_sequence = sequence;
        p->last_received_at = s->now;
        send_extension_ready(s, p);
        return;
    }
    if (s->config.local_seat && kind == MSG_EXTENSION_READY) {
        if (body_size != 2u || body[0] != SUDEKIMP_LAN_PARTY_EXTENSION_VERSION ||
            !p->extension_offered || body[1] != p->extension_flags ||
            (p->received_sequence &&
             !SudekiMpLanArenaSequenceNewer(sequence, p->received_sequence))) return;
        p->extension_ready = 1;
        p->received_sequence = sequence;
        p->last_received_at = s->now;
        send_peer(s,p,MSG_EXTENSION_READY_ACK,next_sequence(p),0,
            body,body_size);
        return;
    }
    if (!s->config.local_seat && kind == MSG_EXTENSION_READY_ACK) {
        if (body_size != 2u || body[0] != SUDEKIMP_LAN_PARTY_EXTENSION_VERSION ||
            !p->extension_acked || body[1] != p->extension_flags ||
            (p->received_sequence &&
             !SudekiMpLanArenaSequenceNewer(sequence,p->received_sequence))) return;
        p->extension_ready = 1;
        p->received_sequence = sequence;
        p->last_received_at = s->now;
        return;
    }
    if (s->config.local_seat && kind == MSG_AILISH_STATE &&
        p->status.phase == SUDEKIMP_LAN_PARTY_ACTIVE) {
        SudekiMpLanPartyAilishWeaponState state;
        if (!p->extension_offered ||
            !(p->extension_flags & SUDEKIMP_LAN_PARTY_EXTENSION_AILISH_WEAPON) ||
            body_size != AILISH_STATE_SIZE ||
            body[0] != SUDEKIMP_LAN_PARTY_EXTENSION_VERSION || body[1] > 1u)
            return;
        memset(&state, 0, sizeof(state));
        state.valid = body[1]; state.stage = body[2]; state.item = body[3];
        state.charge_q8 = get16(body + 4); state.reload_ms = get16(body + 6);
        state.reload_sequence = get16(body + 8);
        state.shot_count=body[10];
        for(unsigned i=0;i<SUDEKIMP_LAN_WEAPON_SHOT_HISTORY;++i) {
            const uint8_t *p=body+11u+9u*i;
            state.shots[i].sequence=get16(p); state.shots[i].item=p[2];
            state.shots[i].pre_charge_q8=get16(p+3);
            state.shots[i].host_tick=get32(p+5);
        }
        SudekiMpLanWeaponState journal;
        if (!SudekiMpLanPartyAilishWeaponJournal(&state,&journal) ||
            (s->last_frame_sequence &&
             !SudekiMpLanArenaSequenceNewer(sequence, s->last_frame_sequence))) return;
        p->extension_ready = 1;
        p->last_received_at = s->now;
        accept_ailish_state(s, sequence, &state);
        return;
    }
    if (kind == MSG_FRAME && s->config.local_seat &&
        p->status.phase == SUDEKIMP_LAN_PARTY_ACTIVE) {
        SudekiMpLanArenaPacket packet;
        if (!nested_decode(kind, part, body, body_size, sequence, token, &packet) ||
            sequence_ahead(packet.body.snapshot.acknowledged_input, p->last_sent_input)) return;
        p->last_received_at = s->now;
        accept_frame(s, part, &packet.body.snapshot);
        return;
    }
    if (p->received_sequence && !SudekiMpLanArenaSequenceNewer(sequence, p->received_sequence)) return;
    if ((kind == MSG_INPUT || kind == MSG_INPUT_EXTENDED) && !s->config.local_seat &&
        p->status.phase == SUDEKIMP_LAN_PARTY_ACTIVE) {
        SudekiMpLanArenaPacket packet;
        SudekiMpLanPartyCombatInputExtension combat;
        uint32_t world,revision,actor_generation;
        memset(&combat, 0, sizeof(combat));
        if(body_size<INPUT_FENCE_SIZE) return;
        world=get32(body); revision=get32(body+4); actor_generation=get32(body+8);
        if(!input_ready_locked(s,seat,world,revision,actor_generation)) return;
        body+=INPUT_FENCE_SIZE; body_size-=INPUT_FENCE_SIZE;
        if (kind == MSG_INPUT_EXTENDED) {
            const uint8_t *ext;
            if (!p->extension_acked ||
                !(p->extension_flags & SUDEKIMP_LAN_PARTY_EXTENSION_COMBAT_INPUT) ||
                body_size < INPUT_EXTENSION_SIZE) return;
            ext = body + body_size - INPUT_EXTENSION_SIZE;
            if (ext[0] != SUDEKIMP_LAN_PARTY_EXTENSION_VERSION) return;
            combat.strong_pressed = ext[1];
            combat.sweep_pressed = ext[2];
            combat.block_held = ext[3];
            combat.flight_held = ext[4];
        }
        if (!nested_decode(kind, 0, body, body_size, sequence, token, &packet) ||
            packet.body.input.actor_type != SudekiMpLanPartyActorType(player_character_locked(s,seat)) ||
            sequence_ahead(packet.body.input.acknowledged_snapshot, p->last_sent_frame) ||
            !party_combat_extension_valid(packet.body.input.actor_type, &packet.body.input,
                &combat)) return;
        latch_input(p, &packet.body.input, &combat);
        p->input_world=world; p->input_revision=revision;
        p->input_actor_generation=actor_generation;
        if (kind == MSG_INPUT_EXTENDED) p->extension_ready = 1;
        p->status.last_input_received_at_ms = s->now;
    } else if ((kind == MSG_KEEPALIVE || kind == MSG_END) && !body_size) {
        p->status.transport_confirmed = 1;
        if (kind == MSG_END) drain(s, p, SUDEKIMP_LAN_ARENA_REJECT_NONE);
        else if(!s->config.local_seat && s->config.story_observation) {
            /* An exact token echo proves connectivity only. Never approve an
             * actor lease merely because its player connected as an observer. */
            p->status.phase=SUDEKIMP_LAN_PARTY_OBSERVING;
            send_ack(s,p);
        }
    } else return;
    p->received_sequence = sequence; p->last_received_at = s->now;
}

SudekiMpLanPartySession *SudekiMpLanPartyCreate(const SudekiMpLanPartyConfig *config) {
    WSADATA wsa;
    if (!config || config->local_seat >= SUDEKIMP_LAN_PARTY_PLAYERS || config->story_observation>2u ||
        config->port > 65535 || (config->local_seat && (!config->host_ipv4 || !config->port)) ||
        (config->timeout_ms && (config->timeout_ms < 500 || config->timeout_ms > 60000))) {
        SetLastError(ERROR_INVALID_PARAMETER); return NULL;
    }
    if(config->assignment_enabled>1u || config->reserved_mask>15u ||
        (config->reserved_mask && !config->assignment_enabled)) return NULL;
    if(config->assignment_enabled) {
        unsigned used=0;
        for(unsigned i=0;i<4;++i) {
            unsigned c=config->character[i];
            if(c>4 || (c<4 && (used&(1u<<c)))) return NULL;
            if(config->reserved_mask && !!(config->reserved_mask&(1u<<i))!=(c<4))
                return NULL;
            if(c<4) used|=1u<<c;
        }
    }
    if (config->lobby_members) {
        if (config->lobby_members>15 || !(config->lobby_members&1u) ||
            !(config->lobby_members&(1u<<config->local_seat))) return NULL;
        for (unsigned i=0;i<4;++i)
            if ((config->local_seat ? i==config->local_seat : (config->lobby_members>>i)&1u)!=
                (config->lobby_nonce[i]!=0)) return NULL;
    }
    if (WSAStartup(MAKEWORD(2, 2), &wsa)) return NULL;
    SudekiMpLanPartySession *s = calloc(1, sizeof(*s));
    if (!s) { WSACleanup(); return NULL; }
    InitializeSRWLock(&s->lock); s->config = *config;
    s->config.host_ipv4 = NULL; /* endpoint is parsed below, no borrowed pointer */
    if (!s->config.timeout_ms) s->config.timeout_ms = 5000;
    s->now = GetTickCount();
    s->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address)); address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons((u_short)(config->local_seat ? 0 : config->port));
    u_long nonblocking = 1; int size = sizeof(address);
    if (s->socket == INVALID_SOCKET ||
        bind(s->socket, (const struct sockaddr *)&address, size) == SOCKET_ERROR ||
        ioctlsocket(s->socket, FIONBIO, &nonblocking) == SOCKET_ERROR ||
        getsockname(s->socket, (struct sockaddr *)&address, &size) == SOCKET_ERROR) goto fail;
    if (!config->local_seat) s->config.port = ntohs(address.sin_port);
    for (unsigned i = 0; i < SUDEKIMP_LAN_PARTY_PLAYERS; ++i)
        s->peer[i].status.lease.seat = (uint8_t)i;
    if (config->local_seat) {
        Peer *p = &s->peer[config->local_seat];
        p->address.sin_family = AF_INET; p->address.sin_port = htons((u_short)config->port);
        if (InetPtonA(AF_INET, config->host_ipv4, &p->address.sin_addr) != 1 ||
            !random_token(&p->nonce)) goto fail;
        if (config->lobby_members) p->nonce=config->lobby_nonce[config->local_seat];
        p->status.phase = SUDEKIMP_LAN_PARTY_JOINING; p->last_received_at = s->now;
    } else s->peer[0].status.phase = SUDEKIMP_LAN_PARTY_ACTIVE;
    return s;
fail:
    if (s->socket != INVALID_SOCKET) closesocket(s->socket);
    free(s); WSACleanup(); SetLastError(ERROR_NETWORK_UNREACHABLE); return NULL;
}

void SudekiMpLanPartyDestroy(SudekiMpLanPartySession *s, BOOL notify) {
    if (!s) return;
    /* The owner must first join its worker and drain native leases. */
    if (notify) for (unsigned i = 1; i < SUDEKIMP_LAN_PARTY_PLAYERS; ++i) {
        Peer *p = &s->peer[i];
        if (connected_phase(p->status.phase))
            (void)send_peer(s, p, MSG_END, next_sequence(p), 0, NULL, 0);
    }
    closesocket(s->socket); free(s); WSACleanup();
}
unsigned int SudekiMpLanPartyPort(SudekiMpLanPartySession *s) {
    return s ? s->config.port : 0;
}
unsigned int SudekiMpLanPartyLocalSeat(SudekiMpLanPartySession *s) {
    return s ? s->config.local_seat : SUDEKIMP_LAN_PARTY_PLAYERS;
}
unsigned int SudekiMpLanPartyPlayerCharacter(SudekiMpLanPartySession *s,unsigned player) {
    unsigned c=4;
    if(!s) return c;
    AcquireSRWLockShared(&s->lock); c=player_character_locked(s,player);
    ReleaseSRWLockShared(&s->lock); return c;
}
unsigned int SudekiMpLanPartyLocalCharacter(SudekiMpLanPartySession *s) {
    return s?SudekiMpLanPartyPlayerCharacter(s,s->config.local_seat):4;
}
unsigned int SudekiMpLanPartyCharacterPlayer(SudekiMpLanPartySession *s,unsigned c) {
    unsigned player=4;
    if(!s || c>=4) return player;
    AcquireSRWLockShared(&s->lock);
    for(unsigned p=0;p<4;++p) if(player_character_locked(s,p)==c) { player=p; break; }
    ReleaseSRWLockShared(&s->lock); return player;
}
BOOL SudekiMpLanPartyStoryObservation(SudekiMpLanPartySession *s) {
    return s && s->config.story_observation;
}
void SudekiMpLanPartyPoll(SudekiMpLanPartySession *s, uint32_t now) {
    if (!s) return;
    AcquireSRWLockExclusive(&s->lock); s->now = now;
    if (s->config.lobby_members && !s->lobby_poll_started) {
        s->lobby_poll_started=TRUE;
        for (unsigned i=0;i<4;++i) s->peer[i].last_received_at=now;
    }
    if (s->config.local_seat) {
        Peer *p = &s->peer[s->config.local_seat];
        if ((p->status.phase == SUDEKIMP_LAN_PARTY_JOINING ||
             p->status.phase == SUDEKIMP_LAN_PARTY_PENDING) &&
            (!s->last_hello_at || (uint32_t)(now - s->last_hello_at) >= HELLO_INTERVAL)) {
            uint8_t body[HELLO_SIZE]; put64(body, p->nonce);
            put32(body + 8, SUDEKIMP_LAN_PARTY_BUILD_ID);
            memcpy(body + 12, s->config.game_hash, 32);
            body[44] = (uint8_t)profile_id(s);
            put16(body + 45, SUDEKIMP_LAN_ARENA_PROTOCOL_VERSION);
            (void)send_message(s, &p->address, MSG_HELLO, s->config.local_seat,
                0, 0, 0, 0, body, sizeof(body)); s->last_hello_at = now;
        }
    }
    for (unsigned n = 0; n < MAX_POLL; ++n) {
        uint8_t bytes[MAX_DATAGRAM]; struct sockaddr_in from;
        int from_size = sizeof(from);
        int received = recvfrom(s->socket, (char *)bytes, sizeof(bytes), 0,
            (struct sockaddr *)&from, &from_size);
        if (received == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) break;
            if (error == WSAEMSGSIZE || error == WSAECONNRESET) continue;
            /* No source identity on socket error: never end all host peers. */
            break;
        }
        receive_message(s, &from, bytes, (size_t)received);
    }
    for (unsigned i = 1; i < SUDEKIMP_LAN_PARTY_PLAYERS; ++i) {
        Peer *p = &s->peer[i];
        if (!connected_phase(p->status.phase) &&
            p->status.phase != SUDEKIMP_LAN_PARTY_JOINING) continue;
        if ((uint32_t)(now - p->last_received_at) > s->config.timeout_ms) {
            drain(s, p, SUDEKIMP_LAN_ARENA_REJECT_TIMEOUT); continue;
        }
        if (p->status.phase != SUDEKIMP_LAN_PARTY_JOINING &&
            (uint32_t)(now - p->last_sent_at) >= KEEPALIVE_INTERVAL)
            (void)send_peer(s, p, MSG_KEEPALIVE, next_sequence(p), 0, NULL, 0);
        if (!s->config.local_seat && !s->config.story_observation && !p->extension_ready &&
            (p->status.phase == SUDEKIMP_LAN_PARTY_PENDING ||
             p->status.phase == SUDEKIMP_LAN_PARTY_ACTIVE) &&
            (!p->last_extension_offer_at ||
             (uint32_t)(now - p->last_extension_offer_at) >= KEEPALIVE_INTERVAL))
            send_extension_caps(s, p);
        if(!s->config.local_seat && p->extension_ready && !p->presentation_ready &&
            (!p->last_presentation_offer_at ||
             (uint32_t)(now-p->last_presentation_offer_at)>=KEEPALIVE_INTERVAL))
            send_presentation_caps(s,p);
        service_presence_transport(s,p);
    }
    ReleaseSRWLockExclusive(&s->lock);
}
BOOL SudekiMpLanPartyPeerStatusGet(SudekiMpLanPartySession *s,
    unsigned seat, SudekiMpLanPartyPeerStatus *status) {
    if (!s || !status || seat >= SUDEKIMP_LAN_PARTY_PLAYERS) return FALSE;
    AcquireSRWLockShared(&s->lock); *status = s->peer[seat].status;
    ReleaseSRWLockShared(&s->lock); return TRUE;
}
BOOL SudekiMpLanPartyLeaseActive(SudekiMpLanPartySession *s, const SudekiMpLanPartyLease *l) {
    if (!s) return FALSE;
    AcquireSRWLockShared(&s->lock); Peer *p = leased(s, l);
    BOOL ok = p && p->status.phase == SUDEKIMP_LAN_PARTY_ACTIVE;
    ReleaseSRWLockShared(&s->lock); return ok;
}
BOOL SudekiMpLanPartyApprove(SudekiMpLanPartySession *s, const SudekiMpLanPartyLease *l) {
    if (!s || s->config.local_seat || s->config.story_observation) return FALSE;
    AcquireSRWLockExclusive(&s->lock); Peer *p = leased(s, l);
    BOOL ok = p && p->status.phase == SUDEKIMP_LAN_PARTY_PENDING &&
        p->status.transport_confirmed;
    if (ok) {
        p->status.phase = SUDEKIMP_LAN_PARTY_ACTIVE;
        send_ack(s, p);
        if (!p->extension_ready) send_extension_caps(s, p);
        else send_extension_ready(s, p);
    }
    ReleaseSRWLockExclusive(&s->lock); return ok;
}
BOOL SudekiMpLanPartyDisconnect(SudekiMpLanPartySession *s, const SudekiMpLanPartyLease *l) {
    if (!s) return FALSE;
    AcquireSRWLockExclusive(&s->lock); Peer *p = leased(s, l);
    BOOL ok = p && connected_phase(p->status.phase);
    if (ok) { (void)send_peer(s, p, MSG_END, next_sequence(p), 0, NULL, 0); drain(s, p, SUDEKIMP_LAN_ARENA_REJECT_NONE); }
    ReleaseSRWLockExclusive(&s->lock); return ok;
}
BOOL SudekiMpLanPartyReleaseDrained(SudekiMpLanPartySession *s, const SudekiMpLanPartyLease *l) {
    if (!s || s->config.local_seat) return FALSE;
    AcquireSRWLockExclusive(&s->lock); Peer *p = leased(s, l);
    BOOL ok = p && p->status.phase == SUDEKIMP_LAN_PARTY_DRAINING;
    if (ok) { uint8_t seat = l->seat; memset(p, 0, sizeof(*p)); p->status.lease.seat = seat; }
    ReleaseSRWLockExclusive(&s->lock); return ok;
}
BOOL SudekiMpLanPartyClientRejoin(SudekiMpLanPartySession *s) {
    uint64_t nonce;
    if(!s || !s->config.local_seat || !random_token(&nonce)) return FALSE;
    if (s->config.lobby_members) nonce=s->config.lobby_nonce[s->config.local_seat];
    AcquireSRWLockExclusive(&s->lock);
    Peer *p=&s->peer[s->config.local_seat];
    BOOL ok=p->status.phase==SUDEKIMP_LAN_PARTY_DRAINING ||
        (p->status.phase==SUDEKIMP_LAN_PARTY_REJECTED &&
         p->status.failure==SUDEKIMP_LAN_ARENA_REJECT_BUSY);
    if(ok) {
        struct sockaddr_in address=p->address;
        if(p->status.lease.generation>s->rejoin_generation_floor)
            s->rejoin_generation_floor=p->status.lease.generation;
        memset(p,0,sizeof(*p));
        p->address=address; p->nonce=nonce;
        p->status.lease.seat=s->config.local_seat;
        p->status.phase=SUDEKIMP_LAN_PARTY_JOINING;
        p->last_received_at=s->now;
        s->last_hello_at=s->last_frame_sequence=s->last_frame_tick=0;
        s->last_match_state=0; s->frame_floor=0; s->frame_floor_valid=0;
        s->frame_head=s->frame_count=0;
        memset(s->assembly,0,sizeof(s->assembly));
        memset(s->frames,0,sizeof(s->frames));
        memset(&s->combat_mode,0,sizeof(s->combat_mode));
        s->combat_mode_received_at=0;
        memset(&s->story_scene,0,sizeof(s->story_scene));
        s->story_scene_received_at=s->story_scene_sequence=0;
        memset(&s->story_recruitment,0,sizeof(s->story_recruitment));
        s->story_recruitment_received_at=0;
        clear_story_frames(s);
    }
    ReleaseSRWLockExclusive(&s->lock);
    return ok;
}
BOOL SudekiMpLanPartyTakeInput(SudekiMpLanPartySession *s, unsigned seat,
    SudekiMpLanPartyInput *input) {
    if (!s || s->config.local_seat || !input || !seat || seat >= SUDEKIMP_LAN_PARTY_PLAYERS) return FALSE;
    AcquireSRWLockExclusive(&s->lock); Peer *p = &s->peer[seat];
    BOOL ok = p->status.phase == SUDEKIMP_LAN_PARTY_ACTIVE && p->input_pending &&
        input_ready_locked(s,seat,p->input_world,p->input_revision,p->input_actor_generation);
    if (ok && (uint32_t)(s->now - p->status.last_input_received_at_ms) >
        SUDEKIMP_LAN_PARTY_INPUT_MAX_AGE_MS) {
        p->input_pending = 0; ok = FALSE;
    }
    if (ok) {
        input->lease = p->status.lease; input->input = p->input;
        input->combat = p->combat;
        input->received_at_ms = p->status.last_input_received_at_ms;
        input->world=p->input_world; input->revision=p->input_revision;
        input->actor_generation=p->input_actor_generation;
        p->last_taken_input = p->input.sequence; p->input_pending = 0;
    }
    ReleaseSRWLockExclusive(&s->lock); return ok;
}
BOOL SudekiMpLanPartyInputCurrent(SudekiMpLanPartySession *s,const SudekiMpLanPartyInput *input) {
    if(!s || s->config.local_seat || !input) return FALSE;
    AcquireSRWLockShared(&s->lock); Peer *p=leased(s,&input->lease);
    BOOL ok=p && p->status.phase==SUDEKIMP_LAN_PARTY_ACTIVE &&
        input->input.actor_type==SudekiMpLanPartyActorType(player_character_locked(s,input->lease.seat)) &&
        input_ready_locked(s,input->lease.seat,input->world,input->revision,input->actor_generation) &&
        input->input.sequence && input->input.sequence==p->last_taken_input;
    ReleaseSRWLockShared(&s->lock); return ok;
}
BOOL SudekiMpLanPartyAdmitInput(SudekiMpLanPartySession *s, const SudekiMpLanPartyInput *input) {
    if (!s || s->config.local_seat || !input) return FALSE;
    AcquireSRWLockExclusive(&s->lock); Peer *p = leased(s, &input->lease);
    BOOL ok = p && p->status.phase == SUDEKIMP_LAN_PARTY_ACTIVE &&
        input->input.actor_type == SudekiMpLanPartyActorType(player_character_locked(s,input->lease.seat)) &&
        input_ready_locked(s,input->lease.seat,input->world,input->revision,input->actor_generation) &&
        party_combat_extension_valid(input->input.actor_type,&input->input,
            &input->combat) &&
        SudekiMpLanArenaInputValid(&input->input) && input->input.sequence &&
        (uint32_t)(s->now - input->received_at_ms) <= SUDEKIMP_LAN_PARTY_INPUT_MAX_AGE_MS &&
        input->input.sequence == p->last_taken_input &&
        (!p->status.admitted_input_sequence ||
         SudekiMpLanArenaSequenceNewer(input->input.sequence, p->status.admitted_input_sequence));
    if (ok) p->status.admitted_input_sequence = input->input.sequence;
    ReleaseSRWLockExclusive(&s->lock); return ok;
}
static BOOL send_input_locked(SudekiMpLanPartySession *s,
    const SudekiMpLanArenaInput *input,
    const SudekiMpLanPartyCombatInputExtension *combat, BOOL extended) {
    Peer *p;
    SudekiMpLanArenaPacket packet;
    uint8_t bytes[SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE + INPUT_EXTENSION_SIZE];
    size_t size, body_size;
    uint32_t world=0,revision=0,generation=0;
    if (!s || !s->config.local_seat || s->config.story_observation || !input ||
        input->actor_type != SudekiMpLanPartyActorType(player_character_locked(s,s->config.local_seat)) ||
        (extended && (!combat || !party_combat_extension_valid(
            input->actor_type,input,combat)))) return FALSE;
    p = &s->peer[s->config.local_seat];
    if (p->status.phase != SUDEKIMP_LAN_PARTY_ACTIVE ||
        (extended && (!p->extension_ready ||
         !(p->extension_flags & SUDEKIMP_LAN_PARTY_EXTENSION_COMBAT_INPUT))))
        return FALSE;
    if(s->presence_valid) {
        unsigned c=player_character_locked(s,s->config.local_seat);
        if(c>=4) return FALSE;
        world=s->presence.ownership.assignment.world;
        revision=s->presence.ownership.assignment.revision;
        generation=s->presence.ownership.assignment.generation[c];
    }
    if(!input_ready_locked(s,s->config.local_seat,world,revision,generation)) return FALSE;
    memset(&packet,0,sizeof(packet));
    packet.type = SUDEKIMP_LAN_ARENA_PACKET_INPUT;
    packet.sequence = next_sequence(p);
    packet.session_token = p->status.lease.token;
    packet.body.input = *input;
    packet.body.input.sequence = packet.sequence;
    packet.body.input.acknowledged_snapshot = s->last_frame_sequence;
    if (!SudekiMpLanArenaEncodeForRoster(bytes,&size,&packet,&rosters[0]))
        return FALSE;
    body_size = size - LEGACY_HEADER;
    if (extended) {
        uint8_t *ext = bytes + size;
        ext[0] = SUDEKIMP_LAN_PARTY_EXTENSION_VERSION;
        ext[1] = combat->strong_pressed;
        ext[2] = combat->sweep_pressed;
        ext[3] = combat->block_held;
        ext[4] = combat->flight_held;
        body_size += INPUT_EXTENSION_SIZE;
    }
    uint8_t body[SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE+INPUT_EXTENSION_SIZE+INPUT_FENCE_SIZE];
    put32(body,world); put32(body+4,revision); put32(body+8,generation);
    memcpy(body+INPUT_FENCE_SIZE,bytes+LEGACY_HEADER,body_size);
    BOOL ok = send_peer(s,p,extended ? MSG_INPUT_EXTENDED : MSG_INPUT,
        packet.sequence,0,body,body_size+INPUT_FENCE_SIZE);
    if (ok) p->last_sent_input = packet.sequence;
    return ok;
}
BOOL SudekiMpLanPartySendInput(SudekiMpLanPartySession *s, const SudekiMpLanArenaInput *input) {
    if (!s) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    BOOL ok = send_input_locked(s,input,NULL,FALSE);
    ReleaseSRWLockExclusive(&s->lock); return ok;
}
BOOL SudekiMpLanPartySendInputExtended(SudekiMpLanPartySession *s,
    const SudekiMpLanArenaInput *input,
    const SudekiMpLanPartyCombatInputExtension *combat) {
    if (!s || !input || !combat) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    BOOL ok = send_input_locked(s,input,combat,TRUE);
    ReleaseSRWLockExclusive(&s->lock); return ok;
}
BOOL SudekiMpLanPartyExtensionReady(SudekiMpLanPartySession *s) {
    if (!s || !s->config.local_seat) return FALSE;
    AcquireSRWLockShared(&s->lock);
    Peer *p = &s->peer[s->config.local_seat];
    BOOL ready = p->status.phase == SUDEKIMP_LAN_PARTY_ACTIVE &&
        p->extension_ready &&
        (p->extension_flags & SUDEKIMP_LAN_PARTY_EXTENSION_COMBAT_INPUT) &&
        (p->extension_flags & SUDEKIMP_LAN_PARTY_EXTENSION_AILISH_WEAPON);
    ReleaseSRWLockShared(&s->lock);
    return ready;
}
BOOL SudekiMpLanPartyPublishCombatMode(SudekiMpLanPartySession *s,
    uint8_t enabled,uint32_t host_tick) {
    if(!s || s->config.local_seat || s->config.story_observation || enabled>1u) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    SudekiMpLanPartyCombatMode *mode=&s->combat_mode;
    if(mode->sequence && ((int32_t)(host_tick-mode->observed_tick)<0 ||
        (enabled!=mode->enabled && host_tick==mode->observed_tick))) {
        ReleaseSRWLockExclusive(&s->lock); return FALSE;
    }
    if(!mode->sequence || mode->enabled!=enabled) {
        if(++mode->sequence==0u) ++mode->sequence;
        mode->host_tick=host_tick; mode->enabled=enabled;
    }
    mode->observed_tick=host_tick;
    uint8_t body[COMBAT_MODE_SIZE];
    put32(body,mode->sequence); put32(body+4,mode->host_tick);
    put32(body+8,mode->observed_tick); body[12]=mode->enabled;
    for(unsigned seat=1;seat<SUDEKIMP_LAN_PARTY_PLAYERS;++seat) {
        Peer *p=&s->peer[seat];
        if(p->status.phase!=SUDEKIMP_LAN_PARTY_ACTIVE ||
            (p->last_mode_sequence==mode->sequence &&
             (uint32_t)(host_tick-p->last_mode_sent_at)<COMBAT_MODE_INTERVAL)) continue;
        if(send_peer(s,p,MSG_COMBAT_MODE,next_sequence(p),0,body,sizeof(body))) {
            p->last_mode_sequence=mode->sequence; p->last_mode_sent_at=host_tick;
        }
    }
    ReleaseSRWLockExclusive(&s->lock); return TRUE;
}
BOOL SudekiMpLanPartyGetCombatMode(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyLease *lease,uint32_t now_ms,SudekiMpLanPartyCombatMode *mode) {
    if(!s || !s->config.local_seat || !lease || !mode ||
        lease->seat!=s->config.local_seat) return FALSE;
    AcquireSRWLockShared(&s->lock);
    Peer *p=leased(s,lease);
    BOOL ok=p && p->status.phase==SUDEKIMP_LAN_PARTY_ACTIVE &&
        combat_mode_valid(&s->combat_mode) &&
        (uint32_t)(now_ms-s->combat_mode_received_at)<=SUDEKIMP_LAN_PARTY_MODE_MAX_AGE_MS;
    if(ok) *mode=s->combat_mode;
    ReleaseSRWLockShared(&s->lock); return ok;
}
BOOL SudekiMpLanPartyPublishStoryScene(SudekiMpLanPartySession *s,
    const SudekiMpLanStoryScene *scene) {
    uint8_t bytes[SUDEKIMP_LAN_STORY_WIRE_SIZE];
    if(!s || s->config.local_seat || !s->config.story_observation ||
        !SudekiMpLanStorySceneEncode(scene,bytes,sizeof(bytes))) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    if(!SudekiMpLanStorySceneAdvances(&s->story_scene,scene)) {
        ReleaseSRWLockExclusive(&s->lock); return FALSE;
    }
    if(!SudekiMpLanStorySceneSame(&s->story_scene,scene)) clear_story_frames(s);
    s->story_scene=*scene;
    for(unsigned seat=1;seat<4u;++seat) {
        Peer *p=&s->peer[seat];
        if(p->status.phase!=SUDEKIMP_LAN_PARTY_OBSERVING ||
            !p->status.transport_confirmed ||
            (p->last_story_revision==scene->revision &&
             (uint32_t)(scene->observed_tick-p->last_story_sent_at)<STORY_SCENE_INTERVAL)) continue;
        if(send_peer(s,p,MSG_STORY_SCENE,next_sequence(p),0,bytes,sizeof(bytes))) {
            p->last_story_revision=scene->revision;
            p->last_story_sent_at=scene->observed_tick;
        }
    }
    ReleaseSRWLockExclusive(&s->lock); return TRUE;
}
static BOOL story_receipt_fresh(uint32_t now,uint32_t receipt) {
    /* The caller samples now before taking the queue lock; the worker may
     * publish within that short interval. Permit one timer quantum only. */
    int32_t age=(int32_t)(now-receipt);
    return age>=-16 && age<=(int32_t)SUDEKIMP_LAN_STORY_MAX_AGE_MS;
}
BOOL SudekiMpLanPartyGetStoryScene(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyLease *lease,uint32_t now,SudekiMpLanStoryScene *scene) {
    if(!s || !s->config.local_seat || !s->config.story_observation || !lease ||
        lease->seat!=s->config.local_seat || !scene) return FALSE;
    AcquireSRWLockShared(&s->lock);
    Peer *p=leased(s,lease);
    BOOL ok=p && p->status.phase==SUDEKIMP_LAN_PARTY_OBSERVING &&
        SudekiMpLanStorySceneValid(&s->story_scene) &&
        story_receipt_fresh(now,s->story_scene_received_at);
    if(ok) *scene=s->story_scene;
    ReleaseSRWLockShared(&s->lock); return ok;
}
BOOL SudekiMpLanPartySendStoryPresentation(SudekiMpLanPartySession *s,
    const SudekiMpLanStoryPresentation *frame) {
    if(!s || s->config.local_seat || s->config.story_observation!=2u ||
        !SudekiMpLanStoryPresentationValid(frame)) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    BOOL ok=presentation_scene(s,frame->epoch,frame->revision,frame->host_tick) &&
        frame->sequence==s->story_frame_sequence && frame->host_tick==s->story_frame_tick &&
        (!s->story_presentation_sequence ||
         SudekiMpLanArenaSequenceNewer(frame->sequence,s->story_presentation_sequence));
    if(ok) {
        s->story_presentation_sequence=frame->sequence;
        unsigned chunks=SudekiMpLanStoryPresentationChunkCount(frame);
        for(unsigned i=0;i<chunks;++i) {
            uint8_t bytes[SUDEKIMP_STORY_PRESENTATION_CHUNK_MAX_SIZE]; size_t size=0;
            if(!SudekiMpLanStoryPresentationChunkEncode(frame,i,bytes,sizeof(bytes),&size)) { ok=FALSE; break; }
            for(unsigned player=1;player<4u;++player) {
                Peer *p=&s->peer[player];
                if(p->status.phase!=SUDEKIMP_LAN_PARTY_OBSERVING || !p->status.transport_confirmed ||
                    p->last_story_revision!=frame->revision) continue;
                if(!send_peer(s,p,MSG_STORY_PRESENTATION,next_sequence(p),0,bytes,size)) ok=FALSE;
            }
        }
    }
    ReleaseSRWLockExclusive(&s->lock); return ok;
}
BOOL SudekiMpLanPartyBindStoryLootSave(SudekiMpLanPartySession *s,const uint8_t identity[32]) {
    if(!s || !identity || s->config.story_observation!=2u) return FALSE;
    unsigned any=0; for(unsigned i=0;i<32;++i) any|=identity[i];
    if(!any) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    BOOL ok=!s->story_loot_bound || !memcmp(s->story_loot_save,identity,32);
    if(ok) { memcpy(s->story_loot_save,identity,32); s->story_loot_bound=1; }
    ReleaseSRWLockExclusive(&s->lock); return ok;
}
BOOL SudekiMpLanPartySendStoryLoot(SudekiMpLanPartySession *s,const SudekiMpStoryLootState *state,
    const SudekiMpLanStoryScene *scene) {
    if(!s || s->config.local_seat || s->config.story_observation!=2u || !SudekiMpStoryLootValid(state) ||
        !state->visit || !SudekiMpLanStorySceneValid(scene) || scene->phase!=SUDEKIMP_LAN_STORY_READY) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    uint32_t now=GetTickCount();
    BOOL ok=s->story_loot_bound && !memcmp(s->story_loot_save,state->save_identity,32) &&
        SudekiMpLanStorySceneSame(scene,&s->story_scene) && scene->observed_tick==s->story_scene.observed_tick &&
        story_control_fresh(now,scene->observed_tick) && story_loot_advances(&s->story_loot,state) &&
        (!s->story_loot_epoch || scene->epoch!=s->story_loot_epoch ||
         scene->revision!=s->story_loot_scene_revision || (int32_t)(scene->observed_tick-s->story_loot_tick)>0);
    if(ok) {
        /* Reserve the immutable revision before sending. A partial send is
         * retried from a fresh observation, never as another reward grant. */
        s->story_loot=*state; s->story_loot_epoch=scene->epoch;
        s->story_loot_scene_revision=scene->revision; s->story_loot_tick=scene->observed_tick;
        unsigned count=SudekiMpLanStoryLootChunkCount(state);
        for(unsigned i=0;i<count;++i) {
            uint8_t bytes[SUDEKIMP_STORY_LOOT_CHUNK_MAX_SIZE]; size_t size;
            if(!SudekiMpLanStoryLootEncode(state,scene,i,bytes,sizeof(bytes),&size)) { ok=FALSE; break; }
            for(unsigned player=1;player<4;++player) {
                Peer *p=&s->peer[player];
                if(p->status.phase!=SUDEKIMP_LAN_PARTY_OBSERVING || !p->status.transport_confirmed ||
                    p->last_story_revision!=scene->revision) continue;
                if(!send_peer(s,p,MSG_STORY_LOOT,next_sequence(p),0,bytes,size)) ok=FALSE;
            }
        }
    }
    ReleaseSRWLockExclusive(&s->lock); return ok;
}
BOOL SudekiMpLanPartyGetStoryLoot(SudekiMpLanPartySession *s,const SudekiMpLanPartyLease *lease,
    uint32_t now,SudekiMpStoryLootState *out) {
    if(!s || !out || !lease || !s->config.local_seat || s->config.story_observation!=2u ||
        lease->seat!=s->config.local_seat) return FALSE;
    AcquireSRWLockShared(&s->lock); Peer *p=leased(s,lease);
    BOOL ok=p && p->status.phase==SUDEKIMP_LAN_PARTY_OBSERVING && s->story_loot_bound &&
        s->story_loot.revision && s->story_loot_epoch==s->story_scene.epoch &&
        s->story_loot_scene_revision==s->story_scene.revision && s->story_scene.phase==SUDEKIMP_LAN_STORY_READY &&
        story_receipt_fresh(now,s->story_scene_received_at) && story_receipt_fresh(now,s->story_loot_received_at);
    if(ok) *out=s->story_loot;
    ReleaseSRWLockShared(&s->lock); return ok;
}
BOOL SudekiMpLanPartyPopStoryPresentation(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyLease *lease,uint32_t now,SudekiMpLanStoryPresentation *frame,
    uint32_t *received_at) {
    if(!s || !frame || !lease || !s->config.local_seat || s->config.story_observation!=2u ||
        lease->seat!=s->config.local_seat) return FALSE;
    AcquireSRWLockExclusive(&s->lock); Peer *p=leased(s,lease);
    while(s->story_presentation_count &&
        !story_control_fresh(now,s->story_presentation_receipts[s->story_presentation_head])) {
        s->story_presentation_head=(s->story_presentation_head+1u)%FRAME_QUEUE;
        --s->story_presentation_count;
    }
    unsigned at=s->story_presentation_head;
    BOOL ok=p && p->status.phase==SUDEKIMP_LAN_PARTY_OBSERVING && s->story_presentation_count &&
        story_control_fresh(now,s->story_scene_received_at) &&
        presentation_scene(s,s->story_presentations[at].epoch,s->story_presentations[at].revision,
            s->story_presentations[at].host_tick);
    if(ok) {
        *frame=s->story_presentations[at];
        if(received_at) *received_at=s->story_presentation_receipts[at];
        s->story_presentation_head=(s->story_presentation_head+1u)%FRAME_QUEUE;
        --s->story_presentation_count;
    }
    ReleaseSRWLockExclusive(&s->lock); return ok;
}

BOOL SudekiMpLanPartyPublishStoryRecruitment(SudekiMpLanPartySession *s,
    const SudekiMpLanStoryRecruitment *r) {
    uint8_t bytes[SUDEKIMP_STORY_RECRUITMENT_WIRE_SIZE];
    if(!s || s->config.local_seat || s->config.story_observation!=2u ||
        !SudekiMpLanStoryRecruitmentEncode(r,bytes,sizeof(bytes))) return FALSE;
    AcquireSRWLockExclusive(&s->lock); uint32_t now=GetTickCount();
    BOOL ok=SudekiMpLanStoryRecruitmentMatchesScene(r,&s->story_scene) &&
        story_control_fresh(now,r->observed_tick) && story_control_fresh(now,s->story_scene.observed_tick) &&
        (!s->story_recruitment.transaction ||
         (SudekiMpLanStoryRecruitmentSame(r,&s->story_recruitment) &&
          (int32_t)(r->observed_tick-s->story_recruitment.observed_tick)>=0));
    if(ok) {
        s->story_recruitment=*r;
        for(unsigned player=1;player<4u;++player) {
            Peer *p=&s->peer[player];
            if(p->status.phase!=SUDEKIMP_LAN_PARTY_OBSERVING || !p->status.transport_confirmed ||
                p->last_story_revision!=s->story_scene.revision) continue;
            if(!send_peer(s,p,MSG_STORY_RECRUITMENT,next_sequence(p),0,bytes,sizeof(bytes))) ok=FALSE;
        }
    }
    ReleaseSRWLockExclusive(&s->lock); return ok;
}
BOOL SudekiMpLanPartyGetStoryRecruitment(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyLease *lease,uint32_t now,SudekiMpLanStoryRecruitment *r) {
    if(!s || !r || !s->config.local_seat || s->config.story_observation!=2u ||
        !lease || lease->seat!=s->config.local_seat) return FALSE;
    AcquireSRWLockShared(&s->lock); Peer *p=leased(s,lease);
    BOOL ok=p && p->status.phase==SUDEKIMP_LAN_PARTY_OBSERVING &&
        story_control_fresh(now,s->story_scene_received_at) &&
        story_control_fresh(now,s->story_recruitment_received_at) &&
        SudekiMpLanStoryRecruitmentMatchesScene(&s->story_recruitment,&s->story_scene);
    if(ok) *r=s->story_recruitment;
    ReleaseSRWLockShared(&s->lock); return ok;
}
BOOL SudekiMpLanPartyPublishStoryControl(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyLease *lease,const SudekiMpLanStoryControlState *state) {
    uint8_t bytes[SUDEKIMP_STORY_CONTROL_WIRE_SIZE];
    if(!s || s->config.local_seat || s->config.story_observation!=2u ||
        !SudekiMpLanStoryControlEncode(state,bytes,sizeof(bytes))) return FALSE;
    AcquireSRWLockExclusive(&s->lock); Peer *p=leased(s,lease);
    uint32_t now=GetTickCount();
    BOOL ok=story_control_peer(s,p,&state->fence) && story_control_advances(p,state) &&
        story_control_fresh(now,state->observed_tick) && story_control_fresh(now,s->story_scene.observed_tick) &&
        (state->phase!=SUDEKIMP_STORY_CONTROL_READY ||
         SudekiMpLanStoryControlFenceSame(&p->story_control_ack,&state->fence));
    if(ok) {
        story_control_store(p,state);
        /* Store even if sending fails: future input must obey the latest host
         * decision, and the game thread may retry this same transaction. */
        ok=send_peer(s,p,MSG_STORY_CONTROL,next_sequence(p),0,bytes,sizeof(bytes));
    }
    ReleaseSRWLockExclusive(&s->lock); return ok;
}
BOOL SudekiMpLanPartyGetStoryControl(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyLease *lease,uint32_t now,SudekiMpLanStoryControlState *state) {
    if(!s || !state || !s->config.local_seat || s->config.story_observation!=2u ||
        !lease || lease->seat!=s->config.local_seat) return FALSE;
    AcquireSRWLockShared(&s->lock); Peer *p=leased(s,lease);
    BOOL ok=p && SudekiMpLanStoryControlStateValid(&p->story_control) &&
        story_control_peer(s,p,&p->story_control.fence) &&
        story_control_fresh(now,p->story_control_received_at) &&
        story_control_fresh(now,s->story_scene_received_at);
    if(ok) *state=p->story_control;
    ReleaseSRWLockShared(&s->lock); return ok;
}
BOOL SudekiMpLanPartyAcknowledgeStoryControl(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyLease *lease,const SudekiMpLanStoryControlFence *fence) {
    uint8_t bytes[SUDEKIMP_STORY_CONTROL_FENCE_SIZE];
    if(!s || !s->config.local_seat || s->config.story_observation!=2u || !lease ||
        lease->seat!=s->config.local_seat ||
        !SudekiMpLanStoryControlFenceEncode(fence,bytes,sizeof(bytes))) return FALSE;
    AcquireSRWLockExclusive(&s->lock); Peer *p=leased(s,lease); uint32_t now=GetTickCount();
    BOOL ok=story_control_peer(s,p,fence) &&
        p->story_control.phase!=SUDEKIMP_STORY_CONTROL_REVOKED &&
        SudekiMpLanStoryControlFenceSame(&p->story_control.fence,fence) &&
        story_control_fresh(now,p->story_control_received_at) &&
        story_control_fresh(now,s->story_scene_received_at);
    if(ok) {
        ok=send_peer(s,p,MSG_STORY_CONTROL_ACK,next_sequence(p),0,bytes,sizeof(bytes));
        if(ok) p->story_control_ack=*fence;
    }
    ReleaseSRWLockExclusive(&s->lock); return ok;
}
BOOL SudekiMpLanPartyStoryControlAcknowledged(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyLease *lease,const SudekiMpLanStoryControlFence *fence) {
    if(!s || s->config.local_seat || s->config.story_observation!=2u ||
        !SudekiMpLanStoryControlFenceValid(fence)) return FALSE;
    AcquireSRWLockShared(&s->lock); Peer *p=leased(s,lease); uint32_t now=GetTickCount();
    /* ACK proves preparation for this exact connection and native fence. It
     * survives a publication gap until revoked/replaced. Requiring the old
     * READY timestamp here made renewal impossible after 250 ms: the runtime
     * offered PREPARE, correctly rejected as a same-transaction regression.
     * Fresh native validation belongs to the game-thread caller; publication
     * and movement still independently require fresh READY/scene timestamps. */
    BOOL ok=story_control_peer(s,p,fence) &&
        SudekiMpLanStoryControlStateValid(&p->story_control) &&
        p->story_control.phase!=SUDEKIMP_STORY_CONTROL_REVOKED &&
        SudekiMpLanStoryControlFenceSame(&p->story_control.fence,fence) &&
        SudekiMpLanStoryControlFenceSame(&p->story_control_ack,fence) &&
        story_control_fresh(now,s->story_scene.observed_tick);
    ReleaseSRWLockShared(&s->lock); return ok;
}
BOOL SudekiMpLanPartySendStoryMovement(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyLease *lease,const SudekiMpLanStoryMovement *input) {
    uint8_t bytes[SUDEKIMP_STORY_MOVEMENT_WIRE_SIZE];
    if(!s || !s->config.local_seat || s->config.story_observation!=2u || !lease ||
        lease->seat!=s->config.local_seat ||
        !SudekiMpLanStoryMovementEncode(input,bytes,sizeof(bytes))) return FALSE;
    AcquireSRWLockExclusive(&s->lock); Peer *p=leased(s,lease); uint32_t now=GetTickCount();
    BOOL ok=p && story_movement_current(s,p,input,now) &&
        s->story_frame_sequence && !sequence_ahead(input->acknowledged_frame,s->story_frame_sequence) &&
        (!p->story_movement_sequence ||
         SudekiMpLanArenaSequenceNewer(input->sequence,p->story_movement_sequence));
    if(ok) {
        ok=send_peer(s,p,MSG_STORY_MOVEMENT,next_sequence(p),0,bytes,sizeof(bytes));
        if(ok) p->story_movement_sequence=input->sequence;
    }
    ReleaseSRWLockExclusive(&s->lock); return ok;
}
BOOL SudekiMpLanPartyTakeStoryMovement(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyLease *lease,uint32_t now,SudekiMpLanStoryMovement *input,
    uint32_t *received_at) {
    if(!s || s->config.local_seat || s->config.story_observation!=2u || !input) return FALSE;
    AcquireSRWLockExclusive(&s->lock); Peer *p=leased(s,lease);
    BOOL ok=p && p->story_movement_pending &&
        story_movement_current(s,p,&p->story_movement,now) &&
        story_control_fresh(now,p->story_movement_received_at);
    if(ok) {
        *input=p->story_movement;
        if(received_at) *received_at=p->story_movement_received_at;
    }
    if(p) p->story_movement_pending=0;
    ReleaseSRWLockExclusive(&s->lock); return ok;
}
BOOL SudekiMpLanPartySendStoryAction(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyLease *lease,const SudekiMpLanStoryActionRequest *request) {
    uint8_t bytes[SUDEKIMP_STORY_ACTION_REQUEST_WIRE_SIZE];
    if(!s || !s->config.local_seat || s->config.story_observation!=2u || !lease ||
        lease->seat!=s->config.local_seat ||
        !SudekiMpLanStoryActionRequestEncode(request,bytes,sizeof(bytes))) return FALSE;
    AcquireSRWLockExclusive(&s->lock); Peer *p=leased(s,lease);
    uint32_t now=GetTickCount();
    BOOL same=p && SudekiMpLanStoryActionRequestSame(request,&p->story_action);
    BOOL frame_expired=p && s->story_frame_sequence &&
        !sequence_ahead(request->acknowledged_frame,s->story_frame_sequence) &&
        (uint32_t)(s->story_frame_sequence-request->acknowledged_frame)>8u;
    BOOL ok=p && story_control_peer(s,p,&request->fence) &&
        SudekiMpLanStoryControlFenceSame(&p->story_control.fence,&request->fence) &&
        (same || ((story_action_current(s,p,request,now) || frame_expired) &&
            (!p->story_action.request ||
             (SudekiMpLanArenaSequenceNewer(request->request,p->story_action.request) &&
              SudekiMpLanStoryActionResultMatches(&p->story_action_result,&p->story_action)))));
    if(ok && !same) {
        /* Reserve before the datagram leaves. A send error does not prove
         * that the receiver saw nothing; future calls may retry only this ID. */
        p->story_action=*request; p->story_action_pending=1;
        p->story_action_received_at=now;
        memset(&p->story_action_result,0,sizeof(p->story_action_result));
        if(frame_expired) {
            /* No send was attempted for this first submission. Record local
             * preflight expiry so the UI can release its plain-data outbox.
             * A previously sent ID (same) ALWAYS recovers the host result. */
            p->story_action_pending=0;
            p->story_action_result=(SudekiMpLanStoryActionResult){request->fence,
                request->request,now,request->kind,request->slot,SUDEKIMP_STORY_ACTION_EXPIRED};
        }
    }
    if(ok && !p->story_action_result.request)
        ok=send_peer(s,p,MSG_STORY_ACTION,next_sequence(p),0,bytes,sizeof(bytes));
    ReleaseSRWLockExclusive(&s->lock); return ok;
}
BOOL SudekiMpLanPartyTakeStoryAction(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyLease *lease,uint32_t now,SudekiMpLanStoryActionRequest *request) {
    if(!s || s->config.local_seat || s->config.story_observation!=2u || !request) return FALSE;
    AcquireSRWLockExclusive(&s->lock); Peer *p=leased(s,lease);
    BOOL ok=p && p->story_action_pending && !p->story_action_taken &&
        story_action_current(s,p,&p->story_action,now) &&
        story_control_fresh(now,p->story_action_received_at);
    if(p && p->story_action_pending) {
        p->story_action_pending=0; p->story_action_taken=1;
        if(ok) *request=p->story_action;
        else {
            p->story_action_result=(SudekiMpLanStoryActionResult){p->story_action.fence,
                p->story_action.request,now,p->story_action.kind,p->story_action.slot,
                SUDEKIMP_STORY_ACTION_EXPIRED};
            (void)send_story_action_result(s,p);
        }
    }
    ReleaseSRWLockExclusive(&s->lock); return ok;
}
BOOL SudekiMpLanPartyPublishStoryActionResult(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyLease *lease,const SudekiMpLanStoryActionResult *result) {
    if(!s || s->config.local_seat || s->config.story_observation!=2u ||
        !SudekiMpLanStoryActionResultValid(result)) return FALSE;
    AcquireSRWLockExclusive(&s->lock); Peer *p=leased(s,lease);
    BOOL ok=p && p->story_action_taken &&
        SudekiMpLanStoryActionResultMatches(result,&p->story_action) &&
        (!p->story_action_result.request ||
         (p->story_action_result.outcome==result->outcome &&
          p->story_action_result.observed_tick==result->observed_tick));
    if(ok) {
        /* Keep the native result even when its packet is lost. The network
         * worker may retransmit this plain record without touching the actor. */
        p->story_action_result=*result;
        ok=send_story_action_result(s,p);
    }
    ReleaseSRWLockExclusive(&s->lock); return ok;
}
BOOL SudekiMpLanPartyGetStoryActionResult(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyLease *lease,SudekiMpLanStoryActionResult *result) {
    if(!s || !s->config.local_seat || s->config.story_observation!=2u || !lease ||
        lease->seat!=s->config.local_seat || !result) return FALSE;
    AcquireSRWLockShared(&s->lock); Peer *p=leased(s,lease);
    BOOL ok=p && story_control_peer(s,p,&p->story_action.fence) &&
        SudekiMpLanStoryControlFenceSame(&p->story_control.fence,&p->story_action.fence) &&
        SudekiMpLanStoryActionResultMatches(&p->story_action_result,&p->story_action);
    if(ok) *result=p->story_action_result;
    ReleaseSRWLockShared(&s->lock); return ok;
}
BOOL SudekiMpLanPartyRevokeStoryControl(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyLease *lease) {
    if(!s || s->config.local_seat || s->config.story_observation!=2u) return FALSE;
    AcquireSRWLockExclusive(&s->lock); Peer *p=leased(s,lease);
    BOOL ok=p!=NULL;
    if(p) {
        p->story_movement_pending=0;
        p->story_action_pending=0;
        memset(&p->story_control_ack,0,sizeof(p->story_control_ack));
        if(SudekiMpLanStoryControlStateValid(&p->story_control)) {
            p->story_control.phase=SUDEKIMP_STORY_CONTROL_REVOKED;
            p->story_control.observed_tick=GetTickCount();
            uint8_t bytes[SUDEKIMP_STORY_CONTROL_WIRE_SIZE];
            if(p->status.phase==SUDEKIMP_LAN_PARTY_OBSERVING &&
                SudekiMpLanStoryControlEncode(&p->story_control,bytes,sizeof(bytes)))
                (void)send_peer(s,p,MSG_STORY_CONTROL,next_sequence(p),0,bytes,sizeof(bytes));
        }
    }
    ReleaseSRWLockExclusive(&s->lock); return ok;
}

BOOL SudekiMpLanPartySendStoryFrame(SudekiMpLanPartySession *s,
    const SudekiMpLanStoryFrame *frame) {
    uint8_t bytes[SUDEKIMP_LAN_STORY_FRAME_MAX_SIZE]; size_t size=0;
    if(!s || s->config.local_seat || !s->config.story_observation ||
        !SudekiMpLanStoryFrameEncode(frame,bytes,sizeof(bytes),&size)) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    if(!SudekiMpLanStoryFrameMatchesScene(frame,&s->story_scene) ||
        !story_tick_near_scene(frame->host_tick,s->story_scene.observed_tick) ||
        (s->story_frame_sequence &&
            (!SudekiMpLanArenaSequenceNewer(frame->sequence,s->story_frame_sequence) ||
             (int32_t)(frame->host_tick-s->story_frame_tick)<=0))) {
        ReleaseSRWLockExclusive(&s->lock); return FALSE;
    }
    s->story_frame_sequence=frame->sequence; s->story_frame_tick=frame->host_tick;
    BOOL sent=TRUE;
    for(unsigned seat=1;seat<4u;++seat) {
        Peer *p=&s->peer[seat];
        if(p->status.phase!=SUDEKIMP_LAN_PARTY_OBSERVING ||
            !p->status.transport_confirmed || p->last_story_revision!=frame->revision) continue;
        if(!send_peer(s,p,MSG_STORY_FRAME,next_sequence(p),0,bytes,size)) sent=FALSE;
        else p->story_last_frame=frame->sequence;
    }
    ReleaseSRWLockExclusive(&s->lock); return sent;
}
BOOL SudekiMpLanPartyPopStoryFrame(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyLease *lease,uint32_t now,SudekiMpLanStoryFrame *frame) {
    return SudekiMpLanPartyPopStoryFrameReceived(s,lease,now,frame,NULL);
}
BOOL SudekiMpLanPartyPopStoryFrameReceived(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyLease *lease,uint32_t now,SudekiMpLanStoryFrame *frame,
    uint32_t *received_at) {
    if(!s || !s->config.local_seat || !s->config.story_observation || !lease ||
        lease->seat!=s->config.local_seat || !frame) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    Peer *p=leased(s,lease);
    for(unsigned n=0;n<FRAME_QUEUE && s->story_frame_count &&
        (int32_t)(now-s->story_frame_receipts[s->story_frame_head])>(int32_t)SUDEKIMP_LAN_STORY_MAX_AGE_MS;++n) {
        s->story_frame_head=(s->story_frame_head+1u)%FRAME_QUEUE;
        --s->story_frame_count;
    }
    BOOL okay=p && p->status.phase==SUDEKIMP_LAN_PARTY_OBSERVING &&
        story_receipt_fresh(now,s->story_scene_received_at) &&
        story_receipt_fresh(now,s->story_frame_received_at) &&
        s->story_frame_count &&
        story_receipt_fresh(now,s->story_frame_receipts[s->story_frame_head]) &&
        SudekiMpLanStoryFrameMatchesScene(&s->story_frames[s->story_frame_head],&s->story_scene);
    if(okay) {
        *frame=s->story_frames[s->story_frame_head];
        if(received_at) *received_at=s->story_frame_receipts[s->story_frame_head];
        s->story_frame_head=(s->story_frame_head+1u)%FRAME_QUEUE;
        --s->story_frame_count;
    }
    ReleaseSRWLockExclusive(&s->lock); return okay;
}
BOOL SudekiMpLanPartySendStoryWorld(SudekiMpLanPartySession *s,
    const SudekiMpLanStoryWorldFrame *frame) {
    if(!s || s->config.local_seat || s->config.story_observation!=2u ||
        !SudekiMpLanStoryWorldFrameValid(frame)) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    if(!SudekiMpLanStoryWorldFrameMatchesScene(frame,&s->story_scene) ||
        frame->sequence!=s->story_frame_sequence || frame->host_tick!=s->story_frame_tick ||
        !story_tick_near_scene(frame->host_tick,s->story_scene.observed_tick) ||
        (s->story_world_sequence && !SudekiMpLanArenaSequenceNewer(frame->sequence,s->story_world_sequence))) {
        ReleaseSRWLockExclusive(&s->lock); return FALSE;
    }
    s->story_world_sequence=frame->sequence; s->story_world_tick=frame->host_tick;
    unsigned chunks=SudekiMpLanStoryWorldChunkCount(frame->count); BOOL sent=TRUE;
    for(unsigned index=0;index<chunks;++index) {
        uint8_t bytes[SUDEKIMP_LAN_STORY_WORLD_CHUNK_MAX_SIZE]; size_t size=0;
        if(!SudekiMpLanStoryWorldChunkEncode(frame,index,bytes,sizeof(bytes),&size)) { sent=FALSE; break; }
        for(unsigned seat=1;seat<4u;++seat) {
            Peer *p=&s->peer[seat];
            if(p->status.phase!=SUDEKIMP_LAN_PARTY_OBSERVING || !p->status.transport_confirmed ||
                p->last_story_revision!=frame->revision) continue;
            if(!send_peer(s,p,MSG_STORY_WORLD,next_sequence(p),0,bytes,size)) sent=FALSE;
        }
    }
    ReleaseSRWLockExclusive(&s->lock); return sent;
}
BOOL SudekiMpLanPartyPopStoryWorld(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyLease *lease,uint32_t now,SudekiMpLanStoryWorldFrame *frame,
    uint32_t *received_at) {
    if(!s || !s->config.local_seat || s->config.story_observation!=2u || !lease ||
        lease->seat!=s->config.local_seat || !frame) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    Peer *p=leased(s,lease);
    while(s->story_world_count &&
        (int32_t)(now-s->story_world_receipts[s->story_world_head])>(int32_t)SUDEKIMP_LAN_STORY_MAX_AGE_MS) {
        s->story_world_head=(s->story_world_head+1u)%FRAME_QUEUE; --s->story_world_count;
    }
    BOOL okay=p && p->status.phase==SUDEKIMP_LAN_PARTY_OBSERVING &&
        story_receipt_fresh(now,s->story_scene_received_at) && s->story_world_count &&
        story_receipt_fresh(now,s->story_world_receipts[s->story_world_head]) &&
        SudekiMpLanStoryWorldFrameMatchesScene(&s->story_world_frames[s->story_world_head],&s->story_scene);
    if(okay) {
        *frame=s->story_world_frames[s->story_world_head];
        if(received_at) *received_at=s->story_world_receipts[s->story_world_head];
        s->story_world_head=(s->story_world_head+1u)%FRAME_QUEUE; --s->story_world_count;
    }
    ReleaseSRWLockExclusive(&s->lock); return okay;
}
BOOL SudekiMpLanPartySendFrame(SudekiMpLanPartySession *s, const SudekiMpLanPartyFrame *frame) {
    if (!s || s->config.local_seat || s->config.story_observation || !SudekiMpLanPartyFrameValid(frame)) return FALSE;
    AcquireSRWLockExclusive(&s->lock); BOOL any = FALSE, all = TRUE;
    for (unsigned seat = 1; seat < SUDEKIMP_LAN_PARTY_PLAYERS; ++seat) {
        Peer *p = &s->peer[seat]; if (p->status.phase != SUDEKIMP_LAN_PARTY_ACTIVE) continue;
        uint32_t sequence = next_sequence(p); BOOL sent = TRUE; any = TRUE;
        if(p->presentation_ready) {
            uint8_t body[PRESENTATION_STATE_SIZE]={1};
            for(unsigned i=0;i<2u;++i) {
                const SudekiMpLanPartyRangedPresentation *v=&frame->ranged[i];
                uint8_t *b=body+1u+i*18u;
                uint32_t rate,time,blend;
                b[0]=v->valid; b[1]=v->held; b[2]=v->clip; b[3]=v->state;
                put16(b+4u,v->sequence);
                memcpy(&rate,&v->rate,4u); memcpy(&time,&v->time,4u);
                memcpy(&blend,&v->blend,4u);
                put32(b+6u,rate); put32(b+10u,time); put32(b+14u,blend);
            }
            if(!send_peer(s,p,MSG_PRESENTATION_STATE,sequence,0,body,sizeof(body))) sent=FALSE;
        }
        {
            const SudekiMpLanPartyJetpackState *v=&frame->jetpack;
            uint8_t bytes[JETPACK_STATE_SIZE];
            bytes[0]=v->valid; bytes[1]=v->crystal_present; bytes[2]=v->crystal_active; bytes[3]=v->infinite;
            put_float(bytes+4,v->fuel); put_float(bytes+8,v->maximum); put_float(bytes+12,v->rate);
            for(unsigned i=0;i<3;++i) put_float(bytes+16+4*i,v->crystal_position[i]);
            bytes[28]=v->flight_phase; bytes[29]=v->platform_present;
            for(unsigned i=0;i<3;++i) put_float(bytes+30+4*i,v->platform_position[i]);
            if(!send_peer(s,p,MSG_JETPACK_STATE,sequence,0,bytes,sizeof(bytes))) sent=FALSE;
        }
        for (unsigned part = 0; part < SUDEKIMP_LAN_PARTY_CHUNKS; ++part) {
            SudekiMpLanArenaPacket packet; uint8_t bytes[SUDEKIMP_LAN_ARENA_MAX_PACKET_SIZE]; size_t size;
            memset(&packet, 0, sizeof(packet)); packet.type = SUDEKIMP_LAN_ARENA_PACKET_SNAPSHOT;
            packet.sequence = sequence; packet.session_token = p->status.lease.token;
            packet.body.snapshot = frame->chunk[part]; packet.body.snapshot.sequence = sequence;
            packet.body.snapshot.acknowledged_input = p->status.admitted_input_sequence;
            if (!SudekiMpLanArenaEncodeForRoster(bytes, &size, &packet, &rosters[part]) ||
                !send_peer(s, p, MSG_FRAME, sequence, part, bytes + LEGACY_HEADER, size - LEGACY_HEADER)) sent = FALSE;
        }
        if (p->extension_ready &&
            (p->extension_flags & SUDEKIMP_LAN_PARTY_EXTENSION_AILISH_WEAPON)) {
            uint8_t state[AILISH_STATE_SIZE];
            state[0] = SUDEKIMP_LAN_PARTY_EXTENSION_VERSION;
            state[1] = frame->ailish_weapon.valid;
            state[2] = frame->ailish_weapon.stage;
            state[3] = frame->ailish_weapon.item;
            put16(state + 4,frame->ailish_weapon.charge_q8);
            put16(state + 6,frame->ailish_weapon.reload_ms);
            put16(state + 8,frame->ailish_weapon.reload_sequence);
            state[10]=frame->ailish_weapon.shot_count;
            for(unsigned i=0;i<SUDEKIMP_LAN_WEAPON_SHOT_HISTORY;++i) {
                const SudekiMpLanWeaponShot *shot=&frame->ailish_weapon.shots[i];
                uint8_t *p=state+11u+9u*i;
                put16(p,shot->sequence); p[2]=shot->item;
                put16(p+3,shot->pre_charge_q8); put32(p+5,shot->host_tick);
            }
            if (!send_peer(s,p,MSG_AILISH_STATE,sequence,0,state,sizeof(state)))
                sent = FALSE;
        }
        if (sent) p->last_sent_frame = sequence;
        else all = FALSE;
    }
    ReleaseSRWLockExclusive(&s->lock); return any && all;
}
BOOL SudekiMpLanPartyTakeFrame(SudekiMpLanPartySession *s, SudekiMpLanPartyFrame *frame) {
    if (!s || !s->config.local_seat || !frame) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    BOOL ok = s->peer[s->config.local_seat].status.phase == SUDEKIMP_LAN_PARTY_ACTIVE && s->frame_count;
    if (ok) { *frame = s->frames[s->frame_head]; s->frame_head = (s->frame_head + 1) % FRAME_QUEUE; --s->frame_count; }
    ReleaseSRWLockExclusive(&s->lock); return ok;
}

static void service_presence_transport(SudekiMpLanPartySession *s,Peer *p) {
    if(s->config.story_observation==1u || !connected_phase(p->status.phase)) return;
    if(!s->config.local_seat && s->presence_valid &&
        (!p->last_presence_sent_at ||
         (uint32_t)(s->now-p->last_presence_sent_at)>=PRESENCE_INTERVAL)) {
        uint8_t body[PRESENCE_SIZE]; encode_presence(body,&s->presence);
        if(send_peer(s,p,MSG_PRESENCE,next_sequence(p),0,body,sizeof(body)))
            p->last_presence_sent_at=s->now;
    } else if(s->config.local_seat && p->command_outstanding &&
        (!p->last_command_sent_at ||
         (uint32_t)(s->now-p->last_command_sent_at)>=PRESENCE_INTERVAL)) {
        uint8_t body[COMMAND_SIZE]; encode_command(body,&p->command);
        if(send_peer(s,p,MSG_COMMAND,next_sequence(p),0,body,sizeof(body)))
            p->last_command_sent_at=s->now;
    }
}
BOOL SudekiMpLanPartyQueueCommand(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyCommand *command) {
    BOOL ok=FALSE; DWORD error=ERROR_SUCCESS;
    if(!s || s->config.story_observation==1u || !SudekiMpLanPartyCommandValid(command) ||
        command->player!=s->config.local_seat ||
        (s->config.story_observation==2u && command->kind!=SUDEKIMP_LAN_PARTY_COMMAND_SWAP)) {
        SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
    }
    AcquireSRWLockExclusive(&s->lock);
    Peer *p=&s->peer[s->config.local_seat];
    if(!s->presence_valid || !connected_phase(p->status.phase)) {
        error=ERROR_INVALID_STATE; goto done;
    }
    if(p->command_pending || p->command_outstanding) {
        uint8_t prior[COMMAND_SIZE],next[COMMAND_SIZE];
        encode_command(prior,&p->command); encode_command(next,command);
        ok=!memcmp(prior,next,sizeof(prior));
        if(!ok) error=ERROR_BUSY;
        goto done;
    }
    if(command->request<=p->last_command_taken ||
        command->request<=s->presence.ack_request[s->config.local_seat]) {
        error=ERROR_RETRY; goto done;
    }
    p->command=*command; p->command.lease=p->status.lease;
    p->command_pending=(uint8_t)!s->config.local_seat;
    p->command_outstanding=(uint8_t)!!s->config.local_seat;
    p->last_command_sent_at=0;
    if(s->config.local_seat) service_presence_transport(s,p);
    ok=TRUE;
done:
    ReleaseSRWLockExclusive(&s->lock); SetLastError(error); return ok;
}
BOOL SudekiMpLanPartyTakeCommand(SudekiMpLanPartySession *s,
    SudekiMpLanPartyCommand *command) {
    BOOL ok=FALSE;
    if(!s || s->config.local_seat || s->config.story_observation==1u || !command) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    for(unsigned i=0;i<4;++i) {
        Peer *p=&s->peer[i];
        if(!p->command_pending) continue;
        p->command_pending=0;
        if(!connected_phase(p->status.phase) ||
            (i && !exact(p,&p->command.lease))) continue;
        *command=p->command; p->last_command_taken=command->request; ok=TRUE; break;
    }
    ReleaseSRWLockExclusive(&s->lock); return ok;
}
BOOL SudekiMpLanPartyPublishPresence(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyPresence *presence) {
    BOOL ok=FALSE;
    if(!s || s->config.local_seat || s->config.story_observation==1u ||
        !SudekiMpLanPartyPresenceValid(presence)) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    if(!presence_advances(&s->presence,presence) ||
        (s->assignment_valid && !assignment_advances(&s->assignment,&presence->ownership.assignment))) goto done;
    BOOL changed=!s->presence_valid || presence->sequence!=s->presence.sequence;
    if(!s->presence_valid || presence_input_changed(&s->presence,presence)) fence_inputs(s);
    s->presence=*presence; s->presence_valid=1; s->presence_received_at=s->now;
    s->assignment=presence->ownership.assignment; s->assignment_valid=1;
    for(unsigned i=1;i<4;++i) {
        if(changed) s->peer[i].last_presence_sent_at=0;
        service_presence_transport(s,&s->peer[i]);
    }
    ok=TRUE;
done:
    ReleaseSRWLockExclusive(&s->lock); return ok;
}
BOOL SudekiMpLanPartyGetPresence(SudekiMpLanPartySession *s,
    SudekiMpLanPartyPresence *presence) {
    BOOL ok;
    if(!s || !presence || s->config.story_observation==1u) return FALSE;
    AcquireSRWLockShared(&s->lock);
    ok=s->presence_valid && (s->config.story_observation!=2u ||
        (uint32_t)(GetTickCount()-s->presence_received_at)<=250u);
    if(ok) *presence=s->presence;
    ReleaseSRWLockShared(&s->lock); return ok;
}
BOOL SudekiMpLanPartySetAssignment(SudekiMpLanPartySession *s,
    const SudekiMpPartyAssignment *assignment) {
    BOOL ok=FALSE;
    if(!s || s->config.local_seat || s->config.story_observation==1u ||
        !SudekiMpPartyAssignmentValid(assignment)) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    if(s->assignment_valid && !assignment_advances(&s->assignment,assignment)) goto done;
    if(!s->assignment_valid || s->assignment.revision!=assignment->revision) {
        fence_inputs(s); s->presence_valid=0;
    }
    s->assignment=*assignment; s->assignment_valid=1; s->config.assignment_enabled=1;
    ok=TRUE;
done:
    ReleaseSRWLockExclusive(&s->lock); return ok;
}
BOOL SudekiMpLanPartyRegisterAdmission(SudekiMpLanPartySession *s,unsigned player,uint64_t nonce) {
    BOOL ok=FALSE;
    if(!s || s->config.local_seat || s->config.story_observation || !player ||
        player>=4 || !nonce) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    if(!s->config.lobby_members || s->peer[player].status.phase!=SUDEKIMP_LAN_PARTY_FREE ||
        ((s->config.lobby_members&(1u<<player)) && s->config.lobby_nonce[player]!=nonce)) goto done;
    for(unsigned i=0;i<4;++i)
        if(i!=player && s->config.lobby_nonce[i]==nonce) goto done;
    s->config.lobby_members|=(uint8_t)(1u<<player);
    s->config.lobby_nonce[player]=nonce; ok=TRUE;
done:
    ReleaseSRWLockExclusive(&s->lock); return ok;
}
BOOL SudekiMpLanPartyRegisterStoryAdmission(SudekiMpLanPartySession *s,unsigned player,
    unsigned character,uint64_t nonce) {
    if(!s || s->config.local_seat || s->config.story_observation!=2u || !player ||
        player>=4u || character>=4u || !nonce) return FALSE;
    AcquireSRWLockExclusive(&s->lock); BOOL ok=FALSE;
    if(!s->config.assignment_enabled || !s->config.lobby_members ||
        s->peer[player].status.phase!=SUDEKIMP_LAN_PARTY_FREE ||
        (s->config.lobby_members&(1u<<player)) ||
        (s->assignment_valid && s->assignment.character[player]!=character)) goto done;
    for(unsigned i=0;i<4u;++i) if(i!=player &&
        (s->config.lobby_nonce[i]==nonce || player_character_locked(s,i)==character)) goto done;
    s->config.character[player]=(uint8_t)character;
    s->config.reserved_mask|=(uint8_t)(1u<<player);
    s->config.lobby_members|=(uint8_t)(1u<<player); s->config.lobby_nonce[player]=nonce;
    memset(&s->catchup[player],0,sizeof(s->catchup[player]));
    s->catchup[player].required=1; ok=TRUE;
done:
    ReleaseSRWLockExclusive(&s->lock); return ok;
}
BOOL SudekiMpLanPartyPublishStoryCatchup(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyLease *lease,const SudekiMpLanStoryCatchup *snapshot) {
    if(!s || s->config.local_seat || s->config.story_observation!=2u ||
        !SudekiMpLanStoryCatchupValid(snapshot)) return FALSE;
    AcquireSRWLockExclusive(&s->lock); Peer *p=leased(s,lease); BOOL ok=FALSE;
    uint32_t now=GetTickCount();
    if(!p || p->status.phase!=SUDEKIMP_LAN_PARTY_OBSERVING || !p->status.transport_confirmed ||
        !story_control_fresh(now,s->story_scene.observed_tick) ||
        !SudekiMpLanStoryCatchupMatches(snapshot,&s->story_scene)) goto done;
    StoryCatchupSlot *a=&s->catchup[lease->seat];
    if(!a->required || (a->transaction && a->transaction!=snapshot->transaction)) goto done;
    if(!a->valid) {
        size_t size=0;
        if(!story_control_fresh(now,snapshot->target.observed_tick) || !p->story_last_frame ||
            !SudekiMpLanStoryCatchupEncode(snapshot,a->bytes,sizeof(a->bytes),&size)) goto done;
        a->size=(uint32_t)size; a->digest=catchup_digest(a->bytes,size);
        a->snapshot=*snapshot; a->transaction=snapshot->transaction;
        a->frame_floor=p->story_last_frame; a->valid=1;
    }
    ok=TRUE;
    if(a->acknowledged || (a->last_sent_at && (uint32_t)(now-a->last_sent_at)<250u)) goto done;
    unsigned chunks=(a->size+SUDEKIMP_STORY_CATCHUP_FRAGMENT_SIZE-1u)/SUDEKIMP_STORY_CATCHUP_FRAGMENT_SIZE;
    for(unsigned i=0;i<chunks;++i) {
        uint8_t bytes[SUDEKIMP_STORY_CATCHUP_HEADER_SIZE+SUDEKIMP_STORY_CATCHUP_FRAGMENT_SIZE];
        unsigned offset=i*SUDEKIMP_STORY_CATCHUP_FRAGMENT_SIZE,n=a->size-offset;
        if(n>SUDEKIMP_STORY_CATCHUP_FRAGMENT_SIZE) n=SUDEKIMP_STORY_CATCHUP_FRAGMENT_SIZE;
        put32(bytes,a->transaction); put32(bytes+4,a->size); put32(bytes+8,a->digest); put32(bytes+12,i);
        memcpy(bytes+SUDEKIMP_STORY_CATCHUP_HEADER_SIZE,a->bytes+offset,n);
        if(!send_peer(s,p,MSG_STORY_CATCHUP,next_sequence(p),0,bytes,SUDEKIMP_STORY_CATCHUP_HEADER_SIZE+n)) ok=FALSE;
    }
    a->last_sent_at=now;
done:
    ReleaseSRWLockExclusive(&s->lock); return ok;
}
BOOL SudekiMpLanPartyGetStoryCatchup(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyLease *lease,uint32_t now,SudekiMpLanStoryCatchup *snapshot) {
    if(!s || !s->config.local_seat || s->config.story_observation!=2u || !snapshot ||
        !lease || lease->seat!=s->config.local_seat) return FALSE;
    AcquireSRWLockShared(&s->lock); Peer *p=leased(s,lease);
    StoryCatchupSlot *a=&s->catchup[lease->seat];
    BOOL ok=p && p->status.phase==SUDEKIMP_LAN_PARTY_OBSERVING && a->valid &&
        story_control_fresh(now,s->story_scene_received_at) &&
        SudekiMpLanStoryCatchupMatches(&a->snapshot,&s->story_scene);
    if(ok) *snapshot=a->snapshot;
    ReleaseSRWLockShared(&s->lock); return ok;
}
BOOL SudekiMpLanPartyAcknowledgeStoryCatchup(SudekiMpLanPartySession *s,
    const SudekiMpLanPartyLease *lease,uint32_t transaction,uint32_t frame) {
    if(!s || !s->config.local_seat || s->config.story_observation!=2u || !lease ||
        lease->seat!=s->config.local_seat || !transaction || !frame) return FALSE;
    AcquireSRWLockExclusive(&s->lock); Peer *p=leased(s,lease);
    StoryCatchupSlot *a=&s->catchup[lease->seat]; uint32_t now=GetTickCount();
    BOOL ok=p && p->status.phase==SUDEKIMP_LAN_PARTY_OBSERVING && a->valid &&
        a->transaction==transaction && story_control_fresh(now,s->story_scene_received_at) &&
        story_control_fresh(now,s->story_frame_received_at) &&
        !sequence_ahead(frame,s->story_frame_sequence) && (uint32_t)(s->story_frame_sequence-frame)<=128u &&
        SudekiMpLanStoryCatchupMatches(&a->snapshot,&s->story_scene);
    if(ok) {
        uint8_t bytes[16]; put32(bytes,transaction); put32(bytes+4,a->snapshot.target.epoch);
        put32(bytes+8,a->snapshot.target.revision); put32(bytes+12,frame);
        ok=send_peer(s,p,MSG_STORY_CATCHUP_ACK,next_sequence(p),0,bytes,sizeof(bytes));
    }
    ReleaseSRWLockExclusive(&s->lock); return ok;
}
BOOL SudekiMpLanPartyStoryCatchupComplete(SudekiMpLanPartySession *s,const SudekiMpLanPartyLease *lease) {
    if(!s || s->config.local_seat || s->config.story_observation!=2u) return FALSE;
    AcquireSRWLockShared(&s->lock); Peer *p=leased(s,lease);
    BOOL ok=p && p->status.phase==SUDEKIMP_LAN_PARTY_OBSERVING && p->status.transport_confirmed &&
        (!s->catchup[lease->seat].required || s->catchup[lease->seat].acknowledged);
    ReleaseSRWLockShared(&s->lock); return ok;
}
BOOL SudekiMpLanPartyRevokeAdmission(SudekiMpLanPartySession *s,unsigned player,uint64_t nonce) {
    BOOL ok=FALSE;
    if(!s || s->config.local_seat || s->config.story_observation==1u || !player ||
        player>=4 || !nonce) return FALSE;
    AcquireSRWLockExclusive(&s->lock);
    if(s->peer[player].status.phase==SUDEKIMP_LAN_PARTY_FREE &&
        (((s->config.lobby_members&(1u<<player)) && s->config.lobby_nonce[player]==nonce) ||
         (!(s->config.lobby_members&(1u<<player)) && !s->config.lobby_nonce[player]))) {
        s->config.lobby_members&=(uint8_t)~(1u<<player);
        s->config.lobby_nonce[player]=0; ok=TRUE;
        if(s->config.story_observation==2u) {
            s->config.character[player]=4u; s->config.reserved_mask&=(uint8_t)~(1u<<player);
            memset(&s->catchup[player],0,sizeof(s->catchup[player]));
        }
    }
    ReleaseSRWLockExclusive(&s->lock); return ok;
}
BOOL SudekiMpLanPartyAdmissionMatches(SudekiMpLanPartySession *s,unsigned player,uint64_t nonce) {
    if(!s || s->config.local_seat || !player || player>=4 || !nonce) return FALSE;
    AcquireSRWLockShared(&s->lock);
    BOOL okay=(s->config.lobby_members&(1u<<player)) && s->config.lobby_nonce[player]==nonce;
    ReleaseSRWLockShared(&s->lock); return okay;
}
