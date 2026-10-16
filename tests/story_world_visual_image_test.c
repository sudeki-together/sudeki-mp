/* Execute supported-image weak tracking and item collection; renderer and
 * inventory endpoints are inert fixtures. This does not prove live visuals. */
#include "hooks/lan_party_projectile_lifetime.h"
#include "engine/build_identity.h"
#include "../src/hooks/lan_arena_spirit_visual_host.c"
#include <assert.h>
#include <stdio.h>
void SudekiMpLogFormat(const char *fmt,...) {(void)fmt;}
void SudekiMpLogWrite(const char *text) {(void)text;}
void *SudekiMpCleanroomEngineActorEntity(SudekiMpCleanroomActor actor) {(void)actor;return NULL;}
BOOL SudekiMpLanPartyProjectileLifetimeObserve(SudekiMpLanPartyProjectileObservation *o,unsigned c,unsigned *n) {
    (void)o;(void)c;*n=0;return TRUE;
}
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(f!=INVALID_HANDLE_VALUE); DWORD size=GetFileSize(f,NULL),got=0;
    uint8_t *raw=malloc(size); assert(raw && ReadFile(f,raw,size,&got,NULL) && got==size); CloseHandle(f);
    IMAGE_DOS_HEADER *dos=(void *)raw;
    IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *b=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    assert(b); memcpy(b,raw,nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *sections=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        IMAGE_SECTION_HEADER *s=&sections[i];
        assert(s->PointerToRawData<=size && s->SizeOfRawData<=size-s->PointerToRawData);
        assert(s->VirtualAddress<=nt->OptionalHeader.SizeOfImage &&
            s->SizeOfRawData<=nt->OptionalHeader.SizeOfImage-s->VirtualAddress);
        memcpy(b+s->VirtualAddress,raw+s->PointerToRawData,s->SizeOfRawData);
    }
    uintptr_t delta=(uintptr_t)b-nt->OptionalHeader.ImageBase;
    IMAGE_DATA_DIRECTORY reloc=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    unsigned offset=0;
    while(offset<reloc.Size) {
        IMAGE_BASE_RELOCATION *block=(void *)(b+reloc.VirtualAddress+offset);
        assert(block->SizeOfBlock>=sizeof(*block) && block->SizeOfBlock<=reloc.Size-offset);
        uint16_t *items=(uint16_t *)(block+1);
        unsigned count=(block->SizeOfBlock-sizeof(*block))/2;
        for(unsigned i=0;i<count;++i) {
            unsigned type=items[i]>>12,rva=block->VirtualAddress+(items[i]&0xfffu);
            assert(type==IMAGE_REL_BASED_ABSOLUTE || type==IMAGE_REL_BASED_HIGHLOW);
            if(type==IMAGE_REL_BASED_HIGHLOW) {
                assert(rva<=nt->OptionalHeader.SizeOfImage-4);
                *(uint32_t *)(b+rva)+=(uint32_t)delta;
            }
        }
        offset+=block->SizeOfBlock;
    }
    free(raw); return b;
}

