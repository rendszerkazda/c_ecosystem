#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>
#include <ncurses.h>
#include <string.h>
#include <locale.h>

#include "simulation_constants.h"
#include "datatypes.h"
#include "world_utils.h" // create_world, initialize_world, free_world, is_valid_pos
#include "entity_actions.h"
#include "simulation_utils.h"

#define RANDOM_SEED 42

#define COLOR_PAIR_BORDER 1
#define COLOR_PAIR_PLANT 2
#define COLOR_PAIR_HERBIVORE 3
#define COLOR_PAIR_CARNIVORE 4
#define COLOR_PAIR_INFO 5
#define COLOR_PAIR_EMPTY 6
#define COLOR_PAIR_MENU_ITEM 7
#define COLOR_PAIR_MENU_SELECTED 8
#define COLOR_PAIR_SETTINGS_VALUE 9     // Kiválasztott beállítás értékének színe
#define COLOR_PAIR_SPAWNED_HERBIVORE 10 // Kiemelt háttérrel
#define COLOR_PAIR_SPAWNED_CARNIVORE 11 // Kiemelt háttérrel

// Főmenüelemek
typedef enum
{
    MENU_START,
    MENU_SETTINGS,
    MENU_EXIT,
    MENU_ITEM_COUNT
} MenuItem;

const char *menu_item_names[] = {
    " Start Simulation ",
    "     Settings     ",
    "       Exit       "};

typedef enum
{
    SIZE_SMALL,
    SIZE_MEDIUM,
    SIZE_LARGE,
    WORLD_SIZE_COUNT
} WorldSizeSetting;

const char *world_size_names[] = {"Small (30x20)", "Medium (50x30)", "Large (100x35)"};
Coordinates world_size_values[] = {{30, 20}, {50, 30}, {100, 35}};

typedef enum
{
    DELAY_FAST,
    DELAY_NORMAL,
    DELAY_SLOW,
    DELAY_COUNT
} DelaySetting;

const char *delay_names[] = {"Quick (50ms)", "Normal (150ms)", "Slow (300ms)"};
long delay_values_ms[] = {50, 150, 300}; // Milliszekundum

typedef enum
{
    STEPS_SHORT,
    STEPS_MEDIUM,
    STEPS_LONG,
    SIMULATION_STEPS_COUNT
} SimulationStepsSetting;

const char *simulation_steps_names[] = {"Short (200)", "Normal (500)", "Long (1000)"};
int simulation_steps_values[] = {200, 500, 1000};

typedef struct
{
    WorldSizeSetting world_size_choice;
    DelaySetting delay_choice;
    SimulationStepsSetting steps_choice;
} SimulationSettings;

SimulationSettings current_settings = {SIZE_MEDIUM, DELAY_NORMAL, STEPS_MEDIUM}; // alapértelmezett beállítások

// Beállítások menü elemei
typedef enum
{
    SETTINGS_MENU_WORLD_SIZE,
    SETTINGS_MENU_DELAY,
    SETTINGS_MENU_SIM_STEPS,
    SETTINGS_MENU_BACK,
    SETTINGS_MENU_ITEM_COUNT
} SettingsMenuItem;

const char *settings_menu_item_names[] = {
    "World size:",
    "Delay:",
    "Sim. Steps:",
    "Vissza"};

