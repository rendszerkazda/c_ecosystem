#include <stdio.h>
#include <stdlib.h> // rand()
#include <omp.h>
#include <stdbool.h>
#include <math.h> // abs() miatt

#include "entity_actions.h"
#include "simulation_constants.h" // Paraméterekhez
#include "world_utils.h"          // Pl. get_random_adjacent_empty_cell, is_valid_pos
#include "simulation_utils.h"

// 8 irányú szomszédságot ellenőriz.
static bool are_positions_adjacent(Coordinates pos1, Coordinates pos2)
{
    int dx = abs(pos1.x - pos2.x);
    int dy = abs(pos1.y - pos2.y);
    // Szomszédosak, ha (dx <= 1 ÉS dy <= 1) ÉS (nem ugyanaz a pont, azaz dx != 0 VAGY dy != 0).
    // Ez lefedi a 8 lehetséges szomszédos mezőt.
    return (dx <= 1 && dy <= 1) && (dx != 0 || dy != 0);
}

// Növények akcióinak feldolgozása
// A növények elsősorban szaporodnak, ha elegendő energiájuk van, letelt a szaporodási cooldown,
// és a valószínűségi feltétel is teljesül.
// Paraméterek:
//  - world: A szimulációs világ pointere.
//  - current_step_number: Az aktuális szimulációs lépés sorszáma.
//  - current_plant_state: Pointer az eredeti növény állapotára a `world->entities` tömbben (csak olvasásra).
//  - next_plant_state_prototype: Pointer a növény következő állapotának prototípusára, amit ez a függvény módosíthat.
void process_plant_actions_parallel(World *world, int current_step_number, const Entity *current_plant_state, Entity *next_plant_state_prototype)
{
    // Szaporodási feltételek ellenőrzése
    if (next_plant_state_prototype->energy >= PLANT_INITIAL_ENERGY &&                                         // Elegendő energia a szaporodáshoz
        (current_step_number - current_plant_state->last_reproduction_step) >= PLANT_REPRODUCTION_COOLDOWN && // Szaporodási cooldown letelt
        ((double)rand() / RAND_MAX) < PLANT_REPRODUCTION_PROBABILITY)                                         // Véletlenszerű esély a szaporodásra
    {
        // Üres szomszédos cella keresése az aktuális rácsállapot alapján
        Coordinates empty_cell = get_random_adjacent_empty_cell(world, current_plant_state->position);

        // Ellenőrizzük, hogy a kapott cella érvényes-e és nem az eredeti pozíció
        if (is_valid_pos(world, empty_cell.x, empty_cell.y) &&
            (empty_cell.x != current_plant_state->position.x || empty_cell.y != current_plant_state->position.y))
        {
            // Új növény csak akkor jön létre, ha a növények száma nem érte el a maximumot
            if (count_entities_by_type(world, PLANT) < MAX_PLANTS)
            {
                Entity new_plant_candidate; // Új növény jelölt

                // Új, egyedi ID generálása atomikusan a globális számlálóból
                int new_id;
#pragma omp atomic capture
                new_id = world->next_entity_id++;
                new_plant_candidate.id = new_id;

                new_plant_candidate.type = PLANT;
                new_plant_candidate.position = empty_cell;
                new_plant_candidate.energy = PLANT_INITIAL_ENERGY; // Kezdeti energia az új növénynek
                new_plant_candidate.age = 0;
                new_plant_candidate.sight_range = 0;                              // Növénynek nincs látótávolsága
                new_plant_candidate.last_reproduction_step = current_step_number; // Az új növény most "született", ez a legutóbbi szaporodása
                new_plant_candidate.last_eating_step = -1;                        // Növény nem eszik
                new_plant_candidate.just_spawned_by_keypress = false;

                _commit_entity_to_next_state(world, new_plant_candidate);

                // Az eredeti (szülő) növény utolsó szaporodási idejének frissítése a következő állapotban
                next_plant_state_prototype->last_reproduction_step = current_step_number;
            }
        }
    }
}

