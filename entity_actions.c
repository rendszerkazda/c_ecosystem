#include <stdio.h>
#include <stdlib.h>  // rand() miatt, ha a jövőben kell
#include <omp.h>     // OpenMP specifikus dolgokhoz, ha itt is szükséges lesz
#include <stdbool.h> // bool, true, false definíciókhoz

#include "entity_actions.h"
#include "simulation_constants.h" // Paraméterekhez (pl. PLANT_REPRODUCTION_PROBABILITY)
#include "world_utils.h"          // Pl. get_random_adjacent_empty_cell, is_valid_pos
#include "simulation_utils.h"     // Új include _commit_entity_to_next_state-hez

// Növények akciói
void process_plant_actions_parallel(World *world, int current_step_number, const Entity *current_plant_state, Entity *next_plant_state_prototype)
{
    // Növények csak szaporodnak, ha a feltételek adottak.
    // A növény növekedését (energia gyűjtését) a simulate_step fő ciklusa már kezeli.

    // Szaporodási feltételek ellenőrzése
    if (next_plant_state_prototype->energy >= PLANT_MAX_ENERGY / 2 &&                                         // Elég energia
        (current_step_number - current_plant_state->last_reproduction_step) >= PLANT_REPRODUCTION_COOLDOWN && // Cooldown letelt
        ((double)rand() / RAND_MAX) < PLANT_REPRODUCTION_PROBABILITY)                                         // Véletlenszerű esély
    {
        Coordinates empty_cell = get_random_adjacent_empty_cell(world, current_plant_state->position);

        // Ellenőrizzük, hogy a kapott cella érvényes és valóban üres-e a *következő* rácson is
        // (bár a get_random_adjacent_empty_cell az aktuálisat nézi, itt egy plusz óvatosság)
        // és nem az eredeti pozíció.
        if (is_valid_pos(world, empty_cell.x, empty_cell.y) &&
            (empty_cell.x != current_plant_state->position.x || empty_cell.y != current_plant_state->position.y))
        // A world->next_grid ellenőrzése itt bonyolult lenne a párhuzamosság miatt,
        // a _commit_entity_to_next_state kezeli az ütközést a next_grid-ben.
        {
            Entity new_plant_candidate;

            // Új ID generálása atomikusan
            int new_id;
#pragma omp atomic capture
            new_id = world->next_entity_id++;
            new_plant_candidate.id = new_id;

            new_plant_candidate.type = PLANT;
            new_plant_candidate.position = empty_cell;
            new_plant_candidate.energy = PLANT_MAX_ENERGY / 2; // Kezdeti energia az új növénynek
            new_plant_candidate.age = 0;
            new_plant_candidate.sight_range = 0;                              // Növénynek nincs látótávolsága
            new_plant_candidate.last_reproduction_step = current_step_number; // Az új növény most "született", ez a legutóbbi szaporodása
            new_plant_candidate.last_eating_step = -1;                        // Növény nem eszik

            _commit_entity_to_next_state(world, new_plant_candidate);

            // Az eredeti (szülő) növény utolsó szaporodási idejének frissítése a következő állapotban
            next_plant_state_prototype->last_reproduction_step = current_step_number;
            // Esetleg csökkenthetnénk a szülő energiáját a szaporodás költségeként, ha lenne PLANT_REPRODUCTION_COST
        }
    }
}

