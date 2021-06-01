#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>


sem_t printer;
pthread_mutex_t bridge;
pthread_cond_t next_one;

pthread_mutex_t ticket_mutex;
int current_number = 0, next_number = 0;

pthread_t *cars;
int *carsNumbers;
char *carsState;

short mode;
int N;
short debug;

const int bridgeLeaveDelayMillis = 500;

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


void printCurrentState() {

    int inA = 0, leavingA = 0, leavingB = 0, inB = 0, onBridge = -1, bridgeDirection = -1;

    // zliczenie odpowiednich wartosci na podstawie tablicy stanow samochodow
    int i;
    for (i = 0; i < N; i++) {
        switch(carsState[i]) {
            case 'A':
                inA += 1;
                break;
            case 'a':
                leavingA += 1;
                break;
            case 'M':
                onBridge = i+1;
                bridgeDirection = 0;
                break;
            case 'm':
                onBridge = i+1;
                bridgeDirection = 1;
                break;
            case 'b':
                leavingB += 1;
                break;
            case 'B':
                inB += 1;
                break;
        }
    }

    // wypisanie zformatowanego (i pokolorowanego) wiersza
    printf("\r");
    printf("\033[0;43;30mA\033[0m");
    printf("\033[0;40;37m-\033[0m");
    printf("%d ", inA);
    printf("\033[0;40;91m%d\033[0m", leavingA);
    //todo: wypisanie lity oczekujacych samochodow
    printf("\033[0;40;37m>>>\033[0m");

    if (onBridge >= 0 && bridgeDirection >= 0 && bridgeDirection <= 1) {
        if (bridgeDirection == 0) {
            printf(" [\033[0;40;37m>>\033[0m %d \033[0;40;37m>>\033[0m] ", onBridge);
        } else if (bridgeDirection == 1) {
            printf(" [\033[0;40;37m<<\033[0m %d \033[0;40;37m<<\033[0m] ", onBridge);
        }
    } else {
        printf(" [       ] ");
    }
    
    printf("\033[0;40;37m<<<\033[0m");
    //todo: wypisanie listy oczekujacych samochodow
    printf("\033[0;40;91m%d\033[0m", leavingB);
    printf(" %d", inB);
    printf("\033[0;40;37m-\033[0m");
    printf("\033[0;43;30mB\033[0m                         ");
    fflush(stdout);
}


