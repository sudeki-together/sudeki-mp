#ifndef SUDEKIMP_LAN_STORY_RUNTIME_H
#define SUDEKIMP_LAN_STORY_RUNTIME_H
#include "network/lan_party_session.h"
#include "network/title_lobby.h"

/* Private story profiles: 1 observes sparse native scenes; 2 loads the pinned
 * saved world and contains client simulation for noncombat playback. Native
 * party control and swaps require independent leases and fresh view ACKs;
 * creation is limited to the separately validated Lighthouse recruitment. */
BOOL SudekiMpInstallLanStoryRuntime(HMODULE module,const SudekiMpLanPartyConfig *config);
BOOL SudekiMpUninstallLanStoryRuntime(void);
void SudekiMpLanStoryRuntimeSetStepAway(BOOL enabled);
void SudekiMpLanStoryRuntimeSetClientLeadSpectate(BOOL enabled);
BOOL SudekiMpLanStoryRuntimeStepAwayEnabled(void);
/* -1 unknown, 0 present/not-away, 1 away. Entry admission fails closed; displays do not. */
int SudekiMpLanStoryRuntimeCharacterAwayState(unsigned character);
BOOL SudekiMpLanStoryRuntimeCharacterAway(unsigned character);
/* Host game-thread proof of a connected away seat after native control has
 * drained. The expected transport lease prevents reusing a prior connection. */
enum SudekiMpLanStoryRuntimeAwaySeatRefusal {
    STORY_AWAY_REFUSAL_INVALID_ARGUMENT,
    STORY_AWAY_REFUSAL_NOT_INITIALIZED,
    STORY_AWAY_REFUSAL_CLIENT_SEAT,
    STORY_AWAY_REFUSAL_THREAD,
    STORY_AWAY_REFUSAL_STOPPING,
    STORY_AWAY_REFUSAL_BINDING_NOT_READY,
    STORY_AWAY_REFUSAL_MENU_CAPTURES,
    STORY_AWAY_REFUSAL_NOT_OWNER,
    STORY_AWAY_REFUSAL_SWAP_PHASE,
    STORY_AWAY_REFUSAL_NOT_CONNECTED,
    STORY_AWAY_REFUSAL_NOT_AWAY,
    STORY_AWAY_REFUSAL_CONTROL_NOT_AI,
    STORY_AWAY_REFUSAL_NATIVE_KEY_RETAINED,
    STORY_AWAY_REFUSAL_ACQUIRED,
    STORY_AWAY_REFUSAL_DRAINING,
    STORY_AWAY_REFUSAL_CHARACTER_MISMATCH,
    STORY_AWAY_REFUSAL_PEER_STATUS,
    STORY_AWAY_REFUSAL_PEER_PHASE,
    STORY_AWAY_REFUSAL_TRANSPORT,
    STORY_AWAY_REFUSAL_CONNECTION_MISMATCH,
    STORY_AWAY_REFUSAL_NONE,
};
enum SudekiMpLanStoryRuntimeAwaySeatRefusal SudekiMpLanStoryRuntimeAwaySeatRefusal(
    unsigned character,const SudekiMpLanPartyLease *connection);
BOOL SudekiMpLanStoryRuntimeAwaySeatExact(unsigned character,
    const SudekiMpLanPartyLease *connection);
/* Current unclaimed companion, or a proven fully released away reservation. */
BOOL SudekiMpLanStoryRuntimeCompanionSeatExact(unsigned character);
BOOL SudekiMpLanStoryRuntimeAwayIntent(void);
BOOL SudekiMpLanStoryRuntimeRequestAway(BOOL away);
/* #71: FALSE keeps the client's mathematical orbit camera (ini [StoryAreas] ClientMathCamera=true). */
void SudekiMpLanStoryRuntimeSetClientNativeCamera(BOOL enabled);
/* Lobby-owned terminal UI transaction: prepare restoration while paused,
 * retire observers/transport, balance pause and synchronously exit the world,
 * then remove native input/menu/trigger fences. May need render preparation. */
BOOL SudekiMpLanStoryRuntimeExitToTitle(void);
unsigned SudekiMpLanStoryRuntimePort(void);
/* Positive native saved-load and presentation readiness, not connectivity.
 * Reserved but unavailable characters do not become input-enabled here. */
BOOL SudekiMpLanStoryRuntimeReady(void);
/* Game-thread read-only authority check for the retained C2 turn callback. */
BOOL SudekiMpLanStoryRuntimeTurnLeaseExact(const SudekiMpLanPartyLease *native_key);
/* Same exact acquired owner, including PREPARE. AI movement suppression only;
 * player input/turning still requires the acknowledged TurnLease above. */
BOOL SudekiMpLanStoryRuntimeAiLeaseExact(const SudekiMpLanPartyLease *native_key);
void SudekiMpLanStoryRuntimeLobbyService(SudekiMpLobby *lobby);
/* Lobby player name holding character 0..3 (Buki, Elco, Tal, Ailish) from the
 * last lobby status copy; FALSE when the character is not held by a present
 * member with a non-empty name. Runtime thread only; copy-only. */
BOOL SudekiMpLanStoryRuntimePlayerName(unsigned character,char out[32]);
#endif