static void jump_to(unsigned rva,void *target) {
    uint8_t *p=(uint8_t *)image+rva;p[0]=0xe9;
    int32_t d=(int32_t)((uintptr_t)target-(uintptr_t)(p+5));memcpy(p+1,&d,4);
    assert(FlushInstructionCache(GetCurrentProcess(),p,5));
}
static const float *__attribute__((thiscall)) get_matrix(void *p) {return *(void **)((uint8_t *)p+0x8c);}
static uint8_t inventory[0x130];
static unsigned awards,consumed,checks;static BOOL has_room=TRUE;
static BOOL __attribute__((thiscall)) can_add(void *self,unsigned item,unsigned n) {
    assert(self==inventory && item==204 && n==1);++checks;return has_room;
}
static void __attribute__((thiscall)) add(void *self,unsigned item,unsigned n,unsigned show) {
    assert(self==inventory && item==204 && n==1 && show==1);++awards;
}
static void __attribute__((stdcall)) consume(void *c) {++consumed;((uint8_t *)c)[0x4c]=1;}
static void __attribute__((naked)) cap_notice(void) {__asm__ volatile("ret $4");}
typedef struct VisualFixture {
    uint8_t entity[0x430],wrapper[24],renderer[0x9c],render[0xd0],model[32],parts[16],bank[4],channels[24];
    float matrix[68];
} VisualFixture;
static void render_fixture(VisualFixture *f,unsigned vt,unsigned pos) {
    uint8_t *b=(uint8_t *)image,*e=f->entity,*p=e+pos;
    memset(f,0,sizeof(*f));
    *(void **)e=b+vt;*(void **)(e+0x44)=p;*(void **)p=b+RVA_POSITION_VTABLE;*(void **)(p+0x10)=e;
    *(void **)(p+0x8c)=f->matrix;*(void **)(p+0xb4)=f->wrapper;
    f->matrix[0]=f->matrix[5]=f->matrix[10]=f->matrix[15]=1;f->matrix[12]=10;
    *(void **)(f->wrapper+8)=f->render;*(void **)(f->wrapper+12)=f->renderer;
    *(void **)(f->wrapper+16)=f->renderer;*(void **)(f->wrapper+20)=f->render;
    *(void **)(f->render+20)=f->renderer;*(void **)f->renderer=b+RVA_RENDERER_VTABLE;
    *(void **)(f->renderer+8)=f->model;*(void **)(f->model+28)=f->parts;
    *(unsigned *)(f->parts+12)=1;*(void **)(f->renderer+0x98)=f->bank;*(void **)f->bank=f->channels;
    *(float *)(f->channels+8)=4;
}
static void named_checks(void) {
    uint8_t *b=(uint8_t *)image;
    const unsigned weak[][2]={{0x2d3e68,4},{0x2d919c,4},{0x133150,9},{0x133222,7},
        {0x13db47,10},{0x187960,8},{0x186453,6},{0x186459,4},{0x18645d,9}};
    assert(world_visual_code());
    for(unsigned n=0;n<sizeof(weak)/sizeof(weak[0]);++n) for(unsigned i=0;i<weak[n][1];++i) {
        b[weak[n][0]+i]^=1;assert(!world_visual_code());b[weak[n][0]+i]^=1;
    }
    puts("PASS named WorldVisualWeakDestruction exact bytes and all mutations");
    const unsigned pick[][2]={{0x1338a0,8},{0x1338c2,4},{0x1338c6,10},{0x1338d4,18},{0x1338eb,3},{0x133913,3}};
    assert(world_pickup_code());
    for(unsigned n=0;n<sizeof(pick)/sizeof(pick[0]);++n) for(unsigned i=0;i<pick[n][1];++i) {
        b[pick[n][0]+i]^=1;assert(!world_pickup_code());b[pick[n][0]+i]^=1;
    }
    assert(SudekiMpLanStoryInventoryInstall(image));
    assert(world_pickup_code()); /* Shared seam recognizes its actual owner. */
    b[0x1338dcu]^=1; assert(!world_pickup_code()); b[0x1338dcu]^=1;
    assert(SudekiMpLanStoryInventoryUninstall() && world_pickup_code());
    puts("PASS named WorldDropCollect native and owned observer call; foreign redirect refused");
}
int main(int argc,char **argv) {
    assert(argc==2);wchar_t path[MAX_PATH];assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,MAX_PATH));
    image=(HMODULE)map_image(path);uint8_t *b=(uint8_t *)image;named_checks();
    jump_to(RVA_WORLD_MATRIX,get_matrix);game_thread=GetCurrentThreadId();registry.session=7;registry.next_instance=20;
    VisualFixture fx,shot,drop;
    render_fixture(&fx,RVA_EFFECT_VTABLE,0x160);render_fixture(&shot,0x2d5a10,0x160);render_fixture(&drop,0x2d3e68,0x210);
    uint8_t *e=fx.entity;*(void **)(e+0x58)=e+0x270;*(void **)(e+0x270)=b+RVA_EFFECT_COMPONENT_VTABLE;*(void **)(e+0x280)=e;
    *(unsigned *)(e+0x29c)=0x29;*(unsigned *)(e+0x2a0)=0x37a3bf5f;
    uint8_t missile[0x60]={0},record[0x40]={0},monster[0x40]={0},manager[0x40]={0},items[0xfac]={0},def[0x50]={0};
    *(void **)missile=b+0x2d915c;*(void **)(missile+4)=b+0x2d919c;*(void **)(missile+16)=shot.entity;
    *(void **)(missile+0x3c)=record;*(void **)(missile+0x58)=monster;*(void **)monster=b+0x2d5fa0;
    *(void **)(shot.entity+0xfc)=missile;*(unsigned *)(record+0x24)=0x29;*(unsigned *)(record+0x28)=0x75d53da9;
    e=drop.entity;*(void **)(e+0x100)=e+0x1c0;*(void **)(e+0x1c0)=b+0x2d411c;*(void **)(e+0x1d0)=e;
    *(void **)(e+0x4c)=e+0x320;*(void **)(e+0x330)=e;*(float *)(e+0x370)=204;
    *(void **)(e+0x200)=def;*(unsigned *)(def+0x14)=204;*(unsigned *)(def+0x44)=0x29;*(unsigned *)(def+0x48)=0x50778e63;
    *(void **)(b+0x408d80)=items;*(void **)items=b+0x2c693c;*(void **)(items+12+204*4)=def;
    void *entries[]={fx.entity,shot.entity,drop.entity};*(void **)(b+0x409d8c)=manager;
    *(void **)(manager+0x3c)=entries;*(unsigned *)(manager+0x34)=3;
    SudekiMpStoryVfx s={.tick=100,.observed=1},before;
    assert(SudekiMpLanStoryWorldVisuals(7,&s) && s.count==3);
    assert(SudekiMpLanStoryCastVfxEntriesValid(s.tick,0,1,s.count,s.entries,64));
    for(unsigned i=0;i<3;++i) assert(s.entries[i].instance_sequence==21+i && s.entries[i].phase==4 && s.entries[i].position[0]==10 && s.entries[i].owner_actor_type==255);
    s.count=0;s.tick=120;shot.matrix[12]=12;
    assert(SudekiMpLanStoryWorldVisuals(7,&s) && s.entries[1].position[0]==12 && s.entries[1].instance_sequence==22);
    before=s;*(void **)(shot.entity+0xfc)=NULL;
    assert(!SudekiMpLanStoryWorldVisuals(7,&s) && !memcmp(&before,&s,sizeof(s)));*(void **)(shot.entity+0xfc)=missile;
    s.count=63;before=s;
    assert(!SudekiMpLanStoryWorldVisuals(7,&s) && !memcmp(&before,&s,sizeof(s)));
    s.count=0;fx.entity[0x3e0]=8;drop.entity[0x20c]=1;
    assert(SudekiMpLanStoryWorldVisuals(7,&s) && s.count==1 && s.entries[0].instance_sequence==22);
    fx.entity[0x3e0]=0;drop.entity[0x20c]=0;
    /* Native TPtr destruction clears our observer; the same address becomes a
     * fresh incarnation instead of inheriting an earlier visual instance. */
    ((void (__attribute__((thiscall)) *)(void *))(b+0x4d30))(missile+4);
    assert(!world_visuals[1].weak.entity);
    *(void **)(missile+4)=b+0x2d919c; /* New native constructor restores its secondary base. */
    s.count=0;
    assert(SudekiMpLanStoryWorldVisuals(7,&s) && s.count==3 && s.entries[2].instance_sequence==24);
    uint8_t actor[0x80]={0},pos[0x30]={0},stats[0x40]={0};
    *(void **)actor=b+0x2d5010;*(void **)(actor+0x44)=pos;*(void **)(actor+0x4c)=stats;
    *(void **)pos=b+RVA_POSITION_VTABLE;*(void **)(pos+0x10)=actor;*(void **)(stats+0x10)=actor;*(float *)(stats+0x2c)=100;
    *(void **)(b+0x408d84)=inventory;jump_to(0x219d0,can_add);jump_to(0x217e0,add);jump_to(0x133920,consume);jump_to(0x215e0,cap_notice);
    *(float *)(pos+0x18)=1.1f;
    assert(!SudekiMpLanStoryWorldCollectNearby(actor,35) && !checks);
    *(float *)(pos+0x18)=0;has_room=FALSE;
    assert(!SudekiMpLanStoryWorldCollectNearby(actor,35) && checks==1 && !awards && !consumed && *(float *)(drop.entity+0x208)>0);
    has_room=TRUE;assert(!SudekiMpLanStoryWorldCollectNearby(actor,35) && checks==1);
    *(float *)(drop.entity+0x208)=0;*(float *)(stats+0x2c)=0;
    assert(!SudekiMpLanStoryWorldCollectNearby(actor,35) && checks==1);*(float *)(stats+0x2c)=100;
    assert(SudekiMpLanStoryWorldCollectNearby(actor,35) && awards==1 && consumed==1);
    assert(!SudekiMpLanStoryWorldCollectNearby(actor,35) && awards==1 && consumed==1);
    s.count=0;assert(SudekiMpLanStoryWorldVisuals(7,&s) && s.count==2);
    assert(world_visual_reset());assert(!*(void **)(fx.entity+4) && !*(void **)(drop.entity+4) && !*(void **)(missile+8));
    VirtualFree(image,0,MEM_RELEASE);
    puts("PASS world effects/projectiles/drops: shared IDs, poses, native weak destruction/reuse, unknown/overflow preservation, pickup caps and exactly-once consumption");return 0;
}
