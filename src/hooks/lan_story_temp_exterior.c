#include "hooks/lan_story_temp_exterior.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include <math.h>
#include <string.h>
#if !defined(__GNUC__) || !defined(__i386__)
#error "TEMP exterior separation requires the supported x86 ABI"
#endif
/* Enter body after the observer's 9-byte prologue patch through RET 8; exit
 * body after its 12-byte patch through RET. Relocated operands are normalized
 * to the preferred base before hashing, so the exact bytes are verified
 * without reproducing them here. Any other patch inside either body (for
 * example the legacy party-transition lead-mover hook) refuses installation. */
enum { WORLD=0x408d10, WORLD_VT=0x2c4c3c, DESC_VT=0x2c82a4, DATA_VT=0x2cdcdc,
    SPAWN_VT=0x2cddf4, SPAWN_VT2=0x2cde04, POSITION_VT=0x2cdefc, POSITION_VT2=0x2cdf3c,
    ENTER_BODY=0x64b9, ENTER_END=0x6710, EXIT_BODY=0x671c, EXIT_END=0x6a05,
    ENTER_CALL=0x6527, SUSPEND=0x10b500, ENTER_SKIP=0x65f7,
    EXIT_CALL=0x679c, RESUME=0x10b4b0, EXIT_SKIP=0x686d, PREFERRED=0x400000 };
static const uint16_t enter_relocs[]={0x659c,0x660d,0x6634,0x66cd};
static const uint16_t exit_relocs[]={0x6739,0x6747,0x67e5,0x6823,0x686f,0x690c,0x693e,0x6989};
typedef struct Tracked {
    const uint8_t *spawn,*entity;
    float position[3];
    uint8_t disable;
} Tracked;
typedef struct Record {
    uint64_t ticket;
    const uint8_t *world,*descriptor,*data,*catalog,*terrain;
    const void *neighbors[SUDEKIMP_STORY_TEMP_EXTERIOR_NEIGHBORS];
    uint32_t neighbor_count,catalog_count,tracked_count;
    Tracked tracked[SUDEKIMP_STORY_TEMP_EXTERIOR_TRACKED];
    uint8_t terrain_enabled; uint32_t terrain_mask;
    DWORD started;
    char exterior[SUDEKIMP_STORY_TEMP_EXTERIOR_NAME];
} Record;
static SudekiMpRelativeCallHook enter_hook,exit_hook;
static SRWLOCK lock=SRWLOCK_INIT;
static uint8_t *base;
static void *enter_original __attribute__((used)),*enter_skip __attribute__((used));
static void *exit_original __attribute__((used)),*exit_skip __attribute__((used));
static const SudekiMpLanStoryTempExteriorConsumer *consumer;
static Record record;
static DWORD native_thread;
static uint64_t serial,native_entries,kept_entries,native_exits,skipped_exits,mismatched_exits;
static BOOL installed,unknown;
static unsigned callbacks;
static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || VirtualQuery(p,&m,sizeof(m))!=sizeof(m) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
        a+n>(uintptr_t)m.BaseAddress+m.RegionSize) return FALSE;
    DWORD access=m.Protect&0xffu;
    return access==PAGE_READONLY || access==PAGE_READWRITE || access==PAGE_WRITECOPY ||
        access==PAGE_EXECUTE_READ || access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
}
static BOOL call_exact(const uint8_t *b,unsigned at,unsigned target) {
    int32_t d;
    if(!readable(b+at,5) || b[at]!=0xe8) return FALSE;
    memcpy(&d,b+at+1,4); return b+at+5+d==b+target;
}
static uint64_t body_hash(const uint8_t *b,unsigned begin,unsigned end,
    const uint16_t *relocs,unsigned count) {
    uint64_t h=0xcbf29ce484222325ull; uint32_t delta=(uint32_t)((uintptr_t)b-PREFERRED);
    for(unsigned at=begin;at<end;) {
        uint8_t chunk[4]; unsigned n=1; chunk[0]=b[at];
        for(unsigned i=0;i<count;++i) if(relocs[i]==at) {
            uint32_t v; memcpy(&v,b+at,4); v-=delta; memcpy(chunk,&v,4); n=4; break;
        }
        for(unsigned i=0;i<n;++i) {h^=chunk[i]; h*=0x100000001b3ull;}
        at+=n;
    }
    return h;
}
static BOOL image_exact(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    if(!readable(b,sizeof(IMAGE_DOS_HEADER)) || !SudekiMpCheckLoadedExecutable(image) ||
        !readable(b+ENTER_BODY,ENTER_END-ENTER_BODY) || !readable(b+EXIT_BODY,EXIT_END-EXIT_BODY))
        return FALSE;
    return call_exact(b,ENTER_CALL,SUSPEND) && call_exact(b,EXIT_CALL,RESUME) &&
        body_hash(b,ENTER_BODY,ENTER_END,enter_relocs,4)==0x49f620b3b5426a64ull &&
        body_hash(b,EXIT_BODY,EXIT_END,exit_relocs,8)==0x8b602d3891cc1598ull;
}
static BOOL hooked(const SudekiMpRelativeCallHook *h,unsigned at) {
    int32_t d;
    if(!h->installed || !readable(base+at,5) || base[at]!=0xe8) return FALSE;
    memcpy(&d,base+at+1,4); return d==h->replacement_displacement;
}
static BOOL healthy(void) {
    if(!base || !installed || unknown) return FALSE;
    if(!hooked(&enter_hook,ENTER_CALL) || !hooked(&exit_hook,EXIT_CALL)) {unknown=TRUE;return FALSE;}
    return TRUE;
}
static void text_copy(char out[SUDEKIMP_STORY_TEMP_EXTERIOR_NAME],const char *p) {
    memset(out,0,SUDEKIMP_STORY_TEMP_EXTERIOR_NAME);
    if(!p) return;
    for(unsigned i=0;i+1<SUDEKIMP_STORY_TEMP_EXTERIOR_NAME;++i) {
        if(!readable(p+i,1) || !p[i]) return;
        unsigned char c=(unsigned char)p[i];
        out[i]=(char)((c>=0x20 && c<0x7f)?c:'?');
    }
}
/* World/table/descriptor identity. Enter sees the exterior as current
 * (+0xC); at the exit seam native code has already cleared +0xC and selected
 * the state-3 exterior into +0x10. */
