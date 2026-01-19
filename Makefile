CC = gcc
CFLAGS = -Wall -std=gnu99
HEADERS = common.h utils.h

TARGETS = dyskont kasa_samoobslugowa kasjer kierownik obsluga klient

all: $(TARGETS)

dyskont: main.c utils.c $(HEADERS)
	$(CC) $(CFLAGS) -o dyskont main.c utils.c

kasa_samoobslugowa: kasa_samoobslugowa.c utils.c $(HEADERS)
	$(CC) $(CFLAGS) -o kasa_samoobslugowa kasa_samoobslugowa.c utils.c

kasjer: kasjer.c utils.c $(HEADERS)
	$(CC) $(CFLAGS) -o kasjer kasjer.c utils.c

kierownik: kierownik.c utils.c $(HEADERS)
	$(CC) $(CFLAGS) -o kierownik kierownik.c utils.c

obsluga: obsluga.c utils.c $(HEADERS)
	$(CC) $(CFLAGS) -o obsluga obsluga.c utils.c

klient: klient.c utils.c $(HEADERS)
	$(CC) $(CFLAGS) -o klient klient.c utils.c

clean:
	rm -f $(TARGETS) raport.txt paragony.txt
