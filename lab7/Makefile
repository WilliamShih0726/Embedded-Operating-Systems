CC = gcc
CFLAGS = -Wall -O2
TARGETS = game guess

all: $(TARGETS)

game: game.c
	$(CC) $(CFLAGS) -o game game.c

guess: guess.c
	$(CC) $(CFLAGS) -o guess guess.c

clean:
	rm -f $(TARGETS)
