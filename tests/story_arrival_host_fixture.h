/* Included by StoryHostFilterTest: real host adapter, inert native call spies. */
static BOOL lead_retained,lead_scope_ok=TRUE,guard_registered;
BOOL SudekiMpLanStoryControlRetains(void) {return lead_retained;}
BOOL SudekiMpControlSeparationStoryInputGuard(SudekiMpStoryHostInputGuard fn) {guard_registered=fn!=NULL;return TRUE;}
BOOL SudekiMpLanPartyControlStoryLeadScope(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,BOOL enabled) {(void)w;(void)r;(void)enabled;return lead_scope_ok;}
static uint8_t lead_group[0xe0],lead_world[0x400],camera_manager[0x60],lead_cam[0x110],lead_render[0xe0];
static uint8_t lead_target[0x34],lead_state[4],lead_scene[0x44],lead_renderer[0x80];
static unsigned camera_requests;
static BOOL camera_can=TRUE;
static BOOL __attribute__((thiscall)) can_camera(void *manager,const char *name,const char *state) {
    assert(manager==camera_manager && !strcmp(name,"default") && !strcmp(state,"Exploration"));return camera_can;
}
static void __attribute__((thiscall)) set_camera(void *manager,const char *name,const char *state,float a,BOOL flag,float b) {
    assert(manager==camera_manager && !strcmp(name,"default") && !strcmp(state,"Exploration") && !a && !flag && !b);
    ++camera_requests;*(void **)lead_state=base+0x2d9854u;
}
static void branch_to(unsigned rva,uintptr_t target) {
    base[rva]=0xe9;int32_t rel=(int32_t)(target-(uintptr_t)(base+rva+5));memcpy(base+rva+1,&rel,4);
}
static void arrival_camera_fixture(void) {
    memset(camera_manager,0,sizeof(camera_manager));memset(lead_cam,0,sizeof(lead_cam));
    *(void **)(base+0x409d7c)=camera_manager;*(void **)camera_manager=base+0x2c7b80;
    *(void **)(camera_manager+0x20)=lead_cam;*(void **)lead_cam=base+0x2cce5c;strcpy((char *)lead_cam+0x4c,"default");
    *(void **)(lead_cam+0x34)=lead_render;*(void **)lead_render=base+0x2dd638;
    *(void **)(lead_cam+0xb8)=lead_target;*(void **)lead_target=base+0x2d42cc;*(unsigned *)(lead_target+4)=2;
    *(void **)(lead_target+0x20)=actors[2];*(void **)(lead_cam+0x3c)=lead_state;*(void **)lead_state=base+0x2d6dfc;
    *(void **)(base+0x3d4b10)=lead_state;*(void **)(base+0x408d58)=lead_scene;
    *(void **)(lead_scene+0x40)=lead_renderer;*(void **)(lead_renderer+0x7c)=lead_render;
    /* Execute the guarded entry prefixes, then unwind their stack saves
     * before calling an inert thiscall spy. No retail game code executes. */
    memcpy(base+0x37530,"\xd9\x44\x24\x14\x8b\x44\x24\x10\x56\x51\xd9\x1c\x24\x83\xc4\x08",16);
    branch_to(0x37540,(uintptr_t)set_camera);
    memcpy(base+0x37c30,"\x53\x55\x8b\x6c\x24\x0c\x56\x57\x8b\xd9\x85\xed\x5f\x5e\x5d\x5b",16);
    branch_to(0x37c40,(uintptr_t)can_camera);camera_requests=0;camera_can=TRUE;
}
static void test_arrival_host(void) {
    exterior_fixture();memset(&lead_policy,0,sizeof(lead_policy));
    exterior_roster.world=lead_world;exterior_roster.group=lead_group;
    *(void **)(base+0x408d10)=lead_world;*(void **)(base+0x408d94)=lead_group;*(void **)(base+0x408da4)=controller;
    *(unsigned *)(lead_group+0xcc)=1;*(void **)(lead_group+0x90)=actors[2];
    exterior_roster.available_mask=4;exterior_roster.leader_character=2;
    exterior_roster.actors[0]=exterior_roster.actors[1]=exterior_roster.actors[3]=NULL;
    exterior_roster.ai[0]=exterior_roster.ai[1]=exterior_roster.ai[3]=NULL;
    exterior_scene=(SudekiMpLanStoryScene){.epoch=7,.revision=8,.available_mask=4,.leader_seat=2,
        .phase=SUDEKIMP_LAN_STORY_READY,.inside_mask=4,.world="newbrightwater",.temporary="lica_tals_room"};
    SudekiMpControlUpdateDispatchWitness w={.service_only=TRUE,.service_post_original_exact=TRUE,.dispatch_serial=42};
    *(void **)(controller+0x248)=actors[2];*(void **)actors[2]=base+0x2d5010;
    unsigned calls=none_calls;
    assert(lead_input_guard(controller) && none_calls==calls); /* off */
    assert(SudekiMpLanStoryHostControlLeadPolicy(controller,&w,&exterior_scene,TRUE,TRUE));
    assert(lead_input_guard(controller) && none_calls==calls+1 && input_owner.owned);
    assert(!*(int *)(controller+0x80) && !*(int *)(controller+0x84));
    assert(SudekiMpLanStoryHostControlRemoteLead(&exterior_roster));
    assert(SudekiMpLanStoryHostControlBound(controller,&w,&exterior_scene));
    assert(SudekiMpLanStoryHostControlReady(controller,&w,&exterior_scene));
    assert(!SudekiMpLanStoryHostControlTemporaryLead(&exterior_roster));
    assert(!SudekiMpLanStoryHostControlUninstall() && base && input_owner.owned);
    arrival_camera_fixture();
    assert(SudekiMpLanStoryHostControlService(controller,&w,&exterior_scene,FALSE));
    assert(!camera_requests && *(void **)lead_state==base+0x2d6dfcu);
    assert(SudekiMpLanStoryHostControlService(controller,&w,&exterior_scene,FALSE));
    assert(!camera_requests && *(void **)lead_state==base+0x2d6dfcu);
    arrival_camera_fixture();
    /* Exact native None queues +84 only. The guard must settle its owned
     * request before the original input consumer and notify camera input. */
    memcpy(base+0x38c50,"\x53\x55\x8b\x6c\x24\x0c\x56\x57\x88\x45\x5c\x5f\x5e\x5d\x5b\xc2\x04\x00",18);
    memcpy(base+0x285c9,"\x8b\x83\x84\0\0\0\x3b\x83\x80\0\0\0\x74\x26\x89\x83\x80\0\0\0",20);
    memcpy(base+0x285f8,"\xe8\x53\x06\x01\0",5);memcpy(base+0x38c8b,"\xc2\x04\0",3);
    filters(1,1);native_none_async=TRUE;camera_manager[0x5c]=1;calls=none_calls;
    assert(lead_input_guard(controller) && none_calls==calls+1 && input_owner.owned &&
        !*(int *)(controller+0x80) && !*(int *)(controller+0x84) && !camera_manager[0x5c]);
    assert(lead_input_guard(controller) && none_calls==calls+1);
    for(unsigned fault=0;fault<5;++fault) {
        filters(1,0);camera_manager[0x5c]=1;
        unsigned rva=fault==0?0x285c9: fault==1?0x285f8:fault==2?0x38c50:0x38c8b;
        if(fault<4) base[rva]^=1;else *(void **)(camera_manager+0x24)=lead_target;
        assert(!lead_input_guard(controller) && *(int *)(controller+0x80)==1 && camera_manager[0x5c]==1);
        if(fault<4) base[rva]^=1;else *(void **)(camera_manager+0x24)=NULL;
    }
    filters(0,0);native_none_async=FALSE;
    filters(1,1);filter_exact=FALSE;calls=none_calls;
    assert(!lead_input_guard(controller) && none_calls==calls);filter_exact=TRUE;
    *(void **)(lead_group+0x90)=actors[3];assert(!lead_input_guard(controller));*(void **)(lead_group+0x90)=actors[2];
    /* Native pending/UI filters are never repaired or borrowed. */
    filters(2,2);assert(lead_input_guard(controller) && none_calls==calls);
    /* Disconnect: retain block until the caller proves the native release,
     * then release only our exact None, including async All completion. */
    filters(0,0);lead_retained=TRUE;
    assert(!SudekiMpLanStoryHostControlLeadPolicy(controller,&w,&exterior_scene,FALSE,FALSE));
    assert(SudekiMpLanStoryHostControlLeadPolicy(controller,&w,&exterior_scene,TRUE,TRUE));
    assert(lead_input_guard(controller) && input_owner.owned);
    lead_retained=FALSE;
    assert(SudekiMpLanStoryHostControlLeadPolicy(controller,&w,&exterior_scene,TRUE,FALSE));
    assert(SudekiMpLanStoryHostControlTemporaryLead(&exterior_roster));
    assert(!input_filter(&w,&exterior_roster,FALSE) && input_owner.releasing);
    filters(1,1);assert(input_filter(&w,&exterior_roster,FALSE) && !input_owner.owned);
    assert(lead_input_guard(controller) && *(int *)(controller+0x80)==1);
    /* Reverse native switch is never rotated back to Tal. */
    exterior_roster.available_mask=12;exterior_roster.leader_character=3;
    exterior_roster.actors[3]=actors[3];exterior_roster.ai[3]=ais[3];
    *(void **)(controller+0x248)=actors[3];lead_policy.remote=TRUE;
    assert(lead_input_guard(controller) && *(int *)(controller+0x80)==1);
    assert(!SudekiMpLanStoryHostControlRemoteLead(&exterior_roster));
    assert(SudekiMpLanStoryHostControlBound(controller,&w,&exterior_scene));
    memset(&lead_policy,0,sizeof(lead_policy));
    puts("PASS named ArrivalHostInput Camera Reverse Disconnect (real adapter, synthetic native spies)");
}

/* Unexpected non-arrival paths must not be reached by the host service case. */
BOOL SudekiMpLanStoryTaskTraceGetStatus(SudekiMpLanStoryTaskTraceStatus *o) {(void)o;assert(0);return FALSE;}
BOOL SudekiMpLanStoryTaskTraceGetRecruitmentStatus(SudekiMpLanStoryRecruitmentStatus *o) {(void)o;assert(0);return FALSE;}
BOOL SudekiMpCleanroomEngineCombatMode(BOOL *o) {(void)o;assert(0);return FALSE;}
BOOL SudekiMpCleanroomEngineRangedCombatPrimePending(void) {assert(0);return FALSE;}
BOOL SudekiMpCleanroomEngineSpiritPresentationState(int *o) {(void)o;assert(0);return FALSE;}
BOOL SudekiMpWeaponActivationPending(void *a,BOOL *o) {(void)a;(void)o;assert(0);return FALSE;}
BOOL SudekiMpObserveCharacterSkill(void *a,SudekiMpCharacterSkillState *o) {(void)a;(void)o;assert(0);return FALSE;}
