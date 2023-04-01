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
    PyObject* wsgi_ver_1_0;  // PyTuple(1, 0)

    PyObject* http_scheme;
    PyObject* HTTP_1_1;
    PyObject* HTTP_1_0;

    PyObject* server_host;
    PyObject* server_port;
    PyObject* empty_string;
    PyObject* empty_bytes;  // b""

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
    PyObject* getbuffer;  // "getbuffer"
    PyObject* comma;

    PyObject* i0;
    PyObject* f0;  // float(0.0)
    PyObject* f0_001;  // float(0.001)

    // ====== ASGI 3.0 ===============
    PyObject* http_version;  // "http_version"
    PyObject* method;  // "method"
    PyObject* scheme;  // "scheme"
    PyObject* path;  // "path"
    PyObject* raw_path;  // "raw_path"
    PyObject* query_string;  // "query_string"
    PyObject* root_path;  // "root_path"
    PyObject* headers;  // "headers"

    PyObject* type;  // "type"
    PyObject* asgi;  // "asgi"
    PyObject* version;  // "version"
    PyObject* spec_version;  // "spec_version"
    PyObject* server;  // "server"
    PyObject* body;  // "body"
    PyObject* more_body;  // "more_body"

    PyObject* v3_0;  // "3.0"
    PyObject* v2_0;  // "2.0"
    PyObject* http;  // "http"
    PyObject* https;  // "https"
    PyObject* http_request;  // "http.request"
    PyObject* status;  // "status"

    PyObject* ContentLength;  // bytes "Content-Length"
    PyObject* TransferEncoding;  // bytes "Transfer-Encoding"

    PyObject* __call__;  // "__call__"
    PyObject* add_done_callback;  // "add_done_callback"
    PyObject* done;  // "done"
    PyObject* result;  // "result"
    PyObject* set_result;  // "set_result"
    PyObject* set_exception;  // "set_exception"

    PyObject* footer_last_chunk;  // b"\r\n0\r\n\r\n"
} cvar_t;

extern cvar_t g_cv;

void init_constants();

#endif
