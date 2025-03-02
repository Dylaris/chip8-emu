CC = gcc
CFLAG = -Wall -Wextra -std=c99

SRC = chip8.c main.c
OBJ = $(SRC:.c=.o)
TARGET = chip8-vm

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $^ -o $@

%.o: %.c
	$(CC) $(CFLAG) -c $< -o $@

clean: 
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
