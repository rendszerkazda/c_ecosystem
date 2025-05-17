# Ökoszisztéma Szimulátor - Részletes Dokumentáció

## Bevezetés

Ez a dokumentum az Ökoszisztéma Szimulátor C nyelven írt programjának részletes leírását tartalmazza. A szimulátor egy 2D rácson modellezi a növények (Plant), növényevők (Herbivore) és ragadozók (Carnivore) közötti interakciókat. A program ncurses könyvtárat használ a szöveges felhasználói felület (TUI) megvalósításához, és OpenMP-t a szimulációs lépések párhuzamosításához.

## Felhasználói Felület (Ncurses)

A program indításakor egy szöveges felhasználói felület jelenik meg, amelyet az ncurses könyvtár kezel.

### Főmenü

A program a főmenüvel indul, amely a következő opciókat kínálja:
*   **Szimuláció Indítása**: Elindítja az ökoszisztéma szimulációját az aktuális beállításokkal.
*   **Beállítások**: Megnyit egy almenüt, ahol a szimuláció paraméterei módosíthatók.
*   **Kilépés**: Kilép a programból.

A menüben a **fel/le nyilakkal** vagy a **'w'/'s'** billentyűkkel lehet navigálni, és az **Enter** billentyűvel lehet kiválasztani egy menüpontot.

### Beállítások Menü

A "Beállítások" menüpont a következő konfigurációs lehetőségeket tartalmazza:
*   **Világméret**: A szimulációs rács mérete. Választható opciók:
    *   Kicsi (30x20)
    *   Közepes (50x30) - Alapértelmezett
    *   Nagy (80x40)
*   **Késleltetés**: A szimulációs lépések közötti késleltetés, amely a szimuláció sebességét befolyásolja. Választható opciók:
    *   Gyors (kb. 50ms)
    *   Normál (kb. 150ms) - Alapértelmezett
    *   Lassú (kb. 300ms)
*   **Szim. Lépésszám**: A szimuláció teljes hossza lépésekben kifejezve. Választható opciók:
    *   Rövid (200 lépés)
    *   Közepes (500 lépés) - Alapértelmezett
    *   Hosszú (1000 lépés)
*   **Vissza a főmenübe**: Visszalép a főmenübe.

A beállítások menüben a **fel/le nyilakkal** (vagy **'w'/'s'**) lehet navigálni a beállítandó paraméterek között. A kiválasztott paraméter értékét a **balra/jobbra nyilakkal** (vagy **'a'/'d'**) lehet módosítani. Az **Enter** billentyű a "Vissza a főmenübe" opción aktiválódik, illetve a **'q'** billentyűvel is vissza lehet lépni.

### Szimulációs Nézet

A szimuláció indítása után a következő elemek jelennek meg:
*   **Információs sáv**: A képernyő tetején található, és a következő adatokat mutatja:
    *   Aktuális lépésszám / Összes lépésszám
    *   Jelenlegi entitások száma (Összesen, N: Növény, H: Növényevő, R: Ragadozó)
    *   Világméret (szélesség x magasság)
    *   Kilépési utasítás ('q' billentyű)
*   **Szimulációs Rács**: Az információs sáv alatt helyezkedik el, kerettel körülvéve. Itt jelennek meg az entitások:
    *   **Növény (P)**: Zöld színnel
    *   **Növényevő (H)**: Sárga színnel
    *   **Ragadozó (C)**: Piros színnel

A szimuláció futása közben a **'q'** billentyű lenyomásával bármikor vissza lehet térni a főmenübe.

## Adatstruktúrák

(A `datatypes.h` fájlban definiált főbb struktúrák leírása itt következne, pl. `Coordinates`, `EntityType`, `Entity`, `Cell`, `World`. Ez a rész nagyrészt megegyezik a korábbi dokumentációval, ezért itt most nem részletezem újra, de egy teljes dokumentációban itt lenne a helye.)

```c
// datatypes.h tartalma releváns részekkel

typedef struct {
    int x;
    int y;
} Coordinates;

typedef enum {
    PLANT,
    HERBIVORE,
    CARNIVORE,
    EMPTY // Üres cella vagy halott entitás jelölésére
} EntityType;

typedef struct Entity {
    int id;
    EntityType type;
    Coordinates position;
    int energy;
    int age;
    int sight_range; // Látótávolság állatoknak
    // Szaporodáshoz kapcsolódó mezők:
    int last_reproduction_step; // Utolsó szaporodás lépésszáma
    int last_eating_step;      // Utolsó evés lépésszáma (ragadozóknál releváns lehet)
    // ... egyéb állapotok ...
} Entity;

typedef struct {
    Entity *entity; // Mutató az adott cellában lévő entitásra, vagy NULL
} Cell;

typedef struct {
    int width;
    int height;
    Cell **grid;          // Aktuális állapot rácsa
    Cell **next_grid;     // Következő állapot rácsa (double buffering)
    Entity *entities;     // Aktuális entitások tömbje
    Entity *next_entities;// Következő állapot entitásainak tömbje (double buffering)
    int entity_count;     // Aktuális entitások száma
    int next_entity_count;// Következő állapot entitásainak száma
    int entity_capacity;  // entities tömb kapacitása
    int next_entity_capacity; // next_entities tömb kapacitása
    int next_entity_id;   // Következő kiosztandó egyedi entitás ID
} World;
```

## Szimulációs Logika

(A `simulate_step` függvény és az entitások viselkedésének (`entity_actions.c`) leírása itt következne. Ez a rész is nagyrészt megegyezik a korábbi dokumentációval.)

### Főbb lépések egy szimulációs ciklusban (`simulate_step`):
1.  **Következő állapot puffereinek előkészítése**: A `next_grid` és `next_entities` törlése/előkészítése.
2.  **Entitások párhuzamos feldolgozása (OpenMP)**:
    *   Minden entitás eredeti állapota alapján (`world->entities[i]`) egy új állapot prototípusa (`next_entity_prototype`) jön létre.
    *   **Alapvető állapotfrissítések**: Kor növelése, energiaveszteség (állatoknál), energianövekedés (növényeknél).
    *   **Túlélés ellenőrzése**: Ha az energia elfogyott vagy az entitás elérte a maximális korát (`*_MAX_AGE` konstansok), az entitás elpusztul (nem kerül be a következő állapotba).
    *   **Típus-specifikus akciók** (hívások az `entity_actions.c`-ben):
        *   Növények: Szaporodás.
        *   Növényevők: Mozgás, táplálkozás (növények), szaporodás.
        *   Ragadozók: Mozgás, táplálkozás (növényevők), szaporodás.
    *   **Végleges állapot rögzítése**: Ha az entitás túlélte és az akciók során nem pusztult el, bekerül a `next_entities` tömbbe és a `next_grid`-re.
3.  **Pufferek cseréje**: Az `entities` és `grid` mutatók felcserélődnek a `next_entities` és `next_grid` mutatókkal.

## Fordítás és Futtatás

Lásd a `README.md` fájlt.

## Fejlesztési Folyamat és Tennivalók

A részletes tennivalók és a fejlesztés állapota a `todo.md` fájlban követhető. 