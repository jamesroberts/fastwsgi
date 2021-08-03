#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "uv.h"

static uv_tcp_t server;

void on_new_connection(struct uv_stream_s* handle, int status) {
    assert(handle == &server);
    printf("New connection...\n");

    if (status < 0) {
        fprintf(stderr, "Connection error %s\n", uv_strerror(status));
        // error!
        return;
    }

    uv_tcp_t* client = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));

    uv_tcp_init(uv_default_loop(), client);

    if (uv_accept((uv_stream_t*)&server, (uv_stream_t*)client) == 0) {
        printf("Accepted connection\n");
        // Fix me
        // uv_read_start((uv_stream_t*)client, NULL, NULL);
    }
}

int main() {
    uv_tcp_init(uv_default_loop(), &server);

    struct sockaddr_in addr;
    uv_ip4_addr("0.0.0.0", 5000, &addr);

    int r = uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0);
    if (r) {
        fprintf(stderr, "Bind error %s\n", uv_strerror(r));
        return 1;
    }

    r = uv_listen((uv_stream_t*)&server, 128, on_new_connection);
    if (r) {
        fprintf(stderr, "Listen error %s\n", uv_strerror(r));
        return 1;
    }
    printf("Running server...\n");

    return uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}
