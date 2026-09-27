#include "network/lan_party_session.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <bcrypt.h>
#include <stdlib.h>
#include <string.h>

enum {
    HEADER = 28, LEGACY_HEADER = 20, HELLO_SIZE = 47,
    MAX_DATAGRAM = 1468, MAX_POLL = 64, FRAME_QUEUE = 8,
    ASSEMBLIES = 4, HELLO_INTERVAL = 300, KEEPALIVE_INTERVAL = 250,
    MSG_HELLO = 1, MSG_ACK, MSG_REJECT, MSG_INPUT, MSG_FRAME, MSG_END,
    MSG_KEEPALIVE, MSG_EXTENSION_CAPS, MSG_EXTENSION_CAPS_ACK,
    MSG_EXTENSION_READY, MSG_INPUT_EXTENDED, MSG_AILISH_STATE,
    MSG_EXTENSION_READY_ACK,
    INPUT_EXTENSION_SIZE = 4, AILISH_STATE_SIZE = 10,
    ASSEMBLY_CHUNK_MASK = 0x03, ASSEMBLY_AILISH_MASK = 0x04
};

_Static_assert(SUDEKIMP_LAN_ARENA_PROTOCOL_VERSION == 42u,
    "Version the party envelope when its embedded actor codec changes");
_Static_assert(HEADER + SUDEKIMP_LAN_ARENA_MAX_SNAPSHOT_PACKET_SIZE -
    LEGACY_HEADER <= MAX_DATAGRAM, "Party chunk exceeds path MTU");

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
    uint8_t input_pending;
    uint8_t extension_offered;
    uint8_t extension_acked;
    uint8_t extension_ready;
    uint8_t extension_flags;
    SudekiMpLanArenaInput input;
    SudekiMpLanPartyCombatInputExtension combat;
} Peer;

typedef struct Assembly {
    uint32_t sequence;
    uint8_t mask;
    SudekiMpLanPartyFrame frame;
} Assembly;

