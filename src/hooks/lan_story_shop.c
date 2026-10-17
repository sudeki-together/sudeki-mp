#include "hooks/lan_story_shop.h"
#include "hooks/call_hook.h"
#include "hooks/lan_story_remote_interior.h"
#include "hooks/lan_story_remote_use.h"
#include "hooks/lan_story_main_menu.h"
#include "hooks/lan_story_shopkeepers.h"
#include "hooks/lan_story_cinematic.h"
#include "hooks/lan_story_zone_banner.h"
#include "hooks/lan_story_client_camera.h"
#include "hooks/lan_story_world.h"
#include "hooks/lan_story_npc_gesture.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include "engine/sol_shop_stock.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#if !defined(__GNUC__) || !defined(__i386__)
#error "Story shop requires the supported native x86 ABI"
#endif

enum {
    RVA_SHOP_START=0x8d1a0u, RVA_SHOP_CLEAR=0x17c50u, RVA_SHOP_ADD_PRICE=0x17c70u, RVA_SHOP_ADD=0x17c90u,
    RVA_SHOP_MODE=0x17cd0u, RVA_SHOP_NAME=0x8d160u, RVA_SHOW_INGAME=0xb880u, RVA_ITEM_QUANTITY=0x22200u,
    RVA_SHOW_MOVE_CALL=0x89e70u, RVA_DISABLE_MOVE=0xafd0u, RVA_SHOP_INPUT=0x898a0u, RVA_SHOP_SHOW=0x89e60u,
    RVA_HUD=0x3c2f88u, RVA_HUD_VTABLE=0x2caf9cu, RVA_SHOP_LAYER=0x3c2f70u, RVA_SHOP_VTABLE=0x2cabb4u,
    RVA_SCENE=0x408d1cu, RVA_GROUP=0x408d94u, RVA_SHOP_STOCK=0x408d44u, RVA_INVENTORY=0x408d84u,
    RVA_STRINGS=0x409e04u,
    HUD_STATE=0xb8u, HUD_REQUEST=0xbcu, HUD_SHOP=0x74u, SHOP_STATE=0x88u,
    TALK_FLAG=2u, OPEN_WAIT_MS=2000u, CLOSE_WAIT_MS=3000u
};
/* NPC_Kamo_NB's CTrigger volume (players-only sphere, scale 2.3 in
 * SOLData). Its live reach is about 3.1 around Kamo in every direction:
 * the host's talk flag was set out to 3.10 and clear from 3.09-3.14
 * (CONFIRMED_LIVE 2026-10-17, read-only flag/position poll of Ailish). */
static const float TALK_RADIUS=3.1f,TALK_REARM=3.4f,TALK_HEIGHT=2.5f;
typedef SudekiMpLanStoryShopkeeper Shopkeeper;
/* Examinables in table shops, used on each player's own game (D4/D11): a
 * players-only trigger whose OnAction only plays a positional sound and
 * shows text (no story state). NB_gramophone_trig: OnEntry/OnExit set/clear
 * context flag 0x40; OnAction = EnableEnvironmentSound("gramophone"),
 * HideMessageBox, ExaminableUse(9236, 1) -> MessageBoxDisplay(9236, 1, 0.0,
 * ConfirmClose|IgnoreNewMessagesTillClose = 0x30) (CONFIRMED_STATIC,
 * SOLWORLDM.gex). Its reach is about 1.8-1.9 (CONFIRMED_LIVE host poll of
 * flag 0x40 against Ailish, 30-60 degree bearings only). */
typedef struct Examinable { const char *zone,*entity,*emitter; uint16_t text; uint8_t mode; uint32_t flags; float radius; } Examinable;
static const Examinable examinables[]={
    {"LNBr_Kamo_shop","NB_gramophone_trig","gramophone",9236u,1u,0x30u,1.85f},
};
enum { EXAMINE_FLAG=0x40u };

typedef void (__cdecl *VoidCall)(void);
typedef uint8_t (__cdecl *BoolArg)(int);
typedef void (__cdecl *IntArg)(int);
typedef void (__cdecl *AddPrice)(int,int);
typedef int (__cdecl *Quantity)(int);
typedef uint8_t (__attribute__((thiscall)) *LayerInput)(void *,unsigned,unsigned,unsigned);

static uint8_t *base;
static BOOL installed,stock_ready;
static SudekiMpSolShopStock stock;
static SudekiMpRelativeCallHook move_hook;
static DWORD native_thread;
static unsigned logs;

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && a<=UINTPTR_MAX-n && VirtualQuery(p,&m,sizeof(m))==sizeof(m) &&
        m.State==MEM_COMMIT && !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
        a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static uint8_t *global(unsigned rva) { return readable(base+rva,4u)?*(uint8_t **)(base+rva):NULL; }
static void trace(const char *format,...) __attribute__((format(printf,1,2)));
static void trace(const char *format,...) {
    if(logs>=600u) return;
    ++logs; char line[384]; va_list a; va_start(a,format); vsnprintf(line,sizeof(line),format,a); va_end(a);
    SudekiMpLogWrite(line);
}

/* Exact entry bytes; `at`/`target` pairs are absolute operands relocated
 * with the image (checked against base+target, other bytes compared). */
typedef struct Code { const char *name; unsigned rva,size; uint8_t bytes[52]; unsigned at[2],target[2]; } Code;
static const Code codes[]={
    {"ShopStart",RVA_SHOP_START,24,{0x0f,0xb6,0x54,0x24,0x04,0x8b,0x0d,0,0,0,0,0x8b,0x01,0x8b,0x40,0x2c,0x6a,0x00,0x52,0x6a,0x0c,0xff,0xd0,0xc3},
        {7,0},{RVA_HUD,0}},
    {"ShopClearInventory",RVA_SHOP_CLEAR,30,{0x56,0x8b,0x35,0,0,0,0,0x85,0xf6,0x74,0x11,0x8d,0x46,0x10,0xe8,0xfd,0xb0,0x14,0x00,
        0x8d,0x46,0x20,0x5e,0xe9,0xf4,0xb0,0x14,0x00,0x5e,0xc3},{3,0},{RVA_SHOP_STOCK,0}},
    {"ShopAddItemPrice",RVA_SHOP_ADD_PRICE,25,{0x8b,0x0d,0,0,0,0,0x85,0xc9,0x74,0x0f,0x8b,0x44,0x24,0x08,0x50,0x8b,0x44,0x24,0x08,
        0xe8,0x98,0xfc,0xff,0xff,0xc3},{2,0},{RVA_SHOP_STOCK,0}},
    {"ShopAddItem",RVA_SHOP_ADD,22,{0x8b,0x0d,0,0,0,0,0x85,0xc9,0x74,0x0c,0x8b,0x44,0x24,0x04,0x6a,0xff,0xe8,0x7b,0xfc,0xff,0xff,0xc3},
        {2,0},{RVA_SHOP_STOCK,0}},
    {"ShopSetMode",RVA_SHOP_MODE,24,{0xa1,0,0,0,0,0x85,0xc0,0x74,0x0f,0x33,0xc9,0x38,0x4c,0x24,0x04,0x0f,0x94,0xc1,0x89,0x48,0x0c,
        0xb0,0x01,0xc3},{1,0},{RVA_SHOP_STOCK,0}},
    {"ShopSetName",RVA_SHOP_NAME,49,{0x56,0x8b,0x35,0,0,0,0,0x85,0xf6,0x74,0x24,0x8b,0x44,0x24,0x08,0x8b,0x0d,0,0,0,0,0x57,0x83,0xc1,0x04,
        0x50,0x51,0x33,0xc9,0xe8,0x8e,0xa3,0x09,0x00,0x50,0x8d,0xbe,0xd0,0x00,0x00,0x00,0xe8,0x32,0xce,0x12,0x00,0x5f,0x5e,0xc3},
        {3,17},{RVA_SHOP_LAYER,RVA_STRINGS}},
    {"ShowIngameMenus",RVA_SHOW_INGAME,29,{0xa1,0,0,0,0,0x56,0x8b,0xb0,0x74,0x01,0x00,0x00,0xc7,0x86,0xbc,0x00,0x00,0x00,0x01,0x00,0x00,0x00,
        0xe8,0xc5,0x2c,0x09,0x00,0x5e,0xc3},{1,0},{RVA_SCENE,0}},
    {"GetQuantityOfItemInInventory",RVA_ITEM_QUANTITY,21,{0x8b,0x0d,0,0,0,0,0x85,0xc9,0x74,0x0b,0x8b,0x44,0x24,0x04,0x50,0xe8,0xec,0xfc,0xff,0xff,0xc3},
        {2,0},{RVA_INVENTORY,0}},
    /* Shop Show: PUSH EBX; MOV EDI,ECX; CALL 0x40AFD0 (the skipped call). */
    {"ShopShowMoveCall",0x89e6du,8,{0x53,0x8b,0xf9,0xe8,0x5b,0x11,0xf8,0xff},{0,0},{0,0}},
    {"ShopLayerInput",RVA_SHOP_INPUT,14,{0x53,0x8b,0xd9,0x80,0xbb,0x0c,0x01,0x00,0x00,0x00,0x8b,0x4c,0x24,0x08},{0,0},{0,0}},
};
static BOOL code_exact(const Code *c) {
    if(!readable(base+c->rva,c->size)) return FALSE;
    for(unsigned i=0;i<c->size;++i) {
        BOOL operand=FALSE;
        for(unsigned k=0;k<2u;++k) if(c->target[k] && i>=c->at[k] && i<c->at[k]+4u) operand=TRUE;
        if(!operand && base[c->rva+i]!=c->bytes[i]) return FALSE;
    }
    for(unsigned k=0;k<2u;++k) if(c->target[k] &&
        *(uint32_t *)(base+c->rva+c->at[k])!=(uint32_t)(uintptr_t)(base+c->target[k])) return FALSE;
    return TRUE;
}
static BOOL image_exact(void) {
    for(unsigned i=0;i<sizeof(codes)/sizeof(codes[0]);++i) if(!code_exact(&codes[i])) {
        trace("story_shop event=guard_failed name=%s\r\n",codes[i].name); return FALSE;
    }
    uint8_t *vt=base+RVA_SHOP_VTABLE;
    return readable(vt,0x30u) && *(void **)(vt+0x2cu)==base+RVA_SHOP_INPUT && *(void **)(vt+0x18u)==base+RVA_SHOP_SHOW;
}
/* The shop layer's Show(bool) calls 0x40AFD0(bool) (RET 4); skipped here. */
static void __attribute__((naked,noinline)) move_skip(void) { __asm__ volatile("ret $4"); }

