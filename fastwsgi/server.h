#ifndef FASTWSGI_SERVER_H_
#define FASTWSGI_SERVER_H_

#include "common.h"
#include "llhttp.h"
#include "request.h"
#include "xbuf.h"

#define max_preloaded_body_chunks 48

static const size_t MIN_max_chunk_size = 2*1024;
static const size_t def_max_chunk_size = 256*1024;
static const size_t MAX_max_chunk_size = 64*1024*1024;

static const int def_max_content_length = 999999999;

enum {
    MIN_read_buffer_size = 2 * 1024,
    def_read_buffer_size = 64 * 1024,
    MAX_read_buffer_size = 4 * 1024 * 1024
};


typedef struct {
    uv_write_t req;  // Placement strictly at the beginning of the structure!
    void * client;   // NULL = not sending
    uv_buf_t bufs[max_preloaded_body_chunks + 3];
} write_req_t;

typedef struct {
    uv_tcp_t server;  // Placement strictly at the beginning of the structure!
    PyObject * pysrv; // object fastwsgi.py@_Server
    uv_loop_t* loop;
    uv_idle_t worker;  // worker for HTTP pipelining
    int num_pipeline;  // number of active pipelines
    uv_os_fd_t file_descriptor;
    llhttp_settings_t parser_settings;
    PyObject* wsgi_app;
    int ipv6;
    char host[64];
    int port;
    int backlog;
    int hook_sigint;   // 0 - ignore SIGINT, 1 - handle SIGINT, 2 - handle SIGINT with halt prog
    uv_signal_t signal;
    int allow_keepalive;
    size_t read_buffer_size;
    uint64_t max_content_length;
    size_t max_chunk_size;
    int tcp_nodelay;       // 0 = Nagle's algo enabled; 1 = Nagle's algo disabled;
    int tcp_keepalive;     // negative = disabled; 0 = system default; 1...N = timeout in seconds
    int tcp_send_buf_size; // 0 = system default; 1...N = size in bytes
    int tcp_recv_buf_size; // 0 = system default; 1...N = size in bytes
    struct {
        int mode;          // 0 - disabled, 1 - nowait active, 2 - nowait with wait disconnect all peers
        int base_handles;  // number of base handles (listen socket + signal)
    } nowait;
    int exit_code;
} server_t;

typedef enum {
    PS_RESTING    = 0,  // pipeline not used
    PS_ACTIVE     = 1   // pipeline active for reading from master buffer
} pl_status_t;

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
    char remote_addr[64];
    xbuf_t rbuf[2];      // buffers for reading from socket
    struct {
        pl_status_t status;  // pipeline status
        char * buf_base;     // master buffer with pipeline-requests
        char * buf_pos;      // parser cursor position (into master buf)
        char * buf_end;
    } pipeline;
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
        bool parser_locked;
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
        int64_t body_total_written;
        int chunked;    // 1 = chunked sending; 2 = last chunk send
        write_req_t write_req;
    } response;
    // preallocated buffers
    char buf_head_prealloc[2*1024];
    char buf_read_prealloc[1];
} client_t;

extern server_t g_srv;

PyObject * init_server(PyObject * self, PyObject * server);
PyObject * change_setting(PyObject * self, PyObject * args);
PyObject * run_server(PyObject * self, PyObject * server);
PyObject * run_nowait(PyObject * self, PyObject * server);
PyObject * close_server(PyObject * self, PyObject * server);

void free_start_response(client_t * client);
void reset_response_preload(client_t * client);
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

inline void update_log_prefix(void * _client)
{
    client_t * client = (client_t *)_client;
    set_log_client_addr(client ? client->remote_addr : NULL);
}

#endif
