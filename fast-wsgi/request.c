#include "server.h"
#include "request.h"
#include "llhttp.h"
#include "constants.h"

static void reprint(PyObject* obj) {
    PyObject* repr = PyObject_Repr(obj);
    PyObject* str = PyUnicode_AsEncodedString(repr, "utf-8", "~E~");
    const char* bytes = PyBytes_AS_STRING(str);

    printf("REPR: %s\n", bytes);

    Py_XDECREF(repr);
    Py_XDECREF(str);
}

static void set_header(PyObject* headers, const char* key, const char* value, size_t length) {
    logger("setting header");
    PyObject* item = PyUnicode_FromStringAndSize(value, length);
    PyDict_SetItemString(headers, key, item);
    Py_DECREF(item);
}

int on_message_begin(llhttp_t* parser) {
    logger("on message begin");
    Request* request = (Request*)parser->data;
    request->headers = PyDict_Copy(base_dict);
    Py_INCREF(request->headers);
    return 0;
};

int on_url(llhttp_t* parser, const char* url, size_t length) {
    logger("on url");
    Request* request = (Request*)parser->data;
    set_header(request->headers, "PATH_INFO", url, length);
    return 0;
};

int on_body(llhttp_t* parser, const char* body, size_t length) {
    logger("on body");
    Request* request = (Request*)parser->data;
    set_header(request->headers, "wsgi.input", body, length);
    return 0;
};

int on_header_field(llhttp_t* parser, const char* header, size_t length) {
    logger("on header field");

    char upperHeader[length + 1];
    for (size_t i = 0; i < length; i++) {
        char current = header[i];
        if (current == '-') {
            upperHeader[i] = '_';
        }
        else if (current >= 'a' && current <= 'z') {
            upperHeader[i] = current - ('a' - 'A');
        }
        else {
            upperHeader[i] = current;
        }
    }
    upperHeader[length] = 0;
    asprintf(&current_header, "HTTP_%s", upperHeader);
    return 0;
};

int on_header_value(llhttp_t* parser, const char* value, size_t length) {
    logger("on header value");
    Request* request = (Request*)parser->data;
    set_header(request->headers, current_header, value, length);
    return 0;
};

PyObject* start_response_call(PyObject* self, PyObject* args, PyObject* kwargs) {
    StartResponse* sr = (StartResponse*)self;
    if (!PyArg_UnpackTuple(args, "start_response", 2, 3, &sr->status, &sr->headers, &sr->exc_info)) {
        printf("something went wrong\n");
        return NULL;
    }

    Py_XINCREF(sr->status);
    Py_XINCREF(sr->headers);
    Py_XINCREF(sr->exc_info);

    Py_RETURN_NONE;
}

PyTypeObject StartResponse_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "start_response",
    sizeof(StartResponse),
    0,
    (destructor)PyObject_FREE,
    0, 0, 0, 0, 0, 0, 0, 0, 0,
    start_response_call
};

int on_message_complete(llhttp_t* parser) {
    logger("on message complete");
    Request* request = (Request*)parser->data;
    build_wsgi_environ(parser);

    StartResponse* start_response = PyObject_NEW(StartResponse, &StartResponse_Type);

    logger("calling wsgi application");
    PyObject* wsgi_response = PyObject_CallFunctionObjArgs(
        wsgi_app, request->headers, start_response, NULL
    );
    logger("called wsgi application");
    build_response(wsgi_response, start_response);

    Py_CLEAR(start_response->headers);
    Py_CLEAR(start_response->status);
    Py_CLEAR(start_response->exc_info);
    Py_CLEAR(start_response);

    Py_CLEAR(wsgi_response);

    PyDict_Clear(request->headers);
    Py_CLEAR(request->headers);

    return 0;
};

void build_response(PyObject* wsgi_response, StartResponse* response) {
    logger("building response");
    PyObject* iter = PyObject_GetIter(wsgi_response);
    PyObject* result = PyIter_Next(iter);


    char* buf = "HTTP/1.1";
    PyObject* status = PyUnicode_AsUTF8String(response->status);
    asprintf(&buf, "%s %s", buf, PyBytes_AsString(status));
    Py_DECREF(status);

    while (PyList_Size(response->headers) > 0) {
        PyObject* tuple = PyList_GET_ITEM(response->headers, 0);
        PyObject* field = PyUnicode_AsUTF8String(PyTuple_GET_ITEM(tuple, 0));
        PyObject* value = PyUnicode_AsUTF8String(PyTuple_GET_ITEM(tuple, 1));

        Py_DECREF(tuple);
        Py_DECREF(field);
        Py_DECREF(value);

        asprintf(&buf, "%s\r\n%s: %s", buf, PyBytes_AsString(field), PyBytes_AsString(value));
        logger("added header");

        PySequence_DelItem(response->headers, 0);
    }

    asprintf(&buf, "%s\r\n\r\n%s", buf, PyBytes_AsString(result));

    response_buf.base = buf;
    response_buf.len = strlen(buf);

    PyObject* close = PyObject_GetAttrString(iter, "close");
    if (close != NULL) {
        PyObject* close_result = PyObject_CallObject(close, NULL);
        Py_XDECREF(close_result);
    }

    Py_DECREF(close);
    Py_DECREF(iter);
    Py_XDECREF(result);
}


void build_wsgi_environ(llhttp_t* parser) {
    logger("building wsgi environ");
    Request* request = (Request*)parser->data;

    const char* method = llhttp_method_name(parser->method);
    const char* protocol = parser->http_minor == 1 ? "HTTP/1.1" : "HTTP/1.0";

    set_header(request->headers, "REQUEST_METHOD", method, strlen(method));
    set_header(request->headers, "SERVER_PROTOCOL", protocol, strlen(protocol));
}

void init_request_dict() {
    // Sets up base request dict for new incoming requests
    // https://www.python.org/dev/peps/pep-3333/#specification-details
    base_dict = PyDict_New();
    PyDict_SetItem(base_dict, SCRIPT_NAME, empty_string);
    PyDict_SetItem(base_dict, SERVER_NAME, server_host);
    PyDict_SetItem(base_dict, SERVER_PORT, server_port);
    PyDict_SetItem(base_dict, wsgi_version, version);
    PyDict_SetItem(base_dict, wsgi_url_scheme, http_scheme);
    PyDict_SetItem(base_dict, wsgi_errors, PySys_GetObject("stderr"));
    PyDict_SetItem(base_dict, wsgi_run_once, Py_False);
    PyDict_SetItem(base_dict, wsgi_multithread, Py_False);
    PyDict_SetItem(base_dict, wsgi_multiprocess, Py_True);
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