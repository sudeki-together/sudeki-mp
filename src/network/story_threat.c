#include "network/story_threat.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <float.h>

unsigned SudekiMpStoryThreatSeatKind(const SudekiMpStoryThreatSeat *s,uint32_t now) {
    if(!s || s->down) return STORY_SEAT_DOWN;
    if(s->absent) return STORY_SEAT_ABSENT;
    if(s->away) return STORY_SEAT_AWAY;
    int fresh=s->receipt && (int32_t)(now-s->receipt)>=-100 &&
        (int32_t)(now-s->receipt)<=(int32_t)s->max_age;
    if(s->native_ai || s->owned_ai || (s->connected && (!fresh || s->held || s->draining)))
        return STORY_SEAT_AI;
    if(s->connected && s->ready && fresh && !s->held && !s->draining) return STORY_SEAT_REMOTE;
    return s->host?STORY_SEAT_HOST:STORY_SEAT_AI;
}
const char *SudekiMpStoryThreatSeatName(unsigned k) {
    static const char *const names[]={"down","absent","away","ai","human_remote","human_host"};
    return k<6?names[k]:"unknown";
}
const char *const SudekiMpStoryThreatKeys[STORY_THRESHOLD_COUNT]={
    "EnemyTargetingLeadMarginFraction","EnemyTargetingLeadMarginUnits","EnemyTargetingThreatHalfLife",
    "EnemyTargetingD0","EnemyTargetingThreatFloor","EnemyTargetingThreatMargin","EnemyTargetingFireRecent",
    "EnemyTargetingMeleeReach","EnemyTargetingFarDistance","EnemyTargetingAlertThreshold","EnemyTargetingAlertHold",
    "EnemyTargetingAlertRadius","EnemyTargetingAlertRanged","EnemyTargetingAlertMelee","EnemyTargetingFadeSeconds",
    "EnemyTargetingCloseDistance","EnemyTargetingAlertCooldown","EnemyTargetingIdleSeconds",
    "EnemyTargetingIdleRadius","EnemyTargetingPushHold","EnemyTargetingPushCooldown",
    "CompanionDefendMarginFraction","CompanionDefendMarginUnits"};
