/* Exact supported-image damage gate with fixture-owned actors. The retail
 * DamageStructure constructor, float packing and DoDirectDamage run; hit effects,
 * reference-list binding and allocating cleanup are inert fixture endpoints. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "engine/build_identity.h"
#ifdef SUDEKIMP_DAMAGE_FLUSH_TEST
#include "../src/engine/build_identity.c"
#include "../src/engine/sha256.c"
#include "../src/hooks/lan_story_world.c"
static unsigned death_present_calls;
BOOL SudekiMpLanStorySpawnDeathPresent(void *actor,uint32_t id,unsigned style) {
    (void)actor;assert(id && style>=1 && style<=4);++death_present_calls;return TRUE;
}
BOOL SudekiMpLogResearchEnabled(void) { return FALSE; }
void SudekiMpLogFormat(const char *format,...) { (void)format; }
#else
#include "hooks/story_monster_damage_guard.h"
#endif
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

static uint8_t *image,*entity,*arbiter,*combat,*registry,*old_entries,*new_entries;
static SudekiMpStoryDamageGuard guard;
static SudekiMpStoryDamageIdentity identity;
static unsigned calls,mode;
static BOOL generation_matches;
static BOOL checked_memory(const void *p,size_t n,BOOL write) {
    MEMORY_BASIC_INFORMATION m;
    uintptr_t a=(uintptr_t)p;
    if(!p || !n || a+n<a || VirtualQuery(p,&m,sizeof(m))!=sizeof(m) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_NOACCESS|PAGE_GUARD)) ||
        a+n>(uintptr_t)m.BaseAddress+m.RegionSize) return FALSE;
    DWORD access=m.Protect&0xffu;
    if(write) return access==PAGE_READWRITE || access==PAGE_WRITECOPY ||
        access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
    return access==PAGE_READONLY || access==PAGE_READWRITE || access==PAGE_WRITECOPY ||
        access==PAGE_EXECUTE_READ || access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
}
static void protect(void *p,DWORD access) {
    DWORD old; assert(VirtualProtect(p,4096,access,&old));
}
static void __attribute__((thiscall)) hit(void *self,void *damage) {
    assert(self==combat);
    if(mode==20) {
        uint16_t h=*(uint16_t *)((uint8_t *)damage+0x14);
        unsigned exponent=(h>>10u)&31u;
        uint32_t bits=((h&0x8000u)<<16u)|((h&1023u)<<13u)|((exponent?exponent+112u:0u)<<23u);
        float packed; memcpy(&packed,&bits,4u);
        float *hp=(float *)(entity+0x200u+0x2cu);
        assert(isfinite(packed) && packed>=0.0f && packed<*hp);
        *hp-=packed; assert(*hp>0.0f);
    } else assert(*(uint16_t *)((uint8_t *)damage+0x14)==0x4a40); /* native half: 12.5 */
    assert(!(*(uint32_t *)(arbiter+0x50)&0x400));
    ++calls;
    *(uint32_t *)(arbiter+0x50)|=0x10000000; /* A native flag write must survive. */
    switch(mode) {
    case 1: *(uint32_t *)(arbiter+0x58)=3; break;
    case 2: *(void **)arbiter=image; break;
    case 3: *(void **)(arbiter+0x10)=registry; break;
    case 4: *(void **)(entity+0x90)=registry; break;
    case 5: *(void **)entity=image; break;
    case 6: ++*(uint32_t *)(entity+0x34); break;
    case 7: *(unsigned *)(registry+0x34)=0; break;
    case 8: generation_matches=FALSE; break;
    case 9: protect(arbiter,PAGE_READONLY); break;
    case 10: protect(arbiter,PAGE_NOACCESS); break;
    case 11: *(void **)new_entries=entity; *(void **)(registry+0x3c)=new_entries;
        protect(old_entries,PAGE_NOACCESS); break;
    case 12: *(void **)(entity+0xa4)=registry; break;
    case 13: *(void **)(combat+0x10)=registry; break;
    case 14: *(void **)(image+0x409d8c)=entity; break;
    case 15: protect(entity,PAGE_NOACCESS); break;
    case 16: *(void **)combat=image; break;
#ifdef SUDEKIMP_DAMAGE_FLUSH_TEST
    case 17:
        assert(!SudekiMpUninstallLanStoryWorld() && GetLastError()==ERROR_BUSY);
        assert(!begin());
        assert(!SudekiMpLanStoryWorldFlushMonsterDamage());
        assert(calls==1); break;
    case 18: ++client_bound[0].previous.generation; break;
#endif
    }
}
static void fixture(void) {
    protect(entity,PAGE_READWRITE); protect(arbiter,PAGE_READWRITE); protect(old_entries,PAGE_READWRITE);
    memset(entity,0,4096); memset(arbiter,0,4096); memset(combat,0,4096);
    memset(registry,0,4096); memset(&guard,0,sizeof(guard)); calls=mode=0; generation_matches=TRUE;
    *(void **)entity=image+0x2d5fa0; *(uint32_t *)(entity+0x34)=123;
    *(void **)arbiter=image+0x2cc9ac; *(void **)(arbiter+0x10)=entity;
    *(uint32_t *)(arbiter+0x50)=0x402; *(uint32_t *)(arbiter+0x58)=0xc0000012; arbiter[0x60]=2;
    *(void **)combat=image+0x2c8754; *(void **)(combat+0x10)=entity;
    *(void **)(entity+0x90)=arbiter; *(void **)(entity+0xa4)=combat;
    *(void **)old_entries=entity; *(void **)(registry+0x3c)=old_entries; *(unsigned *)(registry+0x34)=1;
    *(void **)(image+0x409d8c)=registry;
    uint8_t *stats=entity+0x200u;
    *(void **)(entity+0x4cu)=stats; *(void **)stats=image+0x2cc064u;
    *(void **)(stats+0x10u)=entity; *(float *)(stats+0x2cu)=100.0f;
    identity=(SudekiMpStoryDamageIdentity){entity,456,123,7,8};
#ifdef SUDEKIMP_DAMAGE_FLUSH_TEST
    base=image; native_thread=GetCurrentThreadId(); client_seen=TRUE; active=damage_call_active=FALSE;
    memset(&damage_restore,0,sizeof(damage_restore)); memset(monster_hp,0,sizeof(monster_hp));
    memset(feedback,0,sizeof(feedback)); memset(client_bound,0,sizeof(client_bound));
    client_epoch=identity.epoch; client_count=1;
    client_bound[0].target.entity=entity; client_bound[0].target.native_identifier=identity.native_identifier;
    client_bound[0].previous.identifier=identity.identifier; client_bound[0].previous.generation=identity.generation;
    client_bound[0].previous.kind=SUDEKIMP_LAN_STORY_WORLD_MONSTER_KIND;
    monster_hp[0].identity=identity; monster_hp[0].pending=12.5f;
#endif
}
static void execute(void) {
    assert(SudekiMpStoryDamageCapture(image,&identity,&guard,checked_memory));
    SudekiMpStoryDamageLift(&guard);
    ((SudekiMpStoryDirectDamage)(image+SUDEKIMP_RVA_DIRECT_DAMAGE))(guard.combat,12.5f);
    assert(calls==1);
}
static void endpoint(uint32_t rva,const uint8_t *head,unsigned n,void *target) {
    assert(!memcmp(image+rva,head,n));
    if(!target) image[rva]=0xc3;
    else { image[rva]=0xe9; *(int32_t *)(image+rva+1)=(intptr_t)target-(intptr_t)(image+rva+5); }
}
static void byte_checks(void) {
    const uint32_t rvas[]={SUDEKIMP_RVA_DIRECT_DAMAGE,0xd3b61,0xd3b99};
    const unsigned lengths[]={sizeof(story_direct_damage_head),sizeof(story_direct_damage_hit_call),sizeof(story_direct_damage_return)};
    for(unsigned n=0;n<3;++n) for(unsigned i=0;i<lengths[n];++i) {
        image[rvas[n]+i]^=1; assert(!SudekiMpStoryDamageCapture(image,&identity,&guard,checked_memory));
        image[rvas[n]+i]^=1;
    }
    assert(FlushInstructionCache(GetCurrentProcess(),image,0x45f000));
}
#ifdef SUDEKIMP_DAMAGE_FLUSH_TEST
static DWORD WINAPI wrong_thread(void *unused) { (void)unused; assert(!SudekiMpLanStoryWorldFlushMonsterDamage()); return 0; }
static void number_gate(void) {
    fixture(); uint8_t *stat=entity+0x400u,*settings=entity+0x800u;
    *(void **)(entity+0xb0u)=stat; *(void **)stat=image+0x2d21e4u;
    *(void **)(stat+0x10u)=entity; stat[0x24u]=stat[0x30u]=1;
    *(float *)(stat+0x18u)=-10.0f;
    *(void **)(image+0x409dc0u)=settings; *(float *)(settings+0x29cu)=3.0f;
    typedef void (__stdcall *NumberQueue)(void *,float,float,uint32_t);
    NumberQueue queue=(NumberQueue)(image+0x129800u);
    assert(!memcmp(image+0x129800u,"\x55\x8b\x6c\x24\x08\x80\x7d\x24\x00",9u));
    assert(!memcmp(image+0x12989au,"\xc2\x10\x00",3u));
    queue(stat,1.0f,1.0f,0xffffffffu); assert(!*(uint32_t *)(stat+0x48u));
    assert(!SudekiMpStoryDamageArmDisplay(image,entity,FALSE,checked_memory));
    *(void **)(stat+0x10u)=registry;
    assert(!SudekiMpStoryDamageArmDisplay(image,entity,TRUE,checked_memory));
    *(void **)(stat+0x10u)=entity; stat[0x24u]=0;
    assert(!SudekiMpStoryDamageArmDisplay(image,entity,TRUE,checked_memory));
    stat[0x24u]=1; stat[0x30u]=0;
    assert(!SudekiMpStoryDamageArmDisplay(image,entity,TRUE,checked_memory));
    stat[0x30u]=1; image[0x31f0f3u]=0;
    assert(SudekiMpStoryDamageArmDisplay(image,entity,TRUE,checked_memory));
    assert(!image[0x31f0f3u]); /* Do not change the user's number option. */
    queue(stat,1.0f,1.0f,0xffffffffu);
    assert(*(uint32_t *)(stat+0x48u)==1u && *(uint32_t *)(stat+0x70u)==0xffffffffu);
}
static unsigned hidden_widgets;
static void __attribute__((thiscall)) widget_hidden(void *widget,unsigned state) {
    assert(state==2 && *(uint32_t *)((uint8_t *)widget+0x24u)==2);++hidden_widgets;
}
static void health_cleanup(void) {
    fixture();uint8_t *stat=entity+0x400u;
    *(void **)(entity+0xb0u)=stat;*(void **)stat=image+0x2d21e4u;*(void **)(stat+0x10u)=entity;
    *(void **)(stat+0xd0u)=image+0x2d9024u;*(void **)(stat+0x12cu)=image+0x2d8524u;
    *(uint32_t *)(stat+0x48u)=1;*(int32_t *)(stat+0x74u)=-100;stat[0x24u]=stat[0x30u]=1;
    assert(SudekiMpStoryHealthHideCode(image,checked_memory));
    const unsigned rvas[]={0x15c2d0,0x15c230,0x15c2f7,0x15c26d,0x2d9038,0x2d8538};
    const unsigned sizes[]={sizeof(story_health_hide_bar_head),sizeof(story_health_hide_label_head),1,1,4,4};
    const char *names[]={"HealthBarVisibilityHead","HealthLabelVisibilityHead","HealthBarVisibilityReturn",
        "HealthLabelVisibilityReturn","HealthBarVisibilitySlot","HealthLabelVisibilitySlot"};
    for(unsigned i=0;i<6;++i) {
        for(unsigned j=0;j<sizes[i];++j) {
            image[rvas[i]+j]^=1;assert(!SudekiMpStoryHealthHideCode(image,checked_memory));image[rvas[i]+j]^=1;
        }
        printf("%s every-byte refusals PASS\n",names[i]);
    }
    /* Execute retail propagation and empty native child lists. Only renderer
     * visibility endpoints are inert; number state and gameplay stay intact. */
    void *bar=*(void **)(image+0x2d9034),*label=*(void **)(image+0x2d8534);
    *(void **)(image+0x2d9034)=widget_hidden;*(void **)(image+0x2d8534)=widget_hidden;
    assert(FlushInstructionCache(GetCurrentProcess(),image,0x45f000));
    assert(SudekiMpStoryDamageHideHealth(image,entity,checked_memory));assert(hidden_widgets==2);
    assert(*(uint32_t *)(stat+0x48)==1 && *(int32_t *)(stat+0x74)==-100 && stat[0x24] && stat[0x30]);
    assert(*(float *)(entity+0x22c)==100 && *(uint32_t *)(arbiter+0x50)==0x402);
    assert(SudekiMpStoryDamageHideHealth(image,entity,checked_memory));assert(hidden_widgets==2);
    *(void **)(stat+0x10)=registry;assert(!SudekiMpStoryDamageHideHealth(image,entity,checked_memory));
    *(void **)(stat+0x10)=entity;*(void **)(stat+0xd0)=image;
    assert(!SudekiMpStoryDamageHideHealth(image,entity,checked_memory));
    *(void **)(image+0x2d9034)=bar;*(void **)(image+0x2d8534)=label;
    fixture();monster_hp[0].pending=0;monster_hp[0].death_effect=4;
    assert(SudekiMpLanStoryWorldMonsterDamagePending());
    assert(SudekiMpLanStoryWorldFlushMonsterDamage() && death_present_calls==1 && !calls);
    assert(!SudekiMpLanStoryWorldMonsterDamagePending());
    assert(!SudekiMpLanStoryWorldFlushMonsterDamage() && death_present_calls==1);
    puts("Death health cleanup preserves lethal numbers, HP, pause and independent emission PASS");
}
static void flush_cases(void) {
    health_cleanup();
    number_gate();
    fixture(); mode=20;
    for(unsigned i=0;i<30u;++i) {
        monster_hp[0].pending=100000000.0f;
        assert(SudekiMpLanStoryWorldFlushMonsterDamage());
        assert(*(float *)(entity+0x200u+0x2cu)>0.0f);
    }
    assert(SudekiMpStoryNonlethalDamage(NAN,10.0f)==0 &&
        SudekiMpStoryNonlethalDamage(100.0f,INFINITY)==0 && SudekiMpStoryNonlethalDamage(0,10.0f)==0);

    fixture(); active=TRUE; assert(!SudekiMpLanStoryWorldFlushMonsterDamage()); assert(!calls); active=FALSE;
    HANDLE thread=CreateThread(NULL,0,wrong_thread,NULL,0,NULL); assert(thread);
    assert(WaitForSingleObject(thread,5000)==WAIT_OBJECT_0); CloseHandle(thread); assert(!calls);
    mode=17; assert(SudekiMpLanStoryWorldFlushMonsterDamage());
    assert(calls==1 && !damage_restore.lifted && feedback[0].entity==entity);
    assert(*(uint32_t *)(arbiter+0x50)==0x10000402);
    fixture(); mode=18; feedback[0].entity=entity; assert(SudekiMpLanStoryWorldFlushMonsterDamage());
    assert(calls==1 && !damage_restore.lifted && !feedback[0].entity);
    assert(*(uint32_t *)(arbiter+0x50)==0x10000002);
    fixture(); mode=9; assert(SudekiMpLanStoryWorldFlushMonsterDamage());
    assert(calls==1 && damage_restore.lifted && !feedback[0].entity);
    assert(!SudekiMpUninstallLanStoryWorld() && base==image);
    protect(arbiter,PAGE_READWRITE); assert(SudekiMpUninstallLanStoryWorld() && !base);
    assert(*(uint32_t *)(arbiter+0x50)==0x10000402);
    fixture(); mode=15; assert(SudekiMpLanStoryWorldFlushMonsterDamage());
    assert(calls==1 && damage_restore.lifted && !feedback[0].entity);
    protect(entity,PAGE_READWRITE); assert(!SudekiMpLanStoryWorldFlushMonsterDamage()); assert(!damage_restore.lifted && calls==1);
    fixture(); ++client_bound[0].previous.generation; assert(!SudekiMpLanStoryWorldFlushMonsterDamage()); assert(!calls);
}
#endif
int main(int argc,char **argv) {
    assert(argc==2); wchar_t path[1024]; assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024));
    image=map_image(path);
    uint8_t **buffers[]={&entity,&arbiter,&combat,&registry,&old_entries,&new_entries};
    for(unsigned i=0;i<sizeof(buffers)/sizeof(buffers[0]);++i) {
        *buffers[i]=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE); assert(*buffers[i]);
    }
    assert(*(void **)(image+0x2cc9ac)==image+0xdc3c0);
    endpoint(0x1750,(const uint8_t[]){0x8b,0x08,0x85,0xc9},4,NULL);
    endpoint(0x15e0,(const uint8_t[]){0x8b,0x01,0x33,0xd2},4,NULL);
    endpoint(0xcc8d0,(const uint8_t[]){0x56,0x8b,0xf1,0x57},4,NULL);
    endpoint(0xd21d0,(const uint8_t[]){0x55,0x8b,0xec},3,(void *)(uintptr_t)hit);
    assert(FlushInstructionCache(GetCurrentProcess(),image,0x45f000));
    fixture(); byte_checks();
    /* Retail gate rejects 0x400; the guarded call reaches the hit endpoint. */
    ((SudekiMpStoryDirectDamage)(image+SUDEKIMP_RVA_DIRECT_DAMAGE))(combat,12.5f); assert(!calls);
    execute(); assert(SudekiMpStoryDamageRestore(image,&guard,TRUE,checked_memory)==SUDEKIMP_DAMAGE_EXACT);
    assert(!guard.lifted && *(uint32_t *)(arbiter+0x50)==0x10000402);
    for(unsigned m=1;m<=16;++m) {
        fixture(); mode=m; execute();
        int result=SudekiMpStoryDamageRestore(image,&guard,generation_matches,checked_memory);
        if(m==9 || m==10 || m==15) {
            assert(result==SUDEKIMP_DAMAGE_UNKNOWN && guard.lifted);
            protect(arbiter,PAGE_READWRITE); protect(entity,PAGE_READWRITE);
            assert(SudekiMpStoryDamageRestore(image,&guard,TRUE,checked_memory)==SUDEKIMP_DAMAGE_EXACT);
            assert(*(uint32_t *)(arbiter+0x50)==0x10000402);
        } else if(m==1 || m==11) {
            assert(result==SUDEKIMP_DAMAGE_EXACT && !guard.lifted);
            assert(*(uint32_t *)(arbiter+0x50)==(m==1?0x10000002u:0x10000402u));
        } else {
            assert(result==SUDEKIMP_DAMAGE_RETIRED && !guard.lifted);
            assert(*(uint32_t *)(arbiter+0x50)==0x10000002);
        }
    }
    fixture(); *(uint32_t *)(arbiter+0x50)=2; execute();
    assert(!guard.lifted && SudekiMpStoryDamageRestore(image,&guard,TRUE,checked_memory)==SUDEKIMP_DAMAGE_EXACT);
    assert(*(uint32_t *)(arbiter+0x50)==0x10000002);
    fixture(); *(void **)arbiter=image; assert(!SudekiMpStoryDamageCapture(image,&identity,&guard,checked_memory));
    fixture(); *(void **)(arbiter+0x10)=combat; assert(!SudekiMpStoryDamageCapture(image,&identity,&guard,checked_memory));
    fixture(); ++identity.native_identifier; assert(!SudekiMpStoryDamageCapture(image,&identity,&guard,checked_memory));
    fixture(); protect(arbiter,PAGE_READONLY); assert(!SudekiMpStoryDamageCapture(image,&identity,&guard,checked_memory));
#ifdef SUDEKIMP_DAMAGE_FLUSH_TEST
    flush_cases();
    puts("StoryMonsterDamageFlushImageTest PASS");
#else
    puts("StoryMonsterDamageGuardImageTest PASS");
#endif
    return 0;
}
