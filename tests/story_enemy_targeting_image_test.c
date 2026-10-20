/* Supported-image contracts and synthetic native calls, no live gameplay. */
#include <windows.h>
static BOOL clock_forced;
static DWORD clock_tick;
static DWORD test_tick(void) {return clock_forced?clock_tick:GetTickCount();}
#define GetTickCount test_tick
#include "../src/hooks/story_enemy_targeting.c"
#undef GetTickCount
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
static uint8_t *image;
static BOOL proof=TRUE,lease_known=TRUE,lease_departed,spirit_fixture;
static BOOL control_proof=TRUE,session_proof=TRUE,noncap_allowed=TRUE;
static BOOL away_proof=TRUE;
static BOOL companion_proof=TRUE;
BOOL SudekiMpLanStoryRuntimeCompanionSeatExact(unsigned c) {return companion_proof && c==2;}
BOOL SudekiMpLanStoryRuntimeAwaySeatExact(unsigned c,const SudekiMpLanPartyLease *key) {
    return away_proof && c==0 && key && key->token==77 && key->generation==3 && key->seat==1;
}
static unsigned control_character;static void *controlled_actor;
BOOL SudekiMpLanPartyControlStoryTurnOwner(void *actor,SudekiMpLanPartyLease *key) {
    memset(key,0,sizeof(*key));key->seat=(uint8_t)control_character;
    return control_proof && actor==controlled_actor;
}
BOOL SudekiMpLanStoryRuntimeTurnLeaseExact(const SudekiMpLanPartyLease *key) {
    (void)key;return session_proof;
}
static void *leased_actor;static uint32_t leased_serial=7;
static BOOL multiple_leases;
static SudekiMpStorySpawnLeaseObservation test_leases[128];
static unsigned test_lease_count;
static char last_log[2048],journal[2048][2048];
static unsigned journal_count;
static void log_append(void) {assert(journal_count<2048);strcpy(journal[journal_count++],last_log);}
static void log_reset(void) {journal_count=0;last_log[0]=0;}
static unsigned log_count(const char *text) {
    unsigned n=0;for(unsigned i=0;i<journal_count;++i) n+=strstr(journal[i],text)!=NULL;return n;
}
void SudekiMpLogWrite(const char *line) {snprintf(last_log,sizeof(last_log),"%s",line);log_append();}
void SudekiMpLogFormat(const char *fmt,...) {va_list ap;va_start(ap,fmt);vsnprintf(last_log,sizeof(last_log),fmt,ap);va_end(ap);log_append();}
BOOL SudekiMpLanStoryObserverNativeRosterExact(const SudekiMpLanStoryNativeRoster *r) {(void)r;return proof;}
static unsigned same_area_mask=15;
int SudekiMpLanStorySceneSameArea(const SudekiMpLanStoryScene *s,unsigned a,unsigned b) {(void)s;(void)a;return (same_area_mask&(1u<<b))!=0;}
BOOL SudekiMpLanStorySpawnLeasesObserve(SudekiMpStorySpawnLeaseObservation *o,unsigned c,unsigned *n) {
    if(!lease_known) return FALSE;
    if(multiple_leases) {
        if(test_lease_count>c) return FALSE;
        memcpy(o,test_leases,test_lease_count*sizeof(*o));*n=test_lease_count;return TRUE;
    }
    *n=leased_actor && c?1:0;
    if(*n) {memset(o,0,sizeof(*o));o->serial=leased_serial;o->actor=leased_actor;o->departed=lease_departed;}
    return TRUE;
}
static uint32_t shot_sequences[4],shot_ticks[4];
BOOL SudekiMpLanStoryShotsLatest(unsigned c,uint32_t *s,uint32_t *t) {*s=shot_sequences[c];*t=shot_ticks[c];return TRUE;}
BOOL SudekiMpLanStoryCastSpiritInFlight(unsigned c,BOOL *a) {(void)c;*a=spirit_fixture;return TRUE;}
BOOL SudekiMpLanPartyProjectileDamageMatch(void *a,void *d,BOOL *m) {(void)a;(void)d;*m=FALSE;return TRUE;}
static unsigned install_call,fail_install;
BOOL __real_SudekiMpInstallInlineHook(SudekiMpInlineHook *,uint8_t *,const uint8_t *,size_t,const void *);
BOOL __wrap_SudekiMpInstallInlineHook(SudekiMpInlineHook *h,uint8_t *p,const uint8_t *b,size_t n,const void *fn) {
    if(++install_call==fail_install) {SetLastError(ERROR_NOT_ENOUGH_MEMORY);return FALSE;}
    return __real_SudekiMpInstallInlineHook(h,p,b,n,fn);
}
BOOL __real_SudekiMpInstallRelativeCallHook(SudekiMpRelativeCallHook *,uint8_t *,const void *,const void *);
BOOL __wrap_SudekiMpInstallRelativeCallHook(SudekiMpRelativeCallHook *h,uint8_t *p,const void *o,const void *fn) {
    if(++install_call==fail_install) {SetLastError(ERROR_NOT_ENOUGH_MEMORY);return FALSE;}
    return __real_SudekiMpInstallRelativeCallHook(h,p,o,fn);
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


static uint8_t actor[0x150],ai[0x170],candidate[0x170],party[0x150],mode[0x10],pos[0x80];
static uint8_t manager[0x100],list[24],request[0x28],party_arb[0x64];
static unsigned original_calls;
static void *native_pick;
static void * __stdcall synthetic_select(void *r,void **out,void *self,float range) {
    assert(r==request && self==ai && range==30);++original_calls;
    *out=native_pick;return out;
}
static unsigned lead_count;
static void *lead_candidates[4];
static void * __stdcall synthetic_lead_list(void *r,void **out,void *self,float range) {
    assert(r==request && self==ai && range==30);
    *(unsigned *)(image+0x3c30ac)=lead_count;
    for(unsigned i=0;i<lead_count && i<4;++i)
        *(uintptr_t *)(image+0x3d5540+12*i)=(uintptr_t)lead_candidates[i]+4;
    *out=native_pick;return out;
}
static void *noncap_candidates[2],*noncap_denied;
static unsigned noncap_calls;
static unsigned __stdcall noncap(void *r,void *s,float f,void *c) {
    (void)r;(void)s;(void)f;++noncap_calls;
    return noncap_allowed && c!=noncap_denied && c && (!noncap_candidates[0] || c==noncap_candidates[0] || c==noncap_candidates[1]);
}
static void jump(unsigned rva,const void *fn) {
    image[rva]=0xe9;int32_t d=(int32_t)((const uint8_t *)fn-image-rva-5);memcpy(image+rva+1,&d,4);
    FlushInstructionCache(GetCurrentProcess(),image+rva,5);
}
static BOOL consumer(void *c,void *p,BOOL after,void *storage) {(void)c;(void)p;(void)after;(void)storage;return TRUE;}
static DWORD WINAPI wrong_thread(void *unused) {
    (void)unused;assert(!SudekiMpStoryEnemyTargetingUninstall());
    assert(!SudekiMpStoryEnemyTargetingBindThread(GetCurrentThreadId()));
    assert(!SudekiMpStoryEnemyTargetingBindThread(thread));
    assert(!SudekiMpLanHitObserverBindThread(GetCurrentThreadId()));
    assert(!SudekiMpLanHitObserverBindThread(thread));
    assert(!SudekiMpLanHitObserverSubscribe(consumer));return 0;
}
static void contracts(void) {
    assert(SudekiMpStoryEnemyTargetingImageExact(image));
    for(unsigned k=0;k<sizeof(exact_ranges)/sizeof(exact_ranges[0]);++k) for(unsigned i=0;i<exact_ranges[k].size;++i) {
        unsigned at=exact_ranges[k].rva+i;
        image[at]^=1;assert(!SudekiMpStoryEnemyTargetingImageExact(image));
        assert(!SudekiMpStoryEnemyTargetingInstall((HMODULE)image));image[at]^=1;
    }
    for(unsigned i=0;i<24;++i) for(unsigned j=0;j<5;++j) {
        image[callers[i]+j]^=1;assert(!SudekiMpStoryEnemyTargetingImageExact(image));image[callers[i]+j]^=1;
    }
    const unsigned vt[]={0x2dc6a8,0x2dc880,0x2dc7b0,0x2dc748,0x2dc814,0x2dc710};
    for(unsigned i=0;i<6;++i) {
        image[vt[i]+0x2c]^=1;assert(!SudekiMpStoryEnemyTargetingImageExact(image));image[vt[i]+0x2c]^=1;
    }
    for(fail_install=1;fail_install<=5;++fail_install) {
        install_call=0;assert(!SudekiMpStoryEnemyTargetingInstall((HMODULE)image));
        assert(!base && SudekiMpLanHitObserverVacant() && SudekiMpStoryEnemyTargetingImageExact(image));
    }
    fail_install=0;
}
static void fixture(void) {
    *(void **)actor=image+0x2d5fa0;*(void **)(actor+0x94)=ai;*(void **)(actor+0x74)=pos;
    *(void **)(pos+0x10)=actor;
    *(void **)ai=image+0x2d4924;*(void **)(ai+0x10)=actor;*(uint32_t *)(ai+0x148)=1;
    *(void **)(ai+0x3c)=mode;*(void **)candidate=image+0x2d4924;
    *(void **)(candidate+0x3c)=mode;*(void **)(candidate+0x10)=party;*(uint32_t *)(candidate+0x148)=5;
    *(void **)(party+0x94)=candidate;
    *(void **)(party+0x90)=party_arb;*(void **)party_arb=image+0x2cc9ac;
    *(void **)(party_arb+0x10)=party;
    controlled_actor=party;control_character=0;
    seat_inputs[0]=(SudekiMpStoryThreatSeat){.connected=1,.ready=1,.receipt=GetTickCount(),.max_age=250};
    *(void **)(image+0x409de4)=manager;*(void **)(manager+0xf0)=list;*(unsigned *)(manager+0xe8)=1;
    *(void **)list=ai+4;
    roster=(SudekiMpLanStoryNativeRoster){.epoch=1,.available_mask=1,.leader_character=0};
    roster.ai[0]=candidate;roster.actors[0]=party;kinds[0]=STORY_SEAT_REMOTE;
    admitted=cooperative=TRUE;sampled_at=GetTickCount();
}
static void native_calls(void) {
    fixture();uint8_t backup[5];memcpy(backup,image+0x1b6ec0,5);jump(0x1b6ec0,noncap);
    request[0x24]=14;request[0x26]=request[0x27]=3;
    *(void **)(ai+0xc0)=candidate+4;
    *(int16_t *)(candidate+0x16a)=1;
    void *out=NULL;SelectFn select=(SelectFn)(image+SELECTOR);
    /* Poison stale list: mode E must never consume it. Actual native E
     * selector and cap filter execute with the non-cap predicate stubbed. */
    *(unsigned *)(image+0x3c30ac)=0xffffffff;
    for(unsigned i=0;i<1000;++i) {sampled_at=GetTickCount();assert(select(request,&out,ai,30)==&out && out==candidate);}
    candidate[0x47]=2;out=candidate;assert(select(request,&out,ai,30)==&out && out==NULL);
    assert(counters[0].cap_only==1); /* real ECX S2 path, no cap parity */
    assert(*(int16_t *)(candidate+0x16a)==1 && candidate[0x47]==2 && *(void **)(ai+0xc0)==candidate+4);
    candidate[0x47]=0;mode[0xb]=1;assert(select(request,&out,ai,30)==&out && out==candidate);mode[0xb]=0;
    /* Both instructions and their JG/+B continuations preserve native flags. */
    candidate[0x47]=0;*(uint16_t *)(candidate+0x46)=0x1000;
    assert(select(request,&out,ai,30)==&out && !out);
    *(int16_t *)(candidate+0x16a)=0;assert(select(request,&out,ai,30)==&out && out==candidate);
    *(uint16_t *)(candidate+0x46)=0;memcpy(image+0x1b6ec0,backup,5);
    FlushInstructionCache(GetCurrentProcess(),image+0x1b6ec0,5);
    void *real=seams[0].trampoline;seams[0].trampoline=(void *)synthetic_select;
    for(unsigned mode_id=0;mode_id<16;++mode_id) {
        request[0x24]=(uint8_t)mode_id;native_pick=candidate;
        assert(select(request,&out,ai,30)==&out && out==candidate);
        native_pick=NULL;assert(select(request,&out,ai,30)==&out && !out);
    }
    memset(counters,0,sizeof(counters));cooperative=FALSE;
    native_pick=candidate;assert(select(request,&out,ai,30)==&out && out==candidate && !counters[0].count);
    cooperative=TRUE;proof=FALSE;assert(select(request,&out,ai,30)==&out && out==candidate && !counters[0].count);proof=TRUE;
    config.level=STORY_TARGET_OFF;effective_level=SudekiMpStoryThreatEffectiveLevel(&config);assert(select(request,&out,ai,30)==&out && out==candidate && !counters[0].count);
    config.level=STORY_TARGET_WATCH;effective_level=SudekiMpStoryThreatEffectiveLevel(&config);
    for(unsigned type=4;type<=6;++type) {*(uint32_t *)(ai+0x148)=type;assert(select(request,&out,ai,30)==&out && out==candidate && !counters[0].count);}
    seams[0].trampoline=real;
}
static void expect_cap_at(int allowed,unsigned line) {
    sampled_at=GetTickCount();void *out=(void *)1;
    assert(((SelectFn)(image+SELECTOR))(request,&out,ai,30)==&out);
    if(out!=(allowed?candidate:NULL)) fprintf(stderr,"cap expectation line=%u expected=%d got=%p\n",line,allowed,out);
    assert(out==(allowed?candidate:NULL));
}
#define expect_cap(allowed) expect_cap_at(allowed,__LINE__)
static DWORD WINAPI wrong_thread_cap(void *unused) {(void)unused;expect_cap(0);return 0;}
static void parity_tests(void) {
    fixture();uint8_t backup[5];memcpy(backup,image+0x1b6ec0,5);jump(0x1b6ec0,noncap);
    request[0x24]=14;request[0x26]=request[0x27]=3;
    *(void **)(ai+0xc0)=candidate+4;*(int16_t *)(candidate+0x16a)=1;
    *(uint16_t *)(candidate+0x46)=0x200; /* two melee targeters, including self */
    ai[0x46]=0;mode[0xb]=0;
    config.level=STORY_TARGET_TIER0;effective_level=SudekiMpStoryThreatEffectiveLevel(&config);config.cap_parity=1;
    uint8_t saved_ai[sizeof(ai)],saved_candidate[sizeof(candidate)],saved_request[sizeof(request)];
    memcpy(saved_ai,ai,sizeof(ai));memcpy(saved_candidate,candidate,sizeof(candidate));memcpy(saved_request,request,sizeof(request));
    expect_cap(1); /* Previously forced to 1 and rejected; authored 3 admits. */
    assert(!memcmp(saved_ai,ai,sizeof(ai)) && !memcmp(saved_candidate,candidate,sizeof(candidate)) &&
        !memcmp(saved_request,request,sizeof(request)));
    assert(counters[0].cap_parity[0]);
    for(unsigned kind=0;kind<=STORY_SEAT_HOST;++kind) {kinds[0]=kind;expect_cap(kind==STORY_SEAT_REMOTE);}
    kinds[0]=STORY_SEAT_REMOTE;
    config.level=STORY_TARGET_WATCH;effective_level=SudekiMpStoryThreatEffectiveLevel(&config);expect_cap(0);
    config.level=STORY_TARGET_OFF;effective_level=SudekiMpStoryThreatEffectiveLevel(&config);expect_cap(0);config.level=STORY_TARGET_TIER0;effective_level=SudekiMpStoryThreatEffectiveLevel(&config);
    config.cap_parity=0;expect_cap(0);config.cap_parity=1;
    cooperative=FALSE;expect_cap(0);cooperative=TRUE;
    admitted=FALSE;expect_cap(0);admitted=TRUE;
    proof=FALSE;expect_cap(0);proof=TRUE;
    control_proof=FALSE;expect_cap(0);control_proof=TRUE;
    session_proof=FALSE;expect_cap(0);session_proof=TRUE;
    control_character=1;expect_cap(0);control_character=0;
    for(unsigned type=1;type<=6;++type) {
        *(uint32_t *)(ai+0x148)=type;*(void **)actor=image+(type==3?0x2d69ec:0x2d5fa0);
        expect_cap(type<=3);
    }
    *(uint32_t *)(ai+0x148)=1;*(void **)actor=image+0x2d5fa0;
    *(unsigned *)(manager+0xe8)=0;expect_cap(0);*(unsigned *)(manager+0xe8)=1;
    *(void **)(candidate+0x10)=actor;expect_cap(0);*(void **)(candidate+0x10)=party;
    *(void **)(party+0x94)=NULL;expect_cap(0);*(void **)(party+0x94)=candidate;
    *(uint32_t *)(candidate+0x148)=6;expect_cap(0);*(uint32_t *)(candidate+0x148)=5;
    *(void **)candidate=NULL;expect_cap(0);*(void **)candidate=image+0x2d4924;
    *(void **)party_arb=NULL;expect_cap(0);*(void **)party_arb=image+0x2cc9ac;
    *(uint32_t *)(party_arb+0x50)=0x10000;expect_cap(0);*(uint32_t *)(party_arb+0x50)=0;
    candidate[0x44]=0x58;expect_cap(0);candidate[0x44]=0;
    *(int16_t *)(candidate+0x16a)=2;expect_cap(0);*(int16_t *)(candidate+0x16a)=1;
    mode[0xb]=1;expect_cap(0);mode[0xb]=0;
    seat_inputs[0].receipt=GetTickCount()-251;expect_cap(0);seat_inputs[0].receipt=GetTickCount();
    seat_inputs[0].held=1;expect_cap(0);seat_inputs[0].held=0;
    seat_inputs[0].draining=1;expect_cap(0);seat_inputs[0].draining=0;
    seat_inputs[0].away=1;expect_cap(0);seat_inputs[0].away=0;
    seat_inputs[0].owned_ai=1;expect_cap(0);seat_inputs[0].owned_ai=0;
    seat_inputs[0].absent=1;expect_cap(0);seat_inputs[0].absent=0;
    seat_inputs[0].ready=0;expect_cap(0);seat_inputs[0].ready=1;
    /* Exclusion by displayed name OR resource, even when the name is unknown. */
    uint32_t name_record[2]={0,(uint32_t)(uintptr_t)"MON_GuardSparPartner_Melee"};
    *(void **)(actor+0x38)=name_record;expect_cap(0);*(void **)(actor+0x38)=NULL;
    strcpy(config.excluded,"AB");*(uint32_t *)(actor+0x34)=4290;expect_cap(0);
    *(uint32_t *)(actor+0x34)=0;strcpy(config.excluded,"MON_GuardSparPartner_Melee");
    noncap_allowed=FALSE;expect_cap(0);noncap_allowed=TRUE;
    HANDLE thread=CreateThread(NULL,0,wrong_thread_cap,NULL,0,NULL);assert(thread);
    WaitForSingleObject(thread,INFINITE);CloseHandle(thread);
    expect_cap(1);
    /* Both authored caps and the native self-discount still apply. */
    candidate[0x47]=3;expect_cap(1);candidate[0x47]=4;expect_cap(0);
    candidate[0x47]=2;request[0x26]=1;expect_cap(0);
    request[0x26]=0xff;candidate[0x47]=15;expect_cap(1);
    request[0x26]=3;*(uint16_t *)(candidate+0x46)=0x2000;expect_cap(1);
    *(uint16_t *)(candidate+0x46)=0x3000;expect_cap(0);
    ai[0x46]=1;expect_cap(1);*(uint16_t *)(candidate+0x46)=0x4000;expect_cap(0);
    request[0x27]=0xff;*(uint16_t *)(candidate+0x46)=0xf000;expect_cap(1);
    request[0x27]=0;expect_cap(0);request[0x27]=3;
    /* Removing our selector context makes direct native filter users inert. */
    *(uint16_t *)(candidate+0x46)=0x2000;
    typedef unsigned (__stdcall *FilterFn)(void *,void *,float,void *);
    assert(!(((FilterFn)(image+FILTER))(request,ai,30,candidate)&0xff));
    memcpy(image+0x1b6ec0,backup,5);FlushInstructionCache(GetCurrentProcess(),image+0x1b6ec0,5);
    config.level=STORY_TARGET_WATCH;effective_level=SudekiMpStoryThreatEffectiveLevel(&config);ai[0x46]=0;*(uint16_t *)(candidate+0x46)=0;
}
static void away_parity_tests(void) {
    fixture();uint8_t backup[5];memcpy(backup,image+0x1b6ec0,5);jump(0x1b6ec0,noncap);
    request[0x24]=14;request[0x26]=request[0x27]=3;
    *(void **)(ai+0xc0)=candidate+4;ai[0x46]=0;
    *(int16_t *)(candidate+0x16a)=0;mode[0xb]=1;
    kinds[0]=STORY_SEAT_AWAY;
    seat_inputs[0]=(SudekiMpStoryThreatSeat){.away=1,.connected=1,.native_ai=1,.owned_ai=1};
    away_connections[0]=(SudekiMpLanPartyLease){.token=77,.generation=3,.seat=1};
    *(uint16_t *)(candidate+0x46)=0x200;
    config.level=STORY_TARGET_TIER0;effective_level=SudekiMpStoryThreatEffectiveLevel(&config);config.cap_parity=1;
    uint8_t saved_ai[sizeof(ai)],saved_candidate[sizeof(candidate)],saved_request[sizeof(request)];
    memcpy(saved_ai,ai,sizeof(ai));memcpy(saved_candidate,candidate,sizeof(candidate));memcpy(saved_request,request,sizeof(request));
    expect_cap(1); /* Zero override alone was insufficient: mode 1 forced cap 1. */
    assert(!memcmp(saved_ai,ai,sizeof(ai)) && !memcmp(saved_candidate,candidate,sizeof(candidate)) &&
        !memcmp(saved_request,request,sizeof(request)));
    memset(counters,0,sizeof(counters));config.level=STORY_TARGET_WATCH;effective_level=SudekiMpStoryThreatEffectiveLevel(&config);expect_cap(0);
    assert(counters[0].cap_only==1 && counters[0].cap_refusals[0]==1);
    config.level=STORY_TARGET_OFF;effective_level=SudekiMpStoryThreatEffectiveLevel(&config);expect_cap(0);
    config.level=STORY_TARGET_TIER0;effective_level=SudekiMpStoryThreatEffectiveLevel(&config);config.cap_parity=0;expect_cap(0);config.cap_parity=1;
    cooperative=FALSE;expect_cap(0);cooperative=TRUE;
    away_proof=FALSE;expect_cap(0);away_proof=TRUE;
    ++away_connections[0].generation;expect_cap(0);--away_connections[0].generation;
    away_connections[0].token=0;expect_cap(0);away_connections[0].token=77;
    seat_inputs[0].connected=0;expect_cap(0);seat_inputs[0].connected=1;
    seat_inputs[0].away=0;expect_cap(0);seat_inputs[0].away=1;
    seat_inputs[0].absent=1;expect_cap(0);seat_inputs[0].absent=0;
    seat_inputs[0].down=1;expect_cap(0);seat_inputs[0].down=0;
    kinds[0]=STORY_SEAT_AI;expect_cap(0);kinds[0]=STORY_SEAT_AWAY;
    *(int16_t *)(candidate+0x16a)=1;expect_cap(0);*(int16_t *)(candidate+0x16a)=0;
    mode[0xb]=0;assert(!away_cap_lease(candidate,0));mode[0xb]=1;
    *(uint32_t *)(party_arb+0x50)=0x10000;expect_cap(0);*(uint32_t *)(party_arb+0x50)=0;
    candidate[0x44]=0x58;expect_cap(0);candidate[0x44]=0;
    proof=FALSE;expect_cap(0);proof=TRUE;
    noncap_allowed=FALSE;expect_cap(0);noncap_allowed=TRUE;
    HANDLE t=CreateThread(NULL,0,wrong_thread_cap,NULL,0,NULL);assert(t);
    WaitForSingleObject(t,INFINITE);CloseHandle(t);
    /* Authored limits and both native existing-target discounts remain. */
    candidate[0x47]=3;expect_cap(1);candidate[0x47]=4;expect_cap(0);
    request[0x26]=0xff;candidate[0x47]=15;expect_cap(1);
    request[0x26]=0;expect_cap(0);request[0x26]=3;
    *(uint16_t *)(candidate+0x46)=0x2000;expect_cap(1);
    *(uint16_t *)(candidate+0x46)=0x3000;expect_cap(0);
    ai[0x46]=1;expect_cap(1);*(uint16_t *)(candidate+0x46)=0x4000;expect_cap(0);
    request[0x27]=0xff;*(uint16_t *)(candidate+0x46)=0xf000;expect_cap(1);
    request[0x27]=0;expect_cap(0);request[0x27]=3;
    *(uint16_t *)(candidate+0x46)=0x2000;
    typedef unsigned (__stdcall *FilterFn)(void *,void *,float,void *);
    assert(!(((FilterFn)(image+FILTER))(request,ai,30,candidate)&0xff));
    /* The same body is still eligible after an exact human control return. */
    mode[0xb]=0;*(int16_t *)(candidate+0x16a)=1;kinds[0]=STORY_SEAT_REMOTE;
    seat_inputs[0]=(SudekiMpStoryThreatSeat){.connected=1,.ready=1,.receipt=GetTickCount(),.max_age=250};
    expect_cap(1);
    memcpy(image+0x1b6ec0,backup,5);FlushInstructionCache(GetCurrentProcess(),image+0x1b6ec0,5);
    config.level=STORY_TARGET_WATCH;effective_level=SudekiMpStoryThreatEffectiveLevel(&config);ai[0x46]=0;*(uint16_t *)(candidate+0x46)=0;
    memset(away_connections,0,sizeof(away_connections));
}
static void lead_selection_tests(void) {
    fixture();uint8_t backup[5];memcpy(backup,image+0x1b6ec0,5);jump(0x1b6ec0,noncap);
    uint8_t ai2[0x170]={0},party2[0x150]={0},arb2[0x64]={0},pos2[0x80]={0},pos1[0x80]={0},mode2[0x10]={0};
    *(void **)ai2=image+0x2d4924;*(void **)(ai2+0x10)=party2;*(uint32_t *)(ai2+0x148)=5;
    *(void **)(ai2+0x3c)=mode2;*(int16_t *)(ai2+0x16a)=1;
    *(void **)(party2+0x94)=ai2;*(void **)(party2+0x90)=arb2;*(void **)(party2+0x74)=pos2;
    *(void **)arb2=image+0x2cc9ac;*(void **)(arb2+0x10)=party2;
    *(void **)(pos2+0x10)=party2;*(float *)(pos2+0x44)=3;
    *(void **)(party+0x74)=pos1;*(void **)(pos1+0x10)=party;*(float *)(pos1+0x44)=10;
    *(float *)(pos+0x44)=*(float *)(pos+0x4c)=0;
    *(uint16_t *)(candidate+0x46)=0;*(int16_t *)(candidate+0x16a)=0;mode[0xb]=0;
    *(void **)(ai+0xc0)=candidate+4;
    roster.available_mask=3;roster.ai[1]=ai2;roster.actors[1]=party2;
    controlled_actor=party2;control_character=1;
    kinds[0]=STORY_SEAT_HOST;kinds[1]=STORY_SEAT_REMOTE;
    seat_inputs[0]=(SudekiMpStoryThreatSeat){.host=1};
    seat_inputs[1]=(SudekiMpStoryThreatSeat){.connected=1,.ready=1,.receipt=GetTickCount(),.max_age=250};
    request[0x24]=11;request[0x26]=request[0x27]=3;
    config.level=STORY_TARGET_TIER0;effective_level=SudekiMpStoryThreatEffectiveLevel(&config);config.lead_always=config.cap_parity=1;
    lead_candidates[0]=candidate;lead_candidates[1]=ai2;lead_count=2;native_pick=candidate;
    void *real=seams[0].trampoline;seams[0].trampoline=(void *)synthetic_lead_list;
    SelectFn select=(SelectFn)(image+SELECTOR);void *out=NULL;
#define LEAD_EXPECT(expected) do {sampled_at=test_tick();seat_inputs[1].receipt=sampled_at; \
    assert(select(request,&out,ai,30)==&out && out==(expected));} while(0)
    uint8_t saved_self[sizeof(ai)],saved_remote[sizeof(ai2)],saved_request[sizeof(request)];
    memcpy(saved_self,ai,sizeof(ai));memcpy(saved_remote,ai2,sizeof(ai2));memcpy(saved_request,request,sizeof(request));
    LEAD_EXPECT(ai2);assert(lead_substitutions);
    assert(!memcmp(saved_self,ai,sizeof(ai)) && !memcmp(saved_remote,ai2,sizeof(ai2)) &&
        !memcmp(saved_request,request,sizeof(request)));
    *(float *)(pos2+0x44)=9;LEAD_EXPECT(candidate);*(float *)(pos2+0x44)=3;
    for(unsigned m=0;m<16;++m) {request[0x24]=(uint8_t)m;LEAD_EXPECT(m==11?ai2:candidate);}
    request[0x24]=11;
    config.level=STORY_TARGET_WATCH;effective_level=SudekiMpStoryThreatEffectiveLevel(&config);LEAD_EXPECT(candidate);config.level=STORY_TARGET_OFF;effective_level=SudekiMpStoryThreatEffectiveLevel(&config);LEAD_EXPECT(candidate);
    config.level=STORY_TARGET_TIER0;effective_level=SudekiMpStoryThreatEffectiveLevel(&config);config.lead_always=0;LEAD_EXPECT(candidate);
    *(uint32_t *)(party_arb+0x50)=0x10000;LEAD_EXPECT(ai2); /* freshly down, before next sample */
    *(uint32_t *)(party_arb+0x50)=0;config.lead_always=1;
    /* The native filtered list, not merely roster presence, controls admission. */
    lead_count=1;LEAD_EXPECT(candidate);lead_count=33;LEAD_EXPECT(candidate);lead_count=2;
    noncap_allowed=FALSE;LEAD_EXPECT(candidate);noncap_allowed=TRUE;
    *(uint16_t *)(ai2+0x46)=0x300;LEAD_EXPECT(candidate);*(uint16_t *)(ai2+0x46)=0;
    *(uint32_t *)(arb2+0x50)=0x10000;LEAD_EXPECT(candidate);*(uint32_t *)(arb2+0x50)=0;
    *(void **)(pos2+0x10)=NULL;LEAD_EXPECT(candidate);*(void **)(pos2+0x10)=party2;
    strcpy(config.excluded,"AB");*(uint32_t *)(actor+0x34)=4290;LEAD_EXPECT(candidate);
    *(uint32_t *)(actor+0x34)=0;strcpy(config.excluded,"MON_GuardSparPartner_Melee");
    cooperative=FALSE;LEAD_EXPECT(candidate);cooperative=TRUE;
    proof=FALSE;LEAD_EXPECT(candidate);proof=TRUE;
    /* No eligible humans: living AI, then away, remain candidates with native caps. */
    *(uint32_t *)(party_arb+0x50)=0x10000;kinds[1]=STORY_SEAT_AI;
    seat_inputs[1]=(SudekiMpStoryThreatSeat){.owned_ai=1,.native_ai=1};
    *(int16_t *)(ai2+0x16a)=0;mode2[0xb]=1;LEAD_EXPECT(ai2);
    kinds[1]=STORY_SEAT_AWAY;seat_inputs[1].away=1;LEAD_EXPECT(ai2);
    *(uint32_t *)(arb2+0x50)=0x10000;native_pick=NULL;LEAD_EXPECT(NULL);
    *(uint32_t *)(arb2+0x50)=0;native_pick=NULL;LEAD_EXPECT(ai2);
    *(uint32_t *)(party_arb+0x50)=0;
    /* A returning human becomes eligible on the same actor; no ownership write. */
    kinds[1]=STORY_SEAT_REMOTE;mode2[0xb]=0;*(int16_t *)(ai2+0x16a)=1;
    seat_inputs[1]=(SudekiMpStoryThreatSeat){.connected=1,.ready=1,.max_age=250};LEAD_EXPECT(ai2);
    /* A stable committed target must not consume the change-log budget even
     * though native mode B keeps returning the host on every evaluation. */
    clock_forced=TRUE;clock_tick=60000;reset_selection_logs();memset(counters,0,sizeof(counters));log_reset();
    native_pick=candidate;*(void **)(ai+0xc0)=ai2+4;
    for(unsigned i=0;i<500;++i) {LEAD_EXPECT(ai2);}
    assert(lead_substitutions==500 && !lead_changes && !lead_logs);
    assert(!log_count("event=lead_substitution ") && log_count("event=type ")==1);
    report_at=0;report(clock_tick);clock_tick+=1000;LEAD_EXPECT(ai2);report(clock_tick);
    assert(log_count("event=type ")==1); /* count flush does not forget types */
    *(void **)(ai+0xc0)=candidate+4;LEAD_EXPECT(ai2);
    assert(lead_changes==1 && lead_logs==1 && log_count("event=lead_substitution ")==1);
    for(unsigned i=0;i<10;++i) {LEAD_EXPECT(ai2);}
    assert(lead_changes==11 && lead_logs==1);
    clock_tick+=999;LEAD_EXPECT(ai2);assert(lead_logs==1);
    ++clock_tick;LEAD_EXPECT(ai2);assert(lead_logs==2);
    for(unsigned i=2;i<128;++i) {clock_tick+=1000;LEAD_EXPECT(ai2);}
    clock_tick+=1000;LEAD_EXPECT(ai2);
    assert(lead_logs==128 && log_count("event=lead_substitution ")==128);
    report(clock_tick);char totals[96];
    snprintf(totals,sizeof(totals),"substitutions=%lu changes=%lu",(unsigned long)lead_substitutions,(unsigned long)lead_changes);
    assert(log_count(totals));
    clock_forced=FALSE;
#undef LEAD_EXPECT
    seams[0].trampoline=real;memcpy(image+0x1b6ec0,backup,5);
    FlushInstructionCache(GetCurrentProcess(),image+0x1b6ec0,5);
    config.level=STORY_TARGET_WATCH;effective_level=SudekiMpStoryThreatEffectiveLevel(&config);
    /* These list entries were synthetic raw references, not native-owned TPtrs. */
    memset(image+0x3d5540,0,12*4);*(unsigned *)(image+0x3c30ac)=0;
    fixture();
}
static uint8_t ca[4][0x150],cu[4][0x170],cp[4][0x80],cb[4][0x64],cs[4][0x50],cg[4][0x10];
static uint8_t cstates[4][4][0x70],cm[0x130],cl[10*12];
static uint8_t ea[6][0x150],eu[6][0x170],ep[6][0x80];
static uint8_t *cs_array[4][4],*companion_request;
static float companion_range;
static unsigned companion_count;
enum { TIER_ENEMIES=6 };
static void * __stdcall companion_list(void *r,void **out,void *self,float range) {
    assert(r==companion_request && self==cu[2] && range==companion_range);
    *(unsigned *)(image+0x3c30ac)=companion_count;
    for(unsigned i=0;i<companion_count && i<2;++i)
        *(uintptr_t *)(image+0x3d5540+12*i)=(uintptr_t)eu[i]+4;
    *out=eu[0];return out;
}
static void companion_fixture(void) {
    memset(ca,0,sizeof(ca));memset(cu,0,sizeof(cu));memset(cp,0,sizeof(cp));memset(cb,0,sizeof(cb));
    memset(cs,0,sizeof(cs));memset(cg,0,sizeof(cg));memset(cstates,0,sizeof(cstates));memset(cm,0,sizeof(cm));
    memset(ea,0,sizeof(ea));memset(eu,0,sizeof(eu));memset(ep,0,sizeof(ep));memset(companions,0,sizeof(companions));
    roster=(SudekiMpLanStoryNativeRoster){.epoch=1,.available_mask=15,.leader_character=0};
    for(unsigned c=0;c<4;++c) {
        *(void **)cu[c]=image+0x2d4924;*(void **)(cu[c]+0x10)=ca[c];*(unsigned *)(cu[c]+0x148)=5;
        *(void **)(cu[c]+0x3c)=cg[c];*(void **)(cu[c]+0x40)=cm+0xf4;
        *(void **)(ca[c]+0x94)=cu[c];*(void **)(ca[c]+0x74)=cp[c];*(void **)(cp[c]+0x10)=ca[c];
        *(void **)(ca[c]+0x90)=cb[c];*(void **)cb[c]=image+0x2cc9ac;*(void **)(cb[c]+0x10)=ca[c];
        *(void **)(ca[c]+0x4c)=cs[c];*(void **)cs[c]=image+0x2cc064;*(void **)(cs[c]+0x10)=ca[c];
        *(float *)(cs[c]+0x40)=1;*(void **)(cg[c]+4)=cs_array[c];cg[c][9]=4;cg[c][8]=3;cg[c][0xb]=c>=2;
        static const unsigned vt[]={0x2dc7b0,0x2dc6a8,0x2dc7e4,0x2dc5d4},types[]={7,2,8,12};
        for(unsigned s=0;s<4;++s) {cs_array[c][s]=cstates[c][s];*(void **)cstates[c][s]=image+vt[s];cstates[c][s][0x19]=(uint8_t)types[s];}
        cstates[c][1][0x1c]=9;*(float *)(cstates[c][1]+0x20)=-1;
        cstates[c][1][0x24]=cstates[c][1][0x28]=0xff;
        cstates[c][1][0x5c]=0xc0;cstates[c][1][0x5d]=0xa0; /* native ctor bit 7, preserved high bits */
        *(void **)(cm+0xf4+12*c)=ca[c];*(void **)(cl+12*c)=cu[c]+4;
        roster.ai[c]=cu[c];roster.actors[c]=ca[c];
        seat_inputs[c]=(SudekiMpStoryThreatSeat){0};
        if(c==0) seat_inputs[c].host=1;
        else if(c==1) seat_inputs[c]=(SudekiMpStoryThreatSeat){.connected=1,.ready=1,.receipt=GetTickCount(),.max_age=250};
        else seat_inputs[c].native_ai=1;
        kinds[c]=SudekiMpStoryThreatSeatKind(&seat_inputs[c],GetTickCount());
    }
    *(float *)(cp[0]+0x44)=20;*(float *)(cp[1]+0x44)=5;*(float *)(cp[3]+0x44)=1;
    for(unsigned i=0;i<2;++i) {
        *(void **)eu[i]=image+0x2d4924;*(void **)(eu[i]+0x10)=ea[i];*(unsigned *)(eu[i]+0x148)=1;
        *(void **)ea[i]=image+0x2d5fa0;*(void **)(ea[i]+0x94)=eu[i];*(void **)(ea[i]+0x74)=ep[i];
        *(void **)(ep[i]+0x10)=ea[i];*(float *)(ep[i]+0x44)=i?5:20;
        *(void **)(cl+12*(4+i))=eu[i]+4;
    }
    *(void **)(image+0x409de4)=cm;*(void **)(cm+0xf0)=cl;*(unsigned *)(cm+0xe8)=6;*(unsigned *)(cm+0x124)=4;
    admitted=cooperative=proof=companion_proof=noncap_allowed=TRUE;
    config.level=STORY_TARGET_TIER0;effective_level=SudekiMpStoryThreatEffectiveLevel(&config);companion_count=2;companion_range=-1;
    companion_request=cstates[2][1]+0x38;sampled_at=GetTickCount();
}
static void companion_expect_at(unsigned expected,unsigned line) {
    void *out=NULL;sampled_at=GetTickCount();seat_inputs[1].receipt=sampled_at;
    SetLastError(0x4321);
    assert(((SelectFn)(image+SELECTOR))(companion_request,&out,cu[2],companion_range)==&out);
    if(out!=eu[expected]) fprintf(stderr,"companion expectation line=%u expected=%u got=%p\n",line,expected,out);
    assert(out==eu[expected] && GetLastError()==0x4321);
}
#define COMPANION_EXPECT(expected) companion_expect_at(expected,__LINE__)
static void companion_tests(void) {
    companion_fixture();void *real=seams[0].trampoline;seams[0].trampoline=(void *)companion_list;
    uint8_t backup[5];memcpy(backup,image+0x1b6ec0,5);jump(0x1b6ec0,noncap);
    typedef void *(__thiscall *BuilderFn)(void *,void **,void *);
    void *out=NULL;
    assert(((BuilderFn)(image+0x1ae820))(cstates[2][1],&out,cu[2])==&out && out==eu[1]);
    assert(companion_request[0x24]==0xf9 && companion_request[0x25]==0xa4);
    assert(companions[2].anchor_plus_one==2 && cg[2][8]==3); /* evaluated, not active */
    uint8_t saved_ai[sizeof(cu)],saved_states[sizeof(cstates)],saved_stats[sizeof(cs)],saved_formation[sizeof(cm)];
    memcpy(saved_ai,cu,sizeof(cu));memcpy(saved_states,cstates,sizeof(cstates));memcpy(saved_stats,cs,sizeof(cs));memcpy(saved_formation,cm,sizeof(cm));
    COMPANION_EXPECT(1);
    assert(!memcmp(saved_ai,cu,sizeof(cu)) && !memcmp(saved_states,cstates,sizeof(cstates)) &&
        !memcmp(saved_stats,cs,sizeof(cs)) && !memcmp(saved_formation,cm,sizeof(cm)));
    *(float *)(cp[1]+0x44)=21;COMPANION_EXPECT(0);assert(companions[2].anchor_plus_one==2);
    *(float *)(cp[1]+0x44)=40;COMPANION_EXPECT(0);assert(companions[2].anchor_plus_one==1);
    *(unsigned *)(cb[0]+0x50)=*(unsigned *)(cb[1]+0x50)=0x10000;
    COMPANION_EXPECT(1);assert(companions[2].anchor_plus_one==4);
    seat_inputs[3].away=1;COMPANION_EXPECT(1);assert(companions[2].anchor_plus_one==4);
    *(unsigned *)(cb[3]+0x50)=0x10000;COMPANION_EXPECT(1);assert(companions[2].anchor_plus_one==3);
    *(unsigned *)(cb[1]+0x50)=0;*(float *)(cp[1]+0x44)=5;COMPANION_EXPECT(1);assert(companions[2].anchor_plus_one==2);
    cg[2][8]=0xff;COMPANION_EXPECT(1);cg[2][8]=1;COMPANION_EXPECT(1);
#define COMPANION_REFUSES(change,restore) do {change;COMPANION_EXPECT(0);restore;} while(0)
    COMPANION_REFUSES(config.level=STORY_TARGET_WATCH,config.level=STORY_TARGET_TIER0);
    COMPANION_REFUSES(config.level=STORY_TARGET_OFF,config.level=STORY_TARGET_TIER0);
    COMPANION_REFUSES(cooperative=FALSE,cooperative=TRUE);
    COMPANION_REFUSES(proof=FALSE,proof=TRUE);
    COMPANION_REFUSES(companion_proof=FALSE,companion_proof=TRUE);
    COMPANION_REFUSES(noncap_allowed=FALSE,noncap_allowed=TRUE);
    COMPANION_REFUSES(*(int16_t *)(cu[2]+0x16a)=1,*(int16_t *)(cu[2]+0x16a)=0);
    COMPANION_REFUSES(cg[2][0xb]=0,cg[2][0xb]=1);
    COMPANION_REFUSES(*(float *)(cs[2]+0x40)=0,*(float *)(cs[2]+0x40)=1);
    COMPANION_REFUSES(*(float *)(cs[2]+0x40)=2,*(float *)(cs[2]+0x40)=1);
    COMPANION_REFUSES(*(float *)(cs[2]+0x40)=NAN,*(float *)(cs[2]+0x40)=1);
    COMPANION_REFUSES(*(void **)(cs[2]+0x10)=ca[0],*(void **)(cs[2]+0x10)=ca[2]);
    COMPANION_REFUSES(cg[2][9]=5,cg[2][9]=4);
    COMPANION_REFUSES(cs_array[2][1]=cstates[0][1],cs_array[2][1]=cstates[2][1]);
    COMPANION_REFUSES(cstates[2][1][0x19]=7,cstates[2][1][0x19]=2);
    COMPANION_REFUSES(*(void **)cstates[2][1]=image+0x2dc7b0,*(void **)cstates[2][1]=image+0x2dc6a8);
    COMPANION_REFUSES(*(void **)(cu[2]+0x40)=cm,*(void **)(cu[2]+0x40)=cm+0xf4);
    COMPANION_REFUSES(*(unsigned *)(cm+0x124)=5,*(unsigned *)(cm+0x124)=4);
    COMPANION_REFUSES(*(void **)(cm+0xf4)=ea[0],*(void **)(cm+0xf4)=ca[0]);
    COMPANION_REFUSES(*(void **)(ep[1]+0x10)=ea[0],*(void **)(ep[1]+0x10)=ea[1]);
    COMPANION_REFUSES(*(unsigned *)(cb[2]+0x50)=0x10000,*(unsigned *)(cb[2]+0x50)=0);
    *(float *)(cp[2]+0x44)=20;
    COMPANION_REFUSES(seat_inputs[1].absent=1,seat_inputs[1].absent=0);
    assert(companions[2].anchor_plus_one==3);*(float *)(cp[2]+0x44)=0;
    COMPANION_REFUSES(companion_count=33,companion_count=2);
    COMPANION_REFUSES(companion_count=0,companion_count=2);
    for(unsigned m=0;m<16;++m) if(m!=9) {
        companion_request[0x24]=(uint8_t)(0x70|m);COMPANION_EXPECT(0);
    }
    companion_request[0x24]=0x79;
    COMPANION_REFUSES(companion_request[0x24]=0x69,companion_request[0x24]=0x79);
    COMPANION_REFUSES(companion_request[0x25]=12,companion_request[0x25]=4);
    COMPANION_REFUSES(*(float *)(companion_request+0xc)=4,*(float *)(companion_request+0xc)=-1);
    /* Native ranged builders share the request storage. Self-defense mode 0
     * must neither replace its native answer nor discard the protected seat. */
    for(unsigned which=0;which<4;++which) {
        static const unsigned vt[]={0x2dc6a8,0x2dc880,0x2dc748,0x2dc814},type[]={2,11,5,9};
        BOOL ranged=which>=2;uint8_t *s=cstates[2][1];memset(s,0,0x70);
        *(void **)s=image+vt[which];s[0x19]=(uint8_t)type[which];s[0x1c]=9;*(float *)(s+0x20)=-1;
        if(ranged) {*(float *)(s+0x24)=4;s[0x28]=s[0x2c]=0xff;}
        else s[0x24]=s[0x28]=0xff;
        companion_request=s+(ranged?0x44:0x38);companion_range=-1;sampled_at=GetTickCount();
        companion_request[0x24]=0xc0;companion_request[0x25]=0xa0;
        assert(((BuilderFn)(image+(ranged?0x1adf60:0x1ae820)))(s,&out,cu[2])==&out && out==eu[1]);
        if(ranged) {
            unsigned anchor=companions[2].anchor_plus_one;companion_range=4;
            assert(((BuilderFn)(image+0x1ae000))(s,&out,cu[2])==&out && out==eu[0]);
            assert(companions[2].anchor_plus_one==anchor);
        }
    }
    /* Close the native list/metric path as well: actual melee builder,
     * selector list construction, formation-relative mode 9 and S2 execute.
     * Only external world filtering remains synthetic. */
    companion_fixture();seams[0].trampoline=real;
    noncap_candidates[0]=eu[0];noncap_candidates[1]=eu[1];
    config.level=STORY_TARGET_WATCH;effective_level=SudekiMpStoryThreatEffectiveLevel(&config);sampled_at=GetTickCount();
    assert(((BuilderFn)(image+0x1ae820))(cstates[2][1],&out,cu[2])==&out && out==eu[0]);
    config.level=STORY_TARGET_TIER0;effective_level=SudekiMpStoryThreatEffectiveLevel(&config);sampled_at=GetTickCount();
    assert(((BuilderFn)(image+0x1ae820))(cstates[2][1],&out,cu[2])==&out && out==eu[1]);
    noncap_candidates[0]=noncap_candidates[1]=NULL;
    companion_range=-1;config.level=STORY_TARGET_WATCH;effective_level=SudekiMpStoryThreatEffectiveLevel(&config);companion_sample(GetTickCount());
    assert(strstr(last_log,"companion_state") && strstr(last_log,"known=1"));
    *(void **)(cs[3]+0x10)=ca[0];companion_sample(GetTickCount()+1000);
    assert(strstr(last_log,"known=0"));
    companions[2].anchor_plus_one=2;SudekiMpStoryEnemyTargetingSuspend();
    assert(!admitted && companions[2].anchor_plus_one==2); /* per-frame suspension keeps plain anchor memory */
    SudekiMpStoryEnemyTargetingService(NULL,NULL,NULL,NULL,NULL,FALSE,GetTickCount());
    assert(!companions[2].anchor_plus_one);
#undef COMPANION_REFUSES
    seams[0].trampoline=real;memcpy(image+0x1b6ec0,backup,5);FlushInstructionCache(GetCurrentProcess(),image+0x1b6ec0,5);
    fixture();config.level=STORY_TARGET_WATCH;effective_level=SudekiMpStoryThreatEffectiveLevel(&config);
}
static void companion_log_tests(void) {
    companion_fixture();clock_forced=TRUE;clock_tick=200000;
    void *real=seams[0].trampoline;seams[0].trampoline=(void *)companion_list;
    uint8_t backup[5];memcpy(backup,image+0x1b6ec0,5);jump(0x1b6ec0,noncap);
    typedef void *(__thiscall *BuilderFn)(void *,void **,void *);
    void *out=NULL;log_reset();
#define BUILD_DEFEND() do {sampled_at=clock_tick;seat_inputs[1].receipt=clock_tick; \
    assert(((BuilderFn)(image+0x1ae820))(cstates[2][1],&out,cu[2])==&out && out==eu[1]);} while(0)
    BUILD_DEFEND();assert(companions[2].selection_logs==1);
    clock_tick+=249;BUILD_DEFEND();assert(companions[2].selection_logs==1);
    ++clock_tick;BUILD_DEFEND();assert(companions[2].selection_logs==2);
    for(unsigned i=2;i<128;++i) {clock_tick+=250;BUILD_DEFEND();}
    clock_tick+=250;BUILD_DEFEND();
    assert(companions[2].selection_logs==128 && log_count("event=companion_selection ")==128);
    log_reset();for(unsigned c=0;c<4;++c) companions[c].sampled_at=0;
    companion_sample(clock_tick);assert(log_count("event=companion_state ")==4);
    *(void **)(cg[2]+4)=(void *)1;
    companion_sample(clock_tick+999);assert(log_count("event=companion_state ")==4);
    companion_sample(clock_tick+1000);assert(log_count("event=companion_state ")==8);
    assert(log_count("event=companion_state char=2 known=0")==1);
    *(void **)(cg[2]+4)=cs_array[2];
    companion_sample(clock_tick+1999);assert(log_count("event=companion_state ")==8);
    companion_sample(clock_tick+2000);assert(log_count("event=companion_state ")==12);
    assert(log_count("event=companion_state char=2 known=1")==2);
    seams[0].trampoline=real;memcpy(image+0x1b6ec0,backup,5);FlushInstructionCache(GetCurrentProcess(),image+0x1b6ec0,5);
    clock_forced=FALSE;fixture();
