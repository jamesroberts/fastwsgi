#include <Python.h>
#include "uv.h"
#include "llhttp.h"

PyObject* wsgi_app;
char* host;
int port;
int backlog;

uv_tcp_t server;
uv_buf_t response_buf;
uv_loop_t* loop;
uv_os_fd_t fd;

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

PyObject* run_server(PyObject* self, PyObject* args);