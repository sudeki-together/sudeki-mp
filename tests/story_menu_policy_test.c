#include "network/lan_story_menu_policy.h"
#include "network/lan_story_handoff.h"
#include <assert.h>
int main(void) {
    /* Two-member party (Ailish slot 0, Tal slot 1): the host owns slot 0 only. */
    assert(SudekiMpStoryMenuNextSlot(2,0,TRUE,1u)==0);
    assert(SudekiMpStoryMenuNextSlot(2,0,FALSE,1u)==0);
    assert(SudekiMpStoryMenuNextSlot(2,1,TRUE,1u)==0);  /* a stray selection returns home */
    /* Client owning slot 1 only. */
    assert(SudekiMpStoryMenuNextSlot(2,1,TRUE,2u)==1);
    assert(SudekiMpStoryMenuNextSlot(2,0,TRUE,2u)==1);
    /* Four members, host owns slots 0,2,3 (slot 1 is a remote player). */
    assert(SudekiMpStoryMenuNextSlot(4,0,TRUE,0xdu)==2);
    assert(SudekiMpStoryMenuNextSlot(4,2,TRUE,0xdu)==3);
    assert(SudekiMpStoryMenuNextSlot(4,3,TRUE,0xdu)==0);
    assert(SudekiMpStoryMenuNextSlot(4,0,FALSE,0xdu)==3);
    assert(SudekiMpStoryMenuNextSlot(4,2,FALSE,0xdu)==0);
    /* Nothing allowed or malformed input leaves the selection alone. */
    assert(SudekiMpStoryMenuNextSlot(4,1,TRUE,0u)==1);
    assert(SudekiMpStoryMenuNextSlot(0,0,TRUE,1u)==0);
    assert(SudekiMpStoryMenuNextSlot(5,0,TRUE,1u)==0);
    assert(SudekiMpStoryMenuNextSlot(2,2,TRUE,3u)==2);
    assert(SudekiMpStoryMenuInitialSlot(2,2u,0)==1);
    assert(SudekiMpStoryMenuInitialSlot(4,0xcu,0)==2);
    assert(SudekiMpStoryMenuInitialSlot(4,0u,0)==0);
    assert(SudekiMpStoryMenuInitialSlot(0,1u,3)==3);
    /* Weapon families for the equip request. */
    assert(SudekiMpStoryWeaponItemForCharacter(2,0) && SudekiMpStoryWeaponItemForCharacter(2,3) && SudekiMpStoryWeaponItemForCharacter(2,11));
    assert(!SudekiMpStoryWeaponItemForCharacter(2,12) && !SudekiMpStoryWeaponItemForCharacter(3,11));
    assert(SudekiMpStoryWeaponItemForCharacter(3,12) && SudekiMpStoryWeaponItemForCharacter(0,24) && SudekiMpStoryWeaponItemForCharacter(1,47));
    assert(!SudekiMpStoryWeaponItemForCharacter(1,48) && !SudekiMpStoryWeaponItemForCharacter(4,0));
    return 0;
}
