#include "hooks/story_idle_contact.h"
#include "hooks/call_hook.h"
#include "engine/build_identity.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t *image;
static uint8_t actor[0x210],position[0x110],movement[0xc0],collider[0x90],arbiter[0x64],object[0x110];
static uint8_t manager[0xa0],record[0x58];
static void *records[1];
static BOOL grant=TRUE;
static unsigned fail_install,install_calls,test_site;
static DWORD thread;
static unsigned lease_calls;
extern void *SudekiMpStoryIdleContactNext[2];
BOOL __real_SudekiMpInstallInlineHook(SudekiMpInlineHook *,uint8_t *,const uint8_t *,size_t,const void *);
BOOL __wrap_SudekiMpInstallInlineHook(SudekiMpInlineHook *h,uint8_t *p,const uint8_t *b,size_t n,const void *fn) {
    if(++install_calls==fail_install) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return FALSE; }
    return __real_SudekiMpInstallInlineHook(h,p,b,n,fn);
}
static BOOL __attribute__((target("sse2"))) lease(void *a) {
    __asm__ volatile("fninit; pxor %%xmm0,%%xmm0" ::: "xmm0");
    SetLastError(999); ++lease_calls;
    return grant && a==actor && GetCurrentThreadId()==thread;
}
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(file!=INVALID_HANDLE_VALUE); DWORD size=GetFileSize(file,NULL),got=0;
    uint8_t *raw=malloc(size); assert(raw && ReadFile(file,raw,size,&got,NULL) && got==size); CloseHandle(file);
    IMAGE_DOS_HEADER *dos=(void *)raw; IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *b=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    assert(b); memcpy(b,raw,nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *s=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        assert(s[i].PointerToRawData<=size && s[i].SizeOfRawData<=size-s[i].PointerToRawData);
        memcpy(b+s[i].VirtualAddress,raw+s[i].PointerToRawData,s[i].SizeOfRawData);
    }
    uintptr_t delta=(uintptr_t)b-nt->OptionalHeader.ImageBase;
    IMAGE_DATA_DIRECTORY rel=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    for(unsigned offset=0;offset<rel.Size;) {
        IMAGE_BASE_RELOCATION *block=(void *)(b+rel.VirtualAddress+offset);
        assert(block->SizeOfBlock>=sizeof(*block) && block->SizeOfBlock<=rel.Size-offset);
        uint16_t *items=(void *)(block+1);
        for(unsigned i=0;i<(block->SizeOfBlock-sizeof(*block))/2;++i) {
            unsigned type=items[i]>>12,rva=block->VirtualAddress+(items[i]&0xfff);
            assert(type==IMAGE_REL_BASED_ABSOLUTE || type==IMAGE_REL_BASED_HIGHLOW);
            if(type==IMAGE_REL_BASED_HIGHLOW) *(uint32_t *)(b+rva)+=(uint32_t)delta;
        }
        offset+=block->SizeOfBlock;
    }
    assert(FlushInstructionCache(GetCurrentProcess(),b,nt->OptionalHeader.SizeOfImage)); free(raw); return b;
}
static void contracts(void) {
    assert(SudekiMpStoryIdleContactImageExact(image));
    for(unsigned k=0;k<sizeof(story_idle_contact_contracts)/sizeof(*story_idle_contact_contracts);++k) {
        const __typeof__(*story_idle_contact_contracts) *c=&story_idle_contact_contracts[k];
        for(unsigned i=0;i<c->size;++i) {
            image[c->rva+i]^=1; assert(!SudekiMpStoryIdleContactImageExact(image));
            assert(!SudekiMpStoryIdleContactInstall((HMODULE)image,lease)); image[c->rva+i]^=1;
        }
        printf("%s: exact bytes and every-byte install rejection passed\n",c->name);
    }
}
static void reset(void) {
    grant=TRUE; lease_calls=0;
    memset(actor,0,sizeof(actor)); memset(position,0,sizeof(position)); memset(movement,0,sizeof(movement));
    memset(collider,0,sizeof(collider)); memset(arbiter,0,sizeof(arbiter)); memset(object,0,sizeof(object));
    *(void **)actor=image+0x2d555c;
    *(void **)(actor+0x44)=position; *(void **)(actor+0x60)=collider;
    *(void **)(actor+0x80)=movement; *(void **)(actor+0x90)=arbiter;
    *(void **)position=image+0x2cdefc; *(void **)movement=image+0x2c8644;
    *(void **)collider=image+0x2c85fc; *(void **)arbiter=image+0x2cc9ac;
    *(void **)(position+0x10)=*(void **)(movement+0x10)=*(void **)(collider+0x10)=*(void **)(arbiter+0x10)=actor;
    *(void **)(position+0x8c)=object; *(void **)(object+0x100)=position+4;
    movement[0x79]=1; *(float *)(object+0xe4)=-0.004f;
    *(uint32_t *)(collider+0x2c)=0x9300005f;
    records[0]=record; *(void **)(manager+0x90)=records; *(unsigned *)(manager+0x9c)=1;
    *(unsigned *)(record+0x20)=1;
}
/* Execute the real patched instruction with its native frame/register layout;
 * intercept only the continuation to check the entire ABI without gameplay. */
