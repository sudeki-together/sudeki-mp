/* Exact native getter/vtable guards with synthetic retained world ownership. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/engine/build_identity.c"
#include "../src/engine/sha256.c"
#include "../src/network/lan_story_world_frame.c"
#include "../src/hooks/lan_story_world.c"
void SudekiMpLogFormat(const char *format,...) { (void)format; }
BOOL SudekiMpLogResearchEnabled(void) { return FALSE; }
static BOOL roster_ok=TRUE;
BOOL SudekiMpLanStoryObserverNativeRosterExact(const SudekiMpLanStoryNativeRoster *r) {
    return roster_ok && r==&trail_publication.roster;
}
static void ptr(void *p,unsigned off,void *v) { memcpy((uint8_t *)p+off,&v,4); }
static void word(void *p,unsigned off,unsigned v) { memcpy((uint8_t *)p+off,&v,4); }
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(file!=INVALID_HANDLE_VALUE); DWORD size=GetFileSize(file,NULL),got=0;
    uint8_t *raw=malloc(size); assert(raw && ReadFile(file,raw,size,&got,NULL) && got==size); CloseHandle(file);
    IMAGE_DOS_HEADER *dos=(void *)raw;
    IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *base=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
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
    assert(FlushInstructionCache(GetCurrentProcess(),base,nt->OptionalHeader.SizeOfImage));
    free(raw); return base;
}


int main(int argc,char **argv) {
    wchar_t path[32768];assert(argc==2 && MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,32768));base=map_image(path);
    uint8_t actor[0x260]={0},model[0x168]={0},position[0x104]={0},wrapper[0x18]={0},object[0xd0]={0};
    uint8_t renderer[0xb0]={0},bank[0x5c]={0},description[0x24]={0},channels[180]={0},blends[80]={0},rows[5][24]={{0}};
    uint8_t child[0x74]={0},resource[0x24]={0},definition[8]={0},owner[0x40]={0};
    void *entities[2]={actor,NULL},*children[2]={NULL,child};unsigned name[2]={1,(unsigned)(uintptr_t)"PC_TAL"};
    ptr(base,0x409d8c,owner);word(owner,0x34,1);ptr(owner,0x3c,entities);
    ptr(base,0x408d10,model);native_thread=GetCurrentThreadId();client_seen=TRUE;client_count=1;
    trail_publication.valid=TRUE;trail_publication.frame=*(uint16_t *)(base+0x3c3150);trail_publication.tick=GetTickCount();
    trail_publication.roster.world=model;trail_publication.roster.available_mask=4;trail_publication.roster.actors[2]=actor;
    ptr(actor,0,base+0x2d5010);ptr(actor,8,base+0x2d5034);ptr(actor,0x2c,base+0x2d5054);word(actor,0x30,0xf80);
    word(actor,0x34,SudekiMpLanStoryWorldCharacterIdentifier(2));ptr(actor,0x38,name);ptr(actor,0x130,model);
    ptr(model,0,base+0x2c8504);ptr(model,0x10,actor);ptr(actor,0x44,position);
    ptr(position,0,base+0x2cdefc);ptr(position,0x10,actor);ptr(position,0xb4,wrapper);
    ptr(wrapper,8,object);ptr(wrapper,0x10,renderer);ptr(object,0x14,renderer);
    ptr(renderer,0,base+0x2df8ec);ptr(renderer,8,bank);ptr(renderer,0x98,channels);ptr(renderer,0x9c,blends);
    word(renderer,0xa0,5);word(renderer,0xa4,4);ptr(bank,0x1c,description);word(description,0,1);word(description,0xc,1);
    const unsigned topology[4]={0x00010000,0x00030002,0x80018000,0x00048002};
    Target *t=&client_bound[0].target;
    *t=(Target){.entity=actor,.position=position,.model=model,.wrapper=wrapper,.object=object,.renderer=renderer,
        .bank=bank,.description=description,.channels=channels,.blends=blends,.animations=1,.submodels=1,.channel_count=5,
        .native_identifier=SudekiMpLanStoryWorldCharacterIdentifier(2),.character=2,.kind=SUDEKIMP_LAN_STORY_WORLD_PC_KIND};
    for(unsigned i=0;i<5;++i) { ptr(channels,i*36,rows[i]);t->rows[i]=rows[i];if(i<4)word(blends,i*20,topology[i]); }
    ptr(child,0,base+0x2dfb14);ptr(child,0x14,resource);ptr(resource,0,base+0x2dfaf0);word(resource,8,1);
    ptr(resource,0x14,definition);word(definition,4,1);word(bank,0x58,2);ptr(renderer,0x64,children);
#define ACCEPT() assert(SudekiMpLanStoryWorldTrailOwner(renderer,child,1))
#define DENY() assert(!SudekiMpLanStoryWorldTrailOwner(renderer,child,1))
    ACCEPT();roster_ok=FALSE;DENY();roster_ok=TRUE;
    entities[1]=actor;word(owner,0x34,2);DENY();entities[1]=NULL;word(owner,0x34,1);
    children[0]=child;DENY();children[0]=NULL;
    word(resource,8,0);DENY();word(resource,8,1);
    word(definition,4,0);DENY();word(definition,4,1);
    ptr(actor,0x130,NULL);DENY();ptr(actor,0x130,model);
    word(actor,0x34,t->native_identifier^1u);DENY();word(actor,0x34,t->native_identifier);
    ++trail_publication.frame;DENY();--trail_publication.frame;
    trail_publication.tick-=101;DENY();trail_publication.tick=GetTickCount();
    native_thread^=1;DENY();native_thread^=1;
    active=TRUE;DENY();active=FALSE;host_seen=TRUE;DENY();host_seen=FALSE;
    ptr(base,0x408d10,actor);DENY();ptr(base,0x408d10,model);ACCEPT();
    puts("PASS current-frame trail actor/model/registry/child/resource/thread ownership and refusal tests");
    VirtualFree(base,0,MEM_RELEASE);return 0;
}
