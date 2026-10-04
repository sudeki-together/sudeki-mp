#include "hooks/lan_arena_spirit_visual_host.h"
#include "cleanroom/engine.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Diagnostics are not an acceptance assertion and do not need disk I/O in
 * this deterministic standalone fixture. */
void SudekiMpLogFormat(const char *format, ...) { (void)format; }

/* This standalone fixture has no spawned cleanroom actors. Native shield
 * parent admission must consequently fail closed; registry tests below use
 * the explicit fake API and do not claim native actor ownership. */
void *SudekiMpCleanroomEngineActorEntity(SudekiMpCleanroomActor actor) {
    (void)actor;
    return NULL;
}

static unsigned int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); ++failures; } } while (0)

typedef struct TestContext {
    BOOL bind_fail;
    BOOL sample_fail;
    unsigned int binds;
    unsigned int samples;
    float phase;
    float position;
} TestContext;

static BOOL bind_fake(void *context, SudekiMpSpiritVisualWeakNode *node, void *entity) {
    TestContext *test = context;
    ++test->binds;
    if (test->bind_fail) return FALSE;
    node->entity = entity;
    node->previous = NULL;
    node->next = NULL;
    return TRUE;
}

static BOOL sample_fake(void *context, const SudekiMpSpiritVisualWeakNode *node,
    uint8_t kind, SudekiMpLanArenaSpiritVfxSnapshot *value) {
    TestContext *test = context;
    unsigned int i;
    ++test->samples;
    if (test->sample_fail || node->entity == NULL || kind == 0u) return FALSE;
    value->phase_valid = 1u;
    value->phase = test->phase;
    value->position[0] = test->position;
    value->rotation_xyzw[3] = 1.0f;
    for (i = 0u; i < 3u; ++i) value->scale[i] = 1.0f;
    return TRUE;
}

static void retire_fake(SudekiMpSpiritVisualHostRegistry *r, unsigned int token) {
    memset(&r->entries[token - 1u].weak, 0, sizeof(r->entries[token - 1u].weak));
}

static void resource_tests(void) {
    static const struct { const char *name; uint32_t bare, backing; } rows[] = {
        {"SFXSS250_Initiate",0x15fef04du,0x3cef3b8fu},
        {"SFXSS251_Initiate_Loop_Wait",0x2a3f3beeu,0xb5a0cf01u},
        {"SFXSS112_Small_Floor_Pattern",0x2afd906cu,0x03439ed3u},
        {"SFXSS800_Spirit_A2T",0x345218f0u,0xb5661565u},
        {"SFXSS252_Morph_into_Spirit",0xa4a4c41cu,0x903afa53u},
        {"SFXSS801_Spirit_Link",0x663229adu,0x2e5a867bu},
        {"SFXSS802_Spirit_End",0x67c9b7c0u,0x4d727a05u},
        {"SFXSS300_Tal_Spirit_Strike",0xe3c0ac91u,0x449d201bu},
        {"SFXSS350_Tal_Spirit_Strike",0x3eef6091u,0xf007401bu},
        {"SFXSS110_Loop_Invulnerable",0x928165fau,0xc24c6a03u},
        {"SFXSS111_End_Invulnerable",0x4c44c2edu,0xa8171ecfu},
        {"SFXSS900_generic_initate",0x18c4a81eu,0x62dcc5a3u},
        {"SFXSS351_Tal_Hit_Character",0x6696ab0au,0xaeec0c83u},
        {"SFXSTA003_Boost",0x017f61e4u,0x423bad0du},
        {"SFXSS450_Buki_SS_Strike",0xc28dd974u,0xc198e72du},
        {"SFXSS500_Buki_SS_Spell",0x7d191118u,0x26de2bb3u},
        {"SFXSS451_Projectile_Hit_Character",0xfcac5e13u,0xc6cf803bu},
        {"SFXSS501_Hit",0x87404674u,0x07f9bc13u},
        {"SFXB200_Shield_Appear",0xf85c91c7u,0x920d7163u},
        {"SFXB201_Shield_Loop",0x50e04a0du,0x0fdb430fu}
    };
    unsigned int i;
    char malformed[64];
    for (i = 0u; i < sizeof(rows)/sizeof(rows[0]); ++i) {
        size_t size = strlen(rows[i].name) + 1u;
        CHECK(SudekiMpSpiritVisualKindForResource(rows[i].backing) == i + 1u);
        CHECK(SudekiMpSpiritVisualKindForResource(rows[i].bare) == 0u);
        CHECK(SudekiMpSpiritVisualKindForTypedResource(
            0xfa9u, rows[i].bare, rows[i].name, size) == i + 1u);
        CHECK(SudekiMpSpiritVisualKindForTypedResource(
            0xfffu, rows[i].bare, rows[i].name, size) == 0u);
        CHECK(SudekiMpSpiritVisualKindForTypedResource(
            0xfa9u, rows[i].bare ^ 1u, rows[i].name, size) == 0u);
        CHECK(SudekiMpSpiritVisualKindForTypedResource(
            0xfa9u, rows[i].bare, rows[i].name, size - 1u) == 0u);
        CHECK(SudekiMpSpiritVisualKindForTypedResource(
            0xfa9u, rows[i].bare, NULL, 0u) == 0u);
    }
    CHECK(SudekiMpSpiritVisualKindForTypedResource(0xfa9u,0x15fef04du,
        "sfxss250_initiate",sizeof("sfxss250_initiate")) == 1u);
    CHECK(SudekiMpSpiritVisualKindForTypedResource(0xfa9u,0x15fef04du,
        "SFXSS250_Initiate_Camera",sizeof("SFXSS250_Initiate_Camera")) == 0u);
    CHECK(SudekiMpSpiritVisualKindForResource(0x62dcc5a3u) ==
        SUDEKIMP_LAN_ARENA_SPIRIT_VFX_GENERIC_INITIATE);
    CHECK(SudekiMpSpiritVisualKindForTypedResource(0xfa9u,0x18c4a81eu,
        "SFXSS900_generic_initiate",sizeof("SFXSS900_generic_initiate")) == 0u);
    CHECK(SudekiMpSpiritVisualKindForResource(0x94b4876bu) == 0u);
    CHECK(SudekiMpSpiritVisualKindForTypedResource(0xfa9u,0x94b4876bu,
        "SFXHT201_Hit_Magic.HOM",sizeof("SFXHT201_Hit_Magic.HOM")) == 0u);
    CHECK(SudekiMpSpiritVisualKindForResource(0xaeec0c83u) ==
        SUDEKIMP_LAN_ARENA_SPIRIT_VFX_TAL_STRIKE_HIT);
    CHECK(SudekiMpSpiritVisualKindForResource(0xef82dbb3u) == 0u);
    CHECK(SudekiMpSpiritVisualKindForResource(0xc104fa1bu) == 0u); /* Buki camera */
    CHECK(SudekiMpSpiritVisualKindForResource(0x1bce3ca3u) == 0u); /* Spell camera */
    CHECK(SudekiMpSpiritVisualKindForResource(0x423bad0du) == SUDEKIMP_LAN_ARENA_STATUS_VFX_BOOST);
    memset(malformed, 'A', sizeof(malformed));
    CHECK(SudekiMpSpiritVisualKindForTypedResource(
        0xfa9u,0x15fef04du,malformed,sizeof(malformed)) == 0u);
    memcpy(malformed,"SFXSS250_Initiate",sizeof("SFXSS250_Initiate"));
    malformed[40] = '\0';
    CHECK(SudekiMpSpiritVisualKindForTypedResource(
        0xfa9u,0x15fef04du,malformed,41u) == 0u);
}

