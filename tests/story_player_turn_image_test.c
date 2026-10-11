/* Supported retail turn and CanTurn branches against inert actor components.
 * No campaign, network transport, renderer or owner gameplay is simulated. */
#include "hooks/story_player_turn.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t *image;
static uint8_t actor[0x140],ai[0x170],mode[0xc],turn[0x50],position[0x110];
static uint8_t arbiter[0x64],movement[0xc0],combo[0xb0],targeter[0x94];
static BOOL grant=TRUE;
static DWORD thread;
static unsigned writes;
unsigned turn_counts[3];
void *turn_next[3];
extern void *SudekiMpStoryPlayerTurnNext[2];
static unsigned install_calls,fail_install;
BOOL __real_SudekiMpInstallInlineHook(SudekiMpInlineHook *,uint8_t *,const uint8_t *,size_t,const void *);
BOOL __wrap_SudekiMpInstallInlineHook(SudekiMpInlineHook *h,uint8_t *p,const uint8_t *b,size_t n,const void *fn) {
    if(++install_calls==fail_install) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return FALSE; }
    return __real_SudekiMpInstallInlineHook(h,p,b,n,fn);
}
static BOOL __attribute__((target("sse2"))) lease(void *owner) {
    /* Deliberately disturb FP/SSE: the seam must restore the native state. */
    __asm__ volatile("fninit; pxor %%xmm0,%%xmm0" ::: "xmm0");
    SetLastError(999);
    return grant && owner==actor && GetCurrentThreadId()==thread;
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

__attribute__((naked,noinline)) static void tick(void *entry __attribute__((unused)),
    void *component __attribute__((unused)),float dt __attribute__((unused))) {
    __asm__ volatile("push %edi; mov 12(%esp),%edi; pushl 16(%esp); call *12(%esp); pop %edi; ret");
}
static BOOL __cdecl is_player(void *owner) { assert(owner==actor); return FALSE; }
static void __attribute__((used,noinline)) forward_copy(void *p,const float *v) {
    assert(p==position); memcpy(position+0x50,v,12); ++writes;
}
__attribute__((naked,noinline)) static void set_forward(void) {
    __asm__ volatile("push %ecx; push %esi; call _forward_copy; add $8,%esp; ret");
}
static float *__stdcall normalize(float *out,const float *in) {
    float n=sqrtf(in[0]*in[0]+in[1]*in[1]+in[2]*in[2]);
    for(unsigned i=0;i<3;++i) out[i]=n>0?in[i]/n:0;
    return out;
}
#define STEP(name,offset) \
static void __attribute__((naked,noinline)) name(void) { \
    __asm__ volatile("pushfl; incl _turn_counts+" #offset "; popfl; jmp *(_turn_next+" #offset ")"); }
STEP(player,0)
STEP(ai_moving,4)
STEP(ai_standing,8)
static void jump(unsigned rva,void *replacement) {
    image[rva]=0xe9; int32_t d=(int32_t)((uint8_t *)replacement-image-rva-5);
    memcpy(image+rva+1,&d,4); assert(FlushInstructionCache(GetCurrentProcess(),image+rva,5));
}
static void reset_actor(void) {
    memset(turn_counts,0,sizeof(turn_counts)); writes=0; grant=TRUE;
    *(int16_t *)(ai+0x16a)=1; mode[0xb]=0; *(uint32_t *)(arbiter+0x50)=0;
    memset(position+0x50,0,12); *(float *)(position+0x58)=1;
    *(float *)(position+0x90)=1; *(float *)(movement+0x24)=1;
    *(float *)(turn+0x3c)=1; *(float *)(turn+0x40)=0; *(float *)(turn+0x44)=0;
    *(uint32_t *)(combo+0xa8)=0;
}
static void run_tick(void) {
    tick(image+0xb73d0,turn,0.05f);
    assert(*(int16_t *)(ai+0x16a)>=1); /* Callback never writes it. */
}
static void contract(void) {
    const struct { const char *name; unsigned rva,size; } r[]={
        {"STORY_TURN_CHOICE",0xb7420,0x25},{"STORY_CAN_TURN_CHOICE",0xdbb20,0x4a},
        {"STORY_TURN_GATE",0xb73d0,0x33},{"STORY_MOVE_GATE",0xdaeb9,0x21},
        {"STORY_PLAYER_ARGS",0xb759a,0x12},{"STORY_PLAYER_STEP",0xb7b90,0x11}};
    assert(SudekiMpStoryPlayerTurnImageExact(image));
    for(unsigned k=0;k<6;++k) {
        for(unsigned i=0;i<r[k].size;++i) {
            image[r[k].rva+i]^=1; assert(!SudekiMpStoryPlayerTurnImageExact(image));
            assert(!SudekiMpStoryPlayerTurnInstall((HMODULE)image,lease));
            image[r[k].rva+i]^=1;
        }
        printf("%s: exact bytes and every-byte install rejection passed\n",r[k].name);
    }
}
static DWORD WINAPI other_thread(void *unused) { (void)unused; run_tick(); return 0; }
typedef void (__stdcall *Move)(void *,const float *,float,float,unsigned);
static void run_move(BOOL scoped) {
    float direction[3]={0,0,-1};
    if(scoped) assert(SudekiMpStoryPlayerTurnMoveBegin(actor));
    ((Move)(image+0xdae80))(arbiter,direction,0.7f,1.0f,0);
    if(scoped) SudekiMpStoryPlayerTurnMoveEnd();
}
/* Probe the actual replacement at its native entry, ending before the native
 * JG. Verify ABI independently of the rest of the turn function. */
uint32_t probe_regs[8],probe_flags,probe_sse[4];
float probe_fp;
__attribute__((naked,noinline)) static void probe_return(void) {
    __asm__ volatile("pushfl; popl _probe_flags; pushal; mov %esp,%esi; mov $_probe_regs,%edi;"
        "mov $8,%ecx; cld; rep movsl; popal; movups %xmm0,_probe_sse; fstps _probe_fp; ret");
}
__attribute__((naked,noinline)) static void probe(void) {
    __asm__ volatile("pushal; mov $_turn,%edi; mov $_ai,%eax; mov $0x1234,%ecx; mov $0x5678,%edx;"
        "mov $0x1111,%ebx; mov $0x2222,%esi; mov $0x3333,%ebp;"
        "pcmpeqd %xmm0,%xmm0; fld1; call *_probe_entry; popal; ret");
}
void *probe_entry;
int main(int argc,char **argv) {
    wchar_t path[32768]; assert(argc==2);
    assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,32768)); image=map_image(path);
    contract(); thread=GetCurrentThreadId();
    *(void **)(actor+0x44)=position; *(void **)(actor+0x80)=movement;
    *(void **)(actor+0x8c)=turn; *(void **)(actor+0x90)=arbiter; *(void **)(actor+0x94)=ai;
    *(void **)(actor+0xac)=targeter; *(void **)(actor+0xb8)=combo;
    *(void **)ai=image+0x2d4924; *(void **)(ai+0x10)=actor; *(void **)(ai+0x3c)=mode;
    *(void **)turn=image+0x2d48d4; *(void **)(turn+0x10)=actor;
    *(void **)arbiter=image+0x2cc9ac; *(void **)(arbiter+0x10)=actor;
    *(float *)(turn+0x18)=180; *(float *)(turn+0x1c)=180; movement[0xbe]=8;
    jump(0xd4360,is_player); jump(0x1114d0,set_forward);
    *(void **)(image+0x29a2b0)=normalize;
    /* Exact same three observation seams as Research54, separate from C2. */
    SudekiMpRelativeCallHook observers[3]={0};
    unsigned calls[3]={0xb75ac,0xb749f,0xb74dd},entries[3]={0xb7b90,0xb7c50,0xb7cc0};
    void (*spies[3])(void)={player,ai_moving,ai_standing};
    for(unsigned i=0;i<3;++i) {
        turn_next[i]=image+entries[i];
        assert(SudekiMpInstallRelativeCallHook(&observers[i],image+calls[i],image+entries[i],spies[i]));
    }
    reset_actor(); run_tick(); assert(turn_counts[1]==1 && !turn_counts[0]);
    install_calls=0; fail_install=2;
    assert(!SudekiMpStoryPlayerTurnInstall((HMODULE)image,lease));
    assert(SudekiMpStoryPlayerTurnImageExact(image) && !SudekiMpStoryPlayerTurnNext[0]);
    fail_install=0;
    assert(SudekiMpStoryPlayerTurnInstall((HMODULE)image,lease));
    assert(SudekiMpStoryPlayerTurnExact());
    reset_actor(); run_tick(); assert(turn_counts[1]==1 && !turn_counts[0]); /* Unbound thread. */
    assert(SudekiMpStoryPlayerTurnBindThread());
    reset_actor(); run_tick(); assert(turn_counts[0]==1 && !turn_counts[1] && !turn_counts[2]);
    assert(writes==1 && *(float *)(position+0x50)>0 && *(float *)(position+0x58)>0);
    assert(*(int16_t *)(ai+0x16a)==1 && mode[0xb]==0);
    reset_actor(); *(int16_t *)(ai+0x16a)=2; run_tick();
    assert(turn_counts[1]==1 && !turn_counts[0] && *(int16_t *)(ai+0x16a)==2);
    reset_actor(); mode[0xb]=1; run_tick(); assert(turn_counts[1]==1 && !turn_counts[0]);
    reset_actor(); grant=FALSE; run_tick(); assert(turn_counts[1]==1 && !turn_counts[0]);
    reset_actor(); *(void **)(ai+0x10)=NULL; run_tick(); assert(turn_counts[1]==1 && !turn_counts[0]);
    *(void **)(ai+0x10)=actor;
    reset_actor(); HANDLE t=CreateThread(NULL,0,other_thread,NULL,0,NULL); assert(t);
    assert(WaitForSingleObject(t,10000)==WAIT_OBJECT_0); CloseHandle(t);
    assert(turn_counts[1]==1 && !turn_counts[0]);
    /* Native player CanTurn blocks attack/combo turning; the game's explicit
     * 0x20000 turn latch may still choose the player interpolation step. */
    reset_actor(); *(uint32_t *)(arbiter+0x50)=0x1000; run_tick();
    assert(!writes && !turn_counts[0] && !turn_counts[1]);
    *(uint32_t *)(arbiter+0x50)|=0x20000; run_tick(); assert(turn_counts[0]==1 && writes==1);
    reset_actor(); *(uint32_t *)(combo+0xa8)=1; run_tick(); assert(!writes && !turn_counts[0]);
    reset_actor(); *(uint32_t *)(arbiter+0x50)=0x1000;
    run_move(TRUE); assert(*(float *)(turn+0x3c)==1 && *(float *)(turn+0x44)==0);
    run_move(FALSE); assert(*(float *)(turn+0x3c)==0 && *(float *)(turn+0x44)==-1);
    reset_actor(); assert(SudekiMpStoryPlayerTurnMoveBegin(actor));
    assert(!SudekiMpStoryPlayerTurnMoveBegin(actor) && !SudekiMpStoryPlayerTurnUninstall());
    SudekiMpStoryPlayerTurnMoveEnd(); assert(SudekiMpStoryPlayerTurnExact());
    reset_actor(); probe_entry=image+STORY_TURN_BRANCH;
    void *next=SudekiMpStoryPlayerTurnNext[0]; SudekiMpStoryPlayerTurnNext[0]=probe_return;
    SetLastError(123); probe(); assert(GetLastError()==123);
    assert(probe_regs[0]==(uintptr_t)turn && probe_regs[1]==0x2222 && probe_regs[2]==0x3333);
    assert(probe_regs[4]==0x1111 && probe_regs[5]==0x5678 && probe_regs[6]==0x1234 && probe_regs[7]==(uintptr_t)ai);
    assert((probe_flags&0x40)!=0 && probe_fp==1);
    for(unsigned i=0;i<4;++i) assert(probe_sse[i]==UINT32_MAX);
    grant=FALSE; probe(); assert(!(probe_flags&0x40) && probe_fp==1);
    SudekiMpStoryPlayerTurnNext[0]=next;
    /* Failed restore retains continuations; repair foreign byte then retry. */
    uint8_t saved=image[STORY_CAN_TURN_BRANCH+1]; image[STORY_CAN_TURN_BRANCH+1]^=1;
    assert(!SudekiMpStoryPlayerTurnUninstall());
    assert(SudekiMpStoryPlayerTurnNext[0] && SudekiMpStoryPlayerTurnNext[1]);
    image[STORY_CAN_TURN_BRANCH+1]=saved; assert(SudekiMpStoryPlayerTurnUninstall());
    assert(!SudekiMpStoryPlayerTurnNext[0] && !SudekiMpStoryPlayerTurnNext[1]);
    assert(SudekiMpStoryPlayerTurnImageExact(image));
    assert(SudekiMpStoryPlayerTurnInstall((HMODULE)image,lease));
    assert(SudekiMpStoryPlayerTurnUninstall());
    for(unsigned i=3;i-->0;) assert(SudekiMpRestoreRelativeCallHook(&observers[i]));
    reset_actor(); run_tick(); assert(writes==1); /* Restored retail AI path. */
    assert(VirtualFree(image,0,MEM_RELEASE));
    puts("story_player_turn_image_test: PASS"); return 0;
}
