#ifndef SUDEKIMP_LAN_ARENA_SPIRIT_VISUAL_HOST_H
#define SUDEKIMP_LAN_ARENA_SPIRIT_VISUAL_HOST_H

#include "network/lan_arena_protocol.h"
#include <windows.h>
#include <stdint.h>

/* TRUE means this exact host session currently owns a native Tal Spirit.
 * First Capture must run on the verified game/render seam and binds that
 * thread; Initialize itself may run on the loader thread. No native objects
 * are inspected by the hook before the first Capture. */
typedef BOOL (*SudekiMpLanArenaSpiritVisualHostWitness)(
    void *context, uint64_t *session, uint16_t *skill, uint32_t *host_tick);

BOOL SudekiMpLanArenaSpiritVisualHostInitialize(
    HMODULE game_module, SudekiMpLanArenaSpiritVisualHostWitness witness,
    void *context);
/* Logical unbind. The physical hook and stable native weak nodes are pinned
 * until process exit. Failed unlink retains the node and fails closed. */
BOOL SudekiMpLanArenaSpiritVisualHostReset(void);
/* Call only after the host positively observed native Spirit state into
 * output->tal. A new session first requires an inactive baseline; joining an
 * already active Spirit remains UNKNOWN until that baseline is observed. */
BOOL SudekiMpLanArenaSpiritVisualHostCapture(
    uint64_t session, uint16_t current_skill, uint32_t host_tick,
    void *tal, void *ailish,
    SudekiMpLanArenaSnapshot *output);
BOOL SudekiMpLanArenaSpiritVisualHostImageMatches(HMODULE game_module);

/* Deterministic registry seams. Native adapters establish identity before
 * Begin; nodes must never move while attached to an engine observer list. */
enum { SUDEKIMP_SPIRIT_VISUAL_HOST_REGISTRY_CAPACITY = 32 };
typedef struct SudekiMpSpiritVisualWeakNode {
    void *entity;
    struct SudekiMpSpiritVisualWeakNode *previous;
    struct SudekiMpSpiritVisualWeakNode *next;
} SudekiMpSpiritVisualWeakNode;
typedef struct SudekiMpSpiritVisualHostEntry {
    SudekiMpSpiritVisualWeakNode weak;
    SudekiMpLanArenaSpiritVfxSnapshot value;
    uint8_t state;
    void *status_actor;
} SudekiMpSpiritVisualHostEntry;
typedef struct SudekiMpSpiritVisualHostRegistry {
    SudekiMpSpiritVisualHostEntry entries[
        SUDEKIMP_SPIRIT_VISUAL_HOST_REGISTRY_CAPACITY];
    uint64_t session;
    uint32_t next_instance;
    BOOL unknown;
} SudekiMpSpiritVisualHostRegistry;
typedef struct SudekiMpSpiritVisualHostApi {
    void *context;
    BOOL (*bind)(void *context, SudekiMpSpiritVisualWeakNode *node, void *entity);
    BOOL (*sample)(void *context, const SudekiMpSpiritVisualWeakNode *node,
        uint8_t kind, SudekiMpLanArenaSpiritVfxSnapshot *value);
} SudekiMpSpiritVisualHostApi;

uint8_t SudekiMpSpiritVisualKindForResource(uint32_t backing_identifier);
/* Retained-name aliases require native gfx kind 0x29, an exact fixed name,
 * and its matching uppercase hash. text_size includes the terminating NUL. */
uint8_t SudekiMpSpiritVisualKindForTypedResource(
    uint32_t encoded_kind, uint32_t identifier,
    const char *text, size_t text_size);
BOOL SudekiMpSpiritVisualDecomposeMatrix(
    const float matrix[16], SudekiMpLanArenaSpiritVfxSnapshot *value);
BOOL SudekiMpSpiritVisualHostRegistryReset(
    SudekiMpSpiritVisualHostRegistry *registry,
    const SudekiMpSpiritVisualHostApi *api);
/* A nonzero token identifies a pending native finalization, not a spawn.
 * Zero is returned for failure. The caller must mark unknown for a missed
 * recognized event; Begin already does so for its own failure cases. */
unsigned int SudekiMpSpiritVisualHostRegistryBegin(
    SudekiMpSpiritVisualHostRegistry *registry, uint64_t session,
    uint16_t skill, uint32_t tick, uint8_t kind, void *entity,
    const SudekiMpSpiritVisualHostApi *api);
unsigned int SudekiMpSpiritVisualHostRegistryBeginOwned(
    SudekiMpSpiritVisualHostRegistry *registry, uint64_t session,
    uint16_t skill, uint32_t tick, uint8_t kind, uint8_t owner_actor_type,
    void *entity, const SudekiMpSpiritVisualHostApi *api);
void SudekiMpSpiritVisualHostRegistryComplete(
    SudekiMpSpiritVisualHostRegistry *registry, unsigned int token,
    BOOL native_success, const SudekiMpSpiritVisualHostApi *api);
BOOL SudekiMpSpiritVisualHostRegistryCapture(
    SudekiMpSpiritVisualHostRegistry *registry, uint64_t session,
    SudekiMpLanArenaSnapshot *output, const SudekiMpSpiritVisualHostApi *api);

#endif