static void status_registry_tests(void) {
    SudekiMpSpiritVisualHostRegistry r = {0};
    SudekiMpLanArenaSnapshot output;
    TestContext context = {0};
    SudekiMpSpiritVisualHostApi api = {&context, bind_fake, sample_fake};
    unsigned int a, b;
    a = SudekiMpSpiritVisualHostRegistryBeginOwned(&r, 44u, 0u, 100u,
        SUDEKIMP_LAN_ARENA_STATUS_VFX_BOOST, SUDEKIMP_LAN_ARENA_TAL_TYPE, (void *)100u, &api);
    b = SudekiMpSpiritVisualHostRegistryBeginOwned(&r, 44u, 0u, 100u,
        SUDEKIMP_LAN_ARENA_STATUS_VFX_BOOST, SUDEKIMP_LAN_ARENA_AILISH_TYPE, (void *)200u, &api);
    CHECK(a != 0u && b != 0u && a != b);
    SudekiMpSpiritVisualHostRegistryComplete(&r, a, TRUE, &api);
    SudekiMpSpiritVisualHostRegistryComplete(&r, b, TRUE, &api);
    CHECK(SudekiMpSpiritVisualHostRegistryCapture(&r, 44u, &output, &api));
    CHECK(output.spirit_vfx_count == 2u && output.spirit_vfx[0].skill_sequence == 0u);
    CHECK(output.spirit_vfx[0].owner_actor_type == SUDEKIMP_LAN_ARENA_TAL_TYPE);
    CHECK(output.spirit_vfx[1].owner_actor_type == SUDEKIMP_LAN_ARENA_AILISH_TYPE);
    CHECK(SudekiMpSpiritVisualHostRegistryBeginOwned(&r, 44u, 0u, 200u,
        SUDEKIMP_LAN_ARENA_STATUS_VFX_BOOST, SUDEKIMP_LAN_ARENA_TAL_TYPE, (void *)100u, &api) == a);
    CHECK(r.next_instance == 2u); /* Re-observing a long buff is not a new spawn. */
    retire_fake(&r, a);
    CHECK(SudekiMpSpiritVisualHostRegistryCapture(&r, 44u, &output, &api));
    CHECK(output.spirit_vfx_count == 1u && output.spirit_vfx[0].owner_actor_type == SUDEKIMP_LAN_ARENA_AILISH_TYPE);
    CHECK(SudekiMpSpiritVisualHostRegistryReset(&r, &api));
    CHECK(SudekiMpSpiritVisualHostRegistryBeginOwned(&r, 44u, 1u, 100u,
        SUDEKIMP_LAN_ARENA_STATUS_VFX_BOOST, SUDEKIMP_LAN_ARENA_TAL_TYPE, (void *)100u, &api) == 0u);
    CHECK(r.unknown);
}

static void shield_registry_tests(void) {
    SudekiMpSpiritVisualHostRegistry r = {0};
    SudekiMpLanArenaSnapshot output;
    TestContext context = {0};
    SudekiMpSpiritVisualHostApi api = {&context, bind_fake, sample_fake};
    unsigned int appear, loop, kind;
    appear = SudekiMpSpiritVisualHostRegistryBeginOwned(&r, 44u, 0u, 100u,
        SUDEKIMP_LAN_ARENA_BUKI_VFX_SHIELD_APPEAR, SUDEKIMP_LAN_ARENA_BUKI_TYPE,
        (void *)100u, &api);
    CHECK(appear != 0u);
    SudekiMpSpiritVisualHostRegistryComplete(&r, appear, TRUE, &api);
    CHECK(SudekiMpSpiritVisualHostRegistryCapture(&r, 44u, &output, &api));
    CHECK(output.spirit_vfx_count == 1u && output.spirit_vfx[0].skill_sequence == 0u);
    CHECK(output.spirit_vfx[0].owner_actor_type == SUDEKIMP_LAN_ARENA_BUKI_TYPE);
    CHECK(SudekiMpSpiritVisualHostRegistryBeginOwned(&r, 44u, 0u, 101u,
        SUDEKIMP_LAN_ARENA_BUKI_VFX_SHIELD_APPEAR, SUDEKIMP_LAN_ARENA_BUKI_TYPE,
        (void *)100u, &api) == appear);
    CHECK(context.binds == 1u && r.next_instance == 1u);
    loop = SudekiMpSpiritVisualHostRegistryBeginOwned(&r, 44u, 0u, 102u,
        SUDEKIMP_LAN_ARENA_BUKI_VFX_SHIELD_LOOP, SUDEKIMP_LAN_ARENA_BUKI_TYPE,
        (void *)200u, &api);
    CHECK(loop != 0u && loop != appear);
    SudekiMpSpiritVisualHostRegistryComplete(&r, loop, TRUE, &api);
    retire_fake(&r, appear);
    CHECK(SudekiMpSpiritVisualHostRegistryCapture(&r, 44u, &output, &api));
    CHECK(output.spirit_vfx_count == 1u && output.spirit_vfx[0].kind ==
        SUDEKIMP_LAN_ARENA_BUKI_VFX_SHIELD_LOOP);
    retire_fake(&r, loop);
    CHECK(SudekiMpSpiritVisualHostRegistryCapture(&r, 44u, &output, &api));
    CHECK(output.spirit_vfx_count == 0u);
    CHECK(SudekiMpSpiritVisualHostRegistryReset(&r, &api));
    for (kind = SUDEKIMP_LAN_ARENA_BUKI_VFX_SHIELD_APPEAR;
         kind <= SUDEKIMP_LAN_ARENA_BUKI_VFX_SHIELD_LOOP; ++kind) {
        CHECK(SudekiMpSpiritVisualHostRegistryBeginOwned(&r, 44u, 0u, 100u,
            kind, SUDEKIMP_LAN_ARENA_TAL_TYPE, (void *)100u, &api) == 0u);
        CHECK(r.unknown);
        CHECK(SudekiMpSpiritVisualHostRegistryReset(&r, &api));
        CHECK(SudekiMpSpiritVisualHostRegistryBeginOwned(&r, 44u, 1u, 100u,
            kind, SUDEKIMP_LAN_ARENA_BUKI_TYPE, (void *)100u, &api) == 0u);
        CHECK(r.unknown);
        CHECK(SudekiMpSpiritVisualHostRegistryReset(&r, &api));
    }
}

