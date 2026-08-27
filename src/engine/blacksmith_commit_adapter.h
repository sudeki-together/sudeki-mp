#ifndef SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_H
#define SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_H

#include "engine/blacksmith_shadow.h"

#include <stddef.h>
#include <stdint.h>

/* Sudeki persists at most three augmentation bytes per supported equipment
 * item. 0xff is the native empty value and therefore is not a component ID. */
enum {
    SUDEKIMP_BLACKSMITH_NATIVE_SOCKET_COUNT = 3u,
    SUDEKIMP_BLACKSMITH_EMPTY_SOCKET = 0xffu
};

typedef enum SudekiMpBlacksmithCommitAdapterState {
    SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_DISABLED = 0,
    SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_IDLE,
    SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_EXECUTING,
    SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_QUARANTINED
} SudekiMpBlacksmithCommitAdapterState;

typedef enum SudekiMpBlacksmithCommitAdapterStatus {
    SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_DISABLED = 0,
    SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_INVALID_ARGUMENT,
    SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_WRONG_THREAD,
    SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_NO_CLAIM,
    SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_BUSY,
    SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_LIFECYCLE_UNSAFE,
    SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_NATIVE_MODAL_ACTIVE,
    SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_STALE_GENERATION,
    SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_ACTOR_STALE,
    SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_MERCHANT_STALE,
    SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_EQUIPMENT_STALE,
    SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_COMPONENT_UNAVAILABLE,
    SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_CATALOG_MISSING,
    SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_CATALOG_DUPLICATE,
    SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_PRICE_CHANGED,
    /* Zero-price catalog rows remain preview-only until the coordinator has
     * an explicit no-debit/economy-generation contract. */
    SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_ZERO_COST_UNSUPPORTED,
    SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_INVALID_SOCKET,
    SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_SLOT_LOCKED,
    /* This first commit milestone supports only exact 0xff -> component-byte
     * writes. Replacement previews must not expose confirmation yet. */
    SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_SOCKET_OCCUPIED,
    SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_DUPLICATE_COMPONENT,
    SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_INCOMPATIBLE,
    SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_INSUFFICIENT_FUNDS,
    SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_MUTATION_REJECTED,
    SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_VERIFIED,
    SUDEKIMP_BLACKSMITH_COMMIT_ADAPTER_STATUS_QUARANTINED
} SudekiMpBlacksmithCommitAdapterStatus;

typedef enum SudekiMpBlacksmithCommitReadResult {
    SUDEKIMP_BLACKSMITH_COMMIT_READ_RESOLVED = 0,
    /* The backend could not prove a complete observation. This is not the
     * same as a normal missing catalog row represented in the observation. */
    SUDEKIMP_BLACKSMITH_COMMIT_READ_FAILED,
    SUDEKIMP_BLACKSMITH_COMMIT_READ_AMBIGUOUS
} SudekiMpBlacksmithCommitReadResult;

typedef enum SudekiMpBlacksmithLowLevelMutationResult {
    /* Native helper 0x530730 returned AL == 0. */
    SUDEKIMP_BLACKSMITH_LOW_LEVEL_MUTATION_RETURNED_FALSE = 0,
    /* Native helper 0x530730 returned AL != 0. */
    SUDEKIMP_BLACKSMITH_LOW_LEVEL_MUTATION_RETURNED_TRUE,
    /* The backend cannot prove whether the call occurred or returned. */
    SUDEKIMP_BLACKSMITH_LOW_LEVEL_MUTATION_AMBIGUOUS
} SudekiMpBlacksmithLowLevelMutationResult;

typedef enum SudekiMpBlacksmithDebitResult {
    SUDEKIMP_BLACKSMITH_DEBIT_NOT_CALLED = 0,
    SUDEKIMP_BLACKSMITH_DEBIT_CALLED_ONCE,
    SUDEKIMP_BLACKSMITH_DEBIT_AMBIGUOUS
} SudekiMpBlacksmithDebitResult;

/* Pointer-free facts re-resolved from stable ticket IDs. A backend may use
 * transient native pointers internally during one callback, but must not
 * return or retain them as transaction authority. */
