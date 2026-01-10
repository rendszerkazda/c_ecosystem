#ifndef WORLD_UTILS_H
#define WORLD_UTILS_H

#include "datatypes.h" // Szükséges a World, EntityType, Coordinates típusokhoz

// Segédfüggvény a globális Y koordináta lokális rács indexre váltásához
static inline int get_local_y(const World *world, int global_y) {
    return global_y - world->start_y + world->ghost_layer_size;
}

// Ellenőrzi, hogy egy koordináta a saját felelősségi körünkbe tartozik-e (nem ghost)
static inline int is_local(const World *world, int global_y) {
    return (global_y >= world->start_y && global_y <= world->end_y);
}

// Világ létrehozása, inicializálása és felszabadítása
World *create_world(int width, int height, int rank, int num_processes);
void free_world(World *world);
void initialize_world(World *world, int num_plants, int num_herbivores, int num_carnivores);
Entity *add_entity_to_world_initial(World *world, EntityType type, Coordinates pos, int energy, int age, int sight_range);

// Pozíció és segédfüggvények
int is_valid_pos(const World *world, int x, int y);
Coordinates get_random_adjacent_empty_cell(World *world, Coordinates pos);
Coordinates get_step_towards_target(World *world, Coordinates current_pos, Coordinates target_pos);

// Az információs sávhoz
int count_entities_by_type(const World *world, EntityType type);

#endif // WORLD_UTILS_H