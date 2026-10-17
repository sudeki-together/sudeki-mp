/* H1b Kamo gestures: the exact-image guards of the native animation update
 * the client runs, then the driver's state machine against a fake
 * shopkeeper with the native routines it calls jumped to stand-ins (no
 * native routine executes here). */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/hooks/lan_story_npc_gesture.c"
void SudekiMpLogWrite(const char *text) {(void)text;}
void SudekiMpLogFormat(const char *format,...) {(void)format;}
static BOOL local_npc,clock_on; static float clock_dt=0.016f;
BOOL SudekiMpLanStoryWorldLocalIdleNpc(const void *entity) {(void)entity;return local_npc;}
BOOL SudekiMpLanStoryAmbientFrameDelta(float *dt) {*dt=clock_on?clock_dt:0.0f;return clock_on;}
BOOL SudekiMpCheckLoadedExecutable(HMODULE image) {(void)image;return TRUE;}
static uint8_t *map_image(const wchar_t *path) {
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(file!=INVALID_HANDLE_VALUE); DWORD size=GetFileSize(file,NULL),got=0;
    uint8_t *raw=malloc(size); assert(raw && ReadFile(file,raw,size,&got,NULL) && got==size); CloseHandle(file);
    IMAGE_DOS_HEADER *dos=(void *)raw; IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *b=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    assert(b); memcpy(b,raw,nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *sec=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) memcpy(b+sec[i].VirtualAddress,raw+sec[i].PointerToRawData,sec[i].SizeOfRawData);
    uintptr_t delta=(uintptr_t)b-nt->OptionalHeader.ImageBase;
    IMAGE_DATA_DIRECTORY reloc=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    for(unsigned off=0;off<reloc.Size;) {
        IMAGE_BASE_RELOCATION *blk=(void *)(b+reloc.VirtualAddress+off);
        uint16_t *items=(uint16_t *)(blk+1); unsigned n=(blk->SizeOfBlock-sizeof(*blk))/2;
        for(unsigned i=0;i<n;++i) if(items[i]>>12==IMAGE_REL_BASED_HIGHLOW) *(uint32_t *)(b+blk->VirtualAddress+(items[i]&0xfffu))+=(uint32_t)delta;
        off+=blk->SizeOfBlock;
    }
    free(raw); return b;
}

/* ---- the driver's state machine against a fake shopkeeper; the native
 * routines it calls are jumps to stand-ins that model the lock steps. */
