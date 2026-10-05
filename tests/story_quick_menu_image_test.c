/* Exact retail image, synthetic UI/actor owners. Only the verified native Q
 * toggle leaf executes; no retail menu, task, script, skill or world runs. */
#include "hooks/call_hook.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned step,fail_step;
static BOOL test_pointer(SudekiMpPointerHook *,void **,const void *,const void *);
static BOOL test_call(SudekiMpRelativeCallHook *,uint8_t *,const void *,const void *);
#define SudekiMpInstallPointerHook test_pointer
#define SudekiMpInstallRelativeCallHook test_call
#include "../src/hooks/lan_story_quick_menu.c"
#undef SudekiMpInstallPointerHook
#undef SudekiMpInstallRelativeCallHook
static BOOL fail(void) {
    if(++step==fail_step) { SetLastError(ERROR_WRITE_FAULT); return TRUE; } return FALSE;
}
static BOOL test_pointer(SudekiMpPointerHook *h,void **p,const void *a,const void *b) {
    return !fail() && SudekiMpInstallPointerHook(h,p,a,b);
}
static BOOL test_call(SudekiMpRelativeCallHook *h,uint8_t *p,const void *a,const void *b) {
    return !fail() && SudekiMpInstallRelativeCallHook(h,p,a,b);
}
static uint8_t *mapped;
static size_t image_size;
static BOOL contained=TRUE,input_exact=TRUE,realtime=TRUE,queue_ok=TRUE;
static SudekiMpLanStoryNativeRoster roster;
static uint8_t menu[0x224],controller[0x24c],actor[0x100],other_actor[0x100],group[0xd8],front[0x178],hud[0xd0];
static uint8_t list[0x480],row[0x88];
static void *rows[1];
static unsigned opened,closed,ui_calls,requests,mutes,brackets,script_native_calls,selected;
void SudekiMpLogFormat(const char *format,...) { (void)format; }
BOOL SudekiMpLanStoryRealtimeExact(HMODULE image) { return realtime && image==(HMODULE)mapped; }
BOOL SudekiMpLanStoryInputExact(void *c,void *a) { return input_exact && c==controller && a==actor; }
void SudekiMpLanStoryInputMuteMovement(void) { ++mutes; }
BOOL SudekiMpLanStoryClientRosterExact(const SudekiMpLanStoryNativeRoster *r) { return contained && r==&roster; }
BOOL SudekiMpLanStoryClientUiPresent(SudekiMpLanStoryClientPresentation callback,void *context,
    SudekiMpLanStoryClientEffectsWitness witness,void *witness_context) {
    if(!contained || !witness(witness_context)) return FALSE;
    ++brackets; BOOL result=callback(&roster,NULL,context);
    assert(witness(witness_context) || !menu_owner);
    return result && contained;
}
static BOOL queue(void *a,unsigned slot) {
    assert(a==actor && slot<6); if(!queue_ok) return FALSE;
    ++requests; selected=slot; return TRUE;
}
typedef void *(__attribute__((stdcall)) *ScriptCall)(void *,void **,unsigned,unsigned,unsigned,unsigned,unsigned);
static void * __attribute__((stdcall)) script_fallback(void *descriptor,void **out,
    unsigned c,unsigned d,unsigned e,unsigned f,unsigned g) {
    assert(descriptor==(void *)0x11 && c==0x33 && d==0x44 && e==0x55 && f==0x66 && g==0x77);
    ++script_native_calls; *out=(void *)0x1234; return out;
}
static void invoke_script(BOOL suppressed) {
    void *out=(void *)0xdeadbeef;
    ScriptCall fn=(ScriptCall)(uintptr_t)submit_script;
    SetLastError(0x5678); double live;
    __asm__ volatile("fldpi" : : : "memory");
    void *result=fn((void *)0x11,&out,0x33,0x44,0x55,0x66,0x77);
    __asm__ volatile("fstpl %0" : "=m"(live) : : "memory");
    assert(result==&out && out==(suppressed?NULL:(void *)0x1234));
    assert(live>3.14159 && live<3.14160 && GetLastError()==0x5678);
}
static uint8_t __attribute__((thiscall)) fake_input(void *m,unsigned kind,unsigned command,unsigned value) {
    (void)value; assert(m==menu); ++ui_calls;
    /* Native back/cancel dispatches through the close vtable slot. */
    if(kind==5u && command==1u) ((MenuTransition)*(void **)(mapped+MENU_VT+0x44u))(m);
    return 1;
}
static void __attribute__((thiscall)) fake_open(void *m) {
    assert(m==menu && transition==1 && menu_owner==menu); ++opened; menu[0x29]=1;
    invoke_script(TRUE);
}
static void __attribute__((thiscall)) fake_close(void *m) {
    assert(m==menu && transition==2 && menu_owner==menu); ++closed; menu[0x29]=0;
    invoke_script(TRUE);
    *(void **)(front+0x170)=hud;
}
static uint8_t __attribute__((thiscall)) fake_hud(void *m,unsigned kind,unsigned command,unsigned value) {
    assert(m==hud && kind==16 && command==0 && value==0);
    /* HUD49C930 case(event-5)==11 sets active BEFORE invoking open. */
    menu[0x29]=1; ((MenuTransition)*(void **)(mapped+MENU_VT+0x40))(menu);
    *(void **)(front+0x170)=menu; return 1;
}
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
static void setup(void) {
    step=fail_step=0; assert(SudekiMpLanStoryQuickMenuInstall((HMODULE)mapped,queue) && step==5);
    native_open=fake_open; native_close=fake_close; native_input=fake_input;
    native_submit=(void *)(uintptr_t)script_fallback;
    memset(menu,0,sizeof(menu)); memset(front,0,sizeof(front)); memset(hud,0,sizeof(hud));
    memset(list,0,sizeof(list)); memset(row,0,sizeof(row));
    *(void **)(mapped+MENU_GLOBAL)=menu; *(void **)menu=mapped+MENU_VT;
    *(void **)(mapped+FRONT_GLOBAL)=front; front[0x8c]=1;
    *(void **)(mapped+0x408d94)=group; *(void **)(group+0x90)=actor;
    *(void **)(front+0x170)=*(void **)(front+0x174)=hud;
    *(void **)hud=mapped+0x2caf9c; *(void **)(hud+0x70)=menu;
    /* Synthetic HUD receiver at the guarded retail address. Only A080's
     * short toggle body runs from the game image; its virtual call lands here. */
    mapped[0x9c930]=0xe9;
    int32_t branch=(int32_t)((uintptr_t)fake_hud-(uintptr_t)(mapped+0x9c935));
    memcpy(mapped+0x9c931,&branch,4);
    *(void **)(menu+0x208)=hud; /* idle category animation fixture */
    *(void **)(menu+0x214)=list; *(unsigned *)(list+0x474)=1;
    rows[0]=row; *(void **)(list+0x47c)=rows; *(unsigned *)(row+0x84)=3;
    roster=(SudekiMpLanStoryNativeRoster){.controller=controller,.group=group,.leader_character=2,.available_mask=12};
    roster.actors[2]=actor;
    contained=input_exact=queue_ok=TRUE;
    opened=closed=ui_calls=requests=mutes=brackets=script_native_calls=0;
}
static void open_fixture(void) {
    assert(SudekiMpLanStoryQuickMenuService(&roster,7,TRUE,TRUE,TRUE));
    assert(opened==closed+1 && visible() && !fault);
}
static void input(unsigned category,unsigned kind,unsigned command,unsigned value) {
    *(unsigned *)(menu+0x204)=category;
    ((MenuInput)*(void **)(mapped+MENU_VT+0x2c))(menu,kind,command,value);
}
static DWORD WINAPI foreign(void *unused) {
    (void)unused; assert(!SudekiMpLanStoryQuickMenuUninstall()); return 0;
}
static void lifecycle(void) {
    uint8_t *copy=malloc(image_size); assert(copy); memcpy(copy,mapped,image_size);
    for(unsigned i=1;i<=5;++i) {
        step=0; fail_step=i;
        assert(!SudekiMpLanStoryQuickMenuInstall((HMODULE)mapped,queue));
        assert(!installed && !retained && !memcmp(copy,mapped,image_size));
    }
    unsigned sites[]={MENU_VT+0x2c,MENU_VT+0x40,MENU_VT+0x44,0x990fa,0x992e1};
    for(unsigned i=0;i<5;++i) {
        mapped[sites[i]]^=1; step=fail_step=0;
        assert(!SudekiMpLanStoryQuickMenuInstall((HMODULE)mapped,queue));
        mapped[sites[i]]^=1; assert(!retained && !memcmp(copy,mapped,image_size));
    }
    setup(); open_fixture(); assert(!SudekiMpLanStoryQuickMenuUninstall() && menu_owner==menu);
    assert(SudekiMpLanStoryQuickMenuService(&roster,7,FALSE,FALSE,FALSE) && !menu_owner);
    HANDLE worker=CreateThread(NULL,0,foreign,NULL,0,NULL); assert(worker);
    assert(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0); CloseHandle(worker);
    void *owned=*(void **)(mapped+MENU_VT+0x2c); *(void **)(mapped+MENU_VT+0x2c)=NULL;
    assert(!SudekiMpLanStoryQuickMenuUninstall() && script_hooks[0].installed && retained);
    *(void **)(mapped+MENU_VT+0x2c)=owned;
    assert(SudekiMpLanStoryQuickMenuUninstall()); invoke_script(FALSE); assert(script_native_calls==1);
    free(copy);
}
int main(int argc,char **argv) {
    assert(argc==2); wchar_t path[1024]; assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024));
    mapped=map_image(path); lifecycle(); setup(); open_fixture();
    /* A render/resource-job stall must not collapse an independently owned
     * local menu; casting remains closed until a fresh frame is available. */
    for(unsigned i=0;i<120;++i)
        assert(SudekiMpLanStoryQuickMenuService(&roster,7,TRUE,FALSE,FALSE) && visible());
    input(0,5,0,1); assert(!requests && visible());
    assert(SudekiMpLanStoryQuickMenuService(&roster,7,TRUE,TRUE,FALSE));
    /* Retail list population reads native group+90. A readable different
     * character must never display/submit as the controlled character. */
    *(void **)(group+0x90)=other_actor;
    input(0,5,0,1); assert(!requests && visible());
    *(void **)(group+0x90)=actor;
    unsigned before=ui_calls;
    for(unsigned c=1;c<9;++c) input(c,5,0,1);
    input(0,6,0,1); input(0,5,0,0); input(0,0x19,0,1);
    assert(!requests && ui_calls==before && visible());
    queue_ok=FALSE; input(0,5,0,1); assert(!requests && visible()); queue_ok=TRUE;
    contained=FALSE; input(0,5,0,1); assert(!requests); contained=TRUE;
    input_exact=FALSE; input(0,5,0,1); assert(!requests); input_exact=TRUE;
    *(unsigned *)(hud+0xb4)=1; input(0,5,0,1); assert(!requests); *(unsigned *)(hud+0xb4)=0;
    *(unsigned *)(row+0x84)=6; input(0,5,0,1); assert(!requests); *(unsigned *)(row+0x84)=3;
    input(0,5,0,1); assert(requests==1 && selected==3 && !menu_owner && closed==1);
    input(0,5,0,1); assert(requests==1); /* repeated confirm cannot submit again */
    open_fixture(); input(0,5,1,1); assert(!menu_owner && closed==2); /* native back */
    open_fixture(); assert(SudekiMpLanStoryQuickMenuService(&roster,8,TRUE,TRUE,FALSE));
    assert(!menu_owner && closed==3 && owner_transaction==8); /* binding change closes */
    /* The same UI adapter binds either character's local front; never infer
     * the host leader. Its rows submit only against that exact local actor. */
    roster.actors[2]=other_actor; roster.actors[3]=actor; roster.leader_character=3;
    assert(SudekiMpLanStoryQuickMenuService(&roster,9,TRUE,TRUE,TRUE) && visible());
    *(unsigned *)(row+0x84)=1;
    input(0,5,0,1); assert(requests==2 && selected==1 && !visible() && closed==4);
    assert(SudekiMpLanStoryQuickMenuService(&roster,9,TRUE,FALSE,TRUE) && !visible());
    assert(SudekiMpLanStoryQuickMenuService(&roster,9,TRUE,TRUE,TRUE) && visible());
    assert(SudekiMpLanStoryQuickMenuService(&roster,9,TRUE,FALSE,TRUE) && !visible());
    assert(closed==5); /* explicit toggle/cancel still works during a stall */
    assert(!script_native_calls && brackets>0 && !fault);
    assert(SudekiMpLanStoryQuickMenuUninstall());
    VirtualFree(mapped,0,MEM_RELEASE);
    puts("story Q menu exact-image/synthetic UI, request-only selection, RET28 and rollback tests passed (no gameplay)");
    return 0;
}