void init_display()
{
    // setlocale(LC_ALL, "");
    initscr();
    start_color();
    use_default_colors();
    curs_set(0);
    noecho();
    cbreak();
    keypad(stdscr, TRUE);

    // Színpárok inicializálása
    init_pair(COLOR_PAIR_BORDER, COLOR_WHITE, COLOR_BLACK); // Keret
    init_pair(COLOR_PAIR_PLANT, COLOR_GREEN, -1);           // Növény (zöld)
    init_pair(COLOR_PAIR_HERBIVORE, COLOR_YELLOW, -1);      // Növényevő (sárga)
    init_pair(COLOR_PAIR_CARNIVORE, COLOR_RED, -1);         // Ragadozó (piros)
    init_pair(COLOR_PAIR_INFO, COLOR_CYAN, -1);             // Információs sáv
    init_pair(COLOR_PAIR_EMPTY, COLOR_WHITE, -1);           // Üres mező
    init_pair(COLOR_PAIR_MENU_ITEM, COLOR_WHITE, -1);
    init_pair(COLOR_PAIR_MENU_SELECTED, COLOR_BLACK, COLOR_WHITE); // Kijelölt menüelem
    init_pair(COLOR_PAIR_SETTINGS_VALUE, COLOR_YELLOW, -1);        // Kiemelt szín a beállítás értékének
    init_pair(COLOR_PAIR_SPAWNED_HERBIVORE, COLOR_YELLOW, COLOR_MAGENTA);
    init_pair(COLOR_PAIR_SPAWNED_CARNIVORE, COLOR_RED, COLOR_MAGENTA);
}

void cleanup_display()
{
    endwin();
}

MenuItem display_main_menu()
{
    int selected_item = MENU_START;
    int ch;

    erase();
    mvprintw(LINES / 2 - MENU_ITEM_COUNT - 2, COLS / 2 - strlen("Main menu") / 2, "Main menu");

    while (1)
    {
        for (int i = 0; i < MENU_ITEM_COUNT; ++i)
        {
            if (i == selected_item)
            {
                attron(COLOR_PAIR(COLOR_PAIR_MENU_SELECTED));
            }
            else
            {
                attron(COLOR_PAIR(COLOR_PAIR_MENU_ITEM));
            }
            mvprintw(LINES / 2 - MENU_ITEM_COUNT / 2 + i, COLS / 2 - strlen(menu_item_names[i]) / 2, "%s", menu_item_names[i]);
            if (i == selected_item)
            {
                attroff(COLOR_PAIR(COLOR_PAIR_MENU_SELECTED));
            }
            else
            {
                attroff(COLOR_PAIR(COLOR_PAIR_MENU_ITEM));
            }
        }
        refresh();

        ch = getch();

        switch (ch)
        {
        case KEY_UP:
        case 'w':
        case 'W':
            selected_item--;
            if (selected_item < 0)
                selected_item = MENU_ITEM_COUNT - 1;
            break;
        case KEY_DOWN:
        case 's':
        case 'S':
            selected_item++;
            if (selected_item >= MENU_ITEM_COUNT)
                selected_item = 0;
            break;
        case 10: // Enter
            return (MenuItem)selected_item;
        case 'q': // Kilépés 'q'-val a menüből is
        case 'Q':
            return MENU_EXIT;
        }
    }
}

