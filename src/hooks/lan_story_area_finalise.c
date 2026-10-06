#include "hooks/lan_story_area_finalise.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include <string.h>
#include <limits.h>
#if !defined(__GNUC__) || !defined(__i386__)
#error "Area finalisation requires the supported x86 ABI"
#endif
enum { ENQUEUE=0x3cd66,ENQUEUE_NATIVE=0x131fa0,TASK_VT=0x2c89a8,
    RUN=0x14a640,DESTROY=0x3c7e0,UPDATE_SLOT=0x2c824c,UPDATE=0x3e250,
    PVS_BEGIN_SITE=0x109fc7,PVS_END_SITE=0x109fee,NODE_PUBLISH_SITE=0x20bab3,
    NODE_FREE_SITE=0x20a1e5,NODE_FREE=0x24844f };
enum { RETIRE_COPY_SITE=0x107ab6,RETIRE_COPY=0x10ddb0,
    RETIRE_ENQUEUE_SITE=0x107b11,RETIRE_ENQUEUE=0x48b0,
    RETIRE_DELETE_SLOT=0x2cde70,RETIRE_DELETE=0x1079c0 };
enum { COPY_BEGIN,COPY_END,RETIRE_SUBMIT_BEGIN,RETIRE_SUBMIT_END,RETIRE_DELETE_BEGIN,RETIRE_DELETE_END };
enum { SUBMIT_BEGIN,SUBMIT_END,RUN_BEGIN,RUN_END,DELETE_BEGIN,DELETE_END,
    UPDATE_BEGIN,UPDATE_END };
enum { PVS_BEGIN,PVS_END,NODE_PUBLISH,NODE_FREE_BEGIN,NODE_FREE_END };
enum { RESOURCE_CONSTRUCT_SITE=0x3cc4d,RESOURCE_CONSTRUCT=0x107430,
    RESOURCE_BODY_SITE=0x1076b3,RESOURCE_BODY=0x107a10 };
