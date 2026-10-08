#include "hooks/lan_story_ally_seat.h"
#include "hooks/lan_party_control.h"
#include "hooks/lan_story_avatar_spawn.h"
#include "cleanroom/engine.h"
#include "engine/log.h"
#include <math.h>
#include <string.h>

enum { RVA_ALLY_VTABLE=0x2d55d4u, SPAWN_RETRY_MS=6000u, SPAWN_ATTEMPTS=3u, LOG_LIMIT=200u };
static uint8_t *base; static unsigned player; static char resource[48]="ALLY_TALOS";
static BOOL client_mode; static DWORD client_wait_started; static BOOL client_timed_out;
static void *entity; static uint32_t generation,spawn_epoch; static unsigned attempts,logs;
static DWORD spawn_at; static BOOL spawn_pending;
static float offset[3]={2.0f,0.0f,2.0f};

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && VirtualQuery(p,&m,sizeof(m))==sizeof(m) && m.State==MEM_COMMIT &&
        !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) && a+n>=a && a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL component(const uint8_t *e,unsigned off,unsigned vt,size_t size) {
    const uint8_t *p=*(const uint8_t *const *)(e+off);
    return readable(p,size) && *(const void *const *)p==base+vt && *(const void *const *)(p+0x10u)==e;
}
/* The hero component classes the ally shares; absence of CSkill (+0xd8) and
 * the hero weapon component (+0xec) is expected and not checked here. */
/* Settled: the SPAWN state (ANIMID 1) keeps arbiter flag 0x400 raised; movement
 * and combat input are refused until the entity has entered idle. */