static void registry_tests(void) {
    SudekiMpSpiritVisualHostRegistry r = {0};
    SudekiMpLanArenaSnapshot output;
    TestContext context = {0};
    SudekiMpSpiritVisualHostApi api = {&context, bind_fake, sample_fake};
    unsigned int token, other, i;
    CHECK(SudekiMpSpiritVisualHostRegistryCapture(&r, 44u, &output, &api));
    CHECK(output.spirit_vfx_observed == 1u && output.spirit_vfx_count == 0u);
    token = SudekiMpSpiritVisualHostRegistryBegin(&r, 44u, 5u, 100u, 1u, (void *)100u, &api);
    CHECK(token == 1u && context.binds == 1u);
    CHECK(!SudekiMpSpiritVisualHostRegistryCapture(&r, 44u, &output, &api));
    CHECK(output.spirit_vfx_observed == 0u && output.spirit_vfx_count == 0u);
    SudekiMpSpiritVisualHostRegistryComplete(&r, token, TRUE, &api);
    context.phase = 17.0f;
    context.position = 10.0f;
    CHECK(SudekiMpSpiritVisualHostRegistryCapture(&r, 44u, &output, &api));
    CHECK(output.spirit_vfx_count == 1u && output.spirit_vfx[0].phase == 17.0f);
    CHECK(output.spirit_vfx[0].instance_sequence == 1u &&
        output.spirit_vfx[0].skill_sequence == 5u &&
        output.spirit_vfx[0].emitted_host_tick == 100u);
    CHECK(SudekiMpSpiritVisualHostRegistryBegin(&r, 44u, 5u, 999u, 1u,
        (void *)100u, &api) == token);
    CHECK(context.binds == 1u);
    context.phase = 22.0f;
    context.position = 25.0f;
    CHECK(SudekiMpSpiritVisualHostRegistryCapture(&r, 44u, &output, &api));
    CHECK(output.spirit_vfx[0].position[0] == 25.0f &&
        output.spirit_vfx[0].phase == 22.0f &&
        output.spirit_vfx[0].emitted_host_tick == 100u);
    context.sample_fail = TRUE;
    memset(&output, 0xaa, sizeof(output));
    CHECK(!SudekiMpSpiritVisualHostRegistryCapture(&r, 44u, &output, &api));
    CHECK(output.spirit_vfx_observed == 0u && output.spirit_vfx_count == 0u &&
        output.spirit_vfx[0].instance_sequence == 0u);
    context.sample_fail = FALSE;
    retire_fake(&r, token);
    CHECK(SudekiMpSpiritVisualHostRegistryCapture(&r, 44u, &output, &api));
    CHECK(output.spirit_vfx_count == 0u);
    /* Same address after native weak-null must get a new semantic identity. */
    token = SudekiMpSpiritVisualHostRegistryBegin(&r, 44u, 6u, 130u, 1u, (void *)100u, &api);
    SudekiMpSpiritVisualHostRegistryComplete(&r, token, TRUE, &api);
    CHECK(SudekiMpSpiritVisualHostRegistryCapture(&r, 44u, &output, &api));
    CHECK(output.spirit_vfx[0].instance_sequence == 2u);
    other = SudekiMpSpiritVisualHostRegistryBegin(&r, 44u, 7u, 140u, 2u, (void *)200u, &api);
    SudekiMpSpiritVisualHostRegistryComplete(&r, other, TRUE, &api);
    CHECK(SudekiMpSpiritVisualHostRegistryCapture(&r, 44u, &output, &api));
    CHECK(output.spirit_vfx_count == 2u && output.spirit_vfx[0].skill_sequence == 6u &&
        output.spirit_vfx[1].skill_sequence == 7u);
    context.bind_fail = TRUE;
    CHECK(!SudekiMpSpiritVisualHostRegistryReset(&r, &api));
    CHECK(r.entries[token - 1u].weak.entity == (void *)100u && r.unknown);
    context.bind_fail = FALSE;
    CHECK(SudekiMpSpiritVisualHostRegistryReset(&r, &api));
    CHECK(r.session == 0u && !r.unknown);
    /* AL=false is native failed finalize; AL=true with destroyed weak is not
     * an emitted effect either. Neither creates a phantom roster member. */
    token = SudekiMpSpiritVisualHostRegistryBegin(&r, 44u, 8u, 150u, 3u, (void *)300u, &api);
    SudekiMpSpiritVisualHostRegistryComplete(&r, token, FALSE, &api);
    CHECK(SudekiMpSpiritVisualHostRegistryCapture(&r, 44u, &output, &api));
    CHECK(output.spirit_vfx_count == 0u);
    token = SudekiMpSpiritVisualHostRegistryBegin(&r, 44u, 8u, 160u, 4u, (void *)400u, &api);
    retire_fake(&r, token);
    SudekiMpSpiritVisualHostRegistryComplete(&r, token, TRUE, &api);
    CHECK(SudekiMpSpiritVisualHostRegistryCapture(&r, 44u, &output, &api));
    CHECK(output.spirit_vfx_count == 0u);
    CHECK(SudekiMpSpiritVisualHostRegistryReset(&r, &api));
    for (i = 0u; i < 9u; ++i) {
        token = SudekiMpSpiritVisualHostRegistryBegin(&r, 77u, 1u, i, 1u,
            (void *)(uintptr_t)(1000u + i), &api);
        SudekiMpSpiritVisualHostRegistryComplete(&r, token, TRUE, &api);
    }
    CHECK(!SudekiMpSpiritVisualHostRegistryCapture(&r, 77u, &output, &api));
    CHECK(output.spirit_vfx_count == 0u && !r.unknown); /* No truncated complete set. */
    retire_fake(&r, 1u);
    CHECK(SudekiMpSpiritVisualHostRegistryCapture(&r, 77u, &output, &api));
    CHECK(output.spirit_vfx_count == 8u);
    CHECK(!SudekiMpSpiritVisualHostRegistryCapture(&r, 78u, &output, &api));
    CHECK(SudekiMpSpiritVisualHostRegistryReset(&r, &api));
    for (i = 0u; i < SUDEKIMP_SPIRIT_VISUAL_HOST_REGISTRY_CAPACITY + 1u; ++i) {
        token = SudekiMpSpiritVisualHostRegistryBegin(&r, 99u, 1u, i, 1u,
            (void *)(uintptr_t)(2000u + i), &api);
        SudekiMpSpiritVisualHostRegistryComplete(&r, token, TRUE, &api);
    }
    CHECK(r.unknown && token == 0u);
    CHECK(SudekiMpSpiritVisualHostRegistryReset(&r, &api));
    r.next_instance = UINT32_MAX;
    CHECK(SudekiMpSpiritVisualHostRegistryBegin(&r, 1u, 1u, 0u, 1u, (void *)10u, &api) == 0u);
    CHECK(r.unknown);
    CHECK(SudekiMpSpiritVisualHostRegistryReset(&r, &api));
    CHECK(SudekiMpSpiritVisualKindForResource(0x3cef3b8fu) == 1u);
    CHECK(SudekiMpSpiritVisualKindForResource(0xf007401bu) == 9u);
    CHECK(SudekiMpSpiritVisualKindForResource(0xa8171ecfu) == 11u);
    CHECK(SudekiMpSpiritVisualKindForResource(0x15fef04du) == 0u);
}