enum { RESOURCE_BEGIN,RESOURCE_END,RESOURCE_DESTROY_BEGIN,RESOURCE_DESTROY_END };
typedef SudekiMpLanStoryAreaResourceReceipt Resource;
static Resource resources[SUDEKIMP_AREA_FINALISERS];
static uint64_t resource_serial;
static SudekiMpRelativeCallHook resource_construct_hook,resource_body_hook;
static void *resource_construct_original __attribute__((used)),*resource_body_original __attribute__((used));
typedef SudekiMpLanStoryAreaFinaliseReceipt Record;
static Record records[SUDEKIMP_AREA_FINALISERS];
typedef SudekiMpLanStoryAreaRetirementReceipt Retirement;
static Retirement retirements[SUDEKIMP_AREA_FINALISERS];
static unsigned retirement_depth[SUDEKIMP_AREA_FINALISERS];
static uint64_t retirement_serial;
static SudekiMpRelativeCallHook retire_copy_hook,retire_enqueue_hook;
static SudekiMpPointerHook retire_delete_hook;
static void *retire_copy_original __attribute__((used)),*retire_enqueue_original __attribute__((used));
static void *retire_delete_original __attribute__((used));
static SRWLOCK lock=SRWLOCK_INIT;
static uint8_t *base;
static const void *consumer;
static uintptr_t manager;
static uint64_t serial;
static DWORD startup_thread,game_thread;
static unsigned callbacks,dispatch_depth;
static BOOL installed,stopping,unknown,ever_native;
static SudekiMpRelativeCallHook enqueue_hook;
static SudekiMpPointerHook run_hook,delete_hook,update_hook;
static SudekiMpInlineHook pvs_begin_hook,pvs_end_hook,node_publish_hook;
static SudekiMpRelativeCallHook node_free_hook;
static void *pvs_begin_original __attribute__((used)),*pvs_end_original __attribute__((used));
static void *node_publish_original __attribute__((used)),*node_free_original __attribute__((used));
static void *enqueue_original __attribute__((used)),*run_original __attribute__((used));
static void *delete_original __attribute__((used)),*update_original __attribute__((used));
static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m;uintptr_t a=(uintptr_t)p;
    if(!p || !n || a>UINTPTR_MAX-n || VirtualQuery(p,&m,sizeof(m))!=sizeof(m) ||
        m.State!=MEM_COMMIT || (m.Protect&(PAGE_NOACCESS|PAGE_GUARD)) ||
        a+n>(uintptr_t)m.BaseAddress+m.RegionSize) return FALSE;
    DWORD access=m.Protect&0xffu;
    return access==PAGE_READONLY || access==PAGE_READWRITE || access==PAGE_WRITECOPY ||
        access==PAGE_EXECUTE_READ || access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY;
}
static BOOL bytes(uint8_t *b,unsigned rva,const void *p,size_t n) {
    return readable(b+rva,n) && !memcmp(b+rva,p,n);
}
static void relocate(uint8_t *p,uint8_t *b,unsigned rva) {
    uint32_t value=(uint32_t)(uintptr_t)(b+rva);memcpy(p,&value,4);
}
static BOOL call_exact(uint8_t *b,unsigned site,unsigned target) {
    int32_t displacement;
    if(!readable(b+site,5) || b[site]!=0xe8) return FALSE;
    memcpy(&displacement,b+site+1,4);return b+site+5+displacement==b+target;
}
static BOOL startup_exact(uint8_t *b) {
    return readable(b+0x408d10,4) && !*(void **)(b+0x408d10) &&
        readable(b+0x409d8c,4) && !*(void **)(b+0x409d8c) &&
        readable(b+0x324004,8) && *(HANDLE *)(b+0x324004)==INVALID_HANDLE_VALUE &&
        *(HANDLE *)(b+0x324008)==INVALID_HANDLE_VALUE;
}
static BOOL image_exact(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    if(!readable(b,sizeof(IMAGE_DOS_HEADER))) return FALSE;
    IMAGE_DOS_HEADER *dos=(void *)b;
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 ||
        (uint32_t)dos->e_lfanew>SUDEKIMP_EXPECTED_IMAGE_SIZE-sizeof(IMAGE_NT_HEADERS32) ||
        !readable(b+dos->e_lfanew,sizeof(IMAGE_NT_HEADERS32)) || !SudekiMpCheckLoadedExecutable(image)) return FALSE;
    uint8_t factory[]={0xc7,0,0,0,0,0,0x89,0x78,0x10,0xeb,2,0x33,0xc0,
        0x8b,0x15,0,0,0,0,0x52,0x8b,0xf0,0x8b,0xfb};
    relocate(factory+2,b,TASK_VT);relocate(factory+15,b,0x409d8c);
    static const uint8_t run[]={0x56,0x8b,0xf1,0x8b,0x4e,0x10,0x8b,0x41,4,
        0x8b,0x50,0x2c,0x83,0xc1,4,0xff,0xd2,0x8b,0x46,0x10,
        0x80,0xb8,0x28,1,0,0,5,0x5e,0x0f,0x94,0xc0,0xc3};
    uint8_t destruction[]={0xf6,0x44,0x24,4,1,0x56,0x8b,0xf1,0xc7,6,0,0,0,0,
        0x74,9,0x56,0xe8,0x59,0xbc,0x20,0,0x83,0xc4,4,0x8b,0xc6,0x5e,0xc2,4,0};
    relocate(destruction+10,b,0x2c55f4);
    uint8_t owner[]={0xc7,0x47,4,0,0,0,0};relocate(owner+3,b,0x2c8248);
    static const uint8_t dispatch[]={0x8b,0x16,0x8b,0x42,4,0x8b,0xce,0xff,0xd0};
    static const uint8_t ret4[]={0xc2,4,0};
    static const uint8_t pvs_submit[]={0x8b,0x16,0x8b,0x52,0x7c,0x55,0x8d,0x45,0x48,0x50,
        0x8d,0x4c,0x24,0x48,0x51,0x8b,0xce,0xff,0xd2,0xeb,0x12};
    static const uint8_t pvs_tail[]={0x33,0xc0,0x89,0x45,0x30,0x89,0x45,0x2c};
    uint8_t publish[]={0x89,0x50,4,0x89,0x48,8,0xa3,0,0,0,0,0x85,0xff};
    relocate(publish+7,b,0x3c370c);
    uint8_t dispatch_node[]={0x8b,0x46,8,0x8b,0x0e,0x8b,0x11,0x8b,0x12,
        0xa3,0,0,0,0,0x8b,0x46,4,0x50,0xff,0xd2,0x56};
    relocate(dispatch_node+10,b,0x3c370c);
    static const uint8_t copy_args[]={0x8d,0x83,0x1c,1,0,0,0x50,0x8d,0x4d,4,0x51};
    static const uint8_t enqueue_args[]={0x8d,0x4c,0x24,0x14,0x51,0x8d,0x54,0x24,0x1c,0x52,
        0x8d,0x86,0x5c,2,0,0};
    static const uint8_t delete_dispatch[]={0x8b,0x11,0x8b,0x42,8,0x6a,1,0xff,0xd0,0xeb,0x4b};
    static const uint8_t erase_args[]={0x8b,0x4f,0xc,0x8b,0xc6,0x8d,0x14,0x81,0x57,
        0x8d,0x44,0x24,0x28,0x89,0x7c,0x24,0x24,0x46};
    uint8_t remover_destroy[]={0x56,0x57,0x8b,0xf9,0x8d,0x77,4,0xc7,7,0,0,0,0};
    relocate(remover_destroy+9,b,RETIRE_DELETE_SLOT-8);
    uint8_t remover_construct[]={0xc7,0,0,0,0,0,0x89,0x78,4,0x89,0x78,8,0x89,0x78,0xc,
        0x89,0x78,0x10,0x8b,0xe8,0xeb,2,0x33,0xed};
    relocate(remover_construct+2,b,RETIRE_DELETE_SLOT-8);
    static const uint8_t ret8[]={0xc2,8,0};
    static const uint8_t construct_args[]={0x50,0xe8,0xde,0xa7,0xc,0,0x8b,0xf0};
    static const uint8_t construct_return[]={0xc2,0x10,0};
    static const uint8_t body_wrapper[]={0x56,0x8b,0xf1};
    static const uint8_t body_tail[]={0xf6,0x44,0x24,8,1,0x74,9,0x56};
    static const uint8_t construct_entry_bytes[]={0x55,0x8b,0xec,0x83,0xe4,0xf8,0x81,0xec,0x34,1,0,0,
        0x53,0x8b,0x5d,8};
    static const uint8_t body_entry_bytes[]={0x55,0x8b,0xec,0x83,0xe4,0xf8,0x83,0xec,0x10,
        0x53,0x55,0x8b,0xd9};
    return startup_exact(b) && bytes(b,0x3cd4e,factory,sizeof(factory)) &&
        bytes(b,RESOURCE_CONSTRUCT_SITE-1,construct_args,sizeof(construct_args)) &&
        call_exact(b,RESOURCE_CONSTRUCT_SITE,RESOURCE_CONSTRUCT) &&
        bytes(b,RESOURCE_CONSTRUCT,construct_entry_bytes,sizeof(construct_entry_bytes)) &&
        bytes(b,RESOURCE_BODY,body_entry_bytes,sizeof(body_entry_bytes)) &&
        bytes(b,0x1076ab,construct_return,sizeof(construct_return)) &&
        bytes(b,RESOURCE_BODY_SITE-3,body_wrapper,sizeof(body_wrapper)) &&
        call_exact(b,RESOURCE_BODY_SITE,RESOURCE_BODY) && bytes(b,RESOURCE_BODY_SITE+5,body_tail,sizeof(body_tail)) &&
        bytes(b,0x107aab,copy_args,sizeof(copy_args)) && call_exact(b,RETIRE_COPY_SITE,RETIRE_COPY) &&
        bytes(b,0x107b01,enqueue_args,sizeof(enqueue_args)) && call_exact(b,RETIRE_ENQUEUE_SITE,RETIRE_ENQUEUE) &&
        bytes(b,0x3e3e6,delete_dispatch,sizeof(delete_dispatch)) && bytes(b,0x3e43c,erase_args,sizeof(erase_args)) &&
        call_exact(b,0x3e44e,0x38e80) && bytes(b,0x107a07,ret4,sizeof(ret4)) &&
        bytes(b,RETIRE_DELETE,remover_destroy,sizeof(remover_destroy)) &&
        bytes(b,0x107a93,remover_construct,sizeof(remover_construct)) &&
        bytes(b,0x10de19,ret8,sizeof(ret8)) && bytes(b,0x497e,ret8,sizeof(ret8)) &&
        readable(b+RETIRE_DELETE_SLOT,4) && *(void **)(b+RETIRE_DELETE_SLOT)==b+RETIRE_DELETE &&
        readable(b+0x2cde80,8) && *(void **)(b+0x2cde80)==b+0x107970 && *(void **)(b+0x2cde84)==b+0x52c50 &&
        readable(b+0x3c370c,4) && !*(void **)(b+0x3c370c) &&
        bytes(b,PVS_BEGIN_SITE,pvs_submit,sizeof(pvs_submit)) && bytes(b,PVS_END_SITE,pvs_tail,sizeof(pvs_tail)) &&
        bytes(b,NODE_PUBLISH_SITE-6,publish,sizeof(publish)) &&
        bytes(b,0x20a1d0,dispatch_node,sizeof(dispatch_node)) && call_exact(b,NODE_FREE_SITE,NODE_FREE) &&
        call_exact(b,ENQUEUE,ENQUEUE_NATIVE) && bytes(b,RUN,run,sizeof(run)) &&
        bytes(b,DESTROY,destruction,sizeof(destruction)) && bytes(b,0x3cf55,owner,sizeof(owner)) &&
        bytes(b,0x3e2f7,dispatch,sizeof(dispatch)) && bytes(b,0x3e481,ret4,sizeof(ret4)) &&
        bytes(b,0x132022,ret4,sizeof(ret4)) &&
        readable(b+TASK_VT,8) && *(void **)(b+TASK_VT)==b+DESTROY &&
        *(void **)(b+TASK_VT+4)==b+RUN && readable(b+UPDATE_SLOT,4) && *(void **)(b+UPDATE_SLOT)==b+UPDATE;
}
static BOOL hooks_exact(void) {
    int32_t displacement;
    if(!installed || !enqueue_hook.installed || !run_hook.installed || !delete_hook.installed ||
        !update_hook.installed || !readable(base+ENQUEUE,5) || base[ENQUEUE]!=0xe8) return FALSE;
    const SudekiMpRelativeCallHook *retire_calls[]={&retire_copy_hook,&retire_enqueue_hook,&resource_construct_hook,&resource_body_hook};
    for(unsigned i=0;i<4;++i) {
        const SudekiMpRelativeCallHook *h=retire_calls[i];int32_t d;
        if(!h->installed || !readable(h->instruction,5) || h->instruction[0]!=0xe8) return FALSE;
        memcpy(&d,h->instruction+1,4);if(d!=h->replacement_displacement) return FALSE;
    }
    if(!retire_delete_hook.installed || *(void **)retire_delete_hook.slot!=retire_delete_hook.replacement_value) return FALSE;
    memcpy(&displacement,base+ENQUEUE+1,4);
    int32_t free_displacement=0;
    if(!node_free_hook.installed || !readable(base+NODE_FREE_SITE,5) || base[NODE_FREE_SITE]!=0xe8) return FALSE;
    memcpy(&free_displacement,base+NODE_FREE_SITE+1,4);
    const SudekiMpInlineHook *inlines[]={&pvs_begin_hook,&pvs_end_hook,&node_publish_hook};
    for(unsigned i=0;i<3;++i) if(!inlines[i]->installed || !readable(inlines[i]->target,inlines[i]->length) ||
        memcmp(inlines[i]->target,inlines[i]->replacement,inlines[i]->length)) return FALSE;
    return free_displacement==node_free_hook.replacement_displacement && displacement==enqueue_hook.replacement_displacement &&
        *(void **)run_hook.slot==run_hook.replacement_value &&
        *(void **)delete_hook.slot==delete_hook.replacement_value &&
        *(void **)update_hook.slot==update_hook.replacement_value;
}
static Record *by_task(uintptr_t task,BOOL submitting) {
    Record *found=NULL;
    for(unsigned i=0;i<SUDEKIMP_AREA_FINALISERS;++i) if(records[i].ticket && records[i].task==task &&
        (submitting?records[i].submitting:!records[i].destroyed)) {
        if(found) {unknown=TRUE;return NULL;}found=&records[i];
    }
    return found;
}
static Resource *resource_phase(uintptr_t address,unsigned phase) {
    Resource *found=NULL;
    for(unsigned i=0;i<SUDEKIMP_AREA_FINALISERS;++i) if(resources[i].generation &&
        resources[i].resource==address && resources[i].phase==phase) {
        if(found) {unknown=TRUE;return NULL;}found=&resources[i];
    }
    return found;
}
/* Both native construction and destruction-body callbacks use only copied
 * identities. A worker may construct while the game thread later destroys;
 * each individual entry/return must nevertheless be on its original thread. */
