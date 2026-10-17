#include "hooks/lan_story_npc_gesture.h"
#include "hooks/lan_story_world.h"
#include "hooks/lan_story_ambient.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <math.h>
#include <string.h>

/* Native contracts (CONFIRMED_STATIC on the supported image; live host and
 * client reads 2026-10-17):
 * - GMA (CNewGameModelAnimation, vtable 0x2C8504) is NPCEntity+0x3A4 ==
 *   *(entity+0x130) == *(entity+0x58); gma+0x10 = entity; lock state +0x131
 *   (0 none, 1 locking, 2 locked, 3 playing, 4 unlocking); +0x133 bit 4 =
 *   initialised; +0x58 pending bank IO; +0x5C/+0x64 deferred dump list.
 * - Update slot +0x40 = 0xE1930 thiscall(gma, ctx) RET 4 (reads ctx+0xC = dt).
 *   It starts with 0xDFA40: deferred dumps, then the renderer event drain
 *   0x58ABF0 (sounds, script messages, camera shake).
 * - Renderer (*(wrapper+0xC) == *(wrapper+0x10), vtable 0x2DF8EC) slot +0xA8 =
 *   0x223880 thiscall(renderer, out[0x14]) RET 4: pops one pending event
 *   (AL=1) or reports none (AL=0).
 * - Bank: LoadAnimationBank core 0xE0EA0 stdcall(gma, bank, 0, 0) RET 0x10;
 *   IsLoaded 0xE0F30 stdcall(gma, bank) RET 8; InUse 0xE1000 stdcall(gma,
 *   bank) RET 8; Dump 0xE0E20 (EAX = gma, push bank) RET 4 (queues);
 *   0xE01C0 stdcall(gma) RET 4 processes the queue.
 * - Vanilla talk (StartInteractionScript / Say / EndInteractionScript) loads
 *   bank 5 (the BODY* gestures) and dumps it at the end. */
