#include "hooks/lan_story_arbiter_trace.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <stdint.h>
#include <string.h>
#if !defined(__GNUC__) || !defined(__i386__)
#error "Arbiter trace requires the supported x86 ABI"
#endif
/* Exact entry sequences on the supported image (Ghidra span dumps, 2026-10-06):
 *   4DBC90: 53             PUSH EBX
 *           8B 5C 24 08    MOV EBX,[ESP+8]        EAX=arbiter, [ESP+4]=ANIMID
 *   4D0730: 83 EC 0C       SUB ESP,0xC
 *           56             PUSH ESI
 *           8B F0          MOV ESI,EAX            EAX=combo manager, [ESP+4]=kind, [ESP+8]=flag
 * Whole instructions, no relative branches. */
/*   4D0F30: 8B 54 24 04    MOV EDX,[ESP+4]        CComboManager::PlayCombo: ECX=manager, [ESP+4]=index
 *           8B C1          MOV EAX,ECX
 *   4DAE80: 53             PUSH EBX
 *           8B 5C 24 08    MOV EBX,[ESP+8]        arbiter movement request: [ESP+8]=arbiter */
enum { EVENT_RVA=0xdbc90, EVENT_LENGTH=5, COMBO_RVA=0xd0730, COMBO_LENGTH=6,
       PLAY_RVA=0xd0f30, PLAY_LENGTH=6, MOVE_RVA=0xdae80, MOVE_LENGTH=5, LINE_LIMIT=60000 };
