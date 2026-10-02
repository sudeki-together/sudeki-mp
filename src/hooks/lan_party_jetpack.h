#ifndef SUDEKIMP_LAN_PARTY_JETPACK_H
#define SUDEKIMP_LAN_PARTY_JETPACK_H
#include "hooks/lan_party_control.h"
#include "network/lan_party_session.h"
BOOL SudekiMpLanPartyJetpackInstall(HMODULE image,SudekiMpLanPartySession *session);
void SudekiMpLanPartyJetpackService(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyJetpackState *confirmed);
void SudekiMpLanPartyJetpackServiceStop(const SudekiMpControlUpdateDispatchWitness *w);
void SudekiMpLanPartyJetpackCapture(SudekiMpLanPartyJetpackState *state);
BOOL SudekiMpLanPartyJetpackIdle(void);
BOOL SudekiMpLanPartyJetpackLedgeReady(BOOL *enabled);
BOOL SudekiMpLanPartyJetpackToLedge(const SudekiMpControlUpdateDispatchWitness *w);
BOOL SudekiMpLanPartyJetpackFixture(BOOL *enabled);
BOOL SudekiMpLanPartyJetpackSetFixture(BOOL enabled);
BOOL SudekiMpLanPartyJetpackInput(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *lease,void *actor,BOOL held);
/* Requests release and positively observes native landing/visual retirement. */
BOOL SudekiMpLanPartyJetpackDrained(void *actor);
void SudekiMpLanPartyJetpackRequestStop(void);
BOOL SudekiMpLanPartyJetpackUninstall(void);
#endif