/* ---- native state witnesses (positive observation only) ---- */
static uint8_t *hud_exact(void) {
    uint8_t *hud=global(RVA_HUD);
    return readable(hud,0xc0u) && *(void **)hud==base+RVA_HUD_VTABLE?hud:NULL;
}
static uint8_t *shop_layer(uint8_t *hud) {
    uint8_t *shop=global(RVA_SHOP_LAYER);
    return hud && readable(shop,0x110u) && *(void **)shop==base+RVA_SHOP_VTABLE &&
        *(uint8_t **)(hud+HUD_SHOP)==shop?shop:NULL;
}
static uint8_t *focus(void) {
    uint8_t *scene=global(RVA_SCENE);
    return readable(scene,0x174u)?*(uint8_t **)(scene+0x170u):NULL;
}
enum { W_UNKNOWN, W_CLOSED, W_OPEN };
static int shop_witness(void) {
    uint8_t *hud=hud_exact(),*shop=shop_layer(hud);
    if(!hud || !shop) return W_UNKNOWN;
    unsigned state=*(unsigned *)(hud+HUD_STATE);
    if(state==7u || state==8u || *(unsigned *)(shop+SHOP_STATE) || focus()==shop) return W_OPEN;
    return W_CLOSED;
}

/* ---- Kamo's conversation on this game (D11 stage 2) ----
 * The shopkeeper's OnAction, as NPC_Kamo_NB runs it natively (SOLWORLDM.gex,
 * CONFIRMED_STATIC): StartInteractionScript (SetGelScriptInteracting(1),
 * UIDialogClear, UIDialogStartConversation, ShowDialog = SetIgmState(2)),
 * then T1 (story block < 2000) or T2: Ask(Kamo, 2837 | 2839) with the choice
 * menu Shop / Chat (2024, T1 only) / Goodbye (2025); after the shop or the
 * chat the re-ask 2838 | 2840; Goodbye ends with EndInteractionScript
 * (UIDialogEndConversation, ShowIngameMenus). Skipped here: the AI turn
 * and facing steps, PauseScriptsForInteraction, the speech camera
 * (SetMoveCamera(0)) and lip sync (animate 0: Kamo is held by this
 * client's world pause). The native UIDialog takes confirm and up/down
 * itself; Escape is a no-op on the question (as natively).
 * Routines (RVA, CONFIRMED_STATIC on the supported image): CSpeech
 * *(0x408D3C) vtable 0x2C6190: AskQuestion 0x13730 and ActivateLine 0x13910
 * thiscall (speaker TPtr*, listener TPtr*, id, b1, b2, b3, b4) RET 0x1C,
 * GetAnswerToQuestion 0x13860 and IsTalking 0x155E0 and Stop 0x13EE0
 * thiscall, SetGelScriptInteracting 0x132C0 and SetMoveCamera 0x157C0
 * thiscall(bool) RET 4; UIDialog *(0x3C2F94) vtable 0x2CB2B4: cdecl
 * Start 0xA4650, End 0xA4690, IsActive 0xA4730, Clear 0xA4800, its input
 * 0xA3250 (vtable +0x2C) thiscall(kind, cmd, x) RET 0xC; SetIgmState 0xB1E0
 * thiscall(CUIScene*, state) RET 4. The sentence table is CSpeech+0x40
 * (entries *(+0x14), 12 bytes, WORD id; count WORD +0x78; lookup 0x52C6D0).
 * Guard hashes: FNV-1a over the image bytes, relocated operands compared
 * against base+target and hashed as at the preferred base (the zone-banner
 * scheme). */