void display_settings_menu()
{
    int selected_item = SETTINGS_MENU_WORLD_SIZE;
    int ch;
    bool settings_changed = true;

    while (1)
    {
        if (settings_changed)
        {
            erase();
            mvprintw(LINES / 2 - SETTINGS_MENU_ITEM_COUNT - 2, COLS / 2 - 10, "Settings");

            for (int i = 0; i < SETTINGS_MENU_ITEM_COUNT; ++i)
            {
                if (i == selected_item)
                {
                    attron(COLOR_PAIR(COLOR_PAIR_MENU_SELECTED));
                }
                else
                {
                    attron(COLOR_PAIR(COLOR_PAIR_MENU_ITEM));
                }
                mvprintw(LINES / 2 - SETTINGS_MENU_ITEM_COUNT / 2 + i, COLS / 2 - 20, "%s", settings_menu_item_names[i]);
                if (i == selected_item)
                {
                    attroff(COLOR_PAIR(COLOR_PAIR_MENU_SELECTED));
                }
                else
                {
                    attroff(COLOR_PAIR(COLOR_PAIR_MENU_ITEM));
                }

                // Jelenlegi értékek kiírása
                attron(COLOR_PAIR(COLOR_PAIR_SETTINGS_VALUE));
                if (i == SETTINGS_MENU_WORLD_SIZE)
                {
                    mvprintw(LINES / 2 - SETTINGS_MENU_ITEM_COUNT / 2 + i, COLS / 2, "< %s >", world_size_names[current_settings.world_size_choice]);
                }
                else if (i == SETTINGS_MENU_DELAY)
                {
                    mvprintw(LINES / 2 - SETTINGS_MENU_ITEM_COUNT / 2 + i, COLS / 2, "< %s >", delay_names[current_settings.delay_choice]);
                }
                else if (i == SETTINGS_MENU_SIM_STEPS)
                {
                    mvprintw(LINES / 2 - SETTINGS_MENU_ITEM_COUNT / 2 + i, COLS / 2, "< %s >", simulation_steps_names[current_settings.steps_choice]);
                }
                attroff(COLOR_PAIR(COLOR_PAIR_SETTINGS_VALUE));
            }
            refresh();
            settings_changed = false;
        }

        ch = getch();

        switch (ch)
        {
        case KEY_UP:
        case 'w':
        case 'W':
            selected_item = (selected_item - 1 + SETTINGS_MENU_ITEM_COUNT) % SETTINGS_MENU_ITEM_COUNT;
            settings_changed = true;
            break;
        case KEY_DOWN:
        case 's':
        case 'S':
            selected_item = (selected_item + 1) % SETTINGS_MENU_ITEM_COUNT;
            settings_changed = true;
            break;
        case KEY_LEFT:
        case 'a':
        case 'A':
            if (selected_item == SETTINGS_MENU_WORLD_SIZE)
            {
                current_settings.world_size_choice = (current_settings.world_size_choice - 1 + WORLD_SIZE_COUNT) % WORLD_SIZE_COUNT;
                settings_changed = true;
            }
            else if (selected_item == SETTINGS_MENU_DELAY)
            {
                current_settings.delay_choice = (current_settings.delay_choice - 1 + DELAY_COUNT) % DELAY_COUNT;
                settings_changed = true;
            }
            else if (selected_item == SETTINGS_MENU_SIM_STEPS)
            {
                current_settings.steps_choice = (current_settings.steps_choice - 1 + SIMULATION_STEPS_COUNT) % SIMULATION_STEPS_COUNT;
                settings_changed = true;
            }
            break;
        case KEY_RIGHT:
        case 'd':
        case 'D':
            if (selected_item == SETTINGS_MENU_WORLD_SIZE)
            {
                current_settings.world_size_choice = (current_settings.world_size_choice + 1) % WORLD_SIZE_COUNT;
                settings_changed = true;
            }
            else if (selected_item == SETTINGS_MENU_DELAY)
            {
                current_settings.delay_choice = (current_settings.delay_choice + 1) % DELAY_COUNT; // % miatt visszatér a 0-ra ha a DELAY_COUNT-ra ér
                settings_changed = true;
            }
            else if (selected_item == SETTINGS_MENU_SIM_STEPS)
            {
                current_settings.steps_choice = (current_settings.steps_choice + 1) % SIMULATION_STEPS_COUNT;
                settings_changed = true;
            }
            break;
        case 10: // Enter
            if (selected_item == SETTINGS_MENU_BACK)
            {
                return; // Vissza a főmenübe
            }
            break;
        case 'q':
        case 'Q':
            return; // Vissza a főmenübe 'q'-val is
        }
    }
}