// Növényevők akcióinak feldolgozása
// Az akciók sorrendje és prioritása a következő:
// 0. Kritikus evés: Ha az energia kritikusan alacsony, megpróbál enni egy szomszédos növényt
// 1. Mozgás: Ha nem volt kritikus evés, megpróbál elmozdulni (növény felé vagy véletlenszerűen)
// 2. Normál evés: Mozgás után (vagy ha nem mozgott és nem volt kritikus evés) megpróbál enni egy szomszédos növényt az új pozícióján
// 3. Szaporodás: Ha nem evett (sem kritikusan, sem normálisan) és a feltételek adottak, megpróbál szaporodni
// Az `action_taken_this_step` változó tárolja, hogy melyik fő akció történt meg
// Paraméterek:
//  - world
//  - current_step_number: Az aktuális szimulációs lépés sorszáma.
//  - current_herbivore_state_in_entities_array: Pointer az eredeti növényevő állapotára a `world->entities` tömbben; eredeti pozíció lekérdezése
//  - next_herbivore_state_prototype: Pointer a növényevő következő állapotának prototípusára, amit ez a függvény módosít
void process_herbivore_actions_parallel(World *world, int current_step_number, Entity *current_herbivore_state_in_entities_array, Entity *next_herbivore_state_prototype)
{
    int action_taken_this_step = 0; // 0: semmi, 1: kritikus evés, 2: mozgás, 3: normál evés, 4: szaporodás

    // 0. Elsődleges ellenőrzés: Kritikus energia szintű evés
    if (next_herbivore_state_prototype->energy < HERBIVORE_CRITICAL_ENERGY_THRESHOLD)
    {
        // Célpont keresése a látótávolságon belül.
        Entity *target_plant = find_target_in_range(world, current_herbivore_state_in_entities_array->position, next_herbivore_state_prototype->sight_range, PLANT);
        // Evés csak akkor, ha a célpont közvetlenül szomszédos ÉS a saját területünkön van (MPI szabály).
        // Ghost entitást nem ehetünk meg távolról, mert azt a szomszéd is birtokolja.
        if (target_plant && are_positions_adjacent(current_herbivore_state_in_entities_array->position, target_plant->position) &&
            is_local(world, target_plant->position.y))
        {
            bool successfully_ate = false;
            // Kritikus szakasz a célpont energiájának módosításához, megelőzve a versenyhelyzeteket,
            // ha több növényevő is ugyanazt a növényt próbálná megenni egyszerre.
            // Az `EATEN_ENERGY_MARKER` jelzi, hogy ezt a növényt már megették ebben a lépésben.
#pragma omp critical // mivel több növényevő is ugyanazt a növényt probalhatja megenni
            {
                if (target_plant->energy > 0) // Ellenőrizzük, hogy a növény még ehető-e (van energiája).
                {
                    target_plant->energy = EATEN_ENERGY_MARKER; // Jelöljük megevettként.
                    successfully_ate = true;
                }
            }
            if (successfully_ate)
            {
                // next_herbivore_state_prototype->position = target_plant->position; // Ide lép az evés után
                next_herbivore_state_prototype->position = current_herbivore_state_in_entities_array->position;
                next_herbivore_state_prototype->energy += HERBIVORE_ENERGY_FROM_PLANT;
                if (next_herbivore_state_prototype->energy > HERBIVORE_MAX_ENERGY)
                {
                    next_herbivore_state_prototype->energy = HERBIVORE_MAX_ENERGY;
                }
                next_herbivore_state_prototype->last_eating_step = current_step_number;
                action_taken_this_step = 1; // Kritikus evés
            }
        }
    }

    // 1. Mozgás (ha nem történt kritikus evés)
    if (!action_taken_this_step &&
        next_herbivore_state_prototype->energy > HERBIVORE_MOVE_COST)
    {
        Coordinates old_pos = current_herbivore_state_in_entities_array->position;
        Coordinates new_pos = old_pos;

        Entity *target_plant_for_move = find_target_in_range(world, old_pos, next_herbivore_state_prototype->sight_range, PLANT);

        if (target_plant_for_move)
        {
            new_pos = get_step_towards_target(world, old_pos, target_plant_for_move->position);
        }
        else
        {
            new_pos = get_random_adjacent_empty_cell(world, old_pos);
        }

        if (is_valid_pos(world, new_pos.x, new_pos.y) && (new_pos.x != old_pos.x || new_pos.y != old_pos.y))
        {
            next_herbivore_state_prototype->position = new_pos;
            next_herbivore_state_prototype->energy -= HERBIVORE_MOVE_COST;
            action_taken_this_step = 2; // Speciális érték, hogy tudjuk, mozgás történt
        }
    }

    // 2. Táplálkozás (normál), ha nem történt kritikus evés
    if (action_taken_this_step != 1 && action_taken_this_step != 2) // Ha nem volt kritikus evés és nem volt mozgás
    {
        // Célpont keresése az aktuális (next_herbivore_state_prototype->position) pozíció körül.
        Entity *target_plant_for_eat = find_target_in_range(world, next_herbivore_state_prototype->position, next_herbivore_state_prototype->sight_range, PLANT);
        if (target_plant_for_eat && are_positions_adjacent(next_herbivore_state_prototype->position, target_plant_for_eat->position) &&
            is_local(world, target_plant_for_eat->position.y))
        {
            bool successfully_ate = false;
            // Kritikus szakasz a célpont energiájának módosításához.
#pragma omp critical
            {
                // Fontos ellenőrizni, hogy a célpont (target_plant_for_eat) még mindig létezik és ehető-e.
                // A find_target_in_range a world->entities alapján keres, de a target_plant_for_eat->energy értéke
                // frissülhetett más szálak által (ezért a kritikus szakasz).
                if (target_plant_for_eat->energy > 0)
                {
                    target_plant_for_eat->energy = EATEN_ENERGY_MARKER;
                    successfully_ate = true;
                }
            }
            if (successfully_ate)
            {
                // next_herbivore_state_prototype->position = target_plant_for_eat->position; // Ide lép az evés után
                next_herbivore_state_prototype->position = current_herbivore_state_in_entities_array->position;
                next_herbivore_state_prototype->energy += HERBIVORE_ENERGY_FROM_PLANT;
                if (next_herbivore_state_prototype->energy > HERBIVORE_MAX_ENERGY)
                {
                    next_herbivore_state_prototype->energy = HERBIVORE_MAX_ENERGY;
                }
                next_herbivore_state_prototype->last_eating_step = current_step_number;
                action_taken_this_step = 3; // Akció megtörtént (normál evés)
            }
        }
    }

    // 3. Szaporodás megpróbálása (ha nem történt kritikus evés, és nem történt normál evés)
    // Csak akkor szaporodik, ha van elég energiája, letelt a cooldown, és marad elég energia a túléléshez.
    if (action_taken_this_step != 1 && action_taken_this_step != 3 &&
        next_herbivore_state_prototype->energy >= HERBIVORE_REPRODUCTION_THRESHOLD &&
        (current_step_number - current_herbivore_state_in_entities_array->last_reproduction_step) >= HERBIVORE_REPRODUCTION_COOLDOWN &&
        (next_herbivore_state_prototype->energy - HERBIVORE_REPRODUCTION_COST) > HERBIVORE_MOVE_COST) // Maradjon elég energia
    {
        Coordinates empty_cell = get_random_adjacent_empty_cell(world, next_herbivore_state_prototype->position);
        if (is_valid_pos(world, empty_cell.x, empty_cell.y) &&
            (empty_cell.x != next_herbivore_state_prototype->position.x || empty_cell.y != next_herbivore_state_prototype->position.y))
        {
            // Új növényevő csak akkor jön létre, ha a növényevők száma nem érte el a maximumot.
            if (count_entities_by_type(world, HERBIVORE) < MAX_HERBIVORES)
            {
                next_herbivore_state_prototype->energy -= HERBIVORE_REPRODUCTION_COST;
                next_herbivore_state_prototype->last_reproduction_step = current_step_number;

                Entity new_herbivore_candidate;
                int new_id;
#pragma omp atomic capture
                new_id = world->next_entity_id++;
                new_herbivore_candidate.id = new_id;
                new_herbivore_candidate.type = HERBIVORE;
                new_herbivore_candidate.position = empty_cell;
                new_herbivore_candidate.energy = HERBIVORE_INITIAL_ENERGY;
                new_herbivore_candidate.age = 0;
                new_herbivore_candidate.sight_range = HERBIVORE_SIGHT_RANGE;
                new_herbivore_candidate.last_reproduction_step = current_step_number; // Újszülött most szaporodott "először"
                new_herbivore_candidate.last_eating_step = -1;
                new_herbivore_candidate.just_spawned_by_keypress = false;
                _commit_entity_to_next_state(world, new_herbivore_candidate);

                action_taken_this_step = 4; // Akció megtörtént (szaporodás)
            }
        }
    }
}

