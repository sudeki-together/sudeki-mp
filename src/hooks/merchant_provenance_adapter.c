#include "hooks/merchant_provenance_adapter.h"

#include "engine/log.h"
#include "hooks/call_hook.h"
#include "hooks/interaction_provenance.h"
#include "hooks/shop_catalog_adapter.h"
#include "hooks/split_screen_render.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Merchant provenance adapter requires the 32-bit Windows target"
#endif

#define SUDEKIMP_FASTCALL __attribute__((fastcall))

enum {
    RVA_SCRIPT_CALL_OPCODE = 0x001c4970u,
    RVA_SCRIPT_CALL_OPCODE_SLOT = 0x00323fa0u,
    RVA_SCRIPT_RUNTIME_GLOBAL = 0x003c310cu,
    SCRIPT_THREAD_INSTRUCTION_OFFSET = 0x0cu,
    SCRIPT_RUNTIME_BYTECODE_OFFSET = 0x14u
};

typedef int (SUDEKIMP_FASTCALL *ScriptCallOpcodeFunction)(void *, void *);

static const uint8_t script_call_opcode_signature[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u, 0x81u, 0xecu,
    0x90u, 0x00u, 0x00u, 0x00u, 0x53u, 0x55u, 0x56u, 0x57u,
    0x8bu, 0xe9u
};

static uint8_t *game_base;
static SudekiMpPointerHook script_call_opcode_hook;
static ScriptCallOpcodeFunction original_script_call_opcode;
static SudekiMpMerchantProvenanceTracker merchant_tracker;
static SudekiMpMerchantCatalogSnapshot merchant_catalog;
static int merchant_catalog_valid;
static SudekiMpMerchantCheckoutSession merchant_checkout;
static int merchant_checkout_valid;

static BOOL wallet_character_for_roster_type(
    unsigned int roster_type,
    SudekiMpWalletCharacterId *character_id
) {
    SudekiMpWalletCharacterId candidate;

    switch (roster_type) {
    case SUDEKIMP_WALLET_CHARACTER_TAL:
        candidate = SUDEKIMP_WALLET_CHARACTER_TAL;
        break;
    case SUDEKIMP_WALLET_CHARACTER_AILISH:
        candidate = SUDEKIMP_WALLET_CHARACTER_AILISH;
        break;
    case SUDEKIMP_WALLET_CHARACTER_BUKI:
        candidate = SUDEKIMP_WALLET_CHARACTER_BUKI;
        break;
    case SUDEKIMP_WALLET_CHARACTER_ELCO:
        candidate = SUDEKIMP_WALLET_CHARACTER_ELCO;
        break;
    default:
        return FALSE;
    }
    if (character_id != NULL) {
        *character_id = candidate;
    }
    return TRUE;
}

static BOOL readable_memory(const void *pointer, size_t size) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t start;
    uintptr_t end;
    uintptr_t region_end;

    if (pointer == NULL || size == 0u ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0u ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0u) {
        return FALSE;
    }
    start = (uintptr_t)pointer;
    end = start + size;
    region_end = (uintptr_t)information.BaseAddress + information.RegionSize;
    return end >= start && end <= region_end;
}

static BOOL current_opcode_hash(void *thread, uint32_t *call_hash) {
    uint8_t *runtime;
    uint8_t *bytecode;
    uint32_t instruction;

    if (game_base == NULL || thread == NULL || call_hash == NULL ||
        !readable_memory((uint8_t *)thread + SCRIPT_THREAD_INSTRUCTION_OFFSET,
            sizeof(instruction)) ||
        !readable_memory(game_base + RVA_SCRIPT_RUNTIME_GLOBAL,
            sizeof(runtime))) {
        return FALSE;
    }
    runtime = *(uint8_t **)(game_base + RVA_SCRIPT_RUNTIME_GLOBAL);
    if (!readable_memory(runtime, SCRIPT_RUNTIME_BYTECODE_OFFSET +
            sizeof(bytecode))) {
        return FALSE;
    }
    bytecode = *(uint8_t **)(runtime + SCRIPT_RUNTIME_BYTECODE_OFFSET);
    memcpy(&instruction,
        (uint8_t *)thread + SCRIPT_THREAD_INSTRUCTION_OFFSET,
        sizeof(instruction));
    if (!readable_memory(bytecode + instruction, sizeof(*call_hash))) {
        return FALSE;
    }
    memcpy(call_hash, bytecode + instruction, sizeof(*call_hash));
    return TRUE;
}

