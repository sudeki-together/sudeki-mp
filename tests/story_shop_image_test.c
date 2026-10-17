/* H1b client shop on the exact image: entry guards, the owned skip of the
 * shop Show's 0x40AFD0 call, and the open/closed witness on synthetic UI
 * objects. No shop export, UI or inventory routine executes. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/hooks/lan_story_shop.c"
void SudekiMpLogWrite(const char *text) {(void)text;}
void SudekiMpLogFormat(const char *format,...) {(void)format;}
BOOL SudekiMpSolShopStockLoad(HMODULE game,SudekiMpSolShopStock *out) {(void)game;memset(out,0,sizeof(*out));return FALSE;}
const SudekiMpSolShopTier *SudekiMpSolShopStockTier(const SudekiMpSolShopStock *s,int block) {(void)block;return s->tier_count?&s->tiers[0]:NULL;}
static uint32_t icon_bits; static BOOL icon_owner=TRUE,exit_held;
BOOL SudekiMpLanStoryRemoteUseIconSetLocal(uint32_t bits) {icon_bits=bits;return icon_owner;}
void SudekiMpLanStoryRemoteInteriorClientHoldExit(BOOL hold) {exit_held=hold;}
BOOL SudekiMpLanStoryMainMenuVisible(void) {return FALSE;}
static BOOL examine_active; static unsigned examine_requests; static uint16_t examine_text;
BOOL SudekiMpLanStoryZoneBannerExamine(const char *zone,uint16_t text,unsigned mode,uint32_t flags,const char *emitter,uint32_t now) {
    (void)mode;(void)now; assert(zone && !strcmp(zone,"LNBr_Kamo_shop") && flags==0x30 && emitter && !strcmp(emitter,"gramophone"));
    ++examine_requests; examine_text=text; examine_active=TRUE; return TRUE;
}
BOOL SudekiMpLanStoryZoneBannerExamineActive(void) {return examine_active;}
BOOL SudekiMpLanStoryCinematicAudioRetains(void) {return FALSE;}
static const void *kamo; static float kamo_at[3],tal_at[3];
static uint8_t gramophone[4]; static float gramophone_at[3]={-3.24361f,0.0f,-3.1753f};
const void *SudekiMpLanStoryRemoteInteriorEntityNamed(const char *name,const void *cached,DWORD *scan,uint32_t now) {
    (void)cached;(void)scan;(void)now;
    return name && !strcmp(name,"NPC_Kamo_NB")?kamo:name && !strcmp(name,"NB_gramophone_trig")?(const void *)gramophone:NULL;
}
BOOL SudekiMpLanStoryRemoteInteriorEntityPosition(const void *e,float out[3]) {
    if(e==kamo && kamo) { memcpy(out,kamo_at,12); return TRUE; }
    if(e==(const void *)gramophone) { memcpy(out,gramophone_at,12); return TRUE; }
    if(e==(const void *)tal_at) { memcpy(out,tal_at,12); return TRUE; }
    return FALSE;
}
static uint8_t *map_image(const wchar_t *path) {
    SudekiMpBuildCheck check;
    assert(SudekiMpCheckExecutableFile(path,&check) && check.hash_matches && check.pe_matches);
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    assert(file!=INVALID_HANDLE_VALUE); DWORD size=GetFileSize(file,NULL),got=0;
    uint8_t *raw=malloc(size); assert(raw && ReadFile(file,raw,size,&got,NULL) && got==size); CloseHandle(file);
    IMAGE_DOS_HEADER *dos=(void *)raw;
    IMAGE_NT_HEADERS32 *nt=(void *)(raw+dos->e_lfanew);
    uint8_t *base=VirtualAlloc(NULL,nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    assert(base); memcpy(base,raw,nt->OptionalHeader.SizeOfHeaders);
    IMAGE_SECTION_HEADER *sections=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        IMAGE_SECTION_HEADER *s=&sections[i];
        assert(s->PointerToRawData<=size && s->SizeOfRawData<=size-s->PointerToRawData);
        assert(s->VirtualAddress<=nt->OptionalHeader.SizeOfImage &&
            s->SizeOfRawData<=nt->OptionalHeader.SizeOfImage-s->VirtualAddress);
        memcpy(base+s->VirtualAddress,raw+s->PointerToRawData,s->SizeOfRawData);
    }
    uintptr_t delta=(uintptr_t)base-nt->OptionalHeader.ImageBase;
    IMAGE_DATA_DIRECTORY reloc=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    unsigned offset=0;
    while(offset<reloc.Size) {
        IMAGE_BASE_RELOCATION *block=(void *)(base+reloc.VirtualAddress+offset);
        assert(block->SizeOfBlock>=sizeof(*block) && block->SizeOfBlock<=reloc.Size-offset);
        uint16_t *items=(uint16_t *)(block+1);
        unsigned count=(block->SizeOfBlock-sizeof(*block))/2;
        for(unsigned i=0;i<count;++i) {
            unsigned type=items[i]>>12,rva=block->VirtualAddress+(items[i]&0xfffu);
            assert(type==IMAGE_REL_BASED_ABSOLUTE || type==IMAGE_REL_BASED_HIGHLOW);
            if(type==IMAGE_REL_BASED_HIGHLOW) {
                assert(rva<=nt->OptionalHeader.SizeOfImage-4);
                *(uint32_t *)(base+rva)+=(uint32_t)delta;
            }
        }
        offset+=block->SizeOfBlock;
    }
    assert(FlushInstructionCache(GetCurrentProcess(),base,nt->OptionalHeader.SizeOfImage));
    free(raw); return base;
}

static void ptr(void *at,void *value) {memcpy(at,&value,4);}
static void num(void *at,uint32_t value) {memcpy(at,&value,4);}

/* ---- Kamo's conversation: a small native model behind jumps patched over
 * the routines the lease calls (after the guard checks on the real bytes). */
