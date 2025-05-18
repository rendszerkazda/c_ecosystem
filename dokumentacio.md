# Ökoszisztéma Szimulátor - Technikai Működés

## Adatstruktúrák (`datatypes.h`)

A szimuláció alapvető építőkövei a következő adatstruktúrák:

*   **`Coordinates`**: Egy entitás vagy cella 2D pozícióját (x, y) tárolja a rácson.
    ```c
    typedef struct {
        int x;
        int y;
    } Coordinates;
    ```

*   **`EntityType`**: Felsorolásos típus, amely az entitások lehetséges típusait definiálja (PLANT, HERBIVORE, CARNIVORE), valamint egy `EMPTY` állapotot az üres cellák vagy elpusztult entitások jelölésére.
    ```c
    typedef enum {
        PLANT,
        HERBIVORE,
        CARNIVORE,
        EMPTY
    } EntityType;
    ```

*   **`Entity`**: A szimuláció egyedi szereplőit reprezentálja. Tartalmazza:
    *   `id`: Egyedi azonosító.
    *   `type`: Az entitás típusa (`EntityType`).
    *   `position`: Aktuális pozíció (`Coordinates`).
    *   `energy`: Az entitás energiája. Meghatározza a túlélést, szaporodást.
    *   `age`: Az entitás kora szimulációs lépésekben.
    *   `sight_range`: Állatok esetén a látótávolság, amelyen belül érzékelhetnek más entitásokat.
    *   `last_reproduction_step`: Az utolsó sikeres szaporodás szimulációs lépésének sorszáma. A szaporodási cooldownhoz használatos.
    *   `last_eating_step`: Az utolsó sikeres táplálkozás szimulációs lépésének sorszáma.
    *   `just_spawned_by_keypress`: Logikai jelző, ami igaz, ha az entitást az aktuális lépésben manuálisan (billentyűleütéssel) hozták létre. Ez átmeneti vizuális megkülönböztetésre szolgálhat.
    ```c
    typedef struct Entity {
        int id;
        EntityType type;
        Coordinates position;
        int energy;
        int age;
        int sight_range;
        int last_reproduction_step;
        int last_eating_step;
        bool just_spawned_by_keypress;
    } Entity;
    ```

*   **`Cell`**: A szimulációs rács egyetlen celláját írja le. Jelenleg egyetlen `Entity*` mutatót tartalmaz, amely az adott cellában lévő entitásra mutat, vagy `NULL`, ha a cella üres.
    ```c
    typedef struct {
        Entity *entity;
    } Cell;
    ```

*   **`World`**: A teljes szimulációs környezetet foglalja magában:
    *   `width`, `height`: A szimulációs rács méretei.
    *   `grid`: 2D tömb (`Cell**`), amely a rács aktuális állapotát tárolja. A cellák `Entity*` mutatókat tartalmaznak.
    *   `next_grid`: A `grid`-hez hasonló 2D tömb, amely a következő szimulációs lépés állapotát építi fel (double buffering).
    *   `entities`: 1D tömb (`Entity*`), amely az összes aktív entitás aktuális állapotát tárolja.
    *   `next_entities`: Az `entities`-hez hasonló 1D tömb a következő lépés entitásállapotainak (double buffering).
    *   `entity_count`: Az `entities` tömbben lévő aktuális entitások száma.
    *   `next_entity_count`: A `next_entities` tömbben lévő entitások száma a következő lépéshez.
    *   `entity_capacity`: Az `entities` tömb allokált kapacitása.
    *   `next_entity_capacity`: A `next_entities` tömb allokált kapacitása.
    *   `next_entity_id`: Globális számláló a következő kiosztandó egyedi entitás ID-hez.

    ```c
    typedef struct {
        int width;
        int height;
        Cell **grid;
        Cell **next_grid;
        Entity *entities;
        Entity *next_entities;
        int entity_count;
        int next_entity_count;
        int entity_capacity;
        int next_entity_capacity;
        int next_entity_id;
    } World;
    ```

## Szimulációs Ciklus (`simulate_step`)

A szimuláció diszkrét időlépésekben halad. Minden egyes lépést a `simulate_step` függvény vezérel, amely a következő főbb műveleteket hajtja végre:

1.  **Puffer Előkészítés**:
    *   A `world->next_entities` tömb számlálója (`next_entity_count`) nullázódik.
    *   A `world->next_grid` rács minden cellájának `entity` mutatója `NULL`-ra állítódik, előkészítve a következő állapot felépítését.
    *   A `world->next_entity_id` értéke megmarad, hogy az új entitások folyamatosan egyedi ID-t kapjanak.

