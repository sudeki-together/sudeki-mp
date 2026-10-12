/* Exact-image effect update and visual-call ABI with inert allocation,
 * renderer, transform publication and retirement endpoints; no gameplay. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "hooks/story_spawn_schedule_guard.h"
static void (*test_death_observer)(void *,void *);
BOOL SudekiMpLanStoryDeathObserverSet(void (*observer)(void *,void *)) {
    test_death_observer=observer;return TRUE;
}
#include "../src/hooks/lan_story_spawn_trace.c"

static unsigned deferred_calls;
BOOL SudekiMpLanStoryClientSpawnResidencyCall(BOOL (*witness)(void *),void *context,
    uint8_t *actor,const int *native_pause_delta,SudekiMpLanStoryClientResidencyOperation operation) {
    assert(actor && witness(context)); ++deferred_calls;
    assert(operation(context)); assert(native_pause_delta && (*native_pause_delta==0 || *native_pause_delta==-1)); return witness(context);
}
static unsigned retire_admissions;
static BOOL refuse_retirement;
BOOL SudekiMpLanStoryClientEffectRetireCall(BOOL (*witness)(void *),void *context,
    uint8_t *effect,SudekiMpLanStoryClientResidencyOperation operation) {
    assert(effect && witness(context)); ++retire_admissions;
    if(refuse_retirement) return FALSE;
    assert(operation(context)); return witness(context);
}
void SudekiMpLogFormat(const char *format,...) { (void)format; }
void SudekiMpLogWrite(const char *text) { (void)text; }
BOOL SudekiMpLogResearchEnabled(void) { return FALSE; }
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(f!=INVALID_HANDLE_VALUE); DWORD size=GetFileSize(f,NULL),got=0;
    uint8_t *raw=malloc(size); assert(raw && ReadFile(f,raw,size,&got,NULL) && got==size); CloseHandle(f);
    IMAGE_DOS_HEADER *dos=(void *)raw;
    IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *b=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    assert(b); memcpy(b,raw,nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *sections=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        IMAGE_SECTION_HEADER *s=&sections[i];
        assert(s->PointerToRawData<=size && s->SizeOfRawData<=size-s->PointerToRawData);
        assert(s->VirtualAddress<=nt->OptionalHeader.SizeOfImage &&
            s->SizeOfRawData<=nt->OptionalHeader.SizeOfImage-s->VirtualAddress);
        memcpy(b+s->VirtualAddress,raw+s->PointerToRawData,s->SizeOfRawData);
    }
    uintptr_t delta=(uintptr_t)b-nt->OptionalHeader.ImageBase;
    IMAGE_DATA_DIRECTORY reloc=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    unsigned offset=0;
    while(offset<reloc.Size) {
        IMAGE_BASE_RELOCATION *block=(void *)(b+reloc.VirtualAddress+offset);
        assert(block->SizeOfBlock>=sizeof(*block) && block->SizeOfBlock<=reloc.Size-offset);
        uint16_t *items=(uint16_t *)(block+1);
        unsigned count=(block->SizeOfBlock-sizeof(*block))/2;
        for(unsigned i=0;i<count;++i) {
            unsigned type=items[i]>>12,rva=block->VirtualAddress+(items[i]&0xfffu);
            assert(type==IMAGE_REL_BASED_ABSOLUTE || type==IMAGE_REL_BASED_HIGHLOW);
            if(type==IMAGE_REL_BASED_HIGHLOW) {
                assert(rva<=nt->OptionalHeader.SizeOfImage-4);
                *(uint32_t *)(b+rva)+=(uint32_t)delta;
            }
        }
        offset+=block->SizeOfBlock;
    }
    free(raw); return b;
}

static uint8_t entity[0x400],registry[0x40],game_update[0x20];
static uint8_t *registry_entries[1]={entity};
static float scheduler_decoy[5]={7.0f,8.0f,9.0f,0.25f,11.0f};
static float received[4];
static unsigned calls;
static void *received_component;
static void __attribute__((thiscall)) animation_update(void *component,const float *timing) {
    received_component=component;
    memcpy(received,timing,sizeof(received));
    ++calls;
}
static void fixture(void) {
    memset(entity,0,sizeof(entity));
    calls=0;
    *(void **)entity=image_base+RVA_FX_VTABLE;
    *(void **)(entity+8)=image_base+RVA_FX_NODE_VTABLE;
    entity[0x2b]=1; /* Held by the client world pause. */
    entity[0x3e0]=3; /* Active; skip renderer completion/removal branch. */
    *(unsigned *)(entity+8+0x330)=1; /* Transform initialized; no native reset. */
    *(void **)(entity+0x58)=entity+0x270;
    *(void **)(entity+0x270)=image_base+0x2c83f4;
    *(void **)(entity+0x280)=entity;
    *(void **)(image_base+0x2c83f4+0x40)=(void *)(uintptr_t)animation_update;
    *(void **)(registry+0x3c)=registry_entries;
    *(unsigned *)(registry+0x34)=1;
    *(void **)(image_base+RVA_REGISTRY)=registry;
    *(void **)(image_base+0x409e14)=game_update;
    *(void **)(game_update+0x18)=scheduler_decoy;
    assert(fx_exact(entity));
}
static void tick(float input,float expected) {
    unsigned before=calls;
    assert(fx_tick(input));
    assert(calls==before+1);
    assert(received_component==entity+8+0x268);
    assert(received[0]==0.0f && received[1]==0.0f && received[2]==0.0f);
    assert(received[3]==expected);
}
static void no_tick(float input) {
    unsigned before=calls;
    assert(!fx_tick(input));
    assert(calls==before);
}
static void sigil_hide_only(void) {
    uint8_t holder[12]={0},object[0x38]={0};
    fixture();
    *(void **)(entity+0x58)=entity+0x270;
    *(void **)(entity+0x270)=image_base+0x2c83f4;
    *(void **)(entity+0x280)=entity;
    *(uint32_t *)(entity+0x29c)=0x0fa9;
    *(uint32_t *)(entity+0x2a0)=0xb51df833; /* A normal hit is not a sigil. */
    *(void **)(entity+0x214)=holder; *(void **)(holder+8)=object;
    *(uint32_t *)(object+0x34)=0x81;
    assert(!fx_hide(entity) && *(uint32_t *)(object+0x34)==0x81);
    fx_tick(0);
    assert(*(uint32_t *)(object+0x34)==0x81);
    *(uint32_t *)(entity+0x2a0)=0x7e8a6f3f;
    assert(fx_sigil(entity));
    *(void **)(entity+0x280)=holder;
    assert(!fx_hide(entity) && *(uint32_t *)(object+0x34)==0x81);
    *(void **)(entity+0x280)=entity;
    *(unsigned *)(registry+0x34)=0;
    assert(!fx_hide(entity) && *(uint32_t *)(object+0x34)==0x81);
    *(unsigned *)(registry+0x34)=1;
    *(uint32_t *)(entity+0x29c)=0x21;
    assert(!fx_hide(entity) && *(uint32_t *)(object+0x34)==0x81);
    *(uint32_t *)(entity+0x29c)=0x29;
    fx_tick(0);
    assert(*(uint32_t *)(object+0x34)==0x81); /* No host end, no hide. */
    assert(fx_hide(entity) && *(uint32_t *)(object+0x34)==0x84);
}
static unsigned renderer_calls;
static float renderer_delta;
static uint32_t __attribute__((thiscall)) render_update(void *self,const void *matrix,float dt) {
    (void)self; assert(matrix); ++renderer_calls; renderer_delta=dt; return 1;
}
static void native_frame_stamp(void) {
    uint8_t position[0xb8]={0},wrapper[0x14]={0},object[0xd0]={0};
    void *table[3]={NULL,NULL,(void *)(uintptr_t)render_update};
    void *renderer=table;
    fixture();
    *(void **)(entity+0x280)=entity;
    *(void **)(entity+0x44)=position;
    *(void **)(position+0xb4)=wrapper;
    *(void **)(wrapper+8)=object;
    *(void **)(wrapper+0x10)=&renderer;
    *(void **)(object+0x14)=&renderer;
    assert(!memcmp(image_base+RVA_OBJECT_UPDATE,object_update_head,sizeof(object_update_head)));
    *(uint16_t *)(image_base+0x3c3150)=100;
    fx_animate(entity,0.0f); /* First native render walk stamps with zero dt. */
    assert(renderer_calls==1 && renderer_delta==0.0f);
    fx_animate(entity,0.02f);
    assert(renderer_calls==1); /* A later manual call cannot recover it. */
    *(uint16_t *)(image_base+0x3c3150)=101;
    fx_animate(entity,0.02f);
    uint32_t delta_bits; memcpy(&delta_bits,&renderer_delta,4);
    assert(renderer_calls==2 && delta_bits==0x3ca3d70au); /* stored float 0.02 */
    fx_animate(entity,0.02f); assert(renderer_calls==2);
    *(uint32_t *)(object+0x34)=4; /* Native hidden-object guard stays intact. */
    *(uint16_t *)(image_base+0x3c3150)=102;
    fx_animate(entity,0.02f); assert(renderer_calls==2);
}
static void effects_outlive_tracking_windows(void) {
    fixture();
    *(uint32_t *)(entity+0x29c)=0x29;
    *(uint32_t *)(entity+0x2a0)=0xb51df833;
    tick(0.02f,0.02f); /* Old 15-second tracking cutoff must not stop a hit. */
    ticked_count=0; client_role=TRUE;
    uint32_t now=GetTickCount();
    (void)SudekiMpLanStorySpawnTick(now,TRUE);
    unsigned before=calls;
    assert(SudekiMpLanStorySpawnTick(now+20u,TRUE) && calls==before+1u);
    *(void **)(entity+0x280u)=registry;
    assert(!fx_tick(0.02f)); /* Wrong animation owner must not be updated. */
}
static void effect_burst_exceeds_old_slots(void) {
    static uint8_t batch[48][0x400]; static uint8_t *entries[48];
    fixture();
    for(unsigned i=0;i<48u;++i) {
        memcpy(batch[i],entity,sizeof(entity)); entries[i]=batch[i];
        *(void **)(batch[i]+0x58u)=batch[i]+0x270u;
        *(void **)(batch[i]+0x280u)=batch[i];
        *(uint32_t *)(batch[i]+0x34u)=i+1u;
        *(uint32_t *)(batch[i]+0x29cu)=0x29u;
        *(uint32_t *)(batch[i]+0x2a0u)=0xb51df833u;
    }
    *(void **)(registry+0x3cu)=entries; *(unsigned *)(registry+0x34u)=48u;
    assert(fx_tick(0.02f) && calls==48u);
    assert(fx_tick(0.02f) && calls==96u);
    fixture();
}
/* A6 regression: an enrolled group is not evidence of a native call. */
static void idle_groups_do_not_admit_residency(void) {
    fixture();
    *(void **)(image_base+RVA_CLUSTER_GLOBAL)=NULL;
    *(void **)entity=image_base+RVA_GROUP_VTABLE;
    *(void **)(entity+8u)=image_base+RVA_GROUP_NODE_VTABLE;
    const char name[]="fixture_group";
    uint32_t record[2]={1u,(uint32_t)(uintptr_t)name};
    *(void **)(entity+0x38u)=record;
    assert(group_exact(entity));
    memset(ticked,0,sizeof(ticked)); ticked_count=1; client_role=TRUE;
    ticked[0].entity=entity; strcpy(ticked[0].name,name); ticked[0].watch_only=TRUE;
    uint32_t now=GetTickCount(); ticked[0].started=now;
    for(unsigned i=0;i<100u;++i) {
        assert(!SudekiMpLanStorySpawnTick(++now,TRUE));
        assert(ticked_count==1 && !ticked[0].ticks);
    }
    ticked[0].watch_only=FALSE; entity[0x2bu]=0; /* Native scheduler owns it. */
    assert(!SudekiMpLanStorySpawnTick(++now,TRUE) && ticked_count==1);
    entity[0x2bu]=1;
    assert(!SudekiMpLanStorySpawnTick(now,TRUE) && ticked_count==1); /* zero delta */
    *(void **)(entity+8u)=image_base;
    assert(!SudekiMpLanStorySpawnTick(++now,TRUE) && !ticked_count); /* refused identity */
}
static SudekiMpStoryDeathVisual death_request;
static unsigned death_calls;
static uint32_t death_record[2]={2,0};
static uint8_t death_monster[0x100],death_anim[0xe0],death_info[0x100],death_entry[0x5c];
static uint8_t death_position[0xb8],death_wrapper[0x14],death_object[0xd0],death_renderer[0x14],death_manager[0x14];
static void * __stdcall effect_create_endpoint(void *manager,const float *matrix,
    uint32_t kind,uint32_t hash,uint32_t *record,uint32_t empty_kind,uint32_t empty_hash,
    void *empty_record,uint32_t loop,float speed,uint32_t immediate) {
    assert(manager==death_manager && !memcmp(matrix,death_request.matrix,64u));
    assert((kind&0x1fffu)==0xfa9u && hash==0x12345678u && record==death_record);
    assert((empty_kind&0x1fffu)==0xfc0u && empty_hash==0x7ffffu && !empty_record);
    assert(!loop && speed==1.0f && immediate==1u);
    assert(*record==4u); --*record; ++death_calls;
    return entity;
}
static void death_visual_fixture(void) {
    *(void **)death_monster=image_base+0x2d5fa0u;
    *(void **)(death_monster+0x58u)=death_anim; *(void **)death_anim=image_base+0x2c8504u;
    *(void **)(death_anim+0x10u)=death_monster; *(void **)(death_anim+0xdcu)=death_info;
    *(void **)death_info=image_base+0x2d5db4u;
    *(void **)(death_info+0x14u+4u*0x18u)=death_entry;
    *(void **)death_entry=image_base+0x2d5df0u; death_entry[0x56u]=0x18u;
    *(uint32_t *)(death_entry+0x28u)=0xfa9u; *(uint32_t *)(death_entry+0x2cu)=0x12345678u;
    *(void **)(death_entry+0x30u)=death_record;
    *(void **)(death_monster+0x44u)=death_position; *(void **)death_position=image_base+0x2cdefcu;
    *(void **)(death_position+0x10u)=death_monster; *(void **)(death_position+0xb4u)=death_wrapper;
    *(void **)(death_wrapper+8u)=death_object; *(void **)(death_wrapper+0x10u)=death_renderer;
    *(void **)(death_object+0x14u)=death_renderer; *(void **)death_renderer=image_base+0x2df8ecu;
    for(unsigned i=0;i<16u;++i) ((float *)(death_object+0x90u))[i]=(i%5u==0u)?1.0f:0.0f;
    ((float *)(death_object+0x90u))[12]=12.0f;
    *(void **)death_manager=image_base+0x2c62a0u; *(void **)(image_base+0x408d48u)=death_manager;
    death_record[0]=2;death_calls=0;
}
static void death_visual_contract(void) {
    death_visual_fixture();
    assert(SudekiMpStoryDeathVisualPrepare(image_base,death_monster,2u,&death_request,visual_memory));
    assert(!SudekiMpStoryDeathVisualPrepare(image_base,death_monster,0u,&death_request,visual_memory));
    assert(!SudekiMpStoryDeathVisualPrepare(image_base,death_monster,5u,&death_request,visual_memory));
    *(void **)(death_anim+0x10u)=entity;
    assert(!SudekiMpStoryDeathVisualPrepare(image_base,death_monster,2u,&death_request,visual_memory));
    *(void **)(death_anim+0x10u)=death_monster;
    death_entry[0x59u]=1;
    assert(!SudekiMpStoryDeathVisualPrepare(image_base,death_monster,2u,&death_request,visual_memory));
    death_entry[0x59u]=0; death_entry[0x56u]=0x19u;
    assert(!SudekiMpStoryDeathVisualPrepare(image_base,death_monster,2u,&death_request,visual_memory));
    death_entry[0x56u]=0x18u; *(uint32_t *)(death_entry+0x2cu)=0x7ffffu;
    assert(!SudekiMpStoryDeathVisualPrepare(image_base,death_monster,2u,&death_request,visual_memory));
    *(uint32_t *)(death_entry+0x2cu)=0x12345678u;
    for(unsigned i=0;i<sizeof(story_death_effect_head);++i) {
        image_base[0x17da0u+i]^=1; assert(!SudekiMpStoryDeathVisualCode(image_base,visual_memory));
        image_base[0x17da0u+i]^=1;
    }
    image_base[0x17e2cu]^=1; assert(!SudekiMpStoryDeathVisualCode(image_base,visual_memory)); image_base[0x17e2cu]^=1;
    for(unsigned i=0;i<3u;++i) {
        image_base[0x17e49u+i]^=1; assert(!SudekiMpStoryDeathVisualCode(image_base,visual_memory)); image_base[0x17e49u+i]^=1;
    }
    /* Execute retail 417DA0 with only its allocating endpoint replaced. This
     * checks AL/CL, six-word call, nested RET44, outer RET24 and name leases. */
    assert(!memcmp(image_base+0x18a80u,"\x51\x53\x8a\x5c\x24\x34",6u));
    uint8_t saved[5]; memcpy(saved,image_base+0x18a80u,5u);
    image_base[0x18a80u]=0xe9;
    *(int32_t *)(image_base+0x18a81u)=(intptr_t)effect_create_endpoint-(intptr_t)(image_base+0x18a85u);
    assert(FlushInstructionCache(GetCurrentProcess(),image_base,0x45f000u));
    ++*death_record;
    assert(SudekiMpStoryDeathVisualCall(&death_request,image_base+0x17da0u)==entity);
    assert(death_calls==1u && *death_record==2u);
    memcpy(image_base+0x18a80u,saved,5u);
}
static unsigned retired;
static uint32_t __attribute__((thiscall)) completed(void *self,unsigned a,unsigned b) {
    (void)self; assert(!a && !b); return 0x41u;
}
static int __attribute__((thiscall)) one_animation(void *self) { (void)self; return 1; }
static void __stdcall retire_effect(void *self) {
    assert(self==entity); ++retired;
    /* Simulate native registry replacement/removal. No stale entry pointer
     * or post-update object access may be required by fx_tick. */
    *(unsigned *)(registry+0x34u)=0; *(void **)(registry+0x3cu)=NULL;
}
static void effect_completion(void) {
    fixture(); entity[0x3e0u]=5; /* active + native auto-retire, not looping */
    uint8_t position[0xb8]={0},wrapper[0x14]={0};
    void *table[71]={0}; void *renderer=table;
    table[70]=(void *)(uintptr_t)completed;
    *(void **)(entity+0x44u)=position; *(void **)(position+0xb4u)=wrapper;
    *(void **)(wrapper+0x10u)=&renderer;
    void **slot=(void **)(image_base+0x2c83f4u+0x3cu); void *old=*slot;
    *slot=(void *)(uintptr_t)one_animation;
    /* Transform publication consults a live world manager. Keep it inert in
     * this fixture; the completion test executes retail 5319B0's lifecycle. */
    uint8_t transform_head=image_base[0xfc560u];
    assert(transform_head==0x55u); image_base[0xfc560u]=0xc3;
    uint8_t saved[5]; memcpy(saved,image_base+0x131df0u,5u);
    image_base[0x131df0u]=0xe9;
    *(int32_t *)(image_base+0x131df1u)=(intptr_t)retire_effect-(intptr_t)(image_base+0x131df5u);
    assert(FlushInstructionCache(GetCurrentProcess(),image_base,0x45f000u));
    assert(fx_tick(0.02f) && retired==1u && !(entity[0x3e0u]&1u));
    assert(!fx_tick(0.02f) && retired==1u);
    memcpy(image_base+0x131df0u,saved,5u); *slot=old; image_base[0xfc560u]=transform_head;
}

