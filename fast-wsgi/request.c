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
    PyObject* header = Py_BuildValue("s", "PATH_INFO");

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

int on_header_field(llhttp_t* parser, const char* header, size_t length) {
    printf("on header field\n");
    Request* request = (Request*)parser->data;
    PyObject* HTTP_ = PyUnicode_FromString("HTTP_");

    char upperHeader[length];
    for (int i = 0; i < length; i++) {
        upperHeader[i] = toupper(header[i]);
    }
    PyObject* uppercaseHeader = PyUnicode_FromStringAndSize(upperHeader, length);

    current_header = PyUnicode_Concat(HTTP_, uppercaseHeader);
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
    build_wsgi_environ(parser);
    PyObject_CallFunctionObjArgs(
        wsgi_app, request->headers, NULL
    );
    return 0;
};

void build_wsgi_environ(llhttp_t* parser) {
    Request* request = (Request*)parser->data;
    char* method = llhttp_method_name(parser->method);
    char* protocol = parser->http_minor == 1 ? "HTTP/1.1" : "HTTP/1.0";
    PyObject* wsgi_version = PyTuple_Pack(2, PyLong_FromLong(1), PyLong_FromLong(0));

    // Find a better way to set these
    // https://www.python.org/dev/peps/pep-3333/#specification-details
    PyDict_SetItemString(request->headers, "REQUEST_METHOD", PyUnicode_FromString(method));
    PyDict_SetItemString(request->headers, "SCRIPT_NAME", PyUnicode_FromString(""));
    PyDict_SetItemString(request->headers, "SERVER_NAME", PyUnicode_FromString(host));
    PyDict_SetItemString(request->headers, "SERVER_PORT", PyUnicode_FromString("port"));
    PyDict_SetItemString(request->headers, "SERVER_PROTOCOL", PyUnicode_FromString(protocol));
    PyDict_SetItemString(request->headers, "wsgi.version", wsgi_version);
    PyDict_SetItemString(request->headers, "wsgi.url_scheme", PyUnicode_FromString("http"));
    PyDict_SetItemString(request->headers, "wsgi.errors", PySys_GetObject("stderr"));
    PyDict_SetItemString(request->headers, "wsgi.run_once", Py_False);
    PyDict_SetItemString(request->headers, "wsgi.multithread", Py_False);
    PyDict_SetItemString(request->headers, "wsgi.multiprocess", Py_True);
}

void configure_parser_settings() {
    llhttp_settings_init(&parser_settings);
    parser_settings.on_url = on_url;
    parser_settings.on_body = on_body;
    parser_settings.on_header_field = on_header_field;
    parser_settings.on_header_value = on_header_value;
    parser_settings.on_message_begin = on_message_begin;
    parser_settings.on_message_complete = on_message_complete;
}

