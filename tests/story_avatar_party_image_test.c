/* Exact-image signatures plus fixture native membership receipts. Native calls
 * below are fixtures; this test does not delete actors in a running game. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/hooks/lan_story_avatar_party.c"
static size_t image_size;
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(f!=INVALID_HANDLE_VALUE); DWORD size=GetFileSize(f,NULL),got=0;
    uint8_t *raw=malloc(size); assert(raw && ReadFile(f,raw,size,&got,NULL) && got==size); CloseHandle(f);
    IMAGE_DOS_HEADER *dos=(void *)raw; IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    image_size=nt->OptionalHeader.SizeOfImage;
    uint8_t *b=VirtualAlloc(NULL,image_size,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    assert(b); memcpy(b,raw,nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *sections=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        IMAGE_SECTION_HEADER *s=&sections[i];
        assert(s->PointerToRawData<=size && s->SizeOfRawData<=size-s->PointerToRawData);
        assert(s->VirtualAddress<=image_size && s->SizeOfRawData<=image_size-s->VirtualAddress);
        memcpy(b+s->VirtualAddress,raw+s->PointerToRawData,s->SizeOfRawData);
    }
    uintptr_t delta=(uintptr_t)b-nt->OptionalHeader.ImageBase;
    IMAGE_DATA_DIRECTORY reloc=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    for(unsigned offset=0;offset<reloc.Size;) {
        IMAGE_BASE_RELOCATION *block=(void *)(b+reloc.VirtualAddress+offset);
        assert(block->SizeOfBlock>=sizeof(*block) && block->SizeOfBlock<=reloc.Size-offset);
        uint16_t *items=(uint16_t *)(block+1);
        for(unsigned i=0;i<(block->SizeOfBlock-sizeof(*block))/2;++i) {
            unsigned type=items[i]>>12,rva=block->VirtualAddress+(items[i]&0xfffu);
            assert(type==IMAGE_REL_BASED_ABSOLUTE || type==IMAGE_REL_BASED_HIGHLOW);
            if(type==IMAGE_REL_BASED_HIGHLOW) { assert(rva<=image_size-4); *(uint32_t *)(b+rva)+=(uint32_t)delta; }
        }
        offset+=block->SizeOfBlock;
    }
    free(raw); return b;
}

static uint8_t world[0x39b],group[0xd8],controller[0x24c],registry_owner[0x40],descriptor[0x38];
static uint8_t entities[3][0x138],components[3][COMPONENTS][0x180],modes[3][16],buffers[3][2][16];
static uint32_t buffer_data[3][2][8];
static uint8_t listener[0x140],ui[0x110];
static uint8_t hud[0x1a4],scene[0x174],gizmos[4][0xc00],animations[4][0x13c],nodes[4][0x1c],renders[4][0x38];
static uint32_t model_fixture[4],renderer_fixture;
static void *catalogue_entries[8],*listener_entries[1];
static SudekiMpLanStoryNativeRoster roster;
static SudekiMpControlUpdateDispatchWitness witness_fixture={.dispatch_serial=1,.service_only=1,.service_post_original_exact=1};
static BOOL witness_good=TRUE,input_good=TRUE,spawn_good=TRUE,receipt_open,delete_succeeds=TRUE;
static BOOL spawn_exited;
static unsigned add_calls,delete_calls,lead_calls,filter_calls,wrapper_creates,wrapper_deletes;
static unsigned receipt_before;
static SudekiMpLanStoryAvatarSpawnGroupOperation receipt_operation;
void SudekiMpLogWrite(const char *s) { (void)s; }
void SudekiMpLogFormat(const char *s,...) { (void)s; }
BOOL SudekiMpSpiritInstanceFilterNoneEntryExact(HMODULE image) { (void)image; return FALSE; }
BOOL SudekiMpLanStoryTaskTraceAddCallImageExact(HMODULE image) {
    const uint8_t *b=(const uint8_t *)image;
    return b && b[0x23260]==0xe8 && word(b,0x23261)==0x1bu;
}
BOOL SudekiMpControlSeparationUpdateDispatchWitnessStillExact(const SudekiMpControlUpdateDispatchWitness *w) {
    return witness_good && w==&witness_fixture;
}
BOOL SudekiMpLanStoryInputControllerExact(void *c) { return input_good && c==controller; }
BOOL SudekiMpLanStoryObserverRosterStillExact(const SudekiMpControlUpdateDispatchWitness *w,const SudekiMpLanStoryNativeRoster *r) {
    return w==&witness_fixture && r==&roster;
}
BOOL SudekiMpLanStoryTaskTraceGetStatus(SudekiMpLanStoryTaskTraceStatus *s) {
    memset(s,0,sizeof(*s)); s->load_generation=77; return TRUE;
}
BOOL SudekiMpLanStoryTaskTraceEntitySetupExact(HMODULE image) { return image==(HMODULE)base; }
BOOL SudekiMpLanStoryAvatarSpawnWorldExited(uint32_t epoch,uint32_t load,void *w) {
    return spawn_exited && epoch==123 && load==77 && w==world;
}
BOOL SudekiMpLanStoryAvatarSpawnObserve(unsigned player,uint32_t epoch,uint32_t generation,SudekiMpLanStoryAvatarSpawnObservation *out) {
    if(!spawn_good || receipt_open || player!=2 || epoch!=123 || generation!=456) return FALSE;
    *out=(SudekiMpLanStoryAvatarSpawnObservation){.seat=2,.epoch=123,.generation=456,.ready=TRUE,.actor=entities[2],.world=world};
    return TRUE;
}
BOOL SudekiMpLanStoryAvatarSpawnGroupBegin(const void *owner,uint32_t epoch,SudekiMpLanStoryAvatarSpawnGroupOperation operation,void *e) {
    assert(owner==&party && epoch==123 && e && !receipt_open);
    receipt_open=TRUE; receipt_before=word(group,0xcc); receipt_operation=operation; return TRUE;
}
BOOL SudekiMpLanStoryAvatarSpawnGroupEnd(const void *owner,BOOL *changed) {
    assert(owner==&party && changed && receipt_open); receipt_open=FALSE;
    *changed=word(group,0xcc)!=receipt_before || receipt_operation==SUDEKIMP_AVATAR_SPAWN_GROUP_ROTATE;
    return TRUE;
}
static void put(void *p,unsigned offset,void *v) { *(void **)((uint8_t *)p+offset)=v; }
static void number(void *p,unsigned offset,uint32_t v) { *(uint32_t *)((uint8_t *)p+offset)=v; }
static void * __attribute__((stdcall)) fixture_pointer(void *e) {
    assert(busy && e); ++wrapper_creates;
    uint8_t *p=calloc(1,0x18); assert(p); put(p,0,base+0x2c0098); put(p,0xc,e); return p;
}
static void * __attribute__((thiscall)) fixture_pointer_delete(void *p,unsigned flags) {
    assert(busy && flags==1 && object(p,0x18,0x2c0098)); ++wrapper_deletes; free(p); return NULL;
}
static void __attribute__((thiscall)) fixture_filter(void *c) {
    assert(busy && c==controller); ++filter_calls;
    /* Execute the verified relocated native request on fixture memory. It
     * changes pending+84, not current+80; no full controller tick is run. */
    ((FilterCall)(base+0x8ac0))(c);
}
static void __attribute__((thiscall)) fixture_add(void *g,void *p) {
    assert(busy && receipt_open && g==group && ptr(p,0xc)==entities[2] && word(g,0xcc)==1);
    ++add_calls; put(g,0x9c,entities[2]); number(g,0xcc,2);
}
static void __attribute__((thiscall)) fixture_remove(void *g,void *p) {
    void *e=ptr(p,0xc); assert(busy && receipt_open && g==group && word(g,0xcc)==2 && e==ptr(g,0x9c));
    ++delete_calls; put(g,0x9c,NULL); number(g,0xcc,1);
    if(!delete_succeeds) return;
    unsigned count=word(registry_owner,0x34),found=0;
    for(unsigned i=0;i<count;++i) if(catalogue_entries[i]==e) {
        for(unsigned j=i+1;j<count;++j) catalogue_entries[j-1]=catalogue_entries[j];
        catalogue_entries[count-1]=NULL; number(registry_owner,0x34,count-1); found=1; break;
    }
    assert(found); put(p,0xc,NULL);
}
static unsigned char __attribute__((thiscall)) fixture_lead(void *g,void *p) {
    assert(busy && receipt_open && g==group && ptr(p,0xc)==entities[2]);
    ++lead_calls; put(g,0xc0,entities[2]); return 1;
}
static void setup(HMODULE image) {
    assert(SudekiMpLanStoryAvatarPartyInstall(image));
    make_pointer=fixture_pointer; delete_pointer=fixture_pointer_delete; add_player=fixture_add;
    remove_delete=fixture_remove; set_leader=fixture_lead; filter_none=fixture_filter;
    memset(world,0,sizeof(world)); memset(group,0,sizeof(group)); memset(controller,0,sizeof(controller));
    memset(entities,0,sizeof(entities)); memset(components,0,sizeof(components));
    memset(modes,0,sizeof(modes)); memset(buffers,0,sizeof(buffers));
    memset(catalogue_entries,0,sizeof(catalogue_entries));
    witness_good=input_good=spawn_good=delete_succeeds=TRUE; receipt_open=spawn_exited=FALSE;
    witness_fixture.dispatch_serial=1;
    add_calls=delete_calls=lead_calls=filter_calls=wrapper_creates=wrapper_deletes=0;
    put(base,0x408d10,world); put(base,0x408d94,group); put(base,0x408da4,controller); put(base,0x409d8c,registry_owner);
    put(world,0,base+0x2c4c3c); put(world,0xc,descriptor); put(group,0,base+0x2c6d30);
    put(controller,0,base+0x2c9f5c); put(controller,0x2c,base+0x2c9f84);
    put(controller,0x248,entities[0]); number(controller,0x80,1); number(controller,0x84,1);
    *(float *)(controller+0x1b8)=2.0f;
    number(group,0xcc,2); put(group,0x90,entities[0]); put(group,0x9c,entities[1]);
    number(group,0x38,1); put(group,0x40,listener_entries); listener_entries[0]=listener;
    put(listener,0,base+0x2c5248); put(listener,0x13c,ui); put(ui,0,base+0x2caf9c);
    put(base,0x3c2f88,ui); put(base,0x3c2f9c,hud); put(base,0x408d1c,scene);
    put(ui,0x6c,hud); put(scene,0x170,ui); put(scene,0x70,&renderer_fixture);
    put(hud,0,base+0x2cb3e4); put(hud,0x10c,base+0x2d9004); hud[0x134]=1;
    put(hud,0x40,base+0x2d8fb4); put(hud,0x68,base+0x2d8fd8); number(hud,0x158,2);
    for(unsigned i=0;i<4;++i) {
        put(hud,0x138+i*4,gizmos[i]); put(hud,0x148+i*4,animations[i]);
        put(gizmos[i],0,base+0x2cb590); put(gizmos[i],4,base+0x2cb59c); number(gizmos[i],0x32c,i);
        put(gizmos[i],0x320,animations[i]); put(animations[i],0,base+0x2d1da8); put(animations[i],0xbc,nodes[i]);
        put(nodes[i],0,base+0x2d1df0); put(nodes[i],8,renders[i]); put(nodes[i],0xc,&model_fixture[i]);
        put(nodes[i],0x14,&renderer_fixture); put(renders[i],0,base+0x2dd700); put(renders[i],0x14,&model_fixture[i]);
    }
    put(registry_owner,0x3c,catalogue_entries); number(registry_owner,0x34,3);
    for(unsigned a=0;a<3;++a) {
        catalogue_entries[a]=entities[a]; put(entities[a],0,base+(a==2?0x2d55d4:a==0?0x2d555c:0x2d5010));
        put(entities[a],0x2c,base+(a==2?0x2d5618:a==0?0x2d55a0:0x2d5054));
        number(entities[a],0x30,a==2?0xf82:0xf99); number(entities[a],0x34,100+a);
        for(unsigned c=0;c<COMPONENTS;++c) {
            put(entities[a],component_offsets[c],components[a][c]);
            put(components[a][c],0,base+component_vtables[c]); put(components[a][c],0x10,entities[a]);
        }
        put(components[a][6],0x3c,modes[a]); put(modes[a],0,base+(a==2?0x2da360:0x2da340));
        modes[a][8]=0xff; modes[a][0xb]=a?1:0;
        for(unsigned i=0;i<2;++i) {
            put(components[a][6],0x16c+i*4,buffers[a][i]);
            put(buffers[a][i],0,buffer_data[a][i]); number(buffers[a][i],4,8);
        }
        *(float *)(components[a][1]+0x30)=8000; *(float *)(components[a][1]+0x38)=999;
    }
    roster=(SudekiMpLanStoryNativeRoster){.epoch=123,.revision=1,.available_mask=12,.leader_character=3,
        .world=world,.descriptor=descriptor,.group=group,.controller=controller};
    roster.actors[3]=entities[0]; roster.actors[2]=entities[1];
    roster.ai[3]=components[0][6]; roster.ai[2]=components[1][6];
}
static void native_exit(void) {
    receipt_open=FALSE; spawn_exited=TRUE;
    assert(SudekiMpLanStoryAvatarPartyUninstall() && !SudekiMpLanStoryAvatarPartyRetains());
}
static void step(void) {
    ++witness_fixture.dispatch_serial;
    assert(SudekiMpLanStoryAvatarPartyService(&witness_fixture,controller));
}
static void commit_filter(void) {
    assert(word(controller,0x80)==1 && word(controller,0x84)==0);
    /* Model the original controller's285D7 pending-to-current commit. */
    number(controller,0x80,word(controller,0x84));
}
int main(int argc,char **argv) {
    assert(argc==2); wchar_t path[1024]; assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024));
    uint8_t *b=map_image(path);
    assert(SudekiMpLanStoryAvatarPartyImageMatches((HMODULE)b));
    b[0x235e0]^=1; assert(!SudekiMpLanStoryAvatarPartyImageMatches((HMODULE)b)); b[0x235e0]^=1;
    setup((HMODULE)b);
    assert(!SudekiMpLanStoryAvatarPartyBegin(&witness_fixture,&roster,2,456,4));
    number(controller,0x8c,1); assert(!SudekiMpLanStoryAvatarPartyBegin(&witness_fixture,&roster,2,456,0));
    number(controller,0x8c,0); assert(!filter_calls && !SudekiMpLanStoryAvatarPartyRetains());
    *(int16_t *)(components[2][6]+0x16a)=1;
    assert(!SudekiMpLanStoryAvatarPartyBegin(&witness_fixture,&roster,2,456,0));
    *(int16_t *)(components[2][6]+0x16a)=0;
    modes[2][8]=3; assert(!SudekiMpLanStoryAvatarPartyBegin(&witness_fixture,&roster,2,456,0)); modes[2][8]=0xff;
    hud[0x134]=0; assert(!SudekiMpLanStoryAvatarPartyBegin(&witness_fixture,&roster,2,456,0)); hud[0x134]=1;
    assert(SudekiMpLanStoryAvatarPartyBegin(&witness_fixture,&roster,2,456,0));
    assert(filter_calls==1 && !SudekiMpLanStoryAvatarPartyUninstall());
    SudekiMpLanStoryAvatarPartyObservation o;
    assert(SudekiMpLanStoryAvatarPartyObserve(&o) && o.hero_mask==12 && o.phase==SUDEKIMP_AVATAR_PARTY_FILTER_PENDING);
    assert(word(controller,0x80)==1 && word(controller,0x84)==0);
    for(unsigned i=0;i<3;++i) step();
    assert(filter_calls==1 && !delete_calls && !add_calls && !lead_calls && !wrapper_creates && !receipt_open);
    assert(party.out.phase==SUDEKIMP_AVATAR_PARTY_FILTER_PENDING);
    commit_filter();
    step(); assert(delete_calls==1 && word(group,0xcc)==1 && party.out.hero_mask==8);
    step(); assert(add_calls==1 && word(group,0xcc)==2);
    step(); assert(lead_calls==1 && receipt_open && party.out.phase==SUDEKIMP_AVATAR_PARTY_LEAD_PENDING);
    assert(SudekiMpLanStoryAvatarPartyObserve(&o));
    step(); assert(lead_calls==1 && receipt_open); /* Pending is not a retry. */
    put(group,0x90,entities[2]); put(group,0x9c,entities[0]); put(controller,0x248,entities[2]);
    modes[2][0xb]=0; modes[0][0xb]=1;
    step(); assert(!receipt_open && party.out.membership_revision==4 && delete_calls==1);
    assert(SudekiMpLanStoryAvatarPartyObserve(&o) && o.native_leader==entities[2]);
    put(group,0xc0,NULL); step(); assert(delete_calls==2 && word(group,0xcc)==1);
    step(); assert(SudekiMpLanStoryAvatarPartyObserve(&o) && o.phase==SUDEKIMP_AVATAR_PARTY_READY &&
        o.epoch==123 && !o.hero_mask && o.native_leader==entities[2] && o.leader_character==SUDEKIMP_LAN_STORY_NO_SEAT);
    assert(o.member_count==1 && !o.members[1] && !o.members[2] && !o.members[3]);
    for(unsigned c=0;c<4;++c) assert(!o.heroes[c]);
    assert(wrapper_creates==wrapper_deletes && wrapper_creates==4);
    number(group,0xd0,1); assert(SudekiMpLanStoryAvatarPartyObserve(&o)); step();
    number(group,0xd0,0);
    assert(!SudekiMpLanStoryAvatarPartyUninstall()); native_exit();

    setup((HMODULE)b); assert(SudekiMpLanStoryAvatarPartyBegin(&witness_fixture,&roster,2,456,0));
    commit_filter(); ++witness_fixture.dispatch_serial;
    delete_succeeds=FALSE;
    assert(!SudekiMpLanStoryAvatarPartyService(&witness_fixture,controller));
    assert(delete_calls==1 && party.out.phase==SUDEKIMP_AVATAR_PARTY_UNKNOWN &&
        !SudekiMpLanStoryAvatarPartyObserve(&o) && !SudekiMpLanStoryAvatarPartyUninstall());
    assert(!SudekiMpLanStoryAvatarPartyService(&witness_fixture,controller) && delete_calls==1);
    native_exit();
    setup((HMODULE)b); assert(SudekiMpLanStoryAvatarPartyBegin(&witness_fixture,&roster,2,456,0));
    b[0x23230]^=1; assert(!SudekiMpLanStoryAvatarPartyService(&witness_fixture,controller) && !delete_calls);
    b[0x23230]^=1; native_exit();
    setup((HMODULE)b); assert(SudekiMpLanStoryAvatarPartyBegin(&witness_fixture,&roster,2,456,0));
    commit_filter();
    step(); step(); step(); assert(receipt_open && party.out.phase==SUDEKIMP_AVATAR_PARTY_LEAD_PENDING);
    assert(!SudekiMpLanStoryAvatarPartyUninstall()); native_exit();

    /* Even a committed value cannot admit membership on the request's own
     * dispatch. A later exact dispatch is required, and the request is once. */
    setup((HMODULE)b); assert(SudekiMpLanStoryAvatarPartyBegin(&witness_fixture,&roster,2,456,0));
    commit_filter();
    for(unsigned i=0;i<3;++i) assert(SudekiMpLanStoryAvatarPartyService(&witness_fixture,controller));
    assert(party.out.phase==SUDEKIMP_AVATAR_PARTY_FILTER_PENDING && filter_calls==1 && !delete_calls && !add_calls && !lead_calls);
    step(); assert(delete_calls==1 && filter_calls==1); native_exit();

    /* Foreign pending/current transitions cannot masquerade as the one
     * requested native1/0 ->0/0 transition. UNKNOWN remains latched. */
    static const unsigned invalid_filters[][2]={{1,1},{2,0},{0,1},{0,2}};
    for(unsigned i=0;i<sizeof(invalid_filters)/sizeof(invalid_filters[0]);++i) {
        setup((HMODULE)b); assert(SudekiMpLanStoryAvatarPartyBegin(&witness_fixture,&roster,2,456,0));
        number(controller,0x80,invalid_filters[i][0]); number(controller,0x84,invalid_filters[i][1]);
        assert(!SudekiMpLanStoryAvatarPartyObserve(&o)); ++witness_fixture.dispatch_serial;
        assert(!SudekiMpLanStoryAvatarPartyService(&witness_fixture,controller));
        assert(party.out.phase==SUDEKIMP_AVATAR_PARTY_UNKNOWN && filter_calls==1 && !delete_calls && !add_calls && !lead_calls);
        number(controller,0x80,0); number(controller,0x84,0);
        assert(!SudekiMpLanStoryAvatarPartyService(&witness_fixture,controller)); native_exit();
    }
    /* A legitimate pending filter is retained until a positive world exit. */
    setup((HMODULE)b); assert(SudekiMpLanStoryAvatarPartyBegin(&witness_fixture,&roster,2,456,0));
    assert(!SudekiMpLanStoryAvatarPartyUninstall() && SudekiMpLanStoryAvatarPartyObserve(&o));
    assert(o.phase==SUDEKIMP_AVATAR_PARTY_FILTER_PENDING && !delete_calls && !add_calls && !lead_calls); native_exit();
    VirtualFree(b,0,MEM_RELEASE);
    puts("story avatar party: PASS (exact-image signatures, fixture membership/retirement/leader/filter transitions; no gameplay)");
    return 0;
}