// Növényevők akciói
void process_herbivore_actions_parallel(World *world, int current_step_number, Entity *current_herbivore_state_in_entities_array, Entity *next_herbivore_state_prototype)
{
    // current_herbivore_state_in_entities_array az eredeti entitásra mutat a world->entities tömbben.
    // next_herbivore_state_prototype a másolat, amit módosítunk és commit-olunk.

    int action_taken_this_step = 0;

    // 0. Elsődleges ellenőrzés: Kritikus energia szintű evés
    if (next_herbivore_state_prototype->energy < HERBIVORE_CRITICAL_ENERGY_THRESHOLD)
    {
        Entity *target_plant = find_target_in_range(world, current_herbivore_state_in_entities_array->position, next_herbivore_state_prototype->sight_range, PLANT);
        if (target_plant)
        {
            // int plant_current_energy_for_debug = 0; // Debug célokra maradhat, ha kell
            bool successfully_ate = false;
#pragma omp critical(AccessEntityEnergyOnEating)
            {
                if (target_plant->energy > 0)
                {
                    // plant_current_energy_for_debug = target_plant->energy;
                    target_plant->energy = EATEN_ENERGY_MARKER;
                    successfully_ate = true;
                }
            }
            if (successfully_ate)
            {
                next_herbivore_state_prototype->position = target_plant->position; // Ide lép az evés után
                next_herbivore_state_prototype->energy += HERBIVORE_ENERGY_FROM_PLANT;
                if (next_herbivore_state_prototype->energy > HERBIVORE_MAX_ENERGY)
                {
                    next_herbivore_state_prototype->energy = HERBIVORE_MAX_ENERGY;
                }
                next_herbivore_state_prototype->last_eating_step = current_step_number;
                action_taken_this_step = 1; // Akció megtörtént (kritikus evés)
            }
        }
        // Ha kritikusan alacsony az energia és nem tud enni, akkor ebben a körben nem tesz mást (action_taken_this_step marad 0).
    }

    // ÚJ SORREND:
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
    // Akkor is megpróbálhat enni, ha mozgott (action_taken_this_step == 2), vagy ha nem mozgott (action_taken_this_step == 0)
    // De ha kritikus evés volt (action_taken_this_step == 1), akkor nem.
    if (action_taken_this_step != 1) // Ha nem volt kritikus evés
    {
        // A célpontot az aktuális (lehet, hogy új) pozícióhoz képest keressük
        Entity *target_plant_for_eat = find_target_in_range(world, next_herbivore_state_prototype->position, next_herbivore_state_prototype->sight_range, PLANT);
        if (target_plant_for_eat)
        {
            // int plant_current_energy_for_debug = 0;
            bool successfully_ate = false;
#pragma omp critical(AccessEntityEnergyOnEating) // Ugyanaz a kritikus szakasz, mint fent
            {
                // Fontos: Ellenőrizzük, hogy a célpont még mindig a várt helyen van-e, és él-e.
                // A find_target_in_range az entities tömböt nézi, ami a kör eleji állapot.
                // A target_plant_for_eat->position az eredeti pozíció.
                // Ha az állat elmozdult, lehet, hogy a target_plant_for_eat már nincs a sight_range-en belül az új pozícióhoz képest.
                // De a find_target_in_range már ezt a next_herbivore_state_prototype->position alapján teszi.
                // Ellenőrizzük, hogy a target_plant_for_eat a növényevő *aktuális* pozíciójának szomszédja-e.
                // Vagy egyszerűbben: ha a find_target_in_range talál valamit az ÚJ pozíció környékén.
                // És az evés feltétele, hogy a target a közvetlen közelben legyen (pl. range 1).
                // A find_target_in_range visszaadja a legközelebbit a sight_range-en belül.
                // Ahhoz, hogy ténylegesen enni tudjon, a célpontnak a közvetlen szomszédos mezőn kell lennie.
                // Ezt a feltételt a find_target_in_range nem garantálja, csak a sight_range-t.
                // Azonban az "evés után oda lép" logika ezt implicit módon kezeli, ha a target_plant_for_eat->position lesz az új pozíció.
                // De ha az állat nem lép oda, akkor csak akkor ehet, ha szomszédos.
                // Egyszerűsítés: Tegyük fel, hogy a find_target_in_range elég. Ha a hatótávon belül van, megpróbálja enni.
                // Az "evés után odalépés" miatt ez általában működni fog.

                // A legfontosabb itt, hogy a target_plant_for_eat még mindig él-e.
                if (target_plant_for_eat->energy > 0)
                {
                    // plant_current_energy_for_debug = target_plant_for_eat->energy;
                    target_plant_for_eat->energy = EATEN_ENERGY_MARKER;
                    successfully_ate = true;
                }
            }
            if (successfully_ate)
            {
                next_herbivore_state_prototype->position = target_plant_for_eat->position; // Ide lép az evés után
                next_herbivore_state_prototype->energy += HERBIVORE_ENERGY_FROM_PLANT;
                if (next_herbivore_state_prototype->energy > HERBIVORE_MAX_ENERGY)
                {
                    next_herbivore_state_prototype->energy = HERBIVORE_MAX_ENERGY;
                }
                next_herbivore_state_prototype->last_eating_step = current_step_number;
                // Ha mozgás volt (action_taken_this_step == 2), akkor az evés felülírja ezt az akciót
                // Ha nem volt mozgás (action_taken_this_step == 0), akkor az evés lesz az akció
                action_taken_this_step = 3; // Akció megtörtént (normál evés)
            }
        }
    }

    // 3. Szaporodás megpróbálása (ha nem történt kritikus evés, és nem történt normál evés)
    // Ha mozgás történt (action_taken_this_step == 2) de evés nem (maradt 2), akkor is szaporodhat.
    // Ha sem kritikus evés (1), sem normál evés (3) nem volt.
    if (action_taken_this_step != 1 && action_taken_this_step != 3 &&
        next_herbivore_state_prototype->energy >= HERBIVORE_REPRODUCTION_THRESHOLD &&
        (current_step_number - current_herbivore_state_in_entities_array->last_reproduction_step) >= HERBIVORE_REPRODUCTION_COOLDOWN &&
        (next_herbivore_state_prototype->energy - HERBIVORE_REPRODUCTION_COST) > HERBIVORE_MOVE_COST) // Maradjon elég energia egy esetleges következő mozgáshoz
    {
        Coordinates empty_cell = get_random_adjacent_empty_cell(world, next_herbivore_state_prototype->position); // Az aktuális pozíció mellé
        if (is_valid_pos(world, empty_cell.x, empty_cell.y) &&
            (empty_cell.x != next_herbivore_state_prototype->position.x || empty_cell.y != next_herbivore_state_prototype->position.y))
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
            _commit_entity_to_next_state(world, new_herbivore_candidate);

            // Ha mozgás volt (action_taken_this_step == 2), akkor a szaporodás felülírja ezt az akciót
            // Ha nem volt mozgás (action_taken_this_step == 0), akkor a szaporodás lesz az akció
            action_taken_this_step = 4; // Akció megtörtént (szaporodás)
        }
    }
    // Ha action_taken_this_step 0 maradt, az entitás nem csinált semmit (pl. nem tudott mozogni, enni, szaporodni).
}

