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


void close_cb(uv_handle_t* handle) {
    LOGi("disconnected");
    client_t * client = (client_t *)handle;
    Py_XDECREF(client->request.headers);
    xbuf_free(&client->response.buf);
    free(client);
}

void close_connection(uv_stream_t* handle) {
    if (!uv_is_closing((uv_handle_t*)handle))
        uv_close((uv_handle_t*)handle, close_cb);
}

void shutdown_cb(uv_shutdown_t* req, int status) {
    close_connection(req->handle);
    free(req);
}

void shutdown_connection(uv_stream_t* handle) {
    uv_shutdown_t* shutdown = malloc(sizeof(uv_shutdown_t));
    uv_shutdown(shutdown, handle, shutdown_cb);
}

void write_cb(uv_write_t* req, int status) {
    LOGe_IF(status, "Write error %s\n", uv_strerror(status));
    //write_req_t* write_req = (write_req_t*)req;
    free(req);
}

void stream_write(client_t * client, const void* data, size_t size) {
    if (!data || !size) {
        data = (const void*)client->response.buf.data;
        size = (size_t)client->response.buf.size;
        if (!data || size == 0)
            return;
    }
    size_t req_size = _Py_SIZE_ROUND_UP(sizeof(write_req_t), 16);
    write_req_t* req = (write_req_t*)malloc(req_size + size);
    req->buf.base = (char *)req + req_size;
    req->buf.len = size;
    memcpy(req->buf.base, data, size);
    uv_write((uv_write_t*)req, (uv_stream_t*)client, &req->buf, 1, write_cb);
}

void send_fatal(client_t * client, int status, const char* error_string) {
    if (!status)
        status = HTTP_STATUS_BAD_REQUEST;
    LOGe("send_fatal: %d", status);
    int body_size = error_string ? strlen(error_string) : 0;
    build_response_ex(client, 0, status, NULL, error_string, body_size);
    stream_write(client, NULL, 0);
    shutdown_connection((uv_stream_t*)client);
}

void send_error(client_t * client, int status, const char* error_string) {
    if (!status)
        status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
    LOGe("send_error: %d", status);
    int flags = (client->request.state.keep_alive) ? RF_SET_KEEP_ALIVE : 0;
    int body_size = error_string ? strlen(error_string) : 0;
    build_response_ex(client, flags, status, NULL, error_string, body_size);
    stream_write(client, NULL, 0);
    if (!client->request.state.keep_alive)
        shutdown_connection((uv_stream_t*)client);
}

void send_response(client_t * client) {
    LOGi("send_response %d bytes", client->response.buf.size);
    stream_write(client, NULL, 0);
    if (!client->request.state.keep_alive)
        shutdown_connection((uv_stream_t*)client);
}


void read_cb(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf) {
    int continue_read = 0;
    client_t * client = (client_t *)handle;
    llhttp_t * parser = &client->request.parser;

    if (client->response.buf.data == NULL)
        xbuf_init(&client->response.buf, NULL, 64*1024);

    if (nread > 0) {
        if ((ssize_t)buf->len > nread)
            buf->base[nread] = 0;
        LOGd("read_cb: [nread = %d]", (int)nread);
        LOGt(buf->base);
        enum llhttp_errno err = llhttp_execute(parser, buf->base, nread);
        if (err == HPE_OK) {
            LOGi("Successfully parsed (response len = %d)", client->response.buf.size);
            if (client->response.buf.size > 0)
                send_response(client);
            else if (client->request.state.error)
                send_error(client, HTTP_STATUS_INTERNAL_SERVER_ERROR, NULL);
            else
                continue_read = 1;
        }
        else {
            LOGe("Parse error: %s %s\n", llhttp_errno_name(err), client->request.parser.reason);
            send_fatal(client, HTTP_STATUS_BAD_REQUEST, NULL);
        }
    }
    LOGd_IF(!nread, "read_cb: nread = 0");
    if (nread < 0) {
        char err_name[128];
        uv_err_name_r((int)nread, err_name, sizeof(err_name) - 1);
        LOGd("read_cb: nread = %d  error: %s", (int)nread, err_name);

        uv_read_stop(handle);

        if (nread == UV_EOF) {  // remote peer disconnected
            close_connection(handle);
        } else {
            if (nread != UV_ECONNRESET)
                LOGe("Read error: %s", err_name);
            shutdown_connection(handle);
        }
    }

    if (buf->base)
        free(buf->base);

    if (PyErr_Occurred())
        PyErr_Clear();
}

void alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    LOGi("Allocating buffer (size = %d)", (int)suggested_size);
    buf->base = (char*)malloc(suggested_size + 1);
    buf->len = suggested_size;
    buf->base[suggested_size] = 0;
}

void connection_cb(uv_stream_t* server, int status) {
    if (status < 0) {
        LOGe("Connection error %s\n", uv_strerror(status));
        return;
    }
    LOGi("new connection...");
    client_t* client = calloc(1, sizeof(client_t));
    client->srv = &g_srv;

    uv_tcp_init(g_srv.loop, &client->handle);
    uv_tcp_nodelay(&client->handle, 0);
    uv_tcp_keepalive(&client->handle, 1, 60);

    struct sockaddr sockname;
    struct sockaddr_in* addr = (struct sockaddr_in*)&sockname;
    int socklen = sizeof(sockname);

    uv_tcp_getsockname((uv_tcp_t*)&client->handle, &sockname, &socklen);
    uv_ip4_name(addr, client->remote_addr, sizeof(client->remote_addr));

    client->handle.data = client;

    if (uv_accept(server, (uv_stream_t*)&client->handle) == 0) {
        llhttp_init(&client->request.parser, HTTP_REQUEST, &parser_settings);
        client->request.parser.data = client;
        uv_read_start((uv_stream_t*)&client->handle, alloc_cb, read_cb);
    }
    else {
        uv_close((uv_handle_t*)&client->handle, close_cb);
    }
}

void signal_handler(uv_signal_t* req, int signum) {
    if (signum == SIGINT) {
        uv_stop(g_srv.loop);
        uv_signal_stop(req);
        exit(0);
    }
}

int main() {
    g_srv.loop = uv_default_loop();

    configure_parser_settings();
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

PyObject* run_server(PyObject* self, PyObject* args) {
    memset(&g_srv, 0, sizeof(g_srv));
    int log_level = 0;
    PyArg_ParseTuple(args, "Osiii", &g_srv.wsgi_app, &g_srv.host, &g_srv.port, &g_srv.backlog, &log_level);
    set_log_level(log_level);
    main();
    Py_RETURN_NONE;
}
