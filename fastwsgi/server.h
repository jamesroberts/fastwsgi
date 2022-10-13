#ifndef FASTWSGI_SERVER_H_
#define FASTWSGI_SERVER_H_

#include <Python.h>
#include "uv.h"
#include "llhttp.h"
#include "request.h"

extern PyObject* wsgi_app;


typedef struct {
    uv_write_t req;
    uv_buf_t buf;
} write_req_t;

typedef struct {
    uv_tcp_t handle;
    llhttp_t parser;
    char remote_addr[17];
} client_t;

PyObject* run_server(PyObject* self, PyObject* args);

int LOGGING_ENABLED;
void logger(char* message);

#endif
