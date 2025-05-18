# Slide 1: Cím dia

## Ökoszisztéma Szimulátor

Készítette: [A Neved]
Dátum: [Dátum / Kurzus neve]

---

# Slide 2: Tartalom

1.  Feladatleírás / Megvalósított funkciók
2.  Specifikáció
3.  Használt technológiák
4.  Kihívások az elkészítés során (1)
5.  Kihívások az elkészítés során (2)
6.  Tesztelés
7.  Benchmark (párhuzamosítás)
8.  Összefoglalás

---

# Slide 3: Feladatleírás / Megvalósított funkciók

## Feladatleírás
Egy egyszerű ökoszisztéma szimulációja, ahol különböző típusú entitások (növények, növényevők, ragadozók) élnek és interakcióba lépnek egy 2D rácsalapú világban. A cél a populációdinamikák és az entitások viselkedésének modellezése.

## Megvalósított funkciók
*   **Entitástípusok**: Növények (P), Növényevők (H), Ragadozók (C).
*   **Életciklus**: Entitások születnek, öregszenek, energiát gyűjtenek/veszítenek, szaporodnak és elpusztulnak.
*   **Interakciók**:
    *   Növényevők növényeket esznek.
    *   Ragadozók növényevőket esznek.
    *   Mozgás (célkeresés vagy véletlenszerű).
*   **Szimulációs környezet**:
    *   2D rács (konfigurálható méret).
    *   Diszkrét időlépésekben futó szimuláció.
*   **Felhasználói felület (Ncurses)**:
    *   Főmenü a szimuláció indításához és beállításokhoz.
    *   Beállítások menü (világméret, késleltetés, lépésszám).
    *   Szimulációs nézet az entitások megjelenítésével és valós idejű statisztikákkal.
    *   Interaktív vezérlés (szimuláció megállítása, entitások manuális hozzáadása).
*   **Párhuzamosítás (OpenMP)**:
    *   A szimulációs lépés entitásfeldolgozási fázisának párhuzamosítása a teljesítmény növelése érdekében.
*   **Konfigurálhatóság**:
    *   Szimulációs paraméterek (pl. `MAX_ENTITIES`, energiakezelés, szaporodási ráták) konstansokon keresztül (`simulation_constants.h`).

---

# Slide 4: Specifikáció

## Adatstruktúrák (`datatypes.h`)
*   **`Coordinates`**: `{int x, int y}` - Entitás pozíciója.
*   **`EntityType`**: `PLANT`, `HERBIVORE`, `CARNIVORE`, `EMPTY`.
*   **`Entity`**: `id`, `type`, `position`, `energy`, `age`, `sight_range`, `last_reproduction_step`, `last_eating_step`, `just_spawned_by_keypress`.
*   **`Cell`**: `{Entity *entity}` - Rács cellája, entitásra mutat.
*   **`World`**: `width`, `height`, `grid`, `next_grid`, `entities`, `next_entities`, számlálók és kapacitások.

## Szimulációs Ciklus (`simulate_step`)
1.  **Puffer Előkészítés**: `next_grid` és `next_entities` nullázása/előkészítése.
2.  **Entitásfeldolgozás (Párhuzamosítva OpenMP-vel)**:
    *   Sorrend: Ragadozók -> Növényevők -> Növények.
    *   Minden entitásnál:
        *   Kor, energiafrissítés (decay).
        *   Túlélés ellenőrzése.
        *   Specifikus akciók (`process_*_actions_parallel`):
            *   **Kritikus evés**: Alacsony energia esetén.
            *   **Mozgás**: Célpont felé vagy véletlenszerűen (költség).
            *   **Normál evés**: Mozgás után.
            *   **Szaporodás**: Feltételek teljesülése esetén (energia, cooldown, valószínűség növénynél).
        *   Energianövekedés (növényeknél: `PLANT_GROWTH_RATE`).
        *   Állapot rögzítése a `next_entities` és `next_grid`-be (`_commit_entity_to_next_state`).
            *   `EATEN_ENERGY_MARKER` használata a megevett entitások jelölésére.
3.  **Puffercsere (Double Buffering)**: `grid <-> next_grid`, `entities <-> next_entities`.

## Entitás Viselkedés (`entity_actions.c`)
*   **Célpontkeresés (`find_target_in_range`)**: Látótávolságon belül, legközelebbi, élő, nem megevett.
*   **Mozgás (`get_step_towards_target`, `get_random_adjacent_empty_cell`)**: Üres cellára lépés.
*   **Táplálkozás**: Szomszédos célpont, energia növelés, `EATEN_ENERGY_MARKER` kritikus szakaszban (`#pragma omp critical`).
*   **Szaporodás**: Üres szomszédos cella, energia költség/átadás.