static void spawn_schedule_ownership(void) {
    fixture(); *(uint32_t *)(entity+0x34u)=123u;
    entity[0x2bu]=2u; entity[0x144u]=0x84u;
    assert(SudekiMpStorySpawnScheduleFinished(entity,entity,image_base+RVA_FX_VTABLE,
        image_base+RVA_FX_NODE_VTABLE,123u,2,0,0,readable));
    assert(!SudekiMpStorySpawnScheduleFinished(entity,registry,image_base+RVA_FX_VTABLE,
        image_base+RVA_FX_NODE_VTABLE,123u,2,0,0,readable));
    assert(!SudekiMpStorySpawnScheduleFinished(entity,entity,image_base,
        image_base+RVA_FX_NODE_VTABLE,123u,2,0,0,readable));
    assert(!SudekiMpStorySpawnScheduleFinished(entity,entity,image_base+RVA_FX_VTABLE,
        image_base,123u,2,0,0,readable));
    assert(!SudekiMpStorySpawnScheduleFinished(entity,entity,image_base+RVA_FX_VTABLE,
        image_base+RVA_FX_NODE_VTABLE,124u,2,0,0,readable));
    assert(!SudekiMpStorySpawnScheduleFinished(entity,entity,image_base+RVA_FX_VTABLE,
        image_base+RVA_FX_NODE_VTABLE,123u,1,0,0,readable));
    assert(!SudekiMpStorySpawnScheduleFinished(entity,entity,image_base+RVA_FX_VTABLE,
        image_base+RVA_FX_NODE_VTABLE,123u,2,4,0,readable));
    entity[0x2bu]=1u;
    assert(SudekiMpStorySpawnScheduleFinished(entity,entity,image_base+RVA_FX_VTABLE,
        image_base+RVA_FX_NODE_VTABLE,123u,2,0,-1,readable));
    assert(!SudekiMpStorySpawnScheduleFinished(entity,entity,image_base+RVA_FX_VTABLE,
        image_base+RVA_FX_NODE_VTABLE,123u,2,0,0,readable));
    assert(!SudekiMpStorySpawnScheduleFinished(entity,entity,image_base+RVA_FX_VTABLE,
        image_base+RVA_FX_NODE_VTABLE,123u,3,0,-2,readable));
    entity[0x2bu]=0u;
    assert(!SudekiMpStorySpawnScheduleFinished(entity,entity,image_base+RVA_FX_VTABLE,
        image_base+RVA_FX_NODE_VTABLE,123u,1,0,-1,readable));
    entity[0x2bu]=2u;
    entity[0x144u]=0x80u;
    assert(!SudekiMpStorySpawnScheduleFinished(entity,entity,image_base+RVA_FX_VTABLE,
        image_base+RVA_FX_NODE_VTABLE,123u,2,0,0,readable));
}
/* Referenced by the naked assembly constructor below. */
static uint8_t solo_test_group[0x1b4] __attribute__((used)),solo_test_point[0x80] __attribute__((used)),
    solo_test_sequence[0x50] __attribute__((used)),solo_test_job[0x28] __attribute__((used)),solo_test_actor[0x200];
