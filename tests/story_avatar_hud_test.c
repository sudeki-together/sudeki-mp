/* Synthetic native-event fixture. It proves shadow isolation and admission;
 * no supported game executable or native HUD rendering executes here. */
#include "../src/hooks/lan_story_ally_hud.c"
#include <assert.h>
#include <stdio.h>

typedef struct AvatarFixture {
    uint8_t entity[0xbc],combo[0xb4],arbiter[0x64];
    uint32_t generation; BOOL valid; unsigned validations;
} AvatarFixture;
static AvatarFixture fixture[4];
static uint8_t *image;
static unsigned legacy_player;
static BOOL legacy_client;

BOOL SudekiMpLogResearchEnabled(void) { return FALSE; }
void SudekiMpLogFormat(const char *format,...) { (void)format; }
unsigned SudekiMpLanStoryAllySeatPlayer(void) { return legacy_player; }
BOOL SudekiMpLanStoryAllySeatClientEnabled(void) { return legacy_client; }
BOOL SudekiMpCheckLoadedExecutable(HMODULE module) { return module==(HMODULE)image; }
BOOL SudekiMpInstallInlineHook(SudekiMpInlineHook *hook,uint8_t *target,
    const uint8_t *expected,size_t length,const void *replacement) {
    assert(replacement && length<=sizeof(hook->original));
    assert(!memcmp(target,expected,length));
    hook->target=target; hook->length=length; memcpy(hook->original,expected,length);
    hook->trampoline=target; hook->installed=TRUE; return TRUE;
}
BOOL SudekiMpRestoreInlineHook(SudekiMpInlineHook *hook) {
    memset(hook,0,sizeof(*hook)); return TRUE;
}
static void put(void *p,unsigned offset,void *value) { *(void **)((uint8_t *)p+offset)=value; }
static BOOL exact(unsigned player,const void *entity,uint32_t generation,void *context) {
    assert(context==fixture && player<4u);
    AvatarFixture *a=&fixture[player]; ++a->validations;
    return a->valid && entity==a->entity && generation==a->generation;
}
static SudekiMpLanStoryControlFence fence(unsigned player) {
    SudekiMpLanStoryControlFence f={0};
    f.epoch=1; f.revision=2; f.transaction=3; f.actor_generation=fixture[player].generation;
    f.player=(uint8_t)player; f.character=SUDEKIMP_STORY_CHARACTER_ALLY; return f;
}
static SudekiMpLanStoryAllyHud snapshot(unsigned player) {
    SudekiMpLanStoryAllyHud hud; SudekiMpLanStoryControlFence f=fence(player);
    assert(SudekiMpLanStoryAllyHudSnapshotAvatar(player,fixture[player].entity,
        fixture[player].generation,&f,&hud));
    assert(hud.fence.player==player && hud.fence.actor_generation==fixture[player].generation);
    return hud;
}
static void accept_input(unsigned player,unsigned index,int kind,BOOL timed) {
    uint8_t *m=fixture[player].combo;
    *(int32_t *)(m+0x64)=index; *(int32_t *)(m+0x68+index*4)=kind; m[0xa0+index]=(uint8_t)timed;
    SetLastError(1234); observe_accept((uintptr_t)m); assert(GetLastError()==1234);
}
static void setup(void) {
    image=VirtualAlloc(NULL,SUDEKIMP_EXPECTED_IMAGE_SIZE,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    assert(image);
    memcpy(image+RVA_ACCEPT,accept_bytes,SEAM_LENGTH); memcpy(image+RVA_RESET,reset_bytes,SEAM_LENGTH);
    memcpy(image+RVA_COMPLETE,complete_bytes,SEAM_LENGTH); memcpy(image+RVA_HIT,hit_bytes,SEAM_LENGTH);
    memcpy(image+RVA_HUD_UPDATE,update_head,sizeof(update_head));
    put(image,RVA_HUD_UPDATE+8,image+UPDATE_OPERAND_RVA);
    assert(SudekiMpLanStoryAllyHudConfigureAvatars(TRUE));
    assert(SudekiMpLanStoryAllyHudInstall((HMODULE)image));
    assert(host_mode && client_mode && accept_hook.installed && update_hook.installed);
    assert(!SudekiMpLanStoryAllyHudConfigureAvatars(FALSE));
    for(unsigned p=0;p<4;++p) {
        AvatarFixture *a=&fixture[p]; a->generation=10+p; a->valid=TRUE;
        put(a->entity,0x90,a->arbiter); put(a->entity,0xb8,a->combo);
        put(a->combo,0,image+0x2d4bd4); put(a->combo,0x10,a->entity);
        put(a->arbiter,0,image+0x2cc9ac); put(a->arbiter,0x10,a->entity);
        *(uint32_t *)(a->arbiter+0x50)=2; *(uint32_t *)(a->arbiter+0x60)=2;
        assert(SudekiMpLanStoryAllyHudTrackAvatar(p,a->entity,a->generation,exact,fixture));
    }
}
static void isolated_native_events(void) {
    for(unsigned p=0;p<4;++p) {
        SudekiMpLanStoryAllyHud hud=snapshot(p);
        assert(hud.sequence==1 && hud.slots[0]==SLOT_EMPTY && (hud.flags&SUDEKIMP_STORY_ALLY_HUD_COMBAT));
        accept_input(p,0,p%2?2:1,p<2);
    }
    static const uint8_t expected[4]={0,1,2,3};
    for(unsigned p=0;p<4;++p) {
        SudekiMpLanStoryAllyHud hud=snapshot(p);
        assert(hud.sequence==2 && hud.slots[0]==expected[p] && hud.slots[1]==SLOT_EMPTY);
    }
    accept_input(1,1,2,FALSE); accept_input(1,2,1,TRUE);
    observe_complete((uintptr_t)(fixture[1].combo+0x3c),1);
    SudekiMpLanStoryAllyHud hud=snapshot(1);
    assert(hud.sequence==5 && hud.slots[0]==1 && hud.slots[1]==3 && hud.slots[2]==0);
    assert((hud.flags&(SUDEKIMP_STORY_ALLY_HUD_FULL|SUDEKIMP_STORY_ALLY_HUD_ALT_ICONS|
        SUDEKIMP_STORY_ALLY_HUD_FLASH|SUDEKIMP_STORY_ALLY_HUD_ARMED))==
        (SUDEKIMP_STORY_ALLY_HUD_FULL|SUDEKIMP_STORY_ALLY_HUD_ALT_ICONS|
        SUDEKIMP_STORY_ALLY_HUD_FLASH|SUDEKIMP_STORY_ALLY_HUD_ARMED));
    observe_reset((uintptr_t)fixture[1].combo); hud=snapshot(1);
    assert(hud.sequence==6 && hud.slots[0]==SLOT_EMPTY && !(hud.flags&SUDEKIMP_STORY_ALLY_HUD_ARMED));
    assert(hud.flags&SUDEKIMP_STORY_ALLY_HUD_FLASH);
    for(unsigned p=0;p<4;++p) if(p!=1) { hud=snapshot(p); assert(hud.sequence==2 && hud.slots[0]==expected[p]); }
}
static void stale_identity_and_components(void) {
    unsigned checks=fixture[2].validations;
    fixture[2].valid=FALSE; accept_input(2,1,2,TRUE);
    assert(fixture[2].validations==checks+1 && !avatars[2].entity);
    assert(avatars[2].shadow.sequence==1 && avatars[2].shadow.slots[0]==SLOT_EMPTY);
    assert(snapshot(3).sequence==2);
    fixture[2].valid=TRUE; ++fixture[2].generation;
    assert(SudekiMpLanStoryAllyHudTrackAvatar(2,fixture[2].entity,fixture[2].generation,exact,fixture));
    assert(snapshot(2).sequence==1);
    assert(!SudekiMpLanStoryAllyHudTrackAvatar(3,fixture[2].entity,fixture[2].generation,exact,fixture));
    SudekiMpLanStoryControlFence stale=fence(2); --stale.actor_generation;
    SudekiMpLanStoryAllyHud out; memset(&out,0xab,sizeof(out));
    assert(!SudekiMpLanStoryAllyHudSnapshotAvatar(2,fixture[2].entity,fixture[2].generation,&stale,&out));
    assert(!out.sequence && avatars[2].entity); /* Stale wire data cannot retire a fresh avatar. */
    put(fixture[2].entity,0xb8,fixture[3].combo); accept_input(2,0,1,TRUE);
    assert(!avatars[2].entity && snapshot(3).sequence==2);
    put(fixture[2].entity,0xb8,fixture[2].combo);
    ++fixture[1].generation;
    assert(SudekiMpLanStoryAllyHudTrackAvatar(1,fixture[1].entity,fixture[1].generation,exact,fixture));
    out=snapshot(1); assert(out.sequence==1 && out.slots[0]==SLOT_EMPTY && !(out.flags&SUDEKIMP_STORY_ALLY_HUD_FLASH));
    put(fixture[1].arbiter,0x10,fixture[0].entity);
    SudekiMpLanStoryControlFence f=fence(1);
    assert(!SudekiMpLanStoryAllyHudSnapshotAvatar(1,fixture[1].entity,fixture[1].generation,&f,&out));
    assert(!avatars[1].entity && snapshot(0).sequence==2);
}
static void legacy_and_presentation(void) {
    SudekiMpLanStoryAllyHudTrack(fixture[0].entity);
    accept_input(0,0,2,FALSE);
    SudekiMpLanStoryControlFence f=fence(0); SudekiMpLanStoryAllyHud old;
    assert(SudekiMpLanStoryAllyHudSnapshot(fixture[0].entity,&f,&old));
    assert(old.sequence==2 && old.slots[0]==3 && snapshot(0).sequence==3);
    SudekiMpLanStoryAllyHud hud=snapshot(0);
    SudekiMpLanStoryAllyHudClientPresent(&hud); assert(want_active && want.fence.player==0);
    applied_any=TRUE; SudekiMpLanStoryAllyHudClientPresent(&hud); assert(applied_any);
    ++hud.fence.actor_generation; SudekiMpLanStoryAllyHudClientPresent(&hud);
    assert(!applied_any && want.fence.actor_generation==hud.fence.actor_generation);
    SudekiMpLanStoryAllyHudClientPresent(NULL); assert(!want_active);
    assert(SudekiMpLanStoryAllyHudUninstall());
    for(unsigned p=0;p<4;++p) assert(!avatars[p].entity);
    assert(SudekiMpLanStoryAllyHudConfigureAvatars(FALSE));
    assert(SudekiMpLanStoryAllyHudInstall((HMODULE)image)); assert(!base);
    legacy_player=2;
    assert(SudekiMpLanStoryAllyHudInstall((HMODULE)image)); assert(host_mode && !client_mode);
    assert(!SudekiMpLanStoryAllyHudTrackAvatar(0,fixture[0].entity,fixture[0].generation,exact,fixture));
    assert(SudekiMpLanStoryAllyHudUninstall()); legacy_player=0; legacy_client=TRUE;
    assert(SudekiMpLanStoryAllyHudInstall((HMODULE)image)); assert(!host_mode && client_mode);
    assert(SudekiMpLanStoryAllyHudUninstall());
}
int main(void) {
    setup(); isolated_native_events(); stale_identity_and_components(); legacy_and_presentation();
    VirtualFree(image,0,MEM_RELEASE); puts("StoryAvatarHudTest PASS"); return 0;
}
