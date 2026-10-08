#include "hooks/lan_story_talos_damage_probe.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if(!(x)) {fprintf(stderr,"failed line %d: %s (error=%lu)\n",__LINE__,#x,(unsigned long)GetLastError());exit(1);} } while(0)

/* Inert image harness: native game instructions are never executed. */
void SudekiMpLogWrite(const char *message) {(void)message;}
void SudekiMpLogFormat(const char *format,...) {(void)format;}
BOOL SudekiMpLogResearchEnabled(void) {return TRUE;}

static uint8_t *load_image(const wchar_t *path) {
    SudekiMpBuildCheck check; DWORD size,got;
    CHECK(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    CHECK(f!=INVALID_HANDLE_VALUE); size=GetFileSize(f,NULL);
    CHECK(size!=INVALID_FILE_SIZE && size>sizeof(IMAGE_DOS_HEADER));
    uint8_t *file=malloc(size); CHECK(file);
    CHECK(ReadFile(f,file,size,&got,NULL) && got==size); CloseHandle(f);
    IMAGE_DOS_HEADER *dos=(IMAGE_DOS_HEADER *)file;
    CHECK(dos->e_lfanew>0 && (unsigned)dos->e_lfanew<size-sizeof(IMAGE_NT_HEADERS32));
    IMAGE_NT_HEADERS32 *nt=(IMAGE_NT_HEADERS32 *)(file+dos->e_lfanew);
    CHECK(nt->OptionalHeader.SizeOfImage==SUDEKIMP_EXPECTED_IMAGE_SIZE);
    uint8_t *image=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    CHECK(image && nt->OptionalHeader.SizeOfHeaders<=size);
    memcpy(image,file,nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *sections=IMAGE_FIRST_SECTION(nt);
    CHECK((uint8_t *)(sections+nt->FileHeader.NumberOfSections)<=file+size);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        IMAGE_SECTION_HEADER *s=sections+i;
        CHECK(s->PointerToRawData<=size && s->SizeOfRawData<=size-s->PointerToRawData);
        CHECK(s->VirtualAddress<=nt->OptionalHeader.SizeOfImage &&
            s->SizeOfRawData<=nt->OptionalHeader.SizeOfImage-s->VirtualAddress);
        memcpy(image+s->VirtualAddress,file+s->PointerToRawData,s->SizeOfRawData);
    }
    free(file); return image;
}

int main(int argc,char **argv) {
    static const uint32_t calls[]={0xd3a78u,0xdabbcu,0x1046e8u,0xd3b61u,0xd3edau,0xdab91u};
    wchar_t path[32768]; uint8_t original[6][5],prepare[16],apply[16],foreign[5];
    CHECK(argc==2 && MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,32768)>0);
    uint8_t *image=load_image(path);
    for(unsigned i=0;i<6u;++i) memcpy(original[i],image+calls[i],5u);
    memcpy(prepare,image+0xd1b00u,16u); memcpy(apply,image+0xd21d0u,16u);
    CHECK(SudekiMpLanStoryTalosDamageProbeImageMatches((HMODULE)image));
    /* Every malformed CALL fails before installation mutates any seam. */
    for(unsigned i=0;i<6u;++i) {
        image[calls[i]]=0x90;
        CHECK(!SudekiMpLanStoryTalosDamageProbeInstall((HMODULE)image));
        image[calls[i]]=original[i][0];
        for(unsigned j=0;j<6u;++j) CHECK(!memcmp(original[j],image+calls[j],5u));
        image[calls[i]+1u]^=1u;
        CHECK(!SudekiMpLanStoryTalosDamageProbeImageMatches((HMODULE)image));
        image[calls[i]+1u]^=1u;
    }
    image[0xd1b00u]^=1u; CHECK(!SudekiMpLanStoryTalosDamageProbeImageMatches((HMODULE)image)); image[0xd1b00u]^=1u;
    image[0xd21d0u]^=1u; CHECK(!SudekiMpLanStoryTalosDamageProbeImageMatches((HMODULE)image)); image[0xd21d0u]^=1u;
    CHECK(SudekiMpLanStoryTalosDamageProbeInstall((HMODULE)image));
    CHECK(!SudekiMpLanStoryTalosDamageProbeInstall((HMODULE)image));
    for(unsigned i=0;i<6u;++i) CHECK(memcmp(original[i],image+calls[i],5u));
    CHECK(!memcmp(prepare,image+0xd1b00u,16u) && !memcmp(apply,image+0xd21d0u,16u));
    /* A foreign owner blocks that restoration, while every independent CALL
     * still restores. The retained record permits an exact-owner retry. */
    memcpy(foreign,image+calls[4],5u); image[calls[4]]=0xe9;
    CHECK(!SudekiMpLanStoryTalosDamageProbeUninstall());
    CHECK(image[calls[4]]==0xe9);
    for(unsigned i=0;i<6u;++i) if(i!=4u) CHECK(!memcmp(original[i],image+calls[i],5u));
    CHECK(!SudekiMpLanStoryTalosDamageProbeInstall((HMODULE)image));
    memcpy(image+calls[4],foreign,5u);
    CHECK(SudekiMpLanStoryTalosDamageProbeUninstall());
    for(unsigned i=0;i<6u;++i) CHECK(!memcmp(original[i],image+calls[i],5u));
    CHECK(SudekiMpLanStoryTalosDamageProbeUninstall());
    CHECK(SudekiMpLanStoryTalosDamageProbeInstall((HMODULE)image));
    CHECK(SudekiMpLanStoryTalosDamageProbeUninstall());
    for(unsigned i=0;i<6u;++i) CHECK(!memcmp(original[i],image+calls[i],5u));
    CHECK(!memcmp(prepare,image+0xd1b00u,16u) && !memcmp(apply,image+0xd21d0u,16u));
    VirtualFree(image,0,MEM_RELEASE);
    puts("Talos damage probe exact-image checks passed (6 calls; mismatch rejection; retained restore/retry; reinstall; entries unchanged).");
    return 0;
}
