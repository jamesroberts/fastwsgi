#ifndef FASTWSGI_SERVER_H_
#define FASTWSGI_SERVER_H_

#include <Python.h>
#include "uv.h"
#include "uv-common.h"
#include "llhttp.h"
#include "request.h"

extern PyObject* wsgi_app;


typedef struct {
    uv_write_t req;
    uv_buf_t buf;
} write_req_t;

typedef struct {
    int error;
    int keep_alive;
} RequestState;

typedef struct {
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

PyObject* run_server(PyObject* self, PyObject* args);

int LOGGING_ENABLED;
void logger(char* message);

#endif
