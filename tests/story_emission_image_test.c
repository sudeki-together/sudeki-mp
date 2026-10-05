/* Inert supported-image mapping plus synthetic observer inputs. No retail
 * constructor, controller, missile update, or other native method executes. */
#include "../src/hooks/lan_arena_ranged_aim.c"
#include "engine/build_identity.h"
#include <assert.h>
#include <stdio.h>
static unsigned emissions;
static void observe(void *actor,void *manager,const float origin[3],const float direction[3]) {
    assert(actor && manager && origin[0]==3 && direction[2]==1); ++emissions;
}
static BOOL no_actor(void *a,BOOL b,float d[3]) { (void)a;(void)b;(void)d;return FALSE; }
static BOOL no_target(void *a,float d[3]) { (void)a;(void)d;return FALSE; }
static BOOL no_fire(void *a,BOOL *held) { (void)a;(void)held;return FALSE; }
int main(int argc,char **argv) {
    assert(argc==2); wchar_t path[1024];
    assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024));
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(file!=INVALID_HANDLE_VALUE); DWORD n=GetFileSize(file,NULL),read=0;
    uint8_t *raw=malloc(n); assert(raw && ReadFile(file,raw,n,&read,NULL) && read==n); CloseHandle(file);
    IMAGE_DOS_HEADER *dos=(void *)raw;
    IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *image=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    assert(image); memcpy(image,raw,nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *sections=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        IMAGE_SECTION_HEADER *s=&sections[i];
        assert(s->PointerToRawData<=n && s->SizeOfRawData<=n-s->PointerToRawData);
        assert(s->VirtualAddress<=nt->OptionalHeader.SizeOfImage &&
            s->SizeOfRawData<=nt->OptionalHeader.SizeOfImage-s->VirtualAddress);
        memcpy(image+s->VirtualAddress,raw+s->PointerToRawData,s->SizeOfRawData);
    }
    free(raw);
    uint8_t launch[5],pose[5],sample[5],ours[5];
    memcpy(launch,image+AIM_CALL,5); memcpy(pose,image+POSE_CALL,5); memcpy(sample,image+BASE_SAMPLE_CALL,5);
    assert(SudekiMpLanAimObserveEmissionsInstall((HMODULE)image,observe));
    assert(memcmp(launch,image+AIM_CALL,5));
    assert(!memcmp(pose,image+POSE_CALL,5) && !memcmp(sample,image+BASE_SAMPLE_CALL,5));
    assert(!SudekiMpLanAimInstall((HMODULE)image,no_actor,no_target,no_fire));
    uint8_t actor[0xc0]={0},manager[0x14]={0},other[0xc0]={0};
    *(void **)(actor+0xbc)=manager; *(void **)manager=image+MISSILE_VT;
    *(void **)(manager+0x10)=actor;
    SudekiMpLanAimActors(NULL,actor);
    float origin[3]={3,4,5},direction[3]={0,0,1};
    apply_direction(manager,origin,direction); assert(emissions==1 && direction[2]==1);
    *(void **)(manager+0x10)=other; apply_direction(manager,origin,direction); assert(emissions==1);
    *(void **)(manager+0x10)=actor; *(void **)(actor+0xbc)=NULL;
    apply_direction(manager,origin,direction); assert(emissions==1);
    memcpy(ours,image+AIM_CALL,5); image[AIM_CALL]=0x90;
    assert(!SudekiMpLanAimUninstall()); /* retain on a foreign callsite */
    assert(!SudekiMpLanAimObserveEmissionsInstall((HMODULE)image,observe));
    memcpy(image+AIM_CALL,ours,5); assert(SudekiMpLanAimUninstall());
    assert(!memcmp(launch,image+AIM_CALL,5) && !memcmp(pose,image+POSE_CALL,5) && !memcmp(sample,image+BASE_SAMPLE_CALL,5));
    image[AIM_CALL-1]^=1;
    assert(!SudekiMpLanAimObserveEmissionsInstall((HMODULE)image,observe)); image[AIM_CALL-1]^=1;
    assert(SudekiMpLanAimInstall((HMODULE)image,no_actor,no_target,no_fire));
    assert(!SudekiMpLanAimObserveEmissionsInstall((HMODULE)image,observe));
    assert(SudekiMpLanAimUninstall());
    assert(!memcmp(launch,image+AIM_CALL,5) && !memcmp(pose,image+POSE_CALL,5) && !memcmp(sample,image+BASE_SAMPLE_CALL,5));
    assert(SudekiMpLanAimPoseOnlyInstall((HMODULE)image,no_actor));
    assert(!memcmp(launch,image+AIM_CALL,5) && memcmp(pose,image+POSE_CALL,5) &&
        !memcmp(sample,image+BASE_SAMPLE_CALL,5));
    assert(!SudekiMpLanAimObserveEmissionsInstall((HMODULE)image,observe));
    assert(!SudekiMpLanAimInstall((HMODULE)image,no_actor,no_target,no_fire));
    assert(!SudekiMpLanAimPoseOnlyInstall((HMODULE)image,no_actor));
    assert(!SudekiMpLanAimPoseWitness(NULL));
    pose_callbacks=1;pose_callback_thread=GetCurrentThreadId();
    assert(SudekiMpLanAimPoseWitness(NULL) && !SudekiMpLanAimUninstall());
    pose_callbacks=0;pose_callback_thread=0;
    memcpy(ours,image+POSE_CALL,5);image[POSE_CALL]=0x90;
    assert(!SudekiMpLanAimUninstall() && pose_only && aim_witness==no_actor);
    memcpy(image+POSE_CALL,ours,5);assert(SudekiMpLanAimUninstall() && !pose_only);
    assert(!memcmp(launch,image+AIM_CALL,5) && !memcmp(pose,image+POSE_CALL,5) && !memcmp(sample,image+BASE_SAMPLE_CALL,5));
    VirtualFree(image,0,MEM_RELEASE); puts("story passive emission exact-image tests passed"); return 0;
}
