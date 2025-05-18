BEGIN {
    total_duration = 0;
    step_count = 0;
    threads = -1;

    # Mezőelválasztók beállítása: ":" vagy "," vagy "|" vagy szóközök
    FS = "[: ,|]+";
}

# Minden sorra, ahol a 4. mező "Total" (ez illeszkedik a "Total:"-ra a FS miatt)
# és a 12. mező "Threads"
# Példa sor feldarabolva FS alapján:
# $1=Step, $2=<num>, $3=timings, $4=Total, $5=<total_val>ms, $6=Carnivores, $7=<carn_val>ms, ... $12=Threads, $13=<thread_num>
$4 == "Total" && $12 == "Threads" {
    # A "Total" időérték az 5. mező, eltávolítjuk az "ms"-t
    current_total_time = $5;
    sub(/ms/, "", current_total_time); # Eltávolítja az "ms"-t a végéről
    total_duration += current_total_time;
    step_count++;

    if (threads == -1) {
        threads = $13;
    }
}

END {
    if (step_count > 0) {
        average_duration = total_duration / step_count;
        printf "Elemzett log fájl: timings.log\n";
        printf "-----------------------------------\n";
        printf "Szálak maximális száma: %d\n", threads;
        printf "Feldolgozott szimulációs lépések: %d\n", step_count;
        printf "Összesített futási idő (Total): %.4f ms\n", total_duration;
        printf "Átlagos lépésidő (Total): %.4f ms/lépés\n", average_duration;
    } else {
        print "Hiba: Nem található feldolgozható adat a 'timings.log' fájlban a megadott formátumban.";
        print "Ellenőrizd, hogy a log sorai tartalmazzák-e a 'Total: ...ms' és 'Threads: ...' részeket.";
    }
}
