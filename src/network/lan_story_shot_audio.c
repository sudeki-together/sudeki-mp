#include "network/lan_story_shot_audio.h"
#include <string.h>

/* Pure plain-data planner (no native memory, no clock, no log). Every host
 * tick and every instance comparison is an int32 difference. */
enum { CUE_NONE=0, CUE_PENDING=1, CUE_LIVE=2, CUE_DECIDED=3 };
enum { SEEN_PENDING=1, SEEN_LIVE=2, SEEN_DECIDED=3 };
enum {
    TRAVEL_BIND_MS=200,  /* party travel: no journal shot by then is silence */
    WORLD_FIRST_MS=250,  /* world missile: first sight older than this is a late join */
    STALE_MS=250,        /* local clock: no fresh roster for this long retires everything */
    BIND_BEFORE_MS=30,   /* shot host tick may trail the VFX emitted tick by 30 ms */
    BIND_AFTER_MS=100,   /* ... or lead it by 100 ms (the shot_for rule of the muzzle cache) */
    WORLD_CHARACTER=4
};
/* Party projectile (travel) kinds only. Muzzle flashes (Ailish 35-39, Elco
 * 52-57, which include every aimed-flash kind) and hits (40-42, 58) never are. */
enum {
    AILISH_TRAVEL_FIRST=SUDEKIMP_LAN_STORY_VFX_AILISH_SHOT_FIRST+8,
    AILISH_TRAVEL_LAST=SUDEKIMP_LAN_STORY_VFX_AILISH_SHOT_LAST,
    ELCO_TRAVEL_FIRST=SUDEKIMP_LAN_STORY_VFX_ELCO_SHOT_FIRST+7,
    ELCO_TRAVEL_LAST=SUDEKIMP_LAN_STORY_VFX_ELCO_SHOT_LAST
};
_Static_assert(AILISH_TRAVEL_FIRST==43 && AILISH_TRAVEL_LAST==51 &&
    ELCO_TRAVEL_FIRST==59 && ELCO_TRAVEL_LAST==66,"Party projectile VFX kinds moved");
_Static_assert(SUDEKIMP_SHOT_AUDIO_SEEN==SUDEKIMP_STORY_VFX_CAPACITY,"Seen table must cover one roster");

typedef struct Out { SudekiMpStoryShotAudioAction *a; unsigned n,max; } Out;
static SudekiMpStoryShotAudioAction *push(Out *o,unsigned type) {
    if(o->n>=o->max) return NULL;
    SudekiMpStoryShotAudioAction *a=&o->a[o->n++];
    memset(a,0,sizeof(*a)); a->type=(uint8_t)type; return a;
}
static BOOL older(uint32_t a,uint32_t b) { return (int32_t)(a-b)<0; }
/* 1..3 = shooter character (1 Elco, 3 Ailish) of a party projectile;
 * WORLD_CHARACTER = allow-listed Kurian Shooter missile; 0 = not a shot cue. */
