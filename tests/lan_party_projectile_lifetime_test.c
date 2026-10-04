/* Exact retail TPtr copy/base-destructor execution on inert objects, plus
 * journal policy cases. Does not execute native missile gameplay or prove
 * that live AIM notifications capture every asynchronous object. */
#include "../src/hooks/lan_party_projectile_lifetime.c"
#include <assert.h>
#include <stdio.h>

static uint8_t *mapped;
static uint8_t actor[0xc0], managers[JOURNAL_CAPACITY+2][0xdc];
static uint8_t missiles[JOURNAL_CAPACITY+SOURCE_COUNT][0xa4];
static uint8_t entities[JOURNAL_CAPACITY+SOURCE_COUNT][0x100];
static unsigned missile_count;
static BOOL namespace_on;
static SudekiMpLanPartyEffectOwner fixture_owner;
BOOL SudekiMpLanPartyEffectLifetimeCurrent(SudekiMpLanPartyEffectOwner *owner) {
    if(SudekiMpLanPartyProjectileLifetimeCurrent(owner)) return namespace_valid(owner);
    *owner=fixture_owner; return namespace_on;
}

static uint8_t *map_exact_file(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(file!=INVALID_HANDLE_VALUE);
    DWORD size=GetFileSize(file,NULL),got=0;
    uint8_t *raw=HeapAlloc(GetProcessHeap(),0,size);
    assert(raw && ReadFile(file,raw,size,&got,NULL) && got==size);
    CloseHandle(file);
    IMAGE_DOS_HEADER *dos=(IMAGE_DOS_HEADER *)raw;
    IMAGE_NT_HEADERS32 *nt=(IMAGE_NT_HEADERS32 *)(raw+dos->e_lfanew);
    uint8_t *base=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,
        MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    assert(base);
    memcpy(base,raw,nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *sections=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i)
        memcpy(base+sections[i].VirtualAddress,raw+sections[i].PointerToRawData,
            sections[i].SizeOfRawData);
    uintptr_t delta=(uintptr_t)base-nt->OptionalHeader.ImageBase;
    IMAGE_DATA_DIRECTORY reloc=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    unsigned offset=0;
    while(offset<reloc.Size) {
        IMAGE_BASE_RELOCATION *block=(IMAGE_BASE_RELOCATION *)(base+reloc.VirtualAddress+offset);
        assert(block->SizeOfBlock>=sizeof(*block) && block->SizeOfBlock<=reloc.Size-offset);
        unsigned count=(block->SizeOfBlock-sizeof(*block))/2;
        uint16_t *entries=(uint16_t *)(block+1);
        for(unsigned i=0;i<count;++i) {
            unsigned type=entries[i]>>12, rva=block->VirtualAddress+(entries[i]&0xfffu);
            assert(type==IMAGE_REL_BASED_ABSOLUTE || type==IMAGE_REL_BASED_HIGHLOW);
            if(type==IMAGE_REL_BASED_HIGHLOW) *(uint32_t *)(base+rva)+=(uint32_t)delta;
        }
        offset+=block->SizeOfBlock;
    }
    HeapFree(GetProcessHeap(),0,raw);
    assert(FlushInstructionCache(GetCurrentProcess(),base,SUDEKIMP_EXPECTED_IMAGE_SIZE));
    return base;
}
static void setup(unsigned character) {
    assert(!SudekiMpLanPartyProjectileLifetimeRetains());
    assert(SudekiMpLanPartyProjectileLifetimeInstall((HMODULE)mapped));
    memset(actor,0,sizeof(actor)); memset(managers,0,sizeof(managers));
    memset(missiles,0,sizeof(missiles)); memset(entities,0,sizeof(entities));
    *(void **)actor=mapped+(character==1?ELCO_VT:AILISH_VT);
    missile_count=0;
}
static void *launch(unsigned manager_index,unsigned count) {
    assert(manager_index<JOURNAL_CAPACITY+2 && count<=SOURCE_COUNT);
    uint8_t *manager=managers[manager_index];
    assert(!*(void **)manager);
    *(void **)manager=mapped+MANAGER_VT;
    *(void **)(manager+0x10)=actor; *(void **)(actor+0xbc)=manager;
    for(unsigned i=0;i<count;++i) {
        assert(missile_count<JOURNAL_CAPACITY+SOURCE_COUNT);
        uint8_t *m=missiles[missile_count], *entity=entities[missile_count++];
        *(void **)m=mapped+MISSILE_VT; *(void **)(m+4)=mapped+MISSILE_SECONDARY_VT;
        *(void **)(m+0x58)=actor; *(void **)(m+0x10)=entity;
        *(void **)entity=mapped+0x2d5a10;
        *(void **)(entity+0xfc)=m;
        ProjectileTPtr seed={m+4,NULL,NULL};
        copy_observer((ProjectileTPtr *)(manager+SOURCE_OFFSET)+i,&seed);
    }
    return manager;
}
static void destroy(unsigned missile) {
    assert(missile<missile_count);
    /* This exact function only clears intrusive observers. Calling the full
     * missile destructor on an inert missile would require a running world. */
    typedef void (__attribute__((thiscall)) *DestroyBase)(void *object);
    ((DestroyBase)(mapped+TPTR_DESTRUCT))(missiles[missile]+4);
}
static void destroy_all(void) {
    for(unsigned i=0;i<missile_count;++i) destroy(i);
}
static SudekiMpLanPartyProjectileTag tag(unsigned character,unsigned generation) {
    SudekiMpLanPartyProjectileTag t={0};
    t.session=UINT64_C(0x12345678abcdef01); t.generation=generation;
    t.sequence=(uint16_t)(generation-1); t.character=(uint8_t)character;
    t.item=(uint8_t)(character==1?24:12); t.actor=actor;
    return t;
}
static void fixture_reset_fault(void) {
    /* Test isolation only, never an exported/native reset. First execute the
     * destructor for every inert object and prove no cell remains linked. */
    destroy_all();
    assert(!SudekiMpLanPartyProjectileLifetimePoll());
    for(unsigned i=0;i<JOURNAL_CAPACITY;++i) assert(empty(&journal[i].observer));
    for(unsigned i=0;i<DESCENDANT_CAPACITY;++i) assert(empty(&descendants[i].observer));
    assert(!SudekiMpLanPartyProjectileLifetimeUninstall());
    assert(SudekiMpLanPartyProjectileLifetimeRetains());
    memset(journal,0,sizeof(journal));
    InterlockedExchange(&active_count,0); InterlockedExchange(&unknown,0);
    assert(SudekiMpLanPartyProjectileLifetimeUninstall());
}
static DWORD WINAPI wrong_poll(void *unused) {
    (void)unused;
    assert(!SudekiMpLanPartyProjectileLifetimePoll());
    assert(GetLastError()==ERROR_INVALID_THREAD_ID);
    return 0;
}
static void run_foreign_thread(LPTHREAD_START_ROUTINE start) {
    HANDLE thread=CreateThread(NULL,0,start,NULL,0,NULL);
    assert(thread && WaitForSingleObject(thread,5000)==WAIT_OBJECT_0);
    CloseHandle(thread);
}
static void simple_lifetime(unsigned character) {
    setup(character);
    SudekiMpLanPartyProjectileTag t=tag(character,1);
    void *manager=launch(0,2);
    assert(SudekiMpLanPartyProjectileLifetimeCapture(&t,manager));
    assert(active_count==2 && SudekiMpLanPartyProjectileLifetimeRetains());
    assert(SudekiMpLanPartyProjectileLifetimeCapture(&t,manager) && active_count==2);
    assert(!SudekiMpLanPartyProjectileLifetimeUninstall());
    run_foreign_thread(wrong_poll);
    assert(!unknown && SudekiMpLanPartyProjectileLifetimePoll() && active_count==2);
    /* Removing the native manager's first reference does not end the missile.
     * Model its unlink on inert fixture cells, retaining the journal observer. */
    ProjectileTPtr *source=(ProjectileTPtr *)((uint8_t *)manager+SOURCE_OFFSET);
    assert(source->previous && !source->next);
    source->previous->next=NULL; memset(source,0,sizeof(*source));
    assert(SudekiMpLanPartyProjectileLifetimePoll() && active_count==2);
    destroy(0);
    assert(SudekiMpLanPartyProjectileLifetimePoll() && active_count==1);
    /* No dependence on the current actor/controller/session is required to
     * acknowledge an old exact weak observer's terminal state. */
    memset(actor,0,sizeof(actor));
    destroy(1);
    assert(SudekiMpLanPartyProjectileLifetimePoll() && active_count==0);
    assert(!SudekiMpLanPartyProjectileLifetimeRetains());
    assert(SudekiMpLanPartyProjectileLifetimeUninstall());
}
static void sequential_emissions(void) {
    setup(1);
    SudekiMpLanPartyProjectileTag first=tag(1,1), second=tag(1,2);
    void *manager=launch(0,1);
    assert(SudekiMpLanPartyProjectileLifetimeCapture(&first,manager));
    ProjectileTPtr *old=(ProjectileTPtr *)((uint8_t *)manager+SOURCE_OFFSET);
    assert(old->previous && !old->next);
    old->previous->next=NULL; memset(old,0,sizeof(*old));
    memset(manager,0,sizeof(managers[0]));
    manager=launch(0,1); /* same manager, fresh native allocation */
    assert(SudekiMpLanPartyProjectileLifetimeCapture(&second,manager));
    assert(active_count==2 && journal[0].tag.generation==1 && journal[1].tag.generation==2);
    assert(journal[0].observer.object==missiles[0]+4 &&
        journal[1].observer.object==missiles[1]+4);
    destroy_all(); assert(SudekiMpLanPartyProjectileLifetimePoll());
    assert(!SudekiMpLanPartyProjectileLifetimeRetains());
    assert(SudekiMpLanPartyProjectileLifetimeUninstall());
}
static void unknown_cases(void) {
    setup(1);
    SudekiMpLanPartyProjectileTag t=tag(1,1);
    void *manager=launch(0,1);
    assert(SudekiMpLanPartyProjectileLifetimeCapture(&t,manager));
    ++t.generation;
    assert(!SudekiMpLanPartyProjectileLifetimeCapture(&t,manager));
    assert(active_count==1 && unknown);
    fixture_reset_fault();

    setup(3); t=tag(3,1); manager=launch(0,1);
    *(void **)(missiles[0]+0x58)=managers[1];
    assert(!SudekiMpLanPartyProjectileLifetimeCapture(&t,manager));
    assert(!active_count && unknown); fixture_reset_fault();

    setup(1); t=tag(1,1); manager=launch(0,1);
    assert(SudekiMpLanPartyProjectileLifetimeCapture(&t,manager));
    ProjectileTPtr saved=journal[0].observer;
    journal[0].observer.object=NULL; /* not triple-null: observation failure */
    assert(!SudekiMpLanPartyProjectileLifetimePoll() && active_count==1 && unknown);
    journal[0].observer=saved; fixture_reset_fault();

    setup(1); t=tag(1,1); manager=launch(0,0);
    assert(!SudekiMpLanPartyProjectileLifetimeCapture(&t,manager) && unknown);
    fixture_reset_fault();

    setup(1); t=tag(1,1); manager=launch(0,2);
    ((ProjectileTPtr *)((uint8_t *)manager+SOURCE_OFFSET))[2].next=manager;
    assert(!SudekiMpLanPartyProjectileLifetimeCapture(&t,manager) && !active_count);
    ((ProjectileTPtr *)((uint8_t *)manager+SOURCE_OFFSET))[2].next=NULL;
    fixture_reset_fault();
}
static void capacity(void) {
    setup(1);
    unsigned emitted=0,manager=0;
    while(emitted<JOURNAL_CAPACITY) {
        unsigned count=JOURNAL_CAPACITY-emitted;
        if(count>SOURCE_COUNT) count=SOURCE_COUNT;
        SudekiMpLanPartyProjectileTag t=tag(1,manager+1);
        assert(SudekiMpLanPartyProjectileLifetimeCapture(&t,launch(manager,count)));
        emitted+=count; ++manager;
    }
    assert(active_count==JOURNAL_CAPACITY);
    SudekiMpLanPartyProjectileTag t=tag(1,manager+1);
    assert(!SudekiMpLanPartyProjectileLifetimeCapture(&t,launch(manager,1)));
    assert(active_count==JOURNAL_CAPACITY && unknown);
    fixture_reset_fault();
}
static void descendant_lifetime(void) {
    setup(1);
    fixture_owner=(SudekiMpLanPartyEffectOwner){77,9,actor,0x0e};
    void *manager=launch(0,1);
    /* A namespace observer and an ordinary emission observer are independent.
     * Their generations belong to different domains and must not be deduped. */
    SudekiMpLanPartyProjectileTag t=tag(1,500);
    assert(SudekiMpLanPartyProjectileLifetimeCaptureOwned(&t,&fixture_owner,manager));
    assert(descendant_count==1 && active_count==1);
    SudekiMpLanPartyEffectOwner out={0};
    assert(!SudekiMpLanPartyProjectileLifetimeCurrent(&out));
    scope_enter(missiles[0]);
    assert(SudekiMpLanPartyProjectileLifetimeCurrent(&out) && namespace_equal(&out,&fixture_owner));
    assert(!SudekiMpLanPartyProjectileLifetimePoll());
    scope_enter(missiles[1]); /* Explicit unowned nested scope masks parent. */
    assert(SudekiMpLanPartyProjectileLifetimeCurrent(&out) && !namespace_valid(&out));
    assert(!SudekiMpLanPartyEffectLifetimeCurrent(&out));
    scope_leave(TRUE);
    assert(SudekiMpLanPartyProjectileLifetimeCurrent(&out) && namespace_equal(&out,&fixture_owner));
    /* Root attribution may disappear; the missile still owns impact children. */
    namespace_on=FALSE;
    destroy_all();
    assert(SudekiMpLanPartyProjectileLifetimeCurrent(&out) && namespace_equal(&out,&fixture_owner));
    scope_leave(TRUE);
    assert(SudekiMpLanPartyProjectileLifetimePoll());
    assert(!descendant_count && !active_count && !SudekiMpLanPartyProjectileLifetimeRetains());
    assert(SudekiMpLanPartyProjectileLifetimeUninstall());

    setup(3); t=tag(3,1); manager=launch(0,1);
    fixture_owner=(SudekiMpLanPartyEffectOwner){78,10,actor,0x01};
    assert(SudekiMpLanPartyProjectileLifetimeCaptureOwned(&t,&fixture_owner,manager));
    ++fixture_owner.generation;
    assert(!SudekiMpLanPartyProjectileLifetimeCaptureOwned(&t,&fixture_owner,manager));
    assert(unknown && descendant_count==1 && active_count==1);
    fixture_reset_fault();
}
static uint8_t constructed_entity[0x600];
static unsigned update_calls,termination_calls;
__attribute__((naked)) static void construct_stub(void) {
    __asm__ volatile("movl %edi,%eax\n\tret\n\t");
}
static DWORD __attribute__((stdcall)) update_stub(void *missile,void *data) {
    SudekiMpLanPartyEffectOwner current={0};
    assert(missile==constructed_entity+0x414 && data==actor);
    assert(SudekiMpLanPartyProjectileLifetimeCurrent(&current) && namespace_equal(&current,&fixture_owner));
    ++update_calls; return 0x11223344;
}
__attribute__((noinline,used)) static void terminate_record(void *missile) {
    SudekiMpLanPartyEffectOwner current={0};
    assert(missile==constructed_entity+0x414);
    assert(SudekiMpLanPartyProjectileLifetimeCurrent(&current) && namespace_equal(&current,&fixture_owner));
    ++termination_calls;
}
__attribute__((naked)) static void terminate_stub(void) {
    __asm__ volatile("pushl %eax\n\tcall _terminate_record\n\taddl $4,%esp\n\tmovl $0x55667788,%eax\n\tret\n\t");
}
static void bridge_abi(void) {
    setup(1);
    memset(constructed_entity,0,sizeof(constructed_entity));
    *(void **)constructed_entity=mapped+0x2d5a10;
    uint8_t *missile=constructed_entity+0x414;
    *(void **)missile=mapped+MISSILE_VT;
    *(void **)(missile+4)=mapped+MISSILE_SECONDARY_VT;
    fixture_owner=(SudekiMpLanPartyEffectOwner){88,15,actor,0x0e}; namespace_on=TRUE;
    void *saved=construct_original; construct_original=construct_stub;
    uintptr_t entity_arg=(uintptr_t)constructed_entity, missile_arg=(uintptr_t)missile,result;
    void *entry=construct_entry;
    __asm__ volatile("call *%[entry]" : "=a"(result), "+S"(entity_arg), "+D"(missile_arg)
        : [entry] "r"(entry) : "ecx","edx","memory","cc");
    construct_original=saved;
    assert(result==(uintptr_t)missile && descendant_count==1 && !callbacks);
    namespace_on=FALSE;
    saved=update_original; update_original=update_stub;
    entry=update_entry;
    __asm__ volatile("pushl %[data]\n\tpushl %[missile]\n\tcall *%[entry]"
        : "=a"(result) : [entry] "r"(entry), [data] "r"(actor), [missile] "r"(missile)
        : "ecx","edx","memory","cc");
    assert(result==0x11223344 && update_calls==1);
    update_original=saved;
    saved=terminate_original; terminate_original=terminate_stub;
    result=(uintptr_t)missile; entry=terminate_entry;
    __asm__ volatile("call *%[entry]" : "+a"(result)
        : [entry] "r"(entry) : "ecx","edx","memory","cc");
    terminate_original=saved;
    assert(result==0x55667788 && termination_calls==1 && !scope_depth && !callbacks);
    typedef void (__attribute__((thiscall)) *DestroyBase)(void *);
    ((DestroyBase)(mapped+TPTR_DESTRUCT))(missile+4);
    assert(SudekiMpLanPartyProjectileLifetimePoll());
    assert(!SudekiMpLanPartyProjectileLifetimeRetains());
    assert(SudekiMpLanPartyProjectileLifetimeUninstall());
}
static void hook_restore_retry(void) {
    setup(1);
    assert(hooks_exact());
    /* A foreign callsite is unknown ownership; preserve original callbacks
     * and image state for a checked retry instead of reporting teardown. */
    uint8_t saved=mapped[terminate_calls[3]];
    mapped[terminate_calls[3]]=0x90;
    assert(!SudekiMpLanPartyProjectileLifetimeUninstall());
    assert(image==mapped && restore_failed && SudekiMpLanPartyProjectileLifetimeRetains());
    assert(terminate_original==mapped+TERMINATE && terminate_hooks[3].installed);
    mapped[terminate_calls[3]]=saved;
    assert(SudekiMpLanPartyProjectileLifetimeUninstall());
    assert(!SudekiMpLanPartyProjectileLifetimeRetains() && emission_image_matches(mapped));
}
static unsigned retire_calls;
__attribute__((noinline,used)) static void retire_record(void *missile) {
    SudekiMpLanPartyEffectOwner owner={0};
    assert(missile==missiles[0] || missile==missiles[1]);
    assert(SudekiMpLanPartyProjectileLifetimeCurrent(&owner) && namespace_equal(&owner,&fixture_owner));
    assert(SudekiMpLanPartyProjectileLifetimeRetains());
    assert(!operation); /* Descendant callbacks may register during cleanup. */
    ++retire_calls;
}
__attribute__((naked)) static void retire_stub(void) {
    __asm__ volatile("pushl %eax\n\tcall _retire_record\n\taddl $4,%esp\n\tret\n\t");
}
static DWORD WINAPI wrong_retire(void *unused) {
    (void)unused;
    assert(!SudekiMpLanPartyProjectileLifetimeRequestRetire());
    assert(GetLastError()==ERROR_INVALID_THREAD_ID);
    return 0;
}
static DWORD WINAPI wrong_observe(void *unused) {
    (void)unused;
    unsigned count=77;
    assert(!SudekiMpLanPartyProjectileLifetimeObserve(NULL,0,&count));
    assert(count==77 && GetLastError()==ERROR_INVALID_THREAD_ID);
    return 0;
}
static void owned_observation(void) {
    setup(3);
    unsigned count=77;
    SudekiMpLanPartyProjectileObservation out[2],saved[2];
    memset(out,0x5a,sizeof(out)); memcpy(saved,out,sizeof(out));
    assert(SudekiMpLanPartyProjectileLifetimeObserve(NULL,0,&count) && !count && !native_thread);
    fixture_owner=(SudekiMpLanPartyEffectOwner){89,18,actor,0x01};
    SudekiMpLanPartyProjectileTag t=tag(3,700);
    void *manager=launch(0,2);
    assert(SudekiMpLanPartyProjectileLifetimeCapture(&t,manager));
    count=77;
    assert(!SudekiMpLanPartyProjectileLifetimeObserve(out,2,&count));
    assert(count==77 && !memcmp(out,saved,sizeof(out))); /* unowned != empty */
    assert(SudekiMpLanPartyProjectileLifetimeCaptureOwned(&t,&fixture_owner,manager));
    run_foreign_thread(wrong_observe);
    assert(!SudekiMpLanPartyProjectileLifetimeObserve(out,1,&count));
    assert(GetLastError()==ERROR_INSUFFICIENT_BUFFER && count==77 && !memcmp(out,saved,sizeof(out)));
    assert(SudekiMpLanPartyProjectileLifetimeObserve(out,2,&count) && count==2);
    assert(out[0].missile==missiles[0] && out[0].entity==entities[0] && out[0].incarnation);
    assert(out[1].incarnation>out[0].incarnation && namespace_equal(&out[0].owner,&fixture_owner));
    assert(!out[0].termination_entered && !out[0].termination_returned);
    uint32_t prior_incarnation=out[1].incarnation;
    BOOL entered=termination_scope_enter(missiles[0]);
    assert(entered); scope_leave(entered);
    assert(SudekiMpLanPartyProjectileLifetimeObserve(out,2,&count));
    assert(out[0].termination_entered && !out[0].termination_returned);
    termination_scope_returned(missiles[0]);
    assert(SudekiMpLanPartyProjectileLifetimeObserve(out,2,&count));
    assert(out[0].termination_returned && SudekiMpLanPartyProjectileLifetimeRetains());
    destroy_all();
    assert(SudekiMpLanPartyProjectileLifetimeObserve(out,2,&count) && !count);
    assert(descendant_count==2); /* Snapshot does not discard obligations. */
    assert(SudekiMpLanPartyProjectileLifetimePoll());
    assert(SudekiMpLanPartyProjectileLifetimeUninstall());

    setup(3); /* Allocator reuses both fixture addresses; identity must not. */
    t=tag(3,701); manager=launch(0,1);
    assert(SudekiMpLanPartyProjectileLifetimeCaptureOwned(&t,&fixture_owner,manager));
    assert(SudekiMpLanPartyProjectileLifetimeObserve(out,2,&count) && count==1);
    assert(out[0].entity==entities[0] && out[0].incarnation>prior_incarnation);
    memcpy(saved,out,sizeof(out)); count=77;
    *(void **)(missiles[0]+0x10)=entities[1];
    assert(!SudekiMpLanPartyProjectileLifetimeObserve(out,2,&count));
    assert(unknown && count==77 && !memcmp(out,saved,sizeof(out)));
    *(void **)(missiles[0]+0x10)=entities[0]; fixture_reset_fault();
}
static void shutdown_retire(void) {
    setup(1);
    fixture_owner=(SudekiMpLanPartyEffectOwner){79,17,actor,0x0e};
    SudekiMpLanPartyProjectileTag t=tag(1,600);
    assert(SudekiMpLanPartyProjectileLifetimeCaptureOwned(&t,&fixture_owner,launch(0,2)));
    run_foreign_thread(wrong_retire);
    assert(!unknown && !retire_calls);
    void *saved=terminate_original; terminate_original=retire_stub;
    /* One native termination has already occurred. Shutdown must not queue
     * that same entity again, even though its observer remains alive. */
    uintptr_t argument=(uintptr_t)missiles[0]; void *entry=terminate_entry;
    __asm__ volatile("call *%[entry]" : "+a"(argument)
        : [entry] "r"(entry) : "ecx","edx","memory","cc");
    assert(retire_calls==1 && journal[0].termination_entered && descendants[0].termination_entered);
    assert(descendants[0].termination_returned);
    assert(SudekiMpLanPartyProjectileLifetimeRequestRetire());
    assert(retire_calls==2 && active_count==2 && descendant_count==2);
    assert(SudekiMpLanPartyProjectileLifetimeRequestRetire() && retire_calls==2);
    assert(!SudekiMpLanPartyProjectileLifetimeUninstall());
    terminate_original=saved;
    destroy_all();
    assert(SudekiMpLanPartyProjectileLifetimePoll());
    assert(SudekiMpLanPartyProjectileLifetimeRequestRetire());
    assert(!SudekiMpLanPartyProjectileLifetimeRetains());
    assert(SudekiMpLanPartyProjectileLifetimeUninstall());

    setup(1); t=tag(1,601);
    assert(SudekiMpLanPartyProjectileLifetimeCapture(&t,launch(0,1)));
    assert(!SudekiMpLanPartyProjectileLifetimeRequestRetire() && unknown);
    fixture_reset_fault(); /* No native namespace: retain, do not call cleanup. */
}
int wmain(int argc,wchar_t **argv) {
    if(argc!=2) {
        fputs("usage: SudekiMP.ProjectileLifetimeTest.exe SUDEKI.exe\n",stderr);
        return 2;
    }
    mapped=map_exact_file(argv[1]);
    assert(SudekiMpLanPartyProjectileLifetimePoll());
    assert(!native_thread && !SudekiMpLanPartyProjectileLifetimeRetains());
    assert(image_matches((HMODULE)mapped));
    const unsigned guards[]={TPTR_COPY,TPTR_COPY+43,TPTR_DESTRUCT,TPTR_DESTRUCT+8,
        TPTR_DESTRUCT+82,MISSILE_SECONDARY_VT,0x187960,MISSILE_DELETING+3,MISSILE_DESTRUCT+10,
        MISSILE_DESTRUCT+19,MISSILE_DESTRUCT_TAIL+10,MISSILE_DESTRUCT_TAIL+32};
    for(unsigned i=0;i<sizeof(guards)/sizeof(*guards);++i) {
        mapped[guards[i]]^=1;
        assert(!SudekiMpLanPartyProjectileLifetimeInstall((HMODULE)mapped));
        assert(!SudekiMpLanPartyProjectileLifetimeRetains());
        mapped[guards[i]]^=1;
    }
    simple_lifetime(1); simple_lifetime(3); sequential_emissions();
    unknown_cases(); capacity(); descendant_lifetime(); bridge_abi(); hook_restore_retry(); shutdown_retire();
    owned_observation();
    assert(!SudekiMpLanPartyProjectileLifetimeRetains());
    assert(VirtualFree(mapped,0,MEM_RELEASE));
    puts("PASS: projectile observer signatures, native copy/destructor, exact tags, terminal drain and retained faults");
    return 0;
}
