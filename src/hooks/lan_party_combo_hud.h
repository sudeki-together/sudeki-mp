#ifndef SUDEKIMP_LAN_PARTY_COMBO_HUD_H
#define SUDEKIMP_LAN_PARTY_COMBO_HUD_H
#include "hooks/lan_party_control.h"

/* Display accepted host action history on the fixed local Tal HUD. No native
 * combo admission, gameplay task, actor switch, or retained callback. */
BOOL SudekiMpLanPartyComboHudApply(HMODULE image,SudekiMpLanPartySession *session,
    const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *lease,const SudekiMpLanPartyFrame *frame);
#endif
