/* Test the actual split preflight against Windows file-mapping protections
 * and the read-only residency predicate against synthetic native layouts.
 * No hooks, live game objects or native gameplay functions are invoked. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../src/hooks/lan_story_split.c"
static unsigned diagnostic_count;
static char last_diagnostic[512];
static void *fixture_actors[4];
static BOOL fixture_observer_exact=TRUE;
void SudekiMpLogFormat(const char *format,...) {
    va_list args;va_start(args,format);
    vsnprintf(last_diagnostic,sizeof(last_diagnostic),format,args);va_end(args);
    ++diagnostic_count;
    fputs(last_diagnostic,stdout);
}
void SudekiMpLogWrite(const char *text) {if(strstr(text,"begin_refused"))++diagnostic_count;}
BOOL SudekiMpLogResearchEnabled(void) {return FALSE;}
void *SudekiMpCleanroomEngineActorEntity(SudekiMpCleanroomActor actor) {
    static const SudekiMpCleanroomActor types[4]={SUDEKIMP_CLEANROOM_BUKI,
        SUDEKIMP_CLEANROOM_ELCO,SUDEKIMP_CLEANROOM_TAL,SUDEKIMP_CLEANROOM_AILISH};
    for(unsigned c=0;c<4u;++c) if(actor==types[c]) return fixture_actors[c];
    return NULL;
}
BOOL SudekiMpLanStoryObserverNativeRosterExact(const SudekiMpLanStoryNativeRoster *r) {
    return fixture_observer_exact && r && r->dispatch_serial && r->epoch;
}
unsigned SudekiMpLanStoryWorldSplitAnimateTargets(SudekiMpLanStoryWorldAnimateTarget *out,unsigned max) {
    (void)out;(void)max;return 0;
}
unsigned SudekiMpCastLightWorldAdoptions(void) {return 0;}
unsigned SudekiMpCastLightForeignSkips(void) {return 0;}
unsigned SudekiMpCastLightReadyFailure(void) {return 0;}
unsigned SudekiMpCastLightFaultSite(void) {return 0;}
unsigned SudekiMpCastLightFaultLine(void) {return 0;}
BOOL SudekiMpLanStoryTempExteriorInstall(HMODULE image) {(void)image;assert(!"unused");return FALSE;}
BOOL SudekiMpLanStoryTempExteriorAttach(const SudekiMpLanStoryTempExteriorConsumer *owner) {(void)owner;assert(!"unused");return FALSE;}
BOOL SudekiMpLanStoryTempExteriorDetach(void) {assert(!"unused");return FALSE;}
BOOL SudekiMpLanStoryTempExteriorUninstall(void) {assert(!"unused");return FALSE;}
BOOL SudekiMpLanStoryObserverSetSplit(const SudekiMpLanStoryObserverSplit *owner) {(void)owner;assert(!"unused");return FALSE;}
static void state(void) {
    base=(uint8_t *)(uintptr_t)1;active=settled_inside=FALSE;attached=observer_set=host_ready=TRUE;
    outside_mask=4;away_mask=0;memset(parked,0,sizeof(parked));native_thread=GetCurrentThreadId();
    temporary_data=NULL;memset(temporary,0,sizeof(temporary));
    memset(&residency,0,sizeof(residency));memset(fixture_actors,0,sizeof(fixture_actors));
    exempt_count=0;exempt_group=NULL;memset(exempt_actor,0,sizeof(exempt_actor));
    memset(exempt_added,0,sizeof(exempt_added));fixture_observer_exact=TRUE;
    clear_returned();
}
static DWORD WINAPI foreign(void *unused) {
    (void)unused;assert(!SudekiMpLanStorySplitTestCanEnterTemporary("LICo_Frappe_Farm",3));return 0;
}
/* All six Kamo INTERIOR records in the shipped SOL level metadata. These
 * are regression inputs for the generic TEMP path, not an admission list. */