static BOOL exterior_exact(const uint8_t *w,const uint8_t *d,unsigned slot) {
    if(!w || w!=*(uint8_t **)(base+WORLD) || !readable(w,0x3a0) ||
        *(void *const *)w!=base+WORLD_VT || *(const uint8_t *const *)(w+slot)!=d) return FALSE;
    const uint8_t *table=*(const uint8_t *const *)(w+0x50);
    unsigned count=*(const unsigned *)(w+0x54);
    uintptr_t p=(uintptr_t)d,start=(uintptr_t)table;
    return count && count<=4096 && readable(table,(size_t)count*0x54) && p>=start &&
        !((p-start)%0x54) && (p-start)/0x54<count && *(void *const *)d==base+DESC_VT &&
        *(const uint32_t *)(d+0x34)==3;
}
static const uint8_t *entity_position(const uint8_t *spawn) {
    if(!readable(spawn,0x90) || *(void *const *)spawn!=base+SPAWN_VT ||
        *(void *const *)(spawn+4)!=base+SPAWN_VT2) return NULL;
    uintptr_t v=*(const uintptr_t *)(spawn+0x78);
    if(!v || v==0x2c) return NULL;
    const uint8_t *e=(const uint8_t *)(v-0x2c),*p;
    if(!readable(e,0x48) || !(p=*(const uint8_t *const *)(e+0x44)) || !readable(p,0x24) ||
        *(void *const *)p!=base+POSITION_VT || *(void *const *)(p+4)!=base+POSITION_VT2 ||
        *(const uint8_t *const *)(p+0x10)!=e) return NULL;
    return p;
}
/* Copy-only diagnostics. Unreadable/foreign catalog content leaves the entry
 * native, rather than guessing. */
