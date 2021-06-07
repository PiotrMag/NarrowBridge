#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>

// zmienne potrzebne do watkow
sem_t printer;
pthread_mutex_t bridge;
pthread_cond_t next_one;
pthread_t *cars;

// zmienne potrzebne w trybie 1
pthread_mutex_t ticket_mutex;
int current_number = 0, next_number = 0;

// zmienne z informacjami na temat samochodow
int *carsNumbers;
char *carsState;
int *carsTicket;

// zmienne informujace o ustawieniach calego programu
short mode;
int N;
short debug;
short fast;

// stale informujace o opoznieniach, jakie maja byc
// uwzglednione w programie
const int bridgeLeaveDelayMillis = 500; // ile milisekund ma trwac przejazd przez most
const int maxSleepDelaySeconds = 5; // ile maksymalnie sekund moze trwac pobyt w miescie

// prosta struktura przechowujaca dwie wartosci int
// potrzebna do przechowywania numeru biletu samochodu
// oraz numeru samochodu przy wypisywania list kolejnosci
// samochodow w kolejkach do mostu
typedef struct IntTuple {
    int first, second;
} IntTuple;

// struktura przechowujaca pojednyczy element listy
struct Node {
    IntTuple value;
    struct Node *next;
};
typedef struct Node Node;

// funkcja dodajaca element do listy, w takim miejscu
// zeby lista byla posortowana rosnaco wzgledem 
// IntTuple.first
//
// potrzebne do wyswietlania listy samochodow czekajacych 
// na wjazd na most od strony miasta B
void list_add_ascending(Node **starting_node, Node *new_node) {
    if (*starting_node == NULL) {
        *starting_node = new_node;
    } else {
        Node *current_node = *starting_node;
        if (current_node->value.first > new_node->value.first) {
            *starting_node = new_node;
            new_node->next = current_node;
        } else {
            while (current_node->next != NULL) {
                if (current_node->next->value.first > new_node->value.first) {
                    break;
                }
                current_node = current_node->next;
            }
            new_node->next = current_node->next;
            current_node->next = new_node;
        }
    }
}

// funkcja dodajaca element do listy, w takim miejscu
// zeby lista byla posortowana malejaco wzgledem 
// IntTuple.first
//
// potrzebne do wyswietlania listy samochodow czekajacych 
// na wjazd na most od strony miasta A
void list_add_descending(Node **starting_node, Node *new_node) {
    if (*starting_node == NULL) {
        *starting_node = new_node;
    } else {
        Node *current_node = *starting_node;
        if (current_node->value.first <= new_node->value.first) {
            *starting_node = new_node;
            new_node->next = current_node;
        } else {
            while (current_node->next != NULL) {
                if (current_node->next->value.first <= new_node->value.first) {
                    break;
                }
                current_node = current_node->next;
            }
            new_node->next = current_node->next;
            current_node->next = new_node;
        }
    }
}

// funkcja usuwajaca wszystkie elementy listy (zwalnia pamiec)
void delete_list(Node **startingNode) {
    // printf("del\n");
    Node *current_node = *startingNode;
    Node *next_node;
    while (current_node != NULL)
    {
        next_node = current_node->next;
        free(current_node);
        current_node = next_node;
    }
    *startingNode = NULL;
}

// Funkcja ladujaca argumenty z lini polecen
//
// Wartosci argumentow sa loadowane do odpowiednich zmiennych
//
// Zwraca:
// 1 <- jezeli byl blad
// 0 <- jezeli bylo wszystko OK
int loadCmdLineArgs(int argc, char *argv[], short* mode, int* N, short* debug, short* fast) {

    // sprwdzenie, czy zostala podana minimalna wymnagana ilosc argumentow
    if (argc < 3) {
        printf("Za malo argumentow\nMusi byc: tryb N [-debug] [-fast]\n");
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
    // lub -fast
    int i;
    for (i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-debug") == 0) {
            *debug = 1;
        } else if (strcmp(argv[i], "-fast") == 0) {
            *fast = 1;
        }
    }

    return EXIT_SUCCESS;
}

