/* Exact supported image and inert audio-owner fixtures. Native encounter
 * listener executes; no sound device, music cue, script or bank load executes. */
#include <assert.h>
#include <stdlib.h>
#include "../src/hooks/lan_story_music.c"
static BOOL exact_witness=TRUE,current_area=TRUE,admission=TRUE;
static unsigned calls,loads;
void SudekiMpLogFormat(const char *format,...) {(void)format;}
BOOL SudekiMpControlSeparationUpdateDispatchWitnessStillExact(
    const SudekiMpControlUpdateDispatchWitness *w) {return w && exact_witness;}
BOOL SudekiMpLanStoryEffectsInCurrentArea(const SudekiMpStoryEffectState *h) {
    return h && current_area;
}
BOOL SudekiMpLanStoryClientResidencyCall(const SudekiMpControlUpdateDispatchWitness *w,
    SudekiMpLanStoryClientResidencyOperation operation,void *context) {
    if(!w || !admission) return FALSE;
    ++calls; if(operation(context)) ++loads; return TRUE;
}
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
static uint32_t bits(float value) {uint32_t out; memcpy(&out,&value,4); return out;}
int main(int argc,char **argv) {
    wchar_t path[32768];
    assert(argc==2 && MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,32768));
    uint8_t *image=map_image(path);
    assert(SudekiMpLanStoryMusicInitialize((HMODULE)image));
    for(unsigned i=0;i<sizeof(codes)/sizeof(codes[0]);++i) {
        image[codes[i].rva]^=1; assert(!code_exact(image));
        image[codes[i].rva]^=1; assert(code_exact(image));
        printf("Named native guard: %s PASS\n",codes[i].name);
    }
    for(unsigned i=0;i<sizeof(relocations)/sizeof(relocations[0]);++i) {
        image[relocations[i].rva]^=1; assert(!code_exact(image));
        image[relocations[i].rva]^=1; assert(code_exact(image));
    }
    image[0x2c7c00]^=1; assert(!code_exact(image)); image[0x2c7c00]^=1;
    image[0x2c7c30]^=1; assert(!code_exact(image)); image[0x2c7c30]^=1;
    uint8_t *score=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    uint8_t manager[0x15c]={0},pool[0x14]={0},emitter[0x14]={0},bank[0x34]={0};
    uint8_t soundbank[0x3c]={0},entries[24]={0},cue[0x30]={0};
    uint8_t *banks[]={bank}; char name[96]="BS_IllumCountry.xsb";
    assert(score); ptr(score,image+0x2c7bf4); ptr(score+8,image+0x2c7bfc);
    ptr(score+0x2c,image+0x2c7c1c); ptr(score+0x30,image+0x2c7c24);
    ptr(manager,image+0x2e1dc0); ptr(manager+0x2c,banks); num(manager+0x30,1);
    ptr(pool,image+0x2e1ee0); num(pool+8,1); ptr(pool+0x10,entries);
    ptr(emitter,image+0x2e1db4); ptr(emitter+4,bank); emitter[0x10]=1;
    ptr(bank,image+0x2e1e40); ptr(bank+0x10,name); num(bank+0x18,2); ptr(bank+0x1c,soundbank);
    ptr(soundbank,image+0x2e1f68); memcpy(soundbank+8,"SSBN",4);
    ptr(score+0x44,emitter); num(score+0x4c,0x80000000); strcpy((char *)score+0x50,"BS_IllumCountry");
    num(score+0x78,bits(.4f)); num(score+0x7c,bits(.4f)); num(score+0x80,bits(.6f));
    num(score+0xa0,bits(.6f)); num(score+0xa4,bits(120.f)); num(score+0xa8,bits(20.f));
    num(score+0x6c,bits(.8f)); num(score+0x70,bits(.1f)); num(score+0x74,bits(.9f));
    ptr(image+0x409d80,score); ptr(image+0x409e58,manager); ptr(image+0x409e8c,pool);
    SudekiMpControlUpdateDispatchWitness w={.native_thread_id=GetCurrentThreadId(),.service_post_original_exact=TRUE};
    SudekiMpStoryEffectState h={.epoch=1,.revision=1,.sequence=1,.current="castle_road"};
    SudekiMpLanStoryMusicCapture(&h,&w);
    assert(h.music_flags==1 && h.music_intensity==bits(.4f));
    assert(!strcmp(h.music_score,"BS_IllumCountry") && h.music_curve[4]==bits(120.f));
    h.music_flags=3; h.music_intensity=bits(.65f); h.music_curve[4]=bits(110.f);
    assert(SudekiMpLanStoryMusicFollow(&h,&w));
    assert(score[0xac]==1 && word(score+0x78)==bits(.65f) && word(score+0xa4)==bits(110.f));
    assert(calls==1 && loads==0); /* scalar listener never adopts residency */
    assert(word(score+0x6c)==bits(.8f) && word(score+0x70)==bits(.1f) && word(score+0x74)==bits(.9f));
    assert(SudekiMpLanStoryMusicFollow(&h,&w) && calls==1); /* duplicate */
    ++h.sequence; h.music_flags=1; h.music_intensity=bits(.4f);
    assert(SudekiMpLanStoryMusicFollow(&h,&w) && score[0xac]==0 && word(score+0x78)==bits(.4f));
    ++h.sequence; h.music_flags=3; h.music_intensity=bits(.6f);
    exact_witness=FALSE; assert(!SudekiMpLanStoryMusicFollow(&h,&w)); exact_witness=TRUE;
    ++w.native_thread_id; assert(!SudekiMpLanStoryMusicFollow(&h,&w)); --w.native_thread_id;
    current_area=FALSE; assert(!SudekiMpLanStoryMusicFollow(&h,&w)); current_area=TRUE;
    admission=FALSE; assert(!SudekiMpLanStoryMusicFollow(&h,&w)); admission=TRUE;
    h.music_intensity=0x7fc00000; assert(!SudekiMpLanStoryMusicFollow(&h,&w)); h.music_intensity=bits(.6f);
    ptr(score+0x30,image); assert(!SudekiMpLanStoryMusicFollow(&h,&w)); ptr(score+0x30,image+0x2c7c24);
    emitter[0x10]=0; assert(!SudekiMpLanStoryMusicFollow(&h,&w)); emitter[0x10]=1;
    strcpy(h.music_score,"BS_Unknown"); assert(!SudekiMpLanStoryMusicFollow(&h,&w)); strcpy(h.music_score,"BS_IllumCountry");
    DWORD old; assert(VirtualProtect(score,4096,PAGE_READONLY,&old));
    assert(!SudekiMpLanStoryMusicFollow(&h,&w)); assert(VirtualProtect(score,4096,old,&old));
    assert(score[0xac]==0 && word(score+0x78)==bits(.4f));
    /* A live tracker must belong to this bank and cue; retired generations
     * are inert in the native reset. No actual cue method executes. */
    num(score+0x48,256); entries[0]=1; num(entries+8,1); ptr(entries+0xc,cue);
    ptr(cue+8,soundbank); ptr(entries+0x10,emitter);
    assert(!SudekiMpLanStoryMusicFollow(&h,&w)); ptr(entries+0x10,bank);
    assert(SudekiMpLanStoryMusicFollow(&h,&w) && score[0xac]==1 && !loads);
    ++h.sequence; ptr(cue+8,bank); assert(!SudekiMpLanStoryMusicFollow(&h,&w));
    num(entries+8,2); assert(SudekiMpLanStoryMusicFollow(&h,&w));
    /* H1b interior door fade on a client's local house entry (native Fade,
     * IsPlaying and VariationStop execute on the fixture). */
    num(score+0x6c,bits(1.f)); num(score+0x70,0); num(score+0x74,bits(1.f)); score[0xc0]=1;
    unsigned before=calls;
    assert(SudekiMpLanStoryMusicInterior(TRUE,&w) && calls==before+1);
    assert(score[0xc0]==0 && word(score+0x74)==bits(0.f) && word(score+0x70)==bits(-.5f) && word(score+0x6c)==bits(1.f));
    assert(SudekiMpLanStoryMusicInterior(TRUE,&w) && calls==before+1); /* converged */
    assert(SudekiMpLanStoryMusicInterior(FALSE,&w) && word(score+0x74)==bits(1.f) && calls==before+2);
    num(score+0x6c,0); num(score+0x70,0); num(score+0x74,0); /* silent at entry: no fade either way */
    assert(SudekiMpLanStoryMusicInterior(TRUE,&w) && !word(score+0x74) && !word(score+0x70));
    assert(SudekiMpLanStoryMusicInterior(FALSE,&w) && !word(score+0x74) && !word(score+0x70));
    num(score+0x6c,bits(1.f)); num(score+0x74,bits(1.f));
    admission=FALSE; assert(!SudekiMpLanStoryMusicInterior(TRUE,&w)); admission=TRUE;
    exact_witness=FALSE; assert(!SudekiMpLanStoryMusicInterior(TRUE,&w)); exact_witness=TRUE;
    ++w.native_thread_id; assert(!SudekiMpLanStoryMusicInterior(TRUE,&w)); --w.native_thread_id;
    ptr(score,image); assert(!SudekiMpLanStoryMusicInterior(TRUE,&w)); ptr(score,image+0x2c7bf4);
    image[0x39990]^=1; assert(!SudekiMpLanStoryMusicInterior(TRUE,&w)); image[0x39990]^=1;
    assert(word(score+0x74)==bits(1.f)); /* refusals wrote nothing */
    assert(SudekiMpLanStoryMusicInterior(TRUE,&w) && !word(score+0x74));
    assert(SudekiMpLanStoryMusicInteriorReset() && word(score+0x74)==bits(1.f));
    assert(SudekiMpLanStoryMusicInteriorReset()); /* nothing left to restore */
    assert(SudekiMpLanStoryMusicInterior(TRUE,&w) && !word(score+0x74));
    ptr(score,image); assert(!SudekiMpLanStoryMusicInteriorReset()); ptr(score,image+0x2c7bf4);
    assert(!word(score+0x74) && SudekiMpLanStoryMusicInteriorReset() && word(score+0x74)==bits(1.f));
    puts("Story music interior door fade, convergence, refusal and teardown restore: PASS");
    VirtualFree(score,0,MEM_RELEASE); VirtualFree(image,0,MEM_RELEASE);
    puts("Story music exact-image guards, native encounter transitions and owner refusal: PASS");
    return 0;
}
