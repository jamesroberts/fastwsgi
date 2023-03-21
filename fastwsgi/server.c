#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uv.h"
#include "uv-common.h"
#include "llhttp.h"

#include "server.h"
#include "request.h"
#include "constants.h"

server_t g_srv;
static int g_srv_inited = 0;

#define MAGIC_CLIENT ((void *)0xFFAB4321)

void alloc_cb(uv_handle_t * handle, size_t suggested_size, uv_buf_t * buf);
void read_cb(uv_stream_t * handle, ssize_t nread, const uv_buf_t * buf);
int stream_write(client_t * client);

void idle_worker_cb(uv_idle_t * handle);
void pipeline_cb(uv_handle_t * handle, void * arg);
int pipeline_close(client_t * client, bool start_reading);

void free_read_buffer(client_t * client, void * data)
{
    for (size_t i = 0; i < ARRAY_SIZE(client->rbuf); i++) {
        xbuf_t * r_buf = &client->rbuf[i];
        if (data && r_buf->data == data) {
            //LOGi("%s: free buffer = %p", __func__, data);
            r_buf->size = 0;  // free buffer
            return;
        }
        if (!data) {
            xbuf_free(r_buf);
        }
    }
}

typedef enum {
    CA_OK           = 0,  // continue read from socket
    CA_CLOSE        = 1,
    CA_SHUTDOWN     = 2
} client_action_t;

void close_cb(uv_handle_t * handle)
{
    client_t * client = (client_t *)handle;
    update_log_prefix(client);
    LOGn("disconnected =================================");
    pipeline_close(client, false);
    Py_XDECREF(client->request.headers);
    Py_XDECREF(client->request.wsgi_input_empty);
    Py_XDECREF(client->request.wsgi_input);
    xbuf_free(&client->head);
    free_start_response(client);
    reset_response_body(client);
    free_read_buffer(client, NULL);
    free(client);
    update_log_prefix(NULL);
}

void close_connection(client_t * client)
{
    if (!uv_is_closing((uv_handle_t*)client))
        uv_close((uv_handle_t*)client, close_cb);
}

void shutdown_cb(uv_shutdown_t * req, int status)
{
    uv_handle_t * client = (uv_handle_t *)req->handle;
    update_log_prefix(client);
    LOGt("%s: status = %d", __func__, status);
    if (!uv_is_closing(client))
        uv_close(client, close_cb);
    free(req);
}

void shutdown_connection(client_t * client)
{
    bool enotconn = is_stream_notconn((uv_stream_t *)client);
    if (!enotconn) {
        uv_shutdown_t* shutdown = malloc(sizeof(uv_shutdown_t));
        if (shutdown) {
            int rc = uv_shutdown(shutdown, (uv_stream_t *)client, shutdown_cb);
            if (rc == 0)
                return;
            free(shutdown); // uv_shutdown returned UV_ENOTCONN
        }
    }
    close_connection(client);
}

typedef struct {
    uv_write_t req;
    uv_buf_t buf;
    void * client;
    char data[1];
} x_write_req_t;

void x_write_cb(uv_write_t * req, int status)
{
    free(req);
}

int x_send_status(client_t * client, int status)
{
    size_t buf_size = sizeof(x_write_req_t) + 640;
    x_write_req_t * wreq = (x_write_req_t *)malloc(buf_size);
    wreq->client = client;
    const char * status_name = get_http_status_name(status);
    if (!status_name)
        status_name = "_unknown_";
    char * buf = wreq->data;
    int len = 0;
    len += sprintf(buf + len, "HTTP/1.1 %d %s\r\n", status, status_name);
    //len += sprintf(buf + len, "Content-Length: 0\r\n");
    len += sprintf(buf + len, "\r\n");
    wreq->buf.len = len;
    wreq->buf.base = buf;
    LOGi("%s: \"%s\"", __func__, buf);
    uv_write((uv_write_t*)wreq, (uv_stream_t*)client, &wreq->buf, 1, x_write_cb);
    return 0;
}

