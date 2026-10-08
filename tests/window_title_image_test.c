/* Maps the supported SUDEKI.exe as data (not executed), points its
 * CreateWindowExA IAT slot at user32 as the loader would, and checks the
 * [SudekiMP] WindowTitle hook: exact call-site checks, title substitution only
 * for the native main-window call, pass-through for every other caller, and a
 * byte-identical restore. Usage: SudekiMP.WindowTitleImageTest <SUDEKI.exe> */
#include "hooks/window_title.h"
#include "engine/build_identity.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

enum { RVA_IAT=0x29a220u, RVA_CALL=0x28da62u, RVA_RETURN=0x28da68u, RVA_NAME=0x2c21a4u, RVA_CLASS=0x2c2514u };

static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(f!=INVALID_HANDLE_VALUE);DWORD size=GetFileSize(f,NULL),got=0;
    uint8_t *raw=malloc(size);assert(raw && ReadFile(f,raw,size,&got,NULL) && got==size);CloseHandle(f);
    IMAGE_DOS_HEADER *dos=(void *)raw;IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *b=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(b);memcpy(b,raw,nt->OptionalHeader.SizeOfHeaders);IMAGE_SECTION_HEADER *s=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        assert(s[i].PointerToRawData<=size && s[i].SizeOfRawData<=size-s[i].PointerToRawData);
        memcpy(b+s[i].VirtualAddress,raw+s[i].PointerToRawData,s[i].SizeOfRawData);
    }
    IMAGE_DATA_DIRECTORY reloc=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    uintptr_t delta=(uintptr_t)b-nt->OptionalHeader.ImageBase;
    for(unsigned offset=0;offset<reloc.Size;) {
        IMAGE_BASE_RELOCATION *block=(void *)(b+reloc.VirtualAddress+offset);
        uint16_t *items=(void *)(block+1);unsigned count=(block->SizeOfBlock-sizeof(*block))/2;
        for(unsigned i=0;i<count;++i) {
            unsigned type=items[i]>>12,rva=block->VirtualAddress+(items[i]&0xfff);
            if(type==IMAGE_REL_BASED_HIGHLOW) *(uint32_t *)(b+rva)+=(uint32_t)delta;
        }
        offset+=block->SizeOfBlock;
    }
    free(raw);return b;
}
static void write_config(const wchar_t *path,const char *title) {
    FILE *f=_wfopen(path,L"wb");assert(f);
    fprintf(f,"[SudekiMP]\r\n");
    if(title) fprintf(f,"WindowTitle=%s\r\n",title);
    fclose(f);
}
/* Calls the hooked slot as the native call site does: the callee sees the
 * native return address (RVA 0x28da68, made a RET for the test) and returns
 * through it to `resume`. */