// Funkcja samochodu (kazdy samochod wykonuje takie samo zadanie - 
// jezdzenie miedzy miastem A i B)
//
// Argument:
// [arg] - numer samochodu
void *driveAround(void *arg) {

    // odczytanie numeru samochodu
    int carNumber = *((int*)arg) + 1;

    // zmienna pomocnicza, przechowujaca numer biletu
    // samochodu (uzywana tylko dla tryb == 1)
    int ticket_number = -1;

    // zmienna pomocnicza
    char oldState = '0';

    // poczatek przechodzenia przez most
    while(1) {

        // ustawnienie samochodu do odpowiedniej kolejki
        // (jezeli jest to poczatek kolejnej iteracji, to
        // znaczy to, ze samochod chce przezjechac przez
        // most do drugiego miasta) - poprzez ustawienie
        // samochodu do odpowiedniej kolejki rozumiem
        // przestawienie stasnu konkretnego samochodu na
        // nowy wlasciwy (patrz README.md)
        //
        // nie ma potrzeby zablokowywania dostepu do tablicy
        // [carsState], bo kazdy samochodo uzyskuje dostep
        // do odpowiedniej sobie komorki w tablicy
        oldState = carsState[carNumber-1];
        if (oldState == 'A') {
            carsState[carNumber-1] = 'a';
        } else if (oldState == 'B') {
            carsState[carNumber-1] = 'b';
        }

        // zmienila sie stan jednego z samochodow, wiec
        // trzeba na nowo wypisac stan programu w konsoli
        if (sem_wait(&printer) == -1) {
            perror("Blad sem_wait (przed lock)");
        }
        printCurrentState();
        if (sem_post(&printer) == -1) {
            perror("Blad sem_post (przed lock)");
        }

        // jezeli tryb == 1 (na zmiennych warunkowych)
        // to nalezy pobrac bilet i czekac na wpuszczenie na most
        if (mode == 1) {
            pthread_mutex_lock(&ticket_mutex);
            ticket_number = next_number;
            next_number += 1;
            pthread_mutex_unlock(&ticket_mutex);
        }

        // przejscie do oczekiwania na zwolnienie mostu
        pthread_mutex_lock(&bridge);

            if (mode == 1) {
                while (current_number != ticket_number) {
                    pthread_cond_wait(&next_one, &bridge);
                }
            }

            // zapisanie stanu samochodu, zeby pozniej bylo wiadomo
            // do kotrego miasta jechal
            oldState = carsState[carNumber-1];
            
            // wjazd samochodu na most
            if (oldState == 'a') {
                carsState[carNumber-1] = 'M';
            } else if (oldState == 'b') {
                carsState[carNumber-1] = 'm';
            }

            // wypisanie obecnego stanu wszystkich samochodow
            if (sem_wait(&printer) == -1) {
                perror("Blad sem_wait (przed zwolnieniem mostu)");
            }
            printCurrentState();
            if (sem_post(&printer) == -1) {
                perror("Blad sem_post (przed zwolnieniem mostu)");
            }

            // pomocnicze opoznienie w celu pokazania, ze samochod
            // faktycznie przejechal przez most
            usleep(bridgeLeaveDelayMillis * 1000);

            // przejazd samochodu do odpowiedniego miasta
            if (oldState == 'a') {
                carsState[carNumber-1] = 'B';
            } else if (oldState == 'b') {
                carsState[carNumber-1] = 'A';
            }

            // wypisanie w konsoli nowego stanu systemu
            // (po wyjechaniu samochodu z mostu)
            if (sem_wait(&printer) == -1) {
                perror("Blad sem_wait (po zwolnieniu mostu)");
            }
            printCurrentState();
            if (sem_post(&printer) == -1) {
                perror("Blad sem_post (po zwolnieniu mostu)");
            }

            // odczekanie chwili po zwolnieniu mostu
            usleep(bridgeLeaveDelayMillis * 1000);

            if (mode == 1) {
                pthread_mutex_lock(&ticket_mutex);
                current_number += 1;
                pthread_mutex_unlock(&ticket_mutex);
                pthread_cond_broadcast(&next_one);
            }
        pthread_mutex_unlock(&bridge);

        sleep(rand() % 10 + 1);
    }

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

    // ustawienie generatora liczb pseudolowych
    srand(time(NULL));

    // argumenty i flagi wczytane z linii polece
    mode = -1;
    N = -1;
    debug = 0;

    // inicjalizacja semafora chroniacego wypisywanie na konsole
    if (sem_init(&printer, 0, 1) != 0) {
        perror("Blad sem_init");
        return EXIT_FAILURE;
    }

    // zaladowanie argumentow z linii polecen
    int result = loadCmdLineArgs(argc, argv, &mode, &N, &debug);
    if (result > 0) {
        printf("Blad przy czytaniu argumentow z linii polecen\n");
        return EXIT_FAILURE;
    }

    // dzialanie programu
    pthread_mutex_init(&bridge, NULL);
    if (mode == 1) {
        pthread_cond_init(&next_one, NULL);
    }

    // allokacja tablic
    cars = (pthread_t*)malloc(N * sizeof(pthread_t));
    carsNumbers = (int*)malloc(N * sizeof(int));
    carsState = (char*)malloc(N * sizeof(char));

    // zaladowanie wartosci wstepnych do tablicy carsState
    // przechowujacej stan w jakim znajduja sie poszczegolne
    // samochody (domyslnie na starcie wszystkie sa w miescie A)
    int i;
    for (i = 0; i< N; i++) {
        carsState[i] = 'A';
    }

    // utworzenie watkow
    for ( i = 0; i < N; ++i ) {
        carsNumbers[i] = i;
        pthread_create(&cars[i], NULL, driveAround, (void*)&carsNumbers[i]);
    }

    for ( i = 0; i < N; ++i ) {
        pthread_join(cars[i], NULL);
    }

    // zwolnienie niepotrzebnych zmiennych
    free(cars);
    free(carsNumbers);
    free(carsState);
    pthread_mutex_destroy(&bridge);

    return  EXIT_SUCCESS;
}