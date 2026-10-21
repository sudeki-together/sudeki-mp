#ifndef SUDEKIMP_STORY_THREAT_H
#define SUDEKIMP_STORY_THREAT_H
#include <stdint.h>
#include <stddef.h>

/* Plain data only. Watch observations and owner-approved targeting policies. */
enum { STORY_SEAT_DOWN, STORY_SEAT_ABSENT, STORY_SEAT_AWAY, STORY_SEAT_AI,
    STORY_SEAT_REMOTE, STORY_SEAT_HOST };
typedef struct SudekiMpStoryThreatSeat {
    unsigned down,absent,away,native_ai,owned_ai,connected,ready,draining,held,host;
    uint32_t receipt,max_age;
} SudekiMpStoryThreatSeat;
unsigned SudekiMpStoryThreatSeatKind(const SudekiMpStoryThreatSeat *seat,uint32_t now);
const char *SudekiMpStoryThreatSeatName(unsigned kind);
enum { STORY_TARGET_OFF, STORY_TARGET_WATCH, STORY_TARGET_TIER0,
    STORY_TARGET_TIER1, STORY_TARGET_TIER15, STORY_TARGET_TIER2 };
enum { STORY_LEAD_MARGIN_FRACTION, STORY_LEAD_MARGIN_UNITS, STORY_THREAT_HALF_LIFE,
    STORY_D0, STORY_THREAT_FLOOR, STORY_THREAT_MARGIN, STORY_FIRE_RECENT,
    STORY_MELEE_REACH, STORY_FAR_DISTANCE, STORY_ALERT_THRESHOLD, STORY_ALERT_HOLD,
    STORY_ALERT_RADIUS, STORY_ALERT_RANGED, STORY_ALERT_MELEE, STORY_FADE_SECONDS,
    STORY_CLOSE_DISTANCE, STORY_ALERT_COOLDOWN, STORY_IDLE_SECONDS,
    STORY_IDLE_RADIUS, STORY_PUSH_HOLD, STORY_PUSH_COOLDOWN,
    STORY_DEFEND_MARGIN_FRACTION, STORY_DEFEND_MARGIN_UNITS, STORY_THRESHOLD_COUNT };
typedef struct SudekiMpStoryThreatConfig {
    unsigned level,cap_parity,lead_always;
    float value[STORY_THRESHOLD_COUNT];
    char excluded[512];
} SudekiMpStoryThreatConfig;
extern const char *const SudekiMpStoryThreatKeys[STORY_THRESHOLD_COUNT];
void SudekiMpStoryThreatDefaults(SudekiMpStoryThreatConfig *config);
/* Invalid/unknown keys leave the configuration unchanged. */
int SudekiMpStoryThreatParse(SudekiMpStoryThreatConfig *config,const char *key,const char *value);
unsigned SudekiMpStoryThreatEffectiveLevel(const SudekiMpStoryThreatConfig *config);
/* Character 4 means no admitted anchor. Distances are from the companion. */
unsigned SudekiMpStoryThreatDefendAnchor(const SudekiMpStoryThreatConfig *config,int coop,
    unsigned self,unsigned current,unsigned eligible,const unsigned kinds[4],const float distances[4]);
int SudekiMpStoryThreatExcluded(const SudekiMpStoryThreatConfig *config,const char *name);
int SudekiMpStoryThreatResourceExcluded(const SudekiMpStoryThreatConfig *config,uint32_t resource);
/* Admission only: native cap values and arithmetic remain authoritative. */
int SudekiMpStoryThreatCapParity(const SudekiMpStoryThreatConfig *config,int cooperative,
    unsigned seat_kind,unsigned self_type,int excluded);
/* Tier 0b: native Lead-PC requests only. Distances refer to this invocation's
 * filtered candidates; 4 means preserve the native result. */
unsigned SudekiMpStoryThreatLeadPick(const SudekiMpStoryThreatConfig *config,int cooperative,
    unsigned self_type,int excluded,unsigned mode,int host_standing,unsigned current,
    unsigned eligible,const unsigned kinds[4],const float distances[4]);
/* S2 has already passed the native non-cap filter. Reproduce only cap math,
 * including discounting this enemy's existing attack. No native calls. */
int SudekiMpStoryThreatCapOnly(int melee_cap,int ranged_cap,unsigned melee,unsigned ranged,
    int current,int attacker_ranged);

