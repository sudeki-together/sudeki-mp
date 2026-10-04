#include "hooks/lan_party_projectile_lifetime.h"
#include "engine/build_identity.h"
#include "hooks/call_hook.h"
#include <stddef.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Projectile observers require the verified retail x86 ABI"
#endif

enum {
    TPTR_COPY=0x15b0, TPTR_DESTRUCT=0x4d30, TPTR_BASE_VT=0x2c4c34,
    MISSILE_DELETING=0x1862f0, MISSILE_DESTRUCT=0x186310,
    MISSILE_DESTRUCT_TAIL=0x186445, MISSILE_VT=0x2d915c,
    MISSILE_SECONDARY_VT=0x2d919c, MANAGER_VT=0x2d4c8c,
    ELCO_VT=0x2d66fc, AILISH_VT=0x2d555c,
    SOURCE_OFFSET=0x64, SOURCE_COUNT=10, JOURNAL_CAPACITY=512,
    DESCENDANT_CAPACITY=512, SCOPE_CAPACITY=16,
    CONSTRUCT_CALL=0x141d19, CONSTRUCT=0x1861c0,
    UPDATE_CALL=0x141f02, UPDATE=0x1867d0, TERMINATE=0x186610
};
typedef struct ProjectileTPtr {
    void *object;
    struct ProjectileTPtr *previous, *next;
} ProjectileTPtr;
_Static_assert(sizeof(ProjectileTPtr)==12, "native TPtr layout");
typedef struct ProjectileEntry {
    ProjectileTPtr observer;
    SudekiMpLanPartyProjectileTag tag;
    void *target;
    BOOL used, registered, termination_entered;
} ProjectileEntry;

/* Intrusive addresses must not move, even across disconnected player roles.
 * Only the native base destructor may clear a registered observer. */
static ProjectileEntry journal[JOURNAL_CAPACITY];
static uint8_t *image;
static DWORD native_thread;
static volatile LONG operation, active_count, unknown;
typedef struct ProjectileDescendant {
    ProjectileTPtr observer;
    SudekiMpLanPartyEffectOwner owner;
    void *target;
    void *entity;
    uint32_t incarnation;
    BOOL used, registered, termination_entered, termination_returned;
} ProjectileDescendant;
static ProjectileDescendant descendants[DESCENDANT_CAPACITY];
static SudekiMpLanPartyEffectOwner scopes[SCOPE_CAPACITY];
static unsigned scope_depth;
static volatile LONG descendant_count, callbacks, restore_failed;
static uint32_t next_incarnation;
static SudekiMpRelativeCallHook construct_hook, update_hook, terminate_hooks[6];
static const unsigned terminate_calls[]={0xc71b0,0xc77f4,0x186c75,0x187342,0x187371,0x187818};
static void *construct_original __attribute__((used));
static void *update_original __attribute__((used));
static void *terminate_original __attribute__((used));

static void construct_entry(void);
static void update_entry(void);
static void terminate_entry(void);

