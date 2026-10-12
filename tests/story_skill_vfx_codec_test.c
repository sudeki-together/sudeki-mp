#include "network/lan_story_vfx.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    SudekiMpStoryVfx s={.epoch=1,.revision=1,.sequence=1,.tick=100,.stream=5,
        .cast_sequence=2,.observed=1,.count=1,.current="road"},out;
    uint8_t bytes[SUDEKIMP_STORY_VFX_SIZE];
    s.entries[0]=(SudekiMpLanArenaSpiritVfxSnapshot){.instance_sequence=1,.skill_sequence=2,
        .emitted_host_tick=99,.rotation_xyzw={0,0,0,1},.scale={1,1,1}};
    for(unsigned kind=33;kind<=34;++kind) {
        s.entries[0].kind=kind;
        assert(SudekiMpStoryVfxEncode(&s,bytes,sizeof(bytes)));
        assert(SudekiMpStoryVfxDecode(bytes,sizeof(bytes),&out) && !memcmp(&s,&out,sizeof(s)));
        SudekiMpLanArenaSnapshot old={0}; old.host_tick=s.tick;old.seat[0].skill_sequence=2;
        old.seat[0].skill_kind=SUDEKIMP_LAN_ARENA_SKILL_PRESENTATION_SPIRIT;
        old.spirit_vfx_observed=old.spirit_vfx_count=1;old.spirit_vfx[0]=s.entries[0];
        assert(!SudekiMpLanArenaVisualOwnerValid(&s.entries[0]));
        assert(!SudekiMpLanArenaVisualOwnerValidForParty(&s.entries[0]));
        assert(!SudekiMpLanArenaSpiritVfxRosterValid(&old));
        assert(SudekiMpLanStoryCastVfxRosterValid(&old));
        s.entries[0].owner_actor_type=SUDEKIMP_LAN_ARENA_TAL_TYPE;
        assert(!SudekiMpStoryVfxValid(&s));s.entries[0].owner_actor_type=0;
        s.entries[0].skill_sequence=0;assert(!SudekiMpStoryVfxValid(&s));s.entries[0].skill_sequence=2;
    }
    s.entries[0].kind=35;assert(!SudekiMpStoryVfxValid(&s));
    s.entries[0].kind=34;s.entries[0].skill_sequence=3;assert(!SudekiMpStoryVfxValid(&s));
    s.entries[0].skill_sequence=2;s.observed=0;assert(!SudekiMpStoryVfxValid(&s));
    s.observed=1;s.cast_sequence=0;s.entries[0].skill_sequence=0;
    for(unsigned kind=35;kind<=SUDEKIMP_LAN_STORY_VFX_ELCO_SHOT_LAST;++kind) {
        s.entries[0].kind=kind;
        s.entries[0].owner_actor_type=kind<=SUDEKIMP_LAN_STORY_VFX_AILISH_SHOT_LAST?1u:0x0eu;
        assert(SudekiMpStoryVfxEncode(&s,bytes,sizeof(bytes)));
        assert(SudekiMpStoryVfxDecode(bytes,sizeof(bytes),&out) && !memcmp(&s,&out,sizeof(s)));
        assert(!SudekiMpLanArenaVisualOwnerValidForParty(&s.entries[0]));
        s.entries[0].owner_actor_type=kind<=SUDEKIMP_LAN_STORY_VFX_AILISH_SHOT_LAST?0x0eu:1u;
        assert(!SudekiMpStoryVfxValid(&s));
    }
    s.entries[0].kind=SUDEKIMP_LAN_STORY_VFX_MAGIC_WALL_HIT;
    for(unsigned type=0;type<32;++type) {
        s.entries[0].owner_actor_type=type;
        assert(SudekiMpStoryVfxValid(&s)==(type==1u || type==0x0eu));
        if(type!=1u && type!=0x0eu) continue;
        assert(SudekiMpStoryVfxEncode(&s,bytes,sizeof(bytes)));
        assert(SudekiMpStoryVfxDecode(bytes,sizeof(bytes),&out) && !memcmp(&s,&out,sizeof(s)));
        assert(!SudekiMpLanArenaVisualOwnerValidForParty(&s.entries[0]));
        s.entries[0].skill_sequence=1;assert(!SudekiMpStoryVfxValid(&s));s.entries[0].skill_sequence=0;
    }
    const unsigned kinds[]={SUDEKIMP_LAN_ARENA_BUKI_VFX_SHIELD_APPEAR,SUDEKIMP_LAN_ARENA_BUKI_VFX_SHIELD_LOOP,
        SUDEKIMP_LAN_PARTY_TAL_VFX_SHIELD_APPEAR,SUDEKIMP_LAN_PARTY_TAL_VFX_SHIELD_LOOP};
    for(unsigned i=0;i<4;++i) {
        s.entries[0].kind=kinds[i];
        s.entries[0].owner_actor_type=i<2?SUDEKIMP_LAN_ARENA_BUKI_TYPE:SUDEKIMP_LAN_ARENA_TAL_TYPE;
        assert(SudekiMpStoryVfxEncode(&s,bytes,sizeof(bytes)));
        assert(SudekiMpStoryVfxDecode(bytes,sizeof(bytes),&out) && !memcmp(&s,&out,sizeof(s)));
        s.entries[0].skill_sequence=1;assert(!SudekiMpStoryVfxValid(&s));s.entries[0].skill_sequence=0;
        s.entries[0].owner_actor_type=i<2?SUDEKIMP_LAN_ARENA_TAL_TYPE:SUDEKIMP_LAN_ARENA_BUKI_TYPE;
        assert(!SudekiMpStoryVfxValid(&s));
    }
    /* Four independent ordinary casts share one complete host roster. Tal's
     * header frontier must not reject Ailish's higher cast counter. */
    const unsigned skill_kinds[]={SUDEKIMP_LAN_STORY_VFX_SFXBS001_SPIRIT_WIND,
        SUDEKIMP_LAN_STORY_VFX_SFXES008_NANO_REPAIRS,
        SUDEKIMP_LAN_STORY_VFX_SFXTS008_INVINCIBLE_CITADEL,
        SUDEKIMP_LAN_STORY_VFX_SFXAS011_WITCHES_KISS};
    const unsigned types[]={5,14,0,1};
    s.count=4;s.cast_sequence=2;
    for(unsigned i=0;i<4;++i) {
        s.entries[i]=(SudekiMpLanArenaSpiritVfxSnapshot){.kind=skill_kinds[i],
            .owner_actor_type=types[i],.skill_sequence=i==2?2:100+i,.instance_sequence=i+1,
            .emitted_host_tick=99,.rotation_xyzw={0,0,0,1},.scale={1,1,1}};
    }
    assert(SudekiMpStoryVfxEncode(&s,bytes,sizeof(bytes)));
    assert(SudekiMpStoryVfxDecode(bytes,sizeof(bytes),&out) && !memcmp(&s,&out,sizeof(s)));
    for(unsigned i=0;i<4;++i) {
        s.entries[i].owner_actor_type=types[(i+1)%4];assert(!SudekiMpStoryVfxValid(&s));
        s.entries[i].owner_actor_type=types[i];
        unsigned seq=s.entries[i].skill_sequence;s.entries[i].skill_sequence=0;
        assert(!SudekiMpStoryVfxValid(&s));s.entries[i].skill_sequence=seq;
        s.entries[i].emitted_host_tick=101;assert(!SudekiMpStoryVfxValid(&s));s.entries[i].emitted_host_tick=99;
        assert(!SudekiMpLanArenaVisualOwnerValid(&s.entries[i]));
        assert(!SudekiMpLanArenaVisualOwnerValidForParty(&s.entries[i]));
    }
    s.entries[3].instance_sequence=1;assert(!SudekiMpStoryVfxValid(&s));s.entries[3].instance_sequence=4;
    /* Shared startup/target marker is valid for every roster caster. */
    for(unsigned i=0;i<4;++i) s.entries[i].kind=SUDEKIMP_LAN_STORY_VFX_SFXSA005_INTO_SFX;
    assert(SudekiMpStoryVfxValid(&s));
    s.entries[0].kind=SUDEKIMP_LAN_STORY_VFX_LAST+1;assert(!SudekiMpStoryVfxValid(&s));
    s=(SudekiMpStoryVfx){.epoch=1,.revision=1,.sequence=1,.tick=100,.stream=5,
        .observed=1,.count=1,.current="road"};
    s.entries[0]=(SudekiMpLanArenaSpiritVfxSnapshot){.instance_sequence=1,
        .emitted_host_tick=99,.rotation_xyzw={0,0,0,1},.scale={1,1,1},.owner_actor_type=255};
    for(unsigned kind=SUDEKIMP_LAN_STORY_VFX_SKILL_END;kind<=SUDEKIMP_LAN_STORY_VFX_LAST;++kind) {
        s.entries[0].kind=kind;
        assert(SudekiMpLanStoryWorldVfxSourceMask(kind));
        assert(SudekiMpStoryVfxEncode(&s,bytes,sizeof(bytes)));
        assert(SudekiMpStoryVfxDecode(bytes,sizeof(bytes),&out) && !memcmp(&s,&out,sizeof(s)));
        assert(!SudekiMpLanArenaVisualOwnerValid(&s.entries[0]));
        assert(!SudekiMpLanArenaVisualOwnerValidForParty(&s.entries[0]));
        s.entries[0].owner_actor_type=1;assert(!SudekiMpStoryVfxValid(&s));
        s.entries[0].owner_actor_type=255;s.entries[0].skill_sequence=1;
        assert(!SudekiMpStoryVfxValid(&s));s.entries[0].skill_sequence=0;
    }
    s.entries[0].kind=255;assert(!SudekiMpStoryVfxValid(&s));
    puts("PASS story ordinary-skill VFX round-trip, owner/sequence/unknown rejection, legacy profiles unchanged");
    return 0;
}