static const struct { const char *name; unsigned rva,size; uint32_t hash; } talk_codes[]={
    {"GET_SPEECH",0x156b0u,0x6u,0x6e5fe854u},
    {"SPEECH_SET_GEL_INTERACTING",0x132c0u,0xdu,0x52849d2du},
    {"SPEECH_GET_GEL_INTERACTING",0x132d0u,0x7u,0xf90e66d2u},
    {"SPEECH_ASK_QUESTION",0x13730u,0x12bu,0xf442df4au},
    {"SPEECH_GET_ANSWER",0x13860u,0xabu,0xe281ae3au},
    {"SPEECH_ACTIVATE_LINE",0x13910u,0x3b2u,0xb0069e19u},
    {"SPEECH_CLEAR_LINE",0x13500u,0xa0u,0x8895f3eeu},
    {"SPEECH_STOP",0x13ee0u,0x42u,0x99352831u},
    {"SPEECH_UPDATE",0x13f30u,0x21eu,0x8148e6ebu},
    {"SPEECH_LINE_UI",0x153f0u,0x1d4u,0xea0de32au},
    {"SPEECH_IS_TALKING",0x155e0u,0x5au,0x9456b0b6u},
    {"SPEECH_SET_MOVE_CAMERA",0x157c0u,0xdu,0x9b229485u},
    {"SENTENCE_LOOKUP_HEAD",0x12c6d0u,0x41u,0x9fdbe52eu},
    {"UIDIALOG_START",0xa4650u,0x3eu,0x578cdc64u},
    {"UIDIALOG_END",0xa4690u,0x2eu,0x2d5d1b01u},
    {"UIDIALOG_IS_ACTIVE",0xa4730u,0xfu,0xeeac74aeu},
    {"UIDIALOG_IS_ASKING",0xa4740u,0x10u,0x79484c68u},
    {"UIDIALOG_LAST_ANSWER",0xa47f0u,0xcu,0x409ed335u},
    {"UIDIALOG_CLEAR",0xa4800u,0xdu,0xa6e790a4u},
    {"UIDIALOG_ASK_BOX",0xa3d90u,0x103u,0x03223f31u},
    {"UIDIALOG_UPDATE",0xa2ca0u,0x374u,0x6543adaau},
    {"UIDIALOG_INPUT",0xa3250u,0x2b8u,0x57ab0d77u},
    {"UIDIALOG_OPEN_CLOSE_EVENT",0xa43f0u,0x9bu,0x663e774bu},
    {"SET_IGM_STATE",0xb1e0u,0x19u,0x53cb6afdu},
    {"GET_IGM_STATE",0xb200u,0xdu,0x9c7f63e6u},
    {"SHOW_INGAME_MENUS",0xb880u,0x1du,0xfe9bcf0eu},
    {"SPEECH_SET_LISTENER_ANIMATES",0x13cd0u,0x11u,0x637ea738u},
    {"GET_STORY_BLOCK",0xfda00u,0x7u,0xe5a6a06au},
    {"HUD_ENTER_DIALOG",0x9de16u,0x52u,0x71df64e6u},
    {"HUD_EXIT_DIALOG",0x9e22bu,0x33u,0x064221b7u},
};
static const struct { unsigned rva,target; } talk_relocations[]={
   {0x156b1u,0x408d3cu}, {0x13736u,0x408d1cu}, {0x13791u,0x3c2f94u}, {0x137cbu,0x2c56e8u},
   {0x137d0u,0x2c56f4u}, {0x137d5u,0x2c5700u}, {0x137dau,0x2c570cu}, {0x137eau,0x2c56e8u},
   {0x137efu,0x2c56f4u}, {0x137f4u,0x2c5700u}, {0x137f9u,0x2c5734u}, {0x1380au,0x2c516cu},
   {0x1381bu,0x2c516cu}, {0x1382au,0x2c516cu}, {0x13839u,0x2c516cu}, {0x13864u,0x408d1cu},
   {0x13880u,0x3c2f94u}, {0x138a1u,0x408d64u}, {0x13911u,0x408d1cu}, {0x13adau,0x2c5764u},
   {0x13ae3u,0x2c5778u}, {0x13f46u,0x408db8u}, {0x13f71u,0x408d1cu}, {0x140b4u,0x408d64u},
   {0x140f1u,0x408d1cu}, {0x1540bu,0x408d64u}, {0x1541cu,0x408dacu}, {0x15487u,0x408d1cu},
   {0x1548du,0x3c2f94u}, {0x154e9u,0x2c516cu}, {0x1550cu,0x2c6128u}, {0x15529u,0x2c614cu},
   {0x15552u,0x2c6168u}, {0x155e2u,0x3c2f94u}, {0xa4652u,0x3c2f94u}, {0xa467eu,0x408d3cu},
   {0xa4692u,0x3c2f94u}, {0xa46b2u,0x408d3cu}, {0xa4732u,0x3c2f94u}, {0xa4742u,0x3c2f94u},
   {0xa47f1u,0x3c2f94u}, {0xa4801u,0x3c2f94u}, {0xa3e5eu,0x2e36f0u}, {0xa3e64u,0x2e36e8u}, {0xa2d47u,0xa2ff8u},
   {0xa2ef9u,0x408d3cu}, {0xa2ff8u,0xa2e8fu}, {0xa2ffcu,0xa2fefu}, {0xa3000u,0xa2f95u}, {0xa3004u,0xa2d4bu},
   {0xa3008u,0xa2d4bu}, {0xa300cu,0xa2d4bu}, {0xa3010u,0xa2fbau}, {0xa32abu,0xa3490u}, {0xa32b8u,0x408d1cu},
   {0xa32d5u,0xa34a8u}, {0xa32fau,0x2c3d80u}, {0xa3327u,0x2c3d80u}, {0xa3374u,0xa34c8u}, {0xa3395u,0x2c3d80u},
   {0xa33c2u,0x2c3d80u}, {0xa33eeu,0xa34e8u}, {0xa3435u,0x2c67dcu}, {0xa3462u,0x3c2f88u}, {0xa3490u,0xa32afu},
   {0xa3494u,0xa33e2u}, {0xa3498u,0xa32afu}, {0xa349cu,0xa336cu}, {0xa34a0u,0xa32cdu}, {0xa34a4u,0xa334fu},
   {0xa34a8u,0xa32d9u}, {0xa34acu,0xa32d9u}, {0xa34b0u,0xa32d9u}, {0xa34b4u,0xa32d9u}, {0xa34b8u,0xa333du},
   {0xa34bcu,0xa333du}, {0xa34c0u,0xa32f3u}, {0xa34c4u,0xa3320u}, {0xa34c8u,0xa3378u}, {0xa34ccu,0xa3378u},
   {0xa34d0u,0xa3378u}, {0xa34d4u,0xa3378u}, {0xa34d8u,0xa333du}, {0xa34dcu,0xa333du}, {0xa34e0u,0xa338eu},
   {0xa34e4u,0xa33bbu}, {0xa34e8u,0xa33f2u}, {0xa34ecu,0xa333du}, {0xa34f0u,0xa33f2u}, {0xa34f4u,0xa333du},
   {0xa34f8u,0xa333du}, {0xa34fcu,0xa333du}, {0xa3500u,0xa3429u}, {0xa3504u,0xa3447u}, {0xb881u,0x408d1cu},
   {0xfda02u,0x3c2fecu}, {0x9de23u,0x408da4u},
};
enum { RVA_SPEECH=0x408d3cu, RVA_DIALOG=0x3c2f94u, RVA_MOVIE=0x408db8u, RVA_SPEECH_VTABLE=0x2c6190u,
    RVA_DIALOG_VTABLE=0x2cb2b4u, RVA_NPC_VTABLE=0x2d65a0u,
    RVA_ASK=0x13730u, RVA_ANSWER=0x13860u, RVA_LINE=0x13910u, RVA_STOP=0x13ee0u, RVA_TALKING=0x155e0u,
    RVA_SET_GEL=0x132c0u, RVA_SET_MOVE_CAMERA=0x157c0u, RVA_DIALOG_START=0xa4650u, RVA_DIALOG_END=0xa4690u,
    RVA_DIALOG_ACTIVE=0xa4730u, RVA_DIALOG_CLEAR=0xa4800u, RVA_DIALOG_INPUT=0xa3250u, RVA_SET_IGM=0xb1e0u,
    RVA_SET_LISTENER_ANIMATES=0x13cd0u,
    Q_T1=2837u, Q_T1B=2838u, Q_T2=2839u, Q_T2B=2840u, LINE_CHAT=2024u, LINE_FAREWELL=2025u, T2_BLOCK=2000,
    TALK_WAIT_MS=2000u, TALK_STEP_MS=3000u, TALK_QUARANTINE_MS=10000u };
typedef uint8_t (__attribute__((thiscall)) *SpeechLine)(void *,void *,void *,int,uint8_t,uint8_t,uint8_t,uint8_t);
typedef int (__attribute__((thiscall)) *SpeechInt)(void *);
typedef uint8_t (__attribute__((thiscall)) *SpeechBool)(void *);
typedef void (__attribute__((thiscall)) *SpeechSet)(void *,uint8_t);
typedef void (__attribute__((thiscall)) *SpeechVoid)(void *);
typedef void (__attribute__((thiscall)) *SceneState)(void *,int);
typedef uint8_t (__cdecl *BoolVoid)(void);
static BOOL talk_ready;
static uint32_t talk_word(const uint8_t *p) { uint32_t v; memcpy(&v,p,4); return v; }
static BOOL talk_exact(const uint8_t *b) {
    for(unsigned k=0;k<sizeof(talk_codes)/sizeof(talk_codes[0]);++k) {
        uint32_t h=2166136261u;
        if(!readable(b+talk_codes[k].rva,talk_codes[k].size)) return FALSE;
        for(unsigned i=0;i<talk_codes[k].size;++i) {
            unsigned rva=talk_codes[k].rva+i; uint8_t byte=b[rva];
            for(unsigned j=0;j<sizeof(talk_relocations)/sizeof(talk_relocations[0]);++j) {
                unsigned offset=rva-talk_relocations[j].rva;
                if(offset>=4u) continue;
                if(talk_word(b+talk_relocations[j].rva)!=(uint32_t)(uintptr_t)(b+talk_relocations[j].target)) return FALSE;
                byte=(uint8_t)((0x400000u+talk_relocations[j].target)>>(8u*offset)); break;
            }
            h=(h^byte)*16777619u;
        }
        if(h!=talk_codes[k].hash) { trace("story_shop event=talk_guard_failed name=%s\r\n",talk_codes[k].name); return FALSE; }
    }
    const uint8_t *sv=b+RVA_SPEECH_VTABLE,*dv=b+RVA_DIALOG_VTABLE;
    return readable(sv,8u) && *(const void *const *)(sv+4u)==b+0x13f30u &&
        readable(dv,0x30u) && *(const void *const *)(dv+8u)==b+0xa2ca0u &&
        *(const void *const *)(dv+0x18u)==b+0xa3940u && *(const void *const *)(dv+0x2cu)==b+RVA_DIALOG_INPUT;
}
static uint8_t *speech(void) {
    uint8_t *cs=global(RVA_SPEECH);
    return readable(cs,0x480u) && *(void **)cs==base+RVA_SPEECH_VTABLE?cs:NULL;
}
static uint8_t *dialog(void) {
    uint8_t *ui=global(RVA_DIALOG);
    return readable(ui,0x6bcu) && *(void **)ui==base+RVA_DIALOG_VTABLE?ui:NULL;
}
/* The id is in this game's sentence table (no "Can't find Question" box). */
static BOOL sentence_present(const uint8_t *cs,unsigned id) {
    const uint8_t *entries=*(const uint8_t *const *)(cs+0x40u+0x14u);
    unsigned count=*(const uint16_t *)(cs+0x40u+0x78u);
    if(!count || !readable(entries,count*12u)) return FALSE;
    for(unsigned i=0;i<count;++i) if(*(const uint16_t *)(entries+i*12u)==id) return TRUE;
    return FALSE;
}
static BOOL talking(uint8_t *cs) { return ((SpeechBool)(base+RVA_TALKING))(cs)!=0; }
static unsigned hud_state(void) { uint8_t *hud=hud_exact(); return hud?*(unsigned *)(hud+HUD_STATE):99u; }
static unsigned hud_request(void) { uint8_t *hud=hud_exact(); return hud?*(unsigned *)(hud+HUD_REQUEST):99u; }
/* This lease's conversation state. */
static struct {
    BOOL t2,gel_set,cam_set,shop,say_issued,stop_issued,end_issued,menus_issued,quarantine_logged;
    unsigned speech;      /* the native speech camera: SPEECH_* */
    BOOL pressed;         /* the player's interact press, while the farewell waits */
    BOOL gesture;         /* this conversation's lines are animated (KamoGestures) */
    BOOL listener_anim_set; uint8_t listener_anim_saved;
    DWORD face_at;        /* last turn step */
    uint8_t cam_saved;
    unsigned question,answer,line,next;
    float duration;
    const char *close;
    DWORD started,inject_at,end_since;
    uint32_t speaker[3];  /* TPtr{obj,...}: the natives read only its object */
    uint32_t listener[3]; /* this player's character, as vanilla's theActivator */
} talk;
enum { NEXT_REASK=1u, NEXT_END=2u };
/* The native speech camera during this lease (owner 2026-10-17): HELD while
 * CSpeech may select and aim it, RETURN once the conversation is ending
 * (SetMoveCamera(0) stops further aiming) until "default" is selected again,
 * then RETURNED. NONE: the camera was not available (house view kept). */
