INSTALL=/usr/bin/install
CFLAGS=$(shell curl-config --cflags) -Wall $(EXTRA_CFLAGS)
LFLAGS=$(shell curl-config --libs) -lpthread -lkyotocabinet
CC=gcc
PREFIX=/usr/local
BINDIR = $(PREFIX)/bin

default: ferrite

src/%.o: src/%.c src/%.h
	$(CC) -c $(CFLAGS) -o $@ $<

ferrite: ./src/util.o ./src/queue.o ./src/proxy.o ./src/handlers.o ./src/main.o
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

debug:
	make -f Makefile EXTRA_CFLAGS="-g -O0"

run:
	./ferrite

.PHONY: clean debug release

release: clean
	make -f Makefile EXTRA_CFLAGS="-O3"

install:
	$(INSTALL) ./ferrite $(BINDIR)

clean:
	rm -f ./src/*.o
	rm -f ./ferrite