static unsigned solo_native_calls,solo_destroy_calls;
static BOOL solo_test_release;
static void *solo_test_resume_fn;
static void solo_release_native(uint8_t *actor) {
    __asm__ volatile("call *%1" : "+S"(actor) : "m"(solo_test_resume_fn) : "eax","ecx","edx","memory","cc");
}
static void spawn_attach_pause_contract(void) {
    assert(solo_attach_code(image_base));
    const unsigned ranges[][2]={{0x72df9,5},{0x737b4,5},{0xb1683,23},{0xb17d0,30}};
    for(unsigned i=0;i<4;++i) for(unsigned j=0;j<ranges[i][1];++j) {
        image_base[ranges[i][0]+j]^=1; assert(!solo_attach_code(image_base));
        image_base[ranges[i][0]+j]^=1;
    }
    /* Execute the retail reference release with its continuation made inert.
     * With a separately retained world reference it must never schedule AI. */
    uint8_t continuation=image_base[0xb17ee];assert(continuation==0x8b);
    image_base[0xb17ee]=0xc3;solo_test_resume_fn=image_base+0xb17d0;
    assert(FlushInstructionCache(GetCurrentProcess(),image_base,0x45f000));
    for(unsigned n=2;n<255;++n) {
        solo_test_actor[0x2b]=(uint8_t)n;solo_release_native(solo_test_actor);
        assert(solo_test_actor[0x2b]==n-1);
    }
    image_base[0xb17ee]=continuation;
    assert(FlushInstructionCache(GetCurrentProcess(),image_base,0x45f000));
    puts("SpawnAttachPauseRelease mutation refusals and native 2..254 -> 1..253 references PASS");
}
static unsigned __attribute__((thiscall)) solo_test_step(void *raw) {
    assert(raw==solo_test_job); ++solo_native_calls;
    ((uint8_t *)raw)[0x24u]=((uint8_t *)raw)[0x24u]?4:1;
    if(((uint8_t *)raw)[0x24u]==4u && solo_test_release) {
        uint8_t continuation=image_base[0xb17ee];image_base[0xb17ee]=0xc3;
        assert(FlushInstructionCache(GetCurrentProcess(),image_base,0x45f000));
        solo_release_native(solo_test_actor);
        image_base[0xb17ee]=continuation;
        assert(FlushInstructionCache(GetCurrentProcess(),image_base,0x45f000));
    }
    SetLastError(0x1234u); return ((uint8_t *)raw)[0x24u]==4u;
}
static void * __attribute__((thiscall)) solo_test_destroy(void *raw,unsigned flags) {
    assert(raw==solo_test_job && flags==1u); ++solo_destroy_calls;
    memset(raw,0xdd,0x28u); SetLastError(0x5678u); return raw;
}
static void __attribute__((naked,noinline)) solo_test_construct(void) {
    __asm__ volatile("pushl %ebx\n\tpushl %esi\n\tpushl %edi\n\t"
        "pushl $_solo_test_sequence\n\tpushl $_solo_test_point\n\t"
        "movl $_solo_test_job,%eax\n\tmovl $_solo_test_group,%edx\n\t"
        "call _solo_ctor_entry\n\tpopl %edi\n\tpopl %esi\n\tpopl %ebx\n\tret\n\t");
}
static void deferred_spawn_contract(void) {
    fixture(); assert(solo_code(image_base));
    for(unsigned i=0;i<sizeof(solo_step_head);++i) {
        image_base[SOLO_STEP+i]^=1; assert(!solo_code(image_base)); image_base[SOLO_STEP+i]^=1;
    }
    *(void **)solo_test_group=image_base+RVA_GROUP_VTABLE;
    *(void **)(solo_test_group+8u)=image_base+RVA_GROUP_NODE_VTABLE;
    *(uint32_t *)(solo_test_group+0x34u)=123u;
    *(unsigned *)(solo_test_group+0x168u)=1; *(void **)(solo_test_group+0x170u)=solo_test_point;
    *(unsigned *)(solo_test_group+0x16cu)=1; *(void **)(solo_test_group+0x174u)=solo_test_sequence;
    *(uint32_t *)(solo_test_sequence+0x10u)=456u;
    registry_entries[0]=solo_test_group;
    solo_ctor_original=image_base+SOLO_CTOR;
    spawn_jobs_begin(solo_test_group); SetLastError(0xabcdu); solo_test_construct(); spawn_jobs_end();
    assert(GetLastError()==0xabcdu);
    SoloJob *s=solo_find(solo_test_job); assert(s && s->serial && !solo_fault);
    assert(*(void **)(solo_test_job+0x10u)==solo_test_group && *(void **)(solo_test_group+4u)==solo_test_job+0x10u);
    assert(!spawn_jobs_uninstall()); /* A live native job retains hook dependencies. */
    *(void **)(solo_test_job+4u)=solo_test_actor+0x2cu;
    *(void **)solo_test_actor=image_base+0x2d5fa0u;
    *(void **)(solo_test_actor+0x2cu)=image_base+0x2d5fe4u;
    *(uint32_t *)(solo_test_actor+0x34u)=456u;
    original_solo_step=solo_test_step; original_solo_destroy=solo_test_destroy;
    void *replacement=(void *)solo_step;
    solo_step_hook=(SudekiMpPointerHook){.slot=&replacement,.replacement_value=replacement,.installed=TRUE};
    assert(!solo_step(solo_test_job) && deferred_calls==1u && solo_native_calls==1u && GetLastError()==0x1234u);
    solo_test_actor[0x2b]=2u;solo_test_release=TRUE;
    assert(solo_step(solo_test_job)==1u && deferred_calls==2u && solo_native_calls==2u);
    assert(s->native_pause_delta==-1 && solo_test_actor[0x2b]==1u);solo_test_release=FALSE;
    s->active=TRUE; ++solo_callbacks;
    assert(solo_witness(s));
    *(void **)(solo_test_job+0x10u)=entity; assert(!solo_witness(s));
    *(void **)(solo_test_job+0x10u)=solo_test_group;
    *(uint32_t *)(solo_test_actor+0x34u)=999u; assert(!solo_witness(s));
    *(uint32_t *)(solo_test_actor+0x34u)=456u;
    ++solo_thread; assert(!solo_witness(s)); --solo_thread;
    --solo_callbacks; s->active=FALSE;
    assert(solo_destroy(solo_test_job,1u)==solo_test_job && solo_destroy_calls==1u && !solo_find(solo_test_job));
    assert(GetLastError()==0x5678u);
    memset(&solo_step_hook,0,sizeof(solo_step_hook)); registry_entries[0]=entity;
}

