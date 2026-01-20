link do repozytorium: 
https://github.com/Sz0p3ro/SO-2025-2026


opis zadania:

W pewnym dyskoncie jest łącznie 8 kas: 2 kasy stacjonarne i 6 kas samoobsługowych. Klienci
przychodzą do sklepu w dowolnych momentach czasu i przebywają w nim przez pewien określony
(losowy) dla każdego z nich czas robiąc zakupy – od 3 do 10 produktów różnych kategorii (owoce,
warzywa, pieczywo, nabiał, alkohol, wędliny, …). Większość klientów korzysta z kas
samoobsługowych (ok. 95%), pozostali (ok. 5%) stają w kolejce do kas stacjonarnych.
Zasady działania kas samoobsługowych:
• Do wszystkich kas samoobsługowych jest jedna kolejka;
19
• Klient podchodzi do pierwszej wolnej kasy;
• Zawsze działają co najmniej 3 kasy samoobsługowe;
• Na każdych K klientów znajdujących się na terenie dyskontu powinno przypadać min. 1
czynne stanowisko kasowe.
• Jeśli liczba klientów jest mniejsza niż K*(N-3), gdzie N oznacza liczbę czynnych kas, to jedna
z kas zostaje zamknięta;
• Jeżeli czas oczekiwania w kolejce na kasę samoobsługową jest dłuższy niż T, klient może
przejść do kasy stacjonarnej jeżeli jest otwarta;
• Przy zakupie produktów z alkoholem konieczna weryfikacja kupującego przez obsługę
(wiek>18);
• Klient kasuje produkty, płaci kartą i otrzymuje wydruk (raport) z listą zakupów i zapłaconą
kwotą ;
• Co pewien losowy czas kasa się blokuje (np. waga towaru nie zgadza się z wyborem klienta)
– wówczas konieczna jest interwencja obsługi aby odblokować kasę.
Zasady działania kas stacjonarnych:
• Generalnie kasy są zamknięte;
• Jeżeli liczba osób stojących w kolejce do kasy jest większa niż 3 otwierana jest kasa 1;
• Jeżeli po obsłużeniu ostatniego klienta w kolejce przez 30 sekund nie pojawi się następny
klient, kasa jest zamykana;
• Kasa 2 jest otwierana jedynie na bezpośrednie polecenie kierownika – zasady zamykania jak
dla kasy 1;
• Jeżeli kasa 2 jest otwarta, to każdej z kas tworzy się osobna kolejka klientów (klienci z kolejki
do kasy 1 mogą przejść do kolejki do kasy 2);
• Jeśli w kolejce do kasy czekali klienci (przed ogłoszeniem decyzji o jej zamknięciu) to powinni
zostać obsłużeni przez tę kasę.
Na polecenie kierownika sklepu (sygnał 1) jest otwierana kasa 2. Na polecenie kierownika sklepu
(sygnał 2) jest zamykana dana kasa (1 lub 2). Na polecenie kierownika sklepu (sygnał 3) klienci
natychmiast opuszczają supermarket bez robienia zakupów, a następnie po wyjściu klientów
zamykane są wszystkie kasy.
Napisz program kierownika, kasjera, kasy samoobsługowej, pracownika obsługi i klienta. Raport z
przebiegu symulacji zapisać w pliku (plikach) tekstowym.


opis testów:

test1: 
Sprawdzenie poprawności otwierania i zamykania kas samoboobsługowych w zależności od liczby klientów.

Na początku w sklepie znajduje się kilka osób, później zwiększamy liczbę klientów do 30. Program powinien automatycznie otwierać kolejne kasy. Następnie liczba klientów spada, a program zamyka nadmiar kas.

test2:
Test konieczności interwencji pracownika obsługi w przypadku próby zakupu produktu alkoholowego.

Do kas podchodzi dwóch klientów; pierwszy ma zwykłe produkty, a drugi w swoim koszyku ma również alkohol.
Podczas skanowania jego koszyk zostaje zablokowany do czasu przyjścia pracownika obsługi. Po zatwierdzeniu przez pracownika obsługi klient skanuje kolejny produkt alkoholowy - kasa nie powinna się już blokować.

test3:
Sprawdzenie zgodności działania programu z poleceniami kierownika.

Symulacja działa normalnie po czym następuje wydanie przez kierownika po kolei:
sygnał 1, sygnał 2, sygnał 3. Oczekiwany efekt to po kolei: otwarcie kasy nr2, następnie zamknięcie kasy, a po sygnale 3 następuje natychmiastowe zamknięcie całego sklepu, opuszczenie go przez wszystkich klientów oraz zakończenie symulacji.  

test4:
Sprawdzenie mechanizmu zmiany kolejki klienta przy zbyt długim czasie oczekiwania (T).

Generujemy długą kolejkę do kas samoobsługowych i małą do kasy stacjonarnej. Czas oczekiwania przekracza wartość T, sprawdzamy czy klienci przenoszą się do kas stacjonarnych.