2.  **Entitásfeldolgozás (Párhuzamosítva OpenMP-vel)**:
    Az entitások feldolgozása meghatározott sorrendben történik a versenyhelyzetek és logikai konzisztencia érdekében. Az egyes entitástípusok feldolgozása párhuzamosítható az OpenMP `#pragma omp parallel for` direktívájával.
    *   **2.1. Ragadozók (`CARNIVORE`)**:
        *   Iteráció az `world->entities` tömbön.
        *   Minden ragadozó entitás (`current_entity_original_state`) alapján egy `next_entity_prototype` jön létre, amely az entitás következő állapotát képviseli.
        *   **Állapotfrissítés**:
            *   `age` növelése.
            *   `energy` csökkentése az `CARNIVORE_ENERGY_DECAY` értékkel.
        *   **Túlélés Ellenőrzése**: Ha az energia 0 alá csökken vagy az életkor meghaladja a `CARNIVORE_MAX_AGE` értéket, az entitás elpusztul (nem kerül be a `next_entities` tömbbe).
        *   **Akciók Feldolgozása (`process_carnivore_actions_parallel`)**: Ha az entitás túlélte az alapvető állapotfrissítéseket, specifikus akciókat hajt végre (lásd: Entitások Viselkedése).
        *   **Véglegesítés**: Ha az entitás az akciók után is életben marad (`energy > 0`) és nem jelölték meg közben megevettként (`EATEN_ENERGY_MARKER`), akkor az `next_entity_prototype` állapota bekerül a `world->next_entities` tömbbe és a `world->next_grid` megfelelő cellájába a `_commit_entity_to_next_state` segédfüggvényen keresztül. Az `EATEN_ENERGY_MARKER` ellenőrzés kritikus, mert egy másik ragadozó célpontja is lehetett volna.

    *   **2.2. Növényevők (`HERBIVORE`)**:
        *   Hasonló folyamat, mint a ragadozóknál.
        *   **Fontos**: Ellenőrizni kell, hogy a növényevőt egy ragadozó megette-e az előző (ragadozó) feldolgozási fázisban. Ezt az `world->entities[i].energy == EATEN_ENERGY_MARKER` ellenőrzés biztosítja az `world->entities` tömbben (az eredeti állapotban), mielőtt a `next_entity_prototype` létrejönne és feldolgozásra kerülne.
        *   `energy` csökkentése `HERBIVORE_ENERGY_DECAY`-jel.
        *   Túlélés ellenőrzése (`HERBIVORE_MAX_AGE`).
        *   Akciók feldolgozása (`process_herbivore_actions_parallel`).
        *   Véglegesítés a `next_entities` és `next_grid` pufferekbe, ha túlélte és nem ették meg.

    *   **2.3. Növények (`PLANT`)**:
        *   Hasonló folyamat.
        *   Ellenőrzés, hogy egy növényevő megette-e (`EATEN_ENERGY_MARKER` az `world->entities[i].energy`-n).
        *   **Energianövekedés**: Ha a növény energiája `0 < energy < PLANT_MAX_ENERGY`, akkor az `energy` növekszik a `PLANT_GROWTH_RATE` értékkel, de nem haladhatja meg a `PLANT_MAX_ENERGY`-t.
        *   Túlélés ellenőrzése (`PLANT_MAX_AGE`).
        *   Akciók feldolgozása (`process_plant_actions_parallel`).
        *   Véglegesítés. Növények esetén az `EATEN_ENERGY_MARKER` másodlagos ellenőrzése a `_commit_entity_to_next_state` előtt kevésbé releváns a saját feldolgozási fázisukban, mivel más növények nem "eszik meg" őket.

3.  **Puffercsere (Double Buffering)**:
    *   A `world->entities` és `world->next_entities` mutatók felcserélődnek.
    *   A `world->grid` és `world->next_grid` mutatók felcserélődnek.
    *   A `world->entity_count` frissül a `world->next_entity_count` értékével.
    *   A `world->entity_capacity` és `world->next_entity_capacity` is felcserélődik.
    Ez a technika biztosítja, hogy a következő lépés számításai az előző lépés konzisztens állapotán alapuljanak, és az állapotváltás atomi műveletnek tűnjön.

## Entitások Viselkedése (`entity_actions.c`)

Az `entity_actions.c` fájl tartalmazza azokat a függvényeket, amelyek az egyes entitástípusok specifikus viselkedését (mozgás, táplálkozás, szaporodás) implementálják. Ezeket a függvényeket a `simulate_step` hívja meg az entitásfeldolgozási fázisban.

### Közös Akciók (Állatok: Növényevők, Ragadozók)

*   **Célpontkeresés (`find_target_in_range`)**:
    *   Az állatok a `sight_range` látótávolságukon belül keresnek megfelelő típusú célpontot (növényt a növényevők, növényevőt a ragadozók).
    *   A keresés az `world->entities` tömbön (az aktuális lépés eleji, konzisztens állapoton) történik.
    *   Csak élő (pozitív energiájú) és még nem `EATEN_ENERGY_MARKER`-rel jelölt entitásokat vesz figyelembe.
    *   A legközelebbi (Manhattan-távolság alapján) célpontot választja.

