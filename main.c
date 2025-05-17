#include <stdio.h>
#include <stdlib.h>
#include <time.h> // Szükséges lehet a nanosleep-hez, ha használva van
#include <omp.h>
#include <ncurses.h> // Ncurses header
#include <string.h>  // Szükséges a strcmp-hez (vagy nem, ha enumot használunk)
#include <locale.h>  // Locale beállításához

#include "simulation_constants.h"
#include "datatypes.h"
#include "world_utils.h"      // create_world, initialize_world, free_world, is_valid_pos stb.
#include "entity_actions.h"   // process_X_actions_parallel függvények
#include "simulation_utils.h" // Új include

// Színpárok definíciói
#define COLOR_PAIR_BORDER 1
#define COLOR_PAIR_PLANT 2
#define COLOR_PAIR_HERBIVORE 3
#define COLOR_PAIR_CARNIVORE 4
#define COLOR_PAIR_INFO 5
#define COLOR_PAIR_EMPTY 6 // Üres mezőhöz, ha speciális háttérszínt szeretnénk
#define COLOR_PAIR_MENU_ITEM 7
#define COLOR_PAIR_MENU_SELECTED 8
#define COLOR_PAIR_SETTINGS_VALUE 9 // Kiválasztott beállítás értékének színe

// Főmenüelemek
typedef enum
{
    MENU_START,
    MENU_SETTINGS,
    MENU_EXIT,
    MENU_ITEM_COUNT // Menüelemek száma
} MenuItem;

const char *menu_item_names[] = {
    "Szimuláció Indítása",
    "Beállítások",
    "Kilépés"};

// Beállításokhoz kapcsolódó enumok és struktúrák
typedef enum
{
    SIZE_SMALL,
    SIZE_MEDIUM,
    SIZE_LARGE,
    WORLD_SIZE_COUNT
} WorldSizeSetting;

const char *world_size_names[] = {"Kicsi (30x20)", "Közepes (50x30)", "Nagy (100x35)"};
Coordinates world_size_values[] = {{30, 20}, {50, 30}, {100, 35}};

typedef enum
{
    DELAY_FAST,
    DELAY_NORMAL,
    DELAY_SLOW,
    DELAY_COUNT
} DelaySetting;

const char *delay_names[] = {"Gyors (50ms)", "Normál (150ms)", "Lassú (300ms)"};
long delay_values_ms[] = {50, 150, 300}; // Milliszekundum

typedef enum
{
    STEPS_SHORT,
    STEPS_MEDIUM,
    STEPS_LONG,
    SIMULATION_STEPS_COUNT
} SimulationStepsSetting;

const char *simulation_steps_names[] = {"Rövid (200)", "Közepes (500)", "Hosszú (1000)"};
int simulation_steps_values[] = {200, 500, 1000};

typedef struct
{
    WorldSizeSetting world_size_choice;
    DelaySetting delay_choice;
    SimulationStepsSetting steps_choice;
} SimulationSettings;

// Globális aktuális beállítások
SimulationSettings current_settings = {SIZE_MEDIUM, DELAY_NORMAL, STEPS_MEDIUM};

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
    "Világméret:",
    "Késleltetés:",
    "Szim. Lépésszám:",
    "Vissza a főmenübe"};

// Ncurses inicializálása
void init_display()
{
    initscr();            // ncurses mód indítása
    start_color();        // Színek használatának engedélyezése
    use_default_colors(); // Alapértelmezett terminál színek használata (-1)
    curs_set(0);          // Kurzor elrejtése
    noecho();             // Ne írja ki a bevitt karaktereket
    cbreak();             // Karakterek azonnali olvasása (nem vár Enterre)
    keypad(stdscr, TRUE); // Speciális billentyűk (pl. nyilak) engedélyezése

    // Színpárok inicializálása
    init_pair(COLOR_PAIR_BORDER, COLOR_WHITE, COLOR_BLACK); // Keret
    init_pair(COLOR_PAIR_PLANT, COLOR_GREEN, -1);           // Növény (zöld)
    init_pair(COLOR_PAIR_HERBIVORE, COLOR_YELLOW, -1);      // Növényevő (sárga)
    init_pair(COLOR_PAIR_CARNIVORE, COLOR_RED, -1);         // Ragadozó (piros)
    init_pair(COLOR_PAIR_INFO, COLOR_CYAN, -1);             // Információs sáv
    init_pair(COLOR_PAIR_EMPTY, COLOR_WHITE, -1);           // Üres mező, ha kellene
    init_pair(COLOR_PAIR_MENU_ITEM, COLOR_WHITE, -1);
    init_pair(COLOR_PAIR_MENU_SELECTED, COLOR_BLACK, COLOR_WHITE); // Kijelölt menüelem
    init_pair(COLOR_PAIR_SETTINGS_VALUE, COLOR_YELLOW, -1);        // Kiemelt szín a beállítás értékének
}

