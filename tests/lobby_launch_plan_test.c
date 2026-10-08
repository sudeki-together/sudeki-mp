#include "loader/lobby_launch.h"
#include "network/title_lobby.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void native_projection(void) {
    SudekiMpLobbyLaunchPlan p={.revision=1,.generation=19,.seat=0,
        .members=15,.reserved_mask=15,.mode=SUDEKIMP_LOBBY_MODE_DEV_PLAY,
        .character={5,2,2,3},.nonce={101,102,103,104}};
    SudekiMpLobbyLaunchPlan original=p;
    uint8_t character[4]={0},mask=0;
    const uint8_t expected[4]={4,2,4,3};
    assert(SudekiMpLobbyLaunchProjectNative(&p,character,&mask));
    assert(mask==10 && !memcmp(character,expected,4) && !memcmp(&p,&original,sizeof(p)));
    assert(!SudekiMpLobbyLaunchPlanNativeReady(&p,2));
    assert(!SudekiMpLobbyLaunchPlanDevPlayReady(&p,2,15)); /* native leader claimed remotely */
    assert(SudekiMpLobbyLaunchPlanDevPlayReady(&p,0,15));
    p.character[0]=2; p.character[1]=3; p.character[2]=0; p.character[3]=1;
    assert(SudekiMpLobbyLaunchProjectNative(&p,character,&mask));
    assert(mask==15 && !memcmp(character,p.character,4));
    assert(SudekiMpLobbyLaunchPlanNativeReady(&p,2));
    assert(!SudekiMpLobbyLaunchPlanNativeReady(&p,3));
    assert(!SudekiMpLobbyLaunchPlanNativeReady(&p,4));
    assert(!SudekiMpLobbyLaunchPlanNativeReady(NULL,2));
    p.character[3]=0; assert(!SudekiMpLobbyLaunchPlanNativeReady(&p,2));
    assert(SudekiMpLobbyLaunchPlanDevPlayReady(&p,2,15)); /* duplicate hero spectates */
    assert(SudekiMpLobbyLaunchPlanDevPlayReady(&p,3,15)); /* present host rotates from saved leader */
    assert(!SudekiMpLobbyLaunchPlanDevPlayReady(&p,3,8)); /* chosen host absent */
    assert(SudekiMpLobbyLaunchPlanDevPlayReady(&p,3,12));
    assert(!SudekiMpLobbyLaunchPlanDevPlayReady(&p,3,4)); /* saved leader missing from fingerprint */
    assert(!SudekiMpLobbyLaunchPlanDevPlayReady(&p,3,0));
    assert(!SudekiMpLobbyLaunchPlanDevPlayReady(&p,3,16));
    for(unsigned leader=0;leader<4u;++leader) for(unsigned host=0;host<4u;++host) {
        SudekiMpLobbyLaunchPlan choice=p; choice.character[0]=(uint8_t)host;
        unsigned party=(1u<<leader)|(1u<<host);
        assert(SudekiMpLobbyLaunchPlanDevPlayReady(&choice,leader,party));
        if(host!=leader) assert(!SudekiMpLobbyLaunchPlanDevPlayReady(&choice,leader,1u<<leader));
    }
    p.character[0]=p.character[1]=p.character[2]=p.character[3]=5;
    assert(SudekiMpLobbyLaunchProjectNative(&p,character,&mask));
    assert(!mask && character[0]==4 && character[1]==4 && character[2]==4 && character[3]==4);
    assert(SudekiMpLobbyLaunchPlanDevPlayReady(&p,2,15));
    assert(!SudekiMpLobbyLaunchPlanDevPlayReady(&p,4,15));
    assert(!SudekiMpLobbyLaunchPlanDevPlayReady(NULL,2,15));
    p.mode=SUDEKIMP_LOBBY_MODE_MULTIPLAYER;
    memset(character,9,sizeof(character)); mask=9;
    assert(!SudekiMpLobbyLaunchProjectNative(&p,character,&mask));
    assert(mask==9 && character[0]==9 && character[3]==9);
    assert(!SudekiMpLobbyLaunchProjectNative(NULL,character,&mask));
    assert(!SudekiMpLobbyLaunchProjectNative(&p,NULL,&mask));
    assert(!SudekiMpLobbyLaunchProjectNative(&p,character,NULL));
    p.character[0]=2; p.character[1]=3; p.character[2]=0; p.character[3]=1;
    assert(SudekiMpLobbyLaunchPlanNativeReady(&p,2));
    p.character[3]=4; p.reserved_mask=7;
    assert(SudekiMpLobbyLaunchProjectNative(&p,character,&mask));
    assert(mask==7 && character[3]==4 && SudekiMpLobbyLaunchPlanNativeReady(&p,2));
}
int main(void) {
    native_projection();
    SudekiMpLobbyLaunchPlan p={.revision=1,.generation=19,.seat=0,
        .members=3,.reserved_mask=7,.character={2,1,0,4},.nonce={101,102,0,0}};
    assert(SudekiMpLobbyLaunchPlanValid(&p));
    /* Offline player2 keeps Buki but gets no load ticket/ACK obligation. */
    assert(!(p.members&4) && (p.reserved_mask&4) && p.character[2]==0 && !p.nonce[2]);
    p.nonce[2]=103; assert(!SudekiMpLobbyLaunchPlanValid(&p)); p.nonce[2]=0;
    p.reserved_mask=3; assert(!SudekiMpLobbyLaunchPlanValid(&p)); p.reserved_mask=7;
    p.character[2]=1; assert(!SudekiMpLobbyLaunchPlanValid(&p)); p.character[2]=0;
    p.character[2]=4; assert(!SudekiMpLobbyLaunchPlanValid(&p)); p.character[2]=0;
    p.character[2]=SUDEKIMP_LOBBY_TALOS; assert(!SudekiMpLobbyLaunchPlanValid(&p));
    p.mode=SUDEKIMP_LOBBY_MODE_DEV_PLAY; assert(SudekiMpLobbyLaunchPlanValid(&p));
    p.character[0]=SUDEKIMP_LOBBY_TALOS; assert(SudekiMpLobbyLaunchPlanValid(&p));
    p.character[1]=SUDEKIMP_LOBBY_TALOS; assert(SudekiMpLobbyLaunchPlanValid(&p));
    p.character[0]=p.character[1]=p.character[2]=2; assert(SudekiMpLobbyLaunchPlanValid(&p));
    p.character[2]=4; assert(!SudekiMpLobbyLaunchPlanValid(&p));
    p.character[2]=6; assert(!SudekiMpLobbyLaunchPlanValid(&p));
    p.character[2]=255; assert(!SudekiMpLobbyLaunchPlanValid(&p));
    p.character[2]=2; p.character[3]=5; assert(!SudekiMpLobbyLaunchPlanValid(&p)); p.character[3]=4;
    p.mode=2; assert(!SudekiMpLobbyLaunchPlanValid(&p));
    p.mode=255; assert(!SudekiMpLobbyLaunchPlanValid(&p));
    p.mode=SUDEKIMP_LOBBY_MODE_MULTIPLAYER; assert(!SudekiMpLobbyLaunchPlanValid(&p));
    p.character[1]=1; p.character[2]=0;
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
    puts("PASS: launch membership/reservations, regular uniqueness, Dev Play duplicates/Talos, malformed modes");
    return 0;
}
