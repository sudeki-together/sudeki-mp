#include "hooks/talos_seat_probe.h"
#include "hooks/story_flight.h"
#include "cleanroom/engine.h"
#include "engine/arbiter_combat_input.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

enum { RVA_ARBITER_MOVEMENT=0xdae80u, RVA_ARBITER_COMBAT_INPUT=0xdb0e0u,
       RVA_ALLY_VTABLE=0x2d55d4u, RVA_ARBITER_VTABLE=0x2cc9acu, LOG_LIMIT=600 };
/* Exact entries (control_separation.c party_native_entries_exact / combat input). */
static const uint8_t movement_entry[]={0x53,0x8b,0x5c,0x24,0x08,0x55,0x8b,0x6c,0x24,0x10,0x56,0x57,0x8b,0xd3};
static const uint8_t combat_entry[]={0x55,0x8b,0x6c,0x24,0x08,0x56,0x57,0x8b,0xf8,0x8b,0xf1};
typedef void (__stdcall *MovementFunction)(void *arbiter,const float *direction,float speed,float turn_rate,uint32_t mode);
static uint8_t *base; static BOOL installed; static unsigned logs;
static BOOL key_down[3]; static BOOL moving; static float heading[3];

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && VirtualQuery(p,&m,sizeof(m))==sizeof(m) && m.State==MEM_COMMIT &&
        !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) && a+n>=a && a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static BOOL down(int vk) { return (GetAsyncKeyState(vk)&0x8000)!=0; }