static HWND call_as_native(uint8_t *b,const char *cls,const char *name) {
    HWND result;
    void *slot_target=*(void **)(b+RVA_IAT);
    void *fake_return=b+RVA_RETURN;
    __asm__ volatile(
        "push $1f\n\t"
        "push $0; push %[inst]; push $0; push $0\n\t"
        "push $50; push $50; push $0; push $0\n\t"
        "push $0x10000000\n\t"   /* WS_VISIBLE off; plain overlapped */
        "push %[name]; push %[cls]; push $0\n\t"
        "push %[ret]\n\t"
        "jmp *%[target]\n"
        "1:\n\t"
        : "=a"(result)
        : [inst]"r"(GetModuleHandleW(NULL)),[name]"r"(name),[cls]"r"(cls),[ret]"r"(fake_return),[target]"r"(slot_target)
        : "ecx","edx","memory","cc");
    return result;
}
int wmain(int argc,wchar_t **argv) {
    assert(argc==2);
    uint8_t *b=map_image(argv[1]);
    void **slot=(void **)(b+RVA_IAT);
    FARPROC real=GetProcAddress(GetModuleHandleW(L"user32.dll"),"CreateWindowExA");
    assert(real);
    *slot=(void *)(uintptr_t)real; /* as the Windows loader binds it */
    wchar_t temp[MAX_PATH],ini[MAX_PATH];
    GetTempPathW(MAX_PATH,temp);
    swprintf(ini,MAX_PATH,L"%lsSudekiMP-window-title-%lu.ini",temp,(unsigned long)GetCurrentProcessId());

    /* Absent or empty: nothing installed. */
    write_config(ini,NULL);
    assert(SudekiMpWindowTitleInstall((HMODULE)b,ini) && *slot==(void *)(uintptr_t)real);
    /* Non-ASCII refused without touching the slot. */
    { FILE *f=_wfopen(ini,L"wb"); fprintf(f,"[SudekiMP]\r\nWindowTitle=Caf\xc3\xa9\r\n"); fclose(f); }
    assert(!SudekiMpWindowTitleInstall((HMODULE)b,ini) && *slot==(void *)(uintptr_t)real);
    /* Wrong call-site bytes refused. */
    write_config(ini,"SudekiMP HOST");
    b[RVA_CALL+1]^=1; assert(!SudekiMpWindowTitleInstall((HMODULE)b,ini) && *slot==(void *)(uintptr_t)real); b[RVA_CALL+1]^=1;
    b[RVA_NAME]^=1; assert(!SudekiMpWindowTitleInstall((HMODULE)b,ini)); b[RVA_NAME]^=1;
    /* Exact image: installed. */
    assert(SudekiMpWindowTitleInstall((HMODULE)b,ini) && *slot!=(void *)(uintptr_t)real);

    /* The native call: register its class so the window really exists. */
    WNDCLASSA wc={0}; wc.lpfnWndProc=DefWindowProcA; wc.hInstance=GetModuleHandleW(NULL); wc.lpszClassName=(const char *)(b+RVA_CLASS);
    assert(RegisterClassA(&wc));
    DWORD old;
    assert(VirtualProtect(b+RVA_RETURN,1,PAGE_EXECUTE_READWRITE,&old));
    uint8_t saved=b[RVA_RETURN]; b[RVA_RETURN]=0xc3;
    HWND native=call_as_native(b,(const char *)(b+RVA_CLASS),(const char *)(b+RVA_NAME));
    char text[64]={0};
    assert(native && GetWindowTextA(native,text,sizeof(text)) && !strcmp(text,"SudekiMP HOST"));
    DestroyWindow(native);
    /* Same name from another caller passes through unchanged. */
    typedef HWND (WINAPI *Create)(DWORD,LPCSTR,LPCSTR,DWORD,int,int,int,int,HWND,HMENU,HINSTANCE,LPVOID);
    HWND other=((Create)*slot)(0,(const char *)(b+RVA_CLASS),(const char *)(b+RVA_NAME),0,0,0,50,50,NULL,NULL,GetModuleHandleW(NULL),NULL);
    assert(other && GetWindowTextA(other,text,sizeof(text)) && !strcmp(text,"Sudeki"));
    DestroyWindow(other);
    /* Native return address but a different name pointer: unchanged too. */
    HWND named=call_as_native(b,(const char *)(b+RVA_CLASS),"Other");
    assert(named && GetWindowTextA(named,text,sizeof(text)) && !strcmp(text,"Other"));
    DestroyWindow(named);
    b[RVA_RETURN]=saved; VirtualProtect(b+RVA_RETURN,1,old,&old);

    /* Restore is byte-identical; a second install is refused (one owner). */
    assert(!SudekiMpWindowTitleInstall((HMODULE)b,ini));
    assert(SudekiMpWindowTitleUninstall() && *slot==(void *)(uintptr_t)real);
    DeleteFileW(ini);
    puts("WindowTitleImageTest: passed (exact call site, native title only, pass-through, restore)");
    return 0;
}
