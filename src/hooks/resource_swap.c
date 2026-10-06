#include "hooks/resource_swap.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <stdint.h>
#include <string.h>
#if !defined(__GNUC__) || !defined(__i386__)
#error "Resource swap requires the supported x86 ABI"
#endif
enum { HASH_RVA=0x3a4a0, HASH_LENGTH=6, MAX_SWAPS=64, NAME_MAX=64, LOGS_PER_SWAP=3 };
/* 0x43A4A0 resource name checksum, cdecl (const char *name, uint32_t *out):
 *   8B 4C 24 04  MOV ECX,[ESP+4]
 *   33 D2        XOR EDX,EDX
 * Every archive key (NAME.EXT, upper-cased by the caller) and every SOL
 * ResourceHash is computed through it, including the archive's own hash
 * pointer (+0x80C). It is not inside any exact-image identity table; the
 * archive lookup/open functions are (lan_story_resource_file.c). */
static const uint8_t hash_bytes[HASH_LENGTH]={0x8b,0x4c,0x24,0x04,0x33,0xd2};
typedef struct Swap { uint32_t source_key,target_key; char source[NAME_MAX],target[NAME_MAX]; unsigned logs; } Swap;
static Swap swaps[MAX_SWAPS]; static unsigned swap_count;
static SudekiMpInlineHook hash_hook;
void *SudekiMpResourceSwapHashTrampoline __attribute__((used));
static uint8_t *base;
uint32_t SudekiMpResourceChecksum(const char *name) {
    uint32_t value=0;
    for(unsigned i=0;name[i];++i) {
        unsigned char c=(unsigned char)name[i]; if(c>='a' && c<='z') c=(unsigned char)(c-'a'+'A');
        value=(i&1u)?value*c:value+c;
    }
    return value;
}
unsigned SudekiMpResourceSwapCount(void) { return swap_count; }
static BOOL readable_code(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || VirtualQuery(p,&m,sizeof(m))!=sizeof(m) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
        a+n>(uintptr_t)m.BaseAddress+m.RegionSize) return FALSE;
    DWORD access=m.Protect&0xffu;
    return access==PAGE_EXECUTE_READ || access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
}
static BOOL readable_text(const char *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    if(!p || VirtualQuery(p,&m,sizeof(m))!=sizeof(m) || m.State!=MEM_COMMIT ||
        (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) || a+n>(uintptr_t)m.BaseAddress+m.RegionSize) return FALSE;
    return TRUE;
}
static Swap *find_key(uint32_t key) {
    for(unsigned i=0;i<swap_count;++i) if(swaps[i].source_key==key) return &swaps[i];
    return NULL;
}
static void note(Swap *s,const char *how) {
    if(s->logs<LOGS_PER_SWAP) { ++s->logs; SudekiMpLogFormat("resource_swap event=redirect via=%s source=%s target=%s key=%08lx->%08lx\r\n",how,s->source,s->target,(unsigned long)s->source_key,(unsigned long)s->target_key); }
}
/* Native caller's thread; pure table lookup, no native call, no lock needed.
 * Returns 1 after writing the target key to *out (the original is skipped),
 * 0 to let the native checksum run unchanged. */
static int __attribute__((used,noinline)) observe_hash(const char *name,uint32_t *out) {
    DWORD saved=GetLastError(); int handled=0;
    if(swap_count && readable_text(name,1u) && out) {
        size_t n=0; while(n<NAME_MAX && name[n]) ++n;
        if(n && n<NAME_MAX) {
            Swap *s=find_key(SudekiMpResourceChecksum(name));
            if(s) { *out=s->target_key; note(s,"hash"); handled=1; }
        }
    }
    SetLastError(saved); return handled;
}
/* After pushal with EBP=ESP: [EBP+32]=return address, [EBP+36]=name,
 * [EBP+40]=out. The native function touches no FP state, so none is saved.
 * Handled: restore registers and return to the caller (cdecl, caller cleans).
 * Otherwise continue in the native code through the trampoline. */
static void __attribute__((naked,noinline)) hash_stub(void) {
    __asm__ volatile("pushal; mov %esp,%ebp; and $-16,%esp; sub $16,%esp;"
        "mov 36(%ebp),%eax; mov %eax,(%esp); mov 40(%ebp),%eax; mov %eax,4(%esp); call _observe_hash;"
        "mov %ebp,%esp; test %eax,%eax; jz 1f; popal; ret;"
        "1: popal; jmp *_SudekiMpResourceSwapHashTrampoline");
}
/* Built-in profiles: proven sets a lobby can select by name. TalosOnTal is
 * the mp56 set (CONFIRMED_LIVE 2026-10-06: renders on host and client; walk/
 * run/attack not yet). A bank that does not match the skeleton hangs the load
 * (mp57), so PC_TAL.ANI must go with the Talos skeleton. */
