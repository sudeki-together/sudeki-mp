#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "hooks/lan_story_xp.c"
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

static unsigned shown;
static char local_area[40]="FixtureExterior";
BOOL SudekiMpLanStoryAreaCapture(SudekiMpStoryAreaState *s) {memset(s,0,sizeof(*s));memcpy(s->current,local_area,40);return TRUE;}
void SudekiMpLogFormat(const char *format,...) {(void)format;}
static void __stdcall setup(void *object,uint32_t amount,float x,float y,float z,float height) {
    uint8_t *p=object;++shown;p[8]=1;*(uint32_t *)(p+0x64)=amount;
    *(float *)(p+0x80)=height;*(float *)(p+0x88)=0;
    float v[3]={x,y+*(float *)(p+0x84),z};memcpy(p+0x8c,v,12);
}
static void jump(uint8_t *p,void *target) {p[0]=0xe9;*(int32_t *)(p+1)=(uint8_t *)target-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);}
int main(int argc,char **argv) {
    assert(argc==2);wchar_t path[2048];
    assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,2048));
    uint8_t *b=map_image(path);assert(xp_code(b));
    for(unsigned i=0;i<sizeof(xp_contracts)/sizeof(*xp_contracts);++i) {
        printf("guard %s: all %lu bytes\n",xp_contracts[i].name,(unsigned long)xp_contracts[i].size);
        for(unsigned k=0;k<xp_contracts[i].size;++k) {
            uint8_t *p=b+xp_contracts[i].rva+k;*p^=1;assert(!xp_code(b));*p^=1;
        }
    }
    puts("guard XpSetupReturn: all 3 bytes");
    for(unsigned k=0;k<3;++k) {b[0x182f95+k]^=1;assert(!xp_code(b));b[0x182f95+k]^=1;}
    uint8_t *m=calloc(1,0x84),*objects[24];assert(m);*(void **)m=b+0x2ca348;m[0x80]=m[0x81]=1;
    *(void **)(b+0x409dbc)=m;
    for(unsigned i=0;i<24;++i) {
        objects[i]=calloc(1,0x98);assert(objects[i]);*(void **)objects[i]=b+0x2d9058;
        *(void **)(objects[i]+0x78)=objects[i];*(float *)(objects[i]+0x84)=.4f;*(float *)(objects[i]+0x7c)=1.5f;
        *(void **)(m+0x1c+4*i)=objects[i];
    }
    assert(SudekiMpLanStoryXpInitialize((HMODULE)b,TRUE));
    SudekiMpStoryXp state;
    assert(SudekiMpLanStoryXpCapture(1,1,100,local_area,&state));
    assert(!state.slots[0].serial);
    setup(objects[0],64,1,2,3,1.6f);*(float *)(objects[0]+0x88)=.25f;
    assert(SudekiMpLanStoryXpCapture(1,1,200,local_area,&state));
    uint32_t serial=state.slots[0].serial;assert(serial && state.slots[0].amount==64);
    assert(fabsf(state.slots[0].position[1]-2)<.0001f);
    *(float *)(objects[0]+0x88)=.5f;
    assert(SudekiMpLanStoryXpCapture(1,1,300,local_area,&state) && state.slots[0].serial==serial);
    *(float *)(objects[0]+0x88)=.1f; /* Same slot, same amount/place, new native lifetime. */
    assert(SudekiMpLanStoryXpCapture(1,1,400,local_area,&state) && state.slots[0].serial>serial);
    SudekiMpStoryXp receipt=state,unchanged;memset(&unchanged,0x44,sizeof(unchanged));
    *(void **)objects[2]=NULL;state=unchanged;
    assert(!SudekiMpLanStoryXpCapture(1,1,500,local_area,&state) && !memcmp(&state,&unchanged,sizeof(state)));
    *(void **)objects[2]=b+0x2d9058;
    char other[40]="OtherExterior";
    assert(SudekiMpLanStoryXpCapture(2,2,600,other,&state) && !state.slots[0].serial); /* No old-area XP. */
    assert(SudekiMpLanStoryXpCapture(2,2,700,other,&state) && !state.slots[0].serial);
    objects[0][8]=0;assert(SudekiMpLanStoryXpCapture(2,2,800,other,&state));
    setup(objects[0],128,4,5,6,1.6f);
    assert(SudekiMpLanStoryXpCapture(2,2,900,other,&state) && state.slots[0].serial);
    SudekiMpLanStoryXpReset();for(unsigned i=0;i<24;++i)objects[i][8]=0;
    shown=0;assert(SudekiMpLanStoryXpInitialize((HMODULE)b,FALSE));
    /* Execute retail popup/free-slot selector, record allocating renderer leaf. */
    jump(b+0x182e20,(void *)setup);
    assert(SudekiMpLanStoryXpPresent(&receipt,1000,100) && shown==1);
    unsigned slot=*(uint32_t *)(m+0x7c);assert(slot==1 && objects[slot][8]);
    assert(*(uint32_t *)(objects[slot]+0x64)==64 && fabsf(*(float *)(objects[slot]+0x88)-.2f)<.001f);
    assert(fabsf(*(float *)(objects[slot]+0x90)-2.4f)<.001f);
    assert(SudekiMpLanStoryXpPresent(&receipt,1001,101) && shown==1); /* Retransmit. */
    receipt.slots[0].serial++;receipt.slots[0].elapsed=1.4f;
    assert(SudekiMpLanStoryXpPresent(&receipt,1200,200) && shown==1); /* Expired. */
    receipt.slots[0].serial++;receipt.slots[0].elapsed=0;m[0x80]=0;
    assert(SudekiMpLanStoryXpPresent(&receipt,1201,0) && shown==1); /* Native option. */
    m[0x80]=m[0x81]=1;assert(SudekiMpLanStoryXpPresent(&receipt,1202,0) && shown==1);
    receipt.slots[0].serial++;memcpy(receipt.current,other,40);
    assert(!SudekiMpLanStoryXpPresent(&receipt,1203,0) && shown==1);
    memcpy(receipt.current,local_area,40);
    for(unsigned i=0;i<24;++i)objects[i][8]=1;
    assert(SudekiMpLanStoryXpPresent(&receipt,1204,0) && shown==1); /* Full pool, retry later. */
    objects[5][8]=0;assert(SudekiMpLanStoryXpPresent(&receipt,1205,0) && shown==2);
    receipt.slots[0].serial--;assert(SudekiMpLanStoryXpPresent(&receipt,1206,0) && shown==2);
    SudekiMpLanStoryXpReset();assert(!SudekiMpLanStoryXpPresent(&receipt,1300,0));
    for(unsigned i=0;i<24;++i)free(objects[i]);
    free(m);VirtualFree(b,0,MEM_RELEASE);
    puts("PASS native XP capture, reuse, area fence, ABI, once-only playback, native option, expiration and pool retry; no gameplay death/reward call");
    return 0;
}
