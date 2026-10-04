#include "hooks/lan_arena_spirit_visual_host.h"
#include "hooks/call_hook.h"
#include "cleanroom/engine.h"
#include "engine/log.h"
#include <math.h>
#include <stddef.h>
#include <string.h>
#include <wincrypt.h>

#include "network/lan_arena_protocol.h"

static uint8_t seat_host_type(void) {
    uint8_t host_type = 0u, client_type = 0u;
    return SudekiMpLanArenaSeatActorTypes(&host_type, &client_type)
        ? host_type : SUDEKIMP_LAN_ARENA_TAL_TYPE;
}
static uint8_t seat_client_type(void) {
    uint8_t host_type = 0u, client_type = 0u;
    return SudekiMpLanArenaSeatActorTypes(&host_type, &client_type)
        ? client_type : SUDEKIMP_LAN_ARENA_AILISH_TYPE;
}

enum {
    RVA_FINALIZE = 0x18830u,
    RVA_ANIMATION_EMIT = 0xe2810u,
    RVA_SCRIPT_PARENT_CALL = 0x18f2bu,
    RVA_PARENT_CREATE = 0x18c90u,
    RVA_WEAK_BIND = 0x1750u,
    RVA_WEAK_DESTROY = 0x4d30u,
    RVA_EFFECT_VTABLE = 0x2d3c7cu,
    RVA_EFFECT_COMPONENT_VTABLE = 0x2c83f4u,
    RVA_POSITION_VTABLE = 0x2cdefcu,
    RVA_RENDERER_VTABLE = 0x2df8ecu,
    RVA_WORLD_MATRIX = 0x111cc0u,
    ENTRY_PENDING = 1,
    ENTRY_READY = 2
};

static const uint32_t resource_ids[] = {
    0u, 0x3cef3b8fu, 0xb5a0cf01u, 0x03439ed3u, 0xb5661565u,
    0x903afa53u, 0x2e5a867bu, 0x4d727a05u, 0x449d201bu,
    0xf007401bu, 0xc24c6a03u, 0xa8171ecfu, 0x62dcc5a3u, 0xaeec0c83u,
    0x423bad0du, 0xc198e72du, 0x26de2bb3u, 0xc6cf803bu, 0x07f9bc13u,
    0x920d7163u, 0x0fdb430fu,
    0x18a0da0fu, 0x34b6a981u, 0xafbcfa53u, 0x55f90a03u, 0xf3c381cfu,
    0xe2711ed3u, 0xec9a809bu, 0xb0a51d13u, 0xa85bf815u, 0x4ed84d5bu, 0x7eae7163u, 0x8e21830fu
};
static const char *const resource_names[] = {
    NULL, "SFXSS250_INITIATE", "SFXSS251_INITIATE_LOOP_WAIT",
    "SFXSS112_SMALL_FLOOR_PATTERN", "SFXSS800_SPIRIT_A2T",
    "SFXSS252_MORPH_INTO_SPIRIT", "SFXSS801_SPIRIT_LINK",
    "SFXSS802_SPIRIT_END", "SFXSS300_TAL_SPIRIT_STRIKE",
    "SFXSS350_TAL_SPIRIT_STRIKE", "SFXSS110_LOOP_INVULNERABLE",
    "SFXSS111_END_INVULNERABLE", "SFXSS900_GENERIC_INITATE",
    "SFXSS351_TAL_HIT_CHARACTER", "SFXSTA003_BOOST",
    "SFXSS450_BUKI_SS_STRIKE", "SFXSS500_BUKI_SS_SPELL",
    "SFXSS451_PROJECTILE_HIT_CHARACTER", "SFXSS501_HIT",
    "SFXB200_SHIELD_APPEAR", "SFXB201_SHIELD_LOOP",
    "SFXSS550_INITIATE", "SFXSS551_INITIATE_LOOP_WAIT", "SFXSS552_MORPH_INTO_SPIRIT",
    "SFXSS560_LOOP_INVULNERABLE", "SFXSS561_END_INVULNERABLE", "SFXSS562_SMALL_FLOOR_PATTERN",
    "SFXSS600_RAFFI_SPIRIT_STRIKE", "SFXSS601_HIT", "SFXSS650_RAFFI_SS_SPELL", "SFXSS651_HASTE_PCS",
    "SFXT200_SHIELD_APPEAR", "SFXT201_SHIELD_LOOP"
};
enum { RESOURCE_TEXT_CAPACITY = 64, DIAGNOSTIC_CAPACITY = 16 };
static const uint8_t finalize_prefix[] = {
    0x55,0x8b,0xec,0x83,0xe4,0xf8,0x83,0xec,0x14,0x53,0x56,0x57,
    0x8b,0xf8,0x8b,0x47,0x1c,0x85,0xc0,0x0f,0x84,0xd3,0x01,0x00,0x00
};
static const uint8_t animation_emit_prefix[] = {
    0x55,0x8b,0xec,0x83,0xe4,0xf0,0x83,0xec,0x64,0x8b,0x44,0x24,0x08
};
static const uint8_t animation_emit_tail[] = {
    0x5f,0x5e,0x5b,0x8b,0xe5,0x5d,0xc2,0x0c,0x00
};
static const uint8_t weak_bind_body[] = {
    0x8b,0x08,0x85,0xc9,0x74,0x35,0x57,0x39,0x41,0x04,0x75,0x06,
    0x8b,0x78,0x08,0x89,0x79,0x04,0x8b,0x48,0x04,0x85,0xc9,0x74,
    0x06,0x8b,0x78,0x08,0x89,0x79,0x08,0x8b,0x48,0x08,0x85,0xc9,
    0x74,0x06,0x8b,0x78,0x04,0x89,0x79,0x04,0xc7,0x40,0x08,0,0,0,0,
    0xc7,0x40,0x04,0,0,0,0,0x5f,0x89,0x10,0x85,0xd2,0x74,0x13,
    0x8b,0x4a,0x04,0x85,0xc9,0x74,0x03,0x89,0x41,0x04,0x8b,0x4a,
    0x04,0x89,0x48,0x08,0x89,0x42,0x04,0xc3
};
static const uint8_t world_matrix_prefix[] = {
    0x55,0x8b,0xec,0x83,0xe4,0xf8,0x51,0x56,0x8b,0xf1,
    0x8b,0x86,0x94,0,0,0,0x85,0xc0,0x74,0x05,0x83,0xc0,0xfc,0x75,0x11
};
static const uint8_t weak_null_tail[] = {
    0x89,0x30,0x89,0x70,0x08,0x89,0x70,0x04,0x8b,0xc2,0x3b,0xd6,
    0x75,0xc1,0x5f,0x5e,0xc3
};

static SudekiMpInlineHook finalize_hook;
static SudekiMpInlineHook animation_emit_hook;
static SudekiMpRelativeCallHook script_parent_hook;
static SudekiMpInlineHook lifetime_factory_hook,lifetime_forward_hook,lifetime_pump_hook;
enum { LIFETIME_CAPACITY=512 };
typedef struct EffectLifetime {
    SudekiMpSpiritVisualWeakNode weak;
    SudekiMpLanPartyEffectOwner owner;
    BOOL registered;
} EffectLifetime;
static EffectLifetime lifetimes[LIFETIME_CAPACITY];
static SudekiMpLanPartyEffectWitness lifetime_witness;
static BOOL (*lifetime_shutdown_drained)(void);
static uint8_t lifetime_retire_code[0x107];
#ifdef SUDEKIMP_SPIRIT_VISUAL_HOST_TESTING
static void (*lifetime_test_retire)(void *);
void SudekiMpLanPartyEffectLifetimeTestRetire(void (*callback)(void *)) {
    lifetime_test_retire=callback;
}
#endif
static const SudekiMpLanPartyEffectOwner *lifetime_scope;
static volatile LONG lifetime_count,lifetime_unknown,lifetime_depth;
static BOOL lifetime_only;
static BOOL lifetime_source(void *component,SudekiMpLanPartyEffectOwner *owner);
typedef struct EmissionSource {
    uint64_t session;
    uint16_t sequence;
    uint32_t tick;
    uint8_t owner;
    BOOL valid;
} EmissionSource;
typedef struct PendingEmission {
    SudekiMpSpiritVisualWeakNode weak;
    EmissionSource source;
} PendingEmission;
/* Stable native weak nodes retain attribution across asynchronous loading.
 * Never cache a naked effect or infer its caster from whoever is active later. */
static PendingEmission pending_emissions[SUDEKIMP_SPIRIT_VISUAL_HOST_REGISTRY_CAPACITY];
static const EmissionSource *emission_source;
static SudekiMpSpiritVisualHostRegistry registry;
static HMODULE image;
static DWORD game_thread;
static SudekiMpLanArenaSpiritVisualHostWitness active_witness;
static void *witness_context;
static SudekiMpLanPartyShieldHostWitness shield_witness;
static volatile LONG admitted;
static volatile LONG in_flight;
static volatile LONG unexpected_thread;
static BOOL pinned;
static BOOL session_armed;
static const char *sample_reason;
static const char *last_capture_reason;
static int last_capture_count = -1;
static uint32_t diagnostic_ids[DIAGNOSTIC_CAPACITY];
static uint32_t diagnostic_types[DIAGNOSTIC_CAPACITY];
static unsigned int diagnostic_count;
static uint16_t diagnostic_skill;
static uint64_t inactive_kind_mask;
_Static_assert(SUDEKIMP_LAN_PARTY_VFX_LAST < 64u,
    "inactive diagnostics need one bit per closed visual kind");
_Static_assert(sizeof(resource_ids) / sizeof(resource_ids[0]) ==
    SUDEKIMP_LAN_PARTY_VFX_LAST + 1u, "closed visual ID table mismatch");
_Static_assert(sizeof(resource_names) / sizeof(resource_names[0]) ==
    SUDEKIMP_LAN_PARTY_VFX_LAST + 1u, "closed visual name table mismatch");

uint8_t SudekiMpSpiritVisualKindForResource(uint32_t identifier) {
    unsigned int i;
    for (i = 1u; i < sizeof(resource_ids) / sizeof(resource_ids[0]); ++i)
        if (resource_ids[i] == identifier) return (uint8_t)i;
    return 0u;
}

uint8_t SudekiMpSpiritVisualKindForTypedResource(
    uint32_t encoded_kind, uint32_t identifier,
    const char *text, size_t text_size
) {
    char upper[RESOURCE_TEXT_CAPACITY];
    uint32_t hash = 0u;
    size_t i, length;
    uint8_t kind = SudekiMpSpiritVisualKindForResource(identifier);
    if (kind != 0u) return kind;
    if ((encoded_kind & 0x7fu) != 0x29u || text == NULL ||
        text_size < 2u || text_size > sizeof(upper) || text[text_size - 1u] != '\0')
        return 0u;
    length = text_size - 1u;
    for (i = 0u; i < length; ++i) {
        unsigned char c = (unsigned char)text[i];
        if (c == 0u || c > 0x7fu) return 0u;
        if (c >= 'a' && c <= 'z') c = (unsigned char)(c - 'a' + 'A');
        upper[i] = (char)c;
        hash = (i & 1u) != 0u ? hash * c : hash + c;
    }
    if (hash != identifier) return 0u;
    upper[length] = '\0';
    /* Native typed gfx 43f800 -> 5b9510 normalizes retained text to .hom.
     * Unlike the cache identifier, component+2c retains the input RN exactly
     * (418830 -> 4df820 -> 404bc0). No bare hash without text is an alias. */
    if (length >= 4u && strcmp(upper + length - 4u, ".HOM") == 0)
        upper[length - 4u] = '\0';
    for (kind = 1u; kind < sizeof(resource_names)/sizeof(resource_names[0]); ++kind)
        if (strcmp(upper, resource_names[kind]) == 0) return kind;
    return 0u;
}