static unsigned classify(const SudekiMpLanArenaSpiritVfxSnapshot *v) {
    if(v->skill_sequence) return 0;
    if(v->owner_actor_type==SUDEKIMP_LAN_ARENA_AILISH_TYPE &&
        v->kind>=AILISH_TRAVEL_FIRST && v->kind<=AILISH_TRAVEL_LAST) return 3u;
    if(v->owner_actor_type==SUDEKIMP_LAN_ARENA_ELCO_TYPE &&
        v->kind>=ELCO_TRAVEL_FIRST && v->kind<=ELCO_TRAVEL_LAST) return 1u;
    if(v->owner_actor_type==255u && (v->kind==SUDEKIMP_LAN_STORY_VFX_WORLD_SFXKK105_PROJECTILE02 ||
        v->kind==SUDEKIMP_LAN_STORY_VFX_WORLD_SFXKK106_PROJECTILE03)) return WORLD_CHARACTER;
    return 0;
}
static BOOL same_identity(const SudekiMpStoryShotAudioSeen *s,const SudekiMpLanArenaSpiritVfxSnapshot *v) {
    return s->state && !v->skill_sequence && s->instance==v->instance_sequence &&
        s->emitted_tick==v->emitted_host_tick && s->kind==v->kind && s->owner==v->owner_actor_type;
}
static BOOL live(const SudekiMpStoryShotAudioSeen *s) {
    return s->state && (s->fire_state==CUE_LIVE || s->travel_state==CUE_LIVE);
}
static void settle(SudekiMpStoryShotAudioSeen *s) {
    s->state=(uint8_t)(s->fire_state==CUE_PENDING || s->travel_state==CUE_PENDING ? SEEN_PENDING :
        s->fire_state==CUE_LIVE || s->travel_state==CUE_LIVE ? SEEN_LIVE : SEEN_DECIDED);
}
static void describe(SudekiMpStoryShotAudioAction *a,const SudekiMpStoryShotAudioSeen *s,const float position[3]) {
    a->character=s->character; a->item=s->item; a->vfx_kind=s->kind;
    a->key=s->instance; a->emitted_tick=s->emitted_tick;
    memcpy(a->position,position,sizeof(a->position));
}
/* Occupied seen slots (or roster entries) in ascending instance order. */
static unsigned seen_order(const SudekiMpStoryShotAudioPlan *p,uint8_t order[SUDEKIMP_SHOT_AUDIO_SEEN]) {
    unsigned n=0;
    for(unsigned i=0;i<SUDEKIMP_SHOT_AUDIO_SEEN;++i) {
        if(!p->seen[i].state) continue;
        unsigned at=n++;
        while(at && older(p->seen[i].instance,p->seen[order[at-1u]].instance)) { order[at]=order[at-1u]; --at; }
        order[at]=(uint8_t)i;
    }
    return n;
}
static unsigned roster_order(const SudekiMpStoryVfx *v,uint8_t order[SUDEKIMP_STORY_VFX_CAPACITY]) {
    unsigned n=0;
    for(unsigned i=0;i<v->count;++i) {
        unsigned at=n++;
        while(at && older(v->entries[i].instance_sequence,v->entries[order[at-1u]].instance_sequence)) {
            order[at]=order[at-1u]; --at;
        }
        order[at]=(uint8_t)i;
    }
    return n;
}
static SudekiMpStoryShotAudioSeen *find(SudekiMpStoryShotAudioPlan *p,uint32_t instance) {
    for(unsigned i=0;i<SUDEKIMP_SHOT_AUDIO_SEEN;++i)
        if(p->seen[i].state && p->seen[i].instance==instance) return &p->seen[i];
    return NULL;
}
/* RETIRE every live entry that fits and forget every entry it can (a pending
 * or decided entry has no executor record). TRUE when nothing is left. */
static BOOL retire_all(SudekiMpStoryShotAudioPlan *p,Out *o) {
    uint8_t order[SUDEKIMP_SHOT_AUDIO_SEEN]; unsigned n=seen_order(p,order); BOOL done=TRUE;
    for(unsigned i=0;i<n;++i) {
        SudekiMpStoryShotAudioSeen *s=&p->seen[order[i]];
        if(live(s)) {
            SudekiMpStoryShotAudioAction *a=push(o,SUDEKIMP_SHOT_AUDIO_RETIRE);
            if(!a) { done=FALSE; continue; }
            describe(a,s,s->position);
        }
        memset(s,0,sizeof(*s));
    }
    return done;
}
static const SudekiMpLanArenaSpiritVfxSnapshot *roster_entry(const SudekiMpStoryVfx *v,
    const SudekiMpStoryShotAudioSeen *s) {
    for(unsigned i=0;i<v->count;++i) if(same_identity(s,&v->entries[i])) return &v->entries[i];
    return NULL;
}
/* D: absent from a complete fresh roster -> RETIRE (live) and forget. */
static void retire_absent(SudekiMpStoryShotAudioPlan *p,const SudekiMpStoryVfx *v,Out *o) {
    uint8_t order[SUDEKIMP_SHOT_AUDIO_SEEN]; unsigned n=seen_order(p,order);
    for(unsigned i=0;i<n;++i) {
        SudekiMpStoryShotAudioSeen *s=&p->seen[order[i]];
        if(roster_entry(v,s)) continue;
        if(live(s)) {
            SudekiMpStoryShotAudioAction *a=push(o,SUDEKIMP_SHOT_AUDIO_RETIRE);
            if(!a) continue; /* Retried next step; the entry keeps its slot. */
            describe(a,s,s->position);
        }
        memset(s,0,sizeof(*s));
    }
}
/* D: a live travel cue follows its projectile when the position bits change. */
static void moves(SudekiMpStoryShotAudioPlan *p,const SudekiMpStoryVfx *v,Out *o) {
    uint8_t order[SUDEKIMP_SHOT_AUDIO_SEEN]; unsigned n=seen_order(p,order);
    for(unsigned i=0;i<n;++i) {
        const SudekiMpStoryShotAudioSeen *s=&p->seen[order[i]];
        const SudekiMpLanArenaSpiritVfxSnapshot *e;
        if(s->travel_state!=CUE_LIVE || !(e=roster_entry(v,s)) ||
            !memcmp(e->position,s->position,sizeof(s->position))) continue;
        SudekiMpStoryShotAudioAction *a=push(o,SUDEKIMP_SHOT_AUDIO_MOVE);
        if(!a) return;
        describe(a,s,e->position);
    }
}
static BOOL shooter_present(const SudekiMpStoryShotAudioInput *in,const SudekiMpLanStoryFrame *j,unsigned c) {
    const SudekiMpLanStoryActor *a=&j->actors[c];
    return ((j->available_mask&in->available_mask)>>c&1u) && a->character==c && a->generation &&
        SudekiMpLanStoryShotsValid(&a->shots,c);
}
/* A: one FIRE per host journal shot. Namespace = input token (reset in
 * Step) + journal epoch + actor generation; a namespace change, or
 * a journal that restarted below the cursor, seeds to latest and plays
 * nothing, so a late join/reconnect never replays history. */
