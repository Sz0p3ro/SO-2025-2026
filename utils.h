#ifndef UTILS_H
#define UTILS_H

#include "common.h"

// funkcja do obslugi bledow krytycznych
void handle_error(const char *msg);

// funkcja wysylajaca logi do pliku
void loguj(int semid, const char *msg);

// funkcja generujaca paragon
float wystaw_paragon(int semid, int nr_klienta, int *koszyk, int liczba_produktow);

#endif