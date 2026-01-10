---
name: 1D_Domain_Decomposition_Implementation_Plan
overview: Részletes ütemterv a main.c fájl módosításához, amely a hibrid MPI+OpenMP szimulációs logika (Ghost Exchange, Migráció) beépítését célozza.
todos:
  - id: helper_funcs
    content: Segédfüggvények implementálása (clear, pack/unpack, exchange)
    status: pending
  - id: commit_logic
    content: _commit_entity_to_next_state módosítása (migráció kezelése)
    status: pending
  - id: sim_loop
    content: simulate_step átírása (kommunikációs hívások beillesztése)
    status: pending
  - id: draw_update
    content: draw_simulation_state frissítése (adatgyűjtés masterhez)
    status: pending
---

# Terv: 1D Tartományfelbontás (Domain Decomposition) Implementálása a main.c-ben

A cél a `main.c` fájlban lévő szimulációs logika átírása, hogy támogassa a több folyamaton futó, sávokra osztott világot. A módosításokat négy fő lépésben érdemes végrehajtani.

## 1. Segédfüggvények Létrehozása (MPI Kommunikációhoz)

Ezeket a függvényeket a `main.c` elejére (a `simulate_step` elé) érdemes elhelyezni.

### A. `clear_ghosts(World *world)`

- **Feladat:** Minden kör elején törölni (felszabadítani) az előző körből ott maradt "szellem" entitásokat a `local_grid` felső és alsó sávjaiból.
- **Helye:** Új függvény.

### B. `pack_border_entities(...)` és `unpack_ghosts(...)`

- **Feladat:**
- `pack`: Végigmegy a saját területünk legfelső vagy legalsó során, és kigyűjti az ott lévő entitásokat egy `EntityBuffer`-be (másolatot készít).
- `unpack`: A fogadott bufferből "dummy" entitásokat hoz létre (`malloc`) és beleteszi őket a `local_grid` megfelelő szellemsávjába.
- **Helye:** Új függvények.

### C. `exchange_ghosts(World *world)`

- **Feladat:** A "szellemcsere" lebonyolítása.
- **Logika:**

1. Hívd meg a `clear_ghosts`-t.
2. `pack_border_entities` (Felső sávod -> Puffer).
3. `MPI_Sendrecv`: Küldd a Puffert a Felső szomszédnak, és fogadj az Alsó szomszédtól (vagy fordítva, a szomszéd logikája szerint).
4. `unpack_ghosts`: A fogadott adatot tedd a szellemsávba.
5. Ismételd meg az Alsó szomszéddal is.

- **Helye:** Új függvény.

### D. `exchange_migrations(World *world)`

- **Feladat:** A kör végén a sávból kilépett állatok átmozgatása.
- **Logika:**
- Hasonló az `exchange_ghosts`-hoz, de itt a `world->out_top` és `world->out_bottom` puffereket küldöd el.
- A fogadott entitásokat **nem** a szellemsávba, hanem a `world->next_entities` listába és a `local_next_grid`-be kell beírni (`add_entity...` vagy manuális beillesztés).
- A végén ürítsd ki az `out` puffereket (`count = 0`).
- **Helye:** Új függvény.

---

## 2. Commit Logika Módosítása (`_commit_entity_to_next_state`)

Ez a függvény dönti el, mi lesz az entitás sorsa a következő körben.

- **Módosítás:**
- **Ellenőrzés:** Nézd meg az entitás új `y` koordinátáját.
- **Ha `y < start_y` (Kilépett felfelé):** Ne a `next_entities`-be írd! Tedd a `world->out_top` pufferbe (OpenMP kritikus szakaszban!).
- **Ha `y > end_y` (Kilépett lefelé):** Tedd a `world->out_bottom` pufferbe.
- **Ha maradt:** Tedd a `next_entities`-be, ÉS frissítsd a `local_next_grid`-et. **Fontos:** Használd a `get_local_y()` konverziót az indexeléshez!

---

## 3. A Szimulációs Ciklus Átírása (`simulate_step`)

Ez a függvény vezérli az egész folyamatot.

- **Módosítások sorrendje:**

1. **Lépés elején:** Hívd meg az `exchange_ghosts(world)`-öt. (Így a szomszédok adatai láthatóvá válnak).
2. **Számítás:** A növény/növényevő/ragadozó loopok maradhatnak, de ellenőrizd, hogy a `world->entities` (lokális lista) alapján futnak.
3. **Lépés végén (Swap előtt):** Hívd meg az `exchange_migrations(world)`-öt. (Ez elintézi az átlépő állatokat).