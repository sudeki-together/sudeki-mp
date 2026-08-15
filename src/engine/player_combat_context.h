#ifndef SUDEKIMP_PLAYER_COMBAT_CONTEXT_H
#define SUDEKIMP_PLAYER_COMBAT_CONTEXT_H

#include <windows.h>

#define SUDEKIMP_MAX_LOCAL_PLAYERS 4u

typedef enum SudekiMpCombatPhase {
    SUDEKIMP_COMBAT_PHASE_IDLE = 0,
    SUDEKIMP_COMBAT_PHASE_TARGETING = 1,
    SUDEKIMP_COMBAT_PHASE_EXECUTING = 2
} SudekiMpCombatPhase;

typedef enum SudekiMpCombatInputSourceKind {
    SUDEKIMP_COMBAT_INPUT_NONE = 0,
    SUDEKIMP_COMBAT_INPUT_NATIVE_CONTROLLER = 1,
    SUDEKIMP_COMBAT_INPUT_KEYBOARD_PROTOTYPE = 2,
    SUDEKIMP_COMBAT_INPUT_EXTERNAL_BRIDGE = 3
} SudekiMpCombatInputSourceKind;

typedef struct SudekiMpPlayerCombatSnapshot {
    void *character;
    void *input_source;
    SudekiMpCombatInputSourceKind input_source_kind;
    void *viewport_camera;
    void *render_state;
    void *active_skill;
    void *targeter;
    void *target_node;
    DWORD skill_started_tick;
    DWORD targeting_started_tick;
    SudekiMpCombatPhase phase;
} SudekiMpPlayerCombatSnapshot;

void SudekiMpCombatContextsReset(void);
void SudekiMpCombatContextSetCharacter(unsigned int player, void *character);
void SudekiMpCombatContextSetInputSource(
    unsigned int player,
    SudekiMpCombatInputSourceKind kind,
    void *input_source
);
void SudekiMpCombatContextSetView(
    unsigned int player,
    void *camera,
    void *render_state
);
void SudekiMpCombatContextSkillStarted(void *character, void *skill);
void SudekiMpCombatContextSkillEnded(void *skill);
void SudekiMpCombatContextsPollState(
    BOOL targeting_active,
    void *target_node,
    int current_speed_mode,
    int requested_speed_mode,
    BOOL paused
);
void SudekiMpCombatContextsPollGame(HMODULE game_module);
BOOL SudekiMpCombatContextCanStartSkill(
    unsigned int player,
    const char **reason
);
BOOL SudekiMpCombatContextGlobalTargetingActive(void);
BOOL SudekiMpCombatContextRealtimeInvariantHolds(void);
int SudekiMpCombatContextFindPlayer(void *character);
BOOL SudekiMpCombatContextGetSnapshot(
    unsigned int player,
    SudekiMpPlayerCombatSnapshot *snapshot
);

#endif
