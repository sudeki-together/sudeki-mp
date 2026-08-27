#include "engine/blacksmith_read_model.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

int SudekiMpBlacksmithReadModelComponentCompatible(
    const SudekiMpBlacksmithReadEquipment *equipment,
    uint32_t socket_index,
    const SudekiMpBlacksmithReadComponent *component
) {
    const SudekiMpBlacksmithReadSocket *socket;
    uint32_t index;

    if (equipment == NULL || component == NULL ||
        !component->definition_valid || !component->effect_valid ||
        socket_index >= equipment->socket_count ||
        socket_index >= SUDEKIMP_BLACKSMITH_READ_MAX_SOCKETS ||
        component->effect_class == 0u ||
        component->component_id > (uint32_t)INT8_MAX ||
        component->effect_class != equipment->item_class) {
        return 0;
    }
    socket = &equipment->sockets[socket_index];
    if (socket->locked || socket->authored_component_id != -1 ||
        socket->bank != component->bank) {
        return 0;
    }
    if (component->effect_class == 2u) {
        for (index = 0u; index < equipment->socket_count &&
             index < SUDEKIMP_BLACKSMITH_READ_MAX_SOCKETS; ++index) {
            if (equipment->sockets[index].occupant_valid &&
                equipment->sockets[index].occupant_component_id ==
                    (int32_t)component->component_id) {
                return 0;
            }
        }
    }
    return 1;
}

int SudekiMpBlacksmithReadModelProjectStats(
    const SudekiMpBlacksmithReadEquipment *equipment,
    uint32_t socket_index,
    const SudekiMpBlacksmithReadComponent *component,
    float *primary_stat,
    float *secondary_percent
) {
    const SudekiMpBlacksmithReadSocket *socket;
    float primary;
    float secondary;

    if (primary_stat == NULL || secondary_percent == NULL ||
        equipment == NULL || !equipment->stats_valid ||
        !SudekiMpBlacksmithReadModelComponentCompatible(
            equipment, socket_index, component)) {
        return 0;
    }
    socket = &equipment->sockets[socket_index];
    primary = equipment->primary_stat;
    secondary = equipment->secondary_percent;
    if (socket->occupant_valid && socket->occupant_effect_class == 1u) {
        primary -= socket->occupant_effect * equipment->primary_scale;
    } else if (socket->occupant_valid &&
            socket->occupant_effect_class == 2u) {
        secondary -= socket->occupant_effect * 100.0f;
    }
    if (component->effect_class == 1u) {
        primary += component->effect * equipment->primary_scale;
    } else if (component->effect_class == 2u) {
        secondary += component->effect * 100.0f;
    }
    if (!isfinite(primary) || !isfinite(secondary)) {
        return 0;
    }
    *primary_stat = primary;
    *secondary_percent = secondary;
    return 1;
}