static const struct { const char *name; unsigned rva,size; uint32_t hash; } gesture_codes[]={
    {"GMA_UPDATE",0xe1930u,0x4e1u,0x5aaeb4e0u},
    {"GMA_EVENT_PUMP",0xdfa40u,0x6fu,0x930344dfu},
    {"GMA_TICK_CHANNELS",0xdfab0u,0x4f6u,0x5dc18d5eu},
    {"GMA_PROCESS_DUMPS",0xe01c0u,0xc7u,0x7dd823beu},
    {"GMA_SLOT_CHANNEL",0xe1090u,0x77u,0x1eb4ff0fu},
    {"GMA_SLOT_DESTROY_FX",0xe5c50u,0xbfu,0x9c8128ceu},
    {"EVENT_DRAIN",0x18abf0u,0x332u,0xc89e0642u},
    {"EVENT_LISTENERS_18",0x18a760u,0x67u,0xb3c76b27u},
    {"EVENT_LISTENERS_28",0x18a7d0u,0x3eu,0xeea1d66bu},
    {"EVENT_LISTENERS_2C",0x18a810u,0x69u,0xbc768fb4u},
    {"EVENT_LISTENERS_34",0x18a880u,0x3eu,0x5bf88ee7u},
    {"EVENT_LISTENERS_8C0",0x18a8c0u,0x3cu,0x0b46d55bu},
    {"GMA_E3780",0xe3780u,0x2dau,0xfc0bb4cfu},
    {"GMA_E2110",0xe2110u,0x360u,0x76453fecu}  /* hashed without the effect_retire call window */,
    {"GMA_E2470",0xe2470u,0x36eu,0x033ced12u},
    {"GMA_E2810",0xe2810u,0x3b4u,0x88e36a40u},
    {"GMA_E36E0",0xe36e0u,0x94u,0x5936bc65u},
    {"GMA_E3640",0xe3640u,0x94u,0xdc392291u},
    {"GMA_E4D00",0xe4d00u,0xf7u,0xa0841b73u},
    {"GMA_E4E00",0xe4e00u,0x144u,0x2de426c4u},
    {"GMA_E43E0",0xe43e0u,0x76u,0x091e5d3au},
    {"GMA_E43B0",0xe43b0u,0x2du,0x9fa1943cu},
    {"GMA_SLOT_LOADED",0xe1110u,0x76u,0x0c1d4f13u},
    {"GMA_E5490",0xe5490u,0x50u,0xdad5bc9au},
    {"C3650",0xc3650u,0x1d4u,0x54f9f5fbu},
    {"C3500",0xc3500u,0xd8u,0x7371a02au},
    {"C35E0",0xc35e0u,0x62u,0x890cebf9u},
    {"CFFE0",0xcffe0u,0x1d4u,0x3c659766u},
    {"GMA_EA640",0xea640u,0x178u,0x105a5538u},
    {"GMA_E3AF0",0xe3af0u,0xfdu,0x0e6d9cbeu},
    {"GMA_E3D10",0xe3d10u,0x5d4u,0x061946aau},
    {"GMA_ANIM_ENTRY",0xe42f0u,0xb3u,0x0c635873u},
    {"GMA_E4F50",0xe4f50u,0x1e0u,0x1dc00ea0u},
    {"TSA_LOCK",0xe4460u,0x170u,0x02f02549u},
    {"TSA_UNLOCK",0xe45d0u,0x124u,0xee0e6182u},
    {"TSA_IS_LOCKED",0x3aef0u,0x17u,0xad6727b9u},
    {"GMA_BANK_LOAD",0xe0ea0u,0x87u,0x260ba679u},
    {"GMA_BANK_LOADED",0xe0f30u,0xcfu,0x452debadu},
    {"GMA_BANK_IN_USE",0xe1000u,0x83u,0x38f040b3u},
    {"GMA_BANK_DUMP",0xe0e20u,0x7eu,0x66b913e3u},
    {"RENDERER_EVENT_POP",0x223880u,0x175u,0x3037db4du},
    {"RENDERER_EVENT_REWIND",0x220540u,0x212u,0xc2fcaeb4u},
    {"RENDERER_EVENT_NEXT",0x220760u,0xc4u,0xe0ce478au},
    {"RENDERER_EVENT_ADVANCE",0x220830u,0x7eu,0x9738ec82u},
    {"SPEECH_GESTURE_CHOOSE",0x141c0u,0xe8u,0x7389f803u},
    {"SPEECH_GESTURE_START",0x14810u,0x9eu,0x28b7752cu},
};
static const struct { unsigned rva,target; } gesture_relocations[]={
   {0xe1b99u,0x339be0u}, {0xe1cf7u,0x2e36ccu}, {0xe1d06u,0x2e36c8u}, {0xe1d1bu,0x2e36c0u},
   {0xe1d2au,0x2e36b8u}, {0xe1d32u,0x2e36b0u}, {0xe1d3fu,0x2e36a8u}, {0xdfbb3u,0x2e36ccu},
   {0xdfbcau,0x2e36c8u}, {0xdfbe3u,0x2e36c0u}, {0xdfbf2u,0x2e36b8u}, {0xdfbfau,0x2e36b0u},
   {0xdfc07u,0x2e36a8u}, {0xdfd2au,0x2e39b0u}, {0xdfd74u,0x2e36ccu}, {0xdfd89u,0x2e36c8u},
   {0xdfd9eu,0x2e36c0u}, {0xdfdadu,0x2e36b8u}, {0xdfdb5u,0x2e36b0u}, {0xdfdc2u,0x2e36a8u},
   {0xdfe93u,0x408dd0u}, {0xe5cd2u,0x3644e8u}, {0xe5cdau,0x29a068u}, {0xe5ce0u,0x3c31f4u},
   {0xe5ce5u,0x3644e8u}, {0xe5ceeu,0x3c31f4u}, {0xe5cf4u,0x29a064u}, {0x18ac8fu,0x18af08u},
   {0x18ac96u,0x18aeccu}, {0x18aeccu,0x18ac9au}, {0x18aed0u,0x18ace3u}, {0x18aed4u,0x18acf3u},
   {0x18aed8u,0x18ae9cu}, {0x18aedcu,0x18ad36u}, {0x18aee0u,0x18ad6au}, {0x18aee4u,0x18ada2u},
   {0x18aee8u,0x18ade5u}, {0x18aeecu,0x18adf3u}, {0x18aef0u,0x18ae01u}, {0x18aef4u,0x18ae40u},
   {0x18aef8u,0x18ae7bu}, {0x18aefcu,0x18ae86u}, {0x18af00u,0x18ae93u}, {0x18af04u,0x18aea3u},
   {0x18a83au,0x2e3618u}, {0x18a8efu,0x409d7cu}, {0xe3822u,0x2e36ccu}, {0xe3837u,0x2e36c8u},
   {0xe384eu,0x2e36c0u}, {0xe385du,0x2e36b8u}, {0xe3865u,0x2e36b0u}, {0xe3872u,0x2e36a8u},
   {0xe2348u,0x2e35d0u}, {0xe2428u,0x339be4u}, {0xe244cu,0x339be4u}, {0xe2521u,0x412becu},
   {0xe252au,0x412becu}, {0xe2530u,0x40b2c0u}, {0xe253au,0x296db0u}, {0xe254eu,0x40b316u},
   {0xe2559u,0x34e6bcu}, {0xe256bu,0x40b2e8u}, {0xe258du,0x40b319u}, {0xe259du,0x40b319u},
   {0xe25adu,0x40b2d0u}, {0xe25b4u,0x40b2dcu}, {0xe25bfu,0x40b319u}, {0xe25d2u,0x40b2c0u},
   {0xe26c0u,0x2e36ccu}, {0xe26cfu,0x2e36c8u}, {0xe26e4u,0x2e36c0u}, {0xe26f3u,0x2e36b8u},
   {0xe26fbu,0x2e36b0u}, {0xe2708u,0x2e36a8u}, {0xe281fu,0x34e6bcu}, {0xe2825u,0x34e6c0u},
   {0xe283du,0x34e6c4u}, {0xe29f2u,0x408d48u}, {0xe2a61u,0x408d48u}, {0xe2ac3u,0x2c3d80u},
   {0xe2b18u,0x2e35d0u}, {0xe4e6fu,0x339be4u}, {0xc36fcu,0x29a2b0u}, {0xea656u,0x31f951u},
   {0xea68du,0xea748u}, {0xea6c3u,0xea760u}, {0xea6cau,0xea758u}, {0xea6deu,0xea79cu},
   {0xea6e5u,0xea784u}, {0xea727u,0x339360u}, {0xea748u,0xea691u}, {0xea74cu,0xea695u},
   {0xea750u,0xea69cu}, {0xea754u,0xea6a3u}, {0xea758u,0xea6ceu}, {0xea75cu,0xea6d3u},
   {0xea784u,0xea701u}, {0xea788u,0xea70fu}, {0xea78cu,0xea708u}, {0xea790u,0xea6fau},
   {0xea794u,0xea6e9u}, {0xea798u,0xea714u}, {0xe3d39u,0xe42c8u}, {0xe3e41u,0x2e35d0u},
   {0xe3fd0u,0x2c3c40u}, {0xe3fd8u,0x339be0u}, {0xe3feau,0x2e36ccu}, {0xe3ff9u,0x2e36c8u},
   {0xe400eu,0x2e36c0u}, {0xe401du,0x2e36b8u}, {0xe4025u,0x2e36b0u}, {0xe4032u,0x2e36a8u},
   {0xe4193u,0x2e35d0u}, {0xe42c8u,0xe3d3du}, {0xe42ccu,0xe3d4fu}, {0xe42d0u,0xe3d59u},
   {0xe42d4u,0xe3d63u}, {0xe42d8u,0xe3d6du}, {0xe42dcu,0xe3d77u}, {0xe42e0u,0xe3d81u},
   {0xe5036u,0x2e36ccu}, {0xe504bu,0x2e36c8u}, {0xe5060u,0x2e36c0u}, {0xe506fu,0x2e36b8u},
   {0xe5077u,0x2e36b0u}, {0xe5084u,0x2e36a8u}, {0xe5100u,0x2e35d0u}, {0xe458bu,0x339be4u},
   {0xe45abu,0x408d1cu}, {0xe46b4u,0x339be4u}, {0xe46ceu,0x408d1cu}, {0x22388bu,0x2c0ee0u},
   {0x141e8u,0x2e35ccu}, {0x1423bu,0x14294u}, {0x14294u,0x1423fu}, {0x14298u,0x1424du},
   {0x1429cu,0x1425bu}, {0x142a0u,0x14269u}, {0x142a4u,0x14277u}, {0x14869u,0x3363e8u},
   {0x14870u,0x2bfff2u},
};
/* Recommended additions: direct callees on the same paths that were not in the requested list. */
static const struct { const char *name; unsigned rva,size; uint32_t hash; } gesture_extra_codes[]={
    {"GMA_E3A60",0xe3a60u,0x89u,0x8bd0743au},
    {"GMA_SLOT_LOAD",0xe0d70u,0xaeu,0x091d0c60u},
    {"GMA_SLOT_QUEUE_DUMP",0xe0c40u,0x12du,0xa05f016cu},
    {"GMA_SLOT_FX_IDLE",0xe5d10u,0x6du,0xb5654693u},
    {"GMA_SLOT_FX_CREATE",0xe5aa0u,0x115u,0xe56142cau},
    {"GMA_GESTURE_PLAY",0xe4740u,0xd0u,0x5a7e830fu},
    {"ANIM_NAME_INDEX",0x12ab20u,0x55u,0x5983e918u},
    {"RENDERER_SLOT_COUNT",0x21bfc0u,0xau,0xb4595215u},
    {"RENDERER_SLOT_ANIM",0x21bf30u,0x1cu,0xf9fa2846u},
    {"RENDERER_SLOT_RESIDENT",0x21bf50u,0x46u,0xdb6c1b8cu},
    {"RENDERER_SLOT_BANK",0x21bfa0u,0x11u,0xfb5d0834u},
    {"RENDERER_CHANNEL_ANIM",0x2230b0u,0x1eu,0x73fcff08u},
    {"RENDERER_SLOT_REQUEST",0x223bc0u,0x25u,0xe60fb31du},
    {"RENDERER_SLOT_RELEASE",0x223bf0u,0x6eu,0x6336646au},
};
static const struct { unsigned rva,target; } gesture_extra_relocations[]={
   {0xe3accu,0x339be0u}, {0xe0c92u,0x3c3112u}, {0xe0c9du,0x362620u}, {0xe0ca3u,0x3c3112u},
   {0xe0caau,0x29a060u}, {0xe0cafu,0x362620u}, {0xe0cb5u,0x29a068u}, {0xe0cc3u,0x362620u},
   {0xe0ccbu,0x29a064u}, {0xe0cf9u,0x3c3112u}, {0xe0d04u,0x362620u}, {0xe0d0au,0x3c3112u},
   {0xe0d11u,0x29a060u}, {0xe0d16u,0x362620u}, {0xe0d1cu,0x29a068u}, {0xe0d2au,0x362620u},
   {0xe0d30u,0x29a064u}, {0xe5b5bu,0x3c31cdu}, {0xe47bcu,0x34e6bcu}, {0x12ab33u,0x3363e8u},
};
static const struct { unsigned vtable_rva,slot_offset,target_rva; } gesture_vtable_slots[]={
    {0x2c8504u,0x3cu,0x19fae0u},
    {0x2c8504u,0x40u,0xe1930u},
    {0x2c8504u,0x68u,0x19fae0u},
    {0x2df8ecu,0x40u,0x21bac0u},
    {0x2df8ecu,0xa8u,0x223880u},
    {0x2df8ecu,0xf4u,0x223670u},
    {0x2df8ecu,0xf8u,0x21bb10u},
    {0x2df8ecu,0xfcu,0x223000u},
    {0x2df8ecu,0x100u,0x2230b0u},
    {0x2df8ecu,0x104u,0x2230d0u},
    {0x2df8ecu,0x108u,0x223160u},
    {0x2df8ecu,0x10cu,0x223180u},
    {0x2df8ecu,0x110u,0x223220u},
    {0x2df8ecu,0x114u,0x223240u},
    {0x2df8ecu,0x118u,0x223290u},
    {0x2df8ecu,0x144u,0x2234c0u},
    {0x2df8ecu,0x148u,0x2234e0u},
    {0x2df8ecu,0x15cu,0x21be10u},
    {0x2df8ecu,0x17cu,0x21bfc0u},
    {0x2df8ecu,0x180u,0x21bf30u},
    {0x2df8ecu,0x184u,0x21bf50u},
    {0x2df8ecu,0x188u,0x21bfa0u},
    {0x2df8ecu,0x18cu,0x223b70u},
    {0x2df8ecu,0x190u,0x223bc0u},
    {0x2df8ecu,0x194u,0x223bf0u},
    {0x2d65a0u,0x8u,0x14d0f0u},
    {0x2d65a0u,0xcu,0x14d150u},
    {0x2d65a0u,0x10u,0x14d1f0u},
};