/* Higher tiers use HP-fraction memory: x * 2^(-elapsed_ms/(half_life*1000)).
 * Pick alone weights by 1/(1+d/d0); its floor at d0 costs twice the HP fraction.
 * Decisions 10/11: only HOST/REMOTE may be named above tier0; one active alert
 * per shooter per set, including orphan copies. AI/AWAY bodies remain reachable
 * only through the tier 0b lead branch (Decide (d)). Responders compare raw threat
 * (ties qualify), ranged first; the combined count is clamped to 3 even though
 * each ini count accepts 32. Explicit flags make tick 0 valid; deadlines and
 * sequence ordering are wrap-safe. No function derives level from config.
 * AlertRaise/PushEvaluate only mutate an inspected expired cooldown flag. */
enum { STORY_SET_LIVE, STORY_SET_SHADOW, STORY_SET_COUNT, STORY_ALERT_RESPONDERS_MAX=3 };
enum { STORY_LIST_UNKNOWN=-1, STORY_LIST_FALSE=0, STORY_LIST_TRUE=1 };
enum { STORY_RULE_NONE, STORY_RULE_LEAD, STORY_RULE_THREAT, STORY_RULE_ALERT, STORY_RULE_PUSH, STORY_RULE_COUNT };
enum { STORY_END_NONE, STORY_END_LEVEL, STORY_END_DOWN, STORY_END_STOP, STORY_END_CLOSE,
    STORY_END_LEFT, STORY_END_EXPIRED, STORY_END_INVALID, STORY_END_ENGAGED };
typedef struct SudekiMpStoryThreatAlerting {
    unsigned active,shooter;
    uint32_t raised_at,raise_seq,until,last_fire,responders[STORY_ALERT_RESPONDERS_MAX];
    unsigned count,cooldown_active;
    uint32_t cooldown_until;
} SudekiMpStoryThreatAlerting;
typedef struct SudekiMpStoryThreatResponding {
    unsigned active,shooter;
    uint32_t alerting_serial,raised_at,raise_seq,until,last_fire;
    unsigned recheck_failed;
} SudekiMpStoryThreatResponding;
typedef struct SudekiMpStoryThreatRecord {
    uint32_t serial;
    unsigned mode_seen,last_mode,list_seen,last_list_mask;
    uint32_t last_list_at,last_list_seq;
    float threat[4],sniper[4];
    uint32_t updated_at;
    SudekiMpStoryThreatAlerting alerting[STORY_SET_COUNT];
    SudekiMpStoryThreatResponding responding[STORY_SET_COUNT];
    uint32_t lock_blocked;
} SudekiMpStoryThreatRecord;
typedef struct SudekiMpStoryThreatPushState {
    unsigned active;
    uint32_t serial,started_at,start_seq,until;
    unsigned cooldown_active;
    uint32_t cooldown_until;
    unsigned idle_active;
    uint32_t idle_since;
    unsigned recheck_failed;
} SudekiMpStoryThreatPushState;
typedef struct SudekiMpStoryThreatCandidate {
    uint32_t serial;
    float distance;
    unsigned ranged_capable,same_group,type,excluded,substitutable;
    int shooter_listed;
    unsigned designated,current_target_seat;
    float current_target_threat;
} SudekiMpStoryThreatCandidate;
typedef struct SudekiMpStoryThreatSeatFrame {
    unsigned kind,firing,engaged_known,engaged_by_any,engaged_by_other;
} SudekiMpStoryThreatSeatFrame;
typedef struct SudekiMpStoryThreatHitAdded {unsigned added_threat,added_sniper;} SudekiMpStoryThreatHitAdded;
typedef struct SudekiMpStoryThreatDecision {unsigned seat,rule;} SudekiMpStoryThreatDecision;
typedef struct SudekiMpStoryThreatResponderView {
    SudekiMpStoryThreatRecord *record; /* call-local view only; never retained */
    int close,listed;
} SudekiMpStoryThreatResponderView;
/* A zero group id is unknown, including when both ids are zero. */
int SudekiMpStoryThreatSameGroup(uint32_t a,uint32_t b);
int SudekiMpStoryThreatSubstitutable(const SudekiMpStoryThreatRecord *record);
int SudekiMpStoryThreatShooterListed(const SudekiMpStoryThreatConfig *config,
    const SudekiMpStoryThreatRecord *record,unsigned shooter,uint32_t now);
int SudekiMpStoryThreatResponderListed(const SudekiMpStoryThreatRecord *record,unsigned set);
int SudekiMpStoryThreatPushListed(const SudekiMpStoryThreatRecord *record,
    const SudekiMpStoryThreatPushState *push,unsigned seat);
int SudekiMpStoryThreatDesignated(const SudekiMpStoryThreatRecord *record,unsigned set,
    const SudekiMpStoryThreatPushState pushes[4][STORY_SET_COUNT]);