static BOOL settled(const uint8_t *e) {
    const uint8_t *arbiter=*(const uint8_t *const *)(e+0x90u);
    return readable(arbiter,0x64u) && !(*(const uint32_t *)(arbiter+0x50u)&0x400u);
}
static BOOL layout_exact(const uint8_t *e) {
    return readable(e,0x138u) && *(const void *const *)e==base+RVA_ALLY_VTABLE &&
        component(e,0x44u,0x2cdefcu,0x104u) && component(e,0x80u,0x2c8644u,0xc0u) &&
        component(e,0x8cu,0x2d48d4u,0x14u) && component(e,0x90u,0x2cc9acu,0x64u) &&
        component(e,0x94u,0x2d4924u,0x16cu) && component(e,0xa8u,0x2d4abcu,0x64u) &&
        component(e,0xacu,0x2d4b24u,0x54u) && component(e,0xb8u,0x2d4bd4u,0xb4u);
}
static void log_event(const char *event,const char *detail) {
    if(logs>=LOG_LIMIT) return; ++logs;
    SudekiMpLogFormat("ally_seat event=%s player=%u resource=%s entity=%p generation=%lu epoch=%lu attempts=%u %s\r\n",
        event,player,resource,entity,(unsigned long)generation,(unsigned long)spawn_epoch,attempts,detail?detail:"");
}
void SudekiMpLanStoryAllySeatConfigure(HMODULE game_module,const wchar_t *config_path) {
    base=(uint8_t *)game_module; player=0;
    if(!config_path) return;
    unsigned p=(unsigned)GetPrivateProfileIntW(L"DevPlay",L"AllySeatPlayer",0,config_path);
    if(p>=1u && p<=3u) player=p;
    wchar_t w[48]={0}; GetPrivateProfileStringW(L"DevPlay",L"AllyResource",L"",w,47,config_path);
    if(w[0]) {
        unsigned n=0; char out[48];
        for(;n<47u && w[n];++n) { wchar_t c=w[n];
            if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_')) { n=0; break; }
            out[n]=(char)c; }
        if(n) { out[n]=0; strcpy(resource,out); }
    }
    { wchar_t v[16]={0}; GetPrivateProfileStringW(L"DevPlay",L"ClientAlly",L"",v,15,config_path);
      client_mode=v[0] && (!_wcsicmp(v,L"true") || !_wcsicmp(v,L"1") || !_wcsicmp(v,L"yes")); }
    if(player) SudekiMpLogFormat("ally_seat event=configured player=%u resource=%s policy=host_spawn_outside_group_ai_override_story_seat_4\r\n",player,resource);
    if(client_mode) SudekiMpLogFormat("ally_seat event=configured_client resource=%s policy=spawn_before_pause_no_ai_override_catalogue_mirror\r\n",resource);
}
unsigned SudekiMpLanStoryAllySeatPlayer(void) { return player; }
const char *SudekiMpLanStoryAllySeatResource(void) { return resource; }
static void withdraw(const char *why) {
    if(entity) { entity=NULL; if(!client_mode) SudekiMpLanPartyControlStoryAllyEntity(NULL,NULL); log_event("lost",why); }
}
static void service(const SudekiMpLanStoryNativeRoster *roster,BOOL world_ready,BOOL may_spawn);
void SudekiMpLanStoryAllySeatService(const SudekiMpLanStoryNativeRoster *roster,BOOL world_ready) {
    if(!player || client_mode) return;
    service(roster,world_ready,TRUE);
}
BOOL SudekiMpLanStoryAllySeatClientEnabled(void) { return client_mode; }
void SudekiMpLanStoryAllySeatClientService(const SudekiMpLanStoryNativeRoster *roster,BOOL may_spawn) {
    if(!client_mode) return;
    if(may_spawn && !client_wait_started) client_wait_started=GetTickCount();
    service(roster,TRUE,may_spawn);
}
BOOL SudekiMpLanStoryAllySeatClientReady(void) {
    if(!client_mode) return TRUE;
    if(entity && settled(entity)) return TRUE;
    if(client_timed_out) return TRUE;
    if(client_wait_started && GetTickCount()-client_wait_started>15000u) {
        client_timed_out=TRUE;
        SudekiMpLogFormat("ally_seat event=client_wait_timeout resource=%s entity=%p attempts=%u policy=pause_without_ally\r\n",resource,entity,attempts);
        return TRUE;
    }
    return FALSE;
}
static void service(const SudekiMpLanStoryNativeRoster *roster,BOOL world_ready,BOOL may_spawn) {
    if(!base || !roster) return;
    DWORD now=GetTickCount();
    if(roster->epoch!=spawn_epoch) { withdraw("epoch_changed"); spawn_epoch=roster->epoch; attempts=0; spawn_pending=FALSE; }
    uint8_t *found=SudekiMpCleanroomEngineGenericEntity(resource);
    if(entity) {
        if(found!=entity || !layout_exact(entity)) { withdraw(found!=entity?"lookup_changed":"layout_changed"); return; }
        /* Research heartbeat (gated, 2 s): where he is and what his arbiter/movement say. */
        static DWORD beat; static unsigned beats;
        if(SudekiMpLogResearchEnabled() && beats<600u && now-beat>=2000u) { beat=now; ++beats;
            const uint8_t *e=entity,*pos=*(const uint8_t *const *)(e+0x44u),*arb=*(const uint8_t *const *)(e+0x90u),*mv=*(const uint8_t *const *)(e+0x80u);
            SudekiMpLogFormat("ally_seat event=heartbeat pos=%.2f,%.2f,%.2f flags50=%08lx state58=%08lx speed=%.2f,%.2f mv_be=%02x\r\n",
                (double)*(const float *)(pos+0x18u),(double)*(const float *)(pos+0x1cu),(double)*(const float *)(pos+0x20u),
                (unsigned long)*(const uint32_t *)(arb+0x50u),(unsigned long)*(const uint32_t *)(arb+0x58u),
                (double)*(const float *)(mv+0x24u),(double)*(const float *)(mv+0x28u),mv[0xbeu]); }
        return;
    }
    if(found) {
        if(!layout_exact(found)) { if(logs<LOG_LIMIT) { ++logs; SudekiMpLogFormat("ally_seat event=layout_refused entity=%p vtable=%p\r\n",(void *)found,readable(found,4u)?*(void **)found:NULL); } return; }
        entity=found; ++generation; spawn_pending=FALSE;
        if(!client_mode) SudekiMpLanPartyControlStoryAllyEntity(entity,resource);
        log_event("resolved",NULL); return;
    }
    if(!may_spawn || !world_ready || !SudekiMpCleanroomEngineWorldReady()) return;
    if(spawn_pending && now-spawn_at<SPAWN_RETRY_MS) return;
    if(attempts>=SPAWN_ATTEMPTS) { if(spawn_pending) { spawn_pending=FALSE; log_event("spawn_gave_up",NULL); } return; }
    /* Leader CPosition +0x18: world position. Spawn beside, never on top. */
    const uint8_t *leader=roster->actors[roster->leader_character];
    if(!readable(leader,0x48u)) return;
    const uint8_t *pos=*(const uint8_t *const *)(leader+0x44u);
    if(!readable(pos,0x24u) || *(const void *const *)pos!=base+0x2cdefcu) return;
    float where[3]; memcpy(where,pos+0x18u,sizeof(where));
    for(unsigned i=0;i<3u;++i) { where[i]+=offset[i]; if(!isfinite(where[i])) return; }
    ++attempts; spawn_at=now; spawn_pending=TRUE;
    BOOL ok=SudekiMpCleanroomEngineSpawnEntityNamed(resource,where);
    if(logs<LOG_LIMIT) { ++logs; SudekiMpLogFormat("ally_seat event=spawn_requested player=%u resource=%s at=%.2f,%.2f,%.2f accepted=%u attempt=%u epoch=%lu\r\n",
        player,resource,(double)where[0],(double)where[1],(double)where[2],ok,attempts,(unsigned long)spawn_epoch); }
}
BOOL SudekiMpLanStoryAllySeatReady(void **out,uint32_t *gen) {
    if(!entity || !settled(entity)) return FALSE;
    if(out) *out=entity;
    if(gen) *gen=generation;
    return TRUE;
}
void SudekiMpLanStoryAllySeatReset(void) {
    withdraw("runtime_reset"); spawn_epoch=0; attempts=0; spawn_pending=FALSE;
    client_wait_started=0; client_timed_out=FALSE;
    /* The native world owns the spawned entity's lifetime (no despawn export yet). */
}

