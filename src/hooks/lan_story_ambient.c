#include "hooks/lan_story_ambient.h"
#include "hooks/lan_story_world.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <string.h>
#if !defined(__GNUC__) || !defined(__i386__)
#error "Ambient animation requires the supported x86 ABI"
#endif
/* cAnimObjectRenderer::Update entry on the supported image:
 *   D9 44 24 08  FLD float ptr [ESP+8]   (dt, seconds)
 *   53           PUSH EBX
 * ECX=renderer, [ESP+4]=ctx, [ESP+8]=dt. Vtable 0x6DF8EC slot 2. */
/* Zone water clock on the supported image (the zone renderer's one
 * sub-object, class vtable 0x6DF7A4 slot 2 = 0x6171B0, Update(ctx, dt)):
 *   D9 44 24 08  FLD float ptr [ESP+8]   (dt)
 *   B0 01        MOV AL,1
 * then this+0x1C += dt; this+0x20 += dt * this+0x124; RET 8. Six bytes of
 * straight-line code, no relative branch. Same zero-delta substitution. */
enum { WATER_RVA=0x2171b0, WATER_LENGTH=6, WATER_VTABLE_RVA=0x2df7a4 };
static const uint8_t water_bytes[WATER_LENGTH]={0xd9,0x44,0x24,0x08,0xb0,0x01};
static SudekiMpInlineHook water_hook;
void *SudekiMpLanStoryAmbientWaterTrampoline __attribute__((used));
enum { UPDATE_RVA=0x222b50, UPDATE_LENGTH=5, VTABLE_RVA=0x2df8ec, FRAME_STAMP=0x3c3150,
       MAX_OWNED=128, LEASE_MS=500, MAX_DT_MS=50, GAP_MS=250, REFRESH_MS=250 };
static const uint8_t update_bytes[UPDATE_LENGTH]={0xd9,0x44,0x24,0x08,0x53};
static SudekiMpInlineHook hook;
void *SudekiMpLanStoryAmbientTrampoline __attribute__((used));
static uint8_t *base;
static LONG lock;
static volatile LONG active; static volatile DWORD active_tick;
static uint16_t frame_seen; static BOOL frame_valid;
static LARGE_INTEGER frequency,last_frame;
static uint32_t frame_dt_bits;
static void *owned[MAX_OWNED]; static unsigned owned_count; static DWORD owned_tick;
static uint32_t substituted,excluded,passthrough,expired; static DWORD logged; static unsigned logs;
static BOOL readable_code(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || VirtualQuery(p,&m,sizeof(m))!=sizeof(m) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
        a+n>(uintptr_t)m.BaseAddress+m.RegionSize) return FALSE;
    DWORD access=m.Protect&0xffu;
    return access==PAGE_EXECUTE_READ || access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
}
void SudekiMpLanStoryAmbientSetActive(BOOL on) {
    if(on) { active_tick=GetTickCount(); InterlockedExchange(&active,1); }
    else InterlockedExchange(&active,0);
}
/* One local delta per native frame (frame stamp 0x7C3150 changes once per
 * dispatcher pass): wall-clock seconds since the previous frame, capped at
 * 50 ms; a gap above 250 ms yields zero so a stall never replays as a burst. */
static void refresh_frame(DWORD now) {
    uint16_t stamp=*(const uint16_t *)(base+FRAME_STAMP);
    if(frame_valid && stamp==frame_seen) return;
    LARGE_INTEGER t; QueryPerformanceCounter(&t);
    float dt=0.0f;
    if(frame_valid && last_frame.QuadPart && frequency.QuadPart>0 && t.QuadPart>last_frame.QuadPart) {
        uint64_t us=(uint64_t)(t.QuadPart-last_frame.QuadPart)*UINT64_C(1000000)/(uint64_t)frequency.QuadPart;
        if(us<=(uint64_t)GAP_MS*1000u) { if(us>(uint64_t)MAX_DT_MS*1000u) us=(uint64_t)MAX_DT_MS*1000u; dt=(float)us*0.000001f; }
    }
    last_frame=t; frame_seen=stamp; frame_valid=TRUE;
    memcpy(&frame_dt_bits,&dt,sizeof(dt));
    if(!owned_tick || now-owned_tick>=REFRESH_MS) {
        owned_count=SudekiMpLanStoryWorldOwnedRenderers(owned,MAX_OWNED);
        owned_tick=now?now:1u;
    }
}
/* Runs at the native entry on the caller's thread with a clean FP state.
 * Only the incoming dt argument slot may be written, and only from zero. */
