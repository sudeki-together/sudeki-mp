/* Retail image is mapped non-executable; only copied native layouts are read. */
#include "hooks/lan_story_collision_owner.h"
#include "engine/build_identity.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *mapped;
static uint8_t world[0x390],descriptors[2][0x54],data[2][0x12a];
static uint8_t system_data[0x8c],grid[0x15ac],sources[5][0x110];
static uint8_t registry[0x40],entities[2][0x314],positions[2][0x110],children[17][0x110];
static uint8_t catalogs[2][0x1c],spawns[2][2][0x90];
static uint32_t names[4][2];
static char resource_names[2][128],anchor_names[2][128];
static SudekiMpLanStoryCollisionAuthoredOrigin origin;
static void *primary[4],*secondary[4],*grid_items[8],*entity_items[4];
static SudekiMpLanStoryNativeRoster roster;
static SudekiMpControlUpdateDispatchWitness witness;
static SudekiMpLanStoryCollisionOwner result;
static BOOL exact;
static unsigned checks,change_on_check,change_kind;
BOOL SudekiMpLanStoryObserverRosterStillExact(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r) {
    assert(w==&witness && r==&roster);
    if(++checks==change_on_check) switch(change_kind) {
    case 0: *(void **)(sources[1]+8)=sources[4]; break;
    case 1: primary[0]=sources[4]; break;
    case 2: grid_items[1]=sources[4]; break;
    case 3: *(void **)(sources[1]+0x100)=positions[1]+4; break;
    case 4: *(void **)(positions[0]+0x8c)=sources[4]; break;
    case 5: entity_items[0]=entities[1]; break;
    case 6: ++*(uint32_t *)(entities[0]+0x34); break;
    case 7: sources[1][0xf4]=0; break;
    case 8: *(void **)(children[0]+0x94)=positions[1]+4; break;
    case 9: *(void **)(data[1]+0x2c)=sources[4]; break;
    case 10: *(void **)(mapped+0x408dd4)=sources[4]; break;
    case 11: *(void **)(mapped+0x409d8c)=sources[4]; break;
    case 12: ++*(uint32_t *)(grid+0x15a8); break;
    case 13: *(void **)(sources[1]+0x50)=sources[4]; break;
    case 14: *(void **)(positions[0]+4)=mapped; break;
    case 15: *(void **)(spawns[0][0]+0x78)=NULL; break;
    case 16: memcpy(spawns[1][1],spawns[0][0],0x90); break;
    case 17: *(uint32_t *)(catalogs[0]+0x10)=1; break;
    case 18: *(void **)(data[0]+0x118)=catalogs[1]; break;
    case 19: ++*(uint32_t *)(spawns[0][0]+0x54); break;
    case 20: resource_names[0][0]='x'; break;
    case 21: anchor_names[0][0]='x'; break;
    case 22: *(void **)(children[0]+0x94)=entities[1]+0x154; break;
    case 23: *(uint32_t *)(descriptors[0]+0x34)=3; break;
    case 24: data[0][0x129]^=1; break;
    case 25: *(void **)(entities[0]+8)=mapped; break;
    case 26: *(void **)(world+0x58)=NULL; break;
    case 27: *(void **)(entities[0]+0x150+0x94)=positions[0]+4; break;
    case 28: memcpy(spawns[1][1],spawns[0][0],0x90); *(void **)(spawns[0][0]+0x78)=NULL; break;
    default: return FALSE;
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
static void position_init(uint8_t *p) {
    *(void **)p=mapped+0x2cdefc; *(void **)(p+4)=mapped+0x2cdf3c;
}
static void setup(void) {
    memset(world,0,sizeof(world)); memset(descriptors,0,sizeof(descriptors)); memset(data,0,sizeof(data));
    memset(system_data,0,sizeof(system_data)); memset(grid,0,sizeof(grid)); memset(sources,0,sizeof(sources));
    memset(registry,0,sizeof(registry)); memset(entities,0,sizeof(entities)); memset(positions,0,sizeof(positions));
    memset(children,0,sizeof(children)); memset(primary,0,sizeof(primary)); memset(secondary,0,sizeof(secondary));
    memset(grid_items,0,sizeof(grid_items)); memset(entity_items,0,sizeof(entity_items));
    memset(&witness,0,sizeof(witness)); memset(&roster,0,sizeof(roster)); memset(&result,0xa5,sizeof(result));
    checks=change_kind=change_on_check=0; exact=TRUE;
    witness.service_post_original_exact=1; witness.dispatch_serial=7;
    roster.dispatch_serial=7; roster.epoch=9; roster.revision=11; roster.world=world; roster.descriptor=descriptors[0];
    *(void **)(mapped+0x408d10)=world; *(void **)(mapped+0x408dd4)=system_data; *(void **)(mapped+0x409d8c)=registry;
    *(void **)world=mapped+0x2c4c3c; *(void **)(world+0x50)=descriptors; *(uint32_t *)(world+0x54)=2;
    *(void **)(world+0xc)=descriptors[0];
    for(unsigned i=0;i<2;++i) {
        *(void **)(world+0x58+4*i)=descriptors[i]; *(void **)descriptors[i]=mapped+0x2c82a4;
        *(uint32_t *)(descriptors[i]+0x20)=i; *(uint32_t *)(descriptors[i]+0x34)=2;
        *(void **)(descriptors[i]+0x14)=data[i]; *(void **)data[i]=mapped+0x2cdcdc;
        *(uint32_t *)(data[i]+0x24)=i;
        position_init(positions[i]); *(void **)(positions[i]+0x10)=entities[i];
        *(void **)(entities[i]+0x44)=positions[i]; *(uint32_t *)(entities[i]+0x34)=123+i;
        entity_items[i]=entities[i];
    }
    *(void **)(data[1]+0x2c)=sources[0]; *(void **)(sources[0]+0x50)=descriptors[1];
    *(void **)(system_data+0x30)=grid;
    *(uint32_t *)(system_data+0x70)=2; *(void **)(system_data+0x78)=primary;
    *(uint32_t *)(system_data+0x80)=2; *(void **)(system_data+0x88)=secondary;
    primary[0]=sources[1]; primary[1]=sources[2]; secondary[0]=sources[0]; secondary[1]=sources[3];
    *(void **)(grid+0x15a0)=grid_items; *(uint32_t *)(grid+0x15a4)=4; *(uint32_t *)(grid+0x15a8)=8;
    *(uint32_t *)(registry+0x34)=2; *(void **)(registry+0x3c)=entity_items;
    for(unsigned i=0;i<4;++i) {
        grid_items[i]=sources[i]; *(void **)(sources[i]+8)=grid; sources[i][0xf4]=1;
        sources[i][0xf2]=(i==1 || i==2)?2:1;
    }
    *(void **)(sources[1]+0x100)=positions[0]+4; *(void **)(positions[0]+0x8c)=sources[1];
    position_init(children[0]); *(void **)(children[0]+0x8c)=sources[2];
    *(void **)(children[0]+0x94)=positions[0]+4; *(void **)(sources[2]+0x100)=children[0]+4;
    *(void **)(sources[3]+0x100)=positions[1]+4; *(void **)(positions[1]+0x8c)=sources[3];
    /* Trigger-style auxiliary is NOT a ZoneInfo pointer. Do not dereference it. */
    *(void **)(sources[3]+0x50)=(void *)1; *(uint32_t *)(sources[3]+0x44)=0x50000;
}
static BOOL resolve(const void *source) {
    return SudekiMpLanStoryCollisionOwnerResolve((HMODULE)mapped,&witness,&roster,source,&result);
}
static void refused(const void *source) {
    SudekiMpLanStoryCollisionOwner before=result;
    assert(!resolve(source)); assert(!memcmp(&before,&result,sizeof(result)));
}
static void successful_owners(void) {
    setup(); assert(resolve(sources[0]));
    assert(result.kind==SUDEKIMP_STORY_COLLISION_TERRAIN && result.owner==descriptors[1] && result.region==1);
    assert(result.source==sources[0] && result.epoch==9 && result.revision==11 && result.dispatch_serial==7);
    assert(result.registration==1 && result.enabled && !result.parent_depth);
    assert(*(void **)(world+0xc)==descriptors[0]); /* Noncurrent terrain, no global switch. */
    assert(resolve(sources[1]) && result.kind==SUDEKIMP_STORY_COLLISION_ENTITY && result.owner==entities[0]);
    assert(result.region==UINT32_MAX && result.resource_identifier==123 && !result.parent_depth);
    assert(resolve(sources[2]) && result.owner==entities[0] && result.parent_depth==1);
    assert(resolve(sources[3]) && result.kind==SUDEKIMP_STORY_COLLISION_ENTITY && result.owner==entities[1]);
    sources[1][0xf4]=0; assert(resolve(sources[1]) && !result.enabled); /* Ownership is not activation. */
    /* Native collision ownership is independent of party roster availability. */
    assert(roster.available_mask==0 && result.region==UINT32_MAX);
}
static void malformed_and_stale(void) {
    setup(); refused(NULL); refused((void *)1); refused(sources[4]);
    setup(); primary[1]=sources[1]; refused(sources[1]);
    setup(); secondary[1]=sources[1]; refused(sources[1]);
    setup(); grid_items[3]=sources[1]; refused(sources[1]);
    setup(); grid_items[1]=sources[4]; refused(sources[1]);
    setup(); *(uint32_t *)(grid+0x15a8)=3; refused(sources[1]);
    setup(); *(uint32_t *)(grid+0x15a8)=8193; refused(sources[1]);
    setup(); *(uint32_t *)(system_data+0x70)=8193; refused(sources[1]);
    setup(); *(void **)(system_data+0x78)=NULL; refused(sources[1]);
    setup(); *(void **)(sources[1]+8)=sources[4]; refused(sources[1]);
    setup(); sources[1][0xf2]=1; refused(sources[1]);
    setup(); sources[1][0xf2]=0; refused(sources[1]);
    setup(); sources[1][0xf4]=2; refused(sources[1]);
    setup(); *(void **)(sources[1]+0x100)=NULL; refused(sources[1]);
    setup(); *(void **)(sources[1]+0x100)=(void *)4; refused(sources[1]);
    setup(); *(void **)positions[0]=mapped; refused(sources[1]);
    setup(); *(void **)(positions[0]+4)=mapped; refused(sources[1]);
    setup(); *(void **)(positions[0]+0x8c)=sources[4]; refused(sources[1]);
    setup(); *(void **)(entities[0]+0x44)=positions[1]; refused(sources[1]);
    setup(); entity_items[1]=entities[0]; refused(sources[1]);
    setup(); *(uint32_t *)(registry+0x34)=0; refused(sources[1]);
    setup(); *(void **)(children[0]+0x94)=children[0]+4; refused(sources[2]);
    setup(); *(void **)(children[0]+0x94)=(void *)4; refused(sources[2]);
    setup(); *(void **)(children[0]+0x94)=NULL; refused(sources[2]);
    setup(); *(void **)(data[1]+0x2c)=sources[4]; refused(sources[0]);
    setup(); *(uint32_t *)(descriptors[1]+0x34)=1; refused(sources[0]);
    setup(); *(void **)(world+0x58+4)=descriptors[0]; refused(sources[0]);
    setup(); *(void **)(world+0xc)=descriptors[1]; refused(sources[1]);
    setup(); *(uint32_t *)(world+0x54)=4097; refused(sources[1]);
    setup(); witness.dispatch_serial++; refused(sources[1]);
    setup(); witness.service_post_original_exact=0; refused(sources[1]);
    setup(); exact=FALSE; refused(sources[1]);
    for(unsigned i=0;i<15;++i) {
        setup(); change_kind=i; change_on_check=2;
        refused(sources[i==8?2:(i==9?0:1)]);
    }
    setup(); change_kind=99; change_on_check=3; refused(sources[1]);
    /* Bound parent traversal without truncating a chain into a guessed owner. */
    setup();
    for(unsigned i=0;i<16;++i) {
        position_init(children[i]); *(void **)(children[i]+0x94)=children[i+1]+4;
    }
    *(void **)(children[14]+0x94)=positions[0]+4;
    assert(resolve(sources[2]) && result.parent_depth==15);
    *(void **)(children[14]+0x94)=children[15]+4; *(void **)(children[15]+0x94)=positions[0]+4;
    refused(sources[2]);
    void *inaccessible=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_NOACCESS); assert(inaccessible);
    setup(); *(void **)(sources[1]+0x100)=(uint8_t *)inaccessible+4; refused(sources[1]);
    setup(); *(void **)(system_data+0x78)=inaccessible; refused(sources[1]);
    setup(); *(void **)(registry+0x3c)=inaccessible; refused(sources[1]);
    setup(); *(void **)(grid+0x15a0)=inaccessible; refused(sources[1]);
    assert(VirtualFree(inaccessible,0,MEM_RELEASE));
}
static void image_and_readonly(void) {
    static const unsigned targets[]={0x13de33,0x13de4e,0x318eb,0x318f3,0x31903,0x3190b,
        0x3107f,0x31092,0x11230d,0x112321,0x32339,0x32345,0x110628,0x2cdefc};
    for(unsigned i=0;i<sizeof(targets)/sizeof(targets[0]);++i) {
        setup(); mapped[targets[i]]^=1; refused(sources[1]); mapped[targets[i]]^=1;
    }
    setup(); LONG lfanew=((IMAGE_DOS_HEADER *)mapped)->e_lfanew;
    ((IMAGE_DOS_HEADER *)mapped)->e_lfanew=-1; refused(sources[1]);
    ((IMAGE_DOS_HEADER *)mapped)->e_lfanew=0x7fffffff; refused(sources[1]);
    ((IMAGE_DOS_HEADER *)mapped)->e_lfanew=lfanew;
    setup(); uint8_t saved_sources[sizeof(sources)],saved_positions[sizeof(positions)],saved_children[sizeof(children)];
    uint8_t saved_grid[sizeof(grid)],saved_world[sizeof(world)];
    memcpy(saved_sources,sources,sizeof(sources)); memcpy(saved_positions,positions,sizeof(positions));
    memcpy(saved_children,children,sizeof(children)); memcpy(saved_grid,grid,sizeof(grid)); memcpy(saved_world,world,sizeof(world));
    assert(resolve(sources[0]) && resolve(sources[1]) && resolve(sources[2]) && resolve(sources[3]));
    assert(!memcmp(saved_sources,sources,sizeof(sources)) && !memcmp(saved_positions,positions,sizeof(positions)) &&
        !memcmp(saved_children,children,sizeof(children)) && !memcmp(saved_grid,grid,sizeof(grid)) &&
        !memcmp(saved_world,world,sizeof(world)));
    assert(!SudekiMpLanStoryCollisionOwnerResolve(NULL,&witness,&roster,sources[1],&result));
    assert(!SudekiMpLanStoryCollisionOwnerResolve((HMODULE)mapped,NULL,&roster,sources[1],&result));
    assert(!SudekiMpLanStoryCollisionOwnerResolve((HMODULE)mapped,&witness,NULL,sources[1],&result));
    assert(!SudekiMpLanStoryCollisionOwnerResolve((HMODULE)mapped,&witness,&roster,sources[1],NULL));
}
static void origin_class(unsigned entity_index,unsigned kind) {
    static const unsigned classes[3][5]={
        {0x2c8a8c,0x2c8ab0,0x2c8ad0,0x150,0xf84},
        {0x2d5b00,0x2d5b24,0x2d5b44,0x210,0xf8f},
        {0x2d65a0,0x2d65c4,0x2d65e4,0x150,0xf9b}
    };
    uint8_t *e=entities[entity_index],*p=e+classes[kind][3];
    *(void **)e=mapped+classes[kind][0]; *(void **)(e+8)=mapped+classes[kind][1];
    *(void **)(e+0x2c)=mapped+classes[kind][2]; *(uint32_t *)(e+0x30)=classes[kind][4];
    *(void **)(e+0x38)=names[entity_index]; *(void **)(e+0x44)=p;
    position_init(p); *(void **)(p+0x10)=e;
    uint8_t *source=sources[entity_index?3:1];
    *(void **)(p+0x8c)=source; *(void **)(source+0x100)=p+4;
    if(!entity_index) *(void **)(children[0]+0x94)=p+4;
    *(uint32_t *)(spawns[entity_index][0]+0x6c)=classes[kind][4];
}
static void origin_setup(void) {
    setup(); memset(catalogs,0,sizeof(catalogs)); memset(spawns,0,sizeof(spawns));
    memset(names,0,sizeof(names)); memset(resource_names,0,sizeof(resource_names));
    memset(anchor_names,0,sizeof(anchor_names)); memset(&origin,0xa5,sizeof(origin));
    for(unsigned i=0;i<2;++i) {
        strcpy(resource_names[i],i?"resident_npc":"resident_barrel");
        strcpy(anchor_names[i],i?"church_anchor":"exterior_anchor");
        names[i][0]=names[i+2][0]=1;
        names[i][1]=(uint32_t)(uintptr_t)resource_names[i];
        names[i+2][1]=(uint32_t)(uintptr_t)anchor_names[i];
        *(void **)(descriptors[i]+0x24)=i?"CHURCH":"brightwater";
        *(void **)(data[i]+0x118)=catalogs[i]; data[i][0x128]=5;
        *(uint32_t *)(catalogs[i]+0x10)=2; *(void **)(catalogs[i]+0x18)=spawns[i];
        for(unsigned j=0;j<2;++j) {
            *(void **)spawns[i][j]=mapped+0x2cddf4; *(void **)(spawns[i][j]+4)=mapped+0x2cde04;
        }
        *(uint32_t *)(spawns[i][0]+0x70)=123+i; *(void **)(spawns[i][0]+0x74)=names[i];
        *(uint32_t *)(spawns[i][0]+0x54)=456+i; *(void **)(spawns[i][0]+0x58)=names[i+2];
        *(void **)(spawns[i][0]+0x78)=entities[i]+0x2c;
        origin_class(i,i?2:0);
    }
}
static BOOL origin_resolve(const void *source) {
    return SudekiMpLanStoryCollisionAuthoredOriginResolve((HMODULE)mapped,&witness,&roster,source,&origin);
}
static void origin_refused(const void *source) {
    SudekiMpLanStoryCollisionAuthoredOrigin before=origin;
    assert(!origin_resolve(source)); assert(!memcmp(&before,&origin,sizeof(origin)));
}
static void authored_origins(void) {
    origin_setup(); assert(origin_resolve(sources[1]));
    assert(origin.region==0 && origin.collision.region==UINT32_MAX && origin.resource_kind==0xf84);
    assert(origin.descriptor==descriptors[0] && origin.data==data[0] && origin.spawn==spawns[0][0]);
    assert(origin.spawn_index==0 && origin.anchor_identifier==456 && !strcmp(origin.zone,"brightwater"));
    assert(!strcmp(origin.resource,"resident_barrel") && !strcmp(origin.anchor,"exterior_anchor"));
    assert(origin_resolve(sources[3]) && origin.region==1 && !strcmp(origin.zone,"church"));
    assert(*(void **)(world+0xc)==descriptors[0]); /* Noncurrent resident origin. */
    assert(origin_resolve(sources[2]) && origin.region==0 && origin.collision.parent_depth==1);
    /* Reparenting an existing source changes its origin WITHOUT allocation. */
    *(void **)(children[0]+0x94)=entities[1]+0x154;
    assert(origin_resolve(sources[2]) && origin.region==1 && origin.collision.owner==entities[1]);
    for(unsigned kind=0;kind<3;++kind) {
        origin_setup(); origin_class(0,kind);
        assert(origin_resolve(sources[1]) && origin_resolve(sources[2]));
    }
    origin_setup(); sources[1][0xf4]=0; assert(origin_resolve(sources[1])); /* Not active-area proof. */
    origin_setup(); *(uint32_t *)(descriptors[1]+0x34)=0; *(void **)(descriptors[1]+0x14)=NULL;
    assert(origin_resolve(sources[1]));
    origin_setup(); *(uint32_t *)(catalogs[1]+0x10)=0; *(void **)(catalogs[1]+0x18)=NULL;
    assert(origin_resolve(sources[1]));
    origin_setup(); memcpy(spawns[1][1],spawns[0][0],0x90); *(void **)(spawns[0][0]+0x78)=NULL;
    assert(origin_resolve(sources[1]) && origin.region==1 && origin.spawn_index==1);
    origin_setup(); origin_refused(sources[0]); origin_refused(NULL); origin_refused((void *)1);
    origin_setup(); roster.actors[0]=entities[0]; roster.available_mask=1; origin_refused(sources[1]);
    origin_setup(); *(void **)entities[0]=mapped+0x2d555c; origin_refused(sources[1]); /* PC, never origin membership. */
    origin_setup(); *(void **)(entities[0]+0x2c)=mapped; origin_refused(sources[1]);
    origin_setup(); *(uint32_t *)(entities[0]+0x30)=0xf81; origin_refused(sources[1]);
    origin_setup(); *(void **)(entities[0]+0x44)=positions[0]; origin_refused(sources[1]);
    origin_setup(); *(void **)(entities[0]+0x150+0x94)=positions[0]+4; origin_refused(sources[1]);
    origin_setup(); *(void **)(spawns[0][0]+0x78)=NULL; origin_refused(sources[1]);
    origin_setup(); *(void **)(spawns[0][0]+0x78)=entities[1]+0x2c; origin_refused(sources[1]);
    origin_setup(); memcpy(spawns[1][1],spawns[0][0],0x90); origin_refused(sources[1]);
    origin_setup(); memcpy(spawns[0][1],spawns[0][0],0x90); origin_refused(sources[1]);
    origin_setup(); *(void **)(spawns[0][0]+0x74)=names[1]; origin_refused(sources[1]);
    origin_setup(); ++*(uint32_t *)(spawns[0][0]+0x70); origin_refused(sources[1]);
    origin_setup(); ++*(uint32_t *)(spawns[0][0]+0x6c); origin_refused(sources[1]);
    origin_setup(); names[0][0]=0; origin_refused(sources[1]);
    origin_setup(); names[2][0]=UINT32_MAX; origin_refused(sources[1]);
    origin_setup(); memset(resource_names[0],'a',128); origin_refused(sources[1]);
    origin_setup(); resource_names[0][0]=0; origin_refused(sources[1]);
    origin_setup(); *(void **)(descriptors[0]+0x24)="bad/zone"; origin_refused(sources[1]);
    origin_setup(); *(uint32_t *)(catalogs[0]+0x10)=4097; origin_refused(sources[1]);
    origin_setup(); *(void **)spawns[1][1]=mapped; origin_refused(sources[1]);
    origin_setup(); *(void **)(spawns[1][1]+4)=mapped; origin_refused(sources[1]);
    origin_setup(); *(void **)(world+0x58+4)=descriptors[0]; origin_refused(sources[1]);
    origin_setup(); data[1][0x128]=4; origin_refused(sources[1]);
    origin_setup(); *(uint32_t *)(descriptors[1]+0x34)=1; origin_refused(sources[1]);
    origin_setup(); *(uint32_t *)(descriptors[1]+0x34)=0; origin_refused(sources[1]);
    origin_setup(); *(uint32_t *)(descriptors[1]+0x34)=5; origin_refused(sources[1]);
    for(unsigned i=15;i<=28;++i) {
        origin_setup(); change_kind=i; change_on_check=2; origin_refused(sources[2]);
    }
    origin_setup(); change_kind=99; change_on_check=3; origin_refused(sources[1]);
    static const unsigned signatures[]={0x13cf9f,0x13cf16,0x3a490,0x2c8ad0,0x2d5b44,
        0x2d65e4,0x2c8adc,0x2d5b50,0x2d65f0,0x2cddf4,0x2cddf8,0x2cddfc};
    for(unsigned i=0;i<sizeof(signatures)/sizeof(signatures[0]);++i) {
        origin_setup(); mapped[signatures[i]]^=1; origin_refused(sources[1]); mapped[signatures[i]]^=1;
    }
    void *inaccessible=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_NOACCESS); assert(inaccessible);
    origin_setup(); *(void **)(catalogs[0]+0x18)=inaccessible; origin_refused(sources[1]);
    origin_setup(); *(void **)(data[0]+0x118)=inaccessible; origin_refused(sources[1]);
    origin_setup(); names[0][1]=(uint32_t)(uintptr_t)inaccessible; origin_refused(sources[1]);
    assert(VirtualFree(inaccessible,0,MEM_RELEASE));
    origin_setup(); uint8_t saved[sizeof(spawns)+sizeof(entities)+sizeof(data)+sizeof(names)];
    memcpy(saved,spawns,sizeof(spawns)); memcpy(saved+sizeof(spawns),entities,sizeof(entities));
    memcpy(saved+sizeof(spawns)+sizeof(entities),data,sizeof(data));
    memcpy(saved+sizeof(spawns)+sizeof(entities)+sizeof(data),names,sizeof(names));
    assert(origin_resolve(sources[1]) && origin_resolve(sources[2]) && origin_resolve(sources[3]));
    assert(!memcmp(saved,spawns,sizeof(spawns)) && !memcmp(saved+sizeof(spawns),entities,sizeof(entities)) &&
        !memcmp(saved+sizeof(spawns)+sizeof(entities),data,sizeof(data)) &&
        !memcmp(saved+sizeof(spawns)+sizeof(entities)+sizeof(data),names,sizeof(names)));
    assert(!SudekiMpLanStoryCollisionAuthoredOriginResolve(NULL,&witness,&roster,sources[1],&origin));
    assert(!SudekiMpLanStoryCollisionAuthoredOriginResolve((HMODULE)mapped,NULL,&roster,sources[1],&origin));
    assert(!SudekiMpLanStoryCollisionAuthoredOriginResolve((HMODULE)mapped,&witness,NULL,sources[1],&origin));
    assert(!SudekiMpLanStoryCollisionAuthoredOriginResolve((HMODULE)mapped,&witness,&roster,sources[1],NULL));
}
static void origin_catalog_bounds(void) {
    origin_setup();
    uint8_t area_descriptors[5][0x54]={{0}},area_data[5][0x12a]={{0}},area_catalogs[5][0x1c]={{0}};
    uint8_t *rows=calloc(4096,0x90); assert(rows);
    for(unsigned i=0;i<4096;++i) {
        *(void **)(rows+i*0x90)=mapped+0x2cddf4;
        *(void **)(rows+i*0x90+4)=mapped+0x2cde04;
    }
    memcpy(area_descriptors[0],descriptors[0],0x54);
    roster.descriptor=area_descriptors[0]; *(void **)(world+0xc)=area_descriptors[0];
    *(void **)(world+0x50)=area_descriptors; *(uint32_t *)(world+0x54)=5;
    for(unsigned i=0;i<5;++i) {
        *(void **)(world+0x58+4*i)=area_descriptors[i];
        if(!i) continue;
        *(void **)area_descriptors[i]=mapped+0x2c82a4;
        *(uint32_t *)(area_descriptors[i]+0x20)=i; *(uint32_t *)(area_descriptors[i]+0x34)=2;
        *(void **)(area_descriptors[i]+0x14)=area_data[i];
        *(void **)area_data[i]=mapped+0x2cdcdc; *(uint32_t *)(area_data[i]+0x24)=i;
        *(void **)(area_data[i]+0x118)=area_catalogs[i]; area_data[i][0x128]=5;
        *(uint32_t *)(area_catalogs[i]+0x10)=i==4?4094:4096;
        *(void **)(area_catalogs[i]+0x18)=rows;
    }
    assert(origin_resolve(sources[1])); /* Exactly 16384 rows, including empty spawns. */
    ++*(uint32_t *)(area_catalogs[4]+0x10); origin_refused(sources[1]);
    free(rows); setup();
}
int main(int argc,char **argv) {
    assert(argc==2); wchar_t path[1024]; assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024));
    mapped=map_image(path); successful_owners(); malformed_and_stale(); image_and_readonly();
    authored_origins(); origin_catalog_bounds();
    assert(VirtualFree(mapped,0,MEM_RELEASE));
    puts("story collision owner/authored-origin exact-image/synthetic tests passed (no native query, filter or travel)");
    return 0;
}
