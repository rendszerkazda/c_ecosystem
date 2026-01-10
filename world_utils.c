#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <mpi.h>

#include "world_utils.h"
#include "simulation_constants.h"

// Létrehozza és inicializálja a szimulációs világot a megadott méretekkel.
// 1D Domain Decomposition logikával.
World *create_world(int width, int height, int rank, int num_processes)
{
    World *world = (World *)malloc(sizeof(World));
    if (!world)
    {
        perror("Hiba a világ struktúra memória foglalásakor");
        return NULL;
    }
    world->global_width = width;
    world->global_height = height;
    
    // Lokális szelet kiszámítása
    int base_height = height / num_processes;
    int remainder = height % num_processes;
    
    // Az egyszerűség kedvéért a maradékot az utolsó kapja
    if (rank < remainder) {
        world->local_height = base_height + (rank == num_processes - 1 ? remainder : 0);
    } else {
        world->local_height = base_height + (rank == num_processes - 1 ? remainder : 0);
    }
    
    // Start Y kiszámítása (mindenki előtti sorok összege)
    world->start_y = rank * base_height; 

    world->end_y = world->start_y + world->local_height - 1;

    world->ghost_layer_size = GHOST_LAYER_SIZE;
    world->rank = rank;
    world->num_processes = num_processes;
    
    // Szomszédok
    world->top_neighbor_rank = (rank == 0) ? MPI_PROC_NULL : rank - 1;
    world->bottom_neighbor_rank = (rank == num_processes - 1) ? MPI_PROC_NULL : rank + 1;

    world->entity_count = 0;
    world->next_entity_count = 0;
    world->next_entity_id = rank * (MAX_TOTAL_ENTITIES * 10); // ID tartomány szétválasztása
    world->entity_capacity = MAX_TOTAL_ENTITIES; // Lokálisan is ennyi férjen el (biztonsági tartalék)
    world->next_entity_capacity = MAX_TOTAL_ENTITIES;

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
    
    // Migrációs pufferek inicializálása
    // Kezdeti kapacitás, szükség esetén realloc
    int buffer_cap = 100;
    world->out_top.entities = (Entity*)malloc(buffer_cap * sizeof(Entity));
    world->out_top.capacity = buffer_cap;
    world->out_top.count = 0;
    
    world->out_bottom.entities = (Entity*)malloc(buffer_cap * sizeof(Entity));
    world->out_bottom.capacity = buffer_cap;
    world->out_bottom.count = 0;


    // Rácsok foglalása (Ghost sávokkal)
    int total_rows = world->local_height + (2 * world->ghost_layer_size);
    
    world->local_grid = (Cell **)malloc(total_rows * sizeof(Cell *));
    if (!world->local_grid)
    {
        perror("Hiba az aktuális rács sorainak memória foglalásakor");
        // Free everything...
        return NULL;
    }
    for (int i = 0; i < total_rows; i++)
    {
        world->local_grid[i] = (Cell *)malloc(width * sizeof(Cell));
        if (!world->local_grid[i]) return NULL; // TODO: rendes cleanup
        for (int j = 0; j < width; j++)
            world->local_grid[i][j].entity = NULL;
    }

    world->local_next_grid = (Cell **)malloc(total_rows * sizeof(Cell *));
    if (!world->local_next_grid) return NULL; // TODO: cleanup
    
    for (int i = 0; i < total_rows; i++)
    {
        world->local_next_grid[i] = (Cell *)malloc(width * sizeof(Cell));
        if (!world->local_next_grid[i]) return NULL; // TODO: cleanup
        for (int j = 0; j < width; j++)
            world->local_next_grid[i][j].entity = NULL;
    }

    srand(time(NULL) + rank); // Minden ranknak más seed!
    return world;
}

Entity *add_entity_to_world_initial(World *world, EntityType type, Coordinates pos, int energy, int age, int sight_range)
{
    if (!world) return NULL;
    
    // Globális határok ellenőrzése
    if (pos.x < 0 || pos.x >= world->global_width || pos.y < 0 || pos.y >= world->global_height)
        return NULL;

    // Lokális rácsba írás
    int local_y = get_local_y(world, pos.y);
    
    // Biztonsági ellenőrzés: csak a saját területre (nem ghostba) írunk inicializáláskor
    if (pos.y < world->start_y || pos.y > world->end_y) {
        // Ez nem ide tartozik
        return NULL;
    }

    if (world->local_grid[local_y][pos.x].entity != NULL)
        return NULL; // Foglalt

    if (world->entity_count >= world->entity_capacity)
        return NULL;

    Entity *new_entity = &world->entities[world->entity_count];
    new_entity->id = world->next_entity_id++;
    new_entity->type = type;
    new_entity->position = pos; // Globális pozíciót tárolunk!
    new_entity->energy = energy;
    new_entity->age = age;
    new_entity->sight_range = sight_range;
    new_entity->last_reproduction_step = -1;
    new_entity->last_eating_step = -1;
    new_entity->just_spawned_by_keypress = false;

    world->local_grid[local_y][pos.x].entity = new_entity;
    world->entity_count++;
    return new_entity;
}