static void compose(const float q[4], const float scale[3], const float position[3], float m[16]) {
    float x=q[0], y=q[1], z=q[2], w=q[3];
    unsigned int r, c;
    memset(m, 0, sizeof(float)*16u);
    m[0]=1-2*y*y-2*z*z; m[1]=2*x*y+2*z*w; m[2]=2*x*z-2*y*w;
    m[4]=2*x*y-2*z*w; m[5]=1-2*x*x-2*z*z; m[6]=2*y*z+2*x*w;
    m[8]=2*x*z+2*y*w; m[9]=2*y*z-2*x*w; m[10]=1-2*x*x-2*y*y;
    for(r=0;r<3u;++r) { for(c=0;c<3u;++c) m[r*4u+c]*=scale[r]; m[12u+r]=position[r]; }
    m[15]=1.0f;
}

static void matrix_tests(void) {
    static const float quaternions[][4] = {
        {0,0,0,1}, {1,0,0,0}, {0,1,0,0}, {0,0,1,0},
        {0,0.70710678f,0,0.70710678f}, {0.5f,-0.5f,0.5f,0.5f}
    };
    const float scale[3]={2,3,4}, position[3]={10,-20,30};
    float matrix[16], roundtrip[16];
    SudekiMpLanArenaSpiritVfxSnapshot value = {0};
    unsigned int i, j;
    for(i=0;i<sizeof(quaternions)/sizeof(quaternions[0]);++i) {
        compose(quaternions[i],scale,position,matrix);
        CHECK(SudekiMpSpiritVisualDecomposeMatrix(matrix,&value));
        compose(value.rotation_xyzw,value.scale,value.position,roundtrip);
        for(j=0;j<16u;++j) CHECK(fabsf(matrix[j]-roundtrip[j])<0.00001f);
    }
    matrix[0]=NAN;
    CHECK(!SudekiMpSpiritVisualDecomposeMatrix(matrix,&value));
    compose(quaternions[0],scale,position,matrix);
    matrix[1]=1;
    CHECK(!SudekiMpSpiritVisualDecomposeMatrix(matrix,&value));
    compose(quaternions[0],scale,position,matrix);
    matrix[0]=-matrix[0];
    CHECK(!SudekiMpSpiritVisualDecomposeMatrix(matrix,&value));
    compose(quaternions[0],scale,position,matrix);
    matrix[0]=0;
    CHECK(!SudekiMpSpiritVisualDecomposeMatrix(matrix,&value));
}

/* Mapped exact-image fixture, never calls loader entry or imports. Only the
 * verified null-effect branch of SfxSetup finalization is executed below. */
static uint8_t *map_image(const char *path) {
    FILE *file = fopen(path,"rb");
    long size;
    uint8_t *raw, *mapped;
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS32 *nt;
    IMAGE_SECTION_HEADER *section;
    unsigned int i;
    if (!file) return NULL;
    if (fseek(file,0,SEEK_END)!=0 || (size=ftell(file))<=0 || fseek(file,0,SEEK_SET)!=0) { fclose(file); return NULL; }
    raw=malloc((size_t)size);
    if(!raw || fread(raw,1u,(size_t)size,file)!=(size_t)size) { free(raw); fclose(file); return NULL; }
    fclose(file);
    dos=(IMAGE_DOS_HEADER*)raw;
    if((size_t)size<sizeof(*dos) || dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<0 ||
        (size_t)dos->e_lfanew+sizeof(*nt)>(size_t)size) { free(raw); return NULL; }
    nt=(IMAGE_NT_HEADERS32*)(raw+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
        nt->OptionalHeader.SizeOfImage>0x10000000u || nt->OptionalHeader.SizeOfHeaders>(uint32_t)size) { free(raw); return NULL; }
    mapped=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    if(!mapped) { free(raw); return NULL; }
    memcpy(mapped,raw,nt->OptionalHeader.SizeOfHeaders);
    section=IMAGE_FIRST_SECTION(nt);
    for(i=0;i<nt->FileHeader.NumberOfSections;++i) {
        if((uint8_t*)(section+i+1u)>raw+size || section[i].PointerToRawData>(uint32_t)size ||
            section[i].SizeOfRawData>(uint32_t)size-section[i].PointerToRawData ||
            section[i].VirtualAddress>nt->OptionalHeader.SizeOfImage ||
            section[i].SizeOfRawData>nt->OptionalHeader.SizeOfImage-section[i].VirtualAddress) { free(raw); VirtualFree(mapped,0,MEM_RELEASE); return NULL; }
        memcpy(mapped+section[i].VirtualAddress,raw+section[i].PointerToRawData,section[i].SizeOfRawData);
    }
    {
        IMAGE_DATA_DIRECTORY reloc=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
        uint32_t at=0, delta=(uint32_t)(uintptr_t)mapped-nt->OptionalHeader.ImageBase;
        if(reloc.VirtualAddress>nt->OptionalHeader.SizeOfImage || reloc.Size>nt->OptionalHeader.SizeOfImage-reloc.VirtualAddress) { free(raw); VirtualFree(mapped,0,MEM_RELEASE); return NULL; }
        while(at<reloc.Size) {
            IMAGE_BASE_RELOCATION *block=(IMAGE_BASE_RELOCATION*)(mapped+reloc.VirtualAddress+at);
            uint16_t *items=(uint16_t*)(block+1);
            uint32_t count,j;
            if(block->SizeOfBlock<sizeof(*block) || block->SizeOfBlock>reloc.Size-at) { free(raw); VirtualFree(mapped,0,MEM_RELEASE); return NULL; }
            count=(block->SizeOfBlock-sizeof(*block))/2u;
            for(j=0;j<count;++j) if((items[j]>>12)==IMAGE_REL_BASED_HIGHLOW) {
                uint32_t offset=block->VirtualAddress+(items[j]&0xfffu);
                if(offset>nt->OptionalHeader.SizeOfImage-4u) { free(raw); VirtualFree(mapped,0,MEM_RELEASE); return NULL; }
                *(uint32_t*)(mapped+offset)+=delta;
            }
            at+=block->SizeOfBlock;
        }
    }
    free(raw);
    return mapped;
}

