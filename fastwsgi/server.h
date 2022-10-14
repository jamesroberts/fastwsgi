#ifndef FASTWSGI_SERVER_H_
#define FASTWSGI_SERVER_H_

#include <Python.h>
#include "uv.h"
#include "uv-common.h"
#include "llhttp.h"
#include "request.h"
#include "logx.h"

typedef struct {
    uv_write_t req;
    uv_buf_t buf;
} write_req_t;

typedef struct {
    PyObject* wsgi_app;
    char* host;
    int port;
    int backlog;

    uv_tcp_t server;
    uv_loop_t* loop;
    uv_os_fd_t file_descriptor;
} server_t;

typedef struct {
    int error;
    int keep_alive;
} RequestState;

typedef struct {
    server_t * srv;
    uv_tcp_t handle;     // peer connection
    char remote_addr[24];
    struct {
        PyObject* headers;
        char* current_header;
        llhttp_t parser;
        RequestState state;
    } request;
    struct {
        uv_buf_t buffer;
    } response;
} client_t;

extern server_t g_srv;

PyObject* run_server(PyObject* self, PyObject* args);

#endif