typedef struct SudekiMpBlacksmithCommitObservation {
    SudekiMpBlacksmithSharedSnapshot snapshot;

    uint32_t character_id;
    uint32_t actor_generation;
    int actor_resolved;

    uint32_t merchant_id;
    uint32_t merchant_generation;
    int merchant_resolved;

    uint32_t equipment_item_id;
    uint32_t equipment_socket_count;
    int equipment_socket_bank;
    int equipment_resolved;
    int equipment_is_equipped_by_actor;

    uint32_t component_item_id;
    int component_resolved;

    uint32_t catalog_match_count;
    uint32_t catalog_price;
    uint32_t shared_money;

    /* Both must remain false through all three observations. A save/load can
     * otherwise serialize or replace shared state between write and debit;
     * an active native modal could issue a second process-global commit. */
    int save_or_load_active;
    int native_blacksmith_modal_active;

    uint8_t socket_byte;
    int socket_byte_resolved;
    /* The exact persistence domains are equipment IDs 0..0x35 and 100..139,
     * with three signed bytes per item. Effective occupancy uses the authored
     * socket record first, so authored_component_id must be exactly -1 before
     * a backing-byte write is eligible. */
    int equipment_storage_supported;
    int32_t authored_component_id;
    int slot_unlocked;
    int component_compatible;
    /* True only when the candidate's exact native class policy forbids a
     * duplicate and such a duplicate is present. Legal class-1 duplicates do
     * not set this flag. */
    int forbidden_duplicate_present;
} SudekiMpBlacksmithCommitObservation;

/* Every callback runs synchronously without yielding on the game thread.
 * resolve must freshly walk actor/equipment/catalog/inventory state on each
 * invocation. Before a native backend may return RESOLVED it must exact-gate
 * the executable, re-read unchanged singleton roots, prove the current actor
 * lease/equipped chain, require itemDef+0x14 == the ticket equipment ID,
 * require rune ID < manager count with a nonnull definition, and validate
 * executable item vtable slots used by compatibility. The low-
 * level callback must call only the proven 0x530730 helper (EAX=itemDef,
 * EDI=socket, stack=component ID, AL result); it must never call native
 * UIBlackSmith confirmation at 0x4927c0. subtract_money_once may call the
 * proven CInventory::SubtractMoney only after the adapter observed the exact
 * socket-byte write. */
typedef struct SudekiMpBlacksmithCommitBackend {
    void *context;
    int (*is_game_thread)(void *context);
    SudekiMpBlacksmithCommitReadResult (*resolve)(
        void *context,
        const SudekiMpBlacksmithCommitTicket *ticket,
        SudekiMpBlacksmithCommitObservation *observation
    );
    SudekiMpBlacksmithLowLevelMutationResult (*call_low_level_mutation)(
        void *context,
        const SudekiMpBlacksmithCommitTicket *ticket
    );
    SudekiMpBlacksmithDebitResult (*subtract_money_once)(
        void *context,
        const SudekiMpBlacksmithCommitTicket *ticket,
        uint32_t amount
    );
} SudekiMpBlacksmithCommitBackend;

typedef struct SudekiMpBlacksmithCommitAdapter {
    SudekiMpBlacksmithCommitAdapterState state;
    SudekiMpBlacksmithCommitAdapterStatus last_status;
    uint32_t last_ticket_serial;
    uint32_t verified_commit_count;
} SudekiMpBlacksmithCommitAdapter;

/* Exact GOG-build gates for the low-level mutation, compatibility, slot-read,
 * equipment-byte writer, and money-debit entries. The loaded-image form also
 * validates relocated inventory/rune-manager operands. Passing this gate is
 * required of a native backend; it does not enable or install anything. */
int SudekiMpBlacksmithCommitSignaturesMatch(
    const uint8_t *image,
    size_t image_size
);
int SudekiMpBlacksmithCommitLoadedSignaturesMatch(
    const uint8_t *image,
    size_t image_size,
    uintptr_t loaded_image_base
);

/* Initialization is deliberately fail closed. Enabling this engine layer
 * does not wire it to the blacksmith UI or party lifecycle. */
void SudekiMpBlacksmithCommitAdapterInitialize(
    SudekiMpBlacksmithCommitAdapter *adapter
);
int SudekiMpBlacksmithCommitAdapterSetEnabled(
    SudekiMpBlacksmithCommitAdapter *adapter,
    int enabled
);

/* Executes the coordinator's one immutable claimed ticket. Rejection before
 * mutation resolves the ticket as NOT_APPLIED. Any incomplete read, partial
 * write, debit uncertainty, or postcondition mismatch quarantines both the
 * adapter and the coordinator; an ambiguous ticket is never replayed. */
SudekiMpBlacksmithCommitAdapterStatus SudekiMpBlacksmithCommitAdapterExecute(
    SudekiMpBlacksmithCommitAdapter *adapter,
    SudekiMpBlacksmithShadowCoordinator *coordinator,
    const SudekiMpBlacksmithCommitBackend *backend
);

const char *SudekiMpBlacksmithCommitAdapterStatusName(
    SudekiMpBlacksmithCommitAdapterStatus status
);

#endif
