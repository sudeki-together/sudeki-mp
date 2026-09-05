#include "hooks/lan_arena_spirit_visual_host.h"
#include "hooks/call_hook.h"
#include "engine/log.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

enum {
    RVA_FINALIZE = 0x18830u,
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
    0xf007401bu, 0xc24c6a03u, 0xa8171ecfu, 0x62dcc5a3u, 0xaeec0c83u
};
static const char *const resource_names[] = {
    NULL, "SFXSS250_INITIATE", "SFXSS251_INITIATE_LOOP_WAIT",
    "SFXSS112_SMALL_FLOOR_PATTERN", "SFXSS800_SPIRIT_A2T",
    "SFXSS252_MORPH_INTO_SPIRIT", "SFXSS801_SPIRIT_LINK",
    "SFXSS802_SPIRIT_END", "SFXSS300_TAL_SPIRIT_STRIKE",
    "SFXSS350_TAL_SPIRIT_STRIKE", "SFXSS110_LOOP_INVULNERABLE",
    "SFXSS111_END_INVULNERABLE", "SFXSS900_GENERIC_INITATE",
    "SFXSS351_TAL_HIT_CHARACTER"
};
enum { RESOURCE_TEXT_CAPACITY = 64, DIAGNOSTIC_CAPACITY = 16 };
static const uint8_t finalize_prefix[] = {
    0x55,0x8b,0xec,0x83,0xe4,0xf8,0x83,0xec,0x14,0x53,0x56,0x57,
    0x8b,0xf8,0x8b,0x47,0x1c,0x85,0xc0,0x0f,0x84,0xd3,0x01,0x00,0x00
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
static SudekiMpSpiritVisualHostRegistry registry;
static HMODULE image;
static DWORD game_thread;
static SudekiMpLanArenaSpiritVisualHostWitness active_witness;
static void *witness_context;
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
static uint16_t inactive_kind_mask;

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
    unsigned int i, free_slot = SUDEKIMP_SPIRIT_VISUAL_HOST_REGISTRY_CAPACITY;
    SudekiMpSpiritVisualHostEntry *entry;
    if (r == NULL || !api_valid(api)) return 0u;
    if (session == 0u || skill == 0u || entity == NULL || kind == 0u ||
        kind >= sizeof(resource_ids)/sizeof(resource_ids[0]) ||
        (r->session != 0u && r->session != session)) {
        r->unknown = TRUE;
        return 0u;
    }
    r->session = session;
    for (i = 0; i < SUDEKIMP_SPIRIT_VISUAL_HOST_REGISTRY_CAPACITY; ++i) {
        entry = &r->entries[i];
        if (entry->weak.entity == entity) {
            if (entry->value.kind != kind || entry->value.skill_sequence != skill)
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
    const char *skip_reason = NULL;
    InterlockedIncrement(&in_flight);
    if (game_thread != 0u && session_armed &&
        InterlockedCompareExchange(&admitted, 0, 0) != 0) {
        if (GetCurrentThreadId() != game_thread) {
            /* An unexpected thread never dereferences game objects. */
            InterlockedExchange(&unexpected_thread, 1);
        } else {
            uint8_t *s = (uint8_t *)setup;
            uint8_t kind = 0u;
            BOOL active = active_witness != NULL &&
                active_witness(witness_context, &session, &skill, &tick);
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
                if (!active && kind != 0u &&
                    (inactive_kind_mask & (uint16_t)(1u << kind)) == 0u) {
                    inactive_kind_mask |= (uint16_t)(1u << kind);
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
                    else if (entity != NULL) token = SudekiMpSpiritVisualHostRegistryBegin(
                        &registry, session, skill, tick, kind, entity, native_api());
                }
            }
        }
    }
    result = invoke_finalize(setup, mode);
    if (token != 0u) {
        instance = registry.entries[token - 1u].value.instance_sequence;
        SudekiMpSpiritVisualHostRegistryComplete(
            &registry, token, result != 0u, native_api());
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
    unsigned int i;
    if (base == NULL ||
        !matches(base, RVA_FINALIZE, finalize_prefix, sizeof(finalize_prefix)) ||
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
    if (finalize_hook.installed) {
        if (image != module || InterlockedCompareExchange(&in_flight, 0, 0) != 0 ||
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
    }
    active_witness = witness;
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

BOOL SudekiMpLanArenaSpiritVisualHostReset(void) {
    InterlockedExchange(&admitted, 0);
    session_armed = FALSE;
    if (!finalize_hook.installed) return TRUE;
    if (InterlockedCompareExchange(&in_flight, 0, 0) != 0) return FALSE;
    /* Empty registries need no native call and can unbind at the loader seam.
     * A real linked node remains retained if native_bind rejects this thread. */
    return SudekiMpSpiritVisualHostRegistryReset(&registry, native_api());
}

BOOL SudekiMpLanArenaSpiritVisualHostCapture(
    uint64_t session, uint16_t current_skill, uint32_t host_tick,
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
        if (output->tal.skill_active > 1u ||
            (output->tal.skill_kind == SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT &&
             output->tal.skill_active != 0u)) {
            reason = "awaiting_inactive_spirit_baseline";
            goto unknown;
        }
        session_armed = TRUE;
    }
    sample_reason = NULL;
    if (!SudekiMpSpiritVisualHostRegistryCapture(&registry, session, output, native_api())) {
        reason = registry.unknown ? "missed_or_unowned_native_lifetime" :
            (sample_reason != NULL ? sample_reason : "pending_or_overflow_roster");
        goto unknown;
    }
    for (i = 0u; i < output->spirit_vfx_count; ++i) {
        const SudekiMpLanArenaSpiritVfxSnapshot *value = &output->spirit_vfx[i];
        uint16_t skill_delta = (uint16_t)(current_skill - value->skill_sequence);
        uint32_t tick_delta = host_tick - value->emitted_host_tick;
        if (current_skill == 0u || skill_delta >= 0x8000u ||
            tick_delta >= 0x80000000u ||
            (skill_delta == 0u && output->tal.skill_kind !=
                SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT)) {
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
