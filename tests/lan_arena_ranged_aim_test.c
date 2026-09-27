#include "../src/hooks/lan_arena_ranged_aim.c"
#include <stdio.h>
#include <float.h>
static unsigned failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); ++failures; } } while(0)
static void *expected_actor;
static BOOL admitted;
static BOOL target_admitted=TRUE;
static BOOL target_witness(void *actor,float target[3]) {
    if(!target_admitted || actor!=expected_actor) return FALSE;
    memcpy(target,(float[]){0,60,80},12);return TRUE;
}
static BOOL witness(void *actor,BOOL projectile,float direction[3]) {
    if(actor!=expected_actor || !projectile || !admitted) return FALSE;
    direction[0]=0; direction[1]=0.6f; direction[2]=0.8f; return TRUE;
}
static BOOL fp_witness(void *actor,BOOL projectile,float direction[3]) {
    if(actor!=expected_actor || projectile || !admitted) return FALSE;
    memcpy(direction,(float[]){0,0,1},12);return TRUE;
}
static BOOL fp_idle_witness(void *actor) { return actor==expected_actor && admitted; }
static unsigned fp_samples;
static void __stdcall fp_sample_stub(void *bank,void *states,void *output) {
    CHECK(fp_idle_layout(bank));
    CHECK(*(uint16_t *)states==5 && *(float *)((uint8_t *)states+12)==0);
    ++fp_samples;
    for(unsigned i=0;i<FP_BONES;++i) memcpy((float *)output+i*12+8,(float[]){0,0,0,1},16);
}
static void test_fp_idle_loop(void) {
    float pose[FP_BONES*12]={0},start[FP_BONES*12]={0},saved[FP_BONES*12];
    for(unsigned i=0;i<FP_BONES;++i) {
        pose[i*12+3]=1;pose[i*12+4]=pose[i*12+5]=pose[i*12+6]=1;
        pose[i*12+11]=start[i*12+11]=1;
    }
    pose[6*12+8]=sinf(.0062f);pose[6*12+11]=cosf(.0062f);
    memcpy(saved,pose,sizeof(pose));
    CHECK(close_fp_idle_loop(pose,start,FP_BONES,0) && !memcmp(pose,saved,sizeof(pose)));
    CHECK(close_fp_idle_loop(pose,start,FP_BONES,36) && !memcmp(pose,saved,sizeof(pose)));
    float last=.0062f;
    for(unsigned frame=1;frame<=80;++frame) {
        memcpy(pose,saved,sizeof(pose));
        CHECK(close_fp_idle_loop(pose,start,FP_BONES,36+frame*.05f));
        CHECK(pose[6*12+8]>=0 && pose[6*12+8]<=last);last=pose[6*12+8];
        for(unsigned i=0;i<FP_BONES;++i) {
            CHECK(!memcmp(pose+i*12,saved+i*12,8*sizeof(float)));
            if(i!=6) CHECK(!memcmp(pose+i*12,saved+i*12,48));
        }
    }
    CHECK(pose[6*12+8]==0 && pose[6*12+11]==1);
    memcpy(pose,saved,sizeof(pose));start[6*12+11]=-1; /* antipodal same orientation */
    CHECK(close_fp_idle_loop(pose,start,FP_BONES,40) && pose[6*12+11]==1);
    start[6*12+11]=1;start[(FP_BONES-1)*12+8]=NAN;memcpy(pose,saved,sizeof(pose));
    CHECK(!close_fp_idle_loop(pose,start,FP_BONES,39) && !memcmp(pose,saved,sizeof(pose)));
    CHECK(!close_fp_idle_loop(pose,start,FP_BONES+1,39));
    CHECK(!close_fp_idle_loop(pose,start,FP_BONES,NAN));
    CHECK(!close_fp_idle_loop(pose,start,FP_BONES,-1));
    CHECK(!close_fp_idle_loop(pose,start,FP_BONES,41));
    CHECK(!close_fp_idle_loop(NULL,start,FP_BONES,39));

    uint8_t *image=VirtualAlloc(NULL,0x410000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    CHECK(image!=NULL);if(!image) return;
    uint8_t actor[0x138]={0},model[0x168]={0},position[0xb8]={0},arb[0x54]={0};
    uint8_t controller[0x24c]={0},wrapper[0x14]={0},renderer[0xa8]={0},bank[0x28]={0};
    uint32_t header[4]={11,FP_BONES,1,1},palette[FP_BONES],track[FP_BONES]={0};
    uint8_t entries[11*28]={0},clip[24]={0},hier[FP_BONES*8]={0};void *tracks[FP_BONES];
    void *groups[2]={hier,palette};uint8_t channels[5*36]={0},blends[4*20]={0},state[24]={0};
    *(void **)actor=image+ELCO_VT;*(void **)(actor+0x134)=model;
    *(void **)(actor+0x44)=position;*(void **)(actor+0x90)=arb;
    *(void **)model=image+MODEL_VT;*(void **)(model+0x10)=actor;*(void **)(model+0x160)=wrapper;
    *(void **)(position+0x10)=actor;*(void **)(position+0xb4)=wrapper;
    *(void **)(arb+0x10)=actor;*(uint32_t *)(arb+0x50)=0x400000;
    *(void **)(image+0x408da4)=controller;*(void **)(controller+0x248)=actor;
    *(void **)(wrapper+0x10)=renderer;*(void **)renderer=image+RENDERER_VT;
    *(void **)(renderer+8)=bank;*(void **)(bank+0x1c)=header;
    *(void **)(bank+0x20)=entries;*(void **)(bank+0x24)=groups;
    *(void **)(entries+5*28)=clip;*(void **)(entries+5*28+4)=tracks;
    *(uint32_t *)clip=1050148086u;*(float *)(clip+4)=40;
    for(unsigned i=0;i<FP_BONES;++i) {
        palette[i]=i;*(int16_t *)(hier+i*8+4)=(int)i-1;tracks[i]=track+i;
    }
    *(void **)(renderer+0x98)=channels;*(void **)(renderer+0x9c)=blends;
    *(uint32_t *)(renderer+0xa0)=5;*(uint32_t *)(renderer+0xa4)=4;
    const uint16_t links[8]={0,1,2,3,0x8000,0x8001,0x8002,4};
    for(unsigned i=0;i<4;++i) {
        memcpy(blends+i*20,links+i*2,4);
        *(uint32_t *)(blends+i*20+4)=i==3 ? 0x80000002u:0xffffffffu;
    }
    *(void **)channels=state;*(uint16_t *)state=5;
    *(float *)(state+4)=24;*(float *)(state+8)=399;*(float *)(state+12)=39;
    aim_image=image;aim_thread=GetCurrentThreadId();aim_witness=fp_witness;
    idle_witness=fp_idle_witness;
    expected_actor=actor;admitted=TRUE;aim_actors[1]=actor;first_person_renderers[1]=renderer;
    CHECK(first_person_renderer(actor)==renderer && fp_idle_layout(bank));
    float phase=0;CHECK(fp_idle_storage(renderer,bank,&phase) && phase==39);
    *(uint16_t *)(state+2)=128;CHECK(fp_idle_storage(renderer,bank,&phase));
    *(uint16_t *)(state+2)=1;CHECK(!fp_idle_storage(renderer,bank,&phase));
    *(uint16_t *)(state+2)=0;
    for(unsigned selector=0;selector<11;++selector) {
        *(uint16_t *)state=selector;CHECK(fp_idle_storage(renderer,bank,&phase)==(selector==5));
    }
    *(uint16_t *)state=5;
    for(unsigned i=0;i<4;++i) {
        *(float *)(blends+i*20+12)=.001f;CHECK(!fp_idle_storage(renderer,bank,&phase));
        *(float *)(blends+i*20+12)=0;
    }
    *(float *)(state+12)=NAN;CHECK(!fp_idle_storage(renderer,bank,&phase));
    *(float *)(state+12)=39;
    palette[1]=0;CHECK(!fp_idle_layout(bank));palette[1]=1;
    *(int16_t *)(hier+6)=1;CHECK(!fp_idle_layout(bank));*(int16_t *)(hier+6)=0;
    header[1]=109;CHECK(!first_person_renderer(actor) && !fp_idle_layout(bank));header[1]=FP_BONES;
    /* Execute the real hook selection path with a synthetic stdcall sampler.
     * No native clock/state/bank/pose writes, including rejected ownership. */
    image[POSE_SAMPLE]=0xe9;
    int32_t displacement=(int32_t)((uint8_t *)fp_sample_stub-(image+POSE_SAMPLE+5));
    memcpy(image+POSE_SAMPLE+1,&displacement,4);
    FlushInstructionCache(GetCurrentProcess(),image+POSE_SAMPLE,5);
    void *args[5]={bank,NULL,pose,NULL,NULL};memcpy(pose,saved,sizeof(pose));
    float *out=select_pose(args,renderer);
    CHECK(out==fp_idle_result && fp_samples==1 && smoothed_fp_idle_poses==1);
    CHECK(out[6*12+8]<pose[6*12+8]*.11f && !memcmp(pose,saved,sizeof(pose)));
    CHECK(*(float *)(state+8)==399 && *(float *)(state+12)==39);
    /* Local idle continues when the observer aim path is unavailable. It
     * still needs its own fresh local witness on every pose publication. */
    aim_witness=NULL;
    CHECK(select_pose(args,renderer)==fp_idle_result && fp_samples==2);
    idle_witness=NULL;CHECK(select_pose(args,renderer)==pose);
    idle_witness=fp_idle_witness;
    *(void **)(controller+0x248)=NULL;CHECK(select_pose(args,renderer)==pose);
    *(void **)(controller+0x248)=actor;*(void **)(position+0xb4)=NULL;
    CHECK(select_pose(args,renderer)==pose);*(void **)(position+0xb4)=wrapper;
    *(void **)(model+0x10)=NULL;CHECK(select_pose(args,renderer)==pose);*(void **)(model+0x10)=actor;
    admitted=FALSE;CHECK(select_pose(args,renderer)==pose);admitted=TRUE;
    aim_thread=0;CHECK(select_pose(args,renderer)==pose);aim_thread=GetCurrentThreadId();
    pose_depth=1;CHECK(select_pose(args,renderer)==pose);pose_depth=0;
    *(uint16_t *)state=3;CHECK(select_pose(args,renderer)==pose);*(uint16_t *)state=5;
    *(float *)(state+12)=36;CHECK(select_pose(args,renderer)==pose);
    CHECK(fp_samples==2 && smoothed_fp_idle_poses==2 && !memcmp(pose,saved,sizeof(pose)));
    first_person_renderers[1]=NULL;aim_actors[1]=NULL;expected_actor=NULL;
    aim_image=NULL;aim_witness=NULL;idle_witness=NULL;aim_thread=0;smoothed_fp_idle_poses=0;
    VirtualFree(image,0,MEM_RELEASE);
}
static DWORD __stdcall sample_stub(void *bank,void *states,void *output) {
    CHECK(bank==(void *)0x12340000 && states==(void *)0x23450000);
    *(unsigned *)output=1234;return 0xabcdef01u;
}
static void test_held_base(void) {
    HoldState s={0};
    CHECK(advance_hold(&s,1000,TRUE,TRUE,FALSE)==0);
    CHECK(fabsf(advance_hold(&s,1050,TRUE,TRUE,FALSE)-.5f)<.00001f);
    CHECK(advance_hold(&s,1100,TRUE,TRUE,FALSE)==1);
    /* Gaps between actual shots must not lower the arm while trigger is held. */
    for(unsigned i=0;i<20;++i) CHECK(advance_hold(&s,1110+i*20,TRUE,TRUE,i%3==0)==1);
    CHECK(advance_hold(&s,1520,TRUE,FALSE,TRUE)==1); /* final native recoil */
    CHECK(fabsf(advance_hold(&s,1620,TRUE,FALSE,FALSE)-.5f)<.00001f);
    CHECK(advance_hold(&s,1720,TRUE,FALSE,FALSE)==0);
    CHECK(advance_hold(&s,1820,TRUE,TRUE,FALSE)==1);
    CHECK(advance_hold(&s,1821,FALSE,TRUE,TRUE)==0 && !s.valid); /* lost lease */
    CHECK(advance_hold(&s,0xfffffff0u,TRUE,TRUE,FALSE)==0);
    CHECK(fabsf(advance_hold(&s,0x22u,TRUE,TRUE,FALSE)-.5f)<.00001f); /* tick wrap */
    float base[36]={0},straight[36]={0},saved[36];uint8_t mask[3]={0,1,1};
    for(unsigned i=0;i<3;++i) { base[i*12+11]=straight[i*12+11]=1; }
    base[20]=sinf(.5f);base[23]=cosf(.5f);base[12]=.4f;
    memcpy(saved,base,sizeof(base));
    CHECK(blend_hold_base(base,straight,mask,3,0) && !memcmp(base,saved,sizeof(base)));
    CHECK(blend_hold_base(base,straight,mask,3,.5f));
    CHECK(fabsf(base[20]-sinf(.25f))<.00001f);
    CHECK(!memcmp(base,saved,48) && !memcmp(base+12,saved+12,8*sizeof(float)));
    CHECK(blend_hold_base(base,straight,mask,3,1));
    CHECK(fabsf(base[20])<.00001f && fabsf(base[23]-1)<.00001f);
    straight[32]=NAN;memcpy(saved,base,sizeof(base));
    CHECK(!blend_hold_base(base,straight,mask,3,1) && !memcmp(base,saved,sizeof(base)));
    /* The hook invokes the original stdcall sampler first and preserves its
     * output/return/stack when no eligible actor owns this renderer. */
    unsigned sampled=0;sample_original=sample_stub;
    CHECK(sample_bridge((void *)0x12340000,(void *)0x23450000,&sampled)==0xabcdef01u);
    CHECK(sampled==1234);sample_original=NULL;
    uint8_t renderer[0xa8]={0},channels[5*36]={0},blends[4*20]={0},states[5][48]={{0}};
    void *bank=(void *)0x12340000;BOOL recoil=FALSE;
    *(void **)(renderer+8)=bank;*(void **)(renderer+0x98)=channels;
    *(void **)(renderer+0x9c)=blends;*(uint32_t *)(renderer+0xa0)=5;
    *(uint32_t *)(renderer+0xa4)=4;
    const uint16_t links[8]={0,1,2,3,0x8000,0x8001,0x8002,4};
    for(unsigned i=0;i<4;++i) {
        memcpy(blends+i*20,links+i*2,4);
        *(uint32_t *)(blends+i*20+4)=i==3 ? 0xc0000002u:0xffffffffu;
    }
    for(unsigned i=0;i<5;++i) {
        *(void **)(channels+i*36)=states[i];
        for(unsigned ch=0;ch<2;++ch) *(uint16_t *)(states[i]+ch*24)=i<4 ? 22:0;
    }
    CHECK(hold_storage(renderer,bank,states[0],0,&recoil) && !recoil);
    CHECK(!hold_storage(renderer,bank,states[4],4,&recoil)); /* never edit recoil */
    CHECK(!hold_storage(renderer,bank,states[1],0,&recoil));
    *(float *)(blends+72)=1;
    CHECK(!hold_storage(renderer,bank,states[0],0,&recoil)); /* unknown fire branch */
    *(uint16_t *)states[4]=*(uint16_t *)(states[4]+24)=53;
    CHECK(hold_storage(renderer,bank,states[0],0,&recoil) && recoil);
    *(uint16_t *)(states[0]+24)=73;
    CHECK(!hold_storage(renderer,bank,states[0],0,&recoil)); /* skill */
    *(uint16_t *)(states[0]+24)=22;blends[60]^=1;
    CHECK(!hold_storage(renderer,bank,states[0],0,&recoil)); /* foreign blend tree */
}
static void test_reference_keeps_native_animation(void) {
    float straight[36]={0},reference[36]={0},native[36]={0},saved[36],out[36];
    uint8_t mask[3]={0,1,1};
    for(unsigned i=0;i<3;++i) {
        straight[i*12+3]=1;straight[i*12+4]=straight[i*12+5]=straight[i*12+6]=1;
        straight[i*12+11]=1;
    }
    memcpy(reference,straight,sizeof(reference));
    reference[20]=sinf(.3f);reference[23]=cosf(.3f); /* steady aim correction */
    memcpy(native,straight,sizeof(native));
    native[0]=7;native[12]=.25f;native[16]=.9f; /* native root, upper translation/scale */
    for(unsigned frame=0;frame<=40;++frame) {
        /* Idle -> kick -> recovery. This is already blended native motion,
         * independent of which renderer layer supplied it. */
        float phase=(float)frame/40.f;
        float half_angle=.2f+.15f*sinf(phase*3.14159265358979323846f);
        native[20]=sinf(half_angle);native[23]=cosf(half_angle);
        native[25]=phase*.02f; /* preserve animated wrist translation too */
        memcpy(saved,native,sizeof(saved));memcpy(out,native,sizeof(out));
        CHECK(apply_reference(out,reference,straight,mask,3,1));
        CHECK(!memcmp(out,saved,12*sizeof(float))); /* complete lower body */
        for(unsigned i=1;i<3;++i)
            CHECK(!memcmp(out+i*12,saved+i*12,8*sizeof(float))); /* XYZ/scale/w */
        CHECK(fabsf(out[20]-sinf(.3f+half_angle))<.00001f);
        CHECK(fabsf(out[23]-cosf(.3f+half_angle))<.00001f);
        CHECK(fabsf(out[20]-reference[20])>.1f); /* not a held fire reference */
        CHECK(!memcmp(native,saved,sizeof(native))); /* never modify native storage */
    }
    /* Neutral aim must preserve the complete current idle/recoil pose. */
    memcpy(out,native,sizeof(out));
    CHECK(apply_reference(out,straight,straight,mask,3,1));
    for(unsigned i=0;i<36;++i) CHECK(fabsf(out[i]-native[i])<.000001f);
    memcpy(out,native,sizeof(out));reference[32]=NAN;
    CHECK(!apply_reference(out,reference,straight,mask,3,1));
    CHECK(!memcmp(out,native,sizeof(out))); /* invalid later bone: no partial write */
}
static void test_idle_aim_release(void) {
    float idle[36]={0},straight[36]={0},aimed[36],out[36];
    uint8_t mask[3]={0,1,1};
    for(unsigned i=0;i<3;++i) {
        idle[i*12+3]=straight[i*12+3]=1;
        idle[i*12+4]=idle[i*12+5]=idle[i*12+6]=1;
        straight[i*12+4]=straight[i*12+5]=straight[i*12+6]=1;
        idle[i*12+11]=straight[i*12+11]=1;
    }
    idle[12]=.17f;idle[25]=.03f; /* preserve native animated translations */
    idle[20]=sinf(.6f)*.99999f;idle[23]=cosf(.6f)*.99999f;
    idle[32]=sinf(-.2f);idle[35]=cosf(-.2f);
    for(int direction=-4;direction<=4;++direction) {
        memcpy(aimed,straight,sizeof(aimed));
        float half_angle=direction*.12f;
        aimed[20]=sinf(half_angle);aimed[23]=cosf(half_angle);
        aimed[32]=sinf(-half_angle);aimed[35]=cosf(-half_angle);
        /* Off-centre aim is NOT identity. It must nevertheless be absent
         * after release, rather than stacked on the gun-up idle animation. */
        memcpy(out,idle,sizeof(out));
        CHECK(apply_reference(out,aimed,straight,mask,3,0));
        CHECK(!memcmp(out,idle,sizeof(out))); /* includes compressed key bits */
        for(unsigned frame=0;frame<=20;++frame) {
            float stance=(float)frame/20;
            memcpy(out,idle,sizeof(out));
            CHECK(apply_reference(out,aimed,straight,mask,3,stance));
            if(frame) {
                CHECK(fabsf(out[20]-sinf(.6f+half_angle*stance))<.00001f);
                CHECK(fabsf(out[32]-sinf(-.2f-half_angle*stance))<.00001f);
            }
            CHECK(!memcmp(out,idle,12*sizeof(float)));
            for(unsigned i=1;i<3;++i)
                CHECK(!memcmp(out+i*12,idle+i*12,8*sizeof(float)));
        }
        memcpy(out,idle,sizeof(out));
        CHECK(!apply_reference(out,aimed,straight,mask,3,NAN));
        CHECK(!apply_reference(out,aimed,straight,mask,3,1.01f));
        CHECK(!apply_reference(out,aimed,straight,mask,3,-.01f));
        CHECK(!memcmp(out,idle,sizeof(out)));
    }
    HoldState state={1000,.5f,TRUE};
    CHECK(current_stance(&state,1000)==.5f);
    CHECK(current_stance(&state,1100)==.5f);
    CHECK(current_stance(&state,1101)==0); /* no stale fire correction */
    CHECK(current_stance(&state,999)==0);
    state.at=0xfffffff0u;CHECK(current_stance(&state,0x22u)==.5f);
    state.valid=FALSE;CHECK(current_stance(&state,0x22u)==0);
    state.valid=TRUE;state.weight=NAN;CHECK(current_stance(&state,0x22u)==0);
    state.weight=1.01f;CHECK(current_stance(&state,0x22u)==0);
    CHECK(current_stance(NULL,0)==0);
}
static void test_arm_calibration_preserves_authored_torso(void) {
    /* Root -> waist -> spine, with independent head/left-arm and weapon-arm
     * branches. A mathematically aligned barrel is insufficient if calibration
     * achieves it by tipping the entire upper body. */
    enum { COUNT=7, ARM=4, HAND=5 };
    int parents[COUNT]={-1,0,1,2,2,ARM,2};
    unsigned order[COUNT]={0,1,2,3,4,5,6};
    uint8_t mask[COUNT]={0,1,1,1,1,1,1};
    float straight[COUNT*12]={0},authored[COUNT*12],calibrated[COUNT*12];
    float native[COUNT*12],expected[COUNT*12],output[COUNT*12];
    for(unsigned i=0;i<COUNT;++i) {
        straight[i*12+3]=1;
        straight[i*12+4]=straight[i*12+5]=straight[i*12+6]=1;
        straight[i*12+11]=1;
    }
    straight[12+1]=1;straight[ARM*12]=.25f;straight[HAND*12]=.3f;
    for(int yaw=-60;yaw<=60;yaw+=15) for(int pitch=-60;pitch<=60;pitch+=15) {
        float y=yaw*.01745329252f,p=pitch*.01745329252f;
        float target[3]={100*sinf(y)*cosf(p),1+100*sinf(p),100*cosf(y)*cosf(p)};
        memcpy(authored,straight,sizeof(authored));
        /* Preserve some authored spine/head articulation, not a frozen torso. */
        authored[2*12+8]=sinf(p*.1f);authored[2*12+11]=cosf(p*.1f);
        authored[3*12+9]=sinf(y*.3f);authored[3*12+11]=cosf(y*.3f);
        memcpy(calibrated,authored,sizeof(calibrated));
        CHECK(calibrate_pose(calibrated,parents,order,COUNT,ARM,HAND,
            (float[]){-.25f,.1f,0},target));
        for(unsigned i=0;i<COUNT*12;++i)
            if(i<ARM*12+8 || i>=ARM*12+12) CHECK(calibrated[i]==authored[i]);
        /* Existing native idle/recoil animation remains the output base. Only
         * the weapon-arm's local rotation may differ from the authored overlay. */
        memcpy(native,straight,sizeof(native));
        native[HAND*12+8]=sinf(.07f);native[HAND*12+11]=cosf(.07f);
        native[1*12+10]=sinf(.03f);native[1*12+11]=cosf(.03f);
        memcpy(expected,native,sizeof(expected));memcpy(output,native,sizeof(output));
        CHECK(apply_reference(expected,authored,straight,mask,COUNT,1));
        CHECK(apply_reference(output,calibrated,straight,mask,COUNT,1));
        for(unsigned i=0;i<COUNT*12;++i)
            if(i<ARM*12+8 || i>=ARM*12+12) CHECK(output[i]==expected[i]);
        CHECK(!memcmp(output+HAND*12,native+HAND*12,12*sizeof(float)));
    }
}
static void test_coordinated_arm_reference(void) {
    enum { COUNT=7, GUN=2, HAND=3, SUPPORT=4, ELBOW=5, WRIST=6 };
    int parents[COUNT]={-1,0,1,GUN,1,SUPPORT,ELBOW};
    unsigned order[COUNT]={0,1,2,3,4,5,6};
    float rest[COUNT*12]={0},pose[COUNT*12],saved[COUNT*12];
    for(unsigned i=0;i<COUNT;++i) { rest[i*12+4]=rest[i*12+5]=rest[i*12+6]=1;rest[i*12+11]=1; }
    rest[12+1]=1;rest[HAND*12]=-.5f;rest[SUPPORT*12+2]=.3f;
    rest[ELBOW*12]=-.25f;rest[ELBOW*12+1]=-.15f;
    rest[WRIST*12]=-.25f;rest[WRIST*12+1]=.15f;
    for(int degrees=-30;degrees<=30;degrees+=3) {
        float radians=degrees*.01745329252f,target[3]={-100*cosf(radians),1,100*sinf(radians)};
        memcpy(pose,rest,sizeof(pose));
        CHECK(calibrate_arms(pose,parents,order,COUNT,GUN,HAND,SUPPORT,ELBOW,WRIST,
            (float[]){-.1f,0,0},target));
        float gq[4],gp[3],wq[4],wp[3],oldq[4],oldp[3],a[3],aq[4],goal[3];
        CHECK(joint_world(pose,parents,COUNT,GUN,gq,gp));
        CHECK(joint_world(rest,parents,COUNT,WRIST,oldq,oldp));
        CHECK(joint_world(pose,parents,COUNT,WRIST,wq,wp));
        CHECK(joint_world(pose,parents,COUNT,SUPPORT,aq,a));
        for(unsigned i=0;i<3;++i) oldp[i]-=gp[i];
        rotate(gq,oldp,goal);
        for(unsigned i=0;i<3;++i) goal[i]+=gp[i];
        float desired[3],distance=0;
        CHECK(SudekiMpLanAimConverge(a,goal,desired));
        for(unsigned i=0;i<3;++i) distance+=(goal[i]-a[i])*(goal[i]-a[i]);
        float length=sqrtf(.25f*.25f+.15f*.15f);
        distance=fminf(2*length-length*.001f,sqrtf(distance));
        for(unsigned i=0;i<3;++i) CHECK(fabsf(wp[i]-(a[i]+desired[i]*distance))<.0001f);
        /* Wrist follows the corrected gun reference; neither limb is stretched.
         * Everything outside the four explicit arm rotations is untouched. */
        float expected[4];multiply(gq,oldq,expected);
        CHECK(fabsf(wq[0]*expected[0]+wq[1]*expected[1]+wq[2]*expected[2]+wq[3]*expected[3])>.9999f);
        for(unsigned i=0;i<COUNT;++i) {
            CHECK(!memcmp(pose+i*12,rest+i*12,8*sizeof(float)));
            if(i!=GUN && i!=SUPPORT && i!=ELBOW && i!=WRIST)
                CHECK(!memcmp(pose+i*12,rest+i*12,12*sizeof(float)));
        }
        float hq[4],hp[3],axis[3],aim[3];
        CHECK(joint_world(pose,parents,COUNT,HAND,hq,hp));
        rotate(hq,(float[]){-1,0,0},axis);
        CHECK(SudekiMpLanAimConverge(hp,target,aim));
        CHECK(axis[0]*aim[0]+axis[1]*aim[1]+axis[2]*aim[2]>.99999f);
    }
    memcpy(pose,rest,sizeof(pose));memcpy(saved,pose,sizeof(saved));
    parents[WRIST]=SUPPORT; /* rejected after gun calibration: all edits roll back */
    CHECK(!calibrate_arms(pose,parents,order,COUNT,GUN,HAND,SUPPORT,ELBOW,WRIST,
        (float[]){-.1f,0,0},(float[]){-100,1,20}));
    CHECK(!memcmp(pose,saved,sizeof(pose)));parents[WRIST]=ELBOW;
    parents[GUN]=SUPPORT;
    CHECK(!calibrate_arms(pose,parents,order,COUNT,GUN,HAND,SUPPORT,ELBOW,WRIST,
        (float[]){-.1f,0,0},(float[]){-100,1,20}));
    CHECK(!memcmp(pose,saved,sizeof(pose)));parents[GUN]=1;
    CHECK(!calibrate_arms(pose,parents,order,COUNT,GUN,HAND,GUN,ELBOW,WRIST,
        (float[]){-.1f,0,0},(float[]){-100,1,20}));
    CHECK(!memcmp(pose,saved,sizeof(pose)));
    float q[4],p[3];parents[1]=HAND;
    CHECK(!joint_world(pose,parents,COUNT,HAND,q,p)); /* parent cycle */
    parents[1]=0;parents[ELBOW]=COUNT;
    CHECK(!joint_world(pose,parents,COUNT,WRIST,q,p)); /* foreign parent */
    parents[ELBOW]=SUPPORT;
    CHECK(!calibrate_arms(pose,parents,order,COUNT,GUN,HAND,SUPPORT,ELBOW,WRIST,
        (float[]){-.1f,0,0},(float[]){NAN,1,20}));
    CHECK(!memcmp(pose,saved,sizeof(pose)));
    /* One envelope moves both arms to/from the authored stance, before shot
     * blending. Release finishes at the full original idle, not one held arm. */
    uint8_t mask[COUNT]={0,1,1,1,1,1,1};float firing[COUNT*12];memcpy(firing,rest,sizeof(firing));
    firing[GUN*12+9]=sinf(.25f);firing[GUN*12+11]=cosf(.25f);
    firing[SUPPORT*12+9]=sinf(.4f);firing[SUPPORT*12+11]=cosf(.4f);
    for(unsigned frame=0;frame<=20;++frame) {
        float amount=(float)frame/20;memcpy(pose,rest,sizeof(pose));
        CHECK(blend_hold_base(pose,firing,mask,COUNT,amount));
        CHECK(fabsf(pose[GUN*12+9]-sinf(.25f*amount))<.00001f);
        CHECK(fabsf(pose[SUPPORT*12+9]-sinf(.4f*amount))<.00001f);
        CHECK(!memcmp(pose,rest,12*sizeof(float)));
    }
}
int main(void) {
    test_fp_idle_loop();
    test_held_base();
    test_reference_keeps_native_animation();
    test_idle_aim_release();
    test_arm_calibration_preserves_authored_torso();
    test_coordinated_arm_reference();
    float chain[36]={0},unchanged[36];int parents[3]={-1,0,1};unsigned order[3]={0,1,2};
    for(unsigned i=0;i<3;++i) { chain[i*12+4]=chain[i*12+5]=chain[i*12+6]=1;chain[i*12+11]=1; }
    chain[12+1]=1;chain[24+0]=.2f;
    memcpy(unchanged,chain,sizeof(chain));
    CHECK(calibrate_pose(chain,parents,order,3,1,2,(float[]){-.25f,.1f,0},(float[]){0,1,100}));
    CHECK(!memcmp(chain,unchanged,48)); /* no root pitch / locomotion mutation */
    CHECK(!memcmp(chain+24,unchanged+24,48)); /* hand local transform intact */
    float axis[3];rotate(chain+12+8,(float[]){-1,0,0},axis);
    CHECK(axis[2]>.999f);
    /* Authored/compressed rotation keys are valid but not mathematically unit.
     * A length-sensitive dot test must not flicker between corrected/native
     * poses as these keys interpolate through an otherwise stationary idle. */
    for(unsigned key=1;key<=8;++key) {
        memcpy(chain,unchanged,sizeof(chain));
        float n=1.f-(float)key*.00001f;
        chain[24+9]=sinf(.78539816339f)*n;
        chain[24+11]=cosf(.78539816339f)*n;
        float before[36];memcpy(before,chain,sizeof(before));
        CHECK(calibrate_pose(chain,parents,order,3,1,2,(float[]){0,0,0},(float[]){0,1,100}));
        CHECK(!memcmp(chain,before,48) && !memcmp(chain+24,before+24,48));
    }
    memcpy(chain,unchanged,sizeof(chain));parents[1]=2;
    CHECK(!calibrate_pose(chain,parents,order,3,1,2,(float[]){0,0,0},(float[]){0,0,100}));
    CHECK(!memcmp(chain,unchanged,sizeof(chain)));parents[1]=0;
    CHECK(!calibrate_pose(chain,parents,order,3,2,1,(float[]){0,0,0},(float[]){0,0,100}));
    chain[5]=NAN;
    CHECK(!calibrate_pose(chain,parents,order,3,1,2,(float[]){0,0,0},(float[]){0,0,100}));
    float branched[48]={0};int branch_parents[4]={-1,0,1,1};unsigned branch_order[4]={0,1,2,3};
    memcpy(branched,unchanged,sizeof(unchanged));
    branched[40]=.7f;branched[41]=1.2f;branched[42]=1;branched[47]=1;
    float clothing[12];memcpy(clothing,branched+36,sizeof(clothing));
    CHECK(calibrate_pose(branched,branch_parents,branch_order,4,1,2,
        (float[]){-.25f,.1f,0},(float[]){0,1,100}));
    CHECK(!memcmp(clothing,branched+36,sizeof(clothing)));
    branched[17]=.5f; /* non-uniform scale on the actual arm chain is unsupported */
    CHECK(!calibrate_pose(branched,branch_parents,branch_order,4,1,2,
        (float[]){-.25f,.1f,0},(float[]){0,1,100}));
    uint8_t states[SAMPLE_CHANNELS][SAMPLE_STATE_BYTES];
    memset(states,0xcd,sizeof(states));
    sample_states(states,50,24.0f);
    for(unsigned ch=0;ch<SAMPLE_CHANNELS;++ch) {
        uint16_t selector;float phase;
        memcpy(&selector,states[ch],2);memcpy(&phase,states[ch]+12,4);
        CHECK(selector==50 && phase==24.0f);
        CHECK(states[ch][2]==0 && states[ch][23]==0);
    }
    sample_states(states,54,0);
    CHECK(!memcmp(states[0],states[1],SAMPLE_STATE_BYTES));
    CHECK(states[1][0]==54 && states[1][12]==0);
    float out[3],zero[3]={0},muzzle[3]={0};
    CHECK(SudekiMpLanAimConverge((float[]){1,2,0},(float[]){0,2,100},out));
    CHECK(fabsf(out[0]/out[2]+.01f)<.000001f);
    CHECK(SudekiMpLanAimConverge((float[]){-1,2,0},(float[]){0,2,100},out));
    CHECK(fabsf(out[0]/out[2]-.01f)<.000001f);
    CHECK(SudekiMpLanAimTargetNearActor(zero,(float[]){0,0,1},(float[]){0,1.6f,100}));
    CHECK(!SudekiMpLanAimTargetNearActor(zero,(float[]){0,0,1},(float[]){6,1.6f,100}));
    CHECK(!SudekiMpLanAimTargetNearActor(zero,(float[]){0,0,1},(float[]){0,0,-100}));
    CHECK(!SudekiMpLanAimTargetNearActor(zero,(float[]){0,0,1},(float[]){NAN,0,100}));
    memcpy(out,(float[]){1,2,3},12);
    CHECK(!SudekiMpLanAimConverge(zero,zero,out) && out[0]==1 && out[2]==3);
    CHECK(!SudekiMpLanAimConverge(zero,(float[]){NAN,0,0},out) && out[0]==1);
    CHECK(!SudekiMpLanAimNormalize(zero,out));
    CHECK(!SudekiMpLanAimNormalize((float[]){NAN,0,1},out));
    CHECK(!SudekiMpLanAimNormalize((float[]){0,INFINITY,1},out));
    CHECK(!SudekiMpLanAimNormalize((float[]){1,1,1},out));
    CHECK(SudekiMpLanAimNormalize((float[]){0,1,0},out) && out[1]==1);
    CHECK(SudekiMpLanAimNormalize((float[]){0,-1,0},out) && out[1]==-1);
    float p[24]={0},c[24]={0},a[24]={0},saved[24];
    uint8_t mask[2]={0,1};
    float native_pose[24],sampled_pose[24];
    for(unsigned i=0;i<24;++i) { native_pose[i]=(float)i;sampled_pose[i]=100.f+(float)i; }
    native_pose[15]=1;native_pose[19]=0;sampled_pose[15]=sampled_pose[19]=0;
    copy_firing_pose(native_pose,sampled_pose,mask,2);
    for(unsigned i=0;i<12;++i) CHECK(native_pose[i]==(float)i);
    CHECK(native_pose[15]==1 && native_pose[19]==0);
    for(unsigned i=12;i<24;++i) if(i!=15 && i!=19) CHECK(native_pose[i]==sampled_pose[i]);
    p[11]=p[23]=c[11]=c[23]=a[11]=1;
    a[20]=sinf(0.4f); a[23]=cosf(0.4f);
    p[0]=7; p[12]=9; p[16]=1; memcpy(saved,p,sizeof(p));
    CHECK(SudekiMpLanAimOverlay(p,a,c,mask,2,0));
    CHECK(!memcmp(p,saved,sizeof(p)));
    CHECK(SudekiMpLanAimOverlay(p,a,c,mask,2,1));
    CHECK(!memcmp(p,saved,12*sizeof(float)) && !memcmp(p+12,saved+12,8*sizeof(float)));
    CHECK(fabsf(p[20]-sinf(0.4f))<0.00001f && fabsf(p[23]-cosf(0.4f))<0.00001f);
    memcpy(p,saved,sizeof(p));
    CHECK(SudekiMpLanAimOverlay(p,a,c,mask,2,2));
    CHECK(fabsf(p[20]-sinf(0.8f))<0.00001f);
    a[20]=NAN; memcpy(saved,p,sizeof(p));
    CHECK(!SudekiMpLanAimOverlay(p,a,c,mask,2,1) && !memcmp(p,saved,sizeof(p)));
    CHECK(!SudekiMpLanAimOverlay(p,a,c,mask,2,INFINITY));

    uint8_t *image=VirtualAlloc(NULL,0x410000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    uint8_t actor[0xc0]={0},manager[0x14]={0};
    CHECK(image!=NULL);
    aim_image=image; aim_thread=GetCurrentThreadId(); aim_witness=witness;
    aim_target_witness=target_witness;
    uint8_t camera_manager[0x24]={0},camera[0x108]={0},mode[0x10]={0},render[0xd0]={0};
    *(void **)(image+0x409d7c)=camera_manager;*(void **)(image+0x408da8)=mode;
    *(void **)camera_manager=image+0x2c7b80;*(void **)(camera_manager+0x20)=camera;
    *(void **)camera=image+0x2cce5c;*(void **)(camera+0x34)=render;
    *(void **)(mode+0xc)=camera+0x2c;
    memcpy(render+0xb0,(float[]){0,.6f,.8f},12);
    memcpy(render+0xc0,(float[]){1,2,3},12);
    float camera_direction[3],camera_target[3];
    CHECK(SudekiMpLanAimCameraTarget(camera_direction,camera_target));
    CHECK(fabsf(camera_target[0]-1)<.00001f && fabsf(camera_target[1]-62)<.00001f &&
        fabsf(camera_target[2]-83)<.00001f);
    *(void **)(mode+0xc)=NULL;
    CHECK(!SudekiMpLanAimCameraTarget(camera_direction,camera_target));
    CHECK(camera_target[0]==1 && camera_target[2]==83); /* no stale/fallback overwrite */
    *(void **)(mode+0xc)=camera+0x2c;*(float *)(render+0xc0)=NAN;
    CHECK(!SudekiMpLanAimCameraTarget(camera_direction,camera_target));
    *(float *)(render+0xc0)=1;aim_thread=0;
    CHECK(!SudekiMpLanAimCameraTarget(camera_direction,camera_target));
    aim_thread=GetCurrentThreadId();
    expected_actor=actor; aim_actors[1]=actor;
    *(void **)manager=image+MISSILE_VT; *(void **)(manager+0x10)=actor;
    *(void **)(actor+0xbc)=manager;
    admitted=TRUE; memcpy(out,(float[]){0,0,1},12);
    apply_direction(manager,muzzle,out);
    CHECK(fabsf(out[1]-0.6f)<0.00001f && fabsf(out[2]-0.8f)<0.00001f);
    target_admitted=FALSE; memcpy(out,(float[]){0,0,1},12); apply_direction(manager,muzzle,out);
    CHECK(out[1]==0 && out[2]==1); target_admitted=TRUE;
    admitted=FALSE; memcpy(out,(float[]){0,0,1},12); apply_direction(manager,muzzle,out);
    CHECK(out[1]==0 && out[2]==1);
    admitted=TRUE; *(void **)(actor+0xbc)=NULL; apply_direction(manager,muzzle,out);
    CHECK(out[1]==0 && out[2]==1);
    *(void **)(actor+0xbc)=manager; aim_actors[1]=NULL; apply_direction(manager,muzzle,out);
    CHECK(out[1]==0 && out[2]==1);
    aim_actors[1]=actor; aim_thread=0; apply_direction(manager,muzzle,out);
    CHECK(out[1]==0 && out[2]==1);
    aim_image=NULL; aim_witness=NULL; aim_actors[1]=NULL;
    VirtualFree(image,0,MEM_RELEASE);
    if(failures) return 1;
    puts("ranged aim tests passed: finite direction, additive pose, root preservation, stale ownership and thread rejection");
    return 0;
}