// Ncurses lezárása
void cleanup_display()
{
    endwin(); // ncurses mód leállítása
}

// Főmenü megjelenítése és választás kezelése
MenuItem display_main_menu()
{
    int selected_item = MENU_START;
    int ch;

    erase(); // Képernyő törlése a menü előtt
    mvprintw(LINES / 2 - MENU_ITEM_COUNT - 2, COLS / 2 - 15, "Ökoszisztéma Szimulátor Főmenü");

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

        ch = getch(); // Várakozás a billentyűleütésre

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

// Beállítások menü megjelenítése és kezelése
void display_settings_menu()
{
    int selected_item = SETTINGS_MENU_WORLD_SIZE;
    int ch;
    bool settings_changed = true; // Hogy az első belépéskor is kirajzoljon

    while (1)
    {
        if (settings_changed)
        {
            erase();
            mvprintw(LINES / 2 - SETTINGS_MENU_ITEM_COUNT - 2, COLS / 2 - 10, "Beállítások");

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
                current_settings.delay_choice = (current_settings.delay_choice + 1) % DELAY_COUNT;
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
    // stdscr törlése az információs sávnak. Csak az infósáv részét töröljük, ha szükséges.
    // Egy egyszerűbb megoldás, ha az infósáv mindig felülírja magát.
    // erase(); // Ezt eltávolítjuk, hogy ne törölje az egész stdscr-t feleslegesen.

    // Információs sáv (stdscr-re)
    // A stdscr-t célozzuk meg expliciten az mvprintw-vel, ami alapértelmezett.
    // Töröljük az előző infósávot, ha változó hosszúságú lehet (itt fixnek tűnik)
    // vagy használjunk egy dedikált ablakot neki is.
    // Egyelőre feltételezzük, hogy az mvprintw felülírja a sort.
    move(0, 0); // Pozicionálás a sor elejére
    clrtoeol(); // Törli a sort a kurzortól a végéig a stdscr-en

    attron(COLOR_PAIR(COLOR_PAIR_INFO));
    mvprintw(0, 1, "Lépés: %d/%d | Entitások: %d (N: %d, H: %d, R: %d) | Világ: %dx%d | Kilépés: 'q'",
             step_number + 1, total_steps, world->entity_count,
             count_entities_by_type(world, PLANT),
             count_entities_by_type(world, HERBIVORE),
             count_entities_by_type(world, CARNIVORE),
             world->width, world->height);
    attroff(COLOR_PAIR(COLOR_PAIR_INFO));
    // Nincs itt refresh() vagy wrefresh()!

    // Világ ablakának tartalmának törlése és keret újrarajzolása
    werase(world_display_window); // Csak a dedikált ablakot töröljük
    box(world_display_window, 0, 0);

    // Entitások kirajzolása a world_display_window ablakon belül, relatív koordinátákkal
    for (int i = 0; i < world->entity_count; ++i)
    {
        Entity *e = &world->entities[i];
        if (e->id < 0 || e->energy <= 0)
            continue;

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
            color_pair_id = COLOR_PAIR_HERBIVORE;
            break;
        case CARNIVORE:
            display_char = 'C';
            color_pair_id = COLOR_PAIR_CARNIVORE;
            break;
        default:
            continue;
        }

        wattron(world_display_window, COLOR_PAIR(color_pair_id));
        mvwaddch(world_display_window, e->position.y + 1, e->position.x + 1, display_char);
        wattroff(world_display_window, COLOR_PAIR(color_pair_id));
    }

    // Frissítések előkészítése, de még nem a fizikai képernyőre írás
    wnoutrefresh(stdscr);               // Előkészíti a stdscr (infósáv) frissítését
    wnoutrefresh(world_display_window); // Előkészíti a világ ablakának frissítését
    // A tényleges fizikai frissítést a hívó (run_simulation) végzi doupdate()-tel.
}

// Statikus segédfüggvények implementációja (már nem static)
void _ensure_next_entity_capacity(World *world, int min_capacity)
{
    if (world->next_entity_capacity < min_capacity)
    {
        int new_capacity = min_capacity > world->next_entity_capacity * 2 ? min_capacity : world->next_entity_capacity * 2;
        if (new_capacity < INITIAL_ENTITY_CAPACITY) // INITIAL_ENTITY_CAPACITY a simulation_constants.h-ból jön
            new_capacity = INITIAL_ENTITY_CAPACITY;

        Entity *new_array = (Entity *)realloc(world->next_entities, new_capacity * sizeof(Entity));
        if (!new_array)
        {
            perror("Hiba a 'next_entities' tömb átméretezésekor (_ensure_next_entity_capacity)");
            return;
        }
        world->next_entities = new_array;
        world->next_entity_capacity = new_capacity;
    }
}

void _commit_entity_to_next_state(World *world, Entity entity_data)
{
    int next_idx;
#pragma omp atomic capture
    next_idx = world->next_entity_count++;

    if (next_idx >= world->next_entity_capacity)
    {
        fprintf(stderr, "HIBA: next_entities kapacitás (%d) túlcsordult commit közben (idx: %d), ID: %d! Entitás elveszett.\\n",
                world->next_entity_capacity, next_idx, entity_data.id);
#pragma omp atomic update
        world->next_entity_count--;
        return;
    }

    world->next_entities[next_idx] = entity_data;

    if (is_valid_pos(world, entity_data.position.x, entity_data.position.y))
    {
#pragma omp critical(GridWriteNextState)
        {
            if (world->next_grid[entity_data.position.y][entity_data.position.x].entity == NULL)
            {
                world->next_grid[entity_data.position.y][entity_data.position.x].entity = &world->next_entities[next_idx];
            }
            else
            {
                // Ütközés a next_grid-en
            }
        }
    }
}

void simulate_step(World *world, int current_step_number)
{
    if (!world)
        return;

    // Következő állapot előkészítése (rács törlése, entitáslista nullázása)
    world->next_entity_count = 0;
    for (int i = 0; i < world->height; i++)
    {
        for (int j = 0; j < world->width; j++)
        {
            world->next_grid[i][j].entity = NULL;
        }
    }
    // Kapacitás biztosítása (ez maradhat)
    int estimated_min_capacity = world->entity_count + (world->entity_count / 2) + 100;
    if (estimated_min_capacity < INITIAL_ENTITY_CAPACITY)
        estimated_min_capacity = INITIAL_ENTITY_CAPACITY;
    _ensure_next_entity_capacity(world, estimated_min_capacity);

#pragma omp parallel for shared(world, current_step_number) schedule(dynamic)
    for (int i = 0; i < world->entity_count; i++)
    {
        // Ellenőrizzük, hogy az entitást nem ették-e már meg ebben a körben egy másik szálon.
        // Ehhez az eredeti (megosztott) world->entities tömböt kell olvasni.
        int initial_shared_energy;
#pragma omp atomic read
        initial_shared_energy = world->entities[i].energy;

        if (initial_shared_energy == EATEN_ENERGY_MARKER)
        {
            continue; // Ezt az entitást már megették, kihagyjuk a feldolgozását és commit-álását.
        }

        Entity current_entity_original_state = world->entities[i]; // Ez az olvasás most már "biztonságosabb"
        Entity next_entity_prototype = current_entity_original_state;

        // Életkor növelése
        next_entity_prototype.age++;

        // Energia változások (növekedés/csökkenés) típus alapján
        switch (next_entity_prototype.type)
        {
        case PLANT:
            // Növény csak akkor nő, ha van energiája (nem 0 vagy negatív)
            if (next_entity_prototype.energy > 0 && next_entity_prototype.energy < PLANT_MAX_ENERGY)
            {
                next_entity_prototype.energy += PLANT_GROWTH_RATE;
                if (next_entity_prototype.energy > PLANT_MAX_ENERGY)
                    next_entity_prototype.energy = PLANT_MAX_ENERGY;
            }
            break;
        case HERBIVORE:
            if (next_entity_prototype.energy > 0) // Csak akkor csökken, ha pozitív (nem -1)
                next_entity_prototype.energy -= HERBIVORE_ENERGY_DECAY;
            break;
        case CARNIVORE:
            if (next_entity_prototype.energy > 0) // Csak akkor csökken, ha pozitív (nem -1)
                next_entity_prototype.energy -= CARNIVORE_ENERGY_DECAY;
            break;
        case EMPTY: // EMPTY típusú entitás elvileg nem lehet az entities listában
        default:
            continue; // Ismeretlen vagy üres típus, hagyjuk ki
        }

        // Halál ellenőrzése (energia elfogyás, kor)
        // Fontos: Az energiaellenőrzésnek a típus-specifikus energiaváltozások UTÁN kell történnie.
        int max_age_for_type = 0;
        bool known_type_for_age_check = true;
        if (next_entity_prototype.type == PLANT)
            max_age_for_type = PLANT_MAX_AGE;
        else if (next_entity_prototype.type == HERBIVORE)
            max_age_for_type = HERBIVORE_MAX_AGE;
        else if (next_entity_prototype.type == CARNIVORE)
            max_age_for_type = CARNIVORE_MAX_AGE;
        else
            known_type_for_age_check = false; // Ismeretlen típus, ne végezzünk kor ellenőrzést, de az energia <=0 elég

        if (!known_type_for_age_check && next_entity_prototype.energy <= 0)
        { // Ismeretlen típusnál csak energia
            continue;
        }
        if (known_type_for_age_check && (next_entity_prototype.energy <= 0 || next_entity_prototype.age > max_age_for_type))
        {
            continue; // Entitás elpusztul
        }

        // Entitás-specifikus akciók (mozgás, evés, szaporodás)
        // Ezek az akciók most már az EATEN_ENERGY_MARKER-t fogják használni, ha esznek.
        if (next_entity_prototype.type == PLANT)
        {
            process_plant_actions_parallel(world, current_step_number, &current_entity_original_state, &next_entity_prototype);
        }
        else if (next_entity_prototype.type == HERBIVORE)
        {
            // Az current_herbivore_state_in_entities_array paraméternek a world->entities[i]-re kell mutatnia,
            // hogy az AccessEntityEnergyOnEating kritikus szakasz a helyes (megosztott) adatot módosítsa.
            process_herbivore_actions_parallel(world, current_step_number, &world->entities[i], &next_entity_prototype);
        }
        else if (next_entity_prototype.type == CARNIVORE)
        {
            process_carnivore_actions_parallel(world, current_step_number, &world->entities[i], &next_entity_prototype);
        }

        // Entitás commitolása a következő állapotba
        // Újra ellenőrizzük a megosztott energiát, hátha egy akció során megették ezt az entitást.
        int final_shared_energy_at_commit_time;
#pragma omp atomic read
        final_shared_energy_at_commit_time = world->entities[i].energy;

        if (final_shared_energy_at_commit_time == EATEN_ENERGY_MARKER)
        {
            continue; // Akció közben ették meg ezt az entitást (pl. növényevőt egy ragadozó), ne commitoljuk.
        }

        // Csak akkor commitoljuk, ha a prototípusnak még mindig van pozitív energiája
        // és nem jelölték közben EATEN_ENERGY_MARKER-rel a megosztott tömbben.
        // A next_entity_prototype.energy > 0 feltétel fontos, mert az akciók során is csökkenhetett az energia (pl. mozgás, szaporodás).
        if (next_entity_prototype.energy > 0)
        {
            _commit_entity_to_next_state(world, next_entity_prototype);
        }
    }

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
}

void run_simulation(World *world, int simulation_steps_to_run, long step_delay_ms)
{
    timeout((int)step_delay_ms); // Nem blokkoló getch() a szimuláció alatt, a beállított késleltetéssel

    // Világ megjelenítésére szolgáló ablak létrehozása
    // Az információs sávnak 1 sort hagyunk fent, így az ablak y pozíciója 1.
    // A keret miatt +2 szélesség és magasság.
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

        doupdate(); // Fizikai képernyő frissítése az összes előkészített változással

        int ch = getch();
        if (ch == 'q' || ch == 'Q')
        {
            break;
        }
    }

    delwin(world_win); // Ablak törlése a szimuláció végén
    timeout(-1);       // Visszaállítjuk a getch() blokkoló viselkedését
    clear();           // Töröljük a képernyőt a szimuláció után
    refresh();         // Frissítjük a törölt képernyőt
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, ""); // Locale beállítása a wide karakterekhez
    init_display();

    World *world = NULL; // Világot csak akkor hozzuk létre, ha kell

    MenuItem choice;
    do
    {
        choice = display_main_menu();

        switch (choice)
        {
        case MENU_START:
            if (world)
            { // Ha volt előző szimuláció, felszabadítjuk
                free_world(world);
            }
            world = create_world(world_size_values[current_settings.world_size_choice].x, world_size_values[current_settings.world_size_choice].y);
            if (!world)
            {
                cleanup_display();
                fprintf(stderr, "Hiba a világ létrehozásakor!\\n");
                return 1;
            }
            initialize_world(world, 150, 40, 10); // Korábbi num_plants, num_herbivores, num_carnivores értékek
            run_simulation(world, simulation_steps_values[current_settings.steps_choice], delay_values_ms[current_settings.delay_choice]);
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

    if (world)
    {
        free_world(world);
    }
    cleanup_display();
    return 0;
}