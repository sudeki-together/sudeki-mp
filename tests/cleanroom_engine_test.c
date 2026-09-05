#include "cleanroom/engine.h"

#include <stdio.h>

static DWORD ranged_prime_test_tick;
static BOOL ranged_prime_test_combat_enabled;
static BOOL ranged_prime_test_combat_known;
static BOOL ranged_prime_test_ui_active;
static BOOL ranged_prime_test_ui_enter_fails;
static BOOL ranged_prime_test_ui_exit_fails;
static unsigned int ranged_prime_test_ui_enter_count;
static unsigned int ranged_prime_test_ui_exit_count;
static DWORD ranged_prime_test_thread_id;

enum {
    TEST_CHARACTER_SKILL_OFFSET = 0x00d8u,
    TEST_CSKILL_OWNER_OFFSET = 0x0010u,
    TEST_CSKILL_DATA_ARRAY_OFFSET = 0x003cu
};

static int require_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "cleanroom_engine_test: %s\n", message);
        return 0;
    }
    return 1;
}

static BOOL set_ranged_prime_test_ui_active(BOOL active) {
    if ((active && ranged_prime_test_ui_enter_fails) ||
        (!active && ranged_prime_test_ui_exit_fails)) {
        SetLastError(ERROR_WRITE_FAULT);
        return FALSE;
    }
    ranged_prime_test_ui_active = active != FALSE;
    if (active) {
        ++ranged_prime_test_ui_enter_count;
    } else {
        ++ranged_prime_test_ui_exit_count;
    }
    return TRUE;
}

static BOOL ranged_prime_test_combat_mode(BOOL *enabled) {
    if (enabled == NULL || !ranged_prime_test_combat_known) {
        return FALSE;
    }
    *enabled = ranged_prime_test_combat_enabled;
    return TRUE;
}

static DWORD ranged_prime_test_tick_count(void) {
    return ranged_prime_test_tick;
}

static DWORD ranged_prime_test_current_thread_id(void) {
    return ranged_prime_test_thread_id;
}