// Világ állapotának kirajzolása
void draw_simulation_state(World *world, int step_number, int total_steps, WINDOW *world_display_window)
{
    move(0, 0);
    clrtoeol(); // Törli a sort a kurzortól a végéig a

    attron(COLOR_PAIR(COLOR_PAIR_INFO));
    mvprintw(0, 1, "Simulation step: %d/%d | Entities: %d (Plants: %d, Herbivores: %d, Carnivores: %d) | World size: %dx%d | Exit: 'q'",
             step_number + 1, total_steps, world->entity_count,
             count_entities_by_type(world, PLANT),
             count_entities_by_type(world, HERBIVORE),
             count_entities_by_type(world, CARNIVORE),
             world->width, world->height);
    attroff(COLOR_PAIR(COLOR_PAIR_INFO));
    werase(world_display_window);
    box(world_display_window, 0, 0); // keret, 0, 0 a karakterek

    // Entitások kirajzolása a world->grid alapján
    for (int y = 0; y < world->height; ++y)
    {
        for (int x = 0; x < world->width; ++x)
        {
            Entity *e = world->grid[y][x].entity;
            if (e != NULL && e->energy > 0) // Ellenőrizzük, hogy van-e entitás és él-e
            {
                char display_char = '?';
                int color_pair_id = COLOR_PAIR_EMPTY;

                switch (e->type)
                {
                case PLANT:
                    display_char = 'P';
                    color_pair_id = COLOR_PAIR_PLANT;
                    break;
                case HERBIVORE:
                    display_char = 'H';
                    if (e->just_spawned_by_keypress)
                    {
                        color_pair_id = COLOR_PAIR_SPAWNED_HERBIVORE;
                    }
                    else
                    {
                        color_pair_id = COLOR_PAIR_HERBIVORE;
                    }
                    break;
                case CARNIVORE:
                    display_char = 'C';
                    if (e->just_spawned_by_keypress)
                    {
                        color_pair_id = COLOR_PAIR_SPAWNED_CARNIVORE;
                    }
                    else
                    {
                        color_pair_id = COLOR_PAIR_CARNIVORE;
                    }
                    break;
                default:
                    continue;
                }

                wattron(world_display_window, COLOR_PAIR(color_pair_id));
                // A grid koordinátáit használjuk, +1 eltolással a keret miatt
                mvwaddch(world_display_window, y + 1, x + 1, display_char);
                wattroff(world_display_window, COLOR_PAIR(color_pair_id));

                // A billentyűvel spawnolt entitások kiemelését csak egyetlen lépésig tartjuk meg.
                // Ezután visszaállítjuk a jelzőt, hogy a következő lépésben már normál színnel jelenjenek meg.
                if (e->just_spawned_by_keypress)
                {
                    e->just_spawned_by_keypress = false;
                }
            }
        }
    }

    // A wnoutrefresh nem rajzolja ki azonnal, csak előkészíti a változásokat.
    wnoutrefresh(stdscr);               // Előkészíti a stdscr (infósáv) frissítését.
    wnoutrefresh(world_display_window); // Előkészíti a világ ablakának frissítését.
}

void _commit_entity_to_next_state(World *world, Entity entity_data)
{
    // Ellenőrzés, hogy van-e hely a next_entities tömbben
    if (world->next_entity_count >= MAX_TOTAL_ENTITIES)
    {
        // fprintf(stderr, "FIGYELEM: next_entities tömb megtelt (%d/%d). Entitás (ID: %d) nem lett hozzáadva.\\n",
        //         world->next_entity_count, MAX_TOTAL_ENTITIES, entity_data.id);
        return; // Nincs több hely
    }

    int next_idx;
#pragma omp atomic capture // atomikusan növeljük a next_entity_countot és elmentjük az eredeti értéket next_idx-be
    next_idx = world->next_entity_count++;

    if (next_idx >= MAX_TOTAL_ENTITIES)
    {
        // fprintf(stderr, "HIBA: next_entities kapacitás (%d) túlcsordult commit közben (idx: %d), ID: %d! Entitás elveszett.\\n",
        //         MAX_TOTAL_ENTITIES, next_idx, entity_data.id);
#pragma omp atomic update
        world->next_entity_count--;
        return;
    }

    world->next_entities[next_idx] = entity_data;

    if (is_valid_pos(world, entity_data.position.x, entity_data.position.y))
    {
#pragma omp critical
        {
            if (world->next_grid[entity_data.position.y][entity_data.position.x].entity == NULL)
            {
                world->next_grid[entity_data.position.y][entity_data.position.x].entity = &world->next_entities[next_idx];
            }
        }
    }
}

