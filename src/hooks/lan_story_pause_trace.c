#include "hooks/lan_story_pause_trace.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <string.h>
#if !defined(__GNUC__) || !defined(__i386__)
#error "Pause trace requires the supported x86 ABI"
#endif
enum { HOOKS=10, LINE_LIMIT=3000, PREFERRED=0x400000 };
typedef struct Seam {
    const char *name;
    unsigned rva,length,reloc; /* reloc: byte offset of an absolute imm32, or 0 */
    uint8_t bytes[8];
    unsigned kind; /* 0 group name arg, 1 this only, 2 two bool args, 3 this+two bools, 4 this+text, 5 EAX spawn */
} Seam;
static const Seam seams[HOOKS]={
    {"PauseSpawnGroup",0x75630,5,0,{0x56,0x8b,0x74,0x24,0x08},0},
    {"UnpauseSpawnGroup",0x757e0,5,0,{0x56,0x8b,0x74,0x24,0x08},0},
    {"GroupPushPaused",0x1340,7,3,{0x6a,0xff,0x68,0x58,0x47,0x69,0x00},1},
    {"GroupPopPaused",0x1420,7,3,{0x6a,0xff,0x68,0x58,0x47,0x69,0x00},1},
    {"PauseEverythingWorker",0xfd610,6,0,{0x55,0x8b,0xec,0x83,0xe4,0xf8},2},
    {"SetGamePaused",0x272f0,6,0,{0x55,0x8b,0xec,0x83,0xe4,0xf8},3},
    {"ActivateClusterPair",0x358c0,6,0,{0x55,0x8b,0xec,0x83,0xe4,0xf8},4},
    {"ActivateCluster",0x35c70,6,0,{0x55,0x8b,0xec,0x83,0xe4,0xf8},4},
    /* Native NPC cluster activity: EAX=spawn row, entity at spawn+0x78-0x2C. */
    {"NpcActivityPause",0x13d540,7,0,{0xf6,0x80,0x82,0,0,0,0x02},5},
    {"NpcActivityResume",0x13d5b0,7,0,{0xf6,0x80,0x82,0,0,0,0x02},5}
};
static SudekiMpInlineHook hooks[HOOKS];
void *SudekiMpLanStoryPauseTraceTrampolines[HOOKS] __attribute__((used));
static uint8_t *base;
static LONG lines,suppressed;
static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || VirtualQuery(p,&m,sizeof(m))!=sizeof(m) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
        a+n>(uintptr_t)m.BaseAddress+m.RegionSize) return FALSE;
    DWORD access=m.Protect&0xffu;
    return access==PAGE_READONLY || access==PAGE_READWRITE || access==PAGE_WRITECOPY ||
        access==PAGE_EXECUTE_READ || access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
}
static void text(char out[48],const char *p) {
    memset(out,0,48);
    if(!p) {strcpy(out,"-");return;}
    for(unsigned i=0;i<47;++i) {
        if(!readable(p+i,1) || !p[i]) return;
        unsigned char c=(unsigned char)p[i]; out[i]=(char)((c>0x20&&c<0x7f)?c:'?');
    }
}
/* Runs on the native caller's thread with a clean FP environment. Copies
 * values only; never calls native code or changes state. */