struct SudekiMpLanPartySession {
    SRWLOCK lock;
    SOCKET socket;
    SudekiMpLanPartyConfig config;
    Peer peer[SUDEKIMP_LAN_PARTY_PLAYERS];
    uint32_t generations[SUDEKIMP_LAN_PARTY_PLAYERS];
    uint32_t last_hello_at;
    uint32_t last_frame_sequence;
    uint32_t last_frame_tick;
    uint8_t last_match_state;
    uint32_t now;
    Assembly assembly[ASSEMBLIES];
    SudekiMpLanPartyFrame frames[FRAME_QUEUE];
    unsigned int frame_head, frame_count;
};

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
BOOL SudekiMpLanPartyFrameValid(const SudekiMpLanPartyFrame *frame) {
    if (!frame || !SudekiMpLanArenaSnapshotValidForRoster(&frame->chunk[0], &rosters[0]) ||
        !SudekiMpLanArenaSnapshotValidForRoster(&frame->chunk[1], &rosters[1])) return FALSE;
    if (frame->ailish_weapon.valid > 1u ||
        (frame->ailish_weapon.valid &&
         (frame->ailish_weapon.stage > 6u || frame->ailish_weapon.item < 12u ||
          frame->ailish_weapon.item >= 24u || frame->ailish_weapon.charge_q8 > 25600u ||
          frame->ailish_weapon.reload_ms > 60000u || !frame->ailish_weapon.reload_sequence)) ||
        (!frame->ailish_weapon.valid &&
         (frame->ailish_weapon.stage || frame->ailish_weapon.item ||
          frame->ailish_weapon.charge_q8 || frame->ailish_weapon.reload_ms ||
          frame->ailish_weapon.reload_sequence))) return FALSE;
    for (unsigned part = 0; part < 2; ++part)
        for (unsigned v = 0; v < frame->chunk[part].spirit_vfx_count; ++v) {
            uint8_t owner = frame->chunk[part].spirit_vfx[v].owner_actor_type;
            if (owner && owner != actors[part * 2] && owner != actors[part * 2 + 1])
                return FALSE;
        }
    return frame->chunk[0].host_tick == frame->chunk[1].host_tick &&
        frame->chunk[0].match_state == frame->chunk[1].match_state &&
        frame->chunk[0].combat_enabled == frame->chunk[1].combat_enabled &&
        frame->chunk[0].sequence == frame->chunk[1].sequence &&
        frame->chunk[0].acknowledged_input == frame->chunk[1].acknowledged_input &&
        frame->chunk[1].enemy_count == 0u;
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
static void reject(SudekiMpLanPartySession *s, const struct sockaddr_in *source,
    unsigned seat, uint64_t nonce, SudekiMpLanArenaRejectReason reason) {
    uint8_t body[9]; put64(body, nonce); body[8] = (uint8_t)reason;
    (void)send_message(s, source, MSG_REJECT, seat, 0, 0, 0, 0, body, sizeof(body));
}
static void drain(SudekiMpLanPartySession *s, Peer *p, SudekiMpLanArenaRejectReason why) {
    p->status.phase = SUDEKIMP_LAN_PARTY_DRAINING;
    p->status.failure = why;
    p->input_pending = 0; memset(&p->input, 0, sizeof(p->input));
    memset(&p->combat, 0, sizeof(p->combat));
    p->extension_offered = p->extension_acked = p->extension_ready =
        p->extension_flags = 0;
    if (s->config.local_seat) {
        memset(s->assembly, 0, sizeof(s->assembly));
        s->frame_count = s->frame_head = 0;
    }
}

static void hello_received(SudekiMpLanPartySession *s, Peer *p,
    unsigned seat, const struct sockaddr_in *source, const uint8_t *body) {
    uint64_t nonce = get64(body);
    SudekiMpLanArenaRejectReason reason = SUDEKIMP_LAN_ARENA_REJECT_NONE;
    if (!nonce) return;
    if (get32(body + 8) != SUDEKIMP_LAN_PARTY_BUILD_ID ||
        get16(body + 45) != SUDEKIMP_LAN_ARENA_PROTOCOL_VERSION)
        reason = SUDEKIMP_LAN_ARENA_REJECT_BUILD;
    else if (memcmp(body + 12, s->config.game_hash, 32)) reason = SUDEKIMP_LAN_ARENA_REJECT_GAME_HASH;
    else if (body[44] != SUDEKIMP_LAN_ARENA_MAP_CLEANROOM) reason = SUDEKIMP_LAN_ARENA_REJECT_MAP;
    if (reason) { reject(s, source, seat, nonce, reason); return; }
    if (p->status.phase != SUDEKIMP_LAN_PARTY_FREE) {
        if (same_address(source, &p->address) && nonce == p->nonce &&
            (p->status.phase == SUDEKIMP_LAN_PARTY_PENDING ||
             p->status.phase == SUDEKIMP_LAN_PARTY_ACTIVE)) {
            p->last_received_at = s->now;
            send_ack(s, p); /* Lost ACK: never allocate another actor/generation. */
            if (!p->extension_ready) send_extension_caps(s, p);
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
    send_extension_caps(s, p);
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
        combat->block_held <= 1u &&
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
static void accept_frame(SudekiMpLanPartySession *s, unsigned part,
    const SudekiMpLanArenaSnapshot *frame) {
    Assembly *a = assembly_for(s, frame->sequence);
    if (!a) return;
    /* Duplicate halves cannot replace already admitted data. */
    if (a->mask & (1u << part)) return;
    a->frame.chunk[part] = *frame; a->mask |= (uint8_t)(1u << part);
    Peer *p = &s->peer[s->config.local_seat];
    unsigned required = ASSEMBLY_CHUNK_MASK;
    if (p->extension_ready &&
        (p->extension_flags & SUDEKIMP_LAN_PARTY_EXTENSION_AILISH_WEAPON))
        required |= ASSEMBLY_AILISH_MASK;
    accept_assembly(s, a, required);
}
static void accept_ailish_state(SudekiMpLanPartySession *s, uint32_t sequence,
    const SudekiMpLanPartyAilishWeaponState *state) {
    Assembly *a = assembly_for(s, sequence);
    Peer *p = &s->peer[s->config.local_seat];
    if (!a || (a->mask & ASSEMBLY_AILISH_MASK)) return;
    a->frame.ailish_weapon = *state;
    a->mask |= ASSEMBLY_AILISH_MASK;
    p->extension_ready = 1;
    accept_assembly(s, a, ASSEMBLY_CHUNK_MASK | ASSEMBLY_AILISH_MASK);
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
        kind > MSG_EXTENSION_READY_ACK ||
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
            (body[8] != SUDEKIMP_LAN_PARTY_PENDING && body[8] != SUDEKIMP_LAN_PARTY_ACTIVE) ||
            (p->status.phase != SUDEKIMP_LAN_PARTY_JOINING &&
             p->status.phase != SUDEKIMP_LAN_PARTY_PENDING &&
             p->status.phase != SUDEKIMP_LAN_PARTY_ACTIVE)) return;
        if (p->status.phase != SUDEKIMP_LAN_PARTY_JOINING &&
            (token != p->status.lease.token || generation != p->status.lease.generation ||
             !SudekiMpLanArenaSequenceNewer(sequence, p->received_sequence))) return;
        p->status.lease.token = token; p->status.lease.generation = generation;
        p->status.transport_confirmed = 1;
        if (p->status.phase != SUDEKIMP_LAN_PARTY_ACTIVE)
            p->status.phase = (SudekiMpLanPartyPhase)body[8];
        p->last_received_at = s->now; p->received_sequence = sequence;
        return;
    }
    if ((p->status.phase != SUDEKIMP_LAN_PARTY_PENDING &&
         p->status.phase != SUDEKIMP_LAN_PARTY_ACTIVE) || !sequence ||
        !same_address(source, &p->address) || !token || !generation ||
        token != p->status.lease.token || generation != p->status.lease.generation) return;
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
        if ((state.valid &&
             (state.stage > 6u || state.item < 12u || state.item >= 24u ||
              state.charge_q8 > 25600u || state.reload_ms > 60000u ||
              !state.reload_sequence)) ||
            (!state.valid && (state.stage || state.item || state.charge_q8 ||
              state.reload_ms || state.reload_sequence)) ||
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
        memset(&combat, 0, sizeof(combat));
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
        }
        if (!nested_decode(kind, 0, body, body_size, sequence, token, &packet) ||
            packet.body.input.actor_type != actors[seat] ||
            sequence_ahead(packet.body.input.acknowledged_snapshot, p->last_sent_frame) ||
            !party_combat_extension_valid(actors[seat], &packet.body.input,
                &combat)) return;
        latch_input(p, &packet.body.input, &combat);
        if (kind == MSG_INPUT_EXTENDED) p->extension_ready = 1;
        p->status.last_input_received_at_ms = s->now;
    } else if ((kind == MSG_KEEPALIVE || kind == MSG_END) && !body_size) {
        p->status.transport_confirmed = 1;
        if (kind == MSG_END) drain(s, p, SUDEKIMP_LAN_ARENA_REJECT_NONE);
    } else return;
    p->received_sequence = sequence; p->last_received_at = s->now;
}

SudekiMpLanPartySession *SudekiMpLanPartyCreate(const SudekiMpLanPartyConfig *config) {
    WSADATA wsa;
    if (!config || config->local_seat >= SUDEKIMP_LAN_PARTY_PLAYERS ||
        config->port > 65535 || (config->local_seat && (!config->host_ipv4 || !config->port)) ||
        (config->timeout_ms && (config->timeout_ms < 500 || config->timeout_ms > 60000))) {
        SetLastError(ERROR_INVALID_PARAMETER); return NULL;
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
        if (p->status.phase == SUDEKIMP_LAN_PARTY_ACTIVE || p->status.phase == SUDEKIMP_LAN_PARTY_PENDING)
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
void SudekiMpLanPartyPoll(SudekiMpLanPartySession *s, uint32_t now) {
    if (!s) return;
    AcquireSRWLockExclusive(&s->lock); s->now = now;
    if (s->config.local_seat) {
        Peer *p = &s->peer[s->config.local_seat];
        if ((p->status.phase == SUDEKIMP_LAN_PARTY_JOINING ||
             p->status.phase == SUDEKIMP_LAN_PARTY_PENDING) &&
            (!s->last_hello_at || (uint32_t)(now - s->last_hello_at) >= HELLO_INTERVAL)) {
            uint8_t body[HELLO_SIZE]; put64(body, p->nonce);
            put32(body + 8, SUDEKIMP_LAN_PARTY_BUILD_ID);
            memcpy(body + 12, s->config.game_hash, 32);
            body[44] = SUDEKIMP_LAN_ARENA_MAP_CLEANROOM;
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
        if (p->status.phase != SUDEKIMP_LAN_PARTY_ACTIVE &&
            p->status.phase != SUDEKIMP_LAN_PARTY_PENDING &&
            p->status.phase != SUDEKIMP_LAN_PARTY_JOINING) continue;
        if ((uint32_t)(now - p->last_received_at) > s->config.timeout_ms) {
            drain(s, p, SUDEKIMP_LAN_ARENA_REJECT_TIMEOUT); continue;
        }
        if (p->status.phase != SUDEKIMP_LAN_PARTY_JOINING &&
            (uint32_t)(now - p->last_sent_at) >= KEEPALIVE_INTERVAL)
            (void)send_peer(s, p, MSG_KEEPALIVE, next_sequence(p), 0, NULL, 0);
        if (!s->config.local_seat && !p->extension_ready &&
            (p->status.phase == SUDEKIMP_LAN_PARTY_PENDING ||
             p->status.phase == SUDEKIMP_LAN_PARTY_ACTIVE) &&
            (!p->last_extension_offer_at ||
             (uint32_t)(now - p->last_extension_offer_at) >= KEEPALIVE_INTERVAL))
            send_extension_caps(s, p);
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
    if (!s || s->config.local_seat) return FALSE;
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
    BOOL ok = p && (p->status.phase == SUDEKIMP_LAN_PARTY_ACTIVE || p->status.phase == SUDEKIMP_LAN_PARTY_PENDING);
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
BOOL SudekiMpLanPartyTakeInput(SudekiMpLanPartySession *s, unsigned seat,
    SudekiMpLanPartyInput *input) {
    if (!s || s->config.local_seat || !input || !seat || seat >= SUDEKIMP_LAN_PARTY_PLAYERS) return FALSE;
    AcquireSRWLockExclusive(&s->lock); Peer *p = &s->peer[seat];
    BOOL ok = p->status.phase == SUDEKIMP_LAN_PARTY_ACTIVE && p->input_pending;
    if (ok && (uint32_t)(s->now - p->status.last_input_received_at_ms) >
        SUDEKIMP_LAN_PARTY_INPUT_MAX_AGE_MS) {
        p->input_pending = 0; ok = FALSE;
    }
    if (ok) {
        input->lease = p->status.lease; input->input = p->input;
        input->combat = p->combat;
        input->received_at_ms = p->status.last_input_received_at_ms;
        p->last_taken_input = p->input.sequence; p->input_pending = 0;
    }
    ReleaseSRWLockExclusive(&s->lock); return ok;
}
BOOL SudekiMpLanPartyAdmitInput(SudekiMpLanPartySession *s, const SudekiMpLanPartyInput *input) {
    if (!s || s->config.local_seat || !input) return FALSE;
    AcquireSRWLockExclusive(&s->lock); Peer *p = leased(s, &input->lease);
    BOOL ok = p && p->status.phase == SUDEKIMP_LAN_PARTY_ACTIVE &&
        input->input.actor_type == actors[input->lease.seat] &&
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
    if (!s || !s->config.local_seat || !input ||
        input->actor_type != actors[s->config.local_seat] ||
        (extended && (!combat || !party_combat_extension_valid(
            input->actor_type,input,combat)))) return FALSE;
    p = &s->peer[s->config.local_seat];
    if (p->status.phase != SUDEKIMP_LAN_PARTY_ACTIVE ||
        (extended && (!p->extension_ready ||
         !(p->extension_flags & SUDEKIMP_LAN_PARTY_EXTENSION_COMBAT_INPUT))))
        return FALSE;
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
        body_size += INPUT_EXTENSION_SIZE;
    }
    BOOL ok = send_peer(s,p,extended ? MSG_INPUT_EXTENDED : MSG_INPUT,
        packet.sequence,0,bytes+LEGACY_HEADER,body_size);
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
BOOL SudekiMpLanPartySendFrame(SudekiMpLanPartySession *s, const SudekiMpLanPartyFrame *frame) {
    if (!s || s->config.local_seat || !SudekiMpLanPartyFrameValid(frame)) return FALSE;
    AcquireSRWLockExclusive(&s->lock); BOOL any = FALSE, all = TRUE;
    for (unsigned seat = 1; seat < SUDEKIMP_LAN_PARTY_PLAYERS; ++seat) {
        Peer *p = &s->peer[seat]; if (p->status.phase != SUDEKIMP_LAN_PARTY_ACTIVE) continue;
        uint32_t sequence = next_sequence(p); BOOL sent = TRUE; any = TRUE;
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
