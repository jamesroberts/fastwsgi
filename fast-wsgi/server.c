#include <stdio.h>
#include <stdlib.h>

#include "uv.h"
#include "llhttp.h"

#include "server.h"

#include "request.h"

#define SIMPLE_RESPONSE \
  "HTTP/1.1 200 OK\r\n" \
  "Content-Type: text/plain\r\n" \
  "Content-Length: 14\r\n" \
  "\r\n" \
  "Hello, World!\n"

const static char* HOST = "0.0.0.0";
const static int PORT = 5000;
const static int BACKLOG = 256;

static uv_tcp_t server;

static void set_header(PyObject* headers, PyObject* key, const char* value, size_t length) {
    printf("setting header: %s\n", value);
    PyObject* item = PyUnicode_FromStringAndSize(value, length);
    PyDict_SetItem(headers, key, item);
    Py_DECREF(item);
}

int on_message_complete(llhttp_t* parser) {
    printf("on message complete\n");
    return 0;
};

int on_url(llhttp_t* parser, const char* url, size_t length) {
    printf("on url\n");
    Request* request = (Request*)parser->data;
    PyObject* header = Py_BuildValue("s", "url");

    Py_INCREF(header);
    set_header(request->headers, header, url, length);
    return 0;
};

int on_body(llhttp_t* parser, const char* body, size_t length) {
    printf("on body\n");
    Request* request = (Request*)parser->data;
    PyObject* header = Py_BuildValue("s", "body");
    Py_INCREF(header);
    set_header(request->headers, header, body, length);
    return 0;
};

PyObject* current_header;

int on_header_field(llhttp_t* parser, const char* header, size_t length) {
    printf("on header field\n");
    Request* request = (Request*)parser->data;
    current_header = PyUnicode_FromStringAndSize(header, length);
    printf("test\n");
    Py_INCREF(current_header);
    return 0;
};

int on_header_value(llhttp_t* parser, const char* value, size_t length) {
    printf("on header value\n");
    Request* request = (Request*)parser->data;
    set_header(request->headers, current_header, value, length);
    return 0;
};

int on_message_begin(llhttp_t* parser) {
    printf("on message begin\n");
    Request* request = (Request*)parser->data;
    request->headers = PyDict_New();
    return 0;
};

static llhttp_settings_t parser_settings;

static uv_buf_t response_buf;

uv_loop_t* loop;

struct sockaddr_in addr;

void close_cb(uv_handle_t* handle) {
    printf("disconnected\n");
    free(handle);
}

void write_cb(uv_write_t* req, int status) {
    if (status) {
        fprintf(stderr, "Write error %s\n", uv_strerror(status));
    }
    uv_close((uv_handle_t*)req->handle, close_cb);
}

void read_cb(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf) {
    client_t* client = (client_t*)handle;
    if (nread >= 0) {
        enum llhttp_errno err = llhttp_execute(&client->parser, buf->base, nread);
        if (err == HPE_OK) {
            printf("Successfully parsed\n");
            uv_write_t* req = (uv_write_t*)&client->write_req;
            uv_write(req, handle, &response_buf, 1, write_cb);
        }
        else {
            fprintf(stderr, "Parse error: %s %s\n", llhttp_errno_name(err), client->parser.reason);
        }
    }
    else {
        if (nread != UV_EOF)
            fprintf(stderr, "Read error %s\n", uv_err_name(nread));
        uv_close((uv_handle_t*)handle, close_cb);
    }
    free(buf->base);
}

void alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    printf("Allocating buffer\n");
    buf->base = malloc(suggested_size);
    buf->len = suggested_size;
}

void connection_cb(struct uv_stream_s* handle, int status) {
    if (status < 0) {
        fprintf(stderr, "Connection error %s\n", uv_strerror(status));
        return;
    }

    client_t* client = malloc(sizeof(client_t));

    uv_tcp_init(loop, &client->handle);

    if (uv_accept((uv_stream_t*)&server, (uv_stream_t*)&client->handle) == 0) {
        Request* request = malloc(sizeof(Request));
        llhttp_init(&client->parser, HTTP_REQUEST, &parser_settings);
        client->parser.data = request;

        // llhttp_init(&client->parser, HTTP_BOTH, &parser_settings);
        // client->parser.data = client;
        uv_read_start((uv_stream_t*)&client->handle, alloc_cb, read_cb);
    }
}

void configure_parser_settings() {
    parser_settings.on_url = on_url;
    parser_settings.on_body = on_body;
    parser_settings.on_header_field = on_header_field;
    parser_settings.on_header_value = on_header_value;
    parser_settings.on_message_begin = on_message_begin;
    parser_settings.on_message_complete = on_message_complete;
}

int main() {
    loop = uv_default_loop();

    uv_tcp_init(loop, &server);
    llhttp_settings_init(&parser_settings);
    configure_parser_settings();

    response_buf.base = SIMPLE_RESPONSE;
    response_buf.len = sizeof(SIMPLE_RESPONSE);

    uv_ip4_addr(HOST, PORT, &addr);

    int r = uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0);
    if (r) {
        fprintf(stderr, "Bind error %s\n", uv_strerror(r));
        return 1;
    }

    r = uv_listen((uv_stream_t*)&server, BACKLOG, connection_cb);
    if (r) {
        fprintf(stderr, "Listen error %s\n", uv_strerror(r));
        return 1;
    }
    return uv_run(loop, UV_RUN_DEFAULT);
}