static uint8_t *talos_arbiter(uint8_t **entity_out) {
    uint8_t *e=SudekiMpCleanroomEngineGenericEntity("ALLY_TALOS");
    if(!readable(e,0x138u) || *(void **)e!=base+RVA_ALLY_VTABLE) return NULL;
    uint8_t *a=*(uint8_t **)(e+0x90u);
    if(!readable(a,0x64u) || *(void **)a!=base+RVA_ARBITER_VTABLE || *(void **)(a+0x10u)!=e) return NULL;
    if(entity_out) *entity_out=e;
    return a;
}
static void observe(void *owner,uint8_t *movement,const float position[3],float dt) {
    (void)owner;(void)movement;(void)position;(void)dt;
    if(!installed || !base) return;
    uint8_t *entity=NULL,*arbiter=talos_arbiter(&entity);
    /* Movement: numpad 8/2 = +z/-z, 6/4 = +x/-x (world axes). */
    float x=(down(VK_NUMPAD6)?1.0f:0.0f)-(down(VK_NUMPAD4)?1.0f:0.0f);
    float z=(down(VK_NUMPAD8)?1.0f:0.0f)-(down(VK_NUMPAD2)?1.0f:0.0f);
    float magnitude=sqrtf(x*x+z*z);
    if(arbiter) {
        MovementFunction move=(MovementFunction)(base+RVA_ARBITER_MOVEMENT);
        if(magnitude>0.5f) {
            heading[0]=x/magnitude; heading[1]=0; heading[2]=z/magnitude;
            if(!moving && logs<LOG_LIMIT) { ++logs;
                SudekiMpLogFormat("talos_seat_probe event=move_start ms=%lu entity=%p arbiter=%p heading=%.2f,%.2f flags50=%08lx state58=%08lx\r\n",
                    (unsigned long)GetTickCount(),(void *)entity,(void *)arbiter,(double)heading[0],(double)heading[2],
                    (unsigned long)*(uint32_t *)(arbiter+0x50u),(unsigned long)*(uint32_t *)(arbiter+0x58u)); }
            moving=TRUE;
            move(arbiter,heading,1.0f,1.0f,0u);
        } else if(moving) {
            moving=FALSE;
            move(arbiter,heading,0.0f,1.0f,0u);
            if(logs<LOG_LIMIT) { ++logs;
                SudekiMpLogFormat("talos_seat_probe event=move_stop ms=%lu flags50=%08lx state58=%08lx pos=%.2f,%.2f,%.2f\r\n",
                    (unsigned long)GetTickCount(),(unsigned long)*(uint32_t *)(arbiter+0x50u),(unsigned long)*(uint32_t *)(arbiter+0x58u),
                    (double)*(float *)(*(uint8_t **)(entity+0x44u)+0x18u),(double)*(float *)(*(uint8_t **)(entity+0x44u)+0x1cu),
                    (double)*(float *)(*(uint8_t **)(entity+0x44u)+0x20u)); }
        }
    } else moving=FALSE;
    /* Melee: edge-triggered F8 weak, F9 strong, F11 sweep. */
    static const int vk[3]={VK_F8,VK_F9,VK_F11};
    for(unsigned i=0;i<3u;++i) {
        BOOL d=down(vk[i]);
        if(d && !key_down[i]) {
            if(arbiter) {
                uint32_t f50=*(uint32_t *)(arbiter+0x50u),s58=*(uint32_t *)(arbiter+0x58u),f60=*(uint32_t *)(arbiter+0x60u);
                SudekiMpSubmitArbiterCombatInput(base+RVA_ARBITER_COMBAT_INPUT,arbiter,i==0,i==1,i==2,0,0,0);
                if(logs<LOG_LIMIT) { ++logs;
                    SudekiMpLogFormat("talos_seat_probe event=melee_submitted ms=%lu kind=%u entity=%p arbiter=%p flags50=%08lx->%08lx state58=%08lx->%08lx flags60=%08lx\r\n",
                        (unsigned long)GetTickCount(),i+1u,(void *)entity,(void *)arbiter,(unsigned long)f50,(unsigned long)*(uint32_t *)(arbiter+0x50u),
                        (unsigned long)s58,(unsigned long)*(uint32_t *)(arbiter+0x58u),(unsigned long)f60); }
            } else if(logs<LOG_LIMIT) { ++logs;
                SudekiMpLogFormat("talos_seat_probe event=melee_skipped kind=%u reason=no_ally_talos_entity\r\n",i+1u); }
        }
        key_down[i]=d;
    }
}
BOOL SudekiMpTalosSeatProbeInstall(HMODULE game_module) {
    uint8_t *b=(uint8_t *)game_module;
    if(installed || !b || !SudekiMpCheckLoadedExecutable(game_module)) { SetLastError(ERROR_INVALID_STATE); return FALSE; }
    /* The arbiter trace probe may already own the first five movement bytes
     * (inline JMP); the rest of the prologue must still be the retail image. */
    if(memcmp(b+RVA_ARBITER_MOVEMENT+5,movement_entry+5,sizeof(movement_entry)-5) ||
        (b[RVA_ARBITER_MOVEMENT]!=movement_entry[0] && b[RVA_ARBITER_MOVEMENT]!=0xe9) ||
        memcmp(b+RVA_ARBITER_COMBAT_INPUT,combat_entry,sizeof(combat_entry))) { SetLastError(ERROR_INVALID_DATA); return FALSE; }
    base=b;
    if(!SudekiMpStoryFlightAddControlledObserver(observe)) { base=NULL; SetLastError(ERROR_NOT_ENOUGH_MEMORY); return FALSE; }
    installed=TRUE;
    SudekiMpLogWrite("talos_seat_probe event=install status=success keys=numpad8/2/4/6_move,F8_weak,F9_strong,F11_sweep target=ALLY_TALOS policy=hero_native_entries_on_ally_arbiter\r\n");
    return TRUE;
}
BOOL SudekiMpTalosSeatProbeUninstall(void) {
    if(!installed) return TRUE;
    SudekiMpStoryFlightRemoveControlledObserver(observe);
    installed=FALSE; base=NULL;
    SudekiMpLogWrite("talos_seat_probe event=uninstall status=success\r\n");
    return TRUE;
}
