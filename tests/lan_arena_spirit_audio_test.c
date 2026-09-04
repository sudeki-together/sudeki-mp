#include "hooks/lan_arena_spirit_audio.h"

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    RVA_SOUND_PLAY_CUE = 0x00017090u,
    RVA_SOUND_GET = 0x000170b0u,
    RVA_SOUND_GLOBAL = 0x00408d40u,
    RVA_SOUND_SUBMIT_STUB = 0x0028aeb0u,
    FIXTURE_IMAGE_SIZE = 0x00410000u
};

typedef struct WitnessState {
    BOOL active;
    int native_state;
    unsigned int calls;
} WitnessState;

static const uint8_t sound_play_cue_body[] = {
    0x8bu, 0x44u, 0x24u, 0x04u,
    0x6au, 0x00u, 0x6au, 0x00u, 0x6au, 0x01u, 0x6au, 0x00u,
    0x50u, 0x8bu, 0x41u, 0x18u,
    0xe8u, 0x0bu, 0x3eu, 0x27u, 0x00u,
    0xc2u, 0x04u, 0x00u
};

static const uint8_t sound_get_body[] = {
    0xa1u, 0x40u, 0x8du, 0x80u, 0x00u, 0xc3u
};

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s (error=%lu)\n", \
            __FILE__, __LINE__, #condition, (unsigned long)GetLastError()); \
        ++failures; \
    } \
} while (0)

static BOOL active_witness(void *context, int *native_state) {
    WitnessState *state = (WitnessState *)context;
    if (state == NULL || native_state == NULL) return FALSE;
    ++state->calls;
    *native_state = state->native_state;
    return state->active;
}

static uint8_t *allocate_fixture(void *requested_address) {
    uint8_t *image = (uint8_t *)VirtualAlloc(
        requested_address,
        FIXTURE_IMAGE_SIZE,
        MEM_RESERVE | MEM_COMMIT,
        PAGE_EXECUTE_READWRITE);
    uint32_t relocated_sound_global;
    if (image == NULL) return NULL;
    memcpy(image + RVA_SOUND_PLAY_CUE,
        sound_play_cue_body, sizeof(sound_play_cue_body));
    memcpy(image + RVA_SOUND_GET, sound_get_body, sizeof(sound_get_body));
    relocated_sound_global = (uint32_t)(uintptr_t)(
        image + RVA_SOUND_GLOBAL);
    memcpy(image + RVA_SOUND_GET + 1u, &relocated_sound_global,
        sizeof(relocated_sound_global));
    /* The verified body calls preferred VA 0x0068aeb0. A stdcall no-op keeps
     * the fixture faithful while proving the inline trampoline preserves the
     * original thiscall stack. */
    image[RVA_SOUND_SUBMIT_STUB + 0u] = 0xc2u;
    image[RVA_SOUND_SUBMIT_STUB + 1u] = 0x14u;
    image[RVA_SOUND_SUBMIT_STUB + 2u] = 0x00u;
    return image;
}

static void invoke_play_cue(uint8_t *image, const char *cue) {
    uint8_t sound[0x1cu];
    uintptr_t this_register = (uintptr_t)sound;
    void *entry = image + RVA_SOUND_PLAY_CUE;
    memset(sound, 0, sizeof(sound));
    __asm__ volatile(
        "pushl %1\n\t"
        "call *%2"
        : "+c"(this_register)
        : "r"(cue), "r"(entry)
        : "eax", "edx", "memory", "cc");
}

typedef struct ReplayState {
    void *sound;
    unsigned int get_sound_calls;
    unsigned int play_cue_calls;
    BOOL play_result;
    char cue[64];
} ReplayState;

static void *replay_get_sound(void *context) {
    ReplayState *state = (ReplayState *)context;
    if (state == NULL) return NULL;
    ++state->get_sound_calls;
    return state->sound;
}

