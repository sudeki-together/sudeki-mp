#include "hooks/lan_story_xp.h"
#include "hooks/lan_story_area_follow.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <math.h>
#include <string.h>

static uint8_t *xp_base,*xp_manager;
static BOOL xp_host;
static DWORD xp_thread;
static uint32_t xp_stream,xp_serial,xp_seen_stream,xp_seen[24];
static char xp_area[40];
static struct {uint8_t *object;BOOL active;SudekiMpStoryXpPopup popup;} xp_previous[24];
static void *xp_native __attribute__((used));
static const struct {const char *name;uint32_t rva,size,hash;} xp_contracts[]={
    {"XpPopup",0xafb90u,0x4fu,0xa49d4e09u},
    {"XpFreeSlot",0xafbe0u,0x3au,0x6d29ab55u},
    {"XpSetupHead",0x182e20u,0x19u,0x2a82c96du}
};
static BOOL xp_memory(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m;uintptr_t a=(uintptr_t)p;
    return p && n && a<=UINTPTR_MAX-n && VirtualQuery(p,&m,sizeof(m))==sizeof(m) &&
        m.State==MEM_COMMIT && !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
        a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL xp_code(uint8_t *b) {
    if(!b)return FALSE;
    for(unsigned i=0;i<sizeof(xp_contracts)/sizeof(*xp_contracts);++i) {
        uint32_t r=xp_contracts[i].rva,n=xp_contracts[i].size,h=2166136261u;
        if(!xp_memory(b+r,n))return FALSE;
        for(unsigned k=0;k<n;++k)h=(h^b[r+k])*16777619u;
        if(h!=xp_contracts[i].hash)return FALSE;
    }
    return xp_memory(b+0x182f95u,3) && b[0x182f95u]==0xc2 &&
        b[0x182f96u]==0x18 && b[0x182f97u]==0;
}
static BOOL xp_begin(void) {
    if(!xp_base)return FALSE;
    if(!xp_thread)xp_thread=GetCurrentThreadId();
    return xp_thread==GetCurrentThreadId();
}
/* The global manager owns this complete native pool. Unknown/partial reads
 * never publish an empty snapshot or grant a native call. */
static uint8_t *xp_pool(uint8_t *objects[24]) {
    if(!xp_begin() || !xp_memory(xp_base+0x409dbcu,4))return NULL;
    uint8_t *m=*(uint8_t **)(xp_base+0x409dbcu);
    if(!xp_memory(m,0x84) || *(void **)m!=xp_base+0x2ca348u ||
        *(uint32_t *)(m+0x7cu)>=24 || m[0x80u]>1 || m[0x81u]!=1)return NULL;
    for(unsigned i=0;i<24;++i) {
        uint8_t *p=*(uint8_t **)(m+0x1cu+4*i);objects[i]=p;
        if(!xp_memory(p,0x98u) || *(void **)p!=xp_base+0x2d9058u || p[8]>1)return NULL;
        for(unsigned k=0;k<i;++k)if(objects[k]==p)return NULL;
    }
    return m;
}
BOOL SudekiMpLanStoryXpInitialize(HMODULE image,BOOL host) {
    if(xp_base || !SudekiMpCheckLoadedExecutable(image) || !xp_code((uint8_t *)image))return FALSE;
    xp_base=(uint8_t *)image;xp_host=host;xp_native=xp_base+0xafb90u;return TRUE;
}
BOOL SudekiMpLanStoryXpCapture(uint32_t epoch,uint32_t revision,uint32_t tick,
    const char current[40],SudekiMpStoryXp *out) {
    uint8_t *objects[24],*m;
    if(!xp_host || !out || !current || !(m=xp_pool(objects)))return FALSE;
    SudekiMpStoryXp next={.epoch=epoch,.revision=revision,.tick=tick,.stream=1};
    memcpy(next.current,current,40);
    BOOL changed=xp_manager!=m || memcmp(xp_area,current,40);
    BOOL suppress=changed && xp_manager!=NULL;
    for(unsigned i=0;i<24;++i) {
        uint8_t *p=objects[i];
        if(!p[8])continue;
        SudekiMpStoryXpPopup *v=next.slots+i;
        v->serial=1;v->amount=*(uint32_t *)(p+0x64u);
        memcpy(v->position,p+0x8cu,12);
        /* Native setup adds this popup's authored vertical offset. Transmit
         * the original input, since the receiver adds its own offset once. */
        v->position[1]-=*(float *)(p+0x84u);
        v->height=*(float *)(p+0x80u);v->elapsed=*(float *)(p+0x88u);v->duration=*(float *)(p+0x7cu);
    }
    if(!SudekiMpStoryXpValid(&next))return FALSE;
    if(changed) {
        if(!++xp_stream)++xp_stream;
        memset(xp_previous,0,sizeof(xp_previous));xp_manager=m;memcpy(xp_area,current,40);
    }
    next.stream=xp_stream;
    for(unsigned i=0;i<24;++i) {
        SudekiMpStoryXpPopup *v=next.slots+i;
        BOOL active=v->serial!=0;
        if(active) {
            BOOL start=!xp_previous[i].active || xp_previous[i].object!=objects[i] ||
                v->amount!=xp_previous[i].popup.amount ||
                memcmp(v->position,xp_previous[i].popup.position,12) ||
                v->height!=xp_previous[i].popup.height || v->duration!=xp_previous[i].popup.duration || v->elapsed<xp_previous[i].popup.elapsed;
            if(start && !suppress) {if(!++xp_serial)++xp_serial;v->serial=xp_serial;}
            else v->serial=xp_previous[i].popup.serial;
        }
        xp_previous[i].object=objects[i];xp_previous[i].active=active;xp_previous[i].popup=*v;
        if(!v->serial)memset(v,0,sizeof(*v));
    }
    *out=next;return TRUE;
}
/* ESI=manager; five stack words (amount, XYZ, height); native RET 20.
 * This entry only obtains a free visual and calls its setup. It never invokes
 * CCombat death, AddExperienceToParty, loot generation or monster simulation. */
static void __attribute__((naked,noinline)) xp_show(void *manager __attribute__((unused)),
    const SudekiMpStoryXpPopup *popup __attribute__((unused))) {
    __asm__ volatile("push %ebp;mov %esp,%ebp;push %esi;mov 8(%ebp),%esi;mov 12(%ebp),%eax;"
        "push 20(%eax);push 16(%eax);push 12(%eax);push 8(%eax);push 4(%eax);"
        "call *_xp_native;pop %esi;pop %ebp;ret;");
}
BOOL SudekiMpLanStoryXpPresent(const SudekiMpStoryXp *s,uint32_t now,uint32_t received_age) {
    (void)now;
    uint8_t *objects[24],*m;SudekiMpStoryAreaState area;
    if(xp_host || !SudekiMpStoryXpValid(s) || !(m=xp_pool(objects)) ||
        !SudekiMpLanStoryAreaCapture(&area) || _stricmp(area.current,s->current) ||
        (xp_seen_stream && (int32_t)(s->stream-xp_seen_stream)<0))return FALSE;
    if(xp_seen_stream!=s->stream) {memset(xp_seen,0,sizeof(xp_seen));xp_seen_stream=s->stream;}
    for(unsigned i=0;i<24;++i) {
        const SudekiMpStoryXpPopup *v=s->slots+i;
        if(!v->serial || (xp_seen[i] && (int32_t)(v->serial-xp_seen[i])<=0))continue;
        float elapsed=v->elapsed+(float)received_age/1000.0f;
        if(elapsed>=v->duration || !m[0x80u]) {xp_seen[i]=v->serial;continue;}
        /* Native selects the first free slot AFTER its rotating cursor. Prove
         * every pointer and the selected model before entering that routine. */
        unsigned cursor=*(uint32_t *)(m+0x7cu),slot=24;
        for(unsigned k=1;k<24;++k)if(!objects[(cursor+k)%24][8]) {slot=(cursor+k)%24;break;}
        if(slot==24 || !*(void **)(objects[slot]+0x78u) ||
            *(float *)(objects[slot]+0x7cu)!=v->duration)continue;
        xp_show(m,v);
        uint8_t *fresh[24],*owner=xp_pool(fresh);
        MEMORY_BASIC_INFORMATION memory;
        if(owner!=m || fresh[slot]!=objects[slot] || !objects[slot][8] ||
            VirtualQuery(objects[slot],&memory,sizeof(memory))!=sizeof(memory) ||
            !(memory.Protect&(PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return FALSE;
        /* Joining an in-progress popup must not restart its full lifetime. */
        *(float *)(objects[slot]+0x88u)=elapsed;xp_seen[i]=v->serial;
        SudekiMpLogFormat("story_xp event=presented serial=%lu amount=%lu elapsed=%.3f\r\n",
            (unsigned long)v->serial,(unsigned long)v->amount,(double)elapsed);
    }
    return TRUE;
}
void SudekiMpLanStoryXpReset(void) {
    xp_base=xp_manager=NULL;xp_native=NULL;xp_thread=0;xp_host=FALSE;
    xp_stream=xp_serial=xp_seen_stream=0;memset(xp_seen,0,sizeof(xp_seen));
    memset(xp_previous,0,sizeof(xp_previous));memset(xp_area,0,sizeof(xp_area));
}
