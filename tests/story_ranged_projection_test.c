/* Exercise the real read-only adapter on synthetic owned storage. No native
 * method is invoked; this is NOT an executable-installation/gameplay test. */
#include "../src/hooks/lan_story_world.c"
#include <assert.h>
#include <stdio.h>
void SudekiMpLogFormat(const char *format, ...) { (void)format; }

typedef struct TestRenderer {
    uint8_t wrapper[0x14],object[0xd0],renderer[0xb0],bank[0x5c],description[0x24];
    uint8_t entries[12*28],resources[12][0x20],channels[5*36],blends[4*20],rows[5][24];
} TestRenderer;
typedef struct Fixture {
    uint8_t actor[0x200],model[0x168],position[0x108],arbiter[0x64];
    uint8_t manager[0x64],missile_record[0xc4];
    uint8_t table[0x414],states[20],definitions[13][0x58];
    TestRenderer body,arms;
} Fixture;
static void pointer(uint8_t *p,unsigned offset,void *v) { memcpy(p+offset,&v,4); }
static void word(uint8_t *p,unsigned offset,uint32_t v) { memcpy(p+offset,&v,4); }
static void number(uint8_t *p,unsigned offset,float v) { memcpy(p+offset,&v,4); }
static void renderer(TestRenderer *r,uint32_t first,uint32_t second) {
    pointer(r->wrapper,8,r->object); pointer(r->wrapper,0x10,r->renderer);
    pointer(r->object,0x14,r->renderer); pointer(r->renderer,0,base+0x2df8ec);
    pointer(r->renderer,8,r->bank); pointer(r->bank,0x1c,r->description);
    pointer(r->bank,0x20,r->entries); word(r->description,0,12); word(r->description,0xc,1);
    pointer(r->renderer,0x98,r->channels); pointer(r->renderer,0x9c,r->blends);
    word(r->renderer,0xa0,5); word(r->renderer,0xa4,4);
    for(unsigned i=0;i<12;++i) pointer(r->entries,i*28,r->resources[i]);
    word(r->resources[1],0,first); word(r->resources[2],0,second);
    number(r->resources[1],4,60); number(r->resources[2],4,60);
    word(r->resources[3],0,0x55555555); number(r->resources[3],4,40);
    word(r->resources[4],0,0x66666666); number(r->resources[4],4,30);
    word(r->resources[5],0,0x77777778); number(r->resources[5],4,1);
    for(unsigned i=6;i<12;++i) {
        word(r->resources[i],0,0x80000000u+i); number(r->resources[i],4,(i&1)?20:24);
    }
    for(unsigned c=0;c<5;++c) pointer(r->channels,c*36,r->rows[c]);
    static const uint32_t topology[]={0x00010000,0x00030002,0x80018000,0x00048002};
    for(unsigned i=0;i<4;++i) word(r->blends,i*20,topology[i]);
}
static void effect_renderer_ownership(void) {
    uint8_t owner[0x40]={0},effects[129][0x48]={{0}},actor[0x48]={0};
    uint8_t position[130][0xb8]={{0}},wrapper[130][0x14]={{0}};
    uint8_t renderers[130][4]={{0}};
    void *entries[130],*out[128];
    for(unsigned i=0;i<130u;++i) {
        uint8_t *e=i<129u?effects[i]:actor;
        entries[i]=e;
        pointer(e,0,base+(i<129u?0x2d3c7cu:0x2d5fa0u));
        pointer(e,8,base+(i<129u?0x2d3ca4u:0x2d5fc4u));
        pointer(e,0x44,position[i]);
        pointer(position[i],0,base+0x2cdefc);
        pointer(position[i],0x10,e); pointer(position[i],0xb4,wrapper[i]);
        pointer(wrapper[i],0x10,renderers[i]);
    }
    word(owner,0x34,130); pointer(owner,0x3c,entries);
    pointer(base,0x409d8c,owner);
    /* Effects created before an actor must neither freeze their own native
     * animation nor crowd the replicated actor out of the 128-entry list. */
    assert(SudekiMpLanStoryWorldOwnedRenderers(out,128)==1);
    assert(out[0]==renderers[129]);
    /* Unknown/mismatched classes retain the conservative exclusion. */
    pointer(effects[0],8,base);
    assert(SudekiMpLanStoryWorldOwnedRenderers(out,128)==2);
    assert(out[0]==renderers[0] && out[1]==renderers[129]);
    pointer(effects[0],8,base+0x2d3ca4u);
    /* H1b: an NPC of the client's locally entered shop that the host does
     * not pose keeps its native idle clock; posed or foreign NPCs do not. */
    uint8_t npc[0x48]={0},npc_position[0xb8]={0},npc_wrapper[0x14]={0},npc_renderer[4]={0};
    uint8_t zone[0x12a]={0},cluster[0x1c]={0},spawns[0x90]={0};
    pointer(npc,0,base+0x2d65a0u); pointer(npc,0x44,npc_position);
    pointer(npc_position,0,base+0x2cdefc); pointer(npc_position,0x10,npc); pointer(npc_position,0xb4,npc_wrapper);
    pointer(npc_wrapper,0x10,npc_renderer);
    pointer(zone,0,base+0x2cdcdcu); pointer(zone,0x118,cluster); word(cluster,0x10,1); pointer(cluster,0x18,spawns);
    pointer(spawns,0x78,npc+0x2c);
    void *with_npc[131]; memcpy(with_npc,entries,sizeof(entries)); with_npc[130]=npc;
    word(owner,0x34,131); pointer(owner,0x3c,with_npc); pointer(base,0x409d8c,owner);
    assert(SudekiMpLanStoryWorldOwnedRenderers(out,128)==2 && out[1]==npc_renderer); /* client_seen is still FALSE */
    SudekiMpLanStoryWorldSetLocalZone(zone);
    BOOL seen=client_seen; unsigned bound=client_count; client_seen=FALSE;
    assert(!SudekiMpLanStoryWorldLocalIdleNpc(npc));
    assert(SudekiMpLanStoryWorldOwnedRenderers(out,128)==2);                     /* unknown: owned */
    client_seen=TRUE; client_count=0;
    assert(SudekiMpLanStoryWorldLocalIdleNpc(npc));
    assert(SudekiMpLanStoryWorldOwnedRenderers(out,128)==1 && out[0]==renderers[129]); /* idles locally */
    client_bound[0].target.entity=npc; client_count=1;
    assert(!SudekiMpLanStoryWorldLocalIdleNpc(npc));
    assert(SudekiMpLanStoryWorldOwnedRenderers(out,128)==2);                     /* host-posed: owned */
    client_bound[0].target.entity=NULL; client_count=0;
    pointer(spawns,0x78,actor+0x2c);
    assert(!SudekiMpLanStoryWorldLocalIdleNpc(npc));
    assert(SudekiMpLanStoryWorldOwnedRenderers(out,128)==1);                     /* Q1: unbound NPC idles locally, not only shop NPCs */
    pointer(spawns,0x78,npc+0x2c); pointer(zone,0,base);
    assert(!SudekiMpLanStoryWorldLocalIdleNpc(npc));
    assert(SudekiMpLanStoryWorldOwnedRenderers(out,128)==1);                     /* Q1: independent of shop proof */
    pointer(zone,0,base+0x2cdcdcu);
    uint32_t zone_serial=SudekiMpLanStoryWorldOwnershipSerial();
    SudekiMpLanStoryWorldSetLocalZone(zone);
    assert(SudekiMpLanStoryWorldOwnershipSerial()==zone_serial);
    SudekiMpLanStoryWorldSetLocalZone(NULL);
    assert(SudekiMpLanStoryWorldOwnershipSerial()==zone_serial+1u);
    assert(!SudekiMpLanStoryWorldLocalIdleNpc(npc));
    assert(SudekiMpLanStoryWorldOwnedRenderers(out,128)==1);
    client_seen=seen; client_count=bound;
    pointer(base,0x409d8c,NULL);
}
static void hit_reactions(Fixture *f,Target t) {
    Fixture saved=*f;
    /* Both ranged characters use an arms-only flinch absent from the body
     * bank. Exercise entry, promotion, recovery and invalid resources through
     * the real adapter; HP changes alone never manufacture a reaction. */
    for(unsigned c=1;c<=3;c+=2) {
        *f=saved; t.character=c;
        pointer(f->table,0x14+0x36*4,f->definitions[4]);
        word(f->definitions[4],0x14,0x44444444);
        pointer(f->table,0x14+0x2a*4,f->definitions[5]);
        word(f->definitions[5],0x14,0x55555555);
        number(f->arms.resources[2],4,9); number(f->body.resources[3],4,18);
        f->states[10]=0x36; word(f->arms.rows[2],0,2|(1u<<16));
        number(f->arms.rows[2],8,4.5f); number(f->arms.rows[2],4,24);
        number(f->arms.blends,2*20+0xc,.4f);
        Fixture untouched=*f; SudekiMpLanStoryWorldActor out; BOOL stable=TRUE;
        assert(read_pose_detail(&t,&out,&stable) && !stable);
        assert(out.clip[0]==0x11111111 && out.clip[2]==0x55555555);
        assert(out.time[2]==9 && out.rate[2]==48 && fabsf(out.blend[2]-.4f)<.00001f);
        assert(!out.clip[4] && out.blend[3]==0 && !ranged_body_pose(&t,&out));
        assert(!memcmp(f,&untouched,sizeof(*f)));
        f->states[2]=0x36; f->states[10]=0;
        memcpy(f->arms.rows[0],f->arms.rows[2],24);
        memset(f->arms.rows[2],0,24); word(f->arms.rows[2],0,192u<<16);
        number(f->arms.blends,2*20+0xc,0);
        assert(read_pose_detail(&t,&out,&stable) && !stable);
        assert(out.clip[0]==0x55555555 && out.time[0]==9 && out.rate[0]==48);
        assert(!out.clip[2] && !out.clip[4]);
        word(f->definitions[4],0x14,0x77777777);
        assert(!read_pose_detail(&t,&out,&stable)); /* source must be authored */
        word(f->definitions[4],0x14,0x44444444);
        word(f->definitions[5],0x14,0x77777777);
        assert(!read_pose_detail(&t,&out,&stable)); /* no fabricated body clip */
        word(f->definitions[5],0x14,0x55555555);
        memcpy(f->arms.rows,saved.arms.rows,sizeof(f->arms.rows));
        memcpy(f->states,saved.states,sizeof(f->states));
        assert(read_pose_detail(&t,&out,&stable) && stable);
        assert(out.clip[0]==0x11111111 && !out.clip[2]);
        assert(c==3 ? out.clip[4]==0x77777778 : !out.clip[4]);
    }
    /* Once a full body is attached, all four roster members retain the
     * selected native reaction, timing and blend, including strong/floor
     * reactions. Neither ranged mapping nor aimed stance may replace it. */
    for(unsigned c=0;c<4;++c) {
        *f=saved; t.character=c; t.attached_wrapper=t.wrapper;
        word(f->body.renderer,0xa0,(c==1 || c==3)?5:4);
        word(f->body.renderer,0xa4,(c==1 || c==3)?4:3);
        assert(renderer_target(t,&t,FALSE));
        for(unsigned slot=2;slot<=4;++slot) {
            for(unsigned ch=0;ch<t.channel_count;++ch) word(f->body.rows[ch],0,192u<<16);
            word(f->body.rows[0],0,slot|(1u<<16));
            number(f->body.rows[0],4,24); number(f->body.rows[0],8,7);
            SudekiMpLanStoryWorldActor out; BOOL stable=TRUE; Fixture untouched=*f;
            assert(read_pose_detail(&t,&out,&stable) && !stable);
            assert(out.clip[0]==*(uint32_t *)f->body.resources[slot]);
            assert(out.time[0]==7 && out.rate[0]==24 && out.state[0]==1);
            assert(!ranged_body_pose(&t,&out) && !memcmp(f,&untouched,sizeof(*f)));
        }
        /* Native attached bodies already author their own idle/move blend.
         * Preserve that exact four-channel transition for the entire roster. */
        for(unsigned ch=0;ch<4;++ch) {
            word(f->body.rows[ch],0,ch+1u);number(f->body.rows[ch],4,12.f);
            number(f->body.rows[ch],8,3.f);number(f->body.rows[ch],0xc,3.f);
        }
        for(unsigned step=1;step<5;++step) {
            number(f->body.blends,0xc,.99f);number(f->body.blends,2*20+0xc,step*.2f);
            SudekiMpLanStoryWorldActor out;BOOL stable=TRUE;Fixture untouched=*f;
            assert(read_pose_detail(&t,&out,&stable) && !stable);
            for(unsigned ch=0;ch<4;++ch) {
                assert(out.clip[ch]==*(uint32_t *)f->body.resources[ch+1]);
                assert(out.time[ch]==3.f && out.rate[ch]==12.f);
            }
            assert(fabsf(out.blend[0]-.99f)<.00001f && fabsf(out.blend[2]-step*.2f)<.00001f);
            assert(!memcmp(f,&untouched,sizeof(*f)));
        }
    }
    *f=saved;
}
static void strafe_projection(Fixture *f,Target t) {
    Fixture saved=*f;
    for(unsigned character=1;character<=3;character+=2) for(unsigned firing=0;firing<2;++firing) {
        *f=saved; t.character=character;
        for(unsigned semantic=8;semantic<=13;++semantic) {
            pointer(f->table,0x14+semantic*4,f->definitions[semantic-1]);
            word(f->definitions[semantic-1],0x14,0x80000000u+semantic-2);
            word(f->definitions[semantic-1],0x54,0x3c00u|(semantic<<16));
        }
        if(firing) {
            f->states[2]=0x8c;word(f->arms.rows[0],0,2|(1u<<16));
            number(f->arms.rows[0],4,24);number(f->arms.rows[0],8,2);
        }
        assert(renderer_target(t,&t,FALSE));
        /* For aim+Z the established native/Test Room mode2 (left) travels
         * +X; mode3 (right) travels-X. Verify both characters' actual bank
         * handles, not just a self-consistent pair of direction labels. */
        const float directions[4][2]={{0,.2f},{0,-.2f},{.2f,0},{-.2f,0}};
        const unsigned slots[4][2]={{3,4},{6,7},{8,9},{10,11}};
        Fixture untouched=*f;
        for(unsigned direction=0;direction<4;++direction) {
            SudekiMpLanStoryWorldActor out;BOOL stable=FALSE;
            assert(read_pose_detail(&t,&out,&stable) && stable);
            Bound prior={.target=t,.previous=out,.ranged_base=TRUE,.tick=1000},next={.target=t,.ranged_base=TRUE,.tick=1100};
            uint32_t fire_clip=out.clip[4];float fire_time=out.time[4],fire_blend=out.blend[3];
            out.position[0]+=directions[direction][0];out.position[2]+=directions[direction][1];
            assert(project_ranged_translation(&t,&prior,1100,&out,&next));
            assert(next.ranged_move_semantic==6+2*direction);
            for(unsigned c=0;c<2;++c) {
                unsigned slot=slots[direction][c];
                assert(out.clip[c]==*(uint32_t *)f->body.resources[slot]);
                float length=*(float *)(f->body.resources[slot]+4);
                float expected_period=direction ? .835f : 40.f/24.f*.01f+30.f/(24.f*1.2978515625f)*.99f;
                assert(fabsf(length/out.rate[c]-expected_period)<.00001f);
                out.time[c]=length*.5f;
            }
            assert(out.clip[4]==fire_clip && out.time[4]==fire_time && out.blend[3]==fire_blend);
            if(character==3) assert(ranged_body_pose(&t,&out));
            /* Reverse direction: retain the gait phase, not a zero restart.
             * A native hit still goes through the separate reaction bridge. */
            next.previous=out;prior=next;unsigned opposite=direction^1u;
            assert(read_pose_detail(&t,&out,&stable) && stable);
            out.position[0]=prior.previous.position[0]+directions[opposite][0];
            out.position[2]=prior.previous.position[2]+directions[opposite][1];
            next.tick=1200;
            assert(project_ranged_translation(&t,&prior,1200,&out,&next));
            assert(retain_ranged_base(&t,&prior,1200,&out));
            for(unsigned c=0;c<2;++c) {
                float length=*(float *)(f->body.resources[slots[opposite][c]]+4);
                assert(fabsf(out.time[c]-(length*.5f+out.rate[c]*.1f))<.00001f);
            }
            assert(out.clip[4]==fire_clip && out.time[4]==fire_time && out.blend[3]==fire_blend);
            next.previous=out;prior=next;
            assert(read_pose_detail(&t,&out,&stable));memcpy(out.position,prior.previous.position,12);
            assert(project_ranged_translation(&t,&prior,1300,&out,&next));
            assert(out.clip[0]==prior.previous.clip[0]); /* stop grace retains direction */
            assert(read_pose_detail(&t,&out,&stable));memcpy(out.position,prior.previous.position,12);
            assert(project_ranged_translation(&t,&prior,1401,&out,&next) && !out.clip[1]);
        }
        /* The complete capture path must fade both entering and leaving
         * motion, preserve the independent firing layer, and promote idle
         * without resetting its already advancing phase. */
        for(unsigned direction=0;direction<4;++direction) {
            SudekiMpLanStoryWorldActor out;BOOL stable;
            assert(read_pose_detail(&t,&out,&stable) && stable);
            Bound prior={.target=t,.previous=out,.ranged_base=TRUE,.tick=1000};
            float position[3];memcpy(position,out.position,12);
            unsigned starts=0,stops=0;float prior_weight=1.f;
            for(unsigned step=1;step<=40;++step) {
                uint32_t tick=1000+step*20;
                Bound next={.target=t,.ranged_base=TRUE,.tick=tick};
                assert(read_pose_detail(&t,&out,&stable) && stable);
                if(step<=12) {position[0]+=directions[direction][0];position[2]+=directions[direction][1];}
                memcpy(out.position,position,12);
                uint32_t fire_clip=out.clip[4];float fire_time=out.time[4],fire_blend=out.blend[3];
                assert(project_ranged_translation(&t,&prior,tick,&out,&next));
                assert(retain_ranged_base(&t,&prior,tick,&out));
                float weight=out.clip[1]?(out.clip[2]?out.blend[2]:0.f):1.f;
                assert(fabsf(weight-prior_weight)<=.20001f);
                if(out.clip[2]) {
                    assert(out.clip[2]==0x11111111 && out.state[2]==0 && out.rate[2]==12.f);
                    assert(out.blend[1]==0 && out.blend[2]>0 && out.blend[2]<1);
                    if(step<=12) ++starts;else ++stops;
                }
                if(prior.previous.clip[2] && !out.clip[1]) {
                    float expected=prior.previous.time[2]+.24f;
                    assert(out.clip[0]==prior.previous.clip[2] && fabsf(out.time[0]-expected)<.0001f);
                }
                assert(out.clip[4]==fire_clip && out.time[4]==fire_time && out.blend[3]==fire_blend);
                assert(!memcmp(out.position,position,12));
                if(character==3) assert(ranged_body_pose(&t,&out));
                prior_weight=weight;next.previous=out;prior=next;
            }
            assert(starts>=4 && stops>=4 && !out.clip[1] && !out.clip[2] && out.time[0]>0);
            /* A new scene/owner or a stale gap must not carry a fading gait. */
            Bound stale=prior;stale.tick-=500;
            assert(read_pose_detail(&t,&out,&stable));
            assert(retain_ranged_base(&t,&stale,prior.tick,&out) && !out.clip[1] && !out.clip[2]);
            stale=prior;stale.target.entity=NULL;
            assert(retain_ranged_base(&t,&stale,prior.tick+20,&out) && !out.clip[1] && !out.clip[2]);
        }
        /* At rest the live host drifts about .0004 units/second on this
         * ground. That must not accumulate into a 150ms side-step every
         * 12 seconds. Deliberate slow motion still accumulates across frames. */
        for(unsigned slow=0;slow<2;++slow) {
            SudekiMpLanStoryWorldActor out;BOOL stable;
            assert(read_pose_detail(&t,&out,&stable) && stable);
            Bound prior={.target=t,.previous=out,.ranged_base=TRUE,.tick=1000};
            unsigned moving=0;
            for(unsigned step=1;step<=1500;++step) {
                uint32_t tick=1000+step*20;
                Bound next={.target=t,.ranged_base=TRUE,.tick=tick};
                assert(read_pose_detail(&t,&out,&stable) && stable);
                out.position[0]=step*(slow?.001f:.000008f);
                uint32_t fire_clip=out.clip[4];float fire_time=out.time[4],fire_blend=out.blend[3];
                assert(project_ranged_translation(&t,&prior,tick,&out,&next));
                if(out.clip[1]) ++moving;
                assert(out.clip[4]==fire_clip && out.time[4]==fire_time && out.blend[3]==fire_blend);
                next.previous=out;prior=next;
            }
            assert(slow ? moving>1450 : moving==0);
        }
        assert(!memcmp(&untouched,f,sizeof(*f)));
        /* Missing authored side clips refuse projection; no raw selector or
         * exploration-bank fallback, and the host bank is never modified. */
        SudekiMpLanStoryWorldActor out;BOOL stable;
        assert(read_pose_detail(&t,&out,&stable));
        Bound prior={.target=t,.previous=out,.ranged_base=TRUE,.tick=1000},next={0};
        word(f->definitions[9],0x14,0xdeadbeef);out.position[0]+=.2f;
        assert(!project_ranged_translation(&t,&prior,1100,&out,&next));
        word(f->definitions[9],0x14,0x80000008);
        word(f->definitions[9],0x54,0x0c3c00); /* foreign semantic */
        assert(!project_ranged_translation(&t,&prior,1100,&out,&next));
        word(f->definitions[9],0x54,0x0a0000); /* missing authored speed */
        assert(!project_ranged_translation(&t,&prior,1100,&out,&next));
        word(f->definitions[9],0x54,0x0a3c00);
        assert(project_ranged_translation(&t,&prior,1100,&out,&next));
    }
    *f=saved;
}
int main(void) {
    base=VirtualAlloc(NULL,0x410000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    assert(base);
    effect_renderer_ownership();
    /* A material dependency waits without entering animation acquisition.
     * Neither a stale error nor another actor/stage can claim that route. */
    Target pending={.identifier=123};
    observe_stage("material_resource_waiting",123,0); SetLastError(ERROR_IO_PENDING);
    assert(prepare_missing_resource(&pending,NULL,NULL,NULL,NULL)==ERROR_IO_PENDING);
    SetLastError(ERROR_INVALID_DATA);
    assert(prepare_missing_resource(&pending,NULL,NULL,NULL,NULL)==ERROR_NOT_SUPPORTED);
    observe_stage("material_resource_waiting",124,0); SetLastError(ERROR_IO_PENDING);
    assert(prepare_missing_resource(&pending,NULL,NULL,NULL,NULL)==ERROR_NOT_SUPPORTED);
    observe_stage("material_owner",123,0); SetLastError(ERROR_IO_PENDING);
    assert(prepare_missing_resource(&pending,NULL,NULL,NULL,NULL)==ERROR_NOT_SUPPORTED);
    for(unsigned i=0;i<sizeof(identities)/sizeof(identities[0]);++i)
        pointer(base,0x2df8ec+identities[i].slot,base+identities[i].rva);
    pointer(base,0x2df8ec+0x184,base+0x21bf50);
    pointer(base,0x2d65e4+0xc,base+0x3ae30);
    Fixture *f=calloc(1,sizeof(*f)),*before=malloc(sizeof(*f)); assert(f && before);
    renderer(&f->body,0x11111111,0x22222222);
    renderer(&f->arms,0x33333333,0x44444444);
    number(f->body.resources[1],4,55);
    pointer(f->actor,0x134,f->model); pointer(f->actor,0x90,f->arbiter);
    pointer(f->model,0x160,f->arms.wrapper); pointer(f->model,0x164,f->body.wrapper);
    pointer(f->model,0xdc,f->table); pointer(f->model,0xf8,f->states);
    pointer(f->position,0xb4,f->arms.wrapper); number(f->position,0x58,1);
    pointer(f->arbiter,0,base+0x2cc9ac); pointer(f->arbiter,0x10,f->actor);
    word(f->arbiter,0x50,0x400000);
    f->states[2]=5;
    pointer(f->table,0x14+5*4,f->definitions[0]); word(f->definitions[0],0x14,0x33333333);
    pointer(f->table,0x14+2*4,f->definitions[1]); word(f->definitions[1],0x14,0x11111111);
    word(f->definitions[1],0x20,0x22222222); /* must not choose exploration */
    pointer(f->table,0x14+0x98*4,f->definitions[6]); word(f->definitions[6],0x14,0x77777778);
    word(f->arms.rows[0],0,1); number(f->arms.rows[0],4,12);
    number(f->arms.rows[0],8,30); number(f->arms.rows[0],0xc,30);
    for(unsigned c=1;c<5;++c) word(f->arms.rows[c],0,192u<<16);
    Target t={.entity=f->actor,.model=f->model,.position=f->position,
        .wrapper=f->body.wrapper,.attached_wrapper=f->arms.wrapper,.character=3,
        .kind=SUDEKIMP_LAN_STORY_WORLD_PC_KIND,.identifier=0x8557d453};
    assert(renderer_target(t,&t,FALSE));
    SudekiMpLanStoryWorldActor out;
    memcpy(before,f,sizeof(*f));
    assert(read_pose(&t,&out));
    assert(out.clip[0]==0x11111111 && out.state[0]==0 && out.time[0]==0);
    assert(out.rate[0]==12 && out.bank_fingerprint==t.fingerprint);
    for(unsigned c=1;c<4;++c) assert(!out.clip[c] && out.state[c]==192 && !out.time[c]);
    assert(out.clip[4]==0x77777778 && out.state[4]==0 && out.time[4]==0 && out.rate[4]==0 && out.blend[3]==1);
    t.character=1; assert(read_pose(&t,&out) && !out.clip[4] && out.blend[3]==0);t.character=3;
    assert(read_pose(&t,&out));
    word(f->definitions[6],0x14,0x77777777);assert(!read_pose(&t,&out));
    word(f->definitions[6],0x14,0x77777778);
    number(f->body.resources[5],4,2);assert(!read_pose(&t,&out));number(f->body.resources[5],4,1);
    assert(!memcmp(before,f,sizeof(*f))); /* no host-side renderer writes */
    hit_reactions(f,t);
    /* Repeated native FP channel2 -> channel0 promotion must NEVER replace
     * the body base. Its independent clock survives the next shot's reset. */
    pointer(f->table,0x14+0x8c*4,f->definitions[2]);
    pointer(f->table,0x14+0x85*4,f->definitions[3]);
    word(f->definitions[2],0x14,0x44444444); word(f->definitions[3],0x14,0x22222222);
    pointer(f->actor,0xbc,f->manager); pointer(f->manager,0,base+0x2d4c8c);
    pointer(f->manager,0x10,f->actor); pointer(f->manager,0x60,f->missile_record);
    word(f->missile_record,0x98,0x85); word(f->missile_record,0x9c,0x8c);
    number(f->arms.resources[2],4,4); number(f->body.resources[2],4,4);
    f->states[10]=0x8c; word(f->arms.rows[2],0,2|(1u<<16));
    number(f->arms.blends,2*20+0xc,.4f);
    BOOL stable=FALSE; assert(read_pose_detail(&t,&out,&stable) && stable);
    assert(out.clip[0]==0x11111111 && out.clip[2]==0 && out.clip[4]==0x22222222);
    assert(out.state[4]==1 && out.time[4]==0 && out.blend[2]==0 && fabsf(out.blend[3]-.4f)<.0001f);
    word(f->missile_record,0x9c,0x8d); assert(!read_pose_detail(&t,&out,&stable));
    word(f->missile_record,0x9c,0x8c);
    word(f->missile_record,0x98,0x87); assert(!read_pose_detail(&t,&out,&stable));
    pointer(f->table,0x14+0x87*4,f->definitions[3]);
    assert(read_pose_detail(&t,&out,&stable) && out.clip[4]==0x22222222);
    word(f->missile_record,0x98,0x85);
    assert(read_pose_detail(&t,&out,&stable));
    Bound prior={.target=t,.previous=out,.ranged_base=TRUE,.tick=1000};
    prior.previous.time[0]=10;
    assert(retain_ranged_base(&t,&prior,1100,&out) && fabsf(out.time[0]-11.2f)<.001f);
    f->states[2]=0x8c; f->states[10]=0;
    word(f->arms.rows[0],0,2|(1u<<16)); number(f->arms.rows[0],8,2);
    number(f->arms.rows[0],0xc,2); number(f->arms.rows[0],4,24);
    word(f->arms.rows[2],0,192u<<16); number(f->arms.blends,2*20+0xc,0);
    assert(read_pose_detail(&t,&out,&stable) && stable);
    assert(out.clip[0]==0x11111111 && out.clip[4]==0x22222222 && out.time[4]==2);
    assert(out.blend[3]==1 && out.rate[0]==12);
    assert(retain_ranged_base(&t,&prior,1200,&out) && fabsf(out.time[0]-12.4f)<.001f);
    prior.previous.time[0]=54.5f;
    assert(retain_ranged_base(&t,&prior,1100,&out) && fabsf(out.time[0]-.7f)<.001f);
    out.time[0]=0; assert(retain_ranged_base(&t,&prior,1300,&out) && out.time[0]==0);
    prior.target.entity=NULL;
    assert(retain_ranged_base(&t,&prior,1100,&out) && out.time[0]==0);
    /* Translation supplies the missing body locomotion while FP idle/fire
     * supplies only the upper-body layer. No host rows are ever changed. */
    pointer(f->table,0x14+6*4,f->definitions[4]); word(f->definitions[4],0x14,0x55555555);
    word(f->definitions[4],0x54,0x063c00);
    pointer(f->table,0x14+7*4,f->definitions[5]); word(f->definitions[5],0x14,0x66666666);
    word(f->definitions[5],0x54,0x073d31);
    assert(read_pose_detail(&t,&out,&stable) && stable);
    prior=(Bound){.target=t,.previous=out,.ranged_base=TRUE,.tick=1000};
    Bound next={.target=t,.ranged_base=TRUE,.tick=1100};
    out.position[2]=.2f;
    Fixture moving_before=*f;
    assert(project_ranged_translation(&t,&prior,1100,&out,&next));
    assert(out.clip[0]==0x55555555 && out.clip[1]==0x66666666 && fabsf(out.blend[0]-.99f)<.0001f);
    assert(fabsf(out.rate[0]-41.23008f)<.0001f && fabsf(out.rate[1]-30.92256f)<.0001f);
    assert(out.clip[4]==0x22222222 && out.blend[3]==1 && out.time[4]==2);
    assert(next.translated_tick==1100 && !memcmp(&moving_before,f,sizeof(*f)));
    assert(retain_ranged_base(&t,&prior,1100,&out) && out.time[0]==0 && out.time[1]==0);
    next.previous=out; prior=next;
    assert(read_pose_detail(&t,&out,&stable)); out.position[2]=.4f;
    assert(project_ranged_translation(&t,&prior,1200,&out,&next));
    assert(retain_ranged_base(&t,&prior,1200,&out));
    assert(fabsf(out.time[0]-4.123008f)<.0001f && fabsf(out.time[1]-3.092256f)<.0001f);
    next.tick=1200; next.previous=out; prior=next;
    assert(read_pose_detail(&t,&out,&stable)); out.position[2]=.4f;
    assert(project_ranged_translation(&t,&prior,1300,&out,&next) && out.clip[1]==0x66666666);
    assert(read_pose_detail(&t,&out,&stable)); out.position[2]=.4f;
    assert(project_ranged_translation(&t,&prior,1400,&out,&next) && !out.clip[1]);
    /* Unknown owners, gaps and teleports do not inherit a walking lease. */
    assert(project_ranged_translation(&t,NULL,1300,&out,&next) && !next.translated_tick);
    assert(project_ranged_translation(&t,&prior,1600,&out,&next) && !next.translated_tick);
    out.position[2]=100; assert(project_ranged_translation(&t,&prior,1300,&out,&next) && !next.translated_tick);
    prior.target.entity=NULL; out.position[2]=.5f;
    assert(project_ranged_translation(&t,&prior,1300,&out,&next) && !next.translated_tick);
    prior=(Bound){.target=t,.previous=out,.ranged_base=TRUE,.tick=2000};
    prior.previous.position[2]=0; assert(read_pose_detail(&t,&out,&stable)); out.position[2]=.003f;
    assert(project_ranged_translation(&t,&prior,2100,&out,&next) && !out.clip[1]);
    next.tick=2100; next.previous=out; prior=next; out.position[2]=.006f;
    assert(project_ranged_translation(&t,&prior,2200,&out,&next) && out.clip[1]==0x66666666);
    word(f->definitions[5],0x14,0x77777777);
    assert(!project_ranged_translation(&t,&prior,2200,&out,&next)); /* no exploration fallback */
    word(f->definitions[5],0x14,0x66666666);
    pointer(f->table,0x14+0x86*4,f->definitions[3]);
    pointer(f->table,0x14+0x87*4,f->definitions[3]);
    assert(read_pose_detail(&t,&out,&stable) && stable);
    assert(ranged_body_pose(&t,&out));
    memcpy(out.forward,(float[]){0,.6f,.8f},12);
    float forward[3];Fixture no_root_write=*f;
    assert(body_forward(&t,&out,forward) && forward[0]==0 && forward[1]==0 && forward[2]==1);
    assert(out.forward[1]>.59f && !memcmp(&no_root_write,f,sizeof(*f)));
    prior=(Bound){.target=t,.previous=out,.ranged_base=TRUE,.tick=2000};
    out.position[2]+=.2f;
    assert(project_ranged_translation(&t,&prior,2100,&out,&next) && ranged_body_pose(&t,&out));
    assert(body_forward(&t,&out,forward) && forward[1]==0);
    out.clip[2]=0x22222222;out.state[2]=1; /* skill/nonprojected pose: no invented flat transform */
    assert(!ranged_body_pose(&t,&out));
    assert(body_forward(&t,&out,forward) && !memcmp(forward,out.forward,12));
    out.clip[2]=0;out.state[2]=192;t.character=2;
    assert(!ranged_body_pose(&t,&out));t.character=3;
    /* Final FP recoil -> idle retains authored staff aim, with no timer or
     * continued shot. Walking still owns channels0/1 beneath that stance. */
    f->states[2]=5;word(f->arms.rows[0],0,1);number(f->arms.rows[0],0xc,2);
    assert(read_pose_detail(&t,&out,&stable) && stable && ranged_body_pose(&t,&out));
    assert(out.clip[4]==0x77777778 && out.blend[3]==1 && out.state[4]==0 && !out.rate[4]);
    prior=(Bound){.target=t,.previous=out,.ranged_base=TRUE,.tick=3000};out.position[2]+=.2f;
    assert(project_ranged_translation(&t,&prior,3100,&out,&next) && ranged_body_pose(&t,&out));
    assert(out.clip[1]==0x66666666 && out.clip[4]==0x77777778 && out.blend[3]==1);
    out.state[4]=1;assert(!ranged_body_pose(&t,&out));
    strafe_projection(f,t);
    memcpy(f,before,sizeof(*f));
    word(f->definitions[0],0x14,0x77777777); assert(!read_pose(&t,&out));
    memcpy(f,before,sizeof(*f));
    word(f->definitions[1],0x14,0x77777777); word(f->definitions[1],0x20,0x7ffff);
    assert(!read_pose(&t,&out)); /* no raw FP selector fallback */
    memcpy(f,before,sizeof(*f));
    word(f->arms.rows[0],0,0); assert(!read_pose(&t,&out)); /* no empty body */
    memcpy(f,before,sizeof(*f));
    number(f->arms.rows[0],0xc,61); assert(!read_pose(&t,&out));
    memcpy(f,before,sizeof(*f));
    word(f->arbiter,0x50,0); assert(!read_pose(&t,&out));
    memcpy(f,before,sizeof(*f));
    pointer(f->position,0xb4,f->body.wrapper); assert(!read_pose(&t,&out));
    /* Ordinary attached world pose remains unchanged by this bridge. */
    t.attached_wrapper=t.wrapper;
    word(f->body.rows[0],0,1); number(f->body.rows[0],4,12);
    number(f->body.rows[0],0xc,10);
    assert(read_pose(&t,&out) && out.clip[0]==0x11111111 && out.time[0]==10);
    free(before); free(f); VirtualFree(base,0,MEM_RELEASE); base=NULL;
    puts("story ranged projection adapter tests passed"); return 0;
}
