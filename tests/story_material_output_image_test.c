/* Exact retail material-output contract plus read-only animation preflight. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine/build_identity.h"
#include "../src/engine/build_identity.c"
#include "../src/engine/sha256.c"
#include "../src/hooks/lan_story_world.c"
#include <stdarg.h>
void SudekiMpLogFormat(const char *format,...) { (void)format; }
BOOL SudekiMpLogResearchEnabled(void) { return FALSE; }
/* Retail expansion/cleanup is covered in StoryMaterialImageTest. Here prove
 * world preflight delegates with the actual selected/original pair and fails
 * closed when that proof refuses; never execute a replacement on this double. */
static const void *expected_material,*expected_original;
static unsigned expansion_calls;
BOOL SudekiMpLanStoryMaterialShaderOutputExact(HMODULE image,const void *material,
    const void *original,unsigned member) {
    ++expansion_calls;
    return image==(HMODULE)base && member==3u && expected_material &&
        material==expected_material && original==expected_original;
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

int main(int argc,char **argv) {
    wchar_t path[32768];
    assert(argc==2 && MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,32768));
    base=map_image(path);
    uint8_t owner[0x28]={0},resource[0x30]={0},manager[8]={0},mat[0x40]={0},description[0x30]={0};
    void *materials[1]={mat};
    *(void **)(owner+0xc)=base+0x2dedd8;
    *(void **)(resource+0x2c)=manager; *(uint32_t *)manager=1;
    *(void **)(manager+4)=materials; *(void **)mat=base+0x2deb7c;
    *(void **)(mat+4)=description;
    Target t={.identifier=123}; VisualOutput output={0};
    typedef void *(__attribute__((thiscall)) *Property)(void *,unsigned,unsigned,uint32_t *);
    Property native=(Property)(base+0x202e40);
    static const struct {unsigned arg,member,offset,floats; SudekiMpLanStoryCurveKind kind;} cases[]={
        {0,0,0x18,4,SUDEKIMP_STORY_CURVE_COLOR},
        {0,1,0x28,4,SUDEKIMP_STORY_CURVE_COLOR},
        {3,0,0x24,1,SUDEKIMP_STORY_CURVE_SCALAR},
        {3,1,0x34,1,SUDEKIMP_STORY_CURVE_SCALAR}
    };
    for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
        uint32_t flags=0; uint8_t before[sizeof(mat)];
        memcpy(before,mat,sizeof(mat));
        assert(mcon_output(&t,owner,resource,0,cases[i].arg,cases[i].member,&output));
        assert(output.kind==cases[i].kind && output.floats==cases[i].floats);
        assert(!memcmp(before,mat,sizeof(mat))); /* Validation is read-only. */
        assert(native(mat,cases[i].arg,cases[i].member,&flags)==mat+cases[i].offset);
        if(cases[i].arg==3 && !cases[i].member) assert(flags==2 && (*(uint32_t *)(mat+0xc)&1));
        else assert(flags==0);
    }
    assert(!mcon_output(&t,owner,resource,1,0,0,&output)); /* Invalid material slot. */
    assert(!mcon_output(&t,owner,resource,0,3,2,&output)); /* Unknown scalar member. */
    assert(!mcon_output(&t,owner,resource,0,1,0,&output)); /* No arbitrary texture binding. */
    uint32_t mapping[4]={UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};
    *(void **)(description+0x2c)=mapping;
    assert(!mcon_output(&t,owner,resource,0,0,2,&output)); /* Definition replacement remains closed. */
    static const unsigned shifts[4]={0,2,6,8};
    for(unsigned i=0;i<4;++i) for(unsigned flag=1;flag<=3;++flag) {
        *(uint32_t *)(description+8)=flag<<shifts[i];
        uint8_t before[sizeof(mat)],desc_before[sizeof(description)]; uint32_t flags=0;
        memcpy(before,mat,sizeof(mat)); memcpy(desc_before,description,sizeof(description));
        assert(mcon_output(&t,owner,resource,0,0,i+2,&output));
        assert(output.kind==SUDEKIMP_STORY_CURVE_COLOR && output.floats==4);
        assert(!native(mat,0,i+2,&flags) && !flags);
        assert(!memcmp(before,mat,sizeof(mat)) && !memcmp(desc_before,description,sizeof(description)));
    }
    /* Clone replacement may select the original: both graphs must prove
     * the NULL branch or an existing writable constant. */
    uint8_t original[0x40]={0},original_desc[0x30]={0}; void *overrides[1]={mat};
    *(void **)(owner+0x20)=overrides; materials[0]=original;
    *(void **)(original+4)=original_desc; *(void **)(original_desc+0x2c)=mapping;
    *(uint32_t *)(description+8)=4;
    assert(!mcon_output(&t,owner,resource,0,0,3,&output));
    *(uint32_t *)(original_desc+8)=4;
    assert(mcon_output(&t,owner,resource,0,0,3,&output));
    *(uint32_t *)(description+8)=0x80000001u;
    *(uint32_t *)(original_desc+8)=0x80000001u;
    assert(!mcon_output(&t,owner,resource,0,0,3,&output));
    expected_material=mat; expected_original=original; expansion_calls=0;
    assert(mcon_output(&t,owner,resource,0,0,3,&output) && expansion_calls==2);
    assert(output.kind==SUDEKIMP_STORY_CURVE_COLOR && output.floats==4);
    expected_original=mat; assert(!mcon_output(&t,owner,resource,0,0,3,&output));
    expected_material=expected_original=NULL;
    *(uint32_t *)(description+8)=*(uint32_t *)(original_desc+8)=4;
    mapping[1]=0; assert(!mcon_output(&t,owner,resource,0,0,3,&output));
    *(void **)mat=base;
    assert(!mcon_output(&t,owner,resource,0,0,0,&output)); /* Wrong native material. */
    *(void **)mat=base+0x2deb7c; *(void **)(owner+0xc)=base;
    assert(!mcon_output(&t,owner,resource,0,0,0,&output));
    VirtualFree(base,0,MEM_RELEASE);
    puts("Story material animation outputs: PASS (retail getters on inert material; no rendering)");
    return 0;
}