static void kamo_lifecycle(void) {
    static const char *const destinations[]={"LNBr_Kamo_shop","LICa_Kamo_Shop",
        "LSMoB04_Kamo_shop","LTCi_Kamo_Shop","DCRe_Kamo_Shop","DCCi_Kamo_Shop"};
    for(unsigned i=0;i<sizeof(destinations)/sizeof(*destinations);++i) {
        for(unsigned outside=0;outside<4u;++outside) {
            unsigned inside=(outside+1u)%4u;
            SudekiMpLanStoryTempExteriorEntry entry={0};
            state();outside_mask=(uint8_t)(1u<<outside);
            strcpy(entry.destination,destinations[i]);
            assert(!keep_exterior(NULL,&entry) && !begin_exit());
            assert(SudekiMpLanStorySplitTestCanEnterTemporary(destinations[i],inside));
            assert(begin_temp(destinations[i],inside));
            assert(active && !settled_inside && leader==inside && !strcmp(temporary,destinations[i]));
            assert(keep_exterior(NULL,&entry));
            assert(!begin_temp(destinations[i],inside)); /* no nested TEMP */
            for(unsigned c=0;entry.destination[c];++c)
                if(entry.destination[c]>='A' && entry.destination[c]<='Z')
                    entry.destination[c]=(char)(entry.destination[c]+('a'-'A'));
            assert(keep_exterior(NULL,&entry));
            strcpy(entry.destination,"different_interior");
            assert(!keep_exterior(NULL,&entry));
            strcpy(entry.destination,destinations[i]);
            /* Fixture-only settled marker: native descriptor discovery is
             * covered by image tests; no game objects are read here. */
            settled_inside=TRUE;
            assert(!keep_exterior(NULL,&entry) && begin_exit());
            ended(FALSE);
            assert(!active && !settled_inside && !temporary[0] && !temporary_data);
            assert(!keep_exterior(NULL,&entry) && !begin_exit());
            assert(begin_temp(destinations[i],inside)); /* later visit */
            ended(TRUE); /* abandonment also closes the name lease */
            assert(!active && !temporary[0] && !keep_exterior(NULL,&entry));
            state();outside_mask=(uint8_t)(1u<<inside);
            assert(!begin_temp(destinations[i],inside)); /* no exterior player */
        }
        printf("PASS named KamoSplitLifecycle destination=%s outside_characters=4\n",destinations[i]);
    }
    state();
}
#define FPTR(p,o) (*(void **)((uint8_t *)(p)+(o)))
#define FU32(p,o) (*(uint32_t *)((uint8_t *)(p)+(o)))
static uint8_t *fixture_image,*fixture_image_seed,*fixture_heap;
enum { FIXTURE_WORLD_SIZE=0x3a0,FIXTURE_GROUP_SIZE=0xd4 };
static uint8_t *fixture_world,*fixture_descriptor,*fixture_group,*fixture_controller,
    *fixture_actor_bytes[4],*fixture_ai[4],*fixture_data,*fixture_exterior,*fixture_exterior_data;