// Ragadozók akcióinak feldolgozása
// Az akciók sorrendje és prioritása hasonló a növényevőkéhez:
// 0. Kritikus evés: Ha az energia kritikusan alacsony, megpróbál enni egy szomszédos növényevőt
// 1. Mozgás: Ha nem volt kritikus evés, megpróbál elmozdulni (növényevő felé vagy véletlenszerűen)
// 2. Normál evés: Mozgás után (vagy ha nem mozgott és nem volt kritikus evés) megpróbál enni egy szomszédos növényevőt
// 3. Szaporodás: Ha nem evett és a feltételek adottak, megpróbál szaporodni
// Paraméterek:
//  - world
//  - current_step_number: Az aktuális szimulációs lépés sorszáma.
//  - current_carnivore_state_in_entities_array: Pointer az eredeti ragadozó állapotára a world->entities tömbben
//  - next_carnivore_state_prototype: Pointer a ragadozó következő állapotának prototípusára
void process_carnivore_actions_parallel(World *world, int current_step_number, Entity *current_carnivore_state_in_entities_array, Entity *next_carnivore_state_prototype)
{
    int action_taken_this_step = 0; // 0: semmi, 1: kritikus evés, 2: mozgás, 3: normál evés, 4: szaporodás

    // 0. Elsődleges ellenőrzés: Kritikus energia szintű evés
    if (next_carnivore_state_prototype->energy < CARNIVORE_CRITICAL_ENERGY_THRESHOLD)
    {
        // Célpont (növényevő) keresése a látótávolságon belül, az eredeti pozíció alapján
        Entity *target_herbivore = find_target_in_range(world, current_carnivore_state_in_entities_array->position, next_carnivore_state_prototype->sight_range, HERBIVORE);
        // Evés csak akkor, ha a célpont közvetlenül szomszédos ÉS a saját területünkön van
        if (target_herbivore && are_positions_adjacent(current_carnivore_state_in_entities_array->position, target_herbivore->position) &&
            is_local(world, target_herbivore->position.y))
        {
            bool successfully_ate_herbivore = false;
            // Kritikus szakasz a célpont (növényevő) energiájának módosítására
#pragma omp critical
            {
                if (target_herbivore->energy > 0) // Ellenőrizzük, hogy a növényevő még ehető-e
                {
                    target_herbivore->energy = EATEN_ENERGY_MARKER; // Jelöljük megevettként
                    successfully_ate_herbivore = true;
                }
            }
            if (successfully_ate_herbivore)
            {
                // next_carnivore_state_prototype->position = target_herbivore->position;
                next_carnivore_state_prototype->position = current_carnivore_state_in_entities_array->position;
                next_carnivore_state_prototype->energy += CARNIVORE_ENERGY_FROM_HERBIVORE;
                if (next_carnivore_state_prototype->energy > CARNIVORE_MAX_ENERGY)
                {
                    next_carnivore_state_prototype->energy = CARNIVORE_MAX_ENERGY;
                }
                next_carnivore_state_prototype->last_eating_step = current_step_number;
                action_taken_this_step = 1; // Kritikus evés
            }
        }
    }

    // 1. Mozgás (ha nem történt kritikus evés)
    // Célpont (növényevő) keresése a látótávolságon belül, és afelé lépés
    // Ha nincs célpont, véletlenszerű mozgás
    if (!action_taken_this_step &&
        next_carnivore_state_prototype->energy > CARNIVORE_MOVE_COST)
    {
        Coordinates old_pos = current_carnivore_state_in_entities_array->position;
        Coordinates new_pos = old_pos;

        Entity *target_herb_for_move = find_target_in_range(world, old_pos, next_carnivore_state_prototype->sight_range, HERBIVORE);

        if (target_herb_for_move)
        {
            new_pos = get_step_towards_target(world, old_pos, target_herb_for_move->position);
        }
        else
        {
            new_pos = get_random_adjacent_empty_cell(world, old_pos);
        }

        if (is_valid_pos(world, new_pos.x, new_pos.y) && (new_pos.x != old_pos.x || new_pos.y != old_pos.y))
        {
            next_carnivore_state_prototype->position = new_pos;
            next_carnivore_state_prototype->energy -= CARNIVORE_MOVE_COST;
            action_taken_this_step = 2; // Mozgás történt
        }
    }

    // 2. Táplálkozás (normál), ha nem történt kritikus evés
    if (action_taken_this_step != 1)
    {
        // Célpont keresése az aktuális (next_carnivore_state_prototype->position) pozíció körül
        Entity *target_herbivore_for_eat = find_target_in_range(world, next_carnivore_state_prototype->position, next_carnivore_state_prototype->sight_range, HERBIVORE);
        if (target_herbivore_for_eat && are_positions_adjacent(next_carnivore_state_prototype->position, target_herbivore_for_eat->position) &&
            is_local(world, target_herbivore_for_eat->position.y))
        {
            bool successfully_ate_herbivore = false;
            // Kritikus szakasz a célpont energiájának módosításához.
#pragma omp critical
            {
                if (target_herbivore_for_eat->energy > 0)
                {
                    target_herbivore_for_eat->energy = EATEN_ENERGY_MARKER;
                    successfully_ate_herbivore = true;
                }
            }
            if (successfully_ate_herbivore)
            {
                next_carnivore_state_prototype->position = target_herbivore_for_eat->position; // Ide lép az evés után
                // next_carnivore_state_prototype->position = current_carnivore_state_in_entities_array->position;
                next_carnivore_state_prototype->energy += CARNIVORE_ENERGY_FROM_HERBIVORE;
                if (next_carnivore_state_prototype->energy > CARNIVORE_MAX_ENERGY)
                {
                    next_carnivore_state_prototype->energy = CARNIVORE_MAX_ENERGY;
                }
                next_carnivore_state_prototype->last_eating_step = current_step_number;
                action_taken_this_step = 3; // Normál evés
            }
        }
    }

    // 3. Szaporodás megpróbálása (ha nem történt sem kritikus, sem normál evés)
    if (action_taken_this_step != 1 && action_taken_this_step != 3 &&
        next_carnivore_state_prototype->energy >= CARNIVORE_REPRODUCTION_THRESHOLD &&
        (current_step_number - current_carnivore_state_in_entities_array->last_reproduction_step) >= CARNIVORE_REPRODUCTION_COOLDOWN &&
        (next_carnivore_state_prototype->energy - CARNIVORE_REPRODUCTION_COST) > CARNIVORE_MOVE_COST) // Maradjon elég energia
    {
        Coordinates empty_cell = get_random_adjacent_empty_cell(world, next_carnivore_state_prototype->position);
        if (is_valid_pos(world, empty_cell.x, empty_cell.y) &&
            (empty_cell.x != next_carnivore_state_prototype->position.x || empty_cell.y != next_carnivore_state_prototype->position.y))
        {
            // Új ragadozó csak akkor jön létre, ha a ragadozók száma nem érte el a maximumot
            if (count_entities_by_type(world, CARNIVORE) < MAX_CARNIVORES)
            {
                next_carnivore_state_prototype->energy -= CARNIVORE_REPRODUCTION_COST;
                next_carnivore_state_prototype->last_reproduction_step = current_step_number;

                Entity new_carnivore_candidate;
                int new_id;
#pragma omp atomic capture
                new_id = world->next_entity_id++;
                new_carnivore_candidate.id = new_id;
                new_carnivore_candidate.type = CARNIVORE;
                new_carnivore_candidate.position = empty_cell;
                new_carnivore_candidate.energy = CARNIVORE_INITIAL_ENERGY;
                new_carnivore_candidate.age = 0;
                new_carnivore_candidate.sight_range = CARNIVORE_SIGHT_RANGE;
                new_carnivore_candidate.last_reproduction_step = current_step_number;
                new_carnivore_candidate.last_eating_step = -1;
                new_carnivore_candidate.just_spawned_by_keypress = false;
                _commit_entity_to_next_state(world, new_carnivore_candidate);

                action_taken_this_step = 4; // Szaporodás
            }
        }
    }
}

// Grid-alapú keresés a hatékonyság és a ghost cellák elérése érdekében
Entity *find_target_in_range(World *world, Coordinates center, int range, EntityType target_type)
{
    if (!world || range < 0) return NULL;

    Entity *closest_target = NULL;
    int min_manhattan_dist = range + 1;

    for (int dy = -range; dy <= range; dy++) {
        for (int dx = -range; dx <= range; dx++) {
            if (abs(dx) + abs(dy) > range) continue;
            
            int nx = center.x + dx;
            int ny = center.y + dy;
            
            if (is_valid_pos(world, nx, ny)) {
                int local_y = get_local_y(world, ny);
                Entity *potential_target = world->local_grid[local_y][nx].entity;

                if (potential_target && potential_target->type == target_type &&
            potential_target->energy > 0 && potential_target->energy != EATEN_ENERGY_MARKER)
        {
                    int dist = abs(dx) + abs(dy);
                    if (dist < min_manhattan_dist) {
                        min_manhattan_dist = dist;
                    closest_target = potential_target;
                    }
                }
            }
        }
    }
    return closest_target;
}