#include "hooks/lan_party_presence.h"
#include <bcrypt.h>
#include <string.h>

static unsigned bit(unsigned p) { return p<4?1u<<p:0; }
static BOOL live(const SudekiMpLanPartyPresenceCoordinator *p) {
    return p && p->initialized && p->session && p->local_player<4;
}
static BOOL publish(SudekiMpLanPartyPresenceCoordinator *p,
    const SudekiMpLanPartyPresence *before,uint32_t now) {
    BOOL changed=!before || memcmp(&before->ownership,&p->current.ownership,
        sizeof(p->current.ownership)) ||
        memcmp(before->ack_request,p->current.ack_request,sizeof(before->ack_request)) ||
        memcmp(before->ack_result,p->current.ack_result,sizeof(before->ack_result));
    if(changed && ++p->current.sequence==0) ++p->current.sequence;
    p->current.observed_tick=now;
    return SudekiMpLanPartyPublishPresence(p->session,&p->current);
}
BOOL SudekiMpLanPartyPresenceInitialize(SudekiMpLanPartyPresenceCoordinator *p,
    SudekiMpLanPartySession *session,const SudekiMpLanPartyConfig *config) {
    SudekiMpLanPartyPresenceCoordinator next;
    SudekiMpPartyAssignment assignment={0};
    if(!p || !session || !config || config->local_seat>=4 ||
        config->local_seat!=SudekiMpLanPartyLocalSeat(session) || config->story_observation)
        return FALSE;
    memset(&next,0,sizeof(next)); next.session=session;
    next.local_player=config->local_seat; next.initialized=TRUE; next.next_request=1;
    next.observed.world_busy=1;
    for(unsigned i=0;i<4;++i)
        next.initial_character[i]=config->assignment_enabled?config->character[i]:(uint8_t)i;
    if(!next.local_player) {
        next.bound_players=1;
        if(BCryptGenRandom(NULL,(PUCHAR)&assignment.world,sizeof(assignment.world),
            BCRYPT_USE_SYSTEM_PREFERRED_RNG)!=0 || !assignment.world) return FALSE;
        assignment.revision=1; assignment.available=15;
        unsigned reservations=config->reserved_mask?config->reserved_mask:
            config->lobby_members?config->lobby_members:15;
        for(unsigned i=0;i<4;++i) {
            assignment.generation[i]=1; assignment.character[i]=4;
            if((reservations&bit(i)) && next.initial_character[i]<4) {
                assignment.humans|=(uint8_t)bit(i);
                assignment.character[i]=next.initial_character[i];
            }
        }
        if(!SudekiMpPartyOwnershipInitializePresence(&next.current.ownership,&assignment,
            1,assignment.character[0]<4?1:0) || !publish(&next,NULL,GetTickCount())) return FALSE;
    } else (void)SudekiMpLanPartyGetPresence(session,&next.current);
    *p=next; return TRUE;
}
BOOL SudekiMpLanPartyPresenceRead(SudekiMpLanPartyPresenceCoordinator *p,
    SudekiMpLanPartyPresence *out) {
    if(!live(p) || !out) return FALSE;
    if(p->local_player && !SudekiMpLanPartyGetPresence(p->session,&p->current)) return FALSE;
    if(!SudekiMpLanPartyPresenceValid(&p->current)) return FALSE;
    *out=p->current; return TRUE;
}
static BOOL sender_exact(SudekiMpLanPartyPresenceCoordinator *p,
    const SudekiMpLanPartyCommand *c) {
    SudekiMpLanPartyPeerStatus peer;
    if(!c->player) return !c->lease.token && !c->lease.generation && !c->lease.seat;
    return SudekiMpLanPartyPeerStatusGet(p->session,c->player,&peer) &&
        (peer.phase==SUDEKIMP_LAN_PARTY_PENDING || peer.phase==SUDEKIMP_LAN_PARTY_ACTIVE) &&
        peer.lease.seat==c->player && peer.lease.seat==c->lease.seat &&
        peer.lease.token==c->lease.token && peer.lease.generation==c->lease.generation;
}
static SudekiMpPartySwapResult apply(SudekiMpLanPartyPresenceCoordinator *p,
    const SudekiMpLanPartyCommand *c) {
    SudekiMpPartyOwnership *o=&p->current.ownership;
    unsigned character=o->assignment.character[c->player];
    if(!sender_exact(p,c) || !(o->connected&bit(c->player)))
        return SUDEKIMP_PARTY_SWAP_UNAUTHORIZED;
    if(c->world!=o->assignment.world || c->revision!=o->assignment.revision ||
        c->request<=o->last_request[c->player] ||
        (character<4?c->generation!=o->assignment.generation[character]:c->generation!=0))
        return SUDEKIMP_PARTY_SWAP_STALE;
    switch(c->kind) {
        case SUDEKIMP_LAN_PARTY_COMMAND_PRESENCE:
            return SudekiMpPartyPresenceRequest(o,c->player,c->request,c->world,c->revision,
                !!(c->value&1),!!(c->value&2));
        case SUDEKIMP_LAN_PARTY_COMMAND_SWAP:
            if(!(p->bound_players&bit(c->player))) return SUDEKIMP_PARTY_SWAP_BUSY;
            return SudekiMpPartySwapRequest(o,c->player,c->target,c->request,c->world,
                c->revision,p->observed.busy_characters,p->observed.world_busy);
        case SUDEKIMP_LAN_PARTY_COMMAND_POLICY:
            return SudekiMpPartySetAbsencePolicy(o,c->player,c->request,c->world,c->revision,
                (SudekiMpPartyAbsencePolicy)c->value);
        case SUDEKIMP_LAN_PARTY_COMMAND_PAUSE:
            return SudekiMpPartySetPaused(o,c->player,c->request,c->world,c->revision,c->value);
        case SUDEKIMP_LAN_PARTY_COMMAND_RELEASE:
            if((o->connected&bit(c->target)) && !(p->bound_players&bit(c->target)))
                return SUDEKIMP_PARTY_SWAP_BUSY;
            return SudekiMpPartyReleaseReservation(o,c->player,c->target,c->request,c->world,c->revision);
        case SUDEKIMP_LAN_PARTY_COMMAND_REASSIGN:
            if(!(p->bound_players&bit(c->target))) return SUDEKIMP_PARTY_SWAP_BUSY;
            return SudekiMpPartyHostReassignRequest(o,c->player,c->target,c->value,c->request,
                c->world,c->revision,p->observed.busy_characters,p->observed.world_busy);
        case SUDEKIMP_LAN_PARTY_COMMAND_CONTROL_ACK:
            if(!SudekiMpPartyControlAcknowledge(o,c->player,c->world,c->revision,c->generation))
                return SUDEKIMP_PARTY_SWAP_BUSY;
            p->bound_players|=(uint8_t)bit(c->player);
            o->last_request[c->player]=c->request; return SUDEKIMP_PARTY_SWAP_OK;
        case SUDEKIMP_LAN_PARTY_COMMAND_SWAP_ACK:
            if(!SudekiMpPartySwapAcknowledge(o,c->player,c->transaction,c->world,c->revision))
                return SUDEKIMP_PARTY_SWAP_BUSY;
            p->bound_players|=(uint8_t)bit(c->player);
            o->last_request[c->player]=c->request; return SUDEKIMP_PARTY_SWAP_OK;
        case SUDEKIMP_LAN_PARTY_COMMAND_REQUEST_PAUSE:
            p->pause_requests|=(uint8_t)bit(c->player);
            o->last_request[c->player]=c->request; return SUDEKIMP_PARTY_SWAP_OK;
        default: return SUDEKIMP_PARTY_SWAP_INVALID;
    }
}
static void cancel_unstarted(SudekiMpPartyOwnership *o) {
    if(o->phase!=SUDEKIMP_PARTY_SWAP_RESERVED) return;
    unsigned p=o->player,d=o->displaced;
    if(!(o->connected&bit(p)) || o->control[p]==SUDEKIMP_PARTY_CONTROL_DRAINING ||
        (d<4 && (o->connected&bit(d)) && !((o->menu|o->away)&bit(d))))
        (void)SudekiMpPartySwapCancel(o,p,o->request);
}
static void commands(SudekiMpLanPartyPresenceCoordinator *p) {
    SudekiMpLanPartyCommand command;
    /* The transport has one slot per player; bound service work even if a
     * peer concurrently produces another request as its slot is consumed. */
    for(unsigned n=0;n<4 && SudekiMpLanPartyTakeCommand(p->session,&command);++n) {
        if(command.player>=4) continue;
        SudekiMpPartySwapResult result=apply(p,&command);
        p->current.ack_request[command.player]=command.request;
        p->current.ack_result[command.player]=(uint8_t)result;
        cancel_unstarted(&p->current.ownership);
    }
}
static BOOL observation_exact(const SudekiMpPartyOwnership *o,
    const SudekiMpLanPartyPresenceNativeState *n,unsigned player) {
    unsigned c=o->assignment.character[player];
    return c<4 && n->assignment.world==o->assignment.world &&
        n->assignment.character[player]==c &&
        n->assignment.generation[c]==o->assignment.generation[c];
}
static void native_steps(SudekiMpLanPartyPresenceCoordinator *p,
    const SudekiMpLanPartyPresenceNativeState *n) {
    SudekiMpPartyOwnership *o=&p->current.ownership;
    for(unsigned i=1;i<4;++i) {
        if(!(n->connected_mask&bit(i)) && (o->connected&bit(i))) {
            p->bound_players&=(uint8_t)~bit(i);
            (void)SudekiMpPartyOwnershipDisconnect(o,i);
        } else if((n->connected_mask&bit(i)) && !(o->connected&bit(i))) {
            if(SudekiMpPartyOwnershipConnect(o,i)) {
                p->bound_players&=(uint8_t)~bit(i);
                p->current.ack_request[i]=0; p->current.ack_result[i]=0;
            }
        }
    }
    cancel_unstarted(o);
    if(o->phase!=SUDEKIMP_PARTY_SWAP_IDLE && n->swap_request==o->request &&
        n->assignment.world==o->assignment.world &&
        n->assignment.generation[o->target]==o->assignment.generation[o->target] &&
        (o->previous==4 || n->assignment.generation[o->previous]==o->assignment.generation[o->previous])) {
        if(o->phase==SUDEKIMP_PARTY_SWAP_RESERVED && n->swap_ready &&
            !n->world_busy && !(n->busy_characters&(bit(o->target)|bit(o->previous))))
            (void)SudekiMpPartySwapBegin(o,o->player,o->request);
        else if(o->phase==SUDEKIMP_PARTY_SWAP_HANDOFF) {
            if(n->swap_result==SUDEKIMP_LAN_PARTY_PRESENCE_SWAP_COMMITTED)
                (void)SudekiMpPartySwapCommitReleased(o,o->player,o->request);
            else if(n->swap_result==SUDEKIMP_LAN_PARTY_PRESENCE_SWAP_RESTORED)
                (void)SudekiMpPartySwapAbortRestored(o,o->player,o->request);
        }
    }
    for(unsigned i=0;i<4;++i) {
        if(!observation_exact(o,n,i)) continue;
        unsigned c=o->assignment.character[i]; uint32_t g=o->assignment.generation[c];
        if(o->phase!=SUDEKIMP_PARTY_SWAP_IDLE && (o->player==i || o->displaced==i)) {
            if(o->phase!=SUDEKIMP_PARTY_SWAP_WAIT_ACK || o->player!=i) continue;
            if(!(o->connected&bit(i)) && (n->native_ai_mask&bit(i))) {
                if(o->control[i]==SUDEKIMP_PARTY_CONTROL_ACQUIRING)
                    (void)SudekiMpPartyControlAcquireAbortRestored(o,i,o->assignment.world,g);
                (void)SudekiMpPartyOwnershipLeaveDrained(o,i);
                continue;
            }
            if(i==0 && (n->local_bound_mask&1) &&
                (o->control[i]==SUDEKIMP_PARTY_CONTROL_WAIT_ACK ||
                 (o->control[i]==SUDEKIMP_PARTY_CONTROL_AI && ((o->menu|o->away)&1)))) {
                (void)SudekiMpPartySwapAcknowledge(o,0,o->request,o->assignment.world,o->assignment.revision);
                continue;
            }
            /* New target ownership is acquired below before a remote swap
             * ACK can grant input; no client camera claim can replace it. */
        }
        switch(o->control[i]) {
            case SUDEKIMP_PARTY_CONTROL_DRAINING:
                if(n->native_ai_mask&bit(i))
                    (void)SudekiMpPartyControlDrainComplete(o,i,o->assignment.world,g);
                break;
            case SUDEKIMP_PARTY_CONTROL_AI:
                if(((n->native_ai_mask&bit(i)) ||
                    (i==0 && (n->native_owned_mask&1) && (n->local_bound_mask&1))) &&
                    !(n->busy_characters&bit(c)))
                    (void)SudekiMpPartyControlAcquireBegin(o,i,o->assignment.world,o->assignment.revision,g);
                break;
            case SUDEKIMP_PARTY_CONTROL_ACQUIRING:
                if(n->native_owned_mask&bit(i))
                    (void)SudekiMpPartyControlAcquireCommit(o,i,o->assignment.world,g);
                else if((n->native_ai_mask&bit(i)) &&
                    (!(o->connected&bit(i)) || ((o->menu|o->away)&bit(i))))
                    (void)SudekiMpPartyControlAcquireAbortRestored(o,i,o->assignment.world,g);
                break;
            case SUDEKIMP_PARTY_CONTROL_WAIT_ACK:
                if(i==0 && (n->local_bound_mask&1) && o->phase==SUDEKIMP_PARTY_SWAP_IDLE)
                    (void)SudekiMpPartyControlAcknowledge(o,0,o->assignment.world,o->assignment.revision,g);
                break;
            default: break;
        }
    }
}
BOOL SudekiMpLanPartyPresenceService(SudekiMpLanPartyPresenceCoordinator *p,
    const SudekiMpLanPartyPresenceNativeState *n,uint32_t now) {
    if(!live(p)) return FALSE;
    if(p->local_player) return SudekiMpLanPartyGetPresence(p->session,&p->current);
    if(!n || !SudekiMpPartyAssignmentValid(&n->assignment) ||
        n->assignment.world!=p->current.ownership.assignment.world ||
        !(n->connected_mask&1) || ((n->connected_mask|n->native_owned_mask|
        n->native_ai_mask|n->local_bound_mask|n->busy_characters)&~15u) ||
        (n->native_owned_mask&n->native_ai_mask) || (n->local_bound_mask&~1u) ||
        n->world_busy>1 || n->swap_ready>1 ||
        n->swap_result<SUDEKIMP_LAN_PARTY_PRESENCE_SWAP_NONE ||
        n->swap_result>SUDEKIMP_LAN_PARTY_PRESENCE_SWAP_RESTORED) return FALSE;
    SudekiMpLanPartyPresence before=p->current; p->observed=*n;
    native_steps(p,n); commands(p);
    return publish(p,&before,now);
}
BOOL SudekiMpLanPartyPresenceUiFrame(SudekiMpLanPartyPresenceCoordinator *p,uint32_t now) {
    if(!live(p)) return FALSE;
    if(p->local_player) return SudekiMpLanPartyGetPresence(p->session,&p->current);
    SudekiMpLanPartyPresence before=p->current;
    commands(p); return publish(p,&before,now);
}
static BOOL queue_stamped(SudekiMpLanPartyPresenceCoordinator *p,
    const SudekiMpLanPartyPresence *current,const SudekiMpLanPartyPresence *observed,
    SudekiMpLanPartyCommandKind kind,unsigned target,unsigned value,uint32_t transaction) {
    SudekiMpLanPartyCommand command={0};
    uint32_t floor=current->ownership.last_request[p->local_player];
    if(current->ack_request[p->local_player]>floor) floor=current->ack_request[p->local_player];
    if(floor==UINT32_MAX || p->next_request==UINT32_MAX) {
        SetLastError(ERROR_ARITHMETIC_OVERFLOW); return FALSE;
    }
    if(p->next_request<=floor) p->next_request=floor+1;
    command.request=p->next_request; command.world=observed->ownership.assignment.world;
    command.revision=observed->ownership.assignment.revision; command.transaction=transaction;
    command.kind=(uint8_t)kind; command.player=p->local_player;
    command.target=(uint8_t)target; command.value=(uint8_t)value;
    unsigned c=observed->ownership.assignment.character[p->local_player];
    if(c<4) command.generation=observed->ownership.assignment.generation[c];
    if(!SudekiMpLanPartyQueueCommand(p->session,&command)) return FALSE;
    ++p->next_request; return TRUE;
}
BOOL SudekiMpLanPartyPresenceQueue(SudekiMpLanPartyPresenceCoordinator *p,
    SudekiMpLanPartyCommandKind kind,unsigned target,unsigned value,uint32_t transaction) {
    SudekiMpLanPartyPresence state;
    if(!p || target>SUDEKIMP_PARTY_NO_CHARACTER || value>255 ||
        kind<SUDEKIMP_LAN_PARTY_COMMAND_PRESENCE ||
        kind>SUDEKIMP_LAN_PARTY_COMMAND_REQUEST_PAUSE) {
        SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
    }
    if(!live(p) || !SudekiMpLanPartyPresenceRead(p,&state)) {
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    return queue_stamped(p,&state,&state,kind,target,value,transaction);
}
BOOL SudekiMpLanPartyPresenceQueueLocal(SudekiMpLanPartyPresenceCoordinator *p,
    SudekiMpLanPartyCommandKind kind,unsigned value,uint32_t transaction) {
    switch(kind) {
        case SUDEKIMP_LAN_PARTY_COMMAND_PRESENCE:
        case SUDEKIMP_LAN_PARTY_COMMAND_POLICY:
        case SUDEKIMP_LAN_PARTY_COMMAND_PAUSE:
        case SUDEKIMP_LAN_PARTY_COMMAND_REQUEST_PAUSE:
            return SudekiMpLanPartyPresenceQueue(p,kind,
                SUDEKIMP_PARTY_NO_CHARACTER,value,transaction);
        default:
            SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
    }
}
BOOL SudekiMpLanPartyPresenceQueueAcknowledgment(SudekiMpLanPartyPresenceCoordinator *p,
    const SudekiMpLanPartyPresence *observed,SudekiMpLanPartyCommandKind kind,
    uint32_t transaction) {
    SudekiMpLanPartyPresence current,proof;
    if(!p || !SudekiMpLanPartyPresenceValid(observed) ||
        (kind!=SUDEKIMP_LAN_PARTY_COMMAND_CONTROL_ACK &&
         kind!=SUDEKIMP_LAN_PARTY_COMMAND_SWAP_ACK)) {
        SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
    }
    proof=*observed;
    if(!live(p) || !SudekiMpLanPartyPresenceRead(p,&current)) {
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    return queue_stamped(p,&current,&proof,kind,SUDEKIMP_PARTY_NO_CHARACTER,0,transaction);
}
BOOL SudekiMpLanPartyPresenceReserve(SudekiMpLanPartyPresenceCoordinator *p,
    unsigned player,unsigned character,uint32_t now) {
    if(!live(p) || p->local_player) return FALSE;
    SudekiMpLanPartyPresence before=p->current;
    if(!SudekiMpPartyOwnershipReserve(&p->current.ownership,0,player,character,
        p->current.ownership.assignment.world,p->current.ownership.assignment.revision)) return FALSE;
    return publish(p,&before,now);
}
void SudekiMpLanPartyPresenceReset(SudekiMpLanPartyPresenceCoordinator *p) {
    if(p) memset(p,0,sizeof(*p));
}
BOOL SudekiMpLanPartyPresenceReleaseDeparted(SudekiMpLanPartyPresenceCoordinator *p,
    unsigned player,uint32_t now) {
    SudekiMpLanPartyPeerStatus peer;
    if(!live(p) || p->local_player || !player || player>=4 ||
        !SudekiMpLanPartyPeerStatusGet(p->session,player,&peer) ||
        peer.phase!=SUDEKIMP_LAN_PARTY_FREE ||
        (p->observed.connected_mask&bit(player)) ||
        !(p->observed.native_ai_mask&bit(player)) ||
        !observation_exact(&p->current.ownership,&p->observed,player)) return FALSE;
    SudekiMpLanPartyPresence before=p->current;
    if(!SudekiMpPartyOwnershipReleaseDeparted(&p->current.ownership,player)) return FALSE;
    p->current.ack_request[player]=0; p->current.ack_result[player]=0;
    if(!publish(p,&before,now)) {
        /* The lobby must not expose a slot while transport still publishes
         * its previous owner. Retry the same retained claim on failure. */
        p->current=before;
        return FALSE;
    }
    p->bound_players&=(uint8_t)~bit(player);
    p->pause_requests&=(uint8_t)~bit(player);
    return TRUE;
}
