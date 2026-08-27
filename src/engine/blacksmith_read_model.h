#ifndef SUDEKIMP_BLACKSMITH_READ_MODEL_H
#define SUDEKIMP_BLACKSMITH_READ_MODEL_H

#include <stdint.h>

enum {
    SUDEKIMP_BLACKSMITH_READ_MAX_PLAYERS = 4u,
    SUDEKIMP_BLACKSMITH_READ_MAX_EQUIPMENT = 64u,
    SUDEKIMP_BLACKSMITH_READ_MAX_COMPONENTS = 64u,
    SUDEKIMP_BLACKSMITH_READ_MAX_SOCKETS = 3u,
    SUDEKIMP_BLACKSMITH_READ_LABEL_CAPACITY = 32u
};

typedef struct SudekiMpBlacksmithReadSocket {
    int32_t occupant_component_id;
    int32_t authored_component_id;
    uint32_t bank;
    uint32_t occupant_effect_class;
    float occupant_effect;
    int locked;
    int occupant_valid;
    char occupant_name[SUDEKIMP_BLACKSMITH_READ_LABEL_CAPACITY];
} SudekiMpBlacksmithReadSocket;

typedef struct SudekiMpBlacksmithReadEquipment {
    uint32_t item_id;
    uint32_t category_id;
    uint32_t item_class;
    uint32_t socket_count;
    float base_stat;
    float primary_scale;
    float primary_stat;
    float secondary_percent;
    int equipped;
    int stats_valid;
    char name[SUDEKIMP_BLACKSMITH_READ_LABEL_CAPACITY];
    SudekiMpBlacksmithReadSocket
        sockets[SUDEKIMP_BLACKSMITH_READ_MAX_SOCKETS];
} SudekiMpBlacksmithReadEquipment;

typedef struct SudekiMpBlacksmithReadComponent {
    uint32_t component_id;
    int32_t price;
    uint32_t kind;
    uint32_t bank;
    uint32_t effect_class;
    float effect;
    int definition_valid;
    int effect_valid;
    char name[SUDEKIMP_BLACKSMITH_READ_LABEL_CAPACITY];
    char effect_name[SUDEKIMP_BLACKSMITH_READ_LABEL_CAPACITY];
} SudekiMpBlacksmithReadComponent;

typedef struct SudekiMpBlacksmithReadSeat {
    uint32_t character_id;
    uint32_t actor_generation;
    uint32_t equipment_count;
    uint32_t equipped_index;
    int valid;
    int equipment_truncated;
    SudekiMpBlacksmithReadEquipment
        equipment[SUDEKIMP_BLACKSMITH_READ_MAX_EQUIPMENT];
} SudekiMpBlacksmithReadSeat;

typedef struct SudekiMpBlacksmithReadSnapshot {
    uint32_t player_count;
    uint32_t component_count;
    uint64_t catalog_fingerprint;
    uint64_t inventory_fingerprint;
    int valid;
    int catalog_truncated;
    SudekiMpBlacksmithReadComponent
        components[SUDEKIMP_BLACKSMITH_READ_MAX_COMPONENTS];
    SudekiMpBlacksmithReadSeat
        seats[SUDEKIMP_BLACKSMITH_READ_MAX_PLAYERS];
} SudekiMpBlacksmithReadSnapshot;

int SudekiMpBlacksmithReadModelComponentCompatible(
    const SudekiMpBlacksmithReadEquipment *equipment,
    uint32_t socket_index,
    const SudekiMpBlacksmithReadComponent *component
);
int SudekiMpBlacksmithReadModelProjectStats(
    const SudekiMpBlacksmithReadEquipment *equipment,
    uint32_t socket_index,
    const SudekiMpBlacksmithReadComponent *component,
    float *primary_stat,
    float *secondary_percent
);

#endif