static void unknown_output(SudekiMpLanArenaSnapshot *output) {
    if (output == NULL) return;
    output->spirit_vfx_observed = 0u;
    output->spirit_vfx_count = 0u;
    memset(output->spirit_vfx, 0, sizeof(output->spirit_vfx));
}

BOOL SudekiMpSpiritVisualDecomposeMatrix(
    const float matrix[16], SudekiMpLanArenaSpiritVfxSnapshot *value
) {
    float m[9], q[4], norm, s, trace, determinant;
    unsigned int row, column;
    if (matrix == NULL || value == NULL) return FALSE;
    for (row = 0; row < 16u; ++row)
        if (!isfinite(matrix[row])) return FALSE;
    if (fabsf(matrix[3]) > 0.001f || fabsf(matrix[7]) > 0.001f ||
        fabsf(matrix[11]) > 0.001f || fabsf(matrix[15] - 1.0f) > 0.001f)
        return FALSE;
    for (row = 0; row < 3u; ++row) {
        s = sqrtf(matrix[row * 4u] * matrix[row * 4u] +
            matrix[row * 4u + 1u] * matrix[row * 4u + 1u] +
            matrix[row * 4u + 2u] * matrix[row * 4u + 2u]);
        if (!isfinite(s) || s < 0.0001f || s > 1000.0f ||
            fabsf(matrix[12u + row]) > 1000000.0f) return FALSE;
        value->scale[row] = s;
        value->position[row] = matrix[12u + row];
        for (column = 0; column < 3u; ++column)
            m[row * 3u + column] = matrix[row * 4u + column] / s;
    }
    for (row = 0; row < 3u; ++row)
        for (column = row + 1u; column < 3u; ++column)
            if (fabsf(m[row * 3u] * m[column * 3u] +
                    m[row * 3u + 1u] * m[column * 3u + 1u] +
                    m[row * 3u + 2u] * m[column * 3u + 2u]) > 0.002f)
                return FALSE; /* Shear cannot be represented by this wire pose. */
    determinant = m[0] * (m[4] * m[8] - m[5] * m[7]) -
        m[1] * (m[3] * m[8] - m[5] * m[6]) +
        m[2] * (m[3] * m[7] - m[4] * m[6]);
    if (determinant < 0.998f || determinant > 1.002f) return FALSE;
    /* D3DX uses row vectors: transpose the usual column-matrix extraction. */
    trace = m[0] + m[4] + m[8];
    if (trace > 0.0f) {
        s = sqrtf(trace + 1.0f) * 2.0f;
        q[0] = (m[5] - m[7]) / s; q[1] = (m[6] - m[2]) / s;
        q[2] = (m[1] - m[3]) / s; q[3] = 0.25f * s;
    } else if (m[0] > m[4] && m[0] > m[8]) {
        s = sqrtf(1.0f + m[0] - m[4] - m[8]) * 2.0f;
        q[0] = 0.25f * s; q[1] = (m[1] + m[3]) / s;
        q[2] = (m[2] + m[6]) / s; q[3] = (m[5] - m[7]) / s;
    } else if (m[4] > m[8]) {
        s = sqrtf(1.0f + m[4] - m[0] - m[8]) * 2.0f;
        q[0] = (m[1] + m[3]) / s; q[1] = 0.25f * s;
        q[2] = (m[5] + m[7]) / s; q[3] = (m[6] - m[2]) / s;
    } else {
        s = sqrtf(1.0f + m[8] - m[0] - m[4]) * 2.0f;
        q[0] = (m[2] + m[6]) / s; q[1] = (m[5] + m[7]) / s;
        q[2] = 0.25f * s; q[3] = (m[1] - m[3]) / s;
    }
    norm = sqrtf(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    if (!isfinite(norm) || norm < 0.5f) return FALSE;
    if (q[3] < 0.0f) norm = -norm;
    for (row = 0; row < 4u; ++row) value->rotation_xyzw[row] = q[row] / norm;
    return TRUE;
}

static BOOL api_valid(const SudekiMpSpiritVisualHostApi *api) {
    return api != NULL && api->bind != NULL && api->sample != NULL;
}

BOOL SudekiMpSpiritVisualHostRegistryReset(
    SudekiMpSpiritVisualHostRegistry *r, const SudekiMpSpiritVisualHostApi *api
) {
    unsigned int i;
    BOOL result = TRUE;
    if (r == NULL || !api_valid(api)) return FALSE;
    for (i = 0; i < SUDEKIMP_SPIRIT_VISUAL_HOST_REGISTRY_CAPACITY; ++i) {
        SudekiMpSpiritVisualHostEntry *entry = &r->entries[i];
        if ((entry->weak.entity != NULL || entry->weak.previous != NULL ||
                entry->weak.next != NULL) &&
            !api->bind(api->context, &entry->weak, NULL)) {
            result = FALSE;
            continue; /* Never erase a possibly linked stable node. */
        }
        memset(entry, 0, sizeof(*entry));
    }
    if (result) memset(r, 0, sizeof(*r));
    else r->unknown = TRUE;
    return result;
}

unsigned int SudekiMpSpiritVisualHostRegistryBegin(
    SudekiMpSpiritVisualHostRegistry *r, uint64_t session, uint16_t skill,
    uint32_t tick, uint8_t kind, void *entity,
    const SudekiMpSpiritVisualHostApi *api
) {
    return SudekiMpSpiritVisualHostRegistryBeginOwned(
        r, session, skill, tick, kind, 0u, entity, api);
}

unsigned int SudekiMpSpiritVisualHostRegistryBeginOwned(
    SudekiMpSpiritVisualHostRegistry *r, uint64_t session, uint16_t skill,
    uint32_t tick, uint8_t kind, uint8_t owner_actor_type, void *entity,
    const SudekiMpSpiritVisualHostApi *api
) {
    unsigned int i, free_slot = SUDEKIMP_SPIRIT_VISUAL_HOST_REGISTRY_CAPACITY;
    SudekiMpSpiritVisualHostEntry *entry;
    SudekiMpLanArenaSpiritVfxSnapshot identity = {0};
    identity.kind = kind;
    identity.skill_sequence = skill;
    identity.owner_actor_type = owner_actor_type;
    if (r == NULL || !api_valid(api)) return 0u;
    if (session == 0u || !SudekiMpLanArenaVisualOwnerValidForParty(&identity) || entity == NULL || kind == 0u ||
        kind >= sizeof(resource_ids)/sizeof(resource_ids[0]) ||
        (r->session != 0u && r->session != session)) {
        r->unknown = TRUE;
        return 0u;
    }
    r->session = session;
    for (i = 0; i < SUDEKIMP_SPIRIT_VISUAL_HOST_REGISTRY_CAPACITY; ++i) {
        entry = &r->entries[i];
        if (entry->weak.entity == entity) {
            if (entry->value.kind != kind || entry->value.skill_sequence != skill ||
                entry->value.owner_actor_type != owner_actor_type)
                r->unknown = TRUE;
            return i + 1u; /* Repeated finalize does not create another instance. */
        }
        if (entry->weak.entity == NULL && entry->weak.previous == NULL &&
            entry->weak.next == NULL && free_slot ==
                SUDEKIMP_SPIRIT_VISUAL_HOST_REGISTRY_CAPACITY) free_slot = i;
    }
    if (free_slot == SUDEKIMP_SPIRIT_VISUAL_HOST_REGISTRY_CAPACITY ||
        r->next_instance == UINT32_MAX) {
        r->unknown = TRUE; /* A missed lifetime cannot be guessed back later. */
        return 0u;
    }
    entry = &r->entries[free_slot];
    memset(entry, 0, sizeof(*entry));
    entry->state = ENTRY_PENDING;
    entry->value.instance_sequence = ++r->next_instance;
    entry->value.skill_sequence = skill;
    entry->value.kind = kind;
    entry->value.owner_actor_type = owner_actor_type;
    entry->value.emitted_host_tick = tick;
    if (!api->bind(api->context, &entry->weak, entity) ||
        entry->weak.entity != entity) {
        r->unknown = TRUE;
        return 0u; /* Retain possibly installed observer for Reset to drain. */
    }
    return free_slot + 1u;
}

void SudekiMpSpiritVisualHostRegistryComplete(
    SudekiMpSpiritVisualHostRegistry *r, unsigned int token,
    BOOL native_success, const SudekiMpSpiritVisualHostApi *api
) {
    SudekiMpSpiritVisualHostEntry *entry;
    if (r == NULL || !api_valid(api) || token == 0u || token >
        SUDEKIMP_SPIRIT_VISUAL_HOST_REGISTRY_CAPACITY) return;
    entry = &r->entries[token - 1u];
    if (entry->weak.entity == NULL && entry->weak.previous == NULL &&
        entry->weak.next == NULL) {
        memset(entry, 0, sizeof(*entry));
        return; /* Native destruction positively retired this attempt. */
    }
    if (!native_success) {
        if (api->bind(api->context, &entry->weak, NULL))
            memset(entry, 0, sizeof(*entry));
        else r->unknown = TRUE;
        return;
    }
    entry->state = ENTRY_READY;
}

BOOL SudekiMpSpiritVisualHostRegistryCapture(
    SudekiMpSpiritVisualHostRegistry *r, uint64_t session,
    SudekiMpLanArenaSnapshot *output, const SudekiMpSpiritVisualHostApi *api
) {
    SudekiMpLanArenaSpiritVfxSnapshot values[
        SUDEKIMP_LAN_ARENA_SPIRIT_VFX_CAPACITY];
    unsigned int i, count = 0u;
    unknown_output(output);
    if (r == NULL || output == NULL || !api_valid(api) || session == 0u ||
        r->unknown || (r->session != 0u && r->session != session)) return FALSE;
    r->session = session;
    memset(values, 0, sizeof(values));
    for (i = 0; i < SUDEKIMP_SPIRIT_VISUAL_HOST_REGISTRY_CAPACITY; ++i) {
        SudekiMpSpiritVisualHostEntry *entry = &r->entries[i];
        if (entry->weak.entity == NULL) {
            if (entry->weak.previous != NULL || entry->weak.next != NULL)
                return FALSE;
            memset(entry, 0, sizeof(*entry));
            continue;
        }
        if (entry->state != ENTRY_READY || count >=
                SUDEKIMP_LAN_ARENA_SPIRIT_VFX_CAPACITY) return FALSE;
        values[count] = entry->value;
        if (!api->sample(api->context, &entry->weak, entry->value.kind,
                &values[count])) return FALSE;
        ++count;
    }
    /* Native slot reuse is unrelated to instance chronology. */
    for (i = 1u; i < count; ++i) {
        unsigned int at = i;
        SudekiMpLanArenaSpiritVfxSnapshot value = values[i];
        while (at > 0u && values[at - 1u].instance_sequence > value.instance_sequence) {
            values[at] = values[at - 1u]; --at;
        }
        values[at] = value;
    }
    memcpy(output->spirit_vfx, values, sizeof(values));
    output->spirit_vfx_count = (uint8_t)count;
    output->spirit_vfx_observed = 1u;
    return TRUE;
}

static BOOL memory_access(const void *pointer, size_t size, BOOL write) {
    MEMORY_BASIC_INFORMATION info;
    uintptr_t address = (uintptr_t)pointer, end, region_end;
    DWORD protection;
    if (pointer == NULL || size == 0u || address > UINTPTR_MAX - size) return FALSE;
    end = address + size;
    if (VirtualQuery(pointer, &info, sizeof(info)) == 0u ||
        info.State != MEM_COMMIT || (info.Protect & PAGE_GUARD) != 0u) return FALSE;
    protection = info.Protect & 0xffu;
    if (write) {
        if (protection != PAGE_READWRITE && protection != PAGE_WRITECOPY &&
            protection != PAGE_EXECUTE_READWRITE && protection != PAGE_EXECUTE_WRITECOPY)
            return FALSE;
    } else if (protection != PAGE_READONLY && protection != PAGE_READWRITE &&
        protection != PAGE_WRITECOPY && protection != PAGE_EXECUTE_READ &&
        protection != PAGE_EXECUTE_READWRITE && protection != PAGE_EXECUTE_WRITECOPY)
        return FALSE;
    region_end = (uintptr_t)info.BaseAddress + info.RegionSize;
    return region_end >= (uintptr_t)info.BaseAddress && end <= region_end;
}

static BOOL pointer_at(const void *object, size_t offset, void **value) {
    const uint8_t *p = (const uint8_t *)object;
    if (!memory_access(p, offset + sizeof(void *), FALSE)) return FALSE;
    *value = *(void *const *)(p + offset);
    return TRUE;
}

static uint8_t native_resource_kind(const void *resource_name) {
    const uint8_t *rn = resource_name;
    void *reference, *text;
    char bounded[RESOURCE_TEXT_CAPACITY];
    uint32_t identifier, encoded_kind;
    size_t i;
    uint8_t kind;
    if (!memory_access(rn, 12u, FALSE)) return 0u;
    encoded_kind = *(const uint32_t *)rn;
    identifier = *(const uint32_t *)(rn + 4u);
    kind = SudekiMpSpiritVisualKindForResource(identifier);
    if (kind != 0u) return kind;
    if ((encoded_kind & 0x7fu) != 0x29u ||
        !pointer_at(rn, 8u, &reference) ||
        !pointer_at(reference, 4u, &text)) return 0u;
    if (memory_access(text, sizeof(bounded), FALSE)) {
        const char *end;
        memcpy(bounded, text, sizeof(bounded));
        end = memchr(bounded, '\0', sizeof(bounded));
        return end != NULL ? SudekiMpSpiritVisualKindForTypedResource(
            encoded_kind, identifier, bounded, (size_t)(end - bounded) + 1u) : 0u;
    }
    for (i = 0u; i < sizeof(bounded); ++i) {
        if (!memory_access((const uint8_t *)text + i, 1u, FALSE)) return 0u;
        bounded[i] = ((const char *)text)[i];
        if (bounded[i] == '\0') return SudekiMpSpiritVisualKindForTypedResource(
            encoded_kind, identifier, bounded, i + 1u);
    }
    return 0u; /* Unterminated or oversized retained text is never read on. */
}

static BOOL exact_effect(void *entity) {
    uint8_t *base = (uint8_t *)image, *e = (uint8_t *)entity;
    return memory_access(e, 0x3e4u, FALSE) &&
        *(void **)e == base + RVA_EFFECT_VTABLE &&
        *(void **)(e + 0x44u) == e + 0x160u &&
        *(void **)(e + 0x58u) == e + 0x270u &&
        *(void **)(e + 0x160u) == base + RVA_POSITION_VTABLE &&
        *(void **)(e + 0x170u) == entity &&
        *(void **)(e + 0x270u) == base + RVA_EFFECT_COMPONENT_VTABLE &&
        *(void **)(e + 0x280u) == entity;
}

static BOOL weak_links_valid(const SudekiMpSpiritVisualWeakNode *node) {
    void *head;
    if (node->entity == NULL)
        return node->previous == NULL && node->next == NULL;
    if (!exact_effect(node->entity) ||
        !memory_access((uint8_t *)node->entity + 4u, sizeof(void *), TRUE) ||
        !pointer_at(node->entity, 4u, &head)) return FALSE;
    if (node->previous != NULL) {
        if (!memory_access(node->previous, sizeof(*node), TRUE) ||
            node->previous->entity != node->entity || node->previous->next != node)
            return FALSE;
    } else if (head != node) return FALSE;
    return node->next == NULL ||
        (memory_access(node->next, sizeof(*node), TRUE) &&
         node->next->entity == node->entity && node->next->previous == node);
}

static BOOL native_bind(void *context, SudekiMpSpiritVisualWeakNode *node, void *entity) {
    uintptr_t node_register = (uintptr_t)node, entity_register = (uintptr_t)entity;
    void *head = NULL, *entry = (uint8_t *)context + RVA_WEAK_BIND;
    if (GetCurrentThreadId() != game_thread || !weak_links_valid(node)) return FALSE;
    if (entity != NULL) {
        if (!exact_effect(entity) ||
            !memory_access((uint8_t *)entity + 4u, sizeof(void *), TRUE) ||
            !pointer_at(entity, 4u, &head)) return FALSE;
        if (head != NULL &&
            (!memory_access(head, sizeof(*node), TRUE) ||
             ((SudekiMpSpiritVisualWeakNode *)head)->entity != entity ||
             ((SudekiMpSpiritVisualWeakNode *)head)->previous != NULL)) return FALSE;
    }
    __asm__ volatile("call *%2" : "+a"(node_register), "+d"(entity_register)
        : "r"(entry) : "ecx", "memory", "cc");
    return node->entity == entity && weak_links_valid(node);
}

static BOOL lifetime_owner_valid(const SudekiMpLanPartyEffectOwner *o) {
    return o && o->session && o->generation && o->actor &&
        (o->actor_type==SUDEKIMP_LAN_ARENA_BUKI_TYPE ||
         o->actor_type==SUDEKIMP_LAN_ARENA_ELCO_TYPE ||
         o->actor_type==SUDEKIMP_LAN_ARENA_TAL_TYPE ||
         o->actor_type==SUDEKIMP_LAN_ARENA_AILISH_TYPE);
}
static BOOL lifetime_owner_equal(const SudekiMpLanPartyEffectOwner *a,
    const SudekiMpLanPartyEffectOwner *b) {
    return a->session==b->session && a->generation==b->generation &&
        a->actor==b->actor && a->actor_type==b->actor_type;
}
static void lifetime_fault(void) {
    if(!InterlockedExchange(&lifetime_unknown,1))
        SudekiMpLogFormat("lan_party_effect event=lifetime_unknown count=%ld policy=retain_native_dependencies\r\n",
            (long)InterlockedCompareExchange(&lifetime_count,0,0));
}
static EffectLifetime *lifetime_find(void *entity) {
    for(unsigned i=0;i<LIFETIME_CAPACITY;++i) {
        EffectLifetime *e=&lifetimes[i];
        if(e->registered && e->weak.entity==entity && entity) {
            if(weak_links_valid(&e->weak)) return e;
            lifetime_fault(); return NULL;
        }
    }
    return NULL;
}
static void lifetime_capture(void *entity,const SudekiMpLanPartyEffectOwner *owner) {
    EffectLifetime *entry;
    if(!entity) return; /* Native allocation failure creates no callback owner. */
    if(!lifetime_owner_valid(owner) || !exact_effect(entity)) { lifetime_fault(); return; }
    entry=lifetime_find(entity);
    if(entry) {
        if(!lifetime_owner_equal(owner,&entry->owner)) lifetime_fault();
        return;
    }
    for(unsigned i=0;i<LIFETIME_CAPACITY;++i) {
        entry=&lifetimes[i];
        if(entry->registered || entry->weak.entity || entry->weak.previous || entry->weak.next) continue;
        entry->owner=*owner;
        /* Set registration intent before the native write. A failed bind may
         * have attached this stable address; never reclaim it on failure. */
        entry->registered=TRUE; InterlockedIncrement(&lifetime_count);
        if(!native_bind(image,&entry->weak,entity)) lifetime_fault();
        return;
    }
    lifetime_fault();
}
static BOOL lifetime_source(void *component,SudekiMpLanPartyEffectOwner *owner) {
    void *entity=NULL,*backlink=NULL;
    if(!owner || !lifetime_witness || !game_thread) return FALSE;
    memset(owner,0,sizeof(*owner));
    if(GetCurrentThreadId()!=game_thread) { lifetime_fault(); return FALSE; }
    if(component && pointer_at(component,0x10u,&entity) && exact_effect(entity) &&
        pointer_at(entity,0x58u,&backlink) && backlink==component) {
        EffectLifetime *entry=lifetime_find(entity);
        if(entry) *owner=entry->owner;
        return entry!=NULL;
    }
    /* An explicit unowned nested emission masks its enclosing cast. A
     * forwarded effect event retains its original owner through the actor's
     * event dispatcher, even when that actor is merely a buff recipient. */
    if(lifetime_scope) {
        *owner=*lifetime_scope; return lifetime_owner_valid(owner);
    }
    return lifetime_witness(component,owner) && lifetime_owner_valid(owner);
}
BOOL SudekiMpLanPartyEffectLifetimeCurrent(SudekiMpLanPartyEffectOwner *owner) {
    return lifetime_source(NULL,owner);
}
static void * __cdecl lifetime_factory(void) {
    SudekiMpLanPartyEffectOwner owner={0};
    BOOL owned=lifetime_source(NULL,&owner);
    InterlockedIncrement(&lifetime_depth);
    void *entity=((void *(__cdecl *)(void))lifetime_factory_hook.trampoline)();
    if(owned) lifetime_capture(entity,&owner);
    InterlockedDecrement(&lifetime_depth);
    return entity;
}
typedef void (__attribute__((thiscall)) *EffectForward)(void *,void *,void *);
static void __attribute__((thiscall)) lifetime_forward(void *listener,void *source,void *event) {
    SudekiMpLanPartyEffectOwner owner={0};
    const SudekiMpLanPartyEffectOwner *previous=lifetime_scope;
    BOOL observing=lifetime_witness && game_thread && GetCurrentThreadId()==game_thread;
    InterlockedIncrement(&lifetime_depth);
    if(observing) {
        void *effect=(uintptr_t)listener>=0x148u?(uint8_t *)listener-0x148u:NULL;
        EffectLifetime *entry=exact_effect(effect)?lifetime_find(effect):NULL;
        if(entry) owner=entry->owner;
        lifetime_scope=&owner;
    } else if(lifetime_witness && game_thread) lifetime_fault();
    ((EffectForward)lifetime_forward_hook.trampoline)(listener,source,event);
    if(observing) lifetime_scope=previous;
    InterlockedDecrement(&lifetime_depth);
}
static uintptr_t __attribute__((cdecl,used)) lifetime_pump_body(void *source,void *model) {
    SudekiMpLanPartyEffectOwner owner={0};
    const SudekiMpLanPartyEffectOwner *previous=lifetime_scope;
    BOOL observing=lifetime_witness && game_thread && GetCurrentThreadId()==game_thread;
    uintptr_t result=(uintptr_t)source;
    void *entry=lifetime_pump_hook.trampoline;
    InterlockedIncrement(&lifetime_depth);
    if(observing) {
        lifetime_scope=NULL;
        if((uintptr_t)source>=0x18u) (void)lifetime_source((uint8_t *)source-0x18u,&owner);
        lifetime_scope=&owner;
    } else if(lifetime_witness && game_thread) lifetime_fault();
    /* Both retail graphics-update callers pass EAX=component+18, then one
     * model pointer. The pump returns EAX and pops that one stack argument. */
    __asm__ volatile("pushl %1\n\tcall *%2" : "+a"(result)
        : "r"(model),"r"(entry) : "ecx","edx","memory","cc");
    if(observing) lifetime_scope=previous;
    InterlockedDecrement(&lifetime_depth);
    return result;
}
static void __attribute__((naked,used)) lifetime_pump(void) {
    __asm__ volatile("pushl 4(%esp)\n\tpushl %eax\n\tcall _lifetime_pump_body\n\t"
        "addl $8,%esp\n\tret $4");
}

static BOOL native_phase(void *renderer, SudekiMpLanArenaSpiritVfxSnapshot *value) {
    void *model, *parts, *bank, *channels;
    uint32_t count, i;
    float phase;
    uint8_t *base = (uint8_t *)image;
    value->phase_valid = 0u;
    value->phase = 0.0f;
    if (!memory_access(renderer, 0x9cu, FALSE) ||
        *(void **)renderer != base + RVA_RENDERER_VTABLE ||
        !pointer_at(renderer, 8u, &model) || !pointer_at(model, 0x1cu, &parts) ||
        !memory_access(parts, 0x10u, FALSE)) return FALSE;
    count = *(uint32_t *)((uint8_t *)parts + 0xcu);
    if (count == 0u || count > 256u || !pointer_at(renderer, 0x98u, &bank) ||
        !pointer_at(bank, 0u, &channels) ||
        !memory_access(channels, count * 24u, FALSE)) return FALSE;
    phase = *(float *)((uint8_t *)channels + 8u);
    if (!isfinite(phase) || phase < 0.0f || phase > 1000000.0f) return FALSE;
    for (i = 0u; i < count; ++i) {
        const uint8_t *channel = (uint8_t *)channels + i * 24u;
        /* A single phase cannot describe independently selected/timed parts.
         * Default HOM clip 0 is the only admitted fixed-resource playback. */
        if (*(uint16_t *)channel != 0u ||
            !isfinite(*(const float *)(channel + 8u)) ||
            fabsf(*(const float *)(channel + 8u) - phase) > 0.01f) return FALSE;
    }
    value->phase = phase;
    value->phase_valid = 1u;
    return TRUE;
}

static uint8_t shield_owner(uint8_t kind) {
    if(kind == SUDEKIMP_LAN_ARENA_BUKI_VFX_SHIELD_APPEAR ||
       kind == SUDEKIMP_LAN_ARENA_BUKI_VFX_SHIELD_LOOP)
        return SUDEKIMP_LAN_ARENA_BUKI_TYPE;
    if(kind == SUDEKIMP_LAN_PARTY_TAL_VFX_SHIELD_APPEAR ||
       kind == SUDEKIMP_LAN_PARTY_TAL_VFX_SHIELD_LOOP)
        return SUDEKIMP_LAN_ARENA_TAL_TYPE;
    return 0u;
}
static BOOL shield_actor_exact(uint8_t kind, void **actor) {
    uint8_t owner = shield_owner(kind);
    if(!owner || !actor) return FALSE;
    if(shield_witness) return shield_witness(witness_context, owner, actor);
    if(owner != SUDEKIMP_LAN_ARENA_BUKI_TYPE ||
        seat_host_type() != SUDEKIMP_LAN_ARENA_BUKI_TYPE) return FALSE;
    *actor = SudekiMpCleanroomEngineActorEntity(SUDEKIMP_CLEANROOM_BUKI);
    return *actor != NULL;
}

static BOOL native_sample(void *context, const SudekiMpSpiritVisualWeakNode *node,
    uint8_t kind, SudekiMpLanArenaSpiritVfxSnapshot *value) {
    uint8_t *e = (uint8_t *)node->entity;
    uint8_t *position;
    void *wrapper, *renderer, *render_object, *model_interface, *scene, *render_interface;
    const float *matrix;
    typedef const float *(__attribute__((thiscall)) *WorldMatrix)(void *);
    (void)context;
    sample_reason = "native_effect_identity";
    if (GetCurrentThreadId() != game_thread || !weak_links_valid(node) ||
        !exact_effect(e) || kind == 0u || kind >= sizeof(resource_ids)/sizeof(resource_ids[0]))
        return FALSE;
    if (shield_owner(kind)) {
        void *actor = NULL;
        void *actor_position, *owner;
        uint8_t *p = e + 0x160u;
        unsigned int depth;
        BOOL matched = FALSE;
        sample_reason = "shield_parent_identity";
        if (!shield_actor_exact(kind, &actor) ||
            value->owner_actor_type != shield_owner(kind) ||
            ((const SudekiMpSpiritVisualHostEntry *)node)->status_actor != actor ||
            !pointer_at(actor, 0x44u, &actor_position) ||
            !pointer_at(actor_position, 0x10u, &owner) || owner != actor)
            return FALSE;
        /* Finalize is observed before its caller finishes attaching the
         * effect. Admission at capture requires the completed native parent
         * chain to reach this session's exact actor, never just proximity. */
        for (depth = 0u; depth < 16u; ++depth) {
            void *parent;
            if (p == actor_position) { matched = TRUE; break; }
            if (!memory_access(p, 0x104u, FALSE) ||
                *(void **)p != (uint8_t *)image + RVA_POSITION_VTABLE ||
                !pointer_at(p, 0x94u, &parent) ||
                (uintptr_t)parent < 4u) break;
            p = (uint8_t *)parent - 4u;
        }
        if (!matched) return FALSE;
    }
    position = e + 0x160u;
    sample_reason = "native_resource_or_renderer";
    if (native_resource_kind(e + 0x29cu) != kind ||
        !pointer_at(position, 0xb4u, &wrapper) ||
        !pointer_at(wrapper, 8u, &render_object) || render_object == NULL ||
        !pointer_at(wrapper, 0xcu, &model_interface) || model_interface == NULL ||
        !pointer_at(wrapper, 0x10u, &renderer) ||
        !pointer_at(wrapper, 0x14u, &scene) || scene == NULL ||
        !memory_access(render_object, 0xd0u, TRUE) ||
        !pointer_at(render_object, 0x14u, &render_interface) ||
        !memory_access(renderer, 0x9cu, FALSE) ||
        *(void **)renderer != (uint8_t *)image + RVA_RENDERER_VTABLE ||
        model_interface != renderer || render_interface != renderer) return FALSE;
    /* Native wrapper ctor523420 stores the interface at +0c and its ANM
     * query at +10. Concrete61ba40 returns itself for ANM.5d6460 also stores
     * that interface in renderobject+14; renderer+8 is a different model. */
    /* Follow only verified native CPosition parents. The getter composes
     * attached loop effects; reading local +18 would collapse them to origin. */
    {
        uint8_t *p = position;
        unsigned int depth;
        sample_reason = "native_position_parent";
        for (depth = 0u; depth < 16u; ++depth) {
            void *parent, *tree;
            if (!memory_access(p, 0x104u, TRUE) ||
                *(void **)p != (uint8_t *)image + RVA_POSITION_VTABLE ||
                !pointer_at(p, 0x8cu, &tree) ||
                !memory_access(tree, 0x110u, TRUE) ||
                !pointer_at(p, 0x94u, &parent)) return FALSE;
            if (parent == NULL) break;
            if ((uintptr_t)parent < 4u) return FALSE;
            p = (uint8_t *)parent - 4u;
            if (p == position) return FALSE;
        }
        if (depth == 16u) return FALSE;
    }
    sample_reason = "native_world_pose";
    matrix = ((WorldMatrix)((uint8_t *)image + RVA_WORLD_MATRIX))(position);
    if (!memory_access(matrix, 16u * sizeof(float), FALSE) ||
        !SudekiMpSpiritVisualDecomposeMatrix(matrix, value)) return FALSE;
    sample_reason = "native_animation_phase";
    if (!native_phase(renderer, value) || node->entity != e || !exact_effect(e))
        return FALSE;
    sample_reason = NULL;
    return TRUE;
}

static const SudekiMpSpiritVisualHostApi *native_api(void) {
    static SudekiMpSpiritVisualHostApi api;
    api.context = image; api.bind = native_bind; api.sample = native_sample;
    return &api;
}

static BOOL reset_pending_emissions(void) {
    unsigned int i;
    BOOL ok = TRUE;
    for (i = 0u; i < SUDEKIMP_SPIRIT_VISUAL_HOST_REGISTRY_CAPACITY; ++i) {
        PendingEmission *p = &pending_emissions[i];
        if ((p->weak.entity || p->weak.previous || p->weak.next) &&
            !native_bind(image, &p->weak, NULL)) {
            ok = FALSE;
            continue;
        }
        memset(p, 0, sizeof(*p));
    }
    if (!ok) registry.unknown = TRUE;
    return ok;
}

static BOOL same_source(const EmissionSource *a, const EmissionSource *b) {
    return a->valid && b->valid && a->session == b->session &&
        a->sequence == b->sequence && a->owner == b->owner;
}

static PendingEmission *pending_source_for(void *entity) {
    unsigned int i;
    if (!entity) return NULL;
    for (i = 0u; i < SUDEKIMP_SPIRIT_VISUAL_HOST_REGISTRY_CAPACITY; ++i) {
        PendingEmission *p = &pending_emissions[i];
        if (p->weak.entity == entity) {
            if (p->source.session == registry.session && weak_links_valid(&p->weak)) return p;
            registry.unknown = TRUE;
            return NULL;
        }
    }
    return NULL;
}

static BOOL registered_source_for(void *entity, EmissionSource *source) {
    unsigned int i;
    if (!entity || registry.session == 0u) return FALSE;
    for (i = 0u; i < SUDEKIMP_SPIRIT_VISUAL_HOST_REGISTRY_CAPACITY; ++i) {
        SudekiMpSpiritVisualHostEntry *e = &registry.entries[i];
        if (e->weak.entity != entity) continue;
        if (!weak_links_valid(&e->weak) || !e->value.skill_sequence) return FALSE;
        *source = (EmissionSource){registry.session, e->value.skill_sequence,
            e->value.emitted_host_tick, e->value.owner_actor_type, TRUE};
        return TRUE;
    }
    return FALSE;
}

static void retain_emission(void *entity, const EmissionSource *source) {
    EmissionSource recorded = {0};
    PendingEmission *existing;
    unsigned int i;
    if (!source->valid || !entity) return;
    if (!exact_effect(entity) || source->session != registry.session) {
        registry.unknown = TRUE;
        return;
    }
    if (registered_source_for(entity, &recorded)) {
        if (!same_source(source, &recorded)) registry.unknown = TRUE;
        return; /* Immediate finalization already owns the native lifetime. */
    }
    existing = pending_source_for(entity);
    if (existing) {
        if (!same_source(source, &existing->source)) registry.unknown = TRUE;
        return;
    }
    for (i = 0u; i < SUDEKIMP_SPIRIT_VISUAL_HOST_REGISTRY_CAPACITY; ++i) {
        PendingEmission *p = &pending_emissions[i];
        if (p->weak.entity || p->weak.previous || p->weak.next) continue;
        p->source = *source;
        if (!native_bind(image, &p->weak, entity)) registry.unknown = TRUE;
        return;
    }
    registry.unknown = TRUE; /* Never publish a truncated lifetime roster. */
}

typedef void (__attribute__((stdcall)) *AnimationEmit)(void *, void **, uint32_t);
static void __attribute__((stdcall)) observe_animation_emit(
    void *component, void **out_effect, uint32_t event_index
) {
    EmissionSource source = {0};
    SudekiMpLanPartyEffectOwner life={0};
    const SudekiMpLanPartyEffectOwner *previous_life=lifetime_scope;
    BOOL observe_life=lifetime_witness && game_thread && GetCurrentThreadId()==game_thread;
    const EmissionSource *previous = NULL;
    BOOL observing = FALSE;
    InterlockedIncrement(&in_flight);
    if(observe_life) {
        (void)lifetime_source(component,&life);
        lifetime_scope=&life;
        InterlockedIncrement(&lifetime_depth);
    } else if(lifetime_witness && game_thread) lifetime_fault();
    if (game_thread && session_armed && InterlockedCompareExchange(&admitted, 0, 0)) {
        if (GetCurrentThreadId() != game_thread) InterlockedExchange(&unexpected_thread, 1);
        else {
            void *entity = NULL, *backlink = NULL;
            PendingEmission *pending;
            observing = TRUE;
            previous = emission_source;
            /* An explicit foreign source masks an enclosing caster scope. */
            if (component && active_witness && active_witness(witness_context, component,
                    &source.session, &source.sequence, &source.tick, &source.owner))
                source.valid = source.session == registry.session;
            else if (pointer_at(component, 0x10u, &entity) &&
                exact_effect(entity) && pointer_at(entity, 0x58u, &backlink) && backlink == component) {
                if (!registered_source_for(entity, &source) &&
                    (pending = pending_source_for(entity)) != NULL) source = pending->source;
            }
            emission_source = &source;
        }
    }
    ((AnimationEmit)animation_emit_hook.trampoline)(component, out_effect, event_index);
    if(observe_life) {
        /* The common factory already captured this object before async
         * resource setup. The result is an independent completeness check. */
        if(lifetime_owner_valid(&life)) {
            if(memory_access(out_effect,sizeof(*out_effect),FALSE)) lifetime_capture(*out_effect,&life);
            else lifetime_fault();
        }
        lifetime_scope=previous_life;
        InterlockedDecrement(&lifetime_depth);
    }
    if (observing) {
        if (source.valid) {
            if (memory_access(out_effect, sizeof(*out_effect), FALSE)) retain_emission(*out_effect, &source);
            else registry.unknown = TRUE;
        }
        emission_source = previous;
    }
    InterlockedDecrement(&in_flight);
}

/* The script-facing PlaySFXWithAll parent call passes eleven stack words and
 * a bone selector in EDI. The constructor consumes both ResourceName values
 * (including their existing reference counts) and returns the effect in EAX.
 * Copy stack words without acquiring/releasing any extra resource reference.
 * This bridge preserves all nonvolatile registers; the outer bridge also
 * preserves ECX, as the exact native constructor does. */
static void * __attribute__((naked, cdecl, used)) invoke_parent_create(
    void *entry __attribute__((unused)), const uint32_t *args __attribute__((unused)),
    uint32_t selector __attribute__((unused))
) {
    __asm__ volatile(
        "pushl %ebp\n\tmovl %esp,%ebp\n\tpushl %esi\n\tpushl %edi\n\t"
        "movl 12(%ebp),%esi\n\tsubl $44,%esp\n\tmovl %esp,%edi\n\t"
        "movl $11,%ecx\n\trep movsl\n\tmovl 16(%ebp),%edi\n\t"
        "call *8(%ebp)\n\tpopl %edi\n\tpopl %esi\n\tpopl %ebp\n\tret");
}

static void * __attribute__((cdecl, used)) observe_script_parent_body(
    const uint32_t *args, uint32_t selector
) {
    EmissionSource source = {0};
    void *effect;
    BOOL observing = FALSE;
    InterlockedIncrement(&in_flight);
    if (game_thread && session_armed && InterlockedCompareExchange(&admitted, 0, 0)) {
        if (GetCurrentThreadId() != game_thread) InterlockedExchange(&unexpected_thread, 1);
        else if (memory_access(args, 11u * sizeof(*args), FALSE)) {
            uint8_t kind = native_resource_kind(args + 2u);
            /* Status/shield effects have separate target-owned discovery.
             * An unrelated script effect must not consume a pending slot. */
            observing = kind != 0u && kind != SUDEKIMP_LAN_ARENA_STATUS_VFX_BOOST &&
                !shield_owner(kind);
            if (observing) {
                if (emission_source) source = *emission_source;
                else if (active_witness) source.valid = active_witness(witness_context, NULL,
                    &source.session, &source.sequence, &source.tick, &source.owner);
            }
        }
    }
    effect = invoke_parent_create((uint8_t *)image + RVA_PARENT_CREATE, args, selector);
    /* A parent is the effect's recipient, not necessarily its caster. Never
     * attribute a party buff from args[1] or whichever cast is active later. */
    if (observing && source.valid) retain_emission(effect, &source);
    InterlockedDecrement(&in_flight);
    return effect;
}

static void __attribute__((naked, used)) observe_script_parent(void) {
    __asm__ volatile(
        "pushl %ecx\n\tleal 8(%esp),%eax\n\tpushl %edi\n\tpushl %eax\n\t"
        "call _observe_script_parent_body\n\taddl $8,%esp\n\tpopl %ecx\n\tret $44");
}

/* custom EAX SfxSetup*, one callee-cleaned stack mode. regparm(1) alone
 * would use caller cleanup, so both directions use explicit bridges. */
static unsigned char invoke_finalize(void *setup, uint32_t mode) {
    uintptr_t eax = (uintptr_t)setup;
    void *entry = finalize_hook.trampoline;
    __asm__ volatile("pushl %1\n\tcall *%2" : "+a"(eax)
        : "r"(mode), "r"(entry) : "ecx", "edx", "memory", "cc");
    return (unsigned char)eax;
}

static unsigned char __attribute__((cdecl, used)) observe_finalize_body(
    void *setup, uint32_t mode
) {
    unsigned int token = 0u;
    unsigned char result;
    uint64_t session = 0u;
    uint16_t skill = 0u;
    uint32_t tick = 0u;
    uint32_t instance = 0u;
    uint32_t requested_identifier = 0u, requested_type = 0u;
    uint8_t observed_kind = 0u;
    uint8_t owner_type = 0u;
    PendingEmission *pending = NULL;
    const char *skip_reason = NULL;
    SudekiMpLanPartyEffectOwner life={0};
    const SudekiMpLanPartyEffectOwner *previous_life=lifetime_scope;
    BOOL observe_life=lifetime_witness && game_thread && GetCurrentThreadId()==game_thread;
    InterlockedIncrement(&in_flight);
    if(observe_life) {
        if(memory_access(setup,0x48u,FALSE) && mode<=2u) {
            void *effect=*(void **)((uint8_t *)setup+0x1cu);
            EffectLifetime *entry=lifetime_find(effect);
            if(entry) life=entry->owner;
            else (void)lifetime_source(NULL,&life);
            if(effect && lifetime_owner_valid(&life)) lifetime_capture(effect,&life);
        } else if(lifetime_source(NULL,&life)) lifetime_fault();
        lifetime_scope=&life;
        InterlockedIncrement(&lifetime_depth);
    } else if(lifetime_witness && game_thread) lifetime_fault();
    if (game_thread != 0u && session_armed &&
        InterlockedCompareExchange(&admitted, 0, 0) != 0) {
        if (GetCurrentThreadId() != game_thread) {
            /* An unexpected thread never dereferences game objects. */
            InterlockedExchange(&unexpected_thread, 1);
        } else {
            uint8_t *s = (uint8_t *)setup;
            uint8_t kind = 0u;
            EmissionSource source = {0};
            BOOL active;
            if (emission_source) source = *emission_source;
            else if (active_witness) source.valid = active_witness(witness_context, NULL,
                &source.session, &source.sequence, &source.tick, &source.owner);
            if (memory_access(s, 0x48u, FALSE) && mode <= 2u)
                pending = pending_source_for(*(void **)(s + 0x1cu));
            if (pending) {
                /* A delayed native callback retains emission-time ownership,
                 * including after its cast ends. Conflicting scopes fail closed. */
                if ((source.valid && !same_source(&source, &pending->source)) ||
                    (emission_source && !source.valid)) {
                    registry.unknown = TRUE;
                    source.valid = FALSE;
                } else source = pending->source;
            }
            active = source.valid && source.session == registry.session;
            session = source.session; skill = source.sequence;
            tick = source.tick; owner_type = source.owner;
            if (!memory_access(s, 0x48u, FALSE) || mode > 2u) {
                if (active) registry.unknown = TRUE;
            } else {
                static const uint32_t setup_vtables[] = {0x2c62e0u,0x2c6308u,0x2c6330u};
                requested_type = *(uint32_t *)(s + 0x28u) & 0x1fffu;
                requested_identifier = *(uint32_t *)(s + 0x2cu);
                if (*(void **)s != (uint8_t *)image + setup_vtables[mode]) {
                    if (active) registry.unknown = TRUE;
                } else {
                    kind = native_resource_kind(s + 0x28u);
                }
                if (shield_owner(kind)) {
                    /* A candidate native lifetime, not yet a published
                     * owner claim. native_sample validates the final parent
                     * chain against the exact actor before any transmission. */
                    void *entity = *(void **)(s + 0x1cu), *actor = NULL;
                    if (shield_actor_exact(kind, &actor) &&
                        registry.session != 0u && entity != NULL) {
                        observed_kind = kind;
                        if (!exact_effect(entity)) registry.unknown = TRUE;
                        else {
                            token = SudekiMpSpiritVisualHostRegistryBeginOwned(
                                &registry, registry.session, 0u, GetTickCount(), kind,
                                shield_owner(kind), entity, native_api());
                            if (token != 0u) registry.entries[token - 1u].status_actor = actor;
                        }
                    }
                } else if (kind == SUDEKIMP_LAN_ARENA_STATUS_VFX_BOOST) {
                    /* Status effects are discovered from the exact target's
                     * status component after finalization, never attributed
                     * to whichever Spirit happened to be running. */
                } else if (!shield_witness && !active && kind != 0u &&
                    (inactive_kind_mask & (UINT64_C(1) << kind)) == 0u) {
                    inactive_kind_mask |= (UINT64_C(1) << kind);
                    observed_kind = kind;
                    skip_reason = "native_spirit_witness_inactive";
                } else if (active && kind == 0u &&
                    *(void **)s == (uint8_t *)image + setup_vtables[mode]) {
                    unsigned int i;
                    if (diagnostic_skill != skill) {
                        diagnostic_skill = skill;
                        diagnostic_count = 0u;
                        inactive_kind_mask = 0u;
                    }
                    for (i = 0u; i < diagnostic_count; ++i)
                        if (diagnostic_ids[i] == requested_identifier &&
                            diagnostic_types[i] == requested_type) break;
                    if (i == diagnostic_count && diagnostic_count < DIAGNOSTIC_CAPACITY) {
                        diagnostic_ids[diagnostic_count] = requested_identifier;
                        diagnostic_types[diagnostic_count++] = requested_type;
                        skip_reason = "unrecognized_resource_request";
                    }
                } else if (active && kind != 0u) {
                    void *entity = *(void **)(s + 0x1cu);
                    observed_kind = kind;
                    /* Exact 18830 prefix returns immediately when this weak
                     * pointer is NULL; it cannot create an effect on that
                     * branch. A retired/pending setup is not a missed spawn. */
                    if (entity != NULL && !exact_effect(entity)) registry.unknown = TRUE;
                    else if (entity != NULL) token = SudekiMpSpiritVisualHostRegistryBeginOwned(
                        &registry, session, skill, tick, kind, owner_type, entity, native_api());
                }
            }
        }
    }
    result = invoke_finalize(setup, mode);
    if(observe_life) {
        lifetime_scope=previous_life;
        InterlockedDecrement(&lifetime_depth);
    }
    if (token != 0u) {
        instance = registry.entries[token - 1u].value.instance_sequence;
        SudekiMpSpiritVisualHostRegistryComplete(
            &registry, token, result != 0u, native_api());
    }
    if (pending) {
        if (native_bind(image, &pending->weak, NULL)) memset(pending, 0, sizeof(*pending));
        else registry.unknown = TRUE;
    }
    InterlockedDecrement(&in_flight);
    /* This transition log follows native execution; no render-rate disk I/O. */
    if (skip_reason != NULL) SudekiMpLogFormat(
        "lan_arena_spirit_visual_host event=finalize_skipped reason=%s kind=%u "
        "request_type=0x%lx request_id=0x%08lx skill_sequence=%u mode=%lu "
        "native_success=%u policy=bounded_observation_diagnostic\r\n",
        skip_reason, (unsigned int)observed_kind, (unsigned long)requested_type,
        (unsigned long)requested_identifier, (unsigned int)skill,
        (unsigned long)mode, (unsigned int)result);
    else if (observed_kind != 0u) SudekiMpLogFormat(
        "lan_arena_spirit_visual_host event=finalize kind=%u instance=%lu "
        "skill_sequence=%u emitted_host_tick=%lu native_success=%u leased=%u "
        "request_type=0x%lx request_id=0x%08lx "
        "policy=host_observation_not_visual_acceptance\r\n",
        (unsigned int)observed_kind, (unsigned long)instance,
        (unsigned int)skill, (unsigned long)tick, (unsigned int)result,
        token != 0u && registry.entries[token - 1u].weak.entity != NULL ? 1u : 0u,
        (unsigned long)requested_type, (unsigned long)requested_identifier);
    return result;
}

static void __attribute__((naked, used)) observe_finalize(void) {
    __asm__ volatile(
        "pushl 4(%esp)\n\t" /* original mode */
        "pushl %eax\n\t"
        "call _observe_finalize_body\n\t"
        "addl $8, %esp\n\t"
        "ret $4\n\t");
}

static BOOL matches(const uint8_t *base, uint32_t rva, const void *bytes, size_t size) {
    return memory_access(base + rva, size, FALSE) && memcmp(base + rva, bytes, size) == 0;
}
static BOOL call_matches(const uint8_t *base, uint32_t rva, uint32_t target) {
    int32_t displacement;
    if (!memory_access(base + rva, 5u, FALSE) || base[rva] != 0xe8u) return FALSE;
    memcpy(&displacement, base + rva + 1u, sizeof(displacement));
    return base + rva + 5u + displacement == base + target;
}

BOOL SudekiMpLanArenaSpiritVisualHostImageMatches(HMODULE module) {
    const uint8_t *base = (const uint8_t *)module;
    static const uint32_t calls[] = {0x18244u,0x182d5u,0x183d8u,0x18425u,0x18543u,0x18585u};
    static const uint8_t base_destructor_tail[] = {0xe9,0xdf,0x71,0xec,0xff};
    static const uint8_t is_boost_body[] = {0x8b,0x41,0x54,0x8a,0x40,0x4c,0xc3};
    static const uint8_t parent_prefix[] = {0x51,0x8b,0x44,0x24,0x0c,0x8b,0x80,0xb4,0,0,0,0x53};
    static const uint8_t parent_tail[] = {0x8b,0xc5,0x5e,0x5d,0x5b,0x59,0xc2,0x2c,0};
    unsigned int i;
    if (base == NULL ||
        !matches(base, 0x5070u, is_boost_body, sizeof(is_boost_body)) ||
        !matches(base, RVA_FINALIZE, finalize_prefix, sizeof(finalize_prefix)) ||
        !matches(base, RVA_ANIMATION_EMIT, animation_emit_prefix, sizeof(animation_emit_prefix)) ||
        !matches(base, 0xe2bbbu, animation_emit_tail, sizeof(animation_emit_tail)) ||
        !call_matches(base, 0xe236au, RVA_ANIMATION_EMIT) ||
        !call_matches(base, 0x1885a9u, RVA_ANIMATION_EMIT) ||
        !call_matches(base, 0x1887f7u, RVA_ANIMATION_EMIT) ||
        !call_matches(base, RVA_SCRIPT_PARENT_CALL, RVA_PARENT_CREATE) ||
        !matches(base, RVA_PARENT_CREATE, parent_prefix, sizeof(parent_prefix)) ||
        !matches(base, 0x18d9cu, parent_tail, sizeof(parent_tail)) ||
        !call_matches(base, 0x18d4bu, 0x18140u) ||
        !matches(base, RVA_WEAK_BIND, weak_bind_body, sizeof(weak_bind_body)) ||
        !matches(base, 0x4d72u, weak_null_tail, sizeof(weak_null_tail)) ||
        !matches(base, RVA_WORLD_MATRIX, world_matrix_prefix, sizeof(world_matrix_prefix)) ||
        !call_matches(base, 0x111cdau, 0x110d40u) ||
        !call_matches(base, 0x111cebu, 0x110f90u) ||
        !call_matches(base, 0x131908u, 0x13db00u) ||
        !matches(base, 0x13db4cu, base_destructor_tail, sizeof(base_destructor_tail)))
        return FALSE;
    for (i = 0u; i < sizeof(calls)/sizeof(calls[0]); ++i)
        if (!call_matches(base, calls[i], RVA_FINALIZE)) return FALSE;
    if (!memory_access(base + RVA_RENDERER_VTABLE, 0x11cu, FALSE) ||
        *(const void *const *)(base + RVA_RENDERER_VTABLE + 0xf8u) != base + 0x21bb10u ||
        *(const void *const *)(base + RVA_RENDERER_VTABLE + 0x100u) != base + 0x2230b0u ||
        *(const void *const *)(base + RVA_RENDERER_VTABLE + 0x110u) != base + 0x223220u)
        return FALSE;
    return TRUE;
}

BOOL SudekiMpLanArenaSpiritVisualHostInitialize(
    HMODULE module, SudekiMpLanArenaSpiritVisualHostWitness witness, void *context
) {
    HMODULE self;
    if (module == NULL || witness == NULL || InterlockedCompareExchange(&admitted, 0, 0))
        return FALSE;
    if (finalize_hook.installed || animation_emit_hook.installed || script_parent_hook.installed) {
        if (image != module || InterlockedCompareExchange(&in_flight, 0, 0) != 0 ||
            !finalize_hook.installed || !animation_emit_hook.installed || !script_parent_hook.installed ||
            !reset_pending_emissions() ||
            !SudekiMpSpiritVisualHostRegistryReset(&registry, native_api())) return FALSE;
    } else {
        if (!SudekiMpLanArenaSpiritVisualHostImageMatches(module)) return FALSE;
        if (!pinned && !GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_PIN,
            (LPCSTR)(uintptr_t)&SudekiMpLanArenaSpiritVisualHostInitialize, &self)) return FALSE;
        pinned = TRUE;
        image = module;
        game_thread = 0u; /* First verified Capture binds the game thread. */
        if (!SudekiMpInstallInlineHook(&finalize_hook, (uint8_t *)module + RVA_FINALIZE,
            finalize_prefix, 6u, observe_finalize)) return FALSE;
        if (!SudekiMpInstallInlineHook(&animation_emit_hook, (uint8_t *)module + RVA_ANIMATION_EMIT,
            animation_emit_prefix, 6u, observe_animation_emit)) {
            /* Pinned callbacks/trampolines survive a failed rollback. A partial
             * install cannot be rebound or reported as fully initialized. */
            SudekiMpRestoreInlineHook(&finalize_hook);
            return FALSE;
        }
        if (!SudekiMpInstallRelativeCallHook(&script_parent_hook,
                (uint8_t *)module + RVA_SCRIPT_PARENT_CALL,
                (uint8_t *)module + RVA_PARENT_CREATE, observe_script_parent)) {
            SudekiMpRestoreInlineHook(&animation_emit_hook);
            SudekiMpRestoreInlineHook(&finalize_hook);
            return FALSE;
        }
    }
    active_witness = witness;
    shield_witness = NULL;
    witness_context = context;
    InterlockedExchange(&unexpected_thread, 0);
    session_armed = FALSE;
    last_capture_reason = NULL;
    last_capture_count = -1;
    diagnostic_count = 0u;
    diagnostic_skill = 0u;
    inactive_kind_mask = 0u;
    InterlockedExchange(&admitted, 1);
    return TRUE;
}

