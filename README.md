# Ökoszisztéma Szimulátor

Ez egy egyszerű ökoszisztéma szimulátor C nyelven, ncurses könyvtárral a szöveges felhasználói felülethez és OpenMP-vel a párhuzamosításhoz.

## Funkciók

*   Grafikus megjelenítés ncurses segítségével.
*   Főmenü:
    *   Szimuláció Indítása
    *   Beállítások
    *   Kilépés
*   Beállítások menü:
    *   Világméret (Kicsi, Közepes, Nagy)
    *   Szimulációs sebesség (Késleltetés: Gyors, Normál, Lassú)
    *   Szimuláció hossza (Lépésszám: Rövid, Közepes, Hosszú)
*   Szimuláció közben az entitások (Növények, Növényevők, Ragadozók) színekkel jelennek meg.
*   Információs sáv mutatja az aktuális lépésszámot, az összes lépésszámot, az entitások számát típusonként és a világ méretét.
*   A szimulációból 'q' billentyűvel lehet kilépni.
*   Párhuzamosított entitásfeldolgozás OpenMP segítségével.
*   Dupla pufferelés a zökkenőmentesebb megjelenítés érdekében.
*   Entitások öregedése és halála.

## Fordítás

A program fordításához `gcc` fordítóra, `make`-re és az `ncursesw` (wide character ncurses) könyvtárra van szükség.

```bash
make
```

Ez létrehozza az `ecosystem_simulator` futtatható állományt.

## Futtatás

```bash
./ecosystem_simulator
```

A program a főmenüvel indul. Használd a fel/le nyilakat (vagy 'w'/'s') a navigációhoz és az Entert a kiválasztáshoz.
A Beállítások menüben a balra/jobbra nyilakkal (vagy 'a'/'d') módosíthatod az értékeket.

## Tennivalók

A részletes tennivalók listája a `todo.md` fájlban található.

## Részletesebb dokumentáció

A program működéséről, adatstruktúráiról és a szimulációs logikáról részletesebb leírás a `dokumentacio.md` fájlban található.

## Fejlesztési Útiterv

- [ ] Adatszerkezetek implementálása a világhoz és az élőlényekhez.
- [ ] Inicializációs logika kidolgozása.
- [ ] Mozgási, táplálkozási és szaporodási szabályok implementálása.
- [ ] Szimulációs ciklus megvalósítása.
- [ ] OpenMP párhuzamosítás finomhangolása.
- [ ] Parancssori argumentumok kezelése (pl. világméret, entitások száma).
- [ ] Egyszerű szöveges vagy grafikus megjelenítés. 