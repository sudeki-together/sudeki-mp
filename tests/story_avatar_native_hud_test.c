/* Exact-image hook gates plus isolated four-widget ownership/presentation fixture.
 * Native widget calls are captured; this is not a live rendering test. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "engine/build_identity.h"
#include "../src/hooks/lan_story_avatar_native_hud.c"
static size_t image_size;
void SudekiMpLogFormat(const char *s,...) { (void)s; }
void SudekiMpLogWrite(const char *s) { (void)s; }
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(f!=INVALID_HANDLE_VALUE); DWORD size=GetFileSize(f,NULL),got=0;
    uint8_t *raw=malloc(size); assert(raw && ReadFile(f,raw,size,&got,NULL) && got==size); CloseHandle(f);
    IMAGE_DOS_HEADER *dos=(void *)raw; IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    image_size=nt->OptionalHeader.SizeOfImage;
    uint8_t *b=VirtualAlloc(NULL,image_size,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    assert(b); memcpy(b,raw,nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *sections=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        IMAGE_SECTION_HEADER *s=&sections[i];
        assert(s->PointerToRawData<=size && s->SizeOfRawData<=size-s->PointerToRawData);
        assert(s->VirtualAddress<=image_size && s->SizeOfRawData<=image_size-s->VirtualAddress);
        memcpy(b+s->VirtualAddress,raw+s->PointerToRawData,s->SizeOfRawData);
    }
    uintptr_t delta=(uintptr_t)b-nt->OptionalHeader.ImageBase;
    IMAGE_DATA_DIRECTORY reloc=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    for(unsigned offset=0;offset<reloc.Size;) {
        IMAGE_BASE_RELOCATION *block=(void *)(b+reloc.VirtualAddress+offset);
        assert(block->SizeOfBlock>=sizeof(*block) && block->SizeOfBlock<=reloc.Size-offset);
        uint16_t *items=(uint16_t *)(block+1);
        for(unsigned i=0;i<(block->SizeOfBlock-sizeof(*block))/2;++i) {
            unsigned type=items[i]>>12,rva=block->VirtualAddress+(items[i]&0xfffu);
            assert(type==IMAGE_REL_BASED_ABSOLUTE || type==IMAGE_REL_BASED_HIGHLOW);
            if(type==IMAGE_REL_BASED_HIGHLOW) { assert(rva<=image_size-4); *(uint32_t *)(b+rva)+=(uint32_t)delta; }
        }
        offset+=block->SizeOfBlock;
    }
    free(raw); return b;
}

static uint8_t ui[0x200],controller[0x100],hud[0x200],animations[4][0x140];
static uint8_t nodes[4][0x20],render_objects[4][0x40],materials[4][0x20];
static uint8_t text_array[0x10],*gizmos[4];
static void *bar_materials[4][2][2];
static int bar_ids[4][2][2];
static float bar_values[4][2][2];
static SudekiMpLanStoryAvatarNativeHudSnapshot model;
static unsigned portrait_calls,bar_calls,text_calls,observations,restores;
static BOOL allow_present=TRUE,allow_restore=TRUE,destroyed,replace_during_portrait;
static wchar_t texts[32][64];
static void put(void *p,unsigned o,void *v) { *(void **)((uint8_t *)p+o)=v; }
static void putword(void *p,unsigned o,uint32_t v) { *(uint32_t *)((uint8_t *)p+o)=v; }
static BOOL observe_fixture(const SudekiMpLanStoryAvatarNativeHudIdentity *id,
    SudekiMpLanStoryAvatarNativeHudOperation op,SudekiMpLanStoryAvatarNativeHudSnapshot *out,void *context) {
    assert(id->world==(void *)0x1234 && id->scene_manager==ui && context==(void *)7); ++observations;
    if(op==SUDEKIMP_AVATAR_NATIVE_HUD_DESTROYED) return destroyed;
    if(op==SUDEKIMP_AVATAR_NATIVE_HUD_RESTORE) return allow_restore;
    if(out) *out=model;
    return allow_present;
}
static void state_fixture(void *element) {
    for(unsigned s=0;s<4;++s) assert(element!=gizmos[s]+0xec);
    unsigned own=word(element,0x1c),parent=word(element,0x20);
    putword(element,0x24,own>parent?own:parent);
    for(unsigned s=0;s<4;++s) if(element==gizmos[s]+4) {
        for(unsigned c=1;c<CHILD_COUNT;++c) {
            if(c==4) continue; /* Native AA170 never registers this child. */
            uint8_t *child=gizmos[s]+child_offsets[c]; putword(child,0x20,word(element,0x24));
            state_fixture(child);
        }
    }
}
static void bar_fixture(void *bar,unsigned index,float value) {
    assert(index<2 && isfinite(value) && value>=0 && value<=1); ++bar_calls;
    ((float *)ptr(bar,0x58))[index]=value;
}
static void portrait_fixture(void *icon,unsigned character) {
    assert(character==5 || character<4); ++portrait_calls;
    ((uint8_t *)icon)[0x2e]=1;
    if(replace_during_portrait) {
        replace_during_portrait=FALSE; ++model.stats.rows[model.stats.local_player].spawn_generation;
    }
}
static void select_fixture(void *icon,unsigned resource) {
    assert(resource>=0x115 && resource<0x125); ++restores; ((uint8_t *)icon)[0x2e]=1;
}
static void text_fixture(const wchar_t *text,int x,int y,unsigned style,uint32_t color) {
    assert(text_calls<32 && x>=0 && y>=0 && style<=2 && color); wcsncpy(texts[text_calls++],text,63);
}
static void fixture(void) {
    memset(ui,0,sizeof(ui)); memset(hud,0,sizeof(hud)); memset(controller,0,sizeof(controller));
    put(base,WORLD,(void *)0x1234); put(base,UI_SCENE,ui); put(base,HUD,hud); put(base,UI_CONTROLLER,controller);
    put(controller,0,base+UI_CONTROLLER_VT); put(controller,0x6c,hud); put(ui,0x170,controller);
    put(ui,0x70,(void *)0x4567); put(ui,0x12c,text_array);
    put(hud,0,base+HUD_VT); put(hud,0x10c,base+GROUP_VT); hud[0x134]=1;
    memset(&model,0,sizeof(model)); model.stats.epoch=8; model.stats.revision=9; model.stats.local_player=2;
    for(unsigned s=0;s<4;++s) {
        gizmos[s]=VirtualAlloc(NULL,0x1000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE); assert(gizmos[s]);
        uint8_t *g=gizmos[s]; put(g,0,base+GIZMO_VT); put(g,4,base+GIZMO_UI_VT);
        putword(g,0x32c,s); putword(g,0x2a8,s); putword(g,0x2d0,520); putword(g,0x2d4,310-74*s);
        put(hud,0x138+4*s,g); put(hud,0x148+4*s,animations[s]); put(g,0x320,animations[s]);
        put(animations[s],0,base+ANIMATED_VT); put(animations[s],0xbc,nodes[s]);
        put(nodes[s],0,base+NODE_VT); put(nodes[s],8,render_objects[s]);
        put(nodes[s],0xc,(void *)(uintptr_t)(0x700+s)); put(nodes[s],0x14,(void *)0x4567);
        put(render_objects[s],0,base+MODEL_RENDER_VT); put(render_objects[s],0x14,ptr(nodes[s],0xc));
        put(materials[s],0,base+MATERIAL_VT);
        for(unsigned c=0;c<CHILD_COUNT;++c) {
            uint8_t *child=g+child_offsets[c]; putword(child,0x1c,2); putword(child,0x24,2);
            if(c==0) continue;
            if(c==5 || c==6) {
                unsigned b=c-5; put(child,0,base+BAR_VT); put(child,0x2c,nodes[s]);
                putword(child,0x50,2); put(child,0x34,bar_materials[s][b]);
                put(child,0x4c,bar_ids[s][b]); put(child,0x58,bar_values[s][b]);
                for(unsigned k=0;k<2;++k) { bar_materials[s][b][k]=materials[s]; bar_ids[s][b][k]=(int)k; }
            } else {
                put(child,0,base+ICON_VT); put(child,0x28,base+ICON_CALLBACK_VT);
                put(child,0x30,nodes[s]); put(child,0x34,materials[s]);
                *(uint16_t *)(child+0x2c)=(uint16_t)c; child[0x2e]=1;
                if(c==4) {
                    put(child,0x30,NULL); put(child,0x34,NULL);
                    *(uint16_t *)(child+0x2c)=UINT16_MAX; child[0x2e]=0;
                    putword(child,0x1c,3); putword(child,0x24,3);
                } else if(c==8 || c==9) { put(child,0x34,NULL); child[0x2e]=0; }
            }
        }
        SudekiMpStoryAvatarStatsRow *r=&model.stats.rows[s];
        r->present=TRUE; r->epoch=8; r->revision=9; r->spawn_generation=s+1; r->sequence=1;
        r->received_tick=GetTickCount(); snprintf(r->name,32,"Player %u",s+1);
        r->hp=25.f*(s+1); r->max_hp=100; r->sp=10*s; r->max_sp=40; model.character[s]=5;
    }
    set_bar=bar_fixture; set_state=state_fixture; set_portrait=portrait_fixture;
    select_resource=select_fixture; queue_text=text_fixture;
}
static void fresh(void) { for(unsigned p=0;p<4;++p) model.stats.rows[p].received_tick=GetTickCount(); }
static SudekiMpLanStoryAvatarNativeHudIdentity tuple(void) {
    return (SudekiMpLanStoryAvatarNativeHudIdentity){(void *)0x1234,ui,7,8,9};
}
static BOOL __attribute__((thiscall)) no_matrix(void *self,int channel) {
    assert(self && (channel==-1 || channel==-2)); return FALSE;
}
static void actual_native_abi(void) {
    /* Run native409930/409810/5B9FC0 with one already allocated native queue
     * item, so no native allocator or render device is required. */
    uint8_t command[0x54]={0}; void *items[1]={command};
    putword(command,0x10,0x80000000u);
    putword(ui,0x11c,1); putword(ui,0x128,0); putword(text_array,4,1); put(text_array,0xc,items);
    identity.scene_manager=ui;
    native_text(L"Talos",501,302,2,0x123456ffu);
    assert(word(ui,0x128)==1 && word(command,0)==0 && word(command,4)==2 &&
        word(command,8)==501 && word(command,12)==302 && word(command,0x4c)==0x123456ffu);
    assert(!wcscmp((const wchar_t *)(command+0x14),L"Talos") && command[0x50]==0);
    identity.scene_manager=NULL; putword(ui,0x11c,0); putword(ui,0x128,0); putword(text_array,4,0);
    /* Execute the actual native quantizing bar setter. Virtual material
     * queries return false, so the call cannot enter D3D or update geometry. */
    void *vt[16]={0}; vt[0x18/4]=no_matrix; void *material=vt;
    void *matrices[2]={&material,&material}; int indices[2]={0,1}; float cache[2]={-1,-1};
    uint8_t bar[0x5c]={0}; putword(bar,0x50,2); put(bar,0x34,matrices);
    put(bar,0x4c,indices); put(bar,0x58,cache);
    native_bar(bar,0,.75f); native_bar(bar,1,0.f);
    /* Native texture coordinates use half the incoming ratio and truncate
     * to hundredths: this is deliberately not a normalized ratio cache. */
    assert(fabsf(cache[0]-.37f)<.0001f && cache[1]==0.f);
}
int wmain(int argc,wchar_t **argv) {
    assert(argc==2); uint8_t *image=map_image(argv[1]);
    /* A foreign last call must reject and roll back all earlier calls. */
    uint8_t before_calls[8][5];
    for(unsigned n=0;n<8;++n) memcpy(before_calls[n],image+call_rvas[n],5);
    image[call_rvas[7]]=0x90;
    assert(!SudekiMpLanStoryAvatarNativeHudInstall((HMODULE)image) && !base);
    for(unsigned n=0;n<7;++n) assert(!memcmp(before_calls[n],image+call_rvas[n],5));
    memcpy(image+call_rvas[7],before_calls[7],5);
    assert(SudekiMpLanStoryAvatarNativeHudInstall((HMODULE)image));
    fixture(); SudekiMpLanStoryAvatarNativeHudIdentity id=tuple();
    assert(SudekiMpLanStoryAvatarNativeHudBind(&id,observe_fixture,(void *)7));
    assert(!SudekiMpLanStoryAvatarNativeHudBind(&id,observe_fixture,(void *)7));
    /* Real AA170 layouts leave child4 dormant and child9 geometry-only.
     * Partial/foreign dormant identity must still reject before any writes. */
    put(gizmos[0]+0xec,0x30,nodes[0]);
    assert(!SudekiMpLanStoryAvatarNativeHudService() && !layer && !portrait_calls && !bar_calls);
    put(gizmos[0]+0xec,0x30,NULL);
    put(gizmos[0]+0x264,0x34,(void *)0xdead);
    assert(!SudekiMpLanStoryAvatarNativeHudService() && !layer && !portrait_calls && !bar_calls);
    put(gizmos[0]+0x264,0x34,NULL);
    assert(SudekiMpLanStoryAvatarNativeHudService());
    assert(portrait_calls==4 && bar_calls==16 && SudekiMpLanStoryAvatarNativeHudActive());
    assert(bar_values[0][0][0]==.75f && bar_values[1][0][0]==.25f && bar_values[2][0][0]==.5f);
    for(unsigned s=0;s<4;++s) {
        assert(word(gizmos[s],0x28)==0 && word(gizmos[s],0x2ac)==0);
        assert(unused_icon_exact(gizmos[s]+0xec));
        assert(!ptr(gizmos[s]+0x264,0x34) && word(gizmos[s]+0x264,0x24)==0);
    }
    draw_entry(hud+0x10c); assert(text_calls==8 && !wcscmp(texts[0],L"Player 3"));
    assert(!wcscmp(texts[1],L"75") && !wcscmp(texts[3],L"20"));
    fresh(); model.stats.rows[0].received_tick-=251; model.stats.rows[1].hp=NAN;
    model.stats.rows[2].sp=model.stats.rows[2].max_sp=0;
    assert(SudekiMpLanStoryAvatarNativeHudService());
    assert(word(gizmos[1],0x28)==2 && word(gizmos[2],0x28)==2 && bar_values[0][1][0]==0);
    fresh(); model.stats.rows[1].hp=50;
    put(gizmos[3]+0x2c,0x30,(void *)0xdead);
    assert(!SudekiMpLanStoryAvatarNativeHudService() && SudekiMpLanStoryAvatarNativeHudRetains());
    put(gizmos[3]+0x2c,0x30,nodes[3]); assert(SudekiMpLanStoryAvatarNativeHudService());
    putword(gizmos[3]+0xec,0x1c,2);
    assert(!SudekiMpLanStoryAvatarNativeHudService() && SudekiMpLanStoryAvatarNativeHudRetains());
    putword(gizmos[3]+0xec,0x1c,3); fresh(); assert(SudekiMpLanStoryAvatarNativeHudService());
    DWORD old; assert(VirtualProtect(gizmos[3],0x1000,PAGE_NOACCESS,&old));
    assert(!SudekiMpLanStoryAvatarNativeHudService());
    assert(VirtualProtect(gizmos[3],0x1000,old,&old));
    rows[0].character=UINT32_MAX; replace_during_portrait=TRUE;
    assert(!SudekiMpLanStoryAvatarNativeHudService());
    for(unsigned s=0;s<4;++s) assert(word(gizmos[s],0x28)==2);
    fresh(); assert(SudekiMpLanStoryAvatarNativeHudService());
    allow_present=FALSE; assert(!SudekiMpLanStoryAvatarNativeHudService());
    for(unsigned s=0;s<4;++s) assert(word(gizmos[s],0x28)==2);
    allow_present=TRUE; fresh(); assert(SudekiMpLanStoryAvatarNativeHudService());
    allow_restore=FALSE; assert(!SudekiMpLanStoryAvatarNativeHudUnbind()); allow_restore=TRUE;
    putword(gizmos[3],0x20,3); assert(!SudekiMpLanStoryAvatarNativeHudUnbind());
    assert(rows[0].restored && rows[1].restored && rows[2].restored && !rows[3].restored);
    putword(gizmos[3],0x20,rows[3].owned_request[0]);
    assert(SudekiMpLanStoryAvatarNativeHudUnbind() && restores==4);
    assert(!SudekiMpLanStoryAvatarNativeHudRetains());
    actual_native_abi();
    fresh(); assert(SudekiMpLanStoryAvatarNativeHudBind(&id,observe_fixture,(void *)7));
    assert(SudekiMpLanStoryAvatarNativeHudService());
    destroyed=TRUE;
    assert(!SudekiMpLanStoryAvatarNativeHudNativeExitReturned());
    /* A caller assertion alone cannot retire the retained layer. With the
     * native singleton cleared after Quit, no old widget read is allowed. */
    for(unsigned s=0;s<4;++s) assert(VirtualProtect(gizmos[s],0x1000,PAGE_NOACCESS,&old));
    assert(!SudekiMpLanStoryAvatarNativeHudNativeExitReturned());
    put(image,HUD,NULL);
    assert(SudekiMpLanStoryAvatarNativeHudNativeExitReturned());
    /* A foreign call during uninstall retains its exact ownership record;
     * other hooks may restore, and a retry completes after owner recovery. */
    int32_t ours=call_hooks[0].replacement_displacement,foreign=ours+1;
    memcpy(image+call_rvas[0]+1,&foreign,4);
    assert(!SudekiMpLanStoryAvatarNativeHudUninstall() && base==image && call_hooks[0].installed);
    memcpy(image+call_rvas[0]+1,&ours,4);
    assert(SudekiMpLanStoryAvatarNativeHudUninstall());
    for(unsigned n=0;n<8;++n) assert(!memcmp(before_calls[n],image+call_rvas[n],5));
    for(unsigned s=0;s<4;++s) VirtualFree(gizmos[s],0,MEM_RELEASE);
    VirtualFree(image,0,MEM_RELEASE);
    puts("StoryAvatarNativeHudTest PASS: exact-image call/vtable hooks and native text/bar ABI; four widget fixtures; local-first, independent ratios/text, zero SP, stale/invalid omission, owner/NOACCESS rejection, generation change, partial restoration retry, positive destruction, foreign-hook retry");
    return 0;
}