static BOOL no_spirit_scope(void *context, void *source, uint64_t *session,
    uint16_t *skill, uint32_t *tick, uint8_t *owner) {
    (void)context; (void)source; (void)session; (void)skill; (void)tick; (void)owner;
    return FALSE;
}
static BOOL lifetime_image_exact(HMODULE module) {
    static const uint32_t operands[][2]={
        {23,0x3c2fef},{38,0x3cb968},{53,0x3cb968},{70,0x3cb968},
        {86,0x3cb96c},{94,0x3cb970},{102,0x3cb970},{114,0x409d8c}};
    static const uint32_t pump_operands[][2]={
        {0x9f,0x18af08},{0xa6,0x18aecc},{0x2dc,0x18ac9a},{0x2e0,0x18ace3},
        {0x2e4,0x18acf3},{0x2e8,0x18ae9c},{0x2ec,0x18ad36},{0x2f0,0x18ad6a},
        {0x2f4,0x18ada2},{0x2f8,0x18ade5},{0x2fc,0x18adf3},{0x300,0x18ae01},
        {0x304,0x18ae40},{0x308,0x18ae7b},{0x30c,0x18ae86},{0x310,0x18ae93},
        {0x314,0x18aea3}};
    static const uint32_t retire_operands[][2]={{0x93,0x409d8c},{0xd4,0x29a068},{0xf0,0x29a064}};
    static const uint32_t rva[]={0x18760,0x131d20,0x18abf0,0x131df0};
    static const unsigned size[]={0xc8,0x3a,0x332,0x107};
    static const uint8_t hashes[4][32]={
        {0xea,0x81,0xdf,0x2d,0xb8,0xcd,0x83,0xed,0x51,0x97,0x7b,0x0d,0x19,0x8b,0xa9,0x17,
         0x00,0xae,0x1b,0x9f,0x55,0xd9,0xc3,0x0d,0xac,0x42,0xc7,0xc1,0xa9,0xf9,0x68,0x75},
        {0xe7,0x40,0xdd,0x6a,0x1b,0xdd,0xa5,0x6d,0xe1,0x30,0x2c,0x37,0x2f,0x7e,0x42,0x92,
         0x7b,0xcb,0xb1,0xc5,0x86,0x1c,0x82,0xeb,0xc8,0x58,0x04,0x5f,0x7d,0xc8,0x4d,0xec},
        {0xcc,0xed,0xdc,0x78,0x52,0x79,0x20,0xb8,0x09,0x74,0x77,0x95,0x6c,0x09,0xe0,0xb4,
         0xd9,0xc1,0xd6,0x31,0xe3,0x49,0xe3,0x5c,0xa9,0xea,0xab,0xf9,0x16,0x6c,0xf3,0xc8},
        {0x16,0xcb,0x9f,0xff,0x8d,0x93,0xd3,0xdc,0xc3,0xbd,0xd8,0xc7,0x30,0x9f,0xbd,0x3a,
         0x00,0x73,0xcf,0x3f,0x75,0xed,0xe3,0x39,0xdd,0x81,0x96,0x9d,0xd2,0xcb,0x35,0x44}};
    uint8_t *base=(uint8_t *)module;
    if(!call_matches(base,0x18a88,0x18760) || !call_matches(base,0x18cd1,0x18760) ||
        !call_matches(base,0x18b94,0x18760) || !call_matches(base,0xdfa8d,0x18abf0) ||
        !call_matches(base,0xdfaa3,0x18abf0) ||
        !memory_access(base+0x2d3d00,4,FALSE) ||
        *(void **)(base+0x2d3d00)!=base+0x131d20) return FALSE;
    for(unsigned k=0;k<4;++k) {
        uint8_t code[0x332],digest[32]; DWORD n=sizeof(digest);
        HCRYPTPROV provider=0; HCRYPTHASH hash=0; BOOL exact=FALSE;
        if(!memory_access(base+rva[k],size[k],FALSE)) return FALSE;
        memcpy(code,base+rva[k],size[k]);
        if(!k) for(unsigned j=0;j<sizeof(operands)/sizeof(operands[0]);++j) {
            if(*(void **)(code+operands[j][0])!=base+operands[j][1]) return FALSE;
            memcpy(code+operands[j][0],&operands[j][1],4);
        }
        if(k==2u) for(unsigned j=0;j<sizeof(pump_operands)/sizeof(pump_operands[0]);++j) {
            if(*(void **)(code+pump_operands[j][0])!=base+pump_operands[j][1]) return FALSE;
            memcpy(code+pump_operands[j][0],&pump_operands[j][1],4);
        }
        if(k==3u) for(unsigned j=0;j<sizeof(retire_operands)/sizeof(retire_operands[0]);++j) {
            if(*(void **)(code+retire_operands[j][0])!=base+retire_operands[j][1]) return FALSE;
            memcpy(code+retire_operands[j][0],&retire_operands[j][1],4);
        }
        if(CryptAcquireContextW(&provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT|CRYPT_SILENT) &&
            CryptCreateHash(provider,CALG_SHA_256,0,0,&hash) &&
            CryptHashData(hash,code,size[k],0) &&
            CryptGetHashParam(hash,HP_HASHVAL,digest,&n,0) && n==sizeof(digest))
            exact=!memcmp(digest,hashes[k],sizeof(digest));
        if(hash) CryptDestroyHash(hash);
        if(provider) CryptReleaseContext(provider,0);
        if(!exact) return FALSE;
    }
    memcpy(lifetime_retire_code,base+0x131df0,sizeof(lifetime_retire_code));
    return TRUE;
}
BOOL SudekiMpLanPartyEffectLifetimeInitialize(HMODULE module,
    SudekiMpLanPartyEffectWitness witness,BOOL (*shutdown_drained)(void)) {
    static const uint8_t factory_prefix[]={0x83,0xec,0x14,0x53,0x55,0x56};
    static const uint8_t forward_prefix[]={0x83,0xb9,0x8c,0x02,0,0,0};
    static const uint8_t pump_prefix[]={0x55,0x8b,0xec,0x83,0xe4,0xf8};
    if(!module || !witness || !shutdown_drained || lifetime_witness ||
        InterlockedCompareExchange(&lifetime_count,0,0) ||
        InterlockedCompareExchange(&lifetime_unknown,0,0) ||
        InterlockedCompareExchange(&lifetime_depth,0,0)) return FALSE;
    if(!finalize_hook.installed) {
        if(!SudekiMpLanArenaSpiritVisualHostInitialize(module,no_spirit_scope,NULL)) return FALSE;
        lifetime_only=TRUE;
    }
    if(image!=module || !animation_emit_hook.installed || !script_parent_hook.installed) return FALSE;
    if(!lifetime_factory_hook.installed && !lifetime_forward_hook.installed && !lifetime_pump_hook.installed) {
        if(!lifetime_image_exact(module) ||
            !SudekiMpInstallInlineHook(&lifetime_factory_hook,(uint8_t *)module+0x18760,
                factory_prefix,sizeof(factory_prefix),lifetime_factory)) return FALSE;
        if(!SudekiMpInstallInlineHook(&lifetime_forward_hook,(uint8_t *)module+0x131d20,
                forward_prefix,sizeof(forward_prefix),lifetime_forward)) {
            SudekiMpRestoreInlineHook(&lifetime_factory_hook); return FALSE;
        }
        if(!SudekiMpInstallInlineHook(&lifetime_pump_hook,(uint8_t *)module+0x18abf0,
                pump_prefix,sizeof(pump_prefix),lifetime_pump)) {
            SudekiMpRestoreInlineHook(&lifetime_forward_hook);
            SudekiMpRestoreInlineHook(&lifetime_factory_hook); return FALSE;
        }
    }
    if(!lifetime_factory_hook.installed || !lifetime_forward_hook.installed ||
        !lifetime_pump_hook.installed) return FALSE;
    lifetime_witness=witness;
    lifetime_shutdown_drained=shutdown_drained;
    return TRUE;
}
BOOL SudekiMpLanPartyEffectLifetimePoll(void) {
    if(!lifetime_witness) return !SudekiMpLanPartyEffectLifetimeRetains();
    if(!game_thread) game_thread=GetCurrentThreadId();
    if(game_thread!=GetCurrentThreadId() || lifetime_scope ||
        InterlockedCompareExchange(&lifetime_depth,0,0)) return FALSE;
    if(InterlockedCompareExchange(&lifetime_unknown,0,0)) return FALSE;
    for(unsigned i=0;i<LIFETIME_CAPACITY;++i) {
        EffectLifetime *entry=&lifetimes[i];
        if(!entry->registered) continue;
        if(!entry->weak.entity && !entry->weak.previous && !entry->weak.next) {
            /* Native base destructor 4d30 cleared all three fields. The
             * observer is never unlinked to manufacture a drained result. */
            memset(entry,0,sizeof(*entry)); InterlockedDecrement(&lifetime_count);
        } else if(!weak_links_valid(&entry->weak)) { lifetime_fault(); return FALSE; }
    }
    return TRUE;
}
BOOL SudekiMpLanPartyEffectLifetimeRetains(void) {
    return InterlockedCompareExchange(&lifetime_count,0,0) ||
        InterlockedCompareExchange(&lifetime_unknown,0,0) ||
        InterlockedCompareExchange(&lifetime_depth,0,0);
}
BOOL SudekiMpLanPartyEffectLifetimeRequestRetire(void) {
    if(!lifetime_witness || !lifetime_shutdown_drained ||
        !SudekiMpLanPartyEffectLifetimePoll() || !lifetime_shutdown_drained() ||
        !memory_access((uint8_t *)image+0x131df0,sizeof(lifetime_retire_code),FALSE) ||
        memcmp((uint8_t *)image+0x131df0,lifetime_retire_code,sizeof(lifetime_retire_code))) return FALSE;
    for(unsigned i=0;i<LIFETIME_CAPACITY;++i) {
        EffectLifetime *entry=&lifetimes[i];
        if(!entry->registered || !entry->weak.entity) continue;
        if(!weak_links_valid(&entry->weak) || !lifetime_shutdown_drained()) { lifetime_fault(); return FALSE; }
        uint8_t *effect=entry->weak.entity;
        if(!memory_access(effect,0x3e4u,TRUE)) { lifetime_fault(); return FALSE; }
        if(effect[0x3e0u]&8u) continue; /* Already queued by this API or native completion. */
        const SudekiMpLanPartyEffectOwner *previous=lifetime_scope;
        lifetime_scope=&entry->owner;
        InterlockedIncrement(&lifetime_depth);
#ifdef SUDEKIMP_SPIRIT_VISUAL_HOST_TESTING
        if(lifetime_test_retire) lifetime_test_retire(effect);
        else
#endif
            ((void (__attribute__((stdcall)) *)(void *))((uint8_t *)image+0x131df0))(effect);
        InterlockedDecrement(&lifetime_depth);
        lifetime_scope=previous;
        if(!weak_links_valid(&entry->weak) ||
            (entry->weak.entity && !(effect[0x3e0u]&8u))) { lifetime_fault(); return FALSE; }
    }
    return TRUE;
}
BOOL SudekiMpLanPartyEffectLifetimeReset(void) {
    if(!SudekiMpLanPartyEffectLifetimePoll() || SudekiMpLanPartyEffectLifetimeRetains()) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    lifetime_witness=NULL;
    lifetime_shutdown_drained=NULL;
    if(lifetime_only) {
        if(!SudekiMpLanArenaSpiritVisualHostReset()) return FALSE;
        lifetime_only=FALSE;
    }
    return TRUE;
}
BOOL SudekiMpLanPartyShieldHostInitialize(HMODULE module,
    SudekiMpLanPartyShieldHostWitness witness, void *context) {
    if(!witness || !SudekiMpLanArenaSpiritVisualHostInitialize(
            module, no_spirit_scope, context)) return FALSE;
    shield_witness = witness;
    return TRUE;
}
BOOL SudekiMpLanPartyShieldHostCapture(uint64_t session, uint32_t host_tick,
    SudekiMpLanArenaSnapshot *output) {
    const char *reason = "shield_session_or_thread";
    void *buki, *tal;
    unknown_output(output);
    if(!shield_witness || !session || !output ||
        !InterlockedCompareExchange(&admitted, 0, 0)) return FALSE;
    if(game_thread == 0u) game_thread = GetCurrentThreadId();
    if(GetCurrentThreadId() != game_thread) return FALSE;
    if(InterlockedCompareExchange(&unexpected_thread, 0, 0)) registry.unknown = TRUE;
    if(registry.session && registry.session != session) {
        session_armed = FALSE;
        if(!reset_pending_emissions() ||
           !SudekiMpSpiritVisualHostRegistryReset(&registry, native_api())) goto unknown;
    }
    registry.session = session;
    if(!shield_actor_exact(SUDEKIMP_LAN_ARENA_BUKI_VFX_SHIELD_LOOP, &buki) ||
       !shield_actor_exact(SUDEKIMP_LAN_PARTY_TAL_VFX_SHIELD_LOOP, &tal)) goto unknown;
    /* Initialized before combat admission. Replaced actors cannot inherit a
     * previous native effect even if their network seat remains the same. */
    for(unsigned i=0; i<SUDEKIMP_SPIRIT_VISUAL_HOST_REGISTRY_CAPACITY; ++i) {
        SudekiMpSpiritVisualHostEntry *entry=&registry.entries[i];
        void *actor=entry->value.owner_actor_type==SUDEKIMP_LAN_ARENA_BUKI_TYPE?buki:tal;
        if(entry->status_actor && entry->status_actor!=actor) {
            if(!native_bind(image, &entry->weak, NULL)) goto unknown;
            memset(entry, 0, sizeof(*entry));
        }
    }
    session_armed = TRUE;
    sample_reason = NULL;
    if(!SudekiMpSpiritVisualHostRegistryCapture(&registry, session, output, native_api())) {
        reason = sample_reason ? sample_reason : "shield_lifetime_unknown";
        goto unknown;
    }
    for(unsigned i=0; i<output->spirit_vfx_count; ++i)
        if(!SudekiMpLanPartyShieldOwnerValid(&output->spirit_vfx[i]) ||
           (int32_t)(host_tick-output->spirit_vfx[i].emitted_host_tick)<0) {
            reason="shield_publication_identity"; goto unknown;
        }
    if(last_capture_reason || last_capture_count != output->spirit_vfx_count) {
        SudekiMpLogFormat("lan_party event=shield_roster observed=1 count=%u\r\n",
            output->spirit_vfx_count);
        last_capture_reason=NULL; last_capture_count=output->spirit_vfx_count;
    }
    return TRUE;
unknown:
    unknown_output(output);
    if(!last_capture_reason || strcmp(last_capture_reason,reason)) {
        SudekiMpLogFormat("lan_party event=shield_roster observed=0 reason=%s\r\n",reason);
        last_capture_reason=reason; last_capture_count=-1;
    }
    return FALSE;
}