enum { SPEECH_NONE, SPEECH_HELD, SPEECH_RETURN, SPEECH_RETURNED };
enum { FACE_LIMIT_MS=1000u };
static void *service_actor;   /* this player's character, from the service call */
/* The conversation has visibly or natively begun and is not yet undone. */
static BOOL talk_witness(void) {
    uint8_t *cs=speech(),*ui=dialog();
    if(!cs || !ui) return TRUE;
    return *(uint32_t *)(ui+0x4cu) || hud_state()==2u || (talk.gel_set && cs[0x47du]) ||
        *(uint32_t *)(cs+0xc8u)!=2u || talk.speech==SPEECH_HELD || talk.speech==SPEECH_RETURN;
}
static const char *talk_refusal(const void *kamo) {
    uint8_t *cs=speech(),*ui=dialog(),*hud=hud_exact();
    if(!cs || !ui || !hud) return "speech_owners";
    if(*(uint32_t *)(cs+0xc8u)!=2u || cs[0x47au] || cs[0x47bu] || cs[0x47du] || cs[0x470u] || talking(cs)) return "speech_busy";
    if(*(uint32_t *)(ui+0x4cu) || (ui[0x6b8u]&0x82u) || (ui[0x6b9u]&2u)) return "dialog_busy";
    if(*(uint8_t **)(hud+0x80u)!=ui) return "dialog_layer";
    uint8_t *movie=global(RVA_MOVIE),*quick=readable(hud+0x70u,4u)?*(uint8_t **)(hud+0x70u):NULL;
    if(movie) return "movie";
    if(quick && (!readable(quick+0x29u,1u) || quick[0x29u])) return "quick_menu";
    if(SudekiMpLanStoryCinematicAudioRetains()) return "host_voice";
    if(!kamo || !readable(kamo,4u) || *(void *const *)kamo!=base+RVA_NPC_VTABLE) return "shopkeeper";
    unsigned q=talk.t2?Q_T2:Q_T1;
    if(!sentence_present(cs,q) || !sentence_present(cs,q+1u) || !sentence_present(cs,LINE_FAREWELL) ||
        (!talk.t2 && !sentence_present(cs,LINE_CHAT))) return "sentences";
    return NULL;
}

/* ---- lease ---- */
enum { IDLE, REQUESTED, OPENING, OPEN, CLOSING,
    T_REQUESTED, T_OPENING, T_ASK, T_QUESTION, T_ANSWER, T_SETTLE, T_SAY, T_REDIALOG, T_END, T_END_HUD, T_FACE, T_FAREWELL };
static unsigned phase;
static DWORD phase_at;
static BOOL prompt_shown,talk_armed=TRUE;
static const void *keeper_entity; static DWORD keeper_scan;
static const Shopkeeper *keeper;
static void enter(unsigned p,uint32_t now,const char *why) {
    static const char *const names[]={"idle","requested","opening","open","closing",
        "talk_requested","talk_opening","talk_ask","talk_question","talk_answer","talk_settle","talk_say",
        "talk_redialog","talk_end","talk_end_hud","talk_face","talk_farewell"};
    trace("story_shop event=%s why=%s\r\n",names[p],why);
    phase=p; phase_at=now?now:1u;
    if(p==IDLE) SudekiMpLanStoryRemoteInteriorClientHoldExit(FALSE);
}
/* The client's UI clears context flags; re-assert every frame while shown
 * (as the chest prompt, CONFIRMED_LIVE 2026-10-16). */
static uint32_t prompt_flag;          /* TALK_FLAG, EXAMINE_FLAG or 0 */
static const Examinable *examinable;  /* the one in reach (prompt 0x40) */
static void prompt_show(uint32_t flag) {
    if(!SudekiMpLanStoryRemoteUseIconSetLocal(flag)) flag=0u; /* no visible prompt */
    BOOL shown=flag!=0u;
    if(flag==prompt_flag && shown==prompt_shown) return;
    prompt_shown=shown; prompt_flag=flag;
    trace("story_shop event=prompt shown=%u flag=%lu what=%s\r\n",(unsigned)shown,(unsigned long)flag,
        flag==TALK_FLAG && keeper?keeper->npc:flag==EXAMINE_FLAG && examinable?examinable->entity:"-");
}
static void prompt_set(BOOL shown) { prompt_show(shown?TALK_FLAG:0u); }
static BOOL actor_at(const void *actor,float out[3]) {
    return SudekiMpLanStoryRemoteInteriorEntityPosition(actor,out);
}
/* Fill the native shop inventory as StartShopping does: name, buy list for
 * this game's story block plus missing starter weapons, then the sell list. */
static BOOL fill(void) {
    if(!readable(base+0x3c2fecu,2u)) return FALSE;
    int block=*(int16_t *)(base+0x3c2fecu); /* GetStoryBlock (0xFDA00) reads this int16 */
    const SudekiMpSolShopTier *tier=SudekiMpSolShopStockTier(&stock,block);
    if(!tier) return FALSE;
    ((VoidCall)(base+RVA_SHOP_CLEAR))();
    ((IntArg)(base+RVA_SHOP_NAME))(stock.name_text);
    ((BoolArg)(base+RVA_SHOP_MODE))(1);
    for(unsigned i=0;i<tier->count;++i) ((IntArg)(base+RVA_SHOP_ADD))(tier->items[i]);
    unsigned weapons=0;
    for(unsigned i=0;i<stock.weapon_count;++i)
        if(((Quantity)(base+RVA_ITEM_QUANTITY))(stock.weapons[i])==0) { ((IntArg)(base+RVA_SHOP_ADD))(stock.weapons[i]); ++weapons; }
    ((BoolArg)(base+RVA_SHOP_MODE))(0);
    for(unsigned i=0;i<stock.sell_count;++i) ((AddPrice)(base+RVA_SHOP_ADD_PRICE))(stock.sell[i].item,stock.sell[i].price);
    trace("story_shop event=fill block=%d min_block=%ld buy=%u weapons=%u sell=%u\r\n",
        block,(long)tier->min_block,(unsigned)tier->count,weapons,stock.sell_count);
    return TRUE;
}
static const char *open_refusal(void) {
    uint8_t *hud=hud_exact(),*shop=shop_layer(hud),*group=global(RVA_GROUP);
    if(!hud) return "hud";
    if(!shop) return "shop_layer";
    if(*(unsigned *)(hud+HUD_STATE)!=1u || *(unsigned *)(hud+HUD_REQUEST)!=1u ||
        *(unsigned *)(hud+0x54u) || hud[0x41u]) return "hud_busy";
    if(*(unsigned *)(shop+SHOP_STATE)) return "shop_busy";
    if(focus()!=hud) return "focus";
    if(!readable(group,0xd4u) || !*(void **)(group+0x90u)) return "front";
    if(!global(RVA_SHOP_STOCK) || !global(RVA_INVENTORY)) return "inventory";
    if(SudekiMpLanStoryMainMenuVisible()) return "main_menu";
    return NULL;
}
/* ---- the conversation steps (all inside the client's presentation step) ---- */
static uint8_t *scene_exact(void) {
    uint8_t *scene=global(RVA_SCENE),*layer=focus();
    return readable(scene,0x178u) && readable(layer,4u) && readable(*(uint8_t **)layer,0x30u)?scene:NULL;
}
static float line_seconds(const uint8_t *cs) {
    float d=*(const float *)(cs+0xd0u);
    return isfinite(d) && d>=0 && d<=60.0f?d:10.0f;
}
/* The conversation is ending: no further aiming, then the client's own
 * camera again (vanilla SwitchActiveCamera("default")); retried until done. */
