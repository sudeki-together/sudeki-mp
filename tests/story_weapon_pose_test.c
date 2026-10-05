/* Synthetic owned-storage test of the real story weapon adapter. Native
 * setters are inert fixture callbacks: this is not live gameplay proof. */
#include "../src/hooks/lan_story_replica.c"
#include <assert.h>
#include <stdio.h>

static unsigned selector_calls,time_calls,proof_calls,deny_at;
static BOOL resident=TRUE;
static void pointer(uint8_t *p,unsigned n,void *v) { memcpy(p+n,&v,4); }
static void word(uint8_t *p,unsigned n,uint32_t v) { memcpy(p+n,&v,4); }
static void number(uint8_t *p,unsigned n,float v) { memcpy(p+n,&v,4); }
static void jump(unsigned rva,void *function) {
    base[rva]=0xe9; word(base,rva+1u,(uint32_t)((uint8_t *)function-(base+rva+5u)));
}
static void __attribute__((thiscall)) select_clip(void *renderer,int channel,unsigned sub,int value) {
    assert(channel==0); ++selector_calls;
    uint8_t *row=**(uint8_t ***)((uint8_t *)renderer+0x98u)+sub*24u;
    *(uint16_t *)row=(uint16_t)value;
}
static void __attribute__((thiscall)) seek_clip(void *renderer,int channel,unsigned sub,float value,int events) {
    assert(channel==0 && value==0 && events==0); ++time_calls;
    uint8_t *row=**(uint8_t ***)((uint8_t *)renderer+0x98u)+sub*24u;
    number(row,8,value); number(row,0xc,value);
}
SudekiMpLanStoryResidencyState SudekiMpLanStoryResidencyInspect(void *renderer,void *bank,
    unsigned selector,SudekiMpLanStoryResidencyReport *report) {
    (void)report; assert(*(void **)((uint8_t *)renderer+8u)==bank);
    assert(selector==0 || selector==3);
    return resident?SUDEKIMP_STORY_RESIDENCY_READY:SUDEKIMP_STORY_RESIDENCY_NEEDS_LOAD;
}
static BOOL exact_fixture(const SudekiMpLanStoryNativeRoster *r,void *context) {
    assert(r->actors[2]==context); ++proof_calls;
    return !deny_at || proof_calls<deny_at;
}
int main(void) {
    base=VirtualAlloc(NULL,0x410000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE); assert(base);
    for(unsigned i=0;i<sizeof(identities)/sizeof(identities[0]);++i)
        pointer(base,0x2df8ec+identities[i].slot,base+identities[i].rva);
    pointer(base,0xd9218,base+0xd91f6); pointer(base,0xd9224,base+0xd91de);
    base[0xd91de]=0xb8; word(base,0xd91df,3); base[0xd91e3]=0xc3;
    jump(0x223000,select_clip); jump(0x223180,seek_clip);
    uint8_t actor[0x138]={0},weapon[0x3bc]={0},item[0x18]={0},wrapper[0x14]={0};
    uint8_t object[0xd0]={0},renderer[0xb0]={0},bank[0x5c]={0},description[0x18]={0};
    uint8_t entries[4*28]={0},resources[4][0x20]={{0}},channels[36]={0},rows[48]={0};
    pointer(actor,0,base+0x2d5010); pointer(actor,0xc0,weapon); pointer(weapon,0x10,actor);
    pointer(weapon,0x268,item); word(item,0x14,7); word(weapon,0x330,3);
    pointer(weapon,0xf4,wrapper); pointer(wrapper,8,object);
    pointer(wrapper,0xc,renderer); pointer(wrapper,0x10,renderer);
    pointer(object,0,base+0x2dd700); pointer(object,0x14,renderer);
    pointer(renderer,0,base+0x2df8ec); pointer(renderer,8,bank); word(renderer,0xa0,1);
    pointer(bank,0x1c,description); pointer(bank,0x20,entries);
    word(description,0,4); word(description,0xc,2);
    pointer(renderer,0x98,channels); pointer(channels,0,rows);
    for(unsigned i=0;i<4;++i) {
        pointer(entries,i*28,resources[i]); word(resources[i],0,100+i); number(resources[i],4,2);
    }
    for(unsigned i=0;i<2;++i) { word(rows,i*24,3); number(rows,i*24+4,24); }
    SudekiMpLanStoryNativeRoster roster={0}; roster.actors[2]=actor;
    SudekiMpLanStoryActor state={.character=2,.weapon_item_plus_one=8,.weapon_visible=1,.weapon_attachment={1,0}};
    assert(tal_weapon_pose(&roster,&state,exact_fixture,actor));
    assert(selector_calls==2 && time_calls==2 && *(uint16_t *)rows==0 && *(uint16_t *)(rows+24)==0);
    assert(*(unsigned *)(weapon+0x330)==3); /* native combat state stays untouched */
    assert(tal_weapon_pose(&roster,&state,exact_fixture,actor) && selector_calls==2);
    word(rows,0,128u<<16); /* native settled/processed pose is still owned */
    assert(tal_weapon_pose(&roster,&state,exact_fixture,actor) && selector_calls==2);
    word(rows,0,0);
    state.weapon_attachment[0]=2;
    assert(tal_weapon_pose(&roster,&state,exact_fixture,actor) && selector_calls==4);
    assert(*(uint16_t *)rows==3 && *(uint16_t *)(rows+24)==3);
    state.weapon_attachment[0]=1; resident=FALSE;
    assert(!tal_weapon_pose(&roster,&state,exact_fixture,actor) && selector_calls==4);
    resident=TRUE; pointer(weapon,0x26c,item);
    assert(!tal_weapon_pose(&roster,&state,exact_fixture,actor) && selector_calls==4);
    pointer(weapon,0x26c,NULL); word(weapon,0x330,1);
    assert(!tal_weapon_pose(&roster,&state,exact_fixture,actor)); word(weapon,0x330,3);
    word(rows,0,1); assert(!tal_weapon_pose(&roster,&state,exact_fixture,actor)); word(rows,0,3);
    pointer(object,0x14,NULL); assert(!tal_weapon_pose(&roster,&state,exact_fixture,actor)); pointer(object,0x14,renderer);
    word(description,0xc,33); assert(!tal_weapon_pose(&roster,&state,exact_fixture,actor)); word(description,0xc,2);
    state.weapon_item_plus_one=9; assert(!tal_weapon_pose(&roster,&state,exact_fixture,actor)); state.weapon_item_plus_one=8;
    pointer(base,0xd9224,base+0xd91f6); assert(!tal_weapon_pose(&roster,&state,exact_fixture,actor)); pointer(base,0xd9224,base+0xd91de);
    deny_at=proof_calls+1; assert(!tal_weapon_pose(&roster,&state,exact_fixture,actor) && selector_calls==4);
    deny_at=proof_calls+3; /* loss after selector: no seek, no fabricated success */
    assert(!tal_weapon_pose(&roster,&state,exact_fixture,actor) && selector_calls==5 && time_calls==4);
    VirtualFree(base,0,MEM_RELEASE); base=NULL;
    puts("story Tal weapon pose synthetic tests passed"); return 0;
}
