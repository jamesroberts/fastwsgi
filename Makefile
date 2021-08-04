server: server.c
	mkdir -p bin
	gcc -Illhttp/include llhttp/src/*.c server.c -o bin/server -luv