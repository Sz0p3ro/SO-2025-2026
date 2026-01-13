#include "common.h"

// Zmienne globalne (potrzebne do sprzatania w atexit/signal handler)
int shmid = -1;
int semid = -1;
int msgid = -1;
pid_t main_pid;
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
	if (getpid() == main_pid) {
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
}

// Obsluga sygnalu Ctrl+C, zeby posprzatac jak przerwiemy recznie
void handle_sigint(int sig) {
	exit(0); // To wywola funkcje zarejestrowana w atexit(sprzataj)
}

// --- LOGIKA KASY SAMOOBSLUGOWEJ

void proces_kasa_samoobslugowa(int nr_kasy) {
	// nr_kasy to indeks od 0 do 5
	printf("Kasa samoobslugowa %d startuje (PID: %d)\n", nr_kasy, getpid());

	char msg_buf[200];
	Komunikat kom_odb;
	Komunikat kom_nad;
	struct sembuf operacje[1];

	// Ziarno losowosci unikalne dla procesu
	srand(time(NULL) ^ getpid());

	while (1) {
		// 1. Czekanie na klienta (msgrcv blokuje proces az cos przyjdzie)
		// Typ 1 = kolejka do kas samoobslugowych
		if (msgrcv(msgid, &kom_odb, sizeof(Komunikat) - sizeof(long), 1, 0) == -1) {
			if (errno == EINTR) continue; // Przerwanie sygnalem, ponow
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

		sprintf(msg_buf, "Kasa Samoobsl. %d: Obsluguje klienta %d (Prod: %d, Alk: %d)",
				nr_kasy, kom_odb.id_klienta, kom_odb.liczba_produktow, kom_odb.czy_alkohol);
		loguj(msg_buf);

		// 3. Symulacja kasowania (czas zalezy od liczby produktow)
		// Np. 0.1 sekundy na produkt
		usleep(kom_odb.liczba_produktow * 100000);

		// SYMULACJA AWARII
		// 10% szans na awarie
		if (rand() % 100 < 10) {
			sprintf(msg_buf, "Kasa Samoobsl. %d: AWARIA! Blokada kasy.", nr_kasy);
			loguj(msg_buf);

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
				operacje[0].sem_num = SEM_STAN;
				operacje[0].sem_op = -1;
				semop(semid, operacje, 1);

				status = stan_sklepu->kasy_samoobslugowe_status[nr_kasy];

				operacje[0].sem_op = 1;
				semop(semid, operacje, 1);

				// Jesli ktos z obslugi zmienil status na inny niz -1, to naprawione
				if (status != -1) {
					sprintf(msg_buf, "Kasa Samoobsl. %d: Naprawiona! Wznawiam prace.", nr_kasy);
					loguj(msg_buf);
					break;
				}
			}
		}
		// 4. Wyslanie potwierdzenia do klienta
		kom_nad.mtype = kom_odb.id_klienta; // Wysylam na kanal konkretnego procesu
		kom_nad.id_klienta = nr_kasy;      // W polu ID wpisuje nr kasy ktora zostala uzyta

		if (msgsnd(msgid, &kom_nad, sizeof(Komunikat) - sizeof(long), 0) == -1) {
			perror("msgsnd kasa reply");
		}

		// 5. Zwolnienie kasy
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

// --- PRACOWNIK OBSLUGI ---

void proces_obsluga() {
	printf("Pracownik obslugi zaczyna prace (PID: %d)\n", getpid());

	struct sembuf operacje[1];
	char msg_buf[100];

	while (1) {
		// Pracownik co chwile sprawdza wszystkie kasy
		for (int i = 0; i < LICZBA_KAS_SAMOOBSLUGOWYCH; i++) {

			// 1. Sprawdzenie czy jest awaria?
			int status;
			operacje[0].sem_num = SEM_STAN;
			operacje[0].sem_op = -1; // P
			operacje[0].sem_flg = 0;
			semop(semid, operacje, 1);

			status = stan_sklepu->kasy_samoobslugowe_status[i];

			operacje[0].sem_op = 1; // V
			semop(semid, operacje, 1);

			// 2. Reakcja na awarie (-1)
			if (status == -1) {
				sprintf(msg_buf, "Obsluga: Wykryto awarie w kasie %d. Naprawiam...", i);
				loguj(msg_buf);

				// Symulacja naprawy (np. 1 sekunda)
				sleep(2);

				// Zmiana statusu na 0 (WOLNA)
				operacje[0].sem_num = SEM_STAN;
				operacje[0].sem_op = -1;
				semop(semid, operacje, 1);

				stan_sklepu->kasy_samoobslugowe_status[i] = 0; // Odblokowanie kasy

				operacje[0].sem_op = 1;
				semop(semid, operacje, 1);

				sprintf(msg_buf, "Obsluga: Kasa %d naprawiona.", i);
				loguj(msg_buf);
			}
		}

		// przerwa miedzy obchodami sklepu
		usleep(500000); // 0.5 sekundy
	}
}

// --- LOGIKA KLIENTA ---

void proces_klient() {
	pid_t my_pid = getpid();
	srand(time(NULL) ^ my_pid); // Unikalne ziarno losowosci

	// 1. Wejscie do sklepu (Aktualizacja licznika w Pamieci Dzielonej)
	struct sembuf operacje[1];
	operacje[0].sem_num = SEM_STAN;
	operacje[0].sem_op = -1; // P (zablokuj dostep do stanu)
	operacje[0].sem_flg = 0;
	CHECK(semop(semid, operacje, 1), "semop P stan");

	stan_sklepu->liczba_klientow_w_sklepie++;
	int nr_klienta = stan_sklepu->liczba_klientow_w_sklepie;

	operacje[0].sem_op = 1; // V (odblokuj)
	CHECK(semop(semid, operacje, 1), "semop V stan");

	char msg_buf[100];
	sprintf(msg_buf, "Klient %d (PID: %d) wchodzi. Zakupy...", nr_klienta, my_pid);
	loguj(msg_buf);

	// 2. Symulacja zakupow (Losowy czas 1-5s)
	int czas_zakupow = (rand() % 5) + 1;
	sleep(czas_zakupow);

	// 3. Wybor kasy i przygotowanie komunikatu
	Komunikat kom;
	kom.id_klienta = my_pid;
	kom.liczba_produktow = (rand() % (MAX_PRODUKTOW - MIN_PRODUKTOW + 1)) + MIN_PRODUKTOW;
	kom.czy_alkohol = (rand() % 100) < 20 ? 1 : 0; // 20% szans na alkohol

	// Decyzja: 95% samoobslugowa, 5% stacjonarna
	int los = rand() % 100;

	operacje[0].sem_op = -1; // P
	semop(semid, operacje, 1);

	if (los < 95) {
		kom.mtype = 1; // 1 = Kasa Samoobslugowa
		stan_sklepu->kolejka_samoobslugowa_len++; // zwiekszam licznik kolejki samoobsl.
		sprintf(msg_buf, "Klient %d idzie do samoobslugowej (Prod: %d, Alk: %d)", nr_klienta, kom.liczba_produktow, kom.czy_alkohol);
	} else {
		kom.mtype = 2; // 2 = Kasa Stacjonarna
		// na razie byle gdzie potem zrobic logike
		stan_sklepu->kolejka_stacjonarna_len[0]++;
		sprintf(msg_buf, "Klient %d idzie do stacjonarnej (Prod: %d)", nr_klienta, kom.liczba_produktow);
	}

	operacje[0].sem_op = 1; // V
	semop(semid, operacje, 1);

	loguj(msg_buf);

	// 4. Ustawienie sie w kolejce (Wyslanie komunikatu)
	if (msgsnd(msgid, &kom, sizeof(Komunikat) - sizeof(long), 0) == -1) {
		perror("msgsnd klient");
		exit(1);
	}

	// 5. Oczekiwanie na obsluge (Odbior komunikatu zwrotnego na kanal PID)
	// TU KLIENT ZABLOKUJE SIE DO CZASU AZ KASJER GO OBSLUZY
	Komunikat odpowiedz;
	if (msgrcv(msgid, &odpowiedz, sizeof(Komunikat) - sizeof(long), my_pid, 0) == -1) {
		perror("msgrcv klient");
		exit(1);
	}

	sprintf(msg_buf, "Klient %d obsluzony przez kase nr %d. Wychodze.", nr_klienta, (int)odpowiedz.id_klienta);
	loguj(msg_buf);

	// 6. Wyjscie ze sklepu (Dekrementacja licznika)
	operacje[0].sem_op = -1; // P
	CHECK(semop(semid, operacje, 1), "semop P stan wyjscie");

	stan_sklepu->liczba_klientow_w_sklepie--;

	operacje[0].sem_op = 1; // V
	CHECK(semop(semid, operacje, 1), "semop V stan wyjscie");

	exit(0);
}

// --- GENERATOR KLIENTOW ---

void generator_klientow() {
	printf("Generator klientow uruchomiony (PID: %d)\n", getpid());
	while (1) {
		int current_count;
		int koniec;
		struct sembuf operacje[1];

		// Pobranie licznika (sekcja krytyczna)
		operacje[0].sem_num = SEM_STAN;
		operacje[0].sem_op = -1;
		operacje[0].sem_flg = 0;
		semop(semid, operacje, 1);

		current_count = stan_sklepu->liczba_klientow_w_sklepie;
		koniec = stan_sklepu->koniec_symulacji;

		operacje[0].sem_op = 1;
		semop(semid, operacje, 1);

		if (koniec) break;

		if (current_count < MAX_KLIENTOW_W_SKLEPIE) {
			pid_t pid = fork();
			if (pid == 0) {
				proces_klient(); // Proces potomny staje sie Klientem
			} else if (pid == -1) {
				perror("fork klient");
			}
		}

		// Losowy odstep miedzy klientami
		usleep(500000 + (rand() % 1500000));
	}
	exit(0);
}

int main() {
	main_pid = getpid();
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

	// Inicjalizacja pamieci
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

	// 6. Uruchomienie Kas Samoobslugowych (6 procesow)
	for (int i = 0; i < LICZBA_KAS_SAMOOBSLUGOWYCH; i++) {
		if (fork() == 0) {
			proces_kasa_samoobslugowa(i);
			exit(0); // Dla pewnosci
		}
	}

	// 7. Uruchomienie pracownika obslugi
	if (fork() == 0) {
		proces_obsluga();
		exit(0);
	}

	// 8. Uruchomienie Generatora Klientow
	pid_t pid_gen = fork();
	if (pid_gen == 0) {
		generator_klientow();
	}

	// Tutaj dodac Kierownika i Kasjerow

	printf("Symulacja dziala. Nacisnij Ctrl+C aby zakonczyc...\n");

	// Oczekiwanie na zakonczenie procesow potomnych (zeby nie zrobic zombie)
	while(wait(NULL) > 0);

	return 0;
}
