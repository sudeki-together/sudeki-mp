/* Exact-image caller experiment plus separately linked dormant admission guard.
 * Native menu
 * dispatch, menu action and save-selection wrappers execute with synthetic
 * objects and recording stubs for scripts, loading, audio and world reset.
 * RemoveAllZones also executes actual native empty-world context cleanup;
 * no descriptors, terrain or child resources exist in that particular fixture.
 * No game process, window, save, native task or resource is created/destroyed.
 * Default mode contrasts a fixture-only early gate with a late skip. Other
 * modes exercise the real guard, its ABI and injected startup rollback/retry.
 * Neither safe script continuation nor lifetime ownership is established. */
#include "engine/build_identity.h"
#include "hooks/call_hook.h"
#include "hooks/lan_story_area_intent.h"
#include "hooks/save_book_intercept.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(__GNUC__) || !defined(__i386__)
#error "This experiment requires the supported x86 ABI"
#endif
enum { ACTION=0x164b40, NATIVE_INPUT=0x164f80, RELOAD=0x1639e0, QUIT=0xa2740,
    REMOVE=0x7950, REMOVE_ALL=0x5f70, WORLD=0x408d10,
    LOAD=0x101690, INDEX=0x100160, READER=0x100280,
    SCENE=0x408d1c, SCRIPTS=0x408d9c, SPEED=0x408da0, FADE=0x408d50,
    RECORDS=0x34b324, SELECTED=0x34b328, COUNT=0x34b32c, CATALOG=0x34b320 };
static uint8_t *image,scene[0x178],resident[0xf0],title[0x1844],menu[0xa0];
static uint8_t script[0x40],speed[0x30],fade[0x80],panel[0x710],widget[0x20];
static uint8_t catalog[3*0x2c8];
static uint8_t remove_world[0x3a0];
static void *menu_vtable[0x48/4],*widget_vtable[0x18/4];
static unsigned events,dispatches,quits,reads,cues,menu_closes,menu_resets,widgets;
static unsigned fades,pumps,backgrounds,reader_result,reader_slot,order,event_order,quit_order;
static BOOL change_selection,late_refusal;
static unsigned held __attribute__((used)),deferrals __attribute__((used));
static void *action_original __attribute__((used));
static BOOL witness_ok=TRUE;
static SudekiMpControlUpdateDispatchWitness witness;
static int intent_consumer,other_consumer;
static unsigned admission_calls,decision=SUDEKIMP_AREA_INTENT_DEFER;
static BOOL nested_input,nested_export,nested_load,nested_remove;
static uint64_t newest_ticket;
/* Link-time interposition leaves the production module separately compiled.
 * Inject before/after each of the four patches and during restoration; never reset
 * its private state or replace its admission/lifecycle implementation. */
