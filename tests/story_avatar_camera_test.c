/* Synthetic memory/native-call fixture. No supported game image or native
 * camera callbacks execute here. CameraTargetAbiTest owns register ABI proof. */
#include "../src/hooks/lan_story_avatar_camera.c"
/* Same pure geometry implementation; whole-program fixture strips unrelated
 * frame transport functions instead of substituting a weaker validator. */
#include "../src/network/lan_story_frame.c"
#include "../src/engine/orbit_camera.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Fixture {
    uint8_t mode[0x10],scene[0x44],renderer[0x80],camera[0x108],render[0xdc],manager[0x60];
    uint8_t game_target[0x34],offset_target[0xd0],position[0x98],hero[0x48],world[8];
    uint8_t camera_state[4],state_data[0x550],config_handle[0x0c],config[0x80];
} Fixture;
static uint8_t *image,*avatar,*created;
static Fixture *f;
static unsigned creates,installs,releases,deny_bind,deny_restore;
static BOOL refuse_create,refuse_release,refuse_exact,world_destroyed,mutate_slot,notify_moves_view;
static BOOL refuse_transition,change_candidate;
static unsigned transitions;
static SudekiMpLanStoryAvatarCameraIdentity expected;
static void put(void *p,unsigned off,void *value) { *(void **)((uint8_t *)p+off)=value; }
static uint32_t refs(void *p) { return *(uint32_t *)((uint8_t *)p+4); }
BOOL SudekiMpCheckLoadedExecutable(HMODULE module) { return (uint8_t *)module==image; }
static BOOL exact(const SudekiMpLanStoryAvatarCameraIdentity *identity,
    SudekiMpLanStoryAvatarCameraOperation operation,void *context) {
    assert(context==&expected && same_identity(identity,&expected));
    if(operation==SUDEKIMP_AVATAR_CAMERA_NATIVE_TRANSITION) {
        ++transitions;
        if(change_candidate && transitions==2u) put(f->camera,0x40,f->state_data);
        return !refuse_transition && !refuse_exact;
    }
    return operation==SUDEKIMP_AVATAR_CAMERA_WORLD_DESTROYED?world_destroyed:!refuse_exact;
}
static void __stdcall create_target(void *list,void **out,const float *matrix) {
    assert(list==f->manager+0x4c); ++creates;
    if(refuse_create) { *out=NULL; return; }
    created=calloc(1,0x80); assert(created);
    put(created,0,image+MATRIX_VT); *(uint32_t *)(created+4)=1;
    memcpy(created+0x20,matrix,64); memcpy(created+0x14,matrix+12,12);
    put(created,0x60,created+0x20);
    void *old=*(void **)list; put(created,0x70,old);
    if(old) put(old,0x74,created);
    *(void **)list=created; *out=created;
}
void SudekiMpCallCameraTargetInstall(void *camera,void *target,unsigned slot,void *function) {
    assert(camera==f->camera && slot<2 && function==image+INSTALL_RVA); ++installs;
    BOOL binding=target==created;
    if((binding?deny_bind:deny_restore)&(1u<<slot)) return;
    uint8_t *old=*(void **)((uint8_t *)camera+0xb4+4*slot);
    assert(refs(old)>=2u); --*(uint32_t *)(old+4);
    /* Installer consumes the incoming retain; net persistent-slot count is
     * exactly the caller's +1 when this fixture has no state-owned refs. */
    put(camera,0xb4+4*slot,mutate_slot?f->hero:target);
    if(notify_moves_view) {
        *(float *)(f->render+0xc0)=binding?12.0f:-20.0f;
        ++*(uint16_t *)(f->render+0x2c);
    }
}
void SudekiMpCallCameraTargetRelease(void *list,void *target,void *function) {
    assert(list==f->manager+0x4c && target==created && function==image+RELEASE_RVA);
    assert(!refs(target)); ++releases;
    if(refuse_release) return;
    uint8_t *next=*(void **)(created+0x70),*previous=*(void **)(created+0x74);
    if(previous) put(previous,0x70,next); else *(void **)list=next;
    if(next) put(next,0x74,previous);
    free(created); created=NULL;
}
static void setup(BOOL aliased) {
    assert(!base && !lease.retained && !busy);
    image=VirtualAlloc(NULL,SUDEKIMP_EXPECTED_IMAGE_SIZE,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    f=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    avatar=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    assert(image && f && avatar);
    memcpy(image+TARGET_NOTIFY_RVA,notify_entry,sizeof(notify_entry));
    for(unsigned n=0;n<sizeof(native_states)/sizeof(*native_states);++n)
        for(unsigned v=0;v<sizeof(target_virtuals)/sizeof(*target_virtuals);++v)
            put(image,native_states[n].state_vt+0x18+4*v,image+target_virtuals[v]);
    memcpy(image+CREATE_RVA,create_entry,sizeof(create_entry));
    memcpy(image+INSTALL_RVA,install_entry,sizeof(install_entry));
    memcpy(image+RELEASE_RVA,release_entry,sizeof(release_entry));
    put(image,CAMERA_MODE_GLOBAL,f->mode); put(image,SCENE_GLOBAL,f->scene);
    put(image,WORLD_GLOBAL,f->world); put(image,MANAGER_GLOBAL,f->manager); put(image,LIST_OWNER_GLOBAL,f->manager);
    put(f->mode,0x0c,f->camera+0x2c);
    put(f->scene,0,image+SCENE_VT); put(f->scene,0x40,f->renderer);
    put(f->renderer,0x7c,f->render); put(f->render,0,image+RENDER_STATE_VT);
    put(f->camera,0,image+CAMERA_VT); put(f->camera,0x34,f->render);
    put(f->camera,0x3c,f->camera_state); put(f->camera,0x40,f->state_data);
    put(f->camera_state,0,image+EXPLORATION_VT); put(f->state_data,0,image+EXPLORATION_DATA_VT);
    put(image,STATE_REGISTRY,f->camera_state);
    *(uint32_t *)(f->state_data+4)=1u;
    float seed[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,2,-5,1};
    float projection[3]={1.0f,0.1f,1000.0f};
    memcpy(f->render+0x90,seed,sizeof(seed)); memcpy(f->render+0xd0,projection,sizeof(projection));
    static const unsigned fields[]={6,7,8,11,12,13,18,26,27,28,29,30};
    static const float values[]={1,2,10,5,100,160,1,2,1,4,2,4};
    *(uint32_t *)f->config=0x48+sizeof(values);
    put(f->config,0x44,f->config+0x48); put(f->config_handle,8,f->config);
    put(f->camera,0x38,f->config_handle);
    for(unsigned n=0;n<sizeof(fields)/sizeof(*fields);++n)
        *(uint32_t *)(f->config+8+4*(fields[n]/32))|=1u<<(fields[n]%32);
    memcpy(f->config+0x48,values,sizeof(values));
    put(f->manager,0,image+MANAGER_VT); put(f->manager,8,image+MANAGER_SECONDARY_VT);
    put(f->manager,12,image+MANAGER_THIRD_VT); put(f->manager,0x20,f->camera); put(f->manager,0x24,f->camera);
    put(f->game_target,0,image+GAME_OBJECT_VT); *(uint32_t *)(f->game_target+4)=aliased?2u:3u;
    put(f->game_target,0x20,f->hero); put(f->manager,0x50,f->game_target);
    put(f->offset_target,0,image+OFFSET_VT); *(uint32_t *)(f->offset_target+4)=1;
    put(f->offset_target,0x20,f->game_target);
    if(!aliased) put(f->manager,0x54,f->offset_target);
    put(f->camera,0xb4,aliased?f->game_target:f->offset_target); put(f->camera,0xb8,f->game_target);
    put(avatar,0x44,f->position); put(f->position,0,image+POSITION_VT); put(f->position,0x10,avatar);
    float point[3]={10,20,30}; memcpy(f->position+0x18,point,sizeof(point));
    expected=(SudekiMpLanStoryAvatarCameraIdentity){.actor=avatar,.original_hero=f->hero,
        .world=f->world,.scene_manager=f->scene,.session_generation=90,.actor_generation=12,
        .world_epoch=3,.scene_epoch=4,.scene_revision=5};
    creates=installs=releases=deny_bind=deny_restore=0;
    refuse_create=refuse_release=refuse_exact=world_destroyed=mutate_slot=notify_moves_view=FALSE;
    refuse_transition=change_candidate=FALSE; transitions=0;
    assert(SudekiMpLanStoryAvatarCameraInstall((HMODULE)image)); native_create=create_target;
}
static void cleanup(void) {
    assert(!SudekiMpLanStoryAvatarCameraRetains());
    assert(SudekiMpLanStoryAvatarCameraUninstall());
    if(created) { free(created); created=NULL; }
    VirtualFree(avatar,0,MEM_RELEASE); VirtualFree(f,0,MEM_RELEASE); VirtualFree(image,0,MEM_RELEASE);
}
static void bind_ok(void) {
    assert(SudekiMpLanStoryAvatarCameraBind(&expected,exact,&expected));
    assert(lease.ready && lease.installed==3 && refs(created)==3u);
    assert(*(void **)(f->camera+0xb4)==created && *(void **)(f->camera+0xb8)==created);
    assert(!memcmp(created+0x50,f->position+0x18,12));
}
static void normal_and_actor_retirement(void) {
    for(unsigned alias=0;alias<2;++alias) {
        setup(alias); unsigned original_refs=refs(f->game_target); bind_ok();
        assert(!SudekiMpLanStoryAvatarCameraBind(&expected,exact,&expected));
        assert(!SudekiMpLanStoryAvatarCameraUninstall());
        *(float *)(f->position+0x18)=91.0f;
        assert(SudekiMpLanStoryAvatarCameraUpdate(&expected,exact,&expected));
        assert(*(float *)(created+0x50)==91.0f && *(float *)(created+0x14)==91.0f);
        SudekiMpLanStoryAvatarCameraIdentity stale=expected; ++stale.actor_generation;
        assert(!SudekiMpLanStoryAvatarCameraUpdate(&stale,exact,&expected));
        assert(!SudekiMpLanStoryAvatarCameraRestore(&stale,exact,&expected));
        refuse_exact=TRUE;
        assert(!SudekiMpLanStoryAvatarCameraUpdate(&expected,exact,&expected));
        assert(!SudekiMpLanStoryAvatarCameraRestore(&expected,exact,&expected));
        assert(SudekiMpLanStoryAvatarCameraRetains()); refuse_exact=FALSE;
        DWORD old; assert(VirtualProtect(avatar,4096,PAGE_NOACCESS,&old));
        assert(SudekiMpLanStoryAvatarCameraRestore(&expected,exact,&expected));
        assert(!lease.retained && refs(f->game_target)==original_refs);
        assert(*(void **)(f->camera+0xb4)==(alias?f->game_target:f->offset_target));
        assert(*(void **)(f->camera+0xb8)==f->game_target && releases==1u);
        cleanup();
    }
}
static void partial_restoration(void) {
    setup(FALSE); bind_ok();
    deny_restore=1u;
    assert(!SudekiMpLanStoryAvatarCameraRestore(&expected,exact,&expected));
    assert(lease.installed==1u && lease.original_held[0] && lease.original_held[1]);
    assert(refs(created)==2u && *(void **)(f->camera+0xb8)==f->game_target);
    unsigned original_refs=refs(f->game_target),matrix_refs=refs(created);
    assert(!SudekiMpLanStoryAvatarCameraRestore(&expected,exact,&expected));
    assert(refs(f->game_target)==original_refs && refs(created)==matrix_refs);
    assert(!SudekiMpLanStoryAvatarCameraBind(&expected,exact,&expected));
    assert(!SudekiMpLanStoryAvatarCameraUpdate(&expected,exact,&expected));
    deny_restore=0;
    assert(SudekiMpLanStoryAvatarCameraRestore(&expected,exact,&expected)); cleanup();
}
static void acquisition_rollback(void) {
    setup(FALSE); refuse_create=TRUE;
    assert(!SudekiMpLanStoryAvatarCameraBind(&expected,exact,&expected));
    assert(!lease.retained && refs(f->offset_target)==1u && refs(f->game_target)==3u); cleanup();
    setup(FALSE); deny_bind=2u; deny_restore=1u;
    assert(!SudekiMpLanStoryAvatarCameraBind(&expected,exact,&expected));
    assert(lease.retained && lease.installed==1u && refs(created)==2u);
    deny_restore=0;
    assert(SudekiMpLanStoryAvatarCameraRestore(&expected,exact,&expected)); cleanup();
}
static void foreign_owners_and_release_retry(void) {
    setup(FALSE); bind_ok();
    put(f->camera,0xb4,f->hero);
    assert(!SudekiMpLanStoryAvatarCameraRestore(&expected,exact,&expected));
    assert(*(void **)(f->camera+0xb4)==f->hero);
    put(f->camera,0xb4,created);
    put(image,LIST_OWNER_GLOBAL,f->hero);
    assert(!SudekiMpLanStoryAvatarCameraRestore(&expected,exact,&expected));
    put(image,LIST_OWNER_GLOBAL,f->manager);
    image[INSTALL_RVA]^=1u;
    assert(!SudekiMpLanStoryAvatarCameraRestore(&expected,exact,&expected));
    image[INSTALL_RVA]^=1u;
    refuse_release=TRUE;
    assert(!SudekiMpLanStoryAvatarCameraRestore(&expected,exact,&expected));
    assert(lease.release_pending && !refs(created) && lease.installed==0);
    assert(!SudekiMpLanStoryAvatarCameraRestore(&expected,exact,&expected) && !refs(created));
    refuse_release=FALSE;
    assert(SudekiMpLanStoryAvatarCameraRestore(&expected,exact,&expected)); cleanup();
}
static void malformed_and_destruction_witness(void) {
    setup(FALSE); put(f->game_target,0x20,avatar);
    assert(!SudekiMpLanStoryAvatarCameraBind(&expected,exact,&expected) && !creates);
    put(f->game_target,0x20,f->hero); put(f->game_target,0x30,f->game_target);
    assert(!SudekiMpLanStoryAvatarCameraBind(&expected,exact,&expected) && !creates);
    put(f->game_target,0x30,NULL); mutate_slot=TRUE;
    assert(!SudekiMpLanStoryAvatarCameraBind(&expected,exact,&expected));
    assert(lease.retained && lease.uncertain);
    assert(!SudekiMpLanStoryAvatarCameraNativeExitReturned(&expected,exact,&expected));
    world_destroyed=TRUE;
    SudekiMpLanStoryAvatarCameraIdentity stale=expected; ++stale.world_epoch;
    assert(!SudekiMpLanStoryAvatarCameraNativeExitReturned(&stale,exact,&expected));
    DWORD old; assert(VirtualProtect(f,4096,PAGE_NOACCESS,&old));
    assert(VirtualProtect(avatar,4096,PAGE_NOACCESS,&old));
    assert(SudekiMpLanStoryAvatarCameraNativeExitReturned(&expected,exact,&expected));
    assert(!lease.retained); cleanup();
}
static void own_client_view(void) {
    setup(FALSE); notify_moves_view=TRUE; bind_ok();
    assert(lease.original_view.matrix[12]==0.0f && lease.view.matrix[12]==12.0f);
    float x,z;
    assert(!SudekiMpLanStoryAvatarCameraDirection(&expected,exact,&expected,0,1,&x,&z));
    /* The avatar view never reads the hero's CPosition or cached target offset. */
    put(f->hero,0x44,(void *)(uintptr_t)1);
    *(float *)(f->game_target+0x14)=50000.0f;
    *(float *)(f->offset_target+0xb0)=-50000.0f;
    assert(SudekiMpLanStoryAvatarCameraPresent(&expected,exact,&expected,0,0));
    assert(lease.view_published && lease.view_borrowed);
    assert(!SudekiMpLanStoryAvatarCameraNativeDirection(&expected,exact,&expected,0,1,&x,&z));
    assert(*(float *)(f->render+0xc0)==10.0f); /* Eye follows avatar x. */
    assert(SudekiMpLanStoryAvatarCameraDirection(&expected,exact,&expected,0,0.5f,&x,&z));
    assert(fabsf(x)<0.001f && fabsf(z-0.5f)<0.001f);
    *(float *)(f->position+0x18)=100.0f;
    assert(SudekiMpLanStoryAvatarCameraPresent(&expected,exact,&expected,0.01f,0.1f));
    assert(*(float *)(created+0x50)==100.0f && lease.distance<5.0f);
    assert(SudekiMpLanStoryAvatarCameraDirection(&expected,exact,&expected,1,1,&x,&z));
    assert(fabsf(x*x+z*z-1.0f)<0.001f);
    assert(!SudekiMpLanStoryAvatarCameraPresent(&expected,exact,&expected,NAN,0));
    assert(!SudekiMpLanStoryAvatarCameraDirection(&expected,exact,&expected,2,0,&x,&z) && x==0 && z==0);
    *(float *)(f->render+0xc0)=777.0f;
    assert(!SudekiMpLanStoryAvatarCameraPresent(&expected,exact,&expected,0,0));
    assert(!SudekiMpLanStoryAvatarCameraRestore(&expected,exact,&expected));
    assert(*(float *)(f->render+0xc0)==777.0f && lease.retained);
    memcpy(f->render+0x90,lease.view.matrix,sizeof(lease.view.matrix));
    assert(SudekiMpLanStoryAvatarCameraRestore(&expected,exact,&expected));
    assert(*(float *)(f->render+0xc0)==-20.0f && *(float *)(f->render+0xc8)==-5.0f);
    cleanup();
}
static void own_native_direction(void) {
    setup(FALSE); bind_ok(); float x,z;
    assert(SudekiMpLanStoryAvatarCameraNativeDirection(&expected,exact,&expected,0,0.5f,&x,&z));
    assert(fabsf(x)<0.001f && fabsf(z-0.5f)<0.001f && !lease.view_borrowed);
    /* Native ticks may turn the owned camera; direction reads that fresh view
     * instead of demanding the old bind snapshot or writing camera fields. */
    float rotated[16]={0,0,1,0,0,1,0,0,-1,0,0,0,4,2,-8,1};
    memcpy(f->render+0x90,rotated,sizeof(rotated));
    assert(SudekiMpLanStoryAvatarCameraNativeDirection(&expected,exact,&expected,0,1,&x,&z));
    assert(fabsf(x+1.0f)<0.001f && fabsf(z)<0.001f);
    assert(!memcmp(f->render+0x90,rotated,sizeof(rotated)));
    put(f->camera,0xb8,f->game_target);
    assert(!SudekiMpLanStoryAvatarCameraNativeDirection(&expected,exact,&expected,0,1,&x,&z) && x==0 && z==0);
    put(f->camera,0xb8,created);
    refuse_exact=TRUE;
    assert(!SudekiMpLanStoryAvatarCameraNativeDirection(&expected,exact,&expected,0,1,&x,&z));
    refuse_exact=FALSE; *(float *)(f->render+0x90)=NAN;
    assert(!SudekiMpLanStoryAvatarCameraNativeDirection(&expected,exact,&expected,0,1,&x,&z));
    memcpy(f->render+0x90,rotated,sizeof(rotated));
    assert(SudekiMpLanStoryAvatarCameraRestore(&expected,exact,&expected)); cleanup();
}
static uint8_t *install_state_pair(unsigned kind) {
    assert(kind<sizeof(native_states)/sizeof(*native_states));
    uint8_t *p=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE); assert(p);
    put(p,0,image+native_states[kind].state_vt);
    put(p,0x100,image+native_states[kind].data_vt);
    *(uint32_t *)(p+0x104)=1; *(uint32_t *)(p+0x108)=kind;
    put(image,STATE_REGISTRY+4*kind,p);
    put(f->camera,0x3c,p); put(f->camera,0x40,p+0x100);
    return p;
}
static void supported_native_transitions(void) {
    setup(FALSE);
    uint8_t *old=install_state_pair(0); bind_ok();
    uint8_t *combat=install_state_pair(1); DWORD access;
    assert(VirtualProtect(old,4096,PAGE_NOACCESS,&access));
    *(float *)(f->position+0x18)=123;
    assert(SudekiMpLanStoryAvatarCameraUpdate(&expected,exact,&expected));
    assert(lease.camera_state==combat && lease.state_data==combat+0x100);
    assert(*(float *)(created+0x50)==123 && transitions==2u);
    uint8_t *boss=install_state_pair(2); float x,z;
    assert(SudekiMpLanStoryAvatarCameraNativeDirection(&expected,exact,&expected,0,1,&x,&z));
    assert(lease.camera_state==boss && z>0.999f);
    /* Cleanup remains possible after actor retirement and after another native
     * state replacement during a partially completed target restoration. */
    assert(VirtualProtect(avatar,4096,PAGE_NOACCESS,&access)); deny_restore=1u;
    assert(!SudekiMpLanStoryAvatarCameraRestore(&expected,exact,&expected));
    assert(lease.installed==1u && lease.original_held[0] && lease.original_held[1]);
    uint8_t *returned=install_state_pair(0);
    assert(VirtualProtect(boss,4096,PAGE_NOACCESS,&access));
    unsigned calls=installs;
    assert(!SudekiMpLanStoryAvatarCameraRestore(&expected,exact,&expected));
    assert(lease.camera_state==returned && lease.installed==1u && installs==calls+1u);
    deny_restore=0;
    assert(SudekiMpLanStoryAvatarCameraRestore(&expected,exact,&expected));
    assert(releases==1u && !lease.retained);
    VirtualFree(old,0,MEM_RELEASE); VirtualFree(combat,0,MEM_RELEASE);
    VirtualFree(boss,0,MEM_RELEASE); VirtualFree(returned,0,MEM_RELEASE); cleanup();
}
static void transition_rejections_and_retry(void) {
    for(unsigned fault=0;fault<11;++fault) {
        setup(FALSE); bind_ok(); uint8_t *candidate=install_state_pair(1);
        uint8_t *data=candidate+0x100;
        uint8_t *prior_state=lease.camera_state,*prior_data=lease.state_data;
        switch(fault) {
        case 0: put(f->camera,0xb8,f->game_target); break; /* Foreign native slot. */
        case 1: put(image,MANAGER_GLOBAL,f->hero); break;
        case 2: put(candidate,0,image+0x2db80cu); break; /* Unproved cinematic. */
        case 3: put(data,0,image+EXPLORATION_DATA_VT); break;
        case 4: *(uint32_t *)(data+8)=0; break;
        case 5: put(image,STATE_REGISTRY+4,f->camera_state); break;
        case 6: put(data,0x470,created); break;
        case 7: *(uint32_t *)(data+4)=0; break;
        case 8: *(uint32_t *)(data+4)=UINT32_MAX; break;
        case 9: refuse_transition=TRUE; break; /* Client or lost host authority. */
        case 10: put(image,native_states[1].state_vt+0x24,image+TARGET_NOTIFY_RVA); break;
        }
        unsigned calls=installs,matrix_refs=refs(created); float x,z;
        assert(!SudekiMpLanStoryAvatarCameraUpdate(&expected,exact,&expected));
        assert(!SudekiMpLanStoryAvatarCameraNativeDirection(&expected,exact,&expected,0,1,&x,&z));
        assert(x==0 && z==0);
        assert(!SudekiMpLanStoryAvatarCameraRestore(&expected,exact,&expected));
        assert(lease.retained && lease.camera_state==prior_state && lease.state_data==prior_data);
        assert(installs==calls && refs(created)==matrix_refs);
        put(f->camera,0xb8,created); put(image,MANAGER_GLOBAL,f->manager);
        put(candidate,0,image+native_states[1].state_vt); put(data,0,image+native_states[1].data_vt);
        *(uint32_t *)(data+8)=1; *(uint32_t *)(data+4)=1; put(data,0x470,NULL);
        put(image,STATE_REGISTRY+4,candidate); refuse_transition=FALSE;
        put(image,native_states[1].state_vt+0x24,image+target_virtuals[3]);
        assert(SudekiMpLanStoryAvatarCameraRestore(&expected,exact,&expected));
        VirtualFree(candidate,0,MEM_RELEASE); cleanup();
    }
    setup(FALSE); bind_ok(); uint8_t *candidate=install_state_pair(1);
    change_candidate=TRUE;
    assert(!SudekiMpLanStoryAvatarCameraUpdate(&expected,exact,&expected));
    assert(transitions==2u && lease.camera_state==f->camera_state && lease.state_data==f->state_data);
    assert(installs==2u && lease.retained); change_candidate=FALSE;
    put(f->camera,0x40,candidate+0x100);
    assert(SudekiMpLanStoryAvatarCameraRestore(&expected,exact,&expected));
    VirtualFree(candidate,0,MEM_RELEASE); cleanup();
}
static void paused_view_pins_native_state(void) {
    setup(FALSE); bind_ok(); assert(SudekiMpLanStoryAvatarCameraPresent(&expected,exact,&expected,0,0));
    uint8_t *candidate=install_state_pair(1); float x,z;
    assert(!SudekiMpLanStoryAvatarCameraPresent(&expected,exact,&expected,0,0));
    assert(!SudekiMpLanStoryAvatarCameraDirection(&expected,exact,&expected,0,1,&x,&z));
    assert(!SudekiMpLanStoryAvatarCameraUpdate(&expected,exact,&expected));
    assert(!SudekiMpLanStoryAvatarCameraRestore(&expected,exact,&expected));
    assert(!transitions && lease.retained && lease.state_data==f->state_data);
    put(f->camera,0x3c,f->camera_state); put(f->camera,0x40,f->state_data);
    assert(SudekiMpLanStoryAvatarCameraRestore(&expected,exact,&expected));
    VirtualFree(candidate,0,MEM_RELEASE); cleanup();
}
int main(void) {
    normal_and_actor_retirement(); partial_restoration(); acquisition_rollback();
    foreign_owners_and_release_retry(); malformed_and_destruction_witness(); own_client_view(); own_native_direction();
    supported_native_transitions(); transition_rejections_and_retry(); paused_view_pins_native_state();
    puts("PASS: avatar MatrixTarget lifecycle/view, exact native host mode transitions, retired state data, partial restore retry, foreign/unsupported state rejection, paused-client state lease");
    return 0;
}
