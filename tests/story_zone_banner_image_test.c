/* Exact supported image and inert UI fixtures for the H1b zone banner. The
 * native DisplayMessage/HideMessageBox routines are guarded but replaced by
 * recording stubs for the call itself; no UI, font or string data runs. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/hooks/lan_story_zone_banner.c"
static BOOL admission=TRUE;
static unsigned residency_calls,display_calls,hide_calls;
static int last_id=-1,last_mode=-1;
static uint8_t *box_fixture;
static BOOL display_shows=TRUE;
void SudekiMpLogFormat(const char *format,...) {(void)format;}
void SudekiMpLogWrite(const char *text) {(void)text;}
BOOL SudekiMpLanStoryClientResidencyCall(const SudekiMpControlUpdateDispatchWitness *w,
    SudekiMpLanStoryClientResidencyOperation operation,void *context) {
    if(!w || !admission) return FALSE;
    ++residency_calls; assert(!operation(context)); return TRUE;
}
static void __cdecl display_stub(int id,int mode) {
    ++display_calls; last_id=id; last_mode=mode;
    if(display_shows) { box_fixture[0x19d]=1; box_fixture[0x1a7]=1; uint32_t t=0x1234u; memcpy(box_fixture+0x1a0,&t,4); }
}
static void __cdecl hide_stub(void) { ++hide_calls; box_fixture[0x1a7]=0; box_fixture[0x19d]=0; }
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
static void num(void *at,uint32_t value) {memcpy(at,&value,4);}
int main(int argc,char **argv) {
    wchar_t path[32768];
    assert(argc==2 && MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,32768));
    uint8_t *image=map_image(path);
    assert(SudekiMpLanStoryZoneBannerInitialize((HMODULE)image));
    assert(display_call==(DisplayMessageCall)(image+0xac6a0) && hide_call==(HideMessageBoxCall)(image+0xac730));
    for(unsigned i=0;i<sizeof(codes)/sizeof(codes[0]);++i) {
        image[codes[i].rva]^=1; assert(!code_exact(image));
        image[codes[i].rva+codes[i].size-1]^=1; image[codes[i].rva]^=1; assert(!code_exact(image));
        image[codes[i].rva+codes[i].size-1]^=1; assert(code_exact(image));
        printf("Named native guard: %s PASS\n",codes[i].name);
    }
    for(unsigned i=0;i<sizeof(relocations)/sizeof(relocations[0]);++i) {
        image[relocations[i].rva]^=1; assert(!code_exact(image));
        image[relocations[i].rva]^=1; assert(code_exact(image));
    }
    display_call=display_stub; hide_call=hide_stub;
    uint8_t *text=calloc(1,0x10),*box=calloc(1,0x1a8),*help=calloc(1,0x3c),*layer=calloc(1,0xc0);
    uint8_t *scene=calloc(1,0x174),*gizmo=calloc(1,0x60);
    assert(text && box && help && layer && scene && gizmo); box_fixture=box;
    ptr(text,image+0x2ca1b0); ptr(text+4,text); ptr(text+8,text); *(uint16_t *)(text+0xc)=1;
    ptr(box,image+0x2cb5bc); ptr(box+0x40,image+0x2cb610);
    ptr(help,image+0x2cb63c); num(help+0x38,2);
    ptr(layer,image+0x2caf9c); num(layer+0xb8,1);
    ptr(scene,image+0x2c5210); ptr(scene+0x170,layer);
    ptr(gizmo,image+0x2cb784);
    ptr(image+0x409e04,text); ptr(image+0x3c2fb8,box); ptr(image+0x3c2fc0,help);
    ptr(image+0x3c2f88,layer); ptr(image+0x408d1c,scene); ptr(image+0x3c2fc4,gizmo);
    assert(!show_refusal());
    /* Each native Show gate (and each owner the lookup/hide use) refuses. */
    ptr(image+0x409e04,NULL); assert(!strcmp(show_refusal(),"strings")); ptr(image+0x409e04,text);
    *(uint16_t *)(text+0xc)=0; assert(!strcmp(show_refusal(),"strings")); *(uint16_t *)(text+0xc)=1;
    ptr(box+0x40,image); assert(!strcmp(show_refusal(),"box")); ptr(box+0x40,image+0x2cb610);
    num(help+0x38,1); assert(!strcmp(show_refusal(),"help_box")); num(help+0x38,2);
    num(layer+0xb8,2); assert(!strcmp(show_refusal(),"layer")); num(layer+0xb8,1);
    scene[0x60]=1; assert(!strcmp(show_refusal(),"scene")); scene[0x60]=0;
    ptr(scene+0x170,box); assert(!strcmp(show_refusal(),"scene")); ptr(scene+0x170,layer);
    ptr(gizmo,image); assert(!strcmp(show_refusal(),"gizmo")); ptr(gizmo,image+0x2cb784);
    box[0x48]=0x20; assert(!strcmp(show_refusal(),"box_busy")); box[0x48]=0;
    box[0x1a7]=1; assert(!strcmp(show_refusal(),"box_busy")); box[0x1a7]=0;
    box[0x19d]=1; box[0x48]=1; assert(!strcmp(show_refusal(),"box_busy")); box[0x19d]=0; box[0x48]=0;
    image[0xabb80]^=1; assert(!strcmp(show_refusal(),"guards")); image[0xabb80]^=1;
    assert(!show_refusal() && !display_calls);
    puts("Zone banner native Show gates and owner proofs refuse before any call: PASS");
    SudekiMpControlUpdateDispatchWitness w={.native_thread_id=GetCurrentThreadId(),.service_post_original_exact=TRUE};
    /* Shown once, after the gates; hidden at 3 s by token; never twice. */
    SudekiMpLanStoryZoneBannerRequest(5656,1000);
    SudekiMpLanStoryZoneBannerService(1000,&w);
    assert(display_calls==1 && last_id==5656 && last_mode==0 && shown && !pending);
    SudekiMpLanStoryZoneBannerService(3999,&w); assert(!hide_calls && shown);
    SudekiMpLanStoryZoneBannerService(4000,&w); assert(hide_calls==1 && !shown && !box[0x1a7]);
    SudekiMpLanStoryZoneBannerService(5000,&w); assert(display_calls==1 && hide_calls==1);
    /* The game replaced our message: never close it. */
    SudekiMpLanStoryZoneBannerRequest(5656,6000); SudekiMpLanStoryZoneBannerService(6000,&w);
    assert(display_calls==2 && shown); num(box+0x1a0,0x9999);
    SudekiMpLanStoryZoneBannerService(9000,&w); assert(hide_calls==1 && !shown);
    box[0x1a7]=0; box[0x19d]=0;
    /* A refused show retries for 1 s, then drops without a call. */
    help[0x38]=1; SudekiMpLanStoryZoneBannerRequest(5656,10000);
    SudekiMpLanStoryZoneBannerService(10000,&w); SudekiMpLanStoryZoneBannerService(10900,&w);
    assert(pending && display_calls==2);
    help[0x38]=2; SudekiMpLanStoryZoneBannerService(10950,&w); assert(display_calls==3 && shown);
    SudekiMpLanStoryZoneBannerService(14000,&w); assert(!shown && hide_calls==2);
    help[0x38]=1; SudekiMpLanStoryZoneBannerRequest(5656,20000);
    SudekiMpLanStoryZoneBannerService(20000,&w); SudekiMpLanStoryZoneBannerService(21001,&w);
    assert(!pending && display_calls==3); help[0x38]=2;
    /* Closed admission and cancel: no call. */
    admission=FALSE; SudekiMpLanStoryZoneBannerRequest(5656,30000); SudekiMpLanStoryZoneBannerService(30000,&w);
    assert(pending && display_calls==3); admission=TRUE;
    SudekiMpLanStoryZoneBannerCancel(); SudekiMpLanStoryZoneBannerService(30001,&w); assert(display_calls==3);
    ++w.native_thread_id; SudekiMpLanStoryZoneBannerRequest(5656,31000); SudekiMpLanStoryZoneBannerService(31000,&w);
    assert(display_calls==3); --w.native_thread_id; SudekiMpLanStoryZoneBannerCancel();
    /* A box the game dropped is not ours to hide later. */
    display_shows=FALSE; SudekiMpLanStoryZoneBannerRequest(5656,32000); SudekiMpLanStoryZoneBannerService(32000,&w);
    assert(display_calls==4 && !shown && !pending); display_shows=TRUE;
    /* Teardown hides ours by token; a later reset is inert. */
    SudekiMpLanStoryZoneBannerRequest(5656,33000); SudekiMpLanStoryZoneBannerService(33000,&w);
    assert(display_calls==5 && shown);
    SudekiMpLanStoryZoneBannerReset(); assert(hide_calls==3 && !shown);
    SudekiMpLanStoryZoneBannerReset(); assert(hide_calls==3);
    puts("Zone banner show once, token-matched 3 s hide, refusal window, admission, drop and teardown: PASS");
    free(text); free(box); free(help); free(layer); free(scene); free(gizmo);
    VirtualFree(image,0,MEM_RELEASE);
    return 0;
}
