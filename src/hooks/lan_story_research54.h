#ifndef SUDEKIMP_LAN_STORY_RESEARCH54_H
#define SUDEKIMP_LAN_STORY_RESEARCH54_H
#include <windows.h>
#include <stdint.h>

/* #54 research instrumentation (Phase 1). Probe-only and read-only: it never
 * calls native code, writes native memory or changes a native result.
 * Installed only by the saved-story profile when [StoryAreas]
 * ResearchDiagnostics is on and [StoryAreas] Research54=true.
 *
 * Observe-only hooks (all copy registers/arguments, then run the original):
 *   call  0x428E3F/0x428E5E/0x4F4D7F  -> arbiter movement 0x4DAE80 (its entry
 *                   is checked by control_separation before each story lease)
 *   entry 0x4286C0  controller combat (stdcall, controller)
 *   mid   0x5114DF  CPosition set_forward after its validated 12-byte prefix
 *   mid   0x4B6E5E  0x4B6E50 after its validated 8-byte prefix
 *   call  0x42891F  controller -> hero combat input 0x4DB0E0
 *   call  0x4DA816  0x4DA780   -> hero combat input 0x4DB0E0
 *   call  0x4B75AC/0x4B749F/0x4B74DD  turn step: player / AI moving / AI standing
 *   call  0x4F4DE3  AI movement node SetSpeed (0x4DB070)
 *   call  0x4288B1/0x4288C1/0x428510  controller lock on / off / auto (0x4DC610/0x4DC720)
 * 0x4DB0E0 itself is hashed by story_interaction_guard.h, so it is observed at
 * its two native call sites. The LAN arena modules validate 0x4DAE80/0x4286C0
 * entries; this profile never installs them. Events are filtered to the party
 * actors cached by the per-frame sampler. Every line starts with "r54 ". */
BOOL SudekiMpLanStoryResearch54Install(HMODULE image);
BOOL SudekiMpLanStoryResearch54Uninstall(void);
BOOL SudekiMpLanStoryResearch54Enabled(void);
/* Game thread, once per controller update: party/lead/target/cluster/clock. */
void SudekiMpLanStoryResearch54Sample(BOOL client);
/* Host, once per remote player per controller update. */
void SudekiMpLanStoryResearch54Input(unsigned player,BOOL took,uint32_t receipt,DWORD now,
    BOOL fresh,BOOL packet_moving,BOOL applied_moving,BOOL held);
/* The mod's own arbiter movement call (story remote move), same fields. */
void SudekiMpLanStoryResearch54Move(const void *arbiter,const float direction[3],float speed,float rate,uint32_t mode);
/* C3 admitted AI command bypass; gate 0 movement, 1 navigation-adjusted snap. */
void SudekiMpLanStoryResearch54AiGate(const void *actor,unsigned gate);
/* C1 target refresh observation; does not mutate native state. */
void SudekiMpLanStoryResearch54Target(const void *actor,float x,float z,const void *before,BOOL refreshed);
/* Remote melee submit (mod path) with the button values it passed. */
void SudekiMpLanStoryResearch54Melee(const void *actor,unsigned kind);
/* Monster presentation gaps: side 0 host capture, 1 client apply. */
void SudekiMpLanStoryResearch54Monster(unsigned side,uint32_t identity,const float position[3]);
/* Short name of a pointer: party name, "none", or the raw pointer. */
void SudekiMpLanStoryResearch54Name(const void *pointer,char out[16]);
#endif
