/* [SudekiMP] BackgroundAudio: the three XACT buffer-flag immediates and the
 * WM_KILLFOCUS XACT/Bink pause sites of the supported image, every-byte
 * refusal, install sets DSBCAPS_GLOBALFOCUS and clears both pause arguments,
 * uninstall restores. No native routine executes. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/hooks/background_audio.c"
#include "../src/hooks/call_hook.c"
void SudekiMpLogWrite(const char *text) {(void)text;}
void SudekiMpLogFormat(const char *format,...) {(void)format;}
BOOL SudekiMpCheckLoadedExecutable(HMODULE image) {(void)image;return TRUE;}
static uint8_t *map_image(const wchar_t *path) {
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(file!=INVALID_HANDLE_VALUE); DWORD size=GetFileSize(file,NULL),got=0;
    uint8_t *raw=malloc(size); assert(raw && ReadFile(file,raw,size,&got,NULL) && got==size); CloseHandle(file);
    IMAGE_DOS_HEADER *dos=(void *)raw; IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *b=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(b); memcpy(b,raw,nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *sec=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) memcpy(b+sec[i].VirtualAddress,raw+sec[i].PointerToRawData,sec[i].SizeOfRawData);
    free(raw); return b;
}
int main(int argc,char **argv) {
    wchar_t path[1024];
    assert(argc==2 && MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024));
    uint8_t *image=map_image(path);
    /* The file image is mapped at an arbitrary base: relocate the one
     * absolute operand the Bink check compares (the BinkPause IAT slot). */
    { uint32_t iat=(uint32_t)(uintptr_t)(image+RVA_BINK_PAUSE_IAT);
      uint32_t file; memcpy(&file,image+RVA_BINK_PAUSE_SITE+sizeof(bink_pause_site),4); assert(file==0x0069a274u);
      memcpy(image+RVA_BINK_PAUSE_SITE+sizeof(bink_pause_site),&iat,4); }
    assert(image_exact(image));
    for(unsigned i=0;i<FLAG_SITES;++i) for(unsigned j=0;j<8;++j) {
        image[flag_sites[i].rva+j]^=1; assert(!SudekiMpInstallBackgroundAudio((HMODULE)image) && !installed);
        image[flag_sites[i].rva+j]^=1;
    }
    for(unsigned j=0;j<sizeof(xact_pause_site)+4u;++j) {
        image[RVA_XACT_PAUSE_SITE+j]^=1; assert(!SudekiMpInstallBackgroundAudio((HMODULE)image) && !installed);
        image[RVA_XACT_PAUSE_SITE+j]^=1;
    }
    for(unsigned j=0;j<sizeof(bink_pause_site)+4u;++j) {
        image[RVA_BINK_PAUSE_SITE+j]^=1; assert(!SudekiMpInstallBackgroundAudio((HMODULE)image) && !installed);
        image[RVA_BINK_PAUSE_SITE+j]^=1;
    }
    assert(SudekiMpInstallBackgroundAudio((HMODULE)image) && SudekiMpBackgroundAudioInstalled());
    for(unsigned i=0;i<FLAG_SITES;++i) {
        uint32_t flags; memcpy(&flags,image+flag_sites[i].rva+4,4); assert(flags==0x000180a0u);
    }
    assert(image[RVA_XACT_PAUSE_SITE+PAUSE_ARG]==0 && image[RVA_BINK_PAUSE_SITE+PAUSE_ARG]==0);
    assert(SudekiMpInstallBackgroundAudio((HMODULE)image)); /* idempotent */
    assert(SudekiMpUninstallBackgroundAudio() && !SudekiMpBackgroundAudioInstalled());
    for(unsigned i=0;i<FLAG_SITES;++i) {
        uint32_t flags; memcpy(&flags,image+flag_sites[i].rva+4,4); assert(flags==0x000100a0u);
    }
    assert(image[RVA_XACT_PAUSE_SITE+PAUSE_ARG]==1 && image[RVA_BINK_PAUSE_SITE+PAUSE_ARG]==1);
    puts("BackgroundAudioImageTest: PASS (3 XACT buffer-flag sites, focus-loss XACT and Bink pause sites, every-byte refusal, set and restored)");
    VirtualFree(image,0,MEM_RELEASE); return 0;
}
