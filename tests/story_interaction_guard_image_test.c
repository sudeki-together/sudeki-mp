/* Exact-image early native input branches, inert actors/trigger manager.
 * Never load a campaign, dispatch a script, or execute an attack task. */
#include "hooks/story_interaction_guard.h"
#include "hooks/story_block_input.h"
#include "hooks/story_melee_snapshot.h"
#include "network/lan_story_melee.h"
#include "engine/build_identity.h"
#include "engine/arbiter_combat_input.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static uint8_t *image;
static uint8_t actor[0x140],arbiter[0x64],trigger[0x1da],interaction[0x64],mode[0x84];
static unsigned submissions;
static unsigned attack_count,attack_kinds[8];
static void __attribute__((stdcall)) observe_attack(unsigned kind,unsigned flag) {
    assert(kind>=1u && kind<=3u && flag==1u && attack_count<8u);
    attack_kinds[attack_count++]=kind;
}
static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || !VirtualQuery(p,&m,sizeof(m)) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return FALSE;
    return a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
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
static void submit(unsigned kind) {
    ++submissions;
    SudekiMpSubmitArbiterCombatInput(image+0xdb0e0,arbiter,kind==1,kind==2,kind==3,0,0,0);
}
static BOOL guarded(unsigned kind) {
    if(!SudekiMpStoryInteractionInputImageExact(image,readable) ||
        !SudekiMpStoryMeleeInteractionClear(image,actor,kind,readable)) return FALSE;
    submit(kind); return TRUE;
}
/* C5 reproduction: execute the retail input router, replacing only its
 * downstream attack task in this isolated mapping with an inert observer.
 * This proves request selection, not actual combo acceptance or timing. */
