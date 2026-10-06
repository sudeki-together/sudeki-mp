/* Supported-image bytes plus inert synthetic object ownership. No native
 * constructor, loader, scene registration or game callback is executed. */
#include "../src/hooks/lan_story_area_membership.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static uint8_t *mapped;
static uint8_t world[0x39c],descriptors[3][0x54],data[3][0x12c],collision[3][0x110];
static uint8_t actors[4][0x98],membership[4][0x44],navigation[3][8],sectors[3][2][0x40];
static uint8_t movement[4][0xc0];
static char names[3][64];
static uint8_t arrivals[3][2][0x30];
static uint32_t arrival_refs[3][2][2];
static char arrival_names[3][2][SUDEKIMP_STORY_ARRIVAL_NAME];
static SudekiMpLanStoryNativeRoster roster;
static SudekiMpControlUpdateDispatchWitness witness;
static SudekiMpLanStoryAreaMembership output,saved;
static BOOL exact;
static unsigned checks,change_on_check,change_kind;
BOOL SudekiMpLanStoryObserverRosterStillExact(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r) {
    assert(w==&witness && r==&roster);
    if(++checks==change_on_check) {
        switch(change_kind) {
        case 0: *(uint16_t *)(membership[2]+0x40)=0x209; break;
        case 1: *(void **)(world+0x58+4)=descriptors[2]; break;
        case 2: *(void **)(descriptors[1]+0x14)=data[2]; break;
        case 3: collision[1][0xf4]=0; break;
        case 4: names[1][0]='x'; break;
        case 6: *(float *)(arrivals[1][0]+8)=333.0f; break;
        case 7: *(void **)(data[1]+0xc8)=arrivals[2]; break;
        case 8: arrival_names[1][0][0]='x'; break;
        case 9: *(uint32_t *)(arrivals[1][1]+0x24)=420; break;
        case 10: data[1][0x128]=4; break;
        case 11: *(uint16_t *)(arrivals[1][0]+0x2c)=0x202; break;
        case 12: ++arrival_refs[1][0][0]; break;
        case 13: *(uint16_t *)(sectors[1][0]+0xa)=0x109; break;
        case 14: *(void **)(navigation[1]+4)=sectors[2]; break;
        case 15: *(uint32_t *)navigation[1]=1; break;
        case 16: *(void **)(data[1]+0x28)=navigation[2]; break;
        case 17: *(uint16_t *)(movement[2]+0xbc)=0x207; break;
        case 18: *(void **)(actors[2]+0x80)=NULL; break;
        case 19: movement[2][0xbe]^=4; break;
        case 20: *(void **)(movement[2]+0x10)=actors[3]; break;
        default: return FALSE;
        }
    }
    return exact;
}
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(file!=INVALID_HANDLE_VALUE); DWORD size=GetFileSize(file,NULL),got=0;
    uint8_t *raw=malloc(size); assert(raw && ReadFile(file,raw,size,&got,NULL) && got==size); CloseHandle(file);
    IMAGE_DOS_HEADER *dos=(void *)raw;
    IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *base=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(base); memcpy(base,raw,nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *sections=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        IMAGE_SECTION_HEADER *s=&sections[i];
        assert(s->PointerToRawData<=size && s->SizeOfRawData<=size-s->PointerToRawData);
        assert(s->VirtualAddress<=nt->OptionalHeader.SizeOfImage &&
            s->SizeOfRawData<=nt->OptionalHeader.SizeOfImage-s->VirtualAddress);
        memcpy(base+s->VirtualAddress,raw+s->PointerToRawData,s->SizeOfRawData);
    }
    uintptr_t delta=(uintptr_t)base-nt->OptionalHeader.ImageBase;
    IMAGE_DATA_DIRECTORY reloc=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    unsigned offset=0;
    while(offset<reloc.Size) {
        IMAGE_BASE_RELOCATION *block=(void *)(base+reloc.VirtualAddress+offset);
        assert(block->SizeOfBlock>=sizeof(*block) && block->SizeOfBlock<=reloc.Size-offset);
        uint16_t *items=(uint16_t *)(block+1);
        unsigned count=(block->SizeOfBlock-sizeof(*block))/2;
        for(unsigned i=0;i<count;++i) {
            unsigned type=items[i]>>12,rva=block->VirtualAddress+(items[i]&0xfffu);
            assert(type==IMAGE_REL_BASED_ABSOLUTE || type==IMAGE_REL_BASED_HIGHLOW);
            if(type==IMAGE_REL_BASED_HIGHLOW) {
                assert(rva<=nt->OptionalHeader.SizeOfImage-4);
                *(uint32_t *)(base+rva)+=(uint32_t)delta;
            }
        }
        offset+=block->SizeOfBlock;
    }
    free(raw); return base;
}

