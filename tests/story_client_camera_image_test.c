#include "engine/build_identity.h"
#include "hooks/lan_story_client_camera.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* The house static camera builds its ResourceName through the spawn-trace
 * helper; here it never runs a native constructor. */
static unsigned resource_calls;
BOOL SudekiMpLanStoryWithResourceName(const char *name,unsigned kind,
    void (*use)(void *resource,void *context),void *context) {
    (void)name;(void)kind;(void)use;(void)context;++resource_calls;return FALSE;
}
/* Exact-image guards of the client native camera (#71): every checked byte of
 * the camera update, input event, SetCameraState/CanSetCameraState entries and
 * the node vtable slot refuses mutation; a malformed lease never ticks. */
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(file!=INVALID_HANDLE_VALUE);DWORD size=GetFileSize(file,NULL),got=0;
    uint8_t *raw=malloc(size);assert(raw && ReadFile(file,raw,size,&got,NULL) && got==size);CloseHandle(file);
    IMAGE_DOS_HEADER *dos=(void *)raw;IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *b=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(b);memcpy(b,raw,nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *sections=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        IMAGE_SECTION_HEADER *s=&sections[i];
        assert(s->PointerToRawData<=size && s->SizeOfRawData<=size-s->PointerToRawData);
        assert(s->VirtualAddress<=nt->OptionalHeader.SizeOfImage &&
            s->SizeOfRawData<=nt->OptionalHeader.SizeOfImage-s->VirtualAddress);
        memcpy(b+s->VirtualAddress,raw+s->PointerToRawData,s->SizeOfRawData);
    }
    IMAGE_DATA_DIRECTORY reloc=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    uintptr_t delta=(uintptr_t)b-nt->OptionalHeader.ImageBase;
    for(unsigned offset=0;offset<reloc.Size;) {
        IMAGE_BASE_RELOCATION *block=(void *)(b+reloc.VirtualAddress+offset);
        assert(block->SizeOfBlock>=sizeof(*block) && block->SizeOfBlock<=reloc.Size-offset);
        uint16_t *items=(void *)(block+1);unsigned count=(block->SizeOfBlock-sizeof(*block))/2;
        for(unsigned i=0;i<count;++i) {
            unsigned type=items[i]>>12,rva=block->VirtualAddress+(items[i]&0xfff);
            assert(type==IMAGE_REL_BASED_ABSOLUTE || type==IMAGE_REL_BASED_HIGHLOW);
            if(type==IMAGE_REL_BASED_HIGHLOW) {
                assert(rva<=nt->OptionalHeader.SizeOfImage-4);*(uint32_t *)(b+rva)+=(uint32_t)delta;
            }
        }
        offset+=block->SizeOfBlock;
    }
    free(raw);return b;
}
typedef struct CameraFixture {
    SudekiMpLanArenaOwnerViewLease lease;
    uint8_t camera[0x110],render[0xe0],manager[0x60],state[0x20],data[0x20];
    uint8_t inner[0x34],outer[0xd0],actor[0x98],position[0x98],speed[0x2c],ui[0x58];
} CameraFixture;
static void camera_fixture(CameraFixture *f,uint8_t *image) {
    memset(f,0,sizeof(*f));
    f->lease.valid=TRUE;f->lease.camera=f->camera;f->lease.render_state=f->render;
    *(void **)f->camera=image+0x2cce5cu;*(void **)(f->camera+8)=image+0x2cce6cu;
    *(void **)(f->camera+0x34)=f->render;*(void **)(f->camera+0x3c)=f->state;
    *(void **)(f->camera+0x40)=f->data;strcpy((char *)f->camera+0x4c,"default");
    *(void **)f->render=image+0x2dd638u;*(void **)f->manager=image+0x2c7b80u;
    *(void **)(f->manager+0x20)=f->camera;*(void **)(f->manager+0x24)=f->camera;
    *(void **)f->state=image+0x2d9854u;*(void **)f->data=image+0x2ca694u;
    *(void **)(f->camera+0xb8)=f->inner;*(void **)(f->camera+0xb4)=f->outer;
    *(void **)f->inner=image+0x2d42ccu;*(uint32_t *)(f->inner+4)=1;
    *(void **)(f->inner+0x20)=f->actor;
    *(void **)f->outer=image+0x2d436cu;*(uint32_t *)(f->outer+4)=1;
    *(void **)(f->outer+0x20)=f->inner;
    *(void **)f->actor=image+0x2d5010u;*(void **)(f->actor+0x44)=f->position;
    *(void **)f->position=image+0x2cdefcu;*(void **)(f->position+0x10)=f->actor;
    f->camera[0x107]=1;f->speed[0x28]=1;
    *(void **)(image+0x409d7cu)=f->manager;*(void **)(image+0x408da0u)=f->speed;
    *(void **)(image+0x408d8cu)=f->ui;
}
static void readiness_unchanged(CameraFixture *f,BOOL expected,BOOL tick_refused) {
    CameraFixture before=*f;
    assert(!!SudekiMpLanStoryClientCameraContinuityReady(&f->lease)==!!expected);
    assert(!memcmp(f,&before,sizeof(*f)));
    if(tick_refused) {
        /* The exact image is deliberately non-executable. A premature input,
         * state request, refresh or update call faults rather than hiding an
         * incorrect refusal order behind an inert replacement. */
        assert(!SudekiMpLanStoryClientCameraTick(&f->lease,0.016f,0.25f,-0.25f));
        assert(!memcmp(f,&before,sizeof(*f)));
    }
}
static DWORD WINAPI continuity_foreign_thread(void *context) {
    CameraFixture *f=context;
    readiness_unchanged(f,FALSE,TRUE);return 0;
}
static void continuity_preflight(uint8_t *image) {
    CameraFixture f;
    camera_fixture(&f,image);
    assert(SudekiMpLanStoryClientCameraInstall((HMODULE)image));
    readiness_unchanged(&f,FALSE,FALSE); /* Cannot establish its own thread. */
    CameraFixture before=f;
    assert(!SudekiMpLanStoryClientCameraTick(&f.lease,0,0,0));
    assert(!memcmp(&f,&before,sizeof(f))); /* Valid zero-dt normal setup only. */
    readiness_unchanged(&f,TRUE,FALSE);
    assert(!SudekiMpLanStoryClientCameraContinuityReady(NULL));
    f.lease.valid=FALSE;readiness_unchanged(&f,FALSE,TRUE);f.lease.valid=TRUE;
    *(void **)(f.manager+0x20)=NULL;readiness_unchanged(&f,FALSE,TRUE);
    *(void **)(f.manager+0x20)=f.camera;
    *(void **)(f.camera+0x34)=NULL;readiness_unchanged(&f,FALSE,TRUE);
    *(void **)(f.camera+0x34)=f.render;
    /* Unknown, cast or other named states must not trigger SetCameraState.
     * Even native interpolation lacks a proven Exploration destination. */
    *(void **)f.state=image;readiness_unchanged(&f,FALSE,FALSE);
    *(void **)f.state=image+0x2db9a4u;*(void **)f.data=image+0x2db998u;
    readiness_unchanged(&f,FALSE,FALSE);
    *(void **)f.state=image+0x2d9854u;readiness_unchanged(&f,FALSE,FALSE);
    *(void **)f.data=image+0x2ca694u;
    readiness_unchanged(&f,TRUE,FALSE);
    puts("PASS CameraContinuityNoMutation: established Exploration only; unknown/cast/transition state, manager, render and unestablished ownership refuse without native calls");

    *(void **)(f.camera+0xb4)=NULL;readiness_unchanged(&f,FALSE,TRUE);
    *(void **)(f.camera+0xb4)=f.outer;
    *(void **)f.outer=image;readiness_unchanged(&f,FALSE,TRUE);
    *(void **)f.outer=image+0x2d436cu;
    *(uint32_t *)(f.outer+4)=0;readiness_unchanged(&f,FALSE,TRUE);*(uint32_t *)(f.outer+4)=1;
    *(void **)(f.outer+0x20)=f.actor;readiness_unchanged(&f,FALSE,TRUE);
    *(void **)(f.outer+0x20)=f.inner;
    *(void **)(f.camera+0xb8)=NULL;readiness_unchanged(&f,FALSE,TRUE);
    *(void **)(f.camera+0xb8)=f.inner;
    *(uint32_t *)(f.inner+4)=0;readiness_unchanged(&f,FALSE,TRUE);*(uint32_t *)(f.inner+4)=1;
    *(void **)(f.inner+0x20)=NULL;readiness_unchanged(&f,FALSE,TRUE);
    *(void **)(f.inner+0x20)=f.actor;
    *(void **)f.actor=image;readiness_unchanged(&f,FALSE,TRUE);
    *(void **)f.actor=image+0x2d5010u;
    *(void **)(f.actor+0x44)=NULL;readiness_unchanged(&f,FALSE,TRUE);
    *(void **)(f.actor+0x44)=f.position;
    *(void **)(f.position+0x10)=NULL;readiness_unchanged(&f,FALSE,TRUE);
    *(void **)(f.position+0x10)=f.actor;
    *(uintptr_t *)(f.position+0x94)=1;readiness_unchanged(&f,FALSE,TRUE);
    *(uintptr_t *)(f.position+0x94)=0;
    *(float *)(f.position+0x18)=NAN;readiness_unchanged(&f,FALSE,TRUE);
    *(float *)(f.position+0x18)=0;
    unsigned slots[]={0x2d42d0u,0x2d42dcu,0x2d4370u};
    for(unsigned i=0;i<sizeof(slots)/sizeof(*slots);++i) {
        void *saved=*(void **)(image+slots[i]);*(void **)(image+slots[i])=NULL;
        readiness_unchanged(&f,FALSE,TRUE);*(void **)(image+slots[i])=saved;
    }
    uint8_t *page=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(page);DWORD old;
    assert(VirtualProtect(page,4096,PAGE_NOACCESS,&old));
    *(void **)(f.camera+0xb4)=page;readiness_unchanged(&f,FALSE,TRUE);
    assert(VirtualProtect(page,4096,PAGE_READWRITE|PAGE_GUARD,&old));
    readiness_unchanged(&f,FALSE,TRUE);
    *(void **)(f.camera+0xb4)=f.outer;assert(VirtualFree(page,0,MEM_RELEASE));
    /* The direct native GameObjectTarget pair is also a complete chain. */
    *(void **)(f.camera+0xb4)=f.inner;readiness_unchanged(&f,TRUE,FALSE);
    *(void **)(f.camera+0xb4)=f.outer;
    unsigned actors[]={0x2d5a88u,0x2d66fcu,0x2d5010u,0x2d555cu};
    for(unsigned i=0;i<4u;++i) { *(void **)f.actor=image+actors[i];readiness_unchanged(&f,TRUE,FALSE); }
    HANDLE thread=CreateThread(NULL,0,continuity_foreign_thread,&f,0,NULL);
    assert(thread && WaitForSingleObject(thread,10000)==WAIT_OBJECT_0);CloseHandle(thread);
    readiness_unchanged(&f,TRUE,FALSE);
    assert(SudekiMpLanStoryClientCameraUninstall());
    readiness_unchanged(&f,FALSE,TRUE);
    puts("PASS CameraTargetPreflightBeforeWrites: all four party targets, direct/offset chains; broken reference, native virtual, actor/position, protected page and foreign thread refuse before input or refresh");
}

