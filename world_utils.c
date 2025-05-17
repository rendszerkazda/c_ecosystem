#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "world_utils.h"
#include "simulation_constants.h" // Szükséges az INITIAL_ENTITY_CAPACITY miatt

// Világ létrehozása
World *create_world(int width, int height)
{
    World *world = (World *)malloc(sizeof(World));
    if (!world)
    {
        perror("Hiba a Világ struktúra memória foglalásakor");
        return NULL;
    }

    world->width = width;
    world->height = height;
    world->entity_count = 0;
    world->next_entity_count = 0;
    world->next_entity_id = 0;
    world->entity_capacity = INITIAL_ENTITY_CAPACITY;
    world->next_entity_capacity = INITIAL_ENTITY_CAPACITY;

    world->entities = (Entity *)malloc(world->entity_capacity * sizeof(Entity));
    if (!world->entities)
    {
        perror("Hiba az aktuális entitások tömbjének memória foglalásakor");
        free(world);
        return NULL;
    }

    world->next_entities = (Entity *)malloc(world->next_entity_capacity * sizeof(Entity));
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

    srand(time(NULL)); // Ezt elég egyszer meghívni, lehet, hogy a main()-be jobb lenne.
                       // De itt hagyom, mert a create_world hívásakor releváns lehet a randomitás, ha pl. add_entity közvetlenül használja.
    return world;
}

// Entitás hozzáadása a világhoz (kezdeti feltöltéshez)
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

    if (world->entity_count >= world->entity_capacity)
    {
        world->entity_capacity *= 2;
        Entity *new_array = (Entity *)realloc(world->entities, world->entity_capacity * sizeof(Entity));
        if (!new_array)
        {
            perror("Hiba az aktuális entitások tömbjének átméretezésekor (kezdeti)");
            world->entity_capacity /= 2;
            return NULL;
        }
        world->entities = new_array;
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

    world->grid[pos.y][pos.x].entity = new_entity;
    world->entity_count++;
    return new_entity;
}

// Világ inicializálása entitásokkal
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

// Világ által lefoglalt memória felszabadítása
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

// Ellenőrzi, hogy egy pozíció érvényes-e a világ határain belül.
int is_valid_pos(const World *world, int x, int y)
{
    return world && x >= 0 && x < world->width && y >= 0 && y < world->height;
}

// Visszaad egy véletlenszerűen kiválasztott, a megadott pozícióval szomszédos üres cella koordinátáját.
// Ha nincs ilyen, akkor az eredeti pozíciót adja vissza.
// Ez a függvény NEM garantálja, hogy az üres cella továbbra is üres marad a hívás és a felhasználás között,
// különösen párhuzamos környezetben. A hívónak kell ezt kezelnie.
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

// Új függvény: Kiszámolja a legjobb lépést egy adott célpozíció felé
// Figyelem: Ez a world->grid alapján ellenőrzi az üres cellákat.
Coordinates get_step_towards_target(World *world, Coordinates current_pos, Coordinates target_pos)
{
    Coordinates best_step = current_pos; // Alapértelmezés: marad a helyén
    int min_dist_sq = (target_pos.x - current_pos.x) * (target_pos.x - current_pos.x) +
                      (target_pos.y - current_pos.y) * (target_pos.y - current_pos.y);

    // Lehetséges 8 irány (Moore szomszédság)
    int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};

    // Véletlenszerű sorrendben vizsgáljuk az irányokat, hogy ne legyen kitüntetett irány
    // ha több egyformán jó lépés van.
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

// Entitások számolása típus szerint
// Csak az AKTÍV (entity_count-ban lévő, érvényes ID-jű és pozitív energiájú) entitásokat számolja.
int count_entities_by_type(const World *world, EntityType type)
{
    if (!world || !world->entities)
        return 0;

    int count = 0;
    // Mivel a párhuzamosítás miatt az entitások sorrendje és "aktivitása" változhat a world->entities-ben,
    // és az entity_count a releváns szám, ezért csak ezen belül iterálunk.
    // Az entity_actions-ben lévő logika biztosítja, hogy az elpusztult entitások nem kerülnek
    // a next_entities-be, és így a következő lépés entities tömbjébe sem.
    // A megjelenítés mindig a `world->entities` és `world->entity_count` alapján történik.
    for (int i = 0; i < world->entity_count; ++i)
    {
        // Extra ellenőrzés, bár elvileg az entity_count-ban lévőknek aktívnak kellene lenniük.
        if (world->entities[i].id >= 0 && world->entities[i].energy > 0 && world->entities[i].type == type)
        {
            count++;
        }
    }
    return count;
}