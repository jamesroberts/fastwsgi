#include "server.h"
#include "request.h"
#include "llhttp.h"
#include "constants.h"
#include "start_response.h"

PyObject* base_dict = NULL;

void logrepr(int level, PyObject* obj) {
    PyObject* repr = PyObject_Repr(obj);
    PyObject* str = PyUnicode_AsEncodedString(repr, "utf-8", "~E~");
    const char* bytes = PyBytes_AS_STRING(str);
    LOGX(level, "REPR: %s", bytes);
    Py_XDECREF(repr);
    Py_XDECREF(str);
}

static void set_header(PyObject* headers, const char* key, const char* value, size_t length) {
    LOGi("setting header");
    int vlen = (length > 0) ? (int)length : (int)strlen(value);
    PyObject* item = PyUnicode_FromStringAndSize(value, vlen);

    PyObject* existing_item = PyDict_GetItemString(headers, key);
    if (existing_item) {
        PyObject* value_list = Py_BuildValue("[SS]", existing_item, item);
        PyObject* updated_item = PyUnicode_Join(g_cv.comma, value_list);

        PyDict_SetItemString(headers, key, updated_item);
        Py_DECREF(updated_item);
        Py_DECREF(value_list);
    }
    else {
        PyDict_SetItemString(headers, key, item);
    }
    Py_DECREF(item);
}

int on_message_begin(llhttp_t* parser) {
    LOGi("on message begin");
    client_t * client = (client_t *)parser->data;
    client->request.state.keep_alive = 0;
    client->request.state.error = 0;
    client->response.buf.size = 0;
    if (client->request.headers == NULL) {
        PyObject* headers = PyDict_Copy(base_dict);
        // Sets up base request dict for new incoming requests
        // https://www.python.org/dev/peps/pep-3333/#specification-details
        PyObject* io_BytesIO = PyObject_CallMethodObjArgs(g_cv.module_io, g_cv.BytesIO, NULL);
        PyDict_SetItem(headers, g_cv.wsgi_input, io_BytesIO);
        client->request.headers = headers;
    } else {
        PyObject* input = PyDict_GetItem(client->request.headers, g_cv.wsgi_input);
        PyObject* result1 = PyObject_CallMethodObjArgs(input, g_cv.truncate, PyLong_FromLong(0L), NULL);
        Py_DECREF(result1);
        PyObject* result2 = PyObject_CallMethodObjArgs(input, g_cv.seek, PyLong_FromLong(0L), NULL);
        Py_DECREF(result2);
    }
    return 0;
};

int on_url(llhttp_t* parser, const char* data, size_t length) {
    LOGi("on url");
    client_t * client = (client_t *)parser->data;

    char* url = malloc(length + 1);
    strncpy(url, data, length);
    url[length] = 0;

    char* query_string = strchr(url, '?');
    if (query_string) {
        *query_string = 0;
        set_header(client->request.headers, "QUERY_STRING", query_string + 1, strlen(query_string + 1));
    }
    set_header(client->request.headers, "PATH_INFO", url, strlen(url));

    free(url);
    return 0;
};

int on_body(llhttp_t* parser, const char* body, size_t length) {
    LOGi("on body (len = %d)", (int)length);
    client_t * client = (client_t *)parser->data;

    PyObject* input = PyDict_GetItem(client->request.headers, g_cv.wsgi_input);

    PyObject* body_content = PyBytes_FromStringAndSize(body, length);
    LOGREPR(LL_TRACE, body_content);
    PyObject* result = PyObject_CallMethodObjArgs(input, g_cv.write, body_content, NULL);
    Py_XDECREF(result);
    Py_XDECREF(body_content);

    return 0;
};