static void speech_hold_end(void) {
    if(talk.speech!=SPEECH_HELD) return;
    uint8_t *cs=speech();
    if(cs) { ((SpeechSet)(base+RVA_SET_MOVE_CAMERA))(cs,0); talk.cam_set=TRUE; }
    talk.speech=SPEECH_RETURN;
}
/* Vanilla switches back after EndInteractionScript (the box has closed and
 * the HUD is back), not at the end of the last line. */
static void speech_return(void) {
    speech_hold_end();
    if(talk.speech==SPEECH_RETURN && SudekiMpLanStoryClientCameraSpeechReturn()) talk.speech=SPEECH_RETURNED;
}
/* Vanilla animates every shopkeeper line (Ask|PN and Say pass animate 1);
 * here only while the gesture driver can run the speaker's animation. */
static uint8_t talk_animate(void) { return talk.gesture && SudekiMpLanStoryNpcGestureReady()?1u:0u; }
/* This player's character as a TPtr the natives can read, or NULL. */
static void *talk_listener(void) {
    float at[3];
    if(!service_actor || !actor_at(service_actor,at)) return NULL;
    talk.listener[0]=(uint32_t)(uintptr_t)service_actor; talk.listener[1]=talk.listener[2]=0;
    return talk.listener;
}
static void talk_finish(uint32_t now,const char *why) {
    speech_hold_end();
    if(!talk.close) talk.close=why;
    if(!talk.end_since) talk.end_since=now?now:1u;
    talk.stop_issued=talk.end_issued=talk.menus_issued=FALSE; talk.inject_at=0;
    enter(T_END,now,why);
}
static void talk_restore(uint32_t now) {
    uint8_t *cs=speech();
    if(!cs || (talk.speech!=SPEECH_NONE && talk.speech!=SPEECH_RETURNED)) return;
    if(talk.gel_set) { ((SpeechSet)(base+RVA_SET_GEL))(cs,0); talk.gel_set=cs[0x47du]!=0; }
    if(talk.cam_set) { ((SpeechSet)(base+RVA_SET_MOVE_CAMERA))(cs,talk.cam_saved); talk.cam_set=cs[0x475u]!=talk.cam_saved; }
    if(talk.listener_anim_set) {
        ((SpeechSet)(base+RVA_SET_LISTENER_ANIMATES))(cs,talk.listener_anim_saved);
        talk.listener_anim_set=cs[0x473u]!=(talk.listener_anim_saved?1u:0u);
    }
    if(talk.gel_set || talk.cam_set || talk.listener_anim_set) return;
    uint8_t *ui=dialog();
    trace("story_shop event=talk_end why=%s ui=%lu speech=%lu igm=%u gel=%u move_camera=%u\r\n",talk.close?talk.close:"-",
        ui?(unsigned long)*(uint32_t *)(ui+0x4cu):99ul,(unsigned long)*(uint32_t *)(cs+0xc8u),hud_state(),
        (unsigned)cs[0x47du],(unsigned)cs[0x475u]);
    talk_armed=FALSE; talk.speech=SPEECH_NONE; enter(IDLE,now,"talk_done");
}
static void talk_step(uint32_t now,BOOL in_reach,BOOL draining) {
    uint8_t *cs=speech(),*ui=dialog();
    if(draining && !talk.close && phase!=T_REQUESTED) talk.close="stopping";
    if(phase!=T_REQUESTED && phase!=T_END && phase!=T_END_HUD && (!cs || !ui)) { talk_finish(now,"owners"); return; }
    /* An animated line whose speaker lock never comes would hold forever:
     * stop it and animate no further line (the gesture driver logs it). */
    if((phase==T_QUESTION || phase==T_SAY || phase==T_ASK) && SudekiMpLanStoryNpcGestureStalled(cs,now)) {
        ((SpeechVoid)(base+RVA_STOP))(cs); talk.gesture=FALSE;
    }
    switch(phase) {
    case T_REQUESTED: {
        const char *refused=draining?"stopping":!in_reach?"left_shopkeeper":!stock_ready?"stock":open_refusal();
        if(!refused) refused=!scene_exact()?"scene":talk_refusal(keeper_entity);
        if(refused) { trace("story_shop event=talk_refused reason=%s\r\n",refused); enter(IDLE,now,"refused"); break; }
        prompt_set(FALSE);
        talk.question=talk.t2?Q_T2:Q_T1;
        /* The shopkeeper's native gestures (StartInteractionScript loads his
         * gesture bank), when the switch is on and he is local to this game. */
        talk.gesture=SudekiMpLanStoryNpcGestureBegin(keeper_entity,now);
        if(talk.gesture) {
            /* Animated lines would also take the listener's TSA lock
             * (0x413C08, CSpeech+0x473): the listener is this player's own
             * character, posed by the host, whose animation this game never
             * runs. Its native setter turns that off for this conversation. */
            talk.listener_anim_saved=cs[0x473u];
            ((SpeechSet)(base+RVA_SET_LISTENER_ANIMATES))(cs,0); talk.listener_anim_set=TRUE;
            if(cs[0x473u]) { talk.gesture=FALSE; trace("story_shop event=talk_gesture_off reason=listener_flag\r\n"); }
        }
        talk.cam_saved=cs[0x475u];
        /* The native speech camera (vanilla), when the client camera can
         * give way to it and take the view back; else the house view. */
        if(talk.cam_saved==1u && SudekiMpLanStoryClientCameraSpeechReady()) talk.speech=SPEECH_HELD;
        else { ((SpeechSet)(base+RVA_SET_MOVE_CAMERA))(cs,0); talk.cam_set=TRUE; }
        ((SpeechSet)(base+RVA_SET_GEL))(cs,1); talk.gel_set=TRUE;
        ((VoidCall)(base+RVA_DIALOG_CLEAR))(); ((VoidCall)(base+RVA_DIALOG_START))();
        if(cs[0x475u]!=(talk.speech==SPEECH_HELD?1u:0u) || cs[0x47du]!=1u || !(ui[0x6b8u]&0x80u)) {
            talk_finish(now,"open_unproven"); break;
        }
        trace("story_shop event=talk_open t2=%u question=%u igm=%u request=%u speech_camera=%u\r\n",(unsigned)talk.t2,
            talk.question,hud_state(),hud_request(),(unsigned)(talk.speech==SPEECH_HELD));
        /* Vanilla turns the shopkeeper to the player before ShowDialog. */
        talk.face_at=now?now:1u;
        enter(T_FACE,now,"start_conversation");
        break; }
    case T_FACE: {
        if(talk.close) { talk_finish(now,talk.close); break; }
        /* At most 360 degrees a second (his authored standing turn rate),
         * stepped by this frame's real time (bounded). */
        DWORD dt=now-talk.face_at; talk.face_at=now?now:1u;
        if(dt>50u) dt=50u;
        float at[3]; BOOL done=TRUE;
        BOOL turned=service_actor && actor_at(service_actor,at) && keeper_entity &&
            SudekiMpLanStoryWorldLocalTurn(keeper_entity,at,6.2831853f*(float)dt/1000.0f,&done);
        if(!turned || done || now-phase_at>=FACE_LIMIT_MS) {
            trace("story_shop event=talk_face result=%s after_ms=%lu\r\n",!turned?"not_local":done?"facing":"timeout",
                (unsigned long)(now-phase_at));
            ((SceneState)(base+RVA_SET_IGM))(scene_exact(),2);
            enter(T_OPENING,now,"faced");
        }
        break; }
    case T_OPENING:
    case T_REDIALOG: {
        unsigned st=hud_state(),rq=hud_request();
        if(talk.close) { talk_finish(now,talk.close); break; }
        if(st==2u && focus()==ui) {
            if(phase==T_REDIALOG) talk.question=(talk.t2?Q_T2:Q_T1)+1u;
            enter(T_ASK,now,"dialog_shown"); break;
        }
        if(phase==T_REDIALOG && !talk.menus_issued && (st==0u || st==1u) && focus()==hud_exact() && scene_exact()) {
            /* Back from the shop: ShowDialog, as StartShopping does. */
            ((SceneState)(base+RVA_SET_IGM))(scene_exact(),2); talk.menus_issued=TRUE; break;
        }
        if(now-phase_at>=TALK_WAIT_MS || (phase==T_OPENING && st==1u && rq==1u)) talk_finish(now,"dialog_refused");
        break; }
    case T_ASK: {
        if(talk.close) { talk_finish(now,talk.close); break; }
        if(talking(cs) || hud_state()!=2u || *(uint32_t *)(cs+0xc8u)!=2u || cs[0x47au] || *(uint32_t *)(ui+0x4cu)==3u ||
            !keeper_entity || *(void *const *)keeper_entity!=base+RVA_NPC_VTABLE) {
            if(now-phase_at>=TALK_STEP_MS) talk_finish(now,"ask_wait");
            break;
        }
        talk.speaker[0]=(uint32_t)(uintptr_t)keeper_entity; talk.speaker[1]=talk.speaker[2]=0;
        /* Vanilla re-asks (T1b/T2b) name the player as listener; the first
         * question has none. With animate 0 the listener only frames the
         * speech camera (its TSA lock at 0x413C08 needs CSpeech+0x474). */
        void *listener=talk.question!=Q_T1 && talk.question!=Q_T2?talk_listener():NULL;
        uint8_t r=((SpeechLine)(base+RVA_ASK))(cs,talk.speaker,listener,(int)talk.question,1,talk_animate(),0,1);
        const uint16_t *text=*(const uint16_t **)(cs+0xd8u);
        BOOL proven=r && cs[0x47au]==1u && readable(text,2u) && text[0] && *(void **)(cs+0xdcu) &&
            *(void **)(cs+0xe0u) && (ui[0x6b8u]&0x20u) && !*(uint32_t *)(ui+0x6a4u);
        talk.duration=line_seconds(cs);
        trace("story_shop event=talk_ask question=%u proven=%u duration_ms=%u\r\n",talk.question,(unsigned)proven,(unsigned)(talk.duration*1000.0f));
        if(!proven) talk_finish(now,"ask_unproven"); else enter(T_QUESTION,now,"asked");
        break; }
    case T_QUESTION:
        if(*(uint32_t *)(ui+0x4cu)==3u) enter(T_ANSWER,now,"menu_shown");
        else if(talk.close) talk_finish(now,talk.close);
        else if(now-phase_at>(uint32_t)(talk.duration*1000.0f)+4000u) talk_finish(now,"question_timeout");
        break;
    case T_ANSWER: {
        if(talk.close) { talk_finish(now,talk.close); break; }
        uint32_t a=*(uint32_t *)(ui+0x6a4u);
        if(!a || hud_state()!=2u || cs[0x47au]!=1u) break;   /* the player is choosing */
        int got=((SpeechInt)(base+RVA_ANSWER))(cs);
        talk.answer=got>0?(unsigned)got:a;
        trace("story_shop event=talk_answer question=%u answer=%u\r\n",talk.question,talk.answer);
        if(talk.answer<1u || talk.answer>(talk.t2?2u:3u)) talk_finish(now,"answer_invalid");
        else enter(T_SETTLE,now,"answered");
        break; }
    case T_SETTLE: {
        if(talk.close) { talk_finish(now,talk.close); break; }
        if(talking(cs)) { if(now-phase_at>=TALK_STEP_MS) talk_finish(now,"settle_wait"); break; }
        BOOL farewell=talk.answer==(talk.t2?2u:3u);
        if(talk.answer==1u) {
            /* StartShopping from the dialog: fill, ShopStart(0) at HUD 2. */
            uint8_t *hud=hud_exact(),*shop=shop_layer(hud),*group=global(RVA_GROUP);
            BOOL ok=hud && shop && *(unsigned *)(hud+HUD_STATE)==2u && *(unsigned *)(hud+HUD_REQUEST)==2u &&
                !*(unsigned *)(hud+0x54u) && !hud[0x41u] && !*(unsigned *)(shop+SHOP_STATE) &&
                *(uint32_t *)(ui+0x4cu)!=3u && *(uint32_t *)(cs+0xc8u)==2u && readable(group,0xd4u) &&
                *(void **)(group+0x90u) && global(RVA_SHOP_STOCK) && global(RVA_INVENTORY);
            if(!ok) { if(now-phase_at>=TALK_STEP_MS) talk_finish(now,"shop_refused"); break; }
            if(!fill()) { talk_finish(now,"stock_tier"); break; }
            ((BoolArg)(base+RVA_SHOP_START))(0);
            trace("story_shop event=start hud=%u request=%u talk=1\r\n",hud_state(),hud_request());
            talk.shop=TRUE; enter(OPENING,now,"shop_start");
        } else {
            talk.line=farewell?LINE_FAREWELL:LINE_CHAT; talk.next=farewell?NEXT_END:NEXT_REASK;
            talk.say_issued=talk.stop_issued=FALSE; enter(T_SAY,now,farewell?"farewell":"chat");
        }
        break; }
    case T_SAY: {
        if(!talk.say_issued) {
            if(talk.close) { talk_finish(now,talk.close); break; }
            if(talking(cs) || hud_state()!=2u || !keeper_entity || *(void *const *)keeper_entity!=base+RVA_NPC_VTABLE) {
                if(now-phase_at>=TALK_STEP_MS) talk_finish(now,"say_wait");
                break;
            }
            talk.speaker[0]=(uint32_t)(uintptr_t)keeper_entity;
            /* Say(theEntity, theActivator, ...): the player is the listener
             * (vanilla's two-shot framing of the farewell). */
            uint8_t r=((SpeechLine)(base+RVA_LINE))(cs,talk.speaker,talk_listener(),(int)talk.line,0,1,talk_animate(),0);
            uint32_t mode=*(uint32_t *)(cs+0xc8u); const uint16_t *text=*(const uint16_t **)(cs+0xd8u);
            BOOL ok=r && (mode==1u || mode==2u) && readable(text,2u) && text[0];
            talk.say_issued=TRUE; talk.duration=line_seconds(cs); phase_at=now?now:1u;
            trace("story_shop event=talk_say line=%u ok=%u duration_ms=%u\r\n",talk.line,(unsigned)ok,(unsigned)(talk.duration*1000.0f));
            if(ok) break;
        } else if(talking(cs)) {
            if(talk.close && !talk.stop_issued) { ((SpeechVoid)(base+RVA_STOP))(cs); talk.stop_issued=TRUE; phase_at=now?now:1u; }
            else if(!talk.stop_issued && now-phase_at>(uint32_t)(talk.duration*1000.0f)+3000u) {
                ((SpeechVoid)(base+RVA_STOP))(cs); talk.stop_issued=TRUE; phase_at=now?now:1u;
            } else if(talk.stop_issued && now-phase_at>=TALK_STEP_MS) talk_finish(now,"say_stuck");
            break;
        }
        if(talk.close) talk_finish(now,talk.close);
        else if(talk.next==NEXT_REASK) { talk.question=(talk.t2?Q_T2:Q_T1)+1u; enter(T_ASK,now,"reask"); }
        else { talk.pressed=FALSE; enter(T_FAREWELL,now,"farewell_shown"); }
        break; }
    case T_FAREWELL:
        /* As on the host's own game: the farewell stays on screen, in its
         * camera, until the player presses interact again: the press seen by
         * the runtime, or the dialog's own confirm (CSpeech+0x47B, set by
         * the UIDialog input in its text state, 0x4A4590). */
        if(talk.close) talk_finish(now,talk.close);
        else if(talk.pressed || cs[0x47bu]) talk_finish(now,"farewell");
        break;
    case T_END: {
        /* EndInteractionScript, from any point: no line is played on a
         * close; an open question is answered (the highlighted choice,
         * ignored) so the conversation can end. The speech camera stays until
         * the box has closed (or this end is slow: then it goes back now). */
        if(now-talk.end_since>=TALK_STEP_MS) speech_return();
        if(cs && ui) {
            if(*(uint32_t *)(ui+0x4cu)==3u && focus()==ui) {
                if(!talk.inject_at || now-talk.inject_at>=500u) {
                    LayerInput input=(LayerInput)(base+RVA_DIALOG_INPUT);
                    (void)input(ui,6u,0u,0u); (void)input(ui,5u,0u,0u); (void)input(ui,6u,0u,0u);
                    talk.inject_at=now?now:1u;
                }
                break;
            }
            if(*(uint32_t *)(ui+0x6a4u) && cs[0x47au]==1u && hud_state()==2u) (void)((SpeechInt)(base+RVA_ANSWER))(cs);
            if(!talk.stop_issued && (*(uint32_t *)(cs+0xc8u)!=2u || talking(cs))) { ((SpeechVoid)(base+RVA_STOP))(cs); talk.stop_issued=TRUE; }
            if(!talk.end_issued && (ui[0x6b8u]&0x80u)) { ((VoidCall)(base+RVA_DIALOG_END))(); talk.end_issued=TRUE; }
            if(!((BoolVoid)(base+RVA_DIALOG_ACTIVE))() && *(uint32_t *)(cs+0xc8u)==2u) {
                if(hud_state()!=1u) ((VoidCall)(base+RVA_SHOW_INGAME))();
                talk.menus_issued=FALSE; enter(T_END_HUD,now,"dialog_closed"); break;
            }
        }
        if(now-phase_at>=TALK_STEP_MS) { talk.stop_issued=talk.end_issued=FALSE; phase_at=now?now:1u; }
        if(now-talk.end_since>=TALK_QUARANTINE_MS && !talk.quarantine_logged) {
            talk.quarantine_logged=TRUE;
            trace("story_shop event=talk_quarantined ui=%ld speech=%ld igm=%u policy=hold_and_retry\r\n",
                ui?(long)*(uint32_t *)(ui+0x4cu):-1l,cs?(long)*(uint32_t *)(cs+0xc8u):-1l,hud_state());
        }
        break; }
    case T_END_HUD: {
        speech_return();
        unsigned st=hud_state();
        /* EndInteractionScript also releases the speaker's gesture lock and
         * dumps his gesture bank (bounded inside the driver). */
        if(st==1u && cs && !talking(cs) && SudekiMpLanStoryNpcGestureEnd(now)) { talk_restore(now); break; }
        if(now-phase_at>=TALK_STEP_MS) {
            if(st==0u || st==2u) ((VoidCall)(base+RVA_SHOW_INGAME))();
            phase_at=now?now:1u;
            if(now-talk.end_since>=TALK_QUARANTINE_MS && !talk.quarantine_logged) { talk.quarantine_logged=TRUE;
                trace("story_shop event=talk_quarantined igm=%u policy=hold_and_retry\r\n",st); }
        }
        break; }
    default: break;
    }
}
/* One step of the lease. `in_reach`: this frame still finds the player at
 * the shopkeeper in their settled shop; `draining`: runtime teardown (an
 * unopened request ends, an open shop is closed through its back command). */