void simulate_step(World *world, int current_step_number)
{
    if (!world)
        return;

    double step_start_time, step_end_time;
    double carnivore_start_time, carnivore_end_time;
    double herbivore_start_time, herbivore_end_time;
    double plant_start_time, plant_end_time;

    step_start_time = omp_get_wtime();

    // Következő állapot előkészítése:
    // - A next_entity_count nullázása.
    // - A next_grid celláinak kiürítése
    world->next_entity_count = 0;
    for (int i = 0; i < world->height; i++)
    {
        for (int j = 0; j < world->width; j++)
        {
            world->next_grid[i][j].entity = NULL;
        }
    }
    // int estimated_min_capacity = world->entity_count + (world->entity_count / 2) + 100;
    // if (estimated_min_capacity < INITIAL_ENTITY_CAPACITY)
    //     estimated_min_capacity = INITIAL_ENTITY_CAPACITY;
    // _ensure_next_entity_capacity(world, estimated_min_capacity); // Ezt a hívást eltávolítjuk

    // 1. RAGADOZÓK FELDOLGOZÁSA
    // Minden ragadozó entitás feldolgozása párhuzamosan.
    carnivore_start_time = omp_get_wtime();
#pragma omp parallel for shared(world, current_step_number) schedule(dynamic)
    for (int i = 0; i < world->entity_count; i++)
    {
        if (world->entities[i].type != CARNIVORE)
        {
            continue;
        }

        Entity current_entity_original_state = world->entities[i];
        Entity next_entity_prototype = current_entity_original_state;

        next_entity_prototype.age++;
        if (next_entity_prototype.energy > 0)
            next_entity_prototype.energy -= CARNIVORE_ENERGY_DECAY;

        if (next_entity_prototype.energy <= 0 || next_entity_prototype.age > CARNIVORE_MAX_AGE)
        {
            continue; // Entitás elpusztul
        }

        process_carnivore_actions_parallel(world, current_step_number, &world->entities[i], &next_entity_prototype);

        if (next_entity_prototype.energy > 0)
        {
            _commit_entity_to_next_state(world, next_entity_prototype);
        }
    }
    carnivore_end_time = omp_get_wtime();

    // === 2. NÖVÉNYEVŐK FELDOLGOZÁSA ===
    // Minden növényevő entitás feldolgozása párhuzamosan.
    herbivore_start_time = omp_get_wtime();
#pragma omp parallel for shared(world, current_step_number) schedule(dynamic)
    for (int i = 0; i < world->entity_count; i++)
    {
        if (world->entities[i].type != HERBIVORE)
        {
            continue;
        }

        int initial_shared_energy;
#pragma omp atomic read
        initial_shared_energy = world->entities[i].energy;

        if (initial_shared_energy == EATEN_ENERGY_MARKER) // Lehet, hogy egy ragadozó megette
        {
            continue;
        }

        Entity current_entity_original_state = world->entities[i];
        Entity next_entity_prototype = current_entity_original_state;

        next_entity_prototype.age++;
        if (next_entity_prototype.energy > 0)
            next_entity_prototype.energy -= HERBIVORE_ENERGY_DECAY;

        if (next_entity_prototype.energy <= 0 || next_entity_prototype.age > HERBIVORE_MAX_AGE)
        {
            continue; // Entitás elpusztul
        }

        process_herbivore_actions_parallel(world, current_step_number, &world->entities[i], &next_entity_prototype);

        if (next_entity_prototype.energy > 0)
        {
            _commit_entity_to_next_state(world, next_entity_prototype);
        }
    }
    herbivore_end_time = omp_get_wtime();

    // === 3. NÖVÉNYEK FELDOLGOZÁSA ===
    // Minden növény entitás feldolgozása párhuzamosan.
    plant_start_time = omp_get_wtime();
#pragma omp parallel for shared(world, current_step_number) schedule(dynamic)
    for (int i = 0; i < world->entity_count; i++)
    {
        if (world->entities[i].type != PLANT)
        {
            continue;
        }

        int initial_shared_energy;
#pragma omp atomic read
        initial_shared_energy = world->entities[i].energy;

        if (initial_shared_energy == EATEN_ENERGY_MARKER) // Lehet, hogy egy növényevő megette
        {
            continue;
        }

        Entity current_entity_original_state = world->entities[i]; // Növényeknél ezt használjuk a process_plant_actions_parallel-ben
        Entity next_entity_prototype = current_entity_original_state;

        next_entity_prototype.age++;
        if (next_entity_prototype.energy > 0 && next_entity_prototype.energy < PLANT_MAX_ENERGY)
        {
            next_entity_prototype.energy += PLANT_GROWTH_RATE;
            if (next_entity_prototype.energy > PLANT_MAX_ENERGY)
                next_entity_prototype.energy = PLANT_MAX_ENERGY;
        }

        if (next_entity_prototype.energy <= 0 || next_entity_prototype.age > PLANT_MAX_AGE)
        {
            continue; // Entitás elpusztul
        }

        process_plant_actions_parallel(world, current_step_number, &current_entity_original_state, &next_entity_prototype);

        // Növényeknél a 'final_shared_energy_at_commit_time' ellenőrzése nem szükséges itt,
        // mivel más entitás (pl. másik növény) nem "eszi meg" őket a saját feldolgozási fázisukban.
        // Az EATEN_ENERGY_MARKER-t rájuk a növényevők állítják be a *növényevők* feldolgozási fázisában.
        // A fenti `initial_shared_energy == EATEN_ENERGY_MARKER` ellenőrzés kezeli azt az esetet,
        // ha egy növényevő már megette ezt a növényt ebben a `simulate_step`-ben.

        if (next_entity_prototype.energy > 0)
        {
            _commit_entity_to_next_state(world, next_entity_prototype);
        }
    }
    plant_end_time = omp_get_wtime();

    // Állapotváltás (double buffering swap):
    // A `grid` és `next_grid` (cellamátrixok), valamint az `entities` és `next_entities`
    // (entitáslisták) pointereit megcseréljük. Így a `next_` állapotok válnak
    // az aktuális állapottá a következő lépéshez, és a korábbi aktuális állapotok
    // újra felhasználhatók lesznek a következő `next_` állapotok tárolására.
    // Ez hatékony, mert nem igényel nagyméretű adatmozgatást, csak pointercseréket.
    Cell **temp_grid_ptr = world->grid;
    world->grid = world->next_grid;
    world->next_grid = temp_grid_ptr;

    Entity *temp_entities_ptr = world->entities;
    world->entities = world->next_entities;
    world->next_entities = temp_entities_ptr;

    world->entity_count = world->next_entity_count;

    int temp_capacity = world->entity_capacity;
    world->entity_capacity = world->next_entity_capacity;
    world->next_entity_capacity = temp_capacity;

    step_end_time = omp_get_wtime();

    // Időmérési eredmények kiírása az stderr-re
    fprintf(stderr, "Step %d timings: Total: %.4fms, Carnivores: %.4fms, Herbivores: %.4fms, Plants: %.4fms | Threads: %d\n",
            current_step_number,
            (step_end_time - step_start_time) * 1000.0,
            (carnivore_end_time - carnivore_start_time) * 1000.0,
            (herbivore_end_time - herbivore_start_time) * 1000.0,
            (plant_end_time - plant_start_time) * 1000.0,
            omp_get_max_threads()); // Hozzáadva a szálak száma
}

