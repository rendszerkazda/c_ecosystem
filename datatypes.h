#ifndef DATATYPES_H
#define DATATYPES_H

// Enum for entity types
typedef enum
{
    EMPTY, // Empty cell
    PLANT,
    HERBIVORE,
    CARNIVORE
} EntityType;

// Coordinates struct
typedef struct
{
    int x;
    int y;
} Coordinates;

// Forward declaration for Entity struct (if needed in World)
typedef struct Entity Entity;

// Cell struct
typedef struct
{
    Entity *entity; // Pointer to the entity in this cell, NULL if empty
} Cell;

// World struct
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

    int next_entity_id; // Következő kiosztandó egyedi ID (ez globális marad)
} World;

// Entity struct
struct Entity
{
    int id; // Egyedi azonosító
    EntityType type;
    Coordinates position;
    int energy;
    int age;

    // Viselkedés-specifikus attribútumok (később bővíthető)
    int sight_range;            // Pl. növényevőknek, ragadozóknak
    int last_reproduction_step; // Melyik szimulációs lépésben szaporodott utoljára
    int last_eating_step;       // Melyik szimulációs lépésben evett utoljára
};

#endif // DATATYPES_H