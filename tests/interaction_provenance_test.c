#include "engine/player_statehood.h"
#include "hooks/interaction_provenance.h"

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    RVA_ACTION_CANDIDATE_DISPATCH_CALL = 0x0000d75bu,
    RVA_ACTION_CANDIDATE_DISPATCH = 0x0000d7a0u,
    RVA_ENQUEUE_INTERACTION_CALL = 0x0000d951u,
    RVA_ENQUEUE_INTERACTION = 0x0000ccd0u,
    RVA_ON_ACTION_SOL_SUBMISSION_CALL = 0x0000caebu,
    RVA_SOL_SUBMISSION = 0x001c37b0u,
    EXPECTED_IMAGE_SIZE = 0x0045f000u,
    EXPECTED_TIMESTAMP = 0x534d1533u
};

static int failures;

static void check(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

static uint8_t *read_file(const char *path, DWORD *file_size) {
    HANDLE file;
    DWORD size;
    DWORD read_size;
    uint8_t *bytes;

    if (path == NULL || file_size == NULL) {
        return NULL;
    }
    file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return NULL;
    }
    size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE) {
        CloseHandle(file);
        return NULL;
    }
    bytes = (uint8_t *)HeapAlloc(GetProcessHeap(), 0u, size);
    if (bytes == NULL || !ReadFile(file, bytes, size, &read_size, NULL) ||
        read_size != size) {
        if (bytes != NULL) {
            HeapFree(GetProcessHeap(), 0u, bytes);
        }
        CloseHandle(file);
        return NULL;
    }
    CloseHandle(file);
    *file_size = size;
    return bytes;
}

static uint8_t *map_pe_image(const uint8_t *file, DWORD file_size) {
    const IMAGE_DOS_HEADER *dos;
    const IMAGE_NT_HEADERS32 *nt;
    const IMAGE_SECTION_HEADER *section;
    uint8_t *image;
    uint16_t index;

    if (file == NULL || file_size < sizeof(IMAGE_DOS_HEADER)) {
        return NULL;
    }
    dos = (const IMAGE_DOS_HEADER *)file;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        (DWORD)dos->e_lfanew > file_size - sizeof(IMAGE_NT_HEADERS32)) {
        return NULL;
    }
    nt = (const IMAGE_NT_HEADERS32 *)(file + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
        nt->OptionalHeader.SizeOfImage != EXPECTED_IMAGE_SIZE ||
        nt->FileHeader.TimeDateStamp != EXPECTED_TIMESTAMP ||
        nt->OptionalHeader.SizeOfHeaders > file_size) {
        return NULL;
    }
    image = (uint8_t *)VirtualAlloc(
        NULL,
        nt->OptionalHeader.SizeOfImage,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE);
    if (image == NULL) {
        return NULL;
    }
    ZeroMemory(image, nt->OptionalHeader.SizeOfImage);
    memcpy(image, file, nt->OptionalHeader.SizeOfHeaders);
    section = IMAGE_FIRST_SECTION(nt);
    for (index = 0u; index < nt->FileHeader.NumberOfSections; ++index) {
        DWORD raw_end = section[index].PointerToRawData +
            section[index].SizeOfRawData;
        DWORD virtual_end = section[index].VirtualAddress +
            section[index].SizeOfRawData;

        if (raw_end < section[index].PointerToRawData ||
            virtual_end < section[index].VirtualAddress ||
            raw_end > file_size ||
            virtual_end > nt->OptionalHeader.SizeOfImage) {
            VirtualFree(image, 0u, MEM_RELEASE);
            return NULL;
        }
        if (section[index].SizeOfRawData != 0u) {
            memcpy(
                image + section[index].VirtualAddress,
                file + section[index].PointerToRawData,
                section[index].SizeOfRawData);
        }
    }
    return image;
}

static uint8_t *relative_call_target(uint8_t *instruction) {
    int32_t displacement;

    if (instruction == NULL || instruction[0] != 0xe8u) {
        return NULL;
    }
    memcpy(&displacement, instruction + 1u, sizeof(displacement));
    return instruction + 5u + displacement;
}

