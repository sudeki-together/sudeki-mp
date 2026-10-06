/* Supported-image close-control-flow and production close-adapter fixture.
 * Only the native WndProc and its nested message pump execute. Every selected
 * retail OS/cleanup call is synthetic: no game window, native resource destruction,
 * thread suspend/cancel, loading or gameplay. A fixture gate demonstrates the
 * early boundary. Additional modes exercise the production adapter with fake
 * window-property/posting APIs; no coordinator or native area drain is supplied.
 * The properties mode alone uses an owned hidden message-only test window and
 * real Win32 properties/posting, never attaching the retail procedure to it.
 * Defer permission is NOT a native lifetime or quiescence witness. */
#include "engine/build_identity.h"
#include "hooks/call_hook.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static BOOL close_test_install(SudekiMpInlineHook *,uint8_t *,const uint8_t *,size_t,const void *);
static BOOL close_test_restore(SudekiMpInlineHook *);
static DWORD close_test_thread(void);
static DWORD WINAPI close_test_window_thread(HWND,LPDWORD);
static LONG_PTR WINAPI close_test_window_proc(HWND,int);
static BOOL WINAPI close_test_post(HWND,UINT,WPARAM,LPARAM);
static HANDLE WINAPI close_test_get_prop(HWND,LPCWSTR);
static BOOL WINAPI close_test_set_prop(HWND,LPCWSTR,HANDLE);
static HANDLE WINAPI close_test_remove_prop(HWND,LPCWSTR);
#define SudekiMpInstallInlineHook close_test_install
#define SudekiMpRestoreInlineHook close_test_restore
#define GetCurrentThreadId close_test_thread
#define GetWindowThreadProcessId close_test_window_thread
#undef GetWindowLongPtrA
#define GetWindowLongPtrA close_test_window_proc
#define PostMessageW close_test_post
#define GetPropW close_test_get_prop
#define SetPropW close_test_set_prop
#define RemovePropW close_test_remove_prop
#include "../src/hooks/lan_story_area_close.c"
#undef SudekiMpInstallInlineHook
#undef SudekiMpRestoreInlineHook
#undef GetCurrentThreadId
#undef GetWindowThreadProcessId
#undef GetWindowLongPtrA
#undef PostMessageW
#undef GetPropW
#undef SetPropW
#undef RemovePropW
#define GetWindowLongPtrA GetWindowLongA
#if !defined(__GNUC__) || !defined(__i386__)
#error "This supported-image experiment requires x86 GCC"
#endif
enum { WINDOW_PROC=0x28d670, PROC_SIZE=0x1c9, PUMP=0x28bf50, PUMP_SIZE=0xb5,
    CLOSE_BEGIN=0x28d791, CLOSE_CALL=0x28d798, CLOSE_RETURN=0x28d7a7,
    RUNNING=0x3c30b0, DEVICE=0x409e30 };