static BOOL capture(Record *r,const uint8_t *w,const uint8_t *d) {
    memset(r,0,sizeof(*r)); r->world=w; r->descriptor=d;
    r->neighbor_count=*(const uint32_t *)(d+0x3c);
    const void *const *neighbors=*(const void *const *const *)(d+0x44);
    if(r->neighbor_count>SUDEKIMP_STORY_TEMP_EXTERIOR_NEIGHBORS ||
        (r->neighbor_count && !readable(neighbors,r->neighbor_count*4u))) return FALSE;
    for(unsigned i=0;i<r->neighbor_count;++i) r->neighbors[i]=neighbors[i];
    text_copy(r->exterior,*(const char *const *)(d+0x24));
    r->data=*(const uint8_t *const *)(d+0x14);
    if(!readable(r->data,0x12a) || *(void *const *)r->data!=base+DATA_VT || r->data[0x128]!=5)
        return FALSE;
    r->terrain=*(const uint8_t *const *)(r->data+0x2c);
    if(r->terrain) {
        if(!readable(r->terrain,0x110)) return FALSE;
        r->terrain_enabled=r->terrain[0xf4]; r->terrain_mask=*(const uint32_t *)(r->terrain+0x44);
    }
    r->catalog=*(const uint8_t *const *)(r->data+0x118);
    if(!readable(r->catalog,0x1c)) return FALSE;
    r->catalog_count=*(const uint32_t *)(r->catalog+0x10);
    const uint8_t *spawns=*(const uint8_t *const *)(r->catalog+0x18);
    if(r->catalog_count>4096 || (r->catalog_count && !readable(spawns,r->catalog_count*0x90u)))
        return FALSE;
    for(unsigned i=0;i<r->catalog_count && r->tracked_count<SUDEKIMP_STORY_TEMP_EXTERIOR_TRACKED;++i) {
        const uint8_t *sp=spawns+i*0x90u,*p=entity_position(sp);
        if(!p) continue;
        Tracked *t=&r->tracked[r->tracked_count++];
        t->spawn=sp; t->entity=*(const uint8_t *const *)(p+0x10);
        memcpy(t->position,p+0x18,sizeof(t->position)); t->disable=t->entity[0x2b];
    }
    r->started=GetTickCount(); return TRUE;
}
static BOOL same_shape(const Record *r,const uint8_t *w,const uint8_t *d,unsigned slot) {
    if(r->world!=w || r->descriptor!=d || !exterior_exact(w,d,slot) ||
        *(const uint32_t *)(d+0x3c)!=r->neighbor_count ||
        *(const uint8_t *const *)(d+0x14)!=r->data) return FALSE;
    const void *const *neighbors=*(const void *const *const *)(d+0x44);
    if(r->neighbor_count && !readable(neighbors,r->neighbor_count*4u)) return FALSE;
    for(unsigned i=0;i<r->neighbor_count;++i) if(neighbors[i]!=r->neighbors[i]) return FALSE;
    return readable(r->data,0x12a) && *(const uint8_t *const *)(r->data+0x118)==r->catalog &&
        *(const uint8_t *const *)(r->data+0x2c)==r->terrain && readable(r->catalog,0x1c) &&
        *(const uint32_t *)(r->catalog+0x10)==r->catalog_count;
}
static void compare(const Record *r,SudekiMpLanStoryTempExteriorReceipt *out) {
    out->tracked=r->tracked_count;
    for(unsigned i=0;i<r->tracked_count;++i) {
        const Tracked *t=&r->tracked[i]; const uint8_t *p=entity_position(t->spawn);
        if(!p || *(const uint8_t *const *)(p+0x10)!=t->entity) continue;
        ++out->still_present;
        float now[3],sum=0.0f; memcpy(now,p+0x18,sizeof(now));
        for(unsigned k=0;k<3;++k) {float delta=now[k]-t->position[k]; sum+=delta*delta;}
        if(!isfinite(sum)) continue;
        float distance=sqrtf(sum);
        if(distance>0.01f) ++out->moved; else ++out->unchanged;
        if(distance>out->max_displacement) out->max_displacement=distance;
        uint8_t now_disable=t->entity[0x2b];
        if(t->disable) ++out->disable_entry_nonzero;
        if(now_disable) ++out->disable_exit_nonzero;
        if(now_disable!=t->disable) {
            ++out->disable_changed;
            if((int8_t)(now_disable-t->disable)>0) ++out->disable_raised; else ++out->disable_lowered;
        }
    }
    out->terrain_present=r->terrain!=NULL; out->terrain_enabled_entry=r->terrain_enabled;
    out->terrain_mask_entry=r->terrain_mask;
    if(r->terrain && readable(r->terrain,0x110)) {
        out->terrain_enabled_exit=r->terrain[0xf4]; out->terrain_mask_exit=*(const uint32_t *)(r->terrain+0x44);
    }
}
static BOOL thread_exact(void) {
    DWORD thread=GetCurrentThreadId();
    if(!native_thread) native_thread=thread;
    else if(native_thread!=thread) unknown=TRUE;
    return !unknown;
}
/* C is entered with a clean floating-point environment on the TEMP caller's
 * thread. Any uncertainty takes the native suspension path. */
