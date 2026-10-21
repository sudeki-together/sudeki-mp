/* Q2 regressions share the capacity fixture's real production world adapter. */
static unsigned omission(unsigned id,const char *cause,const char *pose_stage) {
    unsigned n=0;char identity[48],why[48],where[80];
    snprintf(identity,sizeof(identity)," wire_identifier=%08lx ",(unsigned long)id);
    snprintf(why,sizeof(why)," cause=%s ",cause);
    snprintf(where,sizeof(where)," pose_stage=%s ",pose_stage);
    for(unsigned i=0;i<target_log;++i) {
        assert(strstr(omission_text[i],"event=entity_omitted side=") && !strstr(omission_text[i],"refused"));
        if(strstr(omission_text[i],identity) && strstr(omission_text[i],why) && strstr(omission_text[i],where)) ++n;
    }
    return n;
}
static void valid_wire(const SudekiMpLanStoryWorldFrame *f) {
    assert(SudekiMpLanStoryWorldFrameValid(f));
    for(unsigned i=0;i<f->count;++i) {assert(f->actors[i].generation);for(unsigned j=0;j<i;++j)assert(f->actors[i].generation!=f->actors[j].generation);}
    for(unsigned i=0;i<(f->count+7u)/8u;++i) {
        uint8_t bytes[SUDEKIMP_LAN_STORY_WORLD_CHUNK_MAX_SIZE];size_t n;SudekiMpLanStoryWorldChunk chunk;
        assert(SudekiMpLanStoryWorldChunkEncode(f,i,bytes,sizeof(bytes),&n));
        assert(SudekiMpLanStoryWorldChunkDecode(bytes,n,&chunk));
    }
}
static void survivors(const SudekiMpLanStoryWorldFrame *a,const SudekiMpLanStoryWorldFrame *b) {
    for(unsigned i=0;i<a->count;++i) {int j=find(b,a->actors[i].identifier);
        if(j>=0)assert(a->actors[i].generation==b->actors[j].generation);
    }
}
static void q2_unbound_pose(void) {
    SudekiMpLanStoryWorldFrame f;
    setup();assert(capture(&f) && find(&f,1031)>=0);
    for(unsigned mode=0;mode<3;++mode) {
        setup();
        if(mode==0)number(entities[31].actor+0x150,0x1c,NAN);
        if(mode==1)number(entities[31].actor+0x150,0x58,0);
        if(mode==2)word(entities[31].rows[0],0,(192u<<16)|2u);
        assert(capture(&f) && find(&f,1031)<0 && target_log==1 && refusal_log==0);
        assert(omission(1031,"pose",mode==2?"channel_pose":"transform")==1);
        assert(strstr(last_target_diagnostic," pose_detail=0 ") && strstr(last_target_diagnostic," detail=31\r"));valid_wire(&f);
    }
}
static void q2_late_pose(void) {
    setup();SudekiMpLanStoryWorldFrame before,f,after;assert(capture(&before));
    /* This NPC is newly bound; an earlier sorted ally's read callback changes it after B. */
    word(entities[31].actor,0x34,2031);late_corruption=31;
    assert(capture(&f) && find(&f,2031)<0 && refusal_log==0 && omission(2031,"pose_late","channel_pose")==1);
    valid_wire(&f);survivors(&before,&f);
    assert(capture(&after));survivors(&f,&after);
}
static void q2_wire_bounds(void) {
    for(unsigned mode=0;mode<3;++mode) {
        setup();
        if(mode==0)word(entities[31].rows[0],0,2u<<16);
        if(mode==1)number(entities[31].rows[0],4,-1);
        if(mode==2)number(entities[31].blends,0xc,2);
        SudekiMpLanStoryWorldFrame f,after;
        assert(capture(&f) && find(&f,1031)<0 && refusal_log==0 && omission(1031,"wire_pose_bounds","-")==1);
        valid_wire(&f);assert(capture(&after));survivors(&f,&after);
    }
}
static void q2_death_omit(void) {
    setup();death_failure=entities[144].actor;number(entities[144].stats,0x2c,40);number(entities[145].stats,0x2c,70);
    SudekiMpLanStoryWorldFrame f;assert(capture(&f) && find(&f,1144)<0 && find(&f,1145)>=0);
    assert(f.actors[find(&f,1145)].hit_points==70 && omission(1144,"death_state","-")==1);valid_wire(&f);
}
static void at_tick(uint32_t tick,SudekiMpLanStoryWorldFrame *f) {party.host_tick=tick-20;assert(capture(f));valid_wire(f);}
static void q2_hold(void) {
    setup();SudekiMpLanStoryWorldFrame first,last,f,mid;assert(capture(&first));last=first;uint32_t tick=first.host_tick;
    number(entities[29].actor+0x150,0x1c,NAN);number(entities[144].actor+0x150,0x1c,NAN);number(entities[144].stats,0x2c,40);
    static const unsigned ages[]={1000,1990,2000};
    for(unsigned k=0;k<3;++k) {
        at_tick(tick+ages[k],&f);
        assert(find(&f,1029)>=0 && find(&f,1144)>=0);
        assert(!memcmp(&first.actors[find(&first,1029)],&f.actors[find(&f,1029)],sizeof(f.actors[0])));
        assert(!memcmp(&first.actors[find(&first,1144)],&f.actors[find(&f,1144)],sizeof(f.actors[0])));
        assert(SudekiMpLanStoryWorldFrameInterpolate(&last,&f,last.host_tick+1,&mid));last=f;
    }
    assert(target_log==2 && omission(1029,"pose_held","transform")==1 && omission(1144,"pose_held","transform")==1);
    at_tick(tick+2010,&f);assert(find(&f,1029)<0 && find(&f,1144)<0 && target_log==4);
    assert(omission(1029,"pose","transform")==1 && omission(1144,"pose","transform")==1);
    number(entities[29].actor+0x150,0x1c,0);number(entities[144].actor+0x150,0x1c,0);assert(capture(&f));
    assert(f.actors[find(&f,1029)].generation!=first.actors[find(&first,1029)].generation);
    assert(f.actors[find(&f,1144)].generation!=first.actors[find(&first,1144)].generation && f.actors[find(&f,1144)].hit_points==40);
    setup();assert(capture(&first));tick=first.host_tick;
    number(entities[144].actor+0x150,0x1c,NAN);number(entities[144].stats,0x2c,40);at_tick(tick+1000,&f);
    number(entities[144].actor+0x150,0x1c,0);at_tick(tick+1500,&f);
    assert(f.actors[find(&f,1144)].generation==first.actors[find(&first,1144)].generation && f.actors[find(&f,1144)].hit_points==40);
    setup();assert(capture(&first));tick=first.host_tick;death_failure=entities[144].actor;
    number(entities[144].stats,0x2c,40);word(entities[144].object,0x34,4);
    at_tick(tick+1000,&f);assert(!memcmp(&first.actors[find(&first,1144)],&f.actors[find(&f,1144)],sizeof(f.actors[0])));
    assert(omission(1144,"pose_held","death_state")==1 && strstr(last_target_diagnostic," pose_detail=0 "));
    at_tick(tick+2010,&f);assert(find(&f,1144)<0 && omission(1144,"death_state","-")==1);
    setup();assert(capture(&first));word(entities[31].resource,0,124);number(entities[31].actor+0x150,0x1c,NAN);
    assert(capture(&f) && find(&f,1031)<0 && omission(1031,"pose","transform")==1 && target_log==1);
}
static void q2_catalog(void) {
    diagnostics_and_failures();SudekiMpLanStoryWorldFrame f;
    setup();registry_entries[31]=NULL;
    assert(capture(&f) && omission(0,"registry_entry","-")==1 && strstr(last_target_diagnostic," detail=31\r"));
    char expected_pointer[40];snprintf(expected_pointer,sizeof(expected_pointer)," entity=%p ",(void *)NULL);assert(strstr(last_target_diagnostic,expected_pointer));
    for(unsigned mode=0;mode<2;++mode) {
        setup();pointer(entities[32].actor+0x210,mode?0x10:0,NULL);
        assert(capture(&f) && omission(1032,"position","-")==1 && strstr(last_target_diagnostic," kind=0f8f identifier=00000408"));
    }
    setup();pointer(base,0x2d5b44+0xc,NULL);
    assert(!capture(&f) && !strcmp(stage,"generic_resource_vtable") && refusal_log==1 && target_log==0);
    setup();roster.actors[2]=registry_entries[149]=(void *)1;
    assert(!capture(&f) && !strcmp(stage,"registry_entry") && stage_detail==149 && stage_entity==(void *)1 && refusal_log==1 && target_log==0);
}
static void q2_recheck(void) {
    setup();SudekiMpLanStoryWorldFrame first,f;assert(capture(&first));recheck_change=140;
    assert(capture(&f) && find(&f,1140)<0 && host_count==f.count && omission(1140,"recheck","-")==1);valid_wire(&f);
    assert(capture(&f) && f.actors[find(&f,1140)].generation!=first.actors[find(&first,1140)].generation);
    setup();recheck_change=149;assert(!capture(&f) && refusal_log==1 && target_log==0);
}
static void partial_apply(const SudekiMpLanStoryWorldFrame *f,unsigned count) {
    set_position=place_position;world_set_forward=place_forward;position_matrix=matrix;placements=0;
    BOOL ok=SudekiMpLanStoryWorldPrepare(&roster,f,exact,NULL,FALSE);
    if(!ok)fprintf(stderr,"partial prepare stage=%s id=%x detail=%u error=%lu\n",stage,stage_identifier,stage_detail,GetLastError());
    assert(ok && prepared.count==count);
    assert(SudekiMpLanStoryWorldApplyPrepared(&roster,f,exact,NULL) && placements==count && client_count==count);
}
static void q2_client_pose(void) {
    setup();SudekiMpLanStoryWorldFrame f;assert(capture(&f));host_seen=FALSE;apply(&f);uint32_t before_serial=ownership_serial;
    number(entities[31].actor+0x150,0x18,NAN);partial_apply(&f,f.count-1);
    assert(omission(1031,"pose","transform")==1 && strstr(last_target_diagnostic,"side=client") && unsupported_logs==0 && refusal_log==0);
    assert(ownership_serial==before_serial+1);number(entities[31].actor+0x150,0x18,169);apply(&f);assert(ownership_serial==before_serial+2);
}
static void q2_load_reprobe(void) {
    setup();word(entities[33].resource,0x1c,0x80000000u);memset(load_package,0,sizeof(load_package));
    pointer(entities[33].bank,0x10,load_package);pointer(load_package,4,load_package+0x18);word(load_package,0x1c,1);pointer(load_package,0x14,load_package+0x20);
    SudekiMpLanStoryWorldFrame f;assert(capture(&f));assert(find(&f,1031)<find(&f,1033));
    host_seen=FALSE;load_fixture=TRUE;partial_apply(&f,f.count-2);
    assert(inspect_calls==1 && acquire_calls==1 && prepared_log==1 && omission(1031,"pose","transform")==1 && target_log==1 && refusal_log==0);
    assert(client_count==(unsigned)f.count-2u);for(unsigned i=0;i<client_count;++i)assert(client_bound[i].previous.identifier!=1033 && client_bound[i].previous.identifier!=1031);
}
static void q2_party(void) {
    SudekiMpLanStoryWorldFrame f;
    for(unsigned bound=0;bound<2;++bound) {
        setup();if(bound)assert(capture(&f));number(entities[149].actor+0x300,0x1c,NAN);
        assert(!capture(&f) && GetLastError()==ERROR_NOT_SUPPORTED && !strcmp(stage,"transform") && stage_identifier==SudekiMpLanStoryWorldCharacterIdentifier(2) && target_log==0 && refusal_log==1);
    }
    setup();assert(capture(&f));host_seen=FALSE;number(entities[149].actor+0x300,0x1c,NAN);
    assert(!SudekiMpLanStoryWorldPrepare(&roster,&f,exact,NULL,FALSE) && !strcmp(stage,"transform") && stage_identifier==SudekiMpLanStoryWorldCharacterIdentifier(2) && target_log==0 && refusal_log==1);
}
static void q2_budget(void) {
    setup();for(unsigned i=21;i<86;++i)pointer(entities[i].renderer,8,(void *)1);
    SudekiMpLanStoryWorldFrame f;assert(capture(&f) && target_log==64 && budget_log==1 && refusal_log==0);
    for(unsigned i=0;i<target_log;++i)assert(strstr(omission_text[i],"event=entity_omitted side=") && !strstr(omission_text[i],"refused"));
    assert(capture(&f) && target_log==64 && budget_log==1);
}
void test_runtime_world_apply_failure(void (*callback)(void));
static SudekiMpLanStoryWorldFrame write_frame;
static void actual_write_failure(void) {
    word(entities[31].actor,0x30,0x0f9a);
    assert(!SudekiMpLanStoryWorldApplyPrepared(&roster,&write_frame,exact,NULL));
    assert(GetLastError()==ERROR_RETRY && placements==0 && refusal_log==1 && target_log==0);
    assert(strstr(refusal_text,"stage=write_owner") && strstr(refusal_text,"vtable_rva=002d65a0") && strstr(refusal_text," kind=0f9a"));
    assert(stage_entity==entities[31].actor && stage_kind==0x0f9a);
    char expected_pointer[40];snprintf(expected_pointer,sizeof(expected_pointer)," entity=%p ",(void *)entities[31].actor);assert(strstr(refusal_text,expected_pointer));
}
static void q2_write_failure(void) {
    setup();assert(capture(&write_frame));host_seen=FALSE;
    set_position=place_position;world_set_forward=place_forward;position_matrix=matrix;placements=0;
    assert(SudekiMpLanStoryWorldPrepare(&roster,&write_frame,exact,NULL,FALSE));
    test_runtime_world_apply_failure(actual_write_failure);
}
static void q2_case(const char *name) {
#define Q2_CASE(label,fn) if(!strcmp(name,"q2-all") || !strcmp(name,"q2-" label)) {fn();puts("PASS Q2 " label);}
    Q2_CASE("unbound",q2_unbound_pose)
    Q2_CASE("late",q2_late_pose)
    Q2_CASE("wire",q2_wire_bounds)
    Q2_CASE("death",q2_death_omit)
    Q2_CASE("hold",q2_hold)
    Q2_CASE("catalog",q2_catalog)
    Q2_CASE("recheck",q2_recheck)
    Q2_CASE("client",q2_client_pose)
    Q2_CASE("load",q2_load_reprobe)
    Q2_CASE("party",q2_party)
    Q2_CASE("budget",q2_budget)
    Q2_CASE("write",q2_write_failure)
#undef Q2_CASE
}