static char *fixture_exterior_name;
static char *fixture_name;
static SudekiMpLanStoryNativeRoster fixture_roster;
static unsigned fixture_outside;
static void residency_fixture(unsigned outside) {
    state();base=fixture_image;fixture_outside=outside;
    /* Native heaps are private writable memory. Writable test-image sections
     * may be PAGE_WRITECOPY under Wine and deliberately fail native-object
     * readability; model these layouts in our private mapping instead. */
    memset(fixture_heap,0,0x4000);
    fixture_world=fixture_heap+0x1000;fixture_descriptor=fixture_heap+0x1400;fixture_group=fixture_heap+0x1500;
    fixture_controller=fixture_heap+0x1600;fixture_data=fixture_heap+0x3000;fixture_name=(char *)(fixture_heap+0x3040);
    fixture_exterior=fixture_heap+0x1900;fixture_exterior_data=fixture_heap+0x3100;
    fixture_exterior_name=(char *)(fixture_heap+0x3140);
    FU32(fixture_exterior,0x34)=3;FPTR(fixture_exterior,0x14)=fixture_exterior_data;
    FPTR(fixture_exterior,0x24)=fixture_exterior_name;strcpy(fixture_exterior_name,"NewBrightwater");
    for(unsigned c=0;c<4u;++c) {
        fixture_actor_bytes[c]=fixture_heap+0x2000+c*0x400;fixture_ai[c]=fixture_heap+0x2100+c*0x400;
    }
    memset(&fixture_roster,0,sizeof(fixture_roster));
    unsigned inside=(outside+1u)%4u;
    /* Seed through the shared view. First capture reads a pristine COPY view;
     * later private writes must not change the reported protection contract. */
    FPTR(fixture_image_seed,WORLD)=fixture_world;FPTR(fixture_image_seed,GROUP)=fixture_group;
    FPTR(fixture_image_seed,CONTROLLER)=fixture_controller;
    FPTR(fixture_world,0xc)=fixture_descriptor;fixture_world[0x399]=fixture_world[0x39a]=1;
    FPTR(fixture_descriptor,0x14)=fixture_data;FPTR(fixture_descriptor,0x24)=fixture_name;
    strcpy(fixture_name,"LNBr_Kamo_shop");FU32(fixture_descriptor,0x34)=4;
    FU32(fixture_group,0xcc)=4;FU32(fixture_group,0xd0)=1;
    for(unsigned c=0,slot=1;c<4u;++c) {
        fixture_actors[c]=fixture_actor_bytes[c];FPTR(fixture_actor_bytes[c],0x94)=fixture_ai[c];
        FPTR(fixture_ai[c],0x10)=fixture_actor_bytes[c];
        FPTR(fixture_group,0x90+(c==inside?0u:slot++)*0xcu)=fixture_actor_bytes[c];
        fixture_roster.actors[c]=fixture_actor_bytes[c];fixture_roster.ai[c]=fixture_ai[c];
        if(c!=inside) {exempt_actor[exempt_count]=fixture_actor_bytes[c];exempt_added[exempt_count++]=1;}
    }
    exempt_group=fixture_group;outside_mask=(uint8_t)(15u&~(1u<<inside));
    FPTR(fixture_controller,0x248)=fixture_actor_bytes[inside];
    fixture_roster.dispatch_serial=1;fixture_roster.epoch=7;fixture_roster.revision=3;
    fixture_roster.available_mask=15;fixture_roster.leader_character=(uint8_t)inside;
    fixture_roster.world=fixture_world;fixture_roster.descriptor=fixture_descriptor;
    fixture_roster.group=fixture_group;fixture_roster.controller=fixture_controller;
    assert(begin_temp(fixture_name,inside));
    /* Real keep-entry callback observes the ordinary exterior before native
     * TEMP selection. It supplies a provisional ticket, completed only by a
     * later matching balanced exit receipt. */
    FPTR(fixture_world,0xc)=fixture_exterior;
    SudekiMpLanStoryTempExteriorEntry entry={.ticket=41u};
    strcpy(entry.exterior,"NewBrightwater");strcpy(entry.destination,"LNBr_Kamo_shop");
    assert(keep_exterior(NULL,&entry) && returned.ticket==41u && !returned.armed);
    FPTR(fixture_world,0xc)=fixture_descriptor;
    assert(!SudekiMpLanStorySplitExteriorActorExact(&fixture_roster,outside)); /* entry gap */
    unsigned diagnostics_before=diagnostic_count;
    settled(TRUE);assert(residency.exact && diagnostic_count==diagnostics_before+1u);
    assert(strstr(last_diagnostic,"residency_exact=1 residency_reason=ready"));
    assert(SudekiMpLanStorySplitExteriorActorExact(&fixture_roster,outside));
}
static DWORD WINAPI residency_foreign(void *unused) {
    (void)unused;assert(!SudekiMpLanStorySplitExteriorActorExact(&fixture_roster,fixture_outside));return 0;
}
static void return_receipt(uint64_t ticket,unsigned result) {
    FPTR(fixture_world,0xc)=NULL;FPTR(fixture_world,0x10)=fixture_exterior;
    fixture_world[0x39a]=0;
    SudekiMpLanStoryTempExteriorReceipt receipt={.ticket=ticket,.result=result};
    strcpy(receipt.exterior,"NewBrightwater");exit_skipped(NULL,&receipt);
}
static void return_settle(void) {
    FPTR(fixture_world,0xc)=fixture_exterior;FPTR(fixture_world,0x10)=NULL;
    fixture_world[0x399]=fixture_world[0x39a]=1;
    fixture_roster.descriptor=fixture_exterior;++fixture_roster.revision;
    settled(FALSE);ended(FALSE);
    /* This proof does not reuse consumed lead-only exemption pointers. */
    FU32(fixture_group,0xd0)=0;exempt_count=0;exempt_group=NULL;
    memset(exempt_actor,0,sizeof(exempt_actor));memset(exempt_added,0,sizeof(exempt_added));
}
static void returned_fixture(unsigned c) {
    residency_fixture(c);
    assert(!SudekiMpLanStorySplitReturnedActorExact(&fixture_roster,c));
    SudekiMpLanStorySplitClearReturned();
    assert(active && returned.ticket==41u && !returned.armed); /* host ready inside */
    assert(begin_exit() && returned.armed && !returned.settled);
    assert(!SudekiMpLanStorySplitReturnedActorExact(&fixture_roster,c));
    return_receipt(41u,SUDEKIMP_STORY_TEMP_EXTERIOR_EXIT_BALANCED);
    assert(returned.balanced && !SudekiMpLanStorySplitReturnedActorExact(&fixture_roster,c));
    return_settle();
    assert(returned.settled && SudekiMpLanStorySplitReturnedActorExact(&fixture_roster,c));
}
static DWORD WINAPI returned_foreign(void *unused) {
    (void)unused;assert(!SudekiMpLanStorySplitReturnedActorExact(&fixture_roster,fixture_outside));return 0;
}
static void returned_residency(void) {
    for(unsigned c=0;c<4u;++c) {
        returned_fixture(c);
        uint8_t before[0x4000];memcpy(before,fixture_heap,sizeof(before));
        assert(SudekiMpLanStorySplitReturnedActorExact(&fixture_roster,c));
        assert(!memcmp(before,fixture_heap,sizeof(before)));
        assert(!SudekiMpLanStorySplitReturnedActorExact(NULL,c));
        assert(!SudekiMpLanStorySplitReturnedActorExact(&fixture_roster,4));
        assert(!SudekiMpLanStorySplitReturnedActorExact(&fixture_roster,fixture_roster.leader_character));
        assert(SudekiMpLanStorySplitReturnedActorExact(&fixture_roster,c));
        /* Actor/world pause readiness belongs to the caller. This function
         * proves residency only and never consumes a native pause reference. */
        fixture_actor_bytes[c][0x2b]=2;
        assert(SudekiMpLanStorySplitReturnedActorExact(&fixture_roster,c) && fixture_actor_bytes[c][0x2b]==2);
        fixture_actor_bytes[c][0x2b]=0;
        SudekiMpLanStorySplitSetEligibility(TRUE,15u,0u); /* scene ready != input ready */
        assert(SudekiMpLanStorySplitReturnedActorExact(&fixture_roster,c));
        HANDLE worker=CreateThread(NULL,0,returned_foreign,NULL,0,NULL);assert(worker);
        assert(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0);CloseHandle(worker);
        assert(SudekiMpLanStorySplitReturnedActorExact(&fixture_roster,c));
        SudekiMpLanStorySplitClearReturned();assert(!SudekiMpLanStorySplitReturnedActorExact(&fixture_roster,c));

#define RETURN_REFUSE(field,value) do { returned_fixture(c); __typeof__(field) saved=(field);(field)=(value); \
    assert(!SudekiMpLanStorySplitReturnedActorExact(&fixture_roster,c) && !returned.settled); \
    (field)=saved;assert(!SudekiMpLanStorySplitReturnedActorExact(&fixture_roster,c)); } while(0)
        RETURN_REFUSE(fixture_observer_exact,FALSE);
        RETURN_REFUSE(fixture_roster.epoch,8u);RETURN_REFUSE(fixture_roster.dispatch_serial,0);
        RETURN_REFUSE(fixture_roster.available_mask,(uint8_t)(15u&~(1u<<c)));
        RETURN_REFUSE(fixture_roster.leader_character,(uint8_t)c);
        RETURN_REFUSE(fixture_roster.world,NULL);RETURN_REFUSE(fixture_roster.descriptor,fixture_descriptor);
        RETURN_REFUSE(fixture_roster.group,NULL);RETURN_REFUSE(fixture_roster.controller,NULL);
        RETURN_REFUSE(fixture_roster.actors[c],NULL);RETURN_REFUSE(fixture_roster.ai[c],NULL);
        RETURN_REFUSE(fixture_roster.actors[(c+2u)%4u],NULL);
        RETURN_REFUSE(FPTR(base,WORLD),NULL);RETURN_REFUSE(FPTR(base,GROUP),NULL);
        RETURN_REFUSE(FPTR(base,CONTROLLER),NULL);RETURN_REFUSE(FPTR(fixture_world,0xc),fixture_descriptor);
        RETURN_REFUSE(FPTR(fixture_world,0x14),fixture_descriptor);
        RETURN_REFUSE(fixture_world[0x399],0);RETURN_REFUSE(fixture_world[0x39a],0);
        RETURN_REFUSE(FU32(fixture_exterior,0x34),4);RETURN_REFUSE(FPTR(fixture_exterior,0x14),fixture_data);
        RETURN_REFUSE(FPTR(fixture_exterior,0x24),NULL);RETURN_REFUSE(fixture_exterior_name[0],'x');
        RETURN_REFUSE(FU32(fixture_group,0xcc),1);RETURN_REFUSE(fixture_actors[c],NULL);
        RETURN_REFUSE(FPTR(fixture_actor_bytes[c],0x94),NULL);
        RETURN_REFUSE(FPTR(fixture_ai[c],0x10),NULL);
        RETURN_REFUSE(FPTR(fixture_controller,0x248),fixture_actor_bytes[c]);
        RETURN_REFUSE(attached,FALSE);RETURN_REFUSE(observer_set,FALSE);
        RETURN_REFUSE(returned.settled_at,GetTickCount()-(RETURN_MAX_AGE_MS+1u));
#undef RETURN_REFUSE
        returned_fixture(c);returned.outside=(uint8_t)(1u<<c);
        unsigned other=(c+2u)%4u;
        assert(!SudekiMpLanStorySplitReturnedActorExact(&fixture_roster,other));
        assert(SudekiMpLanStorySplitReturnedActorExact(&fixture_roster,c));
        returned_fixture(c);ended(TRUE);assert(!returned.ticket);
        returned_fixture(c);assert(begin_temp(fixture_name,fixture_roster.leader_character));
        assert(!returned.ticket && !SudekiMpLanStorySplitReturnedActorExact(&fixture_roster,c));ended(TRUE);

        residency_fixture(c);assert(begin_exit());return_settle();assert(!returned.ticket); /* missing receipt */
        residency_fixture(c);assert(begin_exit());return_receipt(42u,SUDEKIMP_STORY_TEMP_EXTERIOR_EXIT_BALANCED);
        return_settle();assert(!returned.ticket); /* wrong ticket */
        residency_fixture(c);assert(begin_exit());return_receipt(41u,SUDEKIMP_STORY_TEMP_EXTERIOR_EXIT_MISMATCH);
        return_settle();assert(!returned.ticket);
        residency_fixture(c);assert(begin_exit());return_receipt(41u,SUDEKIMP_STORY_TEMP_EXTERIOR_EXIT_BALANCED);
        return_receipt(41u,SUDEKIMP_STORY_TEMP_EXTERIOR_EXIT_BALANCED);return_settle();assert(!returned.ticket); /* duplicate */
        residency_fixture(c);residency.epoch=0;assert(begin_exit() && !returned.armed);
        return_receipt(41u,SUDEKIMP_STORY_TEMP_EXTERIOR_EXIT_BALANCED);return_settle();assert(!returned.ticket);
        residency_fixture(c);exempt_count=0;assert(begin_exit() && !returned.armed);
        return_receipt(41u,SUDEKIMP_STORY_TEMP_EXTERIOR_EXIT_BALANCED);return_settle();assert(!returned.ticket);
        returned_fixture(c); /* repeated visit gets an independent token */
        SudekiMpLanStorySplitClearReturned();
        printf("PASS named SplitReturnedResidency character=%u balanced_ticket_settlement_identity_expiry_and_ready_clear\n",c);
    }
}
static void image_globals_copy(void) {
    static const unsigned slots[3]={WORLD,GROUP,CONTROLLER};
    residency_fixture(2u); /* entire callback/capture path, before private write */
    for(unsigned i=0;i<3u;++i) {
        MEMORY_BASIC_INFORMATION m;
        assert(VirtualQuery(base+slots[i],&m,sizeof(m))==sizeof(m));
        assert(m.AllocationBase==base && m.Type==MEM_MAPPED && m.Protect==PAGE_WRITECOPY);
        assert(residency_slot_protection[i]==PAGE_WRITECOPY && !readable(base+slots[i],sizeof(void *)));
    }
    /* Read before and after a private COW write. The shared view remains
     * unchanged and Windows still reports COPY protection on the private view. */
    *(volatile uintptr_t *)(base+WORLD)=(uintptr_t)fixture_descriptor;
    assert(FPTR(fixture_image_seed,WORLD)==fixture_world);
    *(volatile uintptr_t *)(base+WORLD)=(uintptr_t)fixture_world;
    settled(TRUE);assert(residency.exact);
    for(unsigned i=0;i<3u;++i) assert(residency_slot_protection[i]==PAGE_WRITECOPY);
    assert(SudekiMpLanStorySplitExteriorActorExact(&fixture_roster,2u));
    /* All three native globals reside in this page. Querying protection must
     * refuse without touching guarded/inaccessible memory. */
    uint8_t *page=base+(WORLD&~0xfffu);DWORD old;
    static const DWORD deny[]={PAGE_NOACCESS,PAGE_WRITECOPY|PAGE_GUARD};
    for(unsigned i=0;i<sizeof(deny)/sizeof(*deny);++i) {
        assert(VirtualProtect(page,4096,deny[i],&old));
        capture_residency();assert(!residency.exact && !strcmp(residency_reason,"image_globals"));
        for(unsigned k=0;k<3u;++k) assert(residency_slot_protection[k]==deny[i]);
        assert(!SudekiMpLanStorySplitExteriorActorExact(&fixture_roster,2u));
        assert(VirtualProtect(page,4096,PAGE_WRITECOPY,&old));
        capture_residency();assert(residency.exact);
    }
    uint8_t *original=base;base+=4;
    capture_residency();assert(!residency.exact && !strcmp(residency_reason,"image_globals"));
    base=original;capture_residency();assert(residency.exact);
    ended(FALSE);
    puts("PASS named SplitImageGlobalsReadable FILE_MAP_COPY capture before/after private write; guard/noaccess/allocation refusal");
}
static void parking_cases(void);
static void exterior_residency(void) {
    HANDLE image_map=CreateFileMappingW(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,0x409000,NULL);assert(image_map);
    fixture_image_seed=MapViewOfFile(image_map,FILE_MAP_WRITE,0,0,0x409000);assert(fixture_image_seed);
    fixture_image=MapViewOfFile(image_map,FILE_MAP_COPY,0,0,0x409000);assert(fixture_image);
    fixture_heap=VirtualAlloc(NULL,0x4000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);assert(fixture_heap);
    MEMORY_BASIC_INFORMATION mapping;
    assert(VirtualQuery(fixture_heap,&mapping,sizeof(mapping))==sizeof(mapping));
    assert(mapping.Type==MEM_PRIVATE && mapping.State==MEM_COMMIT && mapping.Protect==PAGE_READWRITE);
    puts("CHECK named SplitResidencyFixture heap=private/readwrite image=FILE_MAP_COPY");
    parking_cases();
    image_globals_copy();
    returned_residency();
    for(unsigned c=0;c<4u;++c) {
        residency_fixture(c);
        uint8_t world_before[FIXTURE_WORLD_SIZE],group_before[FIXTURE_GROUP_SIZE];
        memcpy(world_before,fixture_world,sizeof(world_before));memcpy(group_before,fixture_group,sizeof(group_before));
        assert(SudekiMpLanStorySplitExteriorActorExact(&fixture_roster,c));
        assert(!memcmp(world_before,fixture_world,sizeof(world_before)) && !memcmp(group_before,fixture_group,sizeof(group_before)));
        assert(!SudekiMpLanStorySplitExteriorActorExact(NULL,c));
        assert(!SudekiMpLanStorySplitExteriorActorExact(&fixture_roster,4u));
        assert(!SudekiMpLanStorySplitExteriorActorExact(&fixture_roster,leader));
#define REFUSE_CHANGE(field,value) do { __typeof__(field) saved_value=(field);(field)=(value); \
    assert(!SudekiMpLanStorySplitExteriorActorExact(&fixture_roster,c));(field)=saved_value; } while(0)
        REFUSE_CHANGE(fixture_observer_exact,FALSE);
        REFUSE_CHANGE(fixture_roster.dispatch_serial,0);
        REFUSE_CHANGE(fixture_roster.epoch,0);
        REFUSE_CHANGE(attached,FALSE);REFUSE_CHANGE(observer_set,FALSE);REFUSE_CHANGE(active,FALSE);
        REFUSE_CHANGE(settled_inside,FALSE);REFUSE_CHANGE(residency.exact,FALSE);REFUSE_CHANGE(outside_mask,0);
        REFUSE_CHANGE(temporary_data,NULL);REFUSE_CHANGE(exempt_count,0);REFUSE_CHANGE(exempt_group,NULL);
        REFUSE_CHANGE(fixture_roster.world,NULL);REFUSE_CHANGE(fixture_roster.descriptor,NULL);
        REFUSE_CHANGE(fixture_roster.group,NULL);REFUSE_CHANGE(fixture_roster.controller,NULL);
        REFUSE_CHANGE(fixture_roster.available_mask,(uint8_t)(15u&~(1u<<c)));
        REFUSE_CHANGE(fixture_roster.leader_character,(uint8_t)c);
        REFUSE_CHANGE(fixture_roster.actors[c],NULL);REFUSE_CHANGE(fixture_roster.ai[c],NULL);
        unsigned other=(c+2u)%4u;REFUSE_CHANGE(fixture_roster.actors[other],NULL);
        REFUSE_CHANGE(FPTR(base,WORLD),NULL);REFUSE_CHANGE(FPTR(base,GROUP),NULL);
        REFUSE_CHANGE(FPTR(base,0x408da4u),NULL);REFUSE_CHANGE(FPTR(fixture_world,0xc),NULL);
        REFUSE_CHANGE(FPTR(fixture_world,0x14),fixture_descriptor);
        REFUSE_CHANGE(fixture_world[0x399],0);REFUSE_CHANGE(fixture_world[0x39a],0);
        REFUSE_CHANGE(FU32(fixture_descriptor,0x34),3);REFUSE_CHANGE(FPTR(fixture_descriptor,0x14),NULL);
        REFUSE_CHANGE(FPTR(fixture_descriptor,0x24),NULL);REFUSE_CHANGE(fixture_name[0],'x');
        REFUSE_CHANGE(FU32(fixture_group,0xd0),0);REFUSE_CHANGE(FU32(fixture_group,0xcc),1);
        REFUSE_CHANGE(fixture_actors[c],NULL);
        for(unsigned i=0;i<exempt_count;++i) if(exempt_actor[i]==fixture_roster.actors[c]) {
            REFUSE_CHANGE(exempt_actor[i],NULL);REFUSE_CHANGE(exempt_added[i],0);
        }
#undef REFUSE_CHANGE
        /* Native nested lead-only requests retain the same owned exemption. */
        FU32(fixture_group,0xd0)=2;assert(SudekiMpLanStorySplitExteriorActorExact(&fixture_roster,c));
        FU32(fixture_group,0xd0)=1;
        HANDLE worker=CreateThread(NULL,0,residency_foreign,NULL,0,NULL);assert(worker);
        assert(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0);CloseHandle(worker);
        assert(begin_exit());assert(!residency.exact && !SudekiMpLanStorySplitExteriorActorExact(&fixture_roster,c));
        settled(FALSE);assert(!SudekiMpLanStorySplitExteriorActorExact(&fixture_roster,c));
        ended(FALSE);assert(!SudekiMpLanStorySplitExteriorActorExact(&fixture_roster,c));
        residency_fixture(c);ended(TRUE);assert(!SudekiMpLanStorySplitExteriorActorExact(&fixture_roster,c));
        printf("PASS named SplitExteriorResidency character=%u exact_identity_and_transition_refusal\n",c);
    }
    residency_fixture(2u);
    assert(UnmapViewOfFile(fixture_image));
    capture_residency();assert(!residency.exact && !strcmp(residency_reason,"image_globals"));
    assert(!SudekiMpLanStorySplitExteriorActorExact(&fixture_roster,2u));
    puts("PASS named SplitImageGlobalsReadable unmapped image refuses capture");
    fixture_image=NULL;
    assert(UnmapViewOfFile(fixture_image_seed) && CloseHandle(image_map));fixture_image_seed=NULL;
    assert(VirtualFree(fixture_heap,0,MEM_RELEASE));fixture_heap=NULL;state();
}
static void __attribute__((naked)) fake_pause(void) {__asm__("incb 0x23(%eax); ret");}
static void __attribute__((naked)) fake_resume(void) {__asm__("decb 0x23(%esi); ret");}
static void __attribute__((thiscall)) fake_lead(void *group) {
    uint8_t *g=group;assert(!FU32(g,0xd0));FU32(g,0xd0)=1;
    for(unsigned i=1;i<4;++i)((uint8_t *)FPTR(g,0x90+i*12))[0x2b]+=2;
}
static void __attribute__((thiscall)) fake_full(void *group) {
    uint8_t *g=group;FU32(g,0xd0)=0;
    for(unsigned i=1;i<FU32(g,0xcc);++i)((uint8_t *)FPTR(g,0x90+i*12))[0x2b]-=2;
}
static void parking_cases(void) {
    residency_fixture(2);exempt_count=0;exempt_group=NULL;FU32(fixture_group,0xd0)=0;
    outside_mask=0;away_mask=4;lead_hook.trampoline=(void *)fake_lead;
    SudekiMpLanStorySplitNodePause=(void *)fake_pause;SudekiMpLanStorySplitNodeResume=(void *)fake_resume;
    fixture_actor_bytes[2][0x2b]=3;lead_only(fixture_group);
    assert(parked[2].actor==fixture_actor_bytes[2] && parked[2].added==2 && parked[2].refs_before==3);
    assert(fixture_actor_bytes[2][0x2b]==5 && !exempt_count);
    active=FALSE;assert(!SudekiMpLanStorySplitUninstall());active=TRUE;
    uint8_t changed_group[0xd4];memcpy(changed_group,fixture_group,sizeof(changed_group));
    for(unsigned guard=0;guard<6;++guard) {
        settled_inside=guard!=0;void *a=parked[2].actor,*g=parked[2].group;
        if(guard==1)parked[2].actor=fixture_actor_bytes[1];
        if(guard==2)parked[2].group=changed_group;
        if(guard==3)FU32(fixture_group,0xd0)=2;
        if(guard==4)++native_thread;
        if(guard==5)fixture_actor_bytes[2][0x2b]=1;
        assert(!SudekiMpLanStorySplitUnparkMember(2));
        settled_inside=TRUE;parked[2].actor=a;parked[2].group=g;FU32(fixture_group,0xd0)=1;
        native_thread=GetCurrentThreadId();fixture_actor_bytes[2][0x2b]=5;
    }
    assert(SudekiMpLanStorySplitUnparkMember(2) && fixture_actor_bytes[2][0x2b]==3 && !parked[2].actor);
    assert(SudekiMpLanStorySplitParkMember(2) && fixture_actor_bytes[2][0x2b]==5 && !exempt_count);
    assert(SudekiMpLanStorySplitParkMember(2) && fixture_actor_bytes[2][0x2b]==5);
    full_hook.trampoline=(void *)fake_full;uint8_t foreign_group[0xd4]={0};
    full_party(foreign_group);assert(parked[2].actor);
    full_party(fixture_group);assert(!parked[2].actor && fixture_actor_bytes[2][0x2b]==3);
    residency_fixture(2);exempt_count=0;exempt_group=NULL;FU32(fixture_group,0xd0)=0;
    outside_mask=0;away_mask=4;lead_only(fixture_group);
    assert(parked[2].actor && parking_held());
    ended(TRUE);assert(!parked[2].actor && !parking_held());
    state();outside_mask=0;away_mask=4;assert(begin_temp("LNBr_Kamo_shop",3));
    SudekiMpLanStoryTempExteriorEntry e={0};strcpy(e.destination,"LNBr_Kamo_shop");assert(keep_exterior(NULL,&e));
    state();outside_mask=away_mask=0;begin_no_members_reported=FALSE;unsigned count=diagnostic_count;
    for(unsigned i=0;i<10;++i)assert(!begin_temp("LNBr_Kamo_shop",3));
    assert(diagnostic_count==count+1);
}
int main(void) {
    kamo_lifecycle();
    exterior_residency();
    state();assert(SudekiMpLanStorySplitTestCanEnterTemporary("LICo_Frappe_Farm",3));
    HANDLE map=CreateFileMappingW(INVALID_HANDLE_VALUE,NULL,PAGE_EXECUTE_READWRITE,0,8192,NULL);assert(map);
    char *text=MapViewOfFile(map,FILE_MAP_ALL_ACCESS|FILE_MAP_EXECUTE,0,0,8192);assert(text);
    strcpy(text,"LICo_Frappe_Farm");
    static const DWORD protections[]={PAGE_READWRITE,PAGE_READONLY,PAGE_EXECUTE_READWRITE,
        PAGE_EXECUTE_READ,PAGE_WRITECOPY,PAGE_EXECUTE_WRITECOPY};
    for(unsigned i=0;i<sizeof(protections)/sizeof(*protections);++i) {
        DWORD old;MEMORY_BASIC_INFORMATION region;
        assert(VirtualProtect(text,4096,protections[i],&old));
        assert(VirtualQuery(text,&region,sizeof(region))==sizeof(region));
        printf("CHECK named SplitTestNameReadable protection=%08lx actual=%08lx\n",
            (unsigned long)protections[i],(unsigned long)region.Protect);
        assert((region.Protect&0xffu)==protections[i]);
        assert(SudekiMpLanStorySplitTestCanEnterTemporary(text,3));
    }
    DWORD old;assert(VirtualProtect(text,4096,PAGE_NOACCESS,&old));
    assert(!SudekiMpLanStorySplitTestCanEnterTemporary(text,3));
    assert(strstr(last_diagnostic,"reason=name_page protection=00000001"));
    unsigned logged=diagnostic_count;
    for(unsigned i=0;i<100u;++i) assert(!SudekiMpLanStorySplitTestCanEnterTemporary(text,3));
    assert(diagnostic_count==logged);
    assert(VirtualProtect(text,4096,PAGE_READWRITE|PAGE_GUARD,&old));
    assert(!SudekiMpLanStorySplitTestCanEnterTemporary(text,3));
    assert(VirtualProtect(text,4096,PAGE_READWRITE,&old));
    strcpy(text,"bad/name");assert(!SudekiMpLanStorySplitTestCanEnterTemporary(text,3));
    memset(text,'a',48);text[48]=0;assert(!SudekiMpLanStorySplitTestCanEnterTemporary(text,3));
    text[0]=0;assert(!SudekiMpLanStorySplitTestCanEnterTemporary(text,3));
    strcpy(text,"LICo_Frappe_Farm");
    outside_mask=8;assert(!SudekiMpLanStorySplitTestCanEnterTemporary(text,3));state();
    active=TRUE;assert(!SudekiMpLanStorySplitTestCanEnterTemporary(text,3));state();
    host_ready=FALSE;assert(!SudekiMpLanStorySplitTestCanEnterTemporary(text,3));state();
    attached=FALSE;assert(!SudekiMpLanStorySplitTestCanEnterTemporary(text,3));state();
    observer_set=FALSE;assert(!SudekiMpLanStorySplitTestCanEnterTemporary(text,3));state();
    base=NULL;assert(!SudekiMpLanStorySplitTestCanEnterTemporary(text,3));state();
    assert(!SudekiMpLanStorySplitTestCanEnterTemporary(text,4));
    assert(!SudekiMpLanStorySplitTestCanEnterTemporary(NULL,3));
    HANDLE worker=CreateThread(NULL,0,foreign,NULL,0,NULL);assert(worker);
    assert(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0);CloseHandle(worker);
    assert(UnmapViewOfFile(text) && CloseHandle(map));
    puts("PASS named SplitTestNameReadable copy-on-write names with guarded/inaccessible/state/thread rejection");
    puts("PASS named SplitTestPreflightDiagnostic bounded reason and protection");
    return 0;
}