static void party_fire(SudekiMpStoryShotAudioPlan *p,const SudekiMpStoryShotAudioInput *in,Out *o) {
    static const unsigned shooters[2]={1u,3u};
    const SudekiMpLanStoryFrame *j=in->journal;
    if(!j) return; /* Unknown retains the cursors. */
    BOOL newer=!p->journal_sequence_known || (int32_t)(j->sequence-p->journal_sequence)>0;
    if(j->epoch!=p->journal_epoch) { memset(p->seeded,0,sizeof(p->seeded)); p->journal_epoch=j->epoch; newer=TRUE; }
    if(newer) { p->journal_sequence=j->sequence; p->journal_sequence_known=1; }
    for(unsigned k=0;k<2u;++k) {
        unsigned c=shooters[k]; const SudekiMpLanStoryShots *shots=&j->actors[c].shots;
        if(!shooter_present(in,j,c)) { p->seeded[c]=0; continue; }
        /* An older view of the same journal: keep the cursor, play nothing. */
        if(p->seeded[c] && p->generation[c]==j->actors[c].generation && shots->latest<p->cursor[c] && !newer) continue;
        if(!p->seeded[c] || p->generation[c]!=j->actors[c].generation || shots->latest<p->cursor[c]) {
            p->seeded[c]=1; p->generation[c]=j->actors[c].generation; p->cursor[c]=shots->latest;
            continue;
        }
        const SudekiMpLanStoryShot *e=SudekiMpLanStoryShotNext(shots,c,j->host_tick,p->cursor[c]);
        if(!e) continue;
        SudekiMpStoryShotAudioAction *a=push(o,SUDEKIMP_SHOT_AUDIO_PARTY_FIRE);
        if(!a) return;
        a->character=(uint8_t)c; a->item=e->item; a->local=(uint8_t)(c==in->local_character);
        a->key=e->sequence; a->emitted_tick=e->host_tick;
        memcpy(a->position,e->origin,sizeof(a->position));
    }
}
/* B: the journal shot of the same character nearest the VFX emitted tick,
 * within -30..+100 ms; it supplies the weapon item. No muzzle-cache input. */