static int verify_ranged_prime_deadline_lifecycle(void) {
    SudekiMpCleanroomEngineRangedPrimeTestBackend backend = {
        set_ranged_prime_test_ui_active,
        ranged_prime_test_combat_mode,
        ranged_prime_test_tick_count,
        ranged_prime_test_current_thread_id
    };
    uint32_t first_generation;
    uint32_t stale_generation;

    SudekiMpCleanroomEngineResetRangedPrimeForTesting();
    ranged_prime_test_tick = UINT32_C(0xfffffff0);
    ranged_prime_test_combat_enabled = TRUE;
    ranged_prime_test_combat_known = TRUE;
    ranged_prime_test_ui_active = FALSE;
    ranged_prime_test_ui_enter_fails = FALSE;
    ranged_prime_test_ui_exit_fails = FALSE;
    ranged_prime_test_ui_enter_count = 0u;
    ranged_prime_test_ui_exit_count = 0u;
    ranged_prime_test_thread_id = 10u;

    if (!require_true(
            SudekiMpCleanroomEngineSetRangedPrimeTestBackend(&backend),
            "ranged-prime test backend was rejected") ||
        !require_true(
            !SudekiMpCleanroomEngineRangedCombatPrimePending(),
            "ranged-prime lease began pending") ||
        !require_true(
            SudekiMpCleanroomEnginePrimeRangedCombat(),
            "ranged-prime deadline did not arm") ||
        !require_true(ranged_prime_test_ui_active &&
                ranged_prime_test_ui_enter_count == 1u,
            "ranged-prime deadline did not acquire native UI") ||
        !require_true(
            SudekiMpCleanroomEngineRangedCombatPrimePending(),
            "ranged-prime deadline retired before 75 ms")) {
        return 0;
    }

    first_generation =
        SudekiMpCleanroomEngineRangedPrimeGenerationForTesting();
    stale_generation = first_generation == UINT32_MAX ?
        first_generation - 1u : first_generation + 1u;
    ranged_prime_test_tick += 74u;
    if (!require_true(
            SudekiMpCleanroomEngineRangedCombatPrimePending(),
            "ranged-prime wrap-safe deadline retired at 74 ms") ||
        !require_true(
            !SudekiMpCleanroomEngineServiceRangedPrimeForTesting(
                stale_generation),
            "stale ranged-prime generation was accepted") ||
        !require_true(ranged_prime_test_ui_active &&
                ranged_prime_test_ui_exit_count == 0u,
            "stale ranged-prime generation changed native UI")) {
        return 0;
    }

    ranged_prime_test_tick += 1u;
    if (!require_true(
            SudekiMpCleanroomEngineRangedCombatPrimePending(),
            "pure ranged-prime witness retired due work off service path") ||
        !require_true(
            SudekiMpCleanroomEngineServiceRangedPrimeForTesting(
                first_generation),
            "ranged-prime deadline service failed at 75 ms") ||
        !require_true(
            !SudekiMpCleanroomEngineRangedCombatPrimePending(),
            "ranged-prime deadline remained pending at 75 ms") ||
        !require_true(!ranged_prime_test_ui_active &&
                ranged_prime_test_ui_exit_count == 1u,
            "ranged-prime deadline did not release native UI") ||
        !require_true(
            SudekiMpCleanroomEngineRangedPrimeGenerationForTesting() !=
                first_generation,
            "ranged-prime retirement did not invalidate its generation")) {
        return 0;
    }

    ranged_prime_test_tick = 1000u;
    if (!require_true(
            SudekiMpCleanroomEnginePrimeRangedCombat(),
            "second ranged-prime deadline did not arm")) {
        return 0;
    }
    first_generation =
        SudekiMpCleanroomEngineRangedPrimeGenerationForTesting();
    ranged_prime_test_ui_exit_fails = TRUE;
    if (!require_true(
            !SudekiMpCleanroomEngineCancelRangedPrimeForTesting(),
            "failed native UI release was reported quiesced") ||
        !require_true(
            SudekiMpCleanroomEngineRangedCombatPrimePending(),
            "failed native UI release cleared pending witness") ||
        !require_true(ranged_prime_test_ui_active,
            "failed native UI release dropped UI ownership") ||
        !require_true(
            !SudekiMpCleanroomEnginePrimeRangedCombat() &&
                GetLastError() == ERROR_BUSY,
            "new ranged-prime lease entered during retryable cancellation")) {
        return 0;
    }
    ranged_prime_test_ui_exit_fails = FALSE;
    if (!require_true(
            SudekiMpCleanroomEngineServiceRangedPrimeForTesting(
                first_generation),
            "retryable ranged-prime cancellation service failed") ||
        !require_true(
            !SudekiMpCleanroomEngineRangedCombatPrimePending(),
            "retryable ranged-prime cancellation did not quiesce") ||
        !require_true(!ranged_prime_test_ui_active,
            "retryable ranged-prime cancellation retained native UI")) {
        return 0;
    }

    ranged_prime_test_combat_enabled = FALSE;
    ranged_prime_test_tick = 2000u;
    if (!require_true(
            SudekiMpCleanroomEnginePrimeRangedCombat(),
            "combat-disabled ranged-prime deadline did not arm")) {
        return 0;
    }
    first_generation =
        SudekiMpCleanroomEngineRangedPrimeGenerationForTesting();
    ranged_prime_test_tick += 75u;
    if (!require_true(
            SudekiMpCleanroomEngineServiceRangedPrimeForTesting(
                first_generation),
            "combat-disabled ranged-prime service failed") ||
        !require_true(
            !SudekiMpCleanroomEngineRangedCombatPrimePending(),
            "combat-disabled ranged-prime deadline did not cancel") ||
        !require_true(!ranged_prime_test_ui_active,
            "combat-disabled ranged-prime retained native UI")) {
        return 0;
    }

    ranged_prime_test_combat_enabled = TRUE;
    ranged_prime_test_tick = 3000u;
    if (!require_true(
            SudekiMpCleanroomEnginePrimeRangedCombat(),
            "thread-affinity ranged-prime deadline did not arm")) {
        return 0;
    }
    first_generation =
        SudekiMpCleanroomEngineRangedPrimeGenerationForTesting();
    ranged_prime_test_thread_id = 11u;
    if (!require_true(
            !SudekiMpCleanroomEngineCancelRangedPrimeForTesting() &&
                GetLastError() == ERROR_BUSY,
            "foreign-thread ranged-prime cancellation was accepted") ||
        !require_true(
            SudekiMpCleanroomEngineRangedCombatPrimePending() &&
                ranged_prime_test_ui_active,
            "foreign-thread cancellation dropped the pending UI lease")) {
        return 0;
    }
    ranged_prime_test_thread_id = 10u;
    if (!require_true(
            SudekiMpCleanroomEngineServiceRangedPrimeForTesting(
                first_generation),
            "owner-thread ranged-prime cancellation service failed") ||
        !require_true(
            !SudekiMpCleanroomEngineRangedCombatPrimePending() &&
                !ranged_prime_test_ui_active,
            "owner-thread ranged-prime cancellation did not quiesce")) {
        return 0;
    }

    ranged_prime_test_combat_enabled = TRUE;
    ranged_prime_test_ui_enter_fails = TRUE;
    if (!require_true(
            !SudekiMpCleanroomEnginePrimeRangedCombat(),
            "failed native UI acquire armed a ranged-prime lease") ||
        !require_true(
            !SudekiMpCleanroomEngineRangedCombatPrimePending(),
            "failed native UI acquire left a pending lease")) {
        return 0;
    }

    ranged_prime_test_ui_enter_fails = FALSE;
    SudekiMpCleanroomEngineResetRangedPrimeForTesting();
    return require_true(
        !SudekiMpCleanroomEngineRangedCombatPrimePending(),
        "ranged-prime test reset did not quiesce");
}

