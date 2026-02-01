#include "common.h"
#include "utils.h"
#include <pthread.h>

int shmid = -1;
int semid = -1;
int msgid = -1;

StanSklepu *stan_sklepu = NULL;

pid_t child_pids[MAX_SYSTEM_PROCESSES]; // tablica PID do sprzatania
pthread_mutex_t pid_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t cleaner_thread;

void sprzataj() {
	pthread_cancel(cleaner_thread);

	printf("\nKonczenie symulacji, zabijanie procesow potomnych...\n");

	// wysyla SIGTERM do wszystkich uruchomionych procesow
	for(int i=0; i<MAX_SYSTEM_PROCESSES; i++) {
		if(child_pids[i] > 0) kill(child_pids[i], SIGTERM);
	}

	kill(0, SIGTERM);

	// czekaj na smierc wszystkich procesow
	sleep(1);

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

void zapisz_pid(pid_t pid) {
	pthread_mutex_lock(&pid_mutex);
	for(int i=0; i<MAX_SYSTEM_PROCESSES; i++) {
		if (child_pids[i] == 0) {
			child_pids[i] = pid;
			break;
		}
	}
	pthread_mutex_unlock(&pid_mutex);
}

void usun_pid(pid_t pid) {
	pthread_mutex_lock(&pid_mutex);
	for(int i=0; i<MAX_SYSTEM_PROCESSES; i++) {
		if (child_pids[i] == pid) {
			child_pids[i] = 0;
			break;
		}
	}
	pthread_mutex_unlock(&pid_mutex);
}

int policz_aktywne_procesy() {
	int c = 0;
	pthread_mutex_lock(&pid_mutex);
	for(int i=0; i<MAX_SYSTEM_PROCESSES; i++) {
		if (child_pids[i] != 0) c++;
	}
	pthread_mutex_unlock(&pid_mutex);
	return c;
}

// watek dziala w tle i wylapuje zakonczone procesy
void *watek_sprzatajacy(void *arg){
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	while (1) {
		int status;
		pid_t dead_pid = waitpid(-1, &status, 0);
		if (dead_pid > 0) {
			usun_pid(dead_pid);
		}
		else {
			if (errno == ECHILD) {
				sleep(1);
			}
		}
	}
	return NULL;
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
		zapisz_pid(pid);
	} else {
		perror("fork failed");
	}
}

int main(int argc, char *argv[]) {
	setbuf(stdout, NULL);
	atexit(sprzataj);
	signal(SIGINT, handle_sigint);

	signal(SIGTERM, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);
	signal(SIGUSR2, SIG_IGN);

	memset(child_pids, 0, sizeof(child_pids));

	// wczytanie i walidacja argumentow
	int arg_max_klientow = DEFAULT_MAX_KLIENTOW;
	int arg_limit_kasa = DEFAULT_LIMIT_NA_KASE;
	int arg_total_klientow = -1;

	if (argc > 1) arg_max_klientow = atoi(argv[1]);
	if (argc > 2) arg_limit_kasa = atoi(argv[2]);
	if (argc > 3) arg_total_klientow = atoi(argv[3]);

	int realna_pojemnosc = arg_max_klientow;
	if (realna_pojemnosc > HARD_LIMIT_POJEMNOSC) {
		printf("Ograniczono fizyczna pojemnosc sklepu (Semafor) do bezpiecznego limitu: %d\n", HARD_LIMIT_POJEMNOSC);
		realna_pojemnosc = HARD_LIMIT_POJEMNOSC;
	}

	if (arg_max_klientow <= 0 || arg_limit_kasa <= 0) {
		fprintf(stderr, "Blad: Argumenty musza byc liczba calkowita dodatnia!\nUzycie: %s [max_klientow] [limit_kasa] opcjonalnie[liczba klientow na start]\n", argv[0]);
		exit(1);
	}

	printf("Start symulacji. Konfiguracja: N=%d, K=%d\n", arg_max_klientow, arg_limit_kasa);

	if (arg_total_klientow > 0) {
		printf("Tryb WIELKIE OTWARCIE: Symulacja dla %d klientow.\n", arg_total_klientow);
	}
	else {
		printf("Tryb CIAGLY (Nieskonczony).\n");
	}

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
	semctl(semid, SEM_STAN, SETVAL, 1);
	semctl(semid, SEM_LOG, SETVAL, 1);
	semctl(semid, SEM_POJEMNOSC, SETVAL, realna_pojemnosc);

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

	// uruchomienie watku czyszczacego
	if (pthread_create(&cleaner_thread, NULL, watek_sprzatajacy, NULL) != 0) {
		perror("Nie udalo sie utworzyc watku sprzatajacego.\n");
		exit(1);
	}
	printf("Watek sprzatajacy uruchomiony.\n");

	srand(time(NULL));

	int wygenerowani = 0;

	while(1) {
		// Sprawdzenie flag konca
		struct sembuf op[1];
		op[0].sem_num = SEM_STAN;
		op[0].sem_op = -1;
		op[0].sem_flg = 0;
		semop(semid, op, 1);

		int stop = stan_sklepu->koniec_symulacji || stan_sklepu->ewakuacja;

		op[0].sem_op = 1;
		semop(semid, op, 1);

		if (stop) {
			printf("[MAIN] Koniec symulacji / Ewakuacja. Zamykam.\n");
			break;
		}

		if (arg_total_klientow > 0 && wygenerowani >= arg_total_klientow) {
			// Limit osiagniety. Czekamy az wszyscy wyjda.
			// 10 to liczba wszystkich procesow bez klientow
			if (policz_aktywne_procesy() <= 10) {
				printf("[MAIN] Wszyscy klienci obsluzeni (%d). Koniec symulacji.\n", wygenerowani);
				break;
			}
			sleep(1);
			continue;
		}

		// jesli system przeciazony to nie tworzymy nowych procesow
		if (policz_aktywne_procesy() >= MAX_SYSTEM_PROCESSES - 10) {
			usleep(10000); // Czekamy chwile az ktos wyjdzie
			continue;
		}

		// generowanie klienta
		pid_t p = fork();
		if (p == 0) {
			char *args_c[] = {"./klient", NULL};
			execv("./klient", args_c);
			_exit(1);
		} else if (p > 0) {
 			zapisz_pid(p);
			wygenerowani++;

			if (arg_total_klientow > 0 && wygenerowani < realna_pojemnosc) {
                // symulowacja tlumu przed sklepem.
                // Semafor SEM_POJEMNOSC pilnuje ilosci klientow.
				usleep(1000);
 			}
			else {
                			// sklep jest pelny, reszta klientow przychodzi w losowych odstepach czasowych
                     		usleep(500000 + (rand() % 1500000));
			}
		}
	}
	return 0;
}