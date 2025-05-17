#ifndef WORLD_UTILS_H
#define WORLD_UTILS_H

#include "datatypes.h" // Szükséges a World, EntityType, Coordinates típusokhoz

// Világ létrehozása, inicializálása és felszabadítása
World *create_world(int width, int height);
void free_world(World *world);
void initialize_world(World *world, int num_plants, int num_herbivores, int num_carnivores);
Entity *add_entity_to_world_initial(World *world, EntityType type, Coordinates pos, int energy, int age, int sight_range);

// Pozíció validálása és segédfüggvények
int is_valid_pos(const World *world, int x, int y);                        // world lehet const, mert nem módosítja
Coordinates get_random_adjacent_empty_cell(World *world, Coordinates pos); // Ez módosíthatja a világot (ha pl. új entitást hozna létre, bár jelenleg nem teszi)
                                                                           // de a paraméterében a 'world' nem const, ami rendben van.
Coordinates get_step_towards_target(World *world, Coordinates current_pos, Coordinates target_pos);

// Entitások számolása típus szerint
int count_entities_by_type(const World *world, EntityType type);

#endif // WORLD_UTILS_H