static unsigned owner_retire_calls;
static void __stdcall owner_retire_native(void *raw) {
    assert(raw==entity); ++owner_retire_calls;
    assert(!effect_retire_uninstall());
    *(unsigned *)(registry+0x34u)=0;
    memset(entity,0xdd,sizeof(entity)); /* Post-witness must not read released storage. */
    SetLastError(0x789au);
}
static void delayed_owner_effect_retirement(void) {
    fixture(); client_role=TRUE;
    uint8_t original_body[0x107]; memcpy(original_body,image_base+EFFECT_RETIRE,sizeof(original_body));
    assert(effect_retire_code(image_base));
    image_base[0xe1875u]^=1; assert(!effect_retire_code(image_base)); image_base[0xe1875u]^=1;
    assert(effect_retire_install(image_base));
    assert(!memcmp(original_body,image_base+EFFECT_RETIRE,sizeof(original_body)));
    effect_retire_original=owner_retire_native;
    effect_owner_retire(entity);
    assert(owner_retire_calls==1 && retire_admissions==1 && GetLastError()==0x789au && !retire_active);
    fixture(); refuse_retirement=TRUE; effect_owner_retire(entity);
    assert(owner_retire_calls==2 && retire_admissions==2 && GetLastError()==0x789au);
    fixture(); client_role=FALSE; effect_owner_retire(entity);
    assert(owner_retire_calls==3 && retire_admissions==2);
    fixture(); client_role=TRUE; *(void **)(entity+0x280u)=registry; effect_owner_retire(entity);
    assert(owner_retire_calls==4 && retire_admissions==2);
    refuse_retirement=FALSE;
    assert(effect_retire_uninstall());
    assert(!memcmp(original_body,image_base+EFFECT_RETIRE,sizeof(original_body)));
}

