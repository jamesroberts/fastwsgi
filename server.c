#include <stdio.h>
#include <stdlib.h>

#include "uv.h"
#include "llhttp.h"

#define SIMPLE_RESPONSE \
    "HTTP/1.1 200 OK\r\n" \
    "Content-Type: text/plain\r\n" \
    "Content-Length: 14\r\n" \
    "\r\n" \
    "Hello, World!\n"

const static char* HOST = "0.0.0.0";
const static int PORT = 5000;
const static int BACKLOG = 256;

static uv_tcp_t server;
static llhttp_settings_t parser_settings;
static uv_buf_t response_buf;

uv_loop_t* loop;

struct sockaddr_in addr;

typedef struct {
    uv_write_t req;
    uv_buf_t buf;
} write_req_t;

typedef struct {
    uv_tcp_t handle;
    llhttp_t parser;
    write_req_t write_req;
} client_t;

void close_cb(uv_handle_t* handle) {
    printf("disconnected\n");
    free(handle);
}

void write_cb(uv_write_t* req, int status) {
    if (status) {
        fprintf(stderr, "Write error %s\n", uv_strerror(status));
    }
    uv_close((uv_handle_t*)req->handle, close_cb);
}

void read_cb(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf) {
    client_t* client = (client_t*)handle;

    if (nread >= 0) {
        enum llhttp_errno err = llhttp_execute(&client->parser, buf->base, nread);
        if (err == HPE_OK) {
            printf("Successfully parsed\n");
            uv_write((uv_write_t*)&client->write_req, (uv_stream_t*)client, &response_buf, 1, write_cb);
        }
        else {
            fprintf(stderr, "Parse error: %s %s\n", llhttp_errno_name(err), client->parser.reason);
        }
    }
    else {
        if (nread != UV_EOF)
            fprintf(stderr, "Read error %s\n", uv_err_name(nread));
        uv_close((uv_handle_t*)handle, close_cb);
    }
    free(buf->base);
}

void alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    printf("Allocating buffer\n");
    buf->base = malloc(suggested_size);
    buf->len = suggested_size;
}

void connection_cb(struct uv_stream_s* handle, int status) {
    if (status < 0) {
        fprintf(stderr, "Connection error %s\n", uv_strerror(status));
        return;
    }

    client_t* client = malloc(sizeof(client_t));

    llhttp_settings_init(&parser_settings);

    uv_tcp_init(loop, &client->handle);

    if (uv_accept((uv_stream_t*)&server, (uv_stream_t*)&client->handle) == 0) {
        llhttp_init(&client->parser, HTTP_BOTH, &parser_settings);
        client->parser.data = client;
        uv_read_start((uv_stream_t*)&client->handle, alloc_cb, read_cb);
    }
}

int handle_on_message_complete(llhttp_t* parser) {
    client_t* client = (client_t*)parser->data;
    uv_write((uv_write_t*)&client->write_req, (uv_stream_t*)client, &response_buf, 1, write_cb);
    return 1;
};

void configure_parser_settings() {
    parser_settings.on_message_complete = handle_on_message_complete;
}

int main() {
    configure_parser_settings();

    loop = uv_default_loop();

    uv_tcp_init(loop, &server);

    response_buf.base = SIMPLE_RESPONSE;
    response_buf.len = sizeof(SIMPLE_RESPONSE);

    uv_ip4_addr(HOST, PORT, &addr);

    int r = uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0);
    if (r) {
        fprintf(stderr, "Bind error %s\n", uv_strerror(r));
        return 1;
    }

    r = uv_listen((uv_stream_t*)&server, BACKLOG, connection_cb);
    if (r) {
        fprintf(stderr, "Listen error %s\n", uv_strerror(r));
        return 1;
    }
    return uv_run(loop, UV_RUN_DEFAULT);
}
