#include "hooks/story_ai_movement.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t *image;
static uint8_t actor[0x210],ai[0x174],mode[0xc],node[0xb0],position[0x110];
static uint8_t arbiter[0x64],movement[0xc0],turn[0x50],controller[0x260],group[0x100];
static uint32_t reference[3],previous[3],following[3];
static BOOL grant=TRUE;
static DWORD thread;
static unsigned admitted[2];
unsigned housekeeping[2],planner_calls,move_calls,speed_calls,snap_calls[3],alpha_calls,fade_done;
unsigned observed_move,observed_speed;
void *observer_next[2];
uint8_t plan_bits;
extern void *SudekiMpStoryAiMovementNext[4];
static unsigned install_calls,fail_install;
BOOL __real_SudekiMpInstallInlineHook(SudekiMpInlineHook *,uint8_t *,const uint8_t *,size_t,const void *);
BOOL __wrap_SudekiMpInstallInlineHook(SudekiMpInlineHook *h,uint8_t *p,const uint8_t *b,size_t n,const void *fn) {
    if(++install_calls==fail_install) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return FALSE; }
    return __real_SudekiMpInstallInlineHook(h,p,b,n,fn);
}
static BOOL __attribute__((target("sse2"))) lease(void *owner,unsigned gate) {
    __asm__ volatile("fninit; pxor %%xmm0,%%xmm0" ::: "xmm0");
    SetLastError(999);
    BOOL ok=grant && owner==actor && gate<2 && GetCurrentThreadId()==thread;
    if(ok) ++admitted[gate];
    return ok;
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


static void jump(unsigned rva,void *replacement) {
    image[rva]=0xe9; int32_t d=(int32_t)((uint8_t *)replacement-image-rva-5);
    memcpy(image+rva+1,&d,4); assert(FlushInstructionCache(GetCurrentProcess(),image+rva,5));
}
#define STUB(name,body) static void __attribute__((naked,noinline)) name(void) { __asm__ volatile(body); }
STUB(update_ai,"incl _housekeeping; ret")
STUB(update_ai2,"incl _housekeeping+4; ret")
STUB(ptr_copy,"mov (%ecx),%edx; mov %edx,(%eax); ret")
STUB(ptr_end,"ret")
STUB(native_sqrt,"fsqrt; ret")
STUB(planner,"incl _planner_calls; mov $_plan_bits,%eax; ret $12")
STUB(move,"incl _move_calls; ret $20")
STUB(speed,"incl _speed_calls; ret $8")
STUB(snap_position,"incl _snap_calls; ret")
STUB(snap_forward,"incl _snap_calls+4; ret")
STUB(snap_turn,"incl _snap_calls+8; ret $4")
STUB(alpha,"incl _alpha_calls; ret")
STUB(fade_end,"incl _fade_done; movl $0,0xa0(%ecx); movl $0,0xa4(%ecx); ret")
STUB(navigation,"ret $12")
STUB(observe_move,"pushfl; incl _observed_move; popfl; jmp *_observer_next")
STUB(observe_speed,"pushfl; incl _observed_speed; popfl; jmp *(_observer_next+4)")
static float *__stdcall normalize(float *out,const float *in) {
    float n=sqrtf(in[0]*in[0]+in[1]*in[1]+in[2]*in[2]);
    for(unsigned i=0;i<3;++i) out[i]=n>0?in[i]/n:0;
    return out;
}
static void __attribute__((naked,noinline)) tick(void *entry __attribute__((unused)),
    void *component __attribute__((unused)),const float *timing __attribute__((unused))) {
    __asm__ volatile("mov 8(%esp),%ecx; pushl 12(%esp); call *8(%esp); ret");
}
static void run_tick(void) {
    float timing[4]={0,0,0,0.05f}; tick(image+0xf4bb0,node,timing);
}
static void run_snap(void) { ((void (__stdcall *)(void *))(image+0xf50c0))(node); }
static void reset(void) {
    grant=TRUE; memset(admitted,0,sizeof(admitted)); memset(housekeeping,0,sizeof(housekeeping));
    memset(snap_calls,0,sizeof(snap_calls));
    move_calls=speed_calls=planner_calls=observed_move=observed_speed=alpha_calls=fade_done=0;
    *(int16_t *)(ai+0x16a)=1; mode[0xb]=0;
    memset(node+0x18,0,sizeof(node)-0x18);
    *(float *)(node+0x7c)=2; *(float *)(node+0x80)=1; *(float *)(node+0x94)=1;
    node[0xa8]=1; node[0xa9]=1;
    memset(position+0x18,0,12); *(float *)(position+0x50)=0; *(float *)(position+0x58)=1;
    *(float *)(position+0x90)=3; *(float *)(movement+0x24)=3; *(float *)(movement+0x28)=3;
    for(unsigned i=0x34;i<=0x48;i+=4) *(float *)(movement+i)=4;
    movement[0xbe]=0;
}
static void setup(void) {
    *(void **)(actor+0x44)=position; *(void **)(actor+0x80)=movement;
    *(void **)(actor+0x8c)=turn; *(void **)(actor+0x90)=arbiter;
    *(void **)(actor+0x94)=ai; *(void **)(actor+0x98)=node;
    *(void **)ai=image+0x2d4924; *(void **)(ai+0x10)=actor; *(void **)(ai+0x3c)=mode;
    *(void **)node=image+0x2d4994; *(void **)(node+0x10)=actor;
    *(void **)position=image+0x2cdefc; *(void **)(position+0x10)=actor;
    *(void **)(movement+0x10)=actor; *(void **)(arbiter+0x10)=actor;
    *(void **)(image+0x408da4)=controller; *(void **)(image+0x408d94)=group;
    *(void **)(controller+0x248)=group; /* Another, local actor. */
    jump(0xeaa40,update_ai); jump(0xed6d0,update_ai2);
    jump(0x15b0,ptr_copy); jump(0x15e0,ptr_end); jump(0x28fd60,native_sqrt);
    jump(0xf4250,planner); jump(0xdae80,move); jump(0xdb070,speed);
    jump(0x3050,snap_position); jump(0x1114d0,snap_forward); jump(0xb6e50,snap_turn);
    jump(0xf5030,alpha); jump(0xf3200,fade_end); jump(0xf1730,navigation);
    *(void **)(image+0x29a2b0)=normalize;
}
static void contracts(void) {
    const struct { const char *name; unsigned rva,size; } r[]={
        {"AI_MOVE_FRAME",0xf4bb0,0x2e},{"AI_MOVE_CHOICE",0xf4cb3,0x1a},
        {"AI_MOVE_PLAYER_CACHE",0xf4ccd,0x3e},{"AI_SNAP_FRAME",0xf50c0,0x14},
        {"AI_SNAP_CHOICE",0xf50ff,0x39},{"AI_SNAP_CLEANUP",0xf51ca,0xa7}};
    assert(SudekiMpStoryAiMovementImageExact(image));
    for(unsigned k=0;k<6;++k) {
        for(unsigned i=0;i<r[k].size;++i) {
            image[r[k].rva+i]^=1; assert(!SudekiMpStoryAiMovementImageExact(image));
            assert(!SudekiMpStoryAiMovementInstall((HMODULE)image,lease));
            image[r[k].rva+i]^=1;
        }
        printf("%s: exact bytes and every-byte install rejection passed\n",r[k].name);
    }
}
static void assert_native(void) {
    run_tick(); assert(planner_calls==1 && speed_calls==1 && observed_speed==1 && !admitted[0]);
    run_snap(); assert(snap_calls[0]==1 && snap_calls[1]==1 && snap_calls[2]==1 && !admitted[1]);
}
static DWORD WINAPI other_thread(void *unused) {
    (void)unused; assert_native(); assert(!SudekiMpStoryAiMovementUninstall()); return 0;
}
/* Probe both installed decisions before native continuations to check every
 * register/flag and x87/SSE/LastError on admitted and native fallbacks. */
uint32_t probe_regs[8],probe_flags,probe_sse[4]; float probe_fp;
unsigned probe_kind; void *probe_entry;
STUB(probe_return,"pushfl; popl _probe_flags; pushal; mov %esp,%esi; mov $_probe_regs,%edi;"
    "mov $8,%ecx; cld; rep movsl; popal; movups %xmm0,_probe_sse; fstps _probe_fp; ret")
STUB(probe,"pushal; sub $16,%esp; movl $0,8(%esp); mov $_position,%edi; mov $_node,%esi;"
    "mov $_node,%ebp; mov $_ai,%ebx; mov $0x1111,%eax; mov $0x2222,%ecx; mov $0x3333,%edx;"
    "pcmpeqd %xmm0,%xmm0; fld1; cmp %eax,%eax; call *_probe_entry; add $16,%esp; popal; ret")
static void abi(void) {
    void *saved[4]; memcpy(saved,SudekiMpStoryAiMovementNext,sizeof(saved));
    for(unsigned i=0;i<4;++i) SudekiMpStoryAiMovementNext[i]=probe_return;
    for(unsigned kind=0;kind<2;++kind) for(unsigned allowed=0;allowed<2;++allowed) {
        reset(); grant=allowed; probe_entry=image+(kind?STORY_AI_SNAP_GATE:STORY_AI_MOVE_GATE);
        SetLastError(123); probe(); assert(GetLastError()==123);
        assert(probe_regs[0]==(uintptr_t)position && probe_regs[1]==(uintptr_t)node);
        assert(probe_regs[2]==(uintptr_t)node && probe_regs[4]==(uintptr_t)ai);
        assert(probe_regs[5]==0x3333 && probe_regs[6]==0x2222 && probe_regs[7]==0x1111);
        assert(probe_fp==1); for(unsigned i=0;i<4;++i) assert(probe_sse[i]==UINT32_MAX);
        /* Incoming ZF survives skip. Original snap CMP sees A8=1. */
        assert(!!(probe_flags&0x40)==(allowed || !kind));
    }
    memcpy(SudekiMpStoryAiMovementNext,saved,sizeof(saved));
}
int main(int argc,char **argv) {
    wchar_t path[32768]; assert(argc==2);
    assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,32768)); image=map_image(path);
    contracts(); setup(); thread=GetCurrentThreadId();
    /* Same two observer seams already owned by Research54. */
    SudekiMpRelativeCallHook observers[2]={0};
    observer_next[0]=image+0xdae80; observer_next[1]=image+0xdb070;
    assert(SudekiMpInstallRelativeCallHook(&observers[0],image+0xf4d7f,image+0xdae80,observe_move));
    assert(SudekiMpInstallRelativeCallHook(&observers[1],image+0xf4de3,image+0xdb070,observe_speed));
    reset(); assert_native(); /* Reproduce original AI stop and snaps with count 1. */
    for(unsigned fail=1;fail<=2;++fail) {
        install_calls=0; fail_install=fail;
        assert(!SudekiMpStoryAiMovementInstall((HMODULE)image,lease));
        assert(SudekiMpStoryAiMovementImageExact(image));
        for(unsigned i=0;i<4;++i) assert(!SudekiMpStoryAiMovementNext[i]);
    }
    fail_install=0; assert(SudekiMpStoryAiMovementInstall((HMODULE)image,lease));
    reset(); assert_native(); /* Not bound to a native thread yet. */
    assert(SudekiMpStoryAiMovementBindThread());
    reset(); run_tick();
    assert(admitted[0]==1 && housekeeping[0]==1 && housekeeping[1]==1 && !planner_calls && !speed_calls);
    assert(*(float *)(node+0x74)==0 && *(float *)(node+0x78)==1 && *(float *)(node+0x7c)==3);
    assert(!observed_speed && *(int16_t *)(ai+0x16a)==1 && !mode[0xb]);
    reset(); *(unsigned *)(node+0x4c)=2; run_tick(); assert(!planner_calls && !move_calls && !observed_move);
    grant=FALSE; run_tick(); assert(planner_calls==1 && move_calls==1 && observed_move==1);
    reset(); grant=FALSE; assert_native();
    reset(); *(int16_t *)(ai+0x16a)=2; assert_native();
    reset(); *(int16_t *)(ai+0x16a)=0; assert_native();
    reset(); mode[0xb]=1; assert_native();
    reset(); *(void **)(ai+0x10)=group; assert_native(); *(void **)(ai+0x10)=actor;
    reset(); *(void **)(actor+0x98)=NULL; assert_native(); *(void **)(actor+0x98)=node;
    reset(); *(void **)node=image+0x2d4924; assert_native(); *(void **)node=image+0x2d4994;
    reset(); HANDLE t=CreateThread(NULL,0,other_thread,NULL,0,NULL); assert(t);
    assert(WaitForSingleObject(t,10000)==WAIT_OBJECT_0); CloseHandle(t);
    /* AI snaps skip actual position, facing and speed mutations but execute
     * native state and intrusive-reference retirement. */
    reset(); *(unsigned *)(node+0x4c)=2;
    *(void **)(node+0x50)=reference; *(void **)(node+0x54)=previous; *(void **)(node+0x58)=following;
    reference[1]=(uintptr_t)(node+0x50); previous[2]=(uintptr_t)(node+0x50); following[1]=(uintptr_t)(node+0x50);
    run_snap(); assert(admitted[1]==1 && !snap_calls[0] && !snap_calls[1] && !snap_calls[2]);
    assert(!*(unsigned *)(node+0x4c) && !*(void **)(node+0x50) && !*(void **)(node+0x54) && !*(void **)(node+0x58));
    assert(reference[1]==(uintptr_t)following && previous[2]==(uintptr_t)following && following[1]==(uintptr_t)previous);
    assert(*(float *)(position+0x90)==3 && *(float *)(movement+0x24)==3 && !movement[0xbe]);
    for(unsigned i=0x34;i<=0x48;i+=4) assert(*(float *)(movement+i)==4);
    reset(); node[0xa9]=0; run_snap(); /* Explicit authored/script placement stays native. */
    assert(!admitted[1] && snap_calls[0]==1 && snap_calls[1]==1 && snap_calls[2]==1);
    assert(*(float *)(position+0x90)==0 && *(float *)(movement+0x24)==0 && (movement[0xbe]&4));
    /* Actual fade timer executes its snap and completion branches. */
    reset(); *(float *)(node+0xa0)=0.04f; run_tick();
    assert(alpha_calls==1 && admitted[1]==1 && *(float *)(node+0xa0)<0);
    run_tick(); assert(alpha_calls==2 && fade_done==1 && !*(float *)(node+0xa0));
    assert(!snap_calls[0] && !speed_calls && !move_calls);
    abi();
    /* Restore failure retains both native continuations for a safe retry. */
    uint8_t saved=image[STORY_AI_SNAP_GATE+1]; image[STORY_AI_SNAP_GATE+1]^=1;
    assert(!SudekiMpStoryAiMovementUninstall());
    for(unsigned i=0;i<4;++i) assert(SudekiMpStoryAiMovementNext[i]);
    image[STORY_AI_SNAP_GATE+1]=saved; assert(SudekiMpStoryAiMovementUninstall());
    for(unsigned i=0;i<4;++i) assert(!SudekiMpStoryAiMovementNext[i]);
    assert(SudekiMpStoryAiMovementImageExact(image)); reset(); assert_native();
    assert(SudekiMpStoryAiMovementInstall((HMODULE)image,lease)); assert(SudekiMpStoryAiMovementUninstall());
    for(unsigned i=2;i-->0;) assert(SudekiMpRestoreRelativeCallHook(&observers[i]));
    assert(VirtualFree(image,0,MEM_RELEASE)); puts("story_ai_movement_image_test: PASS"); return 0;
}
