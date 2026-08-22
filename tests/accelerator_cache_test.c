#include "hooks/accelerator_cache.h"

#include <stdio.h>
#include <stdint.h>

static int failures;
static unsigned int load_count;

static void check(BOOL condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s (error=%lu)\n", message,
            (unsigned long)GetLastError());
        ++failures;
    }
}

static HACCEL WINAPI fake_load_accelerators(
    HINSTANCE instance,
    LPCSTR table_name
) {
    (void)instance;
    (void)table_name;
    ++load_count;
    return (HACCEL)(uintptr_t)(0x1000u + load_count);
}

int main(void) {
    SudekiMpAcceleratorCacheState state = {0};
    HMODULE game_module = (HMODULE)(uintptr_t)0x00400000u;
    HACCEL first;
    HACCEL second;
    HACCEL forwarded_first;
    HACCEL forwarded_second;

    state.game_module = game_module;
    first = SudekiMpAcceleratorCacheLoad(
        &state, fake_load_accelerators, game_module,
        MAKEINTRESOURCEA(SUDEKIMP_ACCELERATOR_RESOURCE_ID));
    second = SudekiMpAcceleratorCacheLoad(
        &state, fake_load_accelerators, game_module,
        MAKEINTRESOURCEA(SUDEKIMP_ACCELERATOR_RESOURCE_ID));
    check(first != NULL && second == first,
        "resource 101 returns the cached handle");
    check(load_count == 1u,
        "resource 101 invokes the original loader exactly once");

    forwarded_first = SudekiMpAcceleratorCacheLoad(
        &state, fake_load_accelerators, game_module,
        MAKEINTRESOURCEA(SUDEKIMP_ACCELERATOR_RESOURCE_ID + 1u));
    forwarded_second = SudekiMpAcceleratorCacheLoad(
        &state, fake_load_accelerators,
        (HINSTANCE)(uintptr_t)0x00500000u,
        MAKEINTRESOURCEA(SUDEKIMP_ACCELERATOR_RESOURCE_ID));
    check(forwarded_first != first && forwarded_second != first &&
        forwarded_second != forwarded_first,
        "nonmatching module or resource requests forward unchanged");
    check(load_count == 3u,
        "nonmatching requests each invoke the original loader");

    check(SudekiMpAcceleratorCacheLoad(
            NULL, fake_load_accelerators, game_module,
            MAKEINTRESOURCEA(SUDEKIMP_ACCELERATOR_RESOURCE_ID)) == NULL,
        "null cache state is rejected");
    check(SudekiMpAcceleratorCacheLoad(
            &state, NULL, game_module,
            MAKEINTRESOURCEA(SUDEKIMP_ACCELERATOR_RESOURCE_ID)) == NULL,
        "null original loader is rejected");

    if (failures != 0) {
        fprintf(stderr, "%d accelerator cache test(s) failed\n", failures);
        return 1;
    }
    puts("accelerator_cache_test: PASS");
    return 0;
}