static void host_driven_sigil_end(void) {
    fixture(); client_role=FALSE;
    memset(sigils,0,sizeof(sigils)); memset(sigil_occurrences,0,sizeof(sigil_occurrences));
    memset(sigil_host,0,sizeof(sigil_host)); sigil_fault=FALSE; sigil_thread=GetCurrentThreadId(); sigil_serial=0;
    uint8_t *entries[3]={solo_test_group,solo_test_actor,entity};
    uint8_t animation[0x140]={0},holder[12]={0},object[0x38]={0};
    *(void **)(registry+0x3cu)=entries; *(unsigned *)(registry+0x34u)=3;
    *(void **)(solo_test_actor+0x58u)=animation; *(void **)animation=image_base+0x2c8504u;
    *(void **)(animation+0x10u)=solo_test_actor; *(void **)(animation+0x100u)=entity;
    *(void **)(solo_test_point+0x6cu)=solo_test_actor;
    memset(solo_test_job,0,sizeof(solo_test_job));
    *(void **)solo_test_job=image_base+SOLO_VT;
    *(void **)(solo_test_job+4u)=solo_test_actor+0x2cu;
    *(void **)(solo_test_job+0x10u)=solo_test_group;
    *(void **)(solo_test_job+0x1cu)=solo_test_point;
    *(void **)(solo_test_job+0x20u)=solo_test_sequence;
    *(uint32_t *)(entity+0x29cu)=0x29; *(uint32_t *)(entity+0x2a0u)=0x7e8a6f3f;
    *(void **)(entity+0x214u)=holder; *(void **)(holder+8u)=object;
    *(uint32_t *)(object+0x34u)=0x81;
    sigil_constructed(solo_test_job); sigil_job_step(solo_test_job); sigil_tick();
    assert(!sigil_fault && sigils[0].effect==entity && sigils[0].state.occurrence==1);
    SudekiMpStoryEffectState captured={0}; SudekiMpLanStorySpawnEffectsCapture(&captured);
    assert(captured.sigils[0].spawn_serial && !captured.sigils[0].ended);
    /* Exact site validation includes both call targets and the tail's stack shape. */
    assert(effect_end_sites_code(image_base));
    const unsigned offsets[]={0xe2161u,0xe2162u,0xe2163u,0xe2167u,0xe216eu,0x131acfu,0x131ad3u,0x131ad4u};
    for(unsigned i=0;i<sizeof(offsets)/sizeof(*offsets);++i) {
        image_base[offsets[i]]^=1; assert(!effect_end_sites_code(image_base)); image_base[offsets[i]]^=1;
    }
    assert(effect_retire_install(image_base));
    effect_retire_original=owner_retire_native; unsigned before=owner_retire_calls;
    ((void (__stdcall *)(void *))(image_base+RVA_FX_RETIRE_TAIL))(entity);
    assert(owner_retire_calls==before+1 && sigils[0].state.ended && !sigils[0].effect);
    assert(effect_retire_uninstall() && effect_end_sites_code(image_base));
    SudekiMpStorySigilState old=sigils[0].state;
    sigil_job_destroyed(solo_test_job);
    /* Also model a missing local retirement observation before address reuse. */
    sigils[0].state.ended=0; sigils[0].effect=entity;
    /* Reuse every native address for the next occurrence: old end cannot hide it. */
    fixture(); client_role=TRUE;
    *(void **)(registry+0x3cu)=entries; *(unsigned *)(registry+0x34u)=3;
    *(uint32_t *)(entity+0x29cu)=0x29; *(uint32_t *)(entity+0x2a0u)=0x7e8a6f3f;
    *(void **)(entity+0x214u)=holder; *(void **)(holder+8u)=object;
    *(uint32_t *)(object+0x34u)=0x81;
    sigil_constructed(solo_test_job); sigil_job_step(solo_test_job); sigil_host[0]=old; sigil_tick();
    SigilReceipt *next=NULL;
    for(unsigned i=0;i<SIGIL_RECEIPTS;++i) if(sigils[i].state.occurrence==2) next=&sigils[i];
    assert(next && next->effect==entity && !next->applied && *(uint32_t *)(object+0x34u)==0x81);
    sigil_host[0]=next->state; sigil_host[0].ended=1;
    *(void **)(animation+0x10u)=entity; sigil_tick(); assert(!next->applied); /* wrong owner */
    *(void **)(animation+0x10u)=solo_test_actor;
    *(uint32_t *)(entity+0x2a0u)=0xb51df833; sigil_tick(); assert(!next->applied); /* ordinary hit */
    *(uint32_t *)(entity+0x2a0u)=0x7e8a6f3f; sigil_tick();
    assert(next->applied && *(uint32_t *)(object+0x34u)==0x84);
    /* Duplicate end is idempotent and never destroys the native owner handle. */
    sigil_tick(); assert(*(void **)(animation+0x100u)==entity && in_registry(entity));
    memset(sigils,0,sizeof(sigils)); memset(sigil_host,0,sizeof(sigil_host));
    sigil_fault=FALSE; fixture();
}


