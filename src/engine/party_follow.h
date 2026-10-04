#ifndef SUDEKIMP_PARTY_FOLLOW_H
#define SUDEKIMP_PARTY_FOLLOW_H
#include <stdint.h>
/* Canonical actor indices, never connection slots; 4 means no follow target.
 * The caller positively establishes active-human and native-AI ownership. */
unsigned SudekiMpPartyFollowTarget(unsigned source,unsigned host,
    uint8_t available,uint8_t humans,const float positions[4][3]);
#endif