void write_cb(uv_write_t * req, int status)
{
    int close_conn = 0;
    write_req_t * wreq = (write_req_t*)req;
    client_t * client = (client_t *)wreq->client;
    update_log_prefix(client);
    if (status != 0) {
        LOGe("%s: Write error: %s", __func__, uv_strerror(status));
        goto fin;
    }
    client->response.body_total_written += client->response.body_preloaded_size;
    reset_response_preload(client);
    if (client->response.chunked == 2) {
        LOGd("%s: last chunk sended!", __func__);
        goto fin;
    }
    if (client->response.chunked == 0) {
        int64_t body_total_size = client->response.body_total_size;
        if (client->response.body_total_written > body_total_size) {
            LOGc("%s: ERROR: body_total_written > body_total_size", __func__);
            status = -1; // critical error -> close_connection
            goto fin;
        }
        if (client->response.body_total_written == body_total_size) {
            LOGd("%s: Response body is completely streamed. body_total_size = %lld", __func__, (long long)body_total_size);
            goto fin;
        }
    }
    client->error = 0;
    PyObject * chunk = wsgi_iterator_get_next_chunk(client, 0);
    if (!chunk) {
        if (client->error) {
            LOGe("%s: ERROR on read response body", __func__);
            status = -1; // error -> close_connection
            goto fin;
        }
        if (client->response.chunked == 1) {
            xbuf_reset(&client->head);
            xbuf_add_str(&client->head, "0\r\n\r\n");
            client->response.headers_size = client->head.size;
            client->response.chunked = 2;            
            stream_write(client);
            LOGd("%s: end of iterable response body (chunked)", __func__);
            return;
        }
        LOGd("%s: end of iterable response body", __func__);
        goto fin;
    }
    Py_ssize_t csize = PyBytes_GET_SIZE(chunk);
    xbuf_reset(&client->head);
    if (client->response.chunked == 0) {
        client->response.headers_size = 0; // data without header
    } else {
        char * buf = xbuf_expand(&client->head, 48);
        client->head.size += sprintf(buf, "%X\r\n", (int)csize);
        client->response.headers_size = client->head.size;
    }
    client->response.body[0] = chunk;
    client->response.body_chunk_num = 1;
    client->response.body_preloaded_size = csize;
    stream_write(client);
    return;

fin:
    if (status < 0) {
        close_conn = 1;
    }
    if (!client->request.keep_alive || !client->srv->allow_keepalive) {
        close_conn = 1;
    }
    if (!close_conn) {
        reset_response_body(client);
        wreq->client = NULL;  // free write_req
        if (client->pipeline.status == PS_RESTING) {
            uv_read_start((uv_stream_t *)client, alloc_cb, read_cb);
        }
    }
    if (close_conn) {
        close_connection(client);
    }
}

int stream_write(client_t * client)
{
    write_req_t * wreq = &client->response.write_req;
    uv_buf_t * buf = wreq->bufs;
    int total_len = 0;
    int nbufs = 0;
    if (client->response.headers_size > 0) {
        if (client->response.headers_size != client->head.size)
            return CA_OK; // error ???
        buf->base = client->head.data;
        buf->len = client->head.size;
        buf++;
        nbufs++;
        total_len += client->head.size;
    }
    if (client->response.body_preloaded_size > 0) {
        for (size_t i = 0; i < client->response.body_chunk_num; i++) {
            Py_ssize_t size = PyBytes_GET_SIZE(client->response.body[i]);
            char * data = PyBytes_AS_STRING(client->response.body[i]);
            buf->base = data;
            buf->len = (unsigned int)size;
            buf++;
            nbufs++;
            total_len += (int)size;
        }
    }
    if (client->response.chunked == 1) {
        buf->base = "\r\n";
        buf->len = 2;
        buf++;
        nbufs++;
        total_len += 2;
    }
    uv_read_stop((uv_stream_t*)client);
    LOGi("%s: %d bytes", __func__, total_len);
    wreq->client = client;
    uv_write((uv_write_t*)wreq, (uv_stream_t*)client, wreq->bufs, nbufs, write_cb);
    return CA_OK;
}