static BOOL __attribute__((used,noinline)) enter_decide(const uint8_t *w,const uint8_t *d,const uint8_t *frame) {
    DWORD saved=GetLastError(); BOOL keep=FALSE;
    AcquireSRWLockExclusive(&lock); ++callbacks;
    const SudekiMpLanStoryTempExteriorConsumer *c=consumer;
    Record fresh;
    BOOL candidate=thread_exact() && healthy() && c && c->keep_exterior;
    if(candidate && record.ticket) {unknown=TRUE; candidate=FALSE;}
    if(candidate) candidate=exterior_exact(w,d,0xc) && readable(frame+8,4) && capture(&fresh,w,d) &&
        serial!=UINT64_MAX;
    SudekiMpLanStoryTempExteriorEntry entry={0};
    if(candidate) {
        entry.ticket=serial+1; memcpy(entry.exterior,fresh.exterior,sizeof(entry.exterior));
        text_copy(entry.destination,*(const char *const *)(frame+8));
        entry.neighbors=fresh.neighbor_count; entry.characters=fresh.catalog_count;
        entry.tracked=fresh.tracked_count;
    }
    ReleaseSRWLockExclusive(&lock);
    if(candidate) candidate=c->keep_exterior(c->context,&entry);
    AcquireSRWLockExclusive(&lock);
    if(candidate && healthy() && consumer==c && !record.ticket && same_shape(&fresh,w,d,0xc)) {
        fresh.ticket=++serial; record=fresh; ++kept_entries; keep=TRUE;
    }
    else ++native_entries;
    --callbacks; ReleaseSRWLockExclusive(&lock); SetLastError(saved); return keep;
}
static BOOL __attribute__((used,noinline)) exit_decide(const uint8_t *w,const uint8_t *d) {
    DWORD saved=GetLastError();
    AcquireSRWLockExclusive(&lock); ++callbacks;
    (void)thread_exact();
    if(!record.ticket) {
        ++native_exits; --callbacks; ReleaseSRWLockExclusive(&lock); SetLastError(saved); return FALSE;
    }
    /* Nothing was suspended by the kept entry, so resuming is never correct
     * here, even when identity no longer matches. Mismatch latches unknown. */
    SudekiMpLanStoryTempExteriorReceipt receipt={0};
    receipt.ticket=record.ticket; memcpy(receipt.exterior,record.exterior,sizeof(receipt.exterior));
    receipt.elapsed_ms=GetTickCount()-record.started;
    if(same_shape(&record,w,d,0x10)) {receipt.result=SUDEKIMP_STORY_TEMP_EXTERIOR_EXIT_BALANCED; compare(&record,&receipt);}
    else {receipt.result=SUDEKIMP_STORY_TEMP_EXTERIOR_EXIT_MISMATCH; unknown=TRUE; ++mismatched_exits;}
    memset(&record,0,sizeof(record)); ++skipped_exits;
    const SudekiMpLanStoryTempExteriorConsumer *c=consumer;
    ReleaseSRWLockExclusive(&lock);
    if(c && c->exit_skipped) c->exit_skipped(c->context,&receipt);
    AcquireSRWLockExclusive(&lock); --callbacks; ReleaseSRWLockExclusive(&lock);
    SetLastError(saved); return TRUE;
}
/* Enter: EAX=exterior descriptor, EBX=world, EBP=native frame. Exit: EDI=
 * exterior descriptor, EBX=world. A kept decision discards the call's return
 * address and continues at the native instruction after the whole skipped
 * unit; both targets use only EBX/EBP/ESI and frame-relative state that the
 * native loop leaves unchanged. LEA preserves the restored flags. */