enum { RVA_REGISTRY=0x409d8cu, RVA_NPC_VTABLE=0x2d65a0u, RVA_GMA_VTABLE=0x2c8504u, RVA_POSITION_VTABLE=0x2cdefcu,
    RVA_RENDER_VTABLE=0x2df8ecu, RVA_GMA_UPDATE=0xe1930u, RVA_EVENT_POP=0x223880u, RVA_BANK_LOAD=0xe0ea0u,
    RVA_BANK_LOADED=0xe0f30u, RVA_BANK_IN_USE=0xe1000u, RVA_BANK_DUMP=0xe0e20u, RVA_PROCESS_DUMPS=0xe01c0u,
    RVA_FRAME_STAMP=0x3c3150u, GESTURE_BANK=5, EFFECT_RETIRE_CALL=0xe2162u, EFFECT_RETIRE_SIZE=5u,
    EVENT_DRAIN_LIMIT=4096u, LOAD_LIMIT_MS=3000u, STALL_MS=1500u, END_LIMIT_MS=3000u, DUMP_LIMIT_MS=2000u,
    TAIL_MS=400u, NO_EFFECT=0x7ffffu };
typedef uint8_t (__attribute__((thiscall)) *EventPop)(void *renderer,void *out);
typedef void (__attribute__((thiscall)) *GmaUpdate)(void *gma,void *ctx);
typedef void (__attribute__((stdcall)) *BankLoad)(void *gma,int bank,int a3,int a4);
typedef uint8_t (__attribute__((stdcall)) *BankQuery)(void *gma,int bank);
typedef void (__attribute__((stdcall)) *ProcessDumps)(void *gma);