static void __attribute__((used,noinline)) observe(unsigned id,uintptr_t caller,uintptr_t a1,
    uintptr_t a2,uintptr_t self,uintptr_t eax) {
    DWORD saved=GetLastError();
    if(InterlockedIncrement(&lines)>LINE_LIMIT) {
        if(InterlockedIncrement(&suppressed)==1) SudekiMpLogWrite("pause_trace event=line_limit policy=suppressed\r\n");
        SetLastError(saved); return;
    }
    const Seam *s=&seams[id]; char name[48]; uint32_t rn[3]={0};
    unsigned caller_rva=(caller>=(uintptr_t)base && caller<(uintptr_t)base+SUDEKIMP_EXPECTED_IMAGE_SIZE)?
        (unsigned)(caller-(uintptr_t)base):0;
    switch(s->kind) {
    case 0:
        if(readable((const void *)a1,12)) memcpy(rn,(const void *)a1,12);
        SudekiMpLogFormat("pause_trace event=%s tid=%lu caller_rva=0x%06x group_flags=%08lx group_id=%08lx\r\n",
            s->name,(unsigned long)GetCurrentThreadId(),caller_rva,(unsigned long)rn[0],(unsigned long)rn[1]);
        break;
    case 1:
        SudekiMpLogFormat("pause_trace event=%s tid=%lu caller_rva=0x%06x group=%08lx\r\n",
            s->name,(unsigned long)GetCurrentThreadId(),caller_rva,(unsigned long)self);
        break;
    case 2: case 3:
        SudekiMpLogFormat("pause_trace event=%s tid=%lu caller_rva=0x%06x arg1=%u arg2=%u\r\n",
            s->name,(unsigned long)GetCurrentThreadId(),caller_rva,(unsigned)(a1&0xff),(unsigned)(a2&0xff));
        break;
    case 5: {
        const uint8_t *sp=(const uint8_t *)eax; uint32_t identity=0,cluster=0,flags=0; int refs=-1;
        if(readable(sp,0x84)) {
            identity=*(const uint32_t *)(sp+0x70); cluster=*(const uint32_t *)(sp+0x60); flags=sp[0x82];
            uintptr_t r=*(const uintptr_t *)(sp+0x78);
            if(r>0x2c && readable((const void *)(r-0x2c+0x2b),1)) refs=*(const uint8_t *)(r-0x2c+0x2b);
        }
        SudekiMpLogFormat("pause_trace event=%s tid=%lu caller_rva=0x%06x npc=%08lx cluster=%08lx spawn_flags=%02lx refs_before=%d\r\n",
            s->name,(unsigned long)GetCurrentThreadId(),caller_rva,(unsigned long)identity,(unsigned long)cluster,
            (unsigned long)flags,refs);
        break;
    }
    default:
        text(name,(const char *)a1);
        SudekiMpLogFormat("pause_trace event=%s tid=%lu caller_rva=0x%06x cluster=%s\r\n",
            s->name,(unsigned long)GetCurrentThreadId(),caller_rva,name);
    }
    SetLastError(saved);
}
#define PAUSE_STUB(N) \
static void __attribute__((naked,noinline)) stub##N(void) { \
    __asm__ volatile("pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;" \
        "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld; sub $32,%esp;" \
        "movl $" #N ",(%esp); mov 36(%ebp),%eax; mov %eax,4(%esp); mov 40(%ebp),%eax; mov %eax,8(%esp);" \
        "mov 44(%ebp),%eax; mov %eax,12(%esp); mov 24(%ebp),%eax; mov %eax,16(%esp);" \
        "mov 28(%ebp),%eax; mov %eax,20(%esp); call _observe;" \
        "add $32,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl;" \
        "jmp *_SudekiMpLanStoryPauseTraceTrampolines+" #N "*4"); \
}
PAUSE_STUB(0) PAUSE_STUB(1) PAUSE_STUB(2) PAUSE_STUB(3)
PAUSE_STUB(4) PAUSE_STUB(5) PAUSE_STUB(6) PAUSE_STUB(7)
PAUSE_STUB(8) PAUSE_STUB(9)
static void (*const stubs[HOOKS])(void)={stub0,stub1,stub2,stub3,stub4,stub5,stub6,stub7,stub8,stub9};
BOOL SudekiMpLanStoryPauseTraceUninstall(void) {
    DWORD error=ERROR_SUCCESS;
    for(unsigned i=HOOKS;i>0;--i)
        if(!SudekiMpRestoreInlineHook(&hooks[i-1]) && !error) error=GetLastError()?GetLastError():ERROR_BUSY;
    if(error) {SetLastError(error); return FALSE;}
    /* Trampolines stay referenced: a thread may still be inside one. */
    base=NULL; return TRUE;
}
BOOL SudekiMpLanStoryPauseTraceInstall(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    if(base || !b || !SudekiMpCheckLoadedExecutable(image)) {SetLastError(ERROR_INVALID_STATE); return FALSE;}
    base=b;
    for(unsigned i=0;i<HOOKS;++i) {
        const Seam *s=&seams[i]; uint8_t expected[8];
        memcpy(expected,s->bytes,s->length);
        if(s->reloc) {
            uint32_t v; memcpy(&v,expected+s->reloc,4); v+=(uint32_t)((uintptr_t)b-PREFERRED);
            memcpy(expected+s->reloc,&v,4);
        }
        if(!readable(b+s->rva,s->length) ||
            !SudekiMpInstallInlineHook(&hooks[i],b+s->rva,expected,s->length,(const void *)(uintptr_t)stubs[i])) {
            DWORD error=GetLastError();
            (void)SudekiMpLanStoryPauseTraceUninstall();
            SetLastError(error?error:ERROR_INVALID_DATA); return FALSE;
        }
        SudekiMpLanStoryPauseTraceTrampolines[i]=hooks[i].trampoline;
    }
    return TRUE;
}