void *probe_entry;
uint32_t probe_frame[4],probe_regs[8],probe_flags,probe_sse[4];
float probe_fp; uint16_t probe_cw; uint32_t probe_mxcsr;
float probe_normal[3]={0,1,0},probe_depth=0.000002f;
float probe_separation[3]={0,1,0};
#define STUB(name,code) static void __attribute__((naked,noinline)) name(void) { __asm__ volatile(code); }
STUB(probe_return,"pushfl; popl _probe_flags; pushal; mov %esp,%esi; mov $_probe_regs,%edi;"
    "mov $8,%ecx; cld; rep movsl; popal; movups %xmm0,_probe_sse; fstps _probe_fp;"
    "fnstcw _probe_cw; stmxcsr _probe_mxcsr; add $80,%esp; ret")
STUB(probe,"pushal; push $1f; sub $80,%esp; mov _probe_depth,%eax; mov %eax,12(%esp);"
    "mov _probe_normal,%eax; mov %eax,20(%esp); mov _probe_normal+4,%eax; mov %eax,24(%esp);"
    "mov _probe_normal+8,%eax; mov %eax,28(%esp);"
    "mov _probe_separation,%eax; mov %eax,68(%esp); mov _probe_separation+4,%eax; mov %eax,72(%esp);"
    "mov _probe_separation+8,%eax; mov %eax,76(%esp); mov $_probe_frame,%ebp; mov $_manager,%ebx;"
    "xor %edi,%edi; mov $0x6666,%esi; mov $0x1111,%eax; mov $0x2222,%ecx; mov $0x3333,%edx;"
    "pcmpeqd %xmm0,%xmm0; fld1; cmp %eax,%eax; jmp *_probe_entry; 1: popal; ret")
