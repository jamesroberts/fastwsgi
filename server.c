#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "uv.h"

#define DEFAULT_PORT 5000
#define DEFAULT_BACKLOG 128

static uv_tcp_t server;
struct sockaddr_in addr;

typedef struct {
    uv_write_t req;
    uv_buf_t buf;
} write_req_t;

void on_close(uv_handle_t* handle) {
    printf("disconnected\n");
    free(handle);
}

void free_write_req(uv_write_t* req) {
    write_req_t* wr = (write_req_t*)req;
    free(wr->buf.base);
    free(wr);
}

void echo_write(uv_write_t* req, int status) {
    if (status) {
        fprintf(stderr, "Write error %s\n", uv_strerror(status));
    }
    free_write_req(req);
}

void echo_read_buffer(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf) {
    // Read data into allocated buffer, for now echo data
    if (nread > 0) {
        write_req_t* req = (write_req_t*)malloc(sizeof(write_req_t));
        req->buf = uv_buf_init(buf->base, nread);
        uv_write((uv_write_t*)req, client, &req->buf, 1, echo_write);
        return;
    }
    if (nread < 0) {
        if (nread != UV_EOF)
            fprintf(stderr, "Read error %s\n", uv_err_name(nread));
        uv_close((uv_handle_t*)client, on_close);
    }
    // Free up the buffer we allocated.
    free(buf->base);
}

void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    // Allocate some buffer to read data from socket
    printf("alloc buffer\n");
    buf->base = malloc(suggested_size);
    buf->len = suggested_size;
}

void on_new_connection(struct uv_stream_s* handle, int status) {
    // Connection callback that handles a new incomming connection
    assert(handle == &server);
    printf("New connection...\n");

    if (status < 0) {
        fprintf(stderr, "Connection error %s\n", uv_strerror(status));
        return;
    }

    uv_tcp_t* client = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));

    uv_tcp_init(uv_default_loop(), client);

    if (uv_accept((uv_stream_t*)&server, (uv_stream_t*)client) == 0) {
        printf("Accepted connection\n");
        uv_read_start((uv_stream_t*)client, alloc_buffer, echo_read_buffer);
    }
}

int main() {
    uv_tcp_init(uv_default_loop(), &server);

    uv_ip4_addr("0.0.0.0", DEFAULT_PORT, &addr);

    int r = uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0);
    if (r) {
        fprintf(stderr, "Bind error %s\n", uv_strerror(r));
        return 1;
    }

    r = uv_listen((uv_stream_t*)&server, DEFAULT_BACKLOG, on_new_connection);
    if (r) {
        fprintf(stderr, "Listen error %s\n", uv_strerror(r));
        return 1;
    }
    printf("Running server...\n");

    return uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}
