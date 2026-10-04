#ifndef SUDEKIMP_LAN_ARENA_SPIRIT_VISUAL_HOST_H
#define SUDEKIMP_LAN_ARENA_SPIRIT_VISUAL_HOST_H

#include "network/lan_arena_protocol.h"
#include <windows.h>
#include <stdint.h>

/* TRUE attributes this native emission to its exact session/caster/sequence.
 * A process-wide active flag is insufficient when both casters are active.
 * NULL source requires an exact current native scope; a non-NULL animation
 * component requires its own actor backlink/lease and must not borrow scope.
 * First Capture must run on the verified game/render seam and binds that
 * thread; Initialize itself may run on the loader thread. No native objects
 * are inspected by the hook before the first Capture. */
typedef BOOL (*SudekiMpLanArenaSpiritVisualHostWitness)(
    void *context, void *source_component,
    uint64_t *session, uint16_t *skill, uint32_t *host_tick,
    uint8_t *owner_actor_type);

/* SMP4 uses the same pinned native observer, with an actor-specific retained
 * roster witness. No legacy seat globals or cast ownership are borrowed. */
typedef BOOL (*SudekiMpLanPartyShieldHostWitness)(
    void *context, uint8_t actor_type, void **actor);
BOOL SudekiMpLanPartyShieldHostInitialize(HMODULE game_module,
    SudekiMpLanPartyShieldHostWitness witness, void *context);
BOOL SudekiMpLanPartyShieldHostCapture(uint64_t session, uint32_t host_tick,
    SudekiMpLanArenaSnapshot *output);

BOOL SudekiMpLanArenaSpiritVisualHostInitialize(
    HMODULE game_module, SudekiMpLanArenaSpiritVisualHostWitness witness,
    void *context);
/* Logical unbind. The physical hook and stable native weak nodes are pinned
 * until process exit. Failed unlink retains the node and fails closed. */
BOOL SudekiMpLanArenaSpiritVisualHostReset(void);
/* Call only after the host positively observed native Spirit state into
 * output->seat[0]. A new session first requires an inactive baseline; joining an
 * already active Spirit remains UNKNOWN until that baseline is observed. */
BOOL SudekiMpLanArenaSpiritVisualHostCapture(
    uint64_t session, uint16_t current_skill, uint32_t host_tick,
    void *tal, void *ailish,
    SudekiMpLanArenaSnapshot *output);
BOOL SudekiMpLanArenaSpiritVisualHostImageMatches(HMODULE game_module);

/* Native-only namespace ownership, separate from the closed network VFX
 * resource roster. A generation identifies a retained actor namespace, not a
 * transport seat or the last skill that happened to run. */
typedef struct SudekiMpLanPartyEffectOwner {
    uint64_t session;
    uint32_t generation;
    void *actor;
    uint8_t actor_type;
} SudekiMpLanPartyEffectOwner;
typedef BOOL (*SudekiMpLanPartyEffectWitness)(void *source_component,
    SudekiMpLanPartyEffectOwner *owner);
BOOL SudekiMpLanPartyEffectLifetimeInitialize(HMODULE module,
    SudekiMpLanPartyEffectWitness witness,BOOL (*shutdown_drained)(void));
/* First Poll binds the already verified native service thread. The observer
 * stays active through disconnect drain. Only native destructor-cleared weak
 * nodes prove retirement; no reset/unlink substitutes for completion. */
BOOL SudekiMpLanPartyEffectLifetimePoll(void);
BOOL SudekiMpLanPartyEffectLifetimeRetains(void);
BOOL SudekiMpLanPartyEffectLifetimeReset(void);
BOOL SudekiMpLanPartyEffectLifetimeCurrent(SudekiMpLanPartyEffectOwner *owner);
/* Shutdown owner must positively prove closed admission and drained native
 * bodies/tasks. Queues exact owned effects through native retirement; actual
 * observer destruction remains the sole completion witness. */
BOOL SudekiMpLanPartyEffectLifetimeRequestRetire(void);
#ifdef SUDEKIMP_SPIRIT_VISUAL_HOST_TESTING
void SudekiMpLanPartyEffectLifetimeTestRetire(void (*callback)(void *effect));
#endif

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
