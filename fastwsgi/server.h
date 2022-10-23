#ifndef FASTWSGI_SERVER_H_
#define FASTWSGI_SERVER_H_

#include "common.h"
#include "llhttp.h"
#include "request.h"
#include "xbuf.h"

typedef struct {
    uv_write_t req;  // Placement strictly at the beginning of the structure!
    uv_buf_t buf;
} write_req_t;

typedef struct {
    uv_tcp_t server;  // Placement strictly at the beginning of the structure!
    uv_loop_t* loop;
    uv_os_fd_t file_descriptor;
    PyObject* wsgi_app;
    char* host;
    int port;
    int backlog;
} server_t;

typedef struct {
    int error;
    int keep_alive;
} RequestState;

typedef struct {
    uv_tcp_t handle;     // peer connection. Placement strictly at the beginning of the structure! 
    server_t * srv;
    char remote_addr[24];
    struct {
        PyObject* headers;
        char current_header[128];
        llhttp_t parser;
        RequestState state;
    } request;
    struct {
        xbuf_t head;
        xbuf_t body;
    } response;
} client_t;

extern server_t g_srv;

PyObject* run_server(PyObject* self, PyObject* args);

#endif
