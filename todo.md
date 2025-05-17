# Ökoszisztéma Szimulátor - Tennivalók

Ez a dokumentum tartalmazza az ökoszisztéma szimulátor projekt hátralévő feladatait és fejlesztési lépéseit.

## Magas prioritású feladatok

1.  **Grafikus Megjelenítés (ncurses implementáció)**
    *   [x] Ncurses könyvtár inicializálása és deinicializálása a `main.c`-ben.
    *   [x] A szimulációs világ (rács) megjelenítése ncurses segítségével.
        *   [x] Rács határainak kirajzolása.
        *   [x] Üres mezők megjelenítése.
    *   [x] Entitások megjelenítése a rácson:
        *   [x] Növények (pl. 'P' karakter zöld színnel).
        *   [x] Növényevők (pl. 'H' karakter sárga színnel).
        *   [x] Ragadozók (pl. 'C' karakter piros színnel).
    *   [x] A megjelenítés frissítése minden szimulációs lépés után.
    *   [x] Információs sáv megjelenítése (pl. aktuális lépésszám, entitások száma típusonként).
    *   [x] Wide karakterek támogatása (`ncursesw`, `locale`) az ékezetes karakterek helyes megjelenítéséhez.

2.  **Entitások Öregedése és Élettartama**
    *   [x] Az `age` attribútum növelése minden entitásnál minden szimulációs lépésben.
    *   [x] Entitások eltávolítása a világból, ha elérik a `max_age` kort.
    *   [x] A `max_age` értékek beállítása a `simulation_constants.h`-ban vagy az inicializáláskor mindhárom típusra (NÖVÉNY, NÖVÉNYEVŐ, RAGADOZÓ).
    *   [ ] Tesztelés, hogy az öregedés és eltávolítás megfelelően működik-e, és finomhangolás az elszaporodás ellen (pl. `MAX_AGE` értékek csökkentése, szaporodási paraméterek vizsgálata).

3.  **Főmenü és Alapvető Felhasználói Interakció (ncurses)**
    *   [x] Egyszerű szöveges főmenü létrehozása ncurses segítségével.
    *   [x] Menüpontok:
        *   [x] Szimuláció Indítása
        *   [x] Beállítások
        *   [x] Kilépés a programból
    *   [x] Menü navigáció (fel/le nyilak vagy 'w'/'s') és kiválasztás ('Enter').
    *   [x] Billentyűzetről történő vezérlés implementálása a szimuláció futása közbeni kilépéshez (pl. 'q' billentyűvel).

4.  **Világ és Szimuláció Konfigurálhatósága (Beállítások Menü)**
    *   [x] Beállítások menü létrehozása.
    *   [x] Világméret választás (Kicsi, Közepes, Nagy) és értékek tárolása.
        *   [x] Kicsi (pl. 30x20)
        *   [x] Közepes (pl. 50x30 - alapértelmezett)
        *   [x] Nagy (pl. 80x40)
    *   [x] Lépések közötti késleltetés választás (Gyors, Normál, Lassú) és értékek tárolása.
        *   [x] Gyors (pl. 50ms)
        *   [x] Normál (pl. 150ms - alapértelmezett)
        *   [x] Lassú (pl. 300ms)
    *   [ ] Szimuláció maximális lépésszámának beállítása (3 szint, pl. Rövid, Közepes, Hosszú).
    *   [x] Beállítások alkalmazása a szimuláció indításakor.
    *   [x] "Vissza a főmenübe" opció.
    *   [ ] Kezdeti entitásszámok beállításának lehetősége (egyelőre maradhat a `simulation_constants.h`-ban vagy fix érték).

## Közepes prioritású feladatok

5.  **Szimulációs Paraméterek Finomhangolása és Tesztelése**
    *   [ ] Az entitások viselkedését befolyásoló konstansok (`simulation_constants.h`) áttekintése és értelmes alapértelmezett értékek beállítása (pl. szaporodási ráták, éhségküszöbök, látótávolság) – az öregedéssel összefüggésben is.
    *   [ ] Alapos tesztelés különböző paraméter-kombinációkkal a szimuláció stabilitásának és valószerűségének ellenőrzésére.
    *   [ ] Memóriakezelés ellenőrzése:
        *   [ ] `valgrind` vagy hasonló eszköz használata memóriaszivárgások és hibák felderítésére.
        *   [ ] Dinamikusan allokált memória megfelelő felszabadításának biztosítása mindenhol.
    *   [ ] Párhuzamosítás hatékonyságának mérése és elemzése. Esetleges optimalizációs lehetőségek felkutatása.

6.  **Makefile Bővítése és Karbantartása**
    *   [x] Ncurses könyvtár linkelésének hozzáadása a `Makefile`-hoz (`-lncursesw`).
    *   [ ] `clean` cél kiterjesztése, hogy minden generált fájlt (objektumfájlok, futtatható állomány) töröljön (már létezik, de ellenőrizni).
    *   [ ] Opcionális: `debug` fordítási cél hozzáadása hibakeresési szimbólumokkal (`-g`) (már benne van a CFLAGS-ban).

## Alacsony prioritású feladatok / További fejlesztési ötletek

7.  **Hibakezelés és Robusztusság Növelése**
    *   [ ] Robusztusabb hibakezelés implementálása a program kritikus pontjain.
    *   [ ] Felhasználói input validálása (ha releváns lesz a későbbiekben).

8.  **Dokumentáció Frissítése és Kiegészítése (`README.md`, `dokumentacio.md`)**
    *   [ ] A `README.md` frissítése a program használati utasításaival, beleértve az ncurses felület kezelését, beállításokat és a fordítási lépéseket.
    *   [ ] A `dokumentacio.md` kiegészítése az új funkciók (ncurses UI, főmenü, beállítások) leírásával.
    *   [ ] Kódkommentek átnézése, szükség szerinti javítása és kiegészítése.

9.  **Speciális Funkciók (Opcionális)**
    *   [ ] Szimuláció állapotának mentése fájlba és betöltése.
    *   [ ] Logolási mechanizmus események rögzítésére.
    *   [ ] Maximális entitásszám korlátjának bevezetése (a túlszaporodás ellen).

Ez a lista kiindulópontként szolgál. A fejlesztés során újabb feladatok merülhetnek fel, vagy a meglévők prioritása változhat. 