// Funkcja wyswietlajaca obecny stan systemu (samochodow)
//
// W zaleznosci od wartosci zmiennej globalnej [debug]
// Wyswietlana jest lista samochodow oczekujacych po
// obu stronach mostu
//
// W przypadku trybu 0 opcja ta pokazuje kolejki, ale
// nie musza one byc prezentowane w formie wiazacej
// (w trybie 0 nie ma pilnowania kolejnosci przejazdu
// samochodow - dany samochod moze miec losowo przydzielony
// dostep do mostu, ale moze sie tez zdarzyc w trybie 0
// ze dany samochod nie dostanie dostepu do mostu)
//
// W przypadku trybu 1 opcja [debug] ma sens, bo
// w trybie 1 jest kolejkowanie samochodow przy
// uzyciu systemu biletowego (samochod w momencie
// ustawiania sie w kolejce do przejazdu przez most
// otrzymuje bilet z numerkiem, na bazie ktorego
// pozniej otrzyma pozwolenie na przejazd przez most)
void printCurrentState() {

    int inA = 0, leavingA = 0, leavingB = 0, inB = 0, onBridge = -1, bridgeDirection = -1;
    Node *startingANode = NULL, *startingBNode = NULL;

    // zliczenie odpowiednich wartosci na podstawie tablicy stanow samochodow
    int i;
    for (i = 0; i < N; i++) {
        switch(carsState[i]) {
            case 'A':
                inA += 1;
                break;
            case 'a':
                leavingA += 1;
                if (debug && carsTicket[i] >= 0) {
                    Node *new_node = (Node*)malloc(sizeof(Node));
                    if (new_node != NULL) {
                        if (mode == 0) {
                            new_node->value.first = 0;
                        } else {
                            new_node->value.first = carsTicket[i];
                        }
                        new_node->value.second = i+1;
                        list_add_descending(&startingANode, new_node);
                    }
                }
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
                if (debug && carsTicket[i] >= 0) {
                    Node *new_node = (Node*)malloc(sizeof(Node));
                    if (new_node != NULL) {
                        if (mode == 0) {
                            new_node->value.first = 0;
                        } else {
                            new_node->value.first = carsTicket[i];
                        }
                        new_node->value.second = i+1;
                        list_add_ascending(&startingBNode, new_node);
                    }
                }
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
    if (debug) {
        printf("[");
        Node *tmp = startingANode;
        while (tmp != NULL) {
            printf("%d ", tmp->value.second);
            tmp = tmp->next;
        }
        printf("] ");
    }
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
    if (debug) {
        printf(" [");
        Node *tmp = startingBNode;
        while (tmp != NULL) {
            printf("%d ", tmp->value.second);
            tmp = tmp->next;
        }
        printf("]");
    }
    printf("\033[0;40;91m%d\033[0m", leavingB);
    printf(" %d", inB);
    printf("\033[0;40;37m-\033[0m");
    printf("\033[0;43;30mB\033[0m                         ");
    fflush(stdout);

    if (debug) {
        delete_list(&startingANode);
        delete_list(&startingBNode);
    }
}

// Funkcja odwiedzania miasta
//
// Sprowadza sie do odczekania odpowiedniego czasu
// w miescie (przez samochod)
void visitCity() {
    if (!fast) {
        sleep(rand() % (maxSleepDelaySeconds+1));
    }
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

        // jezeli tryb == 1 (na zmiennych warunkowych)
        // to nalezy pobrac bilet i czekac na wpuszczenie na most
        if (mode == 1) {
            if (pthread_mutex_lock(&ticket_mutex) == 0) {
                ticket_number = next_number;
                carsTicket[carNumber-1] = ticket_number;
                next_number += 1;
                if (pthread_mutex_unlock(&ticket_mutex) != 0) {
                    printf("\nNie udalo sie pthread_mutex_unlock, samochod=%d\n", carNumber);
                    exit(EXIT_FAILURE);
                }
            } else {
                printf("\nNie udalo sie pthread_mutex_lock, samochod=%d\n", carNumber);
            }
        }

        // zmienil sie stan jednego z samochodow, wiec
        // trzeba na nowo wypisac stan programu w konsoli
        if (sem_wait(&printer) == -1) {
            perror("\nBlad sem_wait (przed lock)\n");
        }
        printCurrentState();
        if (sem_post(&printer) == -1) {
            perror("\nBlad sem_post (przed lock)\n");
        }

        // przejscie do oczekiwania na zwolnienie mostu
        if (pthread_mutex_lock(&bridge) == 0) {

            if (mode == 1) {
                while (current_number != ticket_number) {
                    if (pthread_cond_wait(&next_one, &bridge) != 0) {
                        printf("\nBlad dla pthread_cond_wait, samochod=%d\n", carNumber);
                    }
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
                perror("\nBlad sem_wait (przed zwolnieniem mostu)\n");
            }
            printCurrentState();
            if (sem_post(&printer) == -1) {
                perror("\nBlad sem_post (przed zwolnieniem mostu)\n");
            }

            // pomocnicze opoznienie w celu pokazania, ze samochod
            // faktycznie przejechal przez most (ale tylko w trybie nie fast)
            if (!fast) {
                usleep(bridgeLeaveDelayMillis * 1000);
            }

            // przejazd samochodu do odpowiedniego miasta
            if (oldState == 'a') {
                carsState[carNumber-1] = 'B';
            } else if (oldState == 'b') {
                carsState[carNumber-1] = 'A';
            }

            // wypisanie w konsoli nowego stanu systemu
            // (po wyjechaniu samochodu z mostu)
            if (sem_wait(&printer) == -1) {
                perror("\nBlad sem_wait (po zwolnieniu mostu)\n");
            }
            printCurrentState();
            if (sem_post(&printer) == -1) {
                perror("\nBlad sem_post (po zwolnieniu mostu)\n");
            }

            // odczekanie chwili po zwolnieniu mostu (w trybie nie fast)
            if (!fast) {
                usleep(bridgeLeaveDelayMillis * 1000);
            }

            if (mode == 1) {
                if (pthread_mutex_lock(&ticket_mutex) == 0) {
                    current_number += 1;
                    carsTicket[carNumber-1] = -1; // oznaczenie, ze bilet zostal wykorzystany
                    if (pthread_mutex_unlock(&ticket_mutex) != 0) {
                        printf("\nNie udalo sie pthread_mutex_unlock, samochod=%d\n", carNumber);
                        exit(EXIT_FAILURE);
                    }
                } else {
                    printf("\nNie udalo sie pthread_mutex_lock, samochod=%d\n", carNumber);
                }
                if (pthread_cond_broadcast(&next_one) != 0) {
                    printf("\nBlad dla pthread_cond_broadcast, samochod=%d\n", carNumber);
                }
            }
            
            if (pthread_mutex_unlock(&bridge) != 0) {
                printf("\nNie udalo sie zwolnic mostu, samochod=%d\n", carNumber);
                exit(EXIT_FAILURE);
            }

        } else {
            printf("\nNie udalo sie wjechac na most, samochod=%d\n", carNumber);
        }

        visitCity();
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
    fast = 0;

    // inicjalizacja semafora chroniacego wypisywanie na konsole
    if (sem_init(&printer, 0, 1) != 0) {
        perror("Blad sem_init\n");
        return EXIT_FAILURE;
    }

    // zaladowanie argumentow z linii polecen
    int result = loadCmdLineArgs(argc, argv, &mode, &N, &debug, &fast);
    if (result > 0) {
        printf("Blad przy czytaniu argumentow z linii polecen\n");
        return EXIT_FAILURE;
    }

    // dzialanie programu
    if (pthread_mutex_init(&bridge, NULL) != 0) {
        printf("Blad przy pthread_mutex_init!\n");
        return EXIT_FAILURE;
    } 
    if (mode == 1) {
        if (pthread_mutex_init(&ticket_mutex, NULL) != 0) {
            printf("Blad przy pthread_mutex_init!\n");
            return EXIT_FAILURE;
        }
        if (pthread_cond_init(&next_one, NULL) != 0) {
            printf("Blad przy pthread_cond_init!\n");
            return EXIT_FAILURE;
        }
    }

    // allokacja tablic
    cars = (pthread_t*)malloc(N * sizeof(pthread_t));
    if (cars == NULL) { 
        printf("Nie udalo sie zaallokowac cars\n"); 
        return EXIT_FAILURE; 
    }
    carsNumbers = (int*)malloc(N * sizeof(int));
    if (carsNumbers == NULL) { 
        printf("Nie udalo sie zaallokowac carsNumbers\n"); 
        return EXIT_FAILURE; 
    }
    carsState = (char*)malloc(N * sizeof(char));
    if (carsState == NULL) { 
        printf("Nie udalo sie zaallokowac carsState\n"); 
        return EXIT_FAILURE; 
    }
    carsTicket = (int*)malloc(N * sizeof(int));
    if (carsTicket == NULL) { 
        printf("Nie udalo sie zaallokowac carsTicket\n"); 
        return EXIT_FAILURE; 
    }

    // zaladowanie wartosci wstepnych do tablicy carsState
    // przechowujacej stan w jakim znajduja sie poszczegolne
    // samochody (domyslnie na starcie wszystkie sa w miescie A)
    int i;
    for (i = 0; i< N; i++) {
        carsState[i] = 'A';
    }

    // zaladowanie wartosci startowych do tablicy przechowujacej
    // numery biletow poszczegolnych samochodow
    for (i = 0; i < N; i++) {
        carsTicket[i] = -1;
    }

    // utworzenie watkow
    for ( i = 0; i < N; ++i ) {
        carsNumbers[i] = i;
        if (pthread_create(&cars[i], NULL, driveAround, (void*)&carsNumbers[i]) != 0) {
            printf("Blad tworzenia pthread (create), i=%d\n", i);
            return EXIT_FAILURE;
        }
    }

    for ( i = 0; i < N; ++i ) {
        if (pthread_join(cars[i], NULL) != 0) {
            printf("Blad przy dolaczaniu do watkow (join), i=%d\n", i);
            return EXIT_FAILURE;
        }
    }

    // zwolnienie niepotrzebnych zmiennych
    free(cars);
    free(carsNumbers);
    free(carsState);
    free(carsTicket);
    if (pthread_mutex_destroy(&bridge) != 0) {
        printf("Nie udalo sie usunac mutexa mostu (destory bridge)!\n");
        return EXIT_FAILURE;
    }

    return  EXIT_SUCCESS;
}