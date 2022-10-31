#ifndef FASTWSGI_SERVER_H_
#define FASTWSGI_SERVER_H_

#include "common.h"
#include "llhttp.h"
#include "request.h"
#include "xbuf.h"

#define max_read_file_buffer_size (25*1000*1000)  // FIXME: change to 128KB
#define max_preloaded_body_chunks 48

static const int def_max_content_length = 999999999;

typedef struct {
    uv_write_t req;  // Placement strictly at the beginning of the structure!
    void * client;   // NULL = not sending
    uv_buf_t bufs[max_preloaded_body_chunks + 2];
} write_req_t;

typedef struct {
    uv_tcp_t server;  // Placement strictly at the beginning of the structure!
    uv_loop_t* loop;
    uv_os_fd_t file_descriptor;
    llhttp_settings_t parser_settings;
    PyObject* wsgi_app;
    char* host;
    int port;
    int backlog;
    uint64_t max_content_length;
} server_t;

typedef enum {
    LS_WAIT            = 0,
    LS_MSG_BEGIN       = 1,
    LS_MSG_URL         = 2,  // URL fully loaded
    LS_MSG_HEADERS     = 3,  // all request headers loaded
    LS_MSG_BODY        = 4,  // load body
    LS_MSG_END         = 5,  // request readed fully
    LS_OK              = 6   // request loaded fully
} load_state_t;

typedef struct {
    uv_tcp_t handle;     // peer connection. Placement strictly at the beginning of the structure! 
    server_t * srv;
    char remote_addr[48];
    xbuf_t rbuf[2];      // buffers for reading from socket
    struct {
        int load_state;
        int64_t http_content_length; // -1 = "Content-Length" not specified
        int chunked;             // Transfer-Encoding: chunked
        int keep_alive;          // 1 = Connection: Keep-Alive or HTTP/1.1
        int expect_continue;     // 1 = Expect: 100-continue
        size_t current_key_len;
        size_t current_val_len;
        PyObject* headers;     // PyDict
        PyObject* wsgi_input_empty;  // empty io.ByteIO object for requests without body
        PyObject* wsgi_input;  // type: io.BytesIO
        int64_t wsgi_input_size;   // total size of wsgi_input PyBytes stream
        llhttp_t parser;
    } request;
    int error;    // error code on process request and response
    xbuf_t head;  // dynamic buffer for request and response headers data
    StartResponse * start_response;
    struct {
        int headers_size;        // size of headers for sending
        int64_t wsgi_content_length; // -1 = "Content-Length" not specified
        PyObject* wsgi_body;
        PyObject* body_iterator;
        size_t body_chunk_num;
        PyObject* body[max_preloaded_body_chunks + 1]; // pleloaded body's chunks (PyBytes)
        int64_t body_preloaded_size; // sum of all preloaded body's chunks
        int64_t body_total_size;
        write_req_t write_req;
    } response;
} client_t;

extern server_t g_srv;

PyObject* run_server(PyObject* self, PyObject* args);

void free_start_response(client_t * client);
void reset_response_body(client_t * client);

int x_send_status(client_t * client, int status);

int call_wsgi_app(client_t * client);
int process_wsgi_response(client_t * client);
int create_response(client_t * client);

typedef enum {
    RF_EMPTY           = 0x00,
    RF_SET_KEEP_ALIVE  = 0x01,
    RF_HEADERS_PYLIST  = 0x02
} response_flag_t;

int build_response(client_t * client, int flags, int status, const void * headers, const void * body, int body_size);
PyObject* wsgi_iterator_get_next_chunk(client_t * client, int outpyerr);


#endif
