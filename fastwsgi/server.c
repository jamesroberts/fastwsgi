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

void alloc_cb(uv_handle_t * handle, size_t suggested_size, uv_buf_t * buf);
void read_cb(uv_stream_t * handle, ssize_t nread, const uv_buf_t * buf);
int stream_write(client_t * client);

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
    LOGn("disconnected =================================");
    client_t * client = (client_t *)handle;
    Py_XDECREF(client->request.headers);
    Py_XDECREF(client->request.wsgi_input_empty);
    Py_XDECREF(client->request.wsgi_input);
    xbuf_free(&client->head);
    free_start_response(client);
    reset_response_body(client);
    free_read_buffer(client, NULL);
    free(client);
}

void close_connection(client_t * client)
{
    if (!uv_is_closing((uv_handle_t*)client))
        uv_close((uv_handle_t*)client, close_cb);
}

void shutdown_cb(uv_shutdown_t * req, int status)
{
    uv_close((uv_handle_t*)req->handle, close_cb);
    free(req);
}

void shutdown_connection(client_t * client)
{
    uv_shutdown_t* shutdown = malloc(sizeof(uv_shutdown_t));
    uv_shutdown(shutdown, (uv_stream_t *)client, shutdown_cb);
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
    write_req_t * wreq = (write_req_t*)req;
    client_t * client = (client_t *)wreq->client;
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
            LOGd("%s: Response body is completely streamed. body_total_size = %lld", __func__, body_total_size);
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
    uv_read_start((uv_stream_t *)client, alloc_cb, read_cb);
    reset_response_body(client);
    wreq->client = NULL;  // free write_req
    if (status < 0) {
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
    return (client->request.keep_alive) ? CA_OK : CA_SHUTDOWN;
}

void read_cb(uv_stream_t * handle, ssize_t nread, const uv_buf_t * buf)
{
    int err = 0;
    int act = CA_OK;
    client_t * client = (client_t *)handle;
    llhttp_t * parser = &client->request.parser;

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
    
    enum llhttp_errno error = llhttp_execute(parser, buf->base, nread);
    if (error != HPE_OK) {
        LOGe("Parse error: %s %s\n", llhttp_errno_name(error), client->request.parser.reason);
        act = send_fatal(client, HTTP_STATUS_BAD_REQUEST, NULL);
        err = 0;  // skip call send_error
        goto fin;
    }
    if (client->request.load_state < LS_MSG_END) {
        if (client->error) {
            err = HTTP_STATUS_BAD_REQUEST;
            goto fin;
        }
        LOGt("http chunk parsed: load_state = %d, wsgi_input_size = %lld", client->request.load_state, client->request.wsgi_input_size);
        // continue read from socket
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
    LOGd("HTTP request successfully parsed (wsgi_input_size = %lld)", client->request.wsgi_input_size);
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
    LOGi("Response created! (len = %d+%lld)", client->head.size, client->response.body_preloaded_size);
    act = stream_write(client);
    if (!client->request.keep_alive)
        act = CA_SHUTDOWN;

fin:
    if (buf->base)
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
    if (client->request.load_state >= LS_MSG_END) {
        llhttp_reset(&client->request.parser);
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
    const int read_buffer_size = (int)g_srv.read_buffer_size;
    LOGt("%s: size = %d (suggested = %d)", __func__, read_buffer_size, (int)suggested_size);
    client_t * client = (client_t *)handle;
    buf->base = NULL;
    buf->len = 0;

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
    if (status < 0) {
        LOGe("Connection error %s\n", uv_strerror(status));
        return;
    }
    LOGn("new connection =================================");
    client_t* client = calloc(1, sizeof(client_t) + g_srv.read_buffer_size + 8);
    client->srv = &g_srv;

    uv_tcp_init(g_srv.loop, &client->handle);
    uv_tcp_nodelay(&client->handle, 0);
    uv_tcp_keepalive(&client->handle, 1, 60);

    client->handle.data = client;

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
        rc = uv_ip_name(&sock_addr.addr, client->remote_addr, sizeof(client->remote_addr));
        LOGw_IF(rc, "%s: cannot get remote IP-addr (err = %d)", __func__, rc);
        if (rc)
            client->remote_addr[0] = 0;
        LOGn_IF(rc == 0, "remote IP-addr: %s", client->remote_addr);
    }
    llhttp_init(&client->request.parser, HTTP_REQUEST, &g_srv.parser_settings);
    client->request.parser.data = client;
    uv_read_start((uv_stream_t*)&client->handle, alloc_cb, read_cb);
}

void signal_handler(uv_signal_t * req, int signum)
{
    if (signum == SIGINT) {
        uv_stop(g_srv.loop);
        uv_signal_stop(req);
        exit(0);
    }
}

int main()
{
    g_srv.loop = uv_default_loop();

    configure_parser_settings(&g_srv.parser_settings);
    init_constants();
    init_request_dict();

    struct sockaddr_in addr;
    uv_ip4_addr(g_srv.host, g_srv.port, &addr);

    uv_tcp_init_ex(g_srv.loop, &g_srv.server, AF_INET);

    uv_fileno((const uv_handle_t*)&g_srv.server, &g_srv.file_descriptor);

    int enabled = 1;
#ifdef _WIN32
    //uv__socket_sockopt((uv_handle_t*)&g_srv.server, SO_REUSEADDR, &enabled);
#else
    int so_reuseport = 15;  // SO_REUSEPORT
    uv__socket_sockopt((uv_handle_t*)&g_srv.server, so_reuseport, &enabled);
#endif

    int err = uv_tcp_bind(&g_srv.server, (const struct sockaddr*)&addr, 0);
    if (err) {
        LOGe("Bind error %s\n", uv_strerror(err));
        return 1;
    }

    err = uv_listen((uv_stream_t*)&g_srv.server, g_srv.backlog, connection_cb);
    if (err) {
        LOGe("Listen error %s\n", uv_strerror(err));
        return 1;
    }

    uv_signal_t signal;
    uv_signal_init(g_srv.loop, &signal);
    uv_signal_start(&signal, signal_handler, SIGINT);

    uv_run(g_srv.loop, UV_RUN_DEFAULT);
    uv_loop_close(g_srv.loop);
    free(g_srv.loop);
    return 0;
}

PyObject * run_server(PyObject * self, PyObject * args)
{
    int64_t rv;
    memset(&g_srv, 0, sizeof(g_srv));
    int log_level = 0;
    PyArg_ParseTuple(args, "Osiii", &g_srv.wsgi_app, &g_srv.host, &g_srv.port, &g_srv.backlog, &log_level);
    set_log_level(log_level);

    rv = get_env_int("FASTWSGI_MAX_CONTENT_LENGTH");
    g_srv.max_content_length = (rv >= 0) ? rv : def_max_content_length;
    if (g_srv.max_content_length >= INT_MAX)
        g_srv.max_content_length = INT_MAX - 1;

    rv = get_env_int("FASTWSGI_MAX_CHUNK_SIZE");
    g_srv.max_chunk_size = (rv >= 0) ? (size_t)rv : (size_t)def_max_chunk_size;
    g_srv.max_chunk_size = _min(g_srv.max_chunk_size, MAX_max_chunk_size);
    g_srv.max_chunk_size = _max(g_srv.max_chunk_size, MIN_max_chunk_size);

    rv = get_env_int("FASTWSGI_READ_BUFFER_SIZE");
    g_srv.read_buffer_size = (rv >= 0) ? (size_t)rv : (size_t)def_read_buffer_size;
    g_srv.read_buffer_size = _min(g_srv.read_buffer_size, MAX_read_buffer_size);
    g_srv.read_buffer_size = _max(g_srv.read_buffer_size, MIN_read_buffer_size);

    main();
    Py_RETURN_NONE;
}
