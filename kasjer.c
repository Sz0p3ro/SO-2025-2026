#include "common.h"
#include "utils.h"

int semid, msgid, shmid;
StanSklepu *stan_sklepu;

int main(int argc, char *argv[]) {
	// podlaczenie do struktur wspoldzielonych
	key_t key = ftok(FTOK_PATH, ID_PROJEKTU);
	shmid = shmget(key, sizeof(StanSklepu), 0600);
	stan_sklepu = (StanSklepu*) shmat(shmid, NULL, 0);
	semid = semget(key, LICZBA_SEMAFOROW, 0600);
	 msgid = msgget(key, 0600);

	// pobranie numeru kasy z argumentu
	if (argc < 2) return 1;
	int nr_kasy = atoi(argv[1]);
	long my_mtype = 2 + nr_kasy;

	printf("Kasa Stacjonarna S%d startuje (PID: %d, mtype: %ld)\n", nr_kasy + 1, getpid(), my_mtype);

        char msg_buf[200];
        Komunikat kom_odb;
        Komunikat kom_nad;
        struct sembuf operacje[1];
        int czas_bezczynnosci = 0; // Licznik czasu bezczynnosci

        while (1) {
                // 1. Sprawdzenie czy kasa jest OTWARTA
                int status;
                int len_kolejki;

                operacje[0].sem_num = SEM_STAN;
                operacje[0].sem_op = -1;
                semop(semid, operacje, 1);

                status = stan_sklepu->kasy_stacjonarne_status[nr_kasy];
                len_kolejki = stan_sklepu->kolejka_stacjonarna_len[nr_kasy];

                operacje[0].sem_op = 1;
                semop(semid, operacje, 1);

                if (status == 0 && len_kolejki == 0) {
                        // Kasa zamknieta - kasjer czeka na decyzje kierownika
                        czas_bezczynnosci = 0; // Reset licznika jak jest zamknieta
                        usleep(500000); // 0.5s
                        continue;
                }

                // 2. Proba pobrania klienta
                // Uzywamy IPC_NOWAIT, zeby sprawdzic czy ktos jest, a jak nie ma, to liczyc czas
                if (msgrcv(msgid, &kom_odb, sizeof(Komunikat) - sizeof(long), my_mtype, IPC_NOWAIT) == -1) {
                        if (errno == ENOMSG) {
                                if (status == 0) {
                                        // czekamy na kolejny obieg petli, ktory zablokuje sie wyzej
                                        usleep(100000);
                                        continue;
                                }

                                // Brak klientow w kolejce
                                sleep(1); // Czekamy 1 sekunde
                                czas_bezczynnosci++;

                                // Jesli czekamy juz 30 sekund, zamykamy kase
                                if (czas_bezczynnosci >= 30) {
                                        sprintf(msg_buf, "Kasa Stacjonarna S%d: Brak klientow od 30s. Zamykam kase.", nr_kasy + 1);
                                        loguj(semid, msg_buf);

                                        // Zamykanie kasy
                                        operacje[0].sem_num = SEM_STAN;
                                        operacje[0].sem_op = -1;
                                        semop(semid, operacje, 1);

                                        stan_sklepu->kasy_stacjonarne_status[nr_kasy] = 0;

                                        operacje[0].sem_op = 1;
                                        semop(semid, operacje, 1);
                                }
                                continue;
                        }
                        else if (errno == EIDRM || errno == EINVAL) { // w przypadku ewakuacji
                                // wyjscie
                                exit(0);
                        }
                        else {
                                perror("msgrcv stacjonarna");
                                exit(1);
                        }
                }

                // 3. Obsluga klienta (jesli msgrcv cos zwrocil)
                czas_bezczynnosci = 0; // Reset licznika bo kasjer pracuje

                operacje[0].sem_num = SEM_STAN;
                operacje[0].sem_op = -1;
                semop(semid, operacje, 1);

                stan_sklepu->kasy_stacjonarne_status[nr_kasy] = kom_odb.id_klienta; // PID klienta
                if (stan_sklepu->kolejka_stacjonarna_len[nr_kasy] > 0)
                        stan_sklepu->kolejka_stacjonarna_len[nr_kasy]--;

                operacje[0].sem_op = 1;
                semop(semid, operacje, 1);

                sprintf(msg_buf, "Kasa Stacjonarna S%d: Obsluguje klienta %d (Prod: %d)",
                                nr_kasy + 1, kom_odb.id_klienta, kom_odb.liczba_produktow);
                loguj(semid, msg_buf);

                // Symulacja kasowania (0.2s na produkt)
                usleep(kom_odb.liczba_produktow * 200000);

                float zaplacono = wystaw_paragon(semid, kom_odb.nr_klienta_sklepu, kom_odb.koszyk, kom_odb.liczba_produktow); // generowanie paragonu

                // Potwierdzenie
                kom_nad.mtype = kom_odb.id_klienta;
                kom_nad.id_klienta = 100 + nr_kasy; // ID Kasy (100, 101)
                kom_nad.koszt_zakupow = zaplacono;

                if (msgsnd(msgid, &kom_nad, sizeof(Komunikat) - sizeof(long), 0) == -1) {
                        if (errno == EIDRM || errno == EINVAL) exit(0);
                        perror("msgsnd stacjonarna reply");
                }

                // Zwolnienie kasy (Status 1 = OTWARTA, gotowa)
                operacje[0].sem_num = SEM_STAN;
                operacje[0].sem_op = -1;
                semop(semid, operacje, 1);

                // Sprawdzamy czy w miedzyczasie kierownik nie zamknal kasy
                if (stan_sklepu->kasy_stacjonarne_status[nr_kasy] != 0)
                        stan_sklepu->kasy_stacjonarne_status[nr_kasy] = 1;

                operacje[0].sem_op = 1;
                semop(semid, operacje, 1);
        }
}