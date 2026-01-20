#include "common.h"
#include "utils.h"

int semid, shmid;
StanSklepu *stan_sklepu;

int main() {
	// podlaczenie do struktur wspoldzielonych
	key_t key = ftok(FTOK_PATH, ID_PROJEKTU);
	shmid = shmget(key, sizeof(StanSklepu), 0600);
	stan_sklepu = (StanSklepu*) shmat(shmid, NULL, 0);
	semid = semget(key, LICZBA_SEMAFOROW, 0600);

	printf("Pracownik obslugi zaczyna prace (PID: %d)\n", getpid());

        struct sembuf operacje[1];
        char msg_buf[100];

        while (1) {
                // Pracownik co chwile sprawdza wszystkie kasy
                for (int i = 0; i < LICZBA_KAS_SAMOOBSLUGOWYCH; i++) {

                        // 1. Sprawdzenie czy jest awaria?
                        int status;
                        int ewakuacja;

                        operacje[0].sem_num = SEM_STAN;
                        operacje[0].sem_op = -1; // P
                        operacje[0].sem_flg = 0;
                        semop(semid, operacje, 1);

                        status = stan_sklepu->kasy_samoobslugowe_status[i];
                        ewakuacja = stan_sklepu->ewakuacja;

                        operacje[0].sem_op = 1; // V
                        semop(semid, operacje, 1);

                        if (ewakuacja) exit(0);

                        // 2. Reakcja na awarie (-1)
                        if (status == -1) {
                                sprintf(msg_buf, "Obsluga: Wykryto awarie w kasie %d. Naprawiam...", i + 1);
                                loguj(semid, msg_buf, KOLOR_CYJAN);

                                // Symulacja naprawy (2s.)
                                sleep(2);

                                operacje[0].sem_num = SEM_STAN;
                                operacje[0].sem_op = -1;
                                semop(semid, operacje, 1);

                                // sprawdzam czy podczas naprawy nie bylo ogloszonej ewakuacji
                                if (stan_sklepu->ewakuacja) {
                                        operacje[0].sem_op = 1;
                                        semop(semid, operacje, 1);
                                        exit(0);
                                }

                                // Zmiana statusu na 0 (WOLNA)
                                stan_sklepu->kasy_samoobslugowe_status[i] = 0; // Odblokowanie kasy

                                operacje[0].sem_op = 1;
                                semop(semid, operacje, 1);

                                sprintf(msg_buf, "Obsluga: Kasa %d naprawiona.", i + 1);
                                loguj(semid, msg_buf, KOLOR_CYJAN);
                        }

                        // 3. Reakcja na alkohol (-2)
                        else if (status == -2) {
                                sprintf(msg_buf, "Obsluga: Kasa %d wola do alkoholu. Ide zatwierdzic...", i + 1);
                                loguj(semid, msg_buf, KOLOR_CYJAN);

                                sleep(1); // Symulacja zatwierdzania (1s.)

                                operacje[0].sem_num = SEM_STAN;
                                operacje[0].sem_op = -1;
                                semop(semid, operacje, 1);

                                if (stan_sklepu->ewakuacja) {
                                        operacje[0].sem_op = 1;
                                        semop(semid, operacje, 1);
                                        exit(0); // ucieczka w razie ewakuacji
                                }

                                stan_sklepu->kasy_samoobslugowe_status[i] = 1; // kasa zajeta
                                operacje[0].sem_op = 1;
                                semop(semid, operacje, 1);

                                sprintf(msg_buf, "Obsluga: Alkohol w kasie %d zatwierdzony.", i + 1);
                                loguj(semid, msg_buf, KOLOR_CYJAN);
                        }
                }

                // przerwa miedzy obchodami sklepu
                usleep(500000); // 0.5 sekundy
        }
}