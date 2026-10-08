#include "hooks/lan_story_avatar_camera.h"
#include "engine/build_identity.h"
#include "engine/camera_target_abi.h"
#include "engine/orbit_camera.h"
#include "network/lan_story_frame.h"
#include <math.h>
#include <string.h>
#include <limits.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Avatar camera requires the supported 32-bit Windows target"
#endif

enum {
    CAMERA_MODE_GLOBAL=0x408da8u, SCENE_GLOBAL=0x408d58u,
    WORLD_GLOBAL=0x408d10u, MANAGER_GLOBAL=0x409d7cu, LIST_OWNER_GLOBAL=0x3c2f30u,
    MANAGER_VT=0x2c7b80u, MANAGER_SECONDARY_VT=0x2c7b94u, MANAGER_THIRD_VT=0x2c7b9cu,
    SCENE_VT=0x2c66b8u, CAMERA_VT=0x2cce5cu, RENDER_STATE_VT=0x2dd638u,
    POSITION_VT=0x2cdefcu, GAME_OBJECT_VT=0x2d42ccu,
    EXPLORATION_VT=0x2d9854u, EXPLORATION_DATA_VT=0x2ca694u, TARGET_NOTIFY_RVA=0x7ec80u,
    STATE_REGISTRY=0x3d4ad8u,
    OFFSET_VT=0x2d436cu, MATRIX_VT=0x2d43bcu,
    CREATE_RVA=0x134fb0u, INSTALL_RVA=0xe84c0u, RELEASE_RVA=0x135340u,
    LIST_LIMIT=128u
};
typedef void (__stdcall *MatrixCreate)(void *,void **,const float *);
static uint8_t *base;
static MatrixCreate native_create;
static DWORD native_thread;
static BOOL busy;
static struct {
    BOOL retained, ready, restoring, uncertain, release_pending;
    SudekiMpLanStoryAvatarCameraIdentity identity;
    uint8_t *mode,*scene,*renderer,*render_state,*camera,*camera_state,*state_data,*manager,*target;
    uint8_t *original[2];
    BOOL original_held[2];
    unsigned installed;
    BOOL view_seeded,view_borrowed,view_published,view_framed;
    float distance;
    SudekiMpLanStoryView original_view,view;
} lease;
static const uint8_t create_entry[]={0x53,0x55,0x8b,0x6c,0x24,0x0c,0x68,0x80,0x00,0x00,0x00};
static const uint8_t install_entry[]={0x53,0x8b,0x5c,0x24,0x0c,0x8b,0x94,0x9e,0xb4,0x00,0x00,0x00};
static const uint8_t notify_entry[]={0x55,0x8b,0xec,0x83,0xe4,0xf0,0xd9,0xee,0x8b,0x45,0x0c};
static const uint8_t release_entry[]={0x53,0x56,0x8b,0x77,0x04,0x33,0xdb,0x32,0xc0};
/* Native initializer 18C430 publishes these singleton states. Their factories
 * 7C280/19E2F0/1A3050 allocate the listed data sizes and write +8 IDs 0/1/2.
 * All three states share the target notification and its supporting virtuals.
 * E79A0 owns camera+3C/+40 and may destroy the preceding state data. */
static const struct { unsigned state_vt,data_vt,data_size; } native_states[]={
    {EXPLORATION_VT,EXPLORATION_DATA_VT,0x550u},
    {0x2d98e4u,0x2db43cu,0x5d0u},
    {0x2d9984u,0x2db73cu,0x5e0u}
};
static const unsigned target_virtuals[]={
    0x1a1350u,0x1a24d0u,0x7bd90u,0x134610u,0x1346b0u,0x134660u,
    0x1346f0u,0x134780u,0x134730u,0x1347c0u,TARGET_NOTIFY_RVA
};

