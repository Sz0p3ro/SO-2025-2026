CC = gcc
CFLAGS = -Wall -std=gnu99
TARGET = dyskont
SRC = main.c
HEADERS = common.h

all: $(TARGET)

$(TARGET): $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET) raport.txt paragony.txt

run: $(TARGET)
	./$(TARGET)