void initialize_world(World *world, int num_plants, int num_herbivores, int num_carnivores)
{
    if (!world) return;

    // Minden processz a saját területére generál
    // Arányosan osztjuk el a számokat (vagy mindenki megkapja a teljes számot és csak a sajátját rakja le?
    // Jobb, ha arányosan osztjuk el, hogy a sűrűség állandó legyen.)
    
    // Feltételezzük, hogy a num_ paraméterek a GLOBÁLIS darabszámok.
    // Kiszámoljuk, mennyi jut ránk területarányosan.
    double ratio = (double)world->local_height / world->global_height;
    
    int my_plants = num_plants * ratio;
    int my_herbivores = num_herbivores * ratio;
    int my_carnivores = num_carnivores * ratio;

    int placed_count;
    Coordinates pos;
    int initial_age = 0;
    int rand_x, rand_y;

    // Növények
    for (int i = 0; i < my_plants; i++)
    {
        int attempts = 0;
        int local_y_index;
        do
        {
            rand_x = rand() % world->global_width;
            rand_y = world->start_y + (rand() % world->local_height);
            local_y_index = rand_y - world->start_y + world->ghost_layer_size;

            if (world->local_grid[local_y_index][rand_x].entity == NULL)
            {
                pos.x = rand_x;
                pos.y = rand_y;
                break;
            }
            attempts++;
        } while (attempts < 1000);

        if (attempts < 1000)
        {
            add_entity_to_world_initial(world, PLANT, pos, PLANT_MAX_ENERGY / 2, initial_age, 0);
        }
    }

    // Növényevők
    for (int i = 0; i < my_herbivores; i++)
    {
        int attempts = 0;
        int local_y_index;
        do
        {
            rand_x = rand() % world->global_width;
            rand_y = world->start_y + (rand() % world->local_height);
            local_y_index = rand_y - world->start_y + world->ghost_layer_size;

            if (world->local_grid[local_y_index][rand_x].entity == NULL)
            {
                pos.x = rand_x;
                pos.y = rand_y;
                break;
            }
            attempts++;
        } while (attempts < 1000);

        if (attempts < 1000)
        {
            add_entity_to_world_initial(world, HERBIVORE, pos, HERBIVORE_INITIAL_ENERGY, initial_age, HERBIVORE_SIGHT_RANGE);
        }
    }

    // Ragadozók
    for (int i = 0; i < my_carnivores; i++)
    {
        int attempts = 0;
        int local_y_index;
        do
        {
            rand_x = rand() % world->global_width;
            rand_y = world->start_y + (rand() % world->local_height);
            local_y_index = rand_y - world->start_y + world->ghost_layer_size;

            if (world->local_grid[local_y_index][rand_x].entity == NULL)
            {
                pos.x = rand_x;
                pos.y = rand_y;
                break;
            }
            attempts++;
        } while (attempts < 1000);

        if (attempts < 1000)
        {
            add_entity_to_world_initial(world, CARNIVORE, pos, CARNIVORE_INITIAL_ENERGY, initial_age, CARNIVORE_SIGHT_RANGE);
        }
    }
}

void free_world(World *world)
{
    if (!world) return;

    int total_rows = world->local_height + (2 * world->ghost_layer_size);

    if (world->local_grid)
    {
        for (int i = 0; i < total_rows; i++)
            if (world->local_grid[i]) free(world->local_grid[i]);
        free(world->local_grid);
    }
    if (world->local_next_grid)
    {
        for (int i = 0; i < total_rows; i++)
            if (world->local_next_grid[i]) free(world->local_next_grid[i]);
        free(world->local_next_grid);
    }
    
    if (world->entities) free(world->entities);
    if (world->next_entities) free(world->next_entities);
    if (world->out_top.entities) free(world->out_top.entities);
    if (world->out_bottom.entities) free(world->out_bottom.entities);
    
    free(world);
}

// Ellenőrzi, hogy egy (globális) pozíció valid-e a mi szemszögünkből
// A mi szemszögünk: benne van-e a local_grid-ben (tehát saját vagy ghost terület)
int is_valid_pos(const World *world, int x, int y)
{
    if (!world) return 0;
    if (x < 0 || x >= world->global_width) return 0;
    
    // Y ellenőrzés: A teljes allokált tartományban benne van-e?
    // Min: start_y - ghost_size
    // Max: end_y + ghost_size
    int min_y = world->start_y - world->ghost_layer_size;
    int max_y = world->end_y + world->ghost_layer_size;
    
    return (y >= min_y && y <= max_y);
}

Coordinates get_random_adjacent_empty_cell(World *world, Coordinates pos)
{
    Coordinates adjacent_cells[8];
    int count = 0;
    int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};

    for (int i = 0; i < 8; ++i)
    {
        int nx = pos.x + dx[i];
        int ny = pos.y + dy[i];

        if (is_valid_pos(world, nx, ny))
        {
            int local_y = get_local_y(world, ny);
            // Ellenőrzés, hogy index határon belül van-e (is_valid_pos elvileg szűrte, de biztos ami biztos)
            // A local_grid mérete: local_height + 2*ghost
            // local_y 0-tól indul.
            // is_valid_pos már ellenőrizte, hogy a ghost határokon belül van globálisan.
            
            if (world->local_grid[local_y][nx].entity == NULL)
            {
                adjacent_cells[count].x = nx;
                adjacent_cells[count].y = ny;
                count++;
            }
        }
    }

    if (count > 0) return adjacent_cells[rand() % count];
    else return pos;
}

Coordinates get_step_towards_target(World *world, Coordinates current_pos, Coordinates target_pos)
{
    Coordinates best_step = current_pos;
    int min_dist_sq = (target_pos.x - current_pos.x) * (target_pos.x - current_pos.x) +
                      (target_pos.y - current_pos.y) * (target_pos.y - current_pos.y);

    int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};
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

        if (is_valid_pos(world, next_x, next_y))
        {
            int local_y = get_local_y(world, next_y);
            if (world->local_grid[local_y][next_x].entity == NULL)
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
    }
    return best_step;
}

int count_entities_by_type(const World *world, EntityType type)
{
    if (!world || !world->entities) return 0;
    int count = 0;
    for (int i = 0; i < world->entity_count; ++i)
    {
        if (world->entities[i].energy > 0 && world->entities[i].type == type)
            count++;
    }
    return count;
}
