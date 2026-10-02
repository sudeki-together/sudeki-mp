#include "hooks/lan_party_combo_hud.h"
#include "network/lan_party_motion.h"
#include "engine/log.h"
#include <stdint.h>
#include <string.h>

static struct {
    uint64_t token;
    uint32_t generation;
    void *actor,*hud;
    uint16_t sequence;
    unsigned depth;
} shown;

static BOOL memory(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && VirtualQuery(p,&m,sizeof(m)) && m.State==MEM_COMMIT &&
        !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) && a+n>=a &&
        a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}

/* Retail af0d0/af060 take the HUD in ESI; append consumes one stack argument.
 * Preserve the caller's ESI and let the native display routines own gizmos. */
static void __attribute__((naked,noinline)) hud_call(
    void *fn __attribute__((unused)),void *hud __attribute__((unused)),
    unsigned cue __attribute__((unused)),BOOL append __attribute__((unused))) {
    __asm__ volatile(
        "pushl %esi\n\t"
        "movl 12(%esp),%esi\n\t"
        "movl 8(%esp),%eax\n\t"
        "cmpl $0,20(%esp)\n\t"
        "je 1f\n\t"
        "pushl 16(%esp)\n\t"
        "1: call *%eax\n\t"
        "popl %esi\n\t"
        "ret\n\t");
}
static BOOL exact(uint8_t *base,const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyRosterObservation *r,void *hud) {
    static const uint8_t clear[]={0x51,0xb8,7,0,0,0,0x66,0xc7,0x86,0,2,0,0,0,0};
    static const uint8_t append[]={0x80,0xbe,3,2,0,0,0,0x74,0x60,0x33,0xc0};
    uint8_t *h=hud,*assets,*entries,*bank,*textures;
    if(!base || !w || !r || !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) ||
        !SudekiMpLanPartyControlNativeActorExact(r,2u) ||
        !memory(r->actors[2],4) || *(void **)r->actors[2]!=base+0x2d5010 ||
        !memory((uint8_t *)r->controller+0x248,4) ||
        *(void **)((uint8_t *)r->controller+0x248)!=r->actors[2] ||
        !memory(base+0x3c2fc4,4) || *(void **)(base+0x3c2fc4)!=h ||
        !memory(h,0x204) || *(void **)h!=base+0x2cb784 || *(uint32_t *)(h+0x48)!=3u ||
        memcmp(base+0xaf0d0,clear,sizeof(clear)) ||
        memcmp(base+0xaf060,append,sizeof(append)) || !memory(base+0x3c3038,4)) return FALSE;
    assets=*(uint8_t **)(base+0x3c3038);
    if(!memory(assets,0x14)) return FALSE;
    entries=*(uint8_t **)(assets+0x10);
    if(!memory(entries,0xc)) return FALSE;
    bank=*(uint8_t **)(entries+8);
    if(!memory(bank,0x14)) return FALSE;
    textures=*(uint8_t **)(bank+0x10);
    if(!memory(textures,8u*4u)) return FALSE;
    for(unsigned i=0;i<3;++i) {
        uint8_t *g=h+0x12c+i*0x40;
        void **v=*(void ***)g;
        if(!memory(v,0x18) || !v[5]) return FALSE;
    }
    return TRUE;
}
static unsigned cue_for(uint8_t variant,unsigned *cue) {
    switch(variant) {
    case SUDEKIMP_LAN_ARENA_ACTION_WEAK_ONE: *cue=0; return 1;
    case SUDEKIMP_LAN_ARENA_ACTION_STRONG: *cue=1; return 1;
    case SUDEKIMP_LAN_ARENA_ACTION_WEAK_TWO: *cue=0; return 2;
    case SUDEKIMP_LAN_ARENA_ACTION_STRONG_TWO: *cue=1; return 2;
    case SUDEKIMP_LAN_ARENA_ACTION_WEAK_THREE:
    case SUDEKIMP_LAN_ARENA_ACTION_COMBO_SWW:
    case SUDEKIMP_LAN_ARENA_ACTION_COMBO_SSW:
    case SUDEKIMP_LAN_ARENA_ACTION_COMBO_WSW: *cue=0; return 3;
    case SUDEKIMP_LAN_ARENA_ACTION_COMBO_WWS:
    case SUDEKIMP_LAN_ARENA_ACTION_COMBO_SSS:
    case SUDEKIMP_LAN_ARENA_ACTION_COMBO_SWS:
    case SUDEKIMP_LAN_ARENA_ACTION_COMBO_WSS:
    case SUDEKIMP_LAN_ARENA_ACTION_COMBO_WSS_ALTERNATE: *cue=1; return 3;
    default: return 0;
    }
}
BOOL SudekiMpLanPartyComboHudApply(HMODULE image,SudekiMpLanPartySession *session,
    const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *lease,const SudekiMpLanPartyFrame *frame) {
    SudekiMpLanPartyRosterObservation r;
    const SudekiMpLanArenaActorSnapshot *s;
    uint8_t *base=(uint8_t *)image,*hud;
    static DWORD last_trace;
    if(!base || !w || !lease || lease->seat!=2u || !lease->token || !lease->generation ||
        !SudekiMpLanPartyLeaseActive(session,lease) ||
        !SudekiMpLanPartyBasicCombatFrameValid(frame)) return FALSE;
    s=&frame->chunk[1].seat[0];
    DWORD now=GetTickCount();
    if(!last_trace || now-last_trace>=1000u) {
        last_trace=now;
        if(SudekiMpLanPartyControlObserveRoster(w,&r) && memory(base+0x3c2fc4,4) &&
            exact(base,w,&r,(hud=*(uint8_t **)(base+0x3c2fc4))))
            SudekiMpLogFormat("lan_party_combo_diag seat=2 tick=%lu host_tick=%lu sequence=%u shown=%u depth=%u variant=%u icons=%lu,%lu,%lu flags=%u,%u,%u,%u timer=%.3f duration=%.3f\r\n",
                (unsigned long)now,(unsigned long)frame->chunk[1].host_tick,s->action_sequence,shown.sequence,shown.depth,
                s->action_variant,(unsigned long)*(uint32_t *)(hud+0x1f4),
                (unsigned long)*(uint32_t *)(hud+0x1f8),(unsigned long)*(uint32_t *)(hud+0x1fc),
                hud[0x200],hud[0x201],hud[0x202],hud[0x203],
                *(float *)(hud+0x1ec),*(float *)(hud+0x1f0));
    }
    if(shown.token==lease->token && shown.generation==lease->generation &&
        shown.sequence==s->action_sequence) return TRUE; /* no native call */
    if(!SudekiMpLanPartyControlObserveRoster(w,&r) || !r.combat ||
        !memory(base+0x3c2fc4,4)) return FALSE;
    hud=*(uint8_t **)(base+0x3c2fc4);
    if(!exact(base,w,&r,hud)) return FALSE;
    if(shown.token!=lease->token || shown.generation!=lease->generation ||
        shown.actor!=r.actors[2] || shown.hud!=hud) {
        memset(&shown,0,sizeof(shown));
        shown.token=lease->token; shown.generation=lease->generation;
        shown.actor=r.actors[2]; shown.hud=hud;
        /* A joining idle view must not replay old history as a fresh combo. */
        if(!s->action_variant) { shown.sequence=s->action_sequence; return TRUE; }
    }
    for(unsigned i=0;i<s->action_history_count;++i) {
        const SudekiMpLanArenaActionEvent *event=&s->action_history[i];
        unsigned cue=0,depth;
        uint16_t delta=(uint16_t)(event->sequence-shown.sequence);
        if(!event->sequence || (shown.sequence && (!delta || delta>=0x8000u))) continue;
        depth=cue_for(event->variant,&cue);
        if(!depth || (uint32_t)(frame->chunk[1].host_tick-event->host_tick)>2500u) {
            shown.sequence=event->sequence; shown.depth=0; continue;
        }
        if(depth!=1u && depth!=shown.depth+1u) {
            shown.sequence=event->sequence; shown.depth=0; continue;
        }
        if(!exact(base,w,&r,hud)) return FALSE;
        if(depth==1u) hud_call(base+0xaf0d0,hud,0,FALSE);
        if(!exact(base,w,&r,hud)) return FALSE;
        /* A native timeout may have cleared the row between accepted edges.
         * Do not append stage two into an empty first slot or invent a miss. */
        unsigned occupied=0;
        while(occupied<3u && *(uint32_t *)(hud+0x1f4+4u*occupied)!=7u) ++occupied;
        if(occupied!=depth-1u) {
            shown.sequence=event->sequence; shown.depth=0; continue;
        }
        /* Publish the consumed display edge before entering native UI code;
         * a post-call identity failure must not duplicate it on retry. */
        shown.sequence=event->sequence; shown.depth=depth;
        hud_call(base+0xaf060,hud,cue,TRUE);
        if(!exact(base,w,&r,hud)) return FALSE;
    }
    return TRUE;
}