static BOOL enabled,installed;
static uint8_t *base;
static DWORD native_thread;
enum { G_IDLE, G_LOADING, G_READY, G_ENDING, G_DUMPING };
static struct {
    unsigned phase;
    const uint8_t *npc;
    uint8_t *gma;
    BOOL degraded,dump_queued;
    DWORD phase_at,stall_since,tail_until,clock_missing_since;
    const void *position,*renderer;   /* identity witnesses taken at Begin */
    unsigned lock_seen;               /* last lock state read while proven */
    uint16_t dump_frame; BOOL dump_frame_valid;
    uint16_t frame;
    BOOL frame_valid;
    unsigned ticks,discarded;
} g;
static unsigned logs;
/* A conversation line was animated when the driver had to stop (the NPC
 * stopped being local, or the frame clock went away): Stalled() reports it
 * once so the caller stops that line too. */
static BOOL lost_line;

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && a<=UINTPTR_MAX-n && VirtualQuery(p,&m,sizeof(m))==sizeof(m) &&
        m.State==MEM_COMMIT && !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
        a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static uint32_t word(const uint8_t *p) { uint32_t v; memcpy(&v,p,4); return v; }
static void trace(const char *format,...) __attribute__((format(printf,1,2)));
#include <stdarg.h>
#include <stdio.h>
static void trace(const char *format,...) {
    if(logs>=200u) return;
    ++logs; char line[320]; va_list a; va_start(a,format); vsnprintf(line,sizeof(line),format,a); va_end(a);
    SudekiMpLogWrite(line);
}
/* FNV-1a over the image with relocated operands compared and hashed as at
 * the preferred base (the talk/zone-banner scheme). The effect_retire seam
 * (its owned relative call at 0xE2162, inside the commit routine) is not
 * hashed; it must still be a call. */