static BOOL observe_shop_start(void *thread, uint32_t call_hash) {
    SudekiMpSolInteractionProvenance source;
    SudekiMpMerchantInteractionEvidence evidence;
    SudekiMpMerchantProvenanceResult result;
    DWORD now = GetTickCount();

    if (call_hash != SUDEKIMP_MERCHANT_PROVENANCE_SHOP_START_HASH) {
        return FALSE;
    }
    if (!SudekiMpInteractionProvenanceFindSolThread((uintptr_t)thread, now,
            &source) || !SudekiMpSolInteractionAuthorityProven(&source, now)) {
        SudekiMpLogFormat(
            "merchant_provenance event=shop_start_seen thread=0x%08lx "
            "authority=unavailable policy=no_catalog_no_checkout\\r\\n",
            (unsigned long)(uintptr_t)thread);
        return FALSE;
    }
    ZeroMemory(&evidence, sizeof(evidence));
    evidence.sol_thread = (uintptr_t)thread;
    evidence.source_actor = source.source_actor;
    evidence.merchant_owner = source.target_owner;
    evidence.merchant_target = source.target;
    evidence.seat = source.player_index;
    evidence.actor_generation = source.actor_generation;
    evidence.source_generation = source.source_generation;
    evidence.observed_at_ms = now;
    evidence.interaction_authority_proven = 1;
    result = SudekiMpMerchantProvenanceObserveSolCall(&merchant_tracker,
        &evidence, call_hash);
    if (result == SUDEKIMP_MERCHANT_PROVENANCE_ACCEPTED ||
        result == SUDEKIMP_MERCHANT_PROVENANCE_REJECTED_STALE) {
        SudekiMpLogFormat(
            "merchant_provenance event=shop_start seat=%lu thread=0x%08lx "
            "source_actor=0x%08lx target_owner=0x%08lx target=0x%08lx "
            "source_generation=%lu result=%u policy=passive_no_checkout\\r\\n",
            (unsigned long)evidence.seat + 1ul,
            (unsigned long)evidence.sol_thread,
            (unsigned long)evidence.source_actor,
            (unsigned long)evidence.merchant_owner,
            (unsigned long)evidence.merchant_target,
            (unsigned long)evidence.source_generation,
            (unsigned int)result);
    }
    return result == SUDEKIMP_MERCHANT_PROVENANCE_ACCEPTED ||
        result == SUDEKIMP_MERCHANT_PROVENANCE_NO_CHANGE;
}