static BOOL fixture_active;
static uint8_t fixture_sources[2][0x14];
static BOOL fixture_sources_active;
static BOOL inactive_witness(void *context,void *source_component,uint64_t *session,uint16_t *skill,uint32_t *tick,uint8_t *owner) {
    (void)context;
    *session=1u; *skill=1u; *tick=1u;
    *owner=0u;
    if(source_component) {
        if(!fixture_sources_active) return FALSE;
        if(source_component==fixture_sources[0]) return TRUE;
        if(source_component==fixture_sources[1]) { *owner=SUDEKIMP_LAN_ARENA_ELCO_TYPE; return TRUE; }
        return FALSE;
    }
    return fixture_active;
}

static uint8_t *emission_image;
static uint8_t fixture_effects[8][0x3e4];
static unsigned int fixture_nested;
static uint32_t parent_args[11];
static void *parent_result;
static uint32_t parent_selector;
static void * __attribute__((cdecl,used)) fixture_parent_body(const uint32_t *args,uint32_t selector) {
    CHECK(!memcmp(args,parent_args,sizeof(parent_args)));
    CHECK(selector==parent_selector);
    return parent_result;
}
static void __attribute__((naked,used)) fixture_parent_continuation(void) {
    /* The exact constructor already pushed ECX and loaded its parent arg. */
    __asm__ volatile("leal 8(%esp),%eax\n\tpushl %edi\n\tpushl %eax\n\t"
        "call _fixture_parent_body\n\taddl $8,%esp\n\tpopl %ecx\n\tret $44");
}
static void * __attribute__((naked,cdecl,used)) call_parent_fixture(
    void *entry __attribute__((unused)),const uint32_t *args __attribute__((unused))) {
    __asm__ volatile(
        "pushl %ebp\n\tmovl %esp,%ebp\n\tpushl %esi\n\tpushl %edi\n\t"
        "movl 12(%ebp),%esi\n\tsubl $44,%esp\n\tmovl %esp,%edi\n\t"
        "movl $11,%ecx\n\trep movsl\n\tmovl _parent_selector,%edi\n\t"
        "movl $0x12345678,%ecx\n\tcall *8(%ebp)\n\t"
        "cmpl $0x12345678,%ecx\n\tjne 1f\n\tcmpl _parent_selector,%edi\n\tje 2f\n\t"
        "1: xorl %eax,%eax\n\t2: popl %edi\n\tpopl %esi\n\tpopl %ebp\n\tret");
}
static void finish_effect(void *effect) {
    uint8_t setup[0x90]={0};
    uintptr_t eax=(uintptr_t)setup;
    void *entry=emission_image+0x18830u;
    *(void **)setup=emission_image+0x2c6308u;
    *(void **)(setup+0x1cu)=effect;
    *(uint32_t *)(setup+0x2cu)=0x62dcc5a3u; /* generic initiate: either caster */
    __asm__ volatile("pushl $1\n\tcall *%1" : "+a"(eax) : "r"(entry) : "ecx","edx","memory","cc");
    CHECK((unsigned char)eax==1u);
}
static void __attribute__((cdecl,used)) fixture_emit_body(void *component,void **out,uint32_t index) {
    typedef void (__attribute__((stdcall)) *Emit)(void *,void **,uint32_t);
    void *nested=NULL;
    CHECK(index<6u);
    if(index>=6u) return;
    *out=fixture_effects[index];
    if(fixture_nested && index==0u) {
        ((Emit)(emission_image+0xe2810u))(fixture_sources[1],&nested,1u);
        /* Foreign nested source must not borrow the enclosing Buki owner. */
        ((Emit)(emission_image+0xe2810u))((void *)1,&nested,2u);
    }
    if(index!=3u && index!=4u) finish_effect(*out);
    (void)component;
}
static void __attribute__((naked,used)) fixture_emit_continuation(void) {
    __asm__ volatile("pushl 16(%ebp)\n\tpushl 12(%ebp)\n\tpushl 8(%ebp)\n\t"
        "call _fixture_emit_body\n\taddl $12,%esp\n\tmovl %ebp,%esp\n\tpopl %ebp\n\tret $12");
}
static void fixture_jump(void *location,void *target) {
    uint8_t *p=location;
    int32_t d=(uint8_t *)target-p-5;
    p[0]=0xe9; memcpy(p+1,&d,4);
    FlushInstructionCache(GetCurrentProcess(),p,5);
}
static void emission_source_tests(uint8_t *mapped) {
    typedef void (__attribute__((stdcall)) *Emit)(void *,void **,uint32_t);
    static const uint8_t finish_return[]={0x8b,0xe5,0x5d,0xb8,1,0,0,0,0xc2,4,0};
    Emit emit=(Emit)(mapped+0xe2810u);
    void *out=NULL;
    emission_image=mapped;
    /* Exact image admission and both prologues already validated. Replace
     * only fixture continuations to avoid executing an entire game scheduler. */
    memcpy(mapped+0x18836u,finish_return,sizeof(finish_return));
    fixture_jump(mapped+0xe2816u,fixture_emit_continuation);
    for(unsigned int i=0;i<8;++i) {
        uint8_t *e=fixture_effects[i];
        *(void **)e=mapped+0x2d3c7cu;
        *(void **)(e+0x44u)=e+0x160u; *(void **)(e+0x58u)=e+0x270u;
        *(void **)(e+0x160u)=mapped+0x2cdefcu; *(void **)(e+0x170u)=e;
        *(void **)(e+0x270u)=mapped+0x2c83f4u; *(void **)(e+0x280u)=e;
    }
    fixture_active=FALSE; fixture_sources_active=TRUE; fixture_nested=1;
    emit(fixture_sources[0],&out,0u);
    CHECK(out==fixture_effects[0]);
    for(unsigned int i=0;i<2;++i) {
        SudekiMpSpiritVisualHostEntry *e=*(void **)(fixture_effects[i]+4u);
        CHECK(e && e->weak.entity==fixture_effects[i]);
        if(e) CHECK(e->value.skill_sequence==1 && e->value.kind==12 &&
            e->value.owner_actor_type==(i ? SUDEKIMP_LAN_ARENA_ELCO_TYPE:0u));
    }
    CHECK(*(void **)(fixture_effects[2]+4u)==NULL);
    /* Script-origin parent effects finish outside their native caster scope.
     * Execute the installed call replacement and real constructor prefix;
     * only its downstream allocation continuation is fixture code. */
    {
        int32_t displacement;
        void *parent_entry;
        fixture_jump(mapped+0x18c95u,fixture_parent_continuation);
        CHECK(mapped[0x18f2bu]==0xe8);
        memcpy(&displacement,mapped+0x18f2cu,4u);
        parent_entry=mapped+0x18f30u+displacement;
        for(unsigned int i=0;i<11;++i) parent_args[i]=0x10203040u+i;
        parent_args[2]=0xfa9u; parent_args[3]=0x62dcc5a3u; parent_args[4]=0;
        parent_result=fixture_effects[6]; parent_selector=23u;
        fixture_active=TRUE;
        CHECK(call_parent_fixture(parent_entry,parent_args)==parent_result);
        CHECK(*(void **)(fixture_effects[6]+4u)!=NULL);
        fixture_active=FALSE;
        finish_effect(fixture_effects[6]);
        {
            SudekiMpSpiritVisualHostEntry *e=*(void **)(fixture_effects[6]+4u);
            CHECK(e && e->value.skill_sequence==1 && e->value.owner_actor_type==0u);
            CHECK(e && !e->weak.previous && !e->weak.next);
        }
        parent_result=fixture_effects[7];
        CHECK(call_parent_fixture(parent_entry,parent_args)==parent_result);
        CHECK(*(void **)(fixture_effects[7]+4u)==NULL); /* no guessed caster */
        fixture_active=TRUE;
        parent_args[3]=0xdeadbeefu; /* unrelated script effect */
        CHECK(call_parent_fixture(parent_entry,parent_args)==parent_result);
        CHECK(*(void **)(fixture_effects[7]+4u)==NULL);
        fixture_active=FALSE;
    }
    /* Attribution survives outside the synchronous hook and after casting
     * eligibility ends, through an actual native intrusive weak lease. */
    emit(fixture_sources[1],&out,3u);
    CHECK(*(void **)(fixture_effects[3]+4u)!=NULL);
    fixture_sources_active=FALSE;
    finish_effect(fixture_effects[3]);
    {
        SudekiMpSpiritVisualHostEntry *e=*(void **)(fixture_effects[3]+4u);
        CHECK(e && e->value.skill_sequence==1 && e->value.owner_actor_type==SUDEKIMP_LAN_ARENA_ELCO_TYPE);
        CHECK(e && !e->weak.previous && !e->weak.next); /* pending node drained */
    }
    /* Exact already-owned effect is a source for its own animation children. */
    emit(fixture_effects[3]+0x270u,&out,5u);
    {
        SudekiMpSpiritVisualHostEntry *e=*(void **)(fixture_effects[5]+4u);
        CHECK(e && e->value.owner_actor_type==SUDEKIMP_LAN_ARENA_ELCO_TYPE);
    }
    fixture_sources_active=TRUE;
    emit(fixture_sources[0],&out,4u); /* pending node included in logical drain */
    {
        void *saved_head=*(void **)(fixture_effects[4]+4u);
        *(void **)(fixture_effects[4]+4u)=NULL; /* failed native unlink witness */
        CHECK(!SudekiMpLanArenaSpiritVisualHostReset());
        CHECK(((SudekiMpSpiritVisualWeakNode *)saved_head)->entity==fixture_effects[4]);
        *(void **)(fixture_effects[4]+4u)=saved_head;
    }
    CHECK(SudekiMpLanArenaSpiritVisualHostReset());
    for(unsigned int i=0;i<8;++i) CHECK(*(void **)(fixture_effects[i]+4u)==NULL);
    finish_effect(fixture_effects[4]); /* late callback after reset cannot resurrect */
    CHECK(*(void **)(fixture_effects[4]+4u)==NULL);
    fixture_sources_active=FALSE;
}