static void run(BOOL expected) {
    uint32_t before=SudekiMpStoryIdleContactCount();
    __asm__ volatile("fninit"); SetLastError(123); probe();
    assert(GetLastError()==123);
    assert(fabsf(probe_fp-(expected?(test_site?2.00001f:1.00001f):1.01f))<0.000001f);
    assert(SudekiMpStoryIdleContactCount()==before+(expected?1u:0u));
    assert(probe_regs[0]==0 && probe_regs[1]==0x6666 && probe_regs[2]==(uintptr_t)probe_frame);
    assert(probe_regs[4]==(uintptr_t)manager && probe_regs[5]==0x3333 &&
        probe_regs[6]==0x2222 && probe_regs[7]==0x1111);
    assert((probe_flags&0x8d5)==0x44 && probe_cw==0x37f && (probe_mxcsr&~0x3fu)==0x1f80);
    for(unsigned i=0;i<4;++i) assert(probe_sse[i]==UINT32_MAX);
}
static DWORD WINAPI foreign(void *unused) {
    (void)unused; run(FALSE); assert(!SudekiMpStoryIdleContactBindThread());
    assert(!SudekiMpStoryIdleContactUninstall()); return 0;
}
static void guard_tests(void) {
    reset(); run(TRUE);
    *(void **)actor=image+0x2d66fc; run(TRUE); /* Same native Elco movement component. */
    reset(); grant=FALSE; run(FALSE);
    for(unsigned c=0;c<2;++c) {
        reset(); *(void **)actor=image+(c?0x2d5a88:0x2d5010);
        movement[0x79]=0; *(float *)(object+0xe4)=-0.2548f; run(TRUE);
        movement[0x79]=1; run(FALSE);
        movement[0x79]=0; *(float *)(object+0xe4)=-0.6f; run(FALSE);
    }
    reset(); *(void **)actor=image+0x2d5fa0; run(FALSE);
    for(unsigned kind=2;kind<=5;++kind) { reset(); *(unsigned *)(record+0x20)=kind; run(FALSE); }
    reset(); *(unsigned *)(manager+0x9c)=0; run(FALSE);
    reset(); *(unsigned *)(manager+0x9c)=501; run(FALSE);
    reset(); *(void **)(collider+0x10)=NULL; run(FALSE);
    reset(); *(void **)(actor+0x44)=NULL; run(FALSE);
    reset(); *(void **)(position+0x10)=NULL; run(FALSE);
    reset(); *(void **)(movement+0x10)=NULL; run(FALSE);
    reset(); *(void **)(arbiter+0x10)=NULL; run(FALSE);
    reset(); *(void **)(object+0x100)=position; run(FALSE);
    reset(); position[0x103]=1; run(FALSE);
    reset(); *(void **)(position+0x94)=actor; run(FALSE);
    reset(); movement[0x79]=0; run(FALSE);
    const unsigned moving[]={0x24,0x28,0x34,0x3c,0x40,0x44,0x48};
    for(unsigned i=0;i<sizeof(moving)/sizeof(*moving);++i) {
        reset(); *(float *)(movement+moving[i])=0.1f; run(FALSE);
        *(float *)(movement+moving[i])=NAN; run(FALSE);
    }
    const float vertical[]={0,0.004f,-0.1f,NAN,INFINITY};
    for(unsigned i=0;i<sizeof(vertical)/sizeof(*vertical);++i) { reset(); *(float *)(object+0xe4)=vertical[i]; run(FALSE); }
    reset(); *(float *)(object+0xe0)=0.1f; run(FALSE);
    reset(); *(float *)(object+0xe8)=0.1f; run(FALSE);
    for(unsigned bit=0;bit<32;++bit) if((1u<<bit)&0x081812c8u) {
        reset(); *(uint32_t *)(arbiter+0x50)=1u<<bit; run(FALSE);
    }
    reset(); arbiter[0x60]=1; run(FALSE);
    reset(); arbiter[0x60]=4; run(FALSE);
    reset(); probe_normal[0]=0.04f; probe_normal[2]=0.03f;
    probe_normal[1]=sqrtf(1.0f-0.04f*0.04f-0.03f*0.03f); run(TRUE);
    probe_normal[0]=0.08f;probe_normal[2]=0;
    probe_normal[1]=sqrtf(1.0f-0.08f*0.08f);run(FALSE);
    probe_normal[0]=0;probe_normal[1]=0.9f; run(FALSE); probe_normal[1]=1;
    probe_normal[0]=NAN; run(FALSE); probe_normal[0]=0;
    probe_normal[2]=0.5f; run(FALSE); probe_normal[2]=0;
    probe_depth=test_site?0.0002f:0.00002f; run(FALSE); probe_depth=NAN; run(FALSE);
    probe_depth=test_site?0.000012f:0.000002f;
    if(test_site) {
        probe_separation[1]=0.9f;run(FALSE);probe_separation[1]=1;
        probe_separation[0]=NAN;run(FALSE);probe_separation[0]=0;
        probe_depth=0.000002f;run(FALSE);probe_depth=0.000012f;
    }
    reset(); probe_frame[1]=0; run(FALSE); probe_frame[1]=(uintptr_t)(image+0x3265f);
    reset(); HANDLE t=CreateThread(NULL,0,foreign,NULL,0,NULL); assert(t);
    assert(WaitForSingleObject(t,10000)==WAIT_OBJECT_0); CloseHandle(t);
    puts("IDLE_CONTACT_ABI_GUARDS: registers, flags, x87/SSE, LastError, identity, motion and foreign-thread checks passed");
}
/* Run the original complete 33010 routine over a synthetic floor triangle.
 * Math imports are inert equivalents; native triangle distance and response
 * execute unchanged. No game process, save, scene or native object is used. */