void SudekiMpStoryThreatDefaults(SudekiMpStoryThreatConfig *c) {
    static const float defaults[]={.20f,2,4,8,.05f,.25f,1,3,12,.10f,6,25,2,1,3,6,10,5,20,8,15,.20f,2};
    memset(c,0,sizeof(*c)); c->level=STORY_TARGET_WATCH;c->cap_parity=1;c->lead_always=1;
    memcpy(c->value,defaults,sizeof(defaults)); strcpy(c->excluded,"MON_GuardSparPartner_Melee");
}
static int eq(const char *a,const char *b) {
    while(*a && *b && tolower((unsigned char)*a)==tolower((unsigned char)*b)) {++a;++b;}
    return !*a && !*b;
}
int SudekiMpStoryThreatParse(SudekiMpStoryThreatConfig *c,const char *k,const char *v) {
    if(!c || !k || !v) return 0;
    if(eq(k,"EnemyTargeting")) {
        static const char *const levels[]={"off","watch","tier0","tier1","tier15","tier2"};
        for(unsigned i=0;i<6;++i) if(eq(v,levels[i])) {c->level=i;return 1;}
        return 0;
    }
    if(eq(k,"EnemyTargetingCapParity")) {
        if(!eq(v,"on") && !eq(v,"off")) return 0;
        c->cap_parity=eq(v,"on");return 1;
    }
    if(eq(k,"EnemyTargetingLeadRule")) {
        if(!eq(v,"always") && !eq(v,"host_down")) return 0;
        c->lead_always=eq(v,"always");return 1;
    }
    if(eq(k,"EnemyTargetingExcludeTypes")) {
        if(strlen(v)>=sizeof(c->excluded)) return 0;
        strcpy(c->excluded,v);return 1;
    }
    for(unsigned i=0;i<STORY_THRESHOLD_COUNT;++i) if(eq(k,SudekiMpStoryThreatKeys[i])) {
        char *end;float n=strtof(v,&end);
        if(end==v || *end || !isfinite(n) || n<0 || n>10000 ||
            ((i==STORY_THREAT_HALF_LIFE || i==STORY_D0) && n==0) ||
            ((i==STORY_ALERT_RANGED || i==STORY_ALERT_MELEE) && (n>32 || floorf(n)!=n))) return 0;
        c->value[i]=n;return 1;
    }
    return 0;
}
unsigned SudekiMpStoryThreatEffectiveLevel(const SudekiMpStoryThreatConfig *c) {
    return c->level>STORY_TARGET_TIER0 && !c->cap_parity?STORY_TARGET_TIER0:c->level;
}
int SudekiMpStoryThreatExcluded(const SudekiMpStoryThreatConfig *c,const char *name) {
    if(!c || !name) return 0;
    const char *p=c->excluded;
    while(*p) {
        while(*p==',' || *p==';' || isspace((unsigned char)*p)) ++p;
        const char *start=p;while(*p && *p!=',' && *p!=';') ++p;
        const char *end=p;while(end>start && isspace((unsigned char)end[-1])) --end;
        size_t n=(size_t)(end-start);
        if(strlen(name)==n) {
            size_t i=0;for(;i<n && tolower((unsigned char)start[i])==tolower((unsigned char)name[i]);++i) {}
            if(i==n) return 1;
        }
    }
    return 0;
}
int SudekiMpStoryThreatResourceExcluded(const SudekiMpStoryThreatConfig *c,uint32_t resource) {
    if(!c || !resource) return 0;
    const char *p=c->excluded;
    while(*p) {
        while(*p==',' || *p==';' || isspace((unsigned char)*p)) ++p;
        const char *start=p;while(*p && *p!=',' && *p!=';') ++p;
        const char *end=p;while(end>start && isspace((unsigned char)end[-1])) --end;
        /* Native resource checksum, documented in docs/resource-swap.md and
         * independently implemented by tools/sudekimod.py. */
        uint32_t hash=0;
        for(size_t i=0;i<(size_t)(end-start);++i) {
            unsigned char v=(unsigned char)start[i];if(v>='a' && v<='z') v-=32;
            hash=(i&1)?hash*v:hash+v;
        }
        if(end>start && hash==resource) return 1;
    }
    return 0;
}
int SudekiMpStoryThreatCapParity(const SudekiMpStoryThreatConfig *c,int coop,
    unsigned kind,unsigned type,int excluded) {
    return c && coop && c->cap_parity && c->level>=STORY_TARGET_TIER0 &&
        c->level<=STORY_TARGET_TIER2 && (kind==STORY_SEAT_REMOTE || kind==STORY_SEAT_AWAY) &&
        type>=1 && type<=3 && !excluded;
}
unsigned SudekiMpStoryThreatLeadPick(const SudekiMpStoryThreatConfig *c,int coop,
    unsigned type,int excluded,unsigned mode,int host_standing,unsigned current,
    unsigned eligible,const unsigned kinds[4],const float distances[4]) {
    if(!c || !kinds || !distances || !coop || type<1 || type>3 || excluded || mode!=11 ||
        c->level<STORY_TARGET_TIER0 || c->level>STORY_TARGET_TIER2 ||
        (!c->lead_always && host_standing)) return 4;
    unsigned humans=0,living=0;
    for(unsigned i=0;i<4;++i) {
        if(!(eligible&(1u<<i)) || !isfinite(distances[i]) || distances[i]<0) continue;
        if(kinds[i]==STORY_SEAT_HOST || kinds[i]==STORY_SEAT_REMOTE) humans|=1u<<i;
        else if(kinds[i]!=STORY_SEAT_AI && kinds[i]!=STORY_SEAT_AWAY) continue;
        living|=1u<<i;
    }
    unsigned pool=humans?humans:living,best=4;
    for(unsigned i=0;i<4;++i)
        if((pool&(1u<<i)) && (best==4 || distances[i]<distances[best])) best=i;
    if(best<4 && current<4 && (pool&(1u<<current)) &&
        distances[current]-distances[best]<=fmaxf(c->value[STORY_LEAD_MARGIN_FRACTION]*distances[current],
            c->value[STORY_LEAD_MARGIN_UNITS])) return current;
    return best;
}
unsigned SudekiMpStoryThreatDefendAnchor(const SudekiMpStoryThreatConfig *c,int coop,
    unsigned self,unsigned current,unsigned eligible,const unsigned kinds[4],const float distances[4]) {
    if(!c || !kinds || !distances || !coop || self>=4 ||
        c->level<STORY_TARGET_TIER0 || c->level>STORY_TARGET_TIER2 ||
        !(eligible&(1u<<self)) || (kinds[self]!=STORY_SEAT_AI && kinds[self]!=STORY_SEAT_AWAY)) return 4;
    unsigned humans=0,others=0;
    for(unsigned i=0;i<4;++i) {
        if(i==self || !(eligible&(1u<<i)) || !isfinite(distances[i]) || distances[i]<0) continue;
        if(kinds[i]==STORY_SEAT_HOST || kinds[i]==STORY_SEAT_REMOTE) humans|=1u<<i;
        else if(kinds[i]==STORY_SEAT_AI || kinds[i]==STORY_SEAT_AWAY) others|=1u<<i;
    }
    unsigned pool=humans?humans:others,best=4;
    if(!pool) return self;
    for(unsigned i=0;i<4;++i)
        if((pool&(1u<<i)) && (best==4 || distances[i]<distances[best])) best=i;
    if(current<4 && (pool&(1u<<current)) &&
        distances[current]-distances[best]<=fmaxf(c->value[STORY_DEFEND_MARGIN_FRACTION]*distances[current],
            c->value[STORY_DEFEND_MARGIN_UNITS])) return current;
    return best;
}
int SudekiMpStoryThreatCapOnly(int mc,int rc,unsigned melee,unsigned ranged,int current,int ar) {
    int m=(int)melee-(current && !ar),r=(int)ranged-(current && ar);
    int authored=(mc==-1 || m<mc) && (rc==-1 || r<rc);
    return authored && (m>=1 || r>=1);
}