// Segédfüggvények a manuális spawnoláshoz
static bool find_random_empty_cell_for_spawn(World *world, Coordinates *out_pos)
{
    int attempts = 0;
    const int max_attempts = world->width * world->height;
    do
    {
        int r_x = rand() % world->width;
        int r_y = rand() % world->height;
        if (world->grid[r_y][r_x].entity == NULL)
        {
            out_pos->x = r_x;
            out_pos->y = r_y;
            return true;
        }
        attempts++;
    } while (attempts < max_attempts);
    return false; // Nem talált üres cellát
}

static void spawn_entity_manually(World *world, EntityType type, int current_step)
{
    if (!world || world->entity_count >= world->entity_capacity)
    {
        // Opcionálisan logolás: Nincs hely a globális tömbben
        return;
    }

    int max_type_specific_entities = 0;
    int initial_energy = 0;
    int sight_range = 0;
    bool use_visual_spawn_highlight = false;

    switch (type)
    {
    case PLANT:
        max_type_specific_entities = MAX_PLANTS;
        initial_energy = PLANT_MAX_ENERGY / 2;
        sight_range = 0;
        use_visual_spawn_highlight = false;
        break;
    case HERBIVORE:
        max_type_specific_entities = MAX_HERBIVORES;
        initial_energy = HERBIVORE_INITIAL_ENERGY;
        sight_range = HERBIVORE_SIGHT_RANGE;
        use_visual_spawn_highlight = true;
        break;
    case CARNIVORE:
        max_type_specific_entities = MAX_CARNIVORES;
        initial_energy = CARNIVORE_INITIAL_ENERGY;
        sight_range = CARNIVORE_SIGHT_RANGE;
        use_visual_spawn_highlight = true;
        break;
    default:
        // Ismeretlen típus, logolhatnánk
        return;
    }

    if (count_entities_by_type(world, type) >= max_type_specific_entities)
    {
        // Opcionálisan logolás: Elérte a típus-specifikus maximumot
        return;
    }

    Coordinates spawn_pos;
    if (find_random_empty_cell_for_spawn(world, &spawn_pos))
    {
        Entity *new_entity = &world->entities[world->entity_count];
        new_entity->id = world->next_entity_id++;
        new_entity->type = type;
        new_entity->position = spawn_pos;
        new_entity->energy = initial_energy;
        new_entity->age = 0;
        new_entity->sight_range = sight_range;
        new_entity->last_reproduction_step = current_step; // Azért, hogy ne szaporodjon azonnal
        new_entity->last_eating_step = -1;
        new_entity->just_spawned_by_keypress = use_visual_spawn_highlight;

        world->grid[spawn_pos.y][spawn_pos.x].entity = new_entity;
        world->entity_count++;
    }
    else
    {
        // Opcionálisan logolás: Nem talált üres cellát a spawnoláshoz
    }
}

