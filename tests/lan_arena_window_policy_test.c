#include <stdio.h>
#include "hooks/lan_arena_window_policy.c"

static unsigned int failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); ++failures; } } while(0)
static uint8_t controller[0x260], actor[0x140], group[0xd0], skill[0x78];
static void *task;
static BOOL active,replace_task,observer_known;
void SudekiMpLogWrite(const char *text) { (void)text; }
BOOL SudekiMpObserveCharacterSkill(void *a,SudekiMpCharacterSkillState *s) {
    if(a!=actor || !observer_known) return FALSE;
    memset(s,0,sizeof(*s)); s->skill=skill; s->active=active; return TRUE;
}
static void __attribute__((used,noinline)) fake_reset(void) {
    *(uint32_t *)(controller+0x1d0)=15;
    *(float *)(controller+0x1d8)=-1.f;
    *(int *)(controller+0x23c)=0;
    *(float *)(controller+0x1d4)=1.f;
    memset(controller+0x60,0,0x100); /* physical input stays released */
    if(replace_task) task=NULL;
}
static void setup(uint8_t *image,float remaining,int mode) {
    memset(controller,0,sizeof(controller));
    *(void **)(image+RVA_CONTROLLER_GLOBAL)=controller;
    *(void **)(image+RVA_GROUP_GLOBAL)=group;
    *(void **)(group+0x90)=actor; *(unsigned int *)(group+0xcc)=2;
    *(void **)(controller+0x248)=actor; *(void **)actor=image+0x2d5a88;
    *(uint32_t *)(controller+0x1d0)=13; *(float *)(controller+0x1d8)=remaining;
    *(int *)(controller+0x23c)=mode; *(float *)(controller+0x1d4)=4.f;
    controller[0x70]=1; task=actor; *(void **)(skill+0x74)=&task;
    active=observer_known=TRUE; replace_task=FALSE;
    focus_reset_base=image; original_focus_reset=fake_reset;
}
int main(void) {
    uint8_t *image=VirtualAlloc(NULL,0x409000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    CHECK(image!=NULL); if(!image) return 1;
    for(int mode=0;mode<=1;++mode) {
        setup(image,1.75f,mode); focus_reset_scoped(controller);
        CHECK(*(float *)(controller+0x1d8)==1.75f);
        CHECK(*(uint32_t *)(controller+0x1d0)==13);
        CHECK(*(int *)(controller+0x23c)==mode && !controller[0x70]);
    }
    setup(image,2.f,1); replace_task=TRUE; focus_reset_scoped(controller);
    CHECK(*(float *)(controller+0x1d8)==-1.f && *(uint32_t *)(controller+0x1d0)==15);
    setup(image,2.f,1); observer_known=FALSE; focus_reset_scoped(controller);
    CHECK(*(float *)(controller+0x1d8)==-1.f && !controller[0x70]);
    setup(image,2.f,1); active=FALSE; focus_reset_scoped(controller);
    CHECK(*(float *)(controller+0x1d8)==-1.f);
    setup(image,0.f,0); focus_reset_scoped(controller);
    CHECK(*(float *)(controller+0x1d8)==-1.f);
    setup(image,NAN,0); focus_reset_scoped(controller);
    CHECK(*(float *)(controller+0x1d8)==-1.f);
    setup(image,2.f,0); *(uint32_t *)(controller+0x1d0)=15; focus_reset_scoped(controller);
    CHECK(*(float *)(controller+0x1d8)==-1.f);
    VirtualFree(image,0,MEM_RELEASE);
    if(!failures) puts("LAN focus/cast timing tests passed");
    return failures ? 1:0;
}
