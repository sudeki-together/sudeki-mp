#include "network/lan_story_effect_state.h"
#include <string.h>
_Static_assert(28u+SUDEKIMP_STORY_EFFECT_CHUNK_MAX<=1468u,"effect datagram exceeds path MTU");
_Static_assert(SUDEKIMP_STORY_EFFECT_CHUNKS<=8u,"bounded assembly mask");
static void put(uint8_t *p,uint32_t v) {for(unsigned i=0;i<4u;++i) p[i]=(uint8_t)(v>>(8u*i));}
static uint32_t get(const uint8_t *p) {return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint32_t digest(const uint8_t *p,size_t n) {uint32_t h=2166136261u;for(size_t i=0;i<n;++i) h=(h^p[i])*16777619u;return h;}
static BOOL name_valid(const char *n,BOOL allow_empty) {
    unsigned i=0;
    for(;i<SUDEKIMP_STORY_AREA_NAME && n[i];++i) if(n[i]<0x20 || n[i]>0x7e) return FALSE;
    if(i==SUDEKIMP_STORY_AREA_NAME) return FALSE;
    for(unsigned j=i;j<SUDEKIMP_STORY_AREA_NAME;++j) if(n[j]) return FALSE;
    return allow_empty || i>0u;
}
BOOL SudekiMpStorySpawnGroupValid(const SudekiMpStorySpawnGroup *g) {
    return g && name_valid(g->name,FALSE) && g->serial &&
        g->method>=1u && g->method<=3u && g->sequence>=-1 &&
        g->selection>=0 && g->selection<=255 &&
        (g->method!=1u || !g->selection) && name_valid(g->scope,g->method!=3u) &&
        (g->method==3u || !g->scope[0]);
}
BOOL SudekiMpStorySpawnGroupSame(const SudekiMpStorySpawnGroup *a,const SudekiMpStorySpawnGroup *b) {
    return a && b && a->serial==b->serial && a->method==b->method &&
        a->sequence==b->sequence && a->selection==b->selection &&
        !_stricmp(a->name,b->name) && !_stricmp(a->scope,b->scope);
}
BOOL SudekiMpStorySpawnEventValid(const SudekiMpStorySpawnEvent *e) {
    if(!e || !name_valid(e->group,FALSE) || !e->serial || !e->group_id || !e->resource ||
        e->point>=256u || e->entry>=2048u || e->passes < -1 || !e->passes || e->flags>65535u) return FALSE;
    for(unsigned i=0;i<3u;++i)
        if((e->position[i]&0x7f800000u)==0x7f800000u || (e->forward[i]&0x7f800000u)==0x7f800000u) return FALSE;
    return TRUE;
}
BOOL SudekiMpStoryEffectStateValid(const SudekiMpStoryEffectState *s) {
    if(!s || !name_valid(s->current,FALSE)) return FALSE;
    static const SudekiMpStorySpawnGroup zero_group;
    if(s->group_count>SUDEKIMP_STORY_SPAWN_GROUP_MAX) return FALSE;
    for(unsigned i=0;i<s->group_count;++i) {
        if(!SudekiMpStorySpawnGroupValid(&s->groups[i])) return FALSE;
        for(unsigned j=0;j<i;++j)
            if(s->groups[i].serial==s->groups[j].serial ||
                (!_stricmp(s->groups[i].name,s->groups[j].name) &&
                 !_stricmp(s->groups[i].scope,s->groups[j].scope))) return FALSE;
    }
    for(unsigned i=s->group_count;i<SUDEKIMP_STORY_SPAWN_GROUP_MAX;++i)
        if(memcmp(&s->groups[i],&zero_group,sizeof(zero_group))) return FALSE;
    if(!name_valid(s->vertical_fog,TRUE)) return FALSE;
    for(unsigned k=0;k<SUDEKIMP_STORY_SKY_KINDS;++k)
        if(!s->sky[k].serial) for(unsigned a=0;a<SUDEKIMP_STORY_SKY_ARGS;++a) if(s->sky[k].args[a]) return FALSE;
    for(unsigned i=0;i<SUDEKIMP_STORY_DESPAWN_MAX;++i)
        if(!s->despawned[i].serial != !s->despawned[i].identity ||
            s->despawned[i].death_style>4u || (!s->despawned[i].serial && s->despawned[i].death_style)) return FALSE;
    static const SudekiMpStoryLightCall zero_light;
    for(unsigned i=0;i<SUDEKIMP_STORY_LIGHT_MAX;++i) {
        if(!s->light[i].serial) { if(memcmp(&s->light[i],&zero_light,sizeof(zero_light))) return FALSE; continue; }
        if(!s->light[i].kind || s->light[i].kind>SUDEKIMP_STORY_LIGHT_KIND_LAST) return FALSE;
    }
    if(s->light_valid>1u || (!s->light_valid && (s->light_target[0] || s->light_target[1] ||
        s->light_target[2] || s->light_rate))) return FALSE;
    if(!name_valid(s->sky_dome,TRUE) || !s->sky_dome_serial != !s->sky_dome[0]) return FALSE;
    if(s->cluster_valid>1u || s->in_combat>1u || s->flag36>255u || s->suspended>1u ||
        (!s->cluster_valid && (s->cluster_hash || s->sector || s->in_combat || s->flag36 || s->suspended))) return FALSE;
    for(unsigned i=0;i<SUDEKIMP_STORY_SIGIL_MAX;++i) {
        const SudekiMpStorySigilState *r=&s->sigils[i];
        if(!r->spawn_serial) {if(r->identity || r->occurrence || r->ended) return FALSE; continue;}
        if(!r->identity || !r->occurrence || r->ended>1u) return FALSE;
        for(unsigned j=0;j<i;++j) if(s->sigils[j].spawn_serial &&
            (s->sigils[j].spawn_serial==r->spawn_serial || (s->sigils[j].identity==r->identity &&
             s->sigils[j].occurrence==r->occurrence))) return FALSE;
    }
    for(unsigned i=0;i<SUDEKIMP_STORY_DROP_MAX;++i) {
        const SudekiMpStoryDropRecord *r=&s->drops[i];
        if(!r->serial) {
            if(r->spawn_serial || r->resource || r->quantity || r->position[0] ||
                r->position[1] || r->position[2] || r->collected) return FALSE;
            continue;
        }
        if(!r->spawn_serial || !r->resource || !r->quantity || r->collected>1u) return FALSE;
        for(unsigned a=0;a<3u;++a) if((r->position[a]&0x7f800000u)==0x7f800000u) return FALSE;
        for(unsigned j=0;j<i;++j) if(s->drops[j].serial==r->serial) return FALSE;
    }
    if(s->gates_valid>1u || s->gate_count>SUDEKIMP_STORY_GATE_GROUP_MAX ||
        (!s->gates_valid && s->gate_count)) return FALSE;
    static const SudekiMpStoryGateGroup zero_gate;
    for(unsigned i=0;i<SUDEKIMP_STORY_GATE_GROUP_MAX;++i) {
        const SudekiMpStoryGateGroup *g=&s->gates[i];
        if(i>=s->gate_count) {if(memcmp(g,&zero_gate,sizeof(*g))) return FALSE; continue;}
        if(!name_valid(g->name,FALSE) || !g->serial || !g->resource ||
            !g->point_count || g->point_count>SUDEKIMP_STORY_GATE_POINTS ||
            (g->point_count<32u && (g->live_mask>>g->point_count))) return FALSE;
        for(unsigned j=0;j<i;++j)
            if(!_stricmp(g->name,s->gates[j].name) || g->serial==s->gates[j].serial) return FALSE;
    }
    if(s->spawns_valid>1u || s->spawn_count>SUDEKIMP_STORY_SPAWN_MAX ||
        (!s->spawns_valid && s->spawn_count)) return FALSE;
    static const SudekiMpStorySpawnEvent zero_spawn;
    for(unsigned i=0;i<SUDEKIMP_STORY_SPAWN_MAX;++i) {
        const SudekiMpStorySpawnEvent *e=&s->spawns[i];
        if(i>=s->spawn_count) {if(memcmp(e,&zero_spawn,sizeof(*e))) return FALSE;continue;}
        if(!SudekiMpStorySpawnEventValid(e)) return FALSE;
        for(unsigned j=0;j<i;++j) if(e->serial==s->spawns[j].serial) return FALSE;
    }
    if(s->music_flags!=0u && s->music_flags!=1u && s->music_flags!=3u) return FALSE;
    if(!name_valid(s->music_score,!(s->music_flags&1u))) return FALSE;
    if(!s->music_flags) {
        if(s->music_score[0] || s->music_intensity || s->music_variation) return FALSE;
        for(unsigned i=0;i<6u;++i) if(s->music_curve[i]) return FALSE;
    } else {
        if(strncmp(s->music_score,"BS_",3u) || !s->music_score[3] ||
            s->music_intensity>0x3f800000u) return FALSE;
        if(s->music_variation>1u) return FALSE;
        for(unsigned i=0;i<6u;++i) {
            uint32_t limit=(i==0u || i==1u || i==3u)?0x3f800000u:0x7f7fffffu;
            if(s->music_curve[i]>limit) return FALSE;
        }
        if((s->music_flags&2u) && !s->music_curve[4]) return FALSE;
        for(unsigned i=3u;s->music_score[i];++i) {
            char c=s->music_score[i];
            if(!((c>='a' && c<='z') || (c>='A' && c<='Z') ||
                (c>='0' && c<='9') || c=='_')) return FALSE;
        }
    }
    return TRUE;
}
BOOL SudekiMpStoryEffectStateEncode(const SudekiMpStoryEffectState *s,uint8_t *p,size_t n) {
    if(!p || n!=SUDEKIMP_STORY_EFFECT_WIRE_SIZE || !SudekiMpStoryEffectStateValid(s) ||
        !s->epoch || !s->revision || !s->sequence) return FALSE;
    uint8_t *e=p; put(e,SUDEKIMP_STORY_EFFECT_CODEC_VERSION); e+=4;
#define WORD(f) do {put(e,(uint32_t)s->f); e+=4;} while(0)
#define NAME(f) do {memcpy(e,s->f,SUDEKIMP_STORY_AREA_NAME); e+=SUDEKIMP_STORY_AREA_NAME;} while(0)
#define BYTE(f) do {*e++=s->f;} while(0)
    WORD(epoch); WORD(revision); WORD(observed_tick); WORD(sequence); NAME(current); BYTE(group_count);
    for(unsigned i=0;i<SUDEKIMP_STORY_SPAWN_GROUP_MAX;++i) {
        NAME(groups[i].name); WORD(groups[i].sequence); WORD(groups[i].serial);
        WORD(groups[i].method); WORD(groups[i].selection); NAME(groups[i].scope);
    }
    for(unsigned i=0;i<SUDEKIMP_STORY_SKY_KINDS;++i) {
        WORD(sky[i].serial); for(unsigned j=0;j<SUDEKIMP_STORY_SKY_ARGS;++j) {WORD(sky[i].args[j]);}
    }
    NAME(vertical_fog);
    for(unsigned i=0;i<SUDEKIMP_STORY_DESPAWN_MAX;++i) {WORD(despawned[i].serial); WORD(despawned[i].identity); WORD(despawned[i].death_style);}
    for(unsigned i=0;i<SUDEKIMP_STORY_LIGHT_MAX;++i) {
        WORD(light[i].serial); WORD(light[i].kind); for(unsigned j=0;j<4u;++j) {WORD(light[i].args[j]);}
    }
    WORD(light_valid); for(unsigned i=0;i<3u;++i) {WORD(light_target[i]);} WORD(light_rate);
    WORD(sky_dome_serial); NAME(sky_dome); WORD(sky_dome_speed);
    WORD(cluster_valid); WORD(cluster_hash); WORD(sector); WORD(in_combat); WORD(flag36); WORD(suspended);
    for(unsigned i=0;i<SUDEKIMP_STORY_SIGIL_MAX;++i) {
        WORD(sigils[i].identity); WORD(sigils[i].occurrence); WORD(sigils[i].spawn_serial); WORD(sigils[i].ended);
    }
    for(unsigned i=0;i<SUDEKIMP_STORY_DROP_MAX;++i) {
        WORD(drops[i].serial); WORD(drops[i].spawn_serial); WORD(drops[i].resource); WORD(drops[i].quantity);
        for(unsigned j=0;j<3u;++j) {WORD(drops[i].position[j]);} WORD(drops[i].collected);
    }
    WORD(music_flags); WORD(music_intensity); NAME(music_score);
    WORD(music_variation); for(unsigned i=0;i<6u;++i) {WORD(music_curve[i]);}
    BYTE(gates_valid); BYTE(gate_count);
    for(unsigned i=0;i<SUDEKIMP_STORY_GATE_GROUP_MAX;++i) {
        NAME(gates[i].name); WORD(gates[i].serial); WORD(gates[i].resource);
        WORD(gates[i].point_count); WORD(gates[i].live_mask);
    }
    BYTE(spawns_valid); BYTE(spawn_count);
    for(unsigned i=0;i<SUDEKIMP_STORY_SPAWN_MAX;++i) {
        NAME(spawns[i].group); WORD(spawns[i].serial); WORD(spawns[i].group_id);
        WORD(spawns[i].resource); WORD(spawns[i].point); WORD(spawns[i].entry); WORD(spawns[i].passes);
        for(unsigned j=0;j<3u;++j) {WORD(spawns[i].position[j]); WORD(spawns[i].forward[j]);}
        WORD(spawns[i].flags);
    }
#undef WORD
#undef NAME
#undef BYTE
    return (size_t)(e-p)==n;
}
BOOL SudekiMpStoryEffectStateDecode(const uint8_t *p,size_t n,SudekiMpStoryEffectState *out) {
    if(!p || !out || n!=SUDEKIMP_STORY_EFFECT_WIRE_SIZE || get(p)!=SUDEKIMP_STORY_EFFECT_CODEC_VERSION) return FALSE;
    SudekiMpStoryEffectState value={0},*s=&value; const uint8_t *e=p+4;
#define WORD(f) do {s->f=get(e); e+=4;} while(0)
#define NAME(f) do {memcpy(s->f,e,SUDEKIMP_STORY_AREA_NAME); e+=SUDEKIMP_STORY_AREA_NAME;} while(0)
#define BYTE(f) do {s->f=*e++;} while(0)
    WORD(epoch); WORD(revision); WORD(observed_tick); WORD(sequence); NAME(current); BYTE(group_count);
    for(unsigned i=0;i<SUDEKIMP_STORY_SPAWN_GROUP_MAX;++i) {
        NAME(groups[i].name); WORD(groups[i].sequence); WORD(groups[i].serial);
        WORD(groups[i].method); WORD(groups[i].selection); NAME(groups[i].scope);
    }
    for(unsigned i=0;i<SUDEKIMP_STORY_SKY_KINDS;++i) {
        WORD(sky[i].serial); for(unsigned j=0;j<SUDEKIMP_STORY_SKY_ARGS;++j) {WORD(sky[i].args[j]);}
    }
    NAME(vertical_fog);
    for(unsigned i=0;i<SUDEKIMP_STORY_DESPAWN_MAX;++i) {WORD(despawned[i].serial); WORD(despawned[i].identity); WORD(despawned[i].death_style);}
    for(unsigned i=0;i<SUDEKIMP_STORY_LIGHT_MAX;++i) {
        WORD(light[i].serial); WORD(light[i].kind); for(unsigned j=0;j<4u;++j) {WORD(light[i].args[j]);}
    }
    WORD(light_valid); for(unsigned i=0;i<3u;++i) {WORD(light_target[i]);} WORD(light_rate);
    WORD(sky_dome_serial); NAME(sky_dome); WORD(sky_dome_speed);
    WORD(cluster_valid); WORD(cluster_hash); WORD(sector); WORD(in_combat); WORD(flag36); WORD(suspended);
    for(unsigned i=0;i<SUDEKIMP_STORY_SIGIL_MAX;++i) {
        WORD(sigils[i].identity); WORD(sigils[i].occurrence); WORD(sigils[i].spawn_serial); WORD(sigils[i].ended);
    }
    for(unsigned i=0;i<SUDEKIMP_STORY_DROP_MAX;++i) {
        WORD(drops[i].serial); WORD(drops[i].spawn_serial); WORD(drops[i].resource); WORD(drops[i].quantity);
        for(unsigned j=0;j<3u;++j) {WORD(drops[i].position[j]);} WORD(drops[i].collected);
    }
    WORD(music_flags); WORD(music_intensity); NAME(music_score);
    WORD(music_variation); for(unsigned i=0;i<6u;++i) {WORD(music_curve[i]);}
    BYTE(gates_valid); BYTE(gate_count);
    for(unsigned i=0;i<SUDEKIMP_STORY_GATE_GROUP_MAX;++i) {
        NAME(gates[i].name); WORD(gates[i].serial); WORD(gates[i].resource);
        WORD(gates[i].point_count); WORD(gates[i].live_mask);
    }
    BYTE(spawns_valid); BYTE(spawn_count);
    for(unsigned i=0;i<SUDEKIMP_STORY_SPAWN_MAX;++i) {
        NAME(spawns[i].group); WORD(spawns[i].serial); WORD(spawns[i].group_id);
        WORD(spawns[i].resource); WORD(spawns[i].point); WORD(spawns[i].entry); WORD(spawns[i].passes);
        for(unsigned j=0;j<3u;++j) {WORD(spawns[i].position[j]); WORD(spawns[i].forward[j]);}
        WORD(spawns[i].flags);
    }
#undef WORD
#undef NAME
#undef BYTE
    if((size_t)(e-p)!=n || !s->epoch || !s->revision || !s->sequence || !SudekiMpStoryEffectStateValid(s)) return FALSE;
    *out=*s; return TRUE;
}
static size_t chunk_size(unsigned index) {
    size_t n=SUDEKIMP_STORY_EFFECT_WIRE_SIZE-index*SUDEKIMP_STORY_EFFECT_CHUNK_BYTES;
    return n<SUDEKIMP_STORY_EFFECT_CHUNK_BYTES?n:SUDEKIMP_STORY_EFFECT_CHUNK_BYTES;
}
BOOL SudekiMpStoryEffectChunkEncode(const SudekiMpStoryEffectState *s,unsigned index,uint8_t *p,size_t capacity,size_t *written) {
    uint8_t raw[SUDEKIMP_STORY_EFFECT_WIRE_SIZE];
    if(!p || !written || index>=SUDEKIMP_STORY_EFFECT_CHUNKS ||
        capacity<SUDEKIMP_STORY_EFFECT_CHUNK_HEADER+chunk_size(index) ||
        !SudekiMpStoryEffectStateEncode(s,raw,sizeof(raw))) return FALSE;
    put(p,SUDEKIMP_STORY_EFFECT_CODEC_VERSION); put(p+4,s->epoch); put(p+8,s->revision);
    put(p+12,s->sequence); put(p+16,s->observed_tick); put(p+20,digest(raw,sizeof(raw))); put(p+24,index);
    size_t n=chunk_size(index); memcpy(p+28,raw+index*SUDEKIMP_STORY_EFFECT_CHUNK_BYTES,n);
    *written=28u+n; return TRUE;
}
BOOL SudekiMpStoryEffectChunkDecode(const uint8_t *p,size_t n,SudekiMpStoryEffectChunk *out) {
    if(!p || !out || n<28u || get(p)!=SUDEKIMP_STORY_EFFECT_CODEC_VERSION ||
        get(p+24)>=SUDEKIMP_STORY_EFFECT_CHUNKS || n!=28u+chunk_size(get(p+24))) return FALSE;
    SudekiMpStoryEffectChunk c={0}; c.epoch=get(p+4); c.revision=get(p+8); c.sequence=get(p+12);
    c.observed_tick=get(p+16); c.digest=get(p+20); c.index=get(p+24);
    if(!c.epoch || !c.revision || !c.sequence) return FALSE;
    memcpy(c.bytes,p+28,n-28u); *out=c; return TRUE;
}
BOOL SudekiMpStoryEffectAssemble(const SudekiMpStoryEffectChunk *c,unsigned count,SudekiMpStoryEffectState *out) {
    if(!c || !out || count!=SUDEKIMP_STORY_EFFECT_CHUNKS) return FALSE;
    uint8_t raw[SUDEKIMP_STORY_EFFECT_WIRE_SIZE]; unsigned mask=0;
    for(unsigned i=0;i<count;++i) {
        if(c[i].index>=count || (mask&(1u<<c[i].index)) || c[i].epoch!=c[0].epoch ||
            c[i].revision!=c[0].revision || c[i].sequence!=c[0].sequence ||
            c[i].observed_tick!=c[0].observed_tick || c[i].digest!=c[0].digest) return FALSE;
        mask|=1u<<c[i].index;
        memcpy(raw+c[i].index*SUDEKIMP_STORY_EFFECT_CHUNK_BYTES,c[i].bytes,chunk_size(c[i].index));
    }
    SudekiMpStoryEffectState s;
    if(digest(raw,sizeof(raw))!=c[0].digest || !SudekiMpStoryEffectStateDecode(raw,sizeof(raw),&s) ||
        s.epoch!=c[0].epoch || s.revision!=c[0].revision || s.sequence!=c[0].sequence ||
        s.observed_tick!=c[0].observed_tick) return FALSE;
    *out=s; return TRUE;
}