static uint8_t *m_cs,*m_ui,*m_hud,*m_shop,*m_scene,*m_group;
static char calls[2048];
static int asked_id,asked_animate=-1,asked_show=-1,line_id,line_b1=-1,line_b3=-1; static void *asked_speaker,*asked_listener;
static unsigned starts,shop_backs;
static uint16_t m_text[]={'H','i',0};
static void note(const char *what) { if(strlen(calls)+strlen(what)+2<sizeof(calls)) { strcat(calls,what); strcat(calls,","); } }
static uint32_t rd(const uint8_t *p) { uint32_t v; memcpy(&v,p,4); return v; }
static void set_focus(void *layer) { memcpy(m_scene+0x170,&layer,4); }
static uint8_t __attribute__((thiscall)) stub_ask(void *cs,void *spk,void *lst,int id,uint8_t u,uint8_t animate,uint8_t choice,uint8_t show) {
    (void)u;(void)choice; assert(cs==m_cs); note("ask"); asked_id=id; asked_animate=animate; asked_show=show;
    asked_speaker=spk?*(void **)spk:NULL; asked_listener=lst;
    num(m_cs+0xc8,0); void *t=m_text; memcpy(m_cs+0xd8,&t,4); memcpy(m_cs+0xdc,&t,4); memcpy(m_cs+0xe0,&t,4);
    float d=1.0f; memcpy(m_cs+0xd0,&d,4); m_cs[0x47a]=1; m_ui[0x6b8]|=0x20; num(m_ui+0x6a4,0);
    return 1;
}
static uint8_t __attribute__((thiscall)) stub_line(void *cs,void *spk,void *lst,int id,uint8_t b1,uint8_t b2,uint8_t b3,uint8_t b4) {
    (void)spk;(void)lst;(void)b2;(void)b4; assert(cs==m_cs); note("say"); line_id=id; line_b1=b1; line_b3=b3;
    num(m_cs+0xc8,b1==0); void *t=m_text; memcpy(m_cs+0xd8,&t,4); float d=1.0f; memcpy(m_cs+0xd0,&d,4);
    return 1;
}
static int __attribute__((thiscall)) stub_answer(void *cs) {
    assert(cs==m_cs); if(!m_cs[0x47a] || !rd(m_ui+0x6a4)) return 0;
    m_cs[0x47a]=0; note("answer"); return (int)rd(m_ui+0x6a4);
}
static uint8_t __attribute__((thiscall)) stub_talking(void *cs) { assert(cs==m_cs); return rd(m_cs+0xc8)!=2 || rd(m_ui+0x4c)==3; }
static void __attribute__((thiscall)) stub_stop(void *cs) { assert(cs==m_cs); note("stop"); num(m_cs+0xc8,2); }
static void __attribute__((thiscall)) stub_gel(void *cs,uint8_t b) { assert(cs==m_cs); note(b?"gel1":"gel0"); m_cs[0x47d]=b; }
static void __attribute__((thiscall)) stub_cam(void *cs,uint8_t b) { assert(cs==m_cs); note(b?"cam1":"cam0"); m_cs[0x475]=b; }
static void __attribute__((thiscall)) stub_igm(void *scene,int st) {
    assert(scene==m_scene); note("igm"); num(m_hud+HUD_STATE,(uint32_t)st); num(m_hud+HUD_REQUEST,(uint32_t)st);
    set_focus(st==2?(void *)m_ui:(void *)m_hud);
}
static uint8_t __attribute__((thiscall)) stub_dialog_input(void *ui,unsigned kind,unsigned cmd,unsigned x) {
    (void)x; assert(ui==m_ui);
    if((kind==5 || kind==6) && (cmd==0 || cmd==2)) {
        if(kind==5 && !(m_ui[0x6b8]&4)) { m_ui[0x6b8]|=0x0c; if(rd(m_ui+0x4c)==3) { note("confirm"); num(m_ui+0x6a4,1); num(m_ui+0x4c,4); } }
        else if(kind==6) m_ui[0x6b8]&=0xf3;
    }
    return 1;
}
static void __cdecl stub_dialog_start(void) { note("start"); ++starts; m_ui[0x6b8]|=0x80; }
static void __cdecl stub_dialog_end(void) { note("end"); m_ui[0x6b8]=(uint8_t)((m_ui[0x6b8]&0x1e)|0x40); }
static void __cdecl stub_dialog_clear(void) { note("clear"); }
static uint8_t __cdecl stub_dialog_active(void) { return rd(m_ui+0x4c)!=0; }
static void __cdecl stub_show_ingame(void) { note("menus"); num(m_hud+HUD_STATE,1); num(m_hud+HUD_REQUEST,1); set_focus(m_hud); }
static BOOL shop_start_pending;
static uint8_t __cdecl stub_shop_start(int b) {
    (void)b; note("shop_start");
    if(shop_start_pending) { num(m_hud+HUD_STATE,0); num(m_hud+HUD_REQUEST,7); return 1; }
    if(rd(m_hud+HUD_STATE)==2) { num(m_hud+HUD_STATE,8); num(m_hud+HUD_REQUEST,7); num(m_shop+SHOP_STATE,7); set_focus(m_shop); }
    return 1;
}
static void __cdecl stub_shop_clear(void) { note("shop_clear"); }
static void __cdecl stub_int(int v) { (void)v; }
static uint8_t __cdecl stub_mode(int v) { (void)v; return 1; }
static void __cdecl stub_price(int a,int b) { (void)a;(void)b; }
static int __cdecl stub_quantity(int id) { (void)id; return 1; }
static uint8_t __attribute__((thiscall)) stub_shop_input(void *layer,unsigned kind,unsigned cmd,unsigned x) {
    (void)kind;(void)x; assert(layer==m_shop); if(cmd==1) { note("shop_back"); ++shop_backs; } return 1;
}
static void jump(uint8_t *image,unsigned rva,const void *to) {
    image[rva]=0xe9; int32_t rel=(int32_t)((const uint8_t *)to-(image+rva+5)); memcpy(image+rva+1,&rel,4);
}
/* What the native UI does between this client's frames. */
static void native_tick(void) {
    if(rd(m_cs+0xc8)!=2) num(m_cs+0xc8,2);                                  /* the line ends */
    if((m_ui[0x6b8]&0x20) && rd(m_cs+0xc8)==2 && rd(m_ui+0x4c)==2) { m_ui[0x6b8]&=~0x20; num(m_ui+0x4c,3); } /* menu */
    if(rd(m_ui+0x4c)==4) num(m_ui+0x4c,2);                                 /* answered */
    if((m_ui[0x6b8]&0x40) && rd(m_ui+0x4c)==2) { m_ui[0x6b8]&=~0x40; num(m_ui+0x4c,0); }  /* closed */
    if(rd(m_hud+HUD_STATE)==2 && !rd(m_ui+0x4c) && (m_ui[0x6b8]&0x80)) num(m_ui+0x4c,2);  /* dialog open */
}
static void choose(unsigned answer) { num(m_ui+0x6a4,answer); num(m_ui+0x4c,4); }
static void shop_back_native(void) { num(m_shop+SHOP_STATE,0); num(m_hud+HUD_STATE,0); num(m_hud+HUD_REQUEST,0); set_focus(m_hud); }
static unsigned step(DWORD *t,unsigned frames) {
    for(unsigned i=0;i<frames;++i) { native_tick(); SudekiMpLanStoryShopService(tal_at,"LNBr_Kamo_shop",FALSE,(*t)++); }
    return phase;
}
static void talk_reset_model(uint8_t *image) {
    memset(m_cs,0,0x480); memset(m_ui,0,0x6bc); ptr(m_cs,image+RVA_SPEECH_VTABLE); ptr(m_ui,image+RVA_DIALOG_VTABLE);
    num(m_cs+0xc8,2); m_cs[0x475]=1; m_ui[0x6b8]=0x10;
    static uint8_t entries[6*12]; static const uint16_t ids[6]={2024,2025,2837,2838,2839,2840};
    for(unsigned i=0;i<6;++i) memcpy(entries+i*12,&ids[i],2);
    ptr(m_cs+0x40+0x14,entries); uint16_t count=6; memcpy(m_cs+0x40+0x78,&count,2);
    num(m_hud+HUD_STATE,1); num(m_hud+HUD_REQUEST,1); num(m_shop+SHOP_STATE,0); set_focus(m_hud);
    calls[0]=0; asked_id=line_id=0;
}
static void talk_tests(uint8_t *image,DWORD T) {
    /* Guards over the real bytes: every row and relocation, and the vtables. */
    assert(talk_exact(image));
    for(unsigned i=0;i<sizeof(talk_codes)/sizeof(talk_codes[0]);++i) {
        unsigned last=talk_codes[i].rva+talk_codes[i].size-1u;
        image[talk_codes[i].rva]^=1; assert(!talk_exact(image)); image[talk_codes[i].rva]^=1;
        image[last]^=1; assert(!talk_exact(image)); image[last]^=1;
    }
    for(unsigned j=0;j<sizeof(talk_relocations)/sizeof(talk_relocations[0]);++j) {
        image[talk_relocations[j].rva]^=1; assert(!talk_exact(image)); image[talk_relocations[j].rva]^=1;
    }
    ptr(image+RVA_DIALOG_VTABLE+0x2c,image); assert(!talk_exact(image)); ptr(image+RVA_DIALOG_VTABLE+0x2c,image+RVA_DIALOG_INPUT);
    ptr(image+RVA_SPEECH_VTABLE+4,image); assert(!talk_exact(image)); ptr(image+RVA_SPEECH_VTABLE+4,image+0x13f30);
    assert(talk_exact(image));
    printf("Kamo talk guards: %u routines and %u relocated operands, each byte flip refused: PASS\n",
        (unsigned)(sizeof(talk_codes)/sizeof(talk_codes[0])),(unsigned)(sizeof(talk_relocations)/sizeof(talk_relocations[0])));
    /* The model: owners at their globals, the routines behind jumps. */
    m_cs=calloc(1,0x480); m_ui=calloc(1,0x6bc); m_hud=calloc(1,0xc0); m_shop=calloc(1,0x110); m_scene=calloc(1,0x178); m_group=calloc(1,0xd4);
    assert(m_cs && m_ui && m_hud && m_shop && m_scene && m_group);
    ptr(m_hud,image+RVA_HUD_VTABLE); ptr(m_shop,image+RVA_SHOP_VTABLE); ptr(m_hud+HUD_SHOP,m_shop); ptr(m_hud+0x80,m_ui);
    ptr(m_scene+0x174,m_hud); ptr(m_group+0x90,m_group);
    ptr(image+RVA_SPEECH,m_cs); ptr(image+RVA_DIALOG,m_ui); ptr(image+RVA_HUD,m_hud); ptr(image+RVA_SHOP_LAYER,m_shop);
    ptr(image+RVA_SCENE,m_scene); ptr(image+RVA_GROUP,m_group); ptr(image+RVA_SHOP_STOCK,m_group); ptr(image+RVA_INVENTORY,m_group);
    ptr(image+RVA_MOVIE,NULL); num(image+0x3c2fec,1400);
    jump(image,RVA_ASK,(const void *)(uintptr_t)stub_ask); jump(image,RVA_LINE,(const void *)(uintptr_t)stub_line);
    jump(image,RVA_ANSWER,(const void *)(uintptr_t)stub_answer); jump(image,RVA_TALKING,(const void *)(uintptr_t)stub_talking);
    jump(image,RVA_STOP,(const void *)(uintptr_t)stub_stop); jump(image,RVA_SET_GEL,(const void *)(uintptr_t)stub_gel);
    jump(image,RVA_SET_MOVE_CAMERA,(const void *)(uintptr_t)stub_cam); jump(image,RVA_SET_IGM,(const void *)(uintptr_t)stub_igm);
    jump(image,RVA_DIALOG_INPUT,(const void *)(uintptr_t)stub_dialog_input);
    jump(image,RVA_DIALOG_START,(const void *)(uintptr_t)stub_dialog_start); jump(image,RVA_DIALOG_END,(const void *)(uintptr_t)stub_dialog_end);
    jump(image,RVA_DIALOG_CLEAR,(const void *)(uintptr_t)stub_dialog_clear); jump(image,RVA_DIALOG_ACTIVE,(const void *)(uintptr_t)stub_dialog_active);
    jump(image,RVA_SHOW_INGAME,(const void *)(uintptr_t)stub_show_ingame); jump(image,RVA_SHOP_START,(const void *)(uintptr_t)stub_shop_start);
    jump(image,RVA_SHOP_CLEAR,(const void *)(uintptr_t)stub_shop_clear); jump(image,RVA_SHOP_NAME,(const void *)(uintptr_t)stub_int);
    jump(image,RVA_SHOP_ADD,(const void *)(uintptr_t)stub_int); jump(image,RVA_SHOP_MODE,(const void *)(uintptr_t)stub_mode);
    jump(image,RVA_SHOP_ADD_PRICE,(const void *)(uintptr_t)stub_price); jump(image,RVA_ITEM_QUANTITY,(const void *)(uintptr_t)stub_quantity);
    jump(image,RVA_SHOP_INPUT,(const void *)(uintptr_t)stub_shop_input);
    assert(FlushInstructionCache(GetCurrentProcess(),image,0x400000));
    static uint8_t kamo_npc[0x48]; ptr(kamo_npc,image+RVA_NPC_VTABLE); kamo=kamo_npc; kamo_at[0]=1; kamo_at[1]=0; kamo_at[2]=-4;
    tal_at[0]=1; tal_at[1]=0; tal_at[2]=-2.0f;
    installed=TRUE; talk_ready=TRUE; stock_ready=TRUE; phase=IDLE; native_thread=0; talk_armed=TRUE;
    memset(&talk,0,sizeof(talk)); keeper_entity=NULL;
    DWORD t=T+5000;
    /* 1. Greeting, Chat, Shop, back to the question, Goodbye: the vanilla flow. */
    talk_reset_model(image);
    step(&t,1); assert(SudekiMpLanStoryShopPromptShown() && icon_bits==2);
    SudekiMpLanStoryShopRequestOpen(); assert(phase==T_REQUESTED && exit_held && SudekiMpLanStoryShopActive());
    assert(step(&t,1)==T_OPENING && !strcmp(calls,"cam0,gel1,clear,start,igm,") && !icon_bits);
    assert(step(&t,1)==T_ASK);
    assert(step(&t,1)==T_QUESTION && asked_id==2837 && asked_animate==0 && asked_show==1 && asked_speaker==kamo && !asked_listener);
    assert(step(&t,1)==T_ANSWER && step(&t,3)==T_ANSWER);              /* the player is choosing */
    choose(2); assert(step(&t,1)==T_SETTLE);
    assert(step(&t,1)==T_SAY && step(&t,1)==T_SAY && line_id==2024 && line_b1==0 && line_b3==0);
    assert(step(&t,1)==T_ASK && step(&t,1)==T_QUESTION && asked_id==2838);
    assert(step(&t,1)==T_ANSWER); choose(1); assert(step(&t,1)==T_SETTLE);
    assert(step(&t,1)==OPENING && talk.shop); assert(step(&t,1)==OPEN);
    shop_back_native(); assert(step(&t,1)==T_REDIALOG && !talk.shop);
    assert(step(&t,1)==T_REDIALOG && rd(m_hud+HUD_STATE)==2);            /* ShowDialog sent */
    assert(step(&t,1)==T_ASK && step(&t,1)==T_QUESTION && asked_id==2838);
    assert(step(&t,1)==T_ANSWER); choose(3); assert(step(&t,1)==T_SETTLE);
    assert(step(&t,1)==T_SAY && step(&t,1)==T_SAY && line_id==2025);
    assert(step(&t,1)==T_END);
    assert(step(&t,2)==T_END_HUD && rd(m_hud+HUD_STATE)==1);   /* EndConversation, then the box closes */
    assert(step(&t,1)==IDLE && !m_cs[0x47d] && m_cs[0x475]==1 && !exit_held && !talk.gel_set && !talk.cam_set);
    assert(strstr(calls,"shop_start,") && strstr(calls,"shop_clear,") && strstr(calls,"end,menus,") && strstr(calls,"gel0,cam1,"));
    assert(!SudekiMpLanStoryShopActive() && SudekiMpLanStoryShopUninstall()==TRUE); installed=TRUE; base=image;
    puts("Kamo talk: greeting, chat, shop, re-ask and farewell in the vanilla order, all restored: PASS");
    /* 2. A close while the choice menu is up answers it (ignored) and ends: no line plays. */
    talk_reset_model(image); talk_armed=TRUE; tal_at[2]=-0.5f; step(&t,1); tal_at[2]=-2.0f; step(&t,1);
    SudekiMpLanStoryShopRequestOpen(); step(&t,3); assert(step(&t,1)==T_ANSWER && rd(m_ui+0x4c)==3);
    SudekiMpLanStoryShopRequestClose("interior_exit"); line_id=0;
    assert(step(&t,1)==T_END); assert(step(&t,1)==T_END && strstr(calls,"confirm,"));
    unsigned p=step(&t,4); assert(p==IDLE && !line_id && !m_cs[0x47d] && m_cs[0x475]==1 && !rd(m_ui+0x4c));
    puts("Kamo talk: close on the open question answers it, ends and restores without a line: PASS");
    /* 3. A close while Tal's shop is open: back command, then the end. */
    talk_reset_model(image); talk_armed=TRUE; tal_at[2]=-0.5f; step(&t,1); tal_at[2]=-2.0f; step(&t,1);
    SudekiMpLanStoryShopRequestOpen(); step(&t,4); assert(phase==T_ANSWER); choose(1); step(&t,2); assert(step(&t,1)==OPEN);
    shop_backs=0; SudekiMpLanStoryShopRequestClose("interior_exit"); assert(shop_backs==1 && talk.close);
    shop_back_native(); assert(step(&t,1)==T_END); p=step(&t,4);
    assert(p==IDLE && !m_cs[0x47d] && rd(m_hud+HUD_STATE)==1);
    puts("Kamo talk: close during the shop leaves it by its back command and ends: PASS");
    /* 4. Teardown drain during a line stops it and ends; Drain is TRUE only at IDLE. */
    talk_reset_model(image); talk_armed=TRUE; tal_at[2]=-0.5f; step(&t,1); tal_at[2]=-2.0f; step(&t,1);
    SudekiMpLanStoryShopRequestOpen(); step(&t,4); assert(phase==T_ANSWER); choose(2); step(&t,3); assert(phase==T_SAY && rd(m_cs+0xc8)==1);
    unsigned drains=0; while(!SudekiMpLanStoryShopDrain(t++)) { native_tick(); assert(++drains<20); }
    assert(phase==IDLE && strstr(calls,"stop,") && !m_cs[0x47d]);
    puts("Kamo talk: teardown drain stops the line, ends the conversation and restores: PASS");
    /* 5. Story block 2000+: T2 (2839), Shop or Goodbye only. */
    talk_reset_model(image); num(image+0x3c2fec,2000); talk_armed=TRUE; tal_at[2]=-0.5f; step(&t,1); tal_at[2]=-2.0f; step(&t,1);
    SudekiMpLanStoryShopRequestOpen(); step(&t,4); assert(phase==T_ANSWER && asked_id==2839);
    choose(2); step(&t,3); assert(phase==T_SAY && line_id==2025); p=step(&t,5); assert(p==IDLE);
    num(image+0x3c2fec,1400);
    puts("Kamo talk: story block 2000 asks 2839 and its second choice is the farewell: PASS");
    /* 6. Refusals before any native call: missing sentence, busy speech, wrong shopkeeper class. */
    struct { const char *what; } refusals[3]={{"sentence"},{"speech"},{"npc"}};
    for(unsigned i=0;i<3;++i) {
        talk_reset_model(image); talk_armed=TRUE; tal_at[2]=-0.5f; step(&t,1); tal_at[2]=-2.0f; step(&t,1);
        if(i==0) { uint16_t one=1; memcpy(m_cs+0x40+0x78,&one,2); }        /* only 2024 present */
        if(i==1) m_cs[0x47d]=1;
        if(i==2) ptr(kamo_npc,image);
        calls[0]=0; SudekiMpLanStoryShopRequestOpen(); assert(phase==T_REQUESTED);
        SudekiMpLanStoryShopService(tal_at,"LNBr_Kamo_shop",FALSE,t++);
        assert(phase==IDLE && !calls[0] && !exit_held);
        if(i==2) ptr(kamo_npc,image+RVA_NPC_VTABLE);
        printf("Kamo talk refused before any call: %s: PASS\n",refusals[i].what);
    }
    /* 7. An unserviced lease follows the conversation witness; uninstall waits for the restore. */
    talk_reset_model(image); talk_armed=TRUE; tal_at[2]=-0.5f; step(&t,1); tal_at[2]=-2.0f; step(&t,1);
    SudekiMpLanStoryShopRequestOpen(); step(&t,4); assert(phase==T_ANSWER);
    last_service_at=GetTickCount()-5000u; assert(SudekiMpLanStoryShopActive());            /* menu up */
    assert(!SudekiMpLanStoryShopUninstall());
    while(!SudekiMpLanStoryShopDrain(t++)) native_tick();
    assert(SudekiMpLanStoryShopUninstall()); installed=TRUE; base=image; talk_ready=FALSE;
    puts("Kamo talk: unserviced witness and refused uninstall until restored: PASS");
    /* 8. Review 2026-10-17: the conversation's shop closed while the lease is
     * unserviced still holds the gates (the conversation is still set). */
    talk_ready=TRUE; installed=TRUE;
    talk_reset_model(image); talk_armed=TRUE; tal_at[2]=-0.5f; step(&t,1); tal_at[2]=-2.0f; step(&t,1);
    SudekiMpLanStoryShopRequestOpen(); step(&t,4); assert(phase==T_ANSWER); choose(1); step(&t,2); assert(step(&t,1)==OPEN && talk.shop);
    shop_back_native(); last_service_at=GetTickCount()-5000u;
    assert(shop_witness()==W_CLOSED && SudekiMpLanStoryShopActive());
    while(!SudekiMpLanStoryShopDrain(t++)) native_tick();
    assert(phase==IDLE && !m_cs[0x47d] && m_cs[0x475]==1 && !SudekiMpLanStoryShopActive());
    puts("Kamo talk: an unserviced conversation shop keeps the gates until the conversation is restored: PASS");
    /* 9. Back in game (1,1) while the conversation's shop open is pending:
     * the conversation ends and restores (never the plain-shop close). */
    talk_reset_model(image); talk_armed=TRUE; tal_at[2]=-0.5f; step(&t,1); tal_at[2]=-2.0f; step(&t,1);
    SudekiMpLanStoryShopRequestOpen(); step(&t,4); choose(1); shop_start_pending=TRUE; step(&t,2);
    assert(phase==OPENING && talk.shop && rd(m_hud+HUD_STATE)==0);
    num(m_hud+HUD_STATE,1); num(m_hud+HUD_REQUEST,1); set_focus(m_hud); shop_start_pending=FALSE;
    assert(step(&t,1)==T_END && !talk.shop);
    p=step(&t,4); assert(p==IDLE && !m_cs[0x47d] && m_cs[0x475]==1);
    puts("Kamo talk: an in-game HUD under a pending conversation shop ends the conversation and restores: PASS");
    /* 10. A press while the presentation step is not running is not queued. */
    talk_reset_model(image); talk_armed=TRUE; tal_at[2]=-0.5f; step(&t,1); tal_at[2]=-2.0f; step(&t,1);
    assert(SudekiMpLanStoryShopPromptShown());
    last_service_at=GetTickCount()-1000u; SudekiMpLanStoryShopRequestOpen(); assert(phase==IDLE && SudekiMpLanStoryShopIdle());
    talk_ready=FALSE;
    puts("Kamo talk: a press with the lease unserviced is refused: PASS");
    free(m_cs); free(m_ui); free(m_hud); free(m_shop); free(m_scene); free(m_group);
}
int main(int argc,char **argv) {
    wchar_t path[32768];
    assert(argc==2 && MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,32768));
    uint8_t *image=map_image(path); base=image;
    assert(image_exact());
    for(unsigned i=0;i<sizeof(codes)/sizeof(codes[0]);++i) {
        unsigned last=codes[i].rva+codes[i].size-1u;
        image[codes[i].rva]^=1; assert(!image_exact()); image[codes[i].rva]^=1;
        image[last]^=1; assert(!image_exact()); image[last]^=1;
        for(unsigned k=0;k<2u;++k) if(codes[i].target[k]) {
            image[codes[i].rva+codes[i].at[k]]^=1; assert(!image_exact()); image[codes[i].rva+codes[i].at[k]]^=1;
        }
        assert(image_exact());
        printf("Named native guard: %s PASS\n",codes[i].name);
    }
    ptr(image+RVA_SHOP_VTABLE+0x2c,image); assert(!image_exact()); ptr(image+RVA_SHOP_VTABLE+0x2c,image+RVA_SHOP_INPUT);
    /* The owned skip of the Show call: installs over the exact call and restores byte-exactly. */
    uint8_t saved[8]; memcpy(saved,image+0x89e6d,8);
    assert(SudekiMpInstallRelativeCallHook(&move_hook,image+RVA_SHOW_MOVE_CALL,image+RVA_DISABLE_MOVE,(const void *)(uintptr_t)move_skip));
    assert(memcmp(image+0x89e6d,saved,8));
    assert(SudekiMpRestoreRelativeCallHook(&move_hook) && !memcmp(image+0x89e6d,saved,8) && image_exact());
    puts("Shop Show call skip installs and restores byte-exactly: PASS");
    /* Witness: unknown unless the HUD, shop layer and its HUD link prove. */
    uint8_t *hud=calloc(1,0xc0),*shop=calloc(1,0x110),*scene=calloc(1,0x174); assert(hud && shop && scene);
    assert(shop_witness()==W_UNKNOWN);
    ptr(hud,image+RVA_HUD_VTABLE); ptr(shop,image+RVA_SHOP_VTABLE); ptr(hud+HUD_SHOP,shop);
    ptr(image+RVA_HUD,hud); ptr(image+RVA_SHOP_LAYER,shop); ptr(image+RVA_SCENE,scene); ptr(scene+0x170,hud);
    num(hud+HUD_STATE,1); assert(shop_witness()==W_CLOSED);
    num(hud+HUD_STATE,7); assert(shop_witness()==W_OPEN); num(hud+HUD_STATE,8); assert(shop_witness()==W_OPEN);
    num(hud+HUD_STATE,0); num(shop+SHOP_STATE,5); assert(shop_witness()==W_OPEN); num(shop+SHOP_STATE,0);
    ptr(scene+0x170,shop); assert(shop_witness()==W_OPEN); ptr(scene+0x170,hud);
    ptr(hud+HUD_SHOP,hud); assert(shop_witness()==W_UNKNOWN); ptr(hud+HUD_SHOP,shop);
    ptr(shop,image); assert(shop_witness()==W_UNKNOWN); ptr(shop,image+RVA_SHOP_VTABLE);
    assert(shop_witness()==W_CLOSED);
    puts("Shop open/closed/unknown witness: PASS");
    /* Prompt: near Kamo in his shop only; hysteresis; blocked hides it. */
    installed=TRUE; phase=IDLE; native_thread=0;
    const DWORD T=GetTickCount(); /* the lease's service bound is real time */
    stock_ready=FALSE; tal_at[0]=1; tal_at[2]=-2.0f; kamo=NULL;
    SudekiMpLanStoryShopService(tal_at,"LNBr_Kamo_shop",FALSE,T+900); assert(!SudekiMpLanStoryShopPromptShown()); /* no stock: no prompt */
    stock_ready=TRUE; stock.tier_count=1; stock.tiers[0].min_block=INT32_MIN; stock.tiers[0].count=1; stock.tiers[0].items[0]=1;
    static uint8_t kamo_entity[4]; kamo=kamo_entity; kamo_at[0]=1; kamo_at[2]=-4;
    tal_at[0]=1; tal_at[2]=-0.5f;                                          /* 3.5 away */
    SudekiMpLanStoryShopService(tal_at,"LNBr_Kamo_shop",FALSE,T+1000); assert(!SudekiMpLanStoryShopPromptShown() && !icon_bits);
    tal_at[2]=-2.0f;                                                       /* 2.0 away */
    SudekiMpLanStoryShopService(tal_at,"LNBr_Kamo_shop",FALSE,T+1001); assert(SudekiMpLanStoryShopPromptShown() && icon_bits==2);
    SudekiMpLanStoryShopService(tal_at,"LNBr_Kamo_shop",TRUE,T+1002);  assert(!SudekiMpLanStoryShopPromptShown() && !icon_bits);
    SudekiMpLanStoryShopService(tal_at,NULL,FALSE,T+1003);              assert(!SudekiMpLanStoryShopPromptShown());
    SudekiMpLanStoryShopService(tal_at,"LNBr_Kamo_shop",FALSE,T+1004); assert(SudekiMpLanStoryShopPromptShown());
    icon_owner=FALSE; SudekiMpLanStoryShopService(tal_at,"LNBr_Kamo_shop",FALSE,T+1004); assert(!SudekiMpLanStoryShopPromptShown());
    icon_owner=TRUE;  SudekiMpLanStoryShopService(tal_at,"LNBr_Kamo_shop",FALSE,T+1004); assert(SudekiMpLanStoryShopPromptShown());
    /* Pressed while the HUD is not in game state: refused before any native
     * shop call; the exit hold taken at the press is released with it. */
    num(hud+HUD_STATE,7);
    SudekiMpLanStoryShopRequestOpen(); assert(SudekiMpLanStoryShopActive() && exit_held);
    SudekiMpLanStoryShopService(tal_at,"LNBr_Kamo_shop",FALSE,T+1005); assert(!SudekiMpLanStoryShopActive() && !exit_held);
    num(hud+HUD_STATE,1);
    SudekiMpLanStoryShopService(tal_at,"LNBr_Kamo_shop",FALSE,T+1006); assert(!SudekiMpLanStoryShopPromptShown());
    tal_at[2]=-0.5f; SudekiMpLanStoryShopService(tal_at,"LNBr_Kamo_shop",FALSE,T+1007);   /* 3.5: re-armed */
    tal_at[2]=-2.0f; SudekiMpLanStoryShopService(tal_at,"LNBr_Kamo_shop",FALSE,T+1008); assert(SudekiMpLanStoryShopPromptShown());
    /* A press whose player left the shopkeeper before the next step ends. */
    SudekiMpLanStoryShopRequestOpen(); assert(SudekiMpLanStoryShopActive());
    SudekiMpLanStoryShopService(tal_at,NULL,FALSE,T+1009); assert(!SudekiMpLanStoryShopActive());
    /* Teardown drain of an unopened request; an unserviced lease follows the witness. */
    tal_at[2]=-0.5f; SudekiMpLanStoryShopService(tal_at,"LNBr_Kamo_shop",FALSE,T+1010);
    tal_at[2]=-2.0f; SudekiMpLanStoryShopService(tal_at,"LNBr_Kamo_shop",FALSE,T+1011); assert(SudekiMpLanStoryShopPromptShown());
    SudekiMpLanStoryShopRequestOpen(); assert(!SudekiMpLanStoryShopDrain(T+1012) || !SudekiMpLanStoryShopActive());
    assert(SudekiMpLanStoryShopDrain(T+1013) && !SudekiMpLanStoryShopActive());
    phase=OPEN; last_service_at=GetTickCount()-5000u; assert(!SudekiMpLanStoryShopActive());   /* witness CLOSED */
    num(hud+HUD_STATE,8); assert(SudekiMpLanStoryShopActive()); num(hud+HUD_STATE,1); phase=IDLE;
    tal_at[1]=3.0f;  SudekiMpLanStoryShopService(tal_at,"LNBr_Kamo_shop",FALSE,T+1014); assert(!SudekiMpLanStoryShopPromptShown());
    /* The gramophone: examine prompt (0x40) in its reach, the box request on
     * the press, no prompt while the box is up; talk wins where both reach. */
    tal_at[0]=-3.0f; tal_at[1]=0.0f; tal_at[2]=-2.0f;                    /* 1.2 from it, 4.4 from Kamo */
    SudekiMpLanStoryShopService(tal_at,"LNBr_Kamo_shop",FALSE,T+1100);
    assert(SudekiMpLanStoryShopPromptShown() && icon_bits==0x40);
    SudekiMpLanStoryShopRequestOpen(); assert(examine_requests==1 && examine_text==9236 && !SudekiMpLanStoryShopActive());
    SudekiMpLanStoryShopService(tal_at,"LNBr_Kamo_shop",FALSE,T+1101); assert(!SudekiMpLanStoryShopPromptShown() && !icon_bits);
    examine_active=FALSE;
    SudekiMpLanStoryShopService(tal_at,"LNBr_Kamo_shop",FALSE,T+1102); assert(icon_bits==0x40);
    SudekiMpLanStoryShopService(tal_at,"LNBr_Kamo_shop",TRUE,T+1103);  assert(!icon_bits);       /* blocked */
    tal_at[0]=-1.6f; tal_at[2]=-3.2f;    /* 1.64 from the gramophone, 2.70 from Kamo: talk first */
    SudekiMpLanStoryShopService(tal_at,"LNBr_Kamo_shop",FALSE,T+1104); assert(icon_bits==2);
    tal_at[0]=1; tal_at[2]=-2.0f; tal_at[1]=0;
    puts("Shop examinable prompt, box request, suppression and talk priority: PASS");
    puts("Shop talk prompt range, hysteresis, blocking and refused open: PASS");
    talk_tests(image,T);
    free(hud); free(shop); free(scene); VirtualFree(image,0,MEM_RELEASE);
    return 0;
}
