#ifndef SUDEKIMP_STORY_IDLE_CONTACT_H
#define SUDEKIMP_STORY_IDLE_CONTACT_H
#include <windows.h>
#include <stdint.h>

/* Only the saved-story host may grant a current, ordinary-world roster lease.
 * The adapter additionally proves a native, current-query terrain contact and
 * neutral first-person ranged movement. No terrain pointer survives a call. */
typedef BOOL (*SudekiMpStoryIdleContactLease)(void *actor);
BOOL SudekiMpStoryIdleContactImageExact(uint8_t *image);
BOOL SudekiMpStoryIdleContactInstall(HMODULE image,SudekiMpStoryIdleContactLease);
BOOL SudekiMpStoryIdleContactBindThread(void);
BOOL SudekiMpStoryIdleContactUninstall(void);
uint32_t SudekiMpStoryIdleContactCount(void);

/* Relocation-normalized contracts, individually exercised against the retail
 * image. The margin instructions lie inside IDLE_CONTACT_NORMAL and
 * IDLE_CONTACT_SEPARATION. */
static const struct { const char *name; unsigned rva,size; uint32_t hash;
    unsigned relocations,reloc[4]; } story_idle_contact_contracts[]={
    {"IDLE_CONTACT_CALL",0x32654,0xb,0x1b3ab128,0,{0}},
    {"IDLE_CONTACT_FRAME",0x33010,0x90,0xc9f1852f,0,{0}},
    {"IDLE_CONTACT_NORMAL",0x33267,0x80,0x5a9754b6,4,{2,8,0x1d,0x23}},
    {"IDLE_CONTACT_SEPARATION",0x33486,0xe1,0x8b56f3bd,4,{0x14,0x1e,0x24,0x83}},
    {"IDLE_CONTACT_END",0x33633,0x15,0x934d62da,0,{0}},
    {"IDLE_CONTACT_THRESHOLD",0x2c1734,4,0x2d670220,0,{0}},
    {"IDLE_CONTACT_MARGIN",0x2e37c0,8,0x297cfce7,0,{0}},
    {"IDLE_CONTACT_HALF",0x2e3620,8,0x0c927168,0,{0}}
};
#endif