static void lease_step(uint32_t now,BOOL in_reach,BOOL draining) {
    if(phase>=T_REQUESTED) { talk_step(now,in_reach,draining); return; }
    if(draining && talk.shop && !talk.close) talk.close="stopping";
    switch(phase) {
    case REQUESTED: {
        const char *refused=draining?"stopping":!in_reach?"left_shopkeeper":!stock_ready?"stock":open_refusal();
        if(refused) {
            trace("story_shop event=open_refused reason=%s\r\n",refused);
            enter(IDLE,now,"refused"); break;
        }
        prompt_set(FALSE);
        if(!fill()) { enter(IDLE,now,"stock_tier"); break; }
        ((BoolArg)(base+RVA_SHOP_START))(0);
        uint8_t *hud=hud_exact();
        trace("story_shop event=start hud=%u request=%u\r\n",hud?*(unsigned *)(hud+HUD_STATE):99u,
            hud?*(unsigned *)(hud+HUD_REQUEST):99u);
        enter(OPENING,now,"shop_start");
        break; }
    case OPENING: {
        uint8_t *hud=hud_exact();
        unsigned state=hud?*(unsigned *)(hud+HUD_STATE):99u,request=hud?*(unsigned *)(hud+HUD_REQUEST):99u;
        if(state==7u || state==8u) { enter(OPEN,now,"applied"); break; }
        if(state==0u && request==7u && now-phase_at<OPEN_WAIT_MS) break;          /* pending */
        if(talk.shop && state==1u && request==1u) {
            /* Back in game under the conversation's shop: end the
             * conversation (its restore), never the plain-shop close. */
            ((VoidCall)(base+RVA_SHOP_CLEAR))(); talk.shop=FALSE;
            talk_finish(now,talk.close?talk.close:"shop_refused"); break;
        }
        if(talk.shop && (now-phase_at>=OPEN_WAIT_MS || (state==2u && request==2u))) {
            /* Refused from the dialog: ask again, as vanilla when the shop
             * is not active on its first poll. */
            ((VoidCall)(base+RVA_SHOP_CLEAR))(); talk.shop=FALSE; talk.menus_issued=FALSE;
            if(talk.close) talk_finish(now,talk.close); else enter(T_REDIALOG,now,"shop_refused");
            break;
        }
        if(now-phase_at>=OPEN_WAIT_MS || (state==1u && request==1u)) {
            /* Refused or never applied: back to the game HUD. */
            ((VoidCall)(base+RVA_SHOP_CLEAR))();
            if(state!=1u) ((VoidCall)(base+RVA_SHOW_INGAME))();
            enter(CLOSING,now,"open_refused");
        }
        break; }
    case OPEN:
        if(shop_witness()==W_CLOSED && talk.shop) {
            /* Back to Kamo's question (StartShopping: ShopClearInventory,
             * ShowDialog), or to the end of the conversation. */
            ((VoidCall)(base+RVA_SHOP_CLEAR))(); talk.shop=FALSE; talk.menus_issued=FALSE;
            if(talk.close) talk_finish(now,talk.close); else enter(T_REDIALOG,now,"shop_closed");
        } else if(shop_witness()==W_CLOSED) {
            ((VoidCall)(base+RVA_SHOP_CLEAR))();
            ((VoidCall)(base+RVA_SHOW_INGAME))();
            enter(CLOSING,now,"shop_closed");
        } else if(draining) SudekiMpLanStoryShopRequestClose("stopping");
        break;
    case CLOSING: {
        uint8_t *hud=hud_exact();
        if(hud && *(unsigned *)(hud+HUD_STATE)==1u && shop_witness()==W_CLOSED) {
            talk_armed=FALSE; enter(IDLE,now,"in_game");
        } else if(now-phase_at>=CLOSE_WAIT_MS) {
            /* Never leave the HUD hidden: ask once more, then keep waiting. */
            if(hud && *(unsigned *)(hud+HUD_STATE)==0u) ((VoidCall)(base+RVA_SHOW_INGAME))();
            phase_at=now?now:1u; trace("story_shop event=close_wait hud=%u\r\n",hud?*(unsigned *)(hud+HUD_STATE):99u);
        }
        break; }
    default: break;
    }
}
static DWORD last_service_at;
void SudekiMpLanStoryShopService(void *actor,const char *in_shop,BOOL blocked,uint32_t now) {
    if(!installed || (native_thread && native_thread!=GetCurrentThreadId())) return;
    native_thread=GetCurrentThreadId(); last_service_at=now?now:1u; service_actor=actor;
    /* The shopkeeper of the table shop this player is settled in, in reach? */
    keeper=SudekiMpLanStoryShopkeeperOf(in_shop);
    float at[3],npc[3];
    BOOL in_reach=FALSE,out_of_reach=TRUE;
    if(keeper && actor && actor_at(actor,at)) {
        keeper_entity=SudekiMpLanStoryRemoteInteriorEntityNamed(keeper->npc,keeper_entity,&keeper_scan,now);
        if(keeper_entity && actor_at(keeper_entity,npc)) {
            float dx=at[0]-npc[0],dz=at[2]-npc[2],d=sqrtf(dx*dx+dz*dz);
            BOOL level=fabsf(at[1]-npc[1])<TALK_HEIGHT;
            in_reach=level && d<TALK_RADIUS; out_of_reach=!level || d>TALK_REARM;
        } else {
            static BOOL missing_logged;
            if(!missing_logged) { missing_logged=TRUE; trace("story_shop event=shopkeeper_missing npc=%s\r\n",keeper->npc); }
        }
    }
    lease_step(now,in_reach && !blocked,FALSE);
    SudekiMpLanStoryNpcGestureTick(now);
    if(phase!=IDLE) return;
    if(out_of_reach) talk_armed=TRUE;
    /* An examinable in reach (its box not already up). */
    static const void *examine_entity; static DWORD examine_scan;
    const Examinable *near_examinable=NULL;
    for(unsigned i=0;in_shop && actor && !blocked && i<sizeof(examinables)/sizeof(examinables[0]) && !near_examinable;++i) {
        const Examinable *e=&examinables[i]; float p[3];
        if(_stricmp(e->zone,in_shop)) continue;
        examine_entity=SudekiMpLanStoryRemoteInteriorEntityNamed(e->entity,examine_entity,&examine_scan,now);
        if(examine_entity && actor_at(actor,at) && actor_at(examine_entity,p)) {
            float dx=at[0]-p[0],dz=at[2]-p[2];
            if(fabsf(at[1]-p[1])<TALK_HEIGHT && sqrtf(dx*dx+dz*dz)<e->radius) near_examinable=e;
        }
    }
    if(SudekiMpLanStoryZoneBannerExamineActive()) near_examinable=NULL;
    examinable=near_examinable;
    /* Native icon priority: talk (flag 2) before examine (0x40). No prompt
     * the press could not honour (no stock, another modal). */
    if(in_reach && talk_armed && !blocked && stock_ready) prompt_show(TALK_FLAG);
    else prompt_show(near_examinable?EXAMINE_FLAG:0u);
}
BOOL SudekiMpLanStoryShopDrain(uint32_t now) {
    if(!installed || phase==IDLE) return TRUE;
    if(native_thread && native_thread!=GetCurrentThreadId()) return FALSE;
    last_service_at=now?now:1u;
    lease_step(now,FALSE,TRUE);
    SudekiMpLanStoryNpcGestureTick(now);
    if(phase==IDLE) SudekiMpLanStoryNpcGestureDrain("stopping");
    if(phase==IDLE && prompt_shown) prompt_set(FALSE);
    return phase==IDLE;
}
BOOL SudekiMpLanStoryShopPromptShown(void) { return installed && phase==IDLE && prompt_shown; }
BOOL SudekiMpLanStoryShopIdle(void) { return !installed || phase==IDLE; }
void SudekiMpLanStoryShopInteractPress(void) {
    if(installed && phase==T_FAREWELL) talk.pressed=TRUE;
}
BOOL SudekiMpLanStoryShopSpeechCameraHeld(void) {
    return installed && (talk.speech==SPEECH_HELD || talk.speech==SPEECH_RETURN);
}
void SudekiMpLanStoryShopRequestOpen(void) {
    /* Only a prompt the presentation step is servicing now (a press taken
     * while it is not running is never queued). */
    if(!installed || phase!=IDLE || !prompt_shown || (int32_t)(GetTickCount()-last_service_at)>=500) return;
    if(prompt_flag==EXAMINE_FLAG) {
        /* The examinable's box on this game; the prompt hides while it is up. */
        if(examinable && SudekiMpLanStoryZoneBannerExamine(examinable->zone,examinable->text,examinable->mode,
            examinable->flags,examinable->emitter,GetTickCount()))
            trace("story_shop event=examine entity=%s\r\n",examinable->entity);
        prompt_show(0u); return;
    }
    talk_armed=FALSE;
    if(talk_ready && stock_ready) {
        /* Kamo's greeting first, then his shop from the choice (D11). */
        memset(&talk,0,sizeof(talk));
        talk.t2=readable(base+0x3c2fecu,2u) && *(int16_t *)(base+0x3c2fecu)>=T2_BLOCK; /* GetStoryBlock */
        talk.started=GetTickCount(); if(!talk.started) talk.started=1u;
        enter(T_REQUESTED,talk.started,"interact");
    } else enter(REQUESTED,GetTickCount(),"interact");
    SudekiMpLanStoryRemoteInteriorClientHoldExit(TRUE); /* from this frame on */
}
/* Phase alone is not enough: the lease advances only inside the client's
 * presentation step. If that step stops running, the gates (movement mute,
 * Escape, loot, the house exit hold) follow the read-only witness instead,
 * so they cannot stay held for a shop that is no longer shown. */