static void __attribute__((naked,noinline)) enter_entry(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;"
        "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld; sub $16,%esp;"
        "mov 16(%ebp),%eax; mov %eax,(%esp); mov 28(%ebp),%eax; mov %eax,4(%esp);"
        "mov 8(%ebp),%eax; mov %eax,8(%esp); call _enter_decide; test %eax,%eax; jz 1f;"
        "add $16,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl; lea 4(%esp),%esp; jmp *_enter_skip;"
        "1: add $16,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl; jmp *_enter_original");
}
static void __attribute__((naked,noinline)) exit_entry(void) {
    __asm__ volatile("pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;"
        "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld; sub $16,%esp;"
        "mov 16(%ebp),%eax; mov %eax,(%esp); mov 0(%ebp),%eax; mov %eax,4(%esp);"
        "call _exit_decide; test %eax,%eax; jz 1f;"
        "add $16,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl; lea 4(%esp),%esp; jmp *_exit_skip;"
        "1: add $16,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl; jmp *_exit_original");
}
BOOL SudekiMpLanStoryTempExteriorAttach(const SudekiMpLanStoryTempExteriorConsumer *c) {
    if(!c || !c->keep_exterior) return FALSE;
    AcquireSRWLockExclusive(&lock);
    BOOL ok=healthy() && !consumer && !callbacks && !record.ticket;
    if(ok) consumer=c;
    ReleaseSRWLockExclusive(&lock); return ok;
}
BOOL SudekiMpLanStoryTempExteriorDetach(void) {
    AcquireSRWLockExclusive(&lock);
    BOOL ok=!callbacks && !record.ticket;
    if(ok) consumer=NULL;
    ReleaseSRWLockExclusive(&lock); return ok;
}
BOOL SudekiMpLanStoryTempExteriorStatusCopy(SudekiMpLanStoryTempExteriorStatus *out) {
    if(!out) return FALSE;
    AcquireSRWLockShared(&lock);
    out->installed=installed; out->attached=consumer!=NULL; out->outstanding=record.ticket!=0;
    out->unknown=unknown; out->native_entries=native_entries; out->kept_entries=kept_entries;
    out->native_exits=native_exits; out->skipped_exits=skipped_exits;
    out->mismatched_exits=mismatched_exits;
    ReleaseSRWLockShared(&lock); return TRUE;
}
BOOL SudekiMpLanStoryTempExteriorUninstall(void) {
    AcquireSRWLockExclusive(&lock);
    if(!base) {ReleaseSRWLockExclusive(&lock); return TRUE;}
    DWORD error=(callbacks || unknown || record.ticket || consumer)?ERROR_BUSY:ERROR_SUCCESS;
    if(!error) {
        /* Attempt every restoration in reverse order; keep the first error. */
        if(!SudekiMpRestoreRelativeCallHook(&exit_hook)) error=GetLastError()?GetLastError():ERROR_BUSY;
        if(!SudekiMpRestoreRelativeCallHook(&enter_hook) && !error) error=GetLastError()?GetLastError():ERROR_BUSY;
    }
    if(!error) {
        base=NULL; enter_original=enter_skip=exit_original=exit_skip=NULL;
        installed=FALSE; native_thread=0;
    }
    ReleaseSRWLockExclusive(&lock);
    if(error) {
        HMODULE self;
        (void)GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            (LPCSTR)(uintptr_t)&SudekiMpLanStoryTempExteriorUninstall,&self);
        SetLastError(error?error:ERROR_BUSY); return FALSE;
    }
    return TRUE;
}
BOOL SudekiMpLanStoryTempExteriorInstall(HMODULE image) {
    AcquireSRWLockExclusive(&lock);
    uint8_t *b=(uint8_t *)image;
    const uint8_t *w=b && readable(b+WORLD,4)?*(const uint8_t *const *)(b+WORLD):NULL;
    const uint8_t *current=w && readable(w,0x10)?*(const uint8_t *const *)(w+0xc):NULL;
    if(base || installed || unknown || !image_exact(image) || (w && !readable(w,0x10)) ||
        (current && (!readable(current,0x38) || *(const uint32_t *)(current+0x34)==4))) {
        ReleaseSRWLockExclusive(&lock); SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    base=b; enter_original=b+SUSPEND; enter_skip=b+ENTER_SKIP;
    exit_original=b+RESUME; exit_skip=b+EXIT_SKIP;
    BOOL ok=SudekiMpInstallRelativeCallHook(&enter_hook,b+ENTER_CALL,enter_original,(void *)(uintptr_t)enter_entry) &&
        SudekiMpInstallRelativeCallHook(&exit_hook,b+EXIT_CALL,exit_original,(void *)(uintptr_t)exit_entry);
    DWORD error=GetLastError();
    if(ok) installed=TRUE;
    else {
        BOOL restored=SudekiMpRestoreRelativeCallHook(&exit_hook);
        restored=SudekiMpRestoreRelativeCallHook(&enter_hook) && restored;
        if(restored) {base=NULL; enter_original=enter_skip=exit_original=exit_skip=NULL;}
        else unknown=TRUE;
    }
    ReleaseSRWLockExclusive(&lock);
    if(!ok) SetLastError(error);
    return ok;
}
