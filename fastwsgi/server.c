#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <uv.h>
#include "llhttp.h"

#include "server.h"
#include "request.h"
#include "constants.h"

void logger(char* message) {
    if (LOGGING_ENABLED)
        fprintf(stdout, ">>> %s\n", message);
}

void close_cb(uv_handle_t* handle) {
    logger("disconnected");
    free(handle);
}

void write_cb(uv_write_t* req, int status) {
    if (status) {
        fprintf(stderr, "Write error %s\n", uv_strerror(status));
    }
    write_req_t* write_req = (write_req_t*)req;
    free(write_req->buf.base);
    free(write_req);
}

void read_cb(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf) {
    client_t* client = (client_t*)handle->data;

    Request* request = malloc(sizeof(Request));
    strcpy(request->remote_addr, client->remote_addr);
    client->parser.data = request;

    if (nread >= 0) {
        enum llhttp_errno err = llhttp_execute(&client->parser, buf->base, nread);
        if (err == HPE_OK) {
            logger("Successfully parsed");
            write_req_t* req = (write_req_t*)malloc(sizeof(write_req_t));
            req->buf = uv_buf_init(request->response_buffer.base, request->response_buffer.len);
            uv_write((uv_write_t*)req, handle, &req->buf, 1, write_cb);
        }
        else {
            fprintf(stderr, "Parse error: %s %s\n", llhttp_errno_name(err), client->parser.reason);
        }
    }
    else {
        if (nread != UV_EOF && nread != UV_ECONNRESET)
            fprintf(stderr, "Read error %s\n", uv_err_name(nread));
        uv_close((uv_handle_t*)handle, close_cb);
    }
    free(request);
    free(buf->base);
    llhttp_reset(&client->parser);
}

void alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    logger("Allocating buffer");
    buf->base = (char*)malloc(suggested_size);
    buf->len = suggested_size;
}

void connection_cb(uv_stream_t* server, int status) {
    if (status < 0) {
        fprintf(stderr, "Connection error %s\n", uv_strerror(status));
        return;
    }

    client_t* client = malloc(sizeof(client_t));

    uv_tcp_init(loop, &client->handle);
    uv_tcp_nodelay(&client->handle, 0);
    uv_tcp_keepalive(&client->handle, 1, 60);

    struct sockaddr sockname;
    struct sockaddr_in* addr = (struct sockaddr_in*)&sockname;
    int socklen = sizeof(sockname);

    uv_tcp_getsockname((uv_tcp_t*)&client->handle, &sockname, &socklen);
    uv_ip4_name(addr, client->remote_addr, sizeof(client->remote_addr));

    client->handle.data = client;

    if (uv_accept(server, (uv_stream_t*)&client->handle) == 0) {
        llhttp_init(&client->parser, HTTP_REQUEST, &parser_settings);
        uv_read_start((uv_stream_t*)&client->handle, alloc_cb, read_cb);
    }
    else {
        uv_close((uv_handle_t*)&client->handle, close_cb);
    }
}

void signal_handler(uv_signal_t* req, int signum) {
    if (signum == SIGINT) {
        uv_stop(loop);
        uv_signal_stop(req);
        exit(0);
    }
}

int main() {
    loop = uv_default_loop();

    configure_parser_settings();
    init_constants();
    init_request_dict();

    uv_ip4_addr(host, port, &addr);

    uv_tcp_init_ex(loop, &server, AF_INET);

    uv_fileno((const uv_handle_t*)&server, &file_descriptor);

    int enabled = 1;
    int so_reuseport = 15;
    setsockopt(file_descriptor, SOL_SOCKET, so_reuseport, &enabled, sizeof(&enabled));

    int err = uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0);
    if (err) {
        fprintf(stderr, "Bind error %s\n", uv_strerror(err));
        return 1;
    }

    err = uv_listen((uv_stream_t*)&server, backlog, connection_cb);
    if (err) {
        fprintf(stderr, "Listen error %s\n", uv_strerror(err));
        return 1;
    }

    uv_signal_t signal;
    uv_signal_init(loop, &signal);
    uv_signal_start(&signal, signal_handler, SIGINT);

    uv_run(loop, UV_RUN_DEFAULT);
    uv_loop_close(loop);
    free(loop);
    return 0;
}

PyObject* run_server(PyObject* self, PyObject* args) {
    PyArg_ParseTuple(args, "Osiii", &wsgi_app, &host, &port, &backlog, &LOGGING_ENABLED);
    main();
    Py_RETURN_NONE;
}