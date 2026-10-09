/* Codec for the host area residency report (#42). */
#include "network/lan_story_area_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static SudekiMpStoryAreaState sample(void) {
    SudekiMpStoryAreaState s; memset(&s,0,sizeof(s));
    s.observed_tick=0x12345678u;
    strcpy(s.current,"WS_Country_SE_to_Bright_NE");
    strcpy(s.target,"Illumina_Countryside_SE");
    s.count=3;
    strcpy(s.entries[0].name,"Illumina_Countryside_SE"); s.entries[0].state=2;
    strcpy(s.entries[1].name,"WS_Country_Hub_SE_to_Country_SE"); s.entries[1].state=1;
    strcpy(s.entries[2].name,"WS_Country_SE_to_Bright_NE"); s.entries[2].state=3;
    return s;
}
int main(void) {
    uint8_t wire[SUDEKIMP_STORY_AREA_WIRE_SIZE];
    SudekiMpStoryAreaState s=sample(),d;
    assert(SUDEKIMP_STORY_AREA_WIRE_SIZE==413u && SUDEKIMP_STORY_AREA_WIRE_SIZE<1440u);
    assert(SudekiMpStoryAreaStateValid(&s));
    assert(SudekiMpStoryAreaStateEncode(&s,wire,sizeof(wire)));
    assert(SudekiMpStoryAreaStateDecode(wire,sizeof(wire),&d) && !memcmp(&s,&d,sizeof(s)));
    assert(!SudekiMpStoryAreaStateDecode(wire,sizeof(wire)-1u,&d));
    /* Same ignores the observation tick, not residency. */
    d.observed_tick++; assert(SudekiMpStoryAreaStateSame(&s,&d));
    d.entries[0].state=3; assert(!SudekiMpStoryAreaStateSame(&s,&d));
    /* Rejections: empty current, bad state, junk after a name, nonzero unused slot, bad bytes. */
    d=s; d.current[0]=0; assert(!SudekiMpStoryAreaStateValid(&d));
    d=s; d.entries[1].state=5; assert(!SudekiMpStoryAreaStateValid(&d));
    d=s; d.entries[1].state=0; assert(!SudekiMpStoryAreaStateValid(&d));
    d=s; d.target[30]='x'; assert(!SudekiMpStoryAreaStateValid(&d));
    d=s; d.entries[5].state=1; assert(!SudekiMpStoryAreaStateValid(&d));
    d=s; d.count=9; assert(!SudekiMpStoryAreaStateValid(&d));
    d=s; d.current[2]='\x07'; assert(!SudekiMpStoryAreaStateValid(&d));
    memset(d.target,'a',sizeof(d.target)); d=s; memset(d.target,'a',sizeof(d.target)); assert(!SudekiMpStoryAreaStateValid(&d));
    /* Empty target is allowed (no background load pending). */
    d=s; memset(d.target,0,sizeof(d.target)); assert(SudekiMpStoryAreaStateValid(&d));
    wire[4+2*SUDEKIMP_STORY_AREA_NAME]=9; assert(!SudekiMpStoryAreaStateDecode(wire,sizeof(wire),&d));
    puts("story area state tests passed");
    return 0;
}