static void __attribute__((used,noinline)) resource_event(unsigned kind,uintptr_t address,uintptr_t result) {
    DWORD saved=GetLastError(),thread=GetCurrentThreadId();AcquireSRWLockExclusive(&lock);
    ever_native=TRUE;
    if(!(kind&1)) {if(callbacks==UINT_MAX) unknown=TRUE;else ++callbacks;}
    if(!installed) unknown=TRUE;
    if(!unknown) {
        Resource *r=NULL;
        if(kind==RESOURCE_BEGIN) {
            Resource *vacant=NULL;
            for(unsigned i=0;i<SUDEKIMP_AREA_FINALISERS;++i) {
                if(!resources[i].generation && !vacant) vacant=&resources[i];
                if(resources[i].generation && resources[i].resource==address &&
                    resources[i].phase!=SUDEKIMP_AREA_RESOURCE_BODY_RETURNED) unknown=TRUE;
            }
            if(!address || address>UINTPTR_MAX-0x12c || !vacant || resource_serial==UINT64_MAX) unknown=TRUE;
            if(!unknown) *vacant=(Resource){.generation=++resource_serial,.resource=address,
                .phase=SUDEKIMP_AREA_RESOURCE_CONSTRUCTING,.callback_thread=thread};
        } else if(kind==RESOURCE_END) {
            r=resource_phase(address,SUDEKIMP_AREA_RESOURCE_CONSTRUCTING);
            if(!r || r->callback_thread!=thread || result!=address) unknown=TRUE;
            else {r->phase=SUDEKIMP_AREA_RESOURCE_LIVE;r->callback_thread=0;}
        } else if(kind==RESOURCE_DESTROY_BEGIN) {
            r=resource_phase(address,SUDEKIMP_AREA_RESOURCE_LIVE);
            if(!r) unknown=TRUE;
            else {r->phase=SUDEKIMP_AREA_RESOURCE_DESTROYING;r->callback_thread=thread;}
        } else if(kind==RESOURCE_DESTROY_END) {
            r=resource_phase(address,SUDEKIMP_AREA_RESOURCE_DESTROYING);
            if(!r || r->callback_thread!=thread) unknown=TRUE;
            else {r->phase=SUDEKIMP_AREA_RESOURCE_BODY_RETURNED;r->callback_thread=0;}
        } else unknown=TRUE;
    }
    if(kind&1) {if(callbacks) --callbacks;else unknown=TRUE;}
    ReleaseSRWLockExclusive(&lock);SetLastError(saved);
}
/* Called with register copies; no native object is ever dereferenced here.
 * In particular DELETE_END never reads the already-freed finaliser. */
