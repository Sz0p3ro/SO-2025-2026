Dokument zawiera opis i wyniki czterech testów sprawdzających kluczowe funkcjonalności projektu

## Test 1: Stress test IPC + skalowalność
**Cel:** Sprawdzenie poprawności synchronizacji (semafory) i przesyłania danych (kolejki komunikatów) w warunkach, gdy liczba chętnych klientów znacznie przewyższa pojemność sklepu.

* Akcja: Uruchomienie symulacji z parametrami 20 5 100 [limit klientow w sklepie w jednym momencie] [zmienna do obliczania ilości wymaganych kas] [liczba klientow którą chcemy wygenerować na start]
* Oczekiwany wynik: Brak zamrożenia, symulacja przebiega poprawnie i sklep zamyka się po obsłużeniu 100 klientów.
* **Wynik:**
```
Wynik zgodny z oczekiwaniami.
```
**SKALOWALNOŚĆ**
* Akcja: Uruchomienie symulacji z dużym limitem klientów w sklepie i z małym limitem klientów na kasę: `./dyskont 70 1`
* Oczekiwany wynik: Szybko zostają uruchomione wszystkie kasy samoobsługowe
* **Wynik:**
```
Po 15 sekundach działania symulacji możemy zaobserwować, że zostały już otwarte wszystkie dostępne kasy samoobsługowe.
Kasa stacjonarna uruchamia się prawie po minucie, bo przez to, że mamy dużo kas samoobsługowych klienci rzadko zmieniają kolejkę do kasy samoobsługowej
na tą do kasy stacjonarnej (bo czas oczekiwania do kasy samoobsługowej jest krótki).
Jeśli chcielibyśmy zaobserwować dosyć szybkie uruchomienie pierwszej kasy stacjonarnej powinniśmy uruchomić symulację z parametrami domyślnymi.
```
## Test 2: Obsługa zdarzeń losowych (awaria/alkohol) + weryfikacja zgodności paragonów z logami
**Cel:** Weryfikacja interakcji pomiędzy kasami samoobsługowymi, a pracownikiem obsługi.
* Akcja: Uruchomienie symulacji z domyślnymi argumentami: `./dyskont` oraz obserwacja logów.
* Oczekiwany wynik: kasa zgłasza alkohol lub awarię, po chwili obsługa idzie zatwierdzić alkohol lub odblokować kasę.
Po interwencji pracownika obsługi kasa daje komunikat o wznowieniu pracy.

```
ALKOHOL:
[Wed Jan 21 12:15:07 2026] Kasa Samoobsl. 2: ALKOHOL! Wzywam obsluge do zatwierdzenia.
[Wed Jan 21 12:15:07 2026] Obsluga: Kasa 2 wola do alkoholu. Ide zatwierdzic...
[Wed Jan 21 12:15:07 2026] Obsluga: Alkohol w kasie 3 zatwierdzony.
...
[Wed Jan 21 12:15:09 2026] Kasa Samoobsl. 2: Wiek zatwierdzony. Kontynuuje.

AWARIA:
[Wed Jan 21 12:15:10 2026] Kasa Samoobsl. 2: AWARIA! Blokada kasy.
[Wed Jan 21 12:15:10 2026] Obsluga: Wykryto awarie w kasie 2. Naprawiam...
...
[Wed Jan 21 12:15:12 2026] Obsluga: Kasa 2 naprawiona.
[Wed Jan 21 12:15:13 2026] Kasa Samoobsl. 2: Naprawiona! Wznawiam prace.
```

* **należy zwrócić uwagę na logi w kolorze jasno-niebieskim**
* **Wynik:**
```
Możemy zaobserwować poprawne działanie symulacji. 
Kasy ulegają awarii, obsługa je naprawia, kasy wznawiają pracę. 
(łącznie dostajemy 4 komunikaty dla jednej awarii/blokady alkoholowej)
```

**weryfikacja zgodności logów z plikiem paragony.txt**
* Akcja: Uruchomienie symulacji z domyślnymi argumentami: `./dyskont`. Po ok. 30s. zatrzymujemy symulację.
Przeglądając raport.txt wybieramy jakiegoś klienta, który kupował alkohol. Zapisujemy liczbę artykułów, które zakupił oraz numer tego klienta.
Następnie sprawdzamy tego samego klienta w pliku paragony.txt i weryfikujemy czy liczba artykułów jest zgodna, oraz czy faktycznie kupował alkohol.
* Oczekiwany wynik: dane z obu plików będą ze sobą zgodne.
* **Wynik:**
```
z pliku raport.txt:

[Wed Jan 21 12:15:08 2026] Kasa Samoobsl. 4: Obsluguje klienta nr 2 (PID: 13696) (Prod: 5, Alk: 1)

z pliku paragony.txt:

------------------------------------------
 PARAGON - KLIENT NR: 2
------------------------------------------
NAZWA           | ILOSC  | CENA
------------------------------------------
Chleb           | x1     | 4.15 PLN
Maslo           | x2     | 13.00 PLN
Czekolada       | x1     | 5.50 PLN
Piwo            | x1     | 4.00 PLN
------------------------------------------
 RAZEM:                       26.65 PLN
------------------------------------------

Widzimy, że liczba produktów się zgadza oraz że klient ten faktycznie dokonał zakupu alkoholu.
```

## Test 3: Działanie sygnałów
**Cel:** Sprawdzenie czy kierownik poprawnie reaguje na sygnały systemowe.