static float *__stdcall normalize(float *out,const float *in) {
    float n=sqrtf(in[0]*in[0]+in[1]*in[1]+in[2]*in[2]);
    for(unsigned i=0;i<3;++i) out[i]=n>0?in[i]/n:0;
    return out;
}
static float *__stdcall plane(float *out,const float *a,const float *b,const float *c) {
    float u[3],v[3],n[3];
    for(unsigned i=0;i<3;++i) {u[i]=b[i]-a[i];v[i]=c[i]-a[i];}
    n[0]=u[1]*v[2]-u[2]*v[1];n[1]=u[2]*v[0]-u[0]*v[2];n[2]=u[0]*v[1]-u[1]*v[0];
    normalize(out,n);out[3]=-(out[0]*a[0]+out[1]*a[1]+out[2]*a[2]);return out;
}
STUB(native_sqrt,"fsqrt; ret")
void *native_entry,*native_return; float center[3];
STUB(invoke_native,"pushal; mov $_center,%esi; push $1f; push $_collider; push $_manager;"
    "pushl _native_return; jmp *_native_entry; 1: popal; ret")
static void contact_tests(void) {
    *(void **)(image+0x29a2b0)=normalize; *(void **)(image+0x29a2c0)=plane;
    image[0x28fd60]=0xe9; *(int32_t *)(image+0x28fd61)=(int32_t)((uint8_t *)(uintptr_t)native_sqrt-image-0x28fd65);
    image[0x3265f]=0xc3; native_return=image+0x3265f; native_entry=image+0x33010;
    float triangle[9]={-1,0,-1, 1,0,-1, 0,0,1}; memcpy(record+0x34,triangle,sizeof(triangle));
    assert(FlushInstructionCache(GetCurrentProcess(),image,SUDEKIMP_EXPECTED_IMAGE_SIZE));
    reset(); grant=FALSE; center[0]=center[2]=0; center[1]=0.999998f;
    invoke_native(); float retail=center[1];
    printf("native floor baseline %.9f %.9f %.9f\n",center[0],center[1],center[2]);
    assert(retail>1.009f && retail<1.011f);
    grant=TRUE; center[1]=0.999998f; invoke_native();
    assert(center[1]>=1 && center[1]<1.00002f);
    assert(center[0]==0 && center[2]==0);
    printf("IDLE_CONTACT_NATIVE_SOLVER: tiny floor penetration retail=%.9f corrected=%.9f\n",retail,center[1]);
    /* Owner failure: terrain rounding crosses 1e-5 into the neighboring
     * positive-separation branch. Both sides must clear contact without the
     * old .01 lift, including the largest admitted shallow penetration. */
    const float shallow[]={0.999991f,0.999988556f,0.99998f,0.99992f};
    for(unsigned i=0;i<sizeof(shallow)/sizeof(*shallow);++i) {
        grant=FALSE;center[1]=shallow[i];invoke_native();float old=center[1];
        grant=TRUE;center[1]=shallow[i];invoke_native();
        printf("IDLE_CONTACT_BOUNDARY: start=%.9f retail=%.9f corrected=%.9f\n",shallow[i],old,center[1]);
        assert(old>1.009f && center[1]>=1.0f && center[1]<1.00002f);
    }
    /* Significant penetration, air, movement and nonterrain keep retail output. */
    const float heights[]={0.9998f,0.98f,0.5f,1.001f,2};
    for(unsigned i=0;i<sizeof(heights)/sizeof(*heights);++i) {
        grant=FALSE; center[1]=heights[i]; invoke_native(); float expected=center[1];
        grant=TRUE; center[1]=heights[i]; invoke_native(); assert(center[1]==expected);
    }
    *(float *)(movement+0x28)=1; center[1]=0.999998f; invoke_native(); assert(center[1]==retail);
    *(float *)(movement+0x28)=0; *(unsigned *)(record+0x20)=2;
    center[1]=0.999998f; invoke_native(); assert(center[1]==retail);
    puts("IDLE_CONTACT_NATIVE_PASSTHROUGH: air, larger penetration, movement and nonterrain match original");
}
/* A synthetic gentle floor recreates the observed source of the sideways
 * drift. Execute the full native solver, not a duplicate of the policy. */
