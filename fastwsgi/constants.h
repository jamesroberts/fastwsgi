#ifndef FASTWSGI_CONSTANTS_H_
#define FASTWSGI_CONSTANTS_H_

#include <Python.h>

typedef struct {
    PyObject* REQUEST_METHOD;
    PyObject* SCRIPT_NAME;
    PyObject* SERVER_NAME;
    PyObject* SERVER_PORT;
    PyObject* SERVER_PROTOCOL;
    PyObject* QUERY_STRING;
    PyObject* PATH_INFO;
    PyObject* HTTP_;
    PyObject* REMOTE_ADDR;
    PyObject* CONTENT_LENGTH;

    PyObject* wsgi_version;
    PyObject* wsgi_url_scheme;
    PyObject* wsgi_errors;
    PyObject* wsgi_run_once;
    PyObject* wsgi_multithread;
    PyObject* wsgi_multiprocess;
    PyObject* wsgi_input;
    PyObject* version;

    PyObject* http_scheme;
    PyObject* HTTP_1_1;
    PyObject* HTTP_1_0;

    PyObject* server_host;
    PyObject* server_port;
    PyObject* empty_string;

    PyObject* module_io;
    PyObject* BytesIO;
    PyObject* close;
    PyObject* write;
    PyObject* read;
    PyObject* truncate;
    PyObject* seek;
    PyObject* tell;
    PyObject* buffer_size;
    PyObject* getvalue;
    PyObject* comma;

    PyObject* i0;
} cvar_t;

extern cvar_t g_cv;

void init_constants();

#endif