static void capture_shop_catalog(void) {
    SudekiMpMerchantProvenance provenance;
    unsigned int player_one_type;
    unsigned int player_two_type;
    unsigned int roster_type;
    SudekiMpWalletCharacterId character_id;
    SudekiMpMerchantCheckoutResult checkout_result;

    if (!SudekiMpMerchantProvenanceAdapterGet(&provenance)) {
        return;
    }
    ZeroMemory(&merchant_catalog, sizeof(merchant_catalog));
    merchant_catalog_valid = SudekiMpShopCatalogAdapterCapture(
        provenance.token,
        provenance.serial,
        TRUE,
        &merchant_catalog);
    if (!merchant_catalog_valid) {
        SudekiMpLogFormat(
            "merchant_provenance event=catalog_capture token=0x%08lx%08lx "
            "result=unavailable failure=%u policy=no_checkout\\r\\n",
            (unsigned long)(provenance.token >> 32u),
            (unsigned long)provenance.token,
            (unsigned int)SudekiMpShopCatalogAdapterLastFailure());
        return;
    }
    SudekiMpLogFormat(
        "merchant_provenance event=catalog_capture token=0x%08lx%08lx "
        "entries=%lu catalog_generation=%lu policy=read_only_no_checkout\\r\\n",
        (unsigned long)(provenance.token >> 32u),
        (unsigned long)provenance.token,
        (unsigned long)merchant_catalog.entry_count,
        (unsigned long)merchant_catalog.catalog_generation);

    merchant_checkout_valid = FALSE;
    if (provenance.seat >= 2u ||
        !SudekiMpSplitScreenGetRosterTypes(&player_one_type, &player_two_type)) {
        SudekiMpLogWrite(
            "merchant_provenance event=checkout_browse result=unavailable "
            "reason=roster_unavailable policy=no_checkout\r\n");
        return;
    }
    roster_type = provenance.seat == 0u ? player_one_type : player_two_type;
    if (!wallet_character_for_roster_type(roster_type, &character_id) ||
        !SudekiMpSplitScreenRosterActorIdentityMatches(provenance.seat,
            (const void *)provenance.source_actor, roster_type)) {
        SudekiMpLogWrite(
            "merchant_provenance event=checkout_browse result=unavailable "
            "reason=initiator_identity_unproven policy=no_checkout\r\n");
        return;
    }
    checkout_result = SudekiMpMerchantCheckoutOpen(&merchant_checkout,
        provenance.seat, character_id, provenance.actor_generation,
        &merchant_catalog);
    if (checkout_result != SUDEKIMP_MERCHANT_CHECKOUT_OPENED) {
        SudekiMpLogFormat(
            "merchant_provenance event=checkout_browse result=unavailable "
            "reason=session_open_rejected result_code=%u policy=no_checkout\r\n",
            (unsigned int)checkout_result);
        return;
    }
    merchant_checkout_valid = TRUE;
    SudekiMpLogFormat(
        "merchant_provenance event=checkout_browse seat=%lu character=0x%02lx "
        "actor_generation=%lu entries=%lu policy=read_only_no_checkout\r\n",
        (unsigned long)provenance.seat + 1ul,
        (unsigned long)character_id,
        (unsigned long)provenance.actor_generation,
        (unsigned long)merchant_catalog.entry_count);
}

static int SUDEKIMP_FASTCALL observe_script_call_opcode(
    void *thread,
    void *ignored_edx
) {
    uint32_t call_hash;
    BOOL shop_start;
    int result;

    shop_start = current_opcode_hash(thread, &call_hash) &&
        call_hash == SUDEKIMP_MERCHANT_PROVENANCE_SHOP_START_HASH;
    if (shop_start) {
        (void)observe_shop_start(thread, call_hash);
    }
    result = original_script_call_opcode(thread, ignored_edx);
    if (shop_start) {
        capture_shop_catalog();
    }
    return result;
}