#undef BUILD_DEFEND
}
static void config_tests(void) {
    wchar_t dir[MAX_PATH],path[MAX_PATH],saved_ini[MAX_PATH];wcscpy(saved_ini,ini);
    SudekiMpStoryThreatConfig saved=config;uint32_t saved_at=config_at;unsigned saved_level=effective_level;
    level_clamped=FALSE;
    assert(GetTempPathW(MAX_PATH,dir) && GetTempFileNameW(dir,L"smp",0,path));wcscpy(ini,path);
    assert(WritePrivateProfileStringW(L"StoryAreas",L"EnemyTargeting",L"tier2",path));
    assert(WritePrivateProfileStringW(L"StoryAreas",L"EnemyTargetingCapParity",L"off",path));
    assert(WritePrivateProfileStringW(L"StoryAreas",L"EnemyTargetingLeadRule",L"bogus",path));
    config_at=0;log_reset();read_config(500000);
    assert(config.level==STORY_TARGET_TIER2 && !config.cap_parity);
    assert(log_count("event=invalid_config key=EnemyTargetingLeadRule")==1);
    assert(log_count("event=level_clamped effective=tier0")==1);
    assert(log_count("event=level effective=tier0")==1 && !log_count("event=owner_gate"));
    assert(WritePrivateProfileStringW(L"StoryAreas",L"EnemyTargeting",L"watch",path));
    assert(WritePrivateProfileStringW(L"StoryAreas",L"EnemyTargetingCapParity",L"on",path));
    assert(WritePrivateProfileStringW(L"StoryAreas",L"EnemyTargetingLeadRule",L"always",path));
    read_config(500999);assert(config.level==STORY_TARGET_TIER2 && !config.cap_parity);
    read_config(501000);assert(config.level==STORY_TARGET_WATCH && config.cap_parity && config.lead_always);
    assert(log_count("event=invalid_config ")==1 && !log_count("event=owner_gate "));
    assert(log_count("event=level effective=watch")==1);
    assert(DeleteFileW(path));wcscpy(ini,saved_ini);config=saved;config_at=saved_at;effective_level=saved_level;
}
static void selection_log_tests(void) {
    fixture();clock_forced=TRUE;clock_tick=400000;sampled_at=clock_tick;
    config.level=STORY_TARGET_WATCH;effective_level=SudekiMpStoryThreatEffectiveLevel(&config);memset(counters,0,sizeof(counters));reset_selection_logs();log_reset();
    void *real=seams[0].trampoline;seams[0].trampoline=(void *)synthetic_select;
    request[0x24]=0;*(unsigned *)(image+0x3c30ac)=0;native_pick=candidate;void *out=NULL;
    SelectFn select=(SelectFn)(image+SELECTOR);
    assert(select(request,&out,ai,30)==&out && out==candidate);
    native_pick=NULL;assert(select(request,&out,ai,30)==&out && !out);
    kinds[0]=STORY_SEAT_ABSENT;native_pick=candidate;
    assert(select(request,&out,ai,30)==&out && out==candidate);
    assert(counters[0].count==3 && counters[0].picks[0]==2 && counters[0].empty==1 && counters[0].native_absent==1);
    report_at=0;report(clock_tick);
    assert(log_count("requests=3 empty=1 cap_only_requests=0 native_absent=1 picks=2,0,0,0")==1);
    assert(!counters[0].count);
    clock_tick+=1000;sampled_at=clock_tick;assert(select(request,&out,ai,30)==&out);
    assert(log_count("event=type ")==1);
    request[0x24]=11;assert(select(request,&out,ai,30)==&out);
    assert(log_count("event=type ")==2); /* same resource, different mode */
    *(uint32_t *)(actor+0x34)=0x1234;assert(select(request,&out,ai,30)==&out);
    assert(log_count("event=type ")==3);
    for(unsigned i=0;i<125;++i) assert(first_type(0x10000+i,0));
    assert(!first_type(0x20000,0) && !first_type(0,0));
    *(uint32_t *)(actor+0x34)=0;kinds[0]=STORY_SEAT_REMOTE;
    *(void **)(ai+0xc0)=candidate+4;*(uint32_t *)(ai+0x44)=0;ai[0x46]=0;
    *(uint16_t *)(candidate+0x46)=0x0200;
    log_reset();walk(clock_tick);
    assert(log_count("event=stale_count character=0 native_melee=2 native_ranged=0 walk_melee=1 walk_ranged=0 engaged=1")==1);
    *(uint16_t *)(candidate+0x46)=0x0100;clock_tick+=1000;walk(clock_tick);
    assert(log_count("event=stale_count ")==1);
    *(uint16_t *)(candidate+0x46)=0;seams[0].trampoline=real;clock_forced=FALSE;fixture();
}
static void service_tests(void) {
    companion_fixture();clock_forced=TRUE;clock_tick=10000;config_at=clock_tick;
    SudekiMpLanStoryNativeRoster exact=roster;SudekiMpLanStoryScene scene={0};
    SudekiMpStoryThreatSeat input[4]={{0}};
    input[0].max_age=input[1].max_age=input[2].max_age=input[3].max_age=250;
    input[1].connected=input[1].ready=1;input[1].receipt=clock_tick;
    input[2].connected=input[2].away=input[2].owned_ai=1;
    input[3].owned_ai=1;
    SudekiMpLanPartyLease keys[4]={{0}};keys[2]=(SudekiMpLanPartyLease){.token=77,.generation=3,.seat=1};
    unsigned firing[4]={0,1,1,1};
#define SERVICE() do {config_at=clock_tick;SudekiMpStoryEnemyTargetingService(&exact,&scene,input,firing,keys,TRUE,clock_tick);} while(0)
    SERVICE();assert(admitted);
    assert(kinds[0]==STORY_SEAT_HOST && kinds[1]==STORY_SEAT_REMOTE && kinds[2]==STORY_SEAT_AWAY && kinds[3]==STORY_SEAT_AI);
    assert(seat_inputs[0].host && seat_inputs[1].ready && seat_inputs[1].receipt==clock_tick &&
        seat_inputs[2].native_ai && seat_inputs[2].away && seat_inputs[3].native_ai && seat_inputs[3].owned_ai);
    assert(away_connections[2].token==77 && !firing_signal[0] && firing_signal[1] && !firing_signal[2] && !firing_signal[3]);
#define SERVICE_KIND(change,expected,restore) do {change;SERVICE();assert(kinds[1]==expected);restore;SERVICE();assert(kinds[1]==STORY_SEAT_REMOTE);} while(0)
    SERVICE_KIND(input[1].receipt=clock_tick-251,STORY_SEAT_AI,input[1].receipt=clock_tick);
    SERVICE_KIND(input[1].held=1,STORY_SEAT_AI,input[1].held=0);
    SERVICE_KIND(*(unsigned *)(cb[1]+0x50)=0x10000,STORY_SEAT_DOWN,*(unsigned *)(cb[1]+0x50)=0);
    SERVICE_KIND(*(unsigned *)(cu[1]+0x44)=0x58,STORY_SEAT_DOWN,*(unsigned *)(cu[1]+0x44)=0);
    SERVICE_KIND(scene.inside_mask=2,STORY_SEAT_ABSENT,scene.inside_mask=0);
    SERVICE_KIND(same_area_mask=13,STORY_SEAT_ABSENT,same_area_mask=15);
    *(int16_t *)(cu[3]+0x16a)=1;SERVICE();assert(!seat_inputs[3].native_ai);
    *(int16_t *)(cu[3]+0x16a)=0;SERVICE();assert(seat_inputs[3].native_ai);
    cg[3][0xb]=0;SERVICE();assert(!seat_inputs[3].native_ai);
    cg[3][0xb]=1;SERVICE();assert(seat_inputs[3].native_ai);
    unsigned walks=walk_sequence;uint32_t sampled=clock_tick-1000;
    companions[0].sampled_at=sampled;log_reset();
    *(void **)(cu[1]+0x3c)=(void *)1;SERVICE();
    assert(!admitted && walk_sequence==walks && companions[0].sampled_at==sampled);
    assert(!log_count("event=companion_state "));
    *(void **)(cu[1]+0x3c)=cg[1];SERVICE();assert(admitted && walk_sequence==walks+1);
    assert(companions[0].sampled_at==clock_tick && log_count("event=companion_state char=0 ")==1);
    lease_known=FALSE;
    for(unsigned i=0;i<2;++i) {
        companions[2].anchor_plus_one=2;identities[0].epoch=roster.epoch;identities[0].serial=99;
        companions[2].selection_logs=128;companions[2].selection_at=clock_tick;
        lead_substitutions=200;lead_changes=150;lead_logs=128;lead_log_at=clock_tick;
        assert(first_type(0x1234+i,11));
        if(i) ++exact.revision;else ++exact.epoch;
        SERVICE();assert(!companions[2].anchor_plus_one && !identities[0].serial);
        assert(!companions[2].selection_logs && !companions[2].selection_at);
        assert(!lead_substitutions && !lead_changes && !lead_logs && !lead_log_at);
        assert(first_type(0x1234+i,11));
    }
    lease_known=TRUE;companions[2].anchor_plus_one=2;
    companions[2].selection_logs=128;companions[2].selection_at=clock_tick;
    uint32_t selection_tick=clock_tick;clock_tick+=251;input[1].receipt=clock_tick;
    unsigned observations=log_count("event=companion_state char=2 ");
    SERVICE();assert(!companions[2].anchor_plus_one);
    assert(log_count("event=companion_state char=2 ")==observations+1 && companions[2].sampled_at==clock_tick);
    assert(companions[2].selection_logs==128 && companions[2].selection_at==selection_tick);
    *(unsigned *)(cb[0]+0x50)=0x400000;SERVICE();assert(firing_signal[0]);
    *(unsigned *)(cb[0]+0x50)=0;firing[1]=0;SERVICE();assert(!firing_signal[0] && !firing_signal[1]);
    for(unsigned c=0;c<4;++c) {shot_sequences[c]=1;shot_ticks[c]=clock_tick;}
    SERVICE();assert(firing_signal[0] && firing_signal[1] && !firing_signal[2] && !firing_signal[3]);
    for(unsigned c=0;c<4;++c) shot_ticks[c]=clock_tick-1001;
    SERVICE();assert(!firing_signal[0] && !firing_signal[1] && !firing_signal[2] && !firing_signal[3]);
    memset(shot_sequences,0,sizeof(shot_sequences));memset(shot_ticks,0,sizeof(shot_ticks));
    clock_forced=FALSE;config_at=0;fixture();
#undef SERVICE_KIND
#undef SERVICE
}
/* Multi-lease exact-roster service fixture, shared by all C.2 scenarios. */
static SudekiMpLanStoryNativeRoster tier_roster;
static SudekiMpLanStoryScene tier_scene;
static SudekiMpStoryThreatSeat tier_seats[4];
static unsigned tier_firing[4],tier_mask=3;
static uint8_t tier_request[0x28],tier_combat[6][0x74],tier_stats[6][0x34];
static void tier_service(uint32_t delta) {
    clock_tick+=delta;config_at=clock_tick;tier_seats[1].receipt=clock_tick;
    SudekiMpStoryEnemyTargetingService(&tier_roster,&tier_scene,tier_seats,tier_firing,NULL,TRUE,clock_tick);
}
static void tier_advance(uint32_t delta) {
    while(delta) {uint32_t step=delta>250?250:delta;tier_service(step);delta-=step;}
}
static void tier_fixture(unsigned level) {
    multiple_leases=FALSE;leased_actor=NULL;lease_known=TRUE;lease_departed=FALSE;
    companion_fixture();clock_forced=TRUE;clock_tick=1000000;
    SudekiMpStoryThreatDefaults(&config);config.level=level;effective_level=SudekiMpStoryThreatEffectiveLevel(&config);
    level_logged=TRUE;level_clamped=FALSE;config_at=clock_tick;report_at=clock_tick;
    memset(identities,0,sizeof(identities));memset(push_states,0,sizeof(push_states));push_tracked=FALSE;
    memset(shot_sequences,0,sizeof(shot_sequences));memset(shot_ticks,0,sizeof(shot_ticks));
    memset(tier_combat,0,sizeof(tier_combat));memset(tier_stats,0,sizeof(tier_stats));
    memset(tier_request,0,sizeof(tier_request));tier_request[0x26]=tier_request[0x27]=3;
    tier_mask=3;native_pick=cu[0];noncap_denied=NULL;noncap_calls=0;
    memset(noncap_candidates,0,sizeof(noncap_candidates));control_proof=session_proof=TRUE;
    controlled_actor=ca[1];control_character=1;*(int16_t *)(cu[1]+0x16a)=1;
    *(float *)(cp[0]+0x44)=0;*(float *)(cp[1]+0x44)=20;
    for(unsigned i=0;i<TIER_ENEMIES;++i) {
        *(void **)eu[i]=image+0x2d4924;*(void **)(eu[i]+0x10)=ea[i];*(unsigned *)(eu[i]+0x148)=1;
        *(void **)ea[i]=image+0x2d5fa0;*(void **)(ea[i]+0x94)=eu[i];*(void **)(ea[i]+0x74)=ep[i];
        *(void **)(ep[i]+0x10)=ea[i];*(float *)(ep[i]+0x44)=(float)(i*2);
        *(void **)(ea[i]+0xa4)=tier_combat[i];*(void **)tier_combat[i]=image+0x2c8754;*(void **)(tier_combat[i]+0x10)=ea[i];
        *(void **)(ea[i]+0x4c)=tier_stats[i];*(void **)tier_stats[i]=image+0x2cc064;*(void **)(tier_stats[i]+0x10)=ea[i];
        *(float *)(tier_stats[i]+0x2c)=*(float *)(tier_stats[i]+0x30)=100;
        *(void **)(cl+12*(4+i))=eu[i]+4;
        test_leases[i]=(SudekiMpStorySpawnLeaseObservation){.serial=i+1,.actor=ea[i],.resource=0x100+i,.group_id=7};
    }
    *(unsigned *)(cm+0xe8)=4+TIER_ENEMIES;test_lease_count=TIER_ENEMIES;multiple_leases=TRUE;
    tier_roster=roster;tier_roster.epoch=99;tier_roster.revision=1;
    memset(&tier_scene,0,sizeof(tier_scene));memset(tier_seats,0,sizeof(tier_seats));memset(tier_firing,0,sizeof(tier_firing));
    tier_seats[1]=(SudekiMpStoryThreatSeat){.connected=1,.ready=1,.receipt=clock_tick,.max_age=250};
    tier_seats[2].owned_ai=tier_seats[3].owned_ai=1;
    tier_service(0);assert(admitted && table_complete && unit_count==TIER_ENEMIES);
    for(unsigned i=0;i<TIER_ENEMIES;++i) assert(record_of(i+1) && record_of(i+1)->serial==i+1);
    log_reset();
}
static void * __stdcall tier_native(void *r,void **out,void *self,float range) {
    (void)self;assert(r==tier_request && range==30);
    unsigned mode=tier_request[0x24]&15;
    if(mode==0 || mode==11) {
        unsigned count=0;
        for(unsigned c=0;c<4;++c) if(tier_mask&(1u<<c))
            *(uintptr_t *)(image+0x3d5540+12*count++)=(uintptr_t)cu[c]+4;
        *(unsigned *)(image+0x3c30ac)=count;
    }
    *out=native_pick;return out;
}
static void *tier_select(unsigned enemy,unsigned mode) {
    tier_request[0x24]=(uint8_t)mode;void *out=NULL;
    assert(((SelectFn)(image+SELECTOR))(tier_request,&out,eu[enemy],30)==&out);return out;
}
static Hit tier_hit(unsigned enemy,unsigned shooter,float damage,int firing) {
    uint8_t packet[0x68]={0};Hit h={0};
    *(void **)packet=image+0x2d9148;*(void **)(packet+0x30)=ca[shooter];
    *(unsigned *)(cb[shooter]+0x58)=0x20;firing_signal[shooter]=(unsigned)firing;
    *(float *)(tier_stats[enemy]+0x2c)=100;
    assert(hit_observe(tier_combat[enemy],packet,FALSE,&h));
    *(float *)(tier_stats[enemy]+0x2c)=100-damage;
    assert(hit_observe(tier_combat[enemy],packet,TRUE,&h));return h;
}
static void tier_alert_fixture(unsigned level) {
    tier_fixture(level);eu[1][0x46]=eu[2][0x46]=1;tier_firing[1]=1;tier_service(0);
    tier_hit(0,1,30,TRUE);
}
static void tier_push_fixture(unsigned level) {
    tier_fixture(level);
    for(unsigned i=0;i<TIER_ENEMIES;++i) *(uintptr_t *)(eu[i]+0xf4)=1;
    tier_firing[1]=1;tier_service(0);tier_advance(5000);
}