int on_header_field(llhttp_t* parser, const char* header, size_t length) {
    LOGi("on header field");
    client_t * client = (client_t *)parser->data;
    client->request.current_header[0] = 0;   // CVE-2015-0219

    const size_t max_len = sizeof(client->request.current_header) - 1;
    if (length >= max_len - 8)
        return 0;

    char upperHeader[sizeof(client->request.current_header)];
    for (size_t i = 0; i < length; i++) {
        char current = header[i];
        if (current == '_') {
            return 0;
        }
        if (current == '-') {
            upperHeader[i] = '_';
        }
        else {
            upperHeader[i] = toupper(current);
        }
    }
    upperHeader[length] = 0;

    if ((strcmp(upperHeader, "CONTENT_LENGTH") == 0) || (strcmp(upperHeader, "CONTENT_TYPE") == 0)) {
        strcpy(client->request.current_header, upperHeader);
    }
    else {
        strcpy(client->request.current_header, "HTTP_");
        strcat(client->request.current_header, upperHeader);
    }
    return 0;
};

int on_header_value(llhttp_t* parser, const char* value, size_t length) {
    LOGi("on header value");
    client_t * client = (client_t *)parser->data;
    if (client->request.current_header[0]) {
        set_header(client->request.headers, client->request.current_header, value, length);
    }
    return 0;
};

void set_type_error(const char* type) {
    PyErr_Format(
        PyExc_TypeError, "response type should be bytes or a byte iterator, got '%s'", type
    );
}

void close_iterator(PyObject* iterator) {
    if (iterator != NULL && PyObject_HasAttrString(iterator, "close")) {
        PyObject* close = PyObject_GetAttrString(iterator, "close");
        if (close != NULL) {
            PyObject* close_result = PyObject_CallObject(close, NULL);
            Py_XDECREF(close_result);
        }
        Py_XDECREF(close);
    }
}

PyObject* extract_response(PyObject* wsgi_response) {
    PyObject* iterator = NULL;
    PyObject* response = NULL;
    PyObject* item = NULL;

    if (PyBytes_CheckExact(wsgi_response)) {
        return wsgi_response;
    }

    iterator = PyObject_GetIter(wsgi_response);
    if (iterator != NULL && PyIter_Check(iterator)) {
        response = PyBytes_FromString("");
        while ((item = PyIter_Next(iterator))) {
            if (!PyBytes_CheckExact(item)) {
                Py_DECREF(response);
                response = NULL;
                break;
            }
            PyBytes_ConcatAndDel(&response, item);
        }
        close_iterator(iterator);
        Py_XDECREF(iterator);
    }
    if (response == NULL) {
        set_type_error(Py_TYPE(wsgi_response)->tp_name);    
    }
    return response;
}

int on_message_complete(llhttp_t* parser) {
    LOGi("on message complete");
    client_t * client = (client_t *)parser->data;
    PyObject * headers = client->request.headers;

    // Sets the input byte stream position back to 0
    PyObject* body = PyDict_GetItem(headers, g_cv.wsgi_input);
    PyObject* res = PyObject_CallMethodObjArgs(body, g_cv.seek, PyLong_FromLong(0L), NULL);
    Py_DECREF(res);

    build_wsgi_environ(parser);

    StartResponse* start_response = PyObject_NEW(StartResponse, &StartResponse_Type);
    start_response->called = 0;

    LOGi("calling wsgi application");
    PyObject* wsgi_response;
    wsgi_response = PyObject_CallFunctionObjArgs(
        g_srv.wsgi_app, headers, start_response, NULL
    );
    LOGi("called wsgi application");

    if (PyErr_Occurred()) {
        client->request.state.error = 1;
        PyErr_Print();
    }

    if (client->request.state.error == 0) {
        PyObject* response_body = extract_response(wsgi_response);
        if (response_body != NULL) {
            build_response(response_body, start_response, parser);
        }
        Py_CLEAR(response_body);
    }

    // FIXME: Try to not repeat this block in this method
    if (PyErr_Occurred()) {
        client->request.state.error = 1;
        PyErr_Print();
    }

    Py_CLEAR(start_response->headers);
    Py_CLEAR(start_response->status);
    Py_CLEAR(start_response->exc_info);
    Py_CLEAR(start_response);

    Py_CLEAR(wsgi_response);
    Py_CLEAR(client->request.headers);
    return 0;
};

