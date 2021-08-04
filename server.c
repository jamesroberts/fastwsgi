#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "uv.h"
#include "llhttp.h"

#define DEFAULT_PORT 5000
#define DEFAULT_BACKLOG 128

static uv_tcp_t server;
static llhttp_settings_t settings;
static uv_buf_t response_buf;
struct sockaddr_in addr;

typedef struct {
    uv_write_t req;
    uv_buf_t buf;
} write_req_t;


typedef struct {
    uv_tcp_t client;
    llhttp_t parser;
    write_req_t write_req;
} client_t;

void on_close(uv_handle_t* handle) {
    printf("disconnected\n");
    free(handle);
}

// void free_write_req(uv_write_t* req) {
//     write_req_t* wr = (write_req_t*)req;
//     free(wr->buf.base);
//     free(wr);
// }

void echo_write(uv_write_t* req, int status) {
    if (status) {
        fprintf(stderr, "Write error %s\n", uv_strerror(status));
    }
    // Close the socket
    // uv_close((uv_handle_t*)req->handle, on_close);
}

int handle_on_message_complete(llhttp_t* parser) {
    printf("HTTP message\n");
    client_t* client = (client_t*)parser->data;
    uv_write((uv_write_t*)&client->write_req, (uv_stream_t*)client, &response_buf, 1, echo_write);
    return 1;
};

#define RESPONSE \
    "HTTP/1.1 200 OK\r\n" \
    "Content-Type: text/plain\r\n" \
    "Content-Lenght: 12\r\n" \
    "\r\n" \
    "hello world\n"

void echo_read_buffer(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf) {
    // Read data into allocated buffer, for now echo data

    client_t* client = (client_t*)handle;

    if (nread >= 0) {
        // write(1, buf->base, nread);
        // Need to parse http
        enum llhttp_errno err = llhttp_execute(&client->parser, buf->base, nread);
        if (err == HPE_OK) {
            /* Successfully parsed! */
            printf("Successfully parsed\n");
        }
        else {
            fprintf(stderr, "Parse error: %s %s\n", llhttp_errno_name(err), client->parser.reason);
        }

        // Start response
        printf("HTTP message\n");
        uv_write((uv_write_t*)&client->write_req, (uv_stream_t*)client, &response_buf, 1, echo_write);




        // uv_write((uv_write_t*)&client->write_req, handle, &response_buf, 1, echo_write);
        // write_req_t* req = (write_req_t*)malloc(sizeof(write_req_t));
        // req->buf = uv_buf_init(buf->base, nread);
        // uv_write((uv_write_t*)req, handle, &req->buf, 1, echo_write);
        // printf("sent response\n");
        // return;
    }
    else {
        // Might be an error, probably an EOF, but regardless, close the connection
        uv_close((uv_handle_t*)handle, on_close);
    }


    if (nread < 0) {
        if (nread != UV_EOF)
            fprintf(stderr, "Read error %s\n", uv_err_name(nread));
        printf("Closing...\n");
        uv_close((uv_handle_t*)handle, on_close);
    }
    // Free up the buffer we allocated.
    free(buf->base);
    // free(response_buf.base);
}

void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    // Allocate some buffer to read data from socket
    printf("alloc buffer\n");
    buf->base = (char*)malloc(suggested_size);
    buf->len = suggested_size;
}

void on_new_connection(struct uv_stream_s* handle, int status) {
    // Connection callback that handles a new incomming connection
    // assert(handle == &server);
    printf("New connection...\n");

    if (status < 0) {
        fprintf(stderr, "Connection error %s\n", uv_strerror(status));
        return;
    }

    client_t* client = malloc(sizeof(client_t));
    // uv_tcp_t* client = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));

    llhttp_settings_init(&settings);

    uv_tcp_init(uv_default_loop(), &client->client);

    if (uv_accept((uv_stream_t*)&server, (uv_stream_t*)&client->client) == 0) {
        printf("Accepted connection\n");
        llhttp_init(&client->parser, HTTP_BOTH, &settings);
        client->parser.data = client;
        uv_read_start((uv_stream_t*)&client->client, alloc_buffer, echo_read_buffer);
    }
}

int main() {
    uv_tcp_init(uv_default_loop(), &server);

    settings.on_message_complete = handle_on_message_complete;
    response_buf.base = RESPONSE;
    response_buf.len = sizeof(RESPONSE);

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