static void reproduce_cadence(void) {
    assert(SudekiMpStoryMeleeSnapshotImageExact(image,readable));
    for(unsigned g=0;g<sizeof(story_melee_contracts)/sizeof(*story_melee_contracts);++g) {
        for(unsigned i=0;i<story_melee_contracts[g].size;++i) {
            image[story_melee_contracts[g].rva+i]^=1;
            assert(!SudekiMpStoryMeleeSnapshotImageExact(image,readable));
            image[story_melee_contracts[g].rva+i]^=1;
        }
        printf("named byte guard %s passed including mutation refusal\n",story_melee_contracts[g].name);
    }
    uint8_t *call=image+0xdb222u,saved[5]; int32_t displacement;
    memcpy(saved,call,sizeof(saved)); memcpy(&displacement,call+1,4);
    assert(call[0]==0xe8 && call+5+displacement==image+0xdac00u);
    displacement=(int32_t)((uintptr_t)observe_attack-(uintptr_t)(call+5));
    memcpy(call+1,&displacement,4);
    assert(FlushInstructionCache(GetCurrentProcess(),call,5));
    assert(SudekiMpStoryInteractionInputImageExact(image,readable));
    *(uint32_t *)(arbiter+0x50)=2u;
    *(uint32_t *)(arbiter+0x58)=0x12u; arbiter[0x60]=2u;
    assert(!*(void **)(actor+0xa8) && !*(void **)(actor+0xb4));
    trigger[0x1d8]=trigger[0x1d9]=0;
    for(unsigned kind=1;kind<=3;++kind) {
        const unsigned states[]={1,2,2,2,3,0};
        attack_count=0;
        for(unsigned i=0;i<sizeof(states)/sizeof(states[0]);++i) {
            unsigned state=states[i];
            SudekiMpSubmitArbiterCombatInput(image+0xdb0e0,arbiter,
                kind==1?state:0,kind==2?state:0,kind==3?state:0,0,0,0);
            assert(attack_count==1 && attack_kinds[0]==kind);
        }
    }
    /* One native snapshot arbitrates simultaneous buttons. The current story
     * edge queue submits them separately, allowing two admission attempts. */
    attack_count=0;
    SudekiMpSubmitArbiterCombatInput(image+0xdb0e0,arbiter,1,1,0,0,0,0);
    assert(attack_count==1 && attack_kinds[0]==1);
    attack_count=0; submit(1); submit(2);
    assert(attack_count==2 && attack_kinds[0]==1 && attack_kinds[1]==2);
    attack_count=0;
    SudekiMpSubmitArbiterCombatInput(image+0xdb0e0,arbiter,1,1,1,0,0,0);
    assert(attack_count==1 && attack_kinds[0]==3); /* sweep > weak > strong */
    /* The press-only interaction guard must NOT be reused for release states:
     * any melee release clears the host's shared interaction latch. */
    for(unsigned kind=1;kind<=3;++kind) {
        trigger[0x1d8]=1;
        SudekiMpSubmitArbiterCombatInput(image+0xdb0e0,arbiter,
            kind==1?3:0,kind==2?3:0,kind==3?3:0,0,0,0);
        assert(!trigger[0x1d8]);
    }
    /* Execute the production combat-only ABI on every snapshot and host
     * latch/availability combination. The real router selects the attack;
     * only the downstream task is replaced by the inert sink. */
    for(unsigned buttons=0;buttons<64u;++buttons) {
        for(unsigned flags=0;flags<256u;++flags) for(unsigned latch=0;latch<3u;++latch) {
            trigger[0x1d8]=(uint8_t)latch;trigger[0x1d9]=(uint8_t)flags;attack_count=0;
            uintptr_t before,after;
            __asm__ volatile("movl %%esp,%0":"=r"(before));
            SudekiMpSubmitArbiterMeleeSnapshot(image+0xdb189,arbiter,
                buttons&3u,(buttons>>2u)&3u,(buttons>>4u)&3u,0);
            __asm__ volatile("movl %%esp,%0":"=r"(after));
            unsigned selected=SudekiMpStoryMeleePress(buttons);
            assert(before==after && attack_count==(selected?1u:0u));
            if(selected) assert(attack_kinds[0]==selected);
            assert(trigger[0x1d8]==latch && trigger[0x1d9]==flags);
        }
    }
    const unsigned sequence[]={1,2,2,2,3,0};attack_count=0;
    for(unsigned i=0;i<6u;++i) {
        SudekiMpSubmitArbiterMeleeSnapshot(image+0xdb189,arbiter,sequence[i],0,0,0);
        assert(attack_count==1 && attack_kinds[0]==1);
    }
    *(uint32_t *)(arbiter+0x50)=0;attack_count=0;
    SudekiMpSubmitArbiterMeleeSnapshot(image+0xdb189,arbiter,1,1,1,0);
    assert(!attack_count);*(uint32_t *)(arbiter+0x50)=2;
    puts("combat-only native snapshots: all 64 states, held/released cadence, host latch preservation and stack balance passed");
    memcpy(call,saved,sizeof(saved));
    assert(FlushInstructionCache(GetCurrentProcess(),call,sizeof(saved)));
    puts("C5 native routing reproduced: holds do not repeat melee; simultaneous buttons select one attack; separate presses attempt two; releases clear shared latch");
}
static unsigned block_starts;
static void observe_block_start(void) { ++block_starts; }
static void reproduce_block(void) {
    static const struct {const char *name;unsigned rva,n;} guards[]={
        {"BlockInputSelection",0xdb189,0x67},{"BlockStartDispatch",0xdb428,14},{"BlockRelease",0xdad60,0x36},
        {"DodgeAdmissionAndBlockHandoff",0xdae00,0x71}};
    assert(SudekiMpStoryBlockImageExact(image,readable));
    for(unsigned g=0;g<sizeof(guards)/sizeof(*guards);++g) {
        for(unsigned i=0;i<guards[g].n;++i) {
            image[guards[g].rva+i]^=1;assert(!SudekiMpStoryBlockImageExact(image,readable));
            image[guards[g].rva+i]^=1;
        }
        printf("named byte guard %s passed including mutation refusal\n",guards[g].name);
    }
    uint8_t block[0x20]={0};*(void **)(actor+0xb4)=block;
    *(void **)(actor+0xa8)=NULL;*(uint32_t *)(arbiter+0x50)=2;
    *(uint32_t *)(arbiter+0x58)=0x12;arbiter[0x60]=2;
    uint8_t saved[5],*call=image+0xdb42a;int32_t rel;
    memcpy(saved,call,5);memcpy(&rel,call+1,4);assert(call[0]==0xe8 && call+5+rel==image+0xdaca0);
    rel=(int32_t)((uintptr_t)observe_block_start-(uintptr_t)(call+5));memcpy(call+1,&rel,4);
    assert(FlushInstructionCache(GetCurrentProcess(),image+0xdb189,0x2b0));
    for(unsigned state=1;state<=3;++state) {
        trigger[0x1d8]=1;trigger[0x1d9]=2;*(uint32_t *)(block+0x18)=2;
        SudekiMpSubmitArbiterCombatInput(image+0xdb0e0,arbiter,0,0,0,state,0,0);
        assert(trigger[0x1d8]==1 && trigger[0x1d9]==2);
        if(state<=2) assert(block_starts==state);else assert(*(uint32_t *)(block+0x18)==7);
    }
    unsigned starts=block_starts;
    SudekiMpSubmitArbiterMeleeSnapshot(image+0xdb189,arbiter,1,1,1,2);
    assert(block_starts==starts+1 && trigger[0x1d8]==1 && trigger[0x1d9]==2);
    memcpy(call,saved,5);assert(FlushInstructionCache(GetCurrentProcess(),call,5));
    assert(SudekiMpStoryBlockImageExact(image,readable));
    /* Ending input acts only on this native block. It can release during an
     * interaction without advancing its counter or clearing the host latch. */
    *(void **)(actor+0xa8)=interaction;mode[0x4c]=1;*(uint32_t *)(mode+0x80)=0;
    for(unsigned state=0;state<=7;++state) {
        *(uint32_t *)(block+0x18)=state;
        SudekiMpStoryBlockRelease(image,arbiter);
        assert(*(uint32_t *)(block+0x18)==(state>=1 && state<=4?7:state));
        assert(!*(uint32_t *)(mode+0x80) && trigger[0x1d8]==1 && trigger[0x1d9]==2);
    }
    mode[0x4c]=0;*(void **)(actor+0xb4)=NULL;*(void **)(actor+0xa8)=NULL;
    puts("native block press/hold/release routing and isolated ending request passed");
}

