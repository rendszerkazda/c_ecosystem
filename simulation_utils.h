#ifndef SIMULATION_UTILS_H
#define SIMULATION_UTILS_H

#include "datatypes.h" // Szükséges a World és Entity típusokhoz

// Segédfüggvények a double bufferinghoz és párhuzamosításhoz
// Ezeket a main.c definiálja
void _commit_entity_to_next_state(World *world, Entity entity_data);
void free_world(World *world);
int is_valid_pos(const World *world, int x, int y);
Coordinates get_random_adjacent_empty_cell(World *world, Coordinates pos);
Coordinates get_step_towards_target(World *world, Coordinates current_pos, Coordinates target_pos);
int count_entities_by_type(const World *world, EntityType type);

#endif // SIMULATION_UTILS_H