## Kulcs Paraméterek (`simulation_constants.h`)
*   Maximális entitásszámok (`MAX_PLANTS`, stb.)
*   Energia értékek (kezdeti, max, decay, nyereség evésből, növekedési ráta)
*   Életkor (`*_MAX_AGE`)
*   Viselkedési paraméterek (szaporodási cooldown, valószínűség, költségek)
*   `EATEN_ENERGY_MARKER`

---

# Slide 5: Használt technológiák

*   **Programozási nyelv**: C (C99/C11 standard)
*   **Fordító**: GCC (GNU Compiler Collection)
*   **Felhasználói felület**: Ncurses könyvtár (szöveges felülethez)
*   **Párhuzamosítás**: OpenMP API (megosztott memóriás párhuzamosításhoz)
*   **Operációs rendszer**: Linux (fejlesztés és tesztelés WSL2-n Ubuntu alatt)
*   **Verziókezelés**: Git (feltételezhetően)
*   **Build rendszer**: Makefile

---

# Slide 6: Kihívások az elkészítés során (1)

## Párhuzamos Adathozzáférés és Versenyhelyzetek Kezelése (OpenMP)

*   **Probléma**:
    *   Több szál egyidejűleg próbálja módosítani a megosztott adatokat (pl. entitások energiája evésnél, `world->next_entity_count` növelése).
    *   Példa: Két ragadozó ugyanazt a növényevőt próbálja megenni egyszerre.
    *   Példa: `world->next_entity_count` atomikus növelése és az entitás tömbbe helyezése.
*   **Megoldás**:
    *   **Kritikus szakaszok (`#pragma omp critical`)**:
        *   Az `EATEN_ENERGY_MARKER` beállítása a célponton evéskor (`AccessEntityEnergyOnEating`).
        *   Az új entitások `world->next_grid`-be helyezése (`GridWriteNextState`).
    *   **Atomikus műveletek (`#pragma omp atomic`)**:
        *   `world->next_entity_count` növelése (`capture`).
        *   Entitások energiájának olvasása/írása (pl. `EATEN_ENERGY_MARKER` ellenőrzése).
    *   **Feldolgozási sorrend**: Meghatározott sorrend (Ragadozók -> Növényevők -> Növények) a logikai függőségek kezelésére (pl. egy entitás ne végezzen akciót, ha már megették ugyanabban a lépésben).

---

# Slide 7: Kihívások az elkészítés során (2)

## Double Buffering Implementációjának Konzisztenciája

*   **Probléma**:
    *   A szimuláció következő állapotát az aktuális, konzisztens állapot alapján kell kiszámítani.
    *   Mind az entitások listáját (`entities` vs `next_entities`), mind a rácsot (`grid` vs `next_grid`) szinkronban kell tartani és frissíteni.
    *   Hibaforrás lehet, ha a `next_grid` mutatói nem a `next_entities` tömb megfelelő elemeire mutatnak, vagy ha a puffercsere nem teljes.
*   **Megoldás**:
    *   **Dedikált pufferek**: `next_entities` tömb az új/módosult entitásoknak, `next_grid` a következő rácsállapotnak.
    *   **`_commit_entity_to_next_state` függvény**: Ez a függvény felelős egy entitás végleges állapotának rögzítéséért mind a `next_entities` tömbbe, mind a `next_grid` megfelelő cellájába (kritikus szakaszon belül a grid írása).
    *   **Pointercsere**: A szimulációs lépés végén egyszerű pointercserékkel válik az új állapot aktuálissá, ami gyors és hatékony.
    *   **Entitásszámlálók és kapacitások kezelése**: `entity_count` és `next_entity_count`, valamint a (most már fix) kapacitások megfelelő kezelése a puffercsere során.

*(Opcionális harmadik kihívás: Pl. ncurses UI komplexitása, eseménykezelés, vagy a mozgási/célkeresési algoritmus finomhangolása a "realisztikusabb" viselkedésért.)*

---

# Slide 8: Tesztelés

*   **Manuális Tesztelés és Vizuális Ellenőrzés**:
    *   Az ncurses felületen keresztül a szimuláció futásának megfigyelése.
    *   Entitások viselkedésének (mozgás, evés, szaporodás) ellenőrzése különböző forgatókönyvekben.
    *   Entitások manuális hozzáadása és hatásuk vizsgálata.
    *   Szélsőséges esetek (pl. túlszaporodás, kipusztulás) figyelése.