void build_response(PyObject* response_body, StartResponse* response, llhttp_t* parser) {
    LOGi("building response");
    client_t * client = (client_t *)parser->data;
    xbuf_t * xbuf = &client->response.buf;
    xbuf->size = 0;   // reset buffer

    int response_has_no_content = 0;

    size_t status_len = 0;
    const char * status_code = PyUnicode_AsUTF8AndSize(response->status, &status_len);
    if (status_len == 3) {
        if (strncmp(status_code, "204", 3) == 0 || strncmp(status_code, "304", 3) == 0)
            response_has_no_content = 1;
    }

    char * buf = xbuf_expand(xbuf, 32);
    xbuf->size += sprintf(buf, "HTTP/1.1 %s\r\n", status_code);

    char* connection_header = "\r\nConnection: close";
    if (llhttp_should_keep_alive(parser)) {
        client->request.state.keep_alive = 1;
        xbuf_add_str(xbuf, "Connection: Keep-Alive\r\n");
    }
    else {
        xbuf_add_str(xbuf, "Connection: close\r\n");
    }

    int content_length_header_present = 0;
    for (Py_ssize_t i = 0; i < PyList_GET_SIZE(response->headers); i++) {
        PyObject* tuple = PyList_GET_ITEM(response->headers, i);

        size_t key_len = 0;
        const char * key = PyUnicode_AsUTF8AndSize(PyTuple_GET_ITEM(tuple, 0), &key_len);
        size_t value_len = 0;
        const char * value = PyUnicode_AsUTF8AndSize(PyTuple_GET_ITEM(tuple, 1), &value_len);

        if (!content_length_header_present)
            if (strcasecmp(key, "Content-Length") == 0)
                content_length_header_present = 1;

        xbuf_add(xbuf, key, key_len);
        xbuf_add(xbuf, ": ", 2);
        xbuf_add(xbuf, value, value_len);
        xbuf_add(xbuf, "\r\n", 2);

        LOGi("added header \"%s: %s\"", key, value);
    }

    size_t response_body_size = PyBytes_GET_SIZE(response_body);
    char * response_body_data = PyBytes_AS_STRING(response_body);

    if (response_has_no_content) {
        xbuf_add_str(xbuf, "Content-Length: 0\r\n");
        response_body_size = 0;
    }
    else {
        if (content_length_header_present == 0) {
            char * buf = xbuf_expand(xbuf, 48);
            xbuf->size += sprintf(buf, "Content-Length: %d\r\n", (int)response_body_size);
        }
    }
    xbuf_add(xbuf, "\r\n", 2);  // end of headers
    if (response_body_size)
        xbuf_add(xbuf, response_body_data, response_body_size);

    LOGt(xbuf->data);
}


void build_wsgi_environ(llhttp_t* parser) {
    LOGi("building wsgi environ");
    client_t * client = (client_t *)parser->data;
    PyObject * headers = client->request.headers;

    const char* method = llhttp_method_name(parser->method);
    set_header(headers, "REQUEST_METHOD", method, 0);
    const char* protocol = parser->http_minor == 1 ? "HTTP/1.1" : "HTTP/1.0";
    set_header(headers, "SERVER_PROTOCOL", protocol, 0);
    set_header(headers, "REMOTE_ADDR", client->remote_addr, 0);
}

void init_request_dict() {
    // only constant values!!!
    base_dict = PyDict_New();
    PyDict_SetItem(base_dict, g_cv.SCRIPT_NAME, g_cv.empty_string);
    PyDict_SetItem(base_dict, g_cv.SERVER_NAME, g_cv.server_host);
    PyDict_SetItem(base_dict, g_cv.SERVER_PORT, g_cv.server_port);
    //PyDict_SetItem(base_dict, g_cv.wsgi_input, io_BytesIO);   // not const!!!
    PyDict_SetItem(base_dict, g_cv.wsgi_version, g_cv.version);
    PyDict_SetItem(base_dict, g_cv.wsgi_url_scheme, g_cv.http_scheme);
    PyDict_SetItem(base_dict, g_cv.wsgi_errors, PySys_GetObject("stderr"));
    PyDict_SetItem(base_dict, g_cv.wsgi_run_once, Py_False);
    PyDict_SetItem(base_dict, g_cv.wsgi_multithread, Py_False);
    PyDict_SetItem(base_dict, g_cv.wsgi_multiprocess, Py_True);
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