static BOOL row_exact(const uint8_t *b,const char *name,unsigned rva,unsigned size,uint32_t hash,
    const void *relocations,unsigned relocation_count) {
    const struct { unsigned rva,target; } *r=relocations;
    uint32_t h=2166136261u;
    if(!readable(b+rva,size)) return FALSE;
    for(unsigned i=0;i<size;++i) {
        unsigned at=rva+i; uint8_t byte=b[at];
        if(at>=EFFECT_RETIRE_CALL && at<EFFECT_RETIRE_CALL+EFFECT_RETIRE_SIZE) continue;
        for(unsigned j=0;j<relocation_count;++j) {
            unsigned offset=at-r[j].rva;
            if(offset>=4u) continue;
            if(word(b+r[j].rva)!=(uint32_t)(uintptr_t)(b+r[j].target)) return FALSE;
            byte=(uint8_t)((0x400000u+r[j].target)>>(8u*offset)); break;
        }
        h=(h^byte)*16777619u;
    }
    if(h!=hash) { trace("story_npc_gesture event=guard_failed name=%s\r\n",name); return FALSE; }
    return TRUE;
}
static BOOL image_exact(const uint8_t *b) {
    for(unsigned k=0;k<sizeof(gesture_codes)/sizeof(gesture_codes[0]);++k)
        if(!row_exact(b,gesture_codes[k].name,gesture_codes[k].rva,gesture_codes[k].size,gesture_codes[k].hash,
            gesture_relocations,sizeof(gesture_relocations)/sizeof(gesture_relocations[0]))) return FALSE;
    for(unsigned k=0;k<sizeof(gesture_extra_codes)/sizeof(gesture_extra_codes[0]);++k)
        if(!row_exact(b,gesture_extra_codes[k].name,gesture_extra_codes[k].rva,gesture_extra_codes[k].size,
            gesture_extra_codes[k].hash,gesture_extra_relocations,
            sizeof(gesture_extra_relocations)/sizeof(gesture_extra_relocations[0]))) return FALSE;
    for(unsigned k=0;k<sizeof(gesture_vtable_slots)/sizeof(gesture_vtable_slots[0]);++k) {
        const uint8_t *slot=b+gesture_vtable_slots[k].vtable_rva+gesture_vtable_slots[k].slot_offset;
        if(!readable(slot,4u) || word(slot)!=(uint32_t)(uintptr_t)(b+gesture_vtable_slots[k].target_rva)) {
            trace("story_npc_gesture event=guard_failed vtable=%06x slot=%x\r\n",gesture_vtable_slots[k].vtable_rva,
                gesture_vtable_slots[k].slot_offset);
            return FALSE;
        }
    }
    return readable(b+EFFECT_RETIRE_CALL,EFFECT_RETIRE_SIZE) && b[EFFECT_RETIRE_CALL]==0xe8u &&
        readable(b+RVA_FRAME_STAMP,2u);
}
/* The shopkeeper, proven again on every use: in the registry, local to this
 * client (never posed by a host frame), the exact GMA/renderer chain, and
 * none of the update's effect/attachment/rider branches reachable. */