static void __attribute__((used,noinline)) event(unsigned kind,uintptr_t task,uintptr_t resource,uintptr_t value) {
    DWORD saved=GetLastError(),thread=GetCurrentThreadId();AcquireSRWLockExclusive(&lock);
    ever_native=TRUE;
    if(!(kind&1)) {if(callbacks==UINT_MAX) unknown=TRUE;else ++callbacks;}
    if(!unknown && installed) {
        Record *r=NULL;
        if(kind==SUBMIT_BEGIN) {
            Resource *owner=resource>=4?resource_phase(resource-4,SUDEKIMP_AREA_RESOURCE_LIVE):NULL;
            Record *vacant=NULL;
            for(unsigned i=0;i<SUDEKIMP_AREA_FINALISERS;++i) if(!records[i].ticket) {vacant=&records[i];break;}
            if(!owner || !task || !resource || !value || (manager && manager!=value) || !vacant ||
                serial==UINT64_MAX || by_task(task,FALSE) || by_task(task,TRUE)) unknown=TRUE;
            else {manager=value;*vacant=(Record){.ticket=++serial,.task=task,.resource=resource,
                .resource_generation=owner->generation,.manager=value,.submitting=TRUE};}
        } else if(kind==SUBMIT_END) {
            r=by_task(task,TRUE);if(!r) unknown=TRUE;else r->submitting=FALSE;
        } else if(kind==UPDATE_BEGIN) {
            if(!task || (manager && manager!=task) || (game_thread && game_thread!=thread) ||
                dispatch_depth==UINT_MAX) unknown=TRUE;
            else {manager=task;game_thread=thread;++dispatch_depth;}
        } else if(kind==UPDATE_END) {
            if(!dispatch_depth || game_thread!=thread || manager!=task) unknown=TRUE;
            else {
                for(unsigned i=0;i<SUDEKIMP_AREA_FINALISERS;++i)
                    if(retirements[i].ticket && retirements[i].destroyed && !retirements[i].drained &&
                        retirement_depth[i]==dispatch_depth) retirements[i].drained=TRUE;
                --dispatch_depth;
            }
        } else {
            r=by_task(task,FALSE);
            if(!r) unknown=TRUE;
            else if(kind==RUN_BEGIN) {
                Resource *owner=r->resource>=4?resource_phase(r->resource-4,SUDEKIMP_AREA_RESOURCE_LIVE):NULL;
                if(!owner || owner->generation!=r->resource_generation || !dispatch_depth || game_thread!=thread || r->running || r->destroying ||
                    r->terminal_result || r->runs==UINT32_MAX) unknown=TRUE;
                else {r->running=TRUE;++r->runs;}
            } else if(kind==RUN_END) {
                if(!dispatch_depth || game_thread!=thread || !r->running || r->destroying) unknown=TRUE;
                else {r->running=FALSE;r->terminal_result=(value&0xffu)!=0;}
            } else if(kind==DELETE_BEGIN) {
                if(value!=1 || r->running || r->destroying) unknown=TRUE;else r->destroying=TRUE;
            } else if(kind==DELETE_END) {
                if(value!=1 || !r->destroying || r->running) unknown=TRUE;
                else {r->destroying=FALSE;r->destroyed=TRUE;}
            } else unknown=TRUE;
        }
    } else if(!installed) unknown=TRUE;
    if(kind&1) {if(callbacks) --callbacks;else unknown=TRUE;}
    ReleaseSRWLockExclusive(&lock);SetLastError(saved);
}
static Record *running_resource(uintptr_t resource) {
    Record *found=NULL;
    for(unsigned i=0;i<SUDEKIMP_AREA_FINALISERS;++i) if(records[i].ticket && records[i].running &&
        records[i].resource==resource) {if(found) {unknown=TRUE;return NULL;}found=&records[i];}
    return found;
}
static Record *pending_node(uintptr_t node) {
    Record *found=NULL;
    for(unsigned i=0;i<SUDEKIMP_AREA_FINALISERS;++i) if(records[i].ticket && records[i].pvs_node==node &&
        !records[i].pvs_retired) {if(found) {unknown=TRUE;return NULL;}found=&records[i];}
    return found;
}
/* All arguments are register/stack copies. In particular NODE_FREE_END never
 * reads the freed queue node. Unrelated/reload nodes cannot discharge a load
 * merely because their callback object or descriptor matches it. */
static void __attribute__((used,noinline)) pvs_event(unsigned kind,uintptr_t identity,
    uintptr_t resource,uintptr_t argument) {
    DWORD saved=GetLastError(),thread=GetCurrentThreadId();AcquireSRWLockExclusive(&lock);
    ever_native=TRUE;
    if(kind==NODE_FREE_BEGIN) {if(callbacks==UINT_MAX) unknown=TRUE;else ++callbacks;}
    if(!unknown && installed) {
        Record *r=NULL;
        if(kind==PVS_BEGIN || kind==PVS_END) {
            r=running_resource(resource);
            if(!r || !dispatch_depth || game_thread!=thread || !identity || identity>UINTPTR_MAX-0x48) unknown=TRUE;
            else if(kind==PVS_BEGIN) {
                if(r->pvs_descriptor || r->pvs_submitting) unknown=TRUE;
                else {r->pvs_descriptor=identity;r->pvs_submitting=TRUE;}
            } else if(r->pvs_submitting) {
                if(r->pvs_descriptor!=identity || !r->pvs_node) unknown=TRUE;
                else r->pvs_submitting=FALSE;
            } /* Same tail is reached without PVS; native Run owns that init. */
        } else if(kind==NODE_PUBLISH) {
            if(!identity) unknown=TRUE;
            else {
                for(unsigned i=0;i<SUDEKIMP_AREA_FINALISERS;++i) if(records[i].ticket && records[i].pvs_submitting &&
                    records[i].pvs_descriptor==argument && resource==argument+0x48) {
                    if(r) {unknown=TRUE;break;}r=&records[i];
                }
                if(r) {
                    if(game_thread!=thread || !r->running || r->pvs_node || pending_node(identity)) unknown=TRUE;
                    else r->pvs_node=identity;
                } else if(pending_node(identity)) unknown=TRUE;
            }
        } else if(kind==NODE_FREE_BEGIN || kind==NODE_FREE_END) {
            if(identity) r=pending_node(identity);
            if(r) {
                if(game_thread!=thread || (kind==NODE_FREE_BEGIN?r->pvs_retiring:!r->pvs_retiring)) unknown=TRUE;
                else if(kind==NODE_FREE_BEGIN) r->pvs_retiring=TRUE;
                else {r->pvs_retiring=FALSE;r->pvs_retired=TRUE;}
            }
        } else unknown=TRUE;
    } else if(!installed) unknown=TRUE;
    if(kind==NODE_FREE_END) {if(callbacks) --callbacks;else unknown=TRUE;}
    ReleaseSRWLockExclusive(&lock);SetLastError(saved);
}
/* Register/stack copies only. No object dereference, readiness invocation,
 * native cancellation or area-policy release occurs in this callback. */
