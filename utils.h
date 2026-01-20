#ifndef UTILS_H
#define UTILS_H

#include "common.h"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

typedef enum {
	KOLOR_RESET,
	KOLOR_CZERWONY,   // kierownik
	KOLOR_ZIELONY,    // samoobslugowa
	KOLOR_NIEBIESKI,  // stacjonarna
	KOLOR_ZOLTY,      // klienci
	KOLOR_CYJAN       // awaria/alkohol/obsluga
} LogColor;

// funkcja do obslugi bledow krytycznych
void handle_error(const char *msg);

// funkcja wysylajaca logi do pliku
void loguj(int semid, const char *msg, LogColor color);

// Funkcja generujaca paragon
float wystaw_paragon(int semid, int nr_klienta, int *koszyk, int liczba_produktow);

#endif