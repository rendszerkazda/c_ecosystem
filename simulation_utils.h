#ifndef SIMULATION_UTILS_H
#define SIMULATION_UTILS_H

#include "datatypes.h" // Szükséges a World és Entity típusokhoz

// Segédfüggvények a double bufferinghoz és párhuzamosításhoz
// Ezeket a main.c definiálja, de más modulok is használhatják (pl. entity_actions.c)
void _ensure_next_entity_capacity(World *world, int min_capacity);
void _commit_entity_to_next_state(World *world, Entity entity_data);

#endif // SIMULATION_UTILS_H