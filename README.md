# Tresc zadania:

Z miasta A do miasta B prowadzi droga, na której znajduje się wąski most umożliwiający
tylko ruch jednokierunkowy. Most jest również dość słaby, także może po nim przejeżdżać
tylko jeden samochód na raz. Napisać program w którym N samochodów (wątków) będzie
nieustannie przejeżdżało z miasta do miasta, pokonując po drodze most (N przekazywane
jako argument linii poleceń).  "Miasto" jest to funkcja, którą wątki mogą wykonywać
niezależnie od siebie przez krótki, losowy czas (maks. kilka sekund). Zsynchronizuj
dostęp wątków do mostu:
- nie wykorzystując zmiennych warunkowych (tylko mutexy/semafory) [17 p]
- wykorzystując zmienne warunkowe (condition variables) [17 p]

Aby móc obserwować działanie programu, każdemu samochodowi przydziel numer. 
Program powinien wypisywać komunikaty według poniższego przykładu:

`A-5 10>>> [>> 4 >>] <<<4 6-B`

Oznacza to, że po stronie miasta A jest 15 samochodów z czego 10 czeka w kolejce 
przed mostem, przez most przejeżdża samochód z numerem 4 z miasta A do B, po stronie 
miasta B jest 10 samochodów, z czego 4 oczekują w kolejce przed mostem. Po uruchomieniu 
programu z parametrem -debug należy wypisywać całą zawartość kolejek po obu stronach 
mostu, nie tylko ilość samochodów. Komunikat należy wypisywać w momencie, kiedy w 
programie zmieni się którakolwiek z tych wartości.

# Samochody:
### Stany w jakich moga sie znajdowac samochody:
- A - w miescie A
- a - czeka na wjazd na most opuszczajac miasto A
- M - przejezdza przez most z A do B
- m - przejezdza przez most z B do A 
- B - w miescie B
- b - czeka na wjazd na most opuszczajac miasto B
- 0 - stan niezdefiniowany