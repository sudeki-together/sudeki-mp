#include "engine/party_ownership.h"
#include <string.h>

static unsigned bit(unsigned p) { return p<4 ? 1u<<p : 0; }
int SudekiMpPartyAssignmentValid(const SudekiMpPartyAssignment *a) {
    unsigned used=0;
    if(!a || !a->world || !a->revision || (a->humans&~15u) ||
        (a->available&~15u)) return 0;
    for(unsigned p=0;p<4;++p) {
        unsigned c=a->character[p];
        if(!a->generation[p]) return 0;
        if(!(a->humans&bit(p))) {
            if(c!=4) return 0;
        } else {
            if(c>=4 || (used&bit(c))) return 0;
            used|=bit(c);
        }
    }
    return 1;
}
unsigned SudekiMpPartyCharacterOwner(const SudekiMpPartyAssignment *a,unsigned c) {
    if(!SudekiMpPartyAssignmentValid(a) || c>=4) return 4;
    for(unsigned p=0;p<4;++p)
        if((a->humans&bit(p)) && a->character[p]==c) return p;
    return 4;
}
static void clear_swap(SudekiMpPartyOwnership *s) {
    s->phase=SUDEKIMP_PARTY_SWAP_IDLE; s->request=0;
    s->player=s->previous=s->target=s->requester=s->displaced=4;
    s->handoff_control=0;
}
static int yielding(const SudekiMpPartyOwnership *s,unsigned p) {
    return !(s->connected&bit(p)) || ((s->menu|s->away)&bit(p));
}
static void reconcile(SudekiMpPartyOwnership *s,unsigned p) {
    if(yielding(s,p) && (s->control[p]==SUDEKIMP_PARTY_CONTROL_HUMAN ||
        s->control[p]==SUDEKIMP_PARTY_CONTROL_WAIT_ACK))
        s->control[p]=SUDEKIMP_PARTY_CONTROL_DRAINING;
}
int SudekiMpPartyOwnershipValid(const SudekiMpPartyOwnership *s) {
    if(!s || !SudekiMpPartyAssignmentValid(&s->assignment) ||
        !(s->connected&1u) || (s->connected&~15u) ||
        (s->controlling&~s->assignment.humans) ||
        ((s->menu|s->away)&~s->connected) || s->paused>1 ||
        s->absence_policy<SUDEKIMP_PARTY_AI_COVER ||
        s->absence_policy>SUDEKIMP_PARTY_SHARED_PAUSE) return 0;
    for(unsigned p=0;p<4;++p) {
        SudekiMpPartyControlPhase phase=s->control[p];
        if(phase<SUDEKIMP_PARTY_CONTROL_AI || phase>SUDEKIMP_PARTY_CONTROL_WAIT_ACK)
            return 0;
        if(!!(s->controlling&bit(p)) !=
            (phase==SUDEKIMP_PARTY_CONTROL_HUMAN ||
             phase==SUDEKIMP_PARTY_CONTROL_DRAINING ||
             phase==SUDEKIMP_PARTY_CONTROL_WAIT_ACK)) return 0;
        if(!(s->assignment.humans&bit(p)) && phase!=SUDEKIMP_PARTY_CONTROL_AI)
            return 0;
        if(phase==SUDEKIMP_PARTY_CONTROL_HUMAN && yielding(s,p)) return 0;
        if((s->controlling&bit(p)) &&
            !(s->assignment.available&bit(s->assignment.character[p]))) return 0;
    }
    if(s->phase==SUDEKIMP_PARTY_SWAP_IDLE)
        return !s->request && s->player==4 && s->previous==4 && s->target==4 &&
            s->requester==4 && s->displaced==4 && !s->handoff_control;
    if(s->phase<SUDEKIMP_PARTY_SWAP_RESERVED || s->phase>SUDEKIMP_PARTY_SWAP_WAIT_ACK ||
        s->player>=4 || s->requester>=4 || s->previous>4 || s->target>=4 ||
        s->previous==s->target || s->displaced>4 || s->displaced==s->player ||
        s->handoff_control>1 || !s->request ||
        s->last_request[s->requester]<s->request ||
        !(s->assignment.available&bit(s->target))) return 0;
    if(s->phase==SUDEKIMP_PARTY_SWAP_WAIT_ACK)
        return s->assignment.character[s->player]==s->target &&
            (s->previous==4 || SudekiMpPartyCharacterOwner(&s->assignment,s->previous)==4) &&
            (s->displaced==4 || s->assignment.character[s->displaced]==4);
    return s->assignment.character[s->player]==s->previous &&
        SudekiMpPartyCharacterOwner(&s->assignment,s->target)==s->displaced;
}
int SudekiMpPartyOwnershipInitializePresence(SudekiMpPartyOwnership *s,
    const SudekiMpPartyAssignment *a,unsigned connected,unsigned controlling) {
    SudekiMpPartyOwnership v;
    if(!s || !SudekiMpPartyAssignmentValid(a) || !(connected&1u) ||
        (connected&~15u) || (controlling&~(connected&a->humans))) return 0;
    memset(&v,0,sizeof(v)); v.assignment=*a; clear_swap(&v);
    v.connected=(uint8_t)connected; v.controlling=(uint8_t)controlling;
    for(unsigned p=0;p<4;++p) v.control[p]=(controlling&bit(p))?
        SUDEKIMP_PARTY_CONTROL_HUMAN:SUDEKIMP_PARTY_CONTROL_AI;
    if(!SudekiMpPartyOwnershipValid(&v)) return 0;
    *s=v; return 1;
}
int SudekiMpPartyOwnershipInitialize(SudekiMpPartyOwnership *s,
    const SudekiMpPartyAssignment *a) {
    return a && SudekiMpPartyOwnershipInitializePresence(s,a,a->humans,a->humans);
}
static SudekiMpPartySwapResult request_valid(const SudekiMpPartyOwnership *s,
    unsigned p,uint32_t request,uint32_t world,uint32_t revision) {
    if(!SudekiMpPartyOwnershipValid(s) || p>=4 || !request ||
        !(s->connected&bit(p))) return SUDEKIMP_PARTY_SWAP_INVALID;
    if(world!=s->assignment.world || revision!=s->assignment.revision ||
        request<=s->last_request[p]) return SUDEKIMP_PARTY_SWAP_STALE;
    if(s->assignment.revision==UINT32_MAX) return SUDEKIMP_PARTY_SWAP_EXHAUSTED;
    return SUDEKIMP_PARTY_SWAP_OK;
}
static int can_advance(const SudekiMpPartyOwnership *s,unsigned a,unsigned b) {
    return s->assignment.revision<UINT32_MAX &&
        (a==4 || s->assignment.generation[a]<UINT32_MAX) &&
        (b==4 || s->assignment.generation[b]<UINT32_MAX);
}
static int advance(SudekiMpPartyOwnership *s,unsigned a,unsigned b) {
    if(!can_advance(s,a,b)) return 0;
    ++s->assignment.revision;
    if(a<4) ++s->assignment.generation[a];
    if(b<4 && b!=a) ++s->assignment.generation[b];
    return 1;
}
static SudekiMpPartySwapResult swap_request(SudekiMpPartyOwnership *s,
    unsigned requester,unsigned p,unsigned c,uint32_t request,uint32_t world,
    uint32_t revision,unsigned busy,int world_busy,int reassign) {
    SudekiMpPartySwapResult result=request_valid(s,requester,request,world,revision);
    unsigned owner,previous;
    if(result!=SUDEKIMP_PARTY_SWAP_OK) return result;
    if(p>=4 || c>=4 || (busy&~15u) || !(s->connected&bit(p)))
        return SUDEKIMP_PARTY_SWAP_INVALID;
    if(reassign && requester!=0) return SUDEKIMP_PARTY_SWAP_UNAUTHORIZED;
    previous=s->assignment.character[p];
    if(s->phase!=SUDEKIMP_PARTY_SWAP_IDLE || world_busy || s->paused ||
        (busy&(bit(c)|bit(previous))) ||
        (s->control[p]!=SUDEKIMP_PARTY_CONTROL_HUMAN &&
         s->control[p]!=SUDEKIMP_PARTY_CONTROL_AI)) return SUDEKIMP_PARTY_SWAP_BUSY;
    if(!(s->assignment.available&bit(c))) return SUDEKIMP_PARTY_SWAP_UNAVAILABLE;
    if(c==previous) return SUDEKIMP_PARTY_SWAP_SAME_CHARACTER;
    owner=SudekiMpPartyCharacterOwner(&s->assignment,c);
    if(owner!=4 && (!reassign || !yielding(s,owner) ||
        s->control[owner]!=SUDEKIMP_PARTY_CONTROL_AI)) return SUDEKIMP_PARTY_SWAP_OCCUPIED;
    if(!can_advance(s,previous,c)) return SUDEKIMP_PARTY_SWAP_EXHAUSTED;
    s->request=request; s->last_request[requester]=request;
    s->requester=(uint8_t)requester; s->player=(uint8_t)p;
    s->previous=(uint8_t)previous; s->target=(uint8_t)c; s->displaced=(uint8_t)owner;
    s->handoff_control=(uint8_t)!yielding(s,p);
    s->phase=SUDEKIMP_PARTY_SWAP_RESERVED; return SUDEKIMP_PARTY_SWAP_OK;
}
SudekiMpPartySwapResult SudekiMpPartySwapRequest(SudekiMpPartyOwnership *s,
    unsigned p,unsigned c,uint32_t request,uint32_t world,uint32_t revision,
    unsigned busy,int world_busy) {
    return swap_request(s,p,p,c,request,world,revision,busy,world_busy,0);
}
SudekiMpPartySwapResult SudekiMpPartyHostReassignRequest(SudekiMpPartyOwnership *s,
    unsigned requester,unsigned p,unsigned c,uint32_t request,uint32_t world,
    uint32_t revision,unsigned busy,int world_busy) {
    return swap_request(s,requester,p,c,request,world,revision,busy,world_busy,1);
}
static int exact(const SudekiMpPartyOwnership *s,unsigned p,uint32_t request,
    SudekiMpPartySwapPhase phase) {
    return SudekiMpPartyOwnershipValid(s) && s->phase==phase &&
        s->player==p && s->request==request;
}
int SudekiMpPartySwapBegin(SudekiMpPartyOwnership *s,unsigned p,uint32_t request) {
    if(!exact(s,p,request,SUDEKIMP_PARTY_SWAP_RESERVED) || s->paused ||
        !(s->connected&bit(p)) ||
        (s->displaced<4 && (!yielding(s,s->displaced) ||
         s->control[s->displaced]!=SUDEKIMP_PARTY_CONTROL_AI)) ||
        (s->control[p]!=SUDEKIMP_PARTY_CONTROL_AI &&
         s->control[p]!=SUDEKIMP_PARTY_CONTROL_HUMAN)) return 0;
    s->phase=SUDEKIMP_PARTY_SWAP_HANDOFF; return 1;
}
int SudekiMpPartySwapCancel(SudekiMpPartyOwnership *s,unsigned p,uint32_t request) {
    if(!exact(s,p,request,SUDEKIMP_PARTY_SWAP_RESERVED)) return 0;
    clear_swap(s); return 1;
}
int SudekiMpPartySwapAbortRestored(SudekiMpPartyOwnership *s,unsigned p,uint32_t request) {
    if(!exact(s,p,request,SUDEKIMP_PARTY_SWAP_HANDOFF) ||
        !advance(s,s->previous,s->target)) return 0;
    clear_swap(s); reconcile(s,p); return 1;
}
static int commit_swap(SudekiMpPartyOwnership *s,unsigned p,uint32_t request,int released) {
    if(!exact(s,p,request,SUDEKIMP_PARTY_SWAP_HANDOFF) ||
        !advance(s,s->previous,s->target)) return 0;
    if(s->displaced<4) {
        s->assignment.humans&=(uint8_t)~bit(s->displaced);
        s->assignment.character[s->displaced]=4;
    }
    s->assignment.humans|=(uint8_t)bit(p); s->assignment.character[p]=s->target;
    if(s->handoff_control && !released) {
        s->controlling|=(uint8_t)bit(p); s->control[p]=SUDEKIMP_PARTY_CONTROL_WAIT_ACK;
    } else {
        s->controlling&=(uint8_t)~bit(p); s->control[p]=SUDEKIMP_PARTY_CONTROL_AI;
    }
    reconcile(s,p); s->phase=SUDEKIMP_PARTY_SWAP_WAIT_ACK; return 1;
}
int SudekiMpPartySwapCommit(SudekiMpPartyOwnership *s,unsigned p,uint32_t request) {
    return commit_swap(s,p,request,0);
}
int SudekiMpPartySwapCommitReleased(SudekiMpPartyOwnership *s,unsigned p,uint32_t request) {
    return commit_swap(s,p,request,1);
}
int SudekiMpPartySwapAcknowledge(SudekiMpPartyOwnership *s,unsigned p,uint32_t request,
    uint32_t world,uint32_t revision) {
    if(!exact(s,p,request,SUDEKIMP_PARTY_SWAP_WAIT_ACK) ||
        !(s->connected&bit(p)) || s->assignment.world!=world ||
        s->assignment.revision!=revision ||
        (!yielding(s,p) && s->control[p]!=SUDEKIMP_PARTY_CONTROL_WAIT_ACK)) return 0;
    if(s->control[p]==SUDEKIMP_PARTY_CONTROL_WAIT_ACK)
        s->control[p]=SUDEKIMP_PARTY_CONTROL_HUMAN;
    clear_swap(s); reconcile(s,p); return 1;
}
SudekiMpPartySwapResult SudekiMpPartyPresenceRequest(SudekiMpPartyOwnership *s,
    unsigned p,uint32_t request,uint32_t world,uint32_t revision,int menu,int away) {
    SudekiMpPartySwapResult r=request_valid(s,p,request,world,revision);
    if(r!=SUDEKIMP_PARTY_SWAP_OK) return r;
    if((menu!=0 && menu!=1) || (away!=0 && away!=1)) return SUDEKIMP_PARTY_SWAP_INVALID;
    if(away && !(s->away&bit(p)) && s->absence_policy==SUDEKIMP_PARTY_SHARED_PAUSE)
        s->paused=1;
    s->menu=(uint8_t)((s->menu&~bit(p))|(menu?bit(p):0));
    s->away=(uint8_t)((s->away&~bit(p))|(away?bit(p):0));
    s->last_request[p]=request; ++s->assignment.revision;
    reconcile(s,p); return SUDEKIMP_PARTY_SWAP_OK;
}
static int control_exact(const SudekiMpPartyOwnership *s,unsigned p,
    uint32_t world,uint32_t generation,SudekiMpPartyControlPhase phase) {
    return SudekiMpPartyOwnershipValid(s) && p<4 &&
        (s->assignment.humans&bit(p)) && s->assignment.world==world &&
        s->assignment.generation[s->assignment.character[p]]==generation &&
        s->control[p]==phase &&
        (s->phase==SUDEKIMP_PARTY_SWAP_IDLE ||
         (s->phase==SUDEKIMP_PARTY_SWAP_WAIT_ACK && s->player==p) ||
         (s->player!=p && s->displaced!=p));
}
int SudekiMpPartyControlDrainComplete(SudekiMpPartyOwnership *s,unsigned p,
    uint32_t world,uint32_t generation) {
    if(!control_exact(s,p,world,generation,SUDEKIMP_PARTY_CONTROL_DRAINING) ||
        !advance(s,s->assignment.character[p],4)) return 0;
    s->controlling&=(uint8_t)~bit(p); s->control[p]=SUDEKIMP_PARTY_CONTROL_AI; return 1;
}
int SudekiMpPartyControlAcquireBegin(SudekiMpPartyOwnership *s,unsigned p,
    uint32_t world,uint32_t revision,uint32_t generation) {
    if(!control_exact(s,p,world,generation,SUDEKIMP_PARTY_CONTROL_AI) ||
        s->assignment.revision!=revision || yielding(s,p) || s->paused ||
        !(s->assignment.available&bit(s->assignment.character[p])) ||
        !can_advance(s,s->assignment.character[p],4)) return 0;
    s->control[p]=SUDEKIMP_PARTY_CONTROL_ACQUIRING; return 1;
}
int SudekiMpPartyControlAcquireCommit(SudekiMpPartyOwnership *s,unsigned p,
    uint32_t world,uint32_t generation) {
    if(!control_exact(s,p,world,generation,SUDEKIMP_PARTY_CONTROL_ACQUIRING) ||
        !advance(s,s->assignment.character[p],4)) return 0;
    s->controlling|=(uint8_t)bit(p); s->control[p]=SUDEKIMP_PARTY_CONTROL_WAIT_ACK;
    reconcile(s,p); return 1;
}
int SudekiMpPartyControlAcquireAbortRestored(SudekiMpPartyOwnership *s,unsigned p,
    uint32_t world,uint32_t generation) {
    if(!control_exact(s,p,world,generation,SUDEKIMP_PARTY_CONTROL_ACQUIRING) ||
        !advance(s,s->assignment.character[p],4)) return 0;
    s->control[p]=SUDEKIMP_PARTY_CONTROL_AI; return 1;
}
int SudekiMpPartyControlAcknowledge(SudekiMpPartyOwnership *s,unsigned p,
    uint32_t world,uint32_t revision,uint32_t generation) {
    if(!control_exact(s,p,world,generation,SUDEKIMP_PARTY_CONTROL_WAIT_ACK) ||
        (s->phase==SUDEKIMP_PARTY_SWAP_WAIT_ACK && s->player==p) ||
        s->assignment.revision!=revision || yielding(s,p)) return 0;
    s->control[p]=SUDEKIMP_PARTY_CONTROL_HUMAN; return 1;
}
int SudekiMpPartyPlayerInputReady(const SudekiMpPartyOwnership *s,unsigned p,unsigned c,
    uint32_t world,uint32_t revision,uint32_t generation) {
    return SudekiMpPartyOwnershipValid(s) && p<4 && c<4 && !s->paused &&
        !yielding(s,p) && s->control[p]==SUDEKIMP_PARTY_CONTROL_HUMAN &&
        (s->assignment.available&bit(c)) &&
        (s->phase==SUDEKIMP_PARTY_SWAP_IDLE || (s->player!=p && s->displaced!=p)) &&
        s->assignment.character[p]==c && s->assignment.world==world &&
        s->assignment.revision==revision && s->assignment.generation[c]==generation;
}
static SudekiMpPartySwapResult host_request(const SudekiMpPartyOwnership *s,
    unsigned p,uint32_t request,uint32_t world,uint32_t revision) {
    SudekiMpPartySwapResult r=request_valid(s,p,request,world,revision);
    return r==SUDEKIMP_PARTY_SWAP_OK && p!=0?SUDEKIMP_PARTY_SWAP_UNAUTHORIZED:r;
}
SudekiMpPartySwapResult SudekiMpPartySetAbsencePolicy(SudekiMpPartyOwnership *s,
    unsigned p,uint32_t request,uint32_t world,uint32_t revision,SudekiMpPartyAbsencePolicy policy) {
    SudekiMpPartySwapResult r=host_request(s,p,request,world,revision);
    if(r!=SUDEKIMP_PARTY_SWAP_OK) return r;
    if(policy<SUDEKIMP_PARTY_AI_COVER || policy>SUDEKIMP_PARTY_SHARED_PAUSE)
        return SUDEKIMP_PARTY_SWAP_INVALID;
    s->absence_policy=policy; s->last_request[p]=request; ++s->assignment.revision;
    return SUDEKIMP_PARTY_SWAP_OK;
}
SudekiMpPartySwapResult SudekiMpPartySetPaused(SudekiMpPartyOwnership *s,
    unsigned p,uint32_t request,uint32_t world,uint32_t revision,int paused) {
    SudekiMpPartySwapResult r=host_request(s,p,request,world,revision);
    if(r!=SUDEKIMP_PARTY_SWAP_OK) return r;
    if(paused!=0 && paused!=1) return SUDEKIMP_PARTY_SWAP_INVALID;
    s->paused=(uint8_t)paused; s->last_request[p]=request; ++s->assignment.revision;
    return SUDEKIMP_PARTY_SWAP_OK;
}
SudekiMpPartySwapResult SudekiMpPartyReleaseReservation(SudekiMpPartyOwnership *s,
    unsigned requester,unsigned p,uint32_t request,uint32_t world,uint32_t revision) {
    SudekiMpPartySwapResult r=host_request(s,requester,request,world,revision);
    unsigned c;
    if(r!=SUDEKIMP_PARTY_SWAP_OK) return r;
    if(p>=4 || !(s->assignment.humans&bit(p))) return SUDEKIMP_PARTY_SWAP_INVALID;
    if(s->phase!=SUDEKIMP_PARTY_SWAP_IDLE || !yielding(s,p) ||
        s->control[p]!=SUDEKIMP_PARTY_CONTROL_AI) return SUDEKIMP_PARTY_SWAP_BUSY;
    c=s->assignment.character[p];
    if(!advance(s,c,4)) return SUDEKIMP_PARTY_SWAP_EXHAUSTED;
    s->assignment.humans&=(uint8_t)~bit(p); s->assignment.character[p]=4;
    s->last_request[requester]=request; return SUDEKIMP_PARTY_SWAP_OK;
}
static int reserve_character(SudekiMpPartyOwnership *s,unsigned requester,
    unsigned p,unsigned c,uint32_t world,uint32_t revision,int waiting_allowed) {
    if(!SudekiMpPartyOwnershipValid(s) || requester!=0 || p>=4 || c>=4 ||
        s->phase!=SUDEKIMP_PARTY_SWAP_IDLE || s->assignment.world!=world ||
        s->assignment.revision!=revision || (s->assignment.humans&bit(p)) ||
        (!waiting_allowed && !(s->assignment.available&bit(c))) ||
        SudekiMpPartyCharacterOwner(&s->assignment,c)!=4 || !advance(s,c,4)) return 0;
    s->assignment.humans|=(uint8_t)bit(p); s->assignment.character[p]=(uint8_t)c;
    return 1;
}
int SudekiMpPartyOwnershipReserve(SudekiMpPartyOwnership *s,unsigned requester,
    unsigned p,unsigned c,uint32_t world,uint32_t revision) {
    return reserve_character(s,requester,p,c,world,revision,0);
}
int SudekiMpPartyOwnershipReserveStory(SudekiMpPartyOwnership *s,unsigned requester,
    unsigned p,unsigned c,uint32_t world,uint32_t revision) {
    return reserve_character(s,requester,p,c,world,revision,1);
}
int SudekiMpPartyOwnershipConnect(SudekiMpPartyOwnership *s,unsigned p) {
    if(!SudekiMpPartyOwnershipValid(s) || p==0 || p>=4 || (s->connected&bit(p)) ||
        s->control[p]!=SUDEKIMP_PARTY_CONTROL_AI ||
        (s->phase!=SUDEKIMP_PARTY_SWAP_IDLE && (s->player==p || s->displaced==p)) ||
        !advance(s,4,4)) return 0;
    s->connected|=(uint8_t)bit(p); s->last_request[p]=0; return 1;
}
int SudekiMpPartyOwnershipDisconnect(SudekiMpPartyOwnership *s,unsigned p) {
    if(!SudekiMpPartyOwnershipValid(s) || p==0 || p>=4 || !(s->connected&bit(p)) ||
        !advance(s,4,4)) return 0;
    s->connected&=(uint8_t)~bit(p); s->menu&=(uint8_t)~bit(p); s->away&=(uint8_t)~bit(p);
    reconcile(s,p); return 1;
}
int SudekiMpPartyOwnershipJoin(SudekiMpPartyOwnership *s,unsigned p,unsigned c) {
    if(!SudekiMpPartyOwnershipValid(s) || s->phase!=SUDEKIMP_PARTY_SWAP_IDLE ||
        p==0 || p>=4 || c>=4 || (s->connected&bit(p)) ||
        (s->assignment.humans&bit(p)) || !(s->assignment.available&bit(c)) ||
        SudekiMpPartyCharacterOwner(&s->assignment,c)!=4 || !advance(s,c,4)) return 0;
    s->assignment.humans|=(uint8_t)bit(p); s->assignment.character[p]=(uint8_t)c;
    s->connected|=(uint8_t)bit(p); s->controlling|=(uint8_t)bit(p);
    s->control[p]=SUDEKIMP_PARTY_CONTROL_HUMAN; s->last_request[p]=0; return 1;
}
int SudekiMpPartyOwnershipLeaveDrained(SudekiMpPartyOwnership *s,unsigned p) {
    if(!SudekiMpPartyOwnershipValid(s) || p==0 || p>=4 ||
        (s->phase!=SUDEKIMP_PARTY_SWAP_IDLE &&
         !(s->phase==SUDEKIMP_PARTY_SWAP_WAIT_ACK && s->player==p)) ||
        s->control[p]==SUDEKIMP_PARTY_CONTROL_ACQUIRING ||
        (!(s->connected&bit(p)) && s->control[p]==SUDEKIMP_PARTY_CONTROL_AI &&
         !(s->phase==SUDEKIMP_PARTY_SWAP_WAIT_ACK && s->player==p)) ||
        !advance(s,s->assignment.character[p],4)) return 0;
    s->connected&=(uint8_t)~bit(p); s->controlling&=(uint8_t)~bit(p);
    s->menu&=(uint8_t)~bit(p); s->away&=(uint8_t)~bit(p);
    s->control[p]=SUDEKIMP_PARTY_CONTROL_AI;
    if(s->phase==SUDEKIMP_PARTY_SWAP_WAIT_ACK) clear_swap(s);
    return 1;
}
int SudekiMpPartyOwnershipReleaseDeparted(SudekiMpPartyOwnership *s,unsigned p) {
    /* Native drain and departure are proved by the caller. This automatic
     * policy step must not consume the host's independently queued UI request. */
    if(!SudekiMpPartyOwnershipValid(s) || p==0 || p>=4 ||
        (s->connected&bit(p)) || !(s->assignment.humans&bit(p)) ||
        s->phase!=SUDEKIMP_PARTY_SWAP_IDLE ||
        s->control[p]!=SUDEKIMP_PARTY_CONTROL_AI ||
        !advance(s,s->assignment.character[p],4)) return 0;
    s->assignment.humans&=(uint8_t)~bit(p);
    s->assignment.character[p]=SUDEKIMP_PARTY_NO_CHARACTER;
    s->last_request[p]=0;
    return 1;
}
