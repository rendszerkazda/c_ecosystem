#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "world_utils.h"
#include "simulation_constants.h"

// Létrehozza és inicializálja a szimulációs világot a megadott méretekkel.
// Lefoglalja a memóriát a világ struktúrának, a két rácsnak (grid és next_grid)
// és a két listának (entities és next_entities).
World *create_world(int width, int height)
{
    World *world = (World *)malloc(sizeof(World));
    if (!world)
    {
        perror("Hiba a világ struktúra memória foglalásakor");
        return NULL;
    }

    world->width = width;
    world->height = height;
    world->entity_count = 0;
    world->next_entity_count = 0;
    world->next_entity_id = 0;
    world->entity_capacity = MAX_TOTAL_ENTITIES;
    world->next_entity_capacity = MAX_TOTAL_ENTITIES;

    world->entities = (Entity *)malloc(MAX_TOTAL_ENTITIES * sizeof(Entity));
    if (!world->entities)
    {
        perror("Hiba az aktuális entitások tömbjének memória foglalásakor");
        free(world);
        return NULL;
    }

    world->next_entities = (Entity *)malloc(MAX_TOTAL_ENTITIES * sizeof(Entity));
    if (!world->next_entities)
    {
        perror("Hiba a következő entitások tömbjének memória foglalásakor");
        free(world->entities);
        free(world);
        return NULL;
    }

    world->grid = (Cell **)malloc(height * sizeof(Cell *));
    if (!world->grid)
    {
        perror("Hiba az aktuális rács sorainak memória foglalásakor");
        free(world->next_entities);
        free(world->entities);
        free(world);
        return NULL;
    }
    for (int i = 0; i < height; i++)
    {
        world->grid[i] = (Cell *)malloc(width * sizeof(Cell));
        if (!world->grid[i])
        {
            perror("Hiba az aktuális rács celláinak memória foglalásakor");
            for (int j = 0; j < i; j++)
                free(world->grid[j]);
            free(world->grid);
            free(world->next_entities);
            free(world->entities);
            free(world);
            return NULL;
        }
        for (int j = 0; j < width; j++)
            world->grid[i][j].entity = NULL;
    }

    world->next_grid = (Cell **)malloc(height * sizeof(Cell *));
    if (!world->next_grid)
    {
        perror("Hiba a következő rács sorainak memória foglalásakor");
        for (int i = 0; i < height; i++)
            free(world->grid[i]);
        free(world->grid);
        free(world->next_entities);
        free(world->entities);
        free(world);
        return NULL;
    }
    for (int i = 0; i < height; i++)
    {
        world->next_grid[i] = (Cell *)malloc(width * sizeof(Cell));
        if (!world->next_grid[i])
        {
            perror("Hiba a következő rács celláinak memória foglalásakor");
            for (int j = 0; j < i; j++)
                free(world->next_grid[j]);
            free(world->next_grid);
            for (int k = 0; k < height; k++)
                free(world->grid[k]);
            free(world->grid);
            free(world->next_entities);
            free(world->entities);
            free(world);
            return NULL;
        }
    }

    srand(time(NULL));
    return world;
}

// Hozzáad egy új entitást a világhoz a szimuláció kezdeti feltöltése során.
// Ez a függvény közvetlenül a world->entities tömbbe és a world->grid-be írja az új entitást.
// Szükség esetén dinamikusan növeli az entities tömb kapacitását.
Entity *add_entity_to_world_initial(World *world, EntityType type, Coordinates pos, int energy, int age, int sight_range)
{
    if (!world)
        return NULL;
    if (pos.x < 0 || pos.x >= world->width || pos.y < 0 || pos.y >= world->height)
    {
        fprintf(stderr, "Hiba: Érvénytelen pozíció az entitás (kezdeti) hozzáadásakor.\n");
        return NULL;
    }
    if (world->grid[pos.y][pos.x].entity != NULL)
    {
        return NULL; // Cella foglalt
    }

    if (world->entity_count >= MAX_TOTAL_ENTITIES)
    {
        fprintf(stderr, "Hiba: Elérte a maximális entitás számot (%d), nem lehet új entitást hozzáadni (kezdeti).\n", MAX_TOTAL_ENTITIES);
        return NULL; // Nincs több hely
    }

    Entity *new_entity = &world->entities[world->entity_count];
    new_entity->id = world->next_entity_id++;
    new_entity->type = type;
    new_entity->position = pos;
    new_entity->energy = energy;
    new_entity->age = age;
    new_entity->sight_range = sight_range;
    new_entity->last_reproduction_step = -1;
    new_entity->last_eating_step = -1;
    new_entity->just_spawned_by_keypress = false; // Alapértelmezetten hamis

    world->grid[pos.y][pos.x].entity = new_entity;
    world->entity_count++;
    return new_entity;
}

