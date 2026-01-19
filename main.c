#include "common.h"
#include "utils.h"

int shmid = -1;
int semid = -1;
int msgid = -1;

StanSklepu *stan_sklepu = NULL;

pid_t child_pids[20]; // tablica PID do sprzatania
int child_count = 0;

void sprzataj() {
	printf("\nKonczenie symulacji, zabijanie procesow potomnych...\n");
	// wysyla SIGTERM do wszystkich uruchomionych procesow
	for(int i=0; i<child_count; i++) {
		if(child_pids[i] > 0) kill(child_pids[i], SIGTERM);
	}
	// Czekaj na smierc wszystkich procesow
	while(wait(NULL) > 0);

	printf("Usuwanie zasobow\n");
	if (shmid != -1) shmctl(shmid, IPC_RMID, NULL);
	if (semid != -1) semctl(semid, 0, IPC_RMID);
	if (msgid != -1) msgctl(msgid, IPC_RMID, NULL);
	printf("Gotowe\n");
}

// Obsluga sygnalu Ctrl+C, zeby posprzatac jak przerwiemy recznie
void handle_sigint(int sig) {
	exit(0); // To wywola funkcje zarejestrowana w atexit(sprzataj)
}

// funkcja pomocnicza do uruchamiania procesow przy uzyciu exec
void uruchom_proces(const char *program, char *const args[]) {
	pid_t pid = fork();
	if (pid == 0) {
	// proces potomny
	execv(program, args);
	perror("execv failed");
		_exit(1);
	} else if (pid > 0) {
	child_pids[child_count++] = pid;
	} else {
		perror("fork failed");
	}
}

int main(int argc, char *argv[]) {
	atexit(sprzataj);
	signal(SIGINT, handle_sigint);

	signal(SIGTERM, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);
	signal(SIGUSR2, SIG_IGN);

	// wczytanie i walidacja argumentow
	int arg_max_klientow = DEFAULT_MAX_KLIENTOW;
	int arg_limit_kasa = DEFAULT_LIMIT_NA_KASE;

	if (argc > 1) arg_max_klientow = atoi(argv[1]);
	if (argc > 2) arg_limit_kasa = atoi(argv[2]);

	if (arg_max_klientow <= 0 || arg_limit_kasa <= 0) {
		fprintf(stderr, "Blad: Argumenty musza byc dodatnie!\nUzycie: %s [max_klientow] [limit_kasa]\n", argv[0]);
		exit(1);
	}
	printf("Start symulacji. Konfiguracja: N=%d, K=%d\n", arg_max_klientow, arg_limit_kasa);

	// inizjalizacja struktur
	key_t key = ftok(FTOK_PATH, ID_PROJEKTU);
	if (key == -1) handle_error("ftok");

	shmid = shmget(key, sizeof(StanSklepu), IPC_CREAT | 0600);
	if (shmid == -1) handle_error("shmget");
	stan_sklepu = (StanSklepu*) shmat(shmid, NULL, 0);
	if (stan_sklepu == (void*)-1) handle_error("shmat");

	// reset pamieci
	memset(stan_sklepu, 0, sizeof(StanSklepu));
	stan_sklepu->max_klientow_sklep = arg_max_klientow;
	stan_sklepu->limit_klientow_kasa = arg_limit_kasa;

	semid = semget(key, LICZBA_SEMAFOROW, IPC_CREAT | 0600);
	if (semid == -1) handle_error("semget");
	for(int i=0; i<LICZBA_SEMAFOROW; i++) semctl(semid, i, SETVAL, 1);

	msgid = msgget(key, IPC_CREAT | 0600);
	if (msgid == -1) handle_error("msgget");

	// czyszczenie plikow
	FILE *f = fopen("raport.txt", "w"); if(f) { fprintf(f, "START\n"); fclose(f); }
	FILE *fp = fopen("paragony.txt", "w"); if(fp) fclose(fp);

	// URUCHAMIANIE PROCESOW

	// kasy samoobslugowe ./kasa: nr_kasy, typ: 0 - samoobslugowa, 1 - stacjonarna
	for(int i=0; i<LICZBA_KAS_SAMOOBSLUGOWYCH; i++) {
		char arg_nr[10], arg_typ[10];
		sprintf(arg_nr, "%d", i);
		sprintf(arg_typ, "0");
		char *args[] = {"./kasa_samoobslugowa", arg_nr, arg_typ, NULL};
		uruchom_proces("./kasa_samoobslugowa", args);
	}

	// kasy stacjonarne
	for(int i=0; i<LICZBA_KAS_STACJONARNYCH; i++) {
		char arg_nr[10], arg_typ[10];
		sprintf(arg_nr, "%d", i);
		sprintf(arg_typ, "1");
		char *args[] = {"./kasjer", arg_nr, arg_typ, NULL};
		uruchom_proces("./kasjer", args);
	}

	// obsluga
	char *args_obs[] = {"./obsluga", NULL};
	uruchom_proces("./obsluga", args_obs);

	// kierownik
	char *args_kier[] = {"./kierownik", NULL};
	uruchom_proces("./kierownik", args_kier);


	printf("Wszystkie procesy uruchomione.\n");

	srand(time(NULL));

	while(1) {
		// Sprawdzenie flag konca
		struct sembuf op[1];
		op[0].sem_num = SEM_STAN; op[0].sem_op = -1; op[0].sem_flg = 0;
		semop(semid, op, 1);

		int cur = stan_sklepu->liczba_klientow_w_sklepie;
		int max = stan_sklepu->max_klientow_sklep;
		int koniec = stan_sklepu->koniec_symulacji;
		int ew = stan_sklepu->ewakuacja;

		op[0].sem_op = 1; semop(semid, op, 1);

		if (koniec || ew) {
			printf("[MAIN] Koniec symulacji / Ewakuacja. Zamykam.\n");
			break;
		}

		// Generowanie nowego klienta jesli jest miejsce
		if (cur < max) {
			pid_t p = fork();
			if (p == 0) {
				// Proces potomny zamienia sie w klienta
				char *args_c[] = {"./klient", NULL};
				execv("./klient", args_c);
				perror("Blad execv klient"); // To sie wykona tylko przy bledzie
                		_exit(1);
			} else if (p > 0) {
				// Rodzic nie czeka blokujaco, ale sprzata zombie jesli jakies sa
				waitpid(-1, NULL, WNOHANG);
			}
		}

		// Losowy odstep miedzy klientami
		usleep(500000 + (rand() % 1500000));
	}

	return 0;
}