int send_fatal(client_t * client, int status, const char* error_string)
{
    if (!status)
        status = HTTP_STATUS_BAD_REQUEST;
    LOGe("%s: %d", __func__, status);
    if (client->response.write_req.client == NULL) {
        int body_size = error_string ? strlen(error_string) : 0;
        build_response(client, 0, status, NULL, error_string, body_size);
        stream_write(client);
    }
    return CA_SHUTDOWN;
}

int send_error(client_t * client, int status, const char* error_string)
{
    if (!status)
        status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
    LOGe("%s: %d", __func__, status);
    if (client->response.write_req.client == NULL) {
        int flags = (client->request.keep_alive) ? RF_SET_KEEP_ALIVE : 0;
        int body_size = error_string ? strlen(error_string) : 0;
        build_response(client, flags, status, NULL, error_string, body_size);
        stream_write(client);
    }
    if (client->request.keep_alive && client->srv->allow_keepalive)
        return CA_OK;
    
    return CA_SHUTDOWN;
}

int pipeline_close(client_t * client, bool start_reading)
{
    if (client->pipeline.buf_base) {
        free_read_buffer(client, client->pipeline.buf_base);
        client->pipeline.buf_base = NULL;
    }
    client->pipeline.buf_pos = NULL;
    if (client->pipeline.status == PS_RESTING)
        return -1;  // pipeline already closed

    g_srv.num_pipeline--;
    if (g_srv.num_pipeline == 0) {
        uv_idle_stop(&g_srv.worker);
    }
    LOGi("%s: --------- %s", __func__, (g_srv.num_pipeline == 0) ? "LAST" : "");
    client->pipeline.status = PS_RESTING;
    if (start_reading) {
        uv_read_start((uv_stream_t *)client, alloc_cb, read_cb);
    }
    return 0;
}

void idle_worker_cb(uv_idle_t * handle)
{
    if (g_srv.num_pipeline == 0)
        return;

    uv_walk(g_srv.loop, pipeline_cb, NULL);
}

void pipeline_cb(uv_handle_t * handle, void * arg)
{
    if (handle->data != MAGIC_CLIENT)
        return;

    client_t * client = (client_t *)handle;
    if (client->response.write_req.client) {
        // do not call read_cb until active write
        return;
    }
    update_log_prefix(client);
    if (client->pipeline.status == PS_RESTING) {
        return;
    }
    llhttp_resume(&client->request.parser);
    ssize_t nread = (size_t)client->pipeline.buf_end - (size_t)client->pipeline.buf_pos;
    uv_buf_t buf;
    buf.base = client->pipeline.buf_pos;
    buf.len = (int)nread;
    LOGi("%s: not parsed data size = %d", __func__, (int)nread);
    read_cb((uv_stream_t *)client, nread, &buf);
    // continue read master buffer
}

