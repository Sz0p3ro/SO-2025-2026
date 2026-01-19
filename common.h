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
#include <sys/wait.h>
#include <sys/time.h>

// --- KONFIGURACJA ---
#define FTOK_PATH "."
#define ID_PROJEKTU 10

// Limity
#define MAX_PRODUKTOW 10
#define MIN_PRODUKTOW 3
#define LICZBA_KAS_STACJONARNYCH 2
#define LICZBA_KAS_SAMOOBSLUGOWYCH 6
#define DEFAULT_MAX_KLIENTOW 50 // wczytane N (domyslne 50)
#define DEFAULT_LIMIT_NA_KASE 5 // wczytane K (domyslne 5)
#define LIMIT_CIERPLIWOSCI 5 // prog po ktorego przekroczeniu klient przejdzie do kasy stacjonarnej
#define LICZBA_TYPOW_PRODUKTOW 10
// --- STRUKTURY DANYCH ---

typedef struct {
	char nazwa[20];
	float cena;
	int is_alkohol; // 1 - tak
} Produkt;

static const Produkt BAZA_PRODUKTOW[10] = {
	{"Bulka", 0.60, 0},
	{"Chleb", 4.15, 0},
	{"Maslo", 6.50, 0},
	{"Mleko", 3.90, 0},
	{"Jajka", 11.00, 0},
	{"Szynka", 14.00, 0},
	{"Jablka", 3.20, 0},
	{"Czekolada", 5.50, 0},
	{"Piwo", 4.00, 1},    //
	{"Wodka", 35.00, 1}   //
};

#define LICZBA_TYPOW_PRODUKTOW 10

// Pamiec wspoldzielona - Stan Sklepu
// Dostep choniony semaforem (SEM_STAN)!
typedef struct {
	int liczba_klientow_w_sklepie;
	int id_generator; // licznik klientow ktory tylko rosnie

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
	int ewakuacja;

	int max_klientow_sklep; // N
	int limit_klientow_kasa; // K
	pid_t pid_kierownik;
} StanSklepu;

// Komunikat w kolejce komunikatow
// Klienci wysylaja 'chce podejsc do kasy'
// Kasy odbieraja 'obsluguje cie'
typedef struct {
	long mtype; // 1 = do samoobslugowej, 2 = do stacjonarnej 1, 3 = do stacjonarnej 2
	pid_t id_klienta; // PID procesu klienta
	int nr_klienta_sklepu; // do paragonow
	int liczba_produktow;
	int czy_alkohol; // 1 = tak, 0 = nie
	float koszt_zakupow;
	int koszyk[MAX_PRODUKTOW]; //indeksy produktow z bazy
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