#include "hooks/lan_arena_startup_movie_skip.h"

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    RVA_MOVIE_PLAY = 0x00104d90u,
    FIXTURE_IMAGE_SIZE = 0x00106000u
};

static const uint8_t movie_play_body[] = {
    0x55u, 0x8bu, 0xecu, 0x83u, 0xe4u, 0xf8u,
    0xb8u, 0x34u, 0x12u, 0x00u, 0x00u,
    0x89u, 0xecu, 0x5du, 0xc3u
};
typedef BOOL (__attribute__((cdecl)) *MoviePlayFunction)(
    const char *movie_name,
    BOOL skippable
);

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s (error=%lu)\n", \
            __FILE__, __LINE__, #condition, (unsigned long)GetLastError()); \
        ++failures; \
    } \
} while (0)

static uint8_t *allocate_fixture(void) {
    uint8_t *image = (uint8_t *)VirtualAlloc(
        NULL,
        FIXTURE_IMAGE_SIZE,
        MEM_RESERVE | MEM_COMMIT,
        PAGE_EXECUTE_READWRITE);
    if (image == NULL) return NULL;
    memcpy(image + RVA_MOVIE_PLAY, movie_play_body, sizeof(movie_play_body));
    return image;
}

static void test_exact_name_policy(void) {
    char *unreadable = (char *)VirtualAlloc(
        NULL, 4096u, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS);

    CHECK(SudekiMpLanArenaStartupMovieShouldSkip("Publisher.bik"));
    CHECK(SudekiMpLanArenaStartupMovieShouldSkip("ClimaxLogo.bik"));
    CHECK(SudekiMpLanArenaStartupMovieShouldSkip("TWIMTBP.bik"));
    CHECK(!SudekiMpLanArenaStartupMovieShouldSkip(NULL));
    CHECK(!SudekiMpLanArenaStartupMovieShouldSkip("publisher.bik"));
    CHECK(!SudekiMpLanArenaStartupMovieShouldSkip("xPublisher.bik"));
    CHECK(!SudekiMpLanArenaStartupMovieShouldSkip("Publisher.bik.extra"));
    CHECK(!SudekiMpLanArenaStartupMovieShouldSkip(
        "movies\\Publisher.bik"));
    CHECK(!SudekiMpLanArenaStartupMovieShouldSkip("FMA01_poem.bik"));
    CHECK(unreadable != NULL);
    if (unreadable != NULL) {
        CHECK(!SudekiMpLanArenaStartupMovieShouldSkip(unreadable));
        VirtualFree(unreadable, 0u, MEM_RELEASE);
    }
}

static void test_exact_preflight_install_and_passthrough(void) {
    uint8_t *image = allocate_fixture();
    MoviePlayFunction play;
    uint8_t original_entry[6];
    uint8_t installed_entry[6];

    CHECK(image != NULL);
    if (image == NULL) return;
    memcpy(original_entry, image + RVA_MOVIE_PLAY, sizeof(original_entry));

    CHECK(SudekiMpLanArenaStartupMovieSkipImageMatches((HMODULE)image));
    image[RVA_MOVIE_PLAY + 1u] ^= 1u;
    CHECK(!SudekiMpLanArenaStartupMovieSkipImageMatches((HMODULE)image));
    CHECK(!SudekiMpInstallLanArenaStartupMovieSkip((HMODULE)image));
    CHECK(GetLastError() == ERROR_INVALID_DATA);
    image[RVA_MOVIE_PLAY + 1u] ^= 1u;

    CHECK(SudekiMpInstallLanArenaStartupMovieSkip((HMODULE)image));
    CHECK(SudekiMpLanArenaStartupMovieSkipInstalled());
    CHECK(memcmp(image + RVA_MOVIE_PLAY,
        original_entry, sizeof(original_entry)) != 0);
    memcpy(installed_entry, image + RVA_MOVIE_PLAY, sizeof(installed_entry));
    play = (MoviePlayFunction)(image + RVA_MOVIE_PLAY);
    CHECK(play("Publisher.bik", FALSE) == TRUE);
    CHECK(play("ClimaxLogo.bik", TRUE) == TRUE);
    CHECK(play("TWIMTBP.bik", FALSE) == TRUE);
    CHECK(play("FMA01_poem.bik", FALSE) == (BOOL)0x1234);
    CHECK(play("Publisher.bik.extra", FALSE) == (BOOL)0x1234);

    CHECK(!SudekiMpInstallLanArenaStartupMovieSkip((HMODULE)image));
    CHECK(GetLastError() == ERROR_ALREADY_EXISTS);

    image[RVA_MOVIE_PLAY + sizeof(installed_entry) - 1u] ^= 1u;
    CHECK(!SudekiMpUninstallLanArenaStartupMovieSkip());
    CHECK(GetLastError() == ERROR_BUSY);
    CHECK(SudekiMpLanArenaStartupMovieSkipInstalled());
    CHECK(!SudekiMpInstallLanArenaStartupMovieSkip((HMODULE)image));
    CHECK(GetLastError() == ERROR_BUSY);
    memcpy(image + RVA_MOVIE_PLAY, installed_entry, sizeof(installed_entry));
    CHECK(SudekiMpUninstallLanArenaStartupMovieSkip());
    CHECK(!SudekiMpLanArenaStartupMovieSkipInstalled());
    CHECK(memcmp(image + RVA_MOVIE_PLAY,
        original_entry, sizeof(original_entry)) == 0);
    CHECK(play("Publisher.bik", FALSE) == (BOOL)0x1234);
    CHECK(SudekiMpUninstallLanArenaStartupMovieSkip());

    image[RVA_MOVIE_PLAY] ^= 1u;
    CHECK(!SudekiMpLanArenaStartupMovieSkipImageMatches((HMODULE)image));
    image[RVA_MOVIE_PLAY] ^= 1u;
    VirtualFree(image, 0u, MEM_RELEASE);
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
        nt->OptionalHeader.SizeOfHeaders > file_size) {
        return NULL;
    }
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
            VirtualFree(image, 0u, MEM_RELEASE);
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
        CHECK(SudekiMpLanArenaStartupMovieSkipImageMatches((HMODULE)image));
        VirtualFree(image, 0u, MEM_RELEASE);
    }
    free(file);
}

int main(int argc, char **argv) {
    CHECK(!SudekiMpInstallLanArenaStartupMovieSkip(NULL));
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    test_exact_name_policy();
    test_exact_preflight_install_and_passthrough();
    if (argc > 1) test_exact_supported_image(argv[1]);
    if (failures != 0) {
        fprintf(stderr, "%d lan arena startup movie skip test(s) failed\n",
            failures);
        return 1;
    }
    puts("lan arena startup movie skip tests passed");
    return 0;
}
