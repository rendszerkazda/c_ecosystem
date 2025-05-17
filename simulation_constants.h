// World dimensions and other constants
#define DEFAULT_WIDTH 50
#define DEFAULT_HEIGHT 50
#define INITIAL_ENTITY_CAPACITY 50 // Initial maximum number of entities for the dynamic array

// Simulation parameters
#define PLANT_MAX_ENERGY 10
#define PLANT_REPRODUCTION_PROBABILITY 0.04 // Szaporodási esély lépésenként, ha a feltételek adottak (NÖVELT)
#define PLANT_REPRODUCTION_COOLDOWN 10      // Minimum lépésszám két szaporodás között
#define PLANT_MAX_AGE 40
#define PLANT_GROWTH_RATE 1 // Energia növekedés lépésenként (ha van hely)
#define EATEN_ENERGY_MARKER -1

#define HERBIVORE_INITIAL_ENERGY 100
#define HERBIVORE_MAX_ENERGY 200
#define HERBIVORE_ENERGY_DECAY 2 // Alap energiafogyasztás lépésenként (CSÖKKENTETT)
#define HERBIVORE_MOVE_COST 1
#define HERBIVORE_SIGHT_RANGE 7 // (NÖVELT)
#define HERBIVORE_MAX_AGE 80
#define HERBIVORE_REPRODUCTION_THRESHOLD 150   // Minimális energia a szaporodáshoz (CSÖKKENTETT)
#define HERBIVORE_REPRODUCTION_COST 100        // (CSÖKKENTETT)
#define HERBIVORE_REPRODUCTION_COOLDOWN 20     // Minimum lépésszám két szaporodás között
#define HERBIVORE_ENERGY_FROM_PLANT 35         // (NÖVELT)
#define HERBIVORE_CRITICAL_ENERGY_THRESHOLD 20 // Ha ez alá esik, csak enni próbál

#define CARNIVORE_INITIAL_ENERGY 150
#define CARNIVORE_MAX_ENERGY 300
#define CARNIVORE_ENERGY_DECAY 2 // (CSÖKKENTETT)
#define CARNIVORE_MOVE_COST 2
#define CARNIVORE_SIGHT_RANGE 8 // (NÖVELT)
#define CARNIVORE_MAX_AGE 40
#define CARNIVORE_REPRODUCTION_THRESHOLD 190 // (CSÖKKENTETT)
#define CARNIVORE_REPRODUCTION_COST 75       // (CSÖKKENTETT)
#define CARNIVORE_REPRODUCTION_COOLDOWN 25
#define CARNIVORE_ENERGY_FROM_HERBIVORE 70     // (NÖVELT)
#define CARNIVORE_CRITICAL_ENERGY_THRESHOLD 30 // Ha ez alá esik, csak enni próbál