static uint8_t gate_models[3][0x5a0],gate_group_fixture[0x1b4],gate_points[2][0x80],gate_sequence[0x50];
static uint8_t *gate_registry_entries[4];
static uint32_t gate_name_record[2],gate_group_name_record[2];
static uint8_t gate_zone_fixture[0x11c],gate_zone_entries[0x24],gate_world_fixture[0x58],gate_descriptor_fixture[0x38];
static void gate_model_fixture(unsigned i) {
    uint8_t *e=gate_models[i]; memset(e,0,0x5a0);
    *(void **)e=image_base+0x2d5b00u;*(void **)(e+8u)=image_base+0x2d5b24u;
    *(void **)(e+0x2cu)=image_base+0x2d5b44u;*(uint32_t *)(e+0x30u)=0xf8fu;
    *(uint32_t *)(e+0x34u)=0x47b40b4au;*(void **)(e+0x38u)=gate_name_record;
    *(void **)(e+0x44u)=e+0x210u;*(void **)(e+0x220u)=e;
}
static unsigned gate_script_calls;
static void __attribute__((thiscall)) fake_gate_heartbeat(void *component) {(void)component;++gate_script_calls;}
static void gate_contract(void) {
    puts("gate_native_code exact supported-image check");fflush(stdout);
    assert(gate_native_code(image_base));
    const unsigned sites[]={GATE_SPAWN,0x76000u,0x7600bu,GATE_CREATE,0x73907u,0x73adeu,
        0x73a52u,0x73a54u,0x2c9eccu,0x72e30u,0x72e6eu,0x72e8au,GATE_HEARTBEAT_CALL,
        GATE_WEAK_ASSIGN,0x1791u,0x17a4u,GATE_PAUSE,0x1060f4u,GATE_POINT_CLEAR,0x156145u,0x156159u};
    for(unsigned i=0;i<sizeof(sites)/sizeof(sites[0]);++i) {
        image_base[sites[i]]^=1;assert(!gate_native_code(image_base));image_base[sites[i]]^=1;
    }
    assert(gates_install(image_base));gate_thread=GetCurrentThreadId();
    gate_name_record[0]=1;gate_name_record[1]=(uint32_t)(uintptr_t)"HC_AklorianBarrier";
    gate_group_name_record[0]=1;gate_group_name_record[1]=(uint32_t)(uintptr_t)"fixture_gates";
    for(unsigned i=0;i<3u;++i) {gate_model_fixture(i);gate_registry_entries[i]=gate_models[i];}
    memset(gate_group_fixture,0,sizeof(gate_group_fixture));memset(gate_points,0,sizeof(gate_points));
    uint8_t *g=gate_group_fixture;
    *(void **)g=image_base+RVA_GROUP_VTABLE;*(void **)(g+8u)=image_base+RVA_GROUP_NODE_VTABLE;
    *(uint32_t *)(g+0x34u)=77;*(void **)(g+0x38u)=gate_group_name_record;
    *(unsigned *)(g+0x168u)=2;*(unsigned *)(g+0x16cu)=1;
    *(void **)(g+0x170u)=gate_points;*(void **)(g+0x174u)=gate_sequence;
    *(void **)(image_base+0x408d10u)=gate_world_fixture;
    *(void **)(gate_world_fixture+0xcu)=gate_descriptor_fixture;
    *(void **)gate_descriptor_fixture=image_base+0x2c82a4u;
    *(void **)(gate_descriptor_fixture+0x14u)=gate_zone_fixture;
    *(void **)(gate_zone_fixture+0xbcu)=gate_models[0]+0x2cu;
    *(void **)gate_zone_fixture=image_base+0x2cdcdcu;
    *(unsigned *)(gate_zone_fixture+0xfcu)=1;*(void **)(gate_zone_fixture+0x104u)=gate_zone_entries;
    *(void **)(gate_zone_entries+0x1cu)=g+0x2cu;
    gate_registry_entries[3]=g;*(void **)(image_base+RVA_REGISTRY)=registry;
    *(unsigned *)(registry+0x34u)=4;*(void **)(registry+0x3cu)=gate_registry_entries;
    for(unsigned i=0;i<2u;++i) {gate_bind((GateWeak *)(gate_points[i]+0x6cu),gate_models[i+1]);*(uint16_t *)(gate_points[i]+0x7au)=0x800u;}
    assert(gate_observe_group(g,gate_zone_fixture,gate_models[0],"fixture"));
    GateLease *lease=&gate_leases[0];assert(lease->used && lease->state.live_mask==3 && lease->state.serial);
    uint32_t a,b,serial=lease->state.serial;
    assert(SudekiMpLanStoryGateIdentity(gate_models[1],&a));assert(SudekiMpLanStoryGateIdentity(gate_models[2],&b));
    assert(a && b && a!=b && !SudekiMpLanStoryGateIdentity(gate_models[0],&a));
    uint32_t old_id=a;
    SudekiMpStoryEffectState unknown_gates={.current="fixture"};
    assert(!SudekiMpLanStoryGatesFollow(&unknown_gates));
    assert(lease->actors[0].object==gate_models[1] && lease->actors[1].object==gate_models[2]);
    assert(gate_observe_group(g,gate_zone_fixture,gate_models[0],"fixture") && lease->state.serial==serial);
    uint8_t component[0x30]={0};*(void **)(component+0x10u)=gate_models[1];
    *(void **)(gate_models[1]+0x68u)=component;
    GateHeartbeat original=gate_heartbeat_original;gate_heartbeat_original=fake_gate_heartbeat;
    gate_heartbeat(component);assert(gate_script_calls==1); /* Host remains native. */
    lease->owned=TRUE;gate_heartbeat(component);assert(gate_script_calls==1);
    *(void **)(component+0x10u)=gate_models[0];gate_heartbeat(component);assert(gate_script_calls==2);
    lease->owned=FALSE;gate_heartbeat_original=original;
    /* Native TPtr base destruction clears every observer. Reusing the same
     * addresses cannot resurrect the old gate occurrence or template identity. */
    typedef void (__attribute__((thiscall)) *DestroyWeakBase)(void *);
    for(unsigned i=1;i<3u;++i) ((DestroyWeakBase)(image_base+0x4d30u))(gate_models[i]);
    assert(!lease->actors[0].object && !lease->actors[1].object);
    BOOL cleared=FALSE;
    assert(gate_retire(lease,0,&cleared) && cleared && !(*(uint16_t *)(gate_points[0]+0x7au)&0x800u));
    assert(gate_observe_group(g,gate_zone_fixture,gate_models[0],"fixture") && !lease->state.live_mask);
    for(unsigned i=0;i<2u;++i) {gate_model_fixture(i+1);gate_bind((GateWeak *)(gate_points[i]+0x6cu),gate_models[i+1]);*(uint16_t *)(gate_points[i]+0x7au)=0x800u;}
    assert(gate_observe_group(g,gate_zone_fixture,gate_models[0],"fixture"));assert(lease->state.serial>serial);
    assert(SudekiMpLanStoryGateIdentity(gate_models[1],&a) && a!=old_id);
    /* A pending native setup is not safe to cancel. */
    BOOL entered=FALSE;assert(!gate_retire(lease,0,&entered) && !entered && lease->actors[0].object);
    assert(gates_uninstall() && gate_native_code(image_base));
    for(unsigned i=0;i<2u;++i) {
        gate_base=image_base;gate_bind((GateWeak *)(gate_points[i]+0x6cu),NULL);
        assert(!*(void **)(gate_models[i+1]+4u));
    }
    gate_base=NULL;
    puts("gate_native_code and gate identity/lifetime tests PASS");fflush(stdout);
}