static void bind_native_fixture(void *entry, SudekiMpSpiritVisualWeakNode *node, void *entity) {
    uintptr_t eax=(uintptr_t)node, edx=(uintptr_t)entity;
    __asm__ volatile("call *%2" : "+a"(eax), "+d"(edx) : "r"(entry) : "ecx","memory","cc");
}

static void native_weak_tests(uint8_t *mapped) {
    struct { void *vtable; SudekiMpSpiritVisualWeakNode *head; } entity={0};
    SudekiMpSpiritVisualWeakNode first={0}, second={0}, third={0};
    void *bind=mapped+0x1750u;
    uintptr_t ecx;
    void *destroy=mapped+0x4d30u;
    bind_native_fixture(bind,&first,&entity);
    CHECK(entity.head==&first && first.entity==&entity && first.previous==NULL);
    bind_native_fixture(bind,&second,&entity);
    CHECK(entity.head==&second && second.next==&first && first.previous==&second);
    bind_native_fixture(bind,&third,&entity);
    CHECK(entity.head==&third && third.next==&second && second.previous==&third);
    bind_native_fixture(bind,&second,NULL);
    CHECK(second.entity==NULL && second.previous==NULL && second.next==NULL);
    CHECK(third.next==&first && first.previous==&third);
    ecx=(uintptr_t)&entity;
    __asm__ volatile("call *%1" : "+c"(ecx) : "r"(destroy) : "eax","edx","memory","cc");
    CHECK(first.entity==NULL && first.previous==NULL && first.next==NULL);
    CHECK(third.entity==NULL && third.previous==NULL && third.next==NULL);
    CHECK(entity.head==NULL);
}