void read_cb(uv_stream_t * handle, ssize_t nread, const uv_buf_t * buf)
{
    int err = 0;
    int act = CA_OK;
    client_t * client = (client_t *)handle;
    llhttp_t * parser = &client->request.parser;
    update_log_prefix(client);

    if (nread == 0) {
        LOGd("read_cb: nread = 0");
        goto fin;
    }
    if (nread < 0) {
        char err_name[128];
        uv_err_name_r((int)nread, err_name, sizeof(err_name) - 1);
        LOGd("read_cb: nread = %d  error: %s", (int)nread, err_name);
        if (nread == UV_EOF) {  // remote peer disconnected
            act = CA_CLOSE;
            goto fin;
        }
        LOGe_IF(nread != UV_ECONNRESET, "%s: Read error: %s", __func__, err_name);
        act = CA_SHUTDOWN;
        goto fin;
    }

    LOGd("read_cb: [nread = %d]", (int)nread);
    if (g_log_level >= LL_TRACE) {
        if ((ssize_t)buf->len > nread)
            buf->base[nread] = 0;
        LOGt(buf->base);
    }
    
    client->request.parser_locked = true;
    enum llhttp_errno error = llhttp_execute(parser, buf->base, nread);
    if (error == HPE_PAUSED) {
        char * pos = (char *)llhttp_get_error_pos(parser);
        if (pos >= buf->base + nread) {
            if (client->pipeline.status >= PS_ACTIVE) {
                pipeline_close(client, true);  // master buffer freed
                buf = NULL;  // block double "free" call for master buffer
                LOGd("%s: PIPELINE deactivated! (FULL)", __func__);
            }
            client->request.parser_locked = false;
            error = HPE_OK;  // received data fully parsed
        }
        else {
            if (client->pipeline.status == PS_RESTING) {
                client->pipeline.status = PS_ACTIVE;
                client->pipeline.buf_base = buf->base;
                client->pipeline.buf_pos = pos;
                client->pipeline.buf_end = buf->base + nread;
                LOGi("%s: PIPELINE Activate: pos = %p", __func__, pos);
                if (g_srv.num_pipeline == 0) {
                    uv_idle_start(&g_srv.worker, idle_worker_cb);
                }
                g_srv.num_pipeline++;
            }
            if (g_log_level >= LL_DEBUG) {
                ssize_t s1 = (size_t)pos - (size_t)client->pipeline.buf_base;
                ssize_t s2 = (size_t)client->pipeline.buf_end - (size_t)pos;
                LOGd("%s: pos = %p (%d + %d = %d)", __func__, pos, (int)s1, (int)s2, (int)(s1+s2));
            }
            client->pipeline.buf_pos = pos;
            uv_read_stop(handle);
        }
        error = HPE_OK;
    }
    if (error != HPE_OK) {
        const char * err_pos = llhttp_get_error_pos(parser);
        LOGe("Parse error: %s %s\n", llhttp_errno_name(error), client->request.parser.reason);
        act = send_fatal(client, HTTP_STATUS_BAD_REQUEST, NULL);
        err = 0;  // skip call send_error
        goto fin;
    }
    if (client->request.load_state < LS_MSG_END) {
        if (client->pipeline.status >= PS_ACTIVE) {
            if (buf->base + nread < client->pipeline.buf_end) {
                LOGc("%s: incorrect PIPELINE chunk", __func__);
                err = HTTP_STATUS_BAD_REQUEST;
            }
            pipeline_close(client, true);  // master buffer freed
            buf = NULL;  // block double "free" call for master buffer
            LOGd("%s: PIPELINE deactivated! (partial)", __func__);
            client->request.parser_locked = true;
            if (err) {            
                act = send_fatal(client, err, NULL);
                err = 0;  // skip call send_error
                goto fin;
            }
        }
        if (client->error) {
            err = HTTP_STATUS_BAD_REQUEST;
            goto fin;
        }
        LOGt("http chunk parsed: load_state = %d, wsgi_input_size = %lld",
            (long long)client->request.load_state, (long long)client->request.wsgi_input_size);
        // continue read from socket (or read from PIPELINE master buffer)
        goto fin;
    }
    if (client->request.load_state != LS_OK) {
        // error from callback function "on_message_complete"
        if (client->request.expect_continue && client->error == HTTP_STATUS_EXPECTATION_FAILED) {
            client->request.expect_continue = 0;
            err = HTTP_STATUS_EXPECTATION_FAILED;
            goto fin;
        }
        err = HTTP_STATUS_BAD_REQUEST;
        goto fin;
    }
    LOGd("HTTP request successfully parsed (wsgi_input_size = %lld)", (long long)client->request.wsgi_input_size);
    err = call_wsgi_app(client);
    if (err) {
        goto fin;
    }
    err = process_wsgi_response(client);
    if (err) {
        goto fin;
    }
    err = create_response(client);
    if (err) {
        goto fin;
    }
    LOGi("Response created! (len = %d+%lld)", client->head.size, (long long)client->response.body_preloaded_size);
    act = stream_write(client);

fin:
    if (buf && buf->base)
        free_read_buffer(client, buf->base);

    if (PyErr_Occurred()) {
        if (err == 0)
            err = HTTP_STATUS_INTERNAL_SERVER_ERROR;
        PyErr_Print();
        PyErr_Clear();
    }
    if (err && act == CA_OK) {
        if (err < HTTP_STATUS_BAD_REQUEST)
            err = HTTP_STATUS_BAD_REQUEST;
        act = send_error(client, err, NULL);
    }
    if (client->request.parser_locked == false) {
        llhttp_reset(&client->request.parser);
    }
    if (client->request.load_state >= LS_MSG_END) {
        client->request.load_state = LS_WAIT;
    }
    if (act == CA_SHUTDOWN) {
        uv_read_stop(handle);
        shutdown_connection(client);
        return;
    }
    if (act == CA_CLOSE) {
        uv_read_stop(handle);
        close_connection(client);
        return;
    }
}

void alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
    client_t * client = (client_t *)handle;
    update_log_prefix(client);
    const int read_buffer_size = (int)g_srv.read_buffer_size;
    LOGt("%s: size = %d (suggested = %d)", __func__, read_buffer_size, (int)suggested_size);
    buf->base = NULL;
    buf->len = 0;
    if (client->pipeline.status >= PS_ACTIVE) {
        LOGc("%s: __undefined_behavior__ PIPELINE is active", __func__);
        return;
    }
    int buf_prealloc = 0;
    xbuf_t * rb = NULL;
    for (size_t i = 0; i < ARRAY_SIZE(client->rbuf); i++) {
        xbuf_t * r_buf = &client->rbuf[i];
        if (r_buf->data == client->buf_read_prealloc)
            buf_prealloc = 1;  // already used

        if (r_buf->size > 0)
            continue;  // buffer used!

        if (r_buf->capacity >= read_buffer_size) {
            // vacant preallocated buffer
            r_buf->size = read_buffer_size;  // set used flag
            buf->len = read_buffer_size;
            buf->base = r_buf->data;
            buf->base[0] = 0;
            //LOGi("%s: get preallocated buf %p", __func__, buf->base);
            return;
        }
        if (!rb && r_buf->capacity == 0)
            rb = r_buf;
    }
    if (!rb)  // all read buffers used!
        return;  // error

    int err = 0;
    if (buf_prealloc == 0) {
        err = xbuf_init2(rb, client->buf_read_prealloc, read_buffer_size + 3);
    } else {
        err = xbuf_init(rb, NULL, read_buffer_size + 1);
    }
    if (err)
        return;  // error

    buf->len = read_buffer_size;
    buf->base = rb->data;
    buf->base[0] = 0;
    LOGd("%s: alloc new buffer %p (size = %d)", __func__, buf->base, (int)buf->len);
}

void connection_cb(uv_stream_t * server, int status)
{
    update_log_prefix(NULL);
    if (status < 0) {
        LOGe("Connection error %s\n", uv_strerror(status));
        return;
    }
    LOGi("new connection =================================");
    client_t* client = calloc(1, sizeof(client_t) + g_srv.read_buffer_size + 8);
    client->srv = &g_srv;

    uv_tcp_init(g_srv.loop, &client->handle);
    
    uv_tcp_nodelay(&client->handle, (g_srv.tcp_nodelay > 0) ? 1 : 0);
    
    if (g_srv.tcp_keepalive < 0)
        uv_tcp_keepalive(&client->handle, 0, 60);  // disable

    if (g_srv.tcp_keepalive >= 1)
        uv_tcp_keepalive(&client->handle, 1, g_srv.tcp_keepalive);  // enable and set timeout

    if (g_srv.tcp_send_buf_size > 0) 
        uv_send_buffer_size((uv_handle_t *)client, &g_srv.tcp_send_buf_size);

    if (g_srv.tcp_recv_buf_size > 0) 
        uv_recv_buffer_size((uv_handle_t *)client, &g_srv.tcp_recv_buf_size);

    client->handle.data = MAGIC_CLIENT;

    int rc = uv_accept(server, (uv_stream_t*)&client->handle);
    if (rc) {
        uv_close((uv_handle_t*)&client->handle, close_cb);
        return;
    }
    sockaddr_t sock_addr;
    memset(&sock_addr, 0, sizeof(sock_addr));
    int sock_len = sizeof(sock_addr);
    rc = uv_tcp_getpeername((uv_tcp_t*)&client->handle, &sock_addr.addr, &sock_len);
    LOGw_IF(rc, "%s: cannot get remote addr (err = %d)", __func__, rc);
    if (rc == 0) {
        char ip[48];
        rc = uv_ip_name(&sock_addr.addr, ip, sizeof(ip));
        LOGw_IF(rc, "%s: cannot get remote IP-addr (err = %d)", __func__, rc);
        if (rc)
            ip[0] = 0;
        if (sock_addr.addr.sa_family == AF_INET6) {
            sprintf(client->remote_addr, "[%s]:%d", ip, (int)sock_addr.in6.sin6_port);
        } else {
            sprintf(client->remote_addr, "%s:%d", ip, (int)sock_addr.in4.sin_port);
        }
    }
    update_log_prefix(client);
    LOGn("connected =================================");
    llhttp_init(&client->request.parser, HTTP_REQUEST, &g_srv.parser_settings);
    client->request.parser.data = client;
    uv_read_start((uv_stream_t*)&client->handle, alloc_cb, read_cb);
}

