#include "hooks/lan_story_host_pause.h"
#include "engine/build_identity.h"
#include "hooks/call_hook.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/hooks/lan_story_host_pause.c"
static unsigned notices;
void SudekiMpLanStoryMenuNotice(const char *s) { assert(!strcmp(s,"Can't step away as host yet")); ++notices; }
int main(int argc,char **argv) {
    if(argc!=2) { fprintf(stderr,"requires supported SUDEKI.exe\n"); return 2; }
    wchar_t path[1024]; assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024));
    SudekiMpBuildCheck check; assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    FILE *f=fopen(argv[1],"rb"); assert(f); fseek(f,0,SEEK_END); long n=ftell(f); rewind(f);
    uint8_t *raw=malloc(n); assert(raw && fread(raw,1,n,f)==(size_t)n); fclose(f);
    IMAGE_DOS_HEADER *dos=(void *)raw; IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *b=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE); assert(b);
    memcpy(b,raw,nt->OptionalHeader.SizeOfHeaders); IMAGE_SECTION_HEADER *sections=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) memcpy(b+sections[i].VirtualAddress,raw+sections[i].PointerToRawData,sections[i].SizeOfRawData);
    free(raw);
    uint8_t before[5]; memcpy(before,b+PAUSE_CALL,5);
    for(unsigned i=0;i<sizeof(pause_context)+5;++i) {
        b[PAUSE_CONTEXT+i]^=1; assert(!SudekiMpLanStoryHostPauseInstall((HMODULE)b)); b[PAUSE_CONTEXT+i]^=1;
        assert(!pause_hook.installed);
    }
    for(unsigned repeat=0;repeat<2;++repeat) {
        assert(SudekiMpLanStoryHostPauseInstall((HMODULE)b)); assert(pause_hook.installed);
        assert(!SudekiMpLanStoryHostPauseInstall((HMODULE)b));
        assert(!pause_thread);
        pause_entry(); assert(notices==repeat+1 && pause_thread==(LONG)GetCurrentThreadId());
        pause_callbacks=1; assert(!SudekiMpLanStoryHostPauseUninstall()); pause_callbacks=0;
        LONG thread=pause_thread; pause_thread=thread+1;
        assert(!SudekiMpLanStoryHostPauseUninstall()); pause_thread=thread;
        uint8_t patched=b[PAUSE_CALL+1]; b[PAUSE_CALL+1]^=1;
        assert(!SudekiMpLanStoryHostPauseUninstall() && pause_hook.installed && pause_enabled);
        b[PAUSE_CALL+1]=patched;
        assert(SudekiMpLanStoryHostPauseUninstall()); assert(!memcmp(before,b+PAUSE_CALL,5));
    }
    assert(SudekiMpLanStoryHostPauseUninstall()); VirtualFree(b,0,MEM_RELEASE);
    puts("host M pause exact-image guard, refusal, restore and retry passed"); return 0;
}
