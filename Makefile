CFLAGS=$(shell curl-config --cflags) -Wall $(EXTRA_CFLAGS)
LFLAGS=$(shell curl-config --libs) -lpthread -lkyotocabinet
CC=gcc

default: fast-cache

fast-cache:
	$(CC) $(CFLAGS) ./src/main.c -o $@ $(LFLAGS)

debug:
	make -f Makefile EXTRA_CFLAGS="-g"

run:
	./fast-cache

clean:
	rm ./fast-cache
