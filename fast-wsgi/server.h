#include <Python.h>
#include "uv.h"
#include "llhttp.h"

typedef struct {
    PyObject* host;
    PyObject* port;
} ServerArgs;

typedef struct {
    uv_write_t req;
    uv_buf_t buf;
} write_req_t;

typedef struct {
    uv_tcp_t handle;
    llhttp_t parser;
    write_req_t write_req;
} client_t;

// void run_server(ServerArgs*);