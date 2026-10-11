/* Exact retail one-shot timing on inert renderer storage, without rendering. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine/build_identity.h"
#include "../src/engine/build_identity.c"
#include "../src/engine/sha256.c"
#include "../src/hooks/lan_arena_spirit_vfx.c"

/* The timing fixture never creates an actor, effect, or resource. */
void SudekiMpLogFormat(const char *format,...) { (void)format; }
BOOL SudekiMpLogResearchEnabled(void) { return FALSE; }
void *SudekiMpCleanroomEngineActorEntity(SudekiMpCleanroomActor actor) {
    (void)actor; assert(0); return NULL;
}
BOOL SudekiMpCleanroomActorFromType(unsigned type,SudekiMpCleanroomActor *actor) {
    (void)type; (void)actor; assert(0); return FALSE;
}
BOOL SudekiMpCleanroomEngineResourceNameFromText(SudekiMpResourceName *name,const char *text) {
    (void)name; (void)text; assert(0); return FALSE;
}
void SudekiMpCleanroomEngineReleaseResourceName(SudekiMpResourceName *name) {
    (void)name; assert(0);
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

static void native_advance(uint8_t *image,void *renderer,float dt) {
    uint32_t value; memcpy(&value,&dt,4);
    void *entry=image+0x221d70u;
    __asm__ volatile("pushl %[value]\n\tcall *%[entry]"
        : : "D"(renderer), [value] "r"(value), [entry] "r"(entry)
        : "eax","ecx","edx","memory","cc");
}
static void put(void *p,unsigned offset,const void *v) { memcpy((uint8_t *)p+offset,&v,4); }
static void scalar(void *p,unsigned offset,float f) { memcpy((uint8_t *)p+offset,&f,4); }
static void model_clock_ownership(uint8_t *image) {
    uint8_t e[0x3e4]={0},renderer[0xa8]={0},wrapper[0x18]={0},object[0x18]={0};
    uint8_t model[0x5c]={0},child[0x28]={0},resource[0x44]={0},definition[8]={0};
    uint8_t instance[0x8c]={0}; void *children[2]={child,NULL},*sources[1],*listeners[1];
    uint16_t flags[2]={3,0}; void *out[2]={(void *)1,(void *)2}; unsigned count=99;
    SudekiMpLanArenaSpiritVfxVisualState state={.session_token=7};
    SudekiMpLanArenaSpiritVfxVisualSlot *slot=&state.slots[63];
    slot->occupied=slot->spawn_entered=TRUE;slot->identity.kind=8;slot->observer.object=e;
    put(e,0,image+RVA_VISUAL_VTABLE);put(e,4,&slot->observer);
    put(e,0x44,e+0x160);put(e,0x58,e+0x270);put(e,0x160,image+RVA_VISUAL_POSITION_VTABLE);
    put(e,0x270,image+RVA_VISUAL_COMPONENT_VTABLE);put(e,0x170,e);put(e,0x280,e);
    put(e,0x5c,e+0x2e8);put(e,0x2e8,image+RVA_VISUAL_SOUND_VTABLE);put(e,0x2f8,e);
    put(e,0x288,image+RVA_VISUAL_EVENT_SOURCE_VTABLE);put(e,0x300,image+RVA_VISUAL_SOUND_LISTENER_VTABLE);
    *(unsigned *)(e+0x290)=*(unsigned *)(e+0x308)=1;
    put(e,0x298,listeners);put(e,0x310,sources);listeners[0]=e+0x300;sources[0]=e+0x288;
    *(unsigned *)(e+0x2a0)=0x449d201b;put(e,0x214,wrapper);
    put(wrapper,8,object);put(wrapper,0xc,renderer);put(wrapper,0x10,renderer);put(object,0x14,renderer);
    put(renderer,0,image+RVA_VISUAL_RENDERER_VTABLE);put(renderer,8,model);
    put(renderer,0x64,children);put(renderer,0x68,flags);*(unsigned *)(model+0x58)=1;
    put(child,0,image+0x2de564);put(child,4,image+0x2de658);put(child,0x10,resource);put(child,0x14,instance);
    put(resource,0,image+0x2de520);*(unsigned *)(resource+8)=1;put(resource,0x14,definition);
    put(instance,0,image+0x2e22c8);
    assert(collect_visual_model_clocks(image,&state,out,2,&count) && count==1 && out[0]==instance);
    /* A native hold is still model-owned. This observation neither changes
     * flags/lifetime nor requires a new host packet to keep that ownership. */
    for(unsigned i=0;i<20;++i) assert(collect_visual_model_clocks(image,&state,out,2,&count) && count==1);
#define REFUSED() do { out[0]=(void *)1;count=99;assert(!collect_visual_model_clocks(image,&state,out,2,&count));assert(count==99 && out[0]==(void *)1); } while(0)
    flags[0]=1;REFUSED();flags[0]=3;
    *(unsigned *)(child+0x20)=1;REFUSED();*(unsigned *)(child+0x20)=0;
    *(unsigned *)(definition+4)=1;REFUSED();*(unsigned *)(definition+4)=0;
    *(unsigned *)(resource+8)=0;REFUSED();*(unsigned *)(resource+8)=1;
    put(instance,0,image);REFUSED();put(instance,0,image+0x2e22c8);
    children[1]=child;*(unsigned *)(model+0x58)=2;REFUSED();children[1]=NULL;*(unsigned *)(model+0x58)=1;
    put(object,0x14,child);REFUSED();put(object,0x14,renderer);
    put(e,4,NULL);REFUSED();put(e,4,&slot->observer);
    slot->spawn_entered=FALSE;REFUSED();slot->spawn_entered=TRUE;
    state.session_token=0;REFUSED();state.session_token=7;
    assert(!collect_visual_model_clocks(image,&state,out,0,&count));
    slot->retire_entered=TRUE;
    assert(collect_visual_model_clocks(image,&state,out,2,&count) && count==1);
    instance[0x88]=1;assert(collect_visual_model_clocks(image,&state,out,2,&count) && !count);instance[0x88]=0;
    slot->observer.object=NULL;assert(collect_visual_model_clocks(image,&state,out,2,&count) && !count);
    slot->observer.object=e;assert(collect_visual_model_clocks(image,&state,out,2,&count) && count==1);
    slot->identity.kind=SUDEKIMP_LAN_STORY_VFX_SHIN_SPLITTER;
    *(unsigned *)(e+0x2a0)=0x79853f03;
    assert(collect_visual_model_clocks(image,&state,out,2,&count) && count==1);
    *(unsigned *)(e+0x2a0)=0x449d201b; REFUSED();
    slot->identity.kind=12;assert(collect_visual_model_clocks(image,&state,out,2,&count) && !count);
#undef REFUSED
    puts("PASS retained summon child clock ownership, holds, native destruction, foreign/duplicate/resource refusal");
}

int main(int argc,char **argv) {
    wchar_t path[32768];
    assert(argc==2 && MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,32768));
    uint8_t *image=map_image(path);
    assert(visual_timing_image_exact(image));
    model_clock_ownership(image);
    image[0x2e3670u]^=1u; assert(!visual_timing_image_exact(image)); image[0x2e3670u]^=1u;
    for(unsigned i=0;i<sizeof(visual_timing_identities)/sizeof(visual_timing_identities[0]);++i) {
        const struct VisualTimingIdentity *id=&visual_timing_identities[i];
        for(unsigned j=0;j<id->size;++j) {
            image[id->rva+j]^=1u; assert(!visual_timing_image_exact(image)); image[id->rva+j]^=1u;
        }
        printf("PASS %s RVA=%06x bytes=%u exact plus every-byte mutation refusal\n",id->name,id->rva,id->size);
    }
    uint8_t renderer[0xacu]={0},model[0x24u]={0},header[0x10u]={0};
    uint8_t bank[0x24u]={0},channel[0x18u]={0},table[0x1cu]={0},clip[8]={0};
    put(renderer,8,model); put(renderer,0x98,bank); *(uint32_t *)(renderer+0xa0)=1u;
    put(model,0x1c,header); *(uint32_t *)(header+0xc)=1u;
    put(model,0x20,table); put(table,0,clip); put(bank,0,channel);
    scalar(clip,4,220.0f); scalar(channel,4,24.0f);
    VisualSetTime set_time=(VisualSetTime)(image+0x223180u);
    VisualSetState set_state=(VisualSetState)(image+0x223240u);
    float end=(float)(220.0-*(double *)(image+0x2e3670u));
    assert(end>219.99f && end<220.0f);
    /* Reproduce the old client: every host end sample followed by the real
     * native loop update samples the bright beginning again, not the tail. */
    for(unsigned i=0;i<10;++i) {
        set_time(renderer,0,0,end,0); native_advance(image,renderer,.025f);
        assert(*(float *)(channel+8)>220.0f);
        assert(*(float *)(channel+12)<1.0f);
        assert((*(uint16_t *)(channel+2)&0x80u)!=0u);
    }
    puts("PASS reproduced native loop wrap during repeated host final hold");
    VisualPlaybackPlan plan;
    for(unsigned kind=SUDEKIMP_LAN_STORY_VFX_SHIN_SPLITTER;kind<=SUDEKIMP_LAN_STORY_VFX_AIM_ARC;++kind) {
        assert(visual_playback_plan(kind,10,5,60,0,TRUE,&plan));
        assert(plan.set_state && plan.state==1 && !plan.set_phase);
        assert(visual_playback_plan(kind,10,60,60,1,TRUE,&plan));
        assert(plan.set_phase && plan.phase<60);
        assert(visual_playback_plan(kind,59.99f,60,60,0x41,TRUE,&plan));
        assert(!plan.set_phase && !plan.set_state);
    }

    for(unsigned attempt=0;attempt<2;++attempt) {
        memset(channel,0,sizeof(channel)); scalar(channel,4,24.0f);
        set_time(renderer,0,0,attempt?0.0f:219.5f,0);
        for(unsigned i=0;i<20;++i) {
            uint16_t old=*(uint16_t *)(channel+2);
            /* Includes exact-duration late spawn and repeated end packets. */
            float target=attempt?220.0f:end;
            assert(visual_playback_plan(SUDEKIMP_LAN_ARENA_SPIRIT_VFX_TAL_STRIKE_ONE,
                *(float *)(channel+8),target,220.0f,old,TRUE,&plan));
            if(old==0x41u) assert(!plan.set_state && !plan.set_phase);
            if(plan.set_state) set_state(renderer,0,0,plan.state);
            if(plan.set_phase) {
                assert(plan.phase<220.0f);
                set_time(renderer,0,0,plan.phase,0);
            }
            native_advance(image,renderer,.025f);
            assert(*(float *)(channel+8)==end && *(float *)(channel+12)==end);
            assert(*(uint16_t *)(channel+2)==0x41u);
        }
    }
    assert(!visual_playback_plan(8,0,1,220,2,TRUE,&plan));
    assert(!visual_playback_plan(8,0,1,0,0,TRUE,&plan));
    assert(!visual_playback_plan(8,0,NAN,220,0,TRUE,&plan));
    assert(!visual_playback_plan(8,-1,1,220,0,TRUE,&plan));
    assert(visual_playback_plan(8,100,99,220,1,TRUE,&plan) && !plan.set_phase && !plan.set_state);
    assert(visual_playback_plan(8,0,100,220,0,FALSE,&plan) && !plan.set_phase && plan.set_state);
    /* A genuinely looping resource retains its state and backward seek. */
    assert(visual_playback_plan(2,100,1,220,0x80,TRUE,&plan) && plan.set_phase && !plan.set_state);
    assert(plan.phase==1 && plan.state==0x80);
    VirtualFree(image,0,MEM_RELEASE);
    puts("Spirit timing: PASS (native terminal sample held, repeated/late packets, no forced retirement)");
    return 0;
}