// Inicializálja a világot a megadott számú növénnyel, növényevővel és ragadozóval.
// Véletlenszerűen helyezi el az entitásokat az üres cellákba a `world->grid`-en.
// Meghatározott számú próbálkozást tesz minden egyes entitás elhelyezésére.
void initialize_world(World *world, int num_plants, int num_herbivores, int num_carnivores)
{
    if (!world)
        return;

    int placed_count;
    Coordinates pos;
    int initial_age = 0;

    // Növények
    placed_count = 0;
    for (int i = 0; i < num_plants; i++)
    {
        int attempts = 0;
        do
        {
            pos.x = rand() % world->width;
            pos.y = rand() % world->height;
            attempts++;
        } while (world->grid[pos.y][pos.x].entity != NULL && attempts < world->width * world->height * 2);
        if (world->grid[pos.y][pos.x].entity == NULL)
        {
            add_entity_to_world_initial(world, PLANT, pos, PLANT_MAX_ENERGY / 2, initial_age, 0);
            placed_count++;
        }
    }
    // printf("%d/%d növény elhelyezve.\n", placed_count, num_plants); // Ezt a main logolhatja

    // Növényevők
    placed_count = 0;
    for (int i = 0; i < num_herbivores; i++)
    {
        int attempts = 0;
        do
        {
            pos.x = rand() % world->width;
            pos.y = rand() % world->height;
            attempts++;
        } while (world->grid[pos.y][pos.x].entity != NULL && attempts < world->width * world->height * 2);
        if (world->grid[pos.y][pos.x].entity == NULL)
        {
            add_entity_to_world_initial(world, HERBIVORE, pos, HERBIVORE_INITIAL_ENERGY, initial_age, HERBIVORE_SIGHT_RANGE);
            placed_count++;
        }
    }
    // printf("%d/%d növényevő elhelyezve.\n", placed_count, num_herbivores);

    // Ragadozók
    placed_count = 0;
    for (int i = 0; i < num_carnivores; i++)
    {
        int attempts = 0;
        do
        {
            pos.x = rand() % world->width;
            pos.y = rand() % world->height;
            attempts++;
        } while (world->grid[pos.y][pos.x].entity != NULL && attempts < world->width * world->height * 2);
        if (world->grid[pos.y][pos.x].entity == NULL)
        {
            add_entity_to_world_initial(world, CARNIVORE, pos, CARNIVORE_INITIAL_ENERGY, initial_age, CARNIVORE_SIGHT_RANGE);
            placed_count++;
        }
    }
    // printf("%d/%d ragadozó elhelyezve.\n", placed_count, num_carnivores);
}

// Felszabadítja a World objektum és annak minden dinamikusan foglalt erőforrását.
// rácsok sorai és maguk a rácso
// az entitáslisták (entities, next_entities).
void free_world(World *world)
{
    if (!world)
        return;

    if (world->grid)
    {
        for (int i = 0; i < world->height; i++)
        {
            if (world->grid[i])
                free(world->grid[i]);
        }
        free(world->grid);
    }
    if (world->next_grid)
    {
        for (int i = 0; i < world->height; i++)
        {
            if (world->next_grid[i])
                free(world->next_grid[i]);
        }
        free(world->next_grid);
    }
    if (world->entities)
        free(world->entities);
    if (world->next_entities)
        free(world->next_entities);
    free(world);
}