static void slope_tests(void) {
    const unsigned vts[]={0x2d5a88,0x2d66fc,0x2d5010,0x2d555c};
    float triangle[9]={-1,0.07f,-1, 1,-0.01f,-1, 0,-0.03f,1};
    float n[4];plane(n,triangle,triangle+3,triangle+6);
    if(n[1]<0) for(unsigned i=0;i<4;++i) n[i]=-n[i];
    const float depths[]={0.000002f,0.00002f,0.00008f};
    for(unsigned c=0;c<4;++c) for(unsigned d=0;d<3;++d) {
        reset();*(void **)actor=image+vts[c];movement[0x79]=(c==1 || c==3);
        *(float *)(object+0xe4)=movement[0x79]?-0.004f:-0.2548f;
        memcpy(record+0x34,triangle,sizeof(triangle));
        float start=(1.0f-depths[d])/n[1];
        grant=FALSE;center[0]=center[2]=0;center[1]=start;invoke_native();
        float retail_x=center[0],retail_z=center[2];
        /* Only the small-depth branch moves laterally here. The adjacent
         * branch already suppresses horizontal corrections below .01. */
        if(d==0) assert(hypotf(retail_x,retail_z)>0.0004f);
        else assert(retail_x==0 && retail_z==0);
        grant=TRUE;center[0]=center[2]=0;center[1]=start;invoke_native();
        assert(hypotf(center[0],center[2])<0.000006f);
        assert(center[0]*n[0]+center[1]*n[1]+center[2]*n[2]>=1.0f);
        printf("IDLE_CONTACT_SLOPE: character=%u depth=%.9f retail_x=%.9f corrected_x=%.9f\n",
            c,depths[d],retail_x,center[0]);
        /* Wall/object contacts and intentional movement retain native response. */
        *(unsigned *)(record+0x20)=2;
        center[0]=center[2]=0;center[1]=start;invoke_native();
        assert(center[0]==retail_x && center[2]==retail_z);
        *(unsigned *)(record+0x20)=1;*(float *)(movement+0x24)=1;
        center[0]=center[2]=0;center[1]=start;invoke_native();
        assert(center[0]==retail_x && center[2]==retail_z);
    }
}
int main(int argc,char **argv) {
    wchar_t path[32768]; assert(argc==2 && MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,32768));
    image=map_image(path); contracts(); reset(); thread=GetCurrentThreadId();
    for(unsigned fail=1;fail<=2;++fail) {
        install_calls=0;fail_install=fail;assert(!SudekiMpStoryIdleContactInstall((HMODULE)image,lease));
        assert(SudekiMpStoryIdleContactImageExact(image) && !SudekiMpStoryIdleContactNext[0] && !SudekiMpStoryIdleContactNext[1]);
    }
    fail_install=0; assert(SudekiMpStoryIdleContactInstall((HMODULE)image,lease));
    void *next[2];memcpy(next,SudekiMpStoryIdleContactNext,sizeof(next));
    SudekiMpStoryIdleContactNext[0]=SudekiMpStoryIdleContactNext[1]=probe_return;
    probe_entry=image+0x33288;
    probe_frame[1]=(uintptr_t)(image+0x3265f); probe_frame[2]=(uintptr_t)manager; probe_frame[3]=(uintptr_t)collider;
    run(FALSE); assert(SudekiMpStoryIdleContactBindThread());
    for(test_site=0;test_site<2;++test_site) {
        probe_entry=image+(test_site?0x334a8:0x33288);probe_depth=test_site?0.000012f:0.000002f;
        guard_tests();
    }
    memcpy(SudekiMpStoryIdleContactNext,next,sizeof(next)); contact_tests();slope_tests();
    /* The live game uses single-precision x87 control. Repeat native geometry
     * at that precision as well as the harness's extended-precision default. */
    uint16_t native_cw=0x007f;
    __asm__ volatile("fldcw %0" :: "m"(native_cw)); contact_tests();slope_tests();
    __asm__ volatile("fninit");
    uint8_t saved=image[0x334a9]; image[0x334a9]^=1;
    assert(!SudekiMpStoryIdleContactUninstall() && !memcmp(SudekiMpStoryIdleContactNext,next,sizeof(next)));
    assert(!SudekiMpStoryIdleContactInstall((HMODULE)image,lease));
    image[0x334a9]=saved; assert(SudekiMpStoryIdleContactUninstall());
    assert(!SudekiMpStoryIdleContactNext[0] && !SudekiMpStoryIdleContactNext[1] && SudekiMpStoryIdleContactImageExact(image));
    assert(SudekiMpStoryIdleContactInstall((HMODULE)image,lease)); assert(SudekiMpStoryIdleContactUninstall());
    assert(VirtualFree(image,0,MEM_RELEASE)); puts("story_idle_contact_image_test: PASS"); return 0;
}
