all:
	gcc -I/usr/local/include server.c -o server -L/usr/local/lib -luv -lkyotocabinet

run:
	./server

clean:
	rm ./server
