#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <omp.h>
#include <mpi.h>
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

#define DEBUG_LOG_LINES 3
#define DEBUG_LOG_MAX_LENGTH 256

#define CMD_START 1
#define CMD_EXIT 0

char debug_log_buffer[DEBUG_LOG_LINES][DEBUG_LOG_MAX_LENGTH];

void init_debug_log()
{
    for (int i = 0; i < DEBUG_LOG_LINES; i++)
    {
        memset(debug_log_buffer[i], 0, DEBUG_LOG_MAX_LENGTH);
    }
}

void add_debug_log(const char *format, ...)
{
    char buffer[DEBUG_LOG_MAX_LENGTH];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, DEBUG_LOG_MAX_LENGTH, format, args);
    va_end(args);

    // Shiftelés felfelé: a legrégebbi törlődik
    for (int i = 0; i < DEBUG_LOG_LINES - 1; i++)
    {
        strncpy(debug_log_buffer[i], debug_log_buffer[i + 1], DEBUG_LOG_MAX_LENGTH);
    }
    strncpy(debug_log_buffer[DEBUG_LOG_LINES - 1], buffer, DEBUG_LOG_MAX_LENGTH);
}

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
    bool debug_mode;
} SimulationSettings;

SimulationSettings current_settings = {SIZE_MEDIUM, DELAY_NORMAL, STEPS_MEDIUM, false}; // alapértelmezett beállítások

// Beállítások menü elemei
typedef enum
{
    SETTINGS_MENU_WORLD_SIZE,
    SETTINGS_MENU_DELAY,
    SETTINGS_MENU_SIM_STEPS,
    SETTINGS_MENU_DEBUG,
    SETTINGS_MENU_BACK,
    SETTINGS_MENU_ITEM_COUNT
} SettingsMenuItem;

