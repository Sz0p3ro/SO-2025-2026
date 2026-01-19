#include "common.h"
#include "utils.h"

int semid, msgid, shmid;
StanSklepu *stan_sklepu;

int main() {
    key_t key = ftok(FTOK_PATH, ID_PROJEKTU);
	shmid = shmget(key, sizeof(StanSklepu), 0600);
	stan_sklepu = (StanSklepu*) shmat(shmid, NULL, 0); // podlaczenie do pamieci wspoldzielonej
	semid = semget(key, LICZBA_SEMAFOROW, 0600); // podlaczenie do semaforow
	msgid = msgget(key, 0600); // podlaczenie do kolejki komunikatow

	pid_t my_pid = getpid();

        //  mikrosekundy aby nic sie nie zdublowalo
	struct timeval t;
	gettimeofday(&t, NULL);
	srand(t.tv_usec ^ t.tv_sec ^ my_pid);

	// 1. Wejscie do sklepu (Aktualizacja licznika w Pamieci Dzielonej)
	struct sembuf operacje[1];
	operacje[0].sem_num = SEM_STAN;
	operacje[0].sem_op = -1; // P
	operacje[0].sem_flg = 0;
	CHECK(semop(semid, operacje, 1), "semop P stan");

	// 2. Sprawdzenie ewakuacji na wejsciu
	if (stan_sklepu->ewakuacja) {
		operacje[0].sem_op = 1;
	    semop(semid, operacje, 1);
		exit(0); // Sklep zamkniety (alarm) - nie wchodzic
	}

	stan_sklepu->liczba_klientow_w_sklepie++;
	stan_sklepu->id_generator++;
	int nr_klienta = stan_sklepu->id_generator;

	operacje[0].sem_op = 1; // V
	CHECK(semop(semid, operacje, 1), "semop V stan");

	char msg_buf[100];
	sprintf(msg_buf, "Klient %d (PID: %d) wchodzi. Zakupy...", nr_klienta, my_pid);
	loguj(semid, msg_buf);

	// 3. Symulacja zakupow (Losowy czas 1-5s)
	int czas_zakupow = (rand() % 5) + 1;
	sleep(czas_zakupow);

	if (stan_sklepu->ewakuacja) {
		sprintf(msg_buf, "Klient %d: ALARM! Porzucam zakupy i uciekam!", nr_klienta);
		loguj(semid, msg_buf);

		// WYJSCIE
                operacje[0].sem_num = SEM_STAN;
                operacje[0].sem_op = -1;
                semop(semid, operacje, 1);
                stan_sklepu->liczba_klientow_w_sklepie--;
                operacje[0].sem_op = 1;
                semop(semid, operacje, 1);
                exit(0);
        }

        // 4. Wybor kasy i przygotowanie komunikatu
        Komunikat kom;
        kom.id_klienta = my_pid;
        kom.nr_klienta_sklepu = nr_klienta;
        kom.liczba_produktow = (rand() % (MAX_PRODUKTOW - MIN_PRODUKTOW + 1)) + MIN_PRODUKTOW;
        kom.czy_alkohol = 0;

        // wypelnianie koszyka losowymi produktami z bazy
        for (int i = 0; i < kom.liczba_produktow; i++) {
                int idx = rand() % LICZBA_TYPOW_PRODUKTOW; // losowanie produktu z bazy
                kom.koszyk[i] = idx;

                // Sprawdzam czy produkt to alkohol
                if (BAZA_PRODUKTOW[idx].is_alkohol == 1) {
                        kom.czy_alkohol = 1;
                }
        }

        // Decyzja: 95% samoobslugowa, 5% stacjonarna
        int los = rand() % 100;
        int chce_samoobsluge = (los < 95);

        operacje[0].sem_op = -1; // P
        semop(semid, operacje, 1);

        int len_samo = stan_sklepu->kolejka_samoobslugowa_len;
        int s1_stat = stan_sklepu->kasy_stacjonarne_status[0];
        int s2_stat = stan_sklepu->kasy_stacjonarne_status[1];
        int s1_len = stan_sklepu->kolejka_stacjonarna_len[0];
        int s2_len = stan_sklepu->kolejka_stacjonarna_len[1];

        if (chce_samoobsluge && !(len_samo > LIMIT_CIERPLIWOSCI && (s1_stat || s2_stat))) {
                // samoobslugowa
                kom.mtype = 1;
                stan_sklepu->kolejka_samoobslugowa_len++;

                sprintf(msg_buf, "Klient %d idzie do samoobslugowej (Prod: %d, Alk: %d)", nr_klienta, kom.liczba_produktow, kom.czy_alkohol);
        }
        else {
                // stacjonarna
                int wybrana_kasa = 0; // Domyslnie S1

                if (s1_stat == 1 && s2_stat == 1) {
                        // jest obie otwarte idzie tam gdzie jest krotsza kolejka
                        wybrana_kasa = (s1_len <= s2_len) ? 0 : 1;
                } else if (s2_stat == 1) {
                        // jesli tylko S2 otwarta to idz do niej
                        wybrana_kasa = 1;
                }

                kom.mtype = 2 + wybrana_kasa; // 2 dla S1, 3 dla S2
                stan_sklepu->kolejka_stacjonarna_len[wybrana_kasa]++;

                sprintf(msg_buf, "Klient %d idzie do kasy S%d (Prod: %d)", nr_klienta, wybrana_kasa + 1, kom.liczba_produktow);
        }

        operacje[0].sem_op = 1; // V
        semop(semid, operacje, 1);

        loguj(semid, msg_buf);

        // 5. Ustawienie sie w kolejce (Wyslanie komunikatu)
        if (msgsnd(msgid, &kom, sizeof(Komunikat) - sizeof(long), 0) == -1) {
                if (errno == EIDRM || errno == EINVAL) {
                        sprintf(msg_buf, "Klient %d: Kolejka zamknieta (Ewakuacja)! Uciekam!", nr_klienta); // blad wywolany ewakuacja (usunieciem kolejki komunikatow)
                        loguj(semid, msg_buf);
                } else {
                        perror("msgsnd klient error"); // inny blad
                }

                // WYJSCIE
                operacje[0].sem_num = SEM_STAN;
                operacje[0].sem_op = -1;
                semop(semid, operacje, 1);
                stan_sklepu->liczba_klientow_w_sklepie--;
                operacje[0].sem_op = 1;
                semop(semid, operacje, 1);
                exit(0);
        }

        // 6. Oczekiwanie na obsluge (Odbior komunikatu zwrotnego na kanal PID)
        // TU KLIENT ZABLOKUJE SIE DO CZASU AZ KASJER GO OBSLUZY
        Komunikat odpowiedz;
        if (msgrcv(msgid, &odpowiedz, sizeof(Komunikat) - sizeof(long), my_pid, 0) == -1) {
                if (errno == EIDRM || errno == EINVAL) {
                        sprintf(msg_buf, "Klient %d: Wyrzucony z kolejki (Ewakuacja)! Uciekam!", nr_klienta);
                        loguj(semid, msg_buf);
                } else {
                        perror("msgrcv klient error"); // Inny blad
                }

                // WYJSCIE
                operacje[0].sem_num = SEM_STAN;
                operacje[0].sem_op = -1;
                semop(semid, operacje, 1);
                stan_sklepu->liczba_klientow_w_sklepie--;
                operacje[0].sem_op = 1;
                semop(semid, operacje, 1);
                exit(0);
        }

        int id_kasy = (int)odpowiedz.id_klienta;

        if (id_kasy == 100) {
                sprintf(msg_buf, "Klient %d obsluzony przez Kase S1. Wychodze.", nr_klienta);
        } else if (id_kasy == 101) {
                sprintf(msg_buf, "Klient %d obsluzony przez Kase S2. Wychodze.", nr_klienta);
        } else {
                // Kasy samoobslugowe (maja ID 0-5)
                sprintf(msg_buf, "Klient %d obsluzony przez Kase Samoobsl. %d. Wychodze.", nr_klienta, id_kasy + 1);
        }

        loguj(semid, msg_buf);

        // 7. Wyjscie ze sklepu
	operacje[0].sem_op = -1; // P
	CHECK(semop(semid, operacje, 1), "semop P stan wyjscie");

	stan_sklepu->liczba_klientow_w_sklepie--;

	operacje[0].sem_op = 1; // V
	CHECK(semop(semid, operacje, 1), "semop V stan wyjscie");

	exit(0);
}