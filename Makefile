server: server.c
	mkdir -p bin
	gcc -o bin/server server.c -luv