static uint8_t *ent,*gma_p,*pos_p,*wrap_p,*rend_p;
static unsigned loads,loaded_after,process_calls,updates,pending_events,pops; static BOOL endless_events;
static unsigned dumps; static void *dump_eax;
static void __attribute__((stdcall)) s_load(void *gma,int bank,int a,int b) { (void)a;(void)b; assert(gma==gma_p && bank==5); ++loads; }
static uint8_t __attribute__((stdcall)) s_loaded(void *gma,int bank) { assert(gma==gma_p && bank==5); if(loaded_after) { --loaded_after; return 0; } return 1; }
static uint8_t __attribute__((stdcall)) s_in_use(void *gma,int bank) { (void)gma;(void)bank; return 0; }
static void __attribute__((stdcall)) s_process(void *gma) { assert(gma==gma_p); ++process_calls; *(uint32_t *)(gma_p+0x5c)=0; }
static uint8_t __attribute__((thiscall)) s_pop(void *r,void *out) {
    assert(r==rend_p && out);
    ++pops;
    if(endless_events) return 1;
    if(!pending_events) return 0;
    --pending_events;
    return 1;
}
static void __attribute__((thiscall)) s_update(void *gma,void *ctx) {
    assert(gma==gma_p && ctx && !pending_events);              /* events discarded first */
    float dt; memcpy(&dt,(uint8_t *)ctx+0xc,4); assert(dt>0 && dt<=0.05f); ++updates;
    uint8_t *lock=gma_p+0x131;
    if(*lock==1) *lock=2; else if(*lock==4) *lock=0;
}
/* Dump 0xE0E20: EAX = gma, push bank, RET 4. */
static void __attribute__((used,cdecl)) c_dump(void *gma,int bank) {
    assert(gma==gma_p && bank==5); dump_eax=gma; ++dumps; *(uint32_t *)(gma_p+0x5c)=1;
}
static void __attribute__((naked)) s_dump(void) {
    __asm__ volatile("pushl 4(%esp)\n\tpushl %eax\n\tcall _c_dump\n\taddl $8,%esp\n\tret $4\n\t");
}
static void jump(uint8_t *image,unsigned rva,const void *to) {
    image[rva]=0xe9; int32_t rel=(int32_t)((const uint8_t *)to-(image+rva+5)); memcpy(image+rva+1,&rel,4);
}
static uint16_t *stamp_p; static DWORD now_ms=100000;
static void frame(void) { ++*stamp_p; now_ms+=16; SudekiMpLanStoryNpcGestureTick(now_ms); }
static void fake_kamo(uint8_t *image) {
    static uint8_t e[0x900],pos[0xc0],wrap[0x18],rend[0xb0],table[0x14+4*0xb2],entries[8][0x30],record[0x10],owner[0x40];
    static void *entries_list[1];
    memset(e,0,sizeof(e)); memset(pos,0,sizeof(pos));
    ent=e; gma_p=e+0x3a4; pos_p=pos; wrap_p=wrap; rend_p=rend;
    *(void **)e=image+0x2d65a0; *(void **)(e+0x130)=gma_p; *(void **)(e+0x58)=gma_p; *(void **)(e+0x90)=e; *(void **)(e+0x44)=pos;
    *(void **)gma_p=image+0x2c8504; *(void **)(gma_p+0x10)=e; gma_p[0x133]=4; *(void **)(gma_p+0xdc)=table; *(void **)(gma_p+0xf8)=record;
    record[2]=2; record[0xa]=0;
    static const unsigned anims[8]={0,2,0xab,0xac,0xad,0xae,0xaf,0xb0};
    for(unsigned i=0;i<8;++i) { uint32_t none=0x7ffff; memcpy(entries[i]+0x2c,&none,4); *(void **)(table+0x14+4*anims[i])=entries[i]; }
    *(void **)pos=image+0x2cdefc; *(void **)(pos+0x10)=e; *(void **)(pos+0xb4)=wrap;
    *(void **)(wrap+8)=wrap; *(void **)(wrap+0xc)=rend; *(void **)(wrap+0x10)=rend; *(void **)(wrap+0x14)=wrap;
    *(void **)rend=image+0x2df8ec;
    entries_list[0]=e; uint32_t one=1; memcpy(owner+0x34,&one,4); *(void **)(owner+0x3c)=entries_list;
    *(void **)(image+0x409d8c)=owner;
}
static void flow(uint8_t *image) {
    assert(SudekiMpLanStoryNpcGestureInstall((HMODULE)image));
    jump(image,0xe0ea0,(const void *)(uintptr_t)s_load); jump(image,0xe0f30,(const void *)(uintptr_t)s_loaded);
    jump(image,0xe1000,(const void *)(uintptr_t)s_in_use); jump(image,0xe01c0,(const void *)(uintptr_t)s_process);
    jump(image,0xe0e20,(const void *)(uintptr_t)s_dump); jump(image,0x223880,(const void *)(uintptr_t)s_pop);
    jump(image,0xe1930,(const void *)(uintptr_t)s_update);
    assert(FlushInstructionCache(GetCurrentProcess(),image,0x400000));
    stamp_p=(uint16_t *)(image+0x3c3150); fake_kamo(image);
    /* 1. Not local or no clock: never started. */
    local_npc=FALSE; clock_on=TRUE; assert(!SudekiMpLanStoryNpcGestureBegin(ent,now_ms));
    local_npc=TRUE; clock_on=FALSE; assert(!SudekiMpLanStoryNpcGestureBegin(ent,now_ms)); clock_on=TRUE;
    /* 2. Bank requested, then loaded; ready while idle on anim 2. */
    loads=0; loaded_after=3;
    assert(SudekiMpLanStoryNpcGestureBegin(ent,now_ms) && loads==1 && !SudekiMpLanStoryNpcGestureReady());
    for(unsigned i=0;i<4;++i) frame();
    assert(SudekiMpLanStoryNpcGestureReady() && !updates);                        /* lock 0: no update */
    /* 3. A line: lock 1 -> 2 (update), 3 held, 4 -> 0, a short tail, events discarded first. */
    gma_p[0x131]=1; pending_events=5; frame(); assert(updates==1 && gma_p[0x131]==2 && pops>=5);
    gma_p[0x131]=3; frame(); frame(); assert(updates==3);
    unsigned before=updates; uint16_t same=*stamp_p; SudekiMpLanStoryNpcGestureTick(now_ms); assert(updates==before && *stamp_p==same); /* once per frame */
    gma_p[0x131]=4; frame(); assert(gma_p[0x131]==0);
    before=updates; frame(); assert(updates==before+1);                          /* the tail */
    for(unsigned i=0;i<40;++i) frame();
    before=updates; frame(); assert(updates==before);                            /* tail over */
    /* 4. A line still waiting for its lock (1) stalls after 1.5 s (clock stopped here). */
    uint8_t cs[0x480]={0}; cs[0x474]=1; uint32_t one=1; memcpy(cs+0xc8,&one,4);
    gma_p[0x131]=2; assert(!SudekiMpLanStoryNpcGestureStalled(cs,now_ms));
    gma_p[0x131]=1; assert(!SudekiMpLanStoryNpcGestureStalled(cs,now_ms)); assert(!SudekiMpLanStoryNpcGestureStalled(cs,now_ms+1000));
    assert(SudekiMpLanStoryNpcGestureStalled(cs,now_ms+1600) && !SudekiMpLanStoryNpcGestureReady());   /* degraded */
    gma_p[0x131]=0;
    /* 5. End: settled -> dump queued (EAX = gma) -> processed -> idle. */
    BOOL done=FALSE; for(unsigned i=0;i<60 && !done;++i) { ++*stamp_p; now_ms+=16; done=SudekiMpLanStoryNpcGestureEnd(now_ms); }
    assert(done && dumps==1 && dump_eax==gma_p && process_calls>=1 && SudekiMpLanStoryNpcGestureIdle());
    puts("Gesture driver: bank load, lock 1->2->3->4->0 ticks with events discarded, once per frame, tail, stall, end and dump: PASS");
    /* 6. The clock goes away while a lock is held: stops animating, reports the line once. */
    loaded_after=0; assert(SudekiMpLanStoryNpcGestureBegin(ent,now_ms) && SudekiMpLanStoryNpcGestureReady());
    gma_p[0x131]=3; clock_on=FALSE; frame(); now_ms+=300; frame();
    assert(!SudekiMpLanStoryNpcGestureReady() && SudekiMpLanStoryNpcGestureStalled(cs,now_ms) && !SudekiMpLanStoryNpcGestureStalled(cs,now_ms));
    clock_on=TRUE; gma_p[0x131]=0; SudekiMpLanStoryNpcGestureDrain("test");
    /* 7. Kamo stops being local mid-lock: drained, the line reported once, nothing more touched. */
    assert(SudekiMpLanStoryNpcGestureBegin(ent,now_ms)); gma_p[0x131]=3; frame();
    local_npc=FALSE; before=updates; frame();
    assert(SudekiMpLanStoryNpcGestureIdle() && updates==before && SudekiMpLanStoryNpcGestureStalled(cs,now_ms));
    local_npc=TRUE; gma_p[0x131]=0;
    /* 8. A different object at the same address (new sub-objects): not ours. */
    assert(SudekiMpLanStoryNpcGestureBegin(ent,now_ms)); gma_p[0x131]=1;
    static uint8_t pos2[0xc0]; memcpy(pos2,pos_p,sizeof(pos2)); *(void **)(ent+0x44)=pos2;
    before=updates; frame(); assert(SudekiMpLanStoryNpcGestureIdle() && updates==before);
    *(void **)(ent+0x44)=pos_p; gma_p[0x131]=0; (void)SudekiMpLanStoryNpcGestureStalled(cs,now_ms);
    /* 9. An event queue that never empties: no update, drained. */
    assert(SudekiMpLanStoryNpcGestureBegin(ent,now_ms)); gma_p[0x131]=1; endless_events=TRUE;
    before=updates; frame(); assert(SudekiMpLanStoryNpcGestureIdle() && updates==before);
    endless_events=FALSE; gma_p[0x131]=0;
    assert(SudekiMpLanStoryNpcGestureUninstall());
    puts("Gesture driver: no clock, NPC no longer local, replaced NPC, endless events -> stops, never updates: PASS");
}
int main(int argc,char **argv) {
    wchar_t path[1024];
    assert(argc==2 && MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,1024));
    uint8_t *image=map_image(path);
    assert(!SudekiMpLanStoryNpcGestureInstall((HMODULE)image));                   /* switch off */
    SudekiMpLanStoryNpcGestureEnable(TRUE);
    assert(SudekiMpLanStoryNpcGestureInstall((HMODULE)image) && SudekiMpLanStoryNpcGestureUninstall());
    unsigned rows=0;
    for(unsigned k=0;k<sizeof(gesture_codes)/sizeof(gesture_codes[0]);++k,++rows) {
        unsigned first=gesture_codes[k].rva,last=gesture_codes[k].rva+gesture_codes[k].size-1u;
        if(first>=EFFECT_RETIRE_CALL && first<EFFECT_RETIRE_CALL+EFFECT_RETIRE_SIZE) continue;
        image[first]^=1; assert(!SudekiMpLanStoryNpcGestureInstall((HMODULE)image)); image[first]^=1;
        image[last]^=1; assert(!SudekiMpLanStoryNpcGestureInstall((HMODULE)image)); image[last]^=1;
    }
    for(unsigned k=0;k<sizeof(gesture_extra_codes)/sizeof(gesture_extra_codes[0]);++k,++rows) {
        unsigned first=gesture_extra_codes[k].rva;
        image[first]^=1; assert(!SudekiMpLanStoryNpcGestureInstall((HMODULE)image)); image[first]^=1;
    }
    for(unsigned j=0;j<sizeof(gesture_relocations)/sizeof(gesture_relocations[0]);++j) {
        image[gesture_relocations[j].rva]^=1; assert(!SudekiMpLanStoryNpcGestureInstall((HMODULE)image)); image[gesture_relocations[j].rva]^=1;
    }
    for(unsigned k=0;k<sizeof(gesture_vtable_slots)/sizeof(gesture_vtable_slots[0]);++k) {
        uint8_t *slot=image+gesture_vtable_slots[k].vtable_rva+gesture_vtable_slots[k].slot_offset;
        slot[0]^=1; assert(!SudekiMpLanStoryNpcGestureInstall((HMODULE)image)); slot[0]^=1;
    }
    /* The effect_retire seam: its relative call may be retargeted (its owner's
     * hook), but must stay a call. */
    uint8_t saved[5]; memcpy(saved,image+EFFECT_RETIRE_CALL,5);
    image[EFFECT_RETIRE_CALL+1]^=0x55; image[EFFECT_RETIRE_CALL+4]^=0x11;
    assert(SudekiMpLanStoryNpcGestureInstall((HMODULE)image) && SudekiMpLanStoryNpcGestureUninstall());
    image[EFFECT_RETIRE_CALL]=0x90; assert(!SudekiMpLanStoryNpcGestureInstall((HMODULE)image));
    memcpy(image+EFFECT_RETIRE_CALL,saved,5);
    assert(SudekiMpLanStoryNpcGestureInstall((HMODULE)image));
    /* Nothing proven local: a conversation never starts gestures. */
    static uint8_t fake[0x600]; assert(!SudekiMpLanStoryNpcGestureBegin(fake,1000) && SudekiMpLanStoryNpcGestureIdle());
    assert(SudekiMpLanStoryNpcGestureUninstall());
    flow(image);
    printf("StoryNpcGestureImageTest: PASS (%u guarded routines, %u relocations, %u vtable slots, effect_retire seam call-only)\n",rows,
        (unsigned)(sizeof(gesture_relocations)/sizeof(gesture_relocations[0])),(unsigned)(sizeof(gesture_vtable_slots)/sizeof(gesture_vtable_slots[0])));
    VirtualFree(image,0,MEM_RELEASE); return 0;
}