static void tier_seam_tests(void) {
    tier_fixture(STORY_TARGET_TIER1);SudekiMpStoryThreatRecord *r=record_of(1);
    r->threat[1]=.5f;r->updated_at=clock_tick;
    uint8_t self_before[sizeof(eu)],request_before[sizeof(tier_request)];
    memcpy(self_before,eu,sizeof(eu));memcpy(request_before,tier_request,sizeof(tier_request));
    noncap_calls=0;assert(tier_select(0,0)==cu[1] && noncap_calls==1);
    assert(!memcmp(self_before,eu,sizeof(eu)) && !memcmp(request_before,tier_request,sizeof(tier_request)));
    assert(threat_substitutions==1);
    noncap_denied=cu[1];noncap_calls=0;assert(tier_select(0,0)==cu[0] && noncap_calls==1);
    /* Failed higher pick falls back to the tier0 lead pick, with its own proof. */
    native_pick=NULL;noncap_calls=0;assert(tier_select(0,11)==cu[0] && noncap_calls==2);
    noncap_denied=NULL;native_pick=cu[0];
    *(unsigned *)(cb[1]+0x50)=0x10000;noncap_calls=0;assert(tier_select(0,0)==cu[0] && !noncap_calls);
    *(unsigned *)(cb[1]+0x50)=0;r->threat[1]=0;noncap_calls=0;assert(tier_select(0,0)==cu[0] && !noncap_calls);
    r->threat[1]=.5f;strcpy(config.excluded,"AB");test_leases[0].resource=4290;
    SudekiMpStoryThreatRecord before=*r;noncap_calls=0;assert(tier_select(0,0)==cu[0] && !noncap_calls);
    tier_hit(0,1,30,1);assert(!memcmp(r,&before,sizeof(before)));
    strcpy(config.excluded,"MON_GuardSparPartner_Melee");test_leases[0].resource=0x100;
    lease_known=FALSE;native_pick=cu[1];assert(tier_select(0,11)==cu[0]);lease_known=TRUE;

    tier_alert_fixture(STORY_TARGET_TIER15);r=record_of(2);
    assert(record_of(1)->alerting[0].active && r->responding[0].active && r->responding[0].shooter==1);
    assert(record_of(1)->alerting[0].responders[0]==2 && record_of(1)->alerting[0].responders[1]==3 && record_of(1)->alerting[0].responders[2]==1);
    memcpy(self_before,eu,sizeof(eu));memcpy(request_before,tier_request,sizeof(tier_request));
    noncap_calls=0;for(unsigned i=0;i<10;++i) assert(tier_select(1,0)==cu[1]);assert(noncap_calls==10);
    assert(!memcmp(self_before,eu,sizeof(eu)) && !memcmp(request_before,tier_request,sizeof(tier_request)));
    *(unsigned *)(image+0x3c30ac)=0xffffffff;uint32_t blocked=lock_blocked;
    assert(tier_select(1,14)==cu[0] && lock_blocked==blocked+1 && r->lock_blocked==1);
    assert(*(unsigned *)(image+0x3c30ac)==0xffffffff);
    native_pick=cu[1];assert(tier_select(1,14)==cu[1] && lock_blocked==blocked+1);native_pick=cu[0];
    noncap_denied=cu[1];assert(tier_select(1,0)==cu[0] && r->responding[0].recheck_failed);
    noncap_denied=NULL;tier_service(1);assert(!r->responding[0].active && log_count("event=responder_released serial=2 "));
    tier_firing[1]=0;tier_advance(3250);assert(!record_of(1)->alerting[0].active);
    assert(tier_select(1,0)==cu[0]);

    tier_push_fixture(STORY_TARGET_TIER2);assert(push_states[1][0].active && push_states[1][0].serial==6);
    memcpy(self_before,eu,sizeof(eu));memcpy(request_before,tier_request,sizeof(tier_request));
    noncap_calls=0;for(unsigned i=0;i<10;++i) assert(tier_select(5,0)==cu[1]);assert(noncap_calls==10);
    assert(!memcmp(self_before,eu,sizeof(eu)) && !memcmp(request_before,tier_request,sizeof(tier_request)));
    blocked=lock_blocked;assert(tier_select(5,14)==cu[0] && lock_blocked==blocked+1);
    native_pick=cu[1];assert(tier_select(5,14)==cu[1] && lock_blocked==blocked+1);native_pick=cu[0];
    noncap_denied=cu[1];assert(tier_select(5,0)==cu[0] && push_states[1][0].recheck_failed);
    noncap_denied=NULL;tier_service(1);assert(!push_states[1][0].active && log_count("event=push_end reason=invalid seat=1 serial=6 watch=0"));

    tier_fixture(STORY_TARGET_TIER2);for(unsigned i=0;i<TIER_ENEMIES;++i) *(uintptr_t *)(eu[i]+0xf4)=1;
    tier_select(5,1);tier_select(4,14);tier_firing[1]=1;tier_advance(5000);
    assert(push_states[1][0].active && push_states[1][0].serial==4);
    tier_fixture(STORY_TARGET_TIER15);eu[1][0x46]=eu[2][0x46]=1;
    tier_select(1,1);tier_select(2,14);tier_service(0);tier_hit(0,1,30,1);
    assert(record_of(1)->alerting[0].count==1 && record_of(1)->alerting[0].responders[0]==1);
}
static void tier_feed_tests(void) {
    tier_fixture(STORY_TARGET_TIER0);config.cap_parity=0;
    SudekiMpStoryThreatRecord *r=record_of(1);
    *(uintptr_t *)(eu[1]+0xc0)=(uintptr_t)cu[1]+4;eu[1][0x46]=1;eu[1][0xf2]=0;
    *(float *)(ep[1]+0x44)=18;tier_service(0);
    assert(seat_frames[1].melee==0 && seat_frames[1].nearest==2);
    tier_hit(0,1,30,1);assert(r->threat[1]>.29f && r->sniper[1]==0);
    *(float *)(ep[1]+0x44)=16;tier_service(0);tier_hit(0,1,30,1);assert(r->sniper[1]>.29f);
    r->sniper[1]=0;*(void **)(ep[1]+0x10)=NULL;tier_service(0);
    assert(seat_frames[1].position_unknown && seat_frames[1].engaged==1);tier_hit(0,1,30,1);assert(r->sniper[1]==0);
    *(void **)(ep[1]+0x10)=ea[1];*(float *)(ep[0]+0x44)=10;tier_service(0);
    Hit hit=tier_hit(0,1,30,1);assert(hit.is_far && hit.distance==10 && r->sniper[1]==0); /* is_far is the wrong feed */
    *(float *)(ep[0]+0x44)=8;tier_service(0);tier_hit(0,1,30,1);assert(r->sniper[1]==0);
    *(float *)(ep[0]+0x44)=7.9f;tier_service(0);tier_hit(0,1,30,1);assert(r->sniper[1]>.29f);
    r->sniper[1]=0;*(void **)(cp[1]+0x10)=NULL;tier_service(0);tier_hit(0,1,30,1);assert(r->sniper[1]==0);
    *(void **)(cp[1]+0x10)=ca[1];

    tier_fixture(STORY_TARGET_TIER15);r=record_of(1);lease_known=FALSE;tier_service(0);
    assert(!table_complete && record_of(1)==r);lease_known=TRUE;
    uint32_t empty_before=alert_empty;tier_hit(0,1,30,1);
    assert(!r->alerting[0].active && alert_empty==empty_before && !r->sniper[1]);
    *(void **)(cm+0xf0)=(void *)1;tier_service(0);assert(!table_complete && record_of(1)==r);
    *(void **)(cm+0xf0)=cl;
    uint32_t overflow[129*3]={0};for(unsigned i=0;i<129;++i) overflow[i*3]=(uint32_t)(uintptr_t)(eu[0]+4);
    *(void **)(cm+0xf0)=overflow;*(unsigned *)(cm+0xe8)=129;tier_service(0);assert(!table_complete && unit_count==128);
    *(void **)(cm+0xf0)=cl;*(unsigned *)(cm+0xe8)=4+TIER_ENEMIES;tier_service(0);
    /* Only the walk allocates memory, and allies never acquire a record. */
    memset(identities,0,sizeof(identities));unsigned unslotted=hit_unslotted;tier_hit(0,1,10,0);assert(hit_unslotted==unslotted+1 && !record_of(1));
    *(void **)ea[0]=image+0x2d55d4;*(unsigned *)(eu[0]+0x148)=4;tier_service(0);
    assert(!record_of(1));unslotted=hit_unslotted;tier_hit(0,1,10,0);assert(hit_unslotted==unslotted);

    tier_push_fixture(STORY_TARGET_TIER2);*(uintptr_t *)(eu[5]+0xc0)=(uintptr_t)cu[1]+4;tier_service(1);
    assert(push_states[1][0].active && !push_states[1][0].idle_active && seat_frames[1].engaged==1);
    *(uintptr_t *)(eu[0]+0xc0)=(uintptr_t)cu[1]+4;test_leases[0].departed=TRUE;tier_service(1);
    assert(seat_frames[1].engaged==2 && seat_frames[1].unleased==1 && !push_states[1][0].active);
    assert(log_count("event=push_end reason=engaged seat=1 serial=6 watch=0"));
}
static void tier_lifetime_tests(void) {
    tier_alert_fixture(STORY_TARGET_TIER15);
    /* One active alert per shooter, including copies orphaned by retirement. */
    uint32_t alert_count=alerts;tier_hit(3,1,30,1);
    assert(alerts==alert_count && !record_of(4)->alerting[0].active && !record_of(4)->alerting[0].cooldown_active);
    *(uintptr_t *)(cl+4*12)=0;tier_service(1);
    assert(!record_of(1) && record_of(2)->responding[0].active && log_count("event=alert_retired serial=1 watch=0"));
    assert(shooter_alert_active(0,1));tier_hit(3,1,30,1);assert(alerts==alert_count);
    *(uintptr_t *)(cl+5*12)=*(uintptr_t *)(cl+6*12)=0;tier_service(1);
    assert(!record_of(2) && !record_of(3) && !shooter_alert_active(0,1));
    assert(log_count("event=alert_end reason=retired serial=1 watch=0"));

    tier_alert_fixture(STORY_TARGET_TIER2);
    SudekiMpStoryThreatPushStart(&config,0,&push_states[0][0],6,clock_tick,list_seq);
    ++tier_roster.revision;tier_service(1);
    assert(log_count("event=alert_end reason=namespace serial=1 watch=0"));
    assert(log_count("event=push_end reason=namespace seat=0 serial=6 watch=0"));
    assert(!record_of(1)->alerting[0].active && !record_of(2)->responding[0].active && !record_of(1)->threat[1] && !push_states[0][0].active);
    tier_alert_fixture(STORY_TARGET_TIER2);
    SudekiMpStoryThreatPushStart(&config,0,&push_states[0][0],6,clock_tick,list_seq);
    config_at=clock_tick;SudekiMpStoryEnemyTargetingService(&tier_roster,&tier_scene,tier_seats,tier_firing,NULL,FALSE,clock_tick);
    assert(log_count("event=alert_end reason=level serial=1 watch=0") && log_count("event=push_end reason=level seat=0 serial=6 watch=0"));
    tier_service(1);assert(!record_of(1)->alerting[0].active && !push_states[0][0].active);

    tier_alert_fixture(STORY_TARGET_TIER15);*(float *)(ep[1]+0x44)=13.99f;tier_service(1);
    assert(record_of(1)->alerting[0].active); /* 6.01 units */
    *(void **)(cp[1]+0x10)=NULL;*(float *)(ep[1]+0x44)=14;tier_service(1);
    assert(record_of(1)->alerting[0].active && seat_frames[1].position_unknown);
    *(void **)(cp[1]+0x10)=ca[1];tier_service(1);
    assert(!record_of(1)->alerting[0].active && log_count("event=alert_end reason=close serial=1 watch=0"));

    tier_push_fixture(STORY_TARGET_TIER2);
    *(uintptr_t *)(eu[5]+0xc0)=(uintptr_t)cu[1]+4;
    for(unsigned i=0;i<31;++i) {tier_advance(250);assert(tier_select(5,0)==cu[1]);}
    tier_advance(249);assert(tier_select(5,0)==cu[1]);
    tier_advance(1);assert(!push_states[1][0].active && tier_select(5,0)==cu[0]);
    assert(log_count("event=push_end reason=expired seat=1 serial=6 watch=0"));

    tier_fixture(STORY_TARGET_TIER2);tier_mask=1;tier_select(5,0); /* pre-start observation lacks shooter */
    for(unsigned i=0;i<TIER_ENEMIES;++i) *(uintptr_t *)(eu[i]+0xf4)=1;
    tier_firing[1]=1;tier_advance(5000);assert(push_states[1][0].active && push_states[1][0].serial==6);
    tier_service(1);assert(push_states[1][0].active);
    tier_select(5,0);tier_service(1);assert(!push_states[1][0].active);

    for(unsigned gap=0;gap<2;++gap) {
        tier_fixture(STORY_TARGET_TIER2);for(unsigned i=0;i<TIER_ENEMIES;++i) *(uintptr_t *)(eu[i]+0xf4)=1;
        tier_firing[1]=1;tier_advance(4000);assert(!push_states[1][0].active);
        if(gap) tier_service(251);
        else {config_at=clock_tick;SudekiMpStoryEnemyTargetingService(NULL,&tier_scene,tier_seats,tier_firing,NULL,TRUE,clock_tick);tier_service(1);}
        tier_advance(4999);assert(!push_states[1][0].active);tier_advance(1);assert(push_states[1][0].active);
    }
    /* A timestamp one millisecond ahead is not an unobserved service gap. */
    tier_fixture(STORY_TARGET_TIER2);
    for(unsigned i=0;i<TIER_ENEMIES;++i) *(uintptr_t *)(eu[i]+0xf4)=1;
    tier_firing[1]=1;tier_advance(4000);tier_service(UINT32_MAX);
    tier_advance(1000);assert(!push_states[1][0].active);
    tier_advance(1);assert(push_states[1][0].active);
    /* A hit on an unrelated enemy refreshes both the original alert and copies. */
    tier_alert_fixture(STORY_TARGET_TIER15);tier_firing[1]=0;tier_advance(2500);
    assert(!record_of(4)->responding[0].active && !record_of(4)->alerting[0].active);
    tier_hit(3,1,1,0);uint32_t touched=clock_tick;
    assert(record_of(1)->alerting[0].last_fire==touched && record_of(2)->responding[0].last_fire==touched);
    tier_advance(3000);assert(record_of(1)->alerting[0].active);
    tier_advance(1);assert(!record_of(1)->alerting[0].active && log_count("event=alert_end reason=stop serial=1 watch=0"));
}
static void tier_aware_tests(void) {
    tier_fixture(STORY_TARGET_TIER2);for(unsigned i=0;i<TIER_ENEMIES;++i) *(uintptr_t *)(eu[i]+0xf4)=1;tier_service(0);
    assert(aware_pick(1,0)==6);
    EnemyFrame *u=frame_of(6);SudekiMpStoryThreatRecord *r=record_of(6);
#define AWARE_REFUSES(change,restore) do {change;assert(aware_pick(1,0)==5);restore;assert(aware_pick(1,0)==6);} while(0)
    AWARE_REFUSES(u->aware=0,u->aware=1);AWARE_REFUSES(u->type=4,u->type=1);AWARE_REFUSES(u->excluded=1,u->excluded=0);
    AWARE_REFUSES(u->x=40.01f,u->x=10);AWARE_REFUSES(u->x=NAN,u->x=10);
    r->mode_seen=1;AWARE_REFUSES(r->last_mode=1,r->last_mode=0);AWARE_REFUSES(r->last_mode=14,r->last_mode=0);
    AWARE_REFUSES(r->responding[0].active=1,r->responding[0].active=0);
    push_states[0][0].serial=6;AWARE_REFUSES(push_states[0][0].active=1,push_states[0][0].active=0);
    for(unsigned i=0;i<unit_count-1;++i) unit_table[i].aware=0;
    u->x=40;assert(aware_pick(1,0)==6);u->x=40.01f;assert(!aware_pick(1,0));
    table_complete=FALSE;assert(!aware_pick(1,0));table_complete=TRUE;
    seat_frames[1].position_unknown=TRUE;assert(!aware_pick(1,0));
#undef AWARE_REFUSES
}
static void tier_watch_config_tests(void) {
    tier_fixture(STORY_TARGET_WATCH);native_pick=cu[1];
    assert(tier_select(0,11)==cu[1] && !noncap_calls && counters[0].would[STORY_RULE_LEAD]==1);
    tier_fixture(STORY_TARGET_WATCH);record_of(1)->threat[1]=.5f;record_of(1)->updated_at=clock_tick;
    uint8_t units_before[sizeof(eu)],request_before[sizeof(tier_request)];memcpy(units_before,eu,sizeof(eu));memcpy(request_before,tier_request,sizeof(tier_request));
    noncap_calls=0;assert(tier_select(0,0)==cu[0] && !noncap_calls && counters[0].would[STORY_RULE_THREAT]==1);
    assert(!memcmp(units_before,eu,sizeof(eu)) && !memcmp(request_before,tier_request,sizeof(tier_request)));
    tier_alert_fixture(STORY_TARGET_WATCH);assert(!record_of(1)->alerting[0].active && record_of(1)->alerting[1].active);
    noncap_calls=0;memcpy(units_before,eu,sizeof(eu));memcpy(request_before,tier_request,sizeof(tier_request));
    assert(tier_select(1,0)==cu[0] && !noncap_calls && counters[0].would[STORY_RULE_ALERT]==1);
    assert(!memcmp(units_before,eu,sizeof(eu)) && !memcmp(request_before,tier_request,sizeof(tier_request)));
    assert(log_count("event=alert ") && log_count("watch=1"));
    tier_push_fixture(STORY_TARGET_WATCH);assert(!push_states[1][0].active && push_states[1][1].active);
    noncap_calls=0;memcpy(units_before,eu,sizeof(eu));memcpy(request_before,tier_request,sizeof(tier_request));
    assert(tier_select(5,0)==cu[0] && !noncap_calls && counters[0].would[STORY_RULE_PUSH]==1);
    assert(!memcmp(units_before,eu,sizeof(eu)) && !memcmp(request_before,tier_request,sizeof(tier_request)));
    assert(log_count("event=push seat=1 serial=6 watch=1"));
    /* Full searches use only their own designation set. */
    tier_fixture(STORY_TARGET_TIER2);for(unsigned i=0;i<TIER_ENEMIES;++i) *(uintptr_t *)(eu[i]+0xf4)=1;tier_service(0);
    record_of(6)->responding[0].active=1;assert(aware_pick(1,0)==5 && aware_pick(1,1)==6);
    record_of(6)->responding[0].active=0;record_of(6)->responding[1].active=1;assert(aware_pick(1,0)==6 && aware_pick(1,1)==5);
    tier_fixture(STORY_TARGET_TIER15);eu[1][0x46]=eu[2][0x46]=1;tier_service(0);
    record_of(2)->responding[0]=(SudekiMpStoryThreatResponding){.active=1,.shooter=0,.alerting_serial=99};
    tier_hit(0,1,30,1);assert(record_of(2)->responding[0].alerting_serial==99 && record_of(2)->responding[1].alerting_serial==1);
    tier_fixture(STORY_TARGET_TIER15);eu[1][0x46]=eu[2][0x46]=1;tier_service(0);
    record_of(2)->responding[1]=(SudekiMpStoryThreatResponding){.active=1,.shooter=0,.alerting_serial=99};
    tier_hit(0,1,30,1);assert(record_of(2)->responding[1].alerting_serial==99 && record_of(2)->responding[0].alerting_serial==1);

    wchar_t dir[MAX_PATH],path[MAX_PATH],saved_ini[MAX_PATH];wcscpy(saved_ini,ini);
    assert(GetTempPathW(MAX_PATH,dir) && GetTempFileNameW(dir,L"smp",0,path));wcscpy(ini,path);
    tier_alert_fixture(STORY_TARGET_WATCH);
    assert(WritePrivateProfileStringW(L"StoryAreas",L"EnemyTargeting",L"tier15",path));
    config_at=0;read_config(clock_tick);assert(effective_level==STORY_TARGET_TIER15);
    assert(!record_of(1)->alerting[1].active && !record_of(2)->responding[1].active);
    assert(log_count("event=alert_end reason=level serial=1 watch=1"));
    assert(WritePrivateProfileStringW(L"StoryAreas",L"EnemyTargeting",L"tier2",path));
    assert(WritePrivateProfileStringW(L"StoryAreas",L"EnemyTargetingCapParity",L"off",path));
    tier_fixture(STORY_TARGET_TIER0);log_reset();config_at=0;read_config(clock_tick);
    assert(config.level==STORY_TARGET_TIER2 && effective_level==STORY_TARGET_TIER0);
    assert(log_count("event=level effective=tier0")==0); /* effective tier was already logged by fixture */
    level_logged=FALSE;config_at=0;read_config(clock_tick);read_config(clock_tick+1000);
    assert(log_count("event=level effective=tier0")==1);
    native_pick=cu[1];noncap_calls=0;assert(tier_select(0,11)==cu[0] && noncap_calls==1);
    native_pick=cu[0];tier_hit(0,1,30,1);assert(tier_select(0,0)==cu[0]);
    for(unsigned i=0;i<TIER_ENEMIES;++i) *(uintptr_t *)(eu[i]+0xf4)=1;
    tier_firing[1]=1;tier_advance(5000);
    assert(!threat_substitutions && !alerts && !pushes && !log_count("event=alert ") && !log_count("event=push "));
    assert(!log_count("event=owner_gate"));assert(DeleteFileW(path));wcscpy(ini,saved_ini);
}
static void tier_log_bound_tests(void) {
    tier_fixture(STORY_TARGET_TIER2);log_reset();
    eu[1][0x46]=eu[2][0x46]=1;tier_firing[1]=1;
    config.value[STORY_IDLE_SECONDS]=0;
    for(unsigned i=0;i<TIER_ENEMIES;++i) *(uintptr_t *)(eu[i]+0xf4)=1;
    for(unsigned i=0;i<80;++i) {
        /* Repeat real service/hit/retirement paths in one namespace. The
         * fixture resets state, but never the adapter's event budgets. */
        memset(push_states,0,sizeof(push_states));
        for(unsigned j=0;j<TIER_ENEMIES;++j) {
            memset(record_of(j+1)->alerting,0,sizeof(record_of(j+1)->alerting));
            memset(record_of(j+1)->responding,0,sizeof(record_of(j+1)->responding));
        }
        tier_service(0);assert(push_states[1][0].active);
        for(unsigned set=0;set<STORY_SET_COUNT;++set) push_states[1][set].recheck_failed=1;
        tier_service(0);assert(!push_states[1][0].active);
        tier_hit(0,1,30,1);assert(record_of(1)->alerting[0].active);
        for(unsigned set=0;set<STORY_SET_COUNT;++set) record_of(2)->responding[set].recheck_failed=1;
        tier_service(0);assert(!record_of(2)->responding[0].active);
        *(uintptr_t *)(cl+4*12)=0;tier_service(0);assert(!record_of(1));
        *(uintptr_t *)(cl+6*12)=0;tier_service(0);assert(!record_of(3) && !shooter_alert_active(0,1));
        *(uintptr_t *)(cl+4*12)=(uintptr_t)eu[0]+4;
        *(uintptr_t *)(cl+6*12)=(uintptr_t)eu[2]+4;tier_service(0);
    }
    const char *events[]={"event=alert ","event=alert_end ","event=alert_retired ","event=responder_released ","event=push ","event=push_end "};
    for(unsigned i=0;i<EVENT_COUNT;++i) assert(log_count(events[i])==64);
    tier_firing[1]=0;++tier_roster.epoch;tier_service(0);
    for(unsigned i=0;i<EVENT_COUNT;++i) assert(event_logs[i]==0);
    record_of(1)->threat[1]=.5f;record_of(1)->updated_at=clock_tick;log_reset();
    *(uintptr_t *)(eu[0]+0xc0)=(uintptr_t)cu[1]+4;
    for(unsigned i=0;i<500;++i) assert(tier_select(0,0)==cu[1]);
    assert(!threat_logs && !threat_changes);
    *(uintptr_t *)(eu[0]+0xc0)=(uintptr_t)cu[0]+4;assert(tier_select(0,0)==cu[1] && threat_logs==1);
    clock_tick+=999;sampled_at=clock_tick;tier_seats[1].receipt=clock_tick;seat_inputs[1].receipt=clock_tick;
    assert(tier_select(0,0)==cu[1] && threat_logs==1);++clock_tick;sampled_at=clock_tick;seat_inputs[1].receipt=clock_tick;
    assert(tier_select(0,0)==cu[1] && threat_logs==2);
    for(unsigned i=2;i<130;++i) {clock_tick+=1000;sampled_at=clock_tick;seat_inputs[1].receipt=clock_tick;record_of(1)->updated_at=clock_tick;assert(tier_select(0,0)==cu[1]);}
    assert(threat_logs==128 && log_count("event=threat_substitution ")==128);
}
static void higher_tier_tests(void) {
    void *real=seams[0].trampoline;seams[0].trampoline=(void *)tier_native;
    uint8_t backup[5];memcpy(backup,image+0x1b6ec0,5);jump(0x1b6ec0,noncap);
    tier_seam_tests();tier_feed_tests();tier_lifetime_tests();tier_aware_tests();tier_watch_config_tests();tier_log_bound_tests();
    memcpy(image+0x1b6ec0,backup,5);FlushInstructionCache(GetCurrentProcess(),image+0x1b6ec0,5);seams[0].trampoline=real;
    multiple_leases=FALSE;clock_forced=FALSE;noncap_denied=NULL;config.level=STORY_TARGET_WATCH;effective_level=STORY_TARGET_WATCH;
    fixture();log_reset();
}