BOOL SudekiMpLanStoryShopActive(void) {
    if(!installed || phase==IDLE) return FALSE;
    if((int32_t)(GetTickCount()-last_service_at)<3000) return TRUE;
    static BOOL quarantine_logged;
    BOOL shown=shop_witness()!=W_CLOSED ||
        ((phase>=T_REQUESTED || talk.shop || talk.gel_set || talk.cam_set) && talk_witness());
    if(!quarantine_logged) { quarantine_logged=TRUE;
        trace("story_shop event=lease_unserviced phase=%u shown=%u policy=witness_gates\r\n",phase,(unsigned)shown); }
    return shown;
}
/* The player's area is changing under the shop (e.g. the lead left the
 * house): leave the shop through its own back command, at most twice a
 * second, until the witness reads closed. */
void SudekiMpLanStoryShopRequestClose(const char *why) {
    static DWORD sent_at;
    if(!installed) return;
    DWORD now=GetTickCount();
    if(phase==REQUESTED || phase==T_REQUESTED) { enter(IDLE,now,why); return; }
    if(phase>=T_OPENING || ((phase==OPENING || phase==OPEN) && talk.shop)) { if(!talk.close) talk.close=why; }
    if(phase!=OPEN || (sent_at && now-sent_at<500u)) return;
    uint8_t *hud=hud_exact(),*shop=shop_layer(hud);
    unsigned state=shop?*(unsigned *)(shop+SHOP_STATE):0u;
    if(!shop || focus()!=shop || (state!=7u && state!=10u && state!=11u)) return;
    sent_at=now?now:1u;
    uint8_t result=((LayerInput)(base+RVA_SHOP_INPUT))(shop,5u,1u,0u);
    trace("story_shop event=close_request why=%s state=%u result=%u\r\n",why,state,(unsigned)result);
}
void SudekiMpLanStoryShopPollKeys(void) {
    enum { K_ESCAPE, K_M, K_F3, K_COUNT };
    static const int keys[K_COUNT]={VK_ESCAPE,'M',VK_F3};
    static BOOL down[K_COUNT];
    if(!installed || phase!=OPEN) { memset(down,0,sizeof(down)); return; }
    DWORD owner=0; HWND fg=GetForegroundWindow();
    BOOL focused=fg && GetWindowThreadProcessId(fg,&owner) && owner==GetCurrentProcessId();
    BOOL edge[K_COUNT];
    for(unsigned i=0;i<K_COUNT;++i) { BOOL now=(GetAsyncKeyState(keys[i])&0x8000)!=0; edge[i]=now && !down[i] && focused; down[i]=now; }
    if(!edge[K_ESCAPE] && !edge[K_M] && !edge[K_F3]) return;
    uint8_t *hud=hud_exact(),*shop=shop_layer(hud);
    unsigned state=shop?*(unsigned *)(shop+SHOP_STATE):0u;
    /* Only to the focused shop layer, in its browse/confirm states. */
    if(!shop || focus()!=shop || (state!=7u && state!=10u && state!=11u)) return;
    unsigned command=edge[K_F3]?10u:1u;
    if(command==10u && state!=7u) return;
    uint8_t result=((LayerInput)(base+RVA_SHOP_INPUT))(shop,5u,command,0u);
    trace("story_shop event=key command=%u state=%u result=%u\r\n",command,state,(unsigned)result);
}