BOOL SudekiMpLanArenaSpiritVisualHostReset(void) {
    InterlockedExchange(&admitted, 0);
    session_armed = FALSE;
    if (!finalize_hook.installed) return TRUE;
    if (InterlockedCompareExchange(&in_flight, 0, 0) != 0) return FALSE;
    /* Empty registries need no native call and can unbind at the loader seam.
     * A real linked node remains retained if native_bind rejects this thread. */
    if (!reset_pending_emissions()) return FALSE;
    return SudekiMpSpiritVisualHostRegistryReset(&registry, native_api());
}

static BOOL discover_actor_status_visuals(
    void *actor, uint8_t actor_type, uint64_t session, uint32_t tick
) {
    uint8_t *base = (uint8_t *)image;
    void *manager, *owner, *status, *effect, *arbiter, *status_owner;
    unsigned int i, token;
    uint8_t active;
    /* Exact CStatusEffectManager owner at +10; IsBoost at RVA5070 reads
     * manager+54 (slot6) and BoostStatusEffect+4c. The shared native status
     * visual helper owns StatusEffect+44, independent of any cast/source. */
    sample_reason = "status_actor_or_component_identity";
    if (actor == NULL || !pointer_at(actor, 0x90u, &arbiter) ||
        !pointer_at(arbiter, 0x10u, &owner) || owner != actor ||
        !pointer_at(actor, 0xa8u, &manager) ||
        !memory_access(manager, 0x78u, FALSE) ||
        *(void **)manager != base + 0x2d4abcu ||
        !pointer_at(manager, 0x10u, &owner) || owner != actor ||
        !pointer_at(manager, 0x54u, &status) ||
        !memory_access(status, 0x50u, FALSE) ||
        *(void **)status != base + 0x2cbf68u) return FALSE;
    active = *((uint8_t *)status + 0x4cu);
    if (active > 1u || !pointer_at(status, 0x44u, &effect) ||
        !pointer_at(status, 0x38u, &status_owner) ||
        (active && status_owner != (uint8_t *)manager + 4u)) return FALSE;
    /* A replaced actor must not carry its predecessor's aura. Retain weak
     * dependencies if unlink fails, just like session teardown. */
    for (i = 0u; i < SUDEKIMP_SPIRIT_VISUAL_HOST_REGISTRY_CAPACITY; ++i) {
        SudekiMpSpiritVisualHostEntry *entry = &registry.entries[i];
        if (entry->value.owner_actor_type == actor_type &&
            entry->status_actor != NULL && entry->status_actor != actor) {
            if (!native_bind(NULL, &entry->weak, NULL)) return FALSE;
            memset(entry, 0, sizeof(*entry));
        }
    }
    if (!active || effect == NULL) return TRUE;
    sample_reason = "status_visual_identity";
    if (!exact_effect(effect) || native_resource_kind(
            (uint8_t *)effect + 0x29cu) != SUDEKIMP_LAN_ARENA_STATUS_VFX_BOOST)
        return FALSE;
    token = SudekiMpSpiritVisualHostRegistryBeginOwned(
        &registry, session, 0u, tick, SUDEKIMP_LAN_ARENA_STATUS_VFX_BOOST,
        actor_type, effect, native_api());
    if (token == 0u) return FALSE;
    registry.entries[token - 1u].status_actor = actor;
    SudekiMpSpiritVisualHostRegistryComplete(&registry, token, TRUE, native_api());
    return registry.entries[token - 1u].state == ENTRY_READY;
}