volatile uintptr_t story_dodge_direction,story_dodge_block;
volatile unsigned story_dodge_block_state;
__attribute__((naked,noinline)) static void accept_dodge(void) {
    __asm__ volatile("movl %ecx, _story_dodge_direction\n\tmovb $1, %al\n\tret\n\t");
}
__attribute__((naked,noinline)) static void observe_dodge_block_end(void) {
    __asm__ volatile("movl %edi, _story_dodge_block\n\tmovl %edx, _story_dodge_block_state\n\tret\n\t");
}
static void reproduce_dodge(void) {
    uint8_t dodge[0x20]={0},position[0xbc]={0},block[0x20]={0};
    const float heading[3]={0.6f,0,-0.8f};
    assert(SudekiMpArbiterDodgeImageMatches((HMODULE)image));
    assert(SudekiMpBlockResetImageMatches((HMODULE)image));
    puts("named native checks SudekiMpArbiterDodgeImageMatches and SudekiMpBlockResetImageMatches passed");
    *(void **)(actor+0xd0)=dodge;*(void **)(actor+0x44)=position;*(void **)(actor+0xb4)=block;
    *(void **)(dodge+0x10)=actor;*(uint32_t *)(arbiter+0x50)=10;
    *(uint32_t *)(arbiter+0x58)=0x12;arbiter[0x60]=2;
    /* Execute the actual CDodge idle/cooldown branch. Non-idle state rejects
     * before any task, imported math function or scene access. */
    for(unsigned state=1;state<=3;++state) {
        *(uint32_t *)(dodge+0x1c)=state;
        assert(!SudekiMpSubmitArbiterDodgeInput(image+0xdae00,arbiter,heading));
        assert(*(uint32_t *)(dodge+0x1c)==state);
    }
    /* For positive arbitration isolate only downstream CDodge and CBlock
     * task entrypoints. The real arbiter must pass the direction and end the
     * block only after dodge accepts; this is not animation/physics proof. */
    const unsigned calls[2]={0xdae40,0xdae5e};uint8_t saved[2][5];
    void *sinks[2]={(void *)accept_dodge,(void *)observe_dodge_block_end};
    for(unsigned i=0;i<2;++i) {
        memcpy(saved[i],image+calls[i],5);
        int32_t rel=(int32_t)((uintptr_t)sinks[i]-(uintptr_t)(image+calls[i]+5));
        memcpy(image+calls[i]+1,&rel,4);
    }
    assert(FlushInstructionCache(GetCurrentProcess(),image+0xdae00,0x71));
    for(unsigned scenario=0;scenario<6;++scenario) {
        *(uint32_t *)(arbiter+0x50)=10;*(uint32_t *)(arbiter+0x58)=0x12;arbiter[0x60]=2;
        *(void **)(actor+0x134)=NULL;story_dodge_direction=story_dodge_block=0;
        story_dodge_block_state=99;
        if(scenario==1) *(uint32_t *)(arbiter+0x50)=1;
        if(scenario==2) *(uint32_t *)(arbiter+0x58)=0x13;
        if(scenario==3) arbiter[0x60]=0;
        if(scenario==4) *(void **)(actor+0x134)=actor;
        if(scenario==5) *(uint32_t *)(arbiter+0x50)=0x20a; /* native incompatible action flag */
        unsigned accepted=SudekiMpSubmitArbiterDodgeInput(image+0xdae00,arbiter,heading);
        assert(accepted==(scenario==0));
        if(!scenario) {
            assert(story_dodge_direction==(uintptr_t)heading && story_dodge_block==(uintptr_t)block);
            assert(story_dodge_block_state==0);
        } else assert(!story_dodge_direction && !story_dodge_block && story_dodge_block_state==99);
    }
    for(unsigned i=0;i<2;++i) memcpy(image+calls[i],saved[i],5);
    assert(FlushInstructionCache(GetCurrentProcess(),image+0xdae00,0x71));
    assert(SudekiMpStoryBlockImageExact(image,readable));
    *(void **)(actor+0xd0)=*(void **)(actor+0x44)=*(void **)(actor+0xb4)=NULL;
    puts("native dodge ABI, real cooldown rejection, arbiter admission and accepted-block handoff passed (task sinks isolated)");
}
int main(int argc,char **argv) {
    assert(argc==2); wchar_t path[1024];
    assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024)); image=map_image(path);
    assert(SudekiMpStoryInteractionInputImageExact(image,readable));
    for(unsigned i=0;i<0xa9;++i) {
        image[0xdb0e0+i]^=1;
        assert(!SudekiMpStoryInteractionInputImageExact(image,readable));
        image[0xdb0e0+i]^=1;
    }
    assert(FlushInstructionCache(GetCurrentProcess(),image+0xdb0e0,0xa9));
    *(void **)(image+0x408d24)=trigger;
    *(void **)trigger=image+0x2c5458; *(void **)(trigger+8)=image+0x2c5460;
    *(void **)(trigger+0x10)=image+0x2c5470;
    *(void **)(actor+0xa8)=interaction; *(void **)(arbiter+0x10)=actor;
    *(void **)interaction=image+0x2d4abc; *(void **)(interaction+0x10)=actor;
    *(void **)(interaction+0x60)=mode; *(void **)mode=image+0x2cbfcc;
    /* Native combat flags zero: retail returns after the early global branch.
     * Demonstrate the leak even when no actual attack can be accepted. */
    trigger[0x1d9]=2; submit(1); assert(trigger[0x1d8]==1);
    trigger[0x1d8]=0; unsigned before=submissions;
    assert(!guarded(1) && submissions==before && !trigger[0x1d8] && trigger[0x1d9]==2);
    /* Preserve a host press even after proximity bit2 has disappeared. */
    trigger[0x1d9]=0; trigger[0x1d8]=1;
    assert(!guarded(1) && submissions==before && trigger[0x1d8]==1);
    trigger[0x1d8]=0;
    for(unsigned kind=1;kind<=3;++kind) assert(guarded(kind));
    /* Active interaction branch advances on ALL kinds, not just weak. */
    mode[0x4c]=1;
    for(unsigned kind=1;kind<=3;++kind) {
        *(uint32_t *)(mode+0x80)=0; submit(kind); assert(*(uint32_t *)(mode+0x80)==1);
        before=submissions; assert(!guarded(kind) && submissions==before);
        assert(*(uint32_t *)(mode+0x80)==1);
    }
    mode[0x4c]=0; trigger[0x1d8]=1; trigger[0x1d9]=2;
    assert(guarded(2) && guarded(3)); assert(trigger[0x1d8]==1 && trigger[0x1d9]==2);
    /* Exhaust every manager flag byte; policy reads without mutation. */
    for(unsigned flags=0;flags<256;++flags) for(unsigned latch=0;latch<3;++latch) {
        trigger[0x1d9]=(uint8_t)flags; trigger[0x1d8]=(uint8_t)latch;
        for(unsigned kind=1;kind<=3;++kind)
            assert(SudekiMpStoryMeleeInteractionClear(image,actor,kind,readable)==
                (kind!=1 || (!(flags&2) && !latch)));
        assert(trigger[0x1d9]==flags && trigger[0x1d8]==latch);
    }
    trigger[0x1d8]=trigger[0x1d9]=0;
    for(unsigned part=0;part<3;++part) {
        unsigned offset=part==0?0:part==1?8:16;
        void *old=*(void **)(trigger+offset); *(void **)(trigger+offset)=image;
        assert(!guarded(1)); *(void **)(trigger+offset)=old;
    }
    *(void **)(interaction+0x10)=NULL;
    for(unsigned kind=1;kind<=3;++kind) assert(!guarded(kind));
    *(void **)(interaction+0x10)=actor;
    *(void **)(interaction+0x60)=NULL; assert(!guarded(1)); *(void **)(interaction+0x60)=mode;
    *(void **)mode=image; assert(!guarded(2)); *(void **)mode=image+0x2cbfcc;
    *(void **)interaction=image; assert(!guarded(3)); *(void **)interaction=image+0x2d4abc;
    *(void **)(image+0x408d24)=NULL; assert(!guarded(1)); *(void **)(image+0x408d24)=trigger;
    assert(!guarded(0) && !guarded(4));
    /* Retail explicitly supports no interaction component; only its global
     * weak branch then needs containment. */
    *(void **)(actor+0xa8)=NULL; assert(guarded(1));
    assert(!SudekiMpStoryMeleeInteractionClear(image,NULL,1,readable));
    reproduce_cadence();
    reproduce_block();
    reproduce_dodge();
    VirtualFree(image,0,MEM_RELEASE);
    puts("story interaction guard exact-image/early-native-branch tests passed (no campaign or attack tasks)");
    return 0;
}
