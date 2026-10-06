#include "hooks/lan_story_anim_trace.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <string.h>
#if !defined(__GNUC__) || !defined(__i386__)
#error "Animation trace requires the supported x86 ABI"
#endif
/* cAnimObjectRenderer::Update entry on the supported image:
 *   D9 44 24 08  FLD float ptr [ESP+8]   (dt)
 *   53           PUSH EBX
 * Five whole instructions, no relative branch, ECX=renderer, [ESP+4]=ctx. */
enum { UPDATE_RVA=0x222b50, UPDATE_LENGTH=5, CALLERS=24, RENDERERS=192,
       WINDOW_MS=2000, SUMMARY_LIMIT=400 };
static const uint8_t update_bytes[UPDATE_LENGTH]={0xd9,0x44,0x24,0x08,0x53};
typedef struct Caller { unsigned rva; uint32_t calls,moving; float last_dt; } Caller;
typedef struct Seen { uintptr_t renderer; uint8_t moving; } Seen;
static SudekiMpInlineHook hook;
void *SudekiMpLanStoryAnimTraceTrampoline __attribute__((used));
static uint8_t *base;
static LONG lock,summaries,overflow_callers,overflow_renderers;
static DWORD window_start;
static Caller callers[CALLERS]; static unsigned caller_count;
static Seen seen[RENDERERS]; static unsigned seen_count;
static uintptr_t watched; static uint32_t watched_calls,watched_moving; static unsigned watched_callers[4];
static void reset_window(DWORD now) {
    caller_count=0; seen_count=0; window_start=now; overflow_callers=0; overflow_renderers=0;
    watched_calls=watched_moving=0; memset(watched_callers,0,sizeof(watched_callers));
}
static void summarise(DWORD now) {
    if(InterlockedIncrement(&summaries)>SUMMARY_LIMIT) {
        if(summaries==SUMMARY_LIMIT+1) SudekiMpLogWrite("anim_trace event=summary_limit policy=suppressed\r\n");
        reset_window(now); return;
    }
    unsigned moving=0,still=0;
    for(unsigned i=0;i<seen_count;++i) { if(seen[i].moving) ++moving; else ++still; }
    SudekiMpLogFormat("anim_trace event=window ms=%lu callers=%u renderers_moving=%u renderers_still_only=%u overflow_callers=%ld overflow_renderers=%ld\r\n",
        (unsigned long)(now-window_start),caller_count,moving,still,(long)overflow_callers,(long)overflow_renderers);
    if(watched) SudekiMpLogFormat("anim_trace event=watched renderer=%p calls=%lu moving=%lu callers=0x%06x,0x%06x,0x%06x,0x%06x\r\n",
        (void *)watched,(unsigned long)watched_calls,(unsigned long)watched_moving,
        watched_callers[0],watched_callers[1],watched_callers[2],watched_callers[3]);
    for(unsigned i=0;i<caller_count;++i)
        SudekiMpLogFormat("anim_trace event=caller rva=0x%06x calls=%lu moving=%lu last_dt=%.5f\r\n",
            callers[i].rva,(unsigned long)callers[i].calls,(unsigned long)callers[i].moving,(double)callers[i].last_dt);
    reset_window(now);
}
/* Runs on the native caller's thread with a clean FP environment. Copies
 * values only; never calls native code or changes state. */
