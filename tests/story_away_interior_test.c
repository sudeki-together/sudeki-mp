/* Actual H1b new-entry gate; no native entities or zone requests. */
#include "../src/hooks/lan_story_remote_interior.c"
#include <assert.h>
#include <stdio.h>
static unsigned away_mask;
BOOL SudekiMpLanStoryRuntimeCharacterAway(unsigned c) { return !!(away_mask&(1u<<c)); }
int main(void) {
    SudekiMpLanStoryNativeRoster r={.leader_character=0,.available_mask=15};
    SudekiMpLanStoryScene s={0};
    for(unsigned c=0;c<4;++c) {
        away_mask=0; assert(!!entry_member_admitted(c,&r,&s,15)==(c!=0));
        away_mask=1u<<c; assert(!entry_member_admitted(c,&r,&s,15));
        away_mask=0; assert(!entry_member_admitted(c,&r,&s,0));
        members[c].phase=LOADING; assert(!entry_member_admitted(c,&r,&s,15));
        members[c].phase=IDLE; s.inside_mask=1u<<c; assert(!entry_member_admitted(c,&r,&s,15)); s.inside_mask=0;
        r.available_mask=0; assert(!entry_member_admitted(c,&r,&s,15)); r.available_mask=15;
    }
    puts("H1b refuses new AI-away entries; returning IDLE member can enter"); return 0;
}
