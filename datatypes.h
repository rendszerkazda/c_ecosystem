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

typedef struct
{
    int width;
    int height;
    Cell **grid;         // Aktuális rács olvasásra
    Entity *entities;    // Aktuális entitások listája olvasásra
    int entity_count;    // Aktuális entitások száma
    int entity_capacity; // Az 'entities' tömb kapacitása

    Cell **next_grid;         // Következő állapot rácsa írásra
    Entity *next_entities;    // Következő állapot entitáslistája írásra
    int next_entity_count;    // Entitások száma a következő állapotban
    int next_entity_capacity; // A 'next_entities' tömb kapacitása

    int next_entity_id; // Következő kiosztandó egyedi ID
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