/* Retained lease policy only: synthetic controller/actor/AI memory and a
 * tiny fixture-owned x86 mode setter, never a Sudeki image or native task. */
#include "../src/hooks/lan_party_local_control.c"
#include <assert.h>
#include <stdio.h>

static SudekiMpControlUpdateDispatchWitness witness;
static SudekiMpLanStoryNativeRoster current_roster;
static BOOL roster_exact=TRUE,witness_exact=TRUE,fenced=TRUE,drained=TRUE;
static unsigned mode_calls,fail_after_call,party_observations,rotations;
BOOL SudekiMpCheckLoadedExecutable(HMODULE image) { (void)image; return FALSE; }
BOOL SudekiMpControlSeparationUpdateDispatchWitnessStillExact(
    const SudekiMpControlUpdateDispatchWitness *w) {
    return witness_exact && w==&witness && w->service_post_original_exact && w->dispatch_serial;
}
BOOL SudekiMpLanStoryObserverRosterStillExact(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r) {
    return roster_exact && SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) &&
        (!fail_after_call || mode_calls!=fail_after_call) &&
        !memcmp(r,&current_roster,sizeof(*r));
}
BOOL SudekiMpLanPartyControlObserveRoster(const SudekiMpControlUpdateDispatchWitness *w,
    SudekiMpLanPartyRosterObservation *r) {
    (void)w; (void)r; ++party_observations; return FALSE;
}
BOOL SudekiMpLanPartyControlNativeActorExact(const SudekiMpLanPartyRosterObservation *r,unsigned seat) {
    (void)r; (void)seat; return FALSE;
}
BOOL SudekiMpLanPartyControlLocalSwitchReady(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyRosterObservation *r) { (void)w; (void)r; return FALSE; }
BOOL SudekiMpLanPartyControlGameplayReady(const SudekiMpControlUpdateDispatchWitness *w) {
    (void)w; return FALSE;
}
BOOL SudekiMpLanPartyControlRebindLocal(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyRosterObservation *r,unsigned c) { (void)w; (void)r; (void)c; return FALSE; }
BOOL SudekiMpLanArenaCampaignGuardSwitchCharacter(void *group,BOOL previous) {
    (void)group; (void)previous; ++rotations; return FALSE;
}
static BOOL fence(const SudekiMpLanPartyLease *key,void *actor,
    const SudekiMpControlUpdateDispatchWitness *w) {
    return fenced && key && key->seat==current_roster.leader_character &&
        actor==current_roster.actors[key->seat] && w==&witness;
}
static BOOL drain(const SudekiMpLanPartyLease *key,void *actor,
    const SudekiMpControlUpdateDispatchWitness *w) {
    return drained && fence(key,actor,w);
}
int main(void) {
    uint32_t actor_words[0x98u/4u]={0},ai_words[0x174u/4u]={0};
    uint32_t controller_words[0x24cu/4u]={0},group_words[0xd8u/4u]={0};
    uint32_t mode_words[3]={0},other_mode_words[3]={0},directory[5]={0},other_directory[5]={0};
    uint32_t state_a[4]={0},state_b[4]={0},world[4]={0},descriptor[4]={0},other_world[4]={0};
    uint8_t *actor=(uint8_t *)actor_words,*ai=(uint8_t *)ai_words,*mode=(uint8_t *)mode_words;
    uint8_t *controller=(uint8_t *)controller_words,*group=(uint8_t *)group_words;
    uint8_t *image=VirtualAlloc(NULL,0x40a000u,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    assert(image);
    /* Fixture ABI: ECX=AI, AL=enabled, set mode+0B, increment call counter.
     * This is synthetic machine code, not copied retail instructions. */
    const uint8_t fixture_code[]={0x8b,0x51,0x3c,0x88,0x42,0x0b,0xff,0x05,0,0,0,0,0xc3};
    memcpy(image+AI_MODE,fixture_code,sizeof(fixture_code));
    uint32_t counter=(uint32_t)(uintptr_t)&mode_calls; memcpy(image+AI_MODE+8,&counter,4);
    assert(FlushInstructionCache(GetCurrentProcess(),image+AI_MODE,AI_MODE_SIZE));
    memcpy(verified_mode_code,image+AI_MODE,sizeof(verified_mode_code));
    *(void **)actor=image+0x2d5010u; *(void **)(actor+0x94u)=ai;
    *(void **)ai=image+0x2d4924u; *(void **)(ai+0x10u)=actor; *(void **)(ai+0x3cu)=mode;
    *(void **)(ai+0x16cu)=state_a; *(void **)(ai+0x170u)=state_b;
    *(void **)mode=image+0x2da340u;
    void *mode_vtable=image+0x2da340u; memcpy(other_mode_words,&mode_vtable,sizeof(mode_vtable));
    *(void **)(controller+0x248u)=actor; *(void **)(group+0x90u)=actor;
    *(void **)(image+DIRECTORY)=directory;
    witness=(SudekiMpControlUpdateDispatchWitness){.dispatch_serial=77,.service_post_original_exact=1};
    current_roster=(SudekiMpLanStoryNativeRoster){.dispatch_serial=77,.epoch=9,.revision=4,
        .available_mask=4,.leader_character=2,.world=world,.descriptor=descriptor,
        .group=group,.controller=controller,.actors={NULL,NULL,actor,NULL},.ai={NULL,NULL,ai,NULL}};
    SudekiMpLanPartyLease key={.seat=2,.token=123,.generation=8};
    /* Production installation still requires the exact image/function digest.
     * Only this fixture seeds the already-verified policy dependencies. */
    game_base=image; story_route=TRUE; fence_probe=fence; drain_probe=drain;
    assert(!SudekiMpLanPartyLocalControlSetAi(&witness,&key,actor,TRUE));
    assert(!party_observations);
    unsigned selected=0;
    assert(!SudekiMpLanPartyLocalControlSwitchStep(&witness,&key,3,&selected));
    assert(selected==4 && !rotations && !mode_calls);
    SudekiMpLanPartyLease wrong=key; wrong.seat=3;
    assert(!SudekiMpLanPartyLocalControlStorySetAi(&witness,&wrong,&current_roster,TRUE));
    wrong=key; wrong.token=0;
    assert(!SudekiMpLanPartyLocalControlStorySetAi(&witness,&wrong,&current_roster,TRUE));
    fenced=FALSE;
    assert(!SudekiMpLanPartyLocalControlStorySetAi(&witness,&key,&current_roster,TRUE));
    fenced=TRUE; drained=FALSE;
    assert(!SudekiMpLanPartyLocalControlStorySetAi(&witness,&key,&current_roster,TRUE));
    drained=TRUE; *(int16_t *)(ai+0x16au)=1;
    assert(!SudekiMpLanPartyLocalControlStorySetAi(&witness,&key,&current_roster,TRUE));
    assert(*(int16_t *)(ai+0x16au)==1 && !mode_calls); /* no foreign override decrement */
    *(int16_t *)(ai+0x16au)=0; mode[0xbu]=1;
    assert(!SudekiMpLanPartyLocalControlStorySetAi(&witness,&key,&current_roster,TRUE));
    assert(!SudekiMpLanPartyLocalControlRetains() && !mode_calls); /* foreign AI owner */
    mode[0xbu]=0; roster_exact=FALSE;
    assert(!SudekiMpLanPartyLocalControlStorySetAi(&witness,&key,&current_roster,TRUE));
    roster_exact=TRUE; image[AI_MODE+3]^=1;
    assert(!SudekiMpLanPartyLocalControlStorySetAi(&witness,&key,&current_roster,TRUE));
    image[AI_MODE+3]^=1;
    fail_after_call=1;
    assert(!SudekiMpLanPartyLocalControlStorySetAi(&witness,&key,&current_roster,TRUE));
    assert(mode_calls==1 && mode[0xbu]==1 && lease.phase==LOCAL_ENABLING);
    assert(SudekiMpLanPartyLocalControlRetains() && !SudekiMpLanPartyLocalControlUninstall());
    fail_after_call=0;
    assert(SudekiMpLanPartyLocalControlStorySetAi(&witness,&key,&current_roster,TRUE));
    assert(mode_calls==1 && SudekiMpLanPartyLocalControlStoryAiExact(&witness,&key,&current_roster));
    ++current_roster.epoch;
    assert(!SudekiMpLanPartyLocalControlStorySetAi(&witness,&key,&current_roster,FALSE));
    assert(!SudekiMpLanPartyLocalControlStoryAiExact(&witness,&key,&current_roster));
    --current_roster.epoch; current_roster.world=other_world;
    assert(!SudekiMpLanPartyLocalControlStorySetAi(&witness,&key,&current_roster,FALSE));
    current_roster.world=world; *(void **)(image+DIRECTORY)=other_directory;
    assert(!SudekiMpLanPartyLocalControlStorySetAi(&witness,&key,&current_roster,FALSE));
    *(void **)(image+DIRECTORY)=directory; *(void **)(ai+0x3cu)=other_mode_words;
    ((uint8_t *)other_mode_words)[0xbu]=1;
    assert(!SudekiMpLanPartyLocalControlStorySetAi(&witness,&key,&current_roster,FALSE));
    *(void **)(ai+0x3cu)=mode;
    assert(mode_calls==1 && SudekiMpLanPartyLocalControlRetains());
    fenced=FALSE; assert(!SudekiMpLanPartyLocalControlStoryAiExact(&witness,&key,&current_roster));
    assert(!SudekiMpLanPartyLocalControlStorySetAi(&witness,&key,&current_roster,FALSE));
    fenced=TRUE; drained=FALSE;
    assert(!SudekiMpLanPartyLocalControlStorySetAi(&witness,&key,&current_roster,FALSE));
    drained=TRUE; fail_after_call=2;
    assert(!SudekiMpLanPartyLocalControlStorySetAi(&witness,&key,&current_roster,FALSE));
    assert(mode_calls==2 && mode[0xbu]==0 && lease.phase==LOCAL_RESTORING);
    assert(!SudekiMpLanPartyLocalControlUninstall());
    fail_after_call=0;
    assert(SudekiMpLanPartyLocalControlStorySetAi(&witness,&key,&current_roster,FALSE));
    assert(mode_calls==2 && !SudekiMpLanPartyLocalControlRetains());
    assert(*(void **)(controller+0x248u)==actor && *(void **)(group+0x90u)==actor && !rotations);
    assert(SudekiMpLanPartyLocalControlUninstall());
    assert(!story_route && !game_base && !fence_probe && !drain_probe);
    assert(VirtualFree(image,0,MEM_RELEASE));
    puts("story host AI sparse-leader lease, refusal, retained transition and restore tests passed (synthetic)");
    return 0;
}