static void retirement_tests(void) {
    fixture();leased_actor=actor;
    walk(GetTickCount());assert(identities[0].serial==7);
    *(unsigned *)(manager+0xe8)=0;lease_known=FALSE;
    walk(GetTickCount());assert(identities[0].serial==7); /* unknown is not absence */
    lease_known=TRUE;walk(GetTickCount());assert(!identities[0].serial);
    *(unsigned *)(manager+0xe8)=1;walk(GetTickCount());assert(identities[0].serial==7);
    lease_departed=TRUE;walk(GetTickCount());assert(!identities[0].serial);lease_departed=FALSE;
    walk(GetTickCount());assert(identities[0].serial==7);leased_actor=NULL;
    walk(GetTickCount());assert(!identities[0].serial);
}
static void hit_tests(void) {
    fixture();*(uint32_t *)(ai+0x148)=1;leased_actor=actor;
    uint8_t co[0x74]={0},stats[0x34]={0},packet[0x68]={0},arb[0x64]={0},source_pos[0x80]={0};
    *(void **)(actor+0xa4)=co;*(void **)co=image+0x2c8754;*(void **)(co+0x10)=actor;
    *(void **)(actor+0x4c)=stats;*(void **)stats=image+0x2cc064;*(void **)(stats+0x10)=actor;
    *(float *)(stats+0x2c)=100;*(float *)(stats+0x30)=200;
    *(void **)(party+0x90)=arb;*(void **)arb=image+0x2cc9ac;*(void **)(arb+0x10)=party;
    *(uint32_t *)(arb+0x58)=0x20;
    *(void **)(party+0x74)=source_pos;*(void **)(source_pos+0x10)=party;*(float *)(source_pos+0x44)=20;
    *(void **)packet=image+0x2d9148;*(void **)(packet+0x30)=party;packet[0x61]=2;packet[0x64]=3;
    Hit h={0};assert(hit_observe(co,packet,FALSE,&h));
    assert(h.source==0 && h.serial==7 && h.hp==100 && h.max_hp==200 && h.is_far && h.ranged_mode);
    *(float *)(stats+0x2c)=75;assert(hit_observe(co,packet,TRUE,&h));
    assert(strstr(last_log,"amount=25.000") && strstr(last_log,"ranged=1") && strstr(last_log,"hit_all=0"));
    const unsigned source_kinds[]={STORY_SEAT_REMOTE,STORY_SEAT_AWAY,STORY_SEAT_AI};
    for(unsigned i=0;i<3;++i) {
        kinds[0]=source_kinds[i];firing_signal[0]=i==0;
        *(float *)(stats+0x2c)=100;assert(hit_observe(co,packet,FALSE,&h));
        assert(h.kind==source_kinds[i] && h.firing==(i==0));
        *(float *)(stats+0x2c)=75;assert(hit_observe(co,packet,TRUE,&h));
        char field[64];snprintf(field,sizeof(field),"seat_kind=%s",SudekiMpStoryThreatSeatName(source_kinds[i]));
        assert(strstr(last_log,field) && strstr(last_log,i==0?"firing=1":"firing=0"));
    }
    kinds[0]=STORY_SEAT_REMOTE;firing_signal[0]=0;
    spirit_fixture=TRUE;assert(hit_observe(co,packet,FALSE,&h));*(float *)(stats+0x2c)=0;
    assert(hit_observe(co,packet,TRUE,&h));assert(strstr(last_log,"lethal=1") && strstr(last_log,"hit_all=1 ranged=0"));
    spirit_fixture=FALSE;*(void **)(packet+0x30)=actor;
    assert(hit_observe(co,packet,FALSE,&h) && h.source==4);
    *(void **)(packet+0x30)=party;assert(hit_observe(co,packet,FALSE,&h));
    leased_serial=8;assert(!hit_observe(co,packet,TRUE,&h));leased_serial=7;
    lease_known=FALSE;assert(!hit_observe(co,packet,FALSE,&h));lease_known=TRUE;
    lease_departed=TRUE;assert(!hit_observe(co,packet,FALSE,&h));lease_departed=FALSE;
    *(void **)co=NULL;assert(!hit_observe(co,packet,FALSE,&h));*(void **)co=image+0x2c8754;
    cooperative=FALSE;assert(!hit_observe(co,packet,FALSE,&h));cooperative=TRUE;
    leased_actor=NULL;
}
int main(int argc,char **argv) {
    wchar_t path[32768];assert(argc==2);assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,32768));
    image=map_image(path);contracts();assert(SudekiMpStoryEnemyTargetingInstall((HMODULE)image));
    assert(!SudekiMpStoryEnemyTargetingInstall((HMODULE)image));
    assert(!SudekiMpLanHitObserverSubscribe(consumer));
    assert(!SudekiMpStoryEnemyTargetingBindThread(GetCurrentThreadId()+1) && !thread);
    assert(!SudekiMpLanHitObserverBindThread(GetCurrentThreadId()+1));
    assert(SudekiMpStoryEnemyTargetingBindThread(GetCurrentThreadId()));
    assert(SudekiMpLanHitObserverSubscribe(consumer));assert(!SudekiMpLanHitObserverSubscribe(consumer));
    assert(SudekiMpLanHitObserverUnsubscribe(consumer));
    assert(image_exact(image,TRUE));native_calls();parity_tests();away_parity_tests();lead_selection_tests();companion_tests();companion_log_tests();service_tests();selection_log_tests();retirement_tests();hit_tests();higher_tier_tests();config_tests();
    HANDLE t=CreateThread(NULL,0,wrong_thread,NULL,0,NULL);assert(t);WaitForSingleObject(t,INFINITE);CloseHandle(t);
    calls=1;assert(!SudekiMpStoryEnemyTargetingUninstall());calls=0;
    uint8_t owned=image[CAP_MELEE];image[CAP_MELEE]^=1;
    assert(!SudekiMpStoryEnemyTargetingUninstall() && base && SudekiMpStoryEnemyCapNext[0] && SudekiMpStoryEnemyCapAuthored[0]);
    image[CAP_MELEE]=owned;assert(SudekiMpStoryEnemyTargetingUninstall());assert(!base && SudekiMpLanHitObserverVacant());
    assert(!level_logged && !level_clamped && !push_tracked && !table_complete);
    for(unsigned set=0;set<STORY_SET_COUNT;++set) for(unsigned c=0;c<4;++c) assert(!push_states[c][set].active);
    assert(SudekiMpStoryEnemyTargetingImageExact(image));assert(SudekiMpStoryEnemyTargetingInstall((HMODULE)image));
    assert(SudekiMpStoryEnemyTargetingBindThread(GetCurrentThreadId()));
    uint8_t popup_owned=image[0x12969e];image[0x12969e]^=1;
    assert(!SudekiMpStoryEnemyTargetingUninstall() && base && hit_owned);
    for(unsigned i=0;i<3;++i) {
        assert(!seams[i].installed);
        assert(!memcmp(image+sites[i],i?compare:prefix,i?sizeof(compare):sizeof(prefix)));
    }
    assert(installed && !subscribed && !admitted);
    assert(!SudekiMpStoryEnemyTargetingBindThread(GetCurrentThreadId()));
    assert(!SudekiMpLanHitObserverBindThread(GetCurrentThreadId()));
    assert(!SudekiMpLanHitObserverSubscribe(consumer));
    image[0x12969e]=popup_owned;
    assert(SudekiMpStoryEnemyTargetingUninstall());VirtualFree(image,0,MEM_RELEASE);
    puts("story enemy image/ABI/cap/watch/rollback/teardown tests passed");return 0;
}