static uint8_t *renderer_of(const uint8_t *e) {
    const uint8_t *pos=*(const uint8_t *const *)(e+0x44u),*w;
    if(!readable(pos,0xb8u) || *(void *const *)pos!=base+RVA_POSITION_VTABLE || *(const void *const *)(pos+0x10u)!=e ||
        *(const void *const *)(pos+0x94u) || !readable(w=*(const uint8_t *const *)(pos+0xb4u),0x18u) ||
        !*(const void *const *)(w+8u) || !*(const void *const *)(w+0x14u) ||
        *(void *const *)(w+0xcu)!=*(void *const *)(w+0x10u)) return NULL;
    uint8_t *r=*(uint8_t *const *)(w+0xcu);
    return readable(r,0xb0u) && *(void **)r==base+RVA_RENDER_VTABLE?r:NULL;
}
static uint8_t *gma_of(const uint8_t *e) {
    if(!e || !readable(e,0x3a8u+0x140u) || *(void *const *)e!=base+RVA_NPC_VTABLE) return NULL;
    uint8_t *gma=*(uint8_t *const *)(e+0x130u);
    if(gma!=e+0x3a4u || *(uint8_t *const *)(e+0x58u)!=gma || !readable(gma,0x140u) ||
        *(void **)gma!=base+RVA_GMA_VTABLE || *(const void *const *)(gma+0x10u)!=e ||
        !(gma[0x133u]&4u) || *(const void *const *)(gma+0x68u) || *(const void *const *)(gma+0x100u) ||
        !readable(*(uint8_t *const *)(gma+0xdcu),0x18u) || !readable(*(uint8_t *const *)(gma+0xf8u),0x10u) ||
        *(const void *const *)(e+0x84u) || !*(const void *const *)(e+0x90u)) return NULL;
    return gma;
}
static BOOL in_registry(const void *e) {
    const uint8_t *owner=readable(base+RVA_REGISTRY,4u)?*(const uint8_t *const *)(base+RVA_REGISTRY):NULL;
    if(!readable(owner,0x40u)) return FALSE;
    unsigned count=*(const uint32_t *)(owner+0x34u);
    void *const *entries=*(void *const *const *)(owner+0x3cu);
    if(!count || count>8192u || !readable(entries,count*sizeof(void *))) return FALSE;
    for(unsigned i=0;i<count;++i) if(entries[i]==e) return TRUE;
    return FALSE;
}
/* Every animation a gesture tick can commit has no attached effect. */
static BOOL entries_plain(const uint8_t *gma) {
    static const unsigned anims[]={2u,0xabu,0xacu,0xadu,0xaeu,0xafu,0xb0u};
    const uint8_t *table=*(const uint8_t *const *)(gma+0xdcu);
    for(unsigned i=0;i<sizeof(anims)/sizeof(anims[0]);++i) {
        const uint8_t *entry=readable(table+0x14u+4u*anims[i],4u)?*(const uint8_t *const *)(table+0x14u+4u*anims[i]):NULL;
        if(!readable(entry,0x30u) || word(entry+0x2cu)!=NO_EFFECT) return FALSE;
    }
    unsigned locked=*(const uint8_t *)(gma+0x130u);
    const uint8_t *entry=readable(table+0x14u+4u*locked,4u)?*(const uint8_t *const *)(table+0x14u+4u*locked):NULL;
    return !entry || (readable(entry,0x30u) && word(entry+0x2cu)==NO_EFFECT);
}
static uint8_t *npc_ok(void) {
    if(!g.npc || !in_registry(g.npc) || !SudekiMpLanStoryWorldLocalIdleNpc(g.npc)) return NULL;
    uint8_t *gma=gma_of(g.npc);
    /* Same sub-objects as at Begin: a reallocated NPC at the same address
     * is not the conversation's. */
    return gma && gma==g.gma && renderer_of(g.npc)==g.renderer &&
        *(const void *const *)(g.npc+0x44u)==g.position?gma:NULL;
}
static BOOL clock(void) { float dt; return SudekiMpLanStoryAmbientFrameDelta(&dt) && dt>=0.0f && dt<=0.05f; }
static unsigned lock_state(const uint8_t *gma) { return gma[0x131u]; }
static BOOL bank_loaded(uint8_t *gma) { return ((BankQuery)(base+RVA_BANK_LOADED))(gma,GESTURE_BANK)!=0; }

