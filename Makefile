CC = gcc
CFLAGS = -std=c89 -Wpedantic
LDFLAGS = -lm

all: master porti navi

master: master.c
	$(CC) $(CFLAGS) -o master master.c

porti: porti.c
	$(CC) $(CFLAGS) -o porti porti.c

navi: navi.c
	$(CC) $(CFLAGS) -o navi navi.c $(LDFLAGS)

run: all
	./master

clean:
	rm -f master navi porti