void run_simulation(World *world, int simulation_steps_to_run, long step_delay_ms)
{
    // A timeout() beállítja a getch() viselkedését. Pozitív érték esetén
    // a getch() ennyi milliszekundumig vár inputra, majd ERR-t ad vissza, ha nincs.
    // Ez teszi lehetővé a nem blokkoló inputkezelést a szimulációs ciklusban.
    timeout((int)step_delay_ms);

    // Világ megjelenítésére szolgáló dedikált ncurses ablak létrehozása.
    // Az információs sávnak 1 sort hagyunk fent (stdscr tetején), így az ablak y pozíciója 1.
    // A keret miatt (+2) szélesség és magasság szükséges.
    WINDOW *world_win = newwin(world->height + 2, world->width + 2, 1, (COLS - (world->width + 2)) / 2);
    if (!world_win)
    {
        cleanup_display();
        fprintf(stderr, "Hiba: Nem sikerült létrehozni az ncurses ablakot a szimulációhoz.\n");
        return;
    }

    for (int step = 0; step < simulation_steps_to_run; step++)
    {
        simulate_step(world, step);
        draw_simulation_state(world, step, simulation_steps_to_run, world_win); // Ablak átadása

        doupdate(); // Fizikai képernyő frissítése az összes előkészített változással (stdscr és world_win).

        int ch = getch(); // Nem blokkoló olvasás a timeout miatt.
        if (ch == 'q' || ch == 'Q')
        {
            break; // Kilépés a szimulációs ciklusból.
        }
        // Manuális entitás spawnolás billentyűleütésre.
        else if (ch == 'h' || ch == 'H') // Herbivore spawn
        {
            spawn_entity_manually(world, HERBIVORE, step);
        }
        else if (ch == 'c' || ch == 'C') // Carnivore spawn
        {
            spawn_entity_manually(world, CARNIVORE, step);
        }
        else if (ch == 'p' || ch == 'P') // Növény spawnolása
        {
            spawn_entity_manually(world, PLANT, step);
        }
    }

    delwin(world_win); // A dedikált világ ablak törlése.
    timeout(-1);       // Visszaállítjuk a getch() alapértelmezett blokkoló viselkedését a menühöz.
    clear();           // Teljes képernyő törlése a szimuláció után.
    refresh();         // A törölt képernyő tényleges frissítése.
}

