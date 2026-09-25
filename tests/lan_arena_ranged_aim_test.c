#include "../src/hooks/lan_arena_ranged_aim.c"
#include <stdio.h>
#include <float.h>
static unsigned failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); ++failures; } } while(0)
static void *expected_actor;
static BOOL admitted;
static BOOL witness(void *actor,BOOL projectile,float direction[3]) {
    if(actor!=expected_actor || !projectile || !admitted) return FALSE;
    direction[0]=0; direction[1]=0.6f; direction[2]=0.8f; return TRUE;
}
int main(void) {
    uint8_t states[SAMPLE_CHANNELS][SAMPLE_STATE_BYTES];
    memset(states,0xcd,sizeof(states));
    sample_states(states,50,24.0f);
    for(unsigned ch=0;ch<SAMPLE_CHANNELS;++ch) {
        uint16_t selector;float phase;
        memcpy(&selector,states[ch],2);memcpy(&phase,states[ch]+12,4);
        CHECK(selector==50 && phase==24.0f);
        CHECK(states[ch][2]==0 && states[ch][23]==0);
    }
    sample_states(states,54,0);
    CHECK(!memcmp(states[0],states[1],SAMPLE_STATE_BYTES));
    CHECK(states[1][0]==54 && states[1][12]==0);
    float out[3],zero[3]={0};
    CHECK(!SudekiMpLanAimNormalize(zero,out));
    CHECK(!SudekiMpLanAimNormalize((float[]){NAN,0,1},out));
    CHECK(!SudekiMpLanAimNormalize((float[]){0,INFINITY,1},out));
    CHECK(!SudekiMpLanAimNormalize((float[]){1,1,1},out));
    CHECK(SudekiMpLanAimNormalize((float[]){0,1,0},out) && out[1]==1);
    CHECK(SudekiMpLanAimNormalize((float[]){0,-1,0},out) && out[1]==-1);
    float p[24]={0},c[24]={0},a[24]={0},saved[24];
    uint8_t mask[2]={0,1};
    p[11]=p[23]=c[11]=c[23]=a[11]=1;
    a[20]=sinf(0.4f); a[23]=cosf(0.4f);
    p[0]=7; p[12]=9; p[16]=1; memcpy(saved,p,sizeof(p));
    CHECK(SudekiMpLanAimOverlay(p,a,c,mask,2,0));
    CHECK(!memcmp(p,saved,sizeof(p)));
    CHECK(SudekiMpLanAimOverlay(p,a,c,mask,2,1));
    CHECK(!memcmp(p,saved,12*sizeof(float)) && !memcmp(p+12,saved+12,8*sizeof(float)));
    CHECK(fabsf(p[20]-sinf(0.4f))<0.00001f && fabsf(p[23]-cosf(0.4f))<0.00001f);
    memcpy(p,saved,sizeof(p));
    CHECK(SudekiMpLanAimOverlay(p,a,c,mask,2,2));
    CHECK(fabsf(p[20]-sinf(0.8f))<0.00001f);
    a[20]=NAN; memcpy(saved,p,sizeof(p));
    CHECK(!SudekiMpLanAimOverlay(p,a,c,mask,2,1) && !memcmp(p,saved,sizeof(p)));
    CHECK(!SudekiMpLanAimOverlay(p,a,c,mask,2,INFINITY));

    uint8_t *image=VirtualAlloc(NULL,0x400000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    uint8_t actor[0xc0]={0},manager[0x14]={0};
    CHECK(image!=NULL);
    aim_image=image; aim_thread=GetCurrentThreadId(); aim_witness=witness;
    expected_actor=actor; aim_actors[1]=actor;
    *(void **)manager=image+MISSILE_VT; *(void **)(manager+0x10)=actor;
    *(void **)(actor+0xbc)=manager;
    admitted=TRUE; memcpy(out,(float[]){0,0,1},12);
    apply_direction(manager,out);
    CHECK(fabsf(out[1]-0.6f)<0.00001f && fabsf(out[2]-0.8f)<0.00001f);
    admitted=FALSE; memcpy(out,(float[]){0,0,1},12); apply_direction(manager,out);
    CHECK(out[1]==0 && out[2]==1);
    admitted=TRUE; *(void **)(actor+0xbc)=NULL; apply_direction(manager,out);
    CHECK(out[1]==0 && out[2]==1);
    *(void **)(actor+0xbc)=manager; aim_actors[1]=NULL; apply_direction(manager,out);
    CHECK(out[1]==0 && out[2]==1);
    aim_actors[1]=actor; aim_thread=0; apply_direction(manager,out);
    CHECK(out[1]==0 && out[2]==1);
    aim_image=NULL; aim_witness=NULL; aim_actors[1]=NULL;
    VirtualFree(image,0,MEM_RELEASE);
    if(failures) return 1;
    puts("ranged aim tests passed: finite direction, additive pose, root preservation, stale ownership and thread rejection");
    return 0;
}
