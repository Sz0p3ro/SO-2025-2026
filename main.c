#include "common.h"

// Zmienne globalne (potrzebne do sprzatania w atexit/signal handler)
int shmid = -1;
int semid = -1;
int msgid = -1;
StanSklepu *stan_sklepu = NULL;

// Funkcja logowania (zapis do pliku z semaforem)
void loguj(const char *msg) {
        struct sembuf operacje[1];
        operacje[0].sem_num = SEM_LOG;
        operacje[0].sem_op = -1; // Czekaj (P)
        operacje[0].sem_flg = 0;

        if (semop(semid, operacje, 1) == -1 && errno != EINTR) perror("Log sem wait error");

        FILE *f = fopen("raport.txt", "a");
        if (f) {
                time_t now = time(NULL);
                char *t = ctime(&now);
                t[strlen(t)-1] = '\0'; // usun \n
                fprintf(f, "[%s] %s\n", t, msg);
                fclose(f);
        }

        operacje[0].sem_op = 1; // Zwolnij (V)
        if (semop(semid, operacje, 1) == -1 && errno != EINTR) perror("Log sem post error");
}

// Funkcja czyszczaca - wywolywana na koniec
void sprzataj() {
        printf("Sprzatanie zasobow IPC...\n");
        if (shmid != -1) {
                shmdt(stan_sklepu);
                shmctl(shmid, IPC_RMID, NULL);
        }
        if (semid != -1) {
                semctl(semid, 0, IPC_RMID);
        }
        if (msgid != -1) {
                msgctl(msgid, IPC_RMID, NULL);
        }
}

// Obsluga sygnalu Ctrl+C, zeby posprzatac jak przerwiemy recznie
void handle_sigint(int sig) {
        exit(0); // To wywola funkcje zarejestrowana w atexit(sprzataj)
}

int main() {
        // 1. Rejestracja sprzatania
        atexit(sprzataj);
        signal(SIGINT, handle_sigint);

        // 2. Tworzenie klucza
        key_t key = ftok(FTOK_PATH, ID_PROJEKTU);
        CHECK(key, "ftok");

        // 3. Tworzenie Pamieci Dzielonej
        shmid = shmget(key, sizeof(StanSklepu), IPC_CREAT | 0600);
        CHECK(shmid, "shmget");

        stan_sklepu = (StanSklepu*) shmat(shmid, NULL, 0);
        if (stan_sklepu == (void*)-1) { perror("shmat"); exit(1); }

        // Inicjalizacja pamieci (tylko raz, na poczatku!)
        stan_sklepu->liczba_klientow_w_sklepie = 0;
        stan_sklepu->kolejka_samoobslugowa_len = 0;
        stan_sklepu->koniec_symulacji = 0;
        // Domyslnie kasy stacjonarne zamkniete
        stan_sklepu->kasy_stacjonarne_status[0] = 0;
        stan_sklepu->kasy_stacjonarne_status[1] = 0;
        // Domyslnie samoobslugowe wolne
        for(int i=0; i<LICZBA_KAS_SAMOOBSLUGOWYCH; i++)
                stan_sklepu->kasy_samoobslugowe_status[i] = 0;

        // 4. Tworzenie Semaforow
        semid = semget(key, LICZBA_SEMAFOROW, IPC_CREAT | 0600);
        CHECK(semid, "semget");

        // Ustawienie wartosci poczatkowych semaforow na 1 (otwarte)
        for (int i = 0; i < LICZBA_SEMAFOROW; i++) {
                CHECK(semctl(semid, i, SETVAL, 1), "semctl SETVAL");
        }

        // 5. Tworzenie Kolejki Komunikatow
        msgid = msgget(key, IPC_CREAT | 0600);
        CHECK(msgid, "msgget");

        // Wyczysc stary plik raportu
        FILE *f = fopen("raport.txt", "w");
        if(f) { fprintf(f, "START SYMULACJI\n"); fclose(f); }

        printf("IPC zainicjalizowane. Start procesow...\n");

        // TU BEDA TWORZONE PROCESY (FORK)

        // Na razie symuluje oczekiwanie, zeby sprawdzic czy IPC dziala

        // tu bedzie wait() na procesy potomne
        printf("Nacisnij Ctrl+C aby zakonczyc i posprzatac...\n");
        while(1) sleep(1);

        return 0;
}