static const uint8_t play_bytes[PLAY_LENGTH]={0x8b,0x54,0x24,0x04,0x8b,0xc1};
static const uint8_t move_bytes[MOVE_LENGTH]={0x53,0x8b,0x5c,0x24,0x08};
static SudekiMpInlineHook play_hook,move_hook;
void *SudekiMpLanStoryArbiterTracePlayTrampoline __attribute__((used));
void *SudekiMpLanStoryArbiterTraceMoveTrampoline __attribute__((used));
static char name_filter[32];
static const uint8_t event_bytes[EVENT_LENGTH]={0x53,0x8b,0x5c,0x24,0x08};
static const uint8_t combo_bytes[COMBO_LENGTH]={0x83,0xec,0x0c,0x56,0x8b,0xf0};
static SudekiMpInlineHook event_hook,combo_hook;
void *SudekiMpLanStoryArbiterTraceEventTrampoline __attribute__((used));
void *SudekiMpLanStoryArbiterTraceComboTrampoline __attribute__((used));
static uint8_t *base; static LONG lock,lines;

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || VirtualQuery(p,&m,sizeof(m))!=sizeof(m) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
        a+n>(uintptr_t)m.BaseAddress+m.RegionSize) return FALSE;
    return TRUE;
}
static unsigned rva_of(uintptr_t p) {
    return (p>=(uintptr_t)base && p<(uintptr_t)base+SUDEKIMP_EXPECTED_IMAGE_SIZE)?(unsigned)(p-(uintptr_t)base):0u;
}
/* Shared entity base layout: +38 ResourceName {hash, char*}. Copies bytes only. */
static void entity_name(const uint8_t *entity,char out[40]) {
    out[0]='?'; out[1]=0;
    if(!readable(entity,0x3cu)) return;
    { /* Unknown class: report its vtable RVA instead of a bare '?'. */
      unsigned vt=rva_of(*(const uintptr_t *)entity);
      const uint32_t *r0=*(const uint32_t *const *)(entity+0x38u);
      if(!readable(r0,8u) || !readable((const void *)(uintptr_t)r0[1],1u)) { wsprintfA(out,"?vt%06x",vt); return; } }
    const uint32_t *r=*(const uint32_t *const *)(entity+0x38u);
    if(!readable(r,8u)) return;
    const char *s=(const char *)(uintptr_t)r[1]; unsigned k=0;
    while(k<39u && readable(s+k,1u) && s[k]>=0x20 && s[k]<0x7f) { out[k]=s[k]; ++k; }
    out[k]=0;
}
static LONG seen_event,seen_combo,seen_play,seen_move,passed; static DWORD heartbeat_at;
/* Heartbeat (every 10 s): proves the seams fire even when the filter passes nothing. */
static void heartbeat(void) {
    DWORD now=GetTickCount();
    if(now-heartbeat_at<10000u) return;
    heartbeat_at=now;
    SudekiMpLogFormat("arbiter_trace event=heartbeat anim_events=%ld combo_dispatch=%ld play_combo=%ld move_requests=%ld passed=%ld\r\n",
        (long)seen_event,(long)seen_combo,(long)seen_play,(long)seen_move,(long)passed);
}
static BOOL name_passes(const char *name) {
    heartbeat();
    if(!name_filter[0]) return TRUE;
    /* An unresolved name (class without the shared ResourceName slot) passes:
     * it may be the boss we are looking for. */
    if(name[0]=='?') return TRUE;
    size_t n=strlen(name_filter);
    for(const char *p=name;*p;++p) {
        size_t k=0;
        while(k<n && p[k] && ((p[k]|0x20)==(name_filter[k]|0x20))) ++k;
        if(k==n) return TRUE;
    }
    return FALSE;
}
static BOOL take_line(void) {
    InterlockedIncrement(&passed);
    LONG n=InterlockedIncrement(&lines);
    if(n==LINE_LIMIT+1) SudekiMpLogWrite("arbiter_trace event=line_limit policy=suppressed\r\n");
    return n<=LINE_LIMIT;
}
static void __attribute__((used,noinline)) observe_event(uintptr_t arbiter,uint32_t animid,uintptr_t ret) {
    DWORD saved=GetLastError(); InterlockedIncrement(&seen_event);
    if(InterlockedCompareExchange(&lock,1,0)==0) {
        const uint8_t *a=(const uint8_t *)arbiter; char name[40]="?";
        uint32_t f50=0,f58=0,f60=0; const uint8_t *entity=NULL;
        if(readable(a,0x64u)) { f50=*(const uint32_t *)(a+0x50u); f58=*(const uint32_t *)(a+0x58u);
            f60=*(const uint32_t *)(a+0x60u); entity=*(const uint8_t *const *)(a+0x10u); entity_name(entity,name); }
        if(name_passes(name) && take_line()) {
            SudekiMpLogFormat("arbiter_trace event=anim_event ms=%lu arbiter=%p entity=%p name=%s animid=%lu flags50=%08lx state58=%08lx flags60=%08lx caller=0x%06x\r\n",
                (unsigned long)GetTickCount(),(void *)arbiter,(const void *)entity,name,(unsigned long)animid,
                (unsigned long)f50,(unsigned long)f58,(unsigned long)f60,rva_of(ret));
        }
        InterlockedExchange(&lock,0);
    }
    SetLastError(saved);
}
static void __attribute__((used,noinline)) observe_combo(uintptr_t manager,uint32_t kind,uint32_t flag,uintptr_t ret) {
    DWORD saved=GetLastError(); InterlockedIncrement(&seen_combo);
    if(InterlockedCompareExchange(&lock,1,0)==0) {
        const uint8_t *m=(const uint8_t *)manager; char name[40]="?";
        const uint8_t *entity=NULL,*arbiter=NULL,*movement=NULL; uint32_t f50=0,f58=0,state5c=0,m_ac=0;
        if(readable(m,0xb4u)) { entity=*(const uint8_t *const *)(m+0x10u); m_ac=*(const uint32_t *)(m+0xacu); entity_name(entity,name); }
        if(name_passes(name) && take_line()) {
            if(readable(entity,0xa8u)) { arbiter=*(const uint8_t *const *)(entity+0x90u); movement=*(const uint8_t *const *)(entity+0xa4u); }
            if(readable(arbiter,0x5cu)) { f50=*(const uint32_t *)(arbiter+0x50u); f58=*(const uint32_t *)(arbiter+0x58u); }
            if(readable(movement,0x60u)) state5c=*(const uint32_t *)(movement+0x5cu);
            SudekiMpLogFormat("arbiter_trace event=combo_dispatch ms=%lu manager=%p entity=%p name=%s kind=%lu flag=%lu current_ac=%08lx movement5c=%08lx flags50=%08lx state58=%08lx caller=0x%06x\r\n",
                (unsigned long)GetTickCount(),(void *)manager,(const void *)entity,name,(unsigned long)kind,(unsigned long)(flag&0xffu),
                (unsigned long)m_ac,(unsigned long)state5c,(unsigned long)f50,(unsigned long)f58,rva_of(ret));
        }
        InterlockedExchange(&lock,0);
    }
    SetLastError(saved);
}
static void __attribute__((used,noinline)) observe_play(uintptr_t manager,uint32_t index,uintptr_t ret) {
    DWORD saved=GetLastError(); InterlockedIncrement(&seen_play);
    if(InterlockedCompareExchange(&lock,1,0)==0) {
        const uint8_t *m=(const uint8_t *)manager; char name[40]="?"; const uint8_t *entity=NULL;
        uint32_t count=0,slot=0,m_ac=0;
        if(readable(m,0xb4u)) { entity=*(const uint8_t *const *)(m+0x10u); count=*(const uint32_t *)(m+0x58u); m_ac=*(const uint32_t *)(m+0xacu); entity_name(entity,name);
            const uint32_t *table=*(const uint32_t *const *)(m+0x60u);
            if(index<count && readable(table,(index+1u)*4u)) slot=table[index]; }
        if(name_passes(name) && take_line())
            SudekiMpLogFormat("arbiter_trace event=play_combo ms=%lu manager=%p entity=%p name=%s index=%lu count=%lu combo=%08lx current_ac=%08lx caller=0x%06x\r\n",
                (unsigned long)GetTickCount(),(void *)manager,(const void *)entity,name,(unsigned long)index,(unsigned long)count,
                (unsigned long)slot,(unsigned long)m_ac,rva_of(ret));
        InterlockedExchange(&lock,0);
    }
    SetLastError(saved);
}
static struct { uintptr_t arbiter; uint32_t a1,a3,a4; } move_last[8]; static unsigned move_next;
static void __attribute__((used,noinline)) observe_move(uint32_t a1,uintptr_t arbiter,uint32_t a3,uint32_t a4,uintptr_t ret) {
    DWORD saved=GetLastError(); InterlockedIncrement(&seen_move);
    if(InterlockedCompareExchange(&lock,1,0)==0) {
        unsigned i; for(i=0;i<8u && !(move_last[i].arbiter==arbiter && move_last[i].a1==a1 && move_last[i].a3==a3 && move_last[i].a4==a4);++i);
        if(i==8u) {
            move_last[move_next%8u].arbiter=arbiter; move_last[move_next%8u].a1=a1; move_last[move_next%8u].a3=a3; move_last[move_next%8u].a4=a4; ++move_next;
            const uint8_t *a=(const uint8_t *)arbiter; char name[40]="?"; const uint8_t *entity=NULL; uint32_t f50=0,f58=0;
            if(readable(a,0x64u)) { f50=*(const uint32_t *)(a+0x50u); f58=*(const uint32_t *)(a+0x58u); entity=*(const uint8_t *const *)(a+0x10u); entity_name(entity,name); }
            float f1,f3,f4; memcpy(&f1,&a1,4); memcpy(&f3,&a3,4); memcpy(&f4,&a4,4);
            if(name_passes(name) && take_line())
                SudekiMpLogFormat("arbiter_trace event=move_request ms=%lu arbiter=%p entity=%p name=%s a1=%08lx(%.3f) a3=%08lx(%.3f) a4=%08lx(%.3f) flags50=%08lx state58=%08lx caller=0x%06x\r\n",
                    (unsigned long)GetTickCount(),(void *)arbiter,(const void *)entity,name,(unsigned long)a1,(double)f1,(unsigned long)a3,(double)f3,(unsigned long)a4,(double)f4,
                    (unsigned long)f50,(unsigned long)f58,rva_of(ret));
        }
        InterlockedExchange(&lock,0);
    }
    SetLastError(saved);
}
static void __attribute__((naked,noinline)) play_stub(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;"
        "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld; sub $16,%esp;"
        "mov 24(%ebp),%eax; mov %eax,(%esp); mov 40(%ebp),%eax; mov %eax,4(%esp);"
        "mov 36(%ebp),%eax; mov %eax,8(%esp); call _observe_play;"
        "add $16,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl;"
        "jmp *_SudekiMpLanStoryArbiterTracePlayTrampoline");
}
static void __attribute__((naked,noinline)) move_stub(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;"
        "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld; sub $32,%esp;"
        "mov 40(%ebp),%eax; mov %eax,(%esp); mov 44(%ebp),%eax; mov %eax,4(%esp);"
        "mov 48(%ebp),%eax; mov %eax,8(%esp); mov 52(%ebp),%eax; mov %eax,12(%esp);"
        "mov 36(%ebp),%eax; mov %eax,16(%esp); call _observe_move;"
        "add $32,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl;"
        "jmp *_SudekiMpLanStoryArbiterTraceMoveTrampoline");
}
/* Stubs preserve every register and the FP/SSE state. After pushfl/pushal with
 * EBP=ESP: [EBP+28]=EAX, [EBP+36]=return address, [EBP+40]=arg1, [EBP+44]=arg2. */
