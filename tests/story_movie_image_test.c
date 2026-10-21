#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "hooks/call_hook.h"
static DWORD tick=1000;
static DWORD fixture_clock(void) {return tick;}
#define GetTickCount fixture_clock
#include "../src/hooks/lan_story_movie.c"
#undef GetTickCount
#include "../src/hooks/lan_arena_startup_movie_skip.c"
static unsigned role=1,plays,stops,ticks,prepares,logos;
static BOOL available=TRUE,prepare_ok=TRUE,play_ok=TRUE;
static SudekiMpStoryMovie offered={1,7,9,0,1,1};
static SudekiMpLanPartyLease offered_lease={12,2,1};
static unsigned deferred_logs,late_logs,all_logs;
void SudekiMpLogFormat(const char *fmt,...) {
    char line[512];va_list ap;va_start(ap,fmt);vsnprintf(line,sizeof(line),fmt,ap);va_end(ap);
    if(strstr(line,"story_movie ")) ++all_logs;
    if(strstr(line,"event=client_prepare_deferred ")) ++deferred_logs;
    if(strstr(line,"event=client_late_start ")) ++late_logs;
}
void SudekiMpLogWrite(const char *s) {(void)s;}
unsigned SudekiMpLanPartyLocalSeat(SudekiMpLanPartySession *s) {assert(s==(void *)1);return role;}
BOOL SudekiMpLanPartyMovieGet(SudekiMpLanPartySession *s,uint32_t now,SudekiMpLanPartyLease *l,SudekiMpStoryMovie *v) {
    assert(s==(void *)1 && now==tick);*v=offered;*l=offered_lease;return available;
}
BOOL SudekiMpLanPartyMovieBegin(SudekiMpLanPartySession *s,uint32_t now,SudekiMpStoryMovie *v) {
    assert(s==(void *)1 && now==tick);*v=offered;return available;
}
void SudekiMpLanPartyMovieEnd(SudekiMpLanPartySession *s,const SudekiMpStoryMovie *v,uint32_t now) {
    assert(s==(void *)1 && v->serial==offered.serial && now==tick);++stops;
}
static unsigned char __attribute__((cdecl,regparm(0))) fake_tick(void) {++ticks;return 1;}
static unsigned char __attribute__((cdecl,regparm(0))) fake_stop(void) {++stops;*(void **)(base+MOVIE_SLOT)=NULL;return 1;}
static BOOL prepare(void) {++prepares;return prepare_ok;}
static BOOL __attribute__((cdecl,regparm(0))) fake_play(const char *name,BOOL skippable) {
    assert(!skippable);
    if(!strcmp(name,"Publisher.bik")) {++logos;return TRUE;}
    assert(!strcmp(name,"FMA04A"));++plays;
    assert(active_calls==1 && native_movie_calls==1);
    assert(!SudekiMpLanStoryMovieUninstall()); /* dependencies survive in-flight native call */
    InterlockedExchange(&closing,0);
    if(!play_ok)return FALSE;
    *(void **)(base+MOVIE_SLOT)=(void *)0x1234;
    unsigned prior=stops;assert(movie_tick() && stops==prior); /* still active */
    offered.active=0;assert(movie_tick() && stops==prior+1 && !*(void **)(base+MOVIE_SLOT));
    return TRUE;
}
static void relative(uint8_t *p,const void *to,unsigned opcode) {
    p[0]=(uint8_t)opcode;int32_t d=(const uint8_t *)to-(p+5);memcpy(p+1,&d,4);
}
static DWORD WINAPI wrong_thread_uninstall(void *unused) {
    (void)unused;return SudekiMpLanStoryMovieUninstall()?1u:0u;
}
static DWORD WINAPI idle(void *event) {WaitForSingleObject(event,INFINITE);return 0;}
static uint8_t *fixture(HANDLE a,HANDLE b) {
    uint8_t *image=VirtualAlloc(NULL,SUDEKIMP_EXPECTED_IMAGE_SIZE,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);assert(image);
    IMAGE_DOS_HEADER *dos=(void *)image;dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=0x80;
    IMAGE_NT_HEADERS32 *nt=(void *)(image+0x80);nt->Signature=IMAGE_NT_SIGNATURE;
    nt->FileHeader.Machine=IMAGE_FILE_MACHINE_I386;nt->FileHeader.TimeDateStamp=SUDEKIMP_EXPECTED_TIMESTAMP;
    nt->OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR32_MAGIC;nt->OptionalHeader.SizeOfImage=SUDEKIMP_EXPECTED_IMAGE_SIZE;
    memcpy(image+0x104dd6,(uint8_t[]){0x80,0x7d,0x0c,0,0x74,0x3e},6);
    memcpy(image+0x104e4f,(uint8_t[]){0x75,0x80},2);
    memcpy(image+0x1bf455,(uint8_t[]){0x53,0x33,0xdb,0x56,0x57,0x3b,0xc3},7);
    memcpy(image+0x1bf4d8,(uint8_t[]){0x5f,0x5e,0xb0,1,0x5b,0xc3},6);
    image[MOVIE_STOP]=0xa1;*(void **)(image+MOVIE_STOP+1)=image+MOVIE_SLOT;
    relative(image+LOOP_CALL,image+MOVIE_TICK,0xe8);
    relative(image+0x104cf1,image+MOVIE_STOP,0xe8);
    relative(image+0x104e8e,image+0x104cf0,0xe8);
    relative(image+0x104d9c,image+0x104c10,0xe8);
    memcpy(image+0x104d90,(uint8_t[]){0x55,0x8b,0xec,0x83,0xe4,0xf8,0x89,0xec,0x5d},9);
    /* The fixture's native body is a cdecl spy, entered via the real inline
     * trampoline with the production caller's actual stack arguments. */
    relative(image+0x104d99,fake_play,0xe9);
    /* Overlap with the preflight begin-call: separate jmp destination tail. */
    relative(image+0x104d99,image+0x105000,0xe9);
    relative(image+0x105000,fake_play,0xe9);
    relative(image+0x104d9c,image+0x104c10,0xe8);
    *(HANDLE *)(image+WORKER_A)=a;*(HANDLE *)(image+WORKER_B)=b;
    return image;
}
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(file!=INVALID_HANDLE_VALUE);DWORD size=GetFileSize(file,NULL),got=0;
    uint8_t *raw=malloc(size);assert(raw && ReadFile(file,raw,size,&got,NULL) && got==size);CloseHandle(file);
    IMAGE_DOS_HEADER *dos=(void *)raw;IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *b=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
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

