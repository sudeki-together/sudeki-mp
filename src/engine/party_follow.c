#include "engine/party_follow.h"
#include <math.h>
#include <float.h>
unsigned SudekiMpPartyFollowTarget(unsigned source,unsigned host,
    uint8_t available,uint8_t humans,const float positions[4][3]) {
    if(source>=4 || host>4 || !positions || (available|humans)>15 ||
        (humans&~available) || !(available&(1u<<source)) || (humans&(1u<<source))) return 4;
    for(unsigned c=0;c<4;++c) if(available&(1u<<c))
        for(unsigned axis=0;axis<3;++axis)
            if(!isfinite(positions[c][axis]) || fabsf(positions[c][axis])>=1000000.f) return 4;
    if(host<4 && (humans&(1u<<host))) return host;
    unsigned target=4; double nearest=DBL_MAX;
    for(unsigned c=0;c<4;++c) if(humans&(1u<<c)) {
        double distance=0;
        for(unsigned axis=0;axis<3;++axis) {
            double d=(double)positions[source][axis]-positions[c][axis]; distance+=d*d;
        }
        if(distance<nearest) { nearest=distance; target=c; }
    }
    return target;
}
