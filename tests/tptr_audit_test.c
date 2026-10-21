#include "engine/tptr_audit.h"
#include "engine/story_switch_trace.h"
#include <assert.h>
#include <stdio.h>
static SudekiMpTPtrNode n[65];
static int read_node(void *unused,uintptr_t at,void *out,size_t size) {
    (void)unused;
    if(at<(uintptr_t)n || at+size>(uintptr_t)(n+65)) return 0;
    memcpy(out,(void *)at,size); return 1;
}
static SudekiMpTPtrWalk walk(uintptr_t own,uintptr_t low,uintptr_t high) {
    return SudekiMpTPtrAuditWalk(42,(uintptr_t)n,own,low,high,read_node,NULL);
}
int main(void) {
    for(unsigned i=0;i<65;++i) n[i]=(SudekiMpTPtrNode){42,i?(uintptr_t)&n[i-1]:0,i<64?(uintptr_t)&n[i+1]:0};
    n[1].next=0;
    assert(!walk(0,0,0).flags && walk(0,0,0).count==2);
    SudekiMpTPtrWalk own=walk((uintptr_t)n,(uintptr_t)n,(uintptr_t)(n+1));
    assert(!own.flags && own.count==2);
    n[1].next=(uintptr_t)&n[2];n[2].next=0;
    SudekiMpTPtrWalk deeper=walk((uintptr_t)n,(uintptr_t)n,(uintptr_t)(n+3));
    /* Model disjoint heap/stack addresses via a reordered three-node chain. */
    n[0].next=(uintptr_t)&n[3];n[3].previous=(uintptr_t)n;n[3].next=(uintptr_t)&n[1];
    n[1].previous=(uintptr_t)&n[3];n[1].next=0;
    deeper=walk((uintptr_t)n,(uintptr_t)n,(uintptr_t)(n+2));
    assert(deeper.flags==SUDEKIMP_TPTR_STACK && deeper.count==3);
    n[0].next=(uintptr_t)&n[1];n[1].previous=(uintptr_t)n;
    n[2].next=(uintptr_t)&n[3];n[3].previous=(uintptr_t)&n[2];n[3].next=(uintptr_t)&n[4];
    assert(walk(0,(uintptr_t)n,(uintptr_t)(n+1)).flags==SUDEKIMP_TPTR_STACK);
    n[1].previous=0;assert(walk(0,0,0).flags==SUDEKIMP_TPTR_LINK);n[1].previous=(uintptr_t)n;
    n[1].object=43;assert(walk(0,0,0).flags==SUDEKIMP_TPTR_LINK);n[1].object=42;
    n[1].next=(uintptr_t)n;assert(walk(0,0,0).flags==SUDEKIMP_TPTR_CYCLE);
    n[1].next=(uintptr_t)&n[2];assert(walk(0,0,0).flags==SUDEKIMP_TPTR_BOUND);
    n[1].next=1;assert(walk(0,0,0).flags==SUDEKIMP_TPTR_UNREADABLE);
    SudekiMpTPtrWalk saved={.count=2,.nodes={(uintptr_t)&n[2],(uintptr_t)&n[3]}};
    static SudekiMpTPtrCapture captured;uint8_t expected[32];memcpy(expected,(uint8_t *)&n[3]-8,32);
    SudekiMpTPtrAuditCapture(&saved,&captured,read_node,NULL);
    memset(n,0,sizeof(n)); /* stand-in for logger stack reuse after capture */
    assert(captured.readable[1] && !memcmp(captured.bytes[1],expected,32));
    SudekiMpStorySwitchTrace trace={0};
    assert(!SudekiMpStorySwitchTraceTake(&trace,0,1000,0,0));
    assert(SudekiMpStorySwitchTraceTake(&trace,1000,1000,0,0));
    assert(!SudekiMpStorySwitchTraceTake(&trace,1000,1999,0,0));
    assert(SudekiMpStorySwitchTraceTake(&trace,1000,2000,0,0));
    assert(!SudekiMpStorySwitchTraceTake(&trace,1000,21000,0,0));
    trace=(SudekiMpStorySwitchTrace){0};
    assert(!SudekiMpStorySwitchTraceTake(&trace,1000,1000,1,4));
    assert(SudekiMpStorySwitchTraceTake(&trace,1000,1001,1,6));
    assert(!SudekiMpStorySwitchTraceTake(&trace,1000,1002,1,6));
    assert(SudekiMpStorySwitchTraceTake(&trace,1000,1002,1,3));
    for(unsigned i=trace.samples;i<64;++i)assert(SudekiMpStorySwitchTraceTake(&trace,1000,1003+i,1,i%2));
    assert(!SudekiMpStorySwitchTraceTake(&trace,1000,1200,1,6));
    assert(SudekiMpStorySwitchTraceTake(&trace,2000,2000,1,6));
    puts("TPtrAuditTest PASS: observational walker only");return 0;
}