static void __attribute__((used,noinline)) retirement_event(unsigned kind,uintptr_t remover,
    uintptr_t resource,uintptr_t owner,uintptr_t detail,uintptr_t context) {
    DWORD saved=GetLastError(),thread=GetCurrentThreadId();AcquireSRWLockExclusive(&lock);
    ever_native=TRUE;
    if(!(kind&1)) {if(callbacks==UINT_MAX) unknown=TRUE;else ++callbacks;}
    if(!installed || (game_thread && game_thread!=thread)) unknown=TRUE;
    if(!unknown) {
        Retirement *r=NULL,*vacant=NULL;unsigned index=0;
        game_thread=thread;
        for(unsigned i=0;i<SUDEKIMP_AREA_FINALISERS;++i) {
            if(!retirements[i].ticket && !vacant) vacant=&retirements[i];
            if(retirements[i].ticket && retirements[i].remover==remover && !retirements[i].drained) {
                if(r) unknown=TRUE;
                r=&retirements[i];index=i;
            }
        }
        if(kind==COPY_BEGIN) {
            Resource *owner_record=resource_phase(resource,SUDEKIMP_AREA_RESOURCE_DESTROYING);
            if(!owner_record || r || !vacant || retirement_serial==UINT64_MAX || !remover || !resource ||
                remover>UINTPTR_MAX-4 || resource>UINTPTR_MAX-0x11c || detail!=remover+4 || owner!=resource+0x11c)
                unknown=TRUE;
            else *vacant=(Retirement){.ticket=++retirement_serial,.resource=resource,.remover=remover,
                .resource_generation=owner_record->generation,.copying=TRUE};
        } else if(!r) unknown=TRUE;
        else if(kind==COPY_END) {
            if(!r->copying || r->copied || r->resource!=resource) unknown=TRUE;
            else {r->copying=FALSE;r->copied=TRUE;}
        } else if(kind==RETIRE_SUBMIT_BEGIN) {
            if(r->copying || !r->copied || r->submitting || r->submitted || r->destroying || r->destroyed ||
                !owner || owner>UINTPTR_MAX-0x25c || detail!=owner+0x25c || (manager && manager!=owner)) unknown=TRUE;
            else {manager=owner;r->manager=owner;r->submitting=TRUE;}
        } else if(kind==RETIRE_SUBMIT_END) {
            if(!r->submitting || r->manager!=owner) unknown=TRUE;
            else {r->submitting=FALSE;r->submitted=TRUE;}
        } else if(kind==RETIRE_DELETE_BEGIN) {
            if(owner!=1 || detail!=(uintptr_t)(base+0x3e3ef) || !dispatch_depth || r->copying ||
                (!r->submitted && !r->submitting) || r->destroying || r->destroyed ||
                r->manager!=manager || context!=manager+0x25c) unknown=TRUE;
            else {r->destroying=TRUE;retirement_depth[index]=dispatch_depth;}
        } else if(kind==RETIRE_DELETE_END) {
            if(!r->destroying || owner!=1 || retirement_depth[index]!=dispatch_depth) unknown=TRUE;
            else {r->destroying=FALSE;r->destroyed=TRUE;}
        } else unknown=TRUE;
    }
    if(kind&1) {if(callbacks) --callbacks;else unknown=TRUE;}
    ReleaseSRWLockExclusive(&lock);SetLastError(saved);
}
#define SAVE "pushfl; pushal; mov %esp,%ebp; sub $528,%esp; and $-16,%esp;" \
    "fxsave (%esp); fninit; movl $0x1f80,512(%esp); ldmxcsr 512(%esp); cld; sub $16,%esp;"
