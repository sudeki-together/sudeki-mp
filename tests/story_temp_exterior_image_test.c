/* Exact-image test for the TEMP exterior keep-alive adapter. The supported
 * image is hash checked, relocated and mapped privately. The real adapter
 * hooks are installed on pristine text; then only leaf callees (zone lookup,
 * graphics release, resource-name refresh, camera, state change, collision
 * enrollment and resume) are redirected to recording stubs and the actual
 * native EnterTemporaryZone/ExitTemporaryZone bodies and native intrusive
 * reference-list helpers execute against a synthetic world. The adapter's
 * originals are replaced with recording substitutes, so no native entity
 * suspension, collision, audio, loading or gameplay runs here. */
#include "hooks/call_hook.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned installs,fail_install;
static BOOL call_install(SudekiMpRelativeCallHook *,uint8_t *,const void *,const void *);
#define SudekiMpInstallRelativeCallHook call_install
#include "../src/hooks/lan_story_temp_exterior.c"
#undef SudekiMpInstallRelativeCallHook
static BOOL call_install(SudekiMpRelativeCallHook *h,uint8_t *p,const void *e,const void *r) {
    if(++installs==fail_install) {SetLastError(ERROR_WRITE_FAULT);return FALSE;}
    return SudekiMpInstallRelativeCallHook(h,p,e,r);
}
#define PTR(b,o) (*(void **)((uint8_t *)(b)+(o)))
#define U32(b,o) (*(uint32_t *)((uint8_t *)(b)+(o)))
#define F32(b,o) (*(float *)((uint8_t *)(b)+(o)))
enum { IMAGE_SIZE=0x45f000, AUDIO=0x408d58, SETTINGS=0x408da8, GROUP=0x408d94,
    CAMERA=0x409d7c, COLLISION=0x408dd4 };
static uint8_t *mapped;
static uint8_t world[0x3a0] __attribute__((used));
static uint8_t table[3*0x54],data[2][0x130],terrain[2][0x110],catalog[0x20],
    spawns[2*0x90],entities[2][0x80],positions[2][0x60],settings[0xa4],group[0x94],
    lead[0x100],lead_position[0x60],neighbor_list[4],resource_name[12];
static uint8_t *const exterior=table,*const neighbor=table+0x54,*const temporary=table+0xa8;
static unsigned lookups,graphics,refreshes,cameras,changes,enrolls,resumes_stub,camera_sets;
static unsigned fake_suspends __attribute__((used)),fake_resumes __attribute__((used));
static void *suspend_args[4] __attribute__((used)),*resume_args[4] __attribute__((used));
static void *change_desc; static uint32_t change_state __attribute__((used));
static void *resume_stub_arg;
static void *__attribute__((stdcall)) lookup(void *w,const char *name) {
    assert(w==world && !strcmp(name,"LNBr_Church")); ++lookups; return temporary;
}
static void __attribute__((stdcall)) graphics_release(void *p) {assert(p==data[0]||p==data[1]); ++graphics;}
static void __attribute__((naked,noinline)) refresh(void) {__asm__ volatile("incl _refreshes; ret");}
static void __attribute__((stdcall)) camera_release(void *p) {assert(p==PTR(mapped,CAMERA)); ++cameras;}
static void __attribute__((naked,noinline)) change(void) {
    __asm__ volatile("mov %eax,_change_state; mov 4(%esp),%eax; mov %eax,_change_desc; incl _changes; ret $4");
}
static void __attribute__((stdcall)) enroll(void *system,void *t,uint32_t on) {
    assert(system==PTR(mapped,COLLISION) && t==terrain[1] && on==1); ++enrolls;
}
static void __attribute__((stdcall)) resume_stub(void *d) {resume_stub_arg=d; ++resumes_stub;}
static void __attribute__((stdcall)) camera_set(void *camera,void *matrix) {
    assert(camera==PTR(mapped,CAMERA) && matrix); ++camera_sets;
}
static void __attribute__((naked,noinline)) fake_suspend(void) {
    __asm__ volatile("push %ecx; mov _fake_suspends,%ecx; mov %eax,_suspend_args(,%ecx,4);"
        "incl _fake_suspends; pop %ecx; ret");
}
static void __attribute__((naked,noinline)) fake_resume(void) {
    __asm__ volatile("push %ecx; mov _fake_resumes,%ecx; mov %edi,_resume_args(,%ecx,4);"
        "incl _fake_resumes; pop %ecx; ret");
}
typedef struct Redirect { unsigned at,target; void *stub; int32_t saved; } Redirect;
static Redirect redirects[]={
    {0x64e5,0x59b0,NULL,0},{0x65fc,0x59b0,NULL,0},{0x651f,0x10d010,NULL,0},{0x658e,0x10d010,NULL,0},
    {0x662d,0x4bc0,NULL,0},{0x673e,0x379b0,NULL,0},{0x6766,0x10ad70,NULL,0},{0x67ed,0x31830,NULL,0},
    {0x681c,0x10b560,NULL,0},{0x69e8,0x38b30,NULL,0},{0x6595,0x10b500,NULL,0}};
