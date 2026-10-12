/* Hash-verified image contracts plus the separately compiled area follower.
 * Native zone entrypoints are recording stubs after validation. No game world,
 * loading, task, renderer or control binding is executed by this fixture. */
#include "engine/build_identity.h"
#include "hooks/lan_story_area_follow.h"
#include "hooks/lan_story_client.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned switches,enters,releases;
static BOOL lighting_ready=TRUE;
static char called[64];
static void area_name(char out[SUDEKIMP_STORY_AREA_NAME],const char *name) {
    memset(out,0,SUDEKIMP_STORY_AREA_NAME);strcpy(out,name);
}
void SudekiMpLogFormat(const char *format,...) {(void)format;}
BOOL SudekiMpLanStorySkyReleaseLighting(void) {++releases;return lighting_ready;}
/* Unexercised effect entrypoints are linked with inert dependencies. */
unsigned SudekiMpLanStorySpawnTraceGroups(SudekiMpStorySpawnGroup *out,unsigned max) {(void)out;(void)max;return 0;}
BOOL SudekiMpLanStorySpawnReplay(const SudekiMpStorySpawnGroup *s) {(void)s;return FALSE;}
BOOL SudekiMpLanStorySpawnEventsCapture(SudekiMpStoryEffectState *s) {(void)s;return FALSE;}
void SudekiMpLanStorySpawnDespawnCapture(SudekiMpStoryDespawn *s) {(void)s;}
void SudekiMpLanStorySpawnEffectsCapture(SudekiMpStoryEffectState *s) {(void)s;}
void SudekiMpLanStorySkyCapture(SudekiMpStoryEffectState *s) {(void)s;}
BOOL SudekiMpLanStoryClientResidencyCall(const SudekiMpControlUpdateDispatchWitness *w,
    SudekiMpLanStoryClientResidencyOperation op,void *context) {(void)w;(void)op;(void)context;return FALSE;}
static void __cdecl switched(const char *name) {
    assert(SudekiMpLanStoryAreaFollowCalling());++switches;strcpy(called,name);
}
static void __cdecl entered(const char *name) {
    assert(SudekiMpLanStoryAreaFollowCalling());++enters;strcpy(called,name);
}
static void jump(uint8_t *p,void *to) {
    p[0]=0xe9;*(uint32_t *)(p+1)=(uint32_t)((uintptr_t)to-(uintptr_t)p-5u);
    assert(FlushInstructionCache(GetCurrentProcess(),p,5));
}
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(file!=INVALID_HANDLE_VALUE);DWORD size=GetFileSize(file,NULL),got=0;
    uint8_t *raw=malloc(size);assert(raw && ReadFile(file,raw,size,&got,NULL) && got==size);CloseHandle(file);
    IMAGE_DOS_HEADER *dos=(void *)raw;IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *b=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(b);memcpy(b,raw,nt->OptionalHeader.SizeOfHeaders);
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
        uint16_t *items=(void *)(block+1);unsigned count=(block->SizeOfBlock-sizeof(*block))/2;
        for(unsigned i=0;i<count;++i) {
            unsigned type=items[i]>>12,rva=block->VirtualAddress+(items[i]&0xfff);
            assert(type==IMAGE_REL_BASED_ABSOLUTE || type==IMAGE_REL_BASED_HIGHLOW);
            if(type==IMAGE_REL_BASED_HIGHLOW) {
                assert(rva<=nt->OptionalHeader.SizeOfImage-4);*(uint32_t *)(b+rva)+=(uint32_t)delta;
            }
        }
        offset+=block->SizeOfBlock;
    }
    free(raw);return b;
}

