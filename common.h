#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <string.h>

// --- KONFIGURACJA ---
#define FTOK_PATH "."
#define ID_PROJEKTU 10

// Limity
#define LICZBA_KAS_STACJONARNYCH 2
#define LICZBA_KAS_SAMOOBSLUGOWYCH 6
#define MAX_KLIENTOW_W_SKLEPIE 50 // wczytane N (domyslne 50)
#define LIMIT_KLIENTOW_NA_KASE 5  // wczytane  K (domyslne 5)

// --- STRUKTURY DANYCH ---

// Pamiec wspoldzielona - Stan Sklepu
// Dostep choniony semaforem (SEM_STAN)!
typedef struct {
        int liczba_klientow_w_sklepie;

        // Liczniki kolejek (dla Kierownika)
        int kolejka_samoobslugowa_len;
        int kolejka_stacjonarna_len[LICZBA_KAS_STACJONARNYCH];

        // Statusy kas
        // Stacjonarne: 0 = zamknieta 1 = otwarta
        int kasy_stacjonarne_status[LICZBA_KAS_STACJONARNYCH];

        // Samoobslugowe: 0 = wolna, pid_klienta = zajeta, -1 = zablokowana (wymaga pomocy)
        int kasy_samoobslugowe_status[LICZBA_KAS_SAMOOBSLUGOWYCH];

        // Flaga dla Kierownika, aby wiedzial kiedy zakonczyc symulacje
        int koniec_symulacji;
} StanSklepu;

// Komunikat w kolejce komunikatow
// Klienci wysylaja 'chce podejsc do kasy;
// Kasy odbieraja 'obsluguje cie'
typedef struct {
        long mtype;       // 1 = do samoobslugowej, 2 = do stacjonarnej 1, 3 = do stacjonarnej 2
        pid_t id_klienta; // PID procesu klienta
        int liczba_produktow;
        int czy_alkohol;  // 1 = tak, 0 = nie
} Komunikat;

// --- SEMAFORY ---
// Definicje indeks w tablicy semaforow
#define SEM_STAN 0   // Chroni dostep do pamieci wspoldzielonej (StanSklepu)
#define SEM_LOG 1    // Chroni dostep do pliku logu
#define LICZBA_SEMAFOROW 2

// --- MAKRA POMOCNICZE ---
// obsluga bledow
#define CHECK(x, msg) \
        if ((x) == -1) { \
        perror(msg); \
        exit(EXIT_FAILURE); \
        }

#endif