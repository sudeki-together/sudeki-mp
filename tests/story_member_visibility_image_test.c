/* H1b member visibility on the exact image: the real ClrROFlags/SetROFlags
 * exports and the native HideModel/ShowModel depth helpers run on synthetic
 * party objects (no renderer callback bit, no device, no hooks). */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/hooks/lan_story_split.c"
static unsigned diagnostic_count;
static char last_diagnostic[512];
static void *fixture_actors[4];
static BOOL fixture_observer_exact=TRUE;
void SudekiMpLogFormat(const char *format,...) {
    va_list args;va_start(args,format);
    vsnprintf(last_diagnostic,sizeof(last_diagnostic),format,args);va_end(args);
    ++diagnostic_count;
    fputs(last_diagnostic,stdout);
}
void SudekiMpLogWrite(const char *text) {(void)text;}
BOOL SudekiMpLogResearchEnabled(void) {return FALSE;}
void *SudekiMpCleanroomEngineActorEntity(SudekiMpCleanroomActor actor) {
    static const SudekiMpCleanroomActor types[4]={SUDEKIMP_CLEANROOM_BUKI,
        SUDEKIMP_CLEANROOM_ELCO,SUDEKIMP_CLEANROOM_TAL,SUDEKIMP_CLEANROOM_AILISH};
    for(unsigned c=0;c<4u;++c) if(actor==types[c]) return fixture_actors[c];
    return NULL;
}
BOOL SudekiMpLanStoryObserverNativeRosterExact(const SudekiMpLanStoryNativeRoster *r) {
    return fixture_observer_exact && r && r->dispatch_serial && r->epoch;
}
unsigned SudekiMpLanStoryWorldSplitAnimateTargets(SudekiMpLanStoryWorldAnimateTarget *out,unsigned max) {
    (void)out;(void)max;return 0;
}
unsigned SudekiMpCastLightWorldAdoptions(void) {return 0;}
unsigned SudekiMpCastLightForeignSkips(void) {return 0;}
unsigned SudekiMpCastLightReadyFailure(void) {return 0;}
unsigned SudekiMpCastLightFaultSite(void) {return 0;}
unsigned SudekiMpCastLightFaultLine(void) {return 0;}
BOOL SudekiMpLanStoryTempExteriorInstall(HMODULE image) {(void)image;assert(!"unused");return FALSE;}
BOOL SudekiMpLanStoryTempExteriorAttach(const SudekiMpLanStoryTempExteriorConsumer *owner) {(void)owner;assert(!"unused");return FALSE;}
BOOL SudekiMpLanStoryTempExteriorDetach(void) {assert(!"unused");return FALSE;}
BOOL SudekiMpLanStoryTempExteriorUninstall(void) {assert(!"unused");return FALSE;}
BOOL SudekiMpLanStoryObserverSetSplit(const SudekiMpLanStoryObserverSplit *owner) {(void)owner;assert(!"unused");return FALSE;}
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

static void ptr(void *at,void *value) {memcpy(at,&value,4);}
/* Native HideModel 0x4E06D0 / ShowModel 0x4E0720: EAX=model, plain ret. */
__attribute__((naked,noinline,used)) static void eax_call(void *entry __attribute__((unused)),void *model __attribute__((unused))) {
    __asm__ volatile("pushl %esi\n\tpushl %edi\n\tpushl %ebx\n\tmovl 20(%esp),%eax\n\tcall *16(%esp)\n\t"
        "popl %ebx\n\tpopl %edi\n\tpopl %esi\n\tret\n\t");
}
typedef struct Member {
    uint8_t actor[0x100],model[0x80],position[0xb8],wrapper[0x14],body[0x40],renderer[4];
    uint8_t weapon[0x3c0],holder[2][0xc],objects[2][0x40];
} Member;
static uint32_t flags(const uint8_t *o) {uint32_t v;memcpy(&v,o+0x34,4);return v;}
static void set_flags(uint8_t *o,uint32_t v) {memcpy(o+0x34,&v,4);}
static int16_t depth(const Member *m) {int16_t d;memcpy(&d,m->model+0x74,2);return d;}
static void member(Member *m,uint8_t *image) {
    memset(m,0,sizeof(*m));
    ptr(m->actor+0x58,m->model); ptr(m->actor+0x44,m->position); ptr(m->actor+0xc0,m->weapon);
    ptr(m->model+0x10,m->actor);
    ptr(m->position,image+0x2cdefc); ptr(m->position+0x10,m->actor); ptr(m->position+0xb4,m->wrapper);
    ptr(m->wrapper+8,m->body); ptr(m->wrapper+0x10,m->renderer);
    ptr(m->body,image+0x2dd700); ptr(m->body+0x14,m->renderer); ptr(m->renderer,image+0x2df8ec);
    set_flags(m->body,0x00810011u);
    ptr(m->weapon,image+0x2d4d3c); ptr(m->weapon+0x10,m->actor); m->weapon[0x3b8]=2;
    ptr(m->weapon+0xf4,m->holder[0]); ptr(m->weapon+0x204,m->holder[1]);
    for(unsigned k=0;k<2;++k) { ptr(m->holder[k]+8,m->objects[k]); ptr(m->objects[k],image+0x2dd700); set_flags(m->objects[k],0x00810011u); }
}
/* The door script's HidePartyMembers for one follower: body via native
 * HideModel, gated weapon objects OR 4 (as 0x424AF6..). */