static void __attribute__((used,noinline)) observe(uintptr_t caller,uintptr_t ctx,uint32_t dt_bits,uintptr_t renderer) {
    DWORD saved=GetLastError(); (void)ctx;
    if(InterlockedCompareExchange(&lock,1,0)!=0) {SetLastError(saved); return;}
    float dt; memcpy(&dt,&dt_bits,sizeof(dt));
    BOOL moving=dt!=0.0f;
    unsigned rva=(caller>=(uintptr_t)base && caller<(uintptr_t)base+SUDEKIMP_EXPECTED_IMAGE_SIZE)?
        (unsigned)(caller-(uintptr_t)base):0;
    DWORD now=GetTickCount();
    if(!window_start) window_start=now;
    unsigned i;
    for(i=0;i<caller_count && callers[i].rva!=rva;++i);
    if(i==caller_count) {
        if(caller_count<CALLERS) { callers[caller_count].rva=rva; callers[caller_count].calls=0;
            callers[caller_count].moving=0; callers[caller_count].last_dt=0; ++caller_count; }
        else { ++overflow_callers; i=CALLERS; }
    }
    if(i<CALLERS) { ++callers[i].calls; if(moving) { ++callers[i].moving; callers[i].last_dt=dt; } }
    for(i=0;i<seen_count && seen[i].renderer!=renderer;++i);
    if(i==seen_count) {
        if(seen_count<RENDERERS) { seen[seen_count].renderer=renderer; seen[seen_count].moving=0; ++seen_count; }
        else { ++overflow_renderers; i=RENDERERS; }
    }
    if(i<RENDERERS && moving) seen[i].moving=1;
    if(watched && renderer==watched) {
        ++watched_calls; if(moving) ++watched_moving;
        for(unsigned k=0;k<4u;++k) { if(watched_callers[k]==rva) break; if(!watched_callers[k]) { watched_callers[k]=rva; break; } }
    }
    if(now-window_start>=WINDOW_MS) summarise(now);
    InterlockedExchange(&lock,0);
    SetLastError(saved);
}
/* Entry stub: preserves every register and the FP/SSE state. Stack at entry
 * (after pushfl/pushal, EBP=ESP): [EBP+24]=ECX, [EBP+36]=return address,
 * [EBP+40]=ctx, [EBP+44]=dt bits. */
static void __attribute__((naked,noinline)) update_stub(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;"
        "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld; sub $16,%esp;"
        "mov 36(%ebp),%eax; mov %eax,(%esp); mov 40(%ebp),%eax; mov %eax,4(%esp);"
        "mov 44(%ebp),%eax; mov %eax,8(%esp); mov 24(%ebp),%eax; mov %eax,12(%esp); call _observe;"
        "add $16,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl;"
        "jmp *_SudekiMpLanStoryAnimTraceTrampoline");
}
static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || VirtualQuery(p,&m,sizeof(m))!=sizeof(m) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
        a+n>(uintptr_t)m.BaseAddress+m.RegionSize) return FALSE;
    DWORD access=m.Protect&0xffu;
    return access==PAGE_EXECUTE_READ || access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
}
void SudekiMpLanStoryAnimTraceWatch(const void *renderer) { watched=(uintptr_t)renderer; }
BOOL SudekiMpLanStoryAnimTraceUninstall(void) {
    if(!SudekiMpRestoreInlineHook(&hook)) return FALSE;
    /* The trampoline stays referenced: a thread may still be inside it. */
    base=NULL; return TRUE;
}
BOOL SudekiMpLanStoryAnimTraceInstall(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    if(base || !b || !SudekiMpCheckLoadedExecutable(image)) {SetLastError(ERROR_INVALID_STATE); return FALSE;}
    if(!readable(b+UPDATE_RVA,UPDATE_LENGTH) || *(void **)(b+0x2df8ecu+8u)!=b+UPDATE_RVA ||
        !SudekiMpInstallInlineHook(&hook,b+UPDATE_RVA,update_bytes,UPDATE_LENGTH,(const void *)(uintptr_t)update_stub)) {
        DWORD error=GetLastError(); SetLastError(error?error:ERROR_INVALID_DATA); return FALSE;
    }
    SudekiMpLanStoryAnimTraceTrampoline=hook.trampoline; base=b;
    SudekiMpLogWrite("anim_trace event=installed seam=anim_renderer_update rva=0x222b50 policy=log_only\r\n");
    return TRUE;
}