float SudekiMpStoryThreatValue(const SudekiMpStoryThreatConfig *config,
    const SudekiMpStoryThreatRecord *record,unsigned seat,int sniper,uint32_t now);
void SudekiMpStoryThreatDecay(const SudekiMpStoryThreatConfig *config,SudekiMpStoryThreatRecord *record,uint32_t now);
SudekiMpStoryThreatHitAdded SudekiMpStoryThreatAddHit(const SudekiMpStoryThreatConfig *config,
    int cooperative,unsigned self_type,int excluded,SudekiMpStoryThreatRecord *record,
    unsigned seat,unsigned kind,float amount,float max_hp,int ranged,int hit_all_known,
    int hit_all,int firing,int unengaged,int far_hit,uint32_t now);
void SudekiMpStoryThreatTouch(SudekiMpStoryThreatRecord *record,unsigned set,unsigned shooter,uint32_t now);
unsigned SudekiMpStoryThreatPick(const SudekiMpStoryThreatConfig *config,unsigned level,
    int cooperative,unsigned self_type,int excluded,unsigned mode,const SudekiMpStoryThreatRecord *record,
    uint32_t now,unsigned current,unsigned eligible,const unsigned kinds[4],const float distances[4]);
int SudekiMpStoryThreatAlertRaise(const SudekiMpStoryThreatConfig *config,unsigned level,
    int cooperative,unsigned self_type,int excluded,unsigned set,SudekiMpStoryThreatRecord *record,
    unsigned seat,unsigned shooter_kind,int sniper_added,int shooter_alert_active,uint32_t now);
unsigned SudekiMpStoryThreatAlertResponders(const SudekiMpStoryThreatConfig *config,float reference,
    const SudekiMpStoryThreatCandidate *candidates,unsigned n,uint32_t *out,unsigned capacity);
int SudekiMpStoryThreatAlertCommit(const SudekiMpStoryThreatConfig *config,unsigned set,
    SudekiMpStoryThreatRecord *alerting,unsigned shooter,const uint32_t *serials,unsigned count,
    SudekiMpStoryThreatRecord *const *records,uint32_t now,uint32_t seq);
void SudekiMpStoryThreatAlertClear(unsigned set,SudekiMpStoryThreatRecord *record,uint32_t serial,uint32_t seq);
unsigned SudekiMpStoryThreatAlertFade(const SudekiMpStoryThreatConfig *config,unsigned level,
    int cooperative,unsigned set,SudekiMpStoryThreatRecord *alerting,
    const SudekiMpStoryThreatResponderView *responders,unsigned count,unsigned shooter_kind,
    int shooter_firing,uint32_t now,uint32_t released[STORY_ALERT_RESPONDERS_MAX],unsigned *released_count);
unsigned SudekiMpStoryThreatResponderFade(const SudekiMpStoryThreatConfig *config,unsigned level,
    int cooperative,unsigned set,const SudekiMpStoryThreatRecord *record,int close,int listed,
    unsigned shooter_kind,int shooter_firing,uint32_t now);
void SudekiMpStoryThreatPushTrack(unsigned set,SudekiMpStoryThreatPushState *push,
    int engaged_known,unsigned engaged_by_any,uint32_t now);
int SudekiMpStoryThreatPushEvaluate(const SudekiMpStoryThreatConfig *config,unsigned level,
    int cooperative,unsigned set,SudekiMpStoryThreatPushState *push,unsigned seat_kind,int firing,uint32_t now);
void SudekiMpStoryThreatPushStart(const SudekiMpStoryThreatConfig *config,unsigned set,
    SudekiMpStoryThreatPushState *push,uint32_t serial,uint32_t now,uint32_t seq);
unsigned SudekiMpStoryThreatPushEnd(const SudekiMpStoryThreatConfig *config,unsigned level,
    int cooperative,unsigned set,SudekiMpStoryThreatPushState *push,unsigned seat_kind,
    int engaged_known,unsigned engaged_by_other,int pushed_exists,int pushed_listed,uint32_t now);
SudekiMpStoryThreatDecision SudekiMpStoryThreatDecide(const SudekiMpStoryThreatConfig *config,unsigned level,
    int cooperative,unsigned self_type,int excluded,unsigned mode,unsigned set,
    const SudekiMpStoryThreatRecord *record,const SudekiMpStoryThreatPushState pushes[4][STORY_SET_COUNT],
    int table_complete,uint32_t now,unsigned current,unsigned eligible,const unsigned kinds[4],
    const float distances[4],unsigned lead_result);

#endif
