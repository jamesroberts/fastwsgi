#include "server.h"
#include "request.h"
#include "llhttp.h"

static void set_header(PyObject* headers, PyObject* key, const char* value, size_t length) {
    printf("setting header\n");
    PyObject* item = PyUnicode_FromStringAndSize(value, length);
    PyDict_SetItem(headers, key, item);
    Py_DECREF(item);
}

int on_message_begin(llhttp_t* parser) {
    printf("on message begin\n");
    Request* request = (Request*)parser->data;
    request->headers = PyDict_New();
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
    Py_INCREF(current_header);
    return 0;
};

int on_header_value(llhttp_t* parser, const char* value, size_t length) {
    printf("on header value\n");
    Request* request = (Request*)parser->data;
    set_header(request->headers, current_header, value, length);
    return 0;
};

int on_message_complete(llhttp_t* parser) {
    printf("on message complete\n");
    Request* request = (Request*)parser->data;
    PyObject_CallFunctionObjArgs(
        wsgi_app, request->headers, NULL
    );
    return 0;
};

void configure_parser_settings() {
    llhttp_settings_init(&parser_settings);
    parser_settings.on_url = on_url;
    parser_settings.on_body = on_body;
    parser_settings.on_header_field = on_header_field;
    parser_settings.on_header_value = on_header_value;
    parser_settings.on_message_begin = on_message_begin;
    parser_settings.on_message_complete = on_message_complete;
}

