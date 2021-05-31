#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

// Funkcja ladujaca argumenty z lini polecen
//
// Wartosci argumentow sa loadowane do odpowiednich zmiennych
//
// Zwraca:
// 1 <- jezeli byl blad
// 0 <- jezeli bylo wszystko OK
int loadCmdLineArgs(int argc, char *argv[], short* mode, int* N, short* debug) {

    // sprwdzenie, czy zostala podana minimalna wymnagana ilosc argumentow
    if (argc < 3) {
        printf("Za malo argumentow\nMusi byc: tryb N [-debug]\n");
        return EXIT_FAILURE;
    }

    // sprawdzenie pierwszego arguemntu - trybu pracy
    if (strcmp(argv[1], "0") == 0) {
        *mode = 0;
    } else if (strcmp(argv[1], "1") == 0) {
        *mode = 1;
    } else {
        printf("tryb - musi byc 0 lub 1\n");
        return EXIT_FAILURE;
    }

    // sprawdzenie drugiego argumentu - liczby samochodow
    *N = atoi(argv[2]);
    if (*N <= 0) {
        printf("N - liczba samochodow musi byc >0\n");
        return EXIT_FAILURE;
    }

    // ewentualne sprawdzenie, czy podana zostala flaga [-debug]
    if (argc >= 4) {
        if (strcmp(argv[3], "-debug") == 0) {
            *debug = 1;
        }
    }

    return EXIT_SUCCESS;
}


// Funkcja samochodu (kazdy samochod wykonuje takie samo zadanie - 
// jezdzenie miedzy miastem A i B)
//
// Argument:
// [arg] - numer samochodu
void *driveAround(void *arg) {

    // odczytanie numeru samochodu
    int carNumber = *((int*)arg) + 1;

    // testowanie
    printf("%d\n", carNumber);

    return 0;
}


// Do prawidowego dziaania funkcji main powinny byc
// przekazane z konsoli dwa argumenty [tryb] i [N]
// gdzie:
// [tryb] <- mowi o tym, czy program ma dzialac na
//           mutexach/semaforach (== 0), czy na zmiennych
//           warunkowych (== 1)
// [N] <- mowi o tym, ile jest samochodow
//
// Opcjonalnie mozna podac parametr [-debug], ktory powoduje
// wypisywanie wieksdzej ilosci informacji w trakcie dzialania
// programu
int main (int argc, char *argv[]) {

    // argumenty i flagi wczytane z linii polece
    short mode = -1;
    int N = -1;
    short debug = 0;

    // zaladowanie argumentow z linii polecen
    int result = loadCmdLineArgs(argc, argv, &mode, &N, &debug);
    if (result > 0) {
        printf("Blad przy czytaniu argumentow z linii polecen\n");
        return EXIT_FAILURE;
    }

    // dzialanie programu
    if (mode == 0) {

        // utworzenie zmiennych potrzebnych do prawidlowego dzialania mutexow
        pthread_mutex_t bridge;
        pthread_t cars[N]; 
        int carNumber[N];

        pthread_mutex_init(&bridge, NULL);

        // utworzenie watkow
        int i;
        for ( i = 0; i < N; ++i ) {
            carNumber[i] = i;
            pthread_create(&cars[i], NULL, driveAround, (void*)&carNumber[i]);
        }

        for ( i = 0; i < N; ++i ) {
            pthread_join(cars[i], NULL);
        }

        // zwolnienie niepotrzebnych zmiennych
        pthread_mutex_destroy(&bridge);

        return  EXIT_SUCCESS;
    }


    // jezeli zaden z warunkuow if'a sie nie wykonal to znaczy
    // ze byl jakis blad
    return EXIT_FAILURE;
}