static void native_hide(uint8_t *image,Member *m) {
    eax_call(image+0xe06d0,m->model);
    for(unsigned k=0;k<2;++k) set_flags(m->objects[k],flags(m->objects[k])|4u);
}
static void native_show(uint8_t *image,Member *m) {
    eax_call(image+0xe0720,m->model);
    for(unsigned k=0;k<2;++k) set_flags(m->objects[k],flags(m->objects[k])&~4u);
}
int main(int argc,char **argv) {
    wchar_t path[32768];
    assert(argc==2 && MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,32768));
    uint8_t *image=map_image(path);
    base=image; native_thread=GetCurrentThreadId();
    assert(visibility_bind(image) && ro_set==(RoFlags)(image+RO_SET) && ro_clr==(RoFlags)(image+RO_CLR));
    image[0x24aa5]^=1; assert(!visibility_bind(image) && !ro_clr); image[0x24aa5]^=1;
    image[0x1d64da+10]^=1; assert(!visibility_bind(image)); image[0x1d64da+10]^=1;
    assert(visibility_bind(image));
    puts("Member visibility exact-image guards: PASS");
    Member *tal=calloc(1,sizeof(Member)); assert(tal); member(tal,image);
    uint8_t group[0xd4]={0},world[0x40]={0},lead[0x100]={0};
    unsigned two=2; memcpy(group+0xcc,&two,4); ptr(group+0x90,lead); ptr(group+0x90+0xc,tal->actor);
    ptr(image+WORLD,world); ptr(image+GROUP,group);
    fixture_actors[2]=tal->actor; fixture_actors[3]=lead;
    SudekiMpLanStoryNativeRoster r={.dispatch_serial=1,.epoch=1,.leader_character=3,.available_mask=12,.group=group};
    r.actors[2]=tal->actor; r.actors[3]=lead;
    /* Lead in her shop: the door script hid Tal (depth 1). */
    native_hide(image,tal); assert(depth(tal)==1 && (flags(tal->body)&4u));
    active=TRUE; settled_inside=FALSE;
    assert(SudekiMpLanStorySplitShowMember(&r,2)==0 && (flags(tal->body)&4u));   /* not settled yet */
    settled_inside=TRUE;
    assert(SudekiMpLanStorySplitShowMember(&r,3)==-1);                          /* the lead */
    assert(SudekiMpLanStorySplitShowMember(&r,2)==1 && shows[2].held);
    assert(!(flags(tal->body)&4u) && depth(tal)==1 && flags(tal->body)==0x00810011u);
    assert(!(flags(tal->objects[0])&4u) && !(flags(tal->objects[1])&4u));
    /* Upkeep: a native re-hide at the same depth is undone. */
    set_flags(tal->body,flags(tal->body)|4u); set_flags(tal->objects[1],flags(tal->objects[1])|4u);
    assert(SudekiMpLanStorySplitShowMember(&r,2)==1 && !(flags(tal->body)&4u) && !(flags(tal->objects[1])&4u));
    /* He leaves (or the lead exits): ours goes back, then the native show balances. */
    SudekiMpLanStorySplitReleaseMember(2,"exit_trigger");
    assert(!shows[2].held && (flags(tal->body)&4u) && (flags(tal->objects[0])&4u) && (flags(tal->objects[1])&4u));
    native_show(image,tal); assert(depth(tal)==0 && flags(tal->body)==0x00810011u && flags(tal->objects[0])==0x00810011u);
    puts("Member visibility S1 show, upkeep, release and native show balance: PASS");
    /* S2: the native show ran first (depth below the lease's): nothing written. */
    native_hide(image,tal); assert(SudekiMpLanStorySplitShowMember(&r,2)==1);
    native_show(image,tal); assert(depth(tal)==0);
    SudekiMpLanStorySplitReleaseMember(2,"late"); assert(!shows[2].held && !(flags(tal->body)&4u) && depth(tal)==0);
    /* S3: a replaced world retires the lease with no write. */
    native_hide(image,tal); assert(SudekiMpLanStorySplitShowMember(&r,2)==1);
    uint8_t other[0x40]={0}; ptr(image+WORLD,other);
    SudekiMpLanStorySplitReleaseMember(2,"world"); assert(!shows[2].held && !(flags(tal->body)&4u));
    ptr(image+WORLD,world); native_show(image,tal); assert(depth(tal)==0);
    /* S4: wrong thread keeps the lease (Uninstall would refuse). */
    native_hide(image,tal); assert(SudekiMpLanStorySplitShowMember(&r,2)==1);
    DWORD saved=native_thread; native_thread=saved+1;
    SudekiMpLanStorySplitReleaseMember(2,"thread"); assert(shows[2].held && SudekiMpLanStorySplitMemberLeasesHeld());
    native_thread=saved; release_all("test"); assert(!SudekiMpLanStorySplitMemberLeasesHeld() && (flags(tal->body)&4u));
    native_show(image,tal); assert(depth(tal)==0);
    puts("Member visibility S2 native-first, S3 world replaced, S4 wrong-thread retention: PASS");
    /* S5: refusals write nothing. */
    native_hide(image,tal); uint32_t hidden=flags(tal->body);
    ptr(tal->body,image); assert(SudekiMpLanStorySplitShowMember(&r,2)==-1 && flags(tal->body)==hidden); ptr(tal->body,image+0x2dd700);
    ptr(tal->model+0x10,lead); assert(SudekiMpLanStorySplitShowMember(&r,2)==-1 && flags(tal->body)==hidden); ptr(tal->model+0x10,tal->actor);
    ptr(group+0x90+0xc,NULL); assert(SudekiMpLanStorySplitShowMember(&r,2)==-1); ptr(group+0x90+0xc,tal->actor);
    native_show(image,tal); set_flags(tal->body,hidden); memset(tal->model+0x74,0,2);
    assert(SudekiMpLanStorySplitShowMember(&r,2)==-1 && flags(tal->body)==hidden);   /* hidden, no native depth */
    set_flags(tal->body,0x00810011u);
    /* S6: a weapon object with the renderer-callback bit is never written;
     * a logically hidden weapon stays hidden; a natively visible body needs nothing. */
    native_hide(image,tal); set_flags(tal->objects[0],flags(tal->objects[0])|0x4000000u);
    assert(SudekiMpLanStorySplitShowMember(&r,2)==1 && (flags(tal->objects[0])&4u) && !(flags(tal->objects[1])&4u));
    SudekiMpLanStorySplitReleaseMember(2,"t"); native_show(image,tal); set_flags(tal->objects[0],0x00810011u);
    native_hide(image,tal); tal->weapon[0x3b8]=0;
    assert(SudekiMpLanStorySplitShowMember(&r,2)==1 && (flags(tal->objects[0])&4u) && (flags(tal->objects[1])&4u));
    SudekiMpLanStorySplitReleaseMember(2,"t"); native_show(image,tal); tal->weapon[0x3b8]=2;
    assert(SudekiMpLanStorySplitShowMember(&r,2)==1 && !shows[2].held && depth(tal)==0);
    puts("Member visibility S5 refusals and S6 weapon/visible cases: PASS");
    free(tal); VirtualFree(image,0,MEM_RELEASE);
    return 0;
}
