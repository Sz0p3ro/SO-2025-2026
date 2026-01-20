#include "common.h"
#include "utils.h"

int semid, msgid, shmid;
StanSklepu *stan_sklepu;

int main(int argc, char *argv[]) {
	// podlaczenie do struktur wspoldzielonych
	key_t key = ftok(FTOK_PATH, ID_PROJEKTU);
	shmid = shmget(key, sizeof(StanSklepu), 0600);
	stan_sklepu = (StanSklepu*) shmat(shmid, NULL, 0); // podlaczenie pamieci wspoldzielonej
	semid = semget(key, LICZBA_SEMAFOROW, 0600); // podlaczenie semaforow
	msgid = msgget(key, 0600); // podlaczenie kolejki komunikatow

	if (argc < 2) return 1;
	int nr_kasy = atoi(argv[1]);

	// nr_kasy to indeks od 0 do 5
        printf("Kasa samoobslugowa %d startuje (PID: %d)\n", nr_kasy + 1, getpid());

        char msg_buf[200];
        Komunikat kom_odb;
        Komunikat kom_nad;
        struct sembuf operacje[1];

        // Ziarno losowosci unikalne dla procesu
        struct timeval t;
        gettimeofday(&t, NULL);
        srand(t.tv_usec ^ t.tv_sec ^ getpid());

        while (1) {
                int status;
                operacje[0].sem_num = SEM_STAN;
                operacje[0].sem_op = -1;
                semop(semid, operacje, 1);

                status = stan_sklepu->kasy_samoobslugowe_status[nr_kasy];
                operacje[0].sem_op = 1; semop(semid, operacje, 1);

                if (status == -3) {
                        // Kasa zostala wylaczona, czeka na wlaczenie
                        usleep(500000); // 0.5s drzemki
                        continue;
                }

                // 1. Czekanie na klienta (msgrcv blokuje proces az cos przyjdzie)
                // Typ 1 = kolejka do kas samoobslugowych
                if (msgrcv(msgid, &kom_odb, sizeof(Komunikat) - sizeof(long), 1, IPC_NOWAIT) == -1) {
                        if (errno == ENOMSG) {
                                usleep(100000); // krotka przerwa
                                continue;
                        }
                        if (errno == EIDRM || errno == EINVAL) {
                                exit(0);
                        }
                        perror("msgrcv kasa");
                        exit(1);
                }

                // 2. Aktualizacja statusu kasy (ZAJETA) i zmniejszenie kolejki
                operacje[0].sem_num = SEM_STAN;
                operacje[0].sem_op = -1;
                operacje[0].sem_flg = 0;
                semop(semid, operacje, 1);

                stan_sklepu->kasy_samoobslugowe_status[nr_kasy] = kom_odb.id_klienta;
                if (stan_sklepu->kolejka_samoobslugowa_len > 0)
                        stan_sklepu->kolejka_samoobslugowa_len--;

                operacje[0].sem_op = 1;
                semop(semid, operacje, 1);

                sprintf(msg_buf, "Kasa Samoobsl. %d: Obsluguje klienta nr %d (PID: %d) (Prod: %d, Alk: %d)",
                                nr_kasy + 1, kom_odb.nr_klienta_sklepu, kom_odb.id_klienta, kom_odb.liczba_produktow, kom_odb.czy_alkohol);
                loguj(semid, msg_buf, KOLOR_ZIELONY);

                // 3. Weryfikacja wieku przy zakupie alkoholu
                if (kom_odb.czy_alkohol == 1) {
                        sprintf(msg_buf, "Kasa Samoobsl. %d: ALKOHOL! Wzywam obsluge do zatwierdzenia.", nr_kasy + 1);
                        loguj(semid, msg_buf, KOLOR_CYJAN);

                        // Ustawienie statusu -2 (Oczekiwanie na alkohol)
                        operacje[0].sem_num = SEM_STAN;
                        operacje[0].sem_op = -1;
                        semop(semid, operacje, 1);

                        stan_sklepu->kasy_samoobslugowe_status[nr_kasy] = -2; // -2 = blokada alkoholowa

                        operacje[0].sem_op = 1;
                        semop(semid, operacje, 1);

                        // Czekanie na pracownika obslugi
                        while (1) {
                                sleep(1);

                                int status;
                                int czy_ewakuacja;

                                operacje[0].sem_num = SEM_STAN;
                                operacje[0].sem_op = -1;
                                semop(semid, operacje, 1);

                                status = stan_sklepu->kasy_samoobslugowe_status[nr_kasy];
                                czy_ewakuacja = stan_sklepu->ewakuacja;

                                operacje[0].sem_op = 1;
                                semop(semid, operacje, 1);

                                if (czy_ewakuacja) {
                                        exit(0);
                                }

                                // Jesli status zmienil sie na inny niz -2, to znaczy ze obsluga zatwierdzila
                                if (status != -2) {
                                        sprintf(msg_buf, "Kasa Samoobsl. %d: Wiek zatwierdzony. Kontynuuje.", nr_kasy + 1);
                                        loguj(semid, msg_buf, KOLOR_CYJAN);
                                        break;
                                }
                        }
                }

                // 4. Symulacja kasowania (czas zalezy od liczby produktow)
                // Np. 0.1 sekundy na produkt
                usleep(kom_odb.liczba_produktow * 100000);

                // 5. Symulacja awarii
                // 10% szans na awarie
                if (rand() % 100 < 10) {
                        sprintf(msg_buf, "Kasa Samoobsl. %d: AWARIA! Blokada kasy.", nr_kasy + 1);
                        loguj(semid, msg_buf, KOLOR_CYJAN);

                        // Ustawienie flagi awarii (-1)
                        operacje[0].sem_num = SEM_STAN;
                        operacje[0].sem_op = -1;
                        semop(semid, operacje, 1);

                        stan_sklepu->kasy_samoobslugowe_status[nr_kasy] = -1;

                        operacje[0].sem_op = 1;
                        semop(semid, operacje, 1);

                        // Czekanie na naprawe
                        while (1) {
                                sleep(1); // Czekamy sekunde i sprawdzamy status

                                int status;
                                int czy_ewakuacja;

                                operacje[0].sem_num = SEM_STAN;
                                operacje[0].sem_op = -1;
                                semop(semid, operacje, 1);

                                status = stan_sklepu->kasy_samoobslugowe_status[nr_kasy];
                                czy_ewakuacja = stan_sklepu->ewakuacja;

                                operacje[0].sem_op = 1;
                                semop(semid, operacje, 1);

                                if (czy_ewakuacja) {
                                        exit(0);
                                }

                                // Jesli ktos z obslugi zmienil status na inny niz -1, to naprawione
                                if (status != -1) {
                                        sprintf(msg_buf, "Kasa Samoobsl. %d: Naprawiona! Wznawiam prace.", nr_kasy + 1);
                                        loguj(semid, msg_buf, KOLOR_CYJAN);
                                        break;
                                }
                        }
                }

                float zaplacono = wystaw_paragon(semid, kom_odb.nr_klienta_sklepu, kom_odb.koszyk, kom_odb.liczba_produktow);

                // 6. Wyslanie potwierdzenia do klienta
                kom_nad.mtype = kom_odb.id_klienta; // Wysylam na kanal konkretnego procesu
                kom_nad.id_klienta = nr_kasy; // W polu ID wpisuje nr kasy ktora zostala uzyta
                kom_nad.koszt_zakupow = zaplacono; // obliczona kwota za zakupy

		if (msgsnd(msgid, &kom_nad, sizeof(Komunikat) - sizeof(long), 0) == -1) {
                        if (errno == EIDRM || errno == EINVAL) exit(0); // wyjscie w przypadku ewakuacji
                        perror("msgsnd kasa reply");
                }

                // 7. Zwolnienie kasy
                operacje[0].sem_num = SEM_STAN;
                operacje[0].sem_op = -1;
                semop(semid, operacje, 1);

                stan_sklepu->kasy_samoobslugowe_status[nr_kasy] = 0;

                operacje[0].sem_op = 1;
                semop(semid, operacje, 1);

                // Krotka przerwa przed nastepnym klientem
                usleep(100000);
        }
}