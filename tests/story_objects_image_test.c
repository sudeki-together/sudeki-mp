/* Inert exact-image map and synthetic native object topology. No retail
 * constructor, animation, controller or world lifecycle executes. */
#include "../src/hooks/lan_story_objects.c"
#include <assert.h>
#include <stdio.h>

static uint8_t *mapped;
static uint8_t registry[0x40],entities[3][0xa00],wrappers[3][0x14],objects[3][0x38],renderers[3][4];
static uint32_t names[3][2];
static uint32_t anchor_refs[3][2];
static char anchor_text[3][24];
static uint8_t world[0x390],zone_owner[0x54],zone_data[0x12a],zone_descriptor[0x1c],spawns[3][0x90];
static void *entries[3];
static BOOL witness_exact=TRUE;
static unsigned checks,change_on_check;
static SudekiMpLanStoryObjectSnapshot output,saved;
BOOL SudekiMpLanStoryObserverRosterStillExact(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r) {
    (void)w;(void)r;
    if(++checks==change_on_check) *(uint32_t *)(registry+0x34)=1;
    return witness_exact;
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
    memset(entities,0,sizeof(entities)); memset(wrappers,0,sizeof(wrappers));
    memset(objects,0,sizeof(objects)); memset(renderers,0,sizeof(renderers));
    memset(registry,0,sizeof(registry)); checks=change_on_check=0; witness_exact=TRUE;
    memset(world,0,sizeof(world)); memset(zone_owner,0,sizeof(zone_owner)); memset(zone_data,0,sizeof(zone_data));
    memset(zone_descriptor,0,sizeof(zone_descriptor)); memset(spawns,0,sizeof(spawns));
    *(void **)(mapped+0x408d10)=world; *(void **)(world+0x58+4)=zone_owner;
    *(void **)zone_owner=mapped+0x2c82a4; *(void **)(zone_owner+0x14)=zone_data;
    *(void **)zone_data=mapped+0x2cdcdc; *(uint32_t *)(zone_data+0x24)=1;
    *(void **)(zone_data+0x118)=zone_descriptor;
    *(uint32_t *)(zone_descriptor+0x10)=3; *(void **)(zone_descriptor+0x18)=spawns;
    *(void **)(mapped+0x409d8c)=registry; *(void ***)(registry+0x3c)=entries;
    *(uint32_t *)(registry+0x34)=3;
    for(unsigned i=0;i<3;++i) {
        uint8_t *e=entities[i],*p=e+0x150,*c=e+0x70c,*m=e+0x3a4;
        entries[i]=e; *(void **)e=mapped+0x2c8a8c;
        *(void **)(e+8)=mapped+0x2c8ab0; *(void **)(e+0x2c)=mapped+0x2c8ad0;
        *(uint32_t *)(e+0x34)=123; *(void **)(e+0x38)=names[i];
        names[i][0]=3; names[i][1]=(uint32_t)(uintptr_t)"Barrel_Type_A";
        *(void **)(e+0x44)=p; *(void **)(e+0x48)=c; *(void **)(e+0x128)=c;
        *(void **)(e+0x58)=m; *(void **)(e+0x130)=m;
        *(void **)p=mapped+0x2cdefc; *(void **)(p+0x10)=e;
        *(void **)c=mapped+0x2c869c; *(void **)(c+0x10)=e;
        *(void **)m=mapped+0x2c8504; *(void **)(m+0x10)=e;
        *(float *)(p+0x18)=(float)i*10; *(float *)(p+0x58)=1;
        *(void **)(p+0xb4)=wrappers[i]; *(void **)wrappers[i]=mapped+0x2d1df0;
        *(void **)(wrappers[i]+8)=objects[i]; *(void **)(wrappers[i]+0x10)=renderers[i];
        *(void **)objects[i]=mapped+0x2dd700; *(void **)(objects[i]+0x14)=renderers[i];
        *(void **)renderers[i]=mapped+0x2df8ec;
        uint8_t *s=spawns[i];
        *(void **)s=mapped+0x2cddf4; *(void **)(s+4)=mapped+0x2cde04;
        *(uint32_t *)(s+0x6c)=0xf84; *(uint32_t *)(s+0x70)=123; *(void **)(s+0x74)=names[i];
        *(void **)(s+0x78)=e+0x2c;
        snprintf(anchor_text[i],sizeof(anchor_text[i]),"Spawn%u",i);
        anchor_refs[i][0]=3; anchor_refs[i][1]=(uint32_t)(uintptr_t)anchor_text[i];
        *(uint32_t *)(s+0x54)=100+i; *(void **)(s+0x58)=anchor_refs[i];
        *(float *)(s+0x40)=(float)i*10; *(float *)(s+0x38)=1;
    }
}
static BOOL observe(void) {
    SudekiMpControlUpdateDispatchWitness w={.service_post_original_exact=1};
    SudekiMpLanStoryNativeRoster r={.world=world};
    return SudekiMpLanStoryObjectsObserve((HMODULE)mapped,&w,&r,&output);
}
static void refused(void) {
    saved=output; assert(!observe()); assert(!memcmp(&saved,&output,sizeof(output)));
}
int main(int argc,char **argv) {
    assert(argc==2); wchar_t path[1024];
    assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024)); mapped=map_image(path);
    assert(image_exact(mapped));
    setup(); assert(observe() && output.count==3);
    assert(output.placements[0].resource==output.placements[1].resource);
    assert(memcmp(output.placements[0].position,output.placements[1].position,12));
    uint8_t saved_entities[sizeof(entities)]; memcpy(saved_entities,entities,sizeof(entities));
    assert(observe() && !memcmp(saved_entities,entities,sizeof(entities))); /* read-only */
    setup(); entries[1]=entries[0]; refused();
    setup(); *(float *)(entities[1]+0x150+0x18)=1000; assert(observe());
    assert(output.placements[1].position[0]==0x41200000u); /* authored x=10, not current x=1000 */
    setup(); *(uint32_t *)(spawns[1]+0x54)=100; *(void **)(spawns[1]+0x58)=anchor_refs[0]; refused();
    setup(); *(void **)(entities[1]+0x70c+0x10)=entities[0]; refused();
    setup(); *(uint32_t *)(entities[1]+0x70c+0x7c)=4;
    assert(observe() && output.count==3 && output.native_phase[1]==4); /* no identity change */
    setup(); *(uint32_t *)(entities[1]+0x70c+0x7c)=6; refused();
    setup(); *(void **)(spawns[1]+0x78)=NULL; entries[1]=entities[2]; *(uint32_t *)(registry+0x34)=2;
    assert(observe() && output.count==3 && !output.present[1] && output.present[0] && output.present[2]);
    assert(output.placements[1].anchor==101); /* removed object retains its authored record */
    setup(); *(void **)(spawns[1]+0x78)=NULL; refused(); /* unaccounted live entity, not silent destruction */
    setup(); *(void **)(entities[1]+0x150+0x94)=entities[0]; refused(); /* parented object unsupported */
    setup(); *(void **)(objects[1]+0x14)=renderers[0]; refused();
    setup(); names[1][1]=0; refused();
    setup(); witness_exact=FALSE; refused();
    setup(); *(uint32_t *)(zone_data+0x24)=2; refused();
    setup(); *(uint32_t *)(zone_descriptor+0x10)=MAX_SPAWNS+1; refused();
    setup(); change_on_check=2; refused(); /* registry changes before final publication */
    setup(); mapped[0x145b90]^=1; refused(); mapped[0x145b90]^=1;
    setup(); void *slot=*(void **)(mapped+0x2c8700); *(void **)(mapped+0x2c8700)=mapped; refused();
    *(void **)(mapped+0x2c8700)=slot;
    setup(); assert(observe());
    VirtualFree(mapped,0,MEM_RELEASE);
    puts("story object observation exact-image/synthetic-topology tests passed (no native gameplay)"); return 0;
}