/* Lobby-driven avatars never use GenericEntity(resource): several players
 * can choose the same resource and finish their native jobs out of order. */
static struct {
    HMODULE image;
    uint8_t choices[4];
    struct { void *world; uint32_t epoch,generation; } seat[4];
} avatar_seats;
BOOL SudekiMpLanStoryAvatarSeatsConfigure(HMODULE image,const uint8_t avatars[4]) {
    if(!image || !avatars || avatar_seats.image) return FALSE;
    for(unsigned p=0;p<4u;++p) if(avatars[p]>5u) return FALSE;
    avatar_seats.image=image; memcpy(avatar_seats.choices,avatars,4u);
    return TRUE;
}
BOOL SudekiMpLanStoryAvatarSeatsEnabled(void) { return avatar_seats.image!=NULL; }
BOOL SudekiMpLanStoryAvatarSeatChosen(unsigned p) {
    return avatar_seats.image && p<4u && avatar_seats.choices[p]==5u;
}
BOOL SudekiMpLanStoryAvatarSeatReady(unsigned p,const SudekiMpLanStoryNativeRoster *r,
    void **out,uint32_t *gen) {
    SudekiMpLanStoryAvatarSpawnObservation observed;
    if(!SudekiMpLanStoryAvatarSeatChosen(p) || !r || !r->world ||
        avatar_seats.seat[p].world!=r->world || avatar_seats.seat[p].epoch!=r->epoch ||
        !SudekiMpLanStoryAvatarSpawnObserve(p,r->epoch,avatar_seats.seat[p].generation,&observed) ||
        observed.unknown || !observed.ready || !observed.actor || observed.world!=r->world) return FALSE;
    if(out) *out=observed.actor;
    if(gen) *gen=observed.generation;
    return TRUE;
}
BOOL SudekiMpLanStoryAvatarSeatsReady(const SudekiMpLanStoryNativeRoster *r) {
    if(!avatar_seats.image) return TRUE;
    for(unsigned p=0;p<4u;++p)
        if(SudekiMpLanStoryAvatarSeatChosen(p) && !SudekiMpLanStoryAvatarSeatReady(p,r,NULL,NULL)) return FALSE;
    return TRUE;
}
static BOOL stats_copy(void *actor,const uint8_t **component,float copy[4]) {
    if(!avatar_seats.image || !readable(actor,0x50u)) return FALSE;
    const uint8_t *stats=*(const uint8_t *const *)((const uint8_t *)actor+0x4cu);
    if(!readable(stats,0x3cu) || *(const void *const *)stats!=(uint8_t *)avatar_seats.image+0x2cc064u ||
        *(const void *const *)(stats+0x10u)!=actor) return FALSE;
    memcpy(copy,stats+0x2cu,4u*sizeof(float));
    for(unsigned i=0;i<4u;++i) if(!isfinite(copy[i]) || copy[i]<0.0f) return FALSE;
    if(copy[1]<=0.0f || copy[0]>copy[1] || copy[2]>copy[3]) return FALSE;
    *component=stats; return TRUE;
}
static BOOL stats_unchanged(void *actor,const uint8_t *stats,const float copy[4]) {
    return readable(actor,0x50u) && readable(stats,0x3cu) &&
        *(const void *const *)((const uint8_t *)actor+0x4cu)==stats &&
        *(const void *const *)stats==(uint8_t *)avatar_seats.image+0x2cc064u &&
        *(const void *const *)(stats+0x10u)==actor && !memcmp(copy,stats+0x2cu,4u*sizeof(float));
}
BOOL SudekiMpLanStoryAvatarSeatStats(unsigned p,const SudekiMpLanStoryNativeRoster *r,
    uint32_t *generation_out,float values[4]) {
    void *actor=NULL,*again=NULL; uint32_t gen=0,again_gen=0; float copy[4];
    const uint8_t *stats=NULL;
    if(!values || !generation_out || !SudekiMpLanStoryAvatarSeatReady(p,r,&actor,&gen) ||
        !stats_copy(actor,&stats,copy) ||
        !SudekiMpLanStoryAvatarSeatReady(p,r,&again,&again_gen) || again!=actor || again_gen!=gen ||
        !stats_unchanged(actor,stats,copy)) return FALSE;
    memcpy(values,copy,sizeof(copy)); *generation_out=gen; return TRUE;
}
BOOL SudekiMpLanStoryPartySeatStats(unsigned character,const SudekiMpLanStoryNativeRoster *r,
    float values[4]) {
    float copy[4]; const uint8_t *stats=NULL;
    if(!values || !r || character>=4u || !(r->available_mask&(1u<<character)) ||
        !SudekiMpLanStoryObserverNativeRosterExact(r)) return FALSE;
    void *actor=r->actors[character];
    if(!stats_copy(actor,&stats,copy) || !SudekiMpLanStoryObserverNativeRosterExact(r) ||
        r->actors[character]!=actor || !stats_unchanged(actor,stats,copy)) return FALSE;
    memcpy(values,copy,sizeof(copy)); return TRUE;
}
void SudekiMpLanStoryAvatarSeatsService(const SudekiMpLanStoryNativeRoster *r,
    BOOL world_ready,BOOL may_spawn) {
    if(!avatar_seats.image || !r || !r->epoch || !r->world || !world_ready || !may_spawn ||
        r->leader_character>=4u || !(r->available_mask&(1u<<r->leader_character)) ||
        !SudekiMpCleanroomEngineWorldReady()) return;
    const uint8_t *leader=r->actors[r->leader_character];
    if(!readable(leader,0x48u)) return;
    const uint8_t *position=*(const uint8_t *const *)(leader+0x44u);
    if(!readable(position,0x24u) || *(const void *const *)position!=(uint8_t *)avatar_seats.image+0x2cdefcu ||
        *(const void *const *)(position+0x10u)!=leader) return;
    float origin[3]; memcpy(origin,position+0x18u,sizeof(origin));
    for(unsigned i=0;i<3u;++i) if(!isfinite(origin[i])) return;
    for(unsigned p=0;p<4u;++p) {
        if(!SudekiMpLanStoryAvatarSeatChosen(p) ||
            (avatar_seats.seat[p].world==r->world && avatar_seats.seat[p].epoch==r->epoch) ||
            avatar_seats.seat[p].generation==UINT32_MAX) continue;
        uint32_t next=avatar_seats.seat[p].generation+1u;
        float where[3]={origin[0]+((p&1u)?-2.0f:2.0f),origin[1],origin[2]+((p&2u)?-2.0f:2.0f)};
        if(!SudekiMpLanStoryAvatarSpawnBegin(avatar_seats.image,p,r->epoch,next)) continue;
        avatar_seats.seat[p].world=r->world; avatar_seats.seat[p].epoch=r->epoch;
        avatar_seats.seat[p].generation=next;
        BOOL accepted=SudekiMpCleanroomEngineSpawnEntityNamed("ALLY_TALOS",where);
        BOOL scoped=SudekiMpLanStoryAvatarSpawnEnd(p,r->epoch,next);
        SudekiMpLogFormat("avatar_seat event=spawn_requested player=%u epoch=%lu generation=%lu accepted=%u scoped=%u\r\n",
            p,(unsigned long)r->epoch,(unsigned long)next,accepted,scoped);
    }
}
BOOL SudekiMpLanStoryAvatarSeatsShutdown(void) {
    if(!SudekiMpLanStoryAvatarSpawnShutdown()) return FALSE;
    /* Generation floors survive reconfiguration in this process. */
    avatar_seats.image=NULL; memset(avatar_seats.choices,4,sizeof(avatar_seats.choices));
    for(unsigned p=0;p<4u;++p) { avatar_seats.seat[p].world=NULL; avatar_seats.seat[p].epoch=0; }
    return TRUE;
}