static BOOL fail(DWORD error) { SetLastError(error); return FALSE; }
static BOOL memory(const void *pointer,size_t length,BOOL write) {
    MEMORY_BASIC_INFORMATION m; uintptr_t p=(uintptr_t)pointer;
    if(!p || !length || p>UINTPTR_MAX-length ||
        VirtualQuery(pointer,&m,sizeof(m))!=sizeof(m) || m.State!=MEM_COMMIT ||
        (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
        p+length>(uintptr_t)m.BaseAddress+m.RegionSize) return FALSE;
    DWORD access=m.Protect&0xffu;
    if(write) return access==PAGE_READWRITE || access==PAGE_WRITECOPY ||
        access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
    return access==PAGE_READONLY || access==PAGE_READWRITE || access==PAGE_WRITECOPY ||
        access==PAGE_EXECUTE_READ || access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
}
static BOOL object(const uint8_t *p,size_t size,unsigned vtable,BOOL write) {
    return memory(p,size,write) && *(void *const *)p==base+vtable;
}
static BOOL executable(const void *p) {
    MEMORY_BASIC_INFORMATION m;
    if(VirtualQuery(p,&m,sizeof(m))!=sizeof(m) || m.State!=MEM_COMMIT ||
        (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
    DWORD access=m.Protect&0xffu;
    return access==PAGE_EXECUTE || access==PAGE_EXECUTE_READ ||
        access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
}
static BOOL code_exact(void) {
    return base && executable(base+TARGET_NOTIFY_RVA) &&
        memory(base+TARGET_NOTIFY_RVA,sizeof(notify_entry),FALSE) &&
        memory(base+EXPLORATION_VT+0x40,4,FALSE) &&
        *(void **)(base+EXPLORATION_VT+0x40)==base+TARGET_NOTIFY_RVA &&
        !memcmp(base+TARGET_NOTIFY_RVA,notify_entry,sizeof(notify_entry)) && executable(base+CREATE_RVA) && executable(base+INSTALL_RVA) && executable(base+RELEASE_RVA) &&
        memory(base+CREATE_RVA,sizeof(create_entry),FALSE) &&
        memory(base+INSTALL_RVA,sizeof(install_entry),FALSE) &&
        memory(base+RELEASE_RVA,sizeof(release_entry),FALSE) &&
        !memcmp(base+CREATE_RVA,create_entry,sizeof(create_entry)) &&
        !memcmp(base+INSTALL_RVA,install_entry,sizeof(install_entry)) &&
        !memcmp(base+RELEASE_RVA,release_entry,sizeof(release_entry));
}
static BOOL identity_valid(const SudekiMpLanStoryAvatarCameraIdentity *i) {
    return i && i->actor && i->original_hero && i->world && i->scene_manager &&
        i->session_generation && i->actor_generation && i->world_epoch &&
        i->scene_epoch && i->scene_revision;
}
static BOOL same_identity(const SudekiMpLanStoryAvatarCameraIdentity *a,
    const SudekiMpLanStoryAvatarCameraIdentity *b) {
    return identity_valid(a) && identity_valid(b) && a->actor==b->actor &&
        a->original_hero==b->original_hero && a->world==b->world &&
        a->scene_manager==b->scene_manager && a->session_generation==b->session_generation &&
        a->actor_generation==b->actor_generation && a->world_epoch==b->world_epoch &&
        a->scene_epoch==b->scene_epoch && a->scene_revision==b->scene_revision;
}
static BOOL globals_exact(void) {
    return memory(base+CAMERA_MODE_GLOBAL,4,FALSE) && memory(base+SCENE_GLOBAL,4,FALSE) &&
        memory(base+WORLD_GLOBAL,4,FALSE) && memory(base+MANAGER_GLOBAL,4,FALSE) &&
        memory(base+LIST_OWNER_GLOBAL,4,FALSE);
}
static BOOL owner_core_exact(void) {
    if(!globals_exact() || *(void **)(base+CAMERA_MODE_GLOBAL)!=lease.mode ||
        *(void **)(base+SCENE_GLOBAL)!=lease.scene ||
        *(void **)(base+WORLD_GLOBAL)!=lease.identity.world ||
        *(void **)(base+MANAGER_GLOBAL)!=lease.manager ||
        *(void **)(base+LIST_OWNER_GLOBAL)!=lease.manager ||
        !memory(lease.mode,0x10,FALSE) || *(void **)(lease.mode+0x0c)!=lease.camera+0x2c ||
        !object(lease.scene,0x44,SCENE_VT,FALSE) ||
        *(void **)(lease.scene+0x40)!=lease.renderer || !memory(lease.renderer,0x80,FALSE) ||
        *(void **)(lease.renderer+0x7c)!=lease.render_state ||
        !object(lease.camera,0xbc,CAMERA_VT,TRUE) ||
        *(void **)(lease.camera+0x34)!=lease.render_state ||
        !object(lease.render_state,0xdc,RENDER_STATE_VT,FALSE) ||
        !object(lease.manager,0x60,MANAGER_VT,TRUE) ||
        *(void **)(lease.manager+8)!=base+MANAGER_SECONDARY_VT ||
        *(void **)(lease.manager+12)!=base+MANAGER_THIRD_VT ||
        *(void **)(lease.manager+0x20)!=lease.camera) return FALSE;
    unsigned matches=0;
    for(unsigned n=0;n<10u;++n) matches+=*(void **)(lease.manager+0x24+4*n)==lease.camera;
    return matches==1u && code_exact();
}
static BOOL state_pair_exact(uint8_t *state,uint8_t *data) {
    for(unsigned n=0;n<sizeof(native_states)/sizeof(*native_states);++n) {
        unsigned vt=native_states[n].state_vt;
        if(!object(state,4,vt,FALSE)) continue;
        if(!memory(base+STATE_REGISTRY+4*n,4,FALSE) ||
            *(void **)(base+STATE_REGISTRY+4*n)!=state ||
            !object(data,native_states[n].data_size,native_states[n].data_vt,TRUE) ||
            !*(uint32_t *)(data+4) || *(uint32_t *)(data+4)==UINT32_MAX ||
            *(uint32_t *)(data+8)!=n || *(void **)(data+0x470) ||
            !memory(base+vt+0x18,sizeof(target_virtuals),FALSE)) return FALSE;
        for(unsigned v=0;v<sizeof(target_virtuals)/sizeof(*target_virtuals);++v)
            if(*(void **)(base+vt+0x18+4*v)!=base+target_virtuals[v] ||
                !executable(base+target_virtuals[v])) return FALSE;
        return TRUE;
    }
    return FALSE;
}
static BOOL owner_exact(void) {
    return owner_core_exact() &&
        *(void **)(lease.camera+0x3c)==lease.camera_state &&
        *(void **)(lease.camera+0x40)==lease.state_data &&
        state_pair_exact(lease.camera_state,lease.state_data);
}
/* Bounded, typed, reciprocal lists. Membership is required in addition to
 * readable memory and vtables; a forged/recycled target is not a lease. */
static BOOL target_list(unsigned kind,const void *wanted,BOOL *found) {
    static const unsigned heads[]={0x4c,0x50,0x54};
    static const unsigned sizes[]={0x80,0x34,0xd0};
    static const unsigned vtables[]={MATRIX_VT,GAME_OBJECT_VT,OFFSET_VT};
    static const unsigned next_offsets[]={0x70,0x2c,0xc0};
    static const unsigned prev_offsets[]={0x74,0x30,0xc4};
    if(kind>=3u || !owner_core_exact()) return FALSE;
    const uint8_t *node=*(void **)(lease.manager+heads[kind]),*previous=NULL;
    *found=FALSE;
    for(unsigned n=0;node && n<LIST_LIMIT;++n) {
        if(!object(node,sizes[kind],vtables[kind],FALSE) ||
            *(void *const *)(node+prev_offsets[kind])!=previous) return FALSE;
        if(node==wanted) *found=TRUE;
        previous=node; node=*(void *const *)(node+next_offsets[kind]);
    }
    return node==NULL;
}
static BOOL all_lists_exact(void) {
    BOOL ignored;
    return target_list(0,NULL,&ignored) && target_list(1,NULL,&ignored) && target_list(2,NULL,&ignored);
}
static BOOL target_exact(uint8_t *target,BOOL zero_allowed) {
    if(!memory(target,8,TRUE)) return FALSE;
    unsigned kind=*(void **)target==base+MATRIX_VT?0u:
        *(void **)target==base+GAME_OBJECT_VT?1u:
        *(void **)target==base+OFFSET_VT?2u:3u;
    BOOL found=FALSE;
    return kind<3u && (zero_allowed || *(uint32_t *)(target+4)) &&
        target_list(kind,target,&found) && found;
}
static BOOL matrix_exact(BOOL zero_allowed) {
    return object(lease.target,0x80,MATRIX_VT,TRUE) &&
        *(void **)(lease.target+0x60)==lease.target+0x20 && target_exact(lease.target,zero_allowed);
}
static BOOL avatar_matrix(float matrix[16]) {
    uint8_t *actor=lease.identity.actor;
    if(!memory(actor,0x48,FALSE)) return FALSE;
    uint8_t *position=*(void **)(actor+0x44);
    if(!object(position,0x98,POSITION_VT,FALSE) || *(void **)(position+0x10)!=actor ||
        (*(uintptr_t *)(position+0x94) && *(uintptr_t *)(position+0x94)!=4u)) return FALSE;
    memset(matrix,0,64); matrix[0]=matrix[5]=matrix[10]=matrix[15]=1.0f;
    for(unsigned n=0;n<3;++n) {
        float value=*(float *)(position+0x18+4*n);
        if(!isfinite(value) || fabsf(value)>1000000.0f) return FALSE;
        matrix[12+n]=value;
    }
    return TRUE;
}
static BOOL witness(SudekiMpLanStoryAvatarCameraExact exact,void *context,
    SudekiMpLanStoryAvatarCameraOperation operation) {
    return exact && exact(&lease.identity,operation,context) && owner_exact();
}
static BOOL retained_slots_exact(void) {
    if(!owner_core_exact() || !all_lists_exact() || !matrix_exact(lease.release_pending)) return FALSE;
    unsigned protected_refs=lease.release_pending?0u:1u;
    for(unsigned n=0;n<2;++n) {
        uint8_t *expected=(lease.installed&(1u<<n))?lease.target:lease.original[n];
        if(*(void **)(lease.camera+0xb4+4*n)!=expected || !target_exact(expected,FALSE) ||
            (lease.original_held[n] && !target_exact(lease.original[n],FALSE))) return FALSE;
        if(expected==lease.target) ++protected_refs;
        if(lease.original_held[n]) {
            unsigned original_refs=0;
            for(unsigned k=0;k<2;++k) {
                original_refs+=lease.original_held[k] && lease.original[k]==lease.original[n];
                original_refs+=*(void **)(lease.camera+0xb4+4*k)==lease.original[n];
            }
            if(*(uint32_t *)(lease.original[n]+4)<original_refs) return FALSE;
        }
    }
    return *(uint32_t *)(lease.target+4)>=protected_refs;
}
/* Observe a completed native host transition; never call the state setter or
 * inspect the retired tuple. Paused-client view publication pins its tuple.
 * Two independent caller witnesses bracket every candidate observation. */
static BOOL adopt_native_state(SudekiMpLanStoryAvatarCameraExact exact,void *context,
    SudekiMpLanStoryAvatarCameraOperation operation) {
    if(owner_exact()) return TRUE;
    if(!exact || lease.uncertain || lease.view_borrowed ||
        (operation!=SUDEKIMP_AVATAR_CAMERA_UPDATE &&
         operation!=SUDEKIMP_AVATAR_CAMERA_NATIVE_DIRECTION &&
         operation!=SUDEKIMP_AVATAR_CAMERA_RESTORE) ||
        !exact(&lease.identity,SUDEKIMP_AVATAR_CAMERA_NATIVE_TRANSITION,context) ||
        !exact(&lease.identity,operation,context) || !retained_slots_exact()) return FALSE;
    uint8_t *state=*(void **)(lease.camera+0x3c),*data=*(void **)(lease.camera+0x40);
    if((state==lease.camera_state && data==lease.state_data) || !state_pair_exact(state,data) ||
        !exact(&lease.identity,SUDEKIMP_AVATAR_CAMERA_NATIVE_TRANSITION,context) ||
        !exact(&lease.identity,operation,context) || !retained_slots_exact() ||
        *(void **)(lease.camera+0x3c)!=state || *(void **)(lease.camera+0x40)!=data ||
        !state_pair_exact(state,data)) return FALSE;
    lease.camera_state=state; lease.state_data=data;
    return TRUE;
}
static BOOL retain_target(uint8_t *target) {
    if(!owner_exact() || !target_exact(target,FALSE) || *(uint32_t *)(target+4)==UINT32_MAX) return FALSE;
    ++*(uint32_t *)(target+4); return TRUE;
}
/* Native installer consumes the retained argument. Extra native camera-state
 * references may exist: validate our protected ownership lower bounds rather
 * than assuming its notification callback has no reference-count effects. */
static BOOL replace_slot(unsigned slot,uint8_t *from,uint8_t *to) {
    if(!owner_exact() || !all_lists_exact() || slot>=2u || *(void **)(lease.camera+0xb4+4*slot)!=from ||
        !target_exact(from,FALSE) || !target_exact(to,FALSE)) return FALSE;
    uint32_t from_refs=*(uint32_t *)(from+4),to_refs=*(uint32_t *)(to+4);
    if(from_refs<2u || !retain_target(to)) return FALSE;
    SudekiMpCallCameraTargetInstall(lease.camera,to,slot,base+INSTALL_RVA);
    if(owner_exact() && target_exact(from,FALSE) && target_exact(to,FALSE)) {
        if(*(void **)(lease.camera+0xb4+4*slot)==to && *(uint32_t *)(to+4)>=2u)
            return TRUE;
        /* Proven unconsumed no-op: release only the temporary retain. Any
         * other postcondition is unknown and requires a destruction witness. */
        if(*(void **)(lease.camera+0xb4+4*slot)==from &&
            *(uint32_t *)(from+4)==from_refs && *(uint32_t *)(to+4)==to_refs+1u) {
            --*(uint32_t *)(to+4); return FALSE;
        }
    }
    lease.uncertain=TRUE; return FALSE;
}
static BOOL read_view(SudekiMpLanStoryView *out) {
    if(!owner_exact()) return FALSE;
    memset(out,0,sizeof(*out)); out->valid=1;
    memcpy(out->matrix,lease.render_state+0x90,sizeof(out->matrix));
    memcpy(out->projection,lease.render_state+0xd0,sizeof(out->projection));
    return SudekiMpLanStoryViewGeometryValid(out);
}
static BOOL view_matches(const SudekiMpLanStoryView *expected) {
    SudekiMpLanStoryView current;
    return read_view(&current) && !memcmp(current.matrix,expected->matrix,sizeof(current.matrix)) &&
        !memcmp(current.projection,expected->projection,sizeof(current.projection));
}
static BOOL write_view(const SudekiMpLanStoryView *view) {
    if(!owner_exact() || !memory(lease.render_state,0xdc,TRUE) ||
        !SudekiMpLanStoryViewGeometryValid(view)) return FALSE;
    memcpy(lease.render_state+0x90,view->matrix,sizeof(view->matrix));
    memcpy(lease.render_state+0xd0,view->projection,sizeof(view->projection));
    ++*(uint16_t *)(lease.render_state+0x2c);
    return view_matches(view);
}
static BOOL restore_inner(SudekiMpLanStoryAvatarCameraExact exact,void *context) {
    lease.ready=FALSE; lease.restoring=TRUE;
    if(lease.uncertain || !adopt_native_state(exact,context,SUDEKIMP_AVATAR_CAMERA_RESTORE) ||
        !witness(exact,context,SUDEKIMP_AVATAR_CAMERA_RESTORE)) return FALSE;
    if(lease.view_borrowed) {
        if(!view_matches(&lease.view) || !write_view(&lease.original_view)) return FALSE;
        lease.view_borrowed=lease.view_published=FALSE;
    }
    for(unsigned n=2u;n>0;--n) {
        unsigned slot=n-1u;
        if(!lease.original_held[slot]) continue;
        void *current=*(void **)(lease.camera+0xb4+4*slot);
        if(lease.installed&(1u<<slot)) {
            if(current!=lease.target || !matrix_exact(FALSE) ||
                !witness(exact,context,SUDEKIMP_AVATAR_CAMERA_RESTORE) ||
                !replace_slot(slot,lease.target,lease.original[slot])) return FALSE;
            lease.installed&=~(1u<<slot);
        } else if(current!=lease.original[slot]) return FALSE;
    }
    if(!witness(exact,context,SUDEKIMP_AVATAR_CAMERA_RESTORE)) return FALSE;
    /* Keep both originals until both slots restore. Aliased originals have
     * two saved references, matching their two native slot references. */
    for(unsigned n=0;n<2;++n) if(lease.original_held[n]) {
        if(*(void **)(lease.camera+0xb4+4*n)!=lease.original[n] ||
            !target_exact(lease.original[n],FALSE) || *(uint32_t *)(lease.original[n]+4)<2u) return FALSE;
        --*(uint32_t *)(lease.original[n]+4); lease.original_held[n]=FALSE;
    }
    if(lease.target) {
        if(!matrix_exact(lease.release_pending)) return FALSE;
        uint32_t *refs=(uint32_t *)(lease.target+4);
        if(!lease.release_pending) {
            if(!*refs) return FALSE;
            --*refs;
            if(*refs) { lease.target=NULL; memset(&lease,0,sizeof(lease)); return TRUE; }
            lease.release_pending=TRUE;
        }
        /* Never touch the target after native destruction. A bounded walk
         * must positively observe removal before forgetting the pointer. */
        if(!all_lists_exact() || !witness(exact,context,SUDEKIMP_AVATAR_CAMERA_RESTORE)) return FALSE;
        SudekiMpCallCameraTargetRelease(lease.manager+0x4c,lease.target,base+RELEASE_RVA);
        BOOL found=TRUE;
        if(!target_list(0,lease.target,&found) || found) return FALSE;
    }
    memset(&lease,0,sizeof(lease)); return TRUE;
}
static BOOL begin(const SudekiMpLanStoryAvatarCameraIdentity *identity,BOOL retained) {
    if(!base || busy || (native_thread && native_thread!=GetCurrentThreadId()) ||
        !identity_valid(identity) || (retained && (!lease.retained || !same_identity(identity,&lease.identity))))
        return fail(ERROR_BUSY);
    busy=TRUE; return TRUE;
}
BOOL SudekiMpLanStoryAvatarCameraInstall(HMODULE image) {
    if(base || busy || lease.retained || !image || !SudekiMpCheckLoadedExecutable(image)) return fail(ERROR_BUSY);
    base=(uint8_t *)image;
    if(!code_exact()) { base=NULL; return fail(ERROR_INVALID_DATA); }
    native_create=(MatrixCreate)(base+CREATE_RVA);
    return TRUE;
}
BOOL SudekiMpLanStoryAvatarCameraBind(const SudekiMpLanStoryAvatarCameraIdentity *identity,
    SudekiMpLanStoryAvatarCameraExact exact,void *context) {
    if(lease.retained || !begin(identity,FALSE)) return fail(ERROR_BUSY);
    BOOL result=FALSE;
    if(!exact || !exact(identity,SUDEKIMP_AVATAR_CAMERA_BIND,context) || !globals_exact()) goto done;
    lease.identity=*identity;
    lease.mode=*(void **)(base+CAMERA_MODE_GLOBAL); lease.scene=identity->scene_manager;
    lease.manager=*(void **)(base+MANAGER_GLOBAL);
    if(!memory(lease.mode,0x10,FALSE) || (uintptr_t)*(void **)(lease.mode+0xc)<0x2cu ||
        !object(lease.scene,0x44,SCENE_VT,FALSE)) goto done;
    lease.camera=*(uint8_t **)(lease.mode+0xc)-0x2c;
    if(!object(lease.camera,0xbc,CAMERA_VT,TRUE)) goto done;
    lease.render_state=*(void **)(lease.camera+0x34); lease.renderer=*(void **)(lease.scene+0x40);
    lease.camera_state=*(void **)(lease.camera+0x3c); lease.state_data=*(void **)(lease.camera+0x40);
    if(!object(lease.camera_state,4,EXPLORATION_VT,FALSE) || !owner_exact()) goto done;
    lease.original[0]=*(void **)(lease.camera+0xb4); lease.original[1]=*(void **)(lease.camera+0xb8);
    uint8_t *a=lease.original[0],*b=lease.original[1];
    if(!object(b,0x34,GAME_OBJECT_VT,TRUE) || *(void **)(b+0x20)!=identity->original_hero ||
        !target_exact(b,FALSE) || !target_exact(a,FALSE) ||
        (a!=b && (!object(a,0xd0,OFFSET_VT,TRUE) || *(void **)(a+0x20)!=b))) goto done;
    float matrix[16];
    if(!avatar_matrix(matrix) || !all_lists_exact() ||
        !witness(exact,context,SUDEKIMP_AVATAR_CAMERA_BIND)) goto done;
    lease.view_seeded=read_view(&lease.original_view);
    lease.view=lease.original_view;
    native_thread=GetCurrentThreadId(); lease.retained=TRUE;
    for(unsigned n=0;n<2;++n) {
        if(!retain_target(lease.original[n])) goto rollback;
        lease.original_held[n]=TRUE;
    }
    native_create(lease.manager+0x4c,(void **)&lease.target,matrix);
    if(!lease.target) goto rollback;
    if(!matrix_exact(FALSE) || *(uint32_t *)(lease.target+4)!=1u ||
        memcmp(lease.target+0x20,matrix,sizeof(matrix))) {
        lease.uncertain=TRUE; goto done;
    }
    if(!witness(exact,context,SUDEKIMP_AVATAR_CAMERA_BIND)) goto rollback;
    for(unsigned n=0;n<2;++n) {
        if(!witness(exact,context,SUDEKIMP_AVATAR_CAMERA_BIND) ||
            !replace_slot(n,lease.original[n],lease.target)) goto rollback;
        lease.installed|=1u<<n;
    }
    /* Exploration's target notification legitimately rewrites render-state
     * translation (134610 via 7BB90). Keep the pre-bind view for restoration,
     * but lease the freshly observed post-install view for the first Present. */
    lease.view_seeded=lease.view_seeded && read_view(&lease.view);
    lease.ready=TRUE; result=TRUE; goto done;
rollback:
    (void)restore_inner(exact,context);
done:
    if(!lease.retained) memset(&lease,0,sizeof(lease));
    busy=FALSE; if(!result) return fail(ERROR_RETRY); return TRUE;
}
BOOL SudekiMpLanStoryAvatarCameraUpdate(const SudekiMpLanStoryAvatarCameraIdentity *identity,
    SudekiMpLanStoryAvatarCameraExact exact,void *context) {
    if(!begin(identity,TRUE)) return FALSE;
    float matrix[16]; BOOL result=FALSE;
    if(lease.ready && !lease.restoring && !lease.uncertain && lease.installed==3u &&
        adopt_native_state(exact,context,SUDEKIMP_AVATAR_CAMERA_UPDATE) &&
        witness(exact,context,SUDEKIMP_AVATAR_CAMERA_UPDATE) && matrix_exact(FALSE) &&
        *(void **)(lease.camera+0xb4)==lease.target && *(void **)(lease.camera+0xb8)==lease.target &&
        avatar_matrix(matrix) && witness(exact,context,SUDEKIMP_AVATAR_CAMERA_UPDATE)) {
        memcpy(lease.target+0x20,matrix,sizeof(matrix));
        memcpy(lease.target+0x14,matrix+12,12);
        result=TRUE;
    }
    busy=FALSE; return result?TRUE:fail(ERROR_RETRY);
}
static BOOL avatar_readable(const void *p,size_t n) { return memory(p,n,FALSE); }
/* Retail 4E8D50 -> 518580/518850: camera+38 is a resource handle, not
 * the config payload. Float fields are packed according to the 207-bit
 * presence mask; a missing field inherits through payload+4. Read only. */
static BOOL camera_setting(unsigned index,float *out) {
    if(index<2u || index>=200u) return FALSE;
    uint8_t *camera=lease.camera,*handle=*(uint8_t **)(camera+0x38u);
    void *seen[16]; unsigned count=0;
    while(handle && count<16u) {
        if(!avatar_readable(handle,0xcu)) return FALSE;
        uint8_t *config=*(uint8_t **)(handle+8u);
        if(!avatar_readable(config,0x48u)) return FALSE;
        for(unsigned i=0;i<count;++i) if(seen[i]==config) return FALSE;
        seen[count++]=config;
        unsigned size=*(unsigned *)config,offset=0,total=0;
        uint32_t *mask=(uint32_t *)(config+8u);
        if(mask[6]&0xffff8000u) return FALSE;
        for(unsigned i=0;i<207u;++i) if(mask[i/32u]&(1u<<(i%32u))) {
            unsigned bytes=i<200u?4u:12u;
            total+=bytes; if(i<index) offset+=bytes;
        }
        uint8_t *values=*(uint8_t **)(config+0x44u);
        if(size!=0x48u+total || values!=config+0x48u || !avatar_readable(config,size)) return FALSE;
        if(mask[index/32u]&(1u<<(index%32u))) {
            float value=*(float *)(values+offset);
            if(!isfinite(value)) return FALSE;
            *out=value; return TRUE;
        }
        handle=*(uint8_t **)(config+4u);
    }
    return FALSE;
}
static float clamp(float x,float lo,float hi) {return x<lo?lo:x>hi?hi:x;}
static BOOL frame_camera(SudekiMpLanStoryView *next,const float position[3],
    float yaw,float vertical,float *distance_out) {
    float minimum,maximum,initial,scale,at_target,near_distance,near_height,far_height,lookat;
    float rot_min,rot_max,rot_scale;
    if(!camera_setting(6u,&lookat) || !camera_setting(7u,&minimum) ||
        !camera_setting(8u,&maximum) || !camera_setting(11u,&initial) ||
        !camera_setting(12u,&rot_min) || !camera_setting(13u,&rot_max) ||
        !camera_setting(18u,&rot_scale) ||
        !camera_setting(26u,&scale) || !camera_setting(27u,&at_target) ||
        !camera_setting(28u,&near_distance) || !camera_setting(29u,&near_height) ||
        !camera_setting(30u,&far_height) || minimum<0.1f || maximum<=minimum ||
        maximum>30.0f || initial<minimum || initial>maximum || scale<0 || scale>100.0f ||
        near_distance<=0 || near_distance>=maximum || fabsf(lookat)>5.0f ||
        fabsf(at_target)>20 || fabsf(near_height)>20 || fabsf(far_height)>20 ||
        rot_min<0 || rot_max<0 || rot_min>360 || rot_max>360 || fabsf(rot_scale)>10) return FALSE;
    /* Both native target slots are our MatrixTarget. No hero target offset
     * or stale source observation participates in this avatar's framing. */
    float anchor[3]; memcpy(anchor,position,sizeof(anchor));
    anchor[1]+=lookat;
    /* 47CBB0 changes flat distance, clamped to the authored limits.
     * Input supplies sensitivity-scaled axis seconds, not a pitch angle. */
    float distance=clamp((lease.view_framed?lease.distance:initial)-vertical*scale,minimum,maximum);
    /* 47C2C0/47CCD0 interpolate native degrees per second with distance. */
    yaw=clamp(yaw*rot_scale*(rot_min+(rot_max-rot_min)*
        clamp((distance-minimum)/(maximum-minimum),0,1))*0.01745329252f,-0.5f,0.5f);
    float height=distance<near_distance?
        at_target+(near_height-at_target)*clamp(distance/near_distance,0,1):
        near_height+(far_height-near_height)*clamp((distance-near_distance)/(maximum-near_distance),0,1);
    /* Preserve heading only. The old seed's arbitrary lateral offset and
     * pitch must not survive the spectator -> own-character handoff. */
    float fx=next->matrix[8],fz=next->matrix[10],flat=sqrtf(fx*fx+fz*fz);
    if(!isfinite(flat) || flat<0.001f) return FALSE;
    fx/=flat; fz/=flat;
    float sine=sinf(yaw),cosine=cosf(yaw),x=fx*cosine+fz*sine;
    fz=fz*cosine-fx*sine; fx=x;
    float eye[3]={position[0]-fx*distance,position[1]+height,position[2]-fz*distance};
    float forward[3]={anchor[0]-eye[0],anchor[1]-eye[1],anchor[2]-eye[2]};
    float length=sqrtf(forward[0]*forward[0]+forward[1]*forward[1]+forward[2]*forward[2]);
    if(!isfinite(length) || length<0.1f) return FALSE;
    for(unsigned i=0;i<3u;++i) forward[i]/=length;
    flat=sqrtf(forward[0]*forward[0]+forward[2]*forward[2]);
    if(flat<0.001f) return FALSE;
    float right[3]={-forward[2]/flat,0,forward[0]/flat};
    float up[3]={-right[2]*forward[1],right[2]*forward[0]-right[0]*forward[2],right[0]*forward[1]};
    memset(next->matrix,0,sizeof(next->matrix));
    memcpy(next->matrix,right,12u); memcpy(next->matrix+4u,up,12u);
    memcpy(next->matrix+8u,forward,12u); memcpy(next->matrix+12u,eye,12u); next->matrix[15]=1;
    *distance_out=distance;
    return SudekiMpLanStoryViewGeometryValid(next);
}
static BOOL own_targets(void) {
    return lease.ready && !lease.restoring && !lease.uncertain && lease.installed==3u &&
        owner_exact() && matrix_exact(FALSE) &&
        *(void **)(lease.camera+0xb4)==lease.target && *(void **)(lease.camera+0xb8)==lease.target;
}
BOOL SudekiMpLanStoryAvatarCameraPresent(const SudekiMpLanStoryAvatarCameraIdentity *identity,
    SudekiMpLanStoryAvatarCameraExact exact,void *context,float horizontal,float vertical) {
    if(!isfinite(horizontal) || !isfinite(vertical) || fabsf(horizontal)>0.5f || fabsf(vertical)>0.5f)
        return fail(ERROR_INVALID_PARAMETER);
    if(!begin(identity,TRUE)) return FALSE;
    BOOL result=FALSE; float anchor_matrix[16],distance;
    SudekiMpLanStoryView next=lease.view;
    if(lease.view_seeded && own_targets() &&
        witness(exact,context,SUDEKIMP_AVATAR_CAMERA_PRESENT) && view_matches(&lease.view) &&
        avatar_matrix(anchor_matrix) && frame_camera(&next,anchor_matrix+12,horizontal,vertical,&distance) &&
        witness(exact,context,SUDEKIMP_AVATAR_CAMERA_PRESENT) && own_targets() && view_matches(&lease.view)) {
        /* Mark the lease before the write so a failed postcondition cannot
         * discard the original native geometry or permit a new view owner. */
        lease.view_borrowed=TRUE;
        if(write_view(&next)) {
            lease.view=next; lease.distance=distance; lease.view_framed=TRUE;
            memcpy(lease.target+0x20,anchor_matrix,sizeof(anchor_matrix));
            memcpy(lease.target+0x14,anchor_matrix+12,12);
            result=witness(exact,context,SUDEKIMP_AVATAR_CAMERA_PRESENT) && own_targets() && view_matches(&lease.view);
            lease.view_published=result;
        } else lease.uncertain=TRUE;
    }
    busy=FALSE; return result?TRUE:fail(ERROR_RETRY);
}
BOOL SudekiMpLanStoryAvatarCameraDirection(const SudekiMpLanStoryAvatarCameraIdentity *identity,
    SudekiMpLanStoryAvatarCameraExact exact,void *context,float x,float z,float *out_x,float *out_z) {
    if(out_x) *out_x=0;
    if(out_z) *out_z=0;
    if(!out_x || !out_z || !isfinite(x) || !isfinite(z) || fabsf(x)>1.001f || fabsf(z)>1.001f)
        return fail(ERROR_INVALID_PARAMETER);
    if(!begin(identity,TRUE)) return FALSE;
    BOOL result=FALSE; float magnitude=sqrtf(x*x+z*z),local[3]={x,0,z},world[3];
    if(lease.view_published && own_targets() && view_matches(&lease.view) &&
        witness(exact,context,SUDEKIMP_AVATAR_CAMERA_DIRECTION)) {
        if(magnitude<0.0001f) result=TRUE;
        else if(SudekiMpCameraTransformHorizontalDirection(lease.view.matrix,local,world) &&
            witness(exact,context,SUDEKIMP_AVATAR_CAMERA_DIRECTION) && own_targets() && view_matches(&lease.view)) {
            if(magnitude>1.0f) magnitude=1.0f;
            *out_x=world[0]*magnitude; *out_z=world[2]*magnitude; result=TRUE;
        }
    }
    busy=FALSE; return result?TRUE:fail(ERROR_RETRY);
}
BOOL SudekiMpLanStoryAvatarCameraNativeDirection(const SudekiMpLanStoryAvatarCameraIdentity *identity,
    SudekiMpLanStoryAvatarCameraExact exact,void *context,float x,float z,float *out_x,float *out_z) {
    if(out_x) *out_x=0;
    if(out_z) *out_z=0;
    if(!out_x || !out_z || !isfinite(x) || !isfinite(z) || fabsf(x)>1.001f || fabsf(z)>1.001f)
        return fail(ERROR_INVALID_PARAMETER);
    if(!begin(identity,TRUE)) return FALSE;
    BOOL result=FALSE; float magnitude=sqrtf(x*x+z*z),local[3]={x,0,z},world[3],anchor[16];
    SudekiMpLanStoryView current;
    if(!lease.view_borrowed && adopt_native_state(exact,context,SUDEKIMP_AVATAR_CAMERA_NATIVE_DIRECTION) && own_targets() &&
        witness(exact,context,SUDEKIMP_AVATAR_CAMERA_NATIVE_DIRECTION) &&
        avatar_matrix(anchor) && read_view(&current)) {
        if(magnitude<0.0001f) result=TRUE;
        else if(SudekiMpCameraTransformHorizontalDirection(current.matrix,local,world) &&
            witness(exact,context,SUDEKIMP_AVATAR_CAMERA_NATIVE_DIRECTION) &&
            own_targets() && view_matches(&current)) {
            if(magnitude>1.0f) magnitude=1.0f;
            *out_x=world[0]*magnitude; *out_z=world[2]*magnitude; result=TRUE;
        }
    }
    busy=FALSE; return result?TRUE:fail(ERROR_RETRY);
}
BOOL SudekiMpLanStoryAvatarCameraRestore(const SudekiMpLanStoryAvatarCameraIdentity *identity,
    SudekiMpLanStoryAvatarCameraExact exact,void *context) {
    if(!begin(identity,TRUE)) return FALSE;
    BOOL result=restore_inner(exact,context); busy=FALSE;
    return result?TRUE:fail(ERROR_RETRY);
}
BOOL SudekiMpLanStoryAvatarCameraRetains(void) { return lease.retained; }
BOOL SudekiMpLanStoryAvatarCameraNativeExitReturned(const SudekiMpLanStoryAvatarCameraIdentity *identity,
    SudekiMpLanStoryAvatarCameraExact destroyed,void *context) {
    if(!begin(identity,TRUE)) return FALSE;
    BOOL result=destroyed && destroyed(&lease.identity,SUDEKIMP_AVATAR_CAMERA_WORLD_DESTROYED,context);
    if(result) memset(&lease,0,sizeof(lease));
    busy=FALSE; return result?TRUE:fail(ERROR_RETRY);
}
BOOL SudekiMpLanStoryAvatarCameraUninstall(void) {
    if(busy || lease.retained || (native_thread && native_thread!=GetCurrentThreadId())) return fail(ERROR_BUSY);
    base=NULL; native_create=NULL; native_thread=0; return TRUE;
}