void SudekiMpLanStoryNpcGestureEnable(BOOL on) { if(!installed) enabled=on; }
BOOL SudekiMpLanStoryNpcGestureInstall(HMODULE image) {
    uint8_t *b=(uint8_t *)image;
    if(installed) return TRUE;
    if(!enabled) return FALSE;
    if(!b || !SudekiMpCheckLoadedExecutable(image) || !image_exact(b)) {
        trace("story_npc_gesture event=unavailable policy=animate_off\r\n"); return FALSE;
    }
    base=b; native_thread=0; memset(&g,0,sizeof(g)); installed=TRUE;
    trace("story_npc_gesture event=installed rows=%u extra=%u slots=%u\r\n",
        (unsigned)(sizeof(gesture_codes)/sizeof(gesture_codes[0])),
        (unsigned)(sizeof(gesture_extra_codes)/sizeof(gesture_extra_codes[0])),
        (unsigned)(sizeof(gesture_vtable_slots)/sizeof(gesture_vtable_slots[0])));
    return TRUE;
}
BOOL SudekiMpLanStoryNpcGestureIdle(void) { return !installed || g.phase==G_IDLE; }
BOOL SudekiMpLanStoryNpcGestureUninstall(void) {
    if(!installed) return TRUE;
    if(g.phase!=G_IDLE) { SetLastError(ERROR_BUSY); return FALSE; }
    installed=FALSE; base=NULL; native_thread=0;
    return TRUE;
}
static void enter(unsigned phase,uint32_t now,const char *why) {
    static const char *const names[]={"idle","loading","ready","ending","dumping"};
    trace("story_npc_gesture event=%s why=%s lock=%u ticks=%u discarded=%u\r\n",names[phase],why,
        g.lock_seen,g.ticks,g.discarded);
    g.phase=phase; g.phase_at=now?now:1u;
    if(phase==G_IDLE) { g.npc=NULL; g.gma=NULL; }
}
void SudekiMpLanStoryNpcGestureDrain(const char *why) {
    if(!installed || g.phase==G_IDLE) return;
    if(g.lock_seen>=1u && g.lock_seen<=4u) lost_line=TRUE;
    enter(G_IDLE,GetTickCount(),why?why:"drain");
}
BOOL SudekiMpLanStoryNpcGestureBegin(const void *npc,uint32_t now) {
    if(!installed || g.phase!=G_IDLE || (native_thread && native_thread!=GetCurrentThreadId())) return FALSE;
    native_thread=GetCurrentThreadId();
    uint8_t *gma=gma_of(npc);
    if(!gma || !in_registry(npc) || !SudekiMpLanStoryWorldLocalIdleNpc(npc) || !renderer_of(npc) ||
        lock_state(gma) || !entries_plain(gma) || !clock()) {
        trace("story_npc_gesture event=refused reason=npc_state lock=%d clock=%u\r\n",gma?(int)gma[0x131u]:-1,(unsigned)clock());
        return FALSE;
    }
    memset(&g,0,sizeof(g)); g.npc=npc; g.gma=gma; lost_line=FALSE;
    g.renderer=renderer_of(npc); g.position=*(const void *const *)((const uint8_t *)npc+0x44u);
    if(bank_loaded(gma) && !*(const void *const *)(gma+0x58u)) { enter(G_READY,now,"bank_resident"); return TRUE; }
    ((BankLoad)(base+RVA_BANK_LOAD))(gma,GESTURE_BANK,0,0);
    enter(G_LOADING,now,"bank_requested");
    return TRUE;
}
BOOL SudekiMpLanStoryNpcGestureReady(void) {
    if(!installed || g.phase!=G_READY || g.degraded) return FALSE;
    uint8_t *gma=npc_ok();
    if(!gma) return FALSE;
    unsigned lock=lock_state(gma); const uint8_t *record=*(const uint8_t *const *)(gma+0xf8u);
    /* The speaker lock CSpeech takes (0xE4460, anim 2) completes at once
     * when channel 0 plays the idle anim 2 with no pending flags (byte
     * +0xA; 0x4E4494); a lock already held is driven by our ticks. */
    return clock() && !*(const void *const *)(gma+0x58u) && bank_loaded(gma) &&
        (lock || (!record[0xau] && record[2]==2u));
}
/* Pop and discard the renderer's pending animation events: the update's own
 * drain would dispatch them (sounds, script messages, camera shake). */
static BOOL discard_events(uint8_t *renderer) {
    uint8_t out[0x14];
    for(unsigned n=0;n<EVENT_DRAIN_LIMIT;++n) {
        memset(out,0,sizeof(out));
        if(!((EventPop)(base+RVA_EVENT_POP))(renderer,out)) return TRUE;
        ++g.discarded;
    }
    trace("story_npc_gesture event=event_backlog_unbounded policy=stop\r\n");
    return FALSE;
}
static void update_once(uint8_t *gma,uint32_t now) {
    uint16_t stamp=*(const uint16_t *)(base+RVA_FRAME_STAMP);
    if(g.frame_valid && stamp==g.frame) return;           /* once per native frame */
    float dt=0.0f;
    if(!SudekiMpLanStoryAmbientFrameDelta(&dt) || !(dt>=0.0f) || dt>0.05f) {
        /* No clock: the lock and the clip cannot advance. Stop animating. */
        if(!g.clock_missing_since) g.clock_missing_since=now?now:1u;
        else if(now-g.clock_missing_since>=250u && !g.degraded) {
            g.degraded=TRUE; lost_line=TRUE;
            trace("story_npc_gesture event=clock_missing lock=%u policy=line_stopped_animate_off\r\n",g.lock_seen);
        }
        return;
    }
    g.clock_missing_since=0;
    if(!(dt>0.0f)) return;
    uint8_t *renderer=renderer_of(g.npc);
    if(!renderer || !discard_events(renderer)) { SudekiMpLanStoryNpcGestureDrain("events"); return; }
    g.frame=stamp; g.frame_valid=TRUE;
    float ctx[4]={0.0f,0.0f,0.0f,dt};
    ((GmaUpdate)(*(void **)(*(uint8_t **)gma+0x40u)))(gma,ctx);
    ++g.ticks;
}
void SudekiMpLanStoryNpcGestureTick(uint32_t now) {
    if(!installed || g.phase==G_IDLE || native_thread!=GetCurrentThreadId()) return;
    uint8_t *gma=npc_ok();
    if(!gma) { SudekiMpLanStoryNpcGestureDrain("npc_not_local"); return; }
    unsigned lock=lock_state(gma); g.lock_seen=lock;
    if(g.phase==G_LOADING) {
        if(bank_loaded(gma) && !*(const void *const *)(gma+0x58u)) enter(G_READY,now,"bank_loaded");
        else if(now-g.phase_at>=LOAD_LIMIT_MS) { g.degraded=TRUE; enter(G_READY,now,"bank_timeout"); }
    }
    if(lock>=1u && lock<=4u) { update_once(gma,now); g.tail_until=now+TAIL_MS; if(!g.tail_until) g.tail_until=1u; }
    else if(g.tail_until && (int32_t)(now-g.tail_until)<0) update_once(gma,now);  /* the blend back to idle */
    else g.tail_until=0;
    g.lock_seen=lock_state(gma);
}
/* EndInteractionScript's dump, released by the native dump pass once per
 * native frame. */
