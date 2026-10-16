/* Executes the supported CWorld::EnterTemporaryZone entry against a synthetic
 * world. Graphics suspension is inert; no gameplay/arrival acceptance claimed. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/hooks/lan_story_observer.c"

static uint8_t fake_world[0x3a0],fake_group[0xd4],fake_controller[0x24c],descriptors[2*0x54];
static uint8_t actors[4][0xa0],ais[4][0x170],positions[4][0x60],tracking[4][0x44],game[0xa4];
static uint32_t backing[2];static char resource_text[64],noted[64];
static unsigned constructors,releases,begins;static BOOL allow_split=TRUE,decline_begin,early_reject,wrong_resource;
static char result_log[512];
void SudekiMpLogFormat(const char *format,...) {
    if(!strstr(format,"story_test_temp")) return;
    va_list args;va_start(args,format);vsnprintf(result_log,sizeof(result_log),format,args);va_end(args);
}
void SudekiMpLogWrite(const char *text) {(void)text;}
BOOL SudekiMpCleanroomEngineWorldReady(void) {return TRUE;}
void *SudekiMpCleanroomEngineActorEntity(SudekiMpCleanroomActor a) {
    if(a==SUDEKIMP_CLEANROOM_TAL) return actors[2];
    if(a==SUDEKIMP_CLEANROOM_AILISH) return actors[3];
    return NULL;
}
BOOL SudekiMpControlSeparationUpdateDispatchWitnessStillExact(const SudekiMpControlUpdateDispatchWitness *w) {
    return w && w->service_post_original_exact && w->dispatch_serial==123;
}
BOOL SudekiMpCleanroomEngineResourceNameFromText(SudekiMpResourceName *r,const char *text) {
    assert(!backing[0]);++constructors;strcpy(resource_text,wrong_resource?"WRONG_MARKER":text);
    backing[0]=1;backing[1]=(uint32_t)(uintptr_t)resource_text;
    r->encoded_kind=0;r->identifier=23;r->text_reference=backing;return TRUE;
}
void SudekiMpCleanroomEngineReleaseResourceName(SudekiMpResourceName *r) {
    assert(r->text_reference==backing && backing[0]);--backing[0];++releases;memset(r,0,sizeof(*r));
}
void SudekiMpLanStoryAreaNoteTemporary(const char *name) {strcpy(noted,name);}
BOOL SudekiMpLanStoryAreaFollowCalling(void) {return FALSE;}
static BOOL ready_split(const char *name,unsigned lead) {
    return allow_split && lead==3 && !strcmp(name,"lico_frappe_farm");
}
static BOOL begin_split(const char *name,unsigned lead) {
    ++begins;assert(ready_split(name,lead));
    if(early_reject) *(unsigned *)(descriptors+0x54+0x34)=2;
    return !decline_begin;
}
static BOOL exit_split(void) {return TRUE;}
static void settled_split(BOOL inside) {(void)inside;}
static void ended_split(BOOL vanilla) {(void)vanilla;}
static const SudekiMpLanStoryObserverSplit split_fixture={begin_split,exit_split,settled_split,ended_split,ready_split};
static int __cdecl compare_names(const char *a,const char *b) {return _stricmp(a,b);}
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(file!=INVALID_HANDLE_VALUE);DWORD size=GetFileSize(file,NULL),got=0;
    uint8_t *raw=malloc(size);assert(raw && ReadFile(file,raw,size,&got,NULL) && got==size);CloseHandle(file);
    IMAGE_DOS_HEADER *dos=(void *)raw;IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *b=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
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

static void jump_to(uint8_t *b,unsigned rva,void *target) {
    b[rva]=0xe9;int32_t rel=(int32_t)((uintptr_t)target-(uintptr_t)(b+rva+5));
    memcpy(b+rva+1,&rel,4);assert(FlushInstructionCache(GetCurrentProcess(),b+rva,5));
}
static void mutate_byte(void *p) {
    DWORD old,ignored;assert(VirtualProtect(p,1,PAGE_EXECUTE_READWRITE,&old));
    *(uint8_t *)p ^=1;assert(VirtualProtect(p,1,old,&ignored));
}
static void fixture(uint8_t *b) {
    memset(fake_world,0,sizeof(fake_world));memset(fake_group,0,sizeof(fake_group));
    memset(fake_controller,0,sizeof(fake_controller));memset(descriptors,0,sizeof(descriptors));
    memset(actors,0,sizeof(actors));memset(ais,0,sizeof(ais));memset(positions,0,sizeof(positions));
    memset(tracking,0,sizeof(tracking));memset(game,0,sizeof(game));
    memset(backing,0,sizeof(backing));memset(noted,0,sizeof(noted));memset(result_log,0,sizeof(result_log));
    constructors=releases=begins=0;allow_split=TRUE;decline_begin=early_reject=wrong_resource=FALSE;
    *(void **)(b+0x408d10)=fake_world;*(void **)(b+0x408d94)=fake_group;
    *(void **)(b+0x408da4)=fake_controller;*(void **)(b+0x408da8)=game;*(void **)(b+0x408d14)=NULL;
    *(void **)(fake_world+0xc)=descriptors;*(void **)(fake_world+0x50)=descriptors;
    *(unsigned *)(fake_world+0x54)=2;fake_world[0x399]=fake_world[0x39a]=fake_world[0x39b]=1;
    *(const char **)(descriptors+0x24)="Illumina_Countryside_NW";*(unsigned *)(descriptors+0x34)=3;
    *(const char **)(descriptors+0x54+0x24)="LICo_Frappe_Farm";
    *(unsigned *)(fake_group+0xcc)=2;*(void **)(fake_group+0x90)=actors[3];*(void **)(fake_group+0x9c)=actors[2];
    *(void **)(fake_controller+0x248)=actors[3];
    for(unsigned c=2;c<4;++c) {
        *(void **)(actors[c]+0x94)=ais[c];*(void **)(ais[c]+0x10)=actors[c];
        *(void **)(actors[c]+0x44)=positions[c];*(void **)positions[c]=b+0x2cdefc;
        *(void **)(actors[c]+0x74)=tracking[c];*(uint16_t *)(tracking[c]+0x40)=38408;
        *(float *)(positions[c]+0x18)=-168.92819f;*(float *)(positions[c]+0x1c)=4.47097f;
        *(float *)(positions[c]+0x20)=-69.11105f;*(float *)(positions[c]+0x58)=-1.0f;
        actors[c][0x2b]=1; /* Native TEMP increments this; skips scheduler fixture. */
    }
    assert(SudekiMpLanStoryObserverInstall((HMODULE)b));
    assert(SudekiMpLanStoryObserverSetSplit(&split_fixture));
    strcpy(observed.world,"illumina_countryside_nw");
    SudekiMpControlUpdateDispatchWitness witness={0};witness.dispatch_serial=123;witness.service_post_original_exact=TRUE;
    SudekiMpLanStoryScene scene;
    assert(SudekiMpLanStoryObserverSample(fake_controller,&witness,&scene));
    assert(scene.phase==SUDEKIMP_LAN_STORY_READY && scene.available_mask==12 && scene.leader_seat==3);
}
static void named_guards(uint8_t *b) {
    assert(test_temp_code_exact());
    for(unsigned i=0;i<sizeof(test_temp_guards)/sizeof(*test_temp_guards);++i) {
        for(unsigned j=0;j<test_temp_guards[i].size;++j) {
            mutate_byte(b+test_temp_guards[i].rva+j);assert(!test_temp_code_exact());
            assert(SudekiMpLanStoryObserverTestEnterTemporary("lico_frappe_farm","ICS_FRAPPE_HOUSE_START_POS")==-1);
            mutate_byte(b+test_temp_guards[i].rva+j);
        }
        printf("PASS named %s all byte mutations refused before native entry\n",test_temp_guards[i].name);
    }
    for(unsigned j=0;j<9;++j) {mutate_byte(b+rvas[TEMP]+j);assert(!test_temp_code_exact());mutate_byte(b+rvas[TEMP]+j);}
    for(unsigned j=0;j<14;++j) {mutate_byte((uint8_t *)hooks[TEMP].trampoline+j);assert(!test_temp_code_exact());mutate_byte((uint8_t *)hooks[TEMP].trampoline+j);}
    assert(test_temp_code_exact() && !constructors && !begins);
    puts("PASS named TestTemporaryOwnedEntry exact current detour and retained trampoline");
}
static int request(void) {return SudekiMpLanStoryObserverTestEnterTemporary("LICo_Frappe_Farm","ICS_FRAPPE_HOUSE_START_POS");}
static DWORD WINAPI foreign_request(void *unused) {(void)unused;assert(request()==0);return 0;}
static void admission(uint8_t *b) {
    assert(SudekiMpLanStoryObserverTestEnterTemporary("missing","marker")==-1);
    assert(SudekiMpLanStoryObserverTestEnterTemporary("lico_frappe_farm","bad/marker")==-1);
    allow_split=FALSE;assert(request()==0 && !constructors);allow_split=TRUE;
    *(unsigned *)(descriptors+0x54+0x34)=2;assert(request()==-1);*(unsigned *)(descriptors+0x54+0x34)=0;
    *(uint16_t *)(tracking[3]+0x40)=0xffff;assert(request()==0);*(uint16_t *)(tracking[3]+0x40)=38408;
    *(void **)(fake_world+0x14)=descriptors+0x54;assert(request()==0);*(void **)(fake_world+0x14)=NULL;
    *(void **)(actors[3]+0x94)=NULL;assert(request()==0);*(void **)(actors[3]+0x94)=ais[3];
    HANDLE thread=CreateThread(NULL,0,foreign_request,NULL,0,NULL);assert(thread);
    assert(WaitForSingleObject(thread,5000)==WAIT_OBJECT_0);CloseHandle(thread);
    assert(!constructors && !begins && !test_temp_dispatched);
    wrong_resource=TRUE;assert(request()==-1 && constructors==1 && releases==1 && !backing[0]);wrong_resource=FALSE;
    uint8_t before[sizeof(fake_world)];memcpy(before,fake_world,sizeof(before));
    SudekiMpLanStoryScene prior=observed;decline_begin=TRUE;
    assert(request()==0 && !test_temp_dispatched && !call_depth && !split_active && !noted[0]);
    assert(!memcmp(before,fake_world,sizeof(before)) && !memcmp(&prior,&observed,sizeof(prior)));
    decline_begin=FALSE;
    puts("PASS named TestTemporaryAdmission world/roster/thread/split/marker/target fences and no vanilla fallback");
    (void)b;
}
static void native_entry(uint8_t *b) {
    uint8_t outside[sizeof(actors[2])],outside_position[sizeof(positions[2])];
    memcpy(outside,actors[2],sizeof(outside));memcpy(outside_position,positions[2],sizeof(outside_position));
    unsigned epoch=observed.epoch;
    assert(request()==1 && test_temp_dispatched && split_active && split_transition && observed.epoch==epoch);
    assert(observed.inside_mask==8 && !strcmp(noted,"lico_frappe_farm"));
    assert(*(void **)(fake_world+0xc)==descriptors && *(void **)(fake_world+0x14)==descriptors+0x54);
    assert(!fake_world[0x399] && !fake_world[0x39a] && actors[3][0x2b]==2);
    assert(!memcmp(fake_world+0x28,positions[3]+0x18,12) && *(uint16_t *)(fake_world+0x40)==38408);
    assert(!memcmp(actors[2],outside,sizeof(outside)) && !memcmp(positions[2],outside_position,sizeof(outside_position)));
    uint8_t saved_group[sizeof(fake_group)];memcpy(saved_group,fake_group,sizeof(saved_group));
    ((void (__attribute__((thiscall)) *)(void *))(b+0x24850))(fake_group);
    assert(!memcmp(fake_group,saved_group,sizeof(fake_group)) && !memcmp(actors[2],outside,sizeof(outside)));
    puts("PASS named TestTemporaryExitFullPartyZero native depth0 is a no-op without follower resume");
    assert(backing[0]==1 && *(void **)(fake_world+0x4c)==backing && strstr(result_log,"accepted_pending"));
    assert(!SudekiMpLanStoryObserverTestTemporarySettled("lico_frappe_farm"));
    unsigned count=constructors;assert(request()==-1 && constructors==count);
    /* Completion is a separately observed state, never inferred from entry. */
    *(void **)(fake_world+0x14)=NULL;*(void **)(fake_world+0xc)=descriptors+0x54;
    *(unsigned *)(descriptors+0x54+0x34)=4;fake_world[0x399]=fake_world[0x39a]=1;
    SudekiMpControlUpdateDispatchWitness witness={0};witness.dispatch_serial=123;witness.service_post_original_exact=TRUE;
    SudekiMpLanStoryScene scene;assert(SudekiMpLanStoryObserverSample(fake_controller,&witness,&scene));
    assert(SudekiMpLanStoryObserverTestTemporarySettled("LICo_Frappe_Farm"));
    assert(!SudekiMpLanStoryObserverTestTemporarySettled("another_room"));
    *(void **)(fake_world+0x14)=descriptors;assert(!SudekiMpLanStoryObserverTestTemporarySettled("lico_frappe_farm"));
    *(void **)(fake_world+0x14)=NULL;
    puts("PASS named TestTemporaryNativeABI exact TEMP entry, native resource retention and lead-only return context");
    puts("PASS named TestTemporaryCompletion separate exact matching state4 completion and no retry");
    assert(SudekiMpLanStoryObserverUninstall());(void)b;
}
int main(int argc,char **argv) {
    wchar_t path[4096];assert(argc==2 && MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,4096));
    uint8_t *b=map_image(path);
    /* Inert graphics suspension and imported string compare only. The native
     * TEMP entry, resource copy/refcounts, return capture and ABI execute. */
    b[0x10b500]=0xc3;jump_to(b,0x24ae0e,compare_names);
    fixture(b);named_guards(b);admission(b);native_entry(b);
    fixture(b);early_reject=TRUE;
    assert(request()==1 && test_temp_dispatched && !backing[0] && !*(void **)(fake_world+0x14));
    assert(strstr(result_log,"result=unknown") && strstr(result_log,"retry=forbidden"));
    unsigned count=constructors;assert(request()==-1 && constructors==count);
    assert(!SudekiMpLanStoryObserverTestTemporarySettled("lico_frappe_farm"));
    puts("PASS named TestTemporaryNativeReject uncertain dispatched return is consumed without retry");
    assert(SudekiMpLanStoryObserverUninstall());assert(VirtualFree(b,0,MEM_RELEASE));return 0;
}