static void test_candidate_model(void) {
    SudekiMpPlayerStatehood *statehood =
        SudekiMpPlayerStatehoodRuntime();
    SudekiMpInteractionCandidateObservation candidates[2];
    SudekiMpInteractionCandidateObservation ambiguous[2];
    SudekiMpInteractionCandidateObservation overflow[
        SUDEKIMP_INTERACTION_PROVENANCE_CANDIDATE_LIMIT + 1u];
    SudekiMpInteractionSeatObservation snapshot;
    uint32_t index;

    SudekiMpInteractionProvenanceInvalidate();
    SudekiMpPlayerStatehoodInitialize(statehood);
    check(SudekiMpPlayerStatehoodPublishPlayer(
        statehood, 0u, UINT32_C(0x11110000), 3u, 1),
        "publish P1 actor lease");
    check(SudekiMpPlayerStatehoodPublishPlayer(
        statehood, 1u, UINT32_C(0x22220000), 5u, 1),
        "publish P2 actor lease");
    SudekiMpInteractionProvenanceSetSourceGeneration(7u);

    ZeroMemory(candidates, sizeof(candidates));
    candidates[0].target_owner = UINT32_C(0xaaa00000);
    candidates[0].target = UINT32_C(0xaaa00100);
    candidates[0].event_type = 2u;
    candidates[1].target_owner = UINT32_C(0xbbb00000);
    candidates[1].target = UINT32_C(0xbbb00100);
    candidates[1].event_type = 4u;
    check(SudekiMpInteractionProvenanceObserveDispatchBegin(
        UINT32_C(0x11110000), 1, candidates, 2u),
        "begin bounded P1 dispatch observation");
    SudekiMpInteractionProvenanceObserveAcceptedCandidate(
        UINT32_C(0x11110000),
        candidates[0].target_owner,
        candidates[0].target,
        candidates[0].event_type);
    SudekiMpInteractionProvenanceObserveDispatchEnd(
        UINT32_C(0x11110000));
    check(SudekiMpInteractionProvenanceGetSeat(0u, &snapshot),
        "read current P1 observation");
    check(snapshot.completed && snapshot.candidate_count == 2u &&
        snapshot.candidates[0].status ==
            SUDEKIMP_INTERACTION_CANDIDATE_NATIVE_VALIDATED &&
        snapshot.candidates[1].status ==
            SUDEKIMP_INTERACTION_CANDIDATE_SEEN,
        "P1 records only exact accepted native candidate");
    check(SudekiMpInteractionCandidateAuthorityProven(&snapshot, 0u),
        "native-front OnAction acceptance proves P1 eligibility");
    check(!SudekiMpInteractionCandidateAuthorityProven(&snapshot, 1u),
        "unaccepted non-OnAction candidate remains fail-closed");

    check(SudekiMpInteractionProvenanceObserveDispatchBegin(
        UINT32_C(0x22220000), 0, candidates, 2u),
        "begin P2 source-specific dispatch observation");
    SudekiMpInteractionProvenanceObserveAcceptedCandidate(
        UINT32_C(0x22220000),
        candidates[0].target_owner,
        candidates[0].target,
        candidates[0].event_type);
    SudekiMpInteractionProvenanceObserveDispatchEnd(
        UINT32_C(0x22220000));
    check(SudekiMpInteractionProvenanceGetSeat(1u, &snapshot),
        "read current P2 observation");
    check(snapshot.candidates[0].status ==
            SUDEKIMP_INTERACTION_CANDIDATE_ACCEPTED_UNVALIDATED &&
        !SudekiMpInteractionCandidateAuthorityProven(&snapshot, 0u),
        "P2 path cannot authorize while native validator is skipped");

    ZeroMemory(ambiguous, sizeof(ambiguous));
    ambiguous[0] = candidates[0];
    ambiguous[1] = candidates[0];
    check(SudekiMpInteractionProvenanceObserveDispatchBegin(
        UINT32_C(0x11110000), 1, ambiguous, 2u),
        "begin duplicate-identity observation");
    SudekiMpInteractionProvenanceObserveAcceptedCandidate(
        UINT32_C(0x11110000),
        candidates[0].target_owner,
        candidates[0].target,
        candidates[0].event_type);
    SudekiMpInteractionProvenanceObserveDispatchEnd(
        UINT32_C(0x11110000));
    check(SudekiMpInteractionProvenanceGetSeat(0u, &snapshot) &&
        snapshot.identity_ambiguous &&
        !SudekiMpInteractionCandidateAuthorityProven(&snapshot, 0u),
        "duplicate candidate identities fail closed");

    ZeroMemory(overflow, sizeof(overflow));
    for (index = 0u;
         index < SUDEKIMP_INTERACTION_PROVENANCE_CANDIDATE_LIMIT + 1u;
         ++index) {
        overflow[index].target_owner = UINT32_C(0x10000000) + index * 0x100u;
        overflow[index].target = UINT32_C(0x10000040) + index * 0x100u;
        overflow[index].event_type = 2u;
    }
    check(SudekiMpInteractionProvenanceObserveDispatchBegin(
        UINT32_C(0x11110000), 1, overflow,
        SUDEKIMP_INTERACTION_PROVENANCE_CANDIDATE_LIMIT + 1u),
        "observe clamped native candidate array");
    SudekiMpInteractionProvenanceObserveAcceptedCandidate(
        UINT32_C(0x11110000),
        overflow[0].target_owner,
        overflow[0].target,
        overflow[0].event_type);
    SudekiMpInteractionProvenanceObserveDispatchEnd(
        UINT32_C(0x11110000));
    check(SudekiMpInteractionProvenanceGetSeat(0u, &snapshot) &&
        snapshot.overflowed &&
        snapshot.candidate_count ==
            SUDEKIMP_INTERACTION_PROVENANCE_CANDIDATE_LIMIT &&
        !SudekiMpInteractionCandidateAuthorityProven(&snapshot, 0u),
        "overflowed native candidate array cannot authorize");

    check(!SudekiMpInteractionProvenanceObserveDispatchBegin(
        UINT32_C(0x33330000), 1, candidates, 2u),
        "unleased actor dispatch is rejected");
    SudekiMpInteractionProvenanceSetSourceGeneration(8u);
    check(!SudekiMpInteractionProvenanceGetSeat(0u, &snapshot) &&
        !SudekiMpInteractionProvenanceGetSeat(1u, &snapshot),
        "world generation change invalidates candidate identities");
}