#define RESTORE "call _event; add $16,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl;"
#define TASK "mov 36(%ebp),%eax; mov %eax,4(%esp); movl $0,8(%esp); movl $0,12(%esp);"
#define RESOURCE_RESTORE "call _resource_event; add $16,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl;"
static void __attribute__((naked,noinline)) resource_construct_entry(void) {
    __asm__ volatile("push 4(%esp);" SAVE TASK "movl $0,(%esp);" RESOURCE_RESTORE
        "push 20(%esp); push 20(%esp); push 20(%esp); push 20(%esp); call *_resource_construct_original;"
        SAVE TASK "movl $1,(%esp); mov 28(%ebp),%eax; mov %eax,8(%esp);" RESOURCE_RESTORE
        "lea 4(%esp),%esp; ret $16");
}
static void __attribute__((naked,noinline)) resource_body_entry(void) {
    __asm__ volatile("push %ecx;" SAVE TASK "movl $2,(%esp);" RESOURCE_RESTORE
        "call *_resource_body_original;" SAVE TASK "movl $3,(%esp);" RESOURCE_RESTORE
        "lea 4(%esp),%esp; ret");
}
#undef RESOURCE_RESTORE
static void __attribute__((naked,noinline)) enqueue_entry(void) {
    __asm__ volatile("push %esi;" SAVE TASK "movl $0,(%esp); mov 0(%ebp),%eax;"
        "mov %eax,8(%esp); mov 44(%ebp),%eax; mov %eax,12(%esp);" RESTORE
        "push 8(%esp); call *_enqueue_original;" SAVE TASK "movl $1,(%esp);" RESTORE
        "lea 4(%esp),%esp; ret $4");
}
static void __attribute__((naked,noinline)) run_entry(void) {
    __asm__ volatile("push %ecx;" SAVE TASK "movl $2,(%esp);" RESTORE
        "call *_run_original;" SAVE TASK "movl $3,(%esp); mov 28(%ebp),%eax;"
        "mov %eax,12(%esp);" RESTORE "lea 4(%esp),%esp; ret");
}
static void __attribute__((naked,noinline)) delete_entry(void) {
    __asm__ volatile("push %ecx;" SAVE TASK "movl $4,(%esp); mov 44(%ebp),%eax;"
        "mov %eax,12(%esp);" RESTORE "push 8(%esp); call *_delete_original;"
        SAVE TASK "movl $5,(%esp); mov 44(%ebp),%eax; mov %eax,12(%esp);" RESTORE
        "lea 4(%esp),%esp; ret $4");
}
static void __attribute__((naked,noinline)) update_entry(void) {
    __asm__ volatile("push %ecx;" SAVE TASK "subl $4,4(%esp); movl $6,(%esp);" RESTORE
        "push 8(%esp); call *_update_original;" SAVE TASK "subl $4,4(%esp);"
        "movl $7,(%esp);" RESTORE "lea 4(%esp),%esp; ret $4");
}
#define PVS_RESTORE "call _pvs_event; add $16,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl;"
#define PVS_SCOPE "mov 8(%ebp),%eax; mov %eax,4(%esp); mov 16(%ebp),%eax; mov %eax,8(%esp); movl $0,12(%esp);"
static void __attribute__((naked,noinline)) pvs_begin_entry(void) {
    __asm__ volatile(SAVE PVS_SCOPE "movl $0,(%esp);" PVS_RESTORE "jmp *_pvs_begin_original");
}
static void __attribute__((naked,noinline)) pvs_end_entry(void) {
    __asm__ volatile(SAVE PVS_SCOPE "movl $1,(%esp);" PVS_RESTORE "jmp *_pvs_end_original");
}
static void __attribute__((naked,noinline)) node_publish_entry(void) {
    __asm__ volatile(SAVE "movl $2,(%esp); mov 28(%ebp),%eax; mov %eax,4(%esp);"
        "mov 56(%ebp),%eax; mov %eax,8(%esp); mov 20(%ebp),%eax; mov %eax,12(%esp);"
        PVS_RESTORE "jmp *_node_publish_original");
}
static void __attribute__((naked,noinline)) node_free_entry(void) {
    __asm__ volatile("push 4(%esp);" SAVE TASK "movl $3,(%esp);" PVS_RESTORE
        "push 8(%esp); call *_node_free_original; lea 4(%esp),%esp;"
        SAVE TASK "movl $4,(%esp);" PVS_RESTORE "lea 4(%esp),%esp; ret");
}
#define RETIRE_SAVE SAVE "sub $16,%esp;"
#define RETIRE_RESTORE "call _retirement_event; add $32,%esp; fxrstor (%esp); mov %ebp,%esp; popal; popfl;"
#define RETIRE_ID "mov 36(%ebp),%eax; mov %eax,4(%esp); movl $0,8(%esp); movl $0,12(%esp); movl $0,16(%esp); movl $0,20(%esp);"
#define COPY_IDS RETIRE_ID "mov 40(%ebp),%eax; mov %eax,8(%esp); mov 52(%ebp),%eax; mov %eax,12(%esp); mov 48(%ebp),%eax; mov %eax,16(%esp);"
static void __attribute__((naked,noinline)) retire_copy_entry(void) {
    __asm__ volatile("push %ebx; push %ebp;" RETIRE_SAVE COPY_IDS "movl $0,(%esp);" RETIRE_RESTORE
        "push 16(%esp); push 16(%esp); call *_retire_copy_original;"
        RETIRE_SAVE COPY_IDS "movl $1,(%esp);" RETIRE_RESTORE "lea 8(%esp),%esp; ret $8");
}
static void __attribute__((naked,noinline)) retire_enqueue_entry(void) {
    __asm__ volatile("push %esi; push %ebp;" RETIRE_SAVE RETIRE_ID "mov 40(%ebp),%eax; mov %eax,12(%esp);"
        "mov 28(%ebp),%eax; mov %eax,16(%esp); movl $2,(%esp);" RETIRE_RESTORE
        "push 16(%esp); push 16(%esp); call *_retire_enqueue_original;"
        RETIRE_SAVE RETIRE_ID "mov 40(%ebp),%eax; mov %eax,12(%esp); movl $3,(%esp);"
        RETIRE_RESTORE "lea 8(%esp),%esp; ret $8");
}
#define RETIRE_DELETE_IDS RETIRE_ID "mov 44(%ebp),%eax; mov %eax,12(%esp); mov 40(%ebp),%eax; mov %eax,16(%esp); mov 0(%ebp),%eax; mov %eax,20(%esp);"
static void __attribute__((naked,noinline)) retire_delete_entry(void) {
    __asm__ volatile("push %ecx;" RETIRE_SAVE RETIRE_DELETE_IDS "movl $4,(%esp);" RETIRE_RESTORE
        "push 8(%esp); call *_retire_delete_original;"
        RETIRE_SAVE RETIRE_DELETE_IDS "movl $5,(%esp);" RETIRE_RESTORE "lea 4(%esp),%esp; ret $4");
}
#undef RETIRE_DELETE_IDS
#undef COPY_IDS
#undef RETIRE_ID
#undef RETIRE_RESTORE
#undef RETIRE_SAVE
#undef PVS_SCOPE
#undef PVS_RESTORE
#undef SAVE
#undef RESTORE
#undef TASK
static BOOL healthy(HMODULE image) {
    if(!base || base!=(uint8_t *)image || !installed || unknown) return FALSE;
    if(!hooks_exact()) {unknown=TRUE;return FALSE;}return TRUE;
}
static BOOL boundary(const SudekiMpControlUpdateDispatchWitness *w) {
    return w && w->service_post_original_exact && w->dispatch_serial &&
        w->native_thread_id==GetCurrentThreadId() && (!game_thread || game_thread==GetCurrentThreadId()) &&
        !callbacks && !dispatch_depth && SudekiMpControlSeparationUpdateDispatchWitnessStillExact(w);
}
BOOL SudekiMpLanStoryAreaResourceJournalReady(HMODULE image) {
    AcquireSRWLockExclusive(&lock);
    BOOL ok=consumer && healthy(image) && !stopping;
    ReleaseSRWLockExclusive(&lock);return ok;
}
BOOL SudekiMpLanStoryAreaResourceCapture(HMODULE image,uintptr_t resource,uint64_t *generation) {
    if(!resource || !generation) return FALSE;
    AcquireSRWLockExclusive(&lock);BOOL ok=FALSE;
    if(consumer && healthy(image)) {
        Resource *r=resource_phase(resource,SUDEKIMP_AREA_RESOURCE_LIVE);
        if(r && !unknown) {*generation=r->generation;ok=TRUE;}
    }
    ReleaseSRWLockExclusive(&lock);return ok;
}
BOOL SudekiMpLanStoryAreaFinaliseAttach(HMODULE image,const void *who) {
    AcquireSRWLockExclusive(&lock);
    BOOL ok=who && !consumer && healthy(image) && !stopping && !ever_native &&
        GetCurrentThreadId()==startup_thread && startup_exact(base);
    if(ok) consumer=who;
    ReleaseSRWLockExclusive(&lock);return ok;
}
BOOL SudekiMpLanStoryAreaFinaliseSnapshot(HMODULE image,const void *who,
    const SudekiMpControlUpdateDispatchWitness *w,Record *out,unsigned capacity,unsigned *count) {
    if(!out || !count || !who) return FALSE;
    AcquireSRWLockExclusive(&lock);unsigned n=0;
    BOOL ok=consumer==who && healthy(image) && boundary(w);
    if(ok) {
        for(unsigned i=0;i<SUDEKIMP_AREA_FINALISERS;++i) if(records[i].ticket) ++n;
        ok=n<=capacity;
        if(ok) {n=0;for(unsigned i=0;i<SUDEKIMP_AREA_FINALISERS;++i) if(records[i].ticket) out[n++]=records[i];*count=n;}
    }
    ReleaseSRWLockExclusive(&lock);return ok;
}
BOOL SudekiMpLanStoryAreaFinaliseAcknowledge(HMODULE image,const void *who,
    const SudekiMpControlUpdateDispatchWitness *w,uint64_t ticket) {
    AcquireSRWLockExclusive(&lock);BOOL ok=FALSE;
    if(ticket && who && consumer==who && healthy(image) && boundary(w))
        for(unsigned i=0;i<SUDEKIMP_AREA_FINALISERS;++i) {
            Record *r=&records[i];
            if(r->ticket==ticket && r->destroyed && !r->submitting && !r->running && !r->destroying &&
                !r->pvs_submitting && !r->pvs_retiring && (!r->pvs_descriptor || (r->pvs_node && r->pvs_retired))) {
                memset(r,0,sizeof(*r));ok=TRUE;break;
            }
        }
    ReleaseSRWLockExclusive(&lock);return ok;
}
BOOL SudekiMpLanStoryAreaFinaliseDetach(HMODULE image,const void *who,const SudekiMpControlUpdateDispatchWitness *w) {
    AcquireSRWLockExclusive(&lock);
    BOOL ok=who && consumer==who && healthy(image) &&
        (ever_native?boundary(w):(GetCurrentThreadId()==startup_thread && startup_exact(base)));
    for(unsigned i=0;i<SUDEKIMP_AREA_FINALISERS;++i) if(records[i].ticket) ok=FALSE;
    for(unsigned i=0;i<SUDEKIMP_AREA_FINALISERS;++i) if(retirements[i].ticket) ok=FALSE;
    for(unsigned i=0;i<SUDEKIMP_AREA_FINALISERS;++i) if(resources[i].generation) ok=FALSE;
    if(ok) consumer=NULL;
    ReleaseSRWLockExclusive(&lock);return ok;
}
BOOL SudekiMpLanStoryAreaRetirementSnapshot(HMODULE image,const void *who,
    const SudekiMpControlUpdateDispatchWitness *w,Retirement *out,unsigned capacity,unsigned *count) {
    if(!out || !count || !who) return FALSE;
    AcquireSRWLockExclusive(&lock);unsigned n=0;
    BOOL ok=consumer==who && healthy(image) && boundary(w);
    if(ok) {
        for(unsigned i=0;i<SUDEKIMP_AREA_FINALISERS;++i) if(retirements[i].ticket) ++n;
        ok=n<=capacity;
        if(ok) {n=0;for(unsigned i=0;i<SUDEKIMP_AREA_FINALISERS;++i) if(retirements[i].ticket) out[n++]=retirements[i];*count=n;}
    }
    ReleaseSRWLockExclusive(&lock);return ok;
}
BOOL SudekiMpLanStoryAreaRetirementAcknowledge(HMODULE image,const void *who,
    const SudekiMpControlUpdateDispatchWitness *w,uint64_t ticket) {
    AcquireSRWLockExclusive(&lock);BOOL ok=FALSE;
    if(ticket && who && consumer==who && healthy(image) && boundary(w))
        for(unsigned i=0;i<SUDEKIMP_AREA_FINALISERS;++i) {
            Retirement *r=&retirements[i];
            if(r->ticket==ticket && r->copied && r->submitted && r->destroyed && r->drained &&
                !r->copying && !r->submitting && !r->destroying) {
                memset(r,0,sizeof(*r));retirement_depth[i]=0;ok=TRUE;break;
            }
        }
    ReleaseSRWLockExclusive(&lock);return ok;
}
BOOL SudekiMpLanStoryAreaResourceSnapshot(HMODULE image,const void *who,
    const SudekiMpControlUpdateDispatchWitness *w,Resource *out,unsigned capacity,unsigned *count) {
    if(!out || !count || !who) return FALSE;
    AcquireSRWLockExclusive(&lock);unsigned n=0;
    BOOL ok=consumer==who && healthy(image) && boundary(w);
    if(ok) {
        for(unsigned i=0;i<SUDEKIMP_AREA_FINALISERS;++i) if(resources[i].generation) ++n;
        ok=n<=capacity;
        if(ok) {n=0;for(unsigned i=0;i<SUDEKIMP_AREA_FINALISERS;++i) if(resources[i].generation) out[n++]=resources[i];*count=n;}
    }
    ReleaseSRWLockExclusive(&lock);return ok;
}
BOOL SudekiMpLanStoryAreaResourceAcknowledge(HMODULE image,const void *who,
    const SudekiMpControlUpdateDispatchWitness *w,uint64_t generation) {
    AcquireSRWLockExclusive(&lock);BOOL ok=FALSE;
    if(generation && who && consumer==who && healthy(image) && boundary(w)) {
        Resource *r=NULL;BOOL referenced=FALSE;
        for(unsigned i=0;i<SUDEKIMP_AREA_FINALISERS;++i) {
            if(resources[i].generation==generation) r=&resources[i];
            if((records[i].ticket && records[i].resource_generation==generation) ||
                (retirements[i].ticket && retirements[i].resource_generation==generation)) referenced=TRUE;
        }
        if(r && r->phase==SUDEKIMP_AREA_RESOURCE_BODY_RETURNED && !referenced) {
            memset(r,0,sizeof(*r));ok=TRUE;
        }
    }
    ReleaseSRWLockExclusive(&lock);return ok;
}
static BOOL pinned(DWORD error) {
    HMODULE self;(void)GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        (LPCSTR)(uintptr_t)&SudekiMpLanStoryAreaFinaliseUninstall,&self);
    SetLastError(error?error:ERROR_BUSY);return FALSE;
}
BOOL SudekiMpLanStoryAreaFinaliseUninstall(void) {
    AcquireSRWLockExclusive(&lock);
    if(!base) {ReleaseSRWLockExclusive(&lock);return TRUE;}
    stopping=TRUE;DWORD error=0;
    if(ever_native || consumer || callbacks || dispatch_depth || GetCurrentThreadId()!=startup_thread ||
        !startup_exact(base)) error=ERROR_BUSY;
    if(!error) {
        if(!SudekiMpRestoreRelativeCallHook(&resource_construct_hook)) error=GetLastError();
        if(!SudekiMpRestoreRelativeCallHook(&resource_body_hook) && !error) error=GetLastError();
        if(!SudekiMpRestoreRelativeCallHook(&retire_copy_hook) && !error) error=GetLastError();
        if(!SudekiMpRestoreRelativeCallHook(&retire_enqueue_hook) && !error) error=GetLastError();
        if(!SudekiMpRestorePointerHook(&retire_delete_hook) && !error) error=GetLastError();
        if(!SudekiMpRestoreRelativeCallHook(&enqueue_hook) && !error) error=GetLastError();
        if(!SudekiMpRestoreInlineHook(&pvs_begin_hook) && !error) error=GetLastError();
        if(!SudekiMpRestoreInlineHook(&pvs_end_hook) && !error) error=GetLastError();
        if(!SudekiMpRestoreInlineHook(&node_publish_hook) && !error) error=GetLastError();
        if(!SudekiMpRestoreRelativeCallHook(&node_free_hook) && !error) error=GetLastError();
        if(!SudekiMpRestorePointerHook(&update_hook) && !error) error=GetLastError();
        if(!SudekiMpRestorePointerHook(&run_hook) && !error) error=GetLastError();
        if(!SudekiMpRestorePointerHook(&delete_hook) && !error) error=GetLastError();
    }
    if(!error) {base=NULL;installed=FALSE;startup_thread=0;
        enqueue_original=run_original=delete_original=update_original=NULL;
        pvs_begin_original=pvs_end_original=node_publish_original=node_free_original=NULL;
        retire_copy_original=retire_enqueue_original=retire_delete_original=NULL;
        resource_construct_original=resource_body_original=NULL;}
    ReleaseSRWLockExclusive(&lock);return error?pinned(error):TRUE;
}
BOOL SudekiMpLanStoryAreaFinaliseInstall(HMODULE image) {
    AcquireSRWLockExclusive(&lock);
    if(base || installed || unknown || ever_native || serial==UINT64_MAX || retirement_serial==UINT64_MAX ||
        resource_serial==UINT64_MAX || !image_exact(image)) {
        ReleaseSRWLockExclusive(&lock);SetLastError(ERROR_INVALID_STATE);return FALSE;
    }
    base=(uint8_t *)image;startup_thread=GetCurrentThreadId();stopping=FALSE;
    enqueue_original=base+ENQUEUE_NATIVE;run_original=base+RUN;
    delete_original=base+DESTROY;update_original=base+UPDATE;
    node_free_original=base+NODE_FREE;
    retire_copy_original=base+RETIRE_COPY;retire_enqueue_original=base+RETIRE_ENQUEUE;
    retire_delete_original=base+RETIRE_DELETE;
    resource_construct_original=base+RESOURCE_CONSTRUCT;resource_body_original=base+RESOURCE_BODY;
    BOOL ok=SudekiMpInstallPointerHook(&delete_hook,(void **)(base+TASK_VT),delete_original,(void *)(uintptr_t)delete_entry) &&
        SudekiMpInstallPointerHook(&run_hook,(void **)(base+TASK_VT+4),run_original,(void *)(uintptr_t)run_entry) &&
        SudekiMpInstallPointerHook(&update_hook,(void **)(base+UPDATE_SLOT),update_original,(void *)(uintptr_t)update_entry) &&
        SudekiMpInstallRelativeCallHook(&node_free_hook,base+NODE_FREE_SITE,node_free_original,(void *)(uintptr_t)node_free_entry);
    /* Installation is suspended startup only. Publish each trampoline before
     * installing the producer that can lead into it. No runtime patching. */
    if(ok) {ok=SudekiMpInstallInlineHook(&node_publish_hook,base+NODE_PUBLISH_SITE,base+NODE_PUBLISH_SITE,5,(void *)(uintptr_t)node_publish_entry);
        node_publish_original=node_publish_hook.trampoline;}
    if(ok) {ok=SudekiMpInstallInlineHook(&pvs_end_hook,base+PVS_END_SITE,base+PVS_END_SITE,5,(void *)(uintptr_t)pvs_end_entry);
        pvs_end_original=pvs_end_hook.trampoline;}
    if(ok) {ok=SudekiMpInstallInlineHook(&pvs_begin_hook,base+PVS_BEGIN_SITE,base+PVS_BEGIN_SITE,5,(void *)(uintptr_t)pvs_begin_entry);
        pvs_begin_original=pvs_begin_hook.trampoline;}
    if(ok) ok=SudekiMpInstallRelativeCallHook(&enqueue_hook,base+ENQUEUE,enqueue_original,(void *)(uintptr_t)enqueue_entry);
    if(ok) ok=SudekiMpInstallPointerHook(&retire_delete_hook,(void **)(base+RETIRE_DELETE_SLOT),retire_delete_original,(void *)(uintptr_t)retire_delete_entry);
    if(ok) ok=SudekiMpInstallRelativeCallHook(&retire_enqueue_hook,base+RETIRE_ENQUEUE_SITE,retire_enqueue_original,(void *)(uintptr_t)retire_enqueue_entry);
    if(ok) ok=SudekiMpInstallRelativeCallHook(&retire_copy_hook,base+RETIRE_COPY_SITE,retire_copy_original,(void *)(uintptr_t)retire_copy_entry);
    if(ok) ok=SudekiMpInstallRelativeCallHook(&resource_body_hook,base+RESOURCE_BODY_SITE,resource_body_original,(void *)(uintptr_t)resource_body_entry);
    if(ok) ok=SudekiMpInstallRelativeCallHook(&resource_construct_hook,base+RESOURCE_CONSTRUCT_SITE,resource_construct_original,(void *)(uintptr_t)resource_construct_entry);
    DWORD error=GetLastError();if(ok) installed=TRUE;
    ReleaseSRWLockExclusive(&lock);
    if(!ok) {(void)SudekiMpLanStoryAreaFinaliseUninstall();SetLastError(error);}return ok;
}
