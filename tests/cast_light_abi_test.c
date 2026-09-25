#include <windows.h>
#include <stdio.h>
#include <math.h>
#include "../src/engine/cast_light_abi.c"
static int failures,updates,publishes,deletes,registrations;
static BOOL actor_valid=TRUE,delete_foreign;
static uint8_t *image,world[CL_SIZE],schedule[16],device[4],device_table[0x180];
static int actors[2],foreign;
#define CHECK(x) do { if(!(x)) { printf("FAIL %d: %s\n",__LINE__,#x); ++failures; } } while(0)
/* x87 intermediate arithmetic is wider than stored native floats. */
#define NEAR(a,b) (fabsf((a)-(b))<0.000001f)
static void pcall(uint32_t site,uint32_t target) {
    image[site]=0xe8; *(int32_t *)(image+site+1)=(int32_t)(target-site-5);
}
static void fixture(void) {
#define B(r,s) memcpy(image+(r),(s),sizeof(s)-1)
#define A(r,t) (*(void **)(image+(r))=image+(t))
    B(CL_CTOR,"\xd9\xe8\x33\xc9\xd9\x58\x10");
    A(0x76d09,CL_VT); A(0x76d1f,CL_GLOBAL); A(0x76d26,CL_ARRAY_VT);
    B(0x76d2a,"\x89\x48\x60\x89\x48\x64\x89\x48\x68\xc3");
    B(CL_INIT,"\x51\x56\x8b\xf1\x80\x7e\x22\x00");
    pcall(0x1f226,0x1342e0); pcall(0x1f24c,0x106050); A(0x1f253,CL_GROUP);
    pcall(0x1f26b,0x1f280); pcall(0x1f28d,0x162d60); pcall(0x1f2df,0x1f550);
    B(CL_UPDATE,"\x55\x8b\xec\x83\xe4\xf8\x83\xec\x24\x8b\x45\x08\xd9\x40\x0c");
    B(0x1f512,"\x8d\x74\x24\x24\xd9\x41\x3c\xbf\x07\x00\x00\x00");
    pcall(CL_PUBLISH_CALL,CL_PUBLISH); B(0x1f545,"\xc2\x04\x00");
    A(CL_VT,CL_DELETE); A(CL_VT+4,CL_UPDATE);
    B(CL_DELETE,"\x56\x57\x8b\xf9\x8d\x47\x5c");
    pcall(0x7711d,0x162d60); A(0x7712e,CL_GLOBAL); pcall(0x77136,0x4d30);
    pcall(0x7714b,0x134330); B(0x77164,"\xc2\x04\x00");
    B(CL_REMOVE,"\x55\x8b\xec\x83\xe4\xf8\x8b\x45\x08\x83\xec\x0c");
    pcall(0x1f5db,0x1f6f0); pcall(0x1f613,0x24844f); pcall(0x1f628,0x38e80);
    B(0x1f655,"\xc2\x04\x00"); B(0x1f740,"\xd9\x44\x24\x04\xd9\x59\x30\xc2\x04\x00");
    B(0x1f750,"\xa1"); A(0x1f751,CL_GLOBAL); B(0x1f755,"\xc3");
    B(CL_PERIOD,"\x83\xec\x08\xd9\x44\x24\x0c\x56");
    pcall(0x106266,0x134280); pcall(0x106272,0x134330); pcall(0x106281,0x1342e0);
    B(0x10629c,"\xc2\x04\x00"); B(0x77c53,"\x8d\x86\xd4\x06\x00\x00");
    pcall(0x77c66,CL_PERIOD); A(0x77c43,0x2c3bfc); B(0x2c3bfc,"\x00\x00\x00\x40");
#undef A
#undef B
}
static BOOL witness(void *actor,uint64_t session) {
    return actor_valid && (actor==&actors[0] || actor==&actors[1]) && session==7;
}
static void __attribute__((regparm(1))) ctor(void *object) {
    uint8_t *p=object;
    *(void **)p=image+CL_VT; *(void **)(p+0x5c)=image+CL_ARRAY_VT;
    *(int16_t *)(p+0x20)=-1; p[0x22]=1;
    *(void **)(image+CL_GLOBAL)=p;
}
static void __attribute__((thiscall)) init(void *object) {
    uint8_t *p=object;
    uint8_t **items=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,16);
    items[0]=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,16);
    *(int *)items[0]=1;
    memcpy(items[0]+4,image+CL_GROUP,12);
    memcpy(p+0x34,image+CL_GROUP,12); memcpy(p+0x40,image+CL_GROUP,12);
    memcpy(p+0x50,image+CL_GROUP,12); *(float *)(p+0x30)=.4f;
    *(int *)(p+0x4c)=1; *(int *)(p+0x60)=1; *(int *)(p+0x64)=4;
    *(void ***)(p+0x68)=(void **)items;
    p[0x22]=0; /* Native Init does not register a newly constructed node. */
}
static void __attribute__((used,noinline)) register_light(uint8_t *p,float period) {
    CHECK(period==2.f && *(int16_t *)(p+0x20)==-1 && !p[0x22]);
    ++registrations; *(int16_t *)(p+0x20)=33;
}
static void __attribute__((naked,noinline)) period_setter(void) {
    __asm__ volatile("pushl 4(%esp)\n\tpushl %eax\n\tcall _register_light\n\taddl $8,%esp\n\tret $4\n\t");
}
static void __attribute__((thiscall)) remove_light(void *object,int id) {
    uint8_t *p=object;
    uint8_t **items=*(uint8_t ***)(p+0x68);
    unsigned int count=*(unsigned int *)(p+0x60),i;
    for(i=0;i<count;++i) if(*(int *)items[i]==id) break;
    CHECK(i<count); if(i==count) return;
    HeapFree(GetProcessHeap(),0,items[i]);
    memmove(items+i,items+i+1,(count-i-1)*4); items[count-1]=NULL;
    *(unsigned int *)(p+0x60)=--count;
    if(count) memcpy(p+0x40,items[count-1]+4,12);
}
static void * __attribute__((thiscall)) destroy(void *object,unsigned int flags) {
    uint8_t *p=object; CHECK(flags==0 && *(int *)(p+0x60)==0); ++deletes;
    HeapFree(GetProcessHeap(),0,*(void **)(p+0x68)); *(void **)(p+0x68)=NULL;
    *(void **)(image+CL_GLOBAL)=delete_foreign ? &foreign:NULL;
    return object;
}
static void publish(void) { ++publishes; }
static void __attribute__((thiscall)) update(void *object,void *args) {
    uint8_t *p=object; float dt=*(float *)((uint8_t *)args+12);
    ++updates;
    for(unsigned int i=0;i<3;++i) {
        float *v=(float *)(p+0x34+4*i),target=*(float *)(p+0x40+4*i);
        float step=*(float *)(p+0x30)*dt;
        *v=target>*v ? fminf(target,*v+step):fmaxf(target,*v-step);
    }
    light_publish_bridge(); SetLastError(123);
}
static void add_effect(uint8_t *p,int id,float value) {
    unsigned int n=*(unsigned int *)(p+0x60);
    uint8_t **items=*(uint8_t ***)(p+0x68);
    CHECK(n<4); items[n]=HeapAlloc(GetProcessHeap(),0,16); *(int *)items[n]=id;
    for(unsigned int i=0;i<3;++i) *(float *)(items[n]+4+4*i)=value;
    *(unsigned int *)(p+0x60)=n+1; memcpy(p+0x40,items[n]+4,12);
}
static void callbacks(void) {
    light_ctor=ctor; light_init=init; light_delete=destroy;
    light_update_original=update; light_remove=remove_light; light_publish_original=publish;
    light_period=period_setter;
}
int main(void) {
    float args[4]={0,0,0,1},current[3],baseline[3];
    uint8_t *a,*b; void *patched; uint8_t call_bytes[5];
    const uint32_t bad_bytes[]={CL_CTOR,0x76d1f,CL_INIT,0x1f226,0x1f253,0x1f28d,
        CL_UPDATE,0x1f512,CL_PUBLISH_CALL,CL_DELETE,0x7712e,0x7714b,CL_REMOVE,0x1f628,0x1f740,0x1f751,
        CL_PERIOD,0x106266,0x106272,0x106281,0x10629c,0x77c53,0x77c66,0x77c43,0x2c3bfc};
    image=VirtualAlloc(NULL,0x45f000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    CHECK(image!=NULL); if(!image) return 1;
    fixture(); ctor(world);
    *(float *)(image+CL_GROUP)=*(float *)(image+CL_GROUP+4)=*(float *)(image+CL_GROUP+8)=1;
    init(world); *(void **)(image+CL_SCHEDULER)=schedule; *(void **)schedule=image;
    *(void **)(image+CL_DEVICE)=device; *(void **)device=device_table;
    for(unsigned int i=0;i<sizeof(bad_bytes)/sizeof(bad_bytes[0]);++i) {
        image[bad_bytes[i]]^=1;
        CHECK(!SudekiMpInitializeCastLightAbi((HMODULE)image) && !light_image);
        image[bad_bytes[i]]^=1;
    }
    CHECK(SudekiMpInitializeCastLightAbi((HMODULE)image)); callbacks();
    CHECK(SudekiMpCreateCastLight(11,&actors[0],7,witness));
    CHECK(SudekiMpCreateCastLight(22,&actors[1],7,witness));
    CHECK(!SudekiMpCreateCastLight(11,&actors[1],7,witness));
    CHECK(!SudekiMpCreateCastLight(33,&actors[1],8,witness));
    CHECK(registrations==2);
    a=lm_find(11)->object; b=lm_find(22)->object;
    *(int16_t *)(a+0x20)=-1;
    CHECK(!SudekiMpReadCastLight(11,current,baseline) && !SudekiMpCastLightTransitionReady(11));
    *(int16_t *)(a+0x20)=33;
    CHECK(a!=b && a!=world && b!=world && *(void **)(image+CL_GLOBAL)==world);
    CHECK(SudekiMpCastLightTransitionReady(11)); SudekiMpCastLightTransitionCommit(11);
    add_effect(a,2,.2f);
    CHECK(SudekiMpCastLightTransitionReady(22)); SudekiMpCastLightTransitionCommit(22);
    add_effect(b,2,.6f); /* Same native handle, different caster: no collision. */
    CHECK(SudekiMpCastLightTransitionReady(0)); SudekiMpCastLightTransitionCommit(0);
    light_update(a,args); CHECK(GetLastError()==123);
    light_update(b,args); light_update(world,args);
    CHECK(updates==3 && publishes==1 && *(void **)(image+CL_GLOBAL)==world);
    CHECK(SudekiMpReadCastLight(11,current,baseline) && NEAR(current[0],.6f) && baseline[0]==1);
    CHECK(!SudekiMpDestroyCastLight(11) && !SudekiMpResetCastLightAbi());
    remove_light(b,2); light_update(b,args);
    CHECK(SudekiMpCastLightDrained(22) && !SudekiMpCastLightDrained(11));
    CHECK(SudekiMpReadCastLight(11,current,baseline) && NEAR(current[0],.6f));
    /* Cleanup in either order cannot pop or reset the other caster's stack. */
    add_effect(b,2,.7f); remove_light(a,2);
    CHECK(!SudekiMpCastLightDrained(11)); light_update(a,args);
    CHECK(SudekiMpCastLightDrained(11) && !SudekiMpCastLightDrained(22));
    CHECK(SudekiMpDestroyCastLight(11) && !SudekiMpCastLightTransitionReady(11));
    CHECK(*(int *)(b+0x60)==2 && NEAR(*(float *)(b+0x40),.7f));
    /* A nested neutral call retains the latest world value, not a stale save. */
    CHECK(SudekiMpCastLightTransitionReady(22)); SudekiMpCastLightTransitionCommit(22);
    CHECK(SudekiMpCastLightTransitionReady(0)); SudekiMpCastLightTransitionCommit(0);
    *(float *)(world+0x30)=.8f;
    CHECK(SudekiMpCastLightTransitionReady(22)); SudekiMpCastLightTransitionCommit(22);
    CHECK(NEAR(*(float *)(world+0x30),.8f) && NEAR(*(float *)(b+0x30),.4f));
    *(void **)(image+CL_GLOBAL)=&foreign;
    CHECK(!SudekiMpCastLightTransitionReady(0) && selected_key==22);
    *(void **)(image+CL_GLOBAL)=b;
    actor_valid=FALSE; CHECK(!SudekiMpCastLightTransitionReady(0)); actor_valid=TRUE;
    patched=*(void **)(image+CL_VT+4); *(void **)(image+CL_VT+4)=&foreign;
    CHECK(!SudekiMpCastLightTransitionReady(0) && !SudekiMpReadCastLight(22,current,baseline));
    *(void **)(image+CL_VT+4)=patched;
    memcpy(call_bytes,image+CL_PUBLISH_CALL,5); image[CL_PUBLISH_CALL+1]^=1;
    CHECK(!SudekiMpCastLightTransitionReady(0) && !SudekiMpCastLightDrained(22));
    memcpy(image+CL_PUBLISH_CALL,call_bytes,5);
    CHECK(SudekiMpCastLightTransitionReady(0)); SudekiMpCastLightTransitionCommit(0);
    *(float *)(b+0x34)=NAN; CHECK(!SudekiMpReadCastLight(22,current,baseline));
    *(float *)(b+0x34)=1;
    remove_light(b,2); light_update(b,args); CHECK(SudekiMpCastLightDrained(22));
    delete_foreign=TRUE;
    CHECK(!SudekiMpDestroyCastLight(22) && lm_find(22)->destroyed && deletes==2);
    CHECK(!SudekiMpDestroyCastLight(22) && deletes==2 && !SudekiMpResetCastLightAbi());
    *(void **)(image+CL_GLOBAL)=NULL; /* Fixture-only repair of foreign ownership. */
    CHECK(SudekiMpDestroyCastLight(22) && deletes==2 && *(void **)(image+CL_GLOBAL)==world);
    CHECK(registrations==2); /* No re-registration from updates, reads or retirement. */
    /* Failed hook restore retains originals even if the other seam restores. */
    patched=*(void **)(image+CL_VT+4); *(void **)(image+CL_VT+4)=&foreign;
    CHECK(!SudekiMpResetCastLightAbi() && light_image && light_update_original && light_publish_original);
    *(void **)(image+CL_VT+4)=patched;
    CHECK(SudekiMpResetCastLightAbi());
    CHECK(SudekiMpInitializeCastLightAbi((HMODULE)image)); callbacks();
    memcpy(call_bytes,image+CL_PUBLISH_CALL,5); image[CL_PUBLISH_CALL+1]^=1;
    CHECK(!SudekiMpResetCastLightAbi() && light_image && light_publish_original);
    memcpy(image+CL_PUBLISH_CALL,call_bytes,5); CHECK(SudekiMpResetCastLightAbi());
    delete_foreign=FALSE; remove_light(world,1); destroy(world,0);
    VirtualFree(image,0,MEM_RELEASE);
    printf("Caster lighting lifetime tests: %s\n",failures ? "FAIL":"PASS");
    return failures ? 1:0;
}
