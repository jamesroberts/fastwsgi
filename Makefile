server: fast-wsgi/server.c
	mkdir -p bin
	gcc -Illhttp/include llhttp/src/*.c fast-wsgi/server.c -o bin/server -luv -I/usr/include/python3.8 -lpython3.8 -O3