const char *settings_menu_item_names[] = {
    "World size:",
    "Delay:",
    "Sim. Steps:",
    "Debug Log:",
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
                else if (i == SETTINGS_MENU_DEBUG)
                {
                    mvprintw(LINES / 2 - SETTINGS_MENU_ITEM_COUNT / 2 + i, COLS / 2, "< %s >", current_settings.debug_mode ? "ON" : "OFF");
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
            else if (selected_item == SETTINGS_MENU_DEBUG)
            {
                current_settings.debug_mode = !current_settings.debug_mode;
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
            else if (selected_item == SETTINGS_MENU_DEBUG)
            {
                current_settings.debug_mode = !current_settings.debug_mode;
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
             world->global_width, world->global_height);
    attroff(COLOR_PAIR(COLOR_PAIR_INFO));
    werase(world_display_window);
    box(world_display_window, 0, 0); // keret, 0, 0 a karakterek

    // Entitások kirajzolása a world->local_grid alapján
    for (int y = 0; y < world->global_height; ++y)
    {
        for (int x = 0; x < world->global_width; ++x)
        {
            Entity *e = world->local_grid[y][x].entity;
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

void draw_debug_window(WINDOW *debug_win)
{
    werase(debug_win);
    box(debug_win, 0, 0);
    mvwprintw(debug_win, 0, 2, " Debug Log ");

    if (current_settings.debug_mode)
    {
        for (int i = 0; i < DEBUG_LOG_LINES; i++)
        {
            mvwprintw(debug_win, i + 1, 1, "%s", debug_log_buffer[i]);
        }
    }
    else
    {
        mvwprintw(debug_win, 1, 1, "Debug mode is OFF. Enable in Settings.");
    }
    wnoutrefresh(debug_win);
}

void _commit_entity_to_next_state(World *world, Entity entity_data)
{
    EntityBuffer *out_top = &world->out_top;
    EntityBuffer *out_bottom = &world->out_bottom;
    // Ellenőrzés, hogy van-e hely a next_entities tömbben
    if (world->next_entity_count >= MAX_TOTAL_ENTITIES)
    {
        return; // Nincs több hely
    }
    int y = entity_data.position.y;
    if(y < world->start_y){
        if(out_top->count < out_top->capacity){
            out_top->entities[out_top->count] = entity_data;
            out_top->count++;
            return;
        }
    }
    else if(y > world->end_y){
        if(out_bottom->count < out_bottom->capacity){
            out_bottom->entities[out_bottom->count] = entity_data;
            out_bottom->count++;
            return;
        }
    }
    else {
        int next_idx;
#pragma omp atomic capture
        next_idx = world->next_entity_count++;

        if(next_idx >= MAX_TOTAL_ENTITIES){
#pragma omp atomic update
            world->next_entity_count--;
            return;
        }

        world->next_entities[next_idx] = entity_data;

        if(is_valid_pos(world, entity_data.position.x, entity_data.position.y)){
            int local_y = get_local_y(world, y);
#pragma omp critical
            {
                if (world->local_next_grid[local_y][entity_data.position.x].entity == NULL)
                    world->local_next_grid[local_y][entity_data.position.x].entity = &world->next_entities[next_idx];
            }
        }
    }
}

void exchange_migrations(World *world){
    EntityBuffer *out_top = &world->out_top;
    EntityBuffer *out_bottom = &world->out_bottom;
    int top_out_count = out_top->count;
    int bottom_out_count = out_bottom->count;
    int top_in_count = 0;
    int bottom_in_count = 0;

    if (world->rank % 2 == 0){
        if (world->bottom_neighbor_rank != MPI_PROC_NULL) {
            MPI_Send(&bottom_out_count, 1, MPI_INT, world->bottom_neighbor_rank, 0, MPI_COMM_WORLD);
        }
        if (world->top_neighbor_rank != MPI_PROC_NULL) {
            MPI_Send(&top_out_count, 1, MPI_INT, world->top_neighbor_rank, 1, MPI_COMM_WORLD);
        }
        if (world->top_neighbor_rank != MPI_PROC_NULL) {
            MPI_Recv(&top_in_count, 1, MPI_INT, world->top_neighbor_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        if (world->bottom_neighbor_rank != MPI_PROC_NULL) {
            MPI_Recv(&bottom_in_count, 1, MPI_INT, world->bottom_neighbor_rank, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    } else {
        // Páratlan rankok: először fogadnak, aztán küldenek
        if (world->top_neighbor_rank != MPI_PROC_NULL) {
            MPI_Recv(&top_in_count, 1, MPI_INT, world->top_neighbor_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        if (world->bottom_neighbor_rank != MPI_PROC_NULL) {
            MPI_Recv(&bottom_in_count, 1, MPI_INT, world->bottom_neighbor_rank, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        if (world->bottom_neighbor_rank != MPI_PROC_NULL) {
            MPI_Send(&bottom_out_count, 1, MPI_INT, world->bottom_neighbor_rank, 0, MPI_COMM_WORLD);
        }
        if (world->top_neighbor_rank != MPI_PROC_NULL) {
            MPI_Send(&top_out_count, 1, MPI_INT, world->top_neighbor_rank, 1, MPI_COMM_WORLD);
        }
    }
    // 3. Buffer kapacitás biztosítása
    if (top_in_count > out_top->capacity) {
        out_top->capacity = top_in_count + 10;
        out_top->entities = (Entity*)realloc(out_top->entities, out_top->capacity * sizeof(Entity));
        if (!out_top->entities) {
            fprintf(stderr, "Rank %d: Hiba a top buffer realloc során!\n", world->rank);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }
    if (bottom_in_count > out_bottom->capacity) {
        out_bottom->capacity = bottom_in_count + 10;
        out_bottom->entities = (Entity*)realloc(out_bottom->entities, out_bottom->capacity * sizeof(Entity));
        if (!out_bottom->entities) {
            fprintf(stderr, "Rank %d: Hiba a bottom buffer realloc során!\n", world->rank);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }
    
    // 4. Entitások cseréje (ugyanaz a deadlock-elkerülő logika)
    // FONTOS: out_top-ba a top szomszéd out_bottom-ját kapjuk (ő lefelé küld)
    //         out_bottom-ba a bottom szomszéd out_top-ját kapjuk (ő felfelé küld)
    if (world->rank % 2 == 0) {
        // Páros rankok: először küldenek
        if (world->bottom_neighbor_rank != MPI_PROC_NULL && bottom_out_count > 0) {
            MPI_Send(out_bottom->entities, bottom_out_count * sizeof(Entity), MPI_BYTE,
                     world->bottom_neighbor_rank, 2, MPI_COMM_WORLD);
        }
        if (world->top_neighbor_rank != MPI_PROC_NULL && top_out_count > 0) {
            MPI_Send(out_top->entities, top_out_count * sizeof(Entity), MPI_BYTE,
                     world->top_neighbor_rank, 3, MPI_COMM_WORLD);
        }
        // Aztán fogadnak
        if (world->top_neighbor_rank != MPI_PROC_NULL && top_in_count > 0) {
            MPI_Recv(out_top->entities, top_in_count * sizeof(Entity), MPI_BYTE,
                     world->top_neighbor_rank, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            out_top->count = top_in_count;
        }
        if (world->bottom_neighbor_rank != MPI_PROC_NULL && bottom_in_count > 0) {
            MPI_Recv(out_bottom->entities, bottom_in_count * sizeof(Entity), MPI_BYTE,
                     world->bottom_neighbor_rank, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            out_bottom->count = bottom_in_count;
        }
    } else {
            // Páratlan rankok: először fogadnak
            if (world->top_neighbor_rank != MPI_PROC_NULL && top_in_count > 0) {
                MPI_Recv(out_top->entities, top_in_count * sizeof(Entity), MPI_BYTE,
                            world->top_neighbor_rank, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                out_top->count = top_in_count;
            }
            if (world->bottom_neighbor_rank != MPI_PROC_NULL && bottom_in_count > 0) {
                MPI_Recv(out_bottom->entities, bottom_in_count * sizeof(Entity), MPI_BYTE,
                            world->bottom_neighbor_rank, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                out_bottom->count = bottom_in_count;
            }
            // Aztán küldenek (a régi darabszámokkal)
            if (world->bottom_neighbor_rank != MPI_PROC_NULL && bottom_out_count > 0) {
                MPI_Send(out_bottom->entities, bottom_out_count * sizeof(Entity), MPI_BYTE,
                            world->bottom_neighbor_rank, 2, MPI_COMM_WORLD);
            }
            if (world->top_neighbor_rank != MPI_PROC_NULL && top_out_count > 0) {
                MPI_Send(out_top->entities, top_out_count * sizeof(Entity), MPI_BYTE,
                            world->top_neighbor_rank, 3, MPI_COMM_WORLD);
            }
        }
    for (int i = 0; i < out_top->count; i++){ //top szomszédtól jövő entitások amik már lokálba esnek
        _commit_entity_to_next_state(world,out_top->entities[i]);
    }
    for (int i = 0; i < out_bottom->count; i++){ 
        _commit_entity_to_next_state(world,out_bottom->entities[i]);
    } 
    out_top->count = 0;
    out_bottom->count = 0;
}

void clear_ghosts(World *world)
{
    int ghost = world->ghost_layer_size;
    int total_rows = world->local_height + 2 * ghost;
    
    // Felső ghost sávok törlése (0 ... ghost-1)
    for (int i = 0; i < ghost; i++)
    {
        for (int j = 0; j < world->global_width; j++)
        {
            world->local_grid[i][j].entity = NULL;
        }
    }
    
    // Alsó ghost sávok törlése (ghost + local_height ... total_rows - 1)
    int bottom_ghost_start = ghost + world->local_height;
    for (int i = bottom_ghost_start; i < total_rows; i++)
    {
        for (int j = 0; j < world->global_width; j++)
        {
            world->local_grid[i][j].entity = NULL;
        }
    }
    
}

void pack_border_entities(World *world){
    EntityBuffer *out_top = &world->out_top;
    EntityBuffer *out_bottom = &world->out_bottom;
    int ghost = world->ghost_layer_size;

    //Felső határ
    out_top->count = 0;
    for (int i = ghost; i < 2 * ghost; i++)
    {
        for (int j = 0; j < world->global_width; j++){
            if (out_top->count < out_top->capacity){
                if (world->local_grid[i][j].entity != NULL){
                    out_top->entities[out_top->count] = *world->local_grid[i][j].entity;
                    out_top->count++;
                }
            }
        }
    }

    //Alsó határ
    out_bottom->count = 0;
    int bottom_start = world->local_height;
    for (int i = bottom_start; i < bottom_start + ghost; i++){
        for (int j = 0; j < world->global_width; j++){
            if(out_bottom->count < out_bottom->capacity){
                if (world->local_grid[i][j].entity != NULL){
                    out_bottom->entities[out_bottom->count] = *world->local_grid[i][j].entity;
                    out_bottom->count++;
                }
            }
        }
    }
}

void unpack_border_entities(World *world){
    EntityBuffer *out_top = &world->out_top;
    EntityBuffer *out_bottom = &world->out_bottom;
    int ghost = world->ghost_layer_size;
    clear_ghosts(world);
    
    // Felső ghost sáv feltöltése (0 ... ghost-1)
    for (int i = 0; i < out_top->count; i++){
        Entity *entity = &out_top->entities[i];
        int x = entity->position.x;
        int y = entity->position.y;
        if(is_valid_pos(world, x, y)){
            int local_y = get_local_y(world, y);
            if (local_y < ghost){
                world->local_grid[local_y][x].entity = entity;
            }
            else{
                printf("ERROR Top Ghost: Rank %d, Entity ID %d, Y=%d, Local Y=%d (Ghost Size=%d)\n", 
                    world->rank, entity->id, y, local_y, ghost);
            }
        }
    }
    
    // Alsó ghost sáv feltöltése (ghost + local_height ... ghost + local_height + ghost - 1)
    int bottom_ghost_start = ghost + world->local_height;
    for (int i = 0; i < out_bottom->count; i++){
        Entity *entity = &out_bottom->entities[i];
        int x = entity->position.x;
        int y = entity->position.y;
        if(is_valid_pos(world, x, y)){
            int local_y = get_local_y(world, y);
            if (local_y >= bottom_ghost_start){
                world->local_grid[local_y][x].entity = entity;
            }
            else{
                printf("ERROR Top Ghost: Rank %d, Entity ID %d, Y=%d, Local Y=%d (Ghost Size=%d)\n", 
                    world->rank, entity->id, y, local_y, ghost);
            }
        }
    }
}

void exchange_ghosts(World *world){
    // 1. Pack: saját határ-entitások összegyűjtése a pufferekbe (KÜLDENDŐ ADAT)
    pack_border_entities(world);
    
    EntityBuffer *out_top = &world->out_top;       // Ebből küldünk, majd ebbe fogadunk (másolással)
    EntityBuffer *out_bottom = &world->out_bottom; // Ebből küldünk, majd ebbe fogadunk
    
    // Fogadó pufferek és darabszámok
    Entity *in_top_entities = NULL;
    Entity *in_bottom_entities = NULL;
    int in_top_count = 0;
    int in_bottom_count = 0;

    // 2. Darabszámok cseréje
    if (world->rank % 2 == 0) {
        // Páros: küld, aztán fogad
        if (world->bottom_neighbor_rank != MPI_PROC_NULL) {
            MPI_Send(&out_bottom->count, 1, MPI_INT, world->bottom_neighbor_rank, 0, MPI_COMM_WORLD);
        }
        if (world->top_neighbor_rank != MPI_PROC_NULL) {
            MPI_Send(&out_top->count, 1, MPI_INT, world->top_neighbor_rank, 1, MPI_COMM_WORLD);
        }
        
        if (world->top_neighbor_rank != MPI_PROC_NULL) {
            MPI_Recv(&in_top_count, 1, MPI_INT, world->top_neighbor_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        if (world->bottom_neighbor_rank != MPI_PROC_NULL) {
            MPI_Recv(&in_bottom_count, 1, MPI_INT, world->bottom_neighbor_rank, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    } else {
        // Páratlan: fogad, aztán küld
        if (world->top_neighbor_rank != MPI_PROC_NULL) {
            MPI_Recv(&in_top_count, 1, MPI_INT, world->top_neighbor_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        if (world->bottom_neighbor_rank != MPI_PROC_NULL) {
            MPI_Recv(&in_bottom_count, 1, MPI_INT, world->bottom_neighbor_rank, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        
        if (world->bottom_neighbor_rank != MPI_PROC_NULL) {
            MPI_Send(&out_bottom->count, 1, MPI_INT, world->bottom_neighbor_rank, 0, MPI_COMM_WORLD);
        }
        if (world->top_neighbor_rank != MPI_PROC_NULL) {
            MPI_Send(&out_top->count, 1, MPI_INT, world->top_neighbor_rank, 1, MPI_COMM_WORLD);
        }
    }
    
    // 3. Fogadó pufferek allokálása
    if (in_top_count > 0) {
        in_top_entities = (Entity*)malloc(in_top_count * sizeof(Entity));
    }
    if (in_bottom_count > 0) {
        in_bottom_entities = (Entity*)malloc(in_bottom_count * sizeof(Entity));
    }

    // 4. Entitások cseréje
    if (world->rank % 2 == 0) {
        if (world->bottom_neighbor_rank != MPI_PROC_NULL && out_bottom->count > 0) {
            MPI_Send(out_bottom->entities, out_bottom->count * sizeof(Entity), MPI_BYTE, world->bottom_neighbor_rank, 2, MPI_COMM_WORLD);
        }
        if (world->top_neighbor_rank != MPI_PROC_NULL && out_top->count > 0) {
            MPI_Send(out_top->entities, out_top->count * sizeof(Entity), MPI_BYTE, world->top_neighbor_rank, 3, MPI_COMM_WORLD);
        }
        
        if (world->top_neighbor_rank != MPI_PROC_NULL && in_top_count > 0) {
            MPI_Recv(in_top_entities, in_top_count * sizeof(Entity), MPI_BYTE, world->top_neighbor_rank, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        if (world->bottom_neighbor_rank != MPI_PROC_NULL && in_bottom_count > 0) {
            MPI_Recv(in_bottom_entities, in_bottom_count * sizeof(Entity), MPI_BYTE, world->bottom_neighbor_rank, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    } else {
        if (world->top_neighbor_rank != MPI_PROC_NULL && in_top_count > 0) {
            MPI_Recv(in_top_entities, in_top_count * sizeof(Entity), MPI_BYTE, world->top_neighbor_rank, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        if (world->bottom_neighbor_rank != MPI_PROC_NULL && in_bottom_count > 0) {
            MPI_Recv(in_bottom_entities, in_bottom_count * sizeof(Entity), MPI_BYTE, world->bottom_neighbor_rank, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        
        if (world->bottom_neighbor_rank != MPI_PROC_NULL && out_bottom->count > 0) {
            MPI_Send(out_bottom->entities, out_bottom->count * sizeof(Entity), MPI_BYTE, world->bottom_neighbor_rank, 2, MPI_COMM_WORLD);
        }
        if (world->top_neighbor_rank != MPI_PROC_NULL && out_top->count > 0) {
            MPI_Send(out_top->entities, out_top->count * sizeof(Entity), MPI_BYTE, world->top_neighbor_rank, 3, MPI_COMM_WORLD);
        }
    }
    
    // 5. Fogadott adatok átmozgatása a perzisztens pufferekbe (out_top / out_bottom)
    // Először megnöveljük a kapacitást ha kell
    if (in_top_count > out_top->capacity) {
        out_top->capacity = in_top_count;
        out_top->entities = (Entity*)realloc(out_top->entities, out_top->capacity * sizeof(Entity));
    }
    // Bemásoljuk
    out_top->count = in_top_count;
    if (in_top_count > 0 && in_top_entities) {
        memcpy(out_top->entities, in_top_entities, in_top_count * sizeof(Entity));
    } else {
        out_top->count = 0; // Ha nem kaptunk semmit, akkor üres
    }
    
    if (in_bottom_count > out_bottom->capacity) {
        out_bottom->capacity = in_bottom_count;
        out_bottom->entities = (Entity*)realloc(out_bottom->entities, out_bottom->capacity * sizeof(Entity));
    }
    out_bottom->count = in_bottom_count;
    if (in_bottom_count > 0 && in_bottom_entities) {
        memcpy(out_bottom->entities, in_bottom_entities, in_bottom_count * sizeof(Entity));
    } else {
        out_bottom->count = 0; // Ha nem kaptunk semmit, akkor üres
    }
    
    // Ideiglenes pufferek felszabadítása
    if (in_top_entities) free(in_top_entities);
    if (in_bottom_entities) free(in_bottom_entities);

    // 6. Unpack: a fogadott entitások (most már az out_... pufferekben) elhelyezése a ghost sávokba
    unpack_border_entities(world);
}

void simulate_step(World *world, int current_step_number)
{
    if (!world)
        return;

    exchange_ghosts(world);
    double step_start_time, step_end_time;
    double carnivore_start_time, carnivore_end_time;
    double herbivore_start_time, herbivore_end_time;
    double plant_start_time, plant_end_time;

    step_start_time = omp_get_wtime();

    // Következő állapot előkészítése:
    // - A next_entity_count nullázása.
    // - A next_grid celláinak kiürítése
    world->next_entity_count = 0;
    int ghost = world->ghost_layer_size;
    int total_rows = world->local_height + 2 *ghost;
    for (int i = 0; i < total_rows; i++)
    {
        for (int j = 0; j < world->global_width; j++)
        {
            world->local_next_grid[i][j].entity = NULL;
        }
    }


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

    exchange_migrations(world);

    // Állapotváltás (double buffering swap):
    // A `grid` és `next_grid` (cellamátrixok), valamint az `entities` és `next_entities`
    // (entitáslisták) pointereit megcseréljük. Így a `next_` állapotok válnak
    // az aktuális állapottá a következő lépéshez, és a korábbi aktuális állapotok
    // újra felhasználhatók lesznek a következő `next_` állapotok tárolására.
    // Ez hatékony, mert nem igényel nagyméretű adatmozgatást, csak pointercseréket.
    Cell **temp_grid_ptr = world->local_grid;
    world->local_grid = world->local_next_grid;
    world->local_next_grid = temp_grid_ptr;

    Entity *temp_entities_ptr = world->entities;
    world->entities = world->next_entities;
    world->next_entities = temp_entities_ptr;

    world->entity_count = world->next_entity_count;

    int temp_capacity = world->entity_capacity;
    world->entity_capacity = world->next_entity_capacity;
    world->next_entity_capacity = temp_capacity;

    step_end_time = omp_get_wtime();

    // Időmérési eredmények kiírása az stderr-re (csak Debug módban)
    if (current_settings.debug_mode)
    {
        add_debug_log("Step %d: T:%.2fms (C:%.2f H:%.2f P:%.2f)",
                      current_step_number,
                      (step_end_time - step_start_time) * 1000.0,
                      (carnivore_end_time - carnivore_start_time) * 1000.0,
                      (herbivore_end_time - herbivore_start_time) * 1000.0,
                      (plant_end_time - plant_start_time) * 1000.0);
    }
}

// Segédfüggvények a manuális spawnoláshoz
static bool find_random_empty_cell_for_spawn(World *world, Coordinates *out_pos)
{
    int attempts = 0;
    const int max_attempts = world->global_width * world->global_height;
    do
    {
        int r_x = rand() % world->global_width;
        int r_y = rand() % world->global_height;
        if (world->local_grid[r_y][r_x].entity == NULL)
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

        world->local_grid[spawn_pos.y][spawn_pos.x].entity = new_entity;
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
    WINDOW *world_win = newwin(world->global_height + 2, world->global_width + 2, 1, (COLS - (world->global_width + 2)) / 2);
    if (!world_win)
    {
        cleanup_display();
        fprintf(stderr, "Hiba: Nem sikerült létrehozni az ncurses ablakot a szimulációhoz.\n");
        return;
    }

    // Debug ablak létrehozása a világ ablaka alatt
    // 3 sor tartalom + 2 sor keret = 5 sor magasság
    int debug_height = DEBUG_LOG_LINES + 2;
    int debug_y = 1 + world->global_height + 2; // world_win kezdete (1) + magassága
    WINDOW *debug_win = newwin(debug_height, world->global_width + 2, debug_y, (COLS - (world->global_width + 2)) / 2);

    init_debug_log();

    for (int step = 0; step < simulation_steps_to_run; step++)
    {
        simulate_step(world, step);
        draw_simulation_state(world, step, simulation_steps_to_run, world_win); // Ablak átadása

        if (debug_win)
        {
            draw_debug_window(debug_win);
        }

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
    if (debug_win)
        delwin(debug_win);
    
    timeout(-1);       // Visszaállítjuk a getch() alapértelmezett blokkoló viselkedését a menühöz.
    clear();           // Teljes képernyő törlése a szimuláció után.
    refresh();         // A törölt képernyő tényleges frissítése.
}

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Véletlenszám-generátor inicializálása fix seed-del az ismételhetőséghez
    srand(RANDOM_SEED + rank);
    
    World *world = NULL; // Világ pointer, kezdetben null.
    if (rank==0){ // Master kezeli a displayt
        init_display(); // Ncurses és locale inicializálása.
        
        
        
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
                    int cmd = CMD_START;
                    MPI_Bcast(&cmd, 1, MPI_INT,0, MPI_COMM_WORLD);
                    int width = world_size_values[current_settings.world_size_choice].x;
                    int height = world_size_values[current_settings.world_size_choice].y;
                    int steps = simulation_steps_values[current_settings.steps_choice];
                    int params[3] = {width, height, steps};
                    MPI_Bcast(params, 3, MPI_INT, 0, MPI_COMM_WORLD);

                    // Világ létrehozása az aktuális beállítások alapján.
                    world = create_world(world_size_values[current_settings.world_size_choice].x, world_size_values[current_settings.world_size_choice].y, rank, size);
                    // world = create_world(world_size_values[current_settings.world_size_choice].x, world_size_values[current_settings.world_size_choice].y);
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
                    cmd = CMD_EXIT;
                    MPI_Bcast(&cmd, 1, MPI_INT, 0, MPI_COMM_WORLD);
                    // Kilépés, a ciklus feltétele megszakítja
                    break;
                default: // Nem várt eset
                    break;
            }
        } while (choice != MENU_EXIT);
    
    } else { // WORKER ÁG
        while (1) {
            int cmd; // itt fogjuk a kapott parancsot tárolni
            MPI_Bcast(&cmd, 1, MPI_INT, 0, MPI_COMM_WORLD);
            if (cmd == CMD_START){
                int params[3];
                MPI_Bcast(params, 3, MPI_INT, 0, MPI_COMM_WORLD);
                int width = params[0];
                int height = params[1];
                int steps = params[2];
                World *world = create_world(width, height, rank, size);
                if (!world){
                    fprintf(stderr, "Hiba a világ létrehozásakor!\n");
                }
                initialize_world(world, INITIAL_PLANTS, INITIAL_HERBIVORES, INITIAL_CARNIVORES);
                run_simulation(world, steps, 0);
            }
            else if (cmd == CMD_EXIT){
                    break;
                }
        }
    }
    // A program befejezése előtt, ha létezik még világ (pl. ha nem a 'Kilépés'-sel, hanem 'q'-val léptek ki a szim.-ból),
    // akkor annak memóriáját fel kell szabadítani.
    if (world)
    {
        free_world(world);
    }
    if (rank==0) cleanup_display(); // Ncurses lezárása.
    MPI_Finalize();
    return 0;
}