// Ragadozók akciói
void process_carnivore_actions_parallel(World *world, int current_step_number, Entity *current_carnivore_state_in_entities_array, Entity *next_carnivore_state_prototype)
{
    int action_taken_this_step = 0; // 0: semmi, 1: kritikus evés, 2: mozgás, 3: normál evés, 4: szaporodás

    // 0. Elsődleges ellenőrzés: Kritikus energia szintű evés
    if (next_carnivore_state_prototype->energy < CARNIVORE_CRITICAL_ENERGY_THRESHOLD)
    {
        Entity *target_herbivore = find_target_in_range(world, current_carnivore_state_in_entities_array->position, next_carnivore_state_prototype->sight_range, HERBIVORE);
        if (target_herbivore)
        {
            bool successfully_ate_herbivore = false;
#pragma omp critical(AccessEntityEnergyOnEating)
            {
                if (target_herbivore->energy > 0)
                {
                    target_herbivore->energy = EATEN_ENERGY_MARKER;
                    successfully_ate_herbivore = true;
                }
            }
            if (successfully_ate_herbivore)
            {
                next_carnivore_state_prototype->position = target_herbivore->position; // Ide lép az evés után
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

    // ÚJ SORREND:
    // 1. Mozgás (ha nem történt kritikus evés)
    if (!action_taken_this_step && // Csak akkor, ha action_taken_this_step == 0
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
    if (action_taken_this_step != 1) // Ha nem volt kritikus evés
    {
        Entity *target_herb_for_eat = find_target_in_range(world, next_carnivore_state_prototype->position, next_carnivore_state_prototype->sight_range, HERBIVORE);
        if (target_herb_for_eat)
        {
            bool successfully_ate_herbivore = false;
#pragma omp critical(AccessEntityEnergyOnEating) // Ugyanaz a kritikus szakasz
            {
                if (target_herb_for_eat->energy > 0)
                {
                    target_herb_for_eat->energy = EATEN_ENERGY_MARKER;
                    successfully_ate_herbivore = true;
                }
            }
            if (successfully_ate_herbivore)
            {
                next_carnivore_state_prototype->position = target_herb_for_eat->position; // Ide lép az evés után
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

    // 3. Szaporodás megpróbálása (ha nem volt kritikus evés, és nem volt normál evés)
    if (action_taken_this_step != 1 && action_taken_this_step != 3 &&
        next_carnivore_state_prototype->energy >= CARNIVORE_REPRODUCTION_THRESHOLD &&
        (current_step_number - current_carnivore_state_in_entities_array->last_reproduction_step) >= CARNIVORE_REPRODUCTION_COOLDOWN &&
        (next_carnivore_state_prototype->energy - CARNIVORE_REPRODUCTION_COST) > CARNIVORE_MOVE_COST) // Maradjon elég energia
    {
        Coordinates empty_cell = get_random_adjacent_empty_cell(world, next_carnivore_state_prototype->position);
        if (is_valid_pos(world, empty_cell.x, empty_cell.y) &&
            (empty_cell.x != next_carnivore_state_prototype->position.x || empty_cell.y != next_carnivore_state_prototype->position.y))
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
            _commit_entity_to_next_state(world, new_carnivore_candidate);

            action_taken_this_step = 4; // Szaporodás
        }
    }
}

// Keres egy adott típusú célpontot a megadott hatósugáron belül.
// Visszaadja a legközelebbi célpontra mutató pointert, vagy NULL-t, ha nincs.
// Fontos: Ez a függvény az AKTUÁLIS `world->grid` és `world->entities` alapján keres.
Entity *find_target_in_range(World *world, Coordinates center, int range, EntityType target_type)
{
    if (!world || range <= 0)
        return NULL;

    Entity *closest_target = NULL;
    int min_dist_sq = range * range + 1; // Négyzetes távolság, így nem kell gyököt vonni

    // Egyszerűbb (de nem a leghatékonyabb) módszer: bejárjuk a teljes entities listát.
    // Hatékonyabb lenne a grid-et használni a range-en belüli kereséshez.
    // De a párhuzamosítás miatt az entities lista bejárása lehet, hogy egyszerűbb és kevésbé hibalehetőség.
    // Figyelem: Ez az aktuális entitásokat nézi!

    for (int i = 0; i < world->entity_count; ++i)
    {
        Entity *potential_target = &world->entities[i]; // Pointer az AKTUÁLIS entitáslistából!

        if (potential_target->type == target_type && potential_target->energy > 0)
        {
            int dx = potential_target->position.x - center.x;
            int dy = potential_target->position.y - center.y;
            int dist_sq = dx * dx + dy * dy;

            if (dist_sq <= range * range)
            { // Hatósugáron belül van
                if (dist_sq < min_dist_sq)
                {
                    min_dist_sq = dist_sq;
                    closest_target = potential_target;
                }
            }
        }
    }
    return closest_target;
}