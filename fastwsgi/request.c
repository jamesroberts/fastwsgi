#include "server.h"
#include "request.h"
#include "llhttp.h"
#include "constants.h"
#include "start_response.h"

PyObject* base_dict = NULL;

void build_response(client_t * client, StartResponse* response);

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
    XBUF_RESET(client->response.head);
    XBUF_RESET(client->response.body);
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

void set_type_error(PyObject* obj) {
    const char * type = Py_TYPE(obj)->tp_name;
    LOGe("response type should be bytes or a byte iterator, got '%s'", type);
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
            Py_XDECREF(close);
        }
    }
}

int extract_response(client_t * client, PyObject * wsgi_response) {
    int err = 0;
    PyObject* iterator = NULL;
    PyObject* response = NULL;
    PyObject* item = NULL;
    xbuf_t * body = &client->response.body;
    xbuf_reset(body);  // reset body buffer
    
    if (PyBytes_CheckExact(wsgi_response)) {
        Py_ssize_t size = PyBytes_GET_SIZE(wsgi_response);
        xbuf_add(body, PyBytes_AS_STRING(wsgi_response), size);
        return (int)size;
    }
    // wsgi_body types: ClosingIterator, FileWrapper
    LOGd("wsgi_body: type = %s", Py_TYPE(wsgi_response)->tp_name);
    //LOGd_IF(PyIter_Check(wsgi_response), "wsgi_body is ITERATOR!");
    iterator = PyObject_GetIter(wsgi_response);
    if (iterator != NULL && PyIter_Check(iterator)) {
        while (item = PyIter_Next(iterator)) {
            if (!PyBytes_CheckExact(item)) {
                err = -1;
                LOGe("wsgi_body: contain item type = '%s' (expected bytes)", Py_TYPE(item)->tp_name);
                Py_DECREF(item);
                break;
            }
            Py_ssize_t size = PyBytes_GET_SIZE(item);
            LOGd("wsgi_body: get bytes size = %d", (int)size);
            xbuf_add(body, PyBytes_AS_STRING(item), size);
            Py_DECREF(item);
        }
        close_iterator(iterator);
        Py_XDECREF(iterator);
    }
    if (err) {
        set_type_error(wsgi_response);
    }
    return err ? err : body->size;
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
        int len = extract_response(client, wsgi_response);
        if (len >= 0) {
            build_response(client, start_response);
        }
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

int build_response_ex(void * _client, int flags, int status, const void * headers, const void * body_data, int body_size) {
    client_t * client = (client_t *)_client;
    xbuf_t * head = &client->response.head;
    xbuf_reset(head);   // reset headers buffer
    xbuf_t * body = &client->response.body;
    StartResponse * response = NULL;

    if (flags & RF_HEADERS_PYLIST) {
        response = (StartResponse *)headers;
        headers = NULL;
    }
    if (response) {
        char scode[4];
        size_t status_len = 0;
        const char * status_code = PyUnicode_AsUTF8AndSize(response->status, &status_len);
        if (status_len < 3)
            return -2;
        memcpy(scode, status_code, 4);
        scode[3] = 0;
        status = atoi(scode);
    }
    if (status == 204 || status == 304) {
        body_size = 0;
        xbuf_reset(body);  // forced reset body buffer
    }

    const char * status_name = llhttp_status_name(status);
    if (!status_name)
        return -3;

    char * buf = xbuf_expand(head, 128);
    head->size += sprintf(buf, "HTTP/1.1 %d %s\r\n", status, status_name);

    if (response) {
        Py_ssize_t hsize = PyList_GET_SIZE(response->headers);
        for (Py_ssize_t i = 0; i < hsize; i++) {
            PyObject* tuple = PyList_GET_ITEM(response->headers, i);

            size_t key_len = 0;
            const char * key = PyUnicode_AsUTF8AndSize(PyTuple_GET_ITEM(tuple, 0), &key_len);

            if (key_len == 14 && key[7] == '-')
                if (strcasecmp(key, "Content-Length") == 0)
                    continue;  // skip "Content-Length" header

            size_t value_len = 0;
            const char * value = PyUnicode_AsUTF8AndSize(PyTuple_GET_ITEM(tuple, 1), &value_len);

            xbuf_add(head, key, key_len);
            xbuf_add(head, ": ", 2);
            xbuf_add(head, value, value_len);
            xbuf_add(head, "\r\n", 2);

            LOGi("added header '%s: %s'", key, value);
        }
    }
    else if (headers) {
        xbuf_add_str(head, (const char *)headers);
    }

    if (flags & RF_SET_KEEP_ALIVE) {
        xbuf_add_str(head, "Connection: Keep-Alive\r\n");
    } else {
        xbuf_add_str(head, "Connection: close\r\n");
    }

    if (body_size < 0) {
        body_data = body->data;
        body_size = body->size;
    } else {
        xbuf_reset(body);  // reset body buffer
        if (body_data && body_size > 0)
            xbuf_add(body, body_data, body_size);
    }

    if (body_size == 0) {
        xbuf_add_str(head, "Content-Length: 0\r\n");
        LOGi("Added Header 'Content-Length: 0'");
    } else {
        char * buf = xbuf_expand(head, 48);
        head->size += sprintf(buf, "Content-Length: %d\r\n", (int)body_size);
        LOGi("Added Header 'Content-Length: %d'", (int)body_size);
    }
    xbuf_add(head, "\r\n", 2);  // end of headers

    LOGt(head->data);
    LOGt_IF(body->size > 0, body->data);
    return head->size;
}

void build_response(client_t * client, StartResponse* response) {
    LOGi("building response");
    int flags = RF_HEADERS_PYLIST;
    if (client->request.state.keep_alive)
        flags |= RF_SET_KEEP_ALIVE;

    int len = build_response_ex(client, flags, 0, response, NULL, -1);
    if (len <= 0)
        client->request.state.error = 1;
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