/* H1b speech camera: SetRenderCamera stand-in behind a patched jump. */
static uint8_t *speech_manager,*speech_render_obj,*speech_cams[2];
static unsigned set_render_calls;
static uint8_t __attribute__((thiscall)) stub_set_render(void *manager,const char *name) {
    assert(manager==speech_manager && name && !strcmp(name,"default")); ++set_render_calls;
    *(void **)(speech_manager+0x20)=speech_cams[0]; *(void **)(speech_render_obj+0x7c)=*(void **)(speech_cams[0]+0x34);
    return 1;
}
static void speech_camera(uint8_t *image) {
    const struct {unsigned rva,count;} guards[]={{0x36fb0u,32u},{0x370d8u,21u},{0x370fau,3u},{0x15ac0u,27u},
        {0x15ffau,5u},{0x16007u,5u},{0x2c5690u,13u},{0x36e83u,3u}};
    static uint8_t cams[2][0x110],renders[2][0xe0],manager[0x60],scene[0x44],render_obj[0x80];
    for(unsigned i=0;i<2;++i) {
        *(void **)cams[i]=image+0x2cce5cu; *(void **)(cams[i]+8)=image+0x2cce6cu; *(void **)(cams[i]+0x34)=renders[i];
        *(void **)renders[i]=image+0x2dd638u; speech_cams[i]=cams[i];
    }
    strcpy((char *)(cams[0]+0x4c),"default"); strcpy((char *)(cams[1]+0x4c),"SpeechCamera");
    *(void **)manager=image+0x2c7b80u; *(void **)(manager+0x24)=cams[0]; *(void **)(manager+0x28)=cams[1];
    *(void **)(scene+0x40)=render_obj; speech_manager=manager; speech_render_obj=render_obj;
    *(void **)(image+0x409d7cu)=manager; *(void **)(image+0x408d58u)=scene;
    *(void **)(manager+0x20)=cams[0]; *(void **)(render_obj+0x7c)=renders[0];
    assert(SudekiMpLanStoryClientCameraInstall((HMODULE)image));
    assert(SudekiMpLanStoryClientCameraSpeechObserve()==SUDEKIMP_SPEECH_CAMERA_DEFAULT && SudekiMpLanStoryClientCameraSpeechReady());
    assert(SudekiMpLanStoryClientCameraUninstall());
    for(unsigned i=0;i<sizeof(guards)/sizeof(*guards);++i) for(unsigned j=0;j<guards[i].count;++j) {
        image[guards[i].rva+j]^=1u;
        assert(SudekiMpLanStoryClientCameraInstall((HMODULE)image));          /* the module itself still installs */
        assert(!SudekiMpLanStoryClientCameraSpeechReady() &&
            SudekiMpLanStoryClientCameraSpeechObserve()==SUDEKIMP_SPEECH_CAMERA_UNKNOWN);
        assert(SudekiMpLanStoryClientCameraUninstall());
        image[guards[i].rva+j]^=1u;
    }
    assert(SudekiMpLanStoryClientCameraInstall((HMODULE)image));
    /* SPEECH: both the manager and the renderer slot name the speech camera. */
    *(void **)(manager+0x20)=cams[1]; *(void **)(render_obj+0x7c)=renders[1];
    assert(SudekiMpLanStoryClientCameraSpeechObserve()==SUDEKIMP_SPEECH_CAMERA_SPEECH && !SudekiMpLanStoryClientCameraSpeechReady());
    assert(!SudekiMpLanStoryClientCameraUninstall());                          /* refused while selected */
    *(void **)(render_obj+0x7c)=renders[0]; assert(SudekiMpLanStoryClientCameraSpeechObserve()==SUDEKIMP_SPEECH_CAMERA_UNKNOWN);
    *(void **)(render_obj+0x7c)=renders[1];
    *(void **)(cams[1]+0x3c)=cams[0]; assert(SudekiMpLanStoryClientCameraSpeechObserve()==SUDEKIMP_SPEECH_CAMERA_UNKNOWN);
    *(void **)(cams[1]+0x3c)=NULL;
    *(void **)(manager+0x2c)=cams[1]; assert(SudekiMpLanStoryClientCameraSpeechObserve()==SUDEKIMP_SPEECH_CAMERA_UNKNOWN); /* duplicate */
    *(void **)(manager+0x2c)=NULL;
    /* Return: one SetRenderCamera("default"), then DEFAULT. */
    uint8_t saved[5]; memcpy(saved,image+0x36fb0u,5); DWORD old_protect;
    /* The mapped image is not executable; only the patched entry runs, for this call. */
    assert(VirtualProtect(image+0x36fb0u,5,PAGE_EXECUTE_READWRITE,&old_protect));
    image[0x36fb0u]=0xe9; int32_t rel=(int32_t)((uint8_t *)(uintptr_t)stub_set_render-(image+0x36fb0u+5)); memcpy(image+0x36fb1u,&rel,4);
    assert(FlushInstructionCache(GetCurrentProcess(),image+0x36fb0u,5));
    assert(SudekiMpLanStoryClientCameraSpeechReturn() && set_render_calls==1 &&
        SudekiMpLanStoryClientCameraSpeechObserve()==SUDEKIMP_SPEECH_CAMERA_DEFAULT);
    assert(SudekiMpLanStoryClientCameraSpeechReturn() && set_render_calls==1);      /* already back: no call */
    memcpy(image+0x36fb0u,saved,5); assert(FlushInstructionCache(GetCurrentProcess(),image+0x36fb0u,5));
    assert(VirtualProtect(image+0x36fb0u,5,old_protect,&old_protect));
    assert(SudekiMpLanStoryClientCameraUninstall());
    *(void **)(image+0x409d7cu)=NULL; *(void **)(image+0x408d58u)=NULL;
    puts("PASS SpeechCamera guards, observe (default/speech/unknown), uninstall refusal, single return call");
}
int main(int argc,char **argv) {
    setvbuf(stdout,NULL,_IONBF,0);
    assert(argc==2);wchar_t path[1024];
    assert(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,argv[1],-1,path,1024));
    uint8_t *image=map_image(path);
    assert(SudekiMpLanStoryClientCameraInstall((HMODULE)image));
    assert(!SudekiMpLanStoryClientCameraInstall((HMODULE)image)); /* once */
    assert(SudekiMpLanStoryClientCameraUninstall());
    const struct {const char *name;unsigned rva,count;} guards[]={
        {"CameraNodeUpdate",0xe7660u,12u},{"CameraInputEvent",0xe85f0u,12u},
        {"SetCameraState",0x37530u,12u},{"CanSetCameraState",0x37c30u,12u},{"NodeVtableUpdateSlot",0x2cce70u,4u}};
    for(unsigned i=0;i<sizeof(guards)/sizeof(*guards);++i) {
        for(unsigned j=0;j<guards[i].count;++j) {
            image[guards[i].rva+j]^=1u;
            assert(!SudekiMpLanStoryClientCameraInstall((HMODULE)image));
            image[guards[i].rva+j]^=1u;
            assert(SudekiMpLanStoryClientCameraInstall((HMODULE)image) && SudekiMpLanStoryClientCameraUninstall());
        }
        printf("native guard %s: supported bytes pass, every checked byte mutation refused\n",guards[i].name);
    }
    assert(SudekiMpLanStoryClientCameraInstall((HMODULE)image));
    /* A lease that is not the exact default camera never reaches native code. */
    SudekiMpLanArenaOwnerViewLease lease={0};
    assert(!SudekiMpLanStoryClientCameraTick(&lease,0.016f,0,0));
    assert(!SudekiMpLanStoryClientCameraTick(NULL,0.016f,0,0));
    uint8_t camera[0x110]={0},render[0xe0]={0},manager[0x60]={0};
    *(void **)camera=image+0x2cce5cu;*(void **)(camera+8)=image+0x2cce6cu;*(void **)(camera+0x34)=render;
    *(void **)render=image+0x2dd638u;*(void **)manager=image+0x2c7b80u;*(void **)(manager+0x20)=camera;
    lease.valid=TRUE;lease.camera=camera;lease.render_state=render;
    *(void **)(image+0x409d7cu)=manager;
    /* Not in Exploration and not named "default": refused before any state request. */
    assert(!SudekiMpLanStoryClientCameraTick(&lease,0.016f,0,0));
    strcpy((char *)(camera+0x4c),"other");
    assert(!SudekiMpLanStoryClientCameraTick(&lease,0.016f,0,0));
    /* House static camera: refused for a non-default camera, a bad lease or
     * a mutated SetStaticCameraPosition entry, before any native call. */
    assert(!SudekiMpLanStoryClientCameraHouseStatic(&lease,"NB_Kamo_Shop_Cam") && !resource_calls);
    strcpy((char *)(camera+0x4c),"default");
    image[0x38500]^=1; assert(!SudekiMpLanStoryClientCameraHouseStatic(&lease,"NB_Kamo_Shop_Cam") && !resource_calls); image[0x38500]^=1;
    assert(!SudekiMpLanStoryClientCameraHouseStatic(NULL,"NB_Kamo_Shop_Cam") && !SudekiMpLanStoryClientCameraHouseStatic(&lease,""));
    assert(!SudekiMpLanStoryClientCameraHouseStatic(&lease,"NB_Kamo_Shop_Cam") && resource_calls==1); /* name helper refused */
    puts("story client camera house static: refusals before any native call");
    assert(SudekiMpLanStoryClientCameraUninstall());
    continuity_preflight(image);
    speech_camera(image);
    puts("story client camera exact-image guards and lease refusals passed (no native tick)");
    VirtualFree(image,0,MEM_RELEASE);return 0;
}
