#include "hooks/lan_story_spawn_trace.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <stdint.h>
#include <string.h>

enum { CREATE=0, START1, START2, START3, DESPAWN, COUNT };
static const unsigned rvas[COUNT]={0x754a0u,0x75d70u,0x75e10u,0x75eb0u,0xb2300u};
static const uint8_t entries[COUNT][8]={
    {0x53,0x56,0x8b,0x74,0x24,0x0c,0x8b,0x16},  /* PUSH EBX; PUSH ESI; MOV ESI,[ESP+0xc]; MOV EDX,[ESI] */
    {0x56,0x8b,0x74,0x24,0x08,0x8b,0x16},       /* PUSH ESI; MOV ESI,[ESP+8]; MOV EDX,[ESI] */
    {0x56,0x8b,0x74,0x24,0x08,0x8b,0x16},
    {0x55,0x8b,0xec,0x83,0xe4,0xf8},            /* PUSH EBP; MOV EBP,ESP; AND ESP,-8 */
    {0x55,0x8b,0xec,0x83,0xe4,0xf8}};
static const size_t lengths[COUNT]={8,7,7,6,6};
static SudekiMpInlineHook hooks[COUNT];
static unsigned logs;

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && VirtualQuery(p,&m,sizeof(m))==sizeof(m) && m.State==MEM_COMMIT &&
        !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) && a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
/* ResourceName: +0 kind bits, +4 hash, +8 shared name record; the record's
 * second word is the C string in the observed layout (entity+0x38 uses it). */
static void name_of(const void *resource,char out[64],uint32_t *hash) {
    const uint32_t *r=resource; out[0]='?'; out[1]=0; *hash=0;
    if(!readable(r,12u)) return;
    *hash=r[1];
    const uint32_t *rec=(const uint32_t *)(uintptr_t)r[2];
    if(!readable(rec,8u)) return;
    const char *s=(const char *)(uintptr_t)rec[1]; unsigned k=0;
    while(k<63u && readable(s+k,1u) && s[k]>=0x20 && s[k]<0x7f) { out[k]=s[k]; ++k; }
    out[k]=0;
}
static void trace(const char *what,const void *resource,int a,int b,const char *text) {
    char name[64]; uint32_t hash;
    if(logs>=600u) return;
    ++logs; name_of(resource,name,&hash);
    SudekiMpLogFormat("story_spawn_trace event=%s group=%s hash=%08lx a=%d b=%d text=%s tick=%lu\r\n",
        what,name,(unsigned long)hash,a,b,text?text:"-",(unsigned long)GetTickCount());
}
typedef void (__cdecl *Create)(const void *);
typedef void (__cdecl *Start1)(const void *,int);
typedef void (__cdecl *Start2)(const void *,int,int);
typedef void (__cdecl *Start3)(const char *,const void *,int,int);
typedef void (__cdecl *Despawn)(void *);
static void __cdecl create_hook(const void *n) { trace("create_group",n,0,0,NULL); ((Create)hooks[CREATE].trampoline)(n); }
static void __cdecl start1_hook(const void *n,int a) { trace("start_sequence1",n,a,0,NULL); ((Start1)hooks[START1].trampoline)(n,a); }
static void __cdecl start2_hook(const void *n,int a,int b) { trace("start_sequence2",n,a,b,NULL); ((Start2)hooks[START2].trampoline)(n,a,b); }
static void __cdecl start3_hook(const char *t,const void *n,int a,int b) {
    char copy[48]={0};
    for(unsigned k=0;k<47u && t && readable(t+k,1u) && t[k]>=0x20 && t[k]<0x7f;++k) copy[k]=t[k];
    trace("start_sequence3",n,a,b,copy); ((Start3)hooks[START3].trampoline)(t,n,a,b);
}
static void __cdecl despawn_hook(void *ptr) {
    void *entity=readable(ptr,4u)?*(void **)ptr:NULL;
    if(logs<600u) { ++logs; SudekiMpLogFormat("story_spawn_trace event=despawn handle=%p entity=%p tick=%lu\r\n",ptr,entity,(unsigned long)GetTickCount()); }
    ((Despawn)hooks[DESPAWN].trampoline)(ptr);
}
static const void *replacements[COUNT]={(const void *)create_hook,(const void *)start1_hook,
    (const void *)start2_hook,(const void *)start3_hook,(const void *)despawn_hook};

BOOL SudekiMpLanStorySpawnTraceInstall(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    if(!b || !SudekiMpCheckLoadedExecutable(image)) { SetLastError(ERROR_INVALID_DATA); return FALSE; }
    for(unsigned i=0;i<COUNT;++i) {
        if(!SudekiMpInstallInlineHook(&hooks[i],b+rvas[i],entries[i],lengths[i],replacements[i])) {
            DWORD error=GetLastError();
            while(i--) (void)SudekiMpRestoreInlineHook(&hooks[i]);
            SetLastError(error); return FALSE;
        }
    }
    SudekiMpLogWrite("story_spawn_trace event=installed policy=observe_only\r\n");
    return TRUE;
}
BOOL SudekiMpLanStorySpawnTraceUninstall(void) {
    BOOL ok=TRUE;
    for(unsigned i=COUNT;i--;) if(hooks[i].installed && !SudekiMpRestoreInlineHook(&hooks[i])) ok=FALSE;
    return ok;
}
