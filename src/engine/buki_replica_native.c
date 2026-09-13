#include "engine/buki_replica_native.h"
#include <stdint.h>
#include <string.h>
#include <math.h>

typedef void (__attribute__((stdcall)) *FacingCall)(
    void *, const float *, float, float, unsigned int);
typedef void (__attribute__((thiscall)) *InterruptCall)(void *);
#ifdef SUDEKIMP_BUKI_REPLICA_NATIVE_TESTING
static FacingCall test_facing;
static InterruptCall test_interrupt;
void SudekiMpBukiReplicaNativeTestCalls(SudekiMpBukiFacingCall facing,
    SudekiMpBukiInterruptCall interrupt) {
    test_facing = facing; test_interrupt = interrupt;
}
#endif

static BOOL readable(const void *p, size_t n) {
    MEMORY_BASIC_INFORMATION m;
    uintptr_t start = (uintptr_t)p;
    return p && n && VirtualQuery(p, &m, sizeof(m)) &&
        m.State == MEM_COMMIT && !(m.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
        start + n >= start &&
        start + n <= (uintptr_t)m.BaseAddress + m.RegionSize;
}
static BOOL call_matches(uint8_t *base, unsigned int from, unsigned int to) {
    int32_t displacement;
    if (!readable(base + from, 5) || base[from] != 0xe8) return FALSE;
    memcpy(&displacement, base + from + 1, 4);
    return base + from + 5 + displacement == base + to;
}
BOOL SudekiMpBukiReplicaNativeImageMatches(HMODULE module) {
    uint8_t *b = (uint8_t *)module;
    static const uint8_t facing[] = {0x53,0x8b,0x5c,0x24,0x08,0x55,
        0x8b,0x6c,0x24,0x10,0x56,0x57,0x8b,0xd3};
    static const uint8_t interrupt[] = {0x55,0x8b,0xec,0x83,0xe4,0xf8,
        0x83,0xec,0x08,0x53,0x55,0x56,0x8b,0xf1};
    return b && readable(b + 0xdae80u, sizeof(facing)) &&
        readable(b + 0xd0840u, sizeof(interrupt)) &&
        !memcmp(b + 0xdae80u, facing, sizeof(facing)) &&
        !memcmp(b + 0xd0840u, interrupt, sizeof(interrupt)) &&
        call_matches(b, 0xdae8eu, 0xdb800u) &&
        call_matches(b, 0xdaed5u, 0xb6e50u) &&
        call_matches(b, 0xdacdbu, 0xd0840u) &&
        call_matches(b, 0xd08f4u, 0xd0980u) &&
        call_matches(b, 0xd08fbu, 0xd0440u);
}

static BOOL actor_matches(uint8_t *base, uint8_t *actor, void *arbiter) {
    return base && readable(actor, 0xc8u) &&
        *(void **)actor == base + 0x2d5a88u &&
        *(void **)(actor + 0x90u) == arbiter;
}
static uint8_t *component(uint8_t *base, uint8_t *actor,
    unsigned int offset, unsigned int vtable, size_t size) {
    uint8_t *p = *(uint8_t **)(actor + offset);
    return readable(p, size) && *(void **)p == base + vtable &&
        *(void **)(p + 0x10u) == actor ? p : NULL;
}
BOOL SudekiMpBukiReplicaSyncFacing(HMODULE module, void *character,
    void *expected_arbiter, const float direction[3]) {
    uint8_t *b = (uint8_t *)module, *a = character;
    uint8_t *arbiter, *turn, *movement, *accepted;
    float normalized[3], length;
    FacingCall call = (FacingCall)(b + 0xdae80u);
    if (!actor_matches(b, a, expected_arbiter) || !direction ||
        !isfinite(direction[0]) || direction[1] != 0.0f ||
        !isfinite(direction[2])) return FALSE;
    length = sqrtf(direction[0]*direction[0] + direction[2]*direction[2]);
    if (!isfinite(length) || length < 0.5f || length > 1.5f) return FALSE;
    normalized[0] = direction[0]/length; normalized[1] = 0;
    normalized[2] = direction[2]/length;
    arbiter = component(b,a,0x90,0x2cc9ac,0x64);
    turn = component(b,a,0x8c,0x2d48d4,0x14);
    movement = component(b,a,0x80,0x2c8644,0xc0);
    accepted = component(b,a,0xac,0x2d4b24,0x54);
    if (!arbiter || !turn || !movement || !accepted ||
        (*(uint32_t *)(arbiter+0x50) & 0x400000u)) return FALSE;
#ifdef SUDEKIMP_BUKI_REPLICA_NATIVE_TESTING
    if (test_facing) call = test_facing;
#endif
    /* CPosition alone leaves the native turning target pointing elsewhere.
     * Use the same zero-speed direction submission as native aimed rest. */
    call(arbiter, normalized, 0.0f, 1.0f, 0u);
    return actor_matches(b,a,arbiter) &&
        component(b,a,0x90,0x2cc9ac,0x64) == arbiter &&
        component(b,a,0x8c,0x2d48d4,0x14) == turn &&
        component(b,a,0x80,0x2c8644,0xc0) == movement &&
        component(b,a,0xac,0x2d4b24,0x54) == accepted &&
        memcmp(accepted+0x48, normalized, sizeof(normalized)) == 0;
}

BOOL SudekiMpBukiReplicaInterruptAttack(HMODULE module, void *character,
    void *expected_arbiter) {
    uint8_t *b = (uint8_t *)module, *a = character;
    uint8_t *arbiter, *combo, *combat, *events, *slots;
    InterruptCall call = (InterruptCall)(b + 0xd0840u);
    if (!actor_matches(b,a,expected_arbiter)) return FALSE;
    arbiter = component(b,a,0x90,0x2cc9ac,0x64);
    combo = component(b,a,0xb8,0x2d4bd4,0xb2);
    combat = component(b,a,0xa4,0x2c8754,0x74);
    events = component(b,a,0x94,0x2d4924,0x12c);
    slots = component(b,a,0xc4,0x2d4dac,0x14);
    if (!arbiter || !combo || !combat || !events || !slots) return FALSE;
    /* This is the synchronous cleanup called by native block BEFORE its
     * state changes. Not CSkill cancellation, and not a second block input:
     * the authoritative body frame and shield roster own the new visuals. */
#ifdef SUDEKIMP_BUKI_REPLICA_NATIVE_TESTING
    if (test_interrupt) call = test_interrupt;
#endif
    call(combo);
    return actor_matches(b,a,arbiter) &&
        component(b,a,0x90,0x2cc9ac,0x64) == arbiter &&
        component(b,a,0xb8,0x2d4bd4,0xb2) == combo &&
        component(b,a,0xa4,0x2c8754,0x74) == combat &&
        component(b,a,0x94,0x2d4924,0x12c) == events &&
        component(b,a,0xc4,0x2d4dac,0x14) == slots &&
        *(uint32_t *)(combo+0x64) == 0u &&
        *(void **)(combo+0xa8) == NULL && *(void **)(combo+0xac) == NULL &&
        combo[0xb0] == 0xffu && !(combo[0xb1] & 0x1fu) &&
        !(combat[0x72] & 0x10u) &&
        !(*(uint32_t *)(arbiter+0x50) & 0x1000u);
}
