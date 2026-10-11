/* Actual local presenter with inert native-owned objects and ownership gates.
 * No game tick/script/renderer runs; this tests apply, hold and restoration. */
#include "../src/hooks/lan_story_local_control.c"
#include "../src/hooks/lan_arena_owner_view.c"
#include "../src/network/lan_story_frame.c"
#include <assert.h>
#include <stdio.h>
static BOOL owner_exact=TRUE,input_exact=TRUE;
BOOL SudekiMpLanStoryInputExact(void *controller,void *actor) {
    return input_exact && controller==retained.controller && actor==retained.actors[selected];
}
static BOOL exact_fixture(const SudekiMpLanStoryNativeRoster *r,void *context) {
    (void)context;return owner_exact && r==&retained;
}
static void exercise(unsigned character) {
    base=VirtualAlloc(NULL,0x45f000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(base);
    uint8_t actor[0x200]={0},ai[0x180]={0},ai_mode[0x20]={0},position[0xa0]={0};
    uint8_t camera[0x108]={0},target[0x34]={0},manager[0x60]={0};
    uint8_t camera_mode[0x20]={0},scene_manager[0x44]={0},scene[0x80]={0},controller[0x20]={0};
    uint8_t *render=VirtualAlloc(NULL,0x1000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);assert(render);
    uint8_t handle[0xc]={0},config[0x80]={0};
    native_thread=GetCurrentThreadId();entered=TRUE;selected=character;
    retained=(SudekiMpLanStoryNativeRoster){.world=(void *)1,.group=(void *)2,.controller=controller,
        .epoch=1,.available_mask=1u<<character,.leader_character=character};
    retained.actors[character]=actor; retained.ai[character]=ai;
    *(void **)(actor+0x94)=ai;*(void **)ai=base+0x2d4924;*(void **)(ai+0x10)=actor;*(void **)(ai+0x3c)=ai_mode;
    *(void **)(actor+0x44)=position;*(void **)position=base+0x2cdefc;*(void **)(position+0x10)=actor;
    const float xyz[3]={10,2,30};memcpy(position+0x18,xyz,sizeof(xyz));
    *(void **)camera=base+0x2cce5c;*(void **)(camera+0x34)=render;*(void **)render=base+0x2dd638;
    *(void **)(base+0x408da8)=camera_mode;*(void **)(camera_mode+0xc)=camera+0x2c;
    *(void **)(base+0x408d58)=scene_manager;*(void **)(scene_manager+0x40)=scene;*(void **)(scene+0x7c)=render;
    *(void **)(base+0x409d7c)=manager;*(void **)manager=base+0x2c7b80;*(void **)(manager+0x20)=camera;*(void **)(manager+0x24)=camera;
    *(void **)(camera+0xb4)=target;*(void **)(camera+0xb8)=target;*(void **)target=base+0x2d42cc;
    *(uint32_t *)(target+4)=1;*(void **)(target+0x20)=actor;
    *(void **)(camera+0x38)=handle;*(void **)(handle+8)=config;
    const unsigned fields[]={6,7,8,11,12,13,18,26,27,28,29,30};
    const float values[]={1.5f,3,10,6,90,180,1,8,1,2,3,5};
    for(unsigned i=0;i<12;++i) ((uint32_t *)(config+8))[fields[i]/32]|=1u<<(fields[i]%32);
    *(uint32_t *)config=0x48+sizeof(values);*(void **)(config+0x44)=config+0x48;memcpy(config+0x48,values,sizeof(values));
    view.seeded=TRUE;
    view.geometry=(SudekiMpLanStoryView){.valid=1,.matrix={1,0,0,0,0,1,0,0,0,0,1,0,1,3,-3,1},.projection={1,.1f,1000}};
    assert(SudekiMpLanStoryLocalControlPresent(&retained,exact_fixture,NULL,0,0));
    SudekiMpLanStoryView orbit=view.geometry;
    SudekiMpLanStoryView cast=orbit;cast.camera_serial=13;cast.matrix[12]=100;
    assert(SudekiMpLanStoryLocalControlPresentCast(&retained,exact_fixture,NULL,.3f,.2f,&cast));
    assert(view.cast_active && !memcmp(&orbit,&view.geometry,sizeof(orbit)));
    assert(!memcmp(render+0x90,cast.matrix,sizeof(cast.matrix)));
    assert(SudekiMpLanStoryLocalControlPresentCast(&retained,exact_fixture,NULL,.3f,.2f,NULL));
    assert(!memcmp(render+0x90,cast.matrix,sizeof(cast.matrix))); /* Unknown retains. */
    *(float *)(render+0x90+48)=99;
    assert(!SudekiMpLanStoryLocalControlRestoreCastView(&retained,exact_fixture,NULL) && view.cast_active);
    memcpy(render+0x90,cast.matrix,sizeof(cast.matrix)); /* Fixture repair for retry. */
    void *original_slot=*(void **)(scene+0x7c);*(void **)(scene+0x7c)=camera;
    assert(!SudekiMpLanStoryLocalControlRestoreCastView(&retained,exact_fixture,NULL) && view.cast_active);
    *(void **)(scene+0x7c)=original_slot;
    owner_exact=FALSE;assert(!SudekiMpLanStoryLocalControlRestoreCastView(&retained,exact_fixture,NULL));owner_exact=TRUE;
    input_exact=FALSE;assert(!SudekiMpLanStoryLocalControlRestoreCastView(&retained,exact_fixture,NULL));input_exact=TRUE;
    DWORD old,ignored;assert(VirtualProtect(render,0x1000,PAGE_READONLY,&old));
    assert(!SudekiMpLanStoryLocalControlRestoreCastView(&retained,exact_fixture,NULL) && view.cast_active);
    assert(VirtualProtect(render,0x1000,old,&ignored));
    assert(SudekiMpLanStoryLocalControlRestoreCastView(&retained,exact_fixture,NULL) && !view.cast_active);
    assert(!memcmp(render+0x90,orbit.matrix,sizeof(orbit.matrix))); /* Return to heading/distance. */
    for(unsigned i=0;i<2;++i) {
        cast.matrix[12]+=4;assert(SudekiMpLanStoryLocalControlPresentCast(&retained,exact_fixture,NULL,0,0,&cast));
        assert(SudekiMpLanStoryLocalControlRestoreCastView(&retained,exact_fixture,NULL));
    }
    cast.matrix[0]=NAN;assert(!SudekiMpLanStoryLocalControlPresentCast(&retained,exact_fixture,NULL,0,0,&cast));
    assert(!view.cast_active && !memcmp(render+0x90,orbit.matrix,sizeof(orbit.matrix)));
    cast=orbit;cast.camera_serial=17;assert(SudekiMpLanStoryLocalControlPresentCast(&retained,exact_fixture,NULL,0,0,&cast));
    assert(!SudekiMpLanStoryLocalControlUninstall()); /* Live owner retained. */
    uint8_t *image=base;
    assert(SudekiMpLanStoryLocalControlNativeExitReturned());assert(!view.cast_active);
    assert(SudekiMpLanStoryLocalControlUninstall());VirtualFree(render,0,MEM_RELEASE);VirtualFree(image,0,MEM_RELEASE);
}
int main(void) {
    for(unsigned c=0;c<4;++c) exercise(c);
    puts("Story cast view native lease: all characters apply, unknown hold, restore retry, repeat, quit PASS");
    return 0;
}