static uint8_t start_fixture_group[0x1b4];
static void *start_fixture_resource __attribute__((used));
static uint32_t start_fixture_selection __attribute__((used));
static void *start_fixture_receiver __attribute__((used));
static unsigned start_fixture_calls __attribute__((used));
static void __attribute__((naked,noinline,used)) start_lookup(void) {
    __asm__ volatile("movl _start_fixture_resource,%eax\n\tret $16\n\t");
}
static void __attribute__((naked,noinline,used)) start_core(void) {
    __asm__ volatile("movl %eax,_start_fixture_selection\n\tmovl %ecx,_start_fixture_receiver\n\tincl _start_fixture_calls\n\tret\n\t");
}
static unsigned __attribute__((thiscall)) start_kind(void *self) {
    assert(self==start_fixture_resource);return 0x20;
}
static void __cdecl start_scoped(const char *scope,const void *resource,int selection,int count) {
    assert(!strcmp(scope,"dungeon") && resource);
    start_fixture_selection=(uint32_t)selection;
    *(int32_t *)(start_fixture_group+0x178)=count;
    ++start_fixture_calls;
}
static void start_test_endpoint(uint32_t rva,void *fn) {
    image_base[rva]=0xe9;
    *(int32_t *)(image_base+rva+1)=(intptr_t)fn-(intptr_t)(image_base+rva+5);
}
static void spawn_start_variants(void) {
    assert(spawn_start_code());
    for(unsigned i=0;i<3;++i) {
        for(unsigned j=0;j<spawn_start_contracts[i].size;++j) {
            image_base[spawn_start_contracts[i].rva+j]^=1;
            assert(!spawn_start_code());
            image_base[spawn_start_contracts[i].rva+j]^=1;
        }
        printf("%s mutation refusals PASS\n",spawn_start_contracts[i].name);
    }
    uint8_t old_lookup[5],old_core[5],old_scoped[5];
    memcpy(old_lookup,image_base+0x3e540,5);memcpy(old_core,image_base+0x73f00,5);
    memcpy(old_scoped,image_base+rvas[START3],5);
    start_test_endpoint(0x3e540,(void *)(uintptr_t)start_lookup);
    start_test_endpoint(0x73f00,(void *)(uintptr_t)start_core);
    start_test_endpoint(rvas[START3],(void *)(uintptr_t)start_scoped);
    assert(FlushInstructionCache(GetCurrentProcess(),image_base,0x45f000));
    void *table[5]={NULL,NULL,NULL,NULL,(void *)(uintptr_t)start_kind};
    start_fixture_resource=start_fixture_group+0x2c;
    *(void **)start_fixture_resource=table;
    uint32_t resource[3]={0x20,123,0};
    for(unsigned method=1;method<=3;++method) for(int pass=-1;pass<=3;++pass) {
        SudekiMpStorySpawnGroup command={.name="fixture",.sequence=pass,.serial=1,
            .method=method,.selection=method==1?0:1};
        if(method==3) strcpy(command.scope,"dungeon");
        unsigned before=start_fixture_calls;
        replay_start_native(&command,resource);
        assert(start_fixture_calls==before+1 && start_fixture_selection==(uint32_t)command.selection);
        assert(*(int32_t *)(start_fixture_group+0x178)==pass);
        if(method!=3) assert(start_fixture_receiver==start_fixture_group);
    }
    memcpy(image_base+0x3e540,old_lookup,5);memcpy(image_base+0x73f00,old_core,5);
    memcpy(image_base+rvas[START3],old_scoped,5);
    assert(FlushInstructionCache(GetCurrentProcess(),image_base,0x45f000));
    assert(spawn_start_code());
    /* Identical commands are separate occurrences, including scoped groups. */
    uint32_t record[2]={1,(uint32_t)(uintptr_t)"fixture"};resource[2]=(uint32_t)(uintptr_t)record;
    started_count=start_serial=0;
    record_start(resource,2,1,1,NULL);assert(started_count==1 && started[0].selection==1);
    uint32_t serial=started[0].serial;
    record_start(resource,2,1,1,NULL);assert(started_count==1 && started[0].serial==serial+1);
    record_start(resource,3,7,-1,"dungeon");assert(started_count==2 && started[1].selection==7 && started[1].sequence==-1);
    record_start(resource,3,1,1,"0123456789012345678901234567890123456789");assert(started_count==2);
    start_serial=UINT32_MAX;record_start(resource,1,0,1,NULL);assert(started_count==2);
    started_count=start_serial=0;memset(started,0,sizeof(started));
    puts("native default/selected argument routes, scoped dispatch and repeated starts PASS");
}
#include "story_spawn_event_fixture.h"
#include "story_death_interrupt_fixture.h"
int main(int argc,char **argv) {
    setvbuf(stdout,NULL,_IONBF,0);
    assert(argc==2); wchar_t path[1024];
    assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024));
    image_base=map_image(path);
    spawn_event_fixture();
    death_interrupt_fixture();
    spawn_start_variants();
    assert(FlushInstructionCache(GetCurrentProcess(),image_base,0x45f000));
    gate_contract();
    delayed_owner_effect_retirement();
    spawn_schedule_ownership();
    spawn_attach_pause_contract();
    deferred_spawn_contract();
    host_driven_sigil_end();
    puts("death visual contract"); fflush(stdout); death_visual_contract();
    puts("death visual contract PASS"); fflush(stdout);
    puts("native effect completion"); fflush(stdout); effect_completion();
    puts("native effect completion PASS"); fflush(stdout);
    idle_groups_do_not_admit_residency();
    effects_outlive_tracking_windows();
    effect_burst_exceeds_old_slots();
    native_frame_stamp();
    sigil_hide_only();
    fixture();
    /* Every checked byte and vtable identity must still fail closed. */
    for(unsigned i=0;i<sizeof(fx_entry);++i) {
        image_base[RVA_FX_UPDATE+i]^=1;
        assert(!fx_exact(entity));
        image_base[RVA_FX_UPDATE+i]^=1;
    }
    *(void **)entity=image_base; assert(!fx_exact(entity));
    *(void **)entity=image_base+RVA_FX_VTABLE;
    *(void **)(entity+8)=image_base; assert(!fx_exact(entity));
    *(void **)(entity+8)=image_base+RVA_FX_NODE_VTABLE;
    void **slot=(void **)(image_base+RVA_FX_NODE_VTABLE)+1;
    void *saved=*slot; *slot=image_base; assert(!fx_exact(entity)); *slot=saved;
    assert(FlushInstructionCache(GetCurrentProcess(),image_base+RVA_FX_UPDATE,sizeof(fx_entry)));
    /* A plausible scheduler-list float used to override the true delta. */
    tick(0.016f,0.016f); tick(0.033f,0.033f);
    tick(0.1f,0.1f); tick(0.5f,0.1f);
    /* A pointer-shaped subnormal word must also remain irrelevant. */
    uint32_t pointer_word=0x006c1234; memcpy(scheduler_decoy+3,&pointer_word,4);
    tick(0.02f,0.02f);
    no_tick(0.0f); no_tick(-0.02f); no_tick(NAN); no_tick(INFINITY); no_tick(-INFINITY);
    entity[0x2b]=0; no_tick(0.016f); entity[0x2b]=1;
    *(unsigned *)(registry+0x34)=0; no_tick(0.016f);
    VirtualFree(image_base,0,MEM_RELEASE);
    puts("story spawn timing exact-image call shape, native frame-stamp guard, capped dt, scheduler isolation and identity checks passed (no live rendering)");
    return 0;
}
