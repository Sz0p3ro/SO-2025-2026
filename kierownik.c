#include "common.h"
#include "utils.h"

int semid, shmid, msgid;
StanSklepu *stan_sklepu;

volatile sig_atomic_t flaga_otworz_kase_2 = 0;
volatile sig_atomic_t flaga_zamknij_kase = 0;
volatile sig_atomic_t flaga_ewakuacja = 0;

void obsluga_sygnalu_1(int sig) { flaga_otworz_kase_2 = 1; }
void obsluga_sygnalu_2(int sig) { flaga_zamknij_kase = 1; }
void obsluga_sygnalu_3(int sig) { flaga_ewakuacja = 1; }

int main() {
	key_t key = ftok(FTOK_PATH, ID_PROJEKTU);
	shmid = shmget(key, sizeof(StanSklepu), 0600);
	stan_sklepu = (StanSklepu*) shmat(shmid, NULL, 0);
	semid = semget(key, LICZBA_SEMAFOROW, 0600);
	msgid = msgget(key, 0600);

	printf("Kierownik sklepu zaczyna prace (PID: %d)\n", getpid());

        signal(SIGUSR1, obsluga_sygnalu_1); // kierownik ma nasluchiwac sygnalu 1
        signal(SIGUSR2, obsluga_sygnalu_2); // sygnalu 2 tez
        signal(SIGTERM, obsluga_sygnalu_3);
        struct sembuf operacje[1];
        char msg_buf[200];

        while (1) {
                // 1. Sygnal 3 EWAKUACJA
                if (flaga_ewakuacja == 1) {
                        flaga_ewakuacja = 0;
                        loguj(semid, "Kierownik: ALARM! Oglaszam EWAKUACJE! Usuwam kolejke komunikatow!", KOLOR_CZERWONY);

                        // Ustawienie flagi w pamieci (dla nowych klientow i generatora)
                        operacje[0].sem_num = SEM_STAN;
                        operacje[0].sem_op = -1;
                        semop(semid, operacje, 1);
                        stan_sklepu->ewakuacja = 1;
                        operacje[0].sem_op = 1;
                        semop(semid, operacje, 1);

                        // Usuniecie kolejki komunikatow
                        // To spowoduje natychmiastowy blad u wszystkich zablokowanych w msgrcv/msgsnd
                        msgctl(msgid, IPC_RMID, NULL);

                        // Czekanie az wszyscy opuszcza sklep
                        while (1) {
                                int liczba_ludzi;
                                operacje[0].sem_num = SEM_STAN;
                                operacje[0].sem_op = -1;
                                semop(semid, operacje, 1);
                                liczba_ludzi = stan_sklepu->liczba_klientow_w_sklepie;
                                operacje[0].sem_op = 1;
                                semop(semid, operacje, 1);

                                if (liczba_ludzi <= 0) {
                                        loguj(semid, "Kierownik: Sklep pusty. Ewakuacja zakonczona. Koniec symulacji.", KOLOR_CZERWONY);

                                        // Ustawienie flagi konca
                                        operacje[0].sem_num = SEM_STAN;
                                        operacje[0].sem_op = -1;
                                        semop(semid, operacje, 1);
                                        stan_sklepu->koniec_symulacji = 1;
                                        operacje[0].sem_op = 1;
                                        semop(semid, operacje, 1);

                                        exit(0);
                                }
                                sleep(1);
                        }
                }

                // 2. Sprawdzenie flagi SYGNALU 1 (otwarcie kasy 2)
                if (flaga_otworz_kase_2 == 1) {
                        flaga_otworz_kase_2 = 0; // Reset flagi, zeby wykonac to tylko raz

                        operacje[0].sem_num = SEM_STAN;
                        operacje[0].sem_op = -1;
                        semop(semid, operacje, 1);

                        if (stan_sklepu->kasy_stacjonarne_status[1] == 1) {
                                loguj(semid, "Kasa S2 juz jest otwarta", KOLOR_CZERWONY);
                        } else {
                                loguj(semid, "Otwieram Kase S2!", KOLOR_CZERWONY);
                                stan_sklepu->kasy_stacjonarne_status[1] = 1;
                        }

                        operacje[0].sem_op = 1;
                        semop(semid, operacje, 1);

                        // Zmiana statusu w pamieci dzielonej
                        operacje[0].sem_num = SEM_STAN;
                        operacje[0].sem_op = -1;
                        semop(semid, operacje, 1);

                        // Indeks 1 - kasa 2
                        stan_sklepu->kasy_stacjonarne_status[1] = 1;

                        operacje[0].sem_op = 1;
                        semop(semid, operacje, 1);
                }

                // 3. Sprawdzenie flagi SYGNALU 2 (zamkniecie jednej z kas)
                if (flaga_zamknij_kase == 1) {
                        flaga_zamknij_kase = 0;

                        // sprawdzenie statusow
                        operacje[0].sem_num = SEM_STAN;
                        operacje[0].sem_op = -1;
                        semop(semid, operacje, 1);

                        int s1_status = stan_sklepu->kasy_stacjonarne_status[0];
                        int s2_status = stan_sklepu->kasy_stacjonarne_status[1];

                        // Logika priorytetow
                        if (s2_status != 0) {
                                // jesli S2 jest otwarta to zamykamy S2
                                stan_sklepu->kasy_stacjonarne_status[1] = 0;
                                loguj(semid, "Kierownik: Otrzymalem SYGNAL 2. Zamykam Kase S2.", KOLOR_CZERWONY);
                        }
                        else if (s1_status != 0) {
                                // jesli S2 zamknieta, a S1 otwarta to zamykamy S1
                                stan_sklepu->kasy_stacjonarne_status[0] = 0;
                                loguj(semid, "Kierownik: Otrzymalem SYGNAL 2. Zamykam Kase S1.", KOLOR_CZERWONY);
                        }
                        else {
                                loguj(semid, "Kierownik: Otrzymalem SYGNAL 2, ale wszystkie kasy stacjonarne sa juz zamkniete.", KOLOR_CZERWONY);
                        }

                        operacje[0].sem_op = 1;
                        semop(semid, operacje, 1);
                }

                // 4. Pobranie danych o kolejkach i statusach (Sekcja Krytyczna)
                int len_kolejki_0;
                int status_kasy_0;

                operacje[0].sem_num = SEM_STAN;
                operacje[0].sem_op = -1;
                semop(semid, operacje, 1);

                len_kolejki_0 = stan_sklepu->kolejka_stacjonarna_len[0];
                status_kasy_0 = stan_sklepu->kasy_stacjonarne_status[0];

                operacje[0].sem_op = 1;
                semop(semid, operacje, 1);

                // 5. Otwieranie Kasy 1 (indeks 0), jesli kolejka > 3
                if (status_kasy_0 == 0 && len_kolejki_0 > 3) {
                        sprintf(msg_buf, "Kierownik: Kolejka do kasy S1 ma %d osob. OTWIERAM kase S1.", len_kolejki_0);
                        loguj(semid, msg_buf, KOLOR_CZERWONY);

                        // Otwarcie kasy
                        operacje[0].sem_num = SEM_STAN;
                        operacje[0].sem_op = -1;
                        semop(semid, operacje, 1);

                        stan_sklepu->kasy_stacjonarne_status[0] = 1; // 1 = Otwarta

                        operacje[0].sem_op = 1;
                        semop(semid, operacje, 1);
                }

                // 6. OTWIERANIE I ZAMYKANIE KAS SAMOOBSLUGOWYCH W ZALEZNOSCI OD LICZBY KLIENTOW
                operacje[0].sem_num = SEM_STAN;
                operacje[0].sem_op = -1;
                semop(semid, operacje, 1);

                int L = stan_sklepu->liczba_klientow_w_sklepie;
                int K = stan_sklepu->limit_klientow_kasa;

                // N - liczba czynnych kas samoobslugowych
                // Czynna czyli ma status inny niz -3
                int N = 0;
                int pierwsza_wylaczona = -1; // indeks pierwszej znalezionej kasy ze statusem -3
                int ostatnia_wolna = -1;     // indeks kasy, ktora jest czynna, ale wolna (status 0) - potencjalnie do zamkniecia

                for(int i=0; i<LICZBA_KAS_SAMOOBSLUGOWYCH; i++) {
                        if (stan_sklepu->kasy_samoobslugowe_status[i] != -3) {
                                N++;
                                // Szukamy kasy ktora mozna zamknac (wolnej)
                                if (stan_sklepu->kasy_samoobslugowe_status[i] == 0) {
                                        ostatnia_wolna = i;
                                }
                        } else {
                                if (pierwsza_wylaczona == -1) pierwsza_wylaczona = i;

			}
                }

                // OTWIERANIE (na kazde K klientow min 1 kasa)

                int wymagane_przez_klientow = (L + K - 1) / K;
                int cel_minimum = (wymagane_przez_klientow > 3) ? wymagane_przez_klientow : 3;

                if (N < cel_minimum) {
                        // Mamy za malo kas wiec otwieramy kolejne
                        if (pierwsza_wylaczona != -1) {
                                stan_sklepu->kasy_samoobslugowe_status[pierwsza_wylaczona] = 0; // 0 = Wolna
                                sprintf(msg_buf, "Kierownik: Otwieram Kase Samoobsl. %d (Klientow: %d, Czynnych: %d).", pierwsza_wylaczona + 1, L, N);
                                loguj(semid, msg_buf, KOLOR_CZERWONY);
                        }
                }

                // ZAMYKANIE - jesli L < K*(N-3) to zamknij jedna
                // N > 3 bo zawsze minimum 3 czynne kasy

                else if (N > 3) {
                        int prog_zamykania = K * (N - 3);

                        if (L < prog_zamykania) {
                                // warunek spelniony wiec zamykamy jedna kase
                                if (ostatnia_wolna != -1) {
                                        stan_sklepu->kasy_samoobslugowe_status[ostatnia_wolna] = -3; // wylaczona
                                        sprintf(msg_buf, "Kierownik: Zamykam Kase Samoobsl. %d (Klientow: %d, Prog: %d).", ostatnia_wolna + 1, L, prog_zamykania);
                                        loguj(semid, msg_buf, KOLOR_CZERWONY);
                                }
                        }
                }

                operacje[0].sem_op = 1; semop(semid, operacje, 1);
                sleep(1); // kierownik sprawdza stan kolejki co sekunde
        }
}