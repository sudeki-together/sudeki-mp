/* Executes the supported image with synthetic actors and inert camera sinks.
 * No game process, save, constructor, rendering or network is involved. */
#include "hooks/story_player_movement.h"
#include "engine/build_identity.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
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


static uint8_t *image;
static BOOL readable(const void *p,size_t n) { return p && n; }
static void jump(unsigned rva,void *fn) {
    image[rva]=0xe9; int32_t d=(uint8_t *)fn-image-rva-5;
    memcpy(image+rva+1,&d,4); assert(FlushInstructionCache(GetCurrentProcess(),image+rva,5));
}
static float *__stdcall normalize(float *out,const float *in) {
    float n=sqrtf(in[0]*in[0]+in[1]*in[1]+in[2]*in[2]);
    for(unsigned i=0;i<3;++i) out[i]=in[i]/n;
    return out;
}
static void __stdcall camera(void *c,float *out,const float *in) { (void)c; memcpy(out,in,12); }
static unsigned moves,stops;
static float seen_speed,seen_heading[3];
static void __stdcall movement(void *a,const float *d,float s,float t,unsigned m) {
    assert(a && t==1.0f && !m); ++moves; seen_speed=s; memcpy(seen_heading,d,12);
}
static void __attribute__((thiscall)) stop(void *a,float s,float t) {
    assert(a && s==0 && t==1); ++stops;
}
static BOOL no_turn(void) { return FALSE; }
static void contracts(void) {
    assert(SudekiMpStoryMovementImageExact(image,readable));
    for(unsigned k=0;k<sizeof(story_movement_contracts)/sizeof(*story_movement_contracts);++k) {
        const __typeof__(*story_movement_contracts) *c=&story_movement_contracts[k];
        for(unsigned i=0;i<c->size;++i) {
            image[c->rva+i]^=1; assert(!SudekiMpStoryMovementImageExact(image,readable));
            image[c->rva+i]^=1;
        }
        printf("%s RVA %08x: exact image and every-byte mutation refusal passed\n",c->name,c->rva);
    }
    const unsigned constants[]={0x2e35cc,0x2e3628,0x2c02d8};
    for(unsigned i=0;i<3;++i) {
        image[constants[i]]^=1; assert(!SudekiMpStoryMovementImageExact(image,readable));
        image[constants[i]]^=1;
    }
    puts("C4_SHAPE_CONSTANTS: mutation refusal passed");
}
static void controller_parity(void) {
    uint8_t c[0x260]={0},a[0x120]={0},arb[0x64]={0},m[0xc0]={0},tg[0x94]={0};
    *(void **)(c+0x248)=a; *(void **)(a+0x90)=arb; *(void **)(a+0x80)=m;
    *(void **)(a+0xac)=tg; *(void **)(arb+0x10)=a;
    uint8_t saved_move[5],saved_stop[5];
    memcpy(saved_move,image+0xdae80,5); memcpy(saved_stop,image+0xdb070,5);
    jump(0x291a0,camera); jump(0xdae80,movement); jump(0xdb070,stop);
    *(void **)(image+0x29a2b0)=normalize;
    const float inputs[][2]={{0,0},{1,0},{0,1},{1,1},{1.8f,0},{0,1.8f},{1.8f,1.8f},
        {0.3f,0.7f},{-1.5f,-1.5f},{4,4},{-4,-4},{0.001f,0},{0,0.001f},{0,0.0011f},
        {0.0009f,0.0009f},{-0.001f,0},{0,-0.001f},{-0.0011f,0},
        {0,-0.0011f},{0.001f,0.001f},{0.0011f,0},{4.004f,4.004f}};
    for(unsigned i=0;i<sizeof(inputs)/sizeof(*inputs);++i) {
        float local[3],heading[3],speed;
        assert(SudekiMpStoryMovementLocal(inputs[i][0],inputs[i][1],local));
        assert(SudekiMpStoryMovementVector(local[0],local[2],heading,&speed));
        moves=stops=0;
        *(float *)(c+0x1a0)=inputs[i][0]; *(float *)(c+0x1a4)=inputs[i][1];
        ((void (__stdcall *)(void *,float))(image+0x28b00))(c,0.016f);
        if((speed==0 && (stops!=1 || moves)) ||
            (speed!=0 && (moves!=1 || stops || fabsf(seen_speed-speed)>=0.00001f)))
            fprintf(stderr,"controller case %u input=(%.9g,%.9g) speed=%.9g native=%.9g moves=%u stops=%u\n",
                i,inputs[i][0],inputs[i][1],speed,seen_speed,moves,stops);
        if(speed==0) assert(stops==1 && !moves);
        else {
            assert(moves==1 && !stops && fabsf(seen_speed-speed)<0.00001f);
            for(unsigned k=0;k<3;++k) assert(fabsf(seen_heading[k]-heading[k])<0.00001f);
        }
    }
    memcpy(image+0xdae80,saved_move,5); memcpy(image+0xdb070,saved_stop,5);
    assert(FlushInstructionCache(GetCurrentProcess(),image,0x200000));
    puts("C4_CONTROLLER_PARITY: native cardinal/diagonal/scaled/analog/deadzone requests match");
}
static void arbitration(void) {
    uint8_t actor[0x120]={0},arb[0x64]={0},m[0xc0]={0},tg[0x94]={0},combat[0x80]={0},combo[0xc0]={0};
    *(void **)(actor+0x90)=arb; *(void **)(actor+0x80)=m; *(void **)(actor+0xac)=tg;
    *(void **)(actor+0xa4)=combat; *(void **)(actor+0xb8)=combo; *(void **)(arb+0x10)=actor;
    jump(0xdbb20,no_turn); m[0xbe]=8; *(uint32_t *)(arb+0x50)=0x1002;
    float heading[3]={0,0,1}; *(float *)(m+0x28)=0.75f;
    SudekiMpStoryMovementSubmit(image,arb,heading,1.8f);
    assert(fabsf(*(float *)(m+0x24)-1.8f)<0.00001f && *(float *)(m+0x28)==0.75f);
    combat[0x72]=0x10; /* Native combat lock; its combo may re-admit locomotion. */
    SudekiMpStoryMovementSubmit(image,arb,heading,1.8f);
    assert(*(float *)(m+0x24)==0 && *(float *)(m+0x28)==0.75f);
    combo[0xb1]=4; SudekiMpStoryMovementSubmit(image,arb,heading,1.8f);
    assert(fabsf(*(float *)(m+0x24)-1.8f)<0.00001f && *(float *)(m+0x28)==0.75f);
    *(uint32_t *)(arb+0x50)|=0x80000; SudekiMpStoryMovementSubmit(image,arb,heading,1.8f);
    assert(*(float *)(m+0x24)==0); *(uint32_t *)(arb+0x50)=0x1002;
    m[0xbe]=0; *(float *)(m+0x24)=0.4f;
    SudekiMpStoryMovementSubmit(image,arb,heading,1.8f);
    assert(fabsf(*(float *)(m+0x24)-0.4f)<0.00001f && *(float *)(tg+0x50)==1.0f);
    SudekiMpStoryMovementSubmit(image,arb,heading,0);
    assert(fabsf(*(float *)(m+0x24)-0.4f)<0.00001f && *(float *)(m+0x28)==0.75f);
    m[0xbe]=8; SudekiMpStoryMovementSubmit(image,arb,heading,0);
    assert(*(float *)(m+0x24)==0 && *(float *)(m+0x28)==0.75f && *(float *)(m+0x30)==1);
    puts("C4_ARBITRATION: native combo permission, cast lock, write ownership and blended release pass");
}
static void transport(void) {
    SudekiMpLanStoryMovement m={.fence={1,2,3,4,1,2},.sequence=1,.acknowledged_frame=1,.world_x=1,.world_z=1.8f},out;
    uint8_t wire[SUDEKIMP_STORY_MOVEMENT_WIRE_SIZE];
    assert(SudekiMpLanStoryMovementEncode(&m,wire,sizeof(wire)));
    assert(SudekiMpLanStoryMovementDecode(wire,sizeof(wire),&out));
    assert(out.world_x==m.world_x && out.world_z==m.world_z);
    m.world_x=NAN; assert(!SudekiMpLanStoryMovementValid(&m));
    m.world_x=INFINITY; assert(!SudekiMpLanStoryMovementValid(&m));
    m.world_x=4;m.world_z=4; assert(!SudekiMpLanStoryMovementValid(&m));
    m.world_x=0;m.world_z=4.14f; assert(!SudekiMpLanStoryMovementValid(&m));
    m.world_x=0;m.world_z=1.8f; assert(SudekiMpLanStoryMovementValid(&m));
    assert(!SudekiMpLanStoryMovementDecode(wire,sizeof(wire)-1,&out));
    puts("C4_TRANSPORT: scaled magnitude round-trip, truncation/nonfinite/overspeed refusal pass");
}
int main(int argc,char **argv) {
    wchar_t path[32768]; assert(argc==2);
    assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,32768)); image=map_image(path);
    contracts(); transport(); controller_parity(); arbitration();
    puts("story_player_movement_image_test: PASS"); return 0;
}