static BOOL fail_install_before,fail_install_after,fail_restore;
static unsigned install_calls,restore_calls,fail_before_stage,fail_after_stage,fail_restore_stage;
BOOL __real_SudekiMpInstallInlineHook(SudekiMpInlineHook *,uint8_t *,const uint8_t *,size_t,const void *);
BOOL __real_SudekiMpRestoreInlineHook(SudekiMpInlineHook *);
BOOL __wrap_SudekiMpInstallInlineHook(SudekiMpInlineHook *h,uint8_t *p,const uint8_t *e,size_t n,const void *r) {
    ++install_calls;
    if(fail_install_before || install_calls==fail_before_stage) {SetLastError(ERROR_WRITE_FAULT);return FALSE;}
    BOOL ok=__real_SudekiMpInstallInlineHook(h,p,e,n,r);
    if(ok && (fail_install_after || install_calls==fail_after_stage)) {SetLastError(ERROR_WRITE_FAULT);return FALSE;}
    return ok;
}
BOOL __wrap_SudekiMpRestoreInlineHook(SudekiMpInlineHook *h) {
    if(h->installed && (++restore_calls==fail_restore_stage || fail_restore)) {SetLastError(ERROR_ACCESS_DENIED);return FALSE;}
    return __real_SudekiMpRestoreInlineHook(h);
}
BOOL SudekiMpControlSeparationUpdateDispatchWitnessStillExact(const SudekiMpControlUpdateDispatchWitness *w) {
    return witness_ok && w && w->native_thread_id==GetCurrentThreadId();
}
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(file!=INVALID_HANDLE_VALUE);DWORD size=GetFileSize(file,NULL),got=0;
    uint8_t *raw=malloc(size);assert(raw && ReadFile(file,raw,size,&got,NULL) && got==size);CloseHandle(file);
    IMAGE_DOS_HEADER *dos=(void *)raw;IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *b=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(b);memcpy(b,raw,nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *sections=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        IMAGE_SECTION_HEADER *s=&sections[i];
        assert(s->PointerToRawData<=size && s->SizeOfRawData<=size-s->PointerToRawData);
        assert(s->VirtualAddress<=nt->OptionalHeader.SizeOfImage &&
            s->SizeOfRawData<=nt->OptionalHeader.SizeOfImage-s->VirtualAddress);
        memcpy(b+s->VirtualAddress,raw+s->PointerToRawData,s->SizeOfRawData);
    }
    IMAGE_DATA_DIRECTORY reloc=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    uintptr_t delta=(uintptr_t)b-nt->OptionalHeader.ImageBase;
    for(unsigned offset=0;offset<reloc.Size;) {
        IMAGE_BASE_RELOCATION *block=(void *)(b+reloc.VirtualAddress+offset);
        assert(block->SizeOfBlock>=sizeof(*block) && block->SizeOfBlock<=reloc.Size-offset);
        uint16_t *items=(void *)(block+1);unsigned count=(block->SizeOfBlock-sizeof(*block))/2;
        for(unsigned i=0;i<count;++i) {
            unsigned type=items[i]>>12,rva=block->VirtualAddress+(items[i]&0xfff);
            assert(type==IMAGE_REL_BASED_ABSOLUTE || type==IMAGE_REL_BASED_HIGHLOW);
            if(type==IMAGE_REL_BASED_HIGHLOW) {
                assert(rva<=nt->OptionalHeader.SizeOfImage-4);*(uint32_t *)(b+rva)+=(uint32_t)delta;
            }
        }
        offset+=block->SizeOfBlock;
    }
    free(raw);return b;
}
static void __attribute__((naked,noinline)) action_gate(void) {
    __asm__ volatile("pushfl; cmpl $0,_held; je 1f; test %eax,%eax; je 2f;"
        "cmp $2,%eax; jne 1f; 2: incl _deferrals; popfl; ret;"
        "1: popfl; jmp *_action_original");
}
static void __attribute__((stdcall,force_align_arg_pointer)) event_stub(const char *name) {
    assert(!strcmp(name,"OnMainMenuQuit"));++events;event_order=++order;
}
static void __attribute__((stdcall,force_align_arg_pointer)) dispatch_stub(
    uintptr_t a,uintptr_t b,uintptr_t c,unsigned d,unsigned e,unsigned f,unsigned g) {
    (void)a;(void)c;(void)f;assert(b && d==1 && e==1 && !g);++dispatches;
    /* Native caller releases the returned script handle. Supply a null
     * fixture handle; leaving this output unset would consume stack garbage. */
    *(uint32_t *)b=0;
    if(change_selection) *(int *)(image+SELECTED)=2;
}
static void __attribute__((force_align_arg_pointer)) empty_stub(void) {}
static void __attribute__((stdcall,force_align_arg_pointer)) cue_stub(void *p) {assert(p==scene);++cues;}
static void __attribute__((stdcall,force_align_arg_pointer)) menu_reset_stub(void *p) {assert(p==menu);++menu_resets;}
static void __attribute__((thiscall,force_align_arg_pointer)) menu_close_stub(void *p) {assert(p==menu);++menu_closes;}
static void __attribute__((thiscall,force_align_arg_pointer)) widget_stub(void *p) {assert(p==widget);++widgets;}
static void __attribute__((force_align_arg_pointer)) background_stub(void) {++backgrounds;}
static unsigned __attribute__((cdecl,force_align_arg_pointer)) no_screen(void *a,void *b) {(void)a;(void)b;return 0;}
static void __attribute__((thiscall,force_align_arg_pointer)) fade_stub(void *p,float value) {(void)value;assert(p==fade);++fades;}
static void __attribute__((force_align_arg_pointer)) pump_stub(void) {
    ++pumps;assert(*(unsigned *)(image+0x3c3070)>0 && image[0x3c3075]);
}
static void __attribute__((used,noinline,cdecl,force_align_arg_pointer)) quit_record(void *owner) {
    assert(owner==title);++quits;quit_order=++order;
    if(events) assert(dispatches==events && !resident[0x49] && !resident[0xe0] && event_order<quit_order);
    /* Simulate refusal at the destructive callee. Its caller still mutated UI
     * and ran the event. No native reset is executed in either disposition. */
    if(!late_refusal) *(unsigned *)(title+0x44)=1;
}
static void __attribute__((naked,noinline)) quit_stub(void) {
    __asm__ volatile("push %eax; call _quit_record; add $4,%esp; ret");
}
static unsigned __attribute__((used,noinline,cdecl,force_align_arg_pointer)) reader_record(void *record) {
    assert((uintptr_t)record>=(uintptr_t)catalog && (uintptr_t)record<(uintptr_t)(catalog+sizeof(catalog)));
    uintptr_t offset=(uintptr_t)record-(uintptr_t)catalog;assert(!(offset%0x2c8));
    ++reads;reader_slot=(unsigned)(offset/0x2c8);return reader_result;
}
static void __attribute__((naked,noinline)) reader_stub(void) {
    __asm__ volatile("push %eax; call _reader_record; add $4,%esp; ret");
}
static void prepare_remove_world(void) {
    memset(remove_world,0xa5,sizeof(remove_world));
    *(void **)(remove_world+0x50)=NULL;*(unsigned *)(remove_world+0x54)=0;
    *(void **)(image+WORLD)=remove_world;
}
static void remove_areas(void) {((void (__cdecl *)(void))(image+REMOVE))();}
static void check_removed_world(void) {
    uint8_t expected[sizeof(remove_world)];memset(expected,0xa5,sizeof(expected));
    *(void **)(expected+0x50)=NULL;*(unsigned *)(expected+0x54)=0;
    memset(expected+0xc,0,16);expected[0x39d]=1;
    assert(!memcmp(expected,remove_world,sizeof(expected)));
}
static void reset_case(void) {
    prepare_remove_world();
    events=dispatches=quits=reads=cues=menu_closes=menu_resets=widgets=0;
    fades=pumps=backgrounds=order=event_order=quit_order=0;
    reader_result=reader_slot=held=deferrals=0;change_selection=late_refusal=FALSE;
    memset(scene,0,sizeof(scene));memset(resident,0,sizeof(resident));memset(title,0,sizeof(title));
    memset(menu,0,sizeof(menu));memset(fade,0xa5,sizeof(fade));memset(panel,0,sizeof(panel));
    memset(script,0,sizeof(script));memset(speed,0,sizeof(speed));memset(catalog,0,sizeof(catalog));
    memset(widget,0,sizeof(widget));*(void **)widget=widget_vtable;
    *(void **)(scene+0x174)=resident;*(unsigned *)(scene+0x6c)=7;
    *(void **)(resident+0x94)=title;resident[0x49]=resident[0xe0]=resident[0xe1]=1;
    *(unsigned *)(title+0x44)=9;
    *(void **)menu=menu_vtable;*(void **)(menu+0x7c)=panel;menu[0x29]=menu[0x9c]=1;
    *(unsigned *)(menu+0x50)=10;*(void **)(menu+0x94)=widget;
    *(void **)(image+SCENE)=scene;*(void **)(image+SCRIPTS)=script;
    *(void **)(image+SPEED)=speed;*(void **)(image+FADE)=fade;*(void **)(image+0x3c2f78)=NULL;
    *(unsigned *)(image+CATALOG)=5;*(void **)(image+RECORDS)=catalog;
    *(int *)(image+SELECTED)=1;*(uint16_t *)(image+COUNT)=3;image[COUNT+2]=0;
    image[0x409df8]=1;image[0x31f296]=0;*(unsigned *)(image+0x3c3070)=0;
    image[0x3c3075]=0;*(unsigned *)(image+0x3c3078)=0;
}
static unsigned input(unsigned kind) {
    typedef unsigned (__attribute__((thiscall)) *Input)(void *,unsigned,unsigned,unsigned);
    return ((Input)(image+NATIVE_INPUT))(menu,5,kind,0);
}
static void action(unsigned kind) {
    void *entry=image+ACTION,*owner=menu;
    __asm__ volatile("call *%2" : "+a"(kind),"+c"(owner) : "r"(entry) : "edx","cc","memory");
}
static void quit_cases(void) {
    reset_case();late_refusal=TRUE;assert(!(input(0)&255));
    assert(events==1 && dispatches==1 && quits==1 && cues==1 && *(unsigned *)(title+0x44)==9);
    assert(!resident[0x49] && !resident[0xe0]);
    assert(!(input(0)&255));assert(events==2 && dispatches==2 && quits==2);
    reset_case();held=1;
    for(unsigned i=0;i<3;++i) assert(!(input(0)&255));
    assert(deferrals==3 && cues==3 && !events && !dispatches && !quits);
    assert(resident[0x49] && resident[0xe0] && *(unsigned *)(title+0x44)==9 && *(unsigned *)(menu+0x50)==10);
    held=0;assert(!(input(0)&255));assert(events==1 && dispatches==1 && quits==1 && *(unsigned *)(title+0x44)==1);
    /* Action 1 (load-page navigation) and unknown selectors are not quit/reload.
     * This is not permission for that later page's native load. */
    reset_case();held=1;action(1);assert(!deferrals && widgets==1 && *(unsigned *)(menu+0x50)==8);
    action(99);assert(!deferrals && !events && !quits && !reads);
}
static void reload_cases(void) {
    reset_case();held=1;*(unsigned *)(menu+0x50)=11;
    assert(!(input(0)&255));assert(deferrals==1 && cues==1 && !events && !reads && !pumps && !fades);
    assert(menu[0x29] && *(unsigned *)(menu+0x50)==11 && *(unsigned *)(scene+0x6c)==7);
    for(unsigned failure=0;failure<2;++failure) for(unsigned nesting=0;nesting<2;++nesting) {
        reset_case();*(unsigned *)(menu+0x50)=11;reader_result=failure;change_selection=TRUE;
        *(unsigned *)(image+0x3c3070)=nesting;image[0x3c3075]=(uint8_t)nesting;
        assert(!(input(0)&255)); /* Outer menu caller discards reader success/failure. */
        assert(events==1 && dispatches==1 && reads==1 && reader_slot==2 && !quits);
        assert(!resident[0x49] && !resident[0xe0] && !menu[0x29] && !*(unsigned *)(menu+0x50));
        assert(!*(unsigned *)(scene+0x6c) && image[0x31f296] && menu_closes==1 && menu_resets==1);
        assert(backgrounds==1 && fades==1 && pumps==1 && *(unsigned *)(fade+0x74)==0xff000000);
        assert(*(unsigned *)(image+0x3c3070)==nesting && image[0x3c3075]==nesting);
    }
    reset_case();*(unsigned *)(menu+0x50)=11;*(int *)(image+SELECTED)=-1;
    assert(!(input(0)&255));assert(events==1 && !reads && !menu[0x29] && fades==1 && pumps==1);
}
static void exported_cases(void) {
    reset_case();held=1;((void (__cdecl *)(void))(image+QUIT))();
    assert(quits==1 && !deferrals && !events && *(unsigned *)(title+0x44)==1);
    reset_case();held=1;((void (__cdecl *)(int))(image+LOAD))(2);
    assert(reads==1 && reader_slot==2 && !deferrals && !events);
    reset_case();held=1;catalog[2*0x2c8+0x2c4]=1;
    ((void (__cdecl *)(int))(image+LOAD))(2);assert(!reads && !deferrals);
    reset_case();held=1;*(unsigned *)(image+CATALOG)=1;*(uint16_t *)(image+COUNT)=0;image[COUNT+2]=2;
    ((void (__cdecl *)(int))(image+LOAD))(0);assert(!reads && image[COUNT+2]==1 && !deferrals);
    reset_case();held=1;unsigned slot=2;void *entry=image+INDEX;
    __asm__ volatile("call *%1" : "+a"(slot) : "r"(entry) : "ecx","edx","cc","memory");
    assert(reads==1 && reader_slot==2 && *(int *)(image+SELECTED)==2 && !deferrals);
}
static void snapshot_refused(void) {
    SudekiMpLanStoryAreaIntentReceipt out,before;memset(&out,0x5a,sizeof(out));before=out;unsigned count=777;
    assert(!SudekiMpLanStoryAreaIntentSnapshot((HMODULE)image,&intent_consumer,&witness,&out,1,&count));
    assert(count==777 && !memcmp(&out,&before,sizeof(out)));
}
static unsigned __attribute__((force_align_arg_pointer)) admit(const void *who,const SudekiMpLanStoryAreaIntentReceipt *r) {
    assert(who==&intent_consumer && r->ticket>newest_ticket && r->phase==SUDEKIMP_AREA_INTENT_DISPATCHING);
    assert(r->kind>=SUDEKIMP_AREA_INTENT_RELOAD && r->kind<=SUDEKIMP_AREA_INTENT_REMOVE_AREAS);
    assert(r->source>=SUDEKIMP_AREA_INTENT_MENU && r->source<=SUDEKIMP_AREA_INTENT_REMOVE_EXPORT);
    if(r->source==SUDEKIMP_AREA_INTENT_REMOVE_EXPORT) assert(r->kind==SUDEKIMP_AREA_INTENT_REMOVE_AREAS);
    if(r->source==SUDEKIMP_AREA_INTENT_QUIT_EXPORT) assert(r->kind==SUDEKIMP_AREA_INTENT_QUIT);
    if(r->source==SUDEKIMP_AREA_INTENT_LOAD_EXPORT) assert(r->kind==SUDEKIMP_AREA_INTENT_LOAD_SAVE);
    else assert(r->save_index==-1);
    newest_ticket=r->ticket;++admission_calls;
    snapshot_refused();
    assert(!SudekiMpLanStoryAreaIntentDetach((HMODULE)image,who,&witness));
    assert(!SudekiMpLanStoryAreaIntentAcknowledge((HMODULE)image,who,&witness,r->ticket));
    if(nested_input) assert(!(input(0)&255)); /* Deliberately violate callback no-reentry contract. */
    if(nested_export) ((void (__cdecl *)(void))(image+QUIT))();
    if(nested_load) ((void (__cdecl *)(int))(image+LOAD))(2);
    if(nested_remove) remove_areas();
    SetLastError(0x9999);
    __asm__ volatile("fninit; fld1; pxor %%xmm0,%%xmm0; pxor %%xmm7,%%xmm7" : : : "memory");
    return decision;
}
static unsigned take(SudekiMpLanStoryAreaIntentReceipt *out,unsigned capacity) {
    unsigned count=777;
    assert(SudekiMpLanStoryAreaIntentSnapshot((HMODULE)image,&intent_consumer,&witness,out,capacity,&count));
    return count;
}
static void export_admission(void) {
    SudekiMpLanStoryAreaIntentReceipt r[4];
    uint8_t saved_scene[sizeof(scene)],saved_resident[sizeof(resident)],saved_title[sizeof(title)];
    memcpy(saved_scene,scene,sizeof(scene));memcpy(saved_resident,resident,sizeof(resident));
    memcpy(saved_title,title,sizeof(title));
    for(unsigned i=0;i<2;++i) ((void (__cdecl *)(void))(image+QUIT))();
    assert(!events && !dispatches && !quits && !reads && !cues);
    assert(!memcmp(saved_scene,scene,sizeof(scene)) && !memcmp(saved_resident,resident,sizeof(resident)) &&
        !memcmp(saved_title,title,sizeof(title)));
    /* No scene dereference before admission. Missing scene is safe to defer;
     * this does not authorize running the original with a missing scene. */
    *(void **)(image+SCENE)=NULL;((void (__cdecl *)(void))(image+QUIT))();
    *(void **)(image+SCENE)=scene;
    assert(take(r,4)==3 && admission_calls==3);
    for(unsigned i=0;i<3;++i) {
        assert(r[i].source==SUDEKIMP_AREA_INTENT_QUIT_EXPORT && r[i].kind==SUDEKIMP_AREA_INTENT_QUIT &&
            r[i].phase==SUDEKIMP_AREA_INTENT_DEFERRED && (!i || r[i].ticket>r[i-1].ticket));
        assert(SudekiMpLanStoryAreaIntentAcknowledge((HMODULE)image,&intent_consumer,&witness,r[i].ticket));
    }
    assert(!quits);decision=SUDEKIMP_AREA_INTENT_RUN;
    ((void (__cdecl *)(void))(image+QUIT))();assert(quits==1 && !take(r,4));
    /* Native wrapper's no-title path is preserved after admission. */
    *(void **)(scene+0x174)=NULL;((void (__cdecl *)(void))(image+QUIT))();
    assert(quits==1 && !take(r,4));*(void **)(scene+0x174)=resident;
    /* The existing lobby exit owner's prefix/tail identity check still passes. */
    static const uint8_t tail[]={0x85,0xc0,0x74,5,0xe9,0xbe,0xfe,0xff,0xff,0xc3};
    assert(image[QUIT]==0xa1 && *(void **)(image+QUIT+1)==image+SCENE &&
        !memcmp(image+QUIT+25,tail,sizeof(tail)));
    assert(SudekiMpLanStoryAreaIntentDetach((HMODULE)image,&intent_consumer,&witness));
    unsigned calls=admission_calls;reset_case();
    ((void (__cdecl *)(void))(image+QUIT))();assert(quits==1 && admission_calls==calls);
}
static void startup_matrix(void) {
    uint8_t original[4][7];const unsigned sites[]={ACTION,QUIT+5,LOAD,REMOVE},lengths[]={6,6,7,5};
    for(unsigned i=0;i<4;++i) memcpy(original[i],image+sites[i],lengths[i]);
    for(unsigned stage=1;stage<=4;++stage) {
        install_calls=restore_calls=0;fail_before_stage=stage;
        assert(!SudekiMpLanStoryAreaIntentInstall((HMODULE)image) && GetLastError()==ERROR_WRITE_FAULT);
        assert(install_calls==stage && restore_calls==stage-1);fail_before_stage=0;
        for(unsigned i=0;i<4;++i) assert(!memcmp(original[i],image+sites[i],lengths[i]));
        install_calls=restore_calls=0;fail_after_stage=stage;
        assert(!SudekiMpLanStoryAreaIntentInstall((HMODULE)image) && GetLastError()==ERROR_WRITE_FAULT);
        assert(install_calls==stage && restore_calls==stage);fail_after_stage=0;
        for(unsigned i=0;i<4;++i) assert(!memcmp(original[i],image+sites[i],lengths[i]));
        for(unsigned failure=1;failure<=stage;++failure) {
            install_calls=restore_calls=0;fail_after_stage=stage;fail_restore_stage=failure;
            assert(!SudekiMpLanStoryAreaIntentInstall((HMODULE)image) && GetLastError()==ERROR_WRITE_FAULT);
            assert(restore_calls==stage); /* Every independent restoration is attempted. */
            assert(!SudekiMpLanStoryAreaIntentInstall((HMODULE)image));
            assert(!SudekiMpLanStoryAreaIntentAttach((HMODULE)image,&intent_consumer,&witness,admit));
            fail_after_stage=fail_restore_stage=0;
            assert(SudekiMpLanStoryAreaIntentUninstall());
            for(unsigned i=0;i<4;++i) assert(!memcmp(original[i],image+sites[i],lengths[i]));
        }
    }
    for(unsigned failure=1;failure<=4;++failure) {
        assert(SudekiMpLanStoryAreaIntentInstall((HMODULE)image));
        restore_calls=0;fail_restore_stage=failure;
        assert(!SudekiMpLanStoryAreaIntentUninstall() && restore_calls==4);
        assert(!SudekiMpLanStoryAreaIntentAttach((HMODULE)image,&intent_consumer,&witness,admit));
        fail_restore_stage=0;assert(SudekiMpLanStoryAreaIntentUninstall());
        for(unsigned i=0;i<4;++i) assert(!memcmp(original[i],image+sites[i],lengths[i]));
        assert(SudekiMpLanStoryAreaIntentInstall((HMODULE)image));
        unsigned rva=sites[failure-1];uint8_t saved=image[rva];image[rva]=0x90;
        assert(!SudekiMpLanStoryAreaIntentUninstall() && image[rva]==0x90);
        image[rva]=saved;assert(SudekiMpLanStoryAreaIntentUninstall());
        for(unsigned i=0;i<4;++i) assert(!memcmp(original[i],image+sites[i],lengths[i]));
    }
}
static void intent_install(const char *mode) {
    assert(!SudekiMpLanStoryAreaIntentInstall(NULL));
    uint8_t original[6],replacement[6];memcpy(original,image+ACTION,sizeof(original));
    const unsigned gates[]={ACTION,ACTION+6,ACTION+21,NATIVE_INPUT,0x164fe6,0x164ff0,0x164ff5,0x164ff6,0x164ffa,0x165002,
        QUIT,QUIT+1,QUIT+5,QUIT+10,QUIT+11,QUIT+25,QUIT+30,QUIT+34,
        LOAD,LOAD+2,LOAD+6,LOAD+7,LOAD+11,0x101715,0x101718,0x101732,0x101737,
        REMOVE,REMOVE+1,REMOVE+4,REMOVE+5,REMOVE+6,REMOVE+7,REMOVE+10,REMOVE+11};
    for(unsigned i=0;i<sizeof(gates)/sizeof(*gates);++i) {
        image[gates[i]]^=1;assert(!SudekiMpLanStoryAreaIntentInstall((HMODULE)image));image[gates[i]]^=1;
    }
    const unsigned globals[]={0x408d10,SCENE};
    for(unsigned i=0;i<2;++i) {
        *(void **)(image+globals[i])=menu;assert(!SudekiMpLanStoryAreaIntentInstall((HMODULE)image));
        *(void **)(image+globals[i])=NULL;
    }
    startup_matrix();
    if(!strcmp(mode,"startup-retained-remove")) {
        install_calls=restore_calls=0;fail_after_stage=4;fail_restore_stage=1;
        assert(!SudekiMpLanStoryAreaIntentInstall((HMODULE)image) && GetLastError()==ERROR_WRITE_FAULT);
        assert(restore_calls==4 && image[REMOVE]==0xe9 && !memcmp(original,image+ACTION,6));
        fail_after_stage=fail_restore_stage=0;goto witness_ready;
    }
    if(!strcmp(mode,"startup-retained-load")) {
        install_calls=restore_calls=0;fail_after_stage=3;fail_restore_stage=1;
        assert(!SudekiMpLanStoryAreaIntentInstall((HMODULE)image) && GetLastError()==ERROR_WRITE_FAULT);
        assert(restore_calls==3 && image[LOAD]==0xe9 && !memcmp(original,image+ACTION,6));
        fail_after_stage=fail_restore_stage=0;goto witness_ready;
    }
    if(!strcmp(mode,"startup-retained-export")) {
        install_calls=restore_calls=0;fail_after_stage=2;fail_restore_stage=1;
        assert(!SudekiMpLanStoryAreaIntentInstall((HMODULE)image) && GetLastError()==ERROR_WRITE_FAULT);
        assert(restore_calls==2 && image[QUIT+5]==0xe9 && !memcmp(original,image+ACTION,6));
        fail_after_stage=fail_restore_stage=0;
        goto witness_ready;
    }
    fail_install_before=TRUE;
    assert(!SudekiMpLanStoryAreaIntentInstall((HMODULE)image) && GetLastError()==ERROR_WRITE_FAULT);
    assert(!memcmp(original,image+ACTION,sizeof(original)));
    fail_install_before=FALSE;fail_install_after=TRUE;
    assert(!SudekiMpLanStoryAreaIntentInstall((HMODULE)image) && GetLastError()==ERROR_WRITE_FAULT);
    assert(!memcmp(original,image+ACTION,sizeof(original))); /* Successful rollback. */
    fail_restore=TRUE;
    assert(!SudekiMpLanStoryAreaIntentInstall((HMODULE)image) && GetLastError()==ERROR_WRITE_FAULT);
    assert(image[ACTION]==0xe9);memcpy(replacement,image+ACTION,sizeof(replacement));
    assert(!SudekiMpLanStoryAreaIntentInstall((HMODULE)image));
    assert(!memcmp(replacement,image+ACTION,sizeof(replacement)));
    fail_install_after=fail_restore=FALSE;
    if(!strcmp(mode,"startup-retained")) goto witness_ready;
    assert(SudekiMpLanStoryAreaIntentUninstall());
    assert(!memcmp(original,image+ACTION,sizeof(original)));
    assert(SudekiMpLanStoryAreaIntentInstall((HMODULE)image));
    assert(!SudekiMpLanStoryAreaIntentInstall((HMODULE)image));
    memcpy(replacement,image+ACTION,sizeof(replacement));fail_restore=TRUE;
    assert(!SudekiMpLanStoryAreaIntentUninstall() && GetLastError()==ERROR_ACCESS_DENIED);
    assert(!memcmp(replacement,image+ACTION,sizeof(replacement)));
    assert(!SudekiMpLanStoryAreaIntentInstall((HMODULE)image));
    fail_restore=FALSE;assert(SudekiMpLanStoryAreaIntentUninstall());
    assert(!memcmp(original,image+ACTION,sizeof(original)));
    assert(SudekiMpLanStoryAreaIntentInstall((HMODULE)image));
    uint8_t first=image[ACTION];image[ACTION]=0x90;
    assert(!SudekiMpLanStoryAreaIntentUninstall() && image[ACTION]==0x90);
    image[ACTION]=first;assert(SudekiMpLanStoryAreaIntentUninstall());
    assert(SudekiMpLanStoryAreaIntentInstall((HMODULE)image));
witness_ready:
    witness=(SudekiMpControlUpdateDispatchWitness){.dispatch_serial=1,.service_post_original_exact=TRUE,
        .native_thread_id=GetCurrentThreadId()};
}
static void intent_attach(void) {
    assert(!SudekiMpLanStoryAreaIntentAttach((HMODULE)image,NULL,&witness,admit));
    witness_ok=FALSE;assert(!SudekiMpLanStoryAreaIntentAttach((HMODULE)image,&intent_consumer,&witness,admit));
    witness_ok=TRUE;
    assert(SudekiMpLanStoryAreaIntentAttach((HMODULE)image,&intent_consumer,&witness,admit));
    assert(!SudekiMpLanStoryAreaIntentAttach((HMODULE)image,&other_consumer,&witness,admit));
}
static DWORD WINAPI foreign_action(void *unused) {(void)unused;assert(!(input(0)&255));return 0;}
static DWORD WINAPI foreign_export(void *unused) {(void)unused;((void (__cdecl *)(void))(image+QUIT))();return 0;}
static DWORD WINAPI foreign_load(void *unused) {(void)unused;((void (__cdecl *)(int))(image+LOAD))(2);return 0;}
static DWORD WINAPI foreign_remove(void *unused) {(void)unused;remove_areas();return 0;}
static void *abi_native_call __attribute__((used));
static uint32_t abi_regs[9] __attribute__((used)),abi_stack __attribute__((used));
static uint32_t abi_index __attribute__((used));
static void __attribute__((naked,noinline)) abi_return(void) {__asm__ volatile("ret");}
static void __attribute__((naked,noinline)) abi_original_tail(void) {
    /* The production trampoline has executed the original six-byte prologue. */
    __asm__ volatile("mov $0xabcdef12,%eax; stc; mov %ebp,%esp; pop %ebp; ret");
}
static void __attribute__((naked,noinline)) abi_quit_tail(void) {
    __asm__ volatile("mov $0xabcdef12,%eax; stc; ret");
}
static void __attribute__((naked,noinline)) abi_load_tail(void) {
    __asm__ volatile("mov 4(%esp),%eax; mov %eax,_abi_index; mov $0xabcdef12,%eax; stc; ret");
}
static void __attribute__((naked,noinline)) abi_invoke(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,_abi_stack;"
        "xor %eax,%eax; mov $0x22222222,%ecx; mov $0x33333333,%edx;"
        "mov $0x44444444,%ebx; mov $0x55555555,%ebp; mov $0x66666666,%esi; mov $0x77777777,%edi;"
        "std; stc; pushl $0x13579bdf; call *_abi_native_call; lea 4(%esp),%esp;"
        "mov %eax,_abi_regs; mov %ecx,_abi_regs+4; mov %edx,_abi_regs+8; mov %ebx,_abi_regs+12;"
        "mov %ebp,_abi_regs+16; mov %esi,_abi_regs+20; mov %edi,_abi_regs+24;"
        "pushfl; pop _abi_regs+28; mov %esp,_abi_regs+32; popal; popfl; ret");
}
static void admission_abi(unsigned exported) {
    SudekiMpInlineHook return_stub={0},body_stub={0};
    if(exported==3) {
        assert(SudekiMpInstallInlineHook(&body_stub,image+REMOVE+5,image+REMOVE+5,6,(void *)(uintptr_t)abi_quit_tail));
        abi_native_call=image+REMOVE;
    } else if(exported==2) {
        assert(SudekiMpInstallInlineHook(&body_stub,image+LOAD+7,image+LOAD+7,8,(void *)(uintptr_t)abi_load_tail));
        abi_native_call=image+LOAD;
    } else if(exported) {
        assert(SudekiMpInstallInlineHook(&body_stub,image+QUIT+11,image+QUIT+11,10,(void *)(uintptr_t)abi_quit_tail));
        abi_native_call=image+QUIT;
    } else {
        assert(SudekiMpInstallInlineHook(&return_stub,image+0x164ffa,image+0x164ffa,5,(void *)(uintptr_t)abi_return));
        assert(SudekiMpInstallInlineHook(&body_stub,image+ACTION+6,image+ACTION+6,6,(void *)(uintptr_t)abi_original_tail));
        abi_native_call=image+0x164ff5;
    }
    for(unsigned run=0;run<2;++run) {
        uint8_t before[512] __attribute__((aligned(16)))={0},after[512] __attribute__((aligned(16)))={0};
        uint16_t control=0x077f;uint32_t mxcsr=0x3f80;
        decision=run?SUDEKIMP_AREA_INTENT_RUN:SUDEKIMP_AREA_INTENT_DEFER;
        SetLastError(0x7788);
        __asm__ volatile("fninit; fldcw %1; ldmxcsr %2; fldpi; fld1; fldl2t;"
            "pcmpeqd %%xmm0,%%xmm0; pcmpeqd %%xmm7,%%xmm7; fxsave %0"
            : "=m"(before) : "m"(control),"m"(mxcsr) : "memory");
        abi_invoke();
        __asm__ volatile("fxsave %0; fninit" : "=m"(after) : : "memory");
        assert(GetLastError()==0x7788 && !memcmp(before,after,160) && !memcmp(before+160,after+160,128));
        assert(abi_regs[0]==(run?0xabcdef12u:(exported==1?(uint32_t)(uintptr_t)scene:0)));
        if(run && exported==2) assert(abi_index==0x13579bdf);
        for(unsigned i=1;i<7;++i) assert(abi_regs[i]==0x11111111u*(i+1));
        assert((abi_regs[7]&0x401)==0x401 && abi_regs[8]==abi_stack);
        SudekiMpLanStoryAreaIntentReceipt r;
        assert(take(&r,1)==!run);
        if(!run) {
            assert(r.source==(exported==3?SUDEKIMP_AREA_INTENT_REMOVE_EXPORT:
                exported==2?SUDEKIMP_AREA_INTENT_LOAD_EXPORT:
                (exported?SUDEKIMP_AREA_INTENT_QUIT_EXPORT:SUDEKIMP_AREA_INTENT_MENU)));
            assert(r.save_index==(exported==2?0x13579bdf:-1));
        }
        if(!run) assert(SudekiMpLanStoryAreaIntentAcknowledge((HMODULE)image,&intent_consumer,&witness,r.ticket));
    }
    assert(SudekiMpRestoreInlineHook(&body_stub) && SudekiMpRestoreInlineHook(&return_stub));
    assert(SudekiMpLanStoryAreaIntentDetach((HMODULE)image,&intent_consumer,&witness));
}
static void load_admission(void) {
    const int indices[]={2,-1,INT32_MIN,INT32_MAX};
    SudekiMpLanStoryAreaIntentReceipt r[4];
    image[0x409df8]=0;*(void **)(image+RECORDS)=NULL;image[COUNT+2]=2;
    for(unsigned i=0;i<4;++i) ((void (__cdecl *)(int))(image+LOAD))(indices[i]);
    assert(!reads && !events && !quits && !image[0x409df8] && !*(void **)(image+RECORDS) && image[COUNT+2]==2);
    assert(take(r,4)==4 && admission_calls==4);
    for(unsigned i=0;i<4;++i) {
        assert(r[i].kind==SUDEKIMP_AREA_INTENT_LOAD_SAVE && r[i].source==SUDEKIMP_AREA_INTENT_LOAD_EXPORT &&
            r[i].save_index==indices[i] && r[i].phase==SUDEKIMP_AREA_INTENT_DEFERRED);
        assert(SudekiMpLanStoryAreaIntentAcknowledge((HMODULE)image,&intent_consumer,&witness,r[i].ticket));
    }
    assert(!reads);reset_case();decision=SUDEKIMP_AREA_INTENT_RUN;
    ((void (__cdecl *)(int))(image+LOAD))(2);assert(reads==1 && reader_slot==2 && !take(r,4));
    *(unsigned *)(image+CATALOG)=1;*(uint16_t *)(image+COUNT)=0;image[COUNT+2]=2;
    ((void (__cdecl *)(int))(image+LOAD))(0);assert(reads==1 && image[COUNT+2]==1 && !take(r,4));
    assert(!SudekiMpSaveBookStoryLoadUninstall(&other_consumer));
    assert(SudekiMpLanStoryAreaIntentDetach((HMODULE)image,&intent_consumer,&witness));
    unsigned calls=admission_calls;reset_case();((void (__cdecl *)(int))(image+LOAD))(2);
    assert(reads==1 && reader_slot==2 && admission_calls==calls);
}
static void admission_cases(const char *mode) {
    if(!strcmp(mode,"startup-retained") || !strcmp(mode,"startup-retained-export") ||
        !strcmp(mode,"startup-retained-load") || !strcmp(mode,"startup-retained-remove")) {
        /* The installation reported failure after patching, and rollback also
         * failed. A still-live native entry must retain its original trampoline
         * even though consumer admission was never published. */
        reset_case();
        if(!strcmp(mode,"startup-retained-remove")) {
            remove_areas();check_removed_world();assert(!events && !dispatches && !reads);
        } else if(!strcmp(mode,"startup-retained-load")) {
            ((void (__cdecl *)(int))(image+LOAD))(2);assert(reads==1 && reader_slot==2 && !events && !dispatches);
        } else if(!strcmp(mode,"startup-retained-export")) {
            ((void (__cdecl *)(void))(image+QUIT))();assert(!events && !dispatches);
        } else {assert(!(input(0)&255));assert(events==1 && dispatches==1);}
        assert(quits==(!strcmp(mode,"startup-retained-load") || !strcmp(mode,"startup-retained-remove")?0u:1u) && !admission_calls);
        assert(!SudekiMpLanStoryAreaIntentAttach((HMODULE)image,&intent_consumer,&witness,admit));
        snapshot_refused();
        assert(!SudekiMpLanStoryAreaIntentUninstall());
        assert(!SudekiMpLanStoryAreaIntentInstall((HMODULE)image));
        return;
    }
    intent_attach();reset_case();admission_calls=0;
    SudekiMpLanStoryAreaIntentReceipt records[SUDEKIMP_AREA_INTENTS];
    if(!strcmp(mode,"abi") || !strcmp(mode,"export-abi") || !strcmp(mode,"load-abi") || !strcmp(mode,"remove-abi"))
        admission_abi(!strcmp(mode,"remove-abi")?3u:!strcmp(mode,"load-abi")?2u:(!strcmp(mode,"export-abi")?1u:0u));
    else if(!strcmp(mode,"remove-admission")) {
        uint8_t before[sizeof(remove_world)];memcpy(before,remove_world,sizeof(before));
        remove_areas();remove_areas();assert(!memcmp(before,remove_world,sizeof(before)));
        *(void **)(image+WORLD)=NULL;remove_areas();*(void **)(image+WORLD)=remove_world;
        assert(take(records,SUDEKIMP_AREA_INTENTS)==3 && admission_calls==3);
        for(unsigned i=0;i<3;++i) {
            assert(records[i].source==SUDEKIMP_AREA_INTENT_REMOVE_EXPORT &&
                records[i].kind==SUDEKIMP_AREA_INTENT_REMOVE_AREAS && records[i].save_index==-1 &&
                records[i].phase==SUDEKIMP_AREA_INTENT_DEFERRED && (!i || records[i].ticket>records[i-1].ticket));
            assert(SudekiMpLanStoryAreaIntentAcknowledge((HMODULE)image,&intent_consumer,&witness,records[i].ticket));
        }
        assert(!memcmp(before,remove_world,sizeof(before))); /* ACK never replays. */
        decision=SUDEKIMP_AREA_INTENT_RUN;remove_areas();check_removed_world();
        assert(!take(records,SUDEKIMP_AREA_INTENTS));
        assert(SudekiMpLanStoryAreaIntentDetach((HMODULE)image,&intent_consumer,&witness));
        unsigned calls=admission_calls;prepare_remove_world();remove_areas();check_removed_world();
        assert(admission_calls==calls);
    }
    else if(!strcmp(mode,"load-admission")) load_admission();
    else if(!strcmp(mode,"export-admission")) export_admission();
    else if(!strcmp(mode,"admission")) {
        uint8_t saved_menu[sizeof(menu)],saved_scene[sizeof(scene)],saved_resident[sizeof(resident)];
        memcpy(saved_menu,menu,sizeof(menu));memcpy(saved_scene,scene,sizeof(scene));memcpy(saved_resident,resident,sizeof(resident));
        for(unsigned i=0;i<3;++i) assert(!(input(0)&255));
        assert(admission_calls==3 && cues==3 && !events && !dispatches && !quits && !reads);
        assert(!memcmp(saved_menu,menu,sizeof(menu)) && !memcmp(saved_scene,scene,sizeof(scene)) &&
            !memcmp(saved_resident,resident,sizeof(resident)));
        assert(take(records,SUDEKIMP_AREA_INTENTS)==3);
        for(unsigned i=0;i<3;++i) assert(records[i].kind==SUDEKIMP_AREA_INTENT_QUIT &&
            records[i].source==SUDEKIMP_AREA_INTENT_MENU &&
            records[i].phase==SUDEKIMP_AREA_INTENT_DEFERRED && (!i || records[i].ticket>records[i-1].ticket));
        snapshot_refused(); /* Capacity one cannot partially publish three records. */
        witness_ok=FALSE;snapshot_refused();witness_ok=TRUE;
        assert(!SudekiMpLanStoryAreaIntentDetach((HMODULE)image,&intent_consumer,&witness));
        assert(!SudekiMpLanStoryAreaIntentAcknowledge((HMODULE)image,&other_consumer,&witness,records[0].ticket));
        /* Reuse the fixture menu/catalog storage; old receipts contain no native pointers. */
        reset_case();*(unsigned *)(menu+0x50)=11;assert(!(input(0)&255));
        assert(!events && !reads && !fades && !pumps && menu[0x29] && *(unsigned *)(menu+0x50)==11);
        assert(take(records,SUDEKIMP_AREA_INTENTS)==4 && records[3].kind==SUDEKIMP_AREA_INTENT_RELOAD);
        for(unsigned i=0;i<4;++i) {
            assert(SudekiMpLanStoryAreaIntentAcknowledge((HMODULE)image,&intent_consumer,&witness,records[i].ticket));
            assert(!SudekiMpLanStoryAreaIntentAcknowledge((HMODULE)image,&intent_consumer,&witness,records[i].ticket));
        }
        assert(!events && !reads && !quits); /* ACK does not replay native work. */
        assert(!take(records,SUDEKIMP_AREA_INTENTS));
        decision=SUDEKIMP_AREA_INTENT_RUN;reset_case();assert(!(input(0)&255));
        assert(events==1 && dispatches==1 && quits==1 && !take(records,SUDEKIMP_AREA_INTENTS));
        reset_case();*(unsigned *)(menu+0x50)=11;reader_result=1;change_selection=TRUE;
        assert(!(input(0)&255));assert(events==1 && reads==1 && reader_slot==2 && !take(records,SUDEKIMP_AREA_INTENTS));
        decision=SUDEKIMP_AREA_INTENT_DEFER;unsigned calls=admission_calls;
        reset_case();action(1);action(99);assert(admission_calls==calls && widgets==1);
        decision=SUDEKIMP_AREA_INTENT_RUN;exported_cases();
        assert(admission_calls==calls+4);calls=admission_calls;decision=SUDEKIMP_AREA_INTENT_DEFER;
        assert(SudekiMpLanStoryAreaIntentDetach((HMODULE)image,&intent_consumer,&witness));
        reset_case();assert(!(input(0)&255));assert(events==1 && quits==1 && admission_calls==calls);
        intent_attach();reset_case();assert(!(input(0)&255));
        assert(take(records,SUDEKIMP_AREA_INTENTS)==1 && records[0].ticket==newest_ticket);
        assert(SudekiMpLanStoryAreaIntentAcknowledge((HMODULE)image,&intent_consumer,&witness,records[0].ticket));
        assert(SudekiMpLanStoryAreaIntentDetach((HMODULE)image,&intent_consumer,&witness));
    } else {
        if(!strcmp(mode,"unknown")) {decision=99;assert(!(input(0)&255));}
        else if(!strcmp(mode,"overflow")) {
            for(unsigned i=0;i<SUDEKIMP_AREA_INTENTS;++i) assert(!(input(0)&255));
            assert(take(records,SUDEKIMP_AREA_INTENTS)==SUDEKIMP_AREA_INTENTS);
            assert(!(input(0)&255));assert(admission_calls==SUDEKIMP_AREA_INTENTS);
        } else if(!strcmp(mode,"reentry")) {nested_input=TRUE;assert(!(input(0)&255));assert(admission_calls==1);}
        else if(!strcmp(mode,"export-reentry")) {nested_export=TRUE;assert(!(input(0)&255));assert(admission_calls==1);}
        else if(!strcmp(mode,"load-reentry")) {nested_load=TRUE;assert(!(input(0)&255));assert(admission_calls==1);}
        else if(!strcmp(mode,"remove-reentry")) {nested_remove=TRUE;assert(!(input(0)&255));assert(admission_calls==1);}
        else if(!strcmp(mode,"remove-overtake")) {
            assert(!(input(0)&255));decision=SUDEKIMP_AREA_INTENT_RUN;remove_areas();
        } else if(!strcmp(mode,"remove-menu-overtake")) {
            remove_areas();decision=SUDEKIMP_AREA_INTENT_RUN;assert(!(input(0)&255));
        } else if(!strcmp(mode,"remove-load-overtake")) {
            remove_areas();decision=SUDEKIMP_AREA_INTENT_RUN;((void (__cdecl *)(int))(image+LOAD))(2);
        } else if(!strcmp(mode,"load-remove-overtake")) {
            ((void (__cdecl *)(int))(image+LOAD))(2);decision=SUDEKIMP_AREA_INTENT_RUN;remove_areas();
        } else if(!strcmp(mode,"remove-quit-overtake")) {
            remove_areas();decision=SUDEKIMP_AREA_INTENT_RUN;((void (__cdecl *)(void))(image+QUIT))();
        } else if(!strcmp(mode,"quit-remove-overtake")) {
            ((void (__cdecl *)(void))(image+QUIT))();decision=SUDEKIMP_AREA_INTENT_RUN;remove_areas();
        } else if(!strcmp(mode,"remove-owner-loss")) {
            image[REMOVE+4]^=1;assert(!(input(0)&255));image[REMOVE+4]^=1;assert(!admission_calls);
        } else if(!strcmp(mode,"remove-foreign-thread")) {
            HANDLE t=CreateThread(NULL,0,foreign_remove,NULL,0,NULL);assert(t);
            assert(WaitForSingleObject(t,5000)==WAIT_OBJECT_0);CloseHandle(t);assert(!admission_calls);
        }
        else if(!strcmp(mode,"load-overtake")) {
            assert(!(input(0)&255));decision=SUDEKIMP_AREA_INTENT_RUN;((void (__cdecl *)(int))(image+LOAD))(2);
        } else if(!strcmp(mode,"load-menu-overtake")) {
            ((void (__cdecl *)(int))(image+LOAD))(2);decision=SUDEKIMP_AREA_INTENT_RUN;assert(!(input(0)&255));
        } else if(!strcmp(mode,"load-owner-loss")) {
            image[LOAD+6]^=1;assert(!(input(0)&255));image[LOAD+6]^=1;assert(!admission_calls);
        } else if(!strcmp(mode,"load-foreign-thread")) {
            HANDLE t=CreateThread(NULL,0,foreign_load,NULL,0,NULL);assert(t);
            assert(WaitForSingleObject(t,5000)==WAIT_OBJECT_0);CloseHandle(t);assert(!admission_calls);
        }
        else if(!strcmp(mode,"export-overtake")) {
            assert(!(input(0)&255));decision=SUDEKIMP_AREA_INTENT_RUN;((void (__cdecl *)(void))(image+QUIT))();
        } else if(!strcmp(mode,"menu-overtake")) {
            ((void (__cdecl *)(void))(image+QUIT))();decision=SUDEKIMP_AREA_INTENT_RUN;assert(!(input(0)&255));
        } else if(!strcmp(mode,"export-owner-loss")) {
            image[QUIT+10]^=1;assert(!(input(0)&255));image[QUIT+10]^=1;assert(!admission_calls);
        } else if(!strcmp(mode,"export-foreign-thread")) {
            HANDLE t=CreateThread(NULL,0,foreign_export,NULL,0,NULL);assert(t);
            assert(WaitForSingleObject(t,5000)==WAIT_OBJECT_0);CloseHandle(t);assert(!admission_calls);
        }
        else if(!strcmp(mode,"overtake")) {
            assert(!(input(0)&255));decision=SUDEKIMP_AREA_INTENT_RUN;assert(!(input(0)&255));
        } else if(!strcmp(mode,"wrong-caller")) {action(2);assert(!admission_calls);}
        else if(!strcmp(mode,"owner-loss")) {
            image[ACTION+5]^=1;assert(!(input(0)&255));image[ACTION+5]^=1;assert(!admission_calls);
        } else if(!strcmp(mode,"foreign-thread")) {
            HANDLE t=CreateThread(NULL,0,foreign_action,NULL,0,NULL);assert(t);
            assert(WaitForSingleObject(t,5000)==WAIT_OBJECT_0);CloseHandle(t);assert(!admission_calls);
        } else assert(!"Unknown admission mode");
        assert(!events && !dispatches && !quits && !reads);snapshot_refused();
        uint8_t before[sizeof(remove_world)];memcpy(before,remove_world,sizeof(before));
        prepare_remove_world();assert(!memcmp(before,remove_world,sizeof(before)));
        assert(!SudekiMpLanStoryAreaIntentAcknowledge((HMODULE)image,&intent_consumer,&witness,newest_ticket));
        assert(!SudekiMpLanStoryAreaIntentDetach((HMODULE)image,&intent_consumer,&witness));
        assert(!SudekiMpLanStoryAreaIntentInstall((HMODULE)image));
    }
    assert(!SudekiMpLanStoryAreaIntentUninstall()); /* Observed native entry retains its adapter. */
}
int main(int argc,char **argv) {
    assert(argc==2 || argc==3);wchar_t path[MAX_PATH];
    assert(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,argv[1],-1,path,MAX_PATH));
    image=map_image(path);uint8_t *before=malloc(SUDEKIMP_EXPECTED_IMAGE_SIZE);assert(before);
    memcpy(before,image,SUDEKIMP_EXPECTED_IMAGE_SIZE);
    if(argc==3) intent_install(argv[2]);
    menu_vtable[0x44/4]=(void *)menu_close_stub;widget_vtable[0x14/4]=(void *)widget_stub;
    const unsigned sites[]={0x164b84,0x164d20,0x164c21,0x164dc1,0x164c40,0x164de0,
        0x164c7f,0x164c98,0x164e2f,0x164e90,0x164e9f,0x164f0a,0x164f0f,0x164fe1};
    const unsigned targets[]={0x49c0,0x49c0,0x1c37b0,0x1c37b0,0x1bfcb0,0x1bfcb0,
        0xa2620,0x165c00,0x165c00,0x1a630,0x7f7e0,0x1a530,0x7fef0,0xb140};
    void *stubs[]={(void *)event_stub,(void *)event_stub,(void *)dispatch_stub,(void *)dispatch_stub,
        (void *)empty_stub,(void *)empty_stub,(void *)quit_stub,(void *)menu_reset_stub,(void *)menu_reset_stub,
        (void *)background_stub,(void *)no_screen,(void *)fade_stub,(void *)pump_stub,(void *)cue_stub};
    SudekiMpRelativeCallHook calls[sizeof(sites)/sizeof(*sites)]={0};
    for(unsigned i=0;i<sizeof(sites)/sizeof(*sites);++i)
        assert(SudekiMpInstallRelativeCallHook(&calls[i],image+sites[i],image+targets[i],stubs[i]));
    /* The exported quit is a JMP, not a CALL. Gate its inner target in this
     * isolated fixture so no native world cleanup can run. */
    SudekiMpInlineHook quit_body={0},reader={0},gate={0};
    const uint8_t read_sig[]={0x55,0x8b,0xec,0x83,0xe4,0xf0,0x81,0xec,4,6,0,0};
    assert(SudekiMpInstallInlineHook(&reader,image+READER,read_sig,sizeof(read_sig),(void *)reader_stub));
    /* Complete instructions only: sub esp,8; push esi; push 0. */
    const uint8_t quit_sig[]={0x83,0xec,8,0x56,0x6a,0};
    assert(SudekiMpInstallInlineHook(&quit_body,image+0xa2620,quit_sig,sizeof(quit_sig),(void *)quit_stub));
    const unsigned starts[]={ACTION,RELOAD,QUIT,LOAD,INDEX,REMOVE,REMOVE_ALL};
    const unsigned lengths[]={0x680,0x3e,0x23,0xc0,0x33,12,0x220};DWORD protections[7],ignored;
    for(unsigned i=0;i<7;++i) assert(VirtualProtect(image+starts[i],lengths[i],PAGE_EXECUTE_READWRITE,&protections[i]));
    assert(FlushInstructionCache(GetCurrentProcess(),image,SUDEKIMP_EXPECTED_IMAGE_SIZE));
    if(argc==3) admission_cases(argv[2]);
    else {
        /* First prove late refusal still consumes event/UI state without a gate. */
        reset_case();late_refusal=TRUE;assert(!(input(0)&255));
        assert(events==1 && dispatches==1 && quits==1 && !resident[0x49] && !resident[0xe0]);
        const uint8_t action_sig[]={0x55,0x8b,0xec,0x83,0xe4,0xf8};
        assert(SudekiMpInstallInlineHook(&gate,image+ACTION,action_sig,sizeof(action_sig),(void *)action_gate));
        action_original=gate.trampoline;quit_cases();reload_cases();exported_cases();
        prepare_remove_world();remove_areas();check_removed_world();
        assert(SudekiMpRestoreInlineHook(&gate));action_original=NULL;
    }
    assert(SudekiMpRestoreInlineHook(&reader));assert(SudekiMpRestoreInlineHook(&quit_body));
    for(unsigned i=sizeof(sites)/sizeof(*sites);i>0;--i) assert(SudekiMpRestoreRelativeCallHook(&calls[i-1]));
    for(unsigned i=7;i>0;--i) assert(VirtualProtect(image+starts[i-1],lengths[i-1],protections[i-1],&ignored));
    for(unsigned i=0;i<7;++i) {
        MEMORY_BASIC_INFORMATION state;
        assert(VirtualQuery(image+starts[i],&state,sizeof(state))==sizeof(state));
        assert(state.Protect==protections[i]);
    }
    const unsigned globals[]={WORLD,SCENE,SCRIPTS,SPEED,FADE,0x3c2f78,CATALOG,RECORDS,SELECTED,COUNT,
        0x409df8,0x31f296,0x3c3070,0x3c3075,0x3c3078};
    const unsigned sizes[]={4,4,4,4,4,4,4,4,4,3,1,1,4,1,4};
    for(unsigned i=0;i<sizeof(globals)/sizeof(*globals);++i) memcpy(image+globals[i],before+globals[i],sizes[i]);
    if(argc==2) {
        assert(!memcmp(before,image,SUDEKIMP_EXPECTED_IMAGE_SIZE));
        assert(VirtualFree(image,0,MEM_RELEASE));
    } else {
        assert(!memcmp(before,image,REMOVE));
        BOOL only_action=!strcmp(argv[2],"startup-retained"),only_quit=!strcmp(argv[2],"startup-retained-export");
        BOOL only_load=!strcmp(argv[2],"startup-retained-load");
        BOOL only_remove=!strcmp(argv[2],"startup-retained-remove");
        if(only_action || only_quit || only_load) assert(!memcmp(before+REMOVE,image+REMOVE,5));
        else assert(image[REMOVE]==0xe9);
        assert(!memcmp(before+REMOVE+5,image+REMOVE+5,QUIT+5-REMOVE-5));
        if(only_quit || only_load || only_remove) assert(!memcmp(before+ACTION,image+ACTION,6));
        else assert(image[ACTION]==0xe9);
        if(only_action || only_load || only_remove) assert(!memcmp(before+QUIT+5,image+QUIT+5,6));
        else assert(image[QUIT+5]==0xe9);
        assert(!memcmp(before+QUIT+11,image+QUIT+11,LOAD-QUIT-11));
        if(only_action || only_quit || only_remove) assert(!memcmp(before+LOAD,image+LOAD,7));else assert(image[LOAD]==0xe9);
        assert(!memcmp(before+LOAD+7,image+LOAD+7,ACTION-LOAD-7));
        assert(!memcmp(before+ACTION+6,image+ACTION+6,SUDEKIMP_EXPECTED_IMAGE_SIZE-ACTION-6));
        /* Retained adapter/image survive until this isolated process exits. */
    }
    free(before);
    puts("StoryAreaIntentImageTest: PASS (native menu/load/remove wrappers and admission guard; synthetic owners; no world lease/live replay)");
    return 0;
}