// Ellenőrzi, hogy egy adott (x, y) pozíció érvényes-e, azaz a világ határain belül esik-e.
// Visszatérési érték: 1, ha érvényes, 0 egyébként.
int is_valid_pos(const World *world, int x, int y)
{
    return world && x >= 0 && x < world->width && y >= 0 && y < world->height;
}

Coordinates get_random_adjacent_empty_cell(World *world, Coordinates pos)
{
    Coordinates adjacent_cells[8];
    int count = 0;

    // Szomszédos cellák (8 irány)
    int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};

    for (int i = 0; i < 8; ++i)
    {
        int nx = pos.x + dx[i];
        int ny = pos.y + dy[i];

        if (is_valid_pos(world, nx, ny) && world->grid[ny][nx].entity == NULL) // Az aktuális grid-et nézzük
        {
            adjacent_cells[count].x = nx;
            adjacent_cells[count].y = ny;
            count++;
        }
    }

    if (count > 0)
    {
        return adjacent_cells[rand() % count];
    }
    else
    {
        return pos; // Nincs üres szomszédos cella
    }
}

// Kiszámítja a következő lépés koordinátáit `current_pos`-ból `target_pos` felé.
// A cél az, hogy egy lépéssel közelebb kerüljön a célponthoz a Manhattan-távolság csökkentésével.
// Először megpróbál az X tengely mentén közeledni, majd az Y tengely mentén, vagy fordítva,
// attól függően, melyik irányban nagyobb a távolság. Priorizálja azokat a lépéseket,
// amelyek mindkét tengelyen csökkentik a távolságot (átlós lépés), ha lehetséges.
// A függvény nem ellenőrzi, hogy a célcella (a lépés utáni pozíció) üres-e vagy érvényes-e.
// Ezt a hívó félnek kell kezelnie, ha szükséges.
// Ha a `current_pos` már megegyezik `target_pos`-szal, vagy nem tud közelebb lépni,
// akkor `current_pos`-t adja vissza.
Coordinates get_step_towards_target(World *world, Coordinates current_pos, Coordinates target_pos)
{
    Coordinates best_step = current_pos;
    int min_dist_sq = (target_pos.x - current_pos.x) * (target_pos.x - current_pos.x) +
                      (target_pos.y - current_pos.y) * (target_pos.y - current_pos.y);

    // 8 irány
    int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};

    // random nézzük az iráynokat
    int order[] = {0, 1, 2, 3, 4, 5, 6, 7};
    for (int i = 0; i < 8; ++i)
    {
        int r = i + (rand() % (8 - i));
        int temp = order[i];
        order[i] = order[r];
        order[r] = temp;
    }

    for (int i = 0; i < 8; ++i)
    {
        int idx = order[i];
        int next_x = current_pos.x + dx[idx];
        int next_y = current_pos.y + dy[idx];

        if (is_valid_pos(world, next_x, next_y) && world->grid[next_y][next_x].entity == NULL)
        {
            int dist_sq = (target_pos.x - next_x) * (target_pos.x - next_x) +
                          (target_pos.y - next_y) * (target_pos.y - next_y);
            if (dist_sq < min_dist_sq)
            {
                min_dist_sq = dist_sq;
                best_step.x = next_x;
                best_step.y = next_y;
            }
        }
    }
    return best_step;
}

// Megszámolja az adott `type` típusú entitásokat a `world->entities` listában.
int count_entities_by_type(const World *world, EntityType type)
{
    if (!world || !world->entities)
        return 0;

    int count = 0;
    for (int i = 0; i < world->entity_count; ++i)
    {
        if (world->entities[i].id >= 0 && world->entities[i].energy > 0 && world->entities[i].type == type)
        {
            count++;
        }
    }
    return count;
}