static void setup(void) {
    memset(world,0,sizeof(world)); memset(descriptors,0,sizeof(descriptors));
    memset(data,0,sizeof(data)); memset(collision,0,sizeof(collision));
    memset(actors,0,sizeof(actors)); memset(membership,0,sizeof(membership));
    memset(movement,0,sizeof(movement));
    memset(&roster,0,sizeof(roster)); memset(&witness,0,sizeof(witness));
    memset(names,0,sizeof(names)); memset(&output,0xa5,sizeof(output));
    memset(arrivals,0,sizeof(arrivals)); memset(arrival_refs,0,sizeof(arrival_refs));
    memset(arrival_names,0,sizeof(arrival_names));
    memset(navigation,0,sizeof(navigation)); memset(sectors,0,sizeof(sectors));
    checks=change_on_check=change_kind=0; exact=TRUE;
    witness.service_post_original_exact=1; witness.dispatch_serial=17;
    roster.dispatch_serial=17; roster.epoch=4; roster.revision=6;
    roster.world=world; roster.descriptor=descriptors[0]; roster.available_mask=12; roster.leader_character=3;
    *(void **)(mapped+0x408d10)=world; *(void **)world=mapped+0x2c4c3c;
    *(void **)(world+0xc)=descriptors[0]; *(void **)(world+0x50)=descriptors;
    *(uint32_t *)(world+0x54)=3;
    for(unsigned i=0;i<3;++i) {
        *(void **)(world+0x58+4*i)=descriptors[i];
        uint8_t *d=descriptors[i],*z=data[i],*c=collision[i];
        *(void **)d=mapped+0x2c82a4; *(uint32_t *)(d+0x20)=i;
        *(void **)(d+0x24)=names[i]; *(uint32_t *)(d+0x34)=i==0?3:2;
        snprintf(names[i],sizeof(names[i]),"Region_%u",i);
        d[0x50]=i==1?0x32:0x30; *(void **)(d+0x14)=z;
        *(void **)z=mapped+0x2cdcdc; *(uint32_t *)(z+0x24)=i;
        *(void **)(z+0x28)=navigation[i]; *(void **)(z+0x2c)=c;
        *(uint32_t *)navigation[i]=2; *(void **)(navigation[i]+4)=sectors[i];
        *(uint16_t *)(sectors[i][0]+0xa)=(uint16_t)(i*256u+7u);
        *(uint16_t *)(sectors[i][1]+0xa)=(uint16_t)(i*256u+8u);
        *(void **)(c+0x50)=d; *(uint32_t *)(c+0x44)=0x20000;
        c[0xf2]=1; c[0xf4]=1;
        z[0x128]=5; *(uint32_t *)(z+0xc0)=2; *(uint32_t *)(z+0xc4)=2;
        *(void **)(z+0xc8)=arrivals[i];
        for(unsigned j=0;j<2;++j) {
            uint8_t *m=arrivals[i][j];
            *(void **)m=mapped+0x2cddac; *(void **)(m+4)=mapped+0x2cddb4;
            *(float *)(m+8)=(float)(i*100u+j); *(float *)(m+0xc)=4.0f;
            *(float *)(m+0x10)=-8.0f; *(float *)(m+0x1c)=1.0f;
            *(uint32_t *)(m+0x20)=0xfff; *(uint32_t *)(m+0x24)=420+j;
            *(void **)(m+0x28)=arrival_refs[i][j];
            arrival_refs[i][j][0]=2; arrival_refs[i][j][1]=(uint32_t)(uintptr_t)arrival_names[i][j];
            strcpy(arrival_names[i][j],j?"OtherEntry":"DoorEntry");
            *(uint16_t *)(m+0x2c)=(uint16_t)(i*256u+7u);
        }
    }
    /* Ailish in current region0; Tal belongs to region1 even though the
     * native global current area and roster descriptor remain region0. */
    for(unsigned i=2;i<4;++i) {
        roster.actors[i]=actors[i];
        *(void **)(actors[i]+0x74)=membership[i];
        *(void **)membership[i]=mapped+0x2d47bc;
        *(void **)(membership[i]+0x10)=actors[i];
        *(uint16_t *)(membership[i]+0x40)=i==2?0x107:0x008;
        *(void **)(actors[i]+0x80)=movement[i]; *(void **)movement[i]=mapped+0x2c8644;
        *(void **)(movement[i]+0x10)=actors[i];
        *(uint16_t *)(movement[i]+0xbc)=i==2?0x107:0x008;
    }
}
static BOOL observe(void) {
    return SudekiMpLanStoryAreaMembershipObserve((HMODULE)mapped,&witness,&roster,&output);
}
static void refused(void) {
    saved=output; assert(!observe()); assert(!memcmp(&saved,&output,sizeof(output)));
}
static SudekiMpLanStoryAreaDestination destination;
static BOOL resolve(const char *zone,uint32_t id,const char *marker) {
    return SudekiMpLanStoryAreaDestinationResolve((HMODULE)mapped,&witness,&roster,zone,id,marker,&destination);
}
static void destination_refused(const char *zone,uint32_t id,const char *marker) {
    memset(&destination,0xa5,sizeof(destination));
    SudekiMpLanStoryAreaDestination before=destination;
    assert(!resolve(zone,id,marker)); assert(!memcmp(&before,&destination,sizeof(before)));
}
static void destination_tests(void) {
    assert(arrival_image_exact(mapped));
    setup(); assert(resolve("REGION_1",420,"DoorEntry"));
    assert(destination.position[0]==100.0f && destination.position[1]==4.0f && destination.position[2]==-8.0f);
    assert(destination.facing[0]==0 && destination.facing[1]==0 && destination.facing[2]==1);
    assert(destination.sector==0x107 && !strcmp(destination.zone,"region_1"));
    assert(!strcmp(destination.marker,"DoorEntry") && destination.marker_identifier==420);
    assert(destination.dispatch_serial==17 && destination.epoch==4 && destination.revision==6);
    assert(*(void **)(world+0xc)==descriptors[0]); /* No global-current substitution. */
    assert(resolve("region_2",420,"DoorEntry") && destination.position[0]==200.0f);
    assert(resolve("region_0",420,"DoorEntry") && destination.position[0]==0.0f);
    assert(resolve("region_1",421,"OtherEntry") && destination.position[0]==101.0f);
    /* Native IDs repeat across areas; destination namespace is mandatory. */
    destination_refused("",420,"DoorEntry");
    destination_refused("missing",420,"DoorEntry");
    destination_refused("region_1",422,"DoorEntry");
    destination_refused("region_1",420,"OtherEntry");
    destination_refused("region_1",420,"doorentry"); /* Exact authored text, no guessed alias. */
    destination_refused("region_1",0x7ffff,"DoorEntry");
    destination_refused("region_1",UINT32_MAX,"DoorEntry");
    destination_refused(NULL,420,"DoorEntry");
    destination_refused("region_1",420,NULL);
    setup(); memset(arrivals[1][0]+0x14,0,12);
    assert(resolve("region_1",420,"DoorEntry") && destination.facing[2]==0); /* Preserve fallback intent. */
    setup(); *(float *)(arrivals[1][0]+8)=NAN; destination_refused("region_1",420,"DoorEntry");
    setup(); *(float *)(arrivals[1][0]+0x18)=INFINITY; destination_refused("region_1",420,"DoorEntry");
    setup(); *(uint16_t *)(arrivals[1][0]+0x2c)=0x207; destination_refused("region_1",420,"DoorEntry");
    setup(); *(uint16_t *)(arrivals[1][0]+0x2c)=0x1ff; destination_refused("region_1",420,"DoorEntry");
    setup(); *(uint16_t *)(arrivals[1][0]+0x2c)=0x109; destination_refused("region_1",420,"DoorEntry");
    setup(); *(void **)(data[1]+0x28)=NULL; destination_refused("region_1",420,"DoorEntry");
    setup(); *(uint32_t *)navigation[1]=0; destination_refused("region_1",420,"DoorEntry");
    setup(); *(uint32_t *)navigation[1]=MAX_NAV_SECTORS+1; destination_refused("region_1",420,"DoorEntry");
    setup(); *(void **)(navigation[1]+4)=NULL; destination_refused("region_1",420,"DoorEntry");
    setup(); *(uint16_t *)(sectors[1][1]+0xa)=0x107; destination_refused("region_1",420,"DoorEntry");
    setup(); *(uint16_t *)(sectors[1][0]+0xa)=0x207; destination_refused("region_1",420,"DoorEntry");
    setup(); *(uint32_t *)(arrivals[1][1]+0x24)=420; destination_refused("region_1",420,"DoorEntry");
    setup(); strcpy(names[2],names[1]); destination_refused("region_1",420,"DoorEntry");
    setup(); *(void **)(world+0x58+4)=descriptors[2]; destination_refused("region_1",420,"DoorEntry");
    setup(); *(uint32_t *)(descriptors[1]+0x34)=1; destination_refused("region_1",420,"DoorEntry");
    setup(); *(uint32_t *)(descriptors[1]+0x34)=0; destination_refused("region_1",420,"DoorEntry");
    setup(); *(uint32_t *)(descriptors[1]+0x34)=5; destination_refused("region_1",420,"DoorEntry");
    setup(); data[1][0x128]=4; destination_refused("region_1",420,"DoorEntry");
    setup(); *(void **)(descriptors[1]+0x14)=NULL; destination_refused("region_1",420,"DoorEntry");
    setup(); *(uint32_t *)(data[1]+0xc0)=3; destination_refused("region_1",420,"DoorEntry");
    setup(); *(uint32_t *)(data[1]+0xc4)=MAX_ARRIVALS+1; destination_refused("region_1",420,"DoorEntry");
    setup(); *(void **)(data[1]+0xc8)=NULL; destination_refused("region_1",420,"DoorEntry");
    setup(); *(void **)arrivals[1][0]=mapped; destination_refused("region_1",420,"DoorEntry");
    setup(); *(void **)(arrivals[1][0]+4)=mapped; destination_refused("region_1",420,"DoorEntry");
    setup(); *(void **)(arrivals[1][0]+0x28)=NULL; destination_refused("region_1",420,"DoorEntry");
    setup(); arrival_refs[1][0][0]=0; destination_refused("region_1",420,"DoorEntry");
    setup(); arrival_refs[1][0][0]=UINT32_MAX; destination_refused("region_1",420,"DoorEntry");
    setup(); arrival_refs[1][0][1]=0; destination_refused("region_1",420,"DoorEntry");
    setup(); memset(arrival_names[1][0],'x',sizeof(arrival_names[1][0]));
    destination_refused("region_1",420,"DoorEntry");
    setup(); exact=FALSE; destination_refused("region_1",420,"DoorEntry");
    setup(); witness.dispatch_serial++; destination_refused("region_1",420,"DoorEntry");
    setup(); change_kind=5; change_on_check=3; destination_refused("region_1",420,"DoorEntry");
    for(unsigned i=6;i<=16;++i) {
        setup(); change_kind=i; change_on_check=2; destination_refused("region_1",420,"DoorEntry");
    }
    setup(); mapped[0x106c90]^=1; destination_refused("region_1",420,"DoorEntry"); mapped[0x106c90]^=1;
    setup(); mapped[0xf22b3]^=1; destination_refused("region_1",420,"DoorEntry"); mapped[0xf22b3]^=1;
    setup(); void *inaccessible=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_NOACCESS);
    assert(inaccessible); *(void **)(navigation[1]+4)=inaccessible;
    destination_refused("region_1",420,"DoorEntry"); assert(VirtualFree(inaccessible,0,MEM_RELEASE));
    setup(); void *getter=*(void **)(mapped+0x2cddc0); *(void **)(mapped+0x2cddc0)=mapped;
    destination_refused("region_1",420,"DoorEntry"); *(void **)(mapped+0x2cddc0)=getter;
    setup(); uint8_t before_world[sizeof(world)],before_markers[sizeof(arrivals)],before_data[sizeof(data)];
    uint32_t before_refs[3][2][2];
    uint8_t before_navigation[sizeof(navigation)],before_sectors[sizeof(sectors)];
    memcpy(before_navigation,navigation,sizeof(navigation)); memcpy(before_sectors,sectors,sizeof(sectors));
    memcpy(before_world,world,sizeof(world)); memcpy(before_markers,arrivals,sizeof(arrivals));
    memcpy(before_data,data,sizeof(data)); memcpy(before_refs,arrival_refs,sizeof(arrival_refs));
    assert(resolve("region_1",420,"DoorEntry"));
    assert(!memcmp(before_world,world,sizeof(world)) && !memcmp(before_markers,arrivals,sizeof(arrivals)) &&
        !memcmp(before_data,data,sizeof(data)) && !memcmp(before_refs,arrival_refs,sizeof(arrival_refs)) &&
        !memcmp(before_navigation,navigation,sizeof(navigation)) && !memcmp(before_sectors,sectors,sizeof(sectors)));
}
int main(int argc,char **argv) {
    assert(argc==2); wchar_t path[1024];
    assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024)); mapped=map_image(path);
    assert(image_exact(mapped));
    setup(); assert(observe()); assert(output.available_mask==12 && output.epoch==4 && output.revision==6);
    assert(output.members[2].sector==0x107 && !output.members[2].is_current);
    assert(output.members[3].sector==8 && output.members[3].is_current);
    assert(!strcmp(output.members[2].name,"region_1") && output.members[2].zone_flags==0x32);
    assert(output.members[2].collision_present && output.members[2].collision_registration==1);
    assert(output.members[2].collision_enabled && output.members[2].collision_mask==0x20000);
    assert(output.members[2].movement_present && output.members[2].movement_sector_valid &&
        output.members[2].movement_region_matches && output.members[2].movement_sector==0x107);
    SudekiMpLanStoryAreaMember empty={0};
    assert(!memcmp(&output.members[0],&empty,sizeof(empty)) && !memcmp(&output.members[1],&empty,sizeof(empty)));
    /* All four members; two share an area without requiring duplicate loads. */
    for(unsigned i=0;i<2;++i) {
        roster.actors[i]=actors[i]; roster.available_mask|=(uint8_t)(1u<<i);
        *(void **)(actors[i]+0x74)=membership[i]; *(void **)membership[i]=mapped+0x2d47bc;
        *(void **)(membership[i]+0x10)=actors[i]; *(uint16_t *)(membership[i]+0x40)=0x107;
    }
    assert(observe() && output.available_mask==15 && output.members[0].sector==output.members[2].sector);
    setup(); actors[2][0x2b]=3; collision[1][0xf4]=0; collision[1][0xf2]=0;
    *(uint32_t *)(collision[1]+0x44)=0; descriptors[1][0x50]=2;
    assert(observe() && output.members[2].pause_refs==3 && !output.members[2].collision_enabled);
    assert(output.members[2].data_present); /* Resident is not active/playable. */
    setup(); *(void **)(descriptors[1]+0x14)=NULL;
    assert(observe() && !output.members[2].data_present && !output.members[2].collision_present);
    setup(); *(uint32_t *)(descriptors[1]+0x34)=1;
    *(void **)(descriptors[1]+0x14)=(void *)1; *(void **)(descriptors[1]+0x18)=data[1];
    assert(observe() && output.members[2].data_pending && output.members[2].data_present);
    setup(); *(void **)(data[1]+0x2c)=NULL; *(void **)(data[1]+0x28)=NULL;
    assert(observe() && !output.members[2].collision_present && !output.members[2].navigation_present);
    setup(); *(void **)(actors[2]+0x80)=NULL;
    assert(observe() && !output.members[2].movement_present && !output.members[2].movement_sector_valid &&
        !output.members[2].movement_region_matches);
    setup(); *(uint16_t *)(movement[2]+0xbc)=0xffff;
    assert(observe() && output.members[2].movement_present && !output.members[2].movement_sector_valid &&
        !output.members[2].movement_region_matches && output.members[2].movement_sector==0xffff);
    setup(); *(uint16_t *)(movement[2]+0xbc)=0x1ff;
    assert(observe() && !output.members[2].movement_sector_valid);
    setup(); *(uint16_t *)(movement[2]+0xbc)=0xce01;
    assert(observe() && !output.members[2].movement_sector_valid);
    setup(); *(uint16_t *)(movement[2]+0xbc)=0x207; movement[2][0xbe]=4; movement[2][0xbf]=3;
    assert(observe() && output.members[2].movement_sector_valid && !output.members[2].movement_region_matches &&
        output.members[2].movement_flags[0]==4 && output.members[2].movement_flags[1]==3);
    setup(); *(uint16_t *)(movement[2]+0xbc)=0x108;
    assert(observe() && output.members[2].movement_region_matches); /* Region match is NOT sector equality. */
    setup(); *(void **)movement[2]=mapped; refused();
    setup(); *(void **)(movement[2]+0x10)=actors[3]; refused();
    setup(); *(void **)(actors[2]+0x80)=movement[3]; refused();
    for(unsigned i=17;i<=20;++i) { setup(); change_kind=i; change_on_check=2; refused(); }
    setup(); mapped[0xc4698]^=1; refused(); mapped[0xc4698]^=1;
    setup(); mapped[0xc44de]^=1; refused(); mapped[0xc44de]^=1;
    setup(); mapped[0xf5111]^=1; refused(); mapped[0xf5111]^=1;
    setup(); void *movement_slot=*(void **)(mapped+0x2c8680); *(void **)(mapped+0x2c8680)=mapped;
    refused(); *(void **)(mapped+0x2c8680)=movement_slot;
    setup(); *(void **)(world+0x50)=NULL; refused();
    setup(); *(uint32_t *)(world+0x54)=MAX_DESCRIPTORS+1; refused();
    setup(); *(void **)(world+0x58+4)=descriptors[1]+1; refused();
    setup(); *(void **)(world+0x58+4)=descriptors[2]; refused();
    setup(); *(void **)(descriptors[1])=mapped; refused();
    setup(); *(uint32_t *)(descriptors[1]+0x20)=2; refused();
    setup(); *(uint32_t *)(descriptors[1]+0x34)=5; refused();
    setup(); *(uint32_t *)(data[1]+0x24)=2; refused();
    setup(); *(void **)data[1]=mapped; refused();
    setup(); *(void **)(collision[1]+0x50)=descriptors[0]; refused();
    setup(); collision[1][0xf2]=3; refused();
    setup(); collision[1][0xf4]=2; refused();
    setup(); *(void **)(actors[2]+0x74)=NULL; refused();
    setup(); *(void **)membership[2]=mapped; refused();
    setup(); *(void **)(membership[2]+0x10)=actors[3]; refused();
    setup(); *(uint16_t *)(membership[2]+0x40)=0xffff; refused();
    setup(); *(uint16_t *)(membership[2]+0x40)=0xce01; refused();
    setup(); *(uint16_t *)(membership[2]+0x40)=0x1ff; refused();
    setup(); roster.actors[2]=actors[3]; refused();
    setup(); roster.available_mask=8; refused(); /* Pointer in unavailable slot. */
    setup(); roster.available_mask=0x1c; refused();
    setup(); roster.dispatch_serial++; refused();
    setup(); witness.service_post_original_exact=0; refused();
    setup(); exact=FALSE; refused();
    setup(); *(void **)(world+0xc)=descriptors[1]; refused();
    setup(); *(void **)(mapped+0x408d10)=NULL; refused();
    setup(); *(void **)world=mapped; refused();
    setup(); memset(names[1],'a',sizeof(names[1])); refused();
    setup(); names[1][0]='.'; refused();
    setup(); names[1][0]=0; refused();
    setup(); *(void **)(descriptors[1]+0x24)=NULL; refused();
    for(unsigned i=0;i<6;++i) {setup(); change_kind=i; change_on_check=2; refused();}
    setup(); change_kind=5; change_on_check=3; refused();
    setup(); mapped[0xef640]^=1; refused(); mapped[0xef640]^=1;
    setup(); mapped[0x10b4d9]^=1; refused(); mapped[0x10b4d9]^=1;
    setup(); void *slot=*(void **)(mapped+0x2d47d8); *(void **)(mapped+0x2d47d8)=mapped; refused();
    *(void **)(mapped+0x2d47d8)=slot;
    setup(); LONG lfanew=((IMAGE_DOS_HEADER *)mapped)->e_lfanew;
    ((IMAGE_DOS_HEADER *)mapped)->e_lfanew=-1; refused();
    ((IMAGE_DOS_HEADER *)mapped)->e_lfanew=0x7fffffff; refused();
    ((IMAGE_DOS_HEADER *)mapped)->e_lfanew=lfanew;
    setup(); void *inaccessible=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_NOACCESS);
    assert(inaccessible); *(void **)(data[1]+0x2c)=inaccessible;
    refused(); setup(); *(void **)(actors[2]+0x80)=inaccessible; refused();
    assert(VirtualFree(inaccessible,0,MEM_RELEASE));
    setup(); uint8_t saved_world[sizeof(world)],saved_actors[sizeof(actors)],saved_collision[sizeof(collision)];
    memcpy(saved_world,world,sizeof(world)); memcpy(saved_actors,actors,sizeof(actors));
    memcpy(saved_collision,collision,sizeof(collision));
    uint8_t saved_movement[sizeof(movement)]; memcpy(saved_movement,movement,sizeof(movement));
    assert(observe()); assert(!memcmp(saved_world,world,sizeof(world)));
    assert(!memcmp(saved_actors,actors,sizeof(actors)) && !memcmp(saved_collision,collision,sizeof(collision)));
    assert(!memcmp(saved_movement,movement,sizeof(movement)));
    assert(!SudekiMpLanStoryAreaMembershipObserve(NULL,&witness,&roster,&output));
    assert(!SudekiMpLanStoryAreaMembershipObserve((HMODULE)mapped,NULL,&roster,&output));
    assert(!SudekiMpLanStoryAreaMembershipObserve((HMODULE)mapped,&witness,NULL,&output));
    assert(!SudekiMpLanStoryAreaMembershipObserve((HMODULE)mapped,&witness,&roster,NULL));
    destination_tests();
    VirtualFree(mapped,0,MEM_RELEASE);
    puts("story area membership and explicit destination exact-image/synthetic-topology tests passed (no native gameplay)");
    return 0;
}