void signal_handler(uv_signal_t * req, int signum)
{
    if (signum == SIGINT) {
        uv_stop(g_srv.loop);
        uv_signal_stop(req);
        if (g_srv.hook_sigint == 2) {
            update_log_prefix(NULL);
            LOGw("%s: halt process", __func__);
            exit(0);
        }
        g_srv.exit_code = 1; // server interrupted by SIGINT
    }
}

int init_srv()
{
    int hr = -1;
    if (g_srv_inited)
        return -1;

    g_srv.loop = uv_default_loop();

    configure_parser_settings(&g_srv.parser_settings);
    init_constants();
    init_request_dict();

    sockaddr_t addr;
    int tcp_flags = 0;
    if (g_srv.ipv6) {
        tcp_flags = AF_INET6;
        uv_ip6_addr(g_srv.host, g_srv.port, &addr.in6);
    } else {
        tcp_flags = AF_INET;
        uv_ip4_addr(g_srv.host, g_srv.port, &addr.in4);
    }
    uv_tcp_init_ex(g_srv.loop, &g_srv.server, tcp_flags);

    uv_fileno((const uv_handle_t*)&g_srv.server, &g_srv.file_descriptor);

    int enabled = 1;
#ifdef _WIN32
    //uv__socket_sockopt((uv_handle_t*)&g_srv.server, SO_REUSEADDR, &enabled);
#else
    int so_reuseport = 15;  // SO_REUSEPORT
    uv__socket_sockopt((uv_handle_t*)&g_srv.server, so_reuseport, &enabled);
#endif

    int err = uv_tcp_bind(&g_srv.server, &addr.addr, 0);
    if (err) {
        LOGe("Bind error %s\n", uv_strerror(err));
        hr = -5;
        goto fin;
    }
    err = uv_listen((uv_stream_t*)&g_srv.server, g_srv.backlog, connection_cb);
    if (err) {
        LOGe("Listen error %s\n", uv_strerror(err));
        hr = -6;
        goto fin;
    }    
    if (g_srv.hook_sigint > 0) {
        uv_signal_init(g_srv.loop, &g_srv.signal);
        uv_signal_start(&g_srv.signal, signal_handler, SIGINT);
    }
    if (1) {  // always enable support HTTP pipelining
        uv_idle_init(g_srv.loop, &g_srv.worker);
        g_srv.worker.data = NULL;
    }
    g_srv_inited = 1;
    hr = 0;

fin:
    if (hr) {
        if (g_srv.signal.signal_cb)
            uv_signal_stop(&g_srv.signal);

        if (g_srv.worker.type == UV_IDLE) {
            uv_idle_stop(&g_srv.worker);
            uv_close((uv_handle_t *)&g_srv.worker, NULL);
        }
        if (hr <= -5)
            uv_close((uv_handle_t *)&g_srv, NULL);

        if (g_srv.loop)
            uv_loop_close(g_srv.loop);

        memset(&g_srv, 0, sizeof(g_srv));
    }    
    return hr;
}