static const SudekiMpLanStoryShot *travel_shot(const SudekiMpStoryShotAudioInput *in,const SudekiMpStoryVfx *v,
    unsigned c,uint32_t tick) {
    const SudekiMpLanStoryFrame *j=in->journal; const SudekiMpLanStoryShot *best=NULL; uint32_t distance=UINT32_MAX;
    if(!j || j->epoch!=v->epoch || j->revision!=v->revision || !shooter_present(in,j,c)) return NULL;
    for(unsigned i=0;i<j->actors[c].shots.count;++i) {
        const SudekiMpLanStoryShot *s=&j->actors[c].shots.events[i];
        int32_t delta=(int32_t)(tick-s->host_tick);
        if(delta<-BIND_BEFORE_MS || delta>BIND_AFTER_MS) continue;
        uint32_t d=(uint32_t)(delta<0?-delta:delta);
        if(d<distance) { best=s; distance=d; }
    }
    return best;
}
/* B/C: pending cues of one tracked roster entry, while its age rule allows. */
static void pending_cues(SudekiMpStoryShotAudioSeen *s,const SudekiMpLanArenaSpiritVfxSnapshot *e,
    const SudekiMpStoryShotAudioInput *in,const SudekiMpStoryVfx *v,Out *o) {
    int32_t age=(int32_t)(v->tick-s->emitted_tick);
    if(s->character==WORLD_CHARACTER) {
        if(age<0 || age>WORLD_FIRST_MS) {
            if(s->fire_state==CUE_PENDING) s->fire_state=CUE_DECIDED;
            if(s->travel_state==CUE_PENDING) s->travel_state=CUE_DECIDED;
        } else {
            SudekiMpStoryShotAudioAction *a;
            /* Fire stays at the first-sight point; travel starts where the projectile is now. */
            if(s->fire_state==CUE_PENDING && (a=push(o,SUDEKIMP_SHOT_AUDIO_WORLD_FIRE))) describe(a,s,s->position);
            if(s->travel_state==CUE_PENDING && (a=push(o,SUDEKIMP_SHOT_AUDIO_WORLD_TRAVEL))) describe(a,s,e->position);
        }
    } else if(s->travel_state==CUE_PENDING) {
        const SudekiMpLanStoryShot *shot;
        if(age<0 || age>TRAVEL_BIND_MS) s->travel_state=CUE_DECIDED;
        else if((shot=travel_shot(in,v,s->character,s->emitted_tick))) {
            SudekiMpStoryShotAudioAction *a=push(o,SUDEKIMP_SHOT_AUDIO_PARTY_TRAVEL);
            if(a) { describe(a,s,e->position); a->item=shot->item; }
        }
    }
    settle(s);
}
/* B/C/E/H: new cues in ascending instance order. An unseen entry at or
 * below the newest instance known at the start of the step is a
 * resurrection and stays silent; so does one that finds no free slot. */