int main(int argc,char **argv) {
    assert(argc==2);wchar_t path[1024];
    assert(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,argv[1],-1,path,1024));
    uint8_t *image=map_image(path);DWORD old;
    assert(SudekiMpLanStoryAreaFollowInitialize((HMODULE)image));
    /* Check each native byte guard BY NAME, then mutate its actual site. */
    const struct {const char *name;unsigned rva;} guards[]={
        {"SwitchZoneNOW",0x7990u},{"EnterZone",0x7970u}};
    for(unsigned i=0;i<sizeof(guards)/sizeof(*guards);++i) {
        for(unsigned j=0;j<5u;++j) {
            image[guards[i].rva+j]^=1u;
            assert(!SudekiMpLanStoryAreaFollowInitialize((HMODULE)image));
            image[guards[i].rva+j]^=1u;
            assert(SudekiMpLanStoryAreaFollowInitialize((HMODULE)image));
        }
        printf("native guard %s: supported bytes pass, every checked byte mutation refused\n",guards[i].name);
    }
    uint8_t world[0x3a0]={0},table[3*0x54]={0};
    const char *names[]={"Exterior_A","Exterior_B","Farm_Interior"};
    for(unsigned i=0;i<3;++i) *(const char **)(table+i*0x54+0x24)=names[i];
    *(void **)(image+0x408d10)=world;
    *(void **)(world+0x50)=table;*(unsigned *)(world+0x54)=3;
    *(void **)(world+0xc)=table;world[0x39b]=1;
    *(unsigned *)(table+0x34)=3;
    assert(VirtualProtect(image,SUDEKIMP_EXPECTED_IMAGE_SIZE,PAGE_EXECUTE_READWRITE,&old));
    jump(image+0x7990,(void *)switched);jump(image+0x7970,(void *)entered);
    SudekiMpStoryAreaState host={0};host.count=2;
    area_name(host.current,names[2]);area_name(host.target,names[1]);
    area_name(host.entries[0].name,names[0]);host.entries[0].state=3;
    area_name(host.entries[1].name,names[2]);host.entries[1].state=4;
    uint8_t before[sizeof(world)];memcpy(before,world,sizeof(world));
    for(unsigned i=0;i<3;++i) SudekiMpLanStoryAreaFollow(&host,1000+i*1000,NULL);
    assert(!switches && !enters && !releases && !SudekiMpLanStoryAreaFollowLastCall());
    assert(!memcmp(before,world,sizeof(world)));
    /* Same report with the interior already loaded must not EnterZone either. */
    *(unsigned *)(table+2*0x54+0x34)=2;
    SudekiMpLanStoryAreaFollow(&host,4000,NULL);
    assert(!switches && !enters && !releases);
    /* Missing/ambiguous host current is unknown, not an ordinary zone. */
    host.entries[1].state=2;SudekiMpLanStoryAreaFollow(&host,5000,NULL);
    assert(!switches && !enters && !releases);
    /* Observed native house exit: exterior current, but target still names
     * the now-unloaded house. State zero is omitted from the host report.
     * It must not trigger SwitchZoneNOW and replace the client's exterior. */
    area_name(host.current,names[0]);area_name(host.target,names[2]);
    host.count=1;memset(&host.entries[1],0,sizeof(host.entries[1]));
    *(unsigned *)(table+2*0x54+0x34)=0;
    assert(SudekiMpStoryAreaStateExterior(&host));
    SudekiMpLanStoryAreaFollow(&host,6000,NULL);
    assert(!switches && !enters && !releases);
    assert(!SudekiMpLanStoryAreaFollowLastCall() && !memcmp(before,world,sizeof(world)));
    /* A previously resident copy does not make that stale target active. */
    *(unsigned *)(table+2*0x54+0x34)=2;
    SudekiMpLanStoryAreaFollow(&host,6200,NULL);
    assert(!switches && !enters && !releases);
    memset(host.target,0,sizeof(host.target));
    SudekiMpLanStoryAreaFollow(&host,6400,NULL);
    assert(!switches && !enters && !releases);
    /* A real outdoor walk still background-loads then enters normally. */
    area_name(host.target,names[1]);host.count=2;
    area_name(host.entries[1].name,names[1]);host.entries[1].state=1;
    SudekiMpLanStoryAreaFollow(&host,7000,NULL);
    assert(switches==1 && enters==0 && releases==1 && !strcmp(called,names[1]));
    assert(!SudekiMpLanStoryAreaFollowCalling() && SudekiMpLanStoryAreaFollowLastCall());
    area_name(host.current,names[1]);area_name(host.entries[1].name,names[1]);host.entries[1].state=3;
    *(unsigned *)(table+0x54+0x34)=2;
    lighting_ready=FALSE;SudekiMpLanStoryAreaFollow(&host,8000,NULL);
    assert(switches==1 && !enters && releases==2);
    lighting_ready=TRUE;SudekiMpLanStoryAreaFollow(&host,9000,NULL);
    assert(switches==1 && enters==1 && releases==3 && !strcmp(called,names[1]));
    /* An admitted local interior must not be pulled out by this adapter. */
    *(void **)(world+0xc)=table+2*0x54;*(unsigned *)(table+2*0x54+0x34)=4;
    SudekiMpLanStoryAreaFollow(&host,10000,NULL);
    assert(switches==1 && enters==1 && releases==3);
    /* An absent background target must not prevent following a positively
     * observed ordinary current zone that is missing on this client. */
    *(void **)(world+0xc)=table;*(unsigned *)(table+0x34)=3;
    *(unsigned *)(table+0x54+0x34)=0;
    area_name(host.target,names[2]);memset(&host.entries[0],0,sizeof(host.entries[0]));
    host.entries[0]=host.entries[1];memset(&host.entries[1],0,sizeof(host.entries[1]));host.count=1;
    SudekiMpLanStoryAreaFollow(&host,11000,NULL);
    assert(switches==2 && enters==1 && releases==4 && !strcmp(called,names[1]));
    puts("story area follow exact-image guards and interior/return/outdoor call routing passed (no live loading)");
    VirtualFree(image,0,MEM_RELEASE);return 0;
}