PyObject * init_server(PyObject * Py_UNUSED(self), PyObject * server)
{
    int64_t rv;

    update_log_prefix(NULL);
    if (g_srv_inited) {
        PyErr_Format(PyExc_Exception, "server already inited");
        return PyLong_FromLong(-1000);
    }
    memset(&g_srv, 0, sizeof(g_srv));
    g_srv.pysrv = server;

    int64_t loglevel = get_obj_attr_int(server, "loglevel");
    if (loglevel == LLONG_MIN) {
        PyErr_Format(PyExc_ValueError, "Option loglevel not defined");
        return PyLong_FromLong(-1010);
    }
    set_log_level((int)loglevel);

    g_srv.wsgi_app = PyObject_GetAttrString(server, "app");
    Py_XDECREF(g_srv.wsgi_app);
    if (!g_srv.wsgi_app) {
        PyErr_Format(PyExc_ValueError, "Option app not defined");
        return PyLong_FromLong(-1011);
    }    

    const char * host = get_obj_attr_str(server, "host");
    if (!host || strlen(host) >= sizeof(g_srv.host) - 1) {
        PyErr_Format(PyExc_ValueError, "Option host not defined");
        return PyLong_FromLong(-1012);
    }
    strcpy(g_srv.host, host);
    g_srv.ipv6 = (strchr(host, ':') == NULL) ? 0 : 1;

    int64_t port = get_obj_attr_int(server, "port");
    if (port == LLONG_MIN) {
        PyErr_Format(PyExc_ValueError, "Option port not defined");
        return PyLong_FromLong(-1013);
    }
    g_srv.port = (int)port;

    int64_t backlog = get_obj_attr_int(server, "backlog");
    if (backlog == LLONG_MIN) {
        PyErr_Format(PyExc_ValueError, "Option backlog not defined");
        return PyLong_FromLong(-1014);
    }
    g_srv.backlog = (int)backlog;

    rv = get_obj_attr_int(server, "hook_sigint");
    g_srv.hook_sigint = (rv >= 0) ? (int)rv : 2;

    rv = get_obj_attr_int(server, "allow_keepalive");
    g_srv.allow_keepalive = (rv == 0) ? 0 : 1;

    rv = get_obj_attr_int(server, "max_content_length");
    if (rv == LLONG_MIN) {
        rv = get_env_int("FASTWSGI_MAX_CONTENT_LENGTH");
    }
    g_srv.max_content_length = (rv >= 0) ? rv : def_max_content_length;
    if (g_srv.max_content_length >= INT_MAX)
        g_srv.max_content_length = INT_MAX - 1;

    rv = get_obj_attr_int(server, "max_chunk_size");
    if (rv == LLONG_MIN) {
        rv = get_env_int("FASTWSGI_MAX_CHUNK_SIZE");
    }
    g_srv.max_chunk_size = (rv >= 0) ? (size_t)rv : (size_t)def_max_chunk_size;
    g_srv.max_chunk_size = _min(g_srv.max_chunk_size, MAX_max_chunk_size);
    g_srv.max_chunk_size = _max(g_srv.max_chunk_size, MIN_max_chunk_size);

    rv = get_obj_attr_int(server, "read_buffer_size");
    if (rv == LLONG_MIN) {
        rv = get_env_int("FASTWSGI_READ_BUFFER_SIZE");
    }
    g_srv.read_buffer_size = (rv >= 0) ? (size_t)rv : (size_t)def_read_buffer_size;
    g_srv.read_buffer_size = _min(g_srv.read_buffer_size, MAX_read_buffer_size);
    g_srv.read_buffer_size = _max(g_srv.read_buffer_size, MIN_read_buffer_size);

    rv = get_obj_attr_int(server, "tcp_nodelay");
    g_srv.tcp_nodelay = (rv >= 0) ? (int)rv : 0;

    rv = get_obj_attr_int(server, "tcp_keepalive");
    g_srv.tcp_keepalive = (rv >= -1) ? (int)rv : 0;

    rv = get_obj_attr_int(server, "tcp_send_buf_size");
    g_srv.tcp_send_buf_size = (rv >= 0) ? (int)rv : 0;

    rv = get_obj_attr_int(server, "tcp_recv_buf_size");
    g_srv.tcp_recv_buf_size = (rv >= 0) ? (int)rv : 0;

    rv = get_obj_attr_int(server, "nowait");
    g_srv.nowait.mode = (rv <= 0) ? 0 : (int)rv;

    int hr = init_srv();
    if (hr) {
        PyErr_Format(PyExc_Exception, "Cannot init TCP server. Error = %d", hr);
    }
    return PyLong_FromLong(hr);
}

