#include "loader/lobby_launch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    SudekiMpLobbyLaunchPlan p={.revision=1,.generation=19,.seat=0,
        .members=3,.reserved_mask=7,.character={2,1,0,4},.nonce={101,102,0,0}};
    assert(SudekiMpLobbyLaunchPlanValid(&p));
    /* Offline player2 keeps Buki but gets no load ticket/ACK obligation. */
    assert(!(p.members&4) && (p.reserved_mask&4) && p.character[2]==0 && !p.nonce[2]);
    p.nonce[2]=103; assert(!SudekiMpLobbyLaunchPlanValid(&p)); p.nonce[2]=0;
    p.reserved_mask=3; assert(!SudekiMpLobbyLaunchPlanValid(&p)); p.reserved_mask=7;
    p.character[2]=1; assert(!SudekiMpLobbyLaunchPlanValid(&p)); p.character[2]=0;
    p.character[2]=4; assert(!SudekiMpLobbyLaunchPlanValid(&p)); p.character[2]=0;
    p.members=7; assert(!SudekiMpLobbyLaunchPlanValid(&p)); p.members=3;
    p.reserved_mask=0; assert(!SudekiMpLobbyLaunchPlanValid(&p)); p.reserved_mask=7;
    p.seat=2; assert(!SudekiMpLobbyLaunchPlanValid(&p));

    /* A fresh client may join while host spectates; offline reservations still
     * remain, and the host connection remains a member without a human claim. */
    p.seat=1; p.port=26780; strcpy(p.host_ipv4,"127.0.0.1");
    p.nonce[0]=0; p.character[0]=4; p.reserved_mask=6;
    assert(SudekiMpLobbyLaunchPlanValid(&p));
    p.members=2; assert(!SudekiMpLobbyLaunchPlanValid(&p)); p.members=3;
    p.character[1]=4; p.reserved_mask=4; assert(!SudekiMpLobbyLaunchPlanValid(&p));
    assert(!SudekiMpLobbyLaunchPlanValid(NULL));
    puts("PASS: connected load members remain separate from offline character reservations");
    return 0;
}