BOOL SudekiMpInstallMerchantProvenanceAdapter(HMODULE game_module, BOOL enabled) {
    uint8_t *base = (uint8_t *)game_module;
    void **slot;

    SudekiMpUninstallMerchantProvenanceAdapter();
    if (!enabled) {
        return TRUE;
    }
    if (base == NULL || !readable_memory(base + RVA_SCRIPT_CALL_OPCODE,
            sizeof(script_call_opcode_signature)) ||
        memcmp(base + RVA_SCRIPT_CALL_OPCODE, script_call_opcode_signature,
            sizeof(script_call_opcode_signature)) != 0 ||
        !readable_memory(base + RVA_SCRIPT_CALL_OPCODE_SLOT, sizeof(slot))) {
        SetLastError(ERROR_BAD_EXE_FORMAT);
        return FALSE;
    }
    slot = (void **)(base + RVA_SCRIPT_CALL_OPCODE_SLOT);
    if (*slot != base + RVA_SCRIPT_CALL_OPCODE) {
        /* A competing opcode observer is never chained blindly. */
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    game_base = base;
    original_script_call_opcode = (ScriptCallOpcodeFunction)*slot;
    SudekiMpMerchantProvenanceInitialize(&merchant_tracker);
    SudekiMpMerchantCheckoutInitialize(&merchant_checkout);
    merchant_catalog_valid = FALSE;
    merchant_checkout_valid = FALSE;
    if (!SudekiMpShopCatalogAdapterInitialize(game_module)) {
        SudekiMpUninstallMerchantProvenanceAdapter();
        SetLastError(ERROR_BAD_EXE_FORMAT);
        return FALSE;
    }
    if (!SudekiMpInstallPointerHook(&script_call_opcode_hook, slot,
            original_script_call_opcode, observe_script_call_opcode)) {
        SudekiMpUninstallMerchantProvenanceAdapter();
        return FALSE;
    }
    return TRUE;
}

void SudekiMpUninstallMerchantProvenanceAdapter(void) {
    SudekiMpRestorePointerHook(&script_call_opcode_hook);
    original_script_call_opcode = NULL;
    game_base = NULL;
    merchant_catalog_valid = FALSE;
    merchant_checkout_valid = FALSE;
    ZeroMemory(&merchant_catalog, sizeof(merchant_catalog));
    SudekiMpMerchantCheckoutInitialize(&merchant_checkout);
    SudekiMpShopCatalogAdapterReset();
    SudekiMpMerchantProvenanceInvalidate(&merchant_tracker);
}

BOOL SudekiMpMerchantProvenanceAdapterGet(
    SudekiMpMerchantProvenance *provenance
) {
    SudekiMpSolInteractionProvenance current;
    SudekiMpMerchantProvenance candidate;
    DWORD now = GetTickCount();

    if (provenance == NULL || !SudekiMpMerchantProvenanceGet(&merchant_tracker,
            merchant_tracker.active.source_generation, &candidate) ||
        !SudekiMpInteractionProvenanceFindSolThread(candidate.sol_thread, now,
            &current) || !SudekiMpSolInteractionAuthorityProven(&current, now) ||
        current.source_generation != candidate.source_generation ||
        current.actor_generation != candidate.actor_generation ||
        current.source_actor != candidate.source_actor ||
        current.target_owner != candidate.merchant_owner ||
        current.target != candidate.merchant_target) {
        SudekiMpMerchantProvenanceInvalidate(&merchant_tracker);
        return FALSE;
    }
    *provenance = candidate;
    return TRUE;
}

BOOL SudekiMpMerchantProvenanceAdapterGetCatalog(
    SudekiMpMerchantCatalogSnapshot *catalog
) {
    SudekiMpMerchantProvenance provenance;

    if (catalog == NULL || !merchant_catalog_valid ||
        !SudekiMpMerchantProvenanceAdapterGet(&provenance) ||
        merchant_catalog.merchant_provenance != provenance.token ||
        merchant_catalog.merchant_generation != provenance.serial) {
        return FALSE;
    }
    *catalog = merchant_catalog;
    return TRUE;
}

BOOL SudekiMpMerchantProvenanceAdapterGetCheckoutSession(
    SudekiMpMerchantCheckoutSession *session
) {
    SudekiMpMerchantCatalogSnapshot catalog;

    if (session == NULL || !merchant_checkout_valid ||
        !SudekiMpMerchantProvenanceAdapterGetCatalog(&catalog) ||
        merchant_checkout.state != SUDEKIMP_MERCHANT_CHECKOUT_BROWSING ||
        merchant_checkout.catalog.merchant_provenance !=
            catalog.merchant_provenance ||
        merchant_checkout.catalog.catalog_generation != catalog.catalog_generation) {
        return FALSE;
    }
    *session = merchant_checkout;
    return TRUE;
}
