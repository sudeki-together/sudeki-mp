/* Inert supported-image mapping plus synthetic observer inputs. No retail
 * constructor, controller, missile update, or other native method executes. */
#include "../src/hooks/lan_arena_ranged_aim.c"
#include "engine/build_identity.h"
#include "hooks/lan_story_muzzle.h"
#include <assert.h>
#include <stdio.h>
static unsigned emissions;
static BOOL redirect;
static void observe(void *actor,void *manager,const float origin[3],const float direction[3]) {
    assert(actor && manager && origin[0]==3 && (redirect?direction[0]==1:direction[2]==1)); ++emissions;
}
static BOOL remote_target(void *a,float target[3]) {
    if(!a || !redirect) return FALSE;
    target[0]=103;target[1]=4;target[2]=5;return TRUE;
}
static BOOL no_actor(void *a,BOOL b,float d[3]) { (void)a;(void)b;(void)d;return FALSE; }
static BOOL no_target(void *a,float d[3]) { (void)a;(void)d;return FALSE; }
static BOOL no_fire(void *a,BOOL *held) { (void)a;(void)held;return FALSE; }
static void identity_matrix(float *m) {memset(m,0,64);m[0]=m[5]=m[10]=m[15]=1;}
static void muzzle_geometry(uint8_t *image) {
    uint8_t actor[0x138]={0},weapon[0x270]={0},position[0xb8]={0},model[0x168]={0};
    uint8_t item[0x18]={0},db[160]={0},wrapper[2][0x14]={{0}},object[2][0xd0]={{0}};
    uint8_t instance[2][0x10]={{0}},bank[2][0x54]={{0}},header[2][0x18]={{0}};
    uint8_t records[2][0x50]={{0}},names[24]={0};void *groups[1]={names};float poses[2][3][16];
    *(void **)actor=image+0x2d555c;*(void **)(actor+0xc0)=weapon;
    *(void **)(actor+0x44)=position;*(void **)(actor+0x134)=model;
    *(void **)weapon=image+0x2d4d3c;*(void **)(weapon+0x10)=actor;
    *(void **)(weapon+0xd4)=position+4;*(void **)(weapon+0x268)=item;
    *(void **)position=image+0x2cdefc;*(void **)(position+0x10)=actor;
    *(void **)model=image+0x2d5464;*(void **)(model+0x10)=actor;
    *(unsigned *)(item+0x14)=12;*(void **)(db+12+12*4)=item;
    void *old_db=*(void **)(image+0x408d80);*(void **)(image+0x408d80)=db;
    *(void **)(position+0xb4)=wrapper[0];*(void **)(model+0x164)=wrapper[0];
    *(void **)(weapon+0xf4)=wrapper[1];*(void **)(object[1]+0x18)=object[0];
    for(unsigned i=0;i<2;++i) {
        *(void **)(wrapper[i]+8)=object[i];*(void **)(wrapper[i]+12)=instance[i];
        *(void **)(wrapper[i]+16)=instance[i];*(void **)object[i]=image+0x2dd700;
        *(void **)(object[i]+0x14)=instance[i];*(void **)instance[i]=image+0x2df8ec;
        *(void **)(instance[i]+8)=bank[i];*(void **)(instance[i]+12)=poses[i];
        *(void **)(bank[i]+0x1c)=header[i];*(void **)(bank[i]+0x50)=records[i];
        *(void **)(bank[i]+0x24)=groups;*(unsigned *)(header[i]+4)=3;
        *(unsigned *)(header[i]+0x14)=1;*(unsigned *)records[i]=2;
        identity_matrix((float *)(object[i]+0x90));
        for(unsigned j=0;j<3;++j)identity_matrix(poses[i][j]);
    }
    *(unsigned *)(names+16)=124205;
    poses[1][2][13]=.5f;poses[0][2][12]=1;poses[0][2][13]=2;
    ((float *)(object[1]+0x90))[12]=.25f;((float *)(object[0]+0x90))[12]=10;
    ((float *)(object[0]+0x90))[14]=30;
    SudekiMpStoryMuzzle tip={0};
    assert(SudekiMpStoryMuzzleObserve((HMODULE)image,actor,1,&tip));
    assert(tip.valid && tip.item==12 && tip.position[0]==11.25f && tip.position[1]==2.5f && tip.position[2]==30);
    /* Locator number 0 names bone 2, and static bind data deliberately remains
     * zero. Rotation of the weapon moves its tip with the visible wand. */
    float *m=(float *)(object[1]+0x90);m[0]=m[5]=0;m[1]=1;m[4]=-1;
    assert(SudekiMpStoryMuzzleObserve((HMODULE)image,actor,1,&tip));
    assert(tip.position[0]==10.75f && tip.position[1]==2);
    *(void **)(weapon+0xd4)=actor;assert(!SudekiMpStoryMuzzleObserve((HMODULE)image,actor,1,&tip));
    *(void **)(weapon+0xd4)=position+4;*(unsigned *)records[1]=3;
    assert(!SudekiMpStoryMuzzleObserve((HMODULE)image,actor,1,&tip));*(unsigned *)records[1]=2;
    *(unsigned *)(names+16)=0;assert(!SudekiMpStoryMuzzleObserve((HMODULE)image,actor,1,&tip));
    *(unsigned *)(names+16)=124205;*(void **)(weapon+0x26c)=item;
    assert(!SudekiMpStoryMuzzleObserve((HMODULE)image,actor,1,&tip));*(void **)(weapon+0x26c)=NULL;
    *(void **)(model+0x160)=wrapper[0];assert(!SudekiMpStoryMuzzleObserve((HMODULE)image,actor,1,&tip));
    *(void **)(model+0x160)=NULL;*(void **)actor=image+0x2d66fc;*(unsigned *)(item+0x14)=24;
    *(void **)(db+12+24*4)=item;assert(SudekiMpStoryMuzzleObserve((HMODULE)image,actor,0x0e,&tip));
    assert(tip.item==24);*(void **)(db+12+24*4)=NULL;
    assert(!SudekiMpStoryMuzzleObserve((HMODULE)image,actor,0x0e,&tip));
    *(void **)(image+0x408d80)=old_db;
}
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
    assert(SudekiMpStoryMuzzleImageMatches((HMODULE)image));
    muzzle_geometry(image);
    image[0x21bd20u+9u]^=1u;assert(!SudekiMpStoryMuzzleImageMatches((HMODULE)image));image[0x21bd20u+9u]^=1u;
    image[0xc79c0u+29u]^=1u;assert(!SudekiMpStoryMuzzleImageMatches((HMODULE)image));image[0xc79c0u+29u]^=1u;
    assert(SudekiMpStoryMuzzleImageMatches((HMODULE)image));
    assert(SudekiMpLanAimImageMatches((HMODULE)image));
    image[BASE_SAMPLE_CALL+1]^=1;
    assert(!SudekiMpLanAimImageMatches((HMODULE)image)); image[BASE_SAMPLE_CALL+1]^=1;
    image[POSE_CALL+1]^=1;
    assert(!SudekiMpLanAimImageMatches((HMODULE)image)); image[POSE_CALL+1]^=1;
    assert(SudekiMpLanAimImageMatches((HMODULE)image));
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
        memcmp(sample,image+BASE_SAMPLE_CALL,5));
    assert(!SudekiMpLanAimObserveEmissionsInstall((HMODULE)image,observe));
    assert(!SudekiMpLanAimInstall((HMODULE)image,no_actor,no_target,no_fire));
    assert(!SudekiMpLanAimPoseOnlyInstall((HMODULE)image,no_actor));
    assert(!SudekiMpLanAimPoseWitness(NULL));
    pose_callbacks=1;pose_callback_thread=GetCurrentThreadId();
    assert(SudekiMpLanAimPoseWitness(NULL) && !SudekiMpLanAimUninstall());
    pose_callbacks=0;pose_callback_thread=0;
    memcpy(ours,image+BASE_SAMPLE_CALL,5);image[BASE_SAMPLE_CALL]=0x90;
    pose_callbacks=1;pose_callback_thread=GetCurrentThreadId();
    assert(!SudekiMpLanAimPoseWitness(NULL));pose_callbacks=0;pose_callback_thread=0;
    assert(!SudekiMpLanAimUninstall() && sample_hook.installed && pose_hook.installed &&
        sample_original && pose_original && pose_only && aim_witness==no_actor);
    memcpy(image+BASE_SAMPLE_CALL,ours,5);
    memcpy(ours,image+POSE_CALL,5);image[POSE_CALL]=0x90;
    assert(!SudekiMpLanAimUninstall() && pose_only && aim_witness==no_actor);
    memcpy(image+POSE_CALL,ours,5);assert(SudekiMpLanAimUninstall() && !pose_only);
    assert(!memcmp(launch,image+AIM_CALL,5) && !memcmp(pose,image+POSE_CALL,5) && !memcmp(sample,image+BASE_SAMPLE_CALL,5));
    assert(SudekiMpLanAimObserveDirectedEmissionsInstall((HMODULE)image,observe,remote_target));
    *(void **)(actor+0xbc)=manager;SudekiMpLanAimActors(actor,NULL);
    direction[0]=direction[1]=0;direction[2]=1;redirect=TRUE;
    unsigned before=emissions;apply_direction(manager,origin,direction);
    assert(emissions==before+1 && direction[0]==1 && direction[2]==0);
    redirect=FALSE;direction[0]=0;direction[2]=1;
    apply_direction(manager,origin,direction);assert(emissions==before+2 && direction[2]==1);
    assert(SudekiMpLanAimUninstall() && emission_target==NULL);
    assert(!memcmp(launch,image+AIM_CALL,5));
    VirtualFree(image,0,MEM_RELEASE); puts("story passive emission exact-image tests passed"); return 0;
}