static void process_dumps(uint8_t *gma) {
    uint16_t stamp=*(const uint16_t *)(base+RVA_FRAME_STAMP);
    if(g.dump_frame_valid && stamp==g.dump_frame) return;
    g.dump_frame=stamp; g.dump_frame_valid=TRUE;
    ((ProcessDumps)(base+RVA_PROCESS_DUMPS))(gma);
}
BOOL SudekiMpLanStoryNpcGestureStalled(const void *speech,uint32_t now) {
    const uint8_t *cs=speech;
    if(installed && lost_line) { lost_line=FALSE; return cs && cs[0x474u] && *(const uint32_t *)(cs+0xc8u)!=2u; }
    if(!installed || g.phase==G_IDLE || !cs || !cs[0x474u] || *(const uint32_t *)(cs+0xc8u)==2u) { g.stall_since=0; return FALSE; }
    uint8_t *gma=npc_ok();
    if(!gma) { g.stall_since=0; return FALSE; }          /* Tick drains it this frame */
    /* Only a line still waiting for its lock (1) stalls; 2/3 run it and 4/0
     * are its release. */
    unsigned lock=lock_state(gma);
    if(lock!=1u) { g.stall_since=0; return FALSE; }
    if(!g.stall_since) { g.stall_since=now?now:1u; return FALSE; }
    if(now-g.stall_since<STALL_MS) return FALSE;
    g.degraded=TRUE; g.stall_since=0;
    trace("story_npc_gesture event=lock_stall lock=%u policy=line_stopped_animate_off\r\n",lock);
    return TRUE;
}
BOOL SudekiMpLanStoryNpcGestureEnd(uint32_t now) {
    if(!installed || g.phase==G_IDLE) return TRUE;
    if(g.phase==G_LOADING || g.phase==G_READY) enter(G_ENDING,now,"conversation_over");
    uint8_t *gma=npc_ok();
    if(!gma) { SudekiMpLanStoryNpcGestureDrain("npc_not_local"); return TRUE; }
    if(g.phase==G_ENDING) {
        BOOL settled=!lock_state(gma) && !(g.tail_until && (int32_t)(now-g.tail_until)<0);
        if(settled && !((BankQuery)(base+RVA_BANK_IN_USE))(gma,GESTURE_BANK)) {
            /* EndInteractionScript's DumpAnimBank(NPC, 5): queue, then let
             * the native dump pass release it. */
            if(bank_loaded(gma) || *(const void *const *)(gma+0x58u)) {
                void *eax=gma,*dump=base+RVA_BANK_DUMP;
                __asm__ volatile("pushl %[bank]\n\tcall *%[dump]\n\t"
                    : "+a"(eax) : [bank]"i"(GESTURE_BANK),[dump]"S"(dump) : "ecx","edx","memory","cc");
            }
            enter(G_DUMPING,now,"dump_queued");
        } else if(now-g.phase_at>=END_LIMIT_MS) { SudekiMpLanStoryNpcGestureDrain("end_timeout"); return TRUE; }
        return FALSE;
    }
    /* G_DUMPING */
    process_dumps(gma);
    if(!*(const uint32_t *)(gma+0x5cu)) { enter(G_IDLE,now,"dumped"); return TRUE; }
    if(now-g.phase_at>=DUMP_LIMIT_MS) { SudekiMpLanStoryNpcGestureDrain("dump_timeout"); return TRUE; }
    return FALSE;
}
