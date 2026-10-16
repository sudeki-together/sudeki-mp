/* Synthetic lease/transition regression; optional supported-image argument
 * adds IsShopActive byte-guard proof. No retail UI code or gameplay runs. */
#include "../src/hooks/lan_story_host_control.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static BOOL roster_exact=TRUE,filter_exact=TRUE;
static BOOL witness_exact=TRUE,split_exact=TRUE,returned_exact=FALSE;
static BOOL approach_exact;
static SudekiMpLanStoryInteriorTravel approach;
BOOL SudekiMpLanStoryTaskInteriorTravel(SudekiMpLanStoryInteriorTravel *out) {
    if(!approach_exact) return FALSE;
    *out=approach;return TRUE;
}
static uint8_t owned_mask;
static unsigned none_calls,all_calls;
static uint8_t controller[0x250],quick[0x2a],quit[0x1c3],speed[0x2c];
static uint8_t actors[4][0x98],ais[4][0x174],modes[4][0xc],frontend[0xc0],shop[0x8c];
static SudekiMpLanStoryNativeRoster exterior_roster;
static SudekiMpLanStoryScene exterior_scene;
void SudekiMpLogFormat(const char *format,...) { (void)format; }
unsigned SudekiMpSpiritInstanceFilterEntryDiag(HMODULE image,unsigned which) {
    (void)image; (void)which; return 0;
}
unsigned SudekiMpSpiritInstanceFaultSite(void) { return 0; }
unsigned SudekiMpSpiritInstanceEnterFailure(void) { return 0; }
unsigned SudekiMpSpiritInstanceNamedFail(void) { return 0; }
unsigned SudekiMpCastLightReadyFailure(void) { return 0; }
unsigned SudekiMpCastLightFaultSite(void) { return 0; }
unsigned SudekiMpCastLightFaultLine(void) { return 0; }
BOOL SudekiMpSpiritInstanceFilterNoneEntryExact(HMODULE image) {
    return image==(HMODULE)base && filter_exact;
}
BOOL SudekiMpSpiritInstanceFilterAllEntryExact(HMODULE image) {
    return image==(HMODULE)base && filter_exact;
}
BOOL SudekiMpLanStoryObserverRosterStillExact(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r) {
    return roster_exact && w && r && r->controller==controller;
}
BOOL SudekiMpControlSeparationUpdateDispatchWitnessStillExact(
    const SudekiMpControlUpdateDispatchWitness *w) {
    return witness_exact && w && w->dispatch_serial==exterior_roster.dispatch_serial;
}
BOOL SudekiMpLanStoryObserverRoster(void *c,const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryScene *scene,SudekiMpLanStoryNativeRoster *r) {
    if(!roster_exact || c!=controller || scene!=&exterior_scene ||
        !SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w)) return FALSE;
    *r=exterior_roster; return TRUE;
}
BOOL SudekiMpLanStoryObserverNativeRosterExact(const SudekiMpLanStoryNativeRoster *r) {
    return roster_exact && r && !memcmp(r,&exterior_roster,sizeof(*r));
}
BOOL SudekiMpLanStorySplitExteriorActorExact(const SudekiMpLanStoryNativeRoster *r,unsigned c) {
    return split_exact && SudekiMpLanStoryObserverNativeRosterExact(r) && c<4u &&
        (r->available_mask&(1u<<c)) && c!=r->leader_character;
}
BOOL SudekiMpLanStorySplitReturnedActorExact(const SudekiMpLanStoryNativeRoster *r,unsigned c) {
    return returned_exact && SudekiMpLanStoryObserverNativeRosterExact(r) && c<4u &&
        (r->available_mask&(1u<<c)) && c!=r->leader_character;
}
BOOL SudekiMpLanPartyControlStoryTurnOwner(void *actor,SudekiMpLanPartyLease *key) {
    for(unsigned c=0;c<4u;++c) if((owned_mask&(1u<<c)) && actor==actors[c] &&
        *(int16_t *)(ais[c]+0x16a)==1 && modes[c][0xb]==0) {
        *key=(SudekiMpLanPartyLease){.token=1,.generation=1,.seat=(uint8_t)c};
        return TRUE;
    }
    return FALSE;
}
BOOL SudekiMpLanStoryControlActorOwned(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r,unsigned c) {
    SudekiMpLanPartyLease key;
    return c<4u && SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w) &&
        SudekiMpLanStoryObserverNativeRosterExact(r) &&
        SudekiMpLanPartyControlStoryTurnOwner(r->actors[c],&key) && key.seat==c;
}
static void __attribute__((thiscall)) native_none(void *c) {
    assert(c==controller); ++none_calls;
    *(int *)(controller+0x80)=*(int *)(controller+0x84)=0;
}
static void __attribute__((thiscall)) native_all(void *c) {
    assert(c==controller); ++all_calls;
    *(int *)(controller+0x84)=1; /* native application may be asynchronous */
}
static void filters(int current,int pending) {
    *(int *)(controller+0x80)=current; *(int *)(controller+0x84)=pending;
}
static void jump(unsigned offset,Filter fn) {
    base[offset]=0xe9;
    int32_t displacement=(int32_t)((uintptr_t)fn-(uintptr_t)(base+offset+5));
    memcpy(base+offset+1,&displacement,4);
}
static void exterior_fixture(void) {
    memset(&input_owner,0,sizeof(input_owner)); memset(&rotation,0,sizeof(rotation));
    memset(&exterior_roster,0,sizeof(exterior_roster));
    memset(controller,0,sizeof(controller)); memset(actors,0,sizeof(actors));
    memset(ais,0,sizeof(ais)); memset(modes,0,sizeof(modes));
    memset(frontend,0,sizeof(frontend)); memset(shop,0,sizeof(shop));
    stopping=active=FALSE; requested=4; locked=3; native_thread=GetCurrentThreadId();
    roster_exact=filter_exact=witness_exact=split_exact=TRUE; returned_exact=approach_exact=FALSE; owned_mask=4;
    exterior_roster=(SudekiMpLanStoryNativeRoster){.controller=controller,.world=(void *)1,
        .descriptor=(void *)2,.group=(void *)3,.dispatch_serial=42,.epoch=6,.revision=6,
        .leader_character=3,.available_mask=12};
    *(void **)controller=base+CONTROLLER_VT; *(void **)(controller+0x2c)=base+INPUT_VT;
    for(unsigned c=0;c<4u;++c) {
        exterior_roster.actors[c]=actors[c]; exterior_roster.ai[c]=ais[c];
        *(void **)(actors[c]+0x94)=ais[c]; *(void **)ais[c]=base+0x2d4924;
        *(void **)(ais[c]+0x10)=actors[c]; *(void **)(ais[c]+0x3c)=modes[c];
        modes[c][0xb]=(c==locked || (owned_mask&(1u<<c)))?0:1;
        *(int16_t *)(ais[c]+0x16a)=(owned_mask&(1u<<c))?1:0;
    }
    *(void **)(controller+0x248)=actors[3]; filters(1,1);
    base[0x8d1c0]=0xa1; *(void **)(base+0x8d1c1)=base+0x3c2f88;
    memcpy(base+0x8d1c5,shop_active_tail,sizeof(shop_active_tail));
    *(void **)(base+0x3c2f88)=frontend; *(void **)(base+0x3c2f70)=shop;
    *(void **)frontend=base+0x2caf9c; *(void **)(frontend+0x74)=shop;
    *(void **)shop=base+0x2cabb4; *(unsigned *)(frontend+0xb8)=1;
}
static void shop_fixture(void) {
    exterior_fixture(); *(int16_t *)(ais[3]+0x16a)=1; filters(2,2);
    *(unsigned *)(frontend+0xb8)=8; *(int *)(shop+0x88)=7;
}
static BOOL exterior_bound(void) {
    return SudekiMpLanStoryHostControlExteriorNativeBound(&exterior_roster,2);
}
static BOOL exterior_ready(void) {
    return SudekiMpLanStoryHostControlExteriorNativeReady(&exterior_roster,2);
}
static BOOL exterior_steering(void) {
    return SudekiMpLanStoryHostControlExteriorNativeSteeringReady(&exterior_roster,2);
}
static void test_exterior_independent_lease(void) {
    unsigned calls=none_calls+all_calls;
    exterior_fixture(); assert(exterior_bound() && exterior_ready());
    SudekiMpControlUpdateDispatchWitness w={.service_only=TRUE,
        .service_post_original_exact=TRUE,.dispatch_serial=42};
    assert(SudekiMpLanStoryHostControlBound(controller,&w,&exterior_scene));
    assert(SudekiMpLanStoryHostControlReady(controller,&w,&exterior_scene));
    /* Native interaction opening retains the already held exterior actor.
     * Unknown modal ownership does not grant actions or alter filters. */
    *(int16_t *)(ais[3]+0x16a)=1; filters(2,2);
    assert(exterior_bound() && !exterior_ready());
    assert(!SudekiMpLanStoryHostControlBound(controller,&w,&exterior_scene));
    assert(!SudekiMpLanStoryHostControlReady(controller,&w,&exterior_scene));
    assert(SudekiMpLanStoryHostControlExteriorBound(controller,&w,&exterior_scene,2));
    assert(!SudekiMpLanStoryHostControlExteriorReady(controller,&w,&exterior_scene,2));
    /* Positive typed native shop state enables this exterior actor only;
     * the unchanged global host binding remains false throughout shopping. */
    shop_fixture(); assert(exterior_bound() && exterior_ready());
    assert(SudekiMpLanStoryHostControlExteriorReady(controller,&w,&exterior_scene,2));
    assert(!SudekiMpLanStoryHostControlBound(controller,&w,&exterior_scene));
    assert(!SudekiMpLanStoryHostControlExteriorNativeReady(&exterior_roster,3));
    assert(!SudekiMpLanStoryHostControlExteriorNativeBound(&exterior_roster,4));
    *(int16_t *)(ais[3]+0x16a)=2; assert(exterior_bound() && !exterior_ready());
    *(int16_t *)(ais[3]+0x16a)=-1; assert(exterior_bound() && !exterior_ready());
    shop_fixture(); filters(2,1); assert(exterior_bound() && !exterior_ready());
    filters(1,2); assert(exterior_bound() && !exterior_ready());
    filters(0,0); assert(exterior_bound() && !exterior_ready());
    shop_fixture(); *(int *)(shop+0x88)=0; assert(exterior_bound() && !exterior_ready());
    /* Native IsShopActive also positively identifies its opening layer 7. */
    *(unsigned *)(frontend+0xb8)=7; assert(exterior_ready());
    shop_fixture(); *(void **)(frontend+0x74)=NULL; assert(!exterior_ready());
    shop_fixture(); *(void **)(base+0x3c2f70)=NULL; assert(!exterior_ready());
    shop_fixture(); *(void **)frontend=base; assert(!exterior_ready());
    shop_fixture(); *(void **)shop=base; assert(!exterior_ready());
    shop_fixture(); base[0x8d1c5]^=1; assert(!exterior_ready());
    shop_fixture(); *(void **)(base+0x8d1c1)=base; assert(!exterior_ready());
    /* A copied roster or remembered true result never grants a later frame. */
    shop_fixture(); split_exact=FALSE; assert(!exterior_bound() && !exterior_ready());
    shop_fixture(); roster_exact=FALSE; assert(!exterior_bound());
    shop_fixture(); owned_mask=0; assert(!exterior_bound());
    shop_fixture(); *(int16_t *)(ais[2]+0x16a)=2; assert(!exterior_bound());
    shop_fixture(); modes[2][0xb]=1; assert(!exterior_bound());
    shop_fixture(); modes[3][0xb]=1; assert(!exterior_bound());
    shop_fixture(); *(void **)(actors[3]+0x94)=ais[2]; assert(!exterior_bound());
    shop_fixture(); *(void **)ais[3]=base; assert(!exterior_bound());
    shop_fixture(); *(void **)(ais[3]+0x10)=actors[2]; assert(!exterior_bound());
    shop_fixture(); *(void **)(ais[3]+0x3c)=NULL; assert(!exterior_bound());
    shop_fixture(); *(void **)controller=base; assert(!exterior_bound());
    shop_fixture(); *(void **)(controller+0x2c)=base; assert(!exterior_bound());
    shop_fixture(); exterior_roster.available_mask|=1; assert(exterior_bound());
    *(int16_t *)(ais[0]+0x16a)=1; assert(!exterior_bound());
    *(int16_t *)(ais[0]+0x16a)=0; modes[0][0xb]=0; assert(!exterior_bound());
    shop_fixture(); locked=2; assert(!exterior_bound());
    shop_fixture(); native_thread=GetCurrentThreadId()+1; assert(!exterior_bound());
    shop_fixture(); native_thread=0; assert(!exterior_bound());
    shop_fixture(); active=TRUE; assert(!exterior_bound());
    shop_fixture(); stopping=TRUE; assert(!exterior_bound());
    shop_fixture(); requested=2; assert(!exterior_bound());
    shop_fixture(); rotation.entered=TRUE; assert(!exterior_bound());
    shop_fixture(); input_owner.owned=TRUE; assert(!exterior_bound());
    shop_fixture(); input_owner.releasing=TRUE; assert(!exterior_bound());
    shop_fixture(); filter_exact=FALSE; assert(!exterior_bound());
    shop_fixture(); witness_exact=FALSE;
    assert(!SudekiMpLanStoryHostControlExteriorBound(controller,&w,&exterior_scene,2));
    shop_fixture(); w.service_post_original_exact=FALSE;
    assert(!SudekiMpLanStoryHostControlExteriorReady(controller,&w,&exterior_scene,2));
    /* No probe steals host input or releases a native AI override. */
    shop_fixture(); assert(exterior_ready());
    assert(none_calls+all_calls==calls && *(int *)(controller+0x80)==2 &&
        *(int *)(controller+0x84)==2 && *(int16_t *)(ais[3]+0x16a)==1 &&
        *(int16_t *)(ais[2]+0x16a)==1);
}
static void test_exterior_steering(void) {
    unsigned calls=none_calls+all_calls;
    exterior_fixture(); assert(exterior_steering() && exterior_ready());
    /* The host's door or interaction task can own a modal filter while the
     * positively settled exterior body remains independent and unpaused. */
    filters(0,0); assert(exterior_steering() && !exterior_ready());
    *(int16_t *)(ais[3]+0x16a)=1;
    static const int modal_filters[][2]={{0,0},{2,2},{2,1},{1,2}};
    for(unsigned i=0;i<sizeof(modal_filters)/sizeof(*modal_filters);++i) {
        filters(modal_filters[i][0],modal_filters[i][1]);
        assert(exterior_bound() && exterior_steering() && !exterior_ready());
        assert(*(int *)(controller+0x80)==modal_filters[i][0] &&
            *(int *)(controller+0x84)==modal_filters[i][1]);
    }
    shop_fixture(); assert(exterior_steering() && exterior_ready());
    static const uint8_t pauses[]={1,2,255};
    for(unsigned i=0;i<sizeof(pauses)/sizeof(*pauses);++i) {
        actors[2][0x2b]=pauses[i];
        assert(exterior_bound() && !exterior_steering() && actors[2][0x2b]==pauses[i]);
    }
    actors[2][0x2b]=0; assert(exterior_steering());
    /* Revalidate the exact split, native lease and roster on every callback. */
    split_exact=FALSE; assert(!exterior_steering()); split_exact=TRUE;
    roster_exact=FALSE; assert(!exterior_steering()); roster_exact=TRUE;
    owned_mask=0; assert(!exterior_steering()); owned_mask=4;
    *(void **)(actors[2]+0x94)=ais[3]; assert(!exterior_steering());
    shop_fixture(); native_thread=GetCurrentThreadId()+1; assert(!exterior_steering());
    shop_fixture(); input_owner.owned=TRUE; assert(!exterior_steering());
    shop_fixture();
    assert(!SudekiMpLanStoryHostControlExteriorNativeSteeringReady(NULL,2));
    assert(!SudekiMpLanStoryHostControlExteriorNativeSteeringReady(&exterior_roster,3));
    assert(!SudekiMpLanStoryHostControlExteriorNativeSteeringReady(&exterior_roster,4));
    assert(none_calls+all_calls==calls && *(int16_t *)(ais[3]+0x16a)==1 &&
        *(int16_t *)(ais[2]+0x16a)==1 && actors[2][0x2b]==0);
    puts("PASS named ExteriorSteering independent host modal, native pause preserved, stale owners refused; action guards unchanged");
}
static void returned_fixture(void) {
    exterior_fixture(); split_exact=FALSE; returned_exact=TRUE;
    filters(0,0);
}
static void test_returned_steering(void) {
    unsigned calls=none_calls+all_calls;
    SudekiMpControlUpdateDispatchWitness w={.service_only=TRUE,
        .service_post_original_exact=TRUE,.dispatch_serial=42};
    /* The split stub represents a positively completed balanced return, never
     * the unknown EXIT interval. Its lifecycle is tested by the split fixture. */
    returned_fixture(); assert(exterior_bound() && exterior_steering() && !exterior_ready());
    assert(SudekiMpLanStoryHostControlBound(controller,&w,&exterior_scene));
    assert(!SudekiMpLanStoryHostControlReady(controller,&w,&exterior_scene));
    assert(SudekiMpLanStoryHostControlExteriorBound(controller,&w,&exterior_scene,2));
    assert(!SudekiMpLanStoryHostControlExteriorReady(controller,&w,&exterior_scene,2));
    static const uint8_t pauses[]={1,2,255};
    for(unsigned i=0;i<sizeof(pauses)/sizeof(*pauses);++i) {
        actors[2][0x2b]=pauses[i];
        assert(exterior_bound() && !exterior_steering() && !exterior_ready());
        assert(actors[2][0x2b]==pauses[i]);
    }
    actors[2][0x2b]=0; assert(exterior_steering());
    /* Neither an ordinary filter nor a synthetic exact shop can turn the
     * return proof into action authority. Ordinary global readiness survives. */
    filters(1,1); assert(exterior_steering() && !exterior_ready());
    assert(SudekiMpLanStoryHostControlReady(controller,&w,&exterior_scene));
    shop_fixture(); split_exact=FALSE; returned_exact=TRUE;
    assert(exterior_bound() && exterior_steering() && !exterior_ready());
    assert(!SudekiMpLanStoryHostControlExteriorReady(controller,&w,&exterior_scene,2));
    /* Every callback needs a fresh proof and current native ownership. */
    returned_exact=FALSE; assert(!exterior_bound() && !exterior_steering() && !exterior_ready());
    returned_fixture(); roster_exact=FALSE; assert(!exterior_bound() && !exterior_steering());
    returned_fixture(); owned_mask=0; assert(!exterior_bound() && !exterior_steering());
    returned_fixture(); *(void **)(actors[2]+0x94)=ais[3]; assert(!exterior_steering());
    returned_fixture(); native_thread=GetCurrentThreadId()+1; assert(!exterior_steering());
    returned_fixture(); input_owner.owned=TRUE; assert(!exterior_steering());
    returned_fixture(); locked=2; assert(!exterior_steering());
    returned_fixture(); witness_exact=FALSE;
    assert(!SudekiMpLanStoryHostControlExteriorBound(controller,&w,&exterior_scene,2));
    returned_fixture(); w.service_post_original_exact=FALSE;
    assert(!SudekiMpLanStoryHostControlExteriorBound(controller,&w,&exterior_scene,2));
    returned_fixture();
    assert(!SudekiMpLanStoryHostControlExteriorNativeSteeringReady(NULL,2));
    assert(!SudekiMpLanStoryHostControlExteriorNativeSteeringReady(&exterior_roster,3));
    assert(!SudekiMpLanStoryHostControlExteriorNativeSteeringReady(&exterior_roster,4));
    assert(none_calls+all_calls==calls && *(int *)(controller+0x80)==0 &&
        *(int *)(controller+0x84)==0 && *(int16_t *)(ais[3]+0x16a)==0 &&
        *(int16_t *)(ais[2]+0x16a)==1 && actors[2][0x2b]==0);
    puts("PASS named ReturnedExteriorSteering completed return only, pause/stale refusal, action admission unchanged");
}
static void approach_fixture(void) {
    exterior_fixture();split_exact=returned_exact=FALSE;approach_exact=TRUE;
    filters(0,0);
    approach=(SudekiMpLanStoryInteriorTravel){.load_generation=3,.task=29,
        .destination_hash=123,.world=exterior_roster.world,.descriptor=exterior_roster.descriptor,
        .group=exterior_roster.group,.controller=controller,.leader=actors[3],
        .destination=(void *)4};
    strcpy(approach.destination_name,"interior");
}
static void test_interior_approach_steering(void) {
    unsigned calls=none_calls+all_calls;
    SudekiMpControlUpdateDispatchWitness w={.service_only=TRUE,
        .service_post_original_exact=TRUE,.dispatch_serial=42};
    approach_fixture();
    assert(exterior_bound() && exterior_steering() && !exterior_ready());
    assert(!SudekiMpLanStoryHostControlReady(controller,&w,&exterior_scene));
    assert(SudekiMpLanStoryHostControlExteriorBound(controller,&w,&exterior_scene,2));
    actors[2][0x2b]=1;assert(exterior_bound() && !exterior_steering());
    assert(actors[2][0x2b]==1);actors[2][0x2b]=0;
    approach_exact=FALSE;assert(!exterior_bound());approach_exact=TRUE;
    void **identity[]={&approach.world,&approach.descriptor,&approach.group,
        &approach.controller,&approach.leader,&approach.destination};
    for(unsigned i=0;i<sizeof(identity)/sizeof(*identity);++i) {
        void *original=*identity[i];*identity[i]=NULL;
        assert(!exterior_bound() && !exterior_steering());*identity[i]=original;
    }
    approach.load_generation=0;assert(!exterior_steering());approach.load_generation=3;
    approach.task=0;assert(!exterior_steering());approach.task=29;
    approach.destination_hash=0;assert(!exterior_steering());approach.destination_hash=123;
    approach.destination_name[0]=0;assert(!exterior_steering());approach.destination_name[0]='i';
    roster_exact=FALSE;assert(!exterior_steering());roster_exact=TRUE;
    owned_mask=0;assert(!exterior_steering());owned_mask=4;
    witness_exact=FALSE;assert(!SudekiMpLanStoryHostControlExteriorBound(controller,&w,&exterior_scene,2));
    witness_exact=TRUE;
    native_thread=GetCurrentThreadId()+1;assert(!exterior_steering());native_thread=GetCurrentThreadId();
    assert(exterior_steering() && !exterior_ready() && none_calls+all_calls==calls);
    assert(!*(int *)(controller+0x80) && !*(int *)(controller+0x84));
    puts("PASS named InteriorApproachSteering exact task/world/party/controller/lead only; native pause and actions unchanged");
}
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(file!=INVALID_HANDLE_VALUE); DWORD size=GetFileSize(file,NULL),got=0;
    uint8_t *raw=malloc(size);
    assert(raw && ReadFile(file,raw,size,&got,NULL) && got==size); CloseHandle(file);
    IMAGE_DOS_HEADER *dos=(void *)raw; IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *b=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(b); memcpy(b,raw,nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *sections=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        IMAGE_SECTION_HEADER *s=&sections[i];
        assert(s->PointerToRawData<=size && s->SizeOfRawData<=size-s->PointerToRawData);
        assert(s->VirtualAddress<=nt->OptionalHeader.SizeOfImage &&
            s->SizeOfRawData<=nt->OptionalHeader.SizeOfImage-s->VirtualAddress);
        memcpy(b+s->VirtualAddress,raw+s->PointerToRawData,s->SizeOfRawData);
    }
    IMAGE_DATA_DIRECTORY reloc=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    uintptr_t delta=(uintptr_t)b-nt->OptionalHeader.ImageBase;
    for(unsigned offset=0;offset<reloc.Size;) {
        IMAGE_BASE_RELOCATION *block=(void *)(b+reloc.VirtualAddress+offset);
        assert(block->SizeOfBlock>=sizeof(*block) && block->SizeOfBlock<=reloc.Size-offset);
        uint16_t *items=(void *)(block+1); unsigned count=(block->SizeOfBlock-sizeof(*block))/2;
        for(unsigned i=0;i<count;++i) {
            unsigned type=items[i]>>12,rva=block->VirtualAddress+(items[i]&0xfff);
            assert(type==IMAGE_REL_BASED_ABSOLUTE || type==IMAGE_REL_BASED_HIGHLOW);
            if(type==IMAGE_REL_BASED_HIGHLOW) {
                assert(rva<=nt->OptionalHeader.SizeOfImage-4);
                *(uint32_t *)(b+rva)+=(uint32_t)delta;
            }
        }
        offset+=block->SizeOfBlock;
    }
    free(raw); return b;
}
static void test_shop_image(const char *path) {
    wchar_t wide[1024];
    assert(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,wide,1024));
    uint8_t *synthetic=base; base=map_image(wide);
    /* Only singleton objects are synthetic. Code comes unchanged from the
     * supported image and remains non-executable throughout this check. */
    memset(frontend,0,sizeof(frontend)); memset(shop,0,sizeof(shop));
    *(void **)(base+0x3c2f88)=frontend; *(void **)(base+0x3c2f70)=shop;
    *(void **)frontend=base+0x2caf9c; *(void **)(frontend+0x74)=shop;
    *(unsigned *)(frontend+0xb8)=8; *(void **)shop=base+0x2cabb4;
    *(int *)(shop+0x88)=7;
    assert(native_shop_active_exact());
    for(unsigned i=0;i<5u+sizeof(shop_active_tail);++i) {
        base[0x8d1c0+i]^=1; assert(!native_shop_active_exact());
        base[0x8d1c0+i]^=1; assert(native_shop_active_exact());
    }
    puts("PASS named IsShopActive exact supported image, all 42 byte mutations refused");
    /* Native node=actor+8 (the split owner's established node contract).
     * Verify the count read by steering at actor+2B against the supported
     * Pause test/increment and Resume decrement/zero branch. These are image
     * observations only: native scheduler functions are never invoked. */
    const struct { const char *name; unsigned rva,size; uint8_t bytes[7]; } pause_sites[]={
        {"NativeNodePauseCounterTest",0x1060c0u,7u,{0x57,0x8b,0xf8,0x80,0x7f,0x23,0}},
        {"NativeNodePauseCounterIncrement",0x1060f4u,5u,{0xfe,0x47,0x23,0x5f,0xc3}},
        {"NativeNodeResumeCounterDecrementAndZero",0x106100u,5u,{0xfe,0x4e,0x23,0x75,0x16}}};
    for(unsigned i=0;i<sizeof(pause_sites)/sizeof(*pause_sites);++i) {
        assert(!memcmp(base+pause_sites[i].rva,pause_sites[i].bytes,pause_sites[i].size));
        printf("PASS named %s exact supported image node+23 / actor+2B contract\n",pause_sites[i].name);
    }
    assert(VirtualFree(base,0,MEM_RELEASE)); base=synthetic;
}
int main(int argc,char **argv) {
    assert(argc==1 || argc==2);
    base=VirtualAlloc(NULL,0x409000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    assert(base);
    jump(FILTER_NONE,native_none); jump(FILTER_ALL,native_all);
    assert(FlushInstructionCache(GetCurrentProcess(),base,0x409000));
    *(void **)(base+0x408d68)=quit; *(void **)(base+0x3c2f84)=quick;
    *(void **)(base+0x408da0)=speed;
    SudekiMpControlUpdateDispatchWitness w={0};
    SudekiMpLanStoryNativeRoster r={.controller=controller,.world=(void *)1,
        .descriptor=(void *)2,.group=(void *)3};
    /* Repeated F8 combat toggles: priming restores All behind an owned None.
     * Closing retires with NO extra native call; staying open reacquires only
     * after the original pair and a clear native-menu boundary are observed. */
    filters(1,1);
    for(unsigned i=0;i<8;++i) {
        assert(input_filter(&w,&r,TRUE) && input_owner.owned);
        filters(1,1);
        unsigned calls=none_calls+all_calls;
        assert(input_filter(&w,&r,FALSE) && !input_owner.owned);
        assert(none_calls+all_calls==calls);
    }
    assert(input_filter(&w,&r,TRUE)); filters(1,1);
    unsigned calls=none_calls;
    assert(input_filter(&w,&r,TRUE) && input_owner.owned && none_calls==calls+1);
    assert(*(int *)(controller+0x80)==0 && *(int *)(controller+0x84)==0);
    /* A normal asynchronous release is still retained, not replayed. */
    assert(!input_filter(&w,&r,FALSE) && input_owner.releasing);
    calls=all_calls;
    assert(!input_filter(&w,&r,FALSE) && input_owner.owned && all_calls==calls);
    filters(1,1); assert(input_filter(&w,&r,FALSE) && !input_owner.owned);
    /* Pending-only restoration and a foreign mode cannot be repaired. */
    assert(input_filter(&w,&r,TRUE)); filters(0,1);
    assert(!input_filter(&w,&r,FALSE) && input_owner.owned && !input_owner.releasing);
    filters(2,2); assert(!input_filter(&w,&r,FALSE) && input_owner.owned);
    filters(1,1);
    roster_exact=FALSE; assert(!input_filter(&w,&r,FALSE) && input_owner.owned);
    roster_exact=TRUE; filter_exact=FALSE;
    assert(!input_filter(&w,&r,FALSE) && input_owner.owned); filter_exact=TRUE;
    r.group=(void *)4; assert(!input_filter(&w,&r,FALSE) && input_owner.owned);
    r.group=(void *)3;
    /* Native Q/pause ownership blocks reacquisition even after our lease
     * ended. Do not steal another native modal's restored input. */
    quick[0x29]=1; calls=none_calls;
    assert(!input_filter(&w,&r,TRUE) && !input_owner.owned && none_calls==calls);
    quick[0x29]=0; speed[0x28]=1;
    assert(!input_filter(&w,&r,TRUE) && !input_owner.owned && none_calls==calls);
    speed[0x28]=0; assert(input_filter(&w,&r,TRUE));
    /* Reopen while our own asynchronous release is pending. */
    assert(!input_filter(&w,&r,FALSE) && input_owner.releasing);
    assert(input_filter(&w,&r,TRUE) && input_owner.owned && !input_owner.releasing);
    filters(1,1); assert(input_filter(&w,&r,FALSE) && !input_owner.owned);
    test_exterior_independent_lease();
    test_exterior_steering();
    test_returned_steering();
    test_interior_approach_steering();
    if(argc==2) test_shop_image(argv[1]);
    VirtualFree(base,0,MEM_RELEASE); base=NULL;
    puts("story host filter lease regression tests passed (synthetic, no gameplay)");
    return 0;
}
