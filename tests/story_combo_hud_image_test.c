/* Verified image, native hook bridges and native icon refresh; inert GUI
 * resources, no renderer/device or gameplay admission. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/hooks/lan_story_combo_hud.c"
#include "engine/buki_replica_native.h"
static BOOL valid_owner=TRUE;
void SudekiMpLogFormat(const char *format,...) {(void)format;}
BOOL SudekiMpLanStoryObserverRosterStillExact(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *r) {return w && r && valid_owner;}
BOOL SudekiMpLanStoryObserverNativeRosterExact(const SudekiMpLanStoryNativeRoster *r) {return r && valid_owner;}
static BOOL exact(const SudekiMpLanStoryNativeRoster *r,void *ctx) {(void)ctx; return r && valid_owner;}
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


static unsigned hide_calls,show_calls,label_calls,icon_calls;
static uint8_t *expected_hud;
static void __attribute__((used,noinline)) mode_check(unsigned mode,void *h,void *ecx,unsigned show) {
    assert(h==expected_hud && ecx==h && mode<=6u);
    if(show) ++show_calls; else ++hide_calls;
}
static void __attribute__((naked,noinline)) fake_show(void) {
    __asm__ volatile("pushl $1\n\tpushl %ecx\n\tpushl %esi\n\tpushl %eax\n\tcall _mode_check\n\taddl $16,%esp\n\tret\n\t");
}
static void __attribute__((naked,noinline)) fake_hide(void) {
    __asm__ volatile("pushl $0\n\tpushl %ecx\n\tpushl %esi\n\tpushl %eax\n\tcall _mode_check\n\taddl $16,%esp\n\tret\n\t");
}
static void __attribute__((regparm(1))) fake_label(void *h) {assert(h==expected_hud);++label_calls;}
static void __attribute__((thiscall)) fake_gizmo(void *g) {(void)g;++icon_calls;}
static void __attribute__((naked,noinline)) invoke_eax(void *fn __attribute__((unused)),void *c __attribute__((unused))) {
    __asm__ volatile("movl 8(%esp),%eax\n\tjmp *4(%esp)\n\t");
}
static void __attribute__((naked,noinline)) invoke_window(void *fn __attribute__((unused)),void *sub __attribute__((unused)),unsigned mark __attribute__((unused))) {
    __asm__ volatile("movl 8(%esp),%ecx\n\tpushl 12(%esp)\n\tcall *8(%esp)\n\tret\n\t");
}
static void ptr(void *p,void *v) {memcpy(p,&v,4);}
static void num(void *p,unsigned n) {memcpy(p,&n,4);}
int main(int argc,char **argv) {
    wchar_t path[32768]; assert(argc==2 && MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,32768));
    uint8_t *image=map_image(path);
    assert(code_exact(image));
    for(unsigned i=0;i<sizeof(codes)/sizeof(codes[0]);++i) {
        for(unsigned n=0;n<codes[i].n;++n) {
            image[codes[i].rva+n]^=1; assert(!code_exact(image));
            image[codes[i].rva+n]^=1; assert(code_exact(image));
        }
        printf("Named native guard: %s PASS\n",codes[i].name);
    }
    image[ACCEPT_CALL+1]^=1; assert(!code_exact(image));image[ACCEPT_CALL+1]^=1;
    assert(SudekiMpBukiReplicaNativeImageMatches((HMODULE)image));
    assert(SudekiMpLanStoryComboHudInitialize((HMODULE)image,TRUE));
    assert(SudekiMpBukiReplicaNativeImageMatches((HMODULE)image));
    puts("BukiReplicaNativeImageMatches remains exact with all HUD hooks installed PASS");
    uint8_t actor[0xc0]={0},component[0xc0]={0},controller[0x250]={0};
    ptr(actor,image+TAL_VTABLE);ptr(actor+0xb8,component);
    ptr(component,image+COMBO_VTABLE);ptr(component+0x10,actor);ptr(controller+0x248,actor);
    SudekiMpLanStoryNativeRoster r={.epoch=1,.available_mask=4,.leader_character=2,.controller=controller};
    r.actors[2]=actor;
    SudekiMpControlUpdateDispatchWitness w={0};
    /* Prevent original host display functions from resolving a game party. */
    ptr(image+0x408d94,NULL);ptr(image+HUD_GLOBAL,NULL);
    assert(SudekiMpLanStoryComboHudCapture(&w,&r,2u)==EMPTY);
    num(component+0x68,1);component[0xa0]=1;
    invoke_eax((void *)accepted,component);
    assert(SudekiMpLanStoryComboHudCapture(&w,&r,2u)==0x81f8u);
    invoke_window(image+0xd0a10,component+0x3c,1);
    assert(observed[2].state==0x83f8u); /* Native argument, no inferred timeout. */
    num(component+0x64,1);num(component+0x6c,2);component[0xa1]=0;
    invoke_eax((void *)accepted,component); assert(observed[2].state==0x81d8u); /* Mistimed strong. */
    num(component+0x64,2);num(component+0x70,1);component[0xa2]=1;
    invoke_eax((void *)accepted,component); assert(observed[2].state==0x8018u);
    valid_owner=FALSE; invoke_eax(image+RESET,component);assert(observed[2].state==0x8018u);
    valid_owner=TRUE;invoke_eax(image+RESET,component);assert(observed[2].state==EMPTY);
    assert(*(uint32_t *)(component+0x64)==0 && *(uint32_t *)(component+0x68)==0);
    num(component+0x68,1);component[0xa0]=1;invoke_eax((void *)accepted,component);
    ++r.epoch; assert(!SudekiMpLanStoryComboHudCapture(&w,&r,2u)); /* No invented late-join history. */
    invoke_eax(image+RESET,component);assert(observed[2].state==EMPTY);
    /* Both melee actors share native seams, never their accepted rows. */
    uint8_t buki[0xc0]={0},buki_combo[0xc0]={0};
    ptr(buki,image+BUKI_VTABLE);ptr(buki+0xb8,buki_combo);
    ptr(buki_combo,image+COMBO_VTABLE);ptr(buki_combo+0x10,buki);
    r.actors[0]=buki;r.available_mask=5;
    assert(SudekiMpLanStoryComboHudCapture(&w,&r,0u)==EMPTY);
    num(buki_combo+0x68,2);buki_combo[0xa0]=1;
    invoke_eax((void *)accepted,buki_combo);
    assert(observed[0].state==0x81f9u && observed[2].state==EMPTY);
    invoke_window(image+0xd0a10,buki_combo+0x3c,1);
    assert(observed[0].state==0x83f9u && observed[2].state==EMPTY);
    ++r.epoch;
    assert(!SudekiMpLanStoryComboHudCapture(&w,&r,0u));
    assert(SudekiMpLanStoryComboHudCapture(&w,&r,2u)==EMPTY);
    invoke_eax(image+RESET,buki_combo);
    assert(observed[0].state==EMPTY && observed[2].state==EMPTY);
    assert(!SudekiMpLanStoryComboHudCapture(&w,&r,1u));
    uint8_t patch[7];memcpy(patch,image+WINDOW,7);image[WINDOW+1]^=1;
    void *retained=window_original;
    assert(!SudekiMpLanStoryComboHudUninstall() && base && window_original==retained && window_hook.installed);
    memcpy(image+WINDOW,patch,7);assert(SudekiMpLanStoryComboHudUninstall());
    assert(code_exact(image) && SudekiMpBukiReplicaNativeImageMatches((HMODULE)image));
    assert(SudekiMpLanStoryComboHudInitialize((HMODULE)image,FALSE));
    uint8_t hud[0x440]={0},assets[0x14]={0},entries[0xc]={0},bank[0x14]={0};
    void *textures[8]={0};void *vtable[6]={0};vtable[5]=(void *)fake_gizmo;
    ptr(image+HUD_GLOBAL,hud);ptr(hud,image+HUD_VTABLE);hud[0x44]=1;
    num(hud+0x58,0x80);*(float *)(hud+0x1f0)=.1f;
    for(unsigned i=0;i<4;++i) ptr(hud+0xec+i*0x40,vtable);
    ptr(image+0x3c3038,assets);ptr(assets+0x10,entries);ptr(entries+8,bank);ptr(bank+0x10,textures);
    expected_hud=hud;hide_fn=(void *)fake_hide;show_fn=(void *)fake_show;label_fn=(void *)fake_label;
    /* ICONS executes its exact native body, through the real EAX bridge. */
    SudekiMpLanStoryFrame f={.epoch=1,.revision=1,.sequence=1,.available_mask=4,.leader_character=2,.combat_mode=1};
    f.actors[2]=(SudekiMpLanStoryActor){.character=2,.generation=1,.native_pose=1,.hp=10,.facing_z=1,
        .weapon_item_plus_one=1,.weapon_visible=1,.weapon_attachment={1,0},.combo_hud=0x81f8};
    assert(SudekiMpLanStoryFrameValid(&f));
    assert(SudekiMpLanStoryComboHudPresent(&r,&f,TRUE,exact,NULL));
    assert(*(uint32_t *)(hud+0x48)==3 && *(uint32_t *)(hud+0x1f4)==0);
    assert(*(uint32_t *)(hud+0x148)==0 && *(uint32_t *)(hud+0x188)==2); /* first icon visible */
    assert(hide_calls==1 && show_calls==1 && icon_calls==3);
    assert(SudekiMpLanStoryComboHudPresent(&r,&f,TRUE,exact,NULL) && icon_calls==3);
    f.actors[2].combo_hud=0x83f8;assert(SudekiMpLanStoryComboHudPresent(&r,&f,TRUE,exact,NULL));
    assert(*(uint32_t *)(hud+0x188)==0 && *(uint32_t *)(hud+0x1c8)==2); /* timing marker */
    f.actors[2].combo_hud=0x8008;assert(SudekiMpLanStoryComboHudPresent(&r,&f,TRUE,exact,NULL));
    assert(hud[0x200] && hud[0x201]);*(float *)(hud+0x1ec)=.03f;
    uint32_t timer;memcpy(&timer,hud+0x1ec,4);
    assert(SudekiMpLanStoryComboHudPresent(&r,&f,TRUE,exact,NULL) && !memcmp(&timer,hud+0x1ec,4));
    valid_owner=FALSE;assert(!SudekiMpLanStoryComboHudRelease(&r,exact,NULL) && shown.active);valid_owner=TRUE;
    assert(!SudekiMpLanStoryComboHudUninstall());stopping=FALSE; /* fixture retry before native retire */
    assert(SudekiMpLanStoryComboHudPresent(&r,&f,FALSE,exact,NULL) && !shown.active);
    assert(*(uint32_t *)(hud+0x48)==0);
    assert(SudekiMpLanStoryComboHudPresent(&r,&f,TRUE,exact,NULL));
    num(hud+0x58,0x82);assert(SudekiMpLanStoryComboHudPresent(&r,&f,TRUE,exact,NULL) && !shown.active);
    num(hud+0x58,0x80);assert(SudekiMpLanStoryComboHudPresent(&r,&f,TRUE,exact,NULL));
    f.combat_mode=0;assert(SudekiMpLanStoryComboHudPresent(&r,&f,TRUE,exact,NULL) && !shown.active);
    /* Selecting Buki changes only the presented native row, not Tal's history.
     * A later ranged selection retires the old row without mutating its HUD. */
    f.combat_mode=1;r.leader_character=0;ptr(controller+0x248,buki);
    f.available_mask=5;f.actors[0]=f.actors[2];
    f.actors[0].character=0;f.actors[0].weapon_item_plus_one=37;
    f.actors[0].weapon_attachment[1]=1;f.actors[0].combo_hud=0x81f9;
    assert(SudekiMpLanStoryComboHudPresent(&r,&f,TRUE,exact,NULL));
    assert(shown.actor==buki && shown.character==0 && *(uint32_t *)(hud+0x1f4)==1);
    f.actors[0].weapon_attachment[1]=2;
    assert(SudekiMpLanStoryComboHudPresent(&r,&f,TRUE,exact,NULL) && !shown.active);
    f.actors[0].weapon_attachment[1]=1;
    assert(SudekiMpLanStoryComboHudPresent(&r,&f,TRUE,exact,NULL));
    r.leader_character=3;ptr(controller+0x248,NULL);num(hud+0x48,4);
    assert(SudekiMpLanStoryComboHudRelease(&r,exact,NULL) && !shown.active);
    assert(*(uint32_t *)(hud+0x48)==4); /* belongs to the new native selection */
    assert(SudekiMpLanStoryComboHudUninstall());
    VirtualFree(image,0,MEM_RELEASE);
    puts("Story combo native accepted/mistimed/reset/window, UI visibility, duplicate blink, owner refusal, teardown retry PASS");
    return 0;
}
