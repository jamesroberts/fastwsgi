#ifndef FASTWSGI_SERVER_H_
#define FASTWSGI_SERVER_H_

#include "common.h"
#include "llhttp.h"
#include "request.h"
#include "xbuf.h"

#define max_read_file_buffer_size (25*1000*1000)  // FIXME: change to 128KB
#define max_preloaded_body_chunks 48


typedef struct {
    uv_write_t req;  // Placement strictly at the beginning of the structure!
    void * client;   // NULL = not sending
    uv_buf_t bufs[max_preloaded_body_chunks + 2];
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
        int wsgi_content_length; // -1 = "Content-Length" not present
        xbuf_t head;
        PyObject* wsgi_body;
        PyObject* body_iterator;
        int body_chunk_num;
        PyObject* body[max_preloaded_body_chunks + 1]; // pleloaded body's chunks (PyBytes)
        int body_preloaded_size; // sum of all preloaded body's chunks
        int body_total_size;
        write_req_t write_req;
    } response;
} client_t;

extern server_t g_srv;

PyObject* run_server(PyObject* self, PyObject* args);

#endif