static BOOL lifetime_root;
static BOOL lifetime_shutdown;
static void *lifetime_factory_result;
static BOOL fixture_lifetime_shutdown(void) { return lifetime_shutdown; }
#ifdef SUDEKIMP_SPIRIT_VISUAL_HOST_TESTING
static unsigned lifetime_retire_calls;
static void fixture_lifetime_retire(void *effect) {
    ++lifetime_retire_calls;
    ((uint8_t *)effect)[0x3e0u]|=8u;
}
#endif
static BOOL fixture_lifetime_witness(void *component,SudekiMpLanPartyEffectOwner *owner) {
    if(!lifetime_root || (component && component!=fixture_sources[0])) return FALSE;
    *owner=(SudekiMpLanPartyEffectOwner){77u,19u,fixture_sources[0],SUDEKIMP_LAN_ARENA_BUKI_TYPE};
    return TRUE;
}
static void __attribute__((naked,used)) fixture_lifetime_factory_tail(void) {
    __asm__ volatile("movl _lifetime_factory_result,%eax\n\tpopl %esi\n\tpopl %ebp\n\t"
        "popl %ebx\n\taddl $20,%esp\n\tret");
}
static void __attribute__((cdecl,used)) fixture_lifetime_forward_body(void) {
    SudekiMpLanPartyEffectOwner owner={0};
    CHECK(SudekiMpLanPartyEffectLifetimeCurrent(&owner));
    CHECK(owner.session==77u && owner.generation==19u && owner.actor==fixture_sources[0]);
    CHECK(((void *(__cdecl *)(void))(emission_image+0x18760u))()==lifetime_factory_result);
}
static void __attribute__((naked,used)) fixture_lifetime_forward_tail(void) {
    __asm__ volatile("call _fixture_lifetime_forward_body\n\tret $8");
}
static void __attribute__((naked,used)) fixture_lifetime_pump_tail(void) {
    __asm__ volatile("call _fixture_lifetime_forward_body\n\tmovl %ebp,%esp\n\t"
        "popl %ebp\n\tmovl $19,%eax\n\tret $4");
}
static void effect_lifetime_tests(uint8_t *mapped) {
    typedef void (__attribute__((stdcall)) *Emit)(void *,void **,uint32_t);
    typedef void (__attribute__((thiscall)) *Forward)(void *,void *,void *);
    void *out=NULL,*destroy=mapped+0x4d30u;
    uint8_t saved=mapped[0x1877du];
    mapped[0x1877du]^=1u;
    CHECK(!SudekiMpLanPartyEffectLifetimeInitialize((HMODULE)mapped,fixture_lifetime_witness,fixture_lifetime_shutdown));
    mapped[0x1877du]=saved;
    CHECK(SudekiMpLanPartyEffectLifetimeInitialize((HMODULE)mapped,fixture_lifetime_witness,fixture_lifetime_shutdown));
    CHECK(SudekiMpLanPartyEffectLifetimePoll());
    CHECK(!SudekiMpLanPartyEffectLifetimeRetains());
    /* Native admission precedes these inert continuation substitutions. Keep
     * actual hook prologues, native intrusive bind and destructor execution. */
    fixture_jump(mapped+0x18766u,fixture_lifetime_factory_tail);
    fixture_jump(mapped+0x131d27u,fixture_lifetime_forward_tail);
    fixture_jump(mapped+0x18abf6u,fixture_lifetime_pump_tail);
    lifetime_root=TRUE; lifetime_factory_result=fixture_effects[0];
    CHECK(((void *(__cdecl *)(void))(mapped+0x18760u))()==fixture_effects[0]);
    CHECK(SudekiMpLanPartyEffectLifetimeRetains());
    lifetime_root=FALSE;
    CHECK(SudekiMpLanPartyEffectLifetimePoll());
    CHECK(!SudekiMpLanPartyEffectLifetimeReset());
    /* A child emitted after root/GEL completion inherits the exact observed
     * native parent, including asynchronous setup and actor-event forwarding. */
    fixture_nested=0;
    ((Emit)(mapped+0xe2810u))(fixture_effects[0]+0x270u,&out,3u);
    CHECK(out==fixture_effects[3]);
    CHECK(*(void **)(fixture_effects[3]+4u)!=NULL);
    lifetime_factory_result=fixture_effects[4];
    ((Forward)(mapped+0x131d20u))(fixture_effects[0]+0x148u,NULL,NULL);
    CHECK(*(void **)(fixture_effects[4]+4u)!=NULL);
    lifetime_factory_result=fixture_effects[5];
    uintptr_t result=(uintptr_t)(fixture_effects[0]+0x288u);
    void *pump=mapped+0x18abf0u;
    __asm__ volatile("pushl $0\n\tcall *%1" : "+a"(result) : "r"(pump)
        : "ecx","edx","memory","cc");
    CHECK(result==19u && *(void **)(fixture_effects[5]+4u)!=NULL);
#ifdef SUDEKIMP_SPIRIT_VISUAL_HOST_TESTING
    SudekiMpLanPartyEffectLifetimeTestRetire(fixture_lifetime_retire);
    CHECK(!SudekiMpLanPartyEffectLifetimeRequestRetire());
    CHECK(lifetime_retire_calls==0u);
    lifetime_shutdown=TRUE;
    CHECK(SudekiMpLanPartyEffectLifetimeRequestRetire());
    CHECK(lifetime_retire_calls==4u);
    CHECK(SudekiMpLanPartyEffectLifetimeRequestRetire());
    CHECK(lifetime_retire_calls==4u);
    CHECK(SudekiMpLanPartyEffectLifetimeRetains());
    CHECK(!SudekiMpLanPartyEffectLifetimeReset());
#endif
    uintptr_t ecx=(uintptr_t)fixture_effects[0];
    __asm__ volatile("call *%1" : "+c"(ecx) : "r"(destroy) : "eax","edx","memory","cc");
    CHECK(SudekiMpLanPartyEffectLifetimePoll());
    CHECK(SudekiMpLanPartyEffectLifetimeRetains());
    CHECK(!SudekiMpLanPartyEffectLifetimeReset());
    for(unsigned i=3;i<=5;++i) {
        ecx=(uintptr_t)fixture_effects[i];
        __asm__ volatile("call *%1" : "+c"(ecx) : "r"(destroy) : "eax","edx","memory","cc");
    }
    CHECK(SudekiMpLanPartyEffectLifetimePoll());
    CHECK(!SudekiMpLanPartyEffectLifetimeRetains());
    CHECK(SudekiMpLanPartyEffectLifetimeReset());
}