static int verify_training_learned_skill_lease(void) {
    uint32_t actor_words[64] = {0}, skill_words[32] = {0};
    uint32_t context_words[96] = {0}, data_words[6][4] = {{0}};
    uint8_t *actor = (uint8_t *)actor_words;
    uint8_t *skill = (uint8_t *)skill_words;
    uint8_t *context = (uint8_t *)context_words;
    static const unsigned int bases[] = {0x136u, 0x13cu, 0x13au, 0x138u};
    unsigned int hero, slot;
    *(void **)(actor + 0xd8u) = skill;
    *(void **)(actor + 0xd4u) = context;
    *(void **)(skill + 0x10u) = actor;
    *(void **)(context + 0x10u) = actor;
    for (slot = 0; slot < 6; ++slot) {
        *(void **)(skill + 0x3cu + slot * 4u) = data_words[slot];
        data_words[slot][3] = slot;
    }
    for (hero = 0; hero < 4; ++hero) {
        int16_t *learned[6];
        for (slot = 0; slot < 6; ++slot) {
            learned[slot] = (int16_t *)(context + bases[hero] + slot * 8u);
            *learned[slot] = slot == 2 ? 3 : 0;
        }
        if (!require_true(SudekiMpCleanroomEngineTrainingSkillLeaseForTesting(
                hero, actor, TRUE), "training learned-skill lease failed")) return 0;
        for (slot = 0; slot < 6; ++slot) {
            if (!require_true(*learned[slot] == (slot == 2 ? 3 : 1) &&
                    ((uint8_t *)data_words[slot])[8] == 1,
                    "training failed to learn all skills or changed learned rank")) return 0;
        }
        *learned[4] = 2; /* Another owner advanced this skill during the lease. */
        if (!require_true(SudekiMpCleanroomEngineTrainingSkillLeaseForTesting(
                hero, actor, FALSE), "training learned-skill release failed")) return 0;
        for (slot = 0; slot < 6; ++slot) {
            int expected = slot == 2 ? 3 : (slot == 4 ? 2 : 0);
            if (!require_true(*learned[slot] == expected &&
                    ((uint8_t *)data_words[slot])[8] == 0,
                    "training release lost learned values or availability")) return 0;
        }
        for (slot = 0; slot < 6; ++slot) *learned[slot] = 0;
    }
    *(void **)(context + 0x10u) = NULL;
    return require_true(!SudekiMpCleanroomEngineTrainingSkillLeaseForTesting(
        3, actor, TRUE), "training accepted foreign skill context");
}

static int verify_training_skill_allocation_gate(void) {
    uint8_t *actor;
    uint8_t *skill_allocation;
    uint8_t *skill;
    DWORD old_protection;
    int result = 0;

    actor = (uint8_t *)VirtualAlloc(
        NULL, 4096u, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    skill_allocation = (uint8_t *)VirtualAlloc(
        NULL, 8192u, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!require_true(actor != NULL && skill_allocation != NULL,
            "training-skill validation allocation failed")) {
        goto cleanup;
    }

    skill = skill_allocation;
    *(void **)(actor + TEST_CHARACTER_SKILL_OFFSET) = skill;
    *(void **)(skill + TEST_CSKILL_OWNER_OFFSET) = actor;
    if (!require_true(
            SudekiMpCleanroomEngineTrainingSkillAllocationValidForTesting(
                actor, skill),
            "valid exact CSkill allocation/owner tuple was rejected")) {
        goto cleanup;
    }

    *(void **)(skill + TEST_CSKILL_OWNER_OFFSET) = skill;
    if (!require_true(
            !SudekiMpCleanroomEngineTrainingSkillAllocationValidForTesting(
                actor, skill),
            "foreign CSkill owner was accepted")) {
        goto cleanup;
    }
    *(void **)(skill + TEST_CSKILL_OWNER_OFFSET) = actor;
    *(void **)(actor + TEST_CHARACTER_SKILL_OFFSET) = actor;
    if (!require_true(
            !SudekiMpCleanroomEngineTrainingSkillAllocationValidForTesting(
                actor, skill),
            "stale actor-to-CSkill backlink was accepted")) {
        goto cleanup;
    }
    *(void **)(actor + TEST_CHARACTER_SKILL_OFFSET) = skill;
    if (!require_true(VirtualProtect(
            skill_allocation, 4096u, PAGE_NOACCESS, &old_protection),
            "training-skill unreadable allocation protection failed")) {
        goto cleanup;
    }
    if (!require_true(
            !SudekiMpCleanroomEngineTrainingSkillAllocationValidForTesting(
                actor, skill),
            "unreadable cached CSkill allocation was accepted")) {
        VirtualProtect(skill_allocation, 4096u,
            old_protection, &old_protection);
        goto cleanup;
    }
    if (!require_true(VirtualProtect(
            skill_allocation, 4096u, old_protection, &old_protection),
            "training-skill allocation protection restore failed")) {
        goto cleanup;
    }

    /* Keep the owner field readable in page one while forcing the six-entry
     * SkillData array to cross into a no-access second page. */
    skill = skill_allocation + 4096u -
        TEST_CSKILL_DATA_ARRAY_OFFSET - sizeof(void *);
    *(void **)(actor + TEST_CHARACTER_SKILL_OFFSET) = skill;
    *(void **)(skill + TEST_CSKILL_OWNER_OFFSET) = actor;
    if (!require_true(VirtualProtect(
            skill_allocation + 4096u, 4096u, PAGE_NOACCESS,
            &old_protection),
            "training-skill unreadable array protection failed")) {
        goto cleanup;
    }
    if (!require_true(
            !SudekiMpCleanroomEngineTrainingSkillAllocationValidForTesting(
                actor, skill),
            "partially unreadable CSkill data array was accepted")) {
        VirtualProtect(skill_allocation + 4096u, 4096u,
            old_protection, &old_protection);
        goto cleanup;
    }
    if (!require_true(VirtualProtect(
            skill_allocation + 4096u, 4096u, old_protection,
            &old_protection),
            "training-skill array protection restore failed")) {
        goto cleanup;
    }
    result = 1;

cleanup:
    if (skill_allocation != NULL) {
        VirtualFree(skill_allocation, 0u, MEM_RELEASE);
    }
    if (actor != NULL) VirtualFree(actor, 0u, MEM_RELEASE);
    return result;
}

int main(void) {
    unsigned int actor;
    BOOL mode = FALSE;
    DWORD old_protection;
    uint32_t *read_only_reference;
    uint32_t resource_reference[2] = {2u, 1u};
    SudekiMpResourceName resource_source = {
        UINT32_C(0x12345678),
        UINT32_C(0x00004567),
        resource_reference
    };
    SudekiMpResourceName resource_copy;

    if (!verify_ranged_prime_deadline_lifecycle() ||
        !verify_training_learned_skill_lease() ||
        !verify_training_skill_allocation_gate() ||
        !require_true(
            !SudekiMpCleanroomEngineTrainingSkills(&mode),
            "training skills were available before initialization") ||
        !require_true(
            SudekiMpCleanroomEnginePostRestoreControlTupleActive(
                0, 0u, 1, 0u, 0, 1u, 0, 1u),
            "exact post-restore control tuple was rejected") ||
        !require_true(
            SudekiMpCleanroomEnginePostRestoreControlTupleActive(
                0, 0u, 2, 0u, 0, 1u, 0, 1u),
            "nested Ailish native lease was rejected") ||
        !require_true(
            SudekiMpCleanroomEnginePostRestoreControlTupleActive(
                1, 0u, 1, 0u, 0, 1u, 0, 1u),
            "nested Tal native lease was rejected") ||
        !require_true(
            SudekiMpCleanroomEnginePostRestoreControlTupleActive(
                1, 0u, 2, 0u, 1, 0u, 1, 0u),
            "whole-party native skill-camera leases were rejected") ||
        !require_true(
            SudekiMpCleanroomEnginePostRestoreControlTupleActive(
                2, 0u, 3, 0u, 2, 0u, 2, 0u),
            "nested whole-party native leases were rejected") ||
        !require_true(
            !SudekiMpCleanroomEnginePostRestoreControlTupleActive(
                0, 0u, 0, 1u, 0, 1u, 0, 1u),
            "missing owned Ailish lease was accepted") ||
        !require_true(
            !SudekiMpCleanroomEnginePostRestoreControlTupleActive(
                0, 0u, 2, 1u, 0, 1u, 0, 1u),
            "nested Ailish lease with AI mode was accepted") ||
        !require_true(
            !SudekiMpCleanroomEnginePostRestoreControlTupleActive(
                -1, 0u, 1, 0u, 0, 1u, 0, 1u),
            "negative Tal lease count was accepted") ||
        !require_true(
            !SudekiMpCleanroomEnginePostRestoreControlTupleActive(
                0, 0u, 1, 0u, 1, 1u, 0, 1u),
            "Buki positive lease with AI mode was accepted") ||
        !require_true(
            !SudekiMpCleanroomEnginePostRestoreControlTupleActive(
                0, 0u, 1, 0u, 0, 1u, 1, 1u),
            "Elco positive lease with AI mode was accepted") ||
        !require_true(
            !SudekiMpCleanroomEnginePostRestoreControlTupleActive(
                0, 0u, 1, 0u, 0, 0u, 0, 1u),
            "Buki zero lease with disabled AI was accepted") ||
        !require_true(
            !SudekiMpCleanroomEnginePostRestoreControlTupleActive(
                0, 0u, 1, 0u, 0, 1u, -1, 0u),
            "negative Elco lease count was accepted")) {
        return 1;
    }

    if (!require_true(
            SudekiMpCleanroomEngineRetainResourceNameExact(
                &resource_copy, &resource_source),
            "exact ResourceName retain failed") ||
        !require_true(resource_copy.encoded_kind ==
                resource_source.encoded_kind &&
            resource_copy.identifier == resource_source.identifier &&
            resource_copy.text_reference == resource_source.text_reference,
            "exact ResourceName value changed") ||
        !require_true(resource_reference[0] == 3u,
            "exact ResourceName reference was not acquired")) {
        return 1;
    }
    SudekiMpCleanroomEngineReleaseResourceName(&resource_copy);
    if (!require_true(resource_reference[0] == 2u,
            "exact ResourceName reference was not balanced") ||
        !require_true(resource_copy.text_reference == NULL,
            "released ResourceName was not cleared")) {
        return 1;
    }

    read_only_reference = (uint32_t *)VirtualAlloc(
        NULL, 4096u, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!require_true(read_only_reference != NULL,
            "read-only ResourceName test allocation failed")) {
        return 1;
    }
    read_only_reference[0] = 2u;
    read_only_reference[1] = 1u;
    resource_source.text_reference = read_only_reference;
    if (!require_true(VirtualProtect(
            read_only_reference, 4096u, PAGE_READONLY,
            &old_protection),
            "read-only ResourceName test protection failed")) {
        VirtualFree(read_only_reference, 0u, MEM_RELEASE);
        return 1;
    }
    ZeroMemory(&resource_copy, sizeof(resource_copy));
    if (!require_true(
            !SudekiMpCleanroomEngineRetainResourceNameExact(
                &resource_copy, &resource_source),
            "exact ResourceName retained a read-only counter") ||
        !require_true(resource_copy.text_reference == NULL &&
                read_only_reference[0] == 2u,
            "rejected read-only ResourceName changed state")) {
        VirtualProtect(read_only_reference, 4096u, old_protection,
            &old_protection);
        VirtualFree(read_only_reference, 0u, MEM_RELEASE);
        return 1;
    }
    resource_copy = resource_source;
    SudekiMpCleanroomEngineReleaseResourceName(&resource_copy);
    if (!require_true(resource_copy.text_reference == NULL &&
                read_only_reference[0] == 2u,
            "ResourceName release wrote a read-only counter")) {
        VirtualProtect(read_only_reference, 4096u, old_protection,
            &old_protection);
        VirtualFree(read_only_reference, 0u, MEM_RELEASE);
        return 1;
    }
    if (!require_true(VirtualProtect(
            read_only_reference, 4096u, old_protection,
            &old_protection),
            "read-only ResourceName test restore failed")) {
        VirtualFree(read_only_reference, 0u, MEM_RELEASE);
        return 1;
    }
    VirtualFree(read_only_reference, 0u, MEM_RELEASE);
    resource_source.text_reference = resource_reference;

    for (actor = 0u; actor < SUDEKIMP_CLEANROOM_ACTOR_COUNT; ++actor) {
        if (!require_true(
                SudekiMpCleanroomActorLabel(
                    (SudekiMpCleanroomActor)actor) != NULL,
                "actor label is missing") ||
            !require_true(
                SudekiMpCleanroomActorResource(
                    (SudekiMpCleanroomActor)actor) != NULL,
                "actor resource is missing")) {
            return 1;
        }
    }
    if (!require_true(
            !SudekiMpCleanroomEngineCombatMode(&mode),
            "combat state should be unavailable before initialization") ||
        !require_true(
            !SudekiMpCleanroomEngineFirstPersonMode(&mode),
            "camera state should be unavailable before initialization") ||
        !require_true(
            !SudekiMpCleanroomEngineInfiniteSp(&mode),
            "infinite SP should be unavailable before initialization") ||
        !require_true(
            !SudekiMpCleanroomEngineInfiniteSpirit(&mode),
            "infinite spirit should be unavailable before initialization") ||
        !require_true(
            !SudekiMpCleanroomEngineInfiniteJetpackFuel(&mode),
            "infinite jetpack should be unavailable before initialization")) {
        return 1;
    }
    puts("cleanroom_engine_test: pass");
    return 0;
}
