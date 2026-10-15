#include "network/lan_story_menu_policy.h"
unsigned SudekiMpStoryMenuNextSlot(unsigned count,unsigned current,BOOL forward,unsigned allowed_mask) {
    if(!count || count>4u || current>=count) return current;
    unsigned slot=current;
    for(unsigned step=0;step<count;++step) {
        slot=forward?(slot+1u)%count:(slot+count-1u)%count;
        if(allowed_mask&(1u<<slot)) return slot;
    }
    return current;
}
unsigned SudekiMpStoryMenuInitialSlot(unsigned count,unsigned allowed_mask,unsigned fallback) {
    if(!count || count>4u) return fallback;
    for(unsigned slot=0;slot<count;++slot) if(allowed_mask&(1u<<slot)) return slot;
    return fallback;
}
