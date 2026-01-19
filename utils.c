#include "utils.h"

// funkcja do obslugi bledow
void handle_error(const char *msg) {
	perror(msg);
	exit(EXIT_FAILURE);
}

// funkcja zapisujaca  logi do pliku
void loguj(int semid, const char *msg) {
	struct sembuf operacje[1];
	operacje[0].sem_num = SEM_LOG;
	operacje[0].sem_op = -1; // Czekaj (P)
	operacje[0].sem_flg = 0;

	if (semop(semid, operacje, 1) == -1 && errno != EINTR) {
	    perror("Log sem wait error");
	}

	FILE *f = fopen("raport.txt", "a");
	if (f) {
		time_t now = time(NULL);
	    char *t = ctime(&now);
		t[strlen(t)-1] = '\0'; // usun \n
		fprintf(f, "[%s] %s\n", t, msg);
		fclose(f);
	}

	operacje[0].sem_op = 1; // Zwolnij (V)
	if (semop(semid, operacje, 1) == -1 && errno != EINTR) {
		perror("Log sem post error");
	}
}

// funkcja zapisujaca paragon do pliku
float wystaw_paragon(int semid, int nr_klienta, int *koszyk, int liczba_produktow) {
        struct sembuf operacje[1];

        operacje[0].sem_num = SEM_LOG;
        operacje[0].sem_op = -1;
        operacje[0].sem_flg = 0;
        semop(semid, operacje, 1);

        FILE *f = fopen("paragony.txt", "a");
        float suma = 0.0;

        if (f) {
                fprintf(f, "------------------------------------------\n");
                fprintf(f, " PARAGON - KLIENT NR: %d\n", nr_klienta);
                fprintf(f, "------------------------------------------\n");
                fprintf(f, "%-15s | %-6s | %s\n", "NAZWA", "ILOSC", "CENA");
                fprintf(f, "------------------------------------------\n");

                // Zliczanie ilosci produktow
                int ilosci[LICZBA_TYPOW_PRODUKTOW] = {0};
                for(int i=0; i<liczba_produktow; i++) {
                        int idx = koszyk[i];
                        if(idx >= 0 && idx < LICZBA_TYPOW_PRODUKTOW) {
                                ilosci[idx]++;
                        }
                }

                // Wypisywanie
                for(int i=0; i<LICZBA_TYPOW_PRODUKTOW; i++) {
                        if (ilosci[i] > 0) {
                                float wartosc = ilosci[i] * BAZA_PRODUKTOW[i].cena;
                                suma += wartosc;
                                fprintf(f, "%-15s | x%-5d | %.2f PLN\n",
                                        BAZA_PRODUKTOW[i].nazwa, ilosci[i], wartosc);
                        }
                }

                fprintf(f, "------------------------------------------\n");
                fprintf(f, " RAZEM:                       %.2f PLN\n", suma);
                fprintf(f, "------------------------------------------\n\n");
                fclose(f);
        }

        operacje[0].sem_op = 1;
        semop(semid, operacje, 1);

        return suma;
}