PyObject * change_setting(PyObject * Py_UNUSED(self), PyObject * args)
{
    PyObject * server = NULL;
    char * name = NULL;

    int rc = PyArg_ParseTuple(args, "Os", &server, &name);
    if (rc != 1)
        return PyLong_FromLong(-2);

    if (!server)
        return PyLong_FromLong(-3);

    if (!name || strlen(name) < 2)
        return PyLong_FromLong(-4);

    if (strcmp(name, "allow_keepalive") == 0) {
        int64_t rv = get_obj_attr_int(server, name);
        if (rv == 0 || rv == 1) {
            g_srv.allow_keepalive = (int)rv;
            LOGn("%s: SET allow_keepalive = %d", __func__, g_srv.allow_keepalive);
            return PyLong_FromLong(0);
        }
        return PyLong_FromLong(-5); // unsupported value
    }
    return PyLong_FromLong(-1);  // unknown setting
}

PyObject * run_server(PyObject * self, PyObject * server)
{
    if (!g_srv_inited) {
        PyErr_Format(PyExc_Exception, "server not inited");
        return PyLong_FromLong(-1);
    }
    uv_run(g_srv.loop, UV_RUN_DEFAULT);
    
    const char * reason = (g_srv.exit_code == 1) ? "(SIGINT)" : "";
    LOGn("%s: FIN %s", __func__, reason);
    PyObject * rc = close_server(self, server);
    Py_DECREF(rc);
    return PyLong_FromLong(g_srv.exit_code);
}

PyObject * run_nowait(PyObject * self, PyObject * server)
{
    int ret_code = 0;
    if (!g_srv_inited) {
        PyErr_Format(PyExc_Exception, "server not inited!");
        return PyLong_FromLong(-1);
    }
    if (g_srv.nowait.base_handles == 0) {
        uv_run(g_srv.loop, UV_RUN_NOWAIT);
        g_srv.nowait.base_handles = g_srv.loop->active_handles;
        LOGd("%s: base_handles = %d", __func__, g_srv.nowait.base_handles);
    }
    int idle_runs = 0;
    while (1) {
        int rc = uv_run(g_srv.loop, UV_RUN_NOWAIT);
        if (rc != 0) {
            // https://docs.libuv.org/en/v1.x/loop.html?highlight=uv_run#c.uv_run
            // more callbacks are expected (meaning you should run the event loop again sometime in the future.
        }
        if (g_srv.exit_code != 0) {
            ret_code = g_srv.exit_code;
            break;
        }
        if (g_srv.nowait.mode == 1) {
            ret_code = 0;
            break;
        }
        if ((int)g_srv.loop->active_handles > g_srv.nowait.base_handles) {
            idle_runs = 0;
            continue;  // clients are still connected
        }
        idle_runs++;
        if (idle_runs >= 2) {
            ret_code = 0;
            break;
        }
    }
    return PyLong_FromLong(ret_code);
}

PyObject * close_server(PyObject * Py_UNUSED(self), PyObject * Py_UNUSED(server))
{
    if (g_srv_inited) {
        update_log_prefix(NULL);
        LOGn("%s", __func__);
        if (g_srv.signal.signal_cb) {
            uv_signal_stop(&g_srv.signal);
            g_srv.signal.signal_cb = NULL;
        }
        if (g_srv.worker.type == UV_IDLE) {
            uv_idle_stop(&g_srv.worker);
            uv_close((uv_handle_t *)&g_srv.worker, NULL);
        }
        uv_close((uv_handle_t *)&g_srv, NULL);
        uv_loop_close(g_srv.loop);
        g_srv_inited = 0;
        memset(&g_srv, 0, sizeof(g_srv));
    }
    Py_RETURN_NONE;
}
