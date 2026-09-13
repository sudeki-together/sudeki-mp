#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../src/hooks/lan_arena_skill_fade.c"
void SudekiMpLogWrite(const char *message) { (void)message; }
static int failures, draws, writes;
#define CHECK(x) do { if(!(x)) { printf("FAIL %d: %s\n",__LINE__,#x); ++failures; } } while(0)
static uint8_t *image;
static uint8_t light[0x70], scene[0x50], device[4], device_vt[0x180];
static int renderer;
static float requested[3], expected[3];
static const float native_rgb[3]={.15f,.15f,.2f};
static BOOL admitted, replace_device;
static void write_group(int group,float r,float g,float b) {
    float rgb[3]={r,g,b}; CHECK(group==7); ++writes;
    memcpy(image+GROUP_RGB,rgb,12);
}
static BOOL witness(float rgb[3]) { memcpy(rgb,requested,12); return admitted; }
static void __attribute__((regparm(1))) draw(void *ptr) {
    CHECK(ptr==&renderer); ++draws;
    CHECK(!memcmp(image+GROUP_RGB,expected,12));
    CHECK(!memcmp(light+0x34,native_rgb,sizeof(native_rgb))); /* Native easing clock not touched. */
    if(replace_device) *(void **)(image+DEVICE_GLOBAL)=NULL;
}
int main(void) {
    int32_t delta;
    float normal[3]={.15f,.15f,.2f}, baseline[3], current[3];
    image=VirtualAlloc(NULL,0x45f000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    CHECK(image!=NULL); if(!image) return 1;
    memcpy(image+SET_GROUP,setter_prefix,sizeof(setter_prefix));
    memcpy(image+DRAW,draw_prefix,sizeof(draw_prefix));
    *(uint32_t *)(image+EXPORT_FUNCTION)=SET_GROUP;
    *(uint32_t *)(image+EXPORT_NAME)=0x310000;
    *(uint16_t *)(image+EXPORT_ORDINAL)=1008;
    strcpy((char *)image+0x310000,"?SetLightGroupMultiplier@@YAXHMMM@Z");
    image[DRAW_CALL]=0xe8;
    delta=(int32_t)((image+DRAW)-(image+DRAW_CALL+5));
    memcpy(image+DRAW_CALL+1,&delta,4);
    *(void **)(image+LIGHT_GLOBAL)=light; *(void **)light=image+LIGHT_VTABLE;
    memcpy(light+0x34,normal,12);
    *(float *)(light+0x50)=*(float *)(light+0x54)=*(float *)(light+0x58)=1.f;
    *(void **)(image+SCENE_GLOBAL)=scene; *(void **)scene=image+SCENE_VTABLE;
    *(void **)(scene+0x40)=&renderer;
    *(void **)(image+DEVICE_GLOBAL)=device; *(void **)device=device_vt;
    memcpy(image+GROUP_RGB,normal,12);
    CHECK(SudekiMpInstallLanArenaSkillFade((HMODULE)image,witness));
    original_draw=draw; set_group=write_group;
    CHECK(SudekiMpLanArenaReadSkillLight(current,baseline));
    CHECK(!memcmp(current,normal,12) && baseline[0]==1.f);
    memcpy(expected,normal,12);
    draw_local_view(&renderer); CHECK(draws==1 && writes==0);
    admitted=TRUE;
    requested[0]=requested[1]=requested[2]=1.f;
    memcpy(expected,requested,12);
    draw_local_view(&renderer); CHECK(draws==2 && writes==2 && !lease.valid);
    CHECK(!memcmp(image+GROUP_RGB,normal,12));
    requested[0]=NAN; memcpy(expected,normal,12);
    draw_local_view(&renderer); CHECK(writes==2 && !lease.valid);
    requested[0]=1.f; memcpy(expected,requested,12);
    replace_device=TRUE; draw_local_view(&renderer);
    CHECK(lease.valid && writes==3);
    CHECK(!SudekiMpUninstallLanArenaSkillFade() && draw_hook.installed);
    *(void **)(image+DEVICE_GLOBAL)=device;
    CHECK(SudekiMpUninstallLanArenaSkillFade() && !lease.valid && !draw_hook.installed);
    CHECK(!memcmp(image+GROUP_RGB,normal,12));
    memcpy(&delta,image+DRAW_CALL+1,4); CHECK(image+DRAW_CALL+5+delta==image+DRAW);
    image[SET_GROUP]^=1;
    CHECK(!SudekiMpInstallLanArenaSkillFade((HMODULE)image,witness));
    CHECK(!draw_hook.installed);
    image[SET_GROUP]^=1;
    image[EXPORT_FUNCTION]^=1;
    CHECK(!SudekiMpInstallLanArenaSkillFade((HMODULE)image,witness));
    image[EXPORT_FUNCTION]^=1;
    image[EXPORT_ORDINAL]^=1;
    CHECK(!SudekiMpInstallLanArenaSkillFade((HMODULE)image,witness));
    image[EXPORT_ORDINAL]^=1;
    image[0x310000]='!';
    CHECK(!SudekiMpInstallLanArenaSkillFade((HMODULE)image,witness));
    image[0x310000]='?';
    *(uint32_t *)(image+EXPORT_NAME)=UINT32_MAX;
    CHECK(!SudekiMpInstallLanArenaSkillFade((HMODULE)image,witness));
    VirtualFree(image,0,MEM_RELEASE);
    printf("Skill fade rendering tests: %s\n",failures ? "FAIL":"PASS");
    return failures ? 1:0;
}