typedef struct BuiltinSwap { const char *source,*target; } BuiltinSwap;
static const BuiltinSwap talos_on_tal[]={
    {"TAL.HOM","TALOS.HOM"},
    {"TAL_TAL_LORES_ARMOURA.HOM","TALOS_TALOS.HOM"},
    {"TAL_TAL_LORES_ARMOUR2.HOM","TALOS_TALOS.HOM"},
    {"TAL_TAL_LORES_ARMOUR3.HOM","TALOS_TALOS.HOM"},
    {"PC_TAL.ANI","BOSS_TALOS.ANI"},
};
static BOOL add_swap(const char *source,const char *target) {
    if(swap_count>=MAX_SWAPS || !source || !target || strlen(source)>=NAME_MAX || strlen(target)>=NAME_MAX) return FALSE;
    Swap *s=&swaps[swap_count]; memset(s,0,sizeof(*s));
    strcpy(s->source,source); strcpy(s->target,target);
    s->source_key=SudekiMpResourceChecksum(source); s->target_key=SudekiMpResourceChecksum(target);
    if(s->source_key==s->target_key) return FALSE;
    for(unsigned i=0;i<swap_count;++i) if(swaps[i].source_key==s->source_key) { swaps[i]=*s; return TRUE; }
    ++swap_count;
    SudekiMpLogFormat("resource_swap event=table source=%s target=%s key=%08lx->%08lx\r\n",s->source,s->target,(unsigned long)s->source_key,(unsigned long)s->target_key);
    return TRUE;
}
BOOL SudekiMpResourceSwapApplyProfile(const char *name) {
    if(!name || !*name) return FALSE;
    if(!_stricmp(name,"TalosOnTal")) {
        for(unsigned i=0;i<sizeof(talos_on_tal)/sizeof(talos_on_tal[0]);++i) add_swap(talos_on_tal[i].source,talos_on_tal[i].target);
        SudekiMpLogFormat("resource_swap event=profile name=%s swaps=%u\r\n",name,swap_count);
        return TRUE;
    }
    SudekiMpLogFormat("resource_swap event=profile_unknown name=%s\r\n",name);
    return FALSE;
}
void SudekiMpResourceSwapClear(void) { swap_count=0; }
static void load_section(const wchar_t *config_path,const wchar_t *section_name);
static void load_table(const wchar_t *config_path) {
    wchar_t profile[64]; swap_count=0;
    if(GetPrivateProfileStringW(L"ResourceSwap",L"Profile",L"",profile,64,config_path) && profile[0]) {
        char ascii[64]; unsigned i=0;
        for(;i<63u && profile[i];++i) ascii[i]=(profile[i]>=32 && profile[i]<127)?(char)profile[i]:'?';
        ascii[i]=0; (void)SudekiMpResourceSwapApplyProfile(ascii);
        wchar_t named[80]; wcscpy(named,L"ResourceSwap."); wcsncat(named,profile,60);
        load_section(config_path,named); /* ini overrides for a named profile */
    }
    load_section(config_path,L"ResourceSwap");
}
static void load_section(const wchar_t *config_path,const wchar_t *section_name) {
    wchar_t section[8192];
    DWORD n=GetPrivateProfileSectionW(section_name,section,sizeof(section)/sizeof(section[0]),config_path);
    if(!n || n>=sizeof(section)/sizeof(section[0])-2u) return;
    for(const wchar_t *p=section;*p && swap_count<MAX_SWAPS;p+=wcslen(p)+1u) {
        const wchar_t *eq=wcschr(p,L'='); if(!eq || p[0]==L';' || p[0]==L'#') continue;
        if(!_wcsnicmp(p,L"Profile=",8)) continue;
        size_t a=(size_t)(eq-p),b=wcslen(eq+1);
        while(a && (p[a-1]==L' ' || p[a-1]==L'\t')) --a;
        const wchar_t *t=eq+1; while(*t==L' ' || *t==L'\t') { ++t; --b; }
        while(b && (t[b-1]==L' ' || t[b-1]==L'\t' || t[b-1]==L'\r')) --b;
        if(!a || !b || a>=NAME_MAX || b>=NAME_MAX) continue;
        char source[NAME_MAX],target[NAME_MAX]; BOOL ascii=TRUE;
        for(size_t i=0;i<a;++i) { if(p[i]>126 || p[i]<32) ascii=FALSE; source[i]=(char)p[i]; } source[a]=0;
        for(size_t i=0;i<b;++i) { if(t[i]>126 || t[i]<32) ascii=FALSE; target[i]=(char)t[i]; } target[b]=0;
        if(ascii) (void)add_swap(source,target);
    }
}
BOOL SudekiMpResourceSwapUninstall(void) {
    if(!base) return TRUE;
    if(hash_hook.installed && !SudekiMpRestoreInlineHook(&hash_hook)) return FALSE;
    /* Trampolines stay referenced: a thread may still be inside one. */
    base=NULL; return TRUE;
}
BOOL SudekiMpResourceSwapInstall(HMODULE image,const wchar_t *config_path) {
    uint8_t *b=(uint8_t *)image;
    if(base || !b || !config_path || !SudekiMpCheckLoadedExecutable(image)) {SetLastError(ERROR_INVALID_STATE); return FALSE;}
    load_table(config_path);
    if(!swap_count) { SetLastError(ERROR_SUCCESS); return TRUE; } /* nothing to own */
    if(!readable_code(b+HASH_RVA,HASH_LENGTH) || memcmp(b+HASH_RVA,hash_bytes,HASH_LENGTH)) {
        SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    if(!SudekiMpInstallInlineHook(&hash_hook,b+HASH_RVA,hash_bytes,HASH_LENGTH,(const void *)(uintptr_t)hash_stub)) return FALSE;
    SudekiMpResourceSwapHashTrampoline=hash_hook.trampoline; base=b;
    SudekiMpLogFormat("resource_swap event=installed seam=resource_checksum:0x3a4a0 swaps=%u\r\n",swap_count);
    return TRUE;
}