static void exact_image(const char *path) {
    wchar_t wide[2048];assert(MultiByteToWideChar(CP_UTF8,0,path,-1,wide,2048));
    uint8_t *image=map_image(wide);assert(SudekiMpLanStoryMovieImageMatches((HMODULE)image));
    unsigned guards[]={0x104dd6,0x104e4f,0x1bf455,0x1bf4d8,MOVIE_STOP+1,LOOP_CALL+1,0x104cf2,0x104e8f,0x104d9d};
    for(unsigned i=0;i<sizeof(guards)/sizeof(*guards);++i) {image[guards[i]]^=1;
        assert(!SudekiMpLanStoryMovieImageMatches((HMODULE)image));image[guards[i]]^=1;}
    assert(VirtualFree(image,0,MEM_RELEASE));
}
int main(int argc,char **argv) {
    if(argc>1)exact_image(argv[1]);
    HANDLE done=CreateEventW(NULL,TRUE,FALSE,NULL),a=CreateThread(NULL,0,idle,done,0,NULL),b=CreateThread(NULL,0,idle,done,0,NULL);
    assert(done && a && b);uint8_t *image=fixture(a,b);
    assert(castle_movie("d:\\movies\\fma04a") && castle_movie("FMA04A.bik"));
    assert(!castle_movie("FMA04A.bik.extra") && !castle_movie("xfma04a") && !castle_movie(NULL));
    image[0x104d90]^=1; /* second install stage fails after the loop patch */
    assert(!SudekiMpLanStoryMovieInstall((HMODULE)image,(void *)1) && !loop_hook.installed && !observer_attached);
    image[0x104d90]^=1;
    assert(SudekiMpLanStoryMovieInstall((HMODULE)image,(void *)1));
    /* After preflight, replace only native implementation bodies with spies.
     * Keep the real movie-loop callsite and real hook/trampoline transaction. */
    relative(image+0x104d96,fake_play,0xe9); /* trampoline owns first six bytes */
    /* Trampoline copied prologue expects its epilogue before C entry. */
    memcpy(image+0x104d96,(uint8_t[]){0x89,0xec,0x5d},3);relative(image+0x104d99,fake_play,0xe9);
    relative(image+MOVIE_TICK,fake_tick,0xe9);relative(image+MOVIE_STOP,fake_stop,0xe9);
    assert(((MoviePlayFunction)(image+0x104d90))("Publisher.bik",FALSE) && logos==1);
    assert(SudekiMpLanStoryMovieService(TRUE,prepare));assert(plays==1 && stops==1 && prepares==1);
    offered.active=1;assert(SudekiMpLanStoryMovieService(TRUE,prepare) && plays==1);
    ++offered.serial;offered.elapsed_ms=1501;assert(SudekiMpLanStoryMovieService(TRUE,prepare) && plays==1);
    ++offered.serial;offered.elapsed_ms=0;prepare_ok=FALSE;
    for(unsigned i=0;i<100;++i)assert(SudekiMpLanStoryMovieService(TRUE,prepare) && plays==1);
    assert(deferred_logs==1 && late_logs==1);prepare_ok=TRUE;
    *(HANDLE *)(image+WORKER_A)=GetCurrentThread();assert(SudekiMpLanStoryMovieService(TRUE,prepare) && plays==1);
    *(HANDLE *)(image+WORKER_A)=a;*(void **)(image+MOVIE_SLOT)=(void *)0x7777;
    assert(SudekiMpLanStoryMovieService(TRUE,prepare) && plays==1);*(void **)(image+MOVIE_SLOT)=NULL;
    play_ok=FALSE;assert(SudekiMpLanStoryMovieService(TRUE,prepare) && plays==2 && active_calls==0);
    /* Once per reason/serial, and bounded over the complete install. */
    prepare_ok=FALSE;offered.active=1;
    for(unsigned i=0;i<80;++i) {++offered.serial;assert(SudekiMpLanStoryMovieService(TRUE,prepare));}
    assert(movie_logs==64 && all_logs==64);prepare_ok=TRUE;
    available=FALSE;assert(!SudekiMpLanStoryMovieService(TRUE,prepare));available=TRUE;
    role=0;assert(!SudekiMpLanStoryMovieService(TRUE,prepare));
    assert(!host_begin("other_movie") && host_begin("fma04a"));assert(!host_begin("fma04a"));host_end();assert(active_calls==0);
    role=1;client_playing=TRUE;native_handle=(void *)0x1234;*(void **)(image+MOVIE_SLOT)=native_handle;
    current_movie=offered;current_connection=offered_lease;available=FALSE;
    unsigned old=stops;movie_tick();assert(stops==old+1);
    *(void **)(image+MOVIE_SLOT)=(void *)0x4444;movie_tick();assert(stops==old+1); /* foreign owner untouched */
    available=TRUE;*(void **)(image+MOVIE_SLOT)=native_handle;
    InterlockedExchange(&closing,1);movie_tick();assert(stops==old+2);
    InterlockedExchange(&closing,0);
    client_playing=FALSE;*(void **)(image+MOVIE_SLOT)=NULL;
    HANDLE wrong=CreateThread(NULL,0,wrong_thread_uninstall,NULL,0,NULL);DWORD result=1;
    assert(wrong && WaitForSingleObject(wrong,1000)==WAIT_OBJECT_0 && GetExitCodeThread(wrong,&result) && !result);
    CloseHandle(wrong);assert(observer_attached && loop_hook.installed && movie_session);
    uint8_t entry=image[0x104d95];image[0x104d95]^=1;
    assert(!SudekiMpLanStoryMovieUninstall() && observer_attached && loop_hook.installed && movie_session);
    image[0x104d95]=entry;
    uint8_t byte=image[LOOP_CALL+1];image[LOOP_CALL+1]^=1;
    assert(!SudekiMpLanStoryMovieUninstall() && base && movie_session);image[LOOP_CALL+1]=byte;
    assert(SudekiMpLanStoryMovieUninstall() && !base && !movie_session);
    assert(!SudekiMpLanArenaStartupMovieSkipInstalled());
    /* Existing logo policy survives observer detach; no second MoviePlay hook. */
    assert(SudekiMpInstallLanArenaStartupMovieSkip((HMODULE)image));
    assert(SudekiMpLanArenaMovieObserve((HMODULE)image,host_begin,host_end));
    assert(((MoviePlayFunction)(image+0x104d90))("Publisher.bik",FALSE) && logos==1);
    assert(SudekiMpLanArenaMovieUnobserve() && SudekiMpLanArenaStartupMovieSkipInstalled());
    assert(SudekiMpUninstallLanArenaStartupMovieSkip());
    SetEvent(done);assert(WaitForSingleObject(a,1000)==WAIT_OBJECT_0 && WaitForSingleObject(b,1000)==WAIT_OBJECT_0);
    CloseHandle(a);CloseHandle(b);CloseHandle(done);VirtualFree(image,0,MEM_RELEASE);
    puts("StoryMovieImageTest: PASS (native bodies stubbed; optional exact-image preflight)");return 0;
}
