#include "engine/buki_replica_native.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static unsigned char image[0x300000], actor[0x180], parts[8][0x140];
static const unsigned offsets[] = {0x90,0x8c,0x80,0xac,0xb8,0xa4,0x94,0xc4};
static const unsigned vtables[] = {0x2cc9ac,0x2d48d4,0x2c8644,0x2d4b24,
    0x2d4bd4,0x2c8754,0x2d4924,0x2d4dac};
static int failures, calls, mode;
#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n",__LINE__,#x); ++failures; } } while (0)
static void seed(void) {
    unsigned i;
    memset(actor,0,sizeof(actor)); memset(parts,0,sizeof(parts));
    *(void **)actor = image+0x2d5a88;
    for(i=0;i<8;++i) {
        *(void **)(actor+offsets[i])=parts[i];
        *(void **)parts[i]=image+vtables[i];
        *(void **)(parts[i]+0x10)=actor;
    }
    *(uint32_t *)(parts[0]+0x50)=0x1000;
    *(uint32_t *)(parts[4]+0x64)=2;
    *(void **)(parts[4]+0xa8)=actor;
    *(void **)(parts[4]+0xac)=actor;
    parts[4][0xb0]=0; parts[4][0xb1]=0x1f; parts[5][0x72]=0x10;
    mode=calls=0;
}
static void __attribute__((stdcall)) face(void *arb, const float *d,
    float speed, float turn, unsigned int flags) {
    ++calls;
    CHECK(arb==parts[0]); CHECK(speed==0 && turn==1 && flags==0);
    CHECK(fabsf(d[0]*d[0]+d[2]*d[2]-1)<0.00001f && d[1]==0);
    if(mode!=1) memcpy(parts[3]+0x48,d,12);
    if(mode==2) *(void **)(actor+0x8c)=NULL;
}
static void __attribute__((thiscall)) interrupt(void *combo) {
    ++calls; CHECK(combo==parts[4]);
    if(mode==1) return;
    *(uint32_t *)(parts[4]+0x64)=0;
    *(void **)(parts[4]+0xa8)=NULL; *(void **)(parts[4]+0xac)=NULL;
    parts[4][0xb0]=0xff; parts[4][0xb1]=0xe0;
    parts[5][0x72]=0; *(uint32_t *)(parts[0]+0x50)=0;
    if(mode==2) *(void **)(actor+0xa4)=NULL;
}
int main(void) {
    float d[3]={-.6f,0,.8f};
    unsigned i;
    SudekiMpBukiReplicaNativeTestCalls(face,interrupt);
    seed(); CHECK(SudekiMpBukiReplicaSyncFacing((HMODULE)image,actor,parts[0],d));
    CHECK(calls==1); CHECK(!memcmp(parts[3]+0x48,d,12));
    seed(); CHECK(SudekiMpBukiReplicaInterruptAttack((HMODULE)image,actor,parts[0]));
    CHECK(calls==1);
    CHECK(SudekiMpBukiReplicaBodyAvailable((HMODULE)image,actor,parts[0]));
    CHECK(calls==1); /* The observation cannot call native cleanup. */
    for(i=0;i<32;++i) {
        seed(); interrupt(parts[4]); calls=0;
        parts[4][0xb1]=(uint8_t)(0x20u|i);
        CHECK(SudekiMpBukiReplicaBodyAvailable((HMODULE)image,actor,parts[0])
            == ((i & 0x13u)==0));
        CHECK(parts[4][0xb1]==(0x20u|i) && calls==0);
        /* Even otherwise harmless window flags never mask a queued task. */
        *(void **)(parts[4]+0xac)=actor;
        CHECK(!SudekiMpBukiReplicaBodyAvailable((HMODULE)image,actor,parts[0]));
    }
    for(i=0;i<8;++i) {
        seed(); interrupt(parts[4]); calls=0;
        switch(i) {
        case 0: *(uint32_t *)(parts[4]+0x64)=1; break;
        case 1: *(void **)(parts[4]+0xa8)=actor; break;
        case 2: *(void **)(parts[4]+0xac)=actor; break;
        case 3: parts[4][0xb0]=0; break;
        case 4: parts[4][0xb1]|=2; break;
        case 5: parts[5][0x72]|=0x10; break;
        case 6: *(uint32_t *)(parts[0]+0x50)=0x1000; break;
        case 7: *(void **)(parts[4]+0x10)=image; break;
        }
        CHECK(!SudekiMpBukiReplicaBodyAvailable((HMODULE)image,actor,parts[0]));
        CHECK(calls==0);
    }
    seed(); interrupt(parts[4]); calls=0;
    CHECK(!SudekiMpBukiReplicaBodyAvailable((HMODULE)image,actor,parts[1]));
    *(void **)actor=image+0x2d5010;
    CHECK(!SudekiMpBukiReplicaBodyAvailable((HMODULE)image,actor,parts[0]));
    CHECK(calls==0);
    for(i=0;i<8;++i) {
        seed(); *(void **)(parts[i]+0x10)=image;
        if(i<4) CHECK(!SudekiMpBukiReplicaSyncFacing((HMODULE)image,actor,parts[0],d));
        else CHECK(!SudekiMpBukiReplicaInterruptAttack((HMODULE)image,actor,parts[0]));
        CHECK(calls==0);
        seed(); *(void **)parts[i]=NULL;
        if(i<4) CHECK(!SudekiMpBukiReplicaSyncFacing((HMODULE)image,actor,parts[0],d));
        else CHECK(!SudekiMpBukiReplicaInterruptAttack((HMODULE)image,actor,parts[0]));
        CHECK(calls==0);
    }
    seed(); *(void **)actor=image+0x2d5010; /* Tal must not enter Buki policy. */
    CHECK(!SudekiMpBukiReplicaSyncFacing((HMODULE)image,actor,parts[0],d));
    CHECK(!SudekiMpBukiReplicaInterruptAttack((HMODULE)image,actor,parts[0]));
    CHECK(calls==0);
    seed(); CHECK(!SudekiMpBukiReplicaSyncFacing((HMODULE)image,actor,parts[1],d));
    CHECK(!SudekiMpBukiReplicaInterruptAttack((HMODULE)image,actor,parts[1]));
    CHECK(calls==0);
    seed(); d[0]=NAN;
    CHECK(!SudekiMpBukiReplicaSyncFacing((HMODULE)image,actor,parts[0],d));
    d[0]=d[2]=0; CHECK(!SudekiMpBukiReplicaSyncFacing((HMODULE)image,actor,parts[0],d));
    CHECK(calls==0); d[0]=-.6f;d[2]=.8f;
    for(i=1;i<=2;++i) {
        seed();mode=i;
        CHECK(!SudekiMpBukiReplicaSyncFacing((HMODULE)image,actor,parts[0],d));
        CHECK(calls==1);
        seed();mode=i;
        CHECK(!SudekiMpBukiReplicaInterruptAttack((HMODULE)image,actor,parts[0]));
        CHECK(calls==1); /* incomplete or replaced != terminal */
    }
    CHECK(!SudekiMpBukiReplicaNativeImageMatches(NULL));
    CHECK(!SudekiMpBukiReplicaNativeImageMatches((HMODULE)image));
    if(failures) return 1;
    puts("Buki native replica guard tests passed"); return 0;
}