BOOL SudekiMpLanArenaSpiritVisualHostCapture(
    uint64_t session, uint16_t current_skill, uint32_t host_tick,
    void *tal, void *ailish,
    SudekiMpLanArenaSnapshot *output
) {
    unsigned int i;
    const char *reason = "unbound_or_wrong_thread";
    unknown_output(output);
    if (!InterlockedCompareExchange(&admitted, 0, 0) ||
        output == NULL || session == 0u)
        return FALSE;
    if (game_thread == 0u) game_thread = GetCurrentThreadId();
    if (GetCurrentThreadId() != game_thread) return FALSE;
    if (InterlockedCompareExchange(&unexpected_thread, 0, 0)) registry.unknown = TRUE;
    reason = "session_lease_cleanup";
    if (registry.session != 0u && registry.session != session) {
        session_armed = FALSE;
        if (!reset_pending_emissions()) goto unknown;
        if (!SudekiMpSpiritVisualHostRegistryReset(&registry, native_api())) goto unknown;
        diagnostic_count = 0u;
        diagnostic_skill = 0u;
        inactive_kind_mask = 0u;
    }
    registry.session = session;
    if (!session_armed) {
        /* Capture follows a positively observed native Spirit manager state
         * in the host publisher. Late join during a cast cannot reconstruct
         * effects created before this observer owned the session. */
        if (output->seat[0].skill_active > 1u || output->seat[1].skill_active > 1u ||
            (output->seat[1].skill_kind == SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT &&
             output->seat[1].skill_active != 0u) ||
            (output->seat[0].skill_kind == SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT &&
             output->seat[0].skill_active != 0u)) {
            reason = "awaiting_inactive_spirit_baseline";
            goto unknown;
        }
        session_armed = TRUE;
    }
    sample_reason = NULL;
    if (!discover_actor_status_visuals(tal, seat_host_type(),
            session, host_tick) ||
        !discover_actor_status_visuals(ailish, seat_client_type(),
            session, host_tick)) {
        reason = sample_reason != NULL ? sample_reason : "status_visual_discovery";
        goto unknown;
    }
    if (!SudekiMpSpiritVisualHostRegistryCapture(&registry, session, output, native_api())) {
        reason = registry.unknown ? "missed_or_unowned_native_lifetime" :
            (sample_reason != NULL ? sample_reason : "pending_or_overflow_roster");
        goto unknown;
    }
    for (i = 0u; i < output->spirit_vfx_count; ++i) {
        const SudekiMpLanArenaSpiritVfxSnapshot *value = &output->spirit_vfx[i];
        unsigned int owner_seat = 0u;
        uint16_t skill_delta;
        uint32_t tick_delta = host_tick - value->emitted_host_tick;
        if (value->skill_sequence != 0u && value->owner_actor_type != 0u) {
            for (owner_seat = 0u; owner_seat < 2u; ++owner_seat)
                if (output->seat[owner_seat].actor_type == value->owner_actor_type) break;
            if (owner_seat == 2u) { reason = "visual_owner_not_in_session"; goto unknown; }
        }
        current_skill = output->seat[owner_seat].skill_sequence;
        skill_delta = (uint16_t)(current_skill - value->skill_sequence);
        if (tick_delta >= 0x80000000u ||
            (value->skill_sequence != 0u && (current_skill == 0u || skill_delta >= 0x8000u ||
            (skill_delta == 0u && output->seat[owner_seat].skill_kind !=
                SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT)))) {
            unknown_output(output);
            reason = "publication_before_native_emission";
            goto unknown;
        }
    }
    if (last_capture_reason != NULL || last_capture_count != (int)output->spirit_vfx_count) {
        SudekiMpLogFormat(
            "lan_arena_spirit_visual_host event=roster state=observed count=%u "
            "policy=complete_native_lifetime_set\r\n", (unsigned int)output->spirit_vfx_count);
        last_capture_reason = NULL;
        last_capture_count = output->spirit_vfx_count;
    }
    return TRUE;
unknown:
    if (last_capture_reason == NULL || strcmp(last_capture_reason, reason) != 0) {
        SudekiMpLogFormat(
            "lan_arena_spirit_visual_host event=roster state=unknown reason=%s "
            "policy=no_truncated_or_guessed_absence\r\n", reason);
        last_capture_reason = reason;
        last_capture_count = -1;
    }
    return FALSE;
}
