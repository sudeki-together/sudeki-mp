#include "engine/party_ownership.h"
#include <string.h>

int SudekiMpPartyAssignmentValid(const SudekiMpPartyAssignment *a) {
    unsigned used=0;
    if(!a || !a->world || !a->revision || !(a->humans&1u) ||
        (a->humans&~15u) || (a->available&~15u)) return 0;
    for(unsigned p=0;p<4;++p) {
        unsigned c=a->character[p];
        if(!a->generation[p]) return 0;
        if(!(a->humans&(1u<<p))) {
            if(c!=SUDEKIMP_PARTY_NO_CHARACTER) return 0;
        } else {
            if(c>=4 || !(a->available&(1u<<c)) || (used&(1u<<c))) return 0;
            used|=1u<<c;
        }
    }
    return 1;
}
unsigned SudekiMpPartyCharacterOwner(const SudekiMpPartyAssignment *a,unsigned c) {
    if(!SudekiMpPartyAssignmentValid(a) || c>=4) return 4;
    for(unsigned p=0;p<4;++p)
        if((a->humans&(1u<<p)) && a->character[p]==c) return p;
    return 4;
}
static void clear_swap(SudekiMpPartyOwnership *s) {
    s->phase=SUDEKIMP_PARTY_SWAP_IDLE; s->request=0;
    s->player=s->previous=s->target=4;
}
static int state_valid(const SudekiMpPartyOwnership *s) {
    if(!s || !SudekiMpPartyAssignmentValid(&s->assignment)) return 0;
    if(s->phase==SUDEKIMP_PARTY_SWAP_IDLE)
        return !s->request && s->player==4 && s->previous==4 && s->target==4;
    if(s->phase<SUDEKIMP_PARTY_SWAP_RESERVED || s->phase>SUDEKIMP_PARTY_SWAP_WAIT_ACK ||
        s->player>=4 || s->previous>=4 || s->target>=4 || s->previous==s->target ||
        !s->request || s->last_request[s->player]!=s->request ||
        !(s->assignment.humans&(1u<<s->player)) ||
        !(s->assignment.available&(1u<<s->target))) return 0;
    if(s->phase==SUDEKIMP_PARTY_SWAP_WAIT_ACK)
        return s->assignment.character[s->player]==s->target &&
            SudekiMpPartyCharacterOwner(&s->assignment,s->previous)==4;
    return s->assignment.character[s->player]==s->previous &&
        SudekiMpPartyCharacterOwner(&s->assignment,s->target)==4;
}
int SudekiMpPartyOwnershipInitialize(SudekiMpPartyOwnership *s,
    const SudekiMpPartyAssignment *a) {
    SudekiMpPartyOwnership v;
    if(!s || !SudekiMpPartyAssignmentValid(a)) return 0;
    memset(&v,0,sizeof(v)); v.assignment=*a; clear_swap(&v); *s=v; return 1;
}
SudekiMpPartySwapResult SudekiMpPartySwapRequest(SudekiMpPartyOwnership *s,
    unsigned p,unsigned c,uint32_t request,uint32_t world,uint32_t revision,
    unsigned busy,int world_busy) {
    if(!state_valid(s) || p>=4 || c>=4 || !request || (busy&~15u) ||
        !(s->assignment.humans&(1u<<p))) return SUDEKIMP_PARTY_SWAP_INVALID;
    if(world!=s->assignment.world || revision!=s->assignment.revision ||
        request<=s->last_request[p]) return SUDEKIMP_PARTY_SWAP_STALE;
    if(s->phase!=SUDEKIMP_PARTY_SWAP_IDLE || world_busy ||
        (busy&((1u<<c)|(1u<<s->assignment.character[p])))) return SUDEKIMP_PARTY_SWAP_BUSY;
    if(!(s->assignment.available&(1u<<c))) return SUDEKIMP_PARTY_SWAP_UNAVAILABLE;
    if(c==s->assignment.character[p]) return SUDEKIMP_PARTY_SWAP_SAME_CHARACTER;
    if(SudekiMpPartyCharacterOwner(&s->assignment,c)!=4) return SUDEKIMP_PARTY_SWAP_OCCUPIED;
    if(s->assignment.revision==UINT32_MAX || s->assignment.generation[c]==UINT32_MAX ||
        s->assignment.generation[s->assignment.character[p]]==UINT32_MAX)
        return SUDEKIMP_PARTY_SWAP_EXHAUSTED;
    s->request=request; s->last_request[p]=request; s->player=(uint8_t)p;
    s->previous=s->assignment.character[p]; s->target=(uint8_t)c;
    s->phase=SUDEKIMP_PARTY_SWAP_RESERVED; return SUDEKIMP_PARTY_SWAP_OK;
}
static int exact(const SudekiMpPartyOwnership *s,unsigned p,uint32_t request,
    SudekiMpPartySwapPhase phase) {
    return state_valid(s) && s->phase==phase && s->player==p && s->request==request;
}
int SudekiMpPartySwapBegin(SudekiMpPartyOwnership *s,unsigned p,uint32_t request) {
    if(!exact(s,p,request,SUDEKIMP_PARTY_SWAP_RESERVED)) return 0;
    s->phase=SUDEKIMP_PARTY_SWAP_HANDOFF; return 1;
}
int SudekiMpPartySwapCancel(SudekiMpPartyOwnership *s,unsigned p,uint32_t request) {
    if(!exact(s,p,request,SUDEKIMP_PARTY_SWAP_RESERVED)) return 0;
    clear_swap(s); return 1;
}
static int advance(SudekiMpPartyOwnership *s) {
    if(s->assignment.revision==UINT32_MAX ||
        s->assignment.generation[s->previous]==UINT32_MAX ||
        s->assignment.generation[s->target]==UINT32_MAX) return 0;
    ++s->assignment.revision; ++s->assignment.generation[s->previous];
    ++s->assignment.generation[s->target]; return 1;
}
int SudekiMpPartySwapAbortRestored(SudekiMpPartyOwnership *s,unsigned p,uint32_t request) {
    if(!exact(s,p,request,SUDEKIMP_PARTY_SWAP_HANDOFF) || !advance(s)) return 0;
    clear_swap(s); return 1;
}
int SudekiMpPartySwapCommit(SudekiMpPartyOwnership *s,unsigned p,uint32_t request) {
    if(!exact(s,p,request,SUDEKIMP_PARTY_SWAP_HANDOFF) || !advance(s)) return 0;
    s->assignment.character[p]=s->target;
    s->phase=SUDEKIMP_PARTY_SWAP_WAIT_ACK; return 1;
}
int SudekiMpPartySwapAcknowledge(SudekiMpPartyOwnership *s,unsigned p,uint32_t request,
    uint32_t world,uint32_t revision) {
    if(!exact(s,p,request,SUDEKIMP_PARTY_SWAP_WAIT_ACK) ||
        s->assignment.world!=world || s->assignment.revision!=revision) return 0;
    clear_swap(s); return 1;
}
int SudekiMpPartyPlayerInputReady(const SudekiMpPartyOwnership *s,unsigned p,unsigned c,
    uint32_t world,uint32_t revision,uint32_t generation) {
    return state_valid(s) && p<4 && c<4 && (s->assignment.humans&(1u<<p)) &&
        (s->phase==SUDEKIMP_PARTY_SWAP_IDLE || s->player!=p) &&
        s->assignment.character[p]==c && s->assignment.world==world &&
        s->assignment.revision==revision && s->assignment.generation[c]==generation;
}
int SudekiMpPartyOwnershipJoin(SudekiMpPartyOwnership *s,unsigned p,unsigned c) {
    if(!state_valid(s) || s->phase!=SUDEKIMP_PARTY_SWAP_IDLE || p==0 || p>=4 || c>=4 ||
        (s->assignment.humans&(1u<<p)) || !(s->assignment.available&(1u<<c)) ||
        SudekiMpPartyCharacterOwner(&s->assignment,c)!=4 ||
        s->assignment.revision==UINT32_MAX || s->assignment.generation[c]==UINT32_MAX) return 0;
    s->assignment.humans|=(uint8_t)(1u<<p); s->assignment.character[p]=(uint8_t)c;
    /* A fresh authenticated connection has a fresh request sequence. Old
     * inputs/requests still carry the retired assignment revision/generation. */
    s->last_request[p]=0;
    ++s->assignment.revision; ++s->assignment.generation[c]; return 1;
}
int SudekiMpPartyOwnershipLeaveDrained(SudekiMpPartyOwnership *s,unsigned p) {
    if(!state_valid(s) || p==0 || p>=4 ||
        (s->phase!=SUDEKIMP_PARTY_SWAP_IDLE &&
         !(s->phase==SUDEKIMP_PARTY_SWAP_WAIT_ACK && s->player==p)) ||
        !(s->assignment.humans&(1u<<p)) || s->assignment.revision==UINT32_MAX ||
        s->assignment.generation[s->assignment.character[p]]==UINT32_MAX) return 0;
    ++s->assignment.generation[s->assignment.character[p]]; ++s->assignment.revision;
    s->assignment.humans&=(uint8_t)~(1u<<p); s->assignment.character[p]=4;
    if(s->phase==SUDEKIMP_PARTY_SWAP_WAIT_ACK) clear_swap(s);
    return 1;
}