static BOOL replay_play_cue(
    void *context,
    void *sound,
    const char *cue
) {
    ReplayState *state = (ReplayState *)context;
    if (state == NULL || sound != state->sound || cue == NULL) return FALSE;
    ++state->play_cue_calls;
    strncpy(state->cue, cue, sizeof(state->cue) - 1u);
    state->cue[sizeof(state->cue) - 1u] = '\0';
    return state->play_result;
}

static void test_semantic_replay_allowlist(void) {
    int sound;
    ReplayState state;
    SudekiMpLanArenaSpiritAudioReplayApi api;

    memset(&state, 0, sizeof(state));
    state.sound = &sound;
    state.play_result = TRUE;
    api.context = &state;
    api.get_sound = replay_get_sound;
    api.play_cue = replay_play_cue;
    CHECK(SudekiMpLanArenaSpiritAudioReplayWithApi(
        SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_START, &api));
    CHECK(state.get_sound_calls == 1u && state.play_cue_calls == 1u);
    CHECK(strcmp(state.cue, "spiritstrike_start") == 0);

    CHECK(!SudekiMpLanArenaSpiritAudioReplayWithApi(
        SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_NONE, &api));
    CHECK(GetLastError() == ERROR_INVALID_DATA);
    CHECK(!SudekiMpLanArenaSpiritAudioReplayWithApi(
        (SudekiMpLanArenaSpiritAudioCue)2, &api));
    CHECK(GetLastError() == ERROR_INVALID_DATA);
    CHECK(state.get_sound_calls == 1u && state.play_cue_calls == 1u);

    state.sound = NULL;
    CHECK(!SudekiMpLanArenaSpiritAudioReplayWithApi(
        SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_START, &api));
    CHECK(GetLastError() == ERROR_NOT_READY);
    CHECK(state.get_sound_calls == 2u && state.play_cue_calls == 1u);
    state.sound = &sound;
    state.play_result = FALSE;
    SetLastError(ERROR_SUCCESS);
    CHECK(!SudekiMpLanArenaSpiritAudioReplayWithApi(
        SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_START, &api));
    CHECK(GetLastError() == ERROR_GEN_FAILURE);
    CHECK(state.play_cue_calls == 2u);
    CHECK(!SudekiMpLanArenaSpiritAudioReplayWithApi(
        SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_START, NULL));
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
}