static void test_sol_model(void) {
    SudekiMpSolInteractionProvenance input;
    SudekiMpSolInteractionProvenance snapshot;
    uint32_t first_serial;

    SudekiMpInteractionProvenanceSetSourceGeneration(11u);
    ZeroMemory(&input, sizeof(input));
    input.source_actor = UINT32_C(0x11110000);
    input.target_owner = UINT32_C(0xabc00000);
    input.target = UINT32_C(0xabc00100);
    input.event_resource_flags = UINT32_C(0x80000000);
    input.event_resource_storage = UINT32_C(0x12345678);
    input.event_type = 2u;
    input.task_handle = UINT32_C(0x44440000);
    input.sol_thread = UINT32_C(0x55550000);
    input.observed_at_ms = 100u;
    input.source_is_native_front = 1;
    check(SudekiMpInteractionProvenanceObserveSolSubmission(&input),
        "record P1 OnAction SOL thread provenance");
    check(SudekiMpInteractionProvenanceFindSolThread(
            input.sol_thread, 101u, &snapshot) &&
        snapshot.player_index == 0u &&
        snapshot.actor_generation == 3u &&
        snapshot.source_generation == 11u &&
        SudekiMpSolInteractionAuthorityProven(&snapshot, 101u),
        "fresh same-thread P1 SOL provenance is authoritative");
    first_serial = snapshot.serial;

    input.target = UINT32_C(0xabc00200);
    input.observed_at_ms = 200u;
    check(SudekiMpInteractionProvenanceObserveSolSubmission(&input) &&
        SudekiMpInteractionProvenanceFindSolThread(
            input.sol_thread, 201u, &snapshot) &&
        snapshot.serial != first_serial &&
        snapshot.target == input.target,
        "SOL thread reuse replaces old provenance atomically");
    check(!SudekiMpInteractionProvenanceFindSolThread(
            input.sol_thread,
            200u + SUDEKIMP_INTERACTION_PROVENANCE_SOL_LIFETIME_MS + 1u,
            &snapshot),
        "expired SOL provenance is not returned");

    input.source_actor = UINT32_C(0x22220000);
    input.task_handle = UINT32_C(0x66660000);
    input.sol_thread = UINT32_C(0x77770000);
    input.observed_at_ms = 300u;
    input.source_is_native_front = 0;
    check(SudekiMpInteractionProvenanceObserveSolSubmission(&input) &&
        SudekiMpInteractionProvenanceFindSolThread(
            input.sol_thread, 301u, &snapshot) &&
        snapshot.player_index == 1u &&
        !SudekiMpSolInteractionAuthorityProven(&snapshot, 301u),
        "P2 SOL provenance remains unvalidated and fail-closed");
    SudekiMpInteractionProvenanceInvalidateSolThread(input.sol_thread);
    check(!SudekiMpInteractionProvenanceFindSolThread(
            input.sol_thread, 302u, &snapshot),
        "explicit SOL completion invalidates its thread token");

    input.source_actor = UINT32_C(0x11110000);
    input.task_handle = UINT32_C(0x88880000);
    input.sol_thread = UINT32_C(0x99990000);
    input.observed_at_ms = 400u;
    input.source_is_native_front = 1;
    check(SudekiMpInteractionProvenanceObserveSolSubmission(&input),
        "record SOL provenance before world change");
    SudekiMpInteractionProvenanceSetSourceGeneration(12u);
    check(!SudekiMpInteractionProvenanceFindSolThread(
            input.sol_thread, 401u, &snapshot),
        "world generation change invalidates SOL thread map");
}

