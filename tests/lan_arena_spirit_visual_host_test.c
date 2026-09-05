#include "hooks/lan_arena_spirit_visual_host.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Diagnostics are not an acceptance assertion and do not need disk I/O in
 * this deterministic standalone fixture. */
void SudekiMpLogFormat(const char *format, ...) { (void)format; }

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
        {"SFXSS351_Tal_Hit_Character",0x6696ab0au,0xaeec0c83u}
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
static BOOL inactive_witness(void *context,uint64_t *session,uint16_t *skill,uint32_t *tick) {
    (void)context;
    *session=1u; *skill=1u; *tick=1u;
    return fixture_active;
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
    native_weak_tests(mapped);
    CHECK(SudekiMpLanArenaSpiritVisualHostInitialize((HMODULE)mapped,inactive_witness,NULL));
    memset(&output,0,sizeof(output));
    output.tal.skill_kind=SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT;
    output.tal.skill_active=1u;
    CHECK(!SudekiMpLanArenaSpiritVisualHostCapture(1u,0u,1u,actor,actor,&output));
    CHECK(output.spirit_vfx_observed==0u && output.spirit_vfx_count==0u);
    output.tal.skill_active=0u;
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
    output.tal.skill_active=1u;
    output.tal.skill_sequence=1u;
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
    CHECK(SudekiMpLanArenaSpiritVisualHostReset());
    CHECK(!SudekiMpLanArenaSpiritVisualHostCapture(1u,0u,2u,actor,actor,&output));
    /* Physical hook stays valid and native-passthrough after logical Reset. */
    eax=(uintptr_t)setup;
    __asm__ volatile("pushl $2\n\tcall *%1" : "+a"(eax) : "r"(entry) : "ecx","edx","memory","cc");
    CHECK((unsigned char)eax==1u);
    CHECK(SudekiMpLanArenaSpiritVisualHostInitialize((HMODULE)mapped,inactive_witness,NULL));
    CHECK(SudekiMpLanArenaSpiritVisualHostReset());
    /* Fixture allocation intentionally survives every process-lifetime hook. */
}

int main(int argc,char **argv) {
    resource_tests();
    registry_tests();
    status_registry_tests();
    matrix_tests();
    if(argc>1) image_tests(argv[1]);
    if(failures) { fprintf(stderr,"%u failures\n",failures); return 1; }
    puts("LanArenaSpiritVisualHostTest: PASS");
    return 0;
}
