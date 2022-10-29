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

void write_cb(uv_write_t * req, int status)
{
    LOGe_IF(status, "Write error %s\n", uv_strerror(status));
    write_req_t * wreq = (write_req_t*)req;
    client_t * client = (client_t *)wreq->client;
    reset_response_body(client);
    wreq->client = NULL;  // free write_req
}

int stream_write(client_t * client)
{
    if (client->response.headers_size == 0)
        return CA_OK; // error ???
    if (!client->head.data || client->head.size == 0)
        return CA_OK; // error ???
    write_req_t * wreq = &client->response.write_req;
    int total_len = 0;
    int nbufs = 1;
    wreq->bufs[0].base = client->head.data;
    wreq->bufs[0].len = client->head.size;
    total_len += wreq->bufs[0].len;
    if (client->response.body_preloaded_size > 0) {
        for (int i = 0; i < client->response.body_chunk_num; i++) {
            Py_ssize_t size = PyBytes_GET_SIZE(client->response.body[i]);
            char * data = PyBytes_AS_STRING(client->response.body[i]);
            wreq->bufs[i+1].base = data;
            wreq->bufs[i+1].len = (unsigned int)size;
            nbufs++;
            total_len += (int)size;
        }
    }
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
        LOGt("http chunk parsed: load_state = %d, wsgi_input_size = %d", client->request.load_state, client->request.wsgi_input_size);
        // continue read from socket
        goto fin;
    }
    if (client->request.load_state != LS_OK) {
        // error from callback function "on_message_complete"
        err = HTTP_STATUS_BAD_REQUEST;
        goto fin;
    }
    LOGd("HTTP request successfully parsed (wsgi_input_size = %d)", client->request.wsgi_input_size);
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
    LOGi("Response created! (len = %d+%d)", client->head.size, client->response.body_preloaded_size);
    act = stream_write(client);
    if (!client->request.keep_alive)
        act = CA_SHUTDOWN;

fin:
    if (buf->base)
        free(buf->base);

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
    LOGd("Allocating buffer (size = %d)", (int)suggested_size);
    buf->base = (char*)malloc(suggested_size + 1);
    buf->len = suggested_size;
    buf->base[suggested_size] = 0;
}

void connection_cb(uv_stream_t * server, int status)
{
    if (status < 0) {
        LOGe("Connection error %s\n", uv_strerror(status));
        return;
    }
    LOGn("new connection =================================");
    client_t* client = calloc(1, sizeof(client_t));
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

    main();
    Py_RETURN_NONE;
}