static void test_exact_preflight_and_bounded_capture(void) {
    uint8_t *image = allocate_fixture(NULL);
    uint8_t *foreign_image;
    WitnessState witness = {FALSE, 0, 0u};
    WitnessState rebound_witness = {TRUE, 9, 0u};
    SudekiMpLanArenaSpiritAudioEvent events[
        SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_EVENT_CAPACITY];
    uint8_t permanent_patch[6];
    char generated[48];
    char unterminated[SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_CUE_CAPACITY];
    char *protected_cue;
    DWORD old_protection;
    uint32_t dropped = 0u;
    uint32_t sound_global_operand;
    size_t count;
    unsigned int index;

    CHECK(image != NULL);
    if (image == NULL) return;
    memcpy(&sound_global_operand, image + RVA_SOUND_GET + 1u,
        sizeof(sound_global_operand));
    CHECK(sound_global_operand ==
        (uint32_t)(uintptr_t)(image + RVA_SOUND_GLOBAL));
    CHECK(SudekiMpLanArenaSpiritAudioTraceImageMatches((HMODULE)image));
    CHECK(SudekiMpLanArenaSpiritAudioReplayImageMatches((HMODULE)image));

    ++sound_global_operand;
    memcpy(image + RVA_SOUND_GET + 1u, &sound_global_operand,
        sizeof(sound_global_operand));
    CHECK(!SudekiMpLanArenaSpiritAudioReplayImageMatches((HMODULE)image));
    sound_global_operand = (uint32_t)(uintptr_t)(
        image + RVA_SOUND_GLOBAL);
    memcpy(image + RVA_SOUND_GET + 1u, &sound_global_operand,
        sizeof(sound_global_operand));
    CHECK(SudekiMpLanArenaSpiritAudioReplayImageMatches((HMODULE)image));

    image[RVA_SOUND_PLAY_CUE + sizeof(sound_play_cue_body) - 1u] ^= 1u;
    CHECK(!SudekiMpLanArenaSpiritAudioTraceImageMatches((HMODULE)image));
    CHECK(!SudekiMpLanArenaSpiritAudioReplayImageMatches((HMODULE)image));
    CHECK(!SudekiMpInstallLanArenaSpiritAudioTrace(
        (HMODULE)image, active_witness, &witness));
    CHECK(GetLastError() == ERROR_INVALID_DATA);
    CHECK(!SudekiMpLanArenaSpiritAudioTraceInstalled());
    image[RVA_SOUND_PLAY_CUE + sizeof(sound_play_cue_body) - 1u] ^= 1u;
    image[RVA_SOUND_GET + sizeof(sound_get_body) - 1u] ^= 1u;
    CHECK(SudekiMpLanArenaSpiritAudioTraceImageMatches((HMODULE)image));
    CHECK(!SudekiMpLanArenaSpiritAudioReplayImageMatches((HMODULE)image));
    image[RVA_SOUND_GET + sizeof(sound_get_body) - 1u] ^= 1u;

    CHECK(SudekiMpInstallLanArenaSpiritAudioTrace(
        (HMODULE)image, active_witness, &witness));
    CHECK(SudekiMpLanArenaSpiritAudioTraceInstalled());
    memcpy(permanent_patch, image + RVA_SOUND_PLAY_CUE,
        sizeof(permanent_patch));
    CHECK(memcmp(permanent_patch, sound_play_cue_body,
        sizeof(permanent_patch)) != 0);
    CHECK(!SudekiMpInstallLanArenaSpiritAudioTrace(
        (HMODULE)image, active_witness, &witness));
    CHECK(GetLastError() == ERROR_ALREADY_EXISTS);

    invoke_play_cue(image, "menu_select");
    count = SudekiMpLanArenaSpiritAudioTraceSnapshot(
        events, SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_EVENT_CAPACITY, &dropped);
    CHECK(count == 0u && dropped == 0u && witness.calls == 1u);

    witness.active = TRUE;
    witness.native_state = 7;
    invoke_play_cue(image, "spirit_strike_init_1");
    invoke_play_cue(image, "TAL_SS_strikevocal");
    count = SudekiMpLanArenaSpiritAudioTraceSnapshot(
        events, SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_EVENT_CAPACITY, &dropped);
    CHECK(count == 2u && dropped == 0u);
    CHECK(events[0].sequence == 1u && events[0].native_state == 7);
    CHECK(events[0].cue_length == strlen("spirit_strike_init_1"));
    CHECK(strcmp(events[0].cue, "spirit_strike_init_1") == 0);
    CHECK(events[1].sequence == 2u);
    CHECK(strcmp(events[1].cue, "TAL_SS_strikevocal") == 0);

    /* Bad pointers and strings that could forge log fields are ignored by
     * the observer but still forwarded untouched to native PlayCue. */
    invoke_play_cue(image, (const char *)(uintptr_t)1u);
    invoke_play_cue(image, "bad=log_field");
    memset(unterminated, 'A', sizeof(unterminated));
    invoke_play_cue(image, unterminated);
    protected_cue = (char *)VirtualAlloc(
        NULL, 4096u, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    CHECK(protected_cue != NULL);
    if (protected_cue != NULL) {
        strcpy(protected_cue, "protected_cue");
        CHECK(VirtualProtect(
            protected_cue, 4096u, PAGE_NOACCESS, &old_protection));
        invoke_play_cue(image, protected_cue);
        CHECK(VirtualProtect(
            protected_cue, 4096u, PAGE_EXECUTE, &old_protection));
        invoke_play_cue(image, protected_cue);
        CHECK(VirtualProtect(
            protected_cue, 4096u, PAGE_READWRITE, &old_protection));
        VirtualFree(protected_cue, 0u, MEM_RELEASE);
    }
    count = SudekiMpLanArenaSpiritAudioTraceSnapshot(
        events, SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_EVENT_CAPACITY, &dropped);
    CHECK(count == 2u && dropped == 0u);

    for (index = 0u;
         index < SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_EVENT_CAPACITY + 6u;
         ++index) {
        snprintf(generated, sizeof(generated), "trace_cue_%02u", index);
        invoke_play_cue(image, generated);
    }
    count = SudekiMpLanArenaSpiritAudioTraceSnapshot(
        events, SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_EVENT_CAPACITY, &dropped);
    CHECK(count == SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_EVENT_CAPACITY);
    CHECK(dropped == 8u);
    CHECK(events[0].sequence == 9u);
    CHECK(strcmp(events[0].cue, "trace_cue_06") == 0);
    CHECK(events[count - 1u].sequence == 72u);
    CHECK(strcmp(events[count - 1u].cue, "trace_cue_69") == 0);

    count = SudekiMpLanArenaSpiritAudioTraceSnapshot(events, 3u, &dropped);
    CHECK(count == 3u);
    CHECK(events[0].sequence == 70u);
    CHECK(events[2].sequence == 72u);
    CHECK(SudekiMpUninstallLanArenaSpiritAudioTrace());
    CHECK(!SudekiMpLanArenaSpiritAudioTraceInstalled());
    CHECK(memcmp(image + RVA_SOUND_PLAY_CUE,
        permanent_patch, sizeof(permanent_patch)) == 0);

    witness.calls = 0u;
    invoke_play_cue(image, "after_uninstall");
    CHECK(witness.calls == 0u);
    count = SudekiMpLanArenaSpiritAudioTraceSnapshot(
        events, SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_EVENT_CAPACITY, &dropped);
    CHECK(count == SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_EVENT_CAPACITY &&
          events[count - 1u].sequence == 72u);

    foreign_image = allocate_fixture(NULL);
    CHECK(foreign_image != NULL);
    if (foreign_image != NULL) {
        CHECK(SudekiMpLanArenaSpiritAudioTraceImageMatches(
            (HMODULE)foreign_image));
        CHECK(!SudekiMpInstallLanArenaSpiritAudioTrace(
            (HMODULE)foreign_image, active_witness, &rebound_witness));
        CHECK(GetLastError() == ERROR_BUSY);
        VirtualFree(foreign_image, 0u, MEM_RELEASE);
    }

    CHECK(SudekiMpInstallLanArenaSpiritAudioTrace(
        (HMODULE)image, active_witness, &rebound_witness));
    CHECK(SudekiMpLanArenaSpiritAudioTraceInstalled());
    CHECK(memcmp(image + RVA_SOUND_PLAY_CUE,
        permanent_patch, sizeof(permanent_patch)) == 0);
    count = SudekiMpLanArenaSpiritAudioTraceSnapshot(
        events, SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_EVENT_CAPACITY, &dropped);
    CHECK(count == 0u && dropped == 0u);
    invoke_play_cue(image, "TAL_SS_spell");
    count = SudekiMpLanArenaSpiritAudioTraceSnapshot(
        events, SUDEKIMP_LAN_ARENA_SPIRIT_AUDIO_EVENT_CAPACITY, &dropped);
    CHECK(count == 1u && dropped == 0u &&
          events[0].sequence == 1u && events[0].native_state == 9 &&
          strcmp(events[0].cue, "TAL_SS_spell") == 0);
    CHECK(rebound_witness.calls == 1u && witness.calls == 0u);
    CHECK(!SudekiMpInstallLanArenaSpiritAudioTrace(
        (HMODULE)image, active_witness, &rebound_witness));
    CHECK(GetLastError() == ERROR_ALREADY_EXISTS);
    CHECK(SudekiMpUninstallLanArenaSpiritAudioTrace());
    CHECK(!SudekiMpLanArenaSpiritAudioTraceInstalled());
    CHECK(memcmp(image + RVA_SOUND_PLAY_CUE,
        permanent_patch, sizeof(permanent_patch)) == 0);

    /* The image and physical detour intentionally remain alive until process
     * exit. This mirrors the launcher's whole-process lifetime contract. */
}

static uint8_t *read_file(const char *path, DWORD *size) {
    HANDLE file;
    DWORD length;
    DWORD read;
    uint8_t *bytes;
    if (path == NULL || size == NULL) return NULL;
    file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return NULL;
    length = GetFileSize(file, NULL);
    if (length == INVALID_FILE_SIZE || length == 0u) {
        CloseHandle(file);
        return NULL;
    }
    bytes = (uint8_t *)malloc(length);
    if (bytes == NULL || !ReadFile(file, bytes, length, &read, NULL) ||
        read != length) {
        free(bytes);
        CloseHandle(file);
        return NULL;
    }
    CloseHandle(file);
    *size = length;
    return bytes;
}

static uint8_t *map_pe_image(const uint8_t *file, DWORD file_size) {
    const IMAGE_DOS_HEADER *dos;
    const IMAGE_NT_HEADERS32 *nt;
    const IMAGE_SECTION_HEADER *sections;
    uint8_t *image;
    unsigned int index;
    if (file == NULL || file_size < sizeof(IMAGE_DOS_HEADER)) return NULL;
    dos = (const IMAGE_DOS_HEADER *)file;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        (DWORD)dos->e_lfanew > file_size - sizeof(IMAGE_NT_HEADERS32)) {
        return NULL;
    }
    nt = (const IMAGE_NT_HEADERS32 *)(file + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
        nt->OptionalHeader.SizeOfImage == 0u ||
        nt->OptionalHeader.SizeOfHeaders > file_size) return NULL;
    image = (uint8_t *)VirtualAlloc(NULL, nt->OptionalHeader.SizeOfImage,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (image == NULL) return NULL;
    memcpy(image, file, nt->OptionalHeader.SizeOfHeaders);
    sections = IMAGE_FIRST_SECTION(nt);
    for (index = 0u; index < nt->FileHeader.NumberOfSections; ++index) {
        DWORD source = sections[index].PointerToRawData;
        DWORD length = sections[index].SizeOfRawData;
        DWORD destination = sections[index].VirtualAddress;
        if (length == 0u) continue;
        if (source > file_size || length > file_size - source ||
            destination > nt->OptionalHeader.SizeOfImage ||
            length > nt->OptionalHeader.SizeOfImage - destination) {
            VirtualFree(image, 0, MEM_RELEASE);
            return NULL;
        }
        memcpy(image + destination, file + source, length);
    }
    return image;
}

static void test_exact_supported_image(const char *path) {
    DWORD file_size = 0u;
    uint8_t *file = read_file(path, &file_size);
    uint8_t *image;
    CHECK(file != NULL);
    if (file == NULL) return;
    image = map_pe_image(file, file_size);
    CHECK(image != NULL);
    if (image != NULL) {
        uint32_t sound_global_operand;
        memcpy(&sound_global_operand, image + RVA_SOUND_GET + 1u,
            sizeof(sound_global_operand));
        CHECK(sound_global_operand == UINT32_C(0x00808d40));
        sound_global_operand = (uint32_t)(uintptr_t)(
            image + RVA_SOUND_GLOBAL);
        memcpy(image + RVA_SOUND_GET + 1u, &sound_global_operand,
            sizeof(sound_global_operand));
        CHECK(SudekiMpLanArenaSpiritAudioTraceImageMatches((HMODULE)image));
        CHECK(SudekiMpLanArenaSpiritAudioReplayImageMatches((HMODULE)image));
        VirtualFree(image, 0, MEM_RELEASE);
    }
    free(file);
}

int main(int argc, char **argv) {
    test_semantic_replay_allowlist();
    test_exact_preflight_and_bounded_capture();
    if (argc > 1) test_exact_supported_image(argv[1]);
    if (failures != 0) {
        fprintf(stderr, "%d lan arena spirit audio test(s) failed\n", failures);
        return 1;
    }
    puts("lan arena spirit audio tests passed");
    return 0;
}