static void __attribute__((naked,noinline)) event_stub(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;"
        "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld; sub $16,%esp;"
        "mov 28(%ebp),%eax; mov %eax,(%esp); mov 40(%ebp),%eax; mov %eax,4(%esp);"
        "mov 36(%ebp),%eax; mov %eax,8(%esp); call _observe_event;"
        "add $16,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl;"
        "jmp *_SudekiMpLanStoryArbiterTraceEventTrampoline");
}
static void __attribute__((naked,noinline)) combo_stub(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;"
        "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld; sub $16,%esp;"
        "mov 28(%ebp),%eax; mov %eax,(%esp); mov 40(%ebp),%eax; mov %eax,4(%esp);"
        "mov 44(%ebp),%eax; mov %eax,8(%esp); mov 36(%ebp),%eax; mov %eax,12(%esp); call _observe_combo;"
        "add $16,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl;"
        "jmp *_SudekiMpLanStoryArbiterTraceComboTrampoline");
}
BOOL SudekiMpLanStoryArbiterTraceInstalled(void) { return base!=NULL; }
BOOL SudekiMpLanStoryArbiterTraceUninstall(void) {
    BOOL ok=TRUE;
    if(move_hook.target && !SudekiMpRestoreInlineHook(&move_hook)) ok=FALSE;
    if(play_hook.target && !SudekiMpRestoreInlineHook(&play_hook)) ok=FALSE;
    if(combo_hook.target && !SudekiMpRestoreInlineHook(&combo_hook)) ok=FALSE;
    if(event_hook.target && !SudekiMpRestoreInlineHook(&event_hook)) ok=FALSE;
    /* Trampolines stay referenced: a thread may still be inside them. */
    if(ok) base=NULL;
    return ok;
}
BOOL SudekiMpLanStoryArbiterTraceInstall(HMODULE image,const wchar_t *config_path) {
    uint8_t *b=(uint8_t *)image;
    BOOL trace_movement=TRUE;
    if(config_path) {
        wchar_t w[32]={0}; GetPrivateProfileStringW(L"StoryAreas",L"ArbiterTraceName",L"",w,31,config_path);
        for(unsigned i=0;i<31u && w[i];++i) name_filter[i]=(w[i]<0x80)?(char)w[i]:'?';
        /* The movement seam shares its entry bytes with the story control
         * layer's exact-entry proof (party_native_entries_exact); a host that
         * drives seats must leave it off: [StoryAreas] ArbiterTraceMovement=false. */
        trace_movement=GetPrivateProfileIntW(L"StoryAreas",L"ArbiterTraceMovement",1,config_path)!=0;
    }
    if(base || !b || !SudekiMpCheckLoadedExecutable(image)) {SetLastError(ERROR_INVALID_STATE); return FALSE;}
    if(memcmp(b+EVENT_RVA,event_bytes,EVENT_LENGTH) || memcmp(b+COMBO_RVA,combo_bytes,COMBO_LENGTH) ||
        memcmp(b+PLAY_RVA,play_bytes,PLAY_LENGTH) || memcmp(b+MOVE_RVA,move_bytes,MOVE_LENGTH)) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    if(!SudekiMpInstallInlineHook(&event_hook,b+EVENT_RVA,event_bytes,EVENT_LENGTH,(const void *)(uintptr_t)event_stub)) {
        DWORD error=GetLastError(); SetLastError(error?error:ERROR_INVALID_DATA); return FALSE;
    }
    SudekiMpLanStoryArbiterTraceEventTrampoline=event_hook.trampoline;
    if(!SudekiMpInstallInlineHook(&combo_hook,b+COMBO_RVA,combo_bytes,COMBO_LENGTH,(const void *)(uintptr_t)combo_stub)) {
        DWORD error=GetLastError(); (void)SudekiMpRestoreInlineHook(&event_hook);
        SetLastError(error?error:ERROR_INVALID_DATA); return FALSE;
    }
    SudekiMpLanStoryArbiterTraceComboTrampoline=combo_hook.trampoline;
    if(!SudekiMpInstallInlineHook(&play_hook,b+PLAY_RVA,play_bytes,PLAY_LENGTH,(const void *)(uintptr_t)play_stub) ||
        (SudekiMpLanStoryArbiterTracePlayTrampoline=play_hook.trampoline,
         trace_movement && !SudekiMpInstallInlineHook(&move_hook,b+MOVE_RVA,move_bytes,MOVE_LENGTH,(const void *)(uintptr_t)move_stub))) {
        DWORD error=GetLastError(); (void)SudekiMpLanStoryArbiterTraceUninstall();
        SetLastError(error?error:ERROR_INVALID_DATA); return FALSE;
    }
    SudekiMpLanStoryArbiterTraceMoveTrampoline=trace_movement?move_hook.trampoline:NULL; base=b;
    SudekiMpLogFormat("arbiter_trace event=installed seams=anim_event:0xdbc90,combo_dispatch:0xd0730,play_combo:0xd0f30%s name_filter=%s policy=log_only\r\n",
        trace_movement?",move_request:0xdae80":"",name_filter[0]?name_filter:"-");
    return TRUE;
}
