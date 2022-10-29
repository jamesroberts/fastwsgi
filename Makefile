server: fastwsgi/server.c
	mkdir -p bin
	gcc -Illhttp/include -Ilibuv/include -Ilibuv/src llhttp/src/*.c fastwsgi/request.c fastwsgi/server.c fastwsgi/constants.c fastwsgi/logx.c fastwsgi/common.c fastwsgi/pyhacks.c -o bin/server -luv -I/usr/include/python3.8 -lpython3.8 -O3
