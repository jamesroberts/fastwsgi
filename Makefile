
.PHONY: all clean

all: libuv.a server.o

libuv.a:
	cd libuv && sh autogen.sh
	cd libuv && ./configure
	cd libuv && $(MAKE)

server.o: fastwsgi/server.c
	mkdir -p bin
	gcc -o bin/server -O3 \
		fastwsgi/request.c fastwsgi/server.c fastwsgi/constants.c fastwsgi/start_response.c \
		fastwsgi/logx.c fastwsgi/common.c fastwsgi/pyhacks.c \
		-Illhttp/include llhttp/src/*.c \
		-Ilibuv/include -Ilibuv/src -Llibuv/.libs -l:libuv.a \
		-I/usr/include/python3.8 -lpython3.8 \
		-lpthread -ldl
		

clean:
	rm -rf bin