BOOL SudekiMpLanStoryShopInstall(HMODULE image) {
    if(installed) return TRUE;
    uint8_t *b=(uint8_t *)image;
    if(!b || !SudekiMpCheckLoadedExecutable(image)) { SetLastError(ERROR_INVALID_DATA); return FALSE; }
    base=b;
    if(!image_exact()) { base=NULL; SetLastError(ERROR_INVALID_DATA); return FALSE; }
    stock_ready=SudekiMpSolShopStockLoad(image,&stock);
    if(!stock_ready) trace("story_shop event=stock_unavailable policy=no_shop\r\n");
    /* The conversation needs its own exact routines; without them the
     * prompt opens the shop directly (stage one). The verified script file
     * (stock_ready) fixes the sentence ids. */
    talk_ready=talk_exact(b);
    if(!talk_ready) trace("story_shop event=talk_unavailable policy=direct_shop\r\n");
    else (void)SudekiMpLanStoryNpcGestureInstall(image);   /* off unless [StoryAreas] KamoGestures */
    if(!SudekiMpInstallRelativeCallHook(&move_hook,b+RVA_SHOW_MOVE_CALL,b+RVA_DISABLE_MOVE,(const void *)(uintptr_t)move_skip)) {
        DWORD error=GetLastError(); base=NULL; SetLastError(error); return FALSE;
    }
    phase=IDLE; prompt_shown=FALSE; talk_armed=TRUE; keeper_entity=NULL; keeper_scan=0; native_thread=0;
    memset(&talk,0,sizeof(talk));
    installed=TRUE;
    trace("story_shop event=installed stock=%u tiers=%u sell=%u talk=%u\r\n",(unsigned)stock_ready,stock.tier_count,stock.sell_count,(unsigned)talk_ready);
    return TRUE;
}
BOOL SudekiMpLanStoryShopUninstall(void) {
    if(!installed) return TRUE;
    /* Never restore the skipped call while a shop may still be shown: its
     * later Show(false) would run an unbalanced 0x40AFD0. */
    if(phase!=IDLE || shop_witness()==W_OPEN || talk.gel_set || talk.cam_set || talk.listener_anim_set ||
        talk.speech==SPEECH_HELD || talk.speech==SPEECH_RETURN || !SudekiMpLanStoryNpcGestureUninstall()) {
        SetLastError(ERROR_BUSY); return FALSE;
    }
    if(prompt_shown) prompt_set(FALSE);
    if(!SudekiMpRestoreRelativeCallHook(&move_hook)) { SetLastError(ERROR_BUSY); return FALSE; }
    installed=FALSE; base=NULL; native_thread=0;
    return TRUE;
}