static void image_tests(const char *path) {
    uint8_t *mapped=map_image(path);
    uint8_t setup[0x90]={0};
    uint8_t actor[0xacu]={0}, arbiter[0x14u]={0};
    uint8_t manager[0x78u]={0}, status[0x50u]={0};
    SudekiMpLanArenaSnapshot output;
    uintptr_t eax;
    void *entry;
    uint8_t saved;
    CHECK(mapped!=NULL);
    if(!mapped) return;
    *(void **)(actor+0x90u)=arbiter;
    *(void **)(arbiter+0x10u)=actor;
    *(void **)(actor+0xa8u)=manager;
    *(void **)manager=mapped+0x2d4abcu;
    *(void **)(manager+0x10u)=actor;
    *(void **)(manager+0x54u)=status;
    *(void **)status=mapped+0x2cbf68u;
    CHECK(SudekiMpLanArenaSpiritVisualHostImageMatches((HMODULE)mapped));
    saved=mapped[0x1750u]; mapped[0x1750u]^=1u;
    CHECK(!SudekiMpLanArenaSpiritVisualHostImageMatches((HMODULE)mapped));
    mapped[0x1750u]=saved;
    saved=mapped[0x183d8u]; mapped[0x183d8u]^=1u;
    CHECK(!SudekiMpLanArenaSpiritVisualHostImageMatches((HMODULE)mapped));
    mapped[0x183d8u]=saved;
    saved=mapped[0x18f2bu]; mapped[0x18f2bu]^=1u;
    CHECK(!SudekiMpLanArenaSpiritVisualHostImageMatches((HMODULE)mapped));
    mapped[0x18f2bu]=saved;
    native_weak_tests(mapped);
    CHECK(SudekiMpLanArenaSpiritVisualHostInitialize((HMODULE)mapped,inactive_witness,NULL));
    memset(&output,0,sizeof(output));
    output.seat[0].skill_kind=SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT;
    output.seat[0].skill_active=1u;
    CHECK(!SudekiMpLanArenaSpiritVisualHostCapture(1u,0u,1u,actor,actor,&output));
    CHECK(output.spirit_vfx_observed==0u && output.spirit_vfx_count==0u);
    output.seat[0].skill_active=0u;
    CHECK(SudekiMpLanArenaSpiritVisualHostCapture(1u,0u,1u,actor,actor,&output));
    CHECK(output.spirit_vfx_observed==1u && output.spirit_vfx_count==0u);
    *(void **)(manager+0x10u)=NULL;
    CHECK(!SudekiMpLanArenaSpiritVisualHostCapture(1u,0u,1u,actor,actor,&output));
    CHECK(output.spirit_vfx_observed==0u);
    *(void **)(manager+0x10u)=actor;
    status[0x4cu]=2u;
    CHECK(!SudekiMpLanArenaSpiritVisualHostCapture(1u,0u,1u,actor,actor,&output));
    status[0x4cu]=0u;
    CHECK(SudekiMpLanArenaSpiritVisualHostCapture(1u,0u,1u,actor,actor,&output));
    status[0x4cu]=1u;
    CHECK(!SudekiMpLanArenaSpiritVisualHostCapture(1u,0u,1u,actor,actor,&output));
    *(void **)(status+0x38u)=manager+4u;
    CHECK(SudekiMpLanArenaSpiritVisualHostCapture(1u,0u,1u,actor,actor,&output));
    status[0x4cu]=0u;
    eax=(uintptr_t)setup; entry=mapped+0x18830u;
    __asm__ volatile("pushl $1\n\tcall *%1" : "+a"(eax) : "r"(entry) : "ecx","edx","memory","cc");
    CHECK((unsigned char)eax==1u);
    /* A recognized ready-callback whose engine weak target already retired
     * takes native's exact no-allocation branch, even during an active cast. */
    fixture_active=TRUE;
    *(void**)setup=mapped+0x2c6308u;
    *(uint32_t*)(setup+0x2cu)=0x3cef3b8fu;
    eax=(uintptr_t)setup;
    __asm__ volatile("pushl $1\n\tcall *%1" : "+a"(eax) : "r"(entry) : "ecx","edx","memory","cc");
    CHECK((unsigned char)eax==1u);
    output.seat[0].skill_active=1u;
    output.seat[0].skill_sequence=1u;
    CHECK(SudekiMpLanArenaSpiritVisualHostCapture(1u,1u,1u,actor,actor,&output));
    CHECK(output.spirit_vfx_count==0u && output.spirit_vfx_observed==1u);
    /* Native observer rejects unreadable retained-string indirection and an
     * unterminated readable string without dereferencing outside its bound.
     * The exact null-effect branch remains a positive no-spawn outcome. */
    {
        uint32_t reference[2] = {1u, 1u};
        char text[64];
        *(uint32_t *)(setup+0x28u)=0xfa9u;
        *(uint32_t *)(setup+0x2cu)=0x15fef04du;
        *(void **)(setup+0x30u)=reference;
        eax=(uintptr_t)setup;
        __asm__ volatile("pushl $1\n\tcall *%1" : "+a"(eax) : "r"(entry) : "ecx","edx","memory","cc");
        CHECK((unsigned char)eax==1u);
        memset(text,'A',sizeof(text));
        reference[1]=(uint32_t)(uintptr_t)text;
        eax=(uintptr_t)setup;
        __asm__ volatile("pushl $1\n\tcall *%1" : "+a"(eax) : "r"(entry) : "ecx","edx","memory","cc");
        CHECK((unsigned char)eax==1u);
        memcpy(text,"SFXSS250_Initiate",sizeof("SFXSS250_Initiate"));
        eax=(uintptr_t)setup;
        __asm__ volatile("pushl $1\n\tcall *%1" : "+a"(eax) : "r"(entry) : "ecx","edx","memory","cc");
        CHECK((unsigned char)eax==1u);
        CHECK(SudekiMpLanArenaSpiritVisualHostCapture(1u,1u,1u,actor,actor,&output));
        CHECK(output.spirit_vfx_observed==1u && output.spirit_vfx_count==0u);
        *(void **)(setup+0x30u)=NULL;
    }
    fixture_active=FALSE;
    emission_source_tests(mapped);
    CHECK(SudekiMpLanArenaSpiritVisualHostReset());
    CHECK(!SudekiMpLanArenaSpiritVisualHostCapture(1u,0u,2u,actor,actor,&output));
    /* Physical hook stays valid and native-passthrough after logical Reset. */
    eax=(uintptr_t)setup;
    __asm__ volatile("pushl $2\n\tcall *%1" : "+a"(eax) : "r"(entry) : "ecx","edx","memory","cc");
    CHECK((unsigned char)eax==1u);
    CHECK(SudekiMpLanArenaSpiritVisualHostInitialize((HMODULE)mapped,inactive_witness,NULL));
    CHECK(SudekiMpLanArenaSpiritVisualHostReset());
    effect_lifetime_tests(mapped);
    /* Fixture allocation intentionally survives every process-lifetime hook. */
}

int main(int argc,char **argv) {
    resource_tests();
    registry_tests();
    status_registry_tests();
    shield_registry_tests();
    {
        SudekiMpSpiritVisualHostRegistry r={0}; SudekiMpLanArenaSnapshot output;
        TestContext context={0}; SudekiMpSpiritVisualHostApi api={&context,bind_fake,sample_fake};
        for(unsigned kind=SUDEKIMP_LAN_PARTY_TAL_VFX_SHIELD_APPEAR;
            kind<=SUDEKIMP_LAN_PARTY_TAL_VFX_SHIELD_LOOP;++kind) {
            unsigned token=SudekiMpSpiritVisualHostRegistryBeginOwned(&r,44,0,100,
                kind,SUDEKIMP_LAN_ARENA_TAL_TYPE,(void *)100u,&api);
            CHECK(token!=0);
            SudekiMpSpiritVisualHostRegistryComplete(&r,token,TRUE,&api);
            CHECK(SudekiMpSpiritVisualHostRegistryCapture(&r,44,&output,&api));
            CHECK(output.spirit_vfx_count==1 && output.spirit_vfx[0].kind==kind);
            retire_fake(&r,token);
            CHECK(SudekiMpSpiritVisualHostRegistryCapture(&r,44,&output,&api));
            CHECK(output.spirit_vfx_count==0);
            CHECK(SudekiMpSpiritVisualHostRegistryReset(&r,&api));
            CHECK(!SudekiMpSpiritVisualHostRegistryBeginOwned(&r,44,0,100,
                kind,SUDEKIMP_LAN_ARENA_BUKI_TYPE,(void *)100u,&api));
            CHECK(r.unknown && SudekiMpSpiritVisualHostRegistryReset(&r,&api));
        }
        CHECK(SudekiMpSpiritVisualKindForResource(0x7eae7163u)==SUDEKIMP_LAN_PARTY_TAL_VFX_SHIELD_APPEAR);
        CHECK(SudekiMpSpiritVisualKindForResource(0x8e21830fu)==SUDEKIMP_LAN_PARTY_TAL_VFX_SHIELD_LOOP);
    }
    matrix_tests();
    if(argc>1) image_tests(argv[1]);
    if(failures) { fprintf(stderr,"%u failures\n",failures); return 1; }
    puts("LanArenaSpiritVisualHostTest: PASS");
    return 0;
}