/* Pointer-free tier memory; function inputs may contain call-local views. */
static int threat_config(const SudekiMpStoryThreatConfig *c) {
    if(!c) return 0;
    for(unsigned i=0;i<STORY_THRESHOLD_COUNT;++i)
        if(!isfinite(c->value[i]) || c->value[i]<0 || c->value[i]>10000) return 0;
    return c->value[STORY_THREAT_HALF_LIFE]>0 && c->value[STORY_D0]>0;
}
static int threat_admit(const SudekiMpStoryThreatConfig *c,int coop,unsigned type,int excluded) {
    return threat_config(c) && coop && type>=1 && type<=3 && !excluded;
}
static int threat_human(unsigned kind) {return kind==STORY_SEAT_HOST || kind==STORY_SEAT_REMOTE;}
static int threat_mode(unsigned mode) {return mode==0 || mode==11;}
static uint32_t threat_ms(float seconds) {return (uint32_t)(seconds*1000);}
static uint32_t threat_elapsed(uint32_t now,uint32_t then) {
    int32_t elapsed=(int32_t)(now-then);return elapsed<0?0:(uint32_t)elapsed;
}
static int threat_due(uint32_t now,uint32_t until) {return (int32_t)(now-until)>=0;}
static int threat_level(unsigned level,unsigned minimum) {return level>=minimum && level<=STORY_TARGET_TIER2;}
int SudekiMpStoryThreatSubstitutable(const SudekiMpStoryThreatRecord *r) {
    return r && (!r->mode_seen || threat_mode(r->last_mode));
}
int SudekiMpStoryThreatShooterListed(const SudekiMpStoryThreatConfig *c,
    const SudekiMpStoryThreatRecord *r,unsigned shooter,uint32_t now) {
    if(!threat_config(c) || !r || shooter>=4 || !r->list_seen ||
        threat_elapsed(now,r->last_list_at)>threat_ms(c->value[STORY_FIRE_RECENT])) return STORY_LIST_UNKNOWN;
    return !!(r->last_list_mask&(1u<<shooter));
}
int SudekiMpStoryThreatResponderListed(const SudekiMpStoryThreatRecord *r,unsigned set) {
    if(!r || set>=STORY_SET_COUNT || !r->responding[set].active || !r->list_seen ||
        r->responding[set].shooter>=4 || (int32_t)(r->last_list_seq-r->responding[set].raise_seq)<=0)
        return STORY_LIST_UNKNOWN;
    return !!(r->last_list_mask&(1u<<r->responding[set].shooter));
}
int SudekiMpStoryThreatPushListed(const SudekiMpStoryThreatRecord *r,
    const SudekiMpStoryThreatPushState *p,unsigned seat) {
    if(!r || !p || !p->active || seat>=4 || !r->list_seen ||
        (int32_t)(r->last_list_seq-p->start_seq)<=0) return STORY_LIST_UNKNOWN;
    return !!(r->last_list_mask&(1u<<seat));
}
int SudekiMpStoryThreatDesignated(const SudekiMpStoryThreatRecord *r,unsigned set,
    const SudekiMpStoryThreatPushState p[4][STORY_SET_COUNT]) {
    if(!r || !r->serial || set>=STORY_SET_COUNT) return 0;
    if(r->responding[set].active) return 1;
    for(unsigned c=0;p && c<4;++c) if(p[c][set].active && p[c][set].serial==r->serial) return 1;
    return 0;
}
float SudekiMpStoryThreatValue(const SudekiMpStoryThreatConfig *c,
    const SudekiMpStoryThreatRecord *r,unsigned seat,int sniper,uint32_t now) {
    if(!threat_config(c) || !r || seat>=4) return 0;
    float value=sniper?r->sniper[seat]:r->threat[seat];
    if(!isfinite(value) || value<0) return 0;
    return value*exp2f(-(float)threat_elapsed(now,r->updated_at)/(c->value[STORY_THREAT_HALF_LIFE]*1000));
}
void SudekiMpStoryThreatDecay(const SudekiMpStoryThreatConfig *c,SudekiMpStoryThreatRecord *r,uint32_t now) {
    if(!threat_config(c) || !r) return;
    for(unsigned i=0;i<4;++i) {
        r->threat[i]=SudekiMpStoryThreatValue(c,r,i,0,now);
        r->sniper[i]=SudekiMpStoryThreatValue(c,r,i,1,now);
    }
    r->updated_at=now;
}
SudekiMpStoryThreatHitAdded SudekiMpStoryThreatAddHit(const SudekiMpStoryThreatConfig *c,
    int coop,unsigned type,int excluded,SudekiMpStoryThreatRecord *r,unsigned seat,unsigned kind,
    float amount,float max_hp,int ranged,int known,int hit_all,int firing,int unengaged,int far_hit,uint32_t now) {
    SudekiMpStoryThreatHitAdded result={0};
    if(!threat_admit(c,coop,type,excluded) || !r || seat>=4 || !isfinite(amount) || amount<0 ||
        !isfinite(max_hp) || max_hp<=0 || !isfinite(amount/max_hp)) return result;
    SudekiMpStoryThreatDecay(c,r,now);float value=amount/max_hp;
    r->threat[seat]=fminf(FLT_MAX,r->threat[seat]+value);result.added_threat=value>0;
    if(threat_human(kind) && ranged && known && !hit_all && firing && unengaged && far_hit) {
        r->sniper[seat]=fminf(FLT_MAX,r->sniper[seat]+value);result.added_sniper=value>0;
    }
    return result;
}
void SudekiMpStoryThreatTouch(SudekiMpStoryThreatRecord *r,unsigned set,unsigned shooter,uint32_t now) {
    if(!r || set>=STORY_SET_COUNT || shooter>=4) return;
    if(r->alerting[set].active && r->alerting[set].shooter==shooter) r->alerting[set].last_fire=now;
    if(r->responding[set].active && r->responding[set].shooter==shooter) r->responding[set].last_fire=now;
}
unsigned SudekiMpStoryThreatPick(const SudekiMpStoryThreatConfig *c,unsigned level,int coop,unsigned type,
    int excluded,unsigned mode,const SudekiMpStoryThreatRecord *r,uint32_t now,unsigned current,
    unsigned eligible,const unsigned kinds[4],const float distances[4]) {
    if(!threat_admit(c,coop,type,excluded) || !threat_level(level,STORY_TARGET_TIER1) || !threat_mode(mode) ||
        !r || !kinds || !distances) return 4;
    float scores[4]={0};unsigned best=4;
    for(unsigned i=0;i<4;++i) if((eligible&(1u<<i)) && threat_human(kinds[i]) &&
        isfinite(distances[i]) && distances[i]>=0) {
        scores[i]=SudekiMpStoryThreatValue(c,r,i,0,now)/(1+distances[i]/c->value[STORY_D0]);
        if(scores[i]>c->value[STORY_THREAT_FLOOR] && (best==4 || scores[i]>scores[best])) best=i;
    }
    if(best<4 && current<4 && scores[current]>c->value[STORY_THREAT_FLOOR] &&
        scores[best]-scores[current]<=scores[current]*c->value[STORY_THREAT_MARGIN]) return current;
    return best;
}
int SudekiMpStoryThreatAlertRaise(const SudekiMpStoryThreatConfig *c,unsigned level,int coop,unsigned type,
    int excluded,unsigned set,SudekiMpStoryThreatRecord *r,unsigned seat,unsigned kind,int added,
    int shooter_alert_active,uint32_t now) {
    if(!threat_admit(c,coop,type,excluded) || !threat_level(level,STORY_TARGET_TIER15) || set>=STORY_SET_COUNT ||
        !r || !r->serial || seat>=4 || !threat_human(kind) || !added || shooter_alert_active) return 0;
    SudekiMpStoryThreatAlerting *a=&r->alerting[set];
    if(a->cooldown_active && threat_due(now,a->cooldown_until)) a->cooldown_active=0;
    return !a->active && !a->cooldown_active &&
        SudekiMpStoryThreatValue(c,r,seat,1,now)>c->value[STORY_ALERT_THRESHOLD];
}
unsigned SudekiMpStoryThreatAlertResponders(const SudekiMpStoryThreatConfig *c,float reference,
    const SudekiMpStoryThreatCandidate *candidates,unsigned n,uint32_t *out,unsigned capacity) {
    if(!threat_config(c) || !candidates || !out || !isfinite(reference) || reference<0) return 0;
    if(capacity>STORY_ALERT_RESPONDERS_MAX) capacity=STORY_ALERT_RESPONDERS_MAX;
    unsigned count=0;
    for(unsigned partition=0;partition<2;++partition) {
        unsigned limit=(unsigned)c->value[partition?STORY_ALERT_MELEE:STORY_ALERT_RANGED];
        for(unsigned k=0;k<limit && count<capacity;++k) {
            unsigned best=n;
            for(unsigned i=0;i<n;++i) {
                const SudekiMpStoryThreatCandidate *v=&candidates[i];
                if(!v->serial || v->type<1 || v->type>3 || v->excluded || v->designated || !v->substitutable ||
                    !!v->ranged_capable==!!partition || v->shooter_listed==STORY_LIST_FALSE ||
                    !isfinite(v->distance) || v->distance<0 || v->distance>c->value[STORY_ALERT_RADIUS] ||
                    !isfinite(v->current_target_threat) || v->current_target_threat<0 || v->current_target_threat>reference) continue;
                unsigned j=0;for(;j<count && out[j]!=v->serial;++j) {}if(j<count) continue;
                if(best==n || !!v->same_group>!!candidates[best].same_group ||
                    (!!v->same_group==!!candidates[best].same_group && v->distance<candidates[best].distance)) best=i;
            }
            if(best==n) break;
            out[count++]=candidates[best].serial;
        }
    }
    return count;
}
int SudekiMpStoryThreatAlertCommit(const SudekiMpStoryThreatConfig *c,unsigned set,
    SudekiMpStoryThreatRecord *r,unsigned shooter,const uint32_t *serials,unsigned count,
    SudekiMpStoryThreatRecord *const *records,uint32_t now,uint32_t seq) {
    if(!threat_config(c) || set>=STORY_SET_COUNT || !r || !r->serial || shooter>=4 || !count ||
        count>STORY_ALERT_RESPONDERS_MAX || !serials || !records || r->alerting[set].active) return 0;
    for(unsigned i=0;i<count;++i) {
        if(!serials[i] || !records[i] || records[i]->serial!=serials[i] || records[i]->responding[set].active) return 0;
        for(unsigned j=0;j<i;++j) if(serials[j]==serials[i]) return 0;
    }
    SudekiMpStoryThreatAlerting *a=&r->alerting[set];
    a->active=1;a->shooter=shooter;a->raised_at=a->last_fire=now;a->raise_seq=seq;
    a->until=now+threat_ms(c->value[STORY_ALERT_HOLD]);a->count=count;
    memcpy(a->responders,serials,count*sizeof(*serials));
    for(unsigned i=0;i<count;++i) records[i]->responding[set]=(SudekiMpStoryThreatResponding){
        .active=1,.shooter=shooter,.alerting_serial=r->serial,.raised_at=now,.raise_seq=seq,.until=a->until,.last_fire=now};
    return 1;
}
void SudekiMpStoryThreatAlertClear(unsigned set,SudekiMpStoryThreatRecord *r,uint32_t serial,uint32_t seq) {
    if(r && set<STORY_SET_COUNT && r->responding[set].alerting_serial==serial && r->responding[set].raise_seq==seq)
        memset(&r->responding[set],0,sizeof(r->responding[set]));
}
static unsigned alert_reason(const SudekiMpStoryThreatConfig *c,unsigned level,int coop,unsigned kind,
    int firing,uint32_t now,uint32_t last,uint32_t until,int close,int left) {
    if(!threat_level(level,STORY_TARGET_TIER15) || !coop) return STORY_END_LEVEL;
    if(kind==STORY_SEAT_DOWN || kind==STORY_SEAT_ABSENT) return STORY_END_DOWN;
    if(!firing && threat_elapsed(now,last)>threat_ms(c->value[STORY_FADE_SECONDS])) return STORY_END_STOP;
    if(close) return STORY_END_CLOSE;
    if(left) return STORY_END_LEFT;
    if(threat_due(now,until)) return STORY_END_EXPIRED;
    return STORY_END_NONE;
}
unsigned SudekiMpStoryThreatAlertFade(const SudekiMpStoryThreatConfig *c,unsigned level,int coop,unsigned set,
    SudekiMpStoryThreatRecord *r,const SudekiMpStoryThreatResponderView *views,unsigned count,
    unsigned kind,int firing,uint32_t now,uint32_t released[STORY_ALERT_RESPONDERS_MAX],unsigned *released_count) {
    if(released_count) *released_count=0;
    if(!threat_config(c) || !r || set>=STORY_SET_COUNT || !r->alerting[set].active ||
        count>STORY_ALERT_RESPONDERS_MAX || (count && !views) || !released || !released_count) return STORY_END_NONE;
    SudekiMpStoryThreatAlerting *a=&r->alerting[set];unsigned remaining=0;int close=0;
    for(unsigned i=0;i<count;++i) {
        SudekiMpStoryThreatRecord *v=views[i].record;if(!v) continue;
        SudekiMpStoryThreatResponding *b=&v->responding[set];
        if(!b->active || b->alerting_serial!=r->serial || b->raise_seq!=a->raise_seq) continue;
        if(views[i].listed==STORY_LIST_FALSE || b->recheck_failed) {
            released[(*released_count)++]=v->serial;SudekiMpStoryThreatAlertClear(set,v,r->serial,a->raise_seq);
        } else {++remaining;close|=views[i].close;}
    }
    unsigned reason=alert_reason(c,level,coop,kind,firing,now,a->last_fire,a->until,close,!remaining);
    if(reason) {a->active=0;a->cooldown_active=1;a->cooldown_until=now+threat_ms(c->value[STORY_ALERT_COOLDOWN]);}
    return reason;
}
unsigned SudekiMpStoryThreatResponderFade(const SudekiMpStoryThreatConfig *c,unsigned level,int coop,unsigned set,
    const SudekiMpStoryThreatRecord *r,int close,int listed,unsigned kind,int firing,uint32_t now) {
    if(!threat_config(c) || !r || set>=STORY_SET_COUNT || !r->responding[set].active) return STORY_END_NONE;
    const SudekiMpStoryThreatResponding *a=&r->responding[set];
    return alert_reason(c,level,coop,kind,firing,now,a->last_fire,a->until,close,listed==STORY_LIST_FALSE || a->recheck_failed);
}
void SudekiMpStoryThreatPushTrack(unsigned set,SudekiMpStoryThreatPushState *p,int known,unsigned engaged,uint32_t now) {
    if(!p || set>=STORY_SET_COUNT) return;
    if(!known || engaged) p->idle_active=0;
    else if(!p->idle_active) {p->idle_active=1;p->idle_since=now;}
}
int SudekiMpStoryThreatPushEvaluate(const SudekiMpStoryThreatConfig *c,unsigned level,int coop,unsigned set,
    SudekiMpStoryThreatPushState *p,unsigned kind,int firing,uint32_t now) {
    if(!threat_config(c) || !p || set>=STORY_SET_COUNT || !threat_level(level,STORY_TARGET_TIER2) || !coop) return 0;
    if(p->cooldown_active && threat_due(now,p->cooldown_until)) p->cooldown_active=0;
    return threat_human(kind) && firing && p->idle_active && !p->active && !p->cooldown_active &&
        threat_elapsed(now,p->idle_since)>=threat_ms(c->value[STORY_IDLE_SECONDS]);
}
void SudekiMpStoryThreatPushStart(const SudekiMpStoryThreatConfig *c,unsigned set,
    SudekiMpStoryThreatPushState *p,uint32_t serial,uint32_t now,uint32_t seq) {
    if(!threat_config(c) || !p || set>=STORY_SET_COUNT || !serial) return;
    p->active=1;p->serial=serial;p->started_at=now;p->start_seq=seq;p->until=now+threat_ms(c->value[STORY_PUSH_HOLD]);
    p->cooldown_active=1;p->cooldown_until=now+threat_ms(c->value[STORY_PUSH_COOLDOWN]);p->recheck_failed=0;
}
unsigned SudekiMpStoryThreatPushEnd(const SudekiMpStoryThreatConfig *c,unsigned level,int coop,unsigned set,
    SudekiMpStoryThreatPushState *p,unsigned kind,int known,unsigned others,int exists,int listed,uint32_t now) {
    if(!threat_config(c) || !p || set>=STORY_SET_COUNT || !p->active) return STORY_END_NONE;
    unsigned reason=STORY_END_NONE;
    if(!threat_level(level,STORY_TARGET_TIER2) || !coop) reason=STORY_END_LEVEL;
    else if(kind==STORY_SEAT_DOWN || kind==STORY_SEAT_ABSENT) reason=STORY_END_DOWN;
    else if(!exists || p->recheck_failed || listed==STORY_LIST_FALSE) reason=STORY_END_INVALID;
    else if(known && others) reason=STORY_END_ENGAGED;
    else if(threat_due(now,p->until)) reason=STORY_END_EXPIRED;
    if(reason) p->active=0;
    return reason;
}
SudekiMpStoryThreatDecision SudekiMpStoryThreatDecide(const SudekiMpStoryThreatConfig *c,unsigned level,
    int coop,unsigned type,int excluded,unsigned mode,unsigned set,const SudekiMpStoryThreatRecord *r,
    const SudekiMpStoryThreatPushState pushes[4][STORY_SET_COUNT],int complete,uint32_t now,unsigned current,
    unsigned eligible,const unsigned kinds[4],const float distances[4],unsigned lead) {
    SudekiMpStoryThreatDecision none={4,STORY_RULE_NONE};
    if(!threat_admit(c,coop,type,excluded) || !threat_mode(mode) || set>=STORY_SET_COUNT || !kinds || !distances) return none;
    if(r) {
        const SudekiMpStoryThreatResponding *a=&r->responding[set];
        if(threat_level(level,STORY_TARGET_TIER15) && complete && a->active && a->shooter<4 &&
            (eligible&(1u<<a->shooter)) && threat_human(kinds[a->shooter]))
            return (SudekiMpStoryThreatDecision){a->shooter,STORY_RULE_ALERT};
        for(unsigned i=0;pushes && i<4;++i) if(threat_level(level,STORY_TARGET_TIER2) && complete &&
            pushes[i][set].active && r->serial && pushes[i][set].serial==r->serial &&
            (eligible&(1u<<i)) && threat_human(kinds[i])) return (SudekiMpStoryThreatDecision){i,STORY_RULE_PUSH};
        unsigned pick=SudekiMpStoryThreatPick(c,level,coop,type,excluded,mode,r,now,current,eligible,kinds,distances);
        if(pick<4) return (SudekiMpStoryThreatDecision){pick,STORY_RULE_THREAT};
    }
    if(threat_level(level,STORY_TARGET_TIER0) && mode==11 && lead<4 && (eligible&(1u<<lead)) &&
        (threat_human(kinds[lead]) || kinds[lead]==STORY_SEAT_AI || kinds[lead]==STORY_SEAT_AWAY))
        return (SudekiMpStoryThreatDecision){lead,STORY_RULE_LEAD};
    return none;
}