static BOOL memory(const void *p,size_t size,BOOL write) {
    MEMORY_BASIC_INFORMATION m;
    uintptr_t start=(uintptr_t)p;
    if(!p || !size || start>UINTPTR_MAX-size ||
        VirtualQuery(p,&m,sizeof(m))!=sizeof(m) || m.State!=MEM_COMMIT ||
        (m.Protect&(PAGE_NOACCESS|PAGE_GUARD))) return FALSE;
    DWORD access=m.Protect&0xffu;
    BOOL writable=access==PAGE_READWRITE || access==PAGE_WRITECOPY ||
        access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
    BOOL readable=writable || access==PAGE_READONLY || access==PAGE_EXECUTE_READ;
    return readable && (!write || writable) &&
        start+size<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL bytes(const uint8_t *base,unsigned rva,const void *expected,size_t n) {
    return memory(base+rva,n,FALSE) && !memcmp(base+rva,expected,n);
}
static BOOL relocated(const uint8_t *base,unsigned at,unsigned target) {
    return memory(base+at,4,FALSE) && *(void **)(base+at)==base+target;
}
static BOOL call_target(const uint8_t *call,const void *target) {
    int32_t displacement;
    if(!memory(call,5,FALSE) || *call!=0xe8) return FALSE;
    memcpy(&displacement,call+1,4);
    return call+5+displacement==target;
}
static BOOL emission_image_matches(const uint8_t *b) {
    static const uint8_t constructor_call[]={0x89,0x9e,0xe8,0x03,0,0,0xe8};
    static const uint8_t constructor_return[]={0x5f,0x8b,0xc6,0x5e,0x5b,0xc3};
    static const uint8_t update_call[]={0x57,0x81,0xc6,0x0c,0x04,0,0,0x56,0xe8};
    static const uint8_t update_return[]={0x5f,0x5e,0xc2,0x04,0};
    if(!bytes(b,CONSTRUCT_CALL-6,constructor_call,sizeof(constructor_call)) ||
        !bytes(b,CONSTRUCT_CALL+5,constructor_return,sizeof(constructor_return)) ||
        !bytes(b,CONSTRUCT,"\xc7\x47\x04",3) ||
        !relocated(b,CONSTRUCT+3,TPTR_BASE_VT) ||
        !bytes(b,0x1862df,"\x8b\xc7\x5b\xc3",4) ||
        !bytes(b,UPDATE_CALL-8,update_call,sizeof(update_call)) ||
        !bytes(b,UPDATE_CALL+5,update_return,sizeof(update_return)) ||
        !bytes(b,UPDATE,"\x55\x8b\xec\x83\xe4\xf8\x83\xec\x44",9) ||
        !bytes(b,0x186c7a,"\x5f\x5e\x5b\x8b\xe5\x5d\xc2\x08\x00",9) ||
        !bytes(b,TERMINATE,"\x55\x8b\xec\x83\xe4\xf8\x83\xec\x0c\x53\x56\x8b\xf0",13) ||
        !bytes(b,0x1867c3,"\x5f\x5e\x5b\x8b\xe5\x5d\xc3",7) ||
        !call_target(b+CONSTRUCT_CALL,b+CONSTRUCT) || !call_target(b+UPDATE_CALL,b+UPDATE))
        return FALSE;
    for(unsigned i=0;i<6;++i)
        if(!call_target(b+terminate_calls[i],b+TERMINATE)) return FALSE;
    return TRUE;
}
static BOOL image_matches(HMODULE module) {
    const uint8_t *b=(const uint8_t *)module;
    static const uint8_t copy[]={
        0x8b,0x11,0x8b,0xca,0x89,0x10,0xc7,0x40,0x04,0,0,0,0,
        0xc7,0x40,0x08,0,0,0,0,0x85,0xc9,0x74,0x13,0x8b,0x51,0x04,
        0x85,0xd2,0x74,0x03,0x89,0x42,0x04,0x8b,0x51,0x04,0x89,
        0x50,0x08,0x89,0x41,0x04,0xc3};
    static const uint8_t destroy[]={
        0x8b,0x41,0x04,0x56,0x33,0xf6,0xc7,0x01,0x34,0x4c,0x6c,0,
        0x3b,0xc6,0x74,0x41,0x57,0x8b,0x08,0x8b,0x50,0x08,0x3b,0xce,
        0x74,0x28,0x39,0x41,0x04,0x75,0x03,0x89,0x51,0x04,0x8b,0x48,
        0x04,0x3b,0xce,0x74,0x06,0x8b,0x78,0x08,0x89,0x79,0x08,0x8b,
        0x48,0x08,0x3b,0xce,0x74,0x06,0x8b,0x78,0x04,0x89,0x79,0x04,
        0x89,0x70,0x08,0x89,0x70,0x04,0x89,0x30,0x89,0x70,0x08,
        0x89,0x70,0x04,0x8b,0xc2,0x3b,0xd6,0x75,0xc1,0x5f,0x5e,0xc3};
    static const uint8_t deleting[]={
        0x56,0x8b,0xf1,0xe8,0x18,0,0,0,0xf6,0x44,0x24,0x08,0x01,
        0x74,0x09,0x56,0xe8,0x4a,0x21,0x0c,0,0x83,0xc4,0x04,
        0x8b,0xc6,0x5e,0xc2,0x04,0};
    static const uint8_t secondary_destructor[]={0x83,0xe9,0x04,0xe9,0x88,0xe9,0xff,0xff};
    static const uint8_t entry[]={
        0x51,0x8b,0x8e,0x98,0,0,0,0x53,0x55,0x57,0x8d,0x7e,0x04,
        0x8d,0x46,0x70,0x51,0xc7,0x06};
    static const uint8_t tail[]={
        0x8d,0x4e,0x18,0xe8,0x73,0xb0,0xe8,0xff,0xc7,0x07,
        0xec,0x55,0x6c,0,0x8b,0xcf,0x5f,0x5d,0xc7,0x06,
        0xac,0x55,0x6c,0,0x5b,0x83,0xc4,0x04,0xe9,0xca,0xe8,0xe7,0xff};
    if(!memory(b,0x1000,FALSE)) return FALSE;
    const IMAGE_DOS_HEADER *dos=(const IMAGE_DOS_HEADER *)b;
    if(dos->e_lfanew<=0 || (unsigned)dos->e_lfanew>
            0x1000u-sizeof(IMAGE_NT_HEADERS32) ||
        !SudekiMpCheckLoadedExecutable(module)) return FALSE;
    return bytes(b,TPTR_COPY,copy,sizeof(copy)) &&
        bytes(b,TPTR_DESTRUCT,destroy,8) &&
        relocated(b,TPTR_DESTRUCT+8,TPTR_BASE_VT) &&
        bytes(b,TPTR_DESTRUCT+12,destroy+12,sizeof(destroy)-12) &&
        relocated(b,MISSILE_SECONDARY_VT,0x187960) &&
        bytes(b,0x187960,secondary_destructor,sizeof(secondary_destructor)) &&
        bytes(b,MISSILE_DELETING,deleting,sizeof(deleting)) &&
        bytes(b,MISSILE_DESTRUCT,entry,sizeof(entry)) &&
        relocated(b,MISSILE_DESTRUCT+19,MISSILE_VT) &&
        bytes(b,MISSILE_DESTRUCT_TAIL,tail,10) &&
        relocated(b,MISSILE_DESTRUCT_TAIL+10,0x2c55ec) &&
        bytes(b,MISSILE_DESTRUCT_TAIL+14,tail+14,6) &&
        relocated(b,MISSILE_DESTRUCT_TAIL+20,0x2c55ac) &&
        bytes(b,MISSILE_DESTRUCT_TAIL+24,tail+24,sizeof(tail)-24);
}
static BOOL enter(void) {
    if(InterlockedCompareExchange(&operation,1,0)) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    return TRUE;
}
static BOOL leave(BOOL okay,DWORD error) {
    InterlockedExchange(&operation,0);
    SetLastError(okay?ERROR_SUCCESS:error);
    return okay;
}
static BOOL fault(DWORD error) {
    InterlockedExchange(&unknown,1);
    return leave(FALSE,error);
}
static BOOL empty(const ProjectileTPtr *p) {
    return !p->object && !p->previous && !p->next;
}
static BOOL links_exact(const ProjectileTPtr *p) {
    if(!p->object) return empty(p);
    uint8_t *target=p->object;
    if(!memory(target,8,TRUE) || p->previous==p || p->next==p) return FALSE;
    ProjectileTPtr *head=*(ProjectileTPtr **)(target+4);
    /* Copy writes the old head's previous link even when the source itself
     * lies deeper in the list. Prove that mutation target separately. */
    if(!memory(head,sizeof(*head),TRUE) || head->object!=target || head->previous)
        return FALSE;
    if(!p->previous) {
        if(head!=p) return FALSE;
    } else if(!memory(p->previous,sizeof(*p),TRUE) ||
        p->previous->object!=target || p->previous->next!=p) return FALSE;
    return !p->next || (memory(p->next,sizeof(*p),TRUE) &&
        p->next->object==target && p->next->previous==p);
}
static BOOL tag_valid(const SudekiMpLanPartyProjectileTag *t) {
    if(!t || !t->session || !t->generation || !t->actor) return FALSE;
    if(t->character==3) return t->item>=12 && t->item<24;
    return t->character==1 && (t->item==24 || t->item==26 || t->item==27 ||
        t->item==30 || t->item==31 || t->item==34 || t->item==35);
}
static BOOL tag_equal(const SudekiMpLanPartyProjectileTag *a,
    const SudekiMpLanPartyProjectileTag *b) {
    return a->session==b->session && a->generation==b->generation &&
        a->sequence==b->sequence && a->character==b->character &&
        a->item==b->item && a->actor==b->actor;
}
static BOOL source_exact(const ProjectileTPtr *source,void *actor) {
    if(!source->object) return empty(source);
    uintptr_t target=(uintptr_t)source->object;
    if(target<4) return FALSE;
    uint8_t *missile=(uint8_t *)(target-4), *entity;
    if(!memory(missile,0x64,FALSE) ||
        *(void **)missile!=image+MISSILE_VT ||
        *(void **)(missile+4)!=image+MISSILE_SECONDARY_VT ||
        *(void **)(missile+0x58)!=actor || !links_exact(source)) return FALSE;
    entity=*(uint8_t **)(missile+0x10);
    return memory(entity,0x100,FALSE) && *(void **)(entity+0xfc)==missile;
}
static void copy_observer(ProjectileTPtr *destination,const ProjectileTPtr *source) {
    uintptr_t a=(uintptr_t)destination,c=(uintptr_t)source;
    void *entry=image+TPTR_COPY;
    /* Retail TPtr copy: EAX destination, ECX source, no stack arguments. */
    __asm__ volatile("call *%[entry]" : "+a"(a), "+c"(c)
        : [entry] "r"(entry) : "edx", "memory", "cc");
}
static BOOL namespace_valid(const SudekiMpLanPartyEffectOwner *owner) {
    return owner && owner->session && owner->generation && owner->actor &&
        (owner->actor_type==0x05 || owner->actor_type==0x0e ||
         owner->actor_type==0x23 || owner->actor_type==0x01);
}
static BOOL namespace_equal(const SudekiMpLanPartyEffectOwner *a,
    const SudekiMpLanPartyEffectOwner *b) {
    return a->session==b->session && a->generation==b->generation &&
        a->actor==b->actor && a->actor_type==b->actor_type;
}
static BOOL poll_locked(void) {
    if(native_thread && native_thread!=GetCurrentThreadId()) {
        SetLastError(ERROR_INVALID_THREAD_ID); return FALSE;
    }
    for(unsigned i=0;i<JOURNAL_CAPACITY;++i) {
        ProjectileEntry *e=&journal[i];
        if(!e->used) continue;
        if(!e->registered) {
            InterlockedExchange(&unknown,1);
            SetLastError(ERROR_INVALID_STATE); return FALSE;
        }
        if(empty(&e->observer)) {
            /* Registered successfully before used entries can reach here.
             * Native destructor cleared this independent observer; manager
             * inactivity, removal from its ten cells or actor death do not. */
            memset(e,0,sizeof(*e));
            InterlockedDecrement(&active_count);
        } else if(e->observer.object!=e->target || !links_exact(&e->observer)) {
            InterlockedExchange(&unknown,1);
            SetLastError(ERROR_INVALID_DATA); return FALSE;
        }
    }
    for(unsigned i=0;i<DESCENDANT_CAPACITY;++i) {
        ProjectileDescendant *e=&descendants[i];
        if(!e->used) continue;
        if(!e->registered) {
            InterlockedExchange(&unknown,1);
            SetLastError(ERROR_INVALID_STATE); return FALSE;
        }
        if(empty(&e->observer)) {
            memset(e,0,sizeof(*e)); InterlockedDecrement(&descendant_count);
        } else if(e->observer.object!=e->target || !links_exact(&e->observer)) {
            InterlockedExchange(&unknown,1);
            SetLastError(ERROR_INVALID_DATA); return FALSE;
        }
    }
    if(InterlockedCompareExchange(&unknown,0,0)) {
        SetLastError(ERROR_INVALID_STATE); return FALSE;
    }
    return TRUE;
}

static BOOL capture_descendant(void *missile,void *entity,
    const SudekiMpLanPartyEffectOwner *owner) {
    if(!enter()) { InterlockedExchange(&unknown,1); return FALSE; }
    if(!image || !namespace_valid(owner) || !memory(missile,0x18,TRUE) ||
        *(void **)missile!=image+MISSILE_VT ||
        *(void **)((uint8_t *)missile+4)!=image+MISSILE_SECONDARY_VT ||
        !memory(entity,0x100,FALSE) || *(void **)entity!=image+0x2d5a10)
        return fault(ERROR_INVALID_DATA);
    if(native_thread && native_thread!=GetCurrentThreadId()) return fault(ERROR_INVALID_THREAD_ID);
    if(!native_thread) native_thread=GetCurrentThreadId();
    if(!poll_locked()) return fault(GetLastError());
    void *target=(uint8_t *)missile+4;
    ProjectileDescendant *free_entry=NULL;
    for(unsigned i=0;i<DESCENDANT_CAPACITY;++i) {
        ProjectileDescendant *e=&descendants[i];
        if(!e->used) { if(!free_entry) free_entry=e; continue; }
        if(e->target==target) {
            if(!namespace_equal(owner,&e->owner) || e->entity!=entity)
                return fault(ERROR_INVALID_DATA);
            return leave(TRUE,0);
        }
    }
    if(!free_entry || next_incarnation==UINT32_MAX) return fault(ERROR_NOT_ENOUGH_MEMORY);
    /* This observer is installed before the native actor-owner/backlink is
     * attached. Attribution comes only from an exact retained execution
     * namespace at the constructor call, never from an active-cast global. */
    ProjectileTPtr *head=*(ProjectileTPtr **)((uint8_t *)target+4);
    if(head && (!links_exact(head) || head->object!=target || head->previous))
        return fault(ERROR_INVALID_DATA);
    free_entry->owner=*owner; free_entry->target=target; free_entry->used=TRUE;
    free_entry->entity=entity; free_entry->incarnation=++next_incarnation;
    InterlockedIncrement(&descendant_count);
    ProjectileTPtr seed={target,NULL,NULL};
    copy_observer(&free_entry->observer,&seed);
    if(free_entry->observer.object!=target || !links_exact(&free_entry->observer))
        return fault(ERROR_INVALID_DATA);
    free_entry->registered=TRUE;
    return leave(TRUE,0);
}
__attribute__((noinline,used)) static void callback_enter(void) {
    InterlockedIncrement(&callbacks);
}
__attribute__((noinline,used)) static void callback_leave(void) {
    InterlockedDecrement(&callbacks);
}
__attribute__((noinline,used)) static void constructed(void *missile,void *entity) {
    SudekiMpLanPartyEffectOwner owner={0};
    if(!SudekiMpLanPartyEffectLifetimeCurrent(&owner)) return;
    if(!image || !namespace_valid(&owner) || !memory(entity,0x42c,FALSE) ||
        *(void **)entity!=image+0x2d5a10 || missile!=(uint8_t *)entity+0x414) {
        InterlockedExchange(&unknown,1); return;
    }
    (void)capture_descendant(missile,entity,&owner);
}
__attribute__((noinline,used)) static BOOL scope_enter(void *missile) {
    InterlockedIncrement(&callbacks);
    /* A scope always masks its caller, including unowned native missiles. */
    if(!native_thread) return FALSE; /* No verified namespace witnessed yet. */
    if(GetCurrentThreadId()!=native_thread) {
        InterlockedExchange(&unknown,1); return FALSE;
    }
    if(scope_depth>=SCOPE_CAPACITY) {
        InterlockedExchange(&unknown,1); ++scope_depth; return TRUE;
    }
    SudekiMpLanPartyEffectOwner *scope=&scopes[scope_depth++];
    memset(scope,0,sizeof(*scope));
    if(!image || !memory(missile,0x18,FALSE) ||
        *(void **)missile!=image+MISSILE_VT) return TRUE;
    void *target=(uint8_t *)missile+4;
    for(unsigned i=0;i<DESCENDANT_CAPACITY;++i) {
        ProjectileDescendant *e=&descendants[i];
        if(!e->used || e->target!=target) continue;
        if(!e->registered || e->observer.object!=target || !links_exact(&e->observer)) {
            InterlockedExchange(&unknown,1); return TRUE;
        }
        *scope=e->owner; return TRUE;
    }
    return TRUE;
}
__attribute__((noinline,used)) static void scope_leave(BOOL entered) {
    if(!entered) {
        InterlockedDecrement(&callbacks); return;
    }
    if(scope_depth) {
        if(scope_depth<=SCOPE_CAPACITY) memset(&scopes[scope_depth-1],0,sizeof(scopes[0]));
        --scope_depth;
    } else InterlockedExchange(&unknown,1);
    InterlockedDecrement(&callbacks);
}
__attribute__((noinline,used)) static BOOL termination_scope_enter(void *missile) {
    BOOL entered=scope_enter(missile);
    if(!entered || native_thread!=GetCurrentThreadId()) return entered;
    void *target=(uint8_t *)missile+4;
    /* Terminate queues native entity deletion and is not idempotent. Every
     * exact direct caller enters here, so a shutdown request can avoid queuing
     * an already witnessed retirement again. This is not a terminal witness. */
    for(unsigned i=0;i<JOURNAL_CAPACITY;++i)
        if(journal[i].used && journal[i].target==target)
            journal[i].termination_entered=TRUE;
    for(unsigned i=0;i<DESCENDANT_CAPACITY;++i)
        if(descendants[i].used && descendants[i].target==target)
            descendants[i].termination_entered=TRUE;
    return entered;
}
__attribute__((noinline,used)) static void termination_scope_returned(void *missile) {
    if(!native_thread || native_thread!=GetCurrentThreadId()) return;
    void *target=(uint8_t *)missile+4;
    for(unsigned i=0;i<DESCENDANT_CAPACITY;++i) {
        ProjectileDescendant *e=&descendants[i];
        if(!e->used || e->target!=target) continue;
        if(!e->registered || !e->termination_entered ||
            (!empty(&e->observer) && (e->observer.object!=target || !links_exact(&e->observer)))) {
            InterlockedExchange(&unknown,1); return;
        }
        e->termination_returned=TRUE;
        return;
    }
}
BOOL SudekiMpLanPartyProjectileLifetimeCurrent(SudekiMpLanPartyEffectOwner *owner) {
    if(!owner || !scope_depth || scope_depth>SCOPE_CAPACITY ||
        (native_thread && GetCurrentThreadId()!=native_thread)) return FALSE;
    *owner=scopes[scope_depth-1];
    return TRUE;
}
__attribute__((naked,noinline)) static void construct_entry(void) {
    __asm__ volatile(
        "pushfl\n\tpushal\n\tcall _callback_enter\n\tpopal\n\tpopfl\n\t"
        "call *_construct_original\n\tpushfl\n\tpushal\n\t"
        "pushl %esi\n\tpushl %eax\n\tcall _constructed\n\taddl $8,%esp\n\t"
        "call _callback_leave\n\tpopal\n\tpopfl\n\tret\n\t");
}
__attribute__((naked,noinline)) static void update_entry(void) {
    /* Original update takes missile and update data on stack, callee ret8. */
    __asm__ volatile(
        "pushl $0\n\tpushfl\n\tpushal\n\tpushl 44(%esp)\n\tcall _scope_enter\n\t"
        "addl $4,%esp\n\tmovl %eax,36(%esp)\n\tpopal\n\tpopfl\n\t"
        "pushl 12(%esp)\n\tpushl 12(%esp)\n\tcall *_update_original\n\t"
        "pushfl\n\tpushal\n\tpushl 36(%esp)\n\tcall _scope_leave\n\taddl $4,%esp\n\t"
        "popal\n\tpopfl\n\tleal 4(%esp),%esp\n\tret $8\n\t");
}
__attribute__((naked,noinline)) static void terminate_entry(void) {
    /* Every verified call passes CMissile in EAX and has no stack argument. */
    __asm__ volatile(
        "pushl %eax\n\tpushl $0\n\tpushfl\n\tpushal\n\tpushl %eax\n\tcall _termination_scope_enter\n\t"
        "addl $4,%esp\n\tmovl %eax,36(%esp)\n\tpopal\n\tpopfl\n\tcall *_terminate_original\n\t"
        "pushfl\n\tpushal\n\tpushl 40(%esp)\n\tcall _termination_scope_returned\n\taddl $4,%esp\n\t"
        "pushl 36(%esp)\n\tcall _scope_leave\n\taddl $4,%esp\n\t"
        "popal\n\tpopfl\n\tleal 8(%esp),%esp\n\tret\n\t");
}

static BOOL hooks_exact(void) {
    if(!image || !construct_hook.installed || !update_hook.installed ||
        !call_target(image+CONSTRUCT_CALL,construct_entry) ||
        !call_target(image+UPDATE_CALL,update_entry)) return FALSE;
    for(unsigned i=0;i<6;++i) if(!terminate_hooks[i].installed ||
        !call_target(image+terminate_calls[i],terminate_entry)) return FALSE;
    return TRUE;
}
static BOOL restore_hooks(void) {
    BOOL okay=TRUE;
    for(unsigned i=6;i--;) if(!SudekiMpRestoreRelativeCallHook(&terminate_hooks[i])) okay=FALSE;
    if(!SudekiMpRestoreRelativeCallHook(&update_hook)) okay=FALSE;
    if(!SudekiMpRestoreRelativeCallHook(&construct_hook)) okay=FALSE;
    InterlockedExchange(&restore_failed,!okay);
    return okay;
}

BOOL SudekiMpLanPartyProjectileLifetimeInstall(HMODULE module) {
    if(!enter()) return FALSE;
    if(image) return leave(image==(uint8_t *)module && !unknown && !restore_failed &&
        image_matches(module) && hooks_exact(),ERROR_INVALID_DATA);
    if(unknown || active_count || descendant_count || callbacks) return leave(FALSE,ERROR_BUSY);
    if(!image_matches(module) || !emission_image_matches((uint8_t *)module))
        return leave(FALSE,ERROR_BAD_EXE_FORMAT);
    image=(uint8_t *)module;
    native_thread=0;
    construct_original=image+CONSTRUCT; update_original=image+UPDATE; terminate_original=image+TERMINATE;
    BOOL okay=SudekiMpInstallRelativeCallHook(&construct_hook,image+CONSTRUCT_CALL,
        construct_original,construct_entry) && SudekiMpInstallRelativeCallHook(
        &update_hook,image+UPDATE_CALL,update_original,update_entry);
    for(unsigned i=0;okay && i<6;++i) okay=SudekiMpInstallRelativeCallHook(
        &terminate_hooks[i],image+terminate_calls[i],terminate_original,terminate_entry);
    if(!okay) {
        DWORD error=GetLastError();
        if(restore_hooks()) image=NULL;
        return leave(FALSE,error);
    }
    return leave(TRUE,0);
}
BOOL SudekiMpLanPartyProjectileLifetimeCapture(
    const SudekiMpLanPartyProjectileTag *tag,void *manager) {
    if(!enter()) { InterlockedExchange(&unknown,1); return FALSE; }
    if(!image || unknown || !tag_valid(tag)) return fault(ERROR_INVALID_STATE);
    if(native_thread && native_thread!=GetCurrentThreadId())
        return fault(ERROR_INVALID_THREAD_ID);
    uint8_t *m=manager, *actor=tag->actor;
    if(!memory(m,SOURCE_OFFSET+SOURCE_COUNT*sizeof(ProjectileTPtr),TRUE) ||
        !memory(actor,0xc0,FALSE) ||
        *(void **)m!=image+MANAGER_VT || *(void **)(m+0x10)!=actor ||
        *(void **)actor!=image+(tag->character==1?ELCO_VT:AILISH_VT) ||
        *(void **)(actor+0xbc)!=m) return fault(ERROR_INVALID_DATA);
    if(!native_thread) native_thread=GetCurrentThreadId();
    if(!poll_locked()) return fault(GetLastError());
    ProjectileTPtr *sources=(ProjectileTPtr *)(m+SOURCE_OFFSET);
    unsigned selected[SOURCE_COUNT], free_count=0, needed=0, observed=0;
    for(unsigned i=0;i<JOURNAL_CAPACITY && free_count<SOURCE_COUNT;++i)
        if(!journal[i].used) selected[free_count++]=i;
    /* Validate the entire launch before adding any observers. Retail c7160
     * clears all ten source cells before allocation; c74e3 sees this emission
     * only. A surviving object under a different tag is therefore unknown. */
    for(unsigned i=0;i<SOURCE_COUNT;++i) {
        ProjectileTPtr *s=&sources[i];
        if(!source_exact(s,actor)) return fault(ERROR_INVALID_DATA);
        if(!s->object) continue;
        ++observed;
        for(unsigned j=0;j<i;++j)
            if(sources[j].object==s->object) return fault(ERROR_INVALID_DATA);
        BOOL already=FALSE;
        for(unsigned j=0;j<JOURNAL_CAPACITY;++j) if(journal[j].used &&
            journal[j].target==s->object) {
            if(!tag_equal(&journal[j].tag,tag)) return fault(ERROR_INVALID_DATA);
            already=TRUE; break;
        }
        if(!already) ++needed;
    }
    if(!observed) return fault(ERROR_NOT_FOUND);
    if(needed>free_count) return fault(ERROR_NOT_ENOUGH_MEMORY);
    unsigned next=0;
    for(unsigned i=0;i<SOURCE_COUNT;++i) if(sources[i].object) {
        BOOL already=FALSE;
        for(unsigned j=0;j<JOURNAL_CAPACITY;++j)
            if(journal[j].used && journal[j].target==sources[i].object) {
                already=TRUE; break;
            }
        if(already) continue; /* Exact same tag/source, already proven above. */
        ProjectileEntry *e=&journal[selected[next++]];
        e->tag=*tag; e->target=sources[i].object; e->used=TRUE;
        InterlockedIncrement(&active_count); /* obligation before native call */
        copy_observer(&e->observer,&sources[i]);
        if(e->observer.object!=e->target || !links_exact(&e->observer))
            return fault(ERROR_INVALID_DATA);
        e->registered=TRUE;
    }
    return leave(TRUE,0);
}
BOOL SudekiMpLanPartyProjectileLifetimeCaptureOwned(
    const SudekiMpLanPartyProjectileTag *tag,
    const SudekiMpLanPartyEffectOwner *owner,void *manager) {
    if(!tag_valid(tag) || !namespace_valid(owner) || owner->actor!=tag->actor ||
        owner->actor_type!=(tag->character==1?0x0e:0x01)) {
        InterlockedExchange(&unknown,1); SetLastError(ERROR_INVALID_DATA); return FALSE;
    }
    if(!SudekiMpLanPartyProjectileLifetimeCapture(tag,manager)) return FALSE;
    /* Capture above has proved this same native thread, manager/actor pair,
     * full ten-cell launch and exact emission tag. The new independent nodes
     * retain a separate native namespace; never replace the ordinary tag. */
    ProjectileTPtr *sources=(ProjectileTPtr *)((uint8_t *)manager+SOURCE_OFFSET);
    for(unsigned i=0;i<SOURCE_COUNT;++i) {
        if(!source_exact(&sources[i],tag->actor)) {
            InterlockedExchange(&unknown,1); SetLastError(ERROR_INVALID_DATA); return FALSE;
        }
        if(sources[i].object) {
            uint8_t *missile=(uint8_t *)sources[i].object-4;
            if(!capture_descendant(missile,*(void **)(missile+0x10),owner)) return FALSE;
        }
    }
    return TRUE;
}
BOOL SudekiMpLanPartyProjectileLifetimePoll(void) {
    if(!enter()) return FALSE;
    if(callbacks || scope_depth) return leave(FALSE,ERROR_BUSY);
    BOOL okay=poll_locked(); DWORD error=GetLastError();
    return leave(okay,error);
}
BOOL SudekiMpLanPartyProjectileLifetimeObserve(
    SudekiMpLanPartyProjectileObservation *out,unsigned capacity,unsigned *count) {
    if(!count || (capacity && !out) || !enter()) return FALSE;
    if(!image || unknown || restore_failed || callbacks || scope_depth || !hooks_exact())
        return leave(FALSE,ERROR_INVALID_STATE);
    if(native_thread && native_thread!=GetCurrentThreadId())
        return leave(FALSE,ERROR_INVALID_THREAD_ID);
    unsigned needed=0;
    for(unsigned i=0;i<JOURNAL_CAPACITY;++i) {
        const ProjectileEntry *e=&journal[i];
        if(!e->used) continue;
        if(!e->registered) return fault(ERROR_INVALID_DATA);
        if(empty(&e->observer)) continue;
        if(e->observer.object!=e->target || !links_exact(&e->observer))
            return fault(ERROR_INVALID_DATA);
        BOOL owned=FALSE;
        for(unsigned j=0;j<DESCENDANT_CAPACITY;++j)
            if(descendants[j].used && descendants[j].registered &&
                descendants[j].target==e->target) { owned=TRUE; break; }
        if(!owned) return leave(FALSE,ERROR_INVALID_STATE);
    }
    /* First prove every retained cell, then publish. A destructor-cleared
     * cell needs no native dereference and is not a live registry member. */
    for(unsigned i=0;i<DESCENDANT_CAPACITY;++i) {
        const ProjectileDescendant *e=&descendants[i];
        if(!e->used) continue;
        if(!e->registered || !e->incarnation || !namespace_valid(&e->owner))
            return fault(ERROR_INVALID_DATA);
        if(empty(&e->observer)) continue;
        uint8_t *missile=(uint8_t *)e->target-4,*entity=e->entity;
        if(e->observer.object!=e->target || !links_exact(&e->observer) ||
            !memory(missile,0x18,FALSE) || *(void **)missile!=image+MISSILE_VT ||
            *(void **)(missile+4)!=image+MISSILE_SECONDARY_VT ||
            *(void **)(missile+0x10)!=entity || !memory(entity,0x100,FALSE) ||
            *(void **)entity!=image+0x2d5a10 || *(void **)(entity+0xfc)!=missile ||
            (e->termination_returned && !e->termination_entered)) return fault(ERROR_INVALID_DATA);
        ++needed;
    }
    if(needed>capacity) return leave(FALSE,ERROR_INSUFFICIENT_BUFFER);
    unsigned n=0;
    for(unsigned i=0;i<DESCENDANT_CAPACITY;++i) {
        const ProjectileDescendant *e=&descendants[i];
        if(!e->used || empty(&e->observer)) continue;
        out[n++]=(SudekiMpLanPartyProjectileObservation){e->owner,e->incarnation,
            (uint8_t *)e->target-4,e->entity,e->termination_entered,e->termination_returned};
    }
    *count=n;
    return leave(TRUE,0);
}
BOOL SudekiMpLanPartyProjectileLifetimeRequestRetire(void) {
    if(!enter()) return FALSE;
    if(callbacks || scope_depth || restore_failed) return leave(FALSE,ERROR_BUSY);
    if(!poll_locked()) return leave(FALSE,GetLastError());
    if(!active_count && !descendant_count) return leave(TRUE,0);
    if(!image || !native_thread || native_thread!=GetCurrentThreadId() || !hooks_exact())
        return fault(ERROR_INVALID_STATE);
    /* Every production ordinary shot must also carry a native namespace;
     * otherwise shutdown could create unowned impact descendants. */
    for(unsigned i=0;i<JOURNAL_CAPACITY;++i) if(journal[i].used) {
        BOOL owned=FALSE;
        for(unsigned j=0;j<DESCENDANT_CAPACITY;++j)
            if(descendants[j].used && descendants[j].target==journal[i].target &&
                namespace_valid(&descendants[j].owner)) { owned=TRUE; break; }
        if(!owned) return fault(ERROR_INVALID_STATE);
    }
    /* Native cleanup may itself emit descendants. Do not hold the capture
     * operation lock across it; a callback obligation prevents teardown. */
    InterlockedIncrement(&callbacks);
    leave(TRUE,0);
    BOOL okay=TRUE;
    DWORD error=ERROR_SUCCESS;
    for(unsigned i=0;i<DESCENDANT_CAPACITY;++i) {
        ProjectileDescendant *e=&descendants[i];
        if(!e->used || empty(&e->observer) || e->termination_entered) continue;
        uint8_t *missile=(uint8_t *)e->target-4;
        uint8_t *entity=NULL;
        if(!e->registered || e->observer.object!=e->target || !links_exact(&e->observer) ||
            !namespace_valid(&e->owner) || !memory(missile,0xa4,TRUE) ||
            *(void **)missile!=image+MISSILE_VT ||
            *(void **)(missile+4)!=image+MISSILE_SECONDARY_VT ||
            !memory(entity=*(uint8_t **)(missile+0x10),0x100,TRUE) ||
            *(void **)entity!=image+0x2d5a10 || *(void **)(entity+0xfc)!=missile) {
            InterlockedExchange(&unknown,1); okay=FALSE; error=ERROR_INVALID_DATA; break;
        }
        /* The wrapper records entry before native code runs and provides the
         * same namespace scope used for ordinary collision termination. */
        uintptr_t argument=(uintptr_t)missile;
        void *entry=terminate_entry;
        __asm__ volatile("call *%[entry]" : "+a"(argument)
            : [entry] "r"(entry) : "ecx","edx","memory","cc");
        if(InterlockedCompareExchange(&unknown,0,0)) {
            okay=FALSE; error=ERROR_INVALID_STATE; break;
        }
    }
    InterlockedDecrement(&callbacks);
    SetLastError(error);
    return okay;
}
BOOL SudekiMpLanPartyProjectileLifetimeRetains(void) {
    return InterlockedCompareExchange(&operation,0,0)!=0 ||
        InterlockedCompareExchange(&active_count,0,0)!=0 ||
        InterlockedCompareExchange(&descendant_count,0,0)!=0 ||
        InterlockedCompareExchange(&callbacks,0,0)!=0 ||
        InterlockedCompareExchange(&restore_failed,0,0)!=0 ||
        InterlockedCompareExchange(&unknown,0,0)!=0;
}
BOOL SudekiMpLanPartyProjectileLifetimeUninstall(void) {
    if(!enter()) return FALSE;
    if(unknown || active_count || descendant_count || callbacks || scope_depth)
        return leave(FALSE,ERROR_BUSY);
    if(!restore_hooks()) return leave(FALSE,GetLastError());
    image=NULL; native_thread=0;
    construct_original=update_original=terminate_original=NULL;
    return leave(TRUE,0);
}
