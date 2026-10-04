#include "network/lan_story_catchup.h"
#include <string.h>

_Static_assert(8u+2u*SUDEKIMP_LAN_STORY_WIRE_SIZE+SUDEKIMP_STORY_RECRUITMENT_WIRE_SIZE+
    4u+SUDEKIMP_LAN_STORY_FRAME_MAX_SIZE+SUDEKIMP_LAN_STORY_WORLD_MAX_CHUNKS*
    (4u+SUDEKIMP_LAN_STORY_WORLD_CHUNK_MAX_SIZE)<=SUDEKIMP_STORY_CATCHUP_MAX_SIZE,
    "Catch-up buffer must hold the complete bounded snapshot");
_Static_assert(SUDEKIMP_STORY_CATCHUP_MAX_FRAGMENTS*SUDEKIMP_STORY_CATCHUP_FRAGMENT_SIZE>=
    SUDEKIMP_STORY_CATCHUP_MAX_SIZE,"Catch-up fragment mask capacity");

static void put32(uint8_t *p,uint32_t v) {
    for(unsigned i=0;i<4u;++i) p[i]=(uint8_t)(v>>(8u*i));
}
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
BOOL SudekiMpLanStoryCatchupValid(const SudekiMpLanStoryCatchup *s) {
    if(!s || !s->transaction || !SudekiMpLanStorySceneValid(&s->target) ||
        !SudekiMpLanStorySceneValid(&s->seed_scene) ||
        s->target.phase!=SUDEKIMP_LAN_STORY_READY ||
        s->seed_scene.phase!=SUDEKIMP_LAN_STORY_READY ||
        strcmp(s->target.world,s->seed_scene.world) ||
        strcmp(s->target.temporary,s->seed_scene.temporary) ||
        !SudekiMpLanStoryFrameMatchesScene(&s->party,&s->seed_scene) ||
        !SudekiMpLanStoryWorldFrameMatches(&s->world,&s->party) || !s->party.view.valid) return FALSE;
    if(s->recruitment.transaction)
        return s->seed_scene.available_mask==4u && s->seed_scene.leader_seat==2u &&
            s->target.available_mask==12u && SudekiMpLanStoryRecruitmentMatchesScene(&s->recruitment,&s->target) &&
            s->recruitment.before_epoch==s->seed_scene.epoch &&
            s->recruitment.before_revision==s->seed_scene.revision;
    SudekiMpLanStoryRecruitment empty={0};
    return !memcmp(&empty,&s->recruitment,sizeof(empty)) &&
        s->target.revision==s->seed_scene.revision &&
        SudekiMpLanStorySceneSame(&s->target,&s->seed_scene);
}
BOOL SudekiMpLanStoryCatchupMatches(const SudekiMpLanStoryCatchup *s,
    const SudekiMpLanStoryScene *current) {
    return SudekiMpLanStoryCatchupValid(s) && SudekiMpLanStorySceneValid(current) &&
        s->target.revision==current->revision && SudekiMpLanStorySceneSame(&s->target,current);
}
BOOL SudekiMpLanStoryCatchupEncode(const SudekiMpLanStoryCatchup *s,
    uint8_t *p,size_t capacity,size_t *written) {
    if(!p || !written || capacity<SUDEKIMP_STORY_CATCHUP_MAX_SIZE ||
        !SudekiMpLanStoryCatchupValid(s)) return FALSE;
    memcpy(p,"SCS2",4); put32(p+4,s->transaction);
    size_t at=8u;
    if(!SudekiMpLanStorySceneEncode(&s->target,p+at,SUDEKIMP_LAN_STORY_WIRE_SIZE)) return FALSE;
    at+=SUDEKIMP_LAN_STORY_WIRE_SIZE;
    if(!SudekiMpLanStorySceneEncode(&s->seed_scene,p+at,SUDEKIMP_LAN_STORY_WIRE_SIZE)) return FALSE;
    at+=SUDEKIMP_LAN_STORY_WIRE_SIZE;
    memset(p+at,0,SUDEKIMP_STORY_RECRUITMENT_WIRE_SIZE);
    if(s->recruitment.transaction && !SudekiMpLanStoryRecruitmentEncode(&s->recruitment,
        p+at,SUDEKIMP_STORY_RECRUITMENT_WIRE_SIZE)) return FALSE;
    at+=SUDEKIMP_STORY_RECRUITMENT_WIRE_SIZE;
    size_t size=0;
    if(!SudekiMpLanStoryFrameEncode(&s->party,p+at+4u,capacity-at-4u,&size)) return FALSE;
    put32(p+at,(uint32_t)size); at+=4u+size;
    unsigned chunks=SudekiMpLanStoryWorldChunkCount(s->world.count);
    for(unsigned i=0;i<chunks;++i) {
        if(!SudekiMpLanStoryWorldChunkEncode(&s->world,i,p+at+4u,capacity-at-4u,&size)) return FALSE;
        put32(p+at,(uint32_t)size); at+=4u+size;
    }
    *written=at; return TRUE;
}
BOOL SudekiMpLanStoryCatchupDecode(const uint8_t *p,size_t size,SudekiMpLanStoryCatchup *out) {
    SudekiMpLanStoryCatchup s={0};
    size_t at=8u,minimum=8u+2u*SUDEKIMP_LAN_STORY_WIRE_SIZE+SUDEKIMP_STORY_RECRUITMENT_WIRE_SIZE;
    if(!p || !out || size<minimum+4u || size>SUDEKIMP_STORY_CATCHUP_MAX_SIZE || memcmp(p,"SCS2",4)) return FALSE;
    s.transaction=get32(p+4);
    if(!SudekiMpLanStorySceneDecode(p+at,SUDEKIMP_LAN_STORY_WIRE_SIZE,&s.target)) return FALSE;
    at+=SUDEKIMP_LAN_STORY_WIRE_SIZE;
    if(!SudekiMpLanStorySceneDecode(p+at,SUDEKIMP_LAN_STORY_WIRE_SIZE,&s.seed_scene)) return FALSE;
    at+=SUDEKIMP_LAN_STORY_WIRE_SIZE;
    BOOL recruitment=FALSE;
    for(unsigned i=0;i<SUDEKIMP_STORY_RECRUITMENT_WIRE_SIZE;++i) if(p[at+i]) recruitment=TRUE;
    if(recruitment) {
        if(!SudekiMpLanStoryRecruitmentDecode(p+at,SUDEKIMP_STORY_RECRUITMENT_WIRE_SIZE,&s.recruitment)) return FALSE;
    } else for(unsigned i=0;i<SUDEKIMP_STORY_RECRUITMENT_WIRE_SIZE;++i) if(p[at+i]) return FALSE;
    at+=SUDEKIMP_STORY_RECRUITMENT_WIRE_SIZE;
    uint32_t n=get32(p+at); at+=4u;
    if(n>size-at || !SudekiMpLanStoryFrameDecode(p+at,n,&s.party)) return FALSE;
    at+=n;
    unsigned count=0,chunks=0;
    do {
        if(size-at<4u) return FALSE;
        n=get32(p+at); at+=4u;
        SudekiMpLanStoryWorldChunk c;
        if(n>size-at || !SudekiMpLanStoryWorldChunkDecode(p+at,n,&c) || c.index!=count ||
            c.epoch!=s.party.epoch || c.revision!=s.party.revision ||
            c.host_tick!=s.party.host_tick || c.sequence!=s.party.sequence) return FALSE;
        if(!count) {
            chunks=c.chunks; s.world.epoch=c.epoch; s.world.revision=c.revision;
            s.world.host_tick=c.host_tick; s.world.sequence=c.sequence; s.world.count=c.total;
        } else if(c.chunks!=chunks || c.total!=s.world.count) return FALSE;
        memcpy(s.world.actors+count*SUDEKIMP_LAN_STORY_WORLD_CHUNK_ACTORS,c.actors,c.count*sizeof(c.actors[0]));
        at+=n; ++count;
    } while(count<chunks);
    if(at!=size || !SudekiMpLanStoryCatchupValid(&s)) return FALSE;
    *out=s; return TRUE;
}