int main(int argc, char *argv[])
{
    // Véletlenszám-generátor inicializálása fix seed-del az ismételhetőséghez
    srand(RANDOM_SEED);

    // A setlocale(LC_ALL, "") kritikus fontosságú az ncurses számára, hogy helyesen
    // kezelje a nem-ASCII karaktereket, mint például az ékezetes betűk a menüben,
    // vagy a box-drawing karakterek a keretekhez (ha használnánk őket).
    // Ezt az init_display()-be mozgattuk, ami jobb helyen van.
    // init_display() hívja meg a setlocale-t.

    init_display(); // Ncurses és locale inicializálása.

    World *world = NULL; // Világ pointer, kezdetben null.

    MenuItem choice;
    do
    {
        choice = display_main_menu();

        switch (choice)
        {
        case MENU_START:
            if (world)
            { // Ha volt már korábbi szimuláció, a hozzá tartozó memóriát fel kell szabadítani.
                free_world(world);
                world = NULL; // Fontos, hogy nullázzuk, jelezve, hogy nincs aktív világ.
            }
            // Világ létrehozása az aktuális beállítások alapján.
            world = create_world(world_size_values[current_settings.world_size_choice].x, world_size_values[current_settings.world_size_choice].y);
            if (!world)
            {
                cleanup_display();
                fprintf(stderr, "Hiba a világ létrehozásakor!\n");
                return 1; // Kritikus hiba, kilépés.
            }
            // Kezdeti entitásokkal való feltöltés a simulation_constants.h-ban definiált értékekkel.
            initialize_world(world, INITIAL_PLANTS, INITIAL_HERBIVORES, INITIAL_CARNIVORES);
            run_simulation(world, simulation_steps_values[current_settings.steps_choice], delay_values_ms[current_settings.delay_choice]);
            // A run_simulation után a képernyő tiszta, a főmenü újra megjelenik.
            break;
        case MENU_SETTINGS:
            display_settings_menu();
            break;
        case MENU_EXIT:
            // Kilépés, a ciklus feltétele megszakítja
            break;
        default: // Nem várt eset
            break;
        }
    } while (choice != MENU_EXIT);

    // A program befejezése előtt, ha létezik még világ (pl. ha nem a 'Kilépés'-sel, hanem 'q'-val léptek ki a szim.-ból),
    // akkor annak memóriáját fel kell szabadítani.
    if (world)
    {
        free_world(world);
    }
    cleanup_display(); // Ncurses lezárása.
    return 0;
}