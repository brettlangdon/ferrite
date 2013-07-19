all:
	mkdir -p out
	gcc -I/usr/local/include -I~/Downloads/libuv-0.10.12/include ./src/main.c -o ./out/fast-cache -L/usr/local/lib -luv -lkyotocabinet -lcurl

run:
	./out/fast-cache

clean:
	rm -rf out
