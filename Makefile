all:
	mkdir -p out
	gcc -I/usr/local/include ./src/main.c -o ./out/fast-cache -L/usr/local/lib -luv -lkyotocabinet -lcurl

debug:
	mkdir -p out
	gcc -I/usr/local/include ./src/main.c -o ./out/fast-cache -L/usr/local/lib -lkyotocabinet -lcurl -g

run:
	./out/fast-cache

clean:
	rm -rf out
