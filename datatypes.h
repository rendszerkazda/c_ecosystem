#ifndef DATATYPES_H // Ha még nincs a header definiálva
#define DATATYPES_H

#include <stdbool.h>

typedef enum
{
    EMPTY, // Empty cell
    PLANT,
    HERBIVORE,
    CARNIVORE
} EntityType;

typedef struct
{
    int x;
    int y;
} Coordinates;

typedef struct Entity Entity;

typedef struct
{
    Entity *entity; // Pointer a cellában lévő entitásra, NULL ha üres
} Cell;

// Rankok között átlépő állatok pufferje
typedef struct {
    Entity *entities;
    int count;
    int capacity;
} EntityBuffer;

typedef struct
{
    // A méret globális marad
    int global_width;
    int global_height;

    // Lokális infók
    int start_y; 
    int end_y;
    int local_height;

    int ghost_layer_size;

    Cell **local_grid;    // Méret [width] x [local_height + 2*MAX_SIGHT_RANGE] paddinggal
    Entity *entities;    // Aktuális sáv entitásai
    int entity_count;    // Aktuális sáv entitások száma
    int entity_capacity; // Az 'entities' tömb kapacitása

    Cell **local_next_grid;         // Következő állapot rácsa írásra
    Entity *next_entities;    // Következő állapot entitáslistája írásra
    int next_entity_count;    // Entitások száma a következő állapotban
    int next_entity_capacity; // A 'next_entities' tömb kapacitása

    int next_entity_id; // Következő kiosztandó egyedi ID

    // Migráció
    EntityBuffer out_top;
    EntityBuffer out_bottom;

    //MPI
    int rank;
    int num_processes;
    int top_neighbor_rank;
    int bottom_neighbor_rank;
} World;

struct Entity
{
    int id; // Egyedi azonosító
    EntityType type;
    Coordinates position;
    int energy;
    int age;

    int sight_range;
    int last_reproduction_step; // Melyik szimulációs lépésben szaporodott utoljára
    int last_eating_step;
    bool just_spawned_by_keypress; // Igaz, ha az billentyűvel hozták létre ebben a lépésben
};

#endif // DATATYPES_H