static void redirect(BOOL on) {
    void *stubs[]={(void *)(uintptr_t)lookup,(void *)(uintptr_t)lookup,(void *)(uintptr_t)graphics_release,
        (void *)(uintptr_t)graphics_release,(void *)(uintptr_t)refresh,(void *)(uintptr_t)camera_release,
        (void *)(uintptr_t)change,(void *)(uintptr_t)enroll,(void *)(uintptr_t)resume_stub,(void *)(uintptr_t)camera_set,(void *)(uintptr_t)fake_suspend};
    DWORD old;
    assert(VirtualProtect(mapped+0x6000,0x1000,PAGE_EXECUTE_READWRITE,&old));
    for(unsigned i=0;i<sizeof(redirects)/sizeof(redirects[0]);++i) {
        Redirect *r=&redirects[i]; uint8_t *p=mapped+r->at; int32_t d;
        assert(p[0]==0xe8);
        if(on) {
            memcpy(&r->saved,p+1,4); assert(p+5+r->saved==mapped+r->target);
            d=(int32_t)((uint8_t *)stubs[i]-(p+5)); memcpy(p+1,&d,4);
        } else memcpy(p+1,&r->saved,4);
    }
    DWORD ignored; assert(VirtualProtect(mapped+0x6000,0x1000,old,&ignored));
    assert(FlushInstructionCache(GetCurrentProcess(),mapped+0x6000,0x1000));
}
static uint32_t regs[6] __attribute__((used)),stack_before __attribute__((used));
static void *native_target __attribute__((used)),*arg_name __attribute__((used)),*arg_resource __attribute__((used));
static void __attribute__((naked,noinline)) invoke_enter(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,_stack_before;"
        "mov $0x44444444,%ebx; mov $0x55555555,%ebp; mov $0x66666666,%esi; mov $0x77777777,%edi;"
        "push _arg_resource; push _arg_name; mov $_world,%ecx; call *_native_target;"
        "mov %ebx,_regs; mov %ebp,_regs+4; mov %esi,_regs+8; mov %edi,_regs+12; mov %esp,_regs+16;"
        "popal; popfl; ret");
}
static void __attribute__((naked,noinline)) invoke_exit(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,_stack_before;"
        "mov $0x44444444,%ebx; mov $0x55555555,%ebp; mov $0x66666666,%esi; mov $0x77777777,%edi;"
        "mov $_world,%ecx; call *_native_target;"
        "mov %ebx,_regs; mov %ebp,_regs+4; mov %esi,_regs+8; mov %edi,_regs+12; mov %esp,_regs+16;"
        "popal; popfl; ret");
}
static void preserved(void) {
    assert(regs[0]==0x44444444u && regs[1]==0x55555555u && regs[2]==0x66666666u &&
        regs[3]==0x77777777u && regs[4]==stack_before);
}
static void fixture(void) {
    memset(world,0,sizeof(world)); memset(table,0,sizeof(table)); memset(data,0,sizeof(data));
    memset(terrain,0,sizeof(terrain)); memset(catalog,0,sizeof(catalog)); memset(spawns,0,sizeof(spawns));
    memset(entities,0,sizeof(entities)); memset(positions,0,sizeof(positions));
    memset(settings,0,sizeof(settings)); memset(group,0,sizeof(group)); memset(lead,0,sizeof(lead));
    memset(lead_position,0,sizeof(lead_position));
    PTR(world,0)=mapped+WORLD_VT; PTR(world,0xc)=exterior; PTR(world,0x50)=table; U32(world,0x54)=3;
    static const char *names[]={"NewBrightwater","NewBrightwater_N","LNBr_Church"};
    for(unsigned i=0;i<3;++i) {PTR(table+i*0x54,0)=mapped+DESC_VT; PTR(table+i*0x54,0x24)=(void *)names[i];}
    U32(exterior,0x34)=3; PTR(exterior,0x14)=data[0]; U32(exterior,0x3c)=1;
    neighbor_list[0]=0; PTR(neighbor_list,0)=neighbor; PTR(exterior,0x44)=neighbor_list;
    U32(neighbor,0x34)=2; PTR(neighbor,0x14)=data[1];
    for(unsigned i=0;i<2;++i) {
        PTR(data[i],0)=mapped+DATA_VT; data[i][0x128]=5; data[i][0x129]=1; PTR(data[i],0x2c)=terrain[i];
        terrain[i][0xf4]=1; U32(terrain[i],0x44)=0x20000;
        PTR(spawns+i*0x90,0)=mapped+SPAWN_VT; PTR(spawns+i*0x90,4)=mapped+SPAWN_VT2;
        PTR(spawns+i*0x90,0x78)=entities[i]+0x2c; PTR(entities[i],0x44)=positions[i];
        PTR(positions[i],0)=mapped+POSITION_VT; PTR(positions[i],4)=mapped+POSITION_VT2;
        PTR(positions[i],0x10)=entities[i]; F32(positions[i],0x18)=10.0f*(float)(i+1);
    }
    PTR(data[0],0x118)=catalog; U32(catalog,0x10)=2; PTR(catalog,0x18)=spawns;
    U32(temporary,0x34)=0;
    PTR(lead,0x44)=lead_position; lead[0x2b]=1;
    F32(lead_position,0x18)=1.0f; F32(lead_position,0x1c)=2.0f; F32(lead_position,0x20)=3.0f;
    F32(lead_position,0x50)=0.5f;
    PTR(group,0x90)=lead;
    PTR(mapped,WORLD)=world; PTR(mapped,SETTINGS)=settings; PTR(mapped,GROUP)=group;
    PTR(mapped,AUDIO)=NULL; PTR(mapped,CAMERA)=settings; PTR(mapped,COLLISION)=group;
    U32(resource_name,0)=0; U32(resource_name,4)=1; PTR(resource_name,8)=NULL;
    lookups=graphics=refreshes=cameras=changes=enrolls=resumes_stub=camera_sets=0;
    fake_suspends=fake_resumes=0; change_desc=resume_stub_arg=NULL;
}
static unsigned keeps,receipts; static BOOL keep_answer;
static SudekiMpLanStoryTempExteriorEntry last_entry;
static SudekiMpLanStoryTempExteriorReceipt last_receipt;
static BOOL keep(void *context,const SudekiMpLanStoryTempExteriorEntry *e) {
    assert(context==&keeps); last_entry=*e; ++keeps; return keep_answer;
}
static void skipped(void *context,const SudekiMpLanStoryTempExteriorReceipt *r) {
    assert(context==&keeps); last_receipt=*r; ++receipts;
}
static const SudekiMpLanStoryTempExteriorConsumer consumer_fixture={keep,skipped,&keeps};
static void enter(void) {
    native_target=mapped+0x64b0; arg_name="LNBr_Church"; arg_resource=resource_name;
    invoke_enter(); preserved();
    assert(lookups==2 && PTR(world,0x14)==temporary && refreshes==1);
    assert(F32(world,0x28)==1.0f && F32(world,0x2c)==2.0f && F32(world,0x30)==3.0f &&
        F32(world,0x34)==-0.5f && lead[0x2b]==2 && !PTR(lead,4));
}
/* Stand-in for the native foreground selection that makes the TEMP current. */
static void select_temporary(void) {
    PTR(world,0xc)=temporary; PTR(world,0x14)=NULL; U32(temporary,0x34)=4;
}
static void leave(void) {
    native_target=mapped+0x6710; invoke_exit(); preserved();
    assert(cameras==1 && changes==1 && change_desc==temporary && change_state==0);
    assert(camera_sets==1 && !PTR(world,0xc) && !PTR(world,0x10) && !PTR(lead,4));
    assert(!(exterior[0x50]&0x10) && !data[0][0x78]);
}
static void kept_round_trip(unsigned round) {
    fixture(); keep_answer=TRUE; unsigned k=keeps,r=receipts;
    enter();
    assert(keeps==k+1 && last_entry.neighbors==1 && last_entry.characters==2 && last_entry.tracked==2);
    assert(!strcmp(last_entry.exterior,"NewBrightwater") && !strcmp(last_entry.destination,"LNBr_Church"));
    assert(!fake_suspends && graphics==1 && record.ticket==last_entry.ticket);
    /* Exterior keeps advancing: one character walks, one idles. */
    select_temporary(); F32(positions[0],0x18)+=3.0f+(float)round; F32(positions[0],0x20)+=4.0f;
    leave();
    assert(!fake_resumes && !enrolls && !resumes_stub && receipts==r+1 && !record.ticket);
    assert(last_receipt.result==SUDEKIMP_STORY_TEMP_EXTERIOR_EXIT_BALANCED &&
        last_receipt.ticket==last_entry.ticket && last_receipt.tracked==2 &&
        last_receipt.still_present==2 && last_receipt.moved==1 && last_receipt.unchanged==1 &&
        !last_receipt.disable_changed && last_receipt.max_displacement>=5.0f &&
        last_receipt.terrain_present && last_receipt.terrain_enabled_entry==1 &&
        last_receipt.terrain_enabled_exit==1 && last_receipt.terrain_mask_exit==0x20000);
    assert(terrain[0][0xf4]==1 && terrain[1][0xf4]==1 && !entities[0][0x2b] && !entities[1][0x2b]);
}
static void native_round_trip(BOOL attached) {
    fixture(); keep_answer=FALSE; unsigned k=keeps,r=receipts;
    SudekiMpLanStoryTempExteriorStatus before,after;
    assert(SudekiMpLanStoryTempExteriorStatusCopy(&before));
    enter();
    assert(keeps==k+(attached?1u:0u) && fake_suspends==2 && suspend_args[0]==exterior &&
        suspend_args[1]==neighbor && graphics==2 && !record.ticket);
    select_temporary(); leave();
    assert(fake_resumes==1 && resume_args[0]==exterior && enrolls==1 && resumes_stub==1 &&
        resume_stub_arg==neighbor && receipts==r);
    assert(SudekiMpLanStoryTempExteriorStatusCopy(&after) &&
        after.native_entries==before.native_entries+1 && after.native_exits==before.native_exits+1 &&
        after.kept_entries==before.kept_entries && after.skipped_exits==before.skipped_exits);
}
static void state_four_refusal(void) {
    /* A non-state-3 current area is never kept; native path is taken. */
    fixture(); keep_answer=TRUE; U32(exterior,0x34)=2; unsigned k=keeps;
    enter(); assert(keeps==k && fake_suspends==2 && !record.ticket);
}
static void mismatch(void) {
    fixture(); keep_answer=TRUE; enter(); assert(record.ticket && !fake_suspends);
    select_temporary(); U32(exterior,0x3c)=0; unsigned r=receipts;
    leave();
    assert(receipts==r+1 && last_receipt.result==SUDEKIMP_STORY_TEMP_EXTERIOR_EXIT_MISMATCH &&
        !fake_resumes && !enrolls && !resumes_stub && unknown && !record.ticket);
    /* Unknown state keeps later entries native and retains the hooks. */
    fixture(); unsigned k=keeps; enter(); assert(keeps==k && fake_suspends==2);
    assert(!SudekiMpLanStoryTempExteriorUninstall() && enter_hook.installed && exit_hook.installed);
}
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(f!=INVALID_HANDLE_VALUE); DWORD size=GetFileSize(f,NULL),got=0;
    uint8_t *raw=malloc(size); assert(raw && ReadFile(f,raw,size,&got,NULL) && got==size); CloseHandle(f);
    IMAGE_DOS_HEADER *dos=(void *)raw; IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    assert(nt->OptionalHeader.SizeOfImage==IMAGE_SIZE);
    uint8_t *b=VirtualAlloc(NULL,IMAGE_SIZE,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(b); memcpy(b,raw,nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *s=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        assert(s[i].PointerToRawData<=size && s[i].SizeOfRawData<=size-s[i].PointerToRawData);
        assert(s[i].VirtualAddress<=IMAGE_SIZE && s[i].SizeOfRawData<=IMAGE_SIZE-s[i].VirtualAddress);
        memcpy(b+s[i].VirtualAddress,raw+s[i].PointerToRawData,s[i].SizeOfRawData);
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
            if(type==IMAGE_REL_BASED_HIGHLOW) {assert(rva<=IMAGE_SIZE-4); U32(b,rva)+=(uint32_t)delta;}
        }
        offset+=block->SizeOfBlock;
    }
    free(raw); return b;
}
static void install_failure(void) {
    /* Second hook fails: the first is restored and installation can retry. */
    uint8_t *before=malloc(IMAGE_SIZE); assert(before); memcpy(before,mapped,IMAGE_SIZE);
    installs=0; fail_install=2;
    assert(!SudekiMpLanStoryTempExteriorInstall((HMODULE)mapped) && GetLastError()==ERROR_WRITE_FAULT);
    assert(!base && !installed && !unknown && !memcmp(before,mapped,IMAGE_SIZE));
    installs=0; fail_install=1;
    assert(!SudekiMpLanStoryTempExteriorInstall((HMODULE)mapped) && !base);
    assert(!memcmp(before,mapped,IMAGE_SIZE)); fail_install=0; free(before);
}
int main(int argc,char **argv) {
    assert(argc==2 || argc==3); wchar_t path[1024];
    assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024));
    const char *mode=argc==3?argv[2]:"native";
    mapped=map_image(path); uint8_t *before=malloc(IMAGE_SIZE); assert(before);
    memcpy(before,mapped,IMAGE_SIZE);
    /* Foreign/unexpected text refuses installation. */
    mapped[0x6600]^=0xff;
    assert(!SudekiMpLanStoryTempExteriorInstall((HMODULE)mapped) && !base);
    mapped[0x6600]^=0xff;
    /* An active TEMP refuses installation. */
    fixture(); U32(exterior,0x34)=4;
    assert(!SudekiMpLanStoryTempExteriorInstall((HMODULE)mapped) && !base);
    PTR(mapped,WORLD)=NULL;
    install_failure();
    static const unsigned pages[]={0x1000,0x6000}; DWORD protection[2];
    for(unsigned i=0;i<2;++i) {
        assert(VirtualProtect(mapped+pages[i],0x1000,PAGE_EXECUTE_READ,&protection[i]));
        assert(FlushInstructionCache(GetCurrentProcess(),mapped+pages[i],0x1000));
    }
    assert(SudekiMpLanStoryTempExteriorInstall((HMODULE)mapped) && installed);
    enter_original=(void *)(uintptr_t)fake_suspend; exit_original=(void *)(uintptr_t)fake_resume;
    redirect(TRUE);
    native_round_trip(FALSE);
    assert(SudekiMpLanStoryTempExteriorAttach(&consumer_fixture) &&
        !SudekiMpLanStoryTempExteriorAttach(&consumer_fixture));
    if(!strcmp(mode,"mismatch")) {
        mismatch();
        puts("StoryTempExteriorImageTest mismatch: PASS (retained hooks; synthetic world, no gameplay)");
        return 0;
    }
    assert(!strcmp(mode,"native"));
    for(unsigned i=0;i<4;++i) {kept_round_trip(i); native_round_trip(TRUE);}
    state_four_refusal();
    /* Outstanding kept exterior refuses detach and uninstall. */
    fixture(); keep_answer=TRUE; enter(); assert(record.ticket);
    assert(!SudekiMpLanStoryTempExteriorDetach() && !SudekiMpLanStoryTempExteriorUninstall());
    select_temporary(); leave(); assert(!record.ticket);
    assert(!SudekiMpLanStoryTempExteriorUninstall());
    assert(SudekiMpLanStoryTempExteriorDetach());
    SudekiMpLanStoryTempExteriorStatus s; assert(SudekiMpLanStoryTempExteriorStatusCopy(&s));
    assert(s.kept_entries==5 && s.skipped_exits==5 && s.native_entries==6 && s.native_exits==5 &&
        !s.mismatched_exits && !s.unknown && !s.outstanding);
    redirect(FALSE);
    assert(SudekiMpLanStoryTempExteriorUninstall() && !base && !enter_hook.installed);
    PTR(mapped,WORLD)=NULL;
    for(unsigned rva=0x408000;rva<0x40a000;rva+=4) U32(mapped,rva)=U32(before,rva);
    assert(!memcmp(before,mapped,IMAGE_SIZE));
    for(unsigned i=2;i>0;--i) {
        DWORD ignored; MEMORY_BASIC_INFORMATION m;
        assert(VirtualProtect(mapped+pages[i-1],0x1000,protection[i-1],&ignored));
        assert(VirtualQuery(mapped+pages[i-1],&m,sizeof(m)) && m.Protect==protection[i-1]);
    }
    free(before); assert(VirtualFree(mapped,0,MEM_RELEASE));
    puts("StoryTempExteriorImageTest: PASS (native TEMP enter/exit bodies with kept/native exterior; synthetic world, no gameplay)");
    return 0;
}