static uint8_t *mapped,device[0x20];
static unsigned defer_close __attribute__((used)),deferrals __attribute__((used));
static void *close_trampoline __attribute__((used)),*close_return __attribute__((used));
static unsigned events,cleanup_count,destroy_count,quit_count,default_count;
static unsigned cleanup_order,destroy_order,quit_order,outer_busy,cleanup_busy;
static unsigned peek_count,dispatch_count,late_skip_count;
static HWND expected_window;
typedef LRESULT (WINAPI *NativeProc)(HWND,UINT,WPARAM,LPARAM);
static NativeProc native_proc;
static unsigned close_installs,close_restores,close_fail_install,close_fail_restore;
static BOOL close_fail_after_install,close_witness_ok=TRUE,close_disturb;
static BOOL close_bad_window,close_bad_proc,close_post_fail,close_post_nested,nested_cleanup;
static unsigned close_posts;
static DWORD close_window_thread;
static SudekiMpControlUpdateDispatchWitness close_dispatch;
static int coordinator;
static uint64_t held_ticket;
static HANDLE close_test_property;
static BOOL close_property_failure,close_remove_failure,close_real_properties;
static HANDLE WINAPI close_test_get_prop(HWND window,LPCWSTR name) {
    assert(window==expected_window && !wcscmp(name,close_property));
    return close_real_properties?GetPropW(window,name):close_test_property;
}
static BOOL WINAPI close_test_set_prop(HWND window,LPCWSTR name,HANDLE value) {
    assert(window==expected_window && !wcscmp(name,close_property) && value);
    if(close_property_failure) return FALSE;
    if(close_real_properties) return SetPropW(window,name,value);
    close_test_property=value;return TRUE;
}
static HANDLE WINAPI close_test_remove_prop(HWND window,LPCWSTR name) {
    assert(window==expected_window && !wcscmp(name,close_property));
    if(close_remove_failure) {SetLastError(ERROR_ACCESS_DENIED);return NULL;}
    if(close_real_properties) return RemovePropW(window,name);
    HANDLE value=close_test_property;close_test_property=NULL;return value;
}
static BOOL close_test_install(SudekiMpInlineHook *h,uint8_t *p,const uint8_t *e,size_t n,const void *r) {
    if(++close_installs==close_fail_install) {SetLastError(ERROR_WRITE_FAULT);return FALSE;}
    BOOL ok=SudekiMpInstallInlineHook(h,p,e,n,r);
    if(ok && close_fail_after_install) {SetLastError(ERROR_WRITE_FAULT);return FALSE;}
    return ok;
}
static BOOL close_test_restore(SudekiMpInlineHook *h) {
    if(++close_restores==close_fail_restore) {SetLastError(ERROR_WRITE_FAULT);return FALSE;}
    return SudekiMpRestoreInlineHook(h);
}
static DWORD close_test_thread(void) {
    DWORD id=GetCurrentThreadId();
    if(close_disturb) {
        SetLastError(0x9999);
        __asm__ volatile("fninit; fld1; pxor %%xmm0,%%xmm0; pxor %%xmm7,%%xmm7" : : : "memory");
    }
    return id;
}
BOOL SudekiMpControlSeparationUpdateDispatchWitnessStillExact(const SudekiMpControlUpdateDispatchWitness *w) {
    return close_witness_ok && w && w->native_thread_id==GetCurrentThreadId();
}
static DWORD WINAPI close_test_window_thread(HWND window,LPDWORD process) {
    if(close_real_properties) return GetWindowThreadProcessId(window,process);
    assert(process);*process=close_bad_window?0:GetCurrentProcessId();
    return window==expected_window?close_window_thread:0;
}
static LONG_PTR WINAPI close_test_window_proc(HWND window,int which) {
    assert(window==expected_window && which==GWLP_WNDPROC);
    return close_bad_proc?0:(LONG_PTR)(mapped+NATIVE_WNDPROC);
}
static BOOL WINAPI close_test_post(HWND window,UINT message,WPARAM w,LPARAM l) {
    assert(window==expected_window && message==WM_CLOSE && !w && !l);++close_posts;
    if(close_real_properties) return PostMessageW(window,message,w,l);
    if(close_post_nested) {
        assert(close_posting);
        assert(native_proc(window,WM_CLOSE,0,0)==0);
        assert(!SudekiMpLanStoryAreaCloseRelease((HMODULE)mapped,&coordinator,&close_dispatch,held_ticket));
    }
    if(close_post_fail) {SetLastError(ERROR_NOT_ENOUGH_QUOTA);return FALSE;}
    return TRUE;
}
static void __attribute__((naked,noinline)) early_gate(void) {
    __asm__ volatile("pushfl; cmpl $0,_defer_close; je 1f; incl _deferrals;"
        "popfl; jmp *_close_return; 1: popfl; jmp *_close_trampoline");
}
static void __cdecl cleanup_stub(void) {
    assert(mapped[RUNNING]==0);
    cleanup_order=++events;++cleanup_count;cleanup_busy=outer_busy;
    if(nested_cleanup) {
        nested_cleanup=FALSE;
        assert(native_proc(expected_window,WM_CLOSE,0,0)==0);
        assert(cleanup_count==1); /* No second teardown inside the first. */
    }
}
static void __cdecl late_skip_stub(void) { ++late_skip_count; }
static void WINAPI quit_stub(int code) {
    assert(code==0);quit_order=++events;++quit_count;
}
static BOOL WINAPI destroy_stub(HWND window) {
    assert(window==expected_window && mapped[RUNNING]==0);
    destroy_order=++events;++destroy_count;
    /* Synthetic DestroyWindow delivery, not a real window destruction. */
    assert(native_proc(window,WM_DESTROY,0,0)==0);
    return TRUE;
}
static LRESULT WINAPI default_stub(HWND window,UINT message,WPARAM w,LPARAM l) {
    assert(window==expected_window);
    if(message==WM_APP+7) assert(w==0x1357 && l==0x2468);
    else assert(message==WM_ACTIVATEAPP && !l);
    ++default_count;return 0x1234;
}
static HMODULE WINAPI module_stub(LPCSTR name) {
    assert(!name);return (HMODULE)mapped;
}
static HACCEL WINAPI accelerator_stub(HINSTANCE instance,LPCSTR name) {
    assert(instance==(HINSTANCE)mapped && (uintptr_t)name==0x65);
    return (HACCEL)(uintptr_t)0x123456;
}
static BOOL WINAPI peek_stub(LPMSG message,HWND window,UINT first,UINT last,UINT remove) {
    assert(!window && !first && !last && remove==PM_REMOVE);
    if(peek_count++) return FALSE;
    memset(message,0,sizeof(*message));message->hwnd=expected_window;message->message=WM_CLOSE;
    return TRUE;
}
static int WINAPI translate_accelerator_stub(HWND window,HACCEL accelerator,LPMSG message) {
    assert(window==expected_window && accelerator==(HACCEL)(uintptr_t)0x123456);
    assert(message->hwnd==window && message->message==WM_CLOSE);return 0;
}
static BOOL WINAPI translate_stub(const MSG *message) {
    assert(message->message==WM_CLOSE);return TRUE;
}
static LRESULT WINAPI dispatch_stub(const MSG *message) {
    assert(outer_busy==1 && message->message==WM_CLOSE);++dispatch_count;
    return native_proc(message->hwnd,message->message,message->wParam,message->lParam);
}
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(f!=INVALID_HANDLE_VALUE);DWORD size=GetFileSize(f,NULL),got=0;
    uint8_t *raw=malloc(size);assert(raw && ReadFile(f,raw,size,&got,NULL) && got==size);CloseHandle(f);
    IMAGE_DOS_HEADER *dos=(void *)raw;IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *b=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(b);memcpy(b,raw,nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *s=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        assert(s[i].PointerToRawData<=size && s[i].SizeOfRawData<=size-s[i].PointerToRawData);
        assert(s[i].VirtualAddress<=nt->OptionalHeader.SizeOfImage &&
            s[i].SizeOfRawData<=nt->OptionalHeader.SizeOfImage-s[i].VirtualAddress);
        memcpy(b+s[i].VirtualAddress,raw+s[i].PointerToRawData,s[i].SizeOfRawData);
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
static void reset_case(void) {
    mapped[RUNNING]=1;device[0x11]=1;defer_close=deferrals=0;
    events=cleanup_count=destroy_count=quit_count=default_count=0;
    cleanup_order=destroy_order=quit_order=outer_busy=cleanup_busy=0;
    peek_count=dispatch_count=late_skip_count=0;
}
static void run_pump(void) {
    outer_busy=1;
    assert(((unsigned char (__cdecl *)(void))(mapped+PUMP))()==device[0x11]);
    assert(peek_count==2 && dispatch_count==1);
    outer_busy=0;
}
static void closed(BOOL cleanup) {
    assert(mapped[RUNNING]==0 && cleanup_count==(unsigned)cleanup);
    assert(destroy_count==1 && quit_count==1);
    assert(events==(cleanup?3u:2u));
    assert(cleanup_order==(cleanup?1u:0u));
    assert(destroy_order==(cleanup?2u:1u) && quit_order==(cleanup?3u:2u));
}
static void production_lifecycle(void) {
    static const unsigned mismatch[]={NATIVE_WNDPROC,NATIVE_WNDPROC+8,NATIVE_WNDPROC+21,NATIVE_WNDPROC+30,
        CLOSE_SITE,CLOSE_SITE+2,CLOSE_SITE+7,CLOSE_SITE+12,CLOSE_SITE+18,CLOSE_TAIL,CLOSE_TAIL+8};
    for(unsigned i=0;i<sizeof(mismatch)/sizeof(mismatch[0]);++i) {
        mapped[mismatch[i]]^=1;
        assert(!SudekiMpLanStoryAreaCloseInstall((HMODULE)mapped) && !close_base);
        mapped[mismatch[i]]^=1;
    }
    *(void **)(mapped+ROOT_SLOT)=mapped;
    assert(!SudekiMpLanStoryAreaCloseInstall((HMODULE)mapped));*(void **)(mapped+ROOT_SLOT)=NULL;
    *(void **)(mapped+WORLD_SLOT)=mapped;
    assert(!SudekiMpLanStoryAreaCloseInstall((HMODULE)mapped));*(void **)(mapped+WORLD_SLOT)=NULL;
    close_installs=close_restores=0;close_fail_install=1;
    assert(!SudekiMpLanStoryAreaCloseInstall((HMODULE)mapped) && !close_base);
    close_fail_install=0;close_installs=close_restores=0;
    close_fail_after_install=TRUE;close_fail_restore=1;
    assert(!SudekiMpLanStoryAreaCloseInstall((HMODULE)mapped) && close_base && close_original);
    assert(!SudekiMpLanStoryAreaCloseInstall((HMODULE)mapped));
    close_fail_after_install=FALSE;close_fail_restore=0;
    assert(SudekiMpLanStoryAreaCloseUninstall(NULL));
    assert(SudekiMpLanStoryAreaCloseInstall((HMODULE)mapped));
    close_restores=0;close_fail_restore=1;
    assert(!SudekiMpLanStoryAreaCloseUninstall(NULL) && close_base && close_original);
    close_fail_restore=0;assert(SudekiMpLanStoryAreaCloseUninstall(NULL));
    assert(SudekiMpLanStoryAreaCloseInstall((HMODULE)mapped));
    uint8_t saved=mapped[CLOSE_SITE];mapped[CLOSE_SITE]=0x90;
    assert(!SudekiMpLanStoryAreaCloseUninstall(NULL) && close_base && mapped[CLOSE_SITE]==0x90);
    mapped[CLOSE_SITE]=saved;assert(SudekiMpLanStoryAreaCloseUninstall(NULL));
    assert(!close_base && !close_original && !close_continuation);
}
static void hold_window(void) {
    close_window_thread=GetCurrentThreadId();
    close_dispatch=(SudekiMpControlUpdateDispatchWitness){.dispatch_serial=17,
        .service_post_original_exact=1,.native_thread_id=GetCurrentThreadId()};
    if(!close_record.ticket && !close_real_properties) {
        uint64_t refused=887;
        close_test_property=(HANDLE)(uintptr_t)0x7777;
        assert(!SudekiMpLanStoryAreaCloseHold((HMODULE)mapped,&coordinator,&close_dispatch,expected_window,&refused));
        assert(refused==887 && close_test_property==(HANDLE)(uintptr_t)0x7777 && !close_record.ticket);
        close_test_property=NULL;close_property_failure=TRUE;
        assert(!SudekiMpLanStoryAreaCloseHold((HMODULE)mapped,&coordinator,&close_dispatch,expected_window,&refused));
        assert(refused==887 && !close_test_property && !close_record.ticket && !close_unknown);
        close_property_failure=FALSE;
    }
    assert(SudekiMpLanStoryAreaCloseHold((HMODULE)mapped,&coordinator,&close_dispatch,expected_window,&held_ticket));
}
static SudekiMpStoryAreaCloseReceipt close_receipt(unsigned phase) {
    SudekiMpStoryAreaCloseReceipt r={0};
    assert(SudekiMpLanStoryAreaCloseRead((HMODULE)mapped,&coordinator,&close_dispatch,held_ticket,&r));
    assert(r.ticket==held_ticket && r.window==expected_window && r.phase==phase);return r;
}
static DWORD WINAPI foreign_close(void *unused) {
    (void)unused;assert(native_proc(expected_window,WM_CLOSE,0,0)==0);return 0;
}
static DWORD WINAPI close_handoff(void *unused) {
    (void)unused;hold_window();
    assert(close_native_thread==GetCurrentThreadId() && close_native_thread!=close_startup_thread);
    assert(SudekiMpLanStoryAreaCloseRelease((HMODULE)mapped,&coordinator,&close_dispatch,held_ticket));
    assert(SudekiMpLanStoryAreaCloseUninstall(&close_dispatch));return 0;
}
static uint32_t close_abi_frame[6] __attribute__((used)),close_abi_regs[9] __attribute__((used));
static uint32_t close_abi_stack __attribute__((used));
static unsigned close_abi_pass __attribute__((used)),close_abi_skip __attribute__((used));
static void __attribute__((naked,noinline)) close_abi_original(void) {
    __asm__ volatile("pushfl; incl _close_abi_pass; popfl; ret");
}
static void __attribute__((naked,noinline)) close_abi_continuation(void) {
    __asm__ volatile("pushfl; incl _close_abi_skip; popfl; ret");
}
static void __attribute__((naked,noinline)) close_abi_invoke(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,_close_abi_stack;"
        "mov $0x11111111,%eax; mov $0x22222222,%ecx; mov $0x33333333,%edx;"
        "mov $0x44444444,%ebx; mov $_close_abi_frame,%ebp; mov $0x66666666,%esi; mov $0x77777777,%edi;"
        "std; stc; call _close_entry; mov %eax,_close_abi_regs; mov %ecx,_close_abi_regs+4;"
        "mov %edx,_close_abi_regs+8; mov %ebx,_close_abi_regs+12; mov %ebp,_close_abi_regs+16;"
        "mov %esi,_close_abi_regs+20; mov %edi,_close_abi_regs+24; pushfl; pop _close_abi_regs+28;"
        "mov %esp,_close_abi_regs+32; popal; popfl; ret");
}
static void __attribute__((noinline,force_align_arg_pointer)) close_abi_check(BOOL blocked) {
    uint8_t before[512] __attribute__((aligned(16)))={0},after[512] __attribute__((aligned(16)))={0};
    uint16_t control=0x077f;uint32_t mxcsr=0x3f80;
    unsigned pass=close_abi_pass,skip=close_abi_skip;
    close_abi_frame[2]=(uint32_t)(uintptr_t)expected_window;close_disturb=TRUE;SetLastError(0x7788);
    __asm__ volatile("fninit; fldcw %1; ldmxcsr %2; fldpi; fld1; fldl2t;"
        "pcmpeqd %%xmm0,%%xmm0; pcmpeqd %%xmm7,%%xmm7; fxsave %0"
        : "=m"(before) : "m"(control),"m"(mxcsr) : "memory");
    close_abi_invoke();
    __asm__ volatile("fxsave %0; fninit" : "=m"(after) : : "memory");
    close_disturb=FALSE;
    assert(GetLastError()==0x7788 && !memcmp(before,after,160) && !memcmp(before+160,after+160,128));
    for(unsigned i=0;i<7;++i)
        assert(close_abi_regs[i]==(i==4?(uint32_t)(uintptr_t)close_abi_frame:0x11111111u*(i+1)));
    assert((close_abi_regs[7]&0x401)==0x401 && close_abi_regs[8]==close_abi_stack);
    assert(close_abi_pass==pass+!blocked && close_abi_skip==skip+!!blocked);
}
static BOOL production_native(const char *mode,SudekiMpRelativeCallHook *cleanup) {
    /* Restore fixture dependency long enough to validate the unmodified
     * retail close sequence. Reapply it only after production installation. */
    assert(SudekiMpRestoreRelativeCallHook(cleanup));
    assert(SudekiMpLanStoryAreaCloseInstall((HMODULE)mapped));
    assert(SudekiMpInstallRelativeCallHook(cleanup,mapped+CLOSE_CALL,mapped+0x28d5c0,(void *)cleanup_stub));
    reset_case();
    if(!strcmp(mode,"passive")) {
        assert(native_proc(expected_window,WM_CLOSE,0,0)==0);closed(TRUE);
        close_dispatch=(SudekiMpControlUpdateDispatchWitness){.dispatch_serial=17,
            .service_post_original_exact=1,.native_thread_id=GetCurrentThreadId()};
        assert(!close_record.ticket && SudekiMpLanStoryAreaCloseUninstall(&close_dispatch));return FALSE;
    }
    if(!strcmp(mode,"properties")) {
        /* Real message-only Win32 window/property/post APIs, but a synthetic
         * procedure-identity check. Never attach retail WndProc to a real window
         * or permit native resource cleanup. Existing modes test retail dispatch. */
        static const wchar_t class_name[]=L"SudekiMP.AreaClose.ImageTest";
        HINSTANCE instance=GetModuleHandleW(NULL);
        WNDCLASSW wc={.lpfnWndProc=DefWindowProcW,.hInstance=instance,.lpszClassName=class_name};
        assert(RegisterClassW(&wc));
        HWND window=CreateWindowExW(0,class_name,L"",0,0,0,0,0,HWND_MESSAGE,NULL,instance,NULL);
        assert(window);expected_window=window;close_real_properties=TRUE;
        hold_window();assert(GetPropW(window,close_property)==(HANDLE)(uintptr_t)held_ticket);
        assert(SudekiMpLanStoryAreaCloseRelease((HMODULE)mapped,&coordinator,&close_dispatch,held_ticket));
        assert(!GetPropW(window,close_property));
        hold_window();assert(close_should_defer(window));
        assert(SudekiMpLanStoryAreaClosePostDrainedClose((HMODULE)mapped,&coordinator,&close_dispatch,held_ticket));
        assert(IsWindow(window) && GetPropW(window,close_property)==(HANDLE)(uintptr_t)held_ticket);
        MSG message={0};assert(PeekMessageW(&message,window,WM_CLOSE,WM_CLOSE,PM_REMOVE));
        assert(message.hwnd==window && message.message==WM_CLOSE && !message.wParam && !message.lParam);
        /* Simulate out-of-band window destruction, not admitted game cleanup. */
        assert(DestroyWindow(window) && !IsWindow(window) && !GetPropW(window,close_property));
        assert(close_should_defer(window) && close_unknown);
        assert(!SudekiMpLanStoryAreaClosePostDrainedClose((HMODULE)mapped,&coordinator,&close_dispatch,held_ticket));
        assert(!SudekiMpLanStoryAreaCloseUninstall(&close_dispatch));
        assert(UnregisterClassW(class_name,instance));return TRUE;
    }
    if(!strcmp(mode,"handoff")) {
        HANDLE thread=CreateThread(NULL,0,close_handoff,NULL,0,NULL);assert(thread);
        assert(WaitForSingleObject(thread,5000)==WAIT_OBJECT_0);CloseHandle(thread);return FALSE;
    }
    hold_window();uint64_t same=0;
    assert(SudekiMpLanStoryAreaCloseHold((HMODULE)mapped,&coordinator,&close_dispatch,expected_window,&same) && same==held_ticket);
    int foreign_consumer;uint64_t unchanged=887;
    assert(!SudekiMpLanStoryAreaCloseHold((HMODULE)mapped,&foreign_consumer,&close_dispatch,expected_window,&unchanged) && unchanged==887);
    assert(!SudekiMpLanStoryAreaCloseRelease((HMODULE)mapped,&coordinator,&close_dispatch,held_ticket+1));
    close_bad_window=TRUE;
    assert(!SudekiMpLanStoryAreaCloseRelease((HMODULE)mapped,&coordinator,&close_dispatch,held_ticket));close_bad_window=FALSE;
    close_bad_proc=TRUE;
    assert(!SudekiMpLanStoryAreaCloseRelease((HMODULE)mapped,&coordinator,&close_dispatch,held_ticket));close_bad_proc=FALSE;
    close_witness_ok=FALSE;
    assert(!SudekiMpLanStoryAreaCloseRelease((HMODULE)mapped,&coordinator,&close_dispatch,held_ticket));close_witness_ok=TRUE;
    assert(close_receipt(SUDEKIMP_AREA_CLOSE_HELD).requests==0);
    if(!strcmp(mode,"release")) {
        uint64_t old=held_ticket;
        assert(SudekiMpLanStoryAreaCloseRelease((HMODULE)mapped,&coordinator,&close_dispatch,held_ticket));
        hold_window();assert(held_ticket>old);
        assert(!SudekiMpLanStoryAreaCloseRelease((HMODULE)mapped,&coordinator,&close_dispatch,old));
        assert(!SudekiMpLanStoryAreaClosePostDrainedClose((HMODULE)mapped,&coordinator,&close_dispatch,held_ticket));
        assert(SudekiMpLanStoryAreaCloseRelease((HMODULE)mapped,&coordinator,&close_dispatch,held_ticket));
        assert(SudekiMpLanStoryAreaCloseUninstall(&close_dispatch));
        reset_case();assert(native_proc(expected_window,WM_CLOSE,0,0)==0);closed(TRUE);return FALSE;
    }
    if(!strcmp(mode,"abi")) {
        void *original=close_original,*tail=close_continuation;
        close_original=(void *)close_abi_original;close_continuation=(void *)close_abi_continuation;
        close_abi_check(TRUE);
        assert(SudekiMpLanStoryAreaClosePostDrainedClose((HMODULE)mapped,&coordinator,&close_dispatch,held_ticket));
        close_abi_check(FALSE);close_abi_check(TRUE);
        close_original=original;close_continuation=tail;
        assert(close_record.phase==SUDEKIMP_AREA_CLOSE_DISPATCHED && close_record.requests==3 && !close_test_property);return TRUE;
    }
    if(!strcmp(mode,"thread")) {
        HANDLE thread=CreateThread(NULL,0,foreign_close,NULL,0,NULL);assert(thread);
        assert(WaitForSingleObject(thread,5000)==WAIT_OBJECT_0);CloseHandle(thread);
    } else if(!strcmp(mode,"window")) {
        assert(native_proc((HWND)(uintptr_t)0x1234,WM_CLOSE,0,0)==0);
    } else if(!strcmp(mode,"overflow")) {
        close_record.requests=UINT_MAX;
        assert(native_proc(expected_window,WM_CLOSE,0,0)==0);
    } else if(!strcmp(mode,"reuse")) {
        /* Same HWND/thread/procedure, but a new window has no old property. */
        close_test_property=NULL;
        assert(!SudekiMpLanStoryAreaClosePostDrainedClose((HMODULE)mapped,&coordinator,&close_dispatch,held_ticket));
        assert(native_proc(expected_window,WM_CLOSE,0,0)==0);
    } else if(!strcmp(mode,"procedure")) {
        assert(native_proc(expected_window,WM_CLOSE,0,0)==0);
        assert(SudekiMpLanStoryAreaClosePostDrainedClose((HMODULE)mapped,&coordinator,&close_dispatch,held_ticket));
        close_bad_proc=TRUE;
        assert(native_proc(expected_window,WM_CLOSE,0,0)==0);
        assert(close_test_property==(HANDLE)(uintptr_t)held_ticket && close_record.phase==SUDEKIMP_AREA_CLOSE_POSTED);
    } else if(!strcmp(mode,"marker")) {
        close_test_property=(HANDLE)(uintptr_t)0x7777;
        assert(!SudekiMpLanStoryAreaCloseRelease((HMODULE)mapped,&coordinator,&close_dispatch,held_ticket));
        assert(!SudekiMpLanStoryAreaClosePostDrainedClose((HMODULE)mapped,&coordinator,&close_dispatch,held_ticket));
        assert(native_proc(expected_window,WM_CLOSE,0,0)==0);
        assert(close_test_property==(HANDLE)(uintptr_t)0x7777);
    } else if(!strcmp(mode,"remove")) {
        assert(native_proc(expected_window,WM_CLOSE,0,0)==0);
        assert(SudekiMpLanStoryAreaClosePostDrainedClose((HMODULE)mapped,&coordinator,&close_dispatch,held_ticket));
        close_remove_failure=TRUE;
        assert(native_proc(expected_window,WM_CLOSE,0,0)==0);
        assert(close_test_property==(HANDLE)(uintptr_t)held_ticket && close_record.phase==SUDEKIMP_AREA_CLOSE_POSTED);
    } else if(!strcmp(mode,"owner")) {
        uint8_t saved=mapped[CLOSE_SITE+6];mapped[CLOSE_SITE+6]^=1;
        /* Do not execute corrupted code; direct observer models a call
         * already inside the adapter when another patch owner changes bytes. */
        assert(close_should_defer(expected_window));mapped[CLOSE_SITE+6]=saved;
    } else {
        assert(!strcmp(mode,"normal"));
        for(unsigned i=0;i<3;++i) assert(native_proc(expected_window,WM_CLOSE,0,0)==0);
        run_pump();assert(close_receipt(SUDEKIMP_AREA_CLOSE_REQUESTED).requests==4);
        assert(!events && mapped[RUNNING]==1);
        assert(native_proc(expected_window,WM_APP+7,0x1357,0x2468)==0x1234 && default_count==1);
        assert(!SudekiMpLanStoryAreaCloseRelease((HMODULE)mapped,&coordinator,&close_dispatch,held_ticket));
        assert(!SudekiMpLanStoryAreaCloseUninstall(&close_dispatch) && close_base && close_original);
        close_post_fail=TRUE;close_post_nested=TRUE;
        assert(!SudekiMpLanStoryAreaClosePostDrainedClose((HMODULE)mapped,&coordinator,&close_dispatch,held_ticket));
        assert(GetLastError()==ERROR_NOT_ENOUGH_QUOTA && close_posts==1);
        assert(close_receipt(SUDEKIMP_AREA_CLOSE_REQUESTED).requests==5);
        close_post_fail=FALSE;
        assert(SudekiMpLanStoryAreaClosePostDrainedClose((HMODULE)mapped,&coordinator,&close_dispatch,held_ticket));
        assert(close_posts==2 && close_receipt(SUDEKIMP_AREA_CLOSE_POSTED).requests==6);
        assert(!SudekiMpLanStoryAreaClosePostDrainedClose((HMODULE)mapped,&coordinator,&close_dispatch,held_ticket));
        assert(!SudekiMpLanStoryAreaCloseHold((HMODULE)mapped,&coordinator,&close_dispatch,expected_window,&same));
        assert(!events && mapped[RUNNING]==1); /* Posting did not destroy owners synchronously. */
        nested_cleanup=TRUE;
        assert(native_proc(expected_window,WM_CLOSE,0,0)==0);closed(TRUE);
        assert(close_record.phase==SUDEKIMP_AREA_CLOSE_DISPATCHED && close_record.requests==8 && !close_test_property);
        assert(native_proc(expected_window,WM_CLOSE,0,0)==0);closed(TRUE);
        assert(close_record.phase==SUDEKIMP_AREA_CLOSE_DISPATCHED && close_record.requests==9);
        assert(!SudekiMpLanStoryAreaCloseRelease((HMODULE)mapped,&coordinator,&close_dispatch,held_ticket));
        assert(!SudekiMpLanStoryAreaCloseUninstall(&close_dispatch) && close_base && close_original);return TRUE;
    }
    assert(close_unknown && !events && mapped[RUNNING]==1);
    SudekiMpStoryAreaCloseReceipt receipt={.ticket=999},before=receipt;
    assert(!SudekiMpLanStoryAreaCloseRead((HMODULE)mapped,&coordinator,&close_dispatch,held_ticket,&receipt));
    assert(!memcmp(&before,&receipt,sizeof(receipt)));
    assert(!SudekiMpLanStoryAreaCloseRelease((HMODULE)mapped,&coordinator,&close_dispatch,held_ticket));
    assert(!SudekiMpLanStoryAreaClosePostDrainedClose((HMODULE)mapped,&coordinator,&close_dispatch,held_ticket));
    assert(!SudekiMpLanStoryAreaCloseUninstall(&close_dispatch) && close_base && close_original);return TRUE;
}
int main(int argc,char **argv) {
    assert(argc==2 || argc==3);wchar_t path[1024];
    assert(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024));
    mapped=map_image(path);native_proc=(NativeProc)(uintptr_t)(mapped+WINDOW_PROC);
    expected_window=(HWND)(uintptr_t)0x876543;
    production_lifecycle();
    uint8_t proc_before[PROC_SIZE],pump_before[PUMP_SIZE];
    memcpy(proc_before,mapped+WINDOW_PROC,sizeof(proc_before));
    memcpy(pump_before,mapped+PUMP,sizeof(pump_before));
    void *device_before=*(void **)(mapped+DEVICE);uint8_t running_before=mapped[RUNNING];
    *(void **)(mapped+DEVICE)=device;
    /* These are all imports on the selected branches of the two routines.
     * Other code remains non-executable; no Windows imports are resolved. */
    static const unsigned slots[]={0x29a20c,0x29a254,0x29a1f4,0x29a0ec,0x29a1f0,
        0x29a1fc,0x29a1f8,0x29a204,0x29a200};
    void *stubs[]={(void *)destroy_stub,(void *)quit_stub,(void *)default_stub,
        (void *)module_stub,(void *)accelerator_stub,(void *)peek_stub,
        (void *)translate_accelerator_stub,(void *)translate_stub,(void *)dispatch_stub};
    void *slot_before[9];SudekiMpPointerHook imports[9]={{0}};
    for(unsigned i=0;i<9;++i) {
        slot_before[i]=*(void **)(mapped+slots[i]);
        assert(SudekiMpInstallPointerHook(&imports[i],(void **)(mapped+slots[i]),slot_before[i],stubs[i]));
    }
    SudekiMpRelativeCallHook cleanup={0};
    assert(SudekiMpInstallRelativeCallHook(&cleanup,mapped+CLOSE_CALL,mapped+0x28d5c0,(void *)cleanup_stub));
    DWORD proc_protection,pump_protection,ignored;
    assert(VirtualProtect(mapped+WINDOW_PROC,PROC_SIZE,PAGE_EXECUTE_READWRITE,&proc_protection));
    assert(VirtualProtect(mapped+PUMP,PUMP_SIZE,PAGE_EXECUTE_READWRITE,&pump_protection));
    assert(FlushInstructionCache(GetCurrentProcess(),mapped+WINDOW_PROC,PROC_SIZE));
    assert(FlushInstructionCache(GetCurrentProcess(),mapped+PUMP,PUMP_SIZE));
    reset_case();assert(native_proc(expected_window,WM_CLOSE,0,0)==0);closed(TRUE);
    reset_case();run_pump();closed(TRUE);assert(cleanup_busy==1);
    for(unsigned active=0;active<=1;++active) {
        reset_case();device[0x11]=(uint8_t)active;peek_count=1;
        assert(((unsigned char (__cdecl *)(void))(mapped+PUMP))()==active);
        assert(peek_count==2 && !dispatch_count && !events && mapped[RUNNING]==1);
    }

    /* Reject a tempting but too-late design: skipping only the cleanup CALL
     * still stops the native loop and destroys the window. */
    assert(SudekiMpRestoreRelativeCallHook(&cleanup));
    assert(SudekiMpInstallRelativeCallHook(&cleanup,mapped+CLOSE_CALL,mapped+0x28d5c0,(void *)late_skip_stub));
    reset_case();run_pump();closed(FALSE);assert(late_skip_count==1);
    assert(SudekiMpRestoreRelativeCallHook(&cleanup));
    assert(SudekiMpInstallRelativeCallHook(&cleanup,mapped+CLOSE_CALL,mapped+0x28d5c0,(void *)cleanup_stub));

    SudekiMpInlineHook gate={0};
    uint8_t expected[7]={0xc6,0x05,0,0,0,0,0};
    uint32_t running=(uint32_t)(uintptr_t)(mapped+RUNNING);memcpy(expected+2,&running,4);
    close_return=mapped+CLOSE_RETURN;
    assert(SudekiMpInstallInlineHook(&gate,mapped+CLOSE_BEGIN,expected,sizeof(expected),(void *)early_gate));
    close_trampoline=gate.trampoline;
    reset_case();defer_close=1;
    for(unsigned i=0;i<3;++i) assert(native_proc(expected_window,WM_CLOSE,0,0)==0);
    assert(deferrals==3 && mapped[RUNNING]==1 && !events);
    assert(native_proc(expected_window,WM_APP+7,0x1357,0x2468)==0x1234 && default_count==1);
    assert(native_proc(expected_window,WM_ACTIVATEAPP,0,0)==0x1234 && !device[0x11]);
    assert(native_proc(expected_window,WM_ACTIVATEAPP,1,0)==0x1234 && device[0x11]);
    assert(default_count==3 && deferrals==3 && !events && mapped[RUNNING]==1);
    run_pump();assert(deferrals==4 && !events && mapped[RUNNING]==1);
    /* The fixture sends a fresh close after its synthetic obligation ends.
     * This does NOT implement replay, automatic drain, timeout or safe close. */
    defer_close=0;assert(native_proc(expected_window,WM_CLOSE,0,0)==0);
    closed(TRUE);assert(cleanup_busy==0);
    assert(SudekiMpRestoreInlineHook(&gate));close_trampoline=close_return=NULL;
    reset_case();assert(native_proc(expected_window,WM_CLOSE,0,0)==0);closed(TRUE);
    BOOL retained=argc==3?production_native(argv[2],&cleanup):FALSE;
    assert(SudekiMpRestoreRelativeCallHook(&cleanup));
    for(unsigned i=9;i>0;--i) assert(SudekiMpRestorePointerHook(&imports[i-1]));
    for(unsigned i=0;i<9;++i) assert(*(void **)(mapped+slots[i])==slot_before[i]);
    if(!retained) assert(!memcmp(proc_before,mapped+WINDOW_PROC,sizeof(proc_before)));
    else {
        assert(close_hook.installed && !memcmp(mapped+CLOSE_SITE,close_hook.replacement,close_hook.length));
        assert(!memcmp(proc_before,mapped+WINDOW_PROC,CLOSE_SITE-WINDOW_PROC));
        assert(!memcmp(proc_before+CLOSE_SITE-WINDOW_PROC+7,mapped+CLOSE_SITE+7,
            sizeof(proc_before)-(CLOSE_SITE-WINDOW_PROC+7)));
    }
    assert(!memcmp(pump_before,mapped+PUMP,sizeof(pump_before)));
    assert(VirtualProtect(mapped+WINDOW_PROC,PROC_SIZE,proc_protection,&ignored));
    assert(VirtualProtect(mapped+PUMP,PUMP_SIZE,pump_protection,&ignored));
    *(void **)(mapped+DEVICE)=device_before;mapped[RUNNING]=running_before;
    if(!retained) assert(VirtualFree(mapped,0,MEM_RELEASE));
    printf("StoryAreaCloseImageTest: PASS (%s; native WndProc/pump with synthetic cleanup; window_properties=%s; no live drain; retained=%u)\n",
        argc==3?argv[2]:"fixture",close_real_properties?"isolated_Win32":"synthetic",(unsigned)retained);
    return 0;
}