static void __attribute__((used,noinline)) observe(uintptr_t renderer,uint32_t *dt_slot) {
    DWORD saved=GetLastError();
    if(InterlockedCompareExchange(&lock,1,0)!=0) {SetLastError(saved); return;}
    DWORD now=GetTickCount();
    if(!InterlockedCompareExchange(&active,0,0) || now-active_tick>LEASE_MS) { ++expired; goto done; }
    if(*dt_slot!=0u) { ++passthrough; goto done; }
    refresh_frame(now);
    for(unsigned i=0;i<owned_count;++i) if(owned[i]==(void *)renderer) { ++excluded; goto done; }
    if(frame_dt_bits) { *dt_slot=frame_dt_bits; ++substituted; }
done:
    if(SudekiMpLogResearchEnabled() && now-logged>=2000u && logs<300u) {
        logged=now; ++logs; float dt_log; memcpy(&dt_log,&frame_dt_bits,sizeof(dt_log));
        SudekiMpLogFormat("lan_story_ambient event=window substituted=%lu excluded=%lu passthrough=%lu expired=%lu owned=%u dt=%.4f\r\n",
            (unsigned long)substituted,(unsigned long)excluded,(unsigned long)passthrough,(unsigned long)expired,owned_count,
            (double)dt_log);
        substituted=excluded=passthrough=expired=0;
    }
    InterlockedExchange(&lock,0);
    SetLastError(saved);
}
/* Entry stub: preserves every register and the FP/SSE state. After
 * pushfl/pushal with EBP=ESP: [EBP+24]=ECX, [EBP+36]=return address,
 * [EBP+40]=ctx, [EBP+44]=dt bits (the slot observe() may rewrite). */
static void __attribute__((naked,noinline)) update_stub(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;"
        "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld; sub $16,%esp;"
        "mov 24(%ebp),%eax; mov %eax,(%esp); lea 44(%ebp),%eax; mov %eax,4(%esp); call _observe;"
        "add $16,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl;"
        "jmp *_SudekiMpLanStoryAmbientTrampoline");
}
static void __attribute__((naked,noinline)) water_stub(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;"
        "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld; sub $16,%esp;"
        "mov 24(%ebp),%eax; mov %eax,(%esp); lea 44(%ebp),%eax; mov %eax,4(%esp); call _observe;"
        "add $16,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl;"
        "jmp *_SudekiMpLanStoryAmbientWaterTrampoline");
}
BOOL SudekiMpLanStoryAmbientUninstall(void) {
    if(!base) return TRUE;
    InterlockedExchange(&active,0);
    if(water_hook.installed && !SudekiMpRestoreInlineHook(&water_hook)) return FALSE;
    if(!SudekiMpRestoreInlineHook(&hook)) return FALSE;
    /* The trampoline stays referenced: a thread may still be inside it. */
    base=NULL; return TRUE;
}
BOOL SudekiMpLanStoryAmbientInstall(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    if(base || !b || !SudekiMpCheckLoadedExecutable(image)) {SetLastError(ERROR_INVALID_STATE); return FALSE;}
    QueryPerformanceFrequency(&frequency);
    if(!readable_code(b+UPDATE_RVA,UPDATE_LENGTH) || memcmp(b+UPDATE_RVA,update_bytes,UPDATE_LENGTH) ||
        *(void **)(b+VTABLE_RVA+8u)!=b+UPDATE_RVA ||
        !SudekiMpInstallInlineHook(&hook,b+UPDATE_RVA,update_bytes,UPDATE_LENGTH,(const void *)(uintptr_t)update_stub)) {
        DWORD error=GetLastError(); SetLastError(error?error:ERROR_INVALID_DATA); return FALSE;
    }
    SudekiMpLanStoryAmbientTrampoline=hook.trampoline; base=b;
    if(!readable_code(b+WATER_RVA,WATER_LENGTH) || memcmp(b+WATER_RVA,water_bytes,WATER_LENGTH) ||
        *(void **)(b+WATER_VTABLE_RVA+8u)!=b+WATER_RVA ||
        !SudekiMpInstallInlineHook(&water_hook,b+WATER_RVA,water_bytes,WATER_LENGTH,(const void *)(uintptr_t)water_stub)) {
        DWORD error=GetLastError(); (void)SudekiMpRestoreInlineHook(&hook); base=NULL;
        SetLastError(error?error:ERROR_INVALID_DATA); return FALSE;
    }
    SudekiMpLanStoryAmbientWaterTrampoline=water_hook.trampoline;
    SudekiMpLogWrite("lan_story_ambient event=installed seams=anim_renderer_update:0x222b50,zone_water_clock:0x2171b0 policy=client_zero_delta_only\r\n");
    return TRUE;
}
