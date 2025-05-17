#ifndef ENTITY_ACTIONS_H
#define ENTITY_ACTIONS_H

#include "datatypes.h" // Szükséges a World, Entity, EntityType, Coordinates típusokhoz

// Típus-specifikus párhuzamos akciókezelő függvények
void process_plant_actions_parallel(World *world, int current_step_number, const Entity *current_plant_state, Entity *next_plant_state_prototype);
void process_herbivore_actions_parallel(World *world, int current_step_number, Entity *current_herbivore_state_in_entities_array, Entity *next_herbivore_state_prototype);
void process_carnivore_actions_parallel(World *world, int current_step_number, Entity *current_carnivore_state_in_entities_array, Entity *next_carnivore_state_prototype);

// Segédfüggvény célpont kereséséhez (állatoknak)
Entity *find_target_in_range(World *world, Coordinates center, int range, EntityType target_type);

#endif // ENTITY_ACTIONS_H