static void vfx_cues(SudekiMpStoryShotAudioPlan *p,const SudekiMpStoryShotAudioInput *in,
    const SudekiMpStoryVfx *v,Out *o) {
    uint8_t order[SUDEKIMP_STORY_VFX_CAPACITY]; unsigned n=roster_order(v,order);
    uint32_t previous=p->newest;
    for(unsigned i=0;i<n;++i) {
        const SudekiMpLanArenaSpiritVfxSnapshot *e=&v->entries[order[i]];
        SudekiMpStoryShotAudioSeen *s=find(p,e->instance_sequence);
        if(s) { if(same_identity(s,e)) pending_cues(s,e,in,v,o); continue; }
        if(previous && (int32_t)(e->instance_sequence-previous)<=0) continue;
        if(!p->newest || (int32_t)(e->instance_sequence-p->newest)>0) p->newest=e->instance_sequence;
        unsigned character=classify(e);
        if(!character) continue;
        for(unsigned k=0;k<SUDEKIMP_SHOT_AUDIO_SEEN && !s;++k) if(!p->seen[k].state) s=&p->seen[k];
        if(!s) { p->overflow_logged=1; continue; }
        /* First sight; pending_cues applies the age rule (a late join is silent). */
        *s=(SudekiMpStoryShotAudioSeen){.instance=e->instance_sequence,.emitted_tick=e->emitted_host_tick,
            .kind=e->kind,.owner=e->owner_actor_type,.character=(uint8_t)character,
            /* Party fire comes from the journal, not the roster. */
            .fire_state=(uint8_t)(character==WORLD_CHARACTER ? CUE_PENDING : CUE_DECIDED),.travel_state=CUE_PENDING};
        memcpy(s->position,e->position,sizeof(s->position));
        pending_cues(s,e,in,v,o);
    }
}
static BOOL vfx_fresh(const SudekiMpStoryVfx *v) {
    /* An unobserved roster is unknown, never an empty deletion roster. */
    return v && v->observed==1u && SudekiMpStoryVfxValid(v);
}
void SudekiMpStoryShotAudioPlanReset(SudekiMpStoryShotAudioPlan *plan) { if(plan) memset(plan,0,sizeof(*plan)); }
unsigned SudekiMpStoryShotAudioPlanStep(SudekiMpStoryShotAudioPlan *plan,
    const SudekiMpStoryShotAudioInput *input,SudekiMpStoryShotAudioAction *out,unsigned max) {
    if(!plan || !input || (!out && max)) return 0;
    Out o={out,0,max};
    /* F: a new session/lease namespace retires everything and forgets it all. */
    if(plan->namespace_token!=input->namespace_token) {
        if(!retire_all(plan,&o)) return o.n;
        SudekiMpStoryShotAudioPlanReset(plan); plan->namespace_token=input->namespace_token;
    }
    const SudekiMpStoryVfx *v=vfx_fresh(input->vfx) ? input->vfx : NULL;
    if(!v) {
        /* D: stale roster. Keep newest so nothing older ever starts again. */
        if(plan->vfx_known && (int32_t)(input->now-plan->last_fresh)>STALE_MS) retire_all(plan,&o);
    } else {
        plan->last_fresh=input->now;
        if(!plan->vfx_known || plan->vfx_stream!=v->stream || plan->vfx_epoch!=v->epoch) {
            /* F: a new instance namespace. Adopt it only after every retire fit. */
            if(retire_all(plan,&o)) {
                plan->vfx_known=1; plan->vfx_stream=v->stream; plan->vfx_epoch=v->epoch;
                plan->vfx_revision=v->revision; plan->newest=0;
            } else v=NULL;
        } else if(plan->vfx_revision!=v->revision) {
            /* Same instance namespace, new party roster: retire, never replay. */
            if(retire_all(plan,&o)) plan->vfx_revision=v->revision; else v=NULL;
        }
        if(v) { retire_absent(plan,v,&o); moves(plan,v,&o); }
    }
    party_fire(plan,input,&o);
    if(v) vfx_cues(plan,input,v,&o);
    return o.n;
}
void SudekiMpStoryShotAudioPlanCommit(SudekiMpStoryShotAudioPlan *plan,
    const SudekiMpStoryShotAudioAction *action,unsigned result) {
    /* DEFER (or an unknown result) changes nothing: the age rules expire it. */
    if(!plan || !action || (result!=SUDEKIMP_SHOT_AUDIO_PLAYED && result!=SUDEKIMP_SHOT_AUDIO_SILENT)) return;
    unsigned c=action->character; BOOL played=result==SUDEKIMP_SHOT_AUDIO_PLAYED;
    if(action->type==SUDEKIMP_SHOT_AUDIO_PARTY_FIRE) {
        if((c==1u || c==3u) && plan->seeded[c] && action->key>plan->cursor[c]) plan->cursor[c]=action->key;
        return;
    }
    if(action->type==SUDEKIMP_SHOT_AUDIO_RETIRE) return; /* Forgotten at Step; the executor owns the stop. */
    SudekiMpStoryShotAudioSeen *s=find(plan,action->key);
    if(!s || s->kind!=action->vfx_kind || s->character!=c) return;
    BOOL world=c==WORLD_CHARACTER;
    switch(action->type) {
    case SUDEKIMP_SHOT_AUDIO_PARTY_TRAVEL:
        if(world || s->travel_state!=CUE_PENDING) return;
        s->travel_state=(uint8_t)(played ? CUE_LIVE : CUE_DECIDED);
        if(played) { s->item=action->item; memcpy(s->position,action->position,sizeof(s->position)); }
        break;
    case SUDEKIMP_SHOT_AUDIO_WORLD_FIRE:
        if(!world || s->fire_state!=CUE_PENDING) return;
        s->fire_state=(uint8_t)(played ? CUE_LIVE : CUE_DECIDED);
        break;
    case SUDEKIMP_SHOT_AUDIO_WORLD_TRAVEL:
        if(!world || s->travel_state!=CUE_PENDING) return;
        s->travel_state=(uint8_t)(played ? CUE_LIVE : CUE_DECIDED);
        /* A still-pending fire keeps its first-sight point in `position`. */
        if(played && s->fire_state!=CUE_PENDING) memcpy(s->position,action->position,sizeof(s->position));
        break;
    case SUDEKIMP_SHOT_AUDIO_MOVE:
        if(s->travel_state!=CUE_LIVE) return;
        if(s->fire_state!=CUE_PENDING) memcpy(s->position,action->position,sizeof(s->position));
        break;
    default: return;
    }
    settle(s);
}