static void test_exact_image(const char *path) {
    uint8_t *file;
    uint8_t *image;
    DWORD file_size;
    uint8_t dispatch_original[5];
    uint8_t enqueue_original[5];
    uint8_t sol_original[5];
    uint8_t mismatched;
    uint8_t sol_ecx_consumer;

    file = read_file(path, &file_size);
    check(file != NULL, "read exact SUDEKI executable");
    if (file == NULL) {
        return;
    }
    image = map_pe_image(file, file_size);
    HeapFree(GetProcessHeap(), 0u, file);
    check(image != NULL, "map exact SUDEKI PE image");
    if (image == NULL) {
        return;
    }
    check(relative_call_target(
            image + RVA_ACTION_CANDIDATE_DISPATCH_CALL) ==
            image + RVA_ACTION_CANDIDATE_DISPATCH &&
        relative_call_target(image + RVA_ENQUEUE_INTERACTION_CALL) ==
            image + RVA_ENQUEUE_INTERACTION &&
        relative_call_target(image + RVA_ON_ACTION_SOL_SUBMISSION_CALL) ==
            image + RVA_SOL_SUBMISSION,
        "exact interaction and SOL call targets match supported image");
    check(image[RVA_ON_ACTION_SOL_SUBMISSION_CALL + 15u] == 0x51u,
        "native OnAction continuation consumes post-call ECX");
    memcpy(dispatch_original,
        image + RVA_ACTION_CANDIDATE_DISPATCH_CALL,
        sizeof(dispatch_original));
    memcpy(enqueue_original,
        image + RVA_ENQUEUE_INTERACTION_CALL,
        sizeof(enqueue_original));
    memcpy(sol_original,
        image + RVA_ON_ACTION_SOL_SUBMISSION_CALL,
        sizeof(sol_original));

    check(SudekiMpInstallInteractionProvenance((HMODULE)image, FALSE) &&
        memcmp(image + RVA_ACTION_CANDIDATE_DISPATCH_CALL,
            dispatch_original, sizeof(dispatch_original)) == 0 &&
        memcmp(image + RVA_ENQUEUE_INTERACTION_CALL,
            enqueue_original, sizeof(enqueue_original)) == 0 &&
        memcmp(image + RVA_ON_ACTION_SOL_SUBMISSION_CALL,
            sol_original, sizeof(sol_original)) == 0,
        "disabled provenance observer leaves image unchanged");

    mismatched = image[RVA_ENQUEUE_INTERACTION_CALL + 5u];
    image[RVA_ENQUEUE_INTERACTION_CALL + 5u] ^= 0xffu;
    SetLastError(ERROR_SUCCESS);
    check(!SudekiMpInstallInteractionProvenance((HMODULE)image, TRUE) &&
        GetLastError() == ERROR_BAD_EXE_FORMAT &&
        memcmp(image + RVA_ACTION_CANDIDATE_DISPATCH_CALL,
            dispatch_original, sizeof(dispatch_original)) == 0,
        "local signature mismatch rejects all hooks atomically");
    image[RVA_ENQUEUE_INTERACTION_CALL + 5u] = mismatched;

    sol_ecx_consumer = image[RVA_ON_ACTION_SOL_SUBMISSION_CALL + 15u];
    image[RVA_ON_ACTION_SOL_SUBMISSION_CALL + 15u] ^= 0xffu;
    SetLastError(ERROR_SUCCESS);
    check(!SudekiMpInstallInteractionProvenance((HMODULE)image, TRUE) &&
        GetLastError() == ERROR_BAD_EXE_FORMAT &&
        memcmp(image + RVA_ON_ACTION_SOL_SUBMISSION_CALL,
            sol_original, sizeof(sol_original)) == 0,
        "missing post-call ECX consumer rejects SOL observation hook");
    image[RVA_ON_ACTION_SOL_SUBMISSION_CALL + 15u] = sol_ecx_consumer;

    SetLastError(ERROR_SUCCESS);
    check(SudekiMpInstallInteractionProvenance((HMODULE)image, TRUE),
        "exact supported image installs passive observer hooks");
    check(relative_call_target(
            image + RVA_ACTION_CANDIDATE_DISPATCH_CALL) !=
            image + RVA_ACTION_CANDIDATE_DISPATCH &&
        relative_call_target(image + RVA_ENQUEUE_INTERACTION_CALL) !=
            image + RVA_ENQUEUE_INTERACTION &&
        relative_call_target(image + RVA_ON_ACTION_SOL_SUBMISSION_CALL) !=
            image + RVA_SOL_SUBMISSION,
        "all three exact callsites redirect while installed");
    SudekiMpUninstallInteractionProvenance();
    check(memcmp(image + RVA_ACTION_CANDIDATE_DISPATCH_CALL,
            dispatch_original, sizeof(dispatch_original)) == 0 &&
        memcmp(image + RVA_ENQUEUE_INTERACTION_CALL,
            enqueue_original, sizeof(enqueue_original)) == 0 &&
        memcmp(image + RVA_ON_ACTION_SOL_SUBMISSION_CALL,
            sol_original, sizeof(sol_original)) == 0,
        "uninstall restores all exact callsites");
    VirtualFree(image, 0u, MEM_RELEASE);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fputs("usage: SudekiMP.InteractionProvenanceTest.exe SUDEKI.exe\n",
            stderr);
        return 2;
    }
    test_candidate_model();
    test_sol_model();
    test_exact_image(argv[1]);
    SudekiMpUninstallInteractionProvenance();
    if (failures != 0) {
        fprintf(stderr, "%d interaction provenance test(s) failed\n",
            failures);
        return 1;
    }
    puts("interaction provenance checks passed");
    return 0;
}