*   **Loggolás / Debug üzenetek**:
    *   `fprintf(stderr, ...)` használata kritikus pontokon a változók állapotának, eseményeknek a nyomon követésére (főleg a párhuzamos részeknél).
    *   (Esetleg: `printf` alapú logolás fájlba, ha a terminál kimenet nem elég.)
*   **Moduláris tesztelés (Implicit)**:
    *   Az egyes függvények (pl. `get_step_towards_target`, `find_target_in_range`) helyes működésének ellenőrzése a teljes rendszer részeként.
*   **Paraméterezés tesztelése**:
    *   Különböző konstans értékek (`simulation_constants.h`) hatásának vizsgálata a szimuláció kimenetelére.
*   **Hibakezelés tesztelése**:
    *   Memóriafoglalási hibák (pl. `malloc` visszatérési értékének ellenőrzése - már implementálva).
    *   Tömbök túlcímzésének elkerülése (pl. entitás tömbök kapacitásának ellenőrzése - a legutóbbi módosítások alapján).

---

# Slide 9: Benchmark (párhuzamosítás)

## Cél: Az OpenMP párhuzamosítás hatékonyságának mérése

*   **Módszer**:
    *   A `simulate_step` függvény (vagy a teljes `run_simulation` egy adott lépésszámra) futási idejének mérése.
    *   Változó számú OpenMP szál (`omp_set_num_threads()` vagy `OMP_NUM_THREADS` környezeti változó) használata.
        *   Tipikus tesztesetek: 1 szál (szekvenciális referencia), 2 szál, 4 szál, stb., egészen a CPU magok számáig/logikai processzorok számáig.
    *   Fix szimulációs paraméterek:
        *   Azonos világméret.
        *   Azonos kezdeti entitásszám és eloszlás (a `srand` fixálása vagy azonos seed használata fontos lehet az összehasonlíthatósághoz, bár az interakciók dinamikája miatt ez nem mindig garantál teljesen azonos lefutást).
        *   Azonos számú szimulációs lépés.
*   **Mért metrika**:
    *   Teljes futási idő (pl. `omp_get_wtime()` segítségével a mérendő kódrészlet előtt és után).
    *   Speedup számítása: `Speedup(N) = FutásiIdő(1 szál) / FutásiIdő(N szál)`.
*   **Várható eredmények**:
    *   Speedup növekedése a szálak számával egy bizonyos pontig.
    *   A speedup korlátai:
        *   Amdahl törvénye (a program nem teljesen párhuzamosítható részei korlátozzák a maximális gyorsulást).
        *   OpenMP overhead (szálak létrehozása, szinkronizáció).
        *   Terheléselosztás egyenlőtlenségei (`schedule(dynamic)` segít, de nem tökéletes).
*   **Eredmények bemutatása**:
    *   Táblázat a futási időkről és speedup értékekről különböző szálak esetén.
    *   Grafikon a speedup-ról a szálak számának függvényében.

*(Itt kellene bemutatni a konkrét mérési eredményeket és grafikonokat.)*

---

# Slide 10: Összefoglalás

## Elért eredmények
*   Működő ökoszisztéma-szimulátor C nyelven.
*   Entitás-alapú modellezés komplex interakciókkal.
*   Szöveges felhasználói felület ncurses segítségével, interaktív vezérlési lehetőségekkel.
*   A szimulációs logika sikeres párhuzamosítása OpenMP-vel a teljesítmény javítása érdekében.
*   Moduláris és (viszonylag) jól strukturált kód.

## Legfontosabb tanulságok
*   Párhuzamos programozás kihívásai (versenyhelyzetek, szinkronizáció).
*   Memóriakezelés fontossága C nyelven (double buffering, dinamikus tömbök – majd fix méretűek).
*   Nagyobb projekt strukturálásának lépései.
*   Ncurses használatának alapjai.

## Lehetséges jövőbeli fejlesztések
*   Grafikusabb megjelenítés (pl. SDL vagy egyszerűbb grafikus könyvtár).
*   Komplexebb entitásviselkedés (pl. genetika, tanulás, csordaszellem).
*   Több entitástípus, fejlettebb tápláléklánc.
*   Szimuláció állapotának mentése/betöltése.
*   Részletesebb statisztikák, grafikonok készítése a populációk változásáról.
*   Fejlettebb párhuzamosítási technikák vizsgálata.

## Köszönöm a figyelmet! Kérdések?
