CFLAGS=$(shell curl-config --cflags) -Wall $(EXTRA_CFLAGS)
LFLAGS=$(shell curl-config --libs) -lpthread -lkyotocabinet
CC=gcc

default: fast-cache

src/%.o: src/%.c src/%.h
	$(CC) -c $(CFLAGS) -o $@ $<

fast-cache: ./src/util.o ./src/queue.o ./src/proxy.o ./src/handlers.o ./src/main.o
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

debug:
	make -f Makefile EXTRA_CFLAGS="-g"

run:
	./fast-cache

clean:
	rm ./src/*.o
	rm ./fast-cache