*   **Mozgás (`get_step_towards_target`, `get_random_adjacent_empty_cell`)**:
    *   Ha találtak célpontot, egy lépést tesznek felé a `get_step_towards_target` segítségével, amely a célpont felé vezető lehetséges szomszédos cellák közül választ egy üreset (ha van).
    *   Ha nincs célpont, vagy a célpont felé nem tudnak lépni, véletlenszerűen választanak egy üres szomszédos cellát a `get_random_adjacent_empty_cell` segítségével.
    *   A mozgás költséggel jár (`*_MOVE_COST`), ami levonódik az energiájukból.
    *   A mozgás csak akkor történik meg, ha van érvényes célcella és az entitásnak van elég energiája a mozgáshoz.

*   **Táplálkozás**:
    *   **Prioritás**: Kritikus energia szint (`*_CRITICAL_ENERGY_THRESHOLD`) alatt a táplálkozás elsőbbséget élvez.
    *   **Feltétel**: Csak akkor sikeres, ha a célpont közvetlenül szomszédos az állat *aktuális* (akár mozgás utáni) pozíciójával.
    *   **Atomicitás**: A célpont (`target_plant` vagy `target_herbivore`) energiájának módosítása (`energy = EATEN_ENERGY_MARKER`) egy `#pragma omp critical(AccessEntityEnergyOnEating)` blokkban történik, hogy elkerüljük a versenyhelyzeteket, amikor több állat is ugyanazt a célpontot próbálja megenni egy lépésen belül.
    *   **Hatás**: Sikeres evés esetén az állat a megevett célpont pozíciójára lép, energiát nyer (`*_ENERGY_FROM_*`), és frissül a `last_eating_step` mezője. A megevett célpont effektíve eltávolításra kerül, mivel az `EATEN_ENERGY_MARKER` miatt a következő feldolgozási lépések során már nem veszik figyelembe, és nem kerül be a `next_entities` listába.

*   **Szaporodás**:
    *   **Feltételek**:
        *   Elegendő energia (`*_REPRODUCTION_THRESHOLD` felett).
        *   A szaporodási cooldown letelt (`current_step_number - last_reproduction_step >= *_REPRODUCTION_COOLDOWN`).
        *   Állatoknál: A szaporodás után is maradjon elegendő energia a túléléshez (pl. `energy - *_REPRODUCTION_COST > *_MOVE_COST`).
        *   Növényeknél: Egy `PLANT_REPRODUCTION_PROBABILITY` valószínűségi feltétel is teljesül.
    *   **Folyamat**:
        *   Üres szomszédos cella keresése az *aktuális* pozíció mellett.
        *   Ha van ilyen, és az entitások száma nem érte el a maximumot (`MAX_*`), létrejön egy új entitás.
        *   Az új entitás a szülő energiájának egy részét kapja meg (`PLANT_MAX_ENERGY / 2` vagy `HERBIVORE/CARNIVORE_INITIAL_ENERGY`), a szülő energiája csökken (`*_REPRODUCTION_COST`).
        *   A szülő `last_reproduction_step` mezője frissül.
        *   Az új entitás a `_commit_entity_to_next_state` segítségével kerül a `next_entities` listába.

### Növényspecifikus Akciók (`process_plant_actions_parallel`)

*   Főként a szaporodást foglalja magában, a fent leírt feltételekkel.
*   Az energianövekedés (`PLANT_GROWTH_RATE`) a `simulate_step` fő ciklusában, a növények feldolgozási fázisában történik, nem közvetlenül az `process_plant_actions_parallel`-ben.

### Állatspecifikus Akciók (`process_herbivore_actions_parallel`, `process_carnivore_actions_parallel`)

Az akciók sorrendje és prioritása az állatoknál (növényevők, ragadozók) a következőképpen alakul egy lépésen belül:
1.  **Kritikus evés**: Ha az energia kritikusan alacsony, megpróbál enni egy szomszédos célpontot. Ha sikeres, más akció nem történik.
2.  **Mozgás**: Ha nem volt kritikus evés, és van elég energia, megpróbál elmozdulni (célpont felé vagy véletlenszerűen).
3.  **Normál evés**: Mozgás után (vagy ha nem mozgott, de nem volt kritikus evés), megpróbál enni egy szomszédos célpontot az *új* pozícióján. Ez felülírhatja a mozgás akcióját, ha mindkettő megtörténne.
4.  **Szaporodás**: Ha nem történt sem kritikus, sem normál evés, és a feltételek adottak, megpróbál szaporodni. Ez is felülírhatja a mozgás akcióját.

Egy `action_taken_this_step` belső változó segíthet nyomon követni, hogy melyik főbb kategóriájú akció (kritikus evés, mozgás, normál evés, szaporodás) történt meg az adott lépésben, hogy a prioritások érvényesüljenek.