#### Sprawdzenie działania sygnału 1
* Akcja: Uruchomienie symulacji z argumentami: `./dyskont 60 5`. W drugim terminalu wpisujemy komendę: killall -SIGUSR1 kierownik (obserwujemy logi).
* Oczekiwany wynik: kierownik otwiera kase S2, widzimy czerwony komunikat o tym zdarzeniu, możemy zaobserwować, że kasa S2 obsługuje klientów.
* **Wynik:**
```
Podczas obserwacji logów widzimy, że kasa S2 zostaje uruchomiona oraz obsługuje klientów. 
Po 30s. bez klienta w kolejce kasa zamyka się. (niebieski komunikat).

[Wed Jan 21 12:56:58 2026] Otwieram Kase S2!
[Wed Jan 21 12:56:59 2026] Kasa Stacjonarna S2: Obsluguje klienta nr 7 (PID: 14112) (Prod: 7)
[Wed Jan 21 12:57:10 2026] Kasa Stacjonarna S2: Obsluguje klienta nr 17 (PID: 14124) (Prod: 7)
[Wed Jan 21 12:57:41 2026] Kasa Stacjonarna S2: Brak klientow od 30s. Zamykam kase.
```

#### Sprawdzenie działania sygnału 2
* Akcja: Uruchomienie symulacji z argumentami: `./dyskont 60 5`. W drugim terminalu wpisujemy komendę: killall -SIGUSR1 kierownik, a następnie w krótkich
odstępach czasowych (co ok. 5s.) killall -SIGUSR2 kierownik (obserwujemy logi).
* Oczekiwany wynik: najpierw widzimy komunikat o otwarciu kasy S2, a następnie kolejno komunikaty o: zamknięciu kasy S2, zamknięciu kasy S1 oraz o tym, że wszystkie kasy stacjonarne są już zamknięte.
* **Wynik:**
```
Wynik jest zgodny z oczekiwaniami, na czerwono można zaobserwować odpowiednie komunikaty.

[Wed Jan 21 12:59:05 2026] Otwieram Kase S2!
[Wed Jan 21 12:59:11 2026] Kierownik: Otrzymalem SYGNAL 2. Zamykam Kase S2.
[Wed Jan 21 12:59:14 2026] Kierownik: Otrzymalem SYGNAL 2. Zamykam Kase S1.
[Wed Jan 21 12:59:17 2026] Kierownik: Otrzymalem SYGNAL 2, ale wszystkie kasy stacjonarne sa juz zamkniete.
```
#### Sprawdzenie działania sygnału 3
* Akcja: Uruchomienie symulacji z domyślnymi argumentami: `./dyskont`. Dajemy symulacji ok. 30s. ,aby klienci byli na różnych etapach zakupów, a następnie w drugim terminalu wpisujemy komendę: killall -SIGQUIT kierownik.
* Oczekiwany wynik: kierownik ogłasza ewakuację (widzimy czerwony komunikat), wszyscy **natychmiastowo** opuszczają sklep, symulacja się kończy
* **Wynik (log):**
```
[Wed Jan 21 12:00:53 2026] Kierownik: ALARM! Oglaszam EWAKUACJE! Usuwam kolejke komunikatow!
[Wed Jan 21 12:00:53 2026] Klient 35: Wyrzucony z kolejki (Ewakuacja)! Uciekam!
[Wed Jan 21 12:00:53 2026] Klient 36: Wyrzucony z kolejki (Ewakuacja)! Uciekam!
[Wed Jan 21 12:00:53 2026] Klient 39: Wyrzucony z kolejki (Ewakuacja)! Uciekam!
[Wed Jan 21 12:00:53 2026] Klient 40: Wyrzucony z kolejki (Ewakuacja)! Uciekam!
[Wed Jan 21 12:00:53 2026] Klient 41: Wyrzucony z kolejki (Ewakuacja)! Uciekam!
[Wed Jan 21 12:00:53 2026] Klient 38: ALARM! Porzucam zakupy i uciekam!
[MAIN] Koniec symulacji / Ewakuacja. Zamykam.

Konczenie symulacji, zabijanie procesow potomnych...
[Wed Jan 21 12:00:54 2026] Klient 37: ALARM! Porzucam zakupy i uciekam!
[Wed Jan 21 12:00:55 2026] Kierownik: Sklep pusty. Ewakuacja zakonczona. Koniec symulacji.
```

## Test 4: Test fork bomb

* Akcja: Uruchomienie symulacji z domyślnymi argumentami: `./dyskont 100 5 5000`
* Oczekiwany wynik: Mimo ogromnego obciążenia, proces główny powinien poprawnie przechwycić sygnał przerwania. Funkcja sprzątająca musi zabić wszystkie 200+ procesów potomnych (Klienci, Kasjerzy, Obsługa), a następnie usunąć semafory i pamięć współdzieloną. Polecenie ps po zakończeniu testu nie powinno pokazać żadnych aktywnych procesów dyskont ani klient.
* **Wynik:**
```
ps
    PID TTY          TIME CMD
   3539 pts/3    00:00:00 bash
 116279 pts/3    00:00:00 ps

ipcs

------ Message Queues --------
key        msqid      owner      perms      used-bytes   messages    

------ Shared Memory Segments --------
key        shmid      owner      perms      bytes      nattch     status      

------ Semaphore Arrays --------
key        semid      owner      perms      nsems

System poprawnie obsłużył przeciążenie procesami i posprzątał zasoby.
```


