#include "cleanroom/engine.h"

#include <stdio.h>

static int require_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "cleanroom_engine_test: %s\n", message);
        return 0;
    }
    return 1;